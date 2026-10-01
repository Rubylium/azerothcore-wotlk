#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "DynamicObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "LiveTuning.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The Hunter's talents and abilities on the retail-style trees (localTools/hunter/talentTree.json) that spell data
// cannot carry. A talent is its rank spell's aura on the Hunter (learned by mod-custom-classes' TalentTree.cpp), read
// here with HasAura; a specialization is its passive (Maître des bêtes, Tireur d'élite, Survivant). The abilities and
// auras named below are in localTools/hunter/Spells.ps1, the stock spells changed in place in StockSpells.ps1.
//
// The Hunter's power is Focus (power type 2, 100 at most): the core regenerates it (Player::Regenerate) and the spells
// cost it; this module hands out what the generators give back (Steady Shot, Kill Command in Survival, Rapid Fire, the
// chakram). Kill Command, Barbed Shot, Aimed Shot, Wildfire Bomb and Butchery have charges kept here, shown as stacks.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_TURTLE                   = 93104,    // Aspect de la tortue
    TALENT_MARKED_PREY              = 93105,    // Marque de la proie
    TALENT_PACK_BOND_1              = 93110,    // Liens de la meute
    TALENT_PACK_BOND_2              = 93111,
    TALENT_DEATHBLOW                = 93112,    // Coup fatal
    TALENT_ALPHA_PREDATOR           = 93115,    // Prédateur alpha
    TALENT_BEAST_CLEAVE_1           = 93116,    // Fendoir de la bête
    TALENT_BEAST_CLEAVE_2           = 93117,
    TALENT_BARBED_RELOAD            = 93118,    // Rechargement barbelé
    TALENT_IMPROVED_CLEAVE          = 93119,    // Fendoir amélioré
    TALENT_COBRA_VENOM_1            = 93120,    // Venin du cobra
    TALENT_COBRA_VENOM_2            = 93121,
    TALENT_KILL_CLEAVE              = 93122,    // Ordre sauvage
    TALENT_WILD_CALL                = 93123,    // Appel sauvage
    TALENT_STOMP                    = 93124,    // Piétinement
    TALENT_DIRE_BEASTS_1            = 93127,    // Bêtes redoutables
    TALENT_DIRE_BEASTS_2            = 93128,
    TALENT_BLOODY_FRENZY_1          = 93129,    // Frénésie sanglante
    TALENT_BLOODY_FRENZY_2          = 93130,
    TALENT_KILLER_COBRA             = 93131,    // Cobra meurtrier
    TALENT_BRUTAL_COMPANION         = 93132,    // Compagnon brutal
    TALENT_PRECISE_SHOTS            = 93133,    // Tirs précis
    TALENT_STEADY_FOCUS_1           = 93134,    // Tir assuré
    TALENT_STEADY_FOCUS_2           = 93135,
    TALENT_LONE_WOLF                = 93136,    // Loup solitaire
    TALENT_TRICK_SHOTS              = 93137,    // Tirs de ricochet
    TALENT_STREAMLINE_1             = 93138,    // Rafale rationalisée
    TALENT_STREAMLINE_2             = 93139,
    TALENT_VOLLEY_RICOCHET          = 93140,    // Salve de ricochets
    TALENT_LOCK_AND_LOAD            = 93143,    // Chargement éclair
    TALENT_WINDRUNNERS              = 93144,    // Héritage des Coursevent
    TALENT_DEADLY_SALVO_1           = 93145,    // Salve meurtrière
    TALENT_DEADLY_SALVO_2           = 93146,
    TALENT_CAREFUL_AIM_1            = 93147,    // Visée mortelle
    TALENT_CAREFUL_AIM_2            = 93148,
    TALENT_IMPROVED_PRECISE_1       = 93151,    // Tirs précis améliorés
    TALENT_IMPROVED_PRECISE_2       = 93152,
    TALENT_CALLING_THE_SHOTS        = 93153,    // Appel des tirs
    TALENT_TIP_OF_THE_SPEAR         = 93154,    // Pointe de la lance
    TALENT_GUERRILLA                = 93155,    // Tactique de guérilla
    TALENT_TERMS_OF_ENGAGEMENT      = 93156,    // Termes de l'engagement
    TALENT_VIPER_BITE               = 93157,    // Morsure de vipère
    TALENT_MONGOOSE_FURY            = 93158,    // Furie de la mangouste
    TALENT_EXPLOSIVES_1             = 93159,    // Poudre explosive
    TALENT_EXPLOSIVES_2             = 93160,
    TALENT_BLOODSEEKER              = 93161,    // Lance sanglante
    TALENT_BOMBARDIER               = 93162,    // Bombardier
    TALENT_FRENZY_STRIKES_1         = 93163,    // Frappes frénétiques
    TALENT_FRENZY_STRIKES_2         = 93164,
    TALENT_DEADLY_DUO               = 93167,    // Duo mortel
    TALENT_INFUSED_BOMBS            = 93168,    // Bombes infusées
    TALENT_PACK_INSTINCT_1          = 93171,    // Instinct de meute
    TALENT_PACK_INSTINCT_2          = 93172,
    TALENT_VIPERS_VENOM             = 93173,    // Venin de vipère
    TALENT_COORDINATED_KILL         = 93174,    // Mort coordonnée
};

// The abilities and auras of localTools/hunter/Spells.ps1
enum Spells : uint32
{
    SPELL_COUNTER_SHOT              = 93200,
    SPELL_EXHILARATION              = 93201,
    SPELL_BINDING_SHOT              = 93203,
    SPELL_BINDING_STUN              = 93204,
    SPELL_CAMOUFLAGE                = 93205,
    SPELL_TURTLE                    = 93206,
    SPELL_DEATHBLOW                 = 93208,
    SPELL_DEATH_CHAKRAM             = 93209,
    SPELL_DEATH_CHAKRAM_HIT         = 93210,
    SPELL_STAMPEDE                  = 93211,
    SPELL_STAMPEDE_HIT              = 93212,
    SPELL_BARBED_SHOT               = 93220,
    SPELL_FRENZY                    = 93221,
    SPELL_BARBED_FOCUS              = 93222,
    SPELL_COBRA_SHOT                = 93223,
    SPELL_KILL_COMMAND_HIT          = 93224,
    SPELL_DIRE_BEAST                = 93225,
    SPELL_DIRE_BEAST_FOCUS          = 93226,
    SPELL_BEAST_CLEAVE              = 93227,
    SPELL_BEAST_CLEAVE_HIT          = 93228,
    SPELL_ASPECT_OF_THE_BEAST       = 93229,
    SPELL_CALL_OF_THE_WILD          = 93230,
    SPELL_BLOODSHED                 = 93231,
    SPELL_BLOODSHED_BLEED           = 93232,
    SPELL_STOMP                     = 93233,
    SPELL_KILL_COMMAND_CHARGES      = 93234,
    SPELL_BARBED_SHOT_CHARGES       = 93235,
    SPELL_BRUTAL_COMPANION_HIT      = 93236,
    SPELL_RAPID_FIRE                = 93250,
    SPELL_RAPID_FIRE_SHOT           = 93251,
    SPELL_PRECISE_SHOTS             = 93252,
    SPELL_TRICK_SHOTS               = 93253,
    SPELL_TRICK_SHOTS_HIT           = 93254,
    SPELL_STEADY_FOCUS              = 93255,
    SPELL_LONE_WOLF                 = 93256,
    SPELL_VOLLEY                    = 93257,
    SPELL_TRUESHOT                  = 93258,
    SPELL_LOCK_AND_LOAD             = 93259,
    SPELL_STREAMLINE                = 93260,
    SPELL_DOUBLE_TAP                = 93261,
    SPELL_WAILING_ARROW             = 93262,
    SPELL_WAILING_SILENCE           = 93263,
    SPELL_WAILING_SPLASH            = 93264,
    SPELL_WIND_ARROW                = 93265,
    SPELL_AIMED_SHOT_CHARGES        = 93266,
    SPELL_WILDFIRE_BOMB             = 93280,
    SPELL_WILDFIRE_SPLASH           = 93281,
    SPELL_WILDFIRE_BURN             = 93282,
    SPELL_HARPOON                   = 93283,
    SPELL_BUTCHERY                  = 93284,
    SPELL_CARVE                     = 93285,
    SPELL_COORDINATED_ASSAULT       = 93286,
    SPELL_TIP_OF_THE_SPEAR          = 93287,
    SPELL_MONGOOSE_FURY             = 93288,
    SPELL_FLANKING_STRIKE           = 93289,
    SPELL_FLANKING_STRIKE_PET       = 93290,
    SPELL_SPEARHEAD                 = 93291,
    SPELL_SPEARHEAD_BLEED           = 93292,
    SPELL_SPEARHEAD_CRIT            = 93293,
    SPELL_WILDFIRE_CHARGES          = 93296,
    SPELL_BUTCHERY_CHARGES          = 93297,
    SPELL_BLOODSEEKER_BLEED         = 93298,
    SPELL_INFUSED_SLOW              = 93299,
    SPELL_TERMS_HIT                 = 93300,
    SPELL_SERPENT_STING_MELEE       = 93301,

    // The specializations' passives
    SPELL_SPEC_BEAST_MASTERY        = 93380,
    SPELL_SPEC_MARKSMANSHIP         = 93381,
    SPELL_SPEC_SURVIVAL             = 93382,

    // Stock (first ranks)
    SPELL_ARCANE_SHOT_R1            = 3044,
    SPELL_MULTI_SHOT_R1             = 2643,
    SPELL_STEADY_SHOT_R1            = 56641,
    SPELL_AIMED_SHOT_R1             = 19434,
    SPELL_KILL_SHOT_R1              = 53351,
    SPELL_RAPTOR_STRIKE_R1          = 2973,
    SPELL_SERPENT_STING_R1          = 1978,
    SPELL_KILL_COMMAND              = 34026,
    SPELL_BESTIAL_WRATH             = 19574,
    SPELL_DETERRENCE                = 19263,
    SPELL_AUTO_SHOT                 = 75,
    SPELL_PET_CHARGE                = 61685,
};

// The wild beast of Bête sauvage and Appel de la nature sauvage (modules/mod-hunter SQL), for 8 s: an allied guardian
// of the Hunter's (summon properties 61: category ally, type guardian). Force of Nature's 1562 is in the pet category,
// which makes it a controllable guardian - the owner's pet slot - and sent the Hunter's own pet away.
constexpr uint32 NpcWildBeast = 93240;
constexpr uint32 WildBeastSummonProperties = 61;
LiveTuning::KnobUInt const WildBeastMs("hunter.wild_beast_ms", 8000);

// --- Tuning (README.md): shares of the Hunter's ranged attack power ------------------------------------------------
LiveTuning::Knob const KillCommandPower("hunter.kill_command_power", 1.25f);         // the pet's Kill Command hit
LiveTuning::Knob const BeastHitPower("hunter.beast_hit_power", 0.35f);               // a wild beast's melee swing (2 s)
LiveTuning::Knob const StompPower("hunter.stomp_power", 0.35f);                      // Piétinement, each enemy
LiveTuning::Knob const BrutalCompanionShare("hunter.brutal_companion_share", 1.5f);  // of a Kill Command hit
LiveTuning::Knob const BloodshedTickPower("hunter.bloodshed_tick_power", 0.18f);     // each 2 s of Effusion de sang
// Bombe de feu sauvage on each other enemy
LiveTuning::Knob const WildfireSplashPower("hunter.wildfire_splash_power", 0.8f);
LiveTuning::Knob const FlankingPetPower("hunter.flanking_pet_power", 1.0f);  // the pet's half of Frappe de flanc
LiveTuning::Knob const SpearheadTickPower("hunter.spearhead_tick_power", 0.2f);      // each 2 s of Fer de lance
LiveTuning::Knob const TermsPower("hunter.terms_power", 0.6f);                       // Termes de l'engagement's hit
LiveTuning::Knob const WailingSplashShare("hunter.wailing_splash_share", 0.4f);
LiveTuning::Knob const TrickShotsShare("hunter.trick_shots_share", 0.55f);
LiveTuning::Knob const WindArrowShare("hunter.wind_arrow_share", 0.2f);
LiveTuning::Knob const KillCleaveShare("hunter.kill_cleave_share", 0.8f);
// Packs (the combat bench, Fire mage as the reference): Multi-Shot reaches MultiShotMaxTargets (its chain, in
// SpellInfoCorrections.cpp), each hit MultiShotHitFactor of its own and, past AreaFullTargets enemies,
// sqrt(AreaFullTargets / enemies) of that - Beast Cleave too. Its old 3 and Beast Cleave's 6 left the Hunter's
// damage flat from five enemies on.
LiveTuning::KnobInt const MultiShotMaxTargets("hunter.multi_shot_max_targets", 12);
LiveTuning::Knob const MultiShotHitFactor("hunter.multi_shot_hit_factor", 0.85f);
LiveTuning::KnobInt const AreaFullTargets("hunter.area_full_targets", 5);
LiveTuning::KnobInt const BeastCleaveMaxTargets("hunter.beast_cleave_max_targets", 11);  // besides the pet's own target
LiveTuning::KnobInt const TrickShotsMaxTargets("hunter.trick_shots_max_targets", 7);     // besides the one hit
LiveTuning::Knob const WildfireRange("hunter.wildfire_range", 10.0f);
LiveTuning::KnobInt const WildfireMaxTargets("hunter.wildfire_max_targets", 11);         // besides the one hit
LiveTuning::Knob const WildfireBurnFactor("hunter.wildfire_burn_factor", 1.5f);  // its burn, on every enemy of the pack
LiveTuning::Knob const ButcheryFactor("hunter.butchery_factor", 1.4f);

constexpr uint32 HoldCheckMs = 250;
constexpr uint32 BoostWindowMs = 3000;
LiveTuning::KnobInt const FrenzyMax("hunter.frenzy_max", 3);
LiveTuning::KnobInt const TipOfTheSpearMax("hunter.tip_of_the_spear_max", 3);
LiveTuning::KnobInt const MongooseFuryMax("hunter.mongoose_fury_max", 5);
LiveTuning::KnobInt const ChakramHits("hunter.chakram_hits", 7);
LiveTuning::KnobUInt const StampedePulseMs("hunter.stampede_pulse_ms", 1000);
LiveTuning::KnobUInt const CallOfTheWildPulseMs("hunter.call_of_the_wild_pulse_ms", 4000);
LiveTuning::KnobInt const BestialWrathPerBarbedMs("hunter.bestial_wrath_per_barbed_ms", 12000);
LiveTuning::KnobInt const BeastCleaveMs("hunter.beast_cleave_ms", 4000);
LiveTuning::Knob const CleaveRange("hunter.cleave_range", 8.0f);

enum ChargeKind : uint8
{
    CHARGE_KILL_COMMAND,
    CHARGE_BARBED_SHOT,
    CHARGE_AIMED_SHOT,
    CHARGE_WILDFIRE_BOMB,
    CHARGE_BUTCHERY,
    MAX_CHARGE_KINDS
};

struct ChargeDefinition
{
    uint32 spellId;     // first rank
    uint32 displayAura;
};

constexpr std::array<ChargeDefinition, MAX_CHARGE_KINDS> ChargeDefinitions = {{
    { SPELL_KILL_COMMAND, SPELL_KILL_COMMAND_CHARGES },
    { SPELL_BARBED_SHOT, SPELL_BARBED_SHOT_CHARGES },
    { SPELL_AIMED_SHOT_R1, SPELL_AIMED_SHOT_CHARGES },
    { SPELL_WILDFIRE_BOMB, SPELL_WILDFIRE_CHARGES },
    { SPELL_BUTCHERY, SPELL_BUTCHERY_CHARGES },
}};

struct Charges
{
    bool initialized = false;
    uint8 count = 0;
    int32 rechargeMs = 0;
    uint32 spellId = 0;             // the rank last cast
    bool clearCooldown = false;     // the cooldown its cast just started is taken back on the next update
    bool hasteCooldown = false;     // without charges: the cooldown its cast just started is shortened by the rate
};

// A damage bonus for the hits of one cast (a Multi-Shot reaches several enemies): its spell, its factor, until when
struct Boost
{
    float factor = 1.0f;
    uint32 untilMs = 0;
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct HunterState : public DataMap::Base
{
    std::array<Charges, MAX_CHARGE_KINDS> charges;
    std::unordered_map<uint32, Boost> boosts;       // by first rank
    bool lastWasSteady = false;
    uint8 multiShotTargets = 0;
    uint32 multiShotUntilMs = 0;
    bool aimedDouble = false;
    bool aimedTrick = false;
    uint32 aimedUntilMs = 0;
    bool rapidDouble = false;
    bool rapidTrick = false;
    uint32 focusSpent = 0;                          // Appel des tirs
    std::unordered_set<ObjectGuid> bindingStunned;
    uint32 holdTimer = 0;
    uint32 stampedeTimer = 0;
    uint32 callTimer = 0;
    bool volleyExtend = false;
    int32 volleyEndMs = 0;
    // Butchery and Carve reach several enemies at once: their Wildfire Bomb reduction counts 5 at most per strike
    uint32 strikeStamp = 0;
    uint8 strikeHits = 0;
};

constexpr char const* StateKey = "HunterTalentState";

HunterState* GetState(Player* player)
{
    return player->CustomData.GetDefault<HunterState>(StateKey);
}

Player* Hunter(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_HUNTER ? player : nullptr;
}

// The Hunter who owns a pet or a wild beast
Player* HunterOwner(Unit* unit)
{
    if (!unit || unit->IsPlayer())
        return nullptr;
    Unit* owner = unit->GetOwner();
    return owner ? Hunter(owner) : nullptr;
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsBeastMastery(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_BEAST_MASTERY);
}

bool IsMarksmanship(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_MARKSMANSHIP);
}

bool IsSurvival(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_SURVIVAL);
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

uint8 Stacks(Unit const* unit, uint32 spellId, ObjectGuid caster = ObjectGuid::Empty)
{
    Aura* aura = unit->GetAura(spellId, caster);
    return aura ? aura->GetStackAmount() : 0;
}

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
    if (aura)
    {
        aura->SetStackAmount(stacks);
        aura->RefreshDuration();
    }
}

// An aura of the Hunter's on a unit, one more stack (up to max), its duration refreshed
Aura* AddStack(Player* player, Unit* target, uint32 spellId, uint8 max)
{
    if (!target)
        return nullptr;
    if (Aura* aura = target->GetAura(spellId, player->GetGUID()))
    {
        if (aura->GetStackAmount() < max)
            aura->ModStackAmount(1);
        aura->RefreshDuration();
        return aura;
    }
    return player->AddAura(spellId, target);
}

Pet* LivePet(Player* player)
{
    Pet* pet = player->GetPet();
    return pet && pet->IsAlive() ? pet : nullptr;
}

float RangedPower(Player* player)
{
    return player->GetTotalAttackPowerValue(RANGED_ATTACK);
}

int32 Amount(float value)
{
    return int32(std::clamp(value, 1.0f, float(std::numeric_limits<int32>::max() / 2)));
}

void Focus(Player* player, int32 amount)
{
    if (amount && player->getPowerType() == POWER_FOCUS)
        player->ModifyPower(POWER_FOCUS, amount);
}

// Each hit's share on a pack of `enemies`: whole up to AreaFullTargets, then sqrt(AreaFullTargets / enemies)
float AreaFalloff(uint8 enemies)
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

// Every rank of a spell the player knows, its cooldown taken back
void ClearChainCooldown(Player* player, uint32 spellId)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->RemoveSpellCooldown(rank, true);
}

void ModifyChainCooldown(Player* player, uint32 spellId, int32 delta)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->ModifySpellCooldown(rank, delta);
}

// The highest rank of a chain the player knows (Serpent Sting for Morsure de vipère), 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

// --- Charges ---------------------------------------------------------------------------------------------------------

uint8 MaxCharges(Player* player, ChargeKind kind)
{
    switch (kind)
    {
        case CHARGE_KILL_COMMAND:
            return IsBeastMastery(player) && player->HasAura(TALENT_ALPHA_PREDATOR) ? 2 : 1;
        case CHARGE_BARBED_SHOT:
            return IsBeastMastery(player) ? (player->HasAura(TALENT_BARBED_RELOAD) ? 3 : 2) : 1;
        case CHARGE_AIMED_SHOT:
            return IsMarksmanship(player) ? 2 : 1;
        case CHARGE_WILDFIRE_BOMB:
            return player->HasAura(TALENT_GUERRILLA) ? 2 : 1;
        case CHARGE_BUTCHERY:
            return player->HasSpell(SPELL_BUTCHERY) ? 3 : 1;
        default:
            return 1;
    }
}

// How fast the next charge comes back: Appel de la nature sauvage and Assaut coordonné 50% faster for Kill Command,
// the first for Barbed Shot too, Visée parfaite twice as fast for Aimed Shot
float RechargeRate(Player* player, ChargeKind kind)
{
    switch (kind)
    {
        case CHARGE_KILL_COMMAND:
            return player->HasAura(SPELL_CALL_OF_THE_WILD) || player->HasAura(SPELL_COORDINATED_ASSAULT) ? 1.5f : 1.0f;
        case CHARGE_BARBED_SHOT:
            return player->HasAura(SPELL_CALL_OF_THE_WILD) ? 1.5f : 1.0f;
        case CHARGE_AIMED_SHOT:
            return player->HasAura(SPELL_TRUESHOT) ? 2.0f : 1.0f;
        default:
            return 1.0f;
    }
}

// A rank's cooldown with the Hunter's modifiers
int32 Recharge(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime)) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

bool ChargesManaged(Player* player, ChargeKind kind)
{
    return MaxCharges(player, kind) > 1;
}

void SpendCharge(Player* player, HunterState* state, ChargeKind kind, uint32 spellId)
{
    uint8 const max = MaxCharges(player, kind);
    Charges& charges = state->charges[kind];
    if (max <= 1)
    {
        // No charges: a faster recharge (Assaut coordonné on Survival's Kill Command) shortens the cooldown itself
        charges.spellId = spellId;
        charges.hasteCooldown = RechargeRate(player, kind) > 1.0f;
        return;
    }
    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = max;
    }
    charges.spellId = spellId;
    if (charges.count >= max)
        charges.rechargeMs = Recharge(player, spellId);
    if (charges.count > 0)
        --charges.count;
    charges.clearCooldown = charges.count > 0;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

// A cooldown reduction (Tir du cobra, Découpe, Frappes frénétiques): on the charge coming back, or on the cooldown
void ReduceRecharge(Player* player, HunterState* state, ChargeKind kind, int32 ms)
{
    if (ms <= 0)
        return;
    if (!ChargesManaged(player, kind))
    {
        ModifyChainCooldown(player, ChargeDefinitions[kind].spellId, -ms);
        return;
    }
    Charges& charges = state->charges[kind];
    if (charges.initialized && charges.count < MaxCharges(player, kind))
        charges.rechargeMs -= ms;
}

// Every charge back at once (Cobra meurtrier, Bombardier)
void RefillCharges(Player* player, HunterState* state, ChargeKind kind)
{
    uint32 const spellId = ChargeDefinitions[kind].spellId;
    ClearChainCooldown(player, spellId);
    Charges& charges = state->charges[kind];
    if (!ChargesManaged(player, kind))
        return;
    charges.initialized = true;
    charges.count = MaxCharges(player, kind);
    charges.rechargeMs = 0;
    charges.clearCooldown = false;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

void UpdateCharges(Player* player, HunterState* state, ChargeKind kind, uint32 diff)
{
    Charges& charges = state->charges[kind];
    uint8 const max = MaxCharges(player, kind);
    uint32 const display = ChargeDefinitions[kind].displayAura;
    if (max <= 1)
    {
        if (charges.hasteCooldown)
        {
            charges.hasteCooldown = false;
            float const rate = RechargeRate(player, kind);
            if (charges.spellId && player->HasSpellCooldown(charges.spellId))
                player->ModifySpellCooldown(charges.spellId,
                    -int32(float(Recharge(player, charges.spellId)) * (1.0f - 1.0f / rate)));
        }
        if (charges.initialized || player->HasAura(display))
        {
            charges = Charges();
            player->RemoveAurasDueToSpell(display);
        }
        return;
    }

    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = max;
    }

    uint32 const spellId = charges.spellId ? charges.spellId : ChargeDefinitions[kind].spellId;
    if (charges.clearCooldown)
    {
        charges.clearCooldown = false;
        ClearChainCooldown(player, spellId);
    }

    if (charges.count >= max)
    {
        charges.count = max;
        if (Stacks(player, display) != max)
            ShowStacks(player, display, max);
        return;
    }

    charges.rechargeMs -= int32(float(diff) * RechargeRate(player, kind));
    if (charges.rechargeMs > 0)
        return;

    // A charge back: usable again if it was the last one gone
    if (!charges.count)
        ClearChainCooldown(player, spellId);
    ++charges.count;
    charges.rechargeMs = charges.count < max ? Recharge(player, spellId) + charges.rechargeMs : 0;
    ShowStacks(player, display, charges.count);
}

// --- Shared effects --------------------------------------------------------------------------------------------------

// Coup fatal: the next Kill Shot usable at any health, and ready
void RollDeathblow(Player* player)
{
    if (!player->HasAura(TALENT_DEATHBLOW) || !roll_chance_i(10))
        return;
    player->CastSpell(player, SPELL_DEATHBLOW, true);
    ClearChainCooldown(player, SPELL_KILL_SHOT_R1);
}

void SetBoost(HunterState* state, uint32 firstRank, float factor)
{
    state->boosts[firstRank] = { factor, Now() + BoostWindowMs };
}

// The Hunter's Serpent Sting on an enemy, free (Morsure de vipère, Venin de vipère): its copy that reaches a target in
// melee (a triggered spell still checks its range, and the stock ranks have the ranged weapon's)
void ApplySerpentSting(Player* player, Unit* target)
{
    if (target && target->IsAlive() && KnownRank(player, SPELL_SERPENT_STING_R1))
        player->CastSpell(target, SPELL_SERPENT_STING_MELEE, true);
}

// A wild beast (Bête sauvage, Appel de la nature sauvage): a guardian of the Hunter's beside it, next to its pet (it
// fights its owner's target), its swings set from the Hunter's ranged attack power, sent at the
// target. Bêtes redoutables: harder and 1-2 s longer.
void CallWildBeast(Player* player, Unit* target)
{
    if (!target || !target->IsAlive() || !player->IsInMap(target))
        return;
    SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(WildBeastSummonProperties);
    uint8 const dire = Rank(player, TALENT_DIRE_BEASTS_1, TALENT_DIRE_BEASTS_2);
    Position const position = player->GetNearPosition(2.0f, frand(0.0f, 2.0f * float(M_PI)));
    TempSummon* beast = player->GetMap()->SummonCreature(NpcWildBeast, position, properties,
        WildBeastMs + 1000 * dire, player, SPELL_DIRE_BEAST);
    if (!beast)
        return;

    float const swing = RangedPower(player) * BeastHitPower * (1.0f + 0.25f * float(dire));
    beast->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, swing * 0.9f);
    beast->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, swing * 1.1f);
    beast->SetAttackTime(BASE_ATTACK, 2000);
    beast->UpdateDamagePhysical(BASE_ATTACK);
    if (beast->IsAIEnabled && beast->AI())
        beast->AI()->AttackStart(target);
}

// Beast Cleave and Ordre sauvage: a pet's hit reaches the enemies near its target
void Cleave(Unit* pet, Player* player, Unit* victim, uint32 damage, float share)
{
    std::list<Unit*> const enemies = EnemiesNear(player, victim, CleaveRange);
    int32 const amount = Amount(float(damage) * share *
        AreaFalloff(uint8(std::min<std::size_t>(enemies.size(), BeastCleaveMaxTargets) + 1)));
    uint8 hit = 0;
    for (Unit* enemy : enemies)
    {
        if (hit++ >= BeastCleaveMaxTargets)
            break;
        pet->CastCustomSpell(enemy, SPELL_BEAST_CLEAVE_HIT, &amount, nullptr, nullptr, true, nullptr, nullptr,
            pet->GetGUID());
    }
}

float BeastCleaveShare(Player* player)
{
    switch (Rank(player, TALENT_BEAST_CLEAVE_1, TALENT_BEAST_CLEAVE_2))
    {
        case 2:
            return 0.8f;
        case 1:
            return 0.4f;
        default:
            return 0.0f;
    }
}

// The pet's Kill Command hit, and what it brings
void KillCommand(Player* player, HunterState* state, Unit* target)
{
    Pet* pet = LivePet(player);
    if (!pet || !target || !target->IsAlive())
        return;

    int32 amount = Amount(RangedPower(player) * KillCommandPower);
    // Prédateur alpha, Maître des bêtes, Ruse du prédateur: the Hunter's modifiers on Kill Command
    player->ApplySpellMod(SPELL_KILL_COMMAND, SPELLMOD_DAMAGE, amount);
    pet->CastCustomSpell(target, SPELL_KILL_COMMAND_HIT, &amount, nullptr, nullptr, true, nullptr, nullptr,
        pet->GetGUID());
    if (pet->IsAIEnabled && pet->GetVictim() != target && pet->AI())
        pet->AI()->AttackStart(target);

    // Ordre sauvage: during Beast Cleave, Kill Command cleaves too
    if (player->HasAura(TALENT_KILL_CLEAVE) && pet->HasAura(SPELL_BEAST_CLEAVE, player->GetGUID()))
        Cleave(pet, player, target, uint32(amount), KillCleaveShare);

    if (IsSurvival(player))
    {
        Focus(player, 15);
        if (player->HasAura(TALENT_TIP_OF_THE_SPEAR))
            AddStack(player, player, SPELL_TIP_OF_THE_SPEAR, TipOfTheSpearMax);
        if (player->HasAura(TALENT_BLOODSEEKER))
            player->CastSpell(target, SPELL_BLOODSEEKER_BLEED, true);
        // Mort coordonnée: during Coordinated Assault, a quarter of them come straight back
        if (player->HasAura(TALENT_COORDINATED_KILL) && player->HasAura(SPELL_COORDINATED_ASSAULT) &&
            roll_chance_i(25))
            RefillCharges(player, state, CHARGE_KILL_COMMAND);
    }
    RollDeathblow(player);
}

// Barbed Shot: Frenzy on the pet, Focus over time, Bestial Wrath closer, Piétinement, Compagnon brutal
void BarbedShot(Player* player, HunterState* state)
{
    SpendCharge(player, state, CHARGE_BARBED_SHOT, SPELL_BARBED_SHOT);
    player->CastSpell(player, SPELL_BARBED_FOCUS, true);
    ModifyChainCooldown(player, SPELL_BESTIAL_WRATH, -BestialWrathPerBarbedMs);
    RollDeathblow(player);

    Pet* pet = LivePet(player);
    if (!pet)
        return;
    uint8 const before = Stacks(pet, SPELL_FRENZY, player->GetGUID());
    AddStack(player, pet, SPELL_FRENZY, FrenzyMax);
    uint8 const after = Stacks(pet, SPELL_FRENZY, player->GetGUID());

    if (player->HasAura(TALENT_STOMP))
    {
        int32 const amount = Amount(RangedPower(player) * StompPower);
        std::list<Unit*> enemies = EnemiesNear(player, pet, CleaveRange);
        if (Unit* victim = pet->GetVictim())
            if (victim->IsAlive() && pet->IsWithinDistInMap(victim, CleaveRange) &&
                std::find(enemies.begin(), enemies.end(), victim) == enemies.end())
                enemies.push_back(victim);
        uint8 hit = 0;
        for (Unit* enemy : enemies)
        {
            if (hit++ >= 8)
                break;
            pet->CastCustomSpell(enemy, SPELL_STOMP, &amount, nullptr, nullptr, true, nullptr, nullptr,
                pet->GetGUID());
        }
    }

    // Compagnon brutal: Frenzy brought to its full 3 makes the pet strike again
    if (player->HasAura(TALENT_BRUTAL_COMPANION) && before < FrenzyMax && after >= FrenzyMax)
        if (Unit* victim = pet->GetVictim())
            if (victim->IsAlive())
            {
                int32 amount = Amount(RangedPower(player) * KillCommandPower * BrutalCompanionShare);
                player->ApplySpellMod(SPELL_KILL_COMMAND, SPELLMOD_DAMAGE, amount);
                pet->CastCustomSpell(victim, SPELL_BRUTAL_COMPANION_HIT, &amount, nullptr, nullptr, true, nullptr,
                    nullptr, pet->GetGUID());
            }
}

// Death Chakram: 7 hits spread over the target and the enemies near it, 3 Focus each
void DeathChakram(Player* player, Unit* target)
{
    if (!target || !target->IsAlive())
        return;
    std::vector<Unit*> victims = { target };
    for (Unit* enemy : EnemiesNear(player, target, 10.0f))
    {
        if (victims.size() >= ChakramHits)
            break;
        victims.push_back(enemy);
    }
    for (uint8 hit = 0; hit < ChakramHits; ++hit)
    {
        player->CastSpell(victims[hit % victims.size()], SPELL_DEATH_CHAKRAM_HIT, true);
        Focus(player, 3);
    }
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

class HunterTalentSpellScript : public AllSpellScript
{
public:
    HunterTalentSpellScript() : AllSpellScript("HunterTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // A spell kept on charges cannot be cast with none left, whatever its cooldown says
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Hunter(spell->GetCaster());
        if (!player)
            return;
        uint32 const firstRank = FirstRank(spell->GetSpellInfo());
        HunterState* state = GetState(player);
        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
        {
            if (ChargeDefinitions[kind].spellId != firstRank || !ChargesManaged(player, ChargeKind(kind)))
                continue;
            Charges const& charges = state->charges[kind];
            if (charges.initialized && !charges.count)
                result = SPELL_FAILED_NOT_READY;
            return;
        }
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Hunter(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        HunterState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        // Appel des tirs: every 50 Focus spent brings Visée parfaite 2.5 s closer
        if (player->HasAura(TALENT_CALLING_THE_SHOTS) && spell->GetPowerCost() > 0 &&
            spellInfo->PowerType == POWER_FOCUS)
        {
            state->focusSpent += uint32(spell->GetPowerCost());
            while (state->focusSpent >= 50)
            {
                state->focusSpent -= 50;
                if (player->HasSpellCooldown(SPELL_TRUESHOT))
                    player->ModifySpellCooldown(SPELL_TRUESHOT, -2500);
            }
        }

        if (firstRank != SPELL_STEADY_SHOT_R1 && spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER &&
            spellInfo->HasAttribute(SPELL_ATTR0_USES_RANGED_SLOT))
            state->lastWasSteady = false;

        switch (id)
        {
            case SPELL_KILL_COMMAND:
                SpendCharge(player, state, CHARGE_KILL_COMMAND, id);
                KillCommand(player, state, target);
                return;
            case SPELL_BARBED_SHOT:
                BarbedShot(player, state);
                return;
            case SPELL_COBRA_SHOT:
            {
                int32 const reduction = 1000 + 500 * Rank(player, TALENT_COBRA_VENOM_1, TALENT_COBRA_VENOM_2);
                ReduceRecharge(player, state, CHARGE_KILL_COMMAND, reduction);
                // Cobra meurtrier: during Bestial Wrath, Kill Command comes back at once
                Pet* pet = LivePet(player);
                if (player->HasAura(TALENT_KILLER_COBRA) && pet && pet->HasAura(SPELL_BESTIAL_WRATH))
                    RefillCharges(player, state, CHARGE_KILL_COMMAND);
                RollDeathblow(player);
                return;
            }
            case SPELL_DIRE_BEAST:
                player->CastSpell(player, SPELL_DIRE_BEAST_FOCUS, true);
                CallWildBeast(player, target);
                return;
            case SPELL_CALL_OF_THE_WILD:
                state->callTimer = 0;
                CallWildBeast(player, player->GetVictim() ? player->GetVictim() : target);
                return;
            case SPELL_BLOODSHED:
                if (Pet* pet = LivePet(player))
                    if (target && target->IsAlive())
                    {
                        int32 const tick = Amount(RangedPower(player) * BloodshedTickPower);
                        pet->CastCustomSpell(target, SPELL_BLOODSHED_BLEED, &tick, nullptr, nullptr, true, nullptr,
                            nullptr, pet->GetGUID());
                        if (pet->IsAIEnabled && pet->AI())
                            pet->AI()->AttackStart(target);
                    }
                return;
            case SPELL_ASPECT_OF_THE_BEAST:
                if (Pet* pet = LivePet(player))
                    player->AddAura(SPELL_ASPECT_OF_THE_BEAST, pet);
                return;
            case SPELL_EXHILARATION:
                if (Pet* pet = LivePet(player))
                    pet->SetFullHealth();
                return;
            case SPELL_CAMOUFLAGE:
                if (Pet* pet = LivePet(player))
                    player->AddAura(SPELL_CAMOUFLAGE, pet);
                return;
            case SPELL_BINDING_SHOT:
                state->bindingStunned.clear();
                return;
            case SPELL_DEATH_CHAKRAM:
                DeathChakram(player, target);
                return;
            case SPELL_STAMPEDE:
                state->stampedeTimer = StampedePulseMs;
                return;
            case SPELL_RAPID_FIRE:
                state->rapidDouble = player->HasAura(SPELL_DOUBLE_TAP);
                player->RemoveAurasDueToSpell(SPELL_DOUBLE_TAP);
                state->rapidTrick = player->HasAura(SPELL_TRICK_SHOTS);
                player->RemoveAurasDueToSpell(SPELL_TRICK_SHOTS);
                if (uint8 const streamline = Rank(player, TALENT_STREAMLINE_1, TALENT_STREAMLINE_2))
                {
                    int32 const share = -15 * int32(streamline);
                    player->CastCustomSpell(player, SPELL_STREAMLINE, &share, nullptr, nullptr, true);
                }
                return;
            case SPELL_VOLLEY:
                player->CastSpell(player, SPELL_TRICK_SHOTS, true);
                if (player->HasAura(TALENT_VOLLEY_RICOCHET))
                {
                    state->volleyExtend = true;
                    state->volleyEndMs = 8000;
                }
                return;
            case SPELL_WILDFIRE_BOMB:
                SpendCharge(player, state, CHARGE_WILDFIRE_BOMB, id);
                return;
            case SPELL_BUTCHERY:
                SpendCharge(player, state, CHARGE_BUTCHERY, id);
                ConsumeTip(player, state, SPELL_BUTCHERY);
                return;
            case SPELL_CARVE:
                ConsumeTip(player, state, SPELL_CARVE);
                return;
            case SPELL_COORDINATED_ASSAULT:
            {
                Pet* pet = LivePet(player);
                if (pet)
                    player->AddAura(SPELL_COORDINATED_ASSAULT, pet);
                if (player->HasAura(TALENT_BOMBARDIER))
                    RefillCharges(player, state, CHARGE_WILDFIRE_BOMB);
                // Duo mortel: 5 s longer, and the pet faster
                if (player->HasAura(TALENT_DEADLY_DUO))
                {
                    for (Unit* unit : { static_cast<Unit*>(player), static_cast<Unit*>(pet) })
                        if (unit)
                            if (Aura* assault = unit->GetAura(SPELL_COORDINATED_ASSAULT, player->GetGUID()))
                            {
                                assault->SetMaxDuration(assault->GetMaxDuration() + 5000);
                                assault->SetDuration(assault->GetMaxDuration());
                            }
                    if (pet)
                        if (Aura* frenzy = AddStack(player, pet, SPELL_FRENZY, FrenzyMax))
                        {
                            frenzy->SetMaxDuration(25000);
                            frenzy->SetDuration(25000);
                        }
                }
                return;
            }
            case SPELL_FLANKING_STRIKE:
                Focus(player, 30);
                if (Pet* pet = LivePet(player))
                    if (target && target->IsAlive())
                    {
                        int32 const amount = Amount(RangedPower(player) * FlankingPetPower);
                        pet->CastCustomSpell(target, SPELL_FLANKING_STRIKE_PET, &amount, nullptr, nullptr, true,
                            nullptr, nullptr, pet->GetGUID());
                    }
                return;
            case SPELL_SPEARHEAD:
                if (target && target->IsAlive())
                {
                    player->CastSpell(player, SPELL_SPEARHEAD_CRIT, true);
                    if (Pet* pet = LivePet(player))
                    {
                        pet->CastSpell(target, SPELL_PET_CHARGE, true);
                        int32 const tick = Amount(RangedPower(player) * SpearheadTickPower);
                        pet->CastCustomSpell(target, SPELL_SPEARHEAD_BLEED, &tick, nullptr, nullptr, true, nullptr,
                            nullptr, pet->GetGUID());
                    }
                }
                return;
            case SPELL_HARPOON:
                if (player->HasAura(TALENT_TERMS_OF_ENGAGEMENT))
                {
                    Focus(player, 20);
                    if (target && target->IsAlive())
                    {
                        int32 const amount = Amount(RangedPower(player) * TermsPower);
                        player->CastCustomSpell(target, SPELL_TERMS_HIT, &amount, nullptr, nullptr, true);
                    }
                }
                return;
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_STEADY_SHOT_R1:
                Focus(player, 10);
                // Tir assuré: two in a row, ranged haste
                if (uint8 const steady = Rank(player, TALENT_STEADY_FOCUS_1, TALENT_STEADY_FOCUS_2))
                {
                    if (state->lastWasSteady)
                    {
                        int32 const haste = steady > 1 ? 15 : 7;
                        player->CastCustomSpell(player, SPELL_STEADY_FOCUS, &haste, nullptr, nullptr, true);
                        state->lastWasSteady = false;
                    }
                    else
                        state->lastWasSteady = true;
                }
                RollDeathblow(player);
                break;
            case SPELL_ARCANE_SHOT_R1:
                ConsumePrecise(player, state, firstRank);
                RollDeathblow(player);
                break;
            case SPELL_MULTI_SHOT_R1:
            {
                uint8 targets = 1;
                if (target)
                    targets = uint8(std::min<std::size_t>(MultiShotMaxTargets,
                        1 + EnemiesNear(player, target, 10.0f).size()));
                state->multiShotTargets = targets;
                state->multiShotUntilMs = Now() + BoostWindowMs;
                ConsumePrecise(player, state, firstRank);
                // Fendoir de la bête
                if (BeastCleaveShare(player) > 0.0f)
                    if (Pet* pet = LivePet(player))
                        if (Aura* cleave = player->AddAura(SPELL_BEAST_CLEAVE, pet))
                        {
                            int32 const duration = BeastCleaveMs + (player->HasAura(TALENT_IMPROVED_CLEAVE) ? 2000 : 0);
                            cleave->SetMaxDuration(duration);
                            cleave->SetDuration(duration);
                        }
                // Tirs de ricochet
                if (player->HasAura(TALENT_TRICK_SHOTS) && targets >= 3)
                    player->CastSpell(player, SPELL_TRICK_SHOTS, true);
                RollDeathblow(player);
                break;
            }
            case SPELL_AIMED_SHOT_R1:
                SpendCharge(player, state, CHARGE_AIMED_SHOT, id);
                state->aimedUntilMs = Now() + BoostWindowMs;
                state->aimedDouble = player->HasAura(SPELL_DOUBLE_TAP);
                player->RemoveAurasDueToSpell(SPELL_DOUBLE_TAP);
                state->aimedTrick = player->HasAura(SPELL_TRICK_SHOTS);
                player->RemoveAurasDueToSpell(SPELL_TRICK_SHOTS);
                player->RemoveAurasDueToSpell(SPELL_LOCK_AND_LOAD);
                player->RemoveAurasDueToSpell(SPELL_STREAMLINE);
                if (player->HasAura(TALENT_PRECISE_SHOTS))
                    if (Aura* precise = player->AddAura(SPELL_PRECISE_SHOTS, player))
                        precise->SetStackAmount(2);
                RollDeathblow(player);
                break;
            case SPELL_KILL_SHOT_R1:
                player->RemoveAurasDueToSpell(SPELL_DEATHBLOW);
                break;
            case SPELL_RAPTOR_STRIKE_R1:
            {
                float factor = TakeTip(player);
                // Furie de la mangouste: the stacks so far count, then one more
                if (player->HasAura(TALENT_MONGOOSE_FURY))
                {
                    factor *= 1.0f + 0.15f * float(Stacks(player, SPELL_MONGOOSE_FURY));
                    AddStack(player, player, SPELL_MONGOOSE_FURY, MongooseFuryMax);
                }
                SetBoost(state, firstRank, factor);
                if (player->HasAura(TALENT_VIPERS_VENOM))
                    ApplySerpentSting(player, target);
                break;
            }
            default:
                break;
        }
    }

private:
    // Tirs précis: an Arcane Shot or Multi-Shot spends a stack for 75% more (20% or 40% more with the talent)
    static void ConsumePrecise(Player* player, HunterState* state, uint32 firstRank)
    {
        Aura* precise = player->GetAura(SPELL_PRECISE_SHOTS);
        if (!precise)
        {
            state->boosts.erase(firstRank);
            return;
        }
        float const bonus = 0.75f + 0.2f * float(Rank(player, TALENT_IMPROVED_PRECISE_1, TALENT_IMPROVED_PRECISE_2));
        SetBoost(state, firstRank, 1.0f + bonus);
        precise->ModStackAmount(-1);
    }

    // Pointe de la lance: its stacks spent on this strike, 25% each
    static float TakeTip(Player* player)
    {
        uint8 const stacks = Stacks(player, SPELL_TIP_OF_THE_SPEAR);
        if (!stacks)
            return 1.0f;
        player->RemoveAurasDueToSpell(SPELL_TIP_OF_THE_SPEAR);
        return 1.0f + 0.25f * float(stacks);
    }

    static void ConsumeTip(Player* player, HunterState* state, uint32 spellId)
    {
        SetBoost(state, spellId, TakeTip(player));
    }
};

// --- Damage, auras ---------------------------------------------------------------------------------------------------

class HunterTalentUnitScript : public UnitScript
{
public:
    HunterTalentUnitScript() : UnitScript("HunterTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_SPELL_CRIT_CHANCE,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_ON_AURA_REMOVE
    }) { }

    // Liens de la meute: the pet takes less
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        if (!damage || !victim || !victim->IsPet())
            return;
        Player* player = HunterOwner(victim);
        if (uint8 const bond = player ? Rank(player, TALENT_PACK_BOND_1, TALENT_PACK_BOND_2) : 0)
            damage = uint32(float(damage) * (1.0f - 0.05f * float(bond)));
    }

    // The pet's and the wild beasts' swings: their bonuses, and Beast Cleave
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (!damage || !target || !attacker)
            return;
        Player* player = HunterOwner(attacker);
        if (!player)
        {
            // The Hunter's own swings (Survival): Marque de la proie
            if (Player* hunter = Hunter(attacker))
                damage = uint32(float(damage) * MarkedPrey(hunter, target));
            return;
        }
        damage = uint32(float(damage) * PetFactor(player, attacker, target));

        if (attacker->IsPet() && attacker->HasAura(SPELL_BEAST_CLEAVE, player->GetGUID()))
            if (float const share = BeastCleaveShare(player))
                Cleave(attacker, player, target, damage, share);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !attacker || !spellInfo || damage <= 0)
            return;

        if (Player* player = HunterOwner(attacker))
        {
            // The pet's hits (ours and its own abilities): the pet's bonuses. Beast Cleave's and the bleeds are
            // already worked out from the Kill Command or the swing they come from.
            if (spellInfo->Id != SPELL_BEAST_CLEAVE_HIT)
                damage = int32(float(damage) * PetFactor(player, attacker, target));
            return;
        }

        Player* player = Hunter(attacker);
        if (!player)
            return;
        HunterState* state = GetState(player);
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = MarkedPrey(player, target);

        auto const boost = state->boosts.find(firstRank);
        if (boost != state->boosts.end() && Now() <= boost->second.untilMs)
            factor *= boost->second.factor;

        switch (firstRank)
        {
            case SPELL_MULTI_SHOT_R1:
                if (Now() <= state->multiShotUntilMs)
                    factor *= MultiShotHitFactor * AreaFalloff(state->multiShotTargets);
                // Salve meurtrière
                if (uint8 const salvo = Rank(player, TALENT_DEADLY_SALVO_1, TALENT_DEADLY_SALVO_2))
                    if (state->multiShotTargets >= 3 && Now() <= state->multiShotUntilMs)
                        factor *= 1.0f + 0.15f * float(salvo);
                break;
            case SPELL_AIMED_SHOT_R1:
                if (state->aimedDouble && Now() <= state->aimedUntilMs)
                    factor *= 2.0f;
                break;
            case SPELL_RAPID_FIRE_SHOT:
                if (state->rapidDouble)
                    factor *= 2.0f;
                break;
            case SPELL_BUTCHERY:
                factor *= ButcheryFactor;
                break;
            case SPELL_WILDFIRE_BOMB:
                // Tactique de guérilla
                if (player->HasAura(TALENT_GUERRILLA))
                    factor *= 1.5f;
                break;
            case SPELL_WILDFIRE_BURN:
                factor *= WildfireBurnFactor;
                // Bombes infusées
                if (player->HasAura(TALENT_INFUSED_BOMBS))
                    factor *= 1.25f;
                break;
            default:
                break;
        }

        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // Visée mortelle: Aimed Shot and Rapid Fire crit more on a target above 70% health
    void ModifySpellCritChance(Unit const* caster, Unit const* victim, SpellInfo const* spellInfo,
                               float& critChance) override
    {
        if (!caster || !victim || !spellInfo || !caster->IsPlayer() || caster->getClass() != CLASS_HUNTER)
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        if (firstRank != SPELL_AIMED_SHOT_R1 && firstRank != SPELL_RAPID_FIRE_SHOT)
            return;
        if (uint8 const aim = Rank(caster, TALENT_CAREFUL_AIM_1, TALENT_CAREFUL_AIM_2))
            if (victim->GetHealthPct() > 70.0f)
                critChance += 10.0f * float(aim);
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool critical) override
    {
        if (!caster || !victim || !spellInfo || !damage)
            return;

        // A pet's own abilities (Bite, Claw...) cleave during Beast Cleave; the hits this module deals do not
        if (caster->IsPet())
        {
            if (spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && spellInfo->Id >= 93200 && spellInfo->Id <= 93399)
                return;
            if (Player* player = HunterOwner(caster))
                if (caster->HasAura(SPELL_BEAST_CLEAVE, player->GetGUID()))
                    if (float const share = BeastCleaveShare(player))
                        Cleave(caster, player, victim, damage, share);
            return;
        }

        Player* player = Hunter(caster);
        if (!player)
            return;
        HunterState* state = GetState(player);
        uint32 const firstRank = FirstRank(spellInfo);

        switch (firstRank)
        {
            case SPELL_AUTO_SHOT:
                // Appel sauvage: a critical Auto Shot may bring a Barbed Shot back
                if (critical && player->HasAura(TALENT_WILD_CALL) && roll_chance_i(20))
                    ReduceRecharge(player, state, CHARGE_BARBED_SHOT, 60000);
                // Chargement éclair: the next Aimed Shot instant and free
                if (player->HasAura(TALENT_LOCK_AND_LOAD) && roll_chance_i(8))
                    player->CastSpell(player, SPELL_LOCK_AND_LOAD, true);
                break;
            case SPELL_AIMED_SHOT_R1:
                if (Now() <= state->aimedUntilMs && state->aimedTrick)
                    Ricochet(player, victim, damage);
                // Héritage des Coursevent
                if (player->HasAura(TALENT_WINDRUNNERS) && roll_chance_i(25))
                {
                    int32 const amount = Amount(float(damage) * WindArrowShare);
                    for (uint8 arrow = 0; arrow < 3; ++arrow)
                        player->CastCustomSpell(victim, SPELL_WIND_ARROW, &amount, nullptr, nullptr, true);
                }
                break;
            case SPELL_RAPID_FIRE_SHOT:
                Focus(player, 1);
                if (state->rapidTrick)
                    Ricochet(player, victim, damage);
                break;
            case SPELL_WAILING_ARROW:
            {
                int32 const amount = Amount(float(damage) * WailingSplashShare);
                player->CastSpell(victim, SPELL_WAILING_SILENCE, true);
                for (Unit* enemy : EnemiesNear(player, victim, 8.0f))
                {
                    player->CastSpell(enemy, SPELL_WAILING_SILENCE, true);
                    player->CastCustomSpell(enemy, SPELL_WAILING_SPLASH, &amount, nullptr, nullptr, true);
                }
                break;
            }
            case SPELL_WILDFIRE_BOMB:
                WildfireBomb(player, victim);
                break;
            case SPELL_BUTCHERY:
            case SPELL_CARVE:
                // Découpe: a second closer per enemy; Frappes frénétiques: half a second or one more with either
                {
                    int32 const perTarget = (firstRank == SPELL_CARVE ? 1000 : 0) +
                        500 * Rank(player, TALENT_FRENZY_STRIKES_1, TALENT_FRENZY_STRIKES_2);
                    uint32 const now = Now();
                    if (perTarget && (now != state->strikeStamp || state->strikeHits < 5))
                    {
                        if (now != state->strikeStamp)
                        {
                            state->strikeStamp = now;
                            state->strikeHits = 0;
                        }
                        ++state->strikeHits;
                        ReduceRecharge(player, state, CHARGE_WILDFIRE_BOMB, perTarget);
                    }
                }
                break;
            default:
                break;
        }
    }

    // Tir de lien: an enemy leaving the tether's 5 yards (its aura gone while the tether stands) is stunned, once
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!unit || !aurApp || aurApp->GetBase()->GetId() != SPELL_BINDING_SHOT || mode == AURA_REMOVE_BY_DEATH)
            return;
        Player* player = Hunter(aurApp->GetBase()->GetCaster());
        if (!player || !unit->IsAlive() || !player->IsInMap(unit))
            return;
        DynamicObject* tether = player->GetDynObject(SPELL_BINDING_SHOT);
        if (!tether || tether->GetDuration() <= 0 || unit->GetExactDist2d(tether) <= tether->GetRadius())
            return;
        HunterState* state = GetState(player);
        if (!state->bindingStunned.insert(unit->GetGUID()).second)
            return;
        player->CastSpell(unit, SPELL_BINDING_STUN, true);
    }

private:
    // Marque de la proie: 5% more on a target under the Hunter's Mark
    static float MarkedPrey(Player* player, Unit* target)
    {
        if (!player->HasAura(TALENT_MARKED_PREY))
            return 1.0f;
        return target->GetAuraEffect(SPELL_AURA_MOD_STALKED, SPELLFAMILY_HUNTER, 0x400, 0, 0, player->GetGUID())
            ? 1.05f : 1.0f;
    }

    // What the pet (or a wild beast) gains from the Hunter's specialization and talents
    static float PetFactor(Player* player, Unit* attacker, Unit* target)
    {
        float factor = MarkedPrey(player, target);
        if (attacker->GetEntry() == NpcWildBeast)
            return factor;
        if (IsBeastMastery(player))
            factor *= 1.15f;
        factor *= 1.0f + 0.05f * float(Rank(player, TALENT_PACK_BOND_1, TALENT_PACK_BOND_2));
        factor *= 1.0f + 0.1f * float(Rank(player, TALENT_PACK_INSTINCT_1, TALENT_PACK_INSTINCT_2));
        if (uint8 const bloody = Rank(player, TALENT_BLOODY_FRENZY_1, TALENT_BLOODY_FRENZY_2))
            factor *= 1.0f + 0.03f * float(bloody) * float(Stacks(attacker, SPELL_FRENZY, player->GetGUID()));
        // Effusion de sang: the torn target takes 15% more from the pet
        if (target->HasAura(SPELL_BLOODSHED_BLEED, attacker->GetGUID()))
            factor *= 1.15f;
        return factor;
    }

    // Tirs de ricochet: up to TrickShotsMaxTargets other enemies within 10 yd take 55%
    static void Ricochet(Player* player, Unit* victim, uint32 damage)
    {
        int32 const amount = Amount(float(damage) * TrickShotsShare);
        uint8 hit = 0;
        for (Unit* enemy : EnemiesNear(player, victim, 10.0f))
        {
            if (hit++ >= TrickShotsMaxTargets)
                break;
            player->CastCustomSpell(enemy, SPELL_TRICK_SHOTS_HIT, &amount, nullptr, nullptr, true);
        }
    }

    // Wildfire Bomb burst: the enemies within WildfireRange of the one it hit, and all of them burning
    static void WildfireBomb(Player* player, Unit* victim)
    {
        int32 amount = Amount(RangedPower(player) * WildfireSplashPower);
        player->ApplySpellMod(SPELL_WILDFIRE_BOMB, SPELLMOD_DAMAGE, amount);
        std::list<Unit*> enemies = EnemiesNear(player, victim, WildfireRange);
        uint8 hit = 0;
        for (Unit* enemy : enemies)
        {
            if (hit++ >= WildfireMaxTargets)
                break;
            player->CastCustomSpell(enemy, SPELL_WILDFIRE_SPLASH, &amount, nullptr, nullptr, true);
        }
        enemies.push_front(victim);
        bool const viper = player->HasAura(TALENT_VIPER_BITE);
        bool const infused = player->HasAura(TALENT_INFUSED_BOMBS);
        hit = 0;
        for (Unit* enemy : enemies)
        {
            if (hit++ > WildfireMaxTargets || !enemy->IsAlive())
                break;
            player->CastSpell(enemy, SPELL_WILDFIRE_BURN, true);
            if (infused)
                player->CastSpell(enemy, SPELL_INFUSED_SLOW, true);
            if (viper)
                ApplySerpentSting(player, enemy);
        }
    }
};

// --- Every update: charges, what is held while something lasts, the pulses of Stampede and Call of the Wild ---------

class HunterTalentPlayerScript : public PlayerScript
{
public:
    HunterTalentPlayerScript() : PlayerScript("HunterTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_HUNTER || !player->IsInWorld())
            return;
        HunterState* state = GetState(player);

        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
            UpdateCharges(player, state, ChargeKind(kind), diff);

        UpdatePulses(player, state, diff);

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        state->holdTimer = 0;
        UpdateHolds(player);
    }

private:
    static void UpdatePulses(Player* player, HunterState* state, uint32 diff)
    {
        // Débandade: a charge at up to 3 enemies within 30 yd each second
        if (player->HasAura(SPELL_STAMPEDE))
        {
            state->stampedeTimer += diff;
            if (state->stampedeTimer >= StampedePulseMs)
            {
                state->stampedeTimer = 0;
                std::list<Unit*> list = EnemiesNear(player, player, 30.0f);
                std::vector<Unit*> enemies;
                for (Unit* enemy : list)
                    if (enemy->IsInCombat())
                        enemies.push_back(enemy);
                for (uint8 hit = 0; hit < 3 && !enemies.empty(); ++hit)
                {
                    std::size_t const index = urand(0, uint32(enemies.size() - 1));
                    player->CastSpell(enemies[index], SPELL_STAMPEDE_HIT, true);
                    enemies.erase(enemies.begin() + index);
                }
            }
        }

        // Appel de la nature sauvage: a beast every 4 s
        if (player->HasAura(SPELL_CALL_OF_THE_WILD))
        {
            state->callTimer += diff;
            if (state->callTimer >= CallOfTheWildPulseMs)
            {
                state->callTimer = 0;
                if (Unit* victim = player->GetVictim())
                    CallWildBeast(player, victim);
            }
        }

        // Salve de ricochets: Volley 2 s longer, and Trick Shots again as it ends
        if (state->volleyExtend)
            if (DynamicObject* volley = player->GetDynObject(SPELL_VOLLEY))
            {
                state->volleyExtend = false;
                volley->SetDuration(volley->GetDuration() + 2000);
            }
        if (state->volleyEndMs > 0)
        {
            state->volleyEndMs -= int32(diff);
            if (state->volleyEndMs <= 0)
                player->CastSpell(player, SPELL_TRICK_SHOTS, true);
        }
    }

    // A few times a second: Aspect de la tortue while Deterrence lasts, Loup solitaire without a pet, Mort coordonnée
    // during Coordinated Assault
    static void UpdateHolds(Player* player)
    {
        bool const alive = player->IsAlive();
        Hold(player, SPELL_TURTLE, alive && player->HasAura(TALENT_TURTLE) && player->HasAura(SPELL_DETERRENCE));
        Hold(player, SPELL_LONE_WOLF, alive && player->HasAura(TALENT_LONE_WOLF) && !player->GetPet());
        if (alive && player->HasAura(TALENT_COORDINATED_KILL) && player->HasAura(SPELL_COORDINATED_ASSAULT) &&
            !player->HasAura(SPELL_DEATHBLOW))
            player->AddAura(SPELL_DEATHBLOW, player);
    }

    static void Hold(Player* player, uint32 spellId, bool wanted)
    {
        if (wanted && !player->HasAura(spellId))
            player->AddAura(spellId, player);
        else if (!wanted && player->HasAura(spellId))
            player->RemoveAurasDueToSpell(spellId);
    }
};
}

void AddHunterTalentScripts()
{
    new HunterTalentSpellScript();
    new HunterTalentUnitScript();
    new HunterTalentPlayerScript();
}
