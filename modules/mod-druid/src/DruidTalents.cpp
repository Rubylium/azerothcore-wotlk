#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "LiveTuning.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <vector>

// The Druid's talents and abilities on the retail-style trees (localTools/druid/talentTree.json) that spell data cannot
// carry. A talent is its rank spell's aura on the Druid (learned by mod-custom-classes' TalentTree.cpp), read here with
// HasAura; a specialization is its passive (Équilibre, Combat farouche, Restauration). The abilities, auras and hits
// named below are in localTools/druid/Spells.ps1, the stock spells changed in place in StockSpells.ps1; the WotLK
// talents the trees reuse keep their own scripts in the core (spell_druid.cpp).
//
// The Druid keeps mana, its forms, energy and combo points (cat) and rage (bear). Balance fights with Astral Power
// (Puissance astrale), an aura of up to 100 stacks: Wrath, Starfire, Moonfire, Sunfire, Stellar Flare, the shooting
// stars and Fury of Elune fill it, Starsurge and Starfall spend it. Two Wraths bring the Lunar Eclipse (Starfire
// quicker, stronger and splashing wider), two Starfires the Solar Eclipse (Wrath quicker and stronger); Celestial
// Alignment and Incarnation put up both. Starfall strikes the enemies around the Druid every second for 8 s, Fury of
// Elune those around its target. Feral holds the cat and the bear in one tree: the cat bleeds its prey (Rake, Rip,
// Thrash, Primal Wrath's bleed on the whole pack, Feral Frenzy) with Bloodtalons, Predatory Swiftness and Soul of the
// Forest on its finishers, Tiger's Fury gives energy; the bear holds with Ironfur (stacks), Thrash (a stacking bleed),
// Mangle, Gore and Galactic Guardian. Restoration has Efflorescence on the ground, Cenarion Ward, Germination,
// Flourish, Abundance, Soul of the Forest and Cultivation.
//
// A spell whose hit lands with its cast (everything here but Wrath, which only gives Astral Power) reads the procs it
// spends on the hit, which comes before the cast's end (OnSpellCast) spends them. A finishing move's combo points are
// gone by then: OnSpellCheckCast keeps them for it.
namespace
{
// Talent ranks (dummies or auras, read here)
enum Talents : uint32
{
    TALENT_SHOOTING_STARS           = 95908,    // Étoiles filantes
    TALENT_STARLORD                 = 95909,    // Seigneur des étoiles
    TALENT_TWIN_MOONS               = 95910,    // Lunes jumelles
    TALENT_BALANCE_FOREST_1         = 95911,    // Âme de la forêt (Équilibre)
    TALENT_BALANCE_FOREST_2         = 95912,
    TALENT_NATURES_BALANCE          = 95913,    // Équilibre de la nature
    TALENT_IMPROVED_SUNFIRE         = 95914,    // Éclat solaire amélioré
    TALENT_STARWEAVER               = 95919,    // Tisse-étoiles
    TALENT_ORBIT_BREAKER            = 95920,    // Briseur d'orbite
    TALENT_GUARDIAN_PATH            = 95922,    // Voie du gardien (the tank build's choice)
    TALENT_LAYERED_MANE_1           = 95923,    // Crinière étagée
    TALENT_LAYERED_MANE_2           = 95924,
    TALENT_BLOODTALONS              = 95925,    // Griffes sanglantes
    TALENT_GORE                     = 95926,    // Encorner
    TALENT_PREDATORY_SWIFTNESS      = 95927,    // Rapidité du prédateur
    TALENT_GALACTIC_GUARDIAN        = 95928,    // Gardien galactique
    TALENT_FERAL_FOREST             = 95929,    // Âme de la forêt (Combat farouche)
    TALENT_ABUNDANCE                = 95932,    // Abondance
    TALENT_RESTORATION_FOREST       = 95933,    // Âme de la forêt (Restauration)
    TALENT_GERMINATION              = 95934,    // Germination
    TALENT_EXUBERANT_GROWTH         = 95935,    // Croissance exubérante
    TALENT_CULTIVATION              = 95938,    // Culture
};

// The abilities, auras and hits of localTools/druid/Spells.ps1
enum Spells : uint32
{
    SPELL_ASTRAL_POWER              = 96000,
    SPELL_SOLAR_ECLIPSE             = 96001,
    SPELL_LUNAR_ECLIPSE             = 96002,
    SPELL_STARLORD                  = 96003,
    SPELL_STARWEAVER_STARFALL       = 96004,
    SPELL_STARWEAVER_STARSURGE      = 96005,
    SPELL_BLOODTALONS               = 96006,
    SPELL_PREDATORY_SWIFTNESS       = 96007,
    SPELL_GORE                      = 96008,
    SPELL_RESTORATION_FOREST        = 96009,
    SPELL_VORTEX_SNARE              = 96012,
    SPELL_WILD_CHARGE               = 96021,
    SPELL_URSOLS_VORTEX             = 96022,
    SPELL_STARSURGE                 = 96040,
    SPELL_SUNFIRE                   = 96041,
    SPELL_STELLAR_FLARE             = 96042,
    SPELL_CELESTIAL_ALIGNMENT       = 96043,
    SPELL_INCARNATION_ELUNE         = 96044,
    SPELL_FURY_OF_ELUNE             = 96045,
    SPELL_ASTRAL_COMMUNION          = 96046,
    SPELL_STARFIRE_SPLASH           = 96047,
    SPELL_SHOOTING_STAR             = 96048,
    SPELL_STARFALL                  = 96049,
    SPELL_STARFALL_HIT              = 96050,
    SPELL_FURY_OF_ELUNE_HIT         = 96051,
    SPELL_FULL_MOON                 = 96052,
    SPELL_THRASH_CAT                = 96060,
    SPELL_THRASH_BEAR               = 96061,
    SPELL_IRONFUR                   = 96062,
    SPELL_PRIMAL_WRATH              = 96066,
    SPELL_FERAL_FRENZY              = 96067,
    SPELL_PRIMAL_WRATH_HIT          = 96071,
    SPELL_PRIMAL_WRATH_BLEED        = 96072,
    SPELL_FERAL_FRENZY_BLEED        = 96073,
    SPELL_EFFLORESCENCE             = 96080,
    SPELL_CENARION_WARD             = 96081,
    SPELL_FLOURISH                  = 96083,
    SPELL_GERMINATION               = 96084,
    SPELL_EFFLORESCENCE_HEAL        = 96086,
    SPELL_CENARION_WARD_HEAL        = 96087,

    // The specializations' passives
    SPELL_SPEC_BALANCE              = 96180,
    SPELL_SPEC_FERAL                = 96181,
    SPELL_SPEC_RESTORATION          = 96182,

    // Stock (first ranks)
    SPELL_RAKE_R1                   = 1822,
    SPELL_RIP_R1                    = 1079,
    SPELL_CLAW_R1                   = 1082,
    SPELL_WRATH_R1                  = 5176,
    SPELL_STARFIRE_R1               = 2912,
    SPELL_MOONFIRE_R1               = 8921,
    SPELL_SHRED_R1                  = 5221,
    SPELL_TIGERS_FURY_R1            = 5217,
    SPELL_MAUL_R1                   = 6807,
    SPELL_SWIPE_BEAR_R1             = 779,
    SPELL_FEROCIOUS_BITE_R1         = 22568,
    SPELL_MANGLE_CAT_R1             = 33876,
    SPELL_MANGLE_BEAR_R1            = 33878,
    SPELL_SWIPE_CAT                 = 62078,
    SPELL_REJUVENATION_R1           = 774,
    SPELL_REGROWTH_R1               = 8936,
    SPELL_SWIFTMEND                 = 18562,
    SPELL_FERAL_CHARGE_BEAR         = 16979,
    SPELL_FERAL_CHARGE_CAT          = 49376,
    SPELL_GLYPH_OF_WILD_GROWTH      = 62970,
};

// Moonfire (word 0), Sunfire (word 2), Rejuvenation (word 0), the Druid's heals over time
constexpr uint32 FlagMoonfire = 0x2;
constexpr uint32 FlagSunfire = 0x1000000;
constexpr uint32 FlagRejuvenation = 0x10;

// --- Tuning (README.md) ----------------------------------------------------------------------------------------------
// Balance: Astral Power
constexpr int32 AstralPowerMax = 100;
LiveTuning::KnobInt const AstralWrath("druid.astral_wrath", 8);
LiveTuning::KnobInt const AstralStarfire("druid.astral_starfire", 10);
LiveTuning::KnobInt const AstralMoonfire("druid.astral_moonfire", 2);
LiveTuning::KnobInt const AstralSunfire("druid.astral_sunfire", 2);
LiveTuning::KnobInt const AstralStellarFlare("druid.astral_stellar_flare", 8);
LiveTuning::KnobInt const AstralShootingStar("druid.astral_shooting_star", 3);
LiveTuning::KnobInt const AstralFuryOfElune("druid.astral_fury_of_elune", 5);       // each strike
LiveTuning::KnobInt const AstralCommunion("druid.astral_communion", 60);
// Âme de la forêt, entering an Eclipse
LiveTuning::KnobInt const AstralForestPerRank("druid.astral_forest_per_rank", 10);
LiveTuning::KnobInt const StarsurgeCost("druid.starsurge_cost", 40);
LiveTuning::KnobInt const StarfallCost("druid.starfall_cost", 50);
LiveTuning::KnobUInt const AstralFadeMs("druid.astral_fade_ms", 15000);
LiveTuning::KnobUInt const NaturesBalancePeriodMs("druid.natures_balance_period_ms", 1000);
// Balance: Eclipse, the splash, the procs
LiveTuning::KnobInt const EclipseCasts("druid.eclipse_casts", 2);
LiveTuning::Knob const StarfireSplashShare("druid.starfire_splash_share", 0.2f);
LiveTuning::Knob const StarfireLunarSplashShare("druid.starfire_lunar_splash_share", 0.4f);
LiveTuning::Knob const StarfireSplashRange("druid.starfire_splash_range", 8.0f);
LiveTuning::KnobInt const ShootingStarChance("druid.shooting_star_chance", 10);  // each tick of Moonfire or Sunfire
LiveTuning::KnobInt const OrbitBreakerStars("druid.orbit_breaker_stars", 15);
LiveTuning::Knob const FullMoonSplashShare("druid.full_moon_splash_share", 0.5f);
// a Starsurge: the next Starfall free
LiveTuning::KnobInt const StarweaverStarfallChance("druid.starweaver_starfall_chance", 25);
// a Starfall: the next Starsurge free
LiveTuning::KnobInt const StarweaverStarsurgeChance("druid.starweaver_starsurge_chance", 35);
LiveTuning::Knob const TwinMoonsFactor("druid.twin_moons_factor", 1.1f);
LiveTuning::Knob const TwinMoonsRange("druid.twin_moons_range", 15.0f);
LiveTuning::Knob const SunfireSpreadRange("druid.sunfire_spread_range", 8.0f);
LiveTuning::KnobInt const SunfireSpreadTargets("druid.sunfire_spread_targets", 8);
// Starfall and Fury of Elune: a strike a second
LiveTuning::KnobUInt const StarfallMs("druid.starfall_ms", 8000);
LiveTuning::KnobUInt const StarfallPeriodMs("druid.starfall_period_ms", 1000);
LiveTuning::Knob const StarfallRadius("druid.starfall_radius", 30.0f);
// around the Druid's target, even out of combat
LiveTuning::Knob const StarfallPackRadius("druid.starfall_pack_radius", 12.0f);
LiveTuning::KnobUInt const FuryOfEluneMs("druid.fury_of_elune_ms", 8000);
LiveTuning::KnobUInt const FuryOfElunePeriodMs("druid.fury_of_elune_period_ms", 1000);
LiveTuning::Knob const FuryOfEluneRadius("druid.fury_of_elune_radius", 8.0f);
// Feral (cat)
LiveTuning::KnobInt const TigersFuryEnergy("druid.tigers_fury_energy", 50);
LiveTuning::KnobUInt const BloodtalonsWindowMs("druid.bloodtalons_window_ms", 4000);
LiveTuning::KnobInt const BloodtalonsTechniques("druid.bloodtalons_techniques", 3);
LiveTuning::Knob const BloodtalonsFactor("druid.bloodtalons_factor", 1.3f);
LiveTuning::KnobUInt const BloodtalonsRipMs("druid.bloodtalons_rip_ms", 30000);
LiveTuning::KnobInt const PredatorySwiftnessChance("druid.predatory_swiftness_chance", 20);  // a combo point
LiveTuning::KnobInt const FeralForestEnergy("druid.feral_forest_energy", 5);                 // a combo point
LiveTuning::Knob const PrimalWrathRange("druid.primal_wrath_range", 8.0f);
// of attack power, each enemy
LiveTuning::Knob const PrimalWrathHitPerCombo("druid.primal_wrath_hit_per_combo", 0.05f);
// of a Rip's tick of the same combo points
LiveTuning::Knob const PrimalWrathBleedShare("druid.primal_wrath_bleed_share", 1.0f);
LiveTuning::Knob const FeralFrenzyBleed("druid.feral_frenzy_bleed", 0.06f);  // of attack power, each tick
// Feral (bear)
LiveTuning::KnobInt const LayeredManeChance("druid.layered_mane_chance", 15);        // a rank
LiveTuning::KnobInt const GoreChance("druid.gore_chance", 15);
LiveTuning::KnobInt const GoreRage("druid.gore_rage", 40);                           // tenths
LiveTuning::KnobInt const GalacticGuardianChance("druid.galactic_guardian_chance", 5);
LiveTuning::KnobInt const GalacticGuardianRage("druid.galactic_guardian_rage", 80);  // tenths
// The forms' own share of the damage (the combat bench)
LiveTuning::Knob const CatDamageFactor("druid.cat_damage_factor", 0.8f);
// The cat's pack techniques (Swipe, Thrash, Primal Wrath), on top: the melee reaches fewer of a pack than a caster
LiveTuning::Knob const CatAreaFactor("druid.cat_area_factor", 1.3f);
LiveTuning::Knob const BearDamageFactor("druid.bear_damage_factor", 2.0f);
// The bear's thick hide: what it takes, a share (the reworked Paladin's and Warrior's tanks as the reference)
LiveTuning::Knob const BearDamageTaken("druid.bear_damage_taken", 0.5f);
// Restoration
LiveTuning::KnobUInt const EfflorescenceMs("druid.efflorescence_ms", 30000);
LiveTuning::KnobUInt const EfflorescencePeriodMs("druid.efflorescence_period_ms", 2000);
LiveTuning::Knob const EfflorescenceRadius("druid.efflorescence_radius", 10.0f);
LiveTuning::KnobInt const EfflorescenceTargets("druid.efflorescence_targets", 3);
LiveTuning::KnobUInt const FlourishMs("druid.flourish_ms", 8000);
LiveTuning::Knob const FlourishRange("druid.flourish_range", 60.0f);
LiveTuning::Knob const AbundancePerRejuvenation("druid.abundance_per_rejuvenation", 0.06f);
LiveTuning::KnobInt const AbundanceMax("druid.abundance_max", 5);
constexpr uint32 AbundanceCountMs = 500;
LiveTuning::Knob const RestorationForestFactor("druid.restoration_forest_factor", 2.5f);
LiveTuning::Knob const CultivationFactor("druid.cultivation_factor", 1.4f);
LiveTuning::Knob const CultivationHealthPct("druid.cultivation_health_pct", 60.0f);
// Class tree
LiveTuning::KnobUInt const VortexMs("druid.vortex_ms", 10000);
LiveTuning::KnobUInt const VortexPeriodMs("druid.vortex_period_ms", 1000);
LiveTuning::Knob const VortexRadius("druid.vortex_radius", 8.0f);
constexpr float HealRange = 40.0f;
// Packs (the combat bench, Fire mage as the reference): each area hit whole up to AreaFullTargets enemies and
// sqrt(AreaFullTargets / enemies) of it past them, as the Shaman's and the Warlock's
LiveTuning::KnobInt const AreaFullTargets("druid.area_full_targets", 8);
LiveTuning::KnobInt const AreaMaxTargets("druid.area_max_targets", 20);
constexpr uint32 AreaCountMs = 300;
constexpr uint32 HoldCheckMs = 1000;

// An effect held on a spot of the ground (Efflorescence, Ursol's Vortex) or on a target (Fury of Elune)
struct GroundEffect
{
    uint32 untilMs = 0;
    uint32 nextMs = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    ObjectGuid target;

    bool Active() const { return untilMs != 0; }
    Position Spot() const { return Position(x, y, z); }
    void Start(Position const& spot, uint32 now, uint32 durationMs, uint32 firstTickMs)
    {
        x = spot.GetPositionX();
        y = spot.GetPositionY();
        z = spot.GetPositionZ();
        untilMs = now + durationMs;
        nextMs = now + firstTickMs;
        target.Clear();
    }
    void Follow(Unit const* unit)
    {
        x = unit->GetPositionX();
        y = unit->GetPositionY();
        z = unit->GetPositionZ();
    }
};

// The techniques Bloodtalons counts, the last ones used
struct Technique
{
    uint32 spell = 0;
    uint32 stamp = 0;
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct DruidState : public DataMap::Base
{
    // Balance
    uint8 wrathCasts = 0;
    uint8 starfireCasts = 0;
    uint32 outOfCombatMs = 0;
    uint32 balanceMs = 0;
    GroundEffect starfall;
    GroundEffect furyOfElune;
    uint8 shootingStars = 0;
    // Feral
    uint8 finisherCombo = 0;        // the combo points of the finishing move being cast (OnSpellCheckCast)
    uint32 finisherSpell = 0;
    std::array<Technique, 4> techniques{};
    std::map<ObjectGuid, uint32> bloodyRips;    // Rips cast on Bloodtalons, until when
    // Restoration
    GroundEffect efflorescence;
    ObjectGuid germinationTarget;   // a Rejuvenation cast at an ally who had one (OnSpellCheckCast)
    uint32 abundanceStamp = 0;
    uint8 abundanceCount = 0;
    // Class tree
    GroundEffect vortex;
    uint32 holdTimer = 0;
    // The enemies an area spell reaches, counted a few times a second
    uint32 areaSpell = 0;
    uint32 areaStamp = 0;
    uint8 areaCount = 1;
};

constexpr char const* StateKey = "DruidTalentState";

DruidState* GetState(Player* player)
{
    return player->CustomData.GetDefault<DruidState>(StateKey);
}

Player* Druid(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_DRUID ? player : nullptr;
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsBalance(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_BALANCE);
}

bool IsFeral(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_FERAL);
}

bool IsRestoration(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_RESTORATION);
}

bool InCatForm(Unit const* unit)
{
    return unit->GetShapeshiftForm() == FORM_CAT;
}

bool InBearForm(Unit const* unit)
{
    return unit->GetShapeshiftForm() == FORM_BEAR || unit->GetShapeshiftForm() == FORM_DIREBEAR;
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

uint8 Stacks(Unit const* unit, uint32 spellId)
{
    Aura* aura = unit->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
}

// An aura of stacks on the Druid at a count (none removes it)
void ShowStacks(Player* player, uint32 spellId, uint8 stacks)
{
    if (!stacks)
    {
        player->RemoveAurasDueToSpell(spellId);
        return;
    }
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (!aura)
        return;
    aura->SetStackAmount(stacks);
    aura->RefreshDuration();
}

int32 Amount(float value)
{
    return int32(std::clamp(value, 1.0f, float(std::numeric_limits<int32>::max() / 2)));
}

float AttackPower(Player* player)
{
    return player->GetTotalAttackPowerValue(BASE_ATTACK);
}

// Each hit's share on a pack of `enemies`: whole up to AreaFullTargets, then sqrt(AreaFullTargets / enemies)
float AreaFalloff(std::size_t enemies)
{
    return enemies <= AreaFullTargets ? 1.0f : std::sqrt(float(AreaFullTargets) / float(enemies));
}

// Enemies of the player within range of center, center left out
std::list<Unit*> EnemiesNear(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, targets, check);
    Cell::VisitObjects(center, searcher, range);
    targets.remove_if([player, center](Unit* unit)
    {
        return unit == center || !unit->IsAlive() || !player->IsValidAttackTarget(unit) ||
            unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    });
    return targets;
}

// Enemies within `radius` of a spot (the player's search reaches it), AreaMaxTargets at most
std::vector<Unit*> EnemiesAtSpot(Player* player, Position const& spot, float radius)
{
    std::vector<Unit*> enemies;
    float const reach = player->GetExactDist(&spot) + radius;
    for (Unit* enemy : EnemiesNear(player, player, reach))
    {
        if (enemy->GetExactDist(&spot) > radius + enemy->GetCombatReach())
            continue;
        enemies.push_back(enemy);
        if (enemies.size() >= AreaMaxTargets)
            break;
    }
    return enemies;
}

// The Druid and its group's players, alive, within range of a spot
std::vector<Player*> GroupPlayersAt(Player* player, Position const& spot, float range)
{
    std::vector<Player*> players;
    Group* group = player->GetGroup();
    if (!group)
    {
        if (player->IsAlive() && player->GetExactDist(&spot) <= range)
            players.push_back(player);
        return players;
    }
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->IsInMap(player) && member->GetExactDist(&spot) <= range)
            players.push_back(member);
    }
    return players;
}

// The injured among them, most injured first, `count` at most
std::vector<Player*> InjuredAt(Player* player, Position const& spot, float range, std::size_t count)
{
    std::vector<Player*> injured = GroupPlayersAt(player, spot, range);
    std::erase_if(injured, [](Player* member) { return member->IsFullHealth(); });
    std::sort(injured.begin(), injured.end(), [](Player const* left, Player const* right)
    {
        return left->GetHealthPct() < right->GetHealthPct();
    });
    if (injured.size() > count)
        injured.resize(count);
    return injured;
}

void Strike(Player* player, Unit* target, uint32 spellId, float amount)
{
    if (!target || amount < 1.0f || !target->IsAlive())
        return;
    int32 const damage = Amount(amount);
    player->CastCustomSpell(target, spellId, &damage, nullptr, nullptr, true);
}

// Every rank of a spell the player knows, its cooldown taken back
void ClearChainCooldown(Player* player, uint32 spellId)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->RemoveSpellCooldown(rank, true);
}

// The highest rank of a chain the player knows, 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

bool HasMoonfire(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_DRUID, FlagMoonfire, 0, 0, player->GetGUID());
}

bool HasSunfire(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_DRUID, 0, 0, FlagSunfire, player->GetGUID());
}

bool HasRejuvenation(Player* player, Unit* target)
{
    return target->GetAura(SPELL_GERMINATION, player->GetGUID()) ||
        target->GetAuraEffect(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, FlagRejuvenation, 0, 0, player->GetGUID());
}

// The cat's techniques Bloodtalons counts
bool IsCatTechnique(uint32 id, uint32 firstRank)
{
    switch (firstRank)
    {
        case SPELL_RAKE_R1:
        case SPELL_SHRED_R1:
        case SPELL_CLAW_R1:
        case SPELL_MANGLE_CAT_R1:
        case SPELL_SWIPE_CAT:
            return true;
        default:
            return id == SPELL_THRASH_CAT || id == SPELL_FERAL_FRENZY;
    }
}

// The finishing moves Bloodtalons strengthens
bool IsBloodyFinisher(uint32 id, uint32 firstRank)
{
    return firstRank == SPELL_RIP_R1 || firstRank == SPELL_FEROCIOUS_BITE_R1 || id == SPELL_PRIMAL_WRATH;
}

// --- Astral Power and Eclipse (Balance) ------------------------------------------------------------------------------

int32 AstralPower(Player const* player)
{
    return Stacks(player, SPELL_ASTRAL_POWER);
}

void AddAstralPower(Player* player, int32 amount)
{
    if (!amount || !IsBalance(player))
        return;
    int32 const value = std::clamp(AstralPower(player) + amount, 0, AstralPowerMax);
    ShowStacks(player, SPELL_ASTRAL_POWER, uint8(value));
}

// What a spender costs (nothing with Starweaver's boon)
int32 AstralCost(Player const* player, uint32 id)
{
    if (id == SPELL_STARSURGE)
        return player->HasAura(SPELL_STARWEAVER_STARSURGE) ? 0 : StarsurgeCost;
    if (id == SPELL_STARFALL)
        return player->HasAura(SPELL_STARWEAVER_STARFALL) ? 0 : StarfallCost;
    return 0;
}

bool InEclipse(Player const* player)
{
    return player->HasAura(SPELL_SOLAR_ECLIPSE) || player->HasAura(SPELL_LUNAR_ECLIPSE);
}

void EnterEclipse(Player* player, DruidState* state, uint32 eclipse)
{
    state->wrathCasts = 0;
    state->starfireCasts = 0;
    player->AddAura(eclipse, player);
    if (uint8 const rank = Rank(player, TALENT_BALANCE_FOREST_1, TALENT_BALANCE_FOREST_2))
        AddAstralPower(player, AstralForestPerRank * rank);
}

// Two Wraths bring the Lunar Eclipse, two Starfires the Solar one; counted only out of an Eclipse
void CountEclipse(Player* player, DruidState* state, bool wrath)
{
    if (InEclipse(player))
        return;
    if (wrath)
    {
        state->starfireCasts = 0;
        if (++state->wrathCasts >= EclipseCasts)
            EnterEclipse(player, state, SPELL_LUNAR_ECLIPSE);
    }
    else
    {
        state->wrathCasts = 0;
        if (++state->starfireCasts >= EclipseCasts)
            EnterEclipse(player, state, SPELL_SOLAR_ECLIPSE);
    }
}

// Celestial Alignment and Incarnation: both Eclipses for as long as they last
void AlignEclipses(Player* player, uint32 durationMs)
{
    for (uint32 eclipse : { SPELL_SOLAR_ECLIPSE, SPELL_LUNAR_ECLIPSE })
        if (Aura* aura = player->AddAura(eclipse, player))
        {
            aura->SetMaxDuration(int32(durationMs));
            aura->SetDuration(int32(durationMs));
        }
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

class DruidTalentSpellScript : public AllSpellScript
{
public:
    DruidTalentSpellScript() : AllSpellScript("DruidTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // An Astral Power spender needs its power; a finishing move's combo points and a Rejuvenation's target are kept for
    // the cast's end
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Druid(spell->GetCaster());
        if (!player)
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        DruidState* state = GetState(player);

        if (int32 const cost = AstralCost(player, spellInfo->Id))
            if (AstralPower(player) < cost)
            {
                result = SPELL_FAILED_CASTER_AURASTATE;
                return;
            }

        if (spellInfo->NeedsComboPoints())
        {
            state->finisherSpell = spellInfo->Id;
            state->finisherCombo = player->GetComboPoints();
        }

        if (FirstRank(spellInfo) == SPELL_REJUVENATION_R1)
        {
            state->germinationTarget.Clear();
            Unit* target = spell->m_targets.GetUnitTarget();
            if (target && player->HasAura(TALENT_GERMINATION) && HasRejuvenation(player, target) &&
                !target->GetAura(SPELL_GERMINATION, player->GetGUID()))
                state->germinationTarget = target->GetGUID();
        }
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Druid(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        DruidState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);
        uint32 const now = Now();

        // A finishing move: its combo points, kept as it was checked
        uint8 combo = 0;
        if (spellInfo->NeedsComboPoints() && state->finisherSpell == id)
        {
            combo = state->finisherCombo;
            state->finisherSpell = 0;
            state->finisherCombo = 0;
            Finisher(player, state, combo);
        }

        if (IsFeral(player))
            Bloodtalons(player, state, id, firstRank, target, now);

        switch (id)
        {
            // Class tree
            case SPELL_WILD_CHARGE:
                if (target)
                {
                    if (InBearForm(player))
                        player->CastSpell(target, SPELL_FERAL_CHARGE_BEAR, true);
                    else if (InCatForm(player))
                        player->CastSpell(target, SPELL_FERAL_CHARGE_CAT, true);
                }
                return;
            case SPELL_URSOLS_VORTEX:
                state->vortex.Start(Destination(spell, target, player), now, VortexMs, 0);
                return;
            // Balance
            case SPELL_STARSURGE:
                SpendAstralPower(player, id);
                Starlord(player);
                if (player->HasAura(TALENT_STARWEAVER) && roll_chance_i(StarweaverStarfallChance))
                    player->AddAura(SPELL_STARWEAVER_STARFALL, player);
                return;
            case SPELL_STARFALL:
                SpendAstralPower(player, id);
                Starlord(player);
                state->starfall.Start(*player, now, StarfallMs, StarfallPeriodMs / 2);
                if (player->HasAura(TALENT_STARWEAVER) && roll_chance_i(StarweaverStarsurgeChance))
                    player->AddAura(SPELL_STARWEAVER_STARSURGE, player);
                return;
            case SPELL_SUNFIRE:
                AddAstralPower(player, AstralSunfire);
                if (target && player->HasAura(TALENT_IMPROVED_SUNFIRE))
                    SpreadSunfire(player, target);
                return;
            case SPELL_STELLAR_FLARE:
                AddAstralPower(player, AstralStellarFlare);
                return;
            case SPELL_CELESTIAL_ALIGNMENT:
            case SPELL_INCARNATION_ELUNE:
                AlignEclipses(player, uint32(std::max(spellInfo->GetMaxDuration(), 0)));
                return;
            case SPELL_FURY_OF_ELUNE:
                if (target)
                {
                    state->furyOfElune.Start(*target, now, FuryOfEluneMs, FuryOfElunePeriodMs / 2);
                    state->furyOfElune.target = target->GetGUID();
                }
                return;
            case SPELL_ASTRAL_COMMUNION:
                AddAstralPower(player, AstralCommunion);
                return;
            // Feral
            case SPELL_THRASH_CAT:
                // Thrash gives a combo point, as retail
                if (Unit* victim = target ? target : player->GetVictim())
                    player->AddComboPoints(victim, 1);
                return;
            case SPELL_FERAL_FRENZY:
                if (target)
                    Strike(player, target, SPELL_FERAL_FRENZY_BLEED,
                        AttackPower(player) * FeralFrenzyBleed * CatDamageFactor);
                return;
            case SPELL_PRIMAL_WRATH:
                PrimalWrath(player, target, combo);
                return;
            case SPELL_IRONFUR:
                if (uint8 const rank = Rank(player, TALENT_LAYERED_MANE_1, TALENT_LAYERED_MANE_2))
                    if (roll_chance_i(LayeredManeChance * rank))
                        if (Aura* aura = player->GetAura(SPELL_IRONFUR))
                            aura->ModStackAmount(1);
                return;
            // Restoration
            case SPELL_EFFLORESCENCE:
                state->efflorescence.Start(Destination(spell, target, player), now, EfflorescenceMs, 0);
                return;
            case SPELL_FLOURISH:
                Flourish(player);
                return;
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_WRATH_R1:
                if (IsBalance(player))
                {
                    AddAstralPower(player, AstralWrath);
                    CountEclipse(player, state, true);
                }
                break;
            case SPELL_STARFIRE_R1:
                if (IsBalance(player))
                {
                    AddAstralPower(player, AstralStarfire);
                    CountEclipse(player, state, false);
                }
                break;
            case SPELL_MOONFIRE_R1:
                AddAstralPower(player, AstralMoonfire);
                if (target && player->HasAura(TALENT_TWIN_MOONS))
                    TwinMoon(player, target, id);
                break;
            case SPELL_SWIPE_CAT:
                // Swipe gives a combo point, as retail (Primal Wrath's on a pack)
                if (IsFeral(player))
                    if (Unit* victim = target ? target : player->GetVictim())
                        player->AddComboPoints(victim, 1);
                break;
            case SPELL_TIGERS_FURY_R1:
                if (IsFeral(player))
                    player->ModifyPower(POWER_ENERGY, TigersFuryEnergy);
                break;
            case SPELL_MANGLE_BEAR_R1:
                if (player->HasAura(SPELL_GORE))
                {
                    player->RemoveAurasDueToSpell(SPELL_GORE);
                    player->ModifyPower(POWER_RAGE, GoreRage);
                }
                break;
            case SPELL_RIP_R1:
                if (target)
                {
                    if (player->HasAura(SPELL_BLOODTALONS))
                        state->bloodyRips[target->GetGUID()] = now + BloodtalonsRipMs;
                    else
                        state->bloodyRips.erase(target->GetGUID());
                }
                break;
            case SPELL_REJUVENATION_R1:
                if (!state->germinationTarget.IsEmpty())
                {
                    if (Unit* ally = ObjectAccessor::GetUnit(*player, state->germinationTarget))
                        player->CastSpell(ally, SPELL_GERMINATION, true);
                    state->germinationTarget.Clear();
                }
                break;
            case SPELL_REGROWTH_R1:
                player->RemoveAurasDueToSpell(SPELL_RESTORATION_FOREST);
                break;
            default:
                break;
        }

        if (id == SPELL_SWIFTMEND && player->HasAura(TALENT_RESTORATION_FOREST))
            player->AddAura(SPELL_RESTORATION_FOREST, player);

        // Bloodtalons is spent by the finishing move it strengthened
        if (IsBloodyFinisher(id, firstRank))
            if (uint8 const stacks = Stacks(player, SPELL_BLOODTALONS))
                ShowStacks(player, SPELL_BLOODTALONS, stacks - 1);
    }

private:
    // Where a spell aimed at a spot was cast (its target's feet, else the Druid's)
    static Position Destination(Spell* spell, Unit* target, Player* player)
    {
        if (spell->m_targets.HasDst())
            return Position(*spell->m_targets.GetDstPos());
        return target ? Position(*target) : Position(*player);
    }

    static void SpendAstralPower(Player* player, uint32 id)
    {
        if (id == SPELL_STARSURGE && player->HasAura(SPELL_STARWEAVER_STARSURGE))
        {
            player->RemoveAurasDueToSpell(SPELL_STARWEAVER_STARSURGE);
            return;
        }
        if (id == SPELL_STARFALL && player->HasAura(SPELL_STARWEAVER_STARFALL))
        {
            player->RemoveAurasDueToSpell(SPELL_STARWEAVER_STARFALL);
            return;
        }
        AddAstralPower(player, -AstralCost(player, id));
    }

    // Seigneur des étoiles: 3% haste a stack, 3 at most
    static void Starlord(Player* player)
    {
        if (!player->HasAura(TALENT_STARLORD))
            return;
        ShowStacks(player, SPELL_STARLORD, uint8(std::min<uint32>(Stacks(player, SPELL_STARLORD) + 1u, 3u)));
    }

    // Éclat solaire amélioré: Sunfire on the enemies within 8 yd of its target
    static void SpreadSunfire(Player* player, Unit* target)
    {
        uint8 spread = 0;
        for (Unit* enemy : EnemiesNear(player, target, SunfireSpreadRange))
        {
            if (spread >= SunfireSpreadTargets)
                break;
            player->CastSpell(enemy, SPELL_SUNFIRE, true);
            ++spread;
        }
    }

    // Lunes jumelles: Moonfire on the closest enemy near the target that has none of the Druid's
    static void TwinMoon(Player* player, Unit* target, uint32 rank)
    {
        Unit* closest = nullptr;
        for (Unit* enemy : EnemiesNear(player, target, TwinMoonsRange))
            if (!HasMoonfire(player, enemy) &&
                (!closest || target->GetExactDist(enemy) < target->GetExactDist(closest)))
                closest = enemy;
        if (closest)
            player->CastSpell(closest, rank, true);
    }

    // A finishing move's combo points: Predatory Swiftness, Soul of the Forest
    static void Finisher(Player* player, DruidState* /*state*/, uint8 combo)
    {
        if (!combo)
            return;
        if (player->HasAura(TALENT_PREDATORY_SWIFTNESS) && roll_chance_i(PredatorySwiftnessChance * combo))
            player->AddAura(SPELL_PREDATORY_SWIFTNESS, player);
        if (player->HasAura(TALENT_FERAL_FOREST))
            player->ModifyPower(POWER_ENERGY, FeralForestEnergy * combo);
    }

    // Griffes sanglantes: three different techniques within 4 s, the next 2 finishing moves 30% stronger
    static void Bloodtalons(Player* player, DruidState* state, uint32 id, uint32 firstRank, Unit* /*target*/,
        uint32 now)
    {
        if (!player->HasAura(TALENT_BLOODTALONS) || !IsCatTechnique(id, firstRank))
            return;
        uint32 const technique = firstRank ? firstRank : id;
        // The oldest slot (or this technique's) takes it
        Technique* slot = &state->techniques[0];
        for (Technique& entry : state->techniques)
        {
            if (entry.spell == technique)
            {
                slot = &entry;
                break;
            }
            if (entry.stamp < slot->stamp)
                slot = &entry;
        }
        slot->spell = technique;
        slot->stamp = now;

        uint8 recent = 0;
        for (Technique const& entry : state->techniques)
            if (entry.spell && now - entry.stamp <= BloodtalonsWindowMs)
                ++recent;
        if (recent < BloodtalonsTechniques)
            return;
        state->techniques.fill(Technique());
        ShowStacks(player, SPELL_BLOODTALONS, 2);
    }

    // Courroux primordial: the enemies within 8 yd struck and bled by the combo points spent (a Rip's tick of them,
    // a share)
    static void PrimalWrath(Player* player, Unit* target, uint8 combo)
    {
        if (!combo)
            return;
        float const attackPower = AttackPower(player);
        float factor = CatDamageFactor * CatAreaFactor;
        if (player->HasAura(SPELL_BLOODTALONS))
            factor *= BloodtalonsFactor;
        float const hit = attackPower * PrimalWrathHitPerCombo * float(combo) * factor;
        float const ripTick = (36.0f + 93.0f * float(combo) + 0.01f * float(combo) * attackPower) *
            PrimalWrathBleedShare * factor;

        std::vector<Unit*> enemies;
        if (target && target->IsAlive())
            enemies.push_back(target);
        for (Unit* enemy : EnemiesNear(player, player, PrimalWrathRange))
            if (enemy != target && enemies.size() < AreaMaxTargets)
                enemies.push_back(enemy);
        float const falloff = AreaFalloff(enemies.size());
        for (Unit* enemy : enemies)
        {
            Strike(player, enemy, SPELL_PRIMAL_WRATH_HIT, hit * falloff);
            Strike(player, enemy, SPELL_PRIMAL_WRATH_BLEED, ripTick * falloff);
        }
    }

    // Épanouissement: the Druid's heals over time on the group within 60 yd last 8 s longer
    static void Flourish(Player* player)
    {
        for (Player* member : GroupPlayersAt(player, *player, FlourishRange))
            for (AuraEffect* effect : member->GetAuraEffectsByType(SPELL_AURA_PERIODIC_HEAL))
            {
                if (effect->GetCasterGUID() != player->GetGUID() ||
                    effect->GetSpellInfo()->SpellFamilyName != SPELLFAMILY_DRUID)
                    continue;
                Aura* aura = effect->GetBase();
                aura->SetMaxDuration(aura->GetMaxDuration() + int32(FlourishMs));
                aura->SetDuration(aura->GetDuration() + int32(FlourishMs));
            }
    }
};

// --- Damage and healing ----------------------------------------------------------------------------------------------

// The area spells whose hits fall off past eight enemies (Primal Wrath's are shared out as it strikes)
bool IsAreaSpell(uint32 id, uint32 firstRank)
{
    switch (id)
    {
        case SPELL_STARFALL_HIT:
        case SPELL_FURY_OF_ELUNE_HIT:
        case SPELL_THRASH_CAT:
        case SPELL_THRASH_BEAR:
        case SPELL_SWIPE_CAT:
            return true;
        default:
            return firstRank == SPELL_SWIPE_BEAR_R1;
    }
}

// The cat's and the bear's own spells (their damage takes the forms' share)
bool IsFeralSpell(SpellInfo const* spellInfo)
{
    return spellInfo->SpellFamilyName == SPELLFAMILY_DRUID && spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE;
}

class DruidTalentUnitScript : public UnitScript
{
public:
    DruidTalentUnitScript() : UnitScript("DruidTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Protection cénarienne: the first damage its ally takes puts its heal over time on it
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!victim || !damage || !victim->HasAura(SPELL_CENARION_WARD))
            return;
        Aura* ward = victim->GetAura(SPELL_CENARION_WARD);
        Player* player = ward ? Druid(ward->GetCaster()) : nullptr;
        victim->RemoveAurasDueToSpell(SPELL_CENARION_WARD);
        if (player && victim->IsAlive())
            player->CastSpell(victim, SPELL_CENARION_WARD_HEAL, true);
    }

    // The forms' share of the swings; Galactic Guardian from the bear's swings; the bear takes less
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (Player* bear = Druid(target); bear && damage && IsFeral(bear) && InBearForm(bear))
            damage = uint32(float(damage) * BearDamageTaken);
        Player* player = Druid(attacker);
        if (!player || !target || !damage || !IsFeral(player))
            return;
        if (InCatForm(player))
            damage = uint32(float(damage) * CatDamageFactor);
        else if (InBearForm(player))
        {
            damage = uint32(float(damage) * BearDamageFactor);
            GalacticGuardian(player, target);
        }
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || damage <= 0)
            return;
        if (Player* bear = Druid(target); bear && IsFeral(bear) && InBearForm(bear))
            damage = int32(float(damage) * BearDamageTaken);
        Player* player = Druid(attacker);
        if (!player)
            return;
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = 1.0f;
        if (firstRank == SPELL_FEROCIOUS_BITE_R1 && player->HasAura(SPELL_BLOODTALONS))
            factor *= BloodtalonsFactor;
        if (firstRank == SPELL_MOONFIRE_R1 && player->HasAura(TALENT_TWIN_MOONS))
            factor *= TwinMoonsFactor;
        if (IsFeral(player) && IsFeralSpell(spellInfo) && id != SPELL_PRIMAL_WRATH_HIT)
            factor *= InBearForm(player) ? BearDamageFactor : CatDamageFactor;
        if (IsFeral(player) && InCatForm(player) && (id == SPELL_THRASH_CAT || firstRank == SPELL_SWIPE_CAT))
            factor *= CatAreaFactor;
        if (IsAreaSpell(id, firstRank))
            factor *= AreaFalloff(AreaCount(player, spellInfo, target));
        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // Shooting stars from Moonfire's and Sunfire's ticks; the Rips cast on Bloodtalons; the bleeds' forms' share
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !damage || spellInfo->SpellFamilyName != SPELLFAMILY_DRUID ||
            !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE))
            return;
        Player* player = Druid(attacker);
        if (!player)
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        if ((firstRank == SPELL_MOONFIRE_R1 || spellInfo->Id == SPELL_SUNFIRE) &&
            player->HasAura(TALENT_SHOOTING_STARS) && roll_chance_i(ShootingStarChance))
            ShootingStar(player, target);

        float factor = 1.0f;
        if (firstRank == SPELL_RIP_R1)
        {
            DruidState* state = GetState(player);
            auto const bloody = state->bloodyRips.find(target->GetGUID());
            if (bloody != state->bloodyRips.end())
            {
                if (Now() <= bloody->second)
                    factor *= BloodtalonsFactor;
                else
                    state->bloodyRips.erase(bloody);
            }
        }
        if (firstRank == SPELL_MOONFIRE_R1 && player->HasAura(TALENT_TWIN_MOONS))
            factor *= TwinMoonsFactor;
        if (IsFeral(player) && spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE &&
            spellInfo->Id != SPELL_PRIMAL_WRATH_BLEED && spellInfo->Id != SPELL_FERAL_FRENZY_BLEED)
            factor *= InBearForm(player) ? BearDamageFactor : CatDamageFactor;
        if (factor != 1.0f)
            damage = uint32(float(damage) * factor);
    }

    // Abundance and Soul of the Forest strengthen Regrowth; Cultivation the Rejuvenations on the badly hurt
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !heal)
            return;
        Player* player = Druid(healer);
        if (!player)
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = 1.0f;
        if (firstRank == SPELL_REGROWTH_R1)
        {
            if (player->HasAura(TALENT_ABUNDANCE))
                factor *= 1.0f + AbundancePerRejuvenation * float(Rejuvenations(player));
            if (player->HasAura(SPELL_RESTORATION_FOREST))
                factor *= RestorationForestFactor;
        }
        if ((firstRank == SPELL_REJUVENATION_R1 || spellInfo->Id == SPELL_GERMINATION) &&
            player->HasAura(TALENT_CULTIVATION) && target->GetHealthPct() < CultivationHealthPct)
            factor *= CultivationFactor;
        if (factor != 1.0f)
            heal = uint32(float(heal) * factor);
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = Druid(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        // Starfire splashes the enemies near its target, wider in the Lunar Eclipse
        if (firstRank == SPELL_STARFIRE_R1 && IsBalance(player))
        {
            float const share = player->HasAura(SPELL_LUNAR_ECLIPSE) ? StarfireLunarSplashShare : StarfireSplashShare;
            Splash(player, victim, float(damage) * share, StarfireSplashRange);
            return;
        }
        if (id == SPELL_FULL_MOON)
        {
            Splash(player, victim, float(damage) * FullMoonSplashShare, StarfireSplashRange);
            return;
        }

        // The bear: Gore and Galactic Guardian
        if (!InBearForm(player))
            return;
        if (player->HasAura(TALENT_GORE) &&
            (id == SPELL_THRASH_BEAR || firstRank == SPELL_SWIPE_BEAR_R1 || firstRank == SPELL_MAUL_R1 ||
             firstRank == SPELL_MOONFIRE_R1) && roll_chance_i(GoreChance))
        {
            player->AddAura(SPELL_GORE, player);
            ClearChainCooldown(player, SPELL_MANGLE_BEAR_R1);
        }
        if (IsFeralSpell(spellInfo))
            GalacticGuardian(player, victim);
    }

private:
    // A relay of a share of a hit to the enemies near its target, shared out on a big pack
    static void Splash(Player* player, Unit* victim, float amount, float range)
    {
        std::list<Unit*> enemies = EnemiesNear(player, victim, range);
        if (enemies.empty())
            return;
        float const each = amount * AreaFalloff(enemies.size() + 1);
        uint8 hit = 0;
        for (Unit* enemy : enemies)
        {
            if (hit++ >= AreaMaxTargets)
                break;
            Strike(player, enemy, SPELL_STARFIRE_SPLASH, each);
        }
    }

    // Étoiles filantes: a shooting star on the target; Briseur d'orbite counts them for its full moon
    static void ShootingStar(Player* player, Unit* target)
    {
        player->CastSpell(target, SPELL_SHOOTING_STAR, true);
        AddAstralPower(player, AstralShootingStar);
        if (!player->HasAura(TALENT_ORBIT_BREAKER))
            return;
        DruidState* state = GetState(player);
        if (++state->shootingStars < OrbitBreakerStars)
            return;
        state->shootingStars = 0;
        player->CastSpell(target, SPELL_FULL_MOON, true);
    }

    // Gardien galactique: now and then a free Moonfire and some rage
    static void GalacticGuardian(Player* player, Unit* target)
    {
        if (!player->HasAura(TALENT_GALACTIC_GUARDIAN) || !roll_chance_i(GalacticGuardianChance))
            return;
        if (uint32 const moonfire = KnownRank(player, SPELL_MOONFIRE_R1))
            player->CastSpell(target, moonfire, true);
        player->ModifyPower(POWER_RAGE, GalacticGuardianRage);
    }

    // The Druid's Rejuvenations on its group (Abundance), counted twice a second
    static uint8 Rejuvenations(Player* player)
    {
        DruidState* state = GetState(player);
        uint32 const now = Now();
        if (now - state->abundanceStamp <= AbundanceCountMs)
            return state->abundanceCount;
        state->abundanceStamp = now;
        uint8 count = 0;
        for (Player* member : GroupPlayersAt(player, *player, 100.0f))
            if (member->GetAuraEffect(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, FlagRejuvenation, 0, 0,
                    player->GetGUID()))
                ++count;
        state->abundanceCount = std::min(count, uint8(AbundanceMax));
        return state->abundanceCount;
    }

    // The enemies around the target an area spell reaches (the target included), counted a few times a second
    static uint8 AreaCount(Player* player, SpellInfo const* spellInfo, Unit* target)
    {
        DruidState* state = GetState(player);
        uint32 const now = Now();
        if (state->areaSpell == spellInfo->Id && now - state->areaStamp <= AreaCountMs)
            return state->areaCount;
        float radius = spellInfo->Effects[EFFECT_0].CalcRadius(player);
        if (radius <= 0.0f)
            radius = FuryOfEluneRadius;
        state->areaSpell = spellInfo->Id;
        state->areaStamp = now;
        state->areaCount = uint8(std::min<std::size_t>(EnemiesNear(player, target, radius).size() + 1,
            std::numeric_limits<uint8>::max()));
        return state->areaCount;
    }
};

// --- Every update: the effects held on the ground, Astral Power out of combat ----------------------------------------

class DruidTalentPlayerScript : public PlayerScript
{
public:
    DruidTalentPlayerScript() : PlayerScript("DruidTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_DRUID || !player->IsInWorld())
            return;
        DruidState* state = GetState(player);
        uint32 const now = Now();

        UpdateStarfall(player, state, now);
        UpdateFuryOfElune(player, state, now);
        UpdateEfflorescence(player, state, now);
        UpdateVortex(player, state, now);

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        uint32 const elapsed = state->holdTimer;
        state->holdTimer = 0;
        UpdateAstralPower(player, state, elapsed);
        UpdateHolds(player, state, now);
    }

private:
    // Météores: every second, the enemies within 30 yd of the Druid that fight (and the pack around its target)
    static void UpdateStarfall(Player* player, DruidState* state, uint32 now)
    {
        GroundEffect& starfall = state->starfall;
        if (!starfall.Active() || now < starfall.nextMs)
            return;
        if (now > starfall.untilMs + StarfallPeriodMs / 2 || !player->IsAlive())
        {
            starfall.untilMs = 0;
            return;
        }
        starfall.nextMs += StarfallPeriodMs;
        Unit* victim = player->GetVictim();
        uint8 hit = 0;
        for (Unit* enemy : EnemiesNear(player, player, StarfallRadius))
        {
            if (hit >= AreaMaxTargets)
                break;
            bool const pack = victim && (enemy == victim || enemy->GetExactDist(victim) <= StarfallPackRadius);
            if (!enemy->IsInCombat() && !pack)
                continue;
            player->CastSpell(enemy, SPELL_STARFALL_HIT, true);
            ++hit;
        }
    }

    // Furie d'Élune: every second, the enemies within 8 yd of its target (its last spot once it is gone)
    static void UpdateFuryOfElune(Player* player, DruidState* state, uint32 now)
    {
        GroundEffect& fury = state->furyOfElune;
        if (!fury.Active() || now < fury.nextMs)
            return;
        if (now > fury.untilMs + FuryOfElunePeriodMs / 2 || !player->IsAlive())
        {
            fury.untilMs = 0;
            return;
        }
        fury.nextMs += FuryOfElunePeriodMs;
        if (Unit* target = ObjectAccessor::GetUnit(*player, fury.target))
            if (target->IsAlive())
                fury.Follow(target);
        std::vector<Unit*> const enemies = EnemiesAtSpot(player, fury.Spot(), FuryOfEluneRadius);
        for (Unit* enemy : enemies)
            player->CastSpell(enemy, SPELL_FURY_OF_ELUNE_HIT, true);
        if (!enemies.empty())
            AddAstralPower(player, AstralFuryOfElune);
    }

    // Efflorescence: every 2 s, the 3 most injured allies within 10 yd of its spot
    static void UpdateEfflorescence(Player* player, DruidState* state, uint32 now)
    {
        GroundEffect& bloom = state->efflorescence;
        if (!bloom.Active() || now < bloom.nextMs)
            return;
        if (now > bloom.untilMs || !player->IsAlive())
        {
            bloom.untilMs = 0;
            return;
        }
        bloom.nextMs += EfflorescencePeriodMs;
        for (Player* member : InjuredAt(player, bloom.Spot(), EfflorescenceRadius, EfflorescenceTargets))
            player->CastSpell(member, SPELL_EFFLORESCENCE_HEAL, true);
    }

    // Vortex d'Ursol: every second, the enemies within 8 yd of its spot are slowed
    static void UpdateVortex(Player* player, DruidState* state, uint32 now)
    {
        GroundEffect& vortex = state->vortex;
        if (!vortex.Active() || now < vortex.nextMs)
            return;
        if (now > vortex.untilMs || !player->IsAlive())
        {
            vortex.untilMs = 0;
            return;
        }
        vortex.nextMs += VortexPeriodMs;
        for (Unit* enemy : EnemiesAtSpot(player, vortex.Spot(), VortexRadius))
            player->CastSpell(enemy, SPELL_VORTEX_SNARE, true);
    }

    // Astral Power: Balance's only, a point a second with Équilibre de la nature in combat, gone 15 s after combat
    static void UpdateAstralPower(Player* player, DruidState* state, uint32 elapsed)
    {
        if (!IsBalance(player))
        {
            for (uint32 aura :
                     { SPELL_ASTRAL_POWER, SPELL_STARLORD, SPELL_STARWEAVER_STARFALL, SPELL_STARWEAVER_STARSURGE })
                player->RemoveAurasDueToSpell(aura);
            state->wrathCasts = 0;
            state->starfireCasts = 0;
            return;
        }
        if (player->IsInCombat())
        {
            state->outOfCombatMs = 0;
            if (player->HasAura(TALENT_NATURES_BALANCE))
            {
                state->balanceMs += elapsed;
                int32 const points = int32(state->balanceMs / NaturesBalancePeriodMs);
                state->balanceMs %= NaturesBalancePeriodMs;
                AddAstralPower(player, points);
            }
            return;
        }
        if (!player->HasAura(SPELL_ASTRAL_POWER))
        {
            state->outOfCombatMs = 0;
            return;
        }
        state->outOfCombatMs += elapsed;
        if (state->outOfCombatMs < AstralFadeMs)
            return;
        state->outOfCombatMs = 0;
        player->RemoveAurasDueToSpell(SPELL_ASTRAL_POWER);
    }

    // Croissance exubérante holds the Glyph of Wild Growth's aura (Wild Growth's script reads it for its sixth target);
    // a glyph in a slot keeps it. The Rips cast on Bloodtalons are forgotten once they are over.
    static void UpdateHolds(Player* player, DruidState* state, uint32 now)
    {
        bool const exuberant = player->HasAura(TALENT_EXUBERANT_GROWTH);
        if (exuberant && !player->HasAura(SPELL_GLYPH_OF_WILD_GROWTH))
            player->AddAura(SPELL_GLYPH_OF_WILD_GROWTH, player);
        else if (!exuberant && player->HasAura(SPELL_GLYPH_OF_WILD_GROWTH) &&
                 !HasGlyph(player, SPELL_GLYPH_OF_WILD_GROWTH))
            player->RemoveAurasDueToSpell(SPELL_GLYPH_OF_WILD_GROWTH);

        for (auto itr = state->bloodyRips.begin(); itr != state->bloodyRips.end();)
            itr = now > itr->second ? state->bloodyRips.erase(itr) : std::next(itr);
    }

    static bool HasGlyph(Player* player, uint32 spellId)
    {
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
            if (uint32 const glyph = player->GetGlyph(slot))
                if (GlyphPropertiesEntry const* properties = sGlyphPropertiesStore.LookupEntry(glyph))
                    if (properties->SpellId == spellId)
                        return true;
        return false;
    }
};
}

void AddDruidTalentScripts()
{
    new DruidTalentSpellScript();
    new DruidTalentUnitScript();
    new DruidTalentPlayerScript();
}
