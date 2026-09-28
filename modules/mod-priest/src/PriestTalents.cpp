#include "AllSpellScript.h"
#include "CellImpl.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
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
#include <limits>
#include <list>
#include <vector>

// The Priest's talents and abilities on the retail-style trees (localTools/priest/talentTree.json) that spell data
// cannot carry. A talent is its rank spell's aura on the Priest (learned by mod-custom-classes' TalentTree.cpp), read
// here with HasAura; a specialization is its passive (Discipline, Sacré, Ombre). The abilities and auras named below
// are in localTools/priest/Spells.ps1, the stock spells changed in place in StockSpells.ps1; the WotLK talents the
// trees reuse keep their own scripts in the core (spell_priest.cpp).
//
// Discipline heals through Atonement (Expiation): Power Word: Shield, Power Word: Radiance, Flash Heal and Renew put it
// on an ally, and the Priest's damage (its fiend's included) heals every ally carrying it. Holy's Holy Words recharge
// as it heals, and Echo of Light rolls a share of every heal into a heal over time. Shadow fights with Insanity
// (Démence), an aura of up to 100 stacks: its builders fill it, Devouring Plague spends it. Power Word: Radiance, Mind
// Blast (Pensées du Vide) and Holy Word: Serenity (Faiseur de miracles) have charges kept here, shown as stacks.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_PROTECTIVE_LIGHT         = 93402,    // Lumière protectrice
    TALENT_DEATH_AND_MADNESS        = 93403,    // Mort et folie
    TALENT_TWINS                    = 93408,    // Jumeaux de la prêtresse du soleil
    TALENT_TRANSLUCENT_IMAGE        = 93409,    // Image translucide
    TALENT_PENITENT_SHADOWS         = 93412,    // Ombres pénitentes
    TALENT_SINS_OF_THE_MANY         = 93413,    // Péchés du grand nombre
    TALENT_DARK_SIDE                = 93414,    // Pouvoir du côté obscur
    TALENT_SHADOW_COVENANT          = 93415,    // Alliance ténébreuse
    TALENT_LASTING_ATONEMENT_1      = 93416,    // Expiation prolongée
    TALENT_LASTING_ATONEMENT_2      = 93417,
    TALENT_STRONG_ATONEMENT_1       = 93422,    // Expiation renforcée
    TALENT_STRONG_ATONEMENT_2       = 93423,
    TALENT_MALICIOUS_SCHISM         = 93424,    // Schisme malveillant
    TALENT_LINGERING_RADIANCE       = 93425,    // Radiance rémanente
    TALENT_SURGING_LIGHT            = 93426,    // Déferlante de lumière
    TALENT_RELENTLESS_PENANCE       = 93427,    // Pénitence inéluctable
    TALENT_LIGHTWEAVER              = 93428,    // Tisse-lumière
    TALENT_TRAIL_OF_LIGHT_1         = 93429,    // Sillage de lumière
    TALENT_TRAIL_OF_LIGHT_2         = 93430,
    TALENT_STRONG_ECHO_1            = 93431,    // Écho renforcé
    TALENT_STRONG_ECHO_2            = 93432,
    TALENT_RENEWED_FAITH            = 93433,    // Foi renouvelée
    TALENT_MIRACLE_WORKER           = 93434,    // Faiseur de miracles
    TALENT_COSMIC_RIPPLE            = 93435,    // Ondulation cosmique
    TALENT_GUIDING_LIGHT            = 93443,    // Guide de la lumière
    TALENT_SHADOWY_APPARITIONS      = 93446,    // Apparitions ténébreuses
    TALENT_MISERY                   = 93447,    // Détresse
    TALENT_PSYCHIC_LINK_1           = 93448,    // Lien psychique
    TALENT_PSYCHIC_LINK_2           = 93449,
    TALENT_AUSPICIOUS_SPIRITS       = 93450,    // Esprits propices
    TALENT_SHADOWY_INSIGHT          = 93451,    // Intuition ténébreuse
    TALENT_MIND_DEVOURER            = 93452,    // Dévoreur d'esprit
    TALENT_MIND_FLAY_INSANITY       = 93453,    // Fouet mental : Démence
    TALENT_VOID_THOUGHTS            = 93454,    // Pensées du Vide
    TALENT_DEATHSPEAKER             = 93457,    // Porte-mort
    TALENT_OVERFLOWING_MADNESS_1    = 93458,    // Folie débordante
    TALENT_OVERFLOWING_MADNESS_2    = 93459,
    TALENT_MASTER_OF_SHADOWS        = 93460,    // Maître des ombres
    TALENT_WHISPERING_SHADOWS       = 93461,    // Ombres murmurantes
    TALENT_COUNTLESS_APPARITIONS    = 93462,    // Apparitions innombrables
};

// The abilities and auras of localTools/priest/Spells.ps1
enum Spells : uint32
{
    SPELL_INSANITY                  = 93500,
    SPELL_ATONEMENT                 = 93501,
    SPELL_ATONEMENT_HEAL            = 93502,
    SPELL_PROTECTIVE_LIGHT          = 93503,
    SPELL_TRANSLUCENT_IMAGE         = 93504,
    SPELL_SHADOW_COVENANT           = 93505,
    SPELL_DARK_SIDE                 = 93506,
    SPELL_RADIANCE_CHARGES          = 93507,
    SPELL_MIND_BLAST_CHARGES        = 93508,
    SPELL_SERENITY_CHARGES          = 93509,
    SPELL_POWER_WORD_LIFE           = 93510,
    SPELL_LEAP_OF_FAITH             = 93511,
    SPELL_DIVINE_STAR               = 93513,
    SPELL_HALO                      = 93514,
    SPELL_DIVINE_STAR_DAMAGE        = 93515,
    SPELL_DIVINE_STAR_HEAL          = 93516,
    SPELL_HALO_DAMAGE               = 93517,
    SPELL_HALO_HEAL                 = 93518,
    SPELL_MINDGAMES                 = 93519,
    SPELL_MINDGAMES_HEAL            = 93520,
    SPELL_RADIANCE                  = 93530,
    SPELL_RADIANCE_HEAL             = 93531,
    SPELL_SCHISM                    = 93532,
    SPELL_SCHISM_MARK               = 93533,
    SPELL_PURGE_THE_WICKED          = 93534,
    SPELL_MINDBENDER                = 93537,
    SPELL_RAPTURE                   = 93539,
    SPELL_EVANGELISM                = 93540,
    SPELL_SURGING_LIGHT_HEAL        = 93541,
    SPELL_SERENITY                  = 93550,
    SPELL_SANCTIFY                  = 93551,
    SPELL_CHASTISE                  = 93552,
    SPELL_ECHO_OF_LIGHT             = 93553,
    SPELL_APOTHEOSIS                = 93554,
    SPELL_LIGHTWEAVER               = 93555,
    SPELL_SANCTIFY_HEAL             = 93556,
    SPELL_COSMIC_RIPPLE_HEAL        = 93557,
    SPELL_TRAIL_OF_LIGHT_HEAL       = 93558,
    SPELL_DEVOURING_PLAGUE_HIT      = 93560,
    SPELL_VOID_BOLT                 = 93561,
    SPELL_SHADOWY_INSIGHT           = 93562,
    SPELL_VOID_ERUPTION             = 93563,
    SPELL_VOID_ERUPTION_SPLASH      = 93564,
    SPELL_VOIDFORM                  = 93565,
    SPELL_DARK_ASCENSION            = 93567,
    SPELL_APPARITION                = 93568,
    SPELL_SHADOW_CRASH              = 93569,
    SPELL_VOID_TORRENT_TICK         = 93571,
    SPELL_PSYCHIC_LINK              = 93572,
    SPELL_MIND_DEVOURER             = 93573,
    SPELL_MIND_FLAY_INSANITY        = 93574,
    SPELL_DEATHSPEAKER              = 93575,

    // The specializations' passives
    SPELL_SPEC_DISCIPLINE           = 93680,
    SPELL_SPEC_HOLY                 = 93681,
    SPELL_SPEC_SHADOW               = 93682,

    // Stock (first ranks)
    SPELL_POWER_WORD_SHIELD_R1      = 17,
    SPELL_RENEW_R1                  = 139,
    SPELL_SMITE_R1                  = 585,
    SPELL_FADE                      = 586,
    SPELL_SHADOW_WORD_PAIN_R1       = 589,
    SPELL_PRAYER_OF_HEALING_R1      = 596,
    SPELL_LESSER_HEAL_R1            = 2050,
    SPELL_HEAL_R1                   = 2054,
    SPELL_GREATER_HEAL_R1           = 2060,
    SPELL_FLASH_HEAL_R1             = 2061,
    SPELL_DEVOURING_PLAGUE_R1       = 2944,
    SPELL_MIND_BLAST_R1             = 8092,
    SPELL_POWER_INFUSION            = 10060,
    SPELL_MIND_FLAY_R1              = 15407,
    SPELL_SHADOW_WORD_DEATH_R1      = 32379,
    SPELL_SHADOWFIEND               = 34433,
    SPELL_VAMPIRIC_TOUCH_R1         = 34914,
    SPELL_PENANCE_R1                = 47540,
};

// The fiend of Shadowfiend and Torve-esprit (its summon is Shadowfiend's)
constexpr uint32 NpcShadowfiend = 19668;

// The family flags the hooks tell the stock ticks apart by (SPELLFAMILY_PRIEST)
constexpr uint32 FlagShadowWordPain = 0x8000;       // word 0
constexpr uint32 FlagMindFlayTick = 0x800000;       // word 0
constexpr uint32 FlagRenew = 0x40;                  // word 0
constexpr uint32 FlagVampiricTouch = 0x400;         // word 1
constexpr uint32 FlagPenanceDamage = 0x8000;        // word 1
constexpr uint32 FlagPenanceHeal = 0x10000;         // word 1
constexpr uint32 FlagMindSearTick = 0x80000;        // word 1

// --- Tuning (README.md) ----------------------------------------------------------------------------------------------
// Discipline: each ally with Atonement is healed for this share of the Priest's damage
constexpr float AtonementShare = 0.35f;
constexpr int32 AtonementMs = 15000;
constexpr float RadianceAtonementShare = 0.6f;      // of AtonementMs, without Radiance rémanente
constexpr uint8 RadianceExtraTargets = 4;
constexpr float AtonementRange = 60.0f;
constexpr uint32 AtonementFlushMs = 250;            // the damage of this long is healed at once
constexpr float SinsPerAlly = 0.03f;                // Péchés du grand nombre
constexpr float SinsMax = 0.15f;
constexpr float SchismFactor = 1.15f;
constexpr float MaliciousSchismFactor = 1.25f;
constexpr int32 SchismMs = 9000;
constexpr float DarkSideFactor = 1.5f;              // Pouvoir du côté obscur
constexpr float PenitentShadowsFactor = 1.3f;       // Ombres pénitentes
constexpr int32 EvangelismMs = 6000;
constexpr float SurgingLightPct = 15.0f;            // Déferlante de lumière, of the ally's health
// Holy: Echo of Light's share of each heal (plus 5% a rank of Écho renforcé), over its 3 ticks
constexpr float EchoShare = 0.2f;
constexpr uint8 EchoTicks = 3;
constexpr int32 EchoTickMs = 2000;
constexpr int32 SerenityPerHealMs = 6000;           // Flash Heal, Heal, Greater Heal
constexpr int32 SanctifyPerPrayerMs = 6000;         // Prayer of Healing
constexpr int32 SanctifyPerRenewMs = 2000;
constexpr int32 ChastisePerSmiteMs = 4000;
constexpr float ApotheosisRate = 4.0f;
constexpr uint8 SanctifyExtraTargets = 5;
constexpr uint8 CosmicRippleTargets = 5;
constexpr float RenewedFaithFactor = 1.1f;
constexpr float GuidingLightFactor = 1.5f;
constexpr float PowerWordLifePct = 35.0f;
// Shadow: Insanity
constexpr uint8 InsanityMax = 100;
constexpr uint8 DevouringPlagueCost = 50;
constexpr uint8 MasterOfShadowsCost = 35;
constexpr uint8 InsanityMindBlast = 8;
constexpr uint8 InsanityVampiricTouch = 5;
constexpr uint8 InsanityShadowWordPain = 4;
constexpr uint8 InsanityShadowWordDeath = 5;
constexpr uint8 InsanityMindFlayTick = 3;
constexpr uint8 InsanityMindSearTick = 1;           // each enemy, 3 a tick at most
constexpr uint8 InsanityVoidBolt = 12;
constexpr uint8 InsanityVoidTorrentTick = 8;
constexpr uint8 InsanityShadowCrash = 6;
constexpr uint8 InsanityDarkAscension = 30;
constexpr uint8 InsanityFiendHit = 2;
constexpr uint8 InsanityDeathAndMadness = 10;
constexpr uint32 InsanityFadeMs = 10000;
// Shadow: damage the module works out, shares of the Priest's shadow spell power
constexpr float ApparitionPower = 0.3f;
constexpr float AuspiciousFactor = 1.15f;
constexpr float EruptionSplashPower = 1.0f;
constexpr float DevouringPlagueTickFactor = 1.5f;   // the stock ranks' 4 ticks in 6 s
constexpr float DarkAscensionPeriodicFactor = 1.25f;
constexpr float MindFlayInsanityFactor = 1.5f;
constexpr float ShadowWordDeathExecute = 2.5f;
constexpr float ExecutePct = 20.0f;
constexpr int32 VoidBoltExtendMs = 3000;
constexpr uint8 ShadowCrashTargets = 5;
constexpr uint8 WhisperingShadowsTargets = 9;
// Packs (the combat bench, Fire mage as the reference): the splashes reach AreaMaxTargets, each hit whole up to
// AreaFullTargets enemies and sqrt(AreaFullTargets / enemies) of it past them - Mind Sear too, which has no cap
constexpr uint8 AreaFullTargets = 5;
constexpr uint8 AreaMaxTargets = 12;
constexpr float DotRange = 40.0f;                   // the enemies the apparitions and Psychic Link reach
constexpr float EruptionRange = 10.0f;
constexpr float MindSearRadius = 10.0f;
// Class tree: Divine Star's lane, Halo's ring
constexpr float StarLength = 24.0f;
constexpr float StarWidth = 4.0f;
constexpr float HaloRange = 30.0f;
constexpr uint8 StarHealTargets = 6;
constexpr float HealRange = 40.0f;

constexpr uint32 HoldCheckMs = 250;
constexpr uint32 BoostWindowMs = 3000;
constexpr uint8 LightweaverMax = 2;

enum ChargeKind : uint8
{
    CHARGE_RADIANCE,
    CHARGE_MIND_BLAST,
    CHARGE_SERENITY,
    MAX_CHARGE_KINDS
};

struct ChargeDefinition
{
    uint32 spellId;     // first rank
    uint32 displayAura;
};

constexpr std::array<ChargeDefinition, MAX_CHARGE_KINDS> ChargeDefinitions = {{
    { SPELL_RADIANCE, SPELL_RADIANCE_CHARGES },
    { SPELL_MIND_BLAST_R1, SPELL_MIND_BLAST_CHARGES },
    { SPELL_SERENITY, SPELL_SERENITY_CHARGES },
}};

struct Charges
{
    bool initialized = false;
    uint8 count = 0;
    int32 rechargeMs = 0;
    uint32 spellId = 0;             // the rank last cast
    bool clearCooldown = false;     // the cooldown its cast just started is taken back on the next update
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct PriestState : public DataMap::Base
{
    std::array<Charges, MAX_CHARGE_KINDS> charges;
    uint64 atonementDamage = 0;     // damage dealt since the last flush, healed on every atoned ally
    uint32 atonementTimer = 0;
    uint8 atonedCount = 0;          // Péchés du grand nombre, counted at each flush
    uint32 outOfCombatMs = 0;
    uint32 holdTimer = 0;
    uint32 darkSideUntilMs = 0;     // this Penance's bolts are empowered
    uint32 flayInsanityUntilMs = 0; // this Mind Flay is Mind Flay: Insanity
    uint32 deathspeakerUntilMs = 0;
    bool clearShieldCooldown = false;
    bool clearPenanceCooldown = false;
    ObjectGuid lastFlashTarget;     // Sillage de lumière
    // Shadow Crash reaches several enemies at once: its Vampiric Touch lands on a few of them per cast
    uint32 crashStamp = 0;
    uint8 crashHits = 0;
    // Mind Sear: its Insanity per tick, and the enemies it reaches (counted twice a second)
    uint32 searStamp = 0;
    uint8 searHits = 0;
    uint32 searCountStamp = 0;
    uint8 searCount = 1;
    // Divine Star and Halo: the falloff of the pack they hit
    float starFalloff = 1.0f;
    uint32 starUntilMs = 0;
};

constexpr char const* StateKey = "PriestTalentState";

PriestState* GetState(Player* player)
{
    return player->CustomData.GetDefault<PriestState>(StateKey);
}

Player* Priest(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_PRIEST ? player : nullptr;
}

// The Priest who owns a fiend
Player* FiendOwner(Unit* unit)
{
    if (!unit || unit->IsPlayer() || unit->GetEntry() != NpcShadowfiend)
        return nullptr;
    return Priest(unit->GetOwner());
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsDiscipline(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_DISCIPLINE);
}

bool IsHoly(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_HOLY);
}

bool IsShadow(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_SHADOW);
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

bool IsPriestSpell(SpellInfo const* spellInfo, uint32 word0, uint32 word1)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_PRIEST &&
        ((word0 && (spellInfo->SpellFamilyFlags[0] & word0)) || (word1 && (spellInfo->SpellFamilyFlags[1] & word1)));
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

// An aura of the Priest's on a unit, one more stack (up to max), its duration refreshed
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

// An aura the Priest holds a little longer (Expiation prolongée, Folie débordante, Schisme malveillant)
void Lengthen(Aura* aura, int32 durationMs)
{
    if (!aura || durationMs <= 0)
        return;
    if (durationMs > aura->GetMaxDuration())
        aura->SetMaxDuration(durationMs);
    aura->SetDuration(durationMs);
}

int32 Amount(float value)
{
    return int32(std::clamp(value, 1.0f, float(std::numeric_limits<int32>::max() / 2)));
}

float ShadowPower(Player* player)
{
    return float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW));
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

// Friendly units in the Priest's group (itself and pets included) within range of center, most injured first
std::vector<Unit*> InjuredAllies(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> units;
    Acore::AnyFriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(center, units, check);
    Cell::VisitObjects(center, searcher, range);

    std::vector<Unit*> allies;
    for (Unit* unit : units)
        if (unit->IsAlive() && !unit->IsFullHealth() && (unit == player || player->IsInRaidWith(unit)))
            allies.push_back(unit);
    std::sort(allies.begin(), allies.end(), [](Unit const* left, Unit const* right)
    {
        return left->GetHealthPct() < right->GetHealthPct();
    });
    return allies;
}

// The Priest and its group's players, alive and near
std::vector<Player*> GroupPlayers(Player* player, float range)
{
    std::vector<Player*> players;
    Group* group = player->GetGroup();
    if (!group)
    {
        players.push_back(player);
        return players;
    }
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->IsInMap(player) &&
            (member == player || member->IsWithinDistInMap(player, range)))
            players.push_back(member);
    }
    return players;
}

void Heal(Player* player, Unit* target, uint32 spellId, float amount)
{
    if (!target || amount < 1.0f || !target->IsAlive())
        return;
    int32 const heal = Amount(amount);
    player->CastCustomSpell(target, spellId, &heal, nullptr, nullptr, true);
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

void ModifyChainCooldown(Player* player, uint32 spellId, int32 delta)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->ModifySpellCooldown(rank, delta);
}

// The highest rank of a chain the player knows (Shadow Word: Pain for Détresse), 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

// --- Insanity -----------------------------------------------------------------------------------------------------

uint8 Insanity(Player const* player)
{
    return Stacks(player, SPELL_INSANITY);
}

void AddInsanity(Player* player, int32 amount)
{
    if (!amount || !IsShadow(player))
        return;
    int32 const value = std::clamp(int32(Insanity(player)) + amount, 0, int32(InsanityMax));
    ShowStacks(player, SPELL_INSANITY, uint8(value));
}

// What Devouring Plague costs now: 50, 35 during Voidform or Dark Ascension with Maître des ombres, nothing with
// Dévoreur d'esprit's proc
uint8 PlagueCost(Player const* player)
{
    if (player->HasAura(SPELL_MIND_DEVOURER))
        return 0;
    if (player->HasAura(TALENT_MASTER_OF_SHADOWS) &&
        (player->HasAura(SPELL_VOIDFORM) || player->HasAura(SPELL_DARK_ASCENSION)))
        return MasterOfShadowsCost;
    return DevouringPlagueCost;
}

// --- Charges ------------------------------------------------------------------------------------------------------

uint8 MaxCharges(Player* player, ChargeKind kind)
{
    switch (kind)
    {
        case CHARGE_RADIANCE:
            return IsDiscipline(player) ? 2 : 1;
        case CHARGE_MIND_BLAST:
            return IsShadow(player) && player->HasAura(TALENT_VOID_THOUGHTS) ? 2 : 1;
        case CHARGE_SERENITY:
            return IsHoly(player) && player->HasAura(TALENT_MIRACLE_WORKER) ? 2 : 1;
        default:
            return 1;
    }
}

// A rank's cooldown with the Priest's modifiers
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

void SpendCharge(Player* player, PriestState* state, ChargeKind kind, uint32 spellId)
{
    uint8 const max = MaxCharges(player, kind);
    Charges& charges = state->charges[kind];
    if (max <= 1)
    {
        charges.spellId = spellId;
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

// A cooldown reduction (the Holy Words): on the charge coming back, or on the cooldown
void ReduceRecharge(Player* player, PriestState* state, ChargeKind kind, int32 ms)
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

// Every charge back at once (Void Eruption, Intuition ténébreuse)
void RefillCharges(Player* player, PriestState* state, ChargeKind kind)
{
    ClearChainCooldown(player, ChargeDefinitions[kind].spellId);
    if (!ChargesManaged(player, kind))
        return;
    Charges& charges = state->charges[kind];
    charges.initialized = true;
    charges.count = MaxCharges(player, kind);
    charges.rechargeMs = 0;
    charges.clearCooldown = false;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

void UpdateCharges(Player* player, PriestState* state, ChargeKind kind, uint32 diff)
{
    Charges& charges = state->charges[kind];
    uint8 const max = MaxCharges(player, kind);
    uint32 const display = ChargeDefinitions[kind].displayAura;
    if (max <= 1)
    {
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

    charges.rechargeMs -= int32(diff);
    if (charges.rechargeMs > 0)
        return;

    // A charge back: usable again if it was the last one gone
    if (!charges.count)
        ClearChainCooldown(player, spellId);
    ++charges.count;
    charges.rechargeMs = charges.count < max ? Recharge(player, spellId) + charges.rechargeMs : 0;
    ShowStacks(player, display, charges.count);
}

// --- Discipline: Atonement -------------------------------------------------------------------------------------------

void ApplyAtonement(Player* player, Unit* target, float share = 1.0f)
{
    if (!target || !target->IsAlive() || !target->IsPlayer() || !IsDiscipline(player))
        return;
    int32 const duration = int32(float(AtonementMs) * share) +
        3000 * Rank(player, TALENT_LASTING_ATONEMENT_1, TALENT_LASTING_ATONEMENT_2);
    Aura* aura = target->GetAura(SPELL_ATONEMENT, player->GetGUID());
    if (!aura)
        aura = player->AddAura(SPELL_ATONEMENT, target);
    if (aura && aura->GetDuration() < duration)
        Lengthen(aura, duration);
}

// The group's players carrying the Priest's Atonement
std::vector<Player*> AtonedAllies(Player* player)
{
    std::vector<Player*> atoned;
    for (Player* member : GroupPlayers(player, AtonementRange))
        if (member->HasAura(SPELL_ATONEMENT, player->GetGUID()))
            atoned.push_back(member);
    return atoned;
}

// The damage of the last quarter second heals every atoned ally
void FlushAtonement(Player* player, PriestState* state)
{
    uint64 const damage = state->atonementDamage;
    state->atonementDamage = 0;
    std::vector<Player*> const atoned = AtonedAllies(player);
    state->atonedCount = uint8(std::min<std::size_t>(atoned.size(), 40));
    if (!damage || atoned.empty())
        return;
    float const share = AtonementShare *
        (1.0f + 0.1f * float(Rank(player, TALENT_STRONG_ATONEMENT_1, TALENT_STRONG_ATONEMENT_2)));
    float const amount = float(std::min<uint64>(damage, uint64(std::numeric_limits<int32>::max()))) * share;
    for (Player* ally : atoned)
        Heal(player, ally, SPELL_ATONEMENT_HEAL, amount);
}

// Péchés du grand nombre and Schism: what the Priest's damage on a target is worth
float DisciplineFactor(Player* player, PriestState* state, Unit* target)
{
    float factor = 1.0f;
    if (player->HasAura(TALENT_SINS_OF_THE_MANY) && state->atonedCount)
        factor *= 1.0f + std::min(SinsMax, SinsPerAlly * float(state->atonedCount));
    if (target->HasAura(SPELL_SCHISM_MARK, player->GetGUID()))
        factor *= player->HasAura(TALENT_MALICIOUS_SCHISM) ? MaliciousSchismFactor : SchismFactor;
    return factor;
}

// Penance at an enemy spreads Purge the Wicked (or, with Ombres pénitentes, Shadow Word: Pain) to one enemy near it
void SpreadWithPenance(Player* player, Unit* target)
{
    uint32 spellId = 0;
    if (player->HasSpell(SPELL_PURGE_THE_WICKED))
        spellId = SPELL_PURGE_THE_WICKED;
    else if (player->HasAura(TALENT_PENITENT_SHADOWS))
        spellId = KnownRank(player, SPELL_SHADOW_WORD_PAIN_R1);
    if (!spellId || !target->HasAura(spellId, player->GetGUID()))
        return;
    std::list<Unit*> enemies = EnemiesNear(player, target, 10.0f);
    enemies.sort([target](Unit const* left, Unit const* right)
    {
        return target->GetExactDist2d(left) < target->GetExactDist2d(right);
    });
    for (Unit* enemy : enemies)
        if (!enemy->HasAura(spellId, player->GetGUID()))
        {
            player->CastSpell(enemy, spellId, true);
            return;
        }
}

// --- Holy -----------------------------------------------------------------------------------------------------------

// A Holy Word's cooldown brought closer; four times as much during Apotheosis
void HolyWord(Player* player, PriestState* state, uint32 spellId, int32 ms)
{
    if (!IsHoly(player) || !player->HasSpell(spellId))
        return;
    if (player->HasAura(SPELL_APOTHEOSIS))
        ms = int32(float(ms) * ApotheosisRate);
    if (spellId == SPELL_SERENITY)
        ReduceRecharge(player, state, CHARGE_SERENITY, ms);
    else if (player->HasSpellCooldown(spellId))
        player->ModifySpellCooldown(spellId, -ms);
}

// Echo of Light: a share of the heal rolled into the heal over time the target already carries (changed in place:
// this runs inside a heal, a heal over time's tick included)
void EchoOfLight(Player* player, Unit* target, uint32 heal)
{
    float const share = EchoShare + 0.05f * float(Rank(player, TALENT_STRONG_ECHO_1, TALENT_STRONG_ECHO_2));
    float total = float(heal) * share;
    if (Aura* echo = target->GetAura(SPELL_ECHO_OF_LIGHT, player->GetGUID()))
        if (AuraEffect* effect = echo->GetEffect(EFFECT_0))
        {
            int32 const ticksLeft = (std::max(0, echo->GetDuration()) + EchoTickMs - 1) / EchoTickMs;
            total += float(effect->GetAmount()) * float(ticksLeft);
            effect->ChangeAmount(Amount(total / float(EchoTicks)));
            echo->RefreshDuration();
            return;
        }
    int32 const tick = Amount(total / float(EchoTicks));
    player->CastCustomSpell(target, SPELL_ECHO_OF_LIGHT, &tick, nullptr, nullptr, true);
}

// Ondulation cosmique: a Holy Word also heals the most injured allies around the Priest
void CosmicRipple(Player* player)
{
    if (!player->HasAura(TALENT_COSMIC_RIPPLE))
        return;
    std::vector<Unit*> const allies = InjuredAllies(player, player, HealRange);
    for (std::size_t index = 0; index < allies.size() && index < CosmicRippleTargets; ++index)
        player->CastSpell(allies[index], SPELL_COSMIC_RIPPLE_HEAL, true);
}

// --- Shadow ----------------------------------------------------------------------------------------------------------

// The enemies near the Priest carrying its Vampiric Touch, `skip` left out, AreaMaxTargets at most
std::vector<Unit*> TouchedEnemies(Player* player, Unit* skip)
{
    std::vector<Unit*> touched;
    for (Unit* enemy : EnemiesNear(player, player, DotRange))
    {
        if (touched.size() >= AreaMaxTargets)
            break;
        if (enemy != skip && enemy->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_PRIEST, 0,
            FlagVampiricTouch, 0, player->GetGUID()))
            touched.push_back(enemy);
    }
    return touched;
}

// Apparitions ténébreuses: Mind Blast, Void Bolt and Devouring Plague send an apparition at every enemy the Priest's
// Vampiric Touch burns
void Apparitions(Player* player)
{
    if (!player->HasAura(TALENT_SHADOWY_APPARITIONS))
        return;
    std::vector<Unit*> const touched = TouchedEnemies(player, nullptr);
    if (touched.empty())
        return;
    bool const auspicious = player->HasAura(TALENT_AUSPICIOUS_SPIRITS);
    float amount = ShadowPower(player) * ApparitionPower * AreaFalloff(touched.size());
    if (auspicious)
        amount *= AuspiciousFactor;
    int32 insanity = 0;
    for (Unit* enemy : touched)
    {
        uint8 const count = player->HasAura(TALENT_COUNTLESS_APPARITIONS) && roll_chance_i(25) ? 2 : 1;
        for (uint8 apparition = 0; apparition < count; ++apparition)
        {
            Strike(player, enemy, SPELL_APPARITION, amount);
            insanity += auspicious ? 2 : 1;
        }
    }
    AddInsanity(player, insanity);
}

// Lien psychique: a share of the hit on every other enemy the Priest's Vampiric Touch burns
void PsychicLink(Player* player, Unit* victim, uint32 damage)
{
    uint8 const rank = Rank(player, TALENT_PSYCHIC_LINK_1, TALENT_PSYCHIC_LINK_2);
    if (!rank)
        return;
    std::vector<Unit*> const touched = TouchedEnemies(player, victim);
    if (touched.empty())
        return;
    float const share = rank > 1 ? 0.35f : 0.2f;
    float const amount = float(damage) * share * AreaFalloff(touched.size() + 1);
    for (Unit* enemy : touched)
        Strike(player, enemy, SPELL_PSYCHIC_LINK, amount);
}

// Void Bolt: the target's Shadow Word: Pain and Vampiric Touch last 3 s longer
void ExtendDots(Player* player, Unit* target)
{
    if (!target)
        return;
    AuraEffect* const pain = target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_PRIEST,
        FlagShadowWordPain, 0, 0, player->GetGUID());
    AuraEffect* const touch = target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_PRIEST, 0,
        FlagVampiricTouch, 0, player->GetGUID());
    for (AuraEffect* effect : { pain, touch })
        if (effect)
            Lengthen(effect->GetBase(), effect->GetBase()->GetDuration() + VoidBoltExtendMs);
}

// Void Eruption: the enemies near the target, and Voidform
void VoidEruption(Player* player, PriestState* state, Unit* target)
{
    if (target && target->IsAlive())
    {
        std::list<Unit*> const enemies = EnemiesNear(player, target, EruptionRange);
        std::size_t const count = std::min<std::size_t>(enemies.size(), AreaMaxTargets - 1);
        float const amount = ShadowPower(player) * EruptionSplashPower * AreaFalloff(count + 1);
        std::size_t hit = 0;
        for (Unit* enemy : enemies)
        {
            if (hit++ >= count)
                break;
            Strike(player, enemy, SPELL_VOID_ERUPTION_SPLASH, amount);
        }
    }
    if (Aura* voidform = player->AddAura(SPELL_VOIDFORM, player))
        Lengthen(voidform, voidform->GetMaxDuration() +
            3000 * Rank(player, TALENT_OVERFLOWING_MADNESS_1, TALENT_OVERFLOWING_MADNESS_2));
    RefillCharges(player, state, CHARGE_MIND_BLAST);
}

// --- Class tree ------------------------------------------------------------------------------------------------------

// Divine Star and Halo reach a whole pack: past five enemies each hit takes less (ModifySpellDamageTaken)
void SetStarFalloff(PriestState* state, std::size_t enemies)
{
    state->starFalloff = AreaFalloff(enemies);
    state->starUntilMs = Now() + BoostWindowMs;
}

// Divine Star: out 24 yd along the Priest's facing and back, striking the enemies and healing the allies in its lane
void DivineStar(Player* player, PriestState* state)
{
    float const facing = player->GetOrientation();
    auto inLane = [player, facing](Unit const* unit)
    {
        float const dx = unit->GetPositionX() - player->GetPositionX();
        float const dy = unit->GetPositionY() - player->GetPositionY();
        float const along = dx * std::cos(facing) + dy * std::sin(facing);
        float const across = std::fabs(-dx * std::sin(facing) + dy * std::cos(facing));
        return along >= 0.0f && along <= StarLength + unit->GetCombatReach() &&
            across <= StarWidth + unit->GetCombatReach();
    };

    std::vector<Unit*> enemies;
    for (Unit* enemy : EnemiesNear(player, player, StarLength + 5.0f))
        if (inLane(enemy) && enemies.size() < AreaMaxTargets)
            enemies.push_back(enemy);
    std::vector<Unit*> allies;
    for (Unit* ally : InjuredAllies(player, player, StarLength + 5.0f))
        if (inLane(ally) && allies.size() < StarHealTargets)
            allies.push_back(ally);

    SetStarFalloff(state, enemies.size());
    for (uint8 pass = 0; pass < 2; ++pass)
    {
        for (Unit* enemy : enemies)
            if (enemy->IsAlive())
                player->CastSpell(enemy, SPELL_DIVINE_STAR_DAMAGE, true);
        for (Unit* ally : allies)
            if (ally->IsAlive())
                player->CastSpell(ally, SPELL_DIVINE_STAR_HEAL, true);
    }
}

// Halo: every enemy and the 6 most injured allies within 30 yd
void Halo(Player* player, PriestState* state)
{
    std::list<Unit*> const enemies = EnemiesNear(player, player, HaloRange);
    std::size_t const count = std::min<std::size_t>(enemies.size(), AreaMaxTargets);
    SetStarFalloff(state, count);
    std::size_t hit = 0;
    for (Unit* enemy : enemies)
    {
        if (hit++ >= count)
            break;
        player->CastSpell(enemy, SPELL_HALO_DAMAGE, true);
    }
    std::vector<Unit*> const allies = InjuredAllies(player, player, HaloRange);
    for (std::size_t index = 0; index < allies.size() && index < StarHealTargets; ++index)
        player->CastSpell(allies[index], SPELL_HALO_HEAL, true);
}

// Leap of Faith: the ally is pulled to the Priest (knocked away from a point beyond it)
void LeapOfFaith(Player* player, Unit* target)
{
    if (!target || target == player || !target->IsAlive() || !player->IsInMap(target))
        return;
    float const distance = target->GetExactDist2d(player);
    if (distance < 3.0f)
        return;
    float const angle = player->GetAngle(target);
    float const x = target->GetPositionX() + std::cos(angle) * 5.0f;
    float const y = target->GetPositionY() + std::sin(angle) * 5.0f;
    float const speedZ = 10.0f;
    float const flight = 2.0f * speedZ / 19.29f;
    target->KnockbackFrom(x, y, (distance - 2.0f) / flight, speedZ);
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

class PriestTalentSpellScript : public AllSpellScript
{
public:
    PriestTalentSpellScript() : AllSpellScript("PriestTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // A spell kept on charges cannot be cast with none left; Devouring Plague needs its Insanity, Void Bolt Voidform,
    // Power Word: Life an ally below 35%
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Priest(spell->GetCaster());
        if (!player)
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        uint32 const firstRank = FirstRank(spellInfo);
        PriestState* state = GetState(player);
        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
        {
            if (ChargeDefinitions[kind].spellId != firstRank || !ChargesManaged(player, ChargeKind(kind)))
                continue;
            Charges const& charges = state->charges[kind];
            if (charges.initialized && !charges.count)
            {
                result = SPELL_FAILED_NOT_READY;
                return;
            }
        }

        switch (firstRank)
        {
            case SPELL_DEVOURING_PLAGUE_R1:
                if (Insanity(player) < PlagueCost(player))
                    result = SPELL_FAILED_CASTER_AURASTATE;
                break;
            case SPELL_VOID_BOLT:
                if (!player->HasAura(SPELL_VOIDFORM))
                    result = SPELL_FAILED_CASTER_AURASTATE;
                break;
            case SPELL_POWER_WORD_LIFE:
            {
                Unit* target = spell->m_targets.GetUnitTarget();
                if (!target)
                    target = player;
                if (target->GetHealthPct() >= PowerWordLifePct)
                    result = SPELL_FAILED_TARGET_AURASTATE;
                break;
            }
            case SPELL_LEAP_OF_FAITH:
                if (spell->m_targets.GetUnitTarget() == player)
                    result = SPELL_FAILED_BAD_TARGETS;
                break;
            default:
                break;
        }
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Priest(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        PriestState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        switch (id)
        {
            // Class tree
            case SPELL_LEAP_OF_FAITH:
                LeapOfFaith(player, target);
                return;
            case SPELL_DIVINE_STAR:
                DivineStar(player, state);
                return;
            case SPELL_HALO:
                Halo(player, state);
                return;
            case SPELL_POWER_INFUSION:
                // Jumeaux de la prêtresse du soleil
                if (target && target != player && player->HasAura(TALENT_TWINS))
                    player->AddAura(SPELL_POWER_INFUSION, player);
                return;
            case SPELL_FADE:
                if (player->HasAura(TALENT_TRANSLUCENT_IMAGE))
                    player->AddAura(SPELL_TRANSLUCENT_IMAGE, player);
                return;
            // Discipline
            case SPELL_RADIANCE:
                Radiance(player, state, target);
                return;
            case SPELL_SCHISM:
                if (target && target->IsAlive())
                    if (Aura* mark = player->AddAura(SPELL_SCHISM_MARK, target))
                        Lengthen(mark, SchismMs + (player->HasAura(TALENT_MALICIOUS_SCHISM) ? 3000 : 0));
                return;
            case SPELL_MINDBENDER:
            case SPELL_SHADOWFIEND:
                if (player->HasAura(TALENT_SHADOW_COVENANT))
                    player->AddAura(SPELL_SHADOW_COVENANT, player);
                return;
            case SPELL_EVANGELISM:
                Evangelism(player);
                return;
            // Holy
            case SPELL_SERENITY:
                SpendCharge(player, state, CHARGE_SERENITY, id);
                CosmicRipple(player);
                return;
            case SPELL_SANCTIFY:
                if (target)
                {
                    uint8 healed = 0;
                    for (Unit* ally : InjuredAllies(player, target, 10.0f))
                    {
                        if (healed >= SanctifyExtraTargets)
                            break;
                        if (ally == target)
                            continue;
                        player->CastSpell(ally, SPELL_SANCTIFY_HEAL, true);
                        ++healed;
                    }
                }
                CosmicRipple(player);
                return;
            // Shadow
            case SPELL_VOID_ERUPTION:
                VoidEruption(player, state, target);
                return;
            case SPELL_VOID_BOLT:
                AddInsanity(player, InsanityVoidBolt);
                ExtendDots(player, target);
                Apparitions(player);
                return;
            case SPELL_DARK_ASCENSION:
                AddInsanity(player, InsanityDarkAscension);
                if (Aura* ascension = player->GetAura(SPELL_DARK_ASCENSION))
                    Lengthen(ascension, ascension->GetMaxDuration() +
                        3000 * Rank(player, TALENT_OVERFLOWING_MADNESS_1, TALENT_OVERFLOWING_MADNESS_2));
                return;
            case SPELL_SHADOW_CRASH:
                AddInsanity(player, InsanityShadowCrash);
                return;
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_POWER_WORD_SHIELD_R1:
                ApplyAtonement(player, target ? target : player);
                // Ravissement: no cooldown while it lasts
                if (player->HasAura(SPELL_RAPTURE))
                    state->clearShieldCooldown = true;
                break;
            case SPELL_FLASH_HEAL_R1:
            {
                Unit* healed = target ? target : player;
                ApplyAtonement(player, healed);
                HolyWord(player, state, SPELL_SERENITY, SerenityPerHealMs);
                // Tisse-lumière
                if (player->HasAura(TALENT_LIGHTWEAVER))
                    AddStack(player, player, SPELL_LIGHTWEAVER, LightweaverMax);
                // Lumière protectrice
                if (healed == player && player->HasAura(TALENT_PROTECTIVE_LIGHT))
                    player->AddAura(SPELL_PROTECTIVE_LIGHT, player);
                break;
            }
            case SPELL_HEAL_R1:
            case SPELL_LESSER_HEAL_R1:
                HolyWord(player, state, SPELL_SERENITY, SerenityPerHealMs);
                break;
            case SPELL_GREATER_HEAL_R1:
                HolyWord(player, state, SPELL_SERENITY, SerenityPerHealMs);
                player->RemoveAurasDueToSpell(SPELL_LIGHTWEAVER);
                break;
            case SPELL_RENEW_R1:
                ApplyAtonement(player, target ? target : player);
                HolyWord(player, state, SPELL_SANCTIFY, SanctifyPerRenewMs);
                break;
            case SPELL_PRAYER_OF_HEALING_R1:
                HolyWord(player, state, SPELL_SANCTIFY, SanctifyPerPrayerMs);
                break;
            case SPELL_SMITE_R1:
                HolyWord(player, state, SPELL_CHASTISE, ChastisePerSmiteMs);
                break;
            case SPELL_PENANCE_R1:
                // Pouvoir du côté obscur: this Penance's bolts are empowered
                if (player->HasAura(SPELL_DARK_SIDE))
                {
                    player->RemoveAurasDueToSpell(SPELL_DARK_SIDE);
                    state->darkSideUntilMs = Now() + BoostWindowMs;
                }
                if (target && target->IsAlive() && player->IsValidAttackTarget(target))
                    SpreadWithPenance(player, target);
                // Pénitence inéluctable
                if (player->HasAura(TALENT_RELENTLESS_PENANCE) && roll_chance_i(20))
                    state->clearPenanceCooldown = true;
                break;
            case SPELL_MIND_BLAST_R1:
                SpendCharge(player, state, CHARGE_MIND_BLAST, id);
                player->RemoveAurasDueToSpell(SPELL_SHADOWY_INSIGHT);
                AddInsanity(player, InsanityMindBlast);
                // Dévoreur d'esprit
                if (player->HasAura(TALENT_MIND_DEVOURER) && IsShadow(player) && roll_chance_i(15))
                    player->AddAura(SPELL_MIND_DEVOURER, player);
                Apparitions(player);
                break;
            case SPELL_SHADOW_WORD_PAIN_R1:
                AddInsanity(player, InsanityShadowWordPain);
                break;
            case SPELL_VAMPIRIC_TOUCH_R1:
                AddInsanity(player, InsanityVampiricTouch);
                // Détresse: Shadow Word: Pain with it
                if (player->HasAura(TALENT_MISERY) && target && target->IsAlive())
                    if (uint32 const pain = KnownRank(player, SPELL_SHADOW_WORD_PAIN_R1))
                        player->CastSpell(target, pain, true);
                break;
            case SPELL_SHADOW_WORD_DEATH_R1:
                AddInsanity(player, InsanityShadowWordDeath);
                if (player->HasAura(SPELL_DEATHSPEAKER))
                {
                    player->RemoveAurasDueToSpell(SPELL_DEATHSPEAKER);
                    state->deathspeakerUntilMs = Now() + BoostWindowMs;
                }
                break;
            case SPELL_DEVOURING_PLAGUE_R1:
                DevouringPlague(player, target);
                break;
            case SPELL_MIND_FLAY_R1:
                if (player->HasAura(SPELL_MIND_FLAY_INSANITY))
                {
                    player->RemoveAurasDueToSpell(SPELL_MIND_FLAY_INSANITY);
                    state->flayInsanityUntilMs = Now() + BoostWindowMs + 1000;
                }
                break;
            default:
                break;
        }
    }

private:
    // Power Word: Radiance: its target (the spell's own heal) and the 4 most injured allies within 30 yd of it, all
    // given Atonement for 60% of its duration
    static void Radiance(Player* player, PriestState* state, Unit* target)
    {
        SpendCharge(player, state, CHARGE_RADIANCE, SPELL_RADIANCE);
        Unit* center = target ? target : player;
        float const share = player->HasAura(TALENT_LINGERING_RADIANCE) ? 1.0f : RadianceAtonementShare;
        ApplyAtonement(player, center, share);
        std::vector<Unit*> allies = InjuredAllies(player, center, 30.0f);
        // Atonement where it is missing first, then the most injured
        std::stable_partition(allies.begin(), allies.end(), [player](Unit const* ally)
        {
            return !ally->HasAura(SPELL_ATONEMENT, player->GetGUID());
        });
        uint8 healed = 0;
        for (Unit* ally : allies)
        {
            if (healed >= RadianceExtraTargets)
                break;
            if (ally == center)
                continue;
            player->CastSpell(ally, SPELL_RADIANCE_HEAL, true);
            ApplyAtonement(player, ally, share);
            ++healed;
        }
        // Without injured allies the group still gets its Atonement
        for (Player* member : GroupPlayers(player, 30.0f))
        {
            if (healed >= RadianceExtraTargets)
                break;
            if (member == center || member->HasAura(SPELL_ATONEMENT, player->GetGUID()))
                continue;
            ApplyAtonement(player, member, share);
            ++healed;
        }
    }

    // Évangélisme: every Atonement 6 s longer (and, with Déferlante de lumière, a heal on each)
    static void Evangelism(Player* player)
    {
        bool const surging = player->HasAura(TALENT_SURGING_LIGHT);
        for (Player* ally : AtonedAllies(player))
        {
            if (Aura* atonement = ally->GetAura(SPELL_ATONEMENT, player->GetGUID()))
                Lengthen(atonement, atonement->GetDuration() + EvangelismMs);
            if (surging)
                Heal(player, ally, SPELL_SURGING_LIGHT_HEAL,
                    float(ally->CountPctFromMaxHealth(int32(SurgingLightPct))));
        }
    }

    // Devouring Plague: its Insanity (or Dévoreur d'esprit) spent, its instant hit, and what it feeds
    static void DevouringPlague(Player* player, Unit* target)
    {
        if (player->HasAura(SPELL_MIND_DEVOURER))
            player->RemoveAurasDueToSpell(SPELL_MIND_DEVOURER);
        else
            AddInsanity(player, -int32(PlagueCost(player)));
        if (target && target->IsAlive())
            player->CastSpell(target, SPELL_DEVOURING_PLAGUE_HIT, true);
        if (player->HasAura(TALENT_MIND_FLAY_INSANITY))
            player->AddAura(SPELL_MIND_FLAY_INSANITY, player);
        Apparitions(player);
    }
};

// --- Damage, heals ---------------------------------------------------------------------------------------------------

class PriestTalentUnitScript : public UnitScript
{
public:
    PriestTalentUnitScript() : UnitScript("PriestTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Atonement counts every damage the Discipline Priest and its fiend deal; the fiend's hits give Shadow Insanity
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage || attacker == victim)
            return;
        Player* player = Priest(attacker);
        bool const fiend = !player;
        if (fiend)
            player = FiendOwner(attacker);
        if (!player || victim == player || victim->IsFriendlyTo(player))
            return;
        if (IsDiscipline(player))
            GetState(player)->atonementDamage += damage;
        else if (fiend)
            AddInsanity(player, InsanityFiendHit);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || damage <= 0)
            return;
        Player* player = Priest(attacker);
        if (!player)
            return;
        PriestState* state = GetState(player);
        float factor = DisciplineFactor(player, state, target);
        uint32 const firstRank = FirstRank(spellInfo);

        // Pouvoir du côté obscur, Ombres pénitentes: Penance's bolts
        if (IsPriestSpell(spellInfo, 0, FlagPenanceDamage))
        {
            if (Now() <= state->darkSideUntilMs)
                factor *= DarkSideFactor;
            if (player->HasAura(TALENT_PENITENT_SHADOWS) && target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE,
                SPELLFAMILY_PRIEST, FlagShadowWordPain, 0, 0, player->GetGUID()))
                factor *= PenitentShadowsFactor;
        }
        // Fouet mental : Démence
        else if (IsPriestSpell(spellInfo, FlagMindFlayTick, 0))
        {
            if (Now() <= state->flayInsanityUntilMs)
                factor *= MindFlayInsanityFactor;
        }
        // Mind Sear reaches every enemy around its target: past five, each takes less
        else if (IsPriestSpell(spellInfo, 0, FlagMindSearTick))
            factor *= AreaFalloff(SearCount(player, state, target));
        // Divine Star and Halo on a pack
        else if (spellInfo->Id == SPELL_DIVINE_STAR_DAMAGE || spellInfo->Id == SPELL_HALO_DAMAGE)
        {
            if (Now() <= state->starUntilMs)
                factor *= state->starFalloff;
        }
        // Shadow Word: Death executes below 20% (or with Porte-mort)
        else if (firstRank == SPELL_SHADOW_WORD_DEATH_R1)
        {
            if (target->GetHealthPct() < ExecutePct || Now() <= state->deathspeakerUntilMs)
                factor *= ShadowWordDeathExecute;
        }

        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // The Priest's damage over time: Schism, Péchés du grand nombre, Dark Ascension, Devouring Plague's retail ticks;
    // Shadow Word: Pain and Purge the Wicked's ticks roll the procs
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !damage)
            return;
        Player* player = Priest(attacker);
        if (!player)
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        bool const pain = firstRank == SPELL_SHADOW_WORD_PAIN_R1;
        bool const purge = spellInfo->Id == SPELL_PURGE_THE_WICKED;
        bool const plague = firstRank == SPELL_DEVOURING_PLAGUE_R1;
        bool const touch = firstRank == SPELL_VAMPIRIC_TOUCH_R1;
        if (!pain && !purge && !plague && !touch)
            return;
        PriestState* state = GetState(player);

        float factor = DisciplineFactor(player, state, target);
        if (plague)
            factor *= DevouringPlagueTickFactor;
        if (player->HasAura(SPELL_DARK_ASCENSION))
            factor *= DarkAscensionPeriodicFactor;
        damage = uint32(float(damage) * factor);

        if (!pain && !purge)
            return;
        // Pouvoir du côté obscur
        if (player->HasAura(TALENT_DARK_SIDE) && roll_chance_i(15))
            player->AddAura(SPELL_DARK_SIDE, player);
        if (!pain)
            return;
        // Intuition ténébreuse: Mind Blast back, and instant
        if (player->HasAura(TALENT_SHADOWY_INSIGHT) && roll_chance_i(8))
        {
            RefillCharges(player, state, CHARGE_MIND_BLAST);
            player->AddAura(SPELL_SHADOWY_INSIGHT, player);
        }
        // Porte-mort
        if (player->HasAura(TALENT_DEATHSPEAKER) && roll_chance_i(10))
            player->AddAura(SPELL_DEATHSPEAKER, player);
    }

    // The heals: Pouvoir du côté obscur on Penance's, Holy's Echo of Light, Foi renouvelée, Guide de la lumière and
    // Sillage de lumière. The hook takes (target, healer); the heals this module hands out are left alone.
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* player = Priest(healer);
        if (!player || !target || !heal || !spellInfo || s_relaying || IsRelayedHeal(spellInfo->Id))
            return;
        PriestState* state = GetState(player);
        float factor = 1.0f;

        if (IsPriestSpell(spellInfo, 0, FlagPenanceHeal) && Now() <= state->darkSideUntilMs)
            factor *= DarkSideFactor;

        bool const holy = IsHoly(player);
        if (holy)
        {
            if (player->HasAura(TALENT_RENEWED_FAITH) && target->GetAuraEffect(SPELL_AURA_PERIODIC_HEAL,
                SPELLFAMILY_PRIEST, FlagRenew, 0, 0, player->GetGUID()))
                factor *= RenewedFaithFactor;
            if (spellInfo->Id == SPELL_SERENITY && player->HasAura(TALENT_GUIDING_LIGHT) &&
                target->GetHealthPct() < PowerWordLifePct)
                factor *= GuidingLightFactor;
        }
        if (factor != 1.0f)
            heal = uint32(float(heal) * factor);
        if (!holy)
            return;

        s_relaying = true;
        EchoOfLight(player, target, heal);
        // Sillage de lumière: the previous Flash Heal's target too
        if (FirstRank(spellInfo) == SPELL_FLASH_HEAL_R1)
        {
            if (uint8 const trail = Rank(player, TALENT_TRAIL_OF_LIGHT_1, TALENT_TRAIL_OF_LIGHT_2))
                if (Unit* previous = ObjectAccessor::GetUnit(*player, state->lastFlashTarget))
                    if (previous != target && previous->IsAlive() && previous->IsWithinDistInMap(player, HealRange))
                        Heal(player, previous, SPELL_TRAIL_OF_LIGHT_HEAL, float(heal) * (trail > 1 ? 0.35f : 0.2f));
            state->lastFlashTarget = target->GetGUID();
        }
        s_relaying = false;
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = Priest(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        PriestState* state = GetState(player);
        uint32 const firstRank = FirstRank(spellInfo);

        if (IsPriestSpell(spellInfo, FlagMindFlayTick, 0))
        {
            AddInsanity(player, InsanityMindFlayTick);
            PsychicLink(player, victim, damage);
            return;
        }
        if (IsPriestSpell(spellInfo, 0, FlagMindSearTick))
        {
            uint32 const now = Now();
            if (now - state->searStamp > 500)
            {
                state->searStamp = now;
                state->searHits = 0;
            }
            if (state->searHits < 3)
            {
                ++state->searHits;
                AddInsanity(player, InsanityMindSearTick);
            }
            return;
        }

        switch (spellInfo->Id)
        {
            case SPELL_VOID_TORRENT_TICK:
                AddInsanity(player, InsanityVoidTorrentTick);
                PsychicLink(player, victim, damage);
                return;
            case SPELL_SHADOW_CRASH:
            {
                // Its Vampiric Touch on the first few enemies it hits
                uint32 const now = Now();
                if (now != state->crashStamp)
                {
                    state->crashStamp = now;
                    state->crashHits = 0;
                }
                uint8 const max = player->HasAura(TALENT_WHISPERING_SHADOWS) ? WhisperingShadowsTargets :
                    ShadowCrashTargets;
                if (state->crashHits < max && victim->IsAlive())
                    if (uint32 const touch = KnownRank(player, SPELL_VAMPIRIC_TOUCH_R1))
                    {
                        ++state->crashHits;
                        player->CastSpell(victim, touch, true);
                    }
                return;
            }
            case SPELL_MINDGAMES:
            {
                std::vector<Unit*> const allies = InjuredAllies(player, player, HealRange);
                if (!allies.empty())
                    Heal(player, allies.front(), SPELL_MINDGAMES_HEAL, float(damage));
                return;
            }
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_MIND_BLAST_R1:
                PsychicLink(player, victim, damage);
                break;
            case SPELL_SHADOW_WORD_DEATH_R1:
                PsychicLink(player, victim, damage);
                // Mort et folie: a kill brings it back
                if (!victim->IsAlive() && player->HasAura(TALENT_DEATH_AND_MADNESS))
                {
                    ClearChainCooldown(player, SPELL_SHADOW_WORD_DEATH_R1);
                    AddInsanity(player, InsanityDeathAndMadness);
                }
                break;
            default:
                break;
        }
    }

private:
    static bool s_relaying;

    // The heals this module hands out at amounts it works out: never echoed nor relayed
    static bool IsRelayedHeal(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_ATONEMENT_HEAL:
            case SPELL_ECHO_OF_LIGHT:
            case SPELL_TRAIL_OF_LIGHT_HEAL:
            case SPELL_SURGING_LIGHT_HEAL:
            case SPELL_MINDGAMES_HEAL:
                return true;
            default:
                return false;
        }
    }

    // The enemies a Mind Sear tick reaches around its target, counted twice a second
    static uint8 SearCount(Player* player, PriestState* state, Unit* target)
    {
        uint32 const now = Now();
        if (now - state->searCountStamp > 500)
        {
            state->searCountStamp = now;
            state->searCount = uint8(std::min<std::size_t>(EnemiesNear(player, target, MindSearRadius).size() + 1,
                std::numeric_limits<uint8>::max()));
        }
        return state->searCount;
    }
};

bool PriestTalentUnitScript::s_relaying = false;

// --- Every update: charges, Atonement's heals, Insanity out of combat, Void Bolt -------------------------------------

class PriestTalentPlayerScript : public PlayerScript
{
public:
    PriestTalentPlayerScript() : PlayerScript("PriestTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_PRIEST || !player->IsInWorld())
            return;
        PriestState* state = GetState(player);

        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
            UpdateCharges(player, state, ChargeKind(kind), diff);

        if (state->clearShieldCooldown)
        {
            state->clearShieldCooldown = false;
            ClearChainCooldown(player, SPELL_POWER_WORD_SHIELD_R1);
        }
        if (state->clearPenanceCooldown)
        {
            state->clearPenanceCooldown = false;
            ClearChainCooldown(player, SPELL_PENANCE_R1);
        }

        state->atonementTimer += diff;
        if (state->atonementTimer >= AtonementFlushMs)
        {
            state->atonementTimer = 0;
            if (IsDiscipline(player))
                FlushAtonement(player, state);
            else
                state->atonementDamage = 0;
        }

        UpdateInsanity(player, state, diff);

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        state->holdTimer = 0;
        UpdateHolds(player);
    }

private:
    // Insanity fades 10 s after combat, and is Shadow's only
    static void UpdateInsanity(Player* player, PriestState* state, uint32 diff)
    {
        if (!player->HasAura(SPELL_INSANITY))
        {
            state->outOfCombatMs = 0;
            return;
        }
        if (!IsShadow(player))
        {
            player->RemoveAurasDueToSpell(SPELL_INSANITY);
            return;
        }
        if (player->IsInCombat())
        {
            state->outOfCombatMs = 0;
            return;
        }
        state->outOfCombatMs += diff;
        if (state->outOfCombatMs < InsanityFadeMs)
            return;
        state->outOfCombatMs = 0;
        player->RemoveAurasDueToSpell(SPELL_INSANITY);
    }

    // A few times a second: Void Bolt is known while Void Eruption is (it is usable in Voidform only)
    static void UpdateHolds(Player* player)
    {
        bool const eruption = player->HasSpell(SPELL_VOID_ERUPTION);
        bool const bolt = player->HasSpell(SPELL_VOID_BOLT);
        if (eruption && !bolt)
            player->learnSpell(SPELL_VOID_BOLT, false);
        else if (!eruption && bolt)
            player->removeSpell(SPELL_VOID_BOLT, SPEC_MASK_ALL, false);
    }
};
}

void AddPriestTalentScripts()
{
    new PriestTalentSpellScript();
    new PriestTalentUnitScript();
    new PriestTalentPlayerScript();
}
