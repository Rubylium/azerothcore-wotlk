#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <list>
#include <vector>

// The Warrior's talents and abilities on the retail-style trees (localTools/warrior/talentTree.json) that spell data
// cannot carry. A talent is its rank spell's aura on the Warrior (learned by mod-custom-classes' TalentTree.cpp), read
// here with HasAura; a specialization is its passive (Armes, Fureur, Protection). The abilities and auras named below
// are in localTools/warrior/Spells.ps1, the stock spells changed in place in StockSpells.ps1; the WotLK talents the
// trees reuse keep their own scripts in the core (spell_warrior.cpp).
//
// The Warrior keeps rage. Builders give it (Bloodthirst, Raging Blow, Shield Slam, Thunder Clap, Skullsplitter,
// Onslaught, the Ravager, Champion's Spear, Shield Charge), spenders cost it in their spell data (Mortal Strike,
// Rampage, Revenge, Ignore Pain...). Arms marks its target with Colossus Smash, plays Overpower on charges and Execute
// below 20% (35% with Massacre); Fury is Enraged by Bloodthirst's critical strikes and Rampage, and Whirlwind's Meat
// Cleaver spreads its single-target attacks over the pack; Protection turns rage into Ignore Pain's absorb and Shield
// Block's charges, and every dodge, parry or block can make Revenge free. Overpower, Raging Blow and Shield Block have
// charges kept here, shown as stacks.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_SECOND_WIND_1            = 95002,    // Seconde inspiration
    TALENT_SECOND_WIND_2            = 95003,
    TALENT_MASSACRE_ARMS            = 95008,    // Massacre
    TALENT_BATTLELORD               = 95009,    // Seigneur de bataille
    TALENT_MARTIAL_PROWESS          = 95010,    // Prouesse martiale
    TALENT_EXECUTIONERS_PRECISION   = 95011,    // Précision de l'exécuteur
    TALENT_TACTICIAN_1              = 95012,    // Tacticien
    TALENT_TACTICIAN_2              = 95013,
    TALENT_IN_FOR_THE_KILL          = 95016,    // Pour la mise à mort
    TALENT_UNHINGED                 = 95018,    // Désaxé
    TALENT_BONEGRINDER              = 95019,    // Broyeur impitoyable
    TALENT_MEAT_CLEAVER             = 95020,    // Fendoir à viande
    TALENT_MASSACRE_FURY            = 95023,    // Massacre
    TALENT_FRENZIED_ENRAGE_1        = 95024,    // Rage prolongée
    TALENT_FRENZIED_ENRAGE_2        = 95025,
    TALENT_KEEN_CLEAVER             = 95026,    // Fendoir affûté
    TALENT_RECKLESS_ABANDON         = 95029,    // Abandon téméraire
    TALENT_DANCING_BLADES           = 95030,    // Lames dansantes
    TALENT_UNCHECKED_RAGE           = 95031,    // Rage incontrôlée
    TALENT_PROTECTIVE_RAGE_1        = 95034,    // Rage protectrice
    TALENT_PROTECTIVE_RAGE_2        = 95035,
    TALENT_RELENTLESS_ONSLAUGHT_1   = 95040,    // Assaut implacable
    TALENT_RELENTLESS_ONSLAUGHT_2   = 95041,
    TALENT_BERSERKERS_TORMENT       = 95046,    // Tourment du berserker
    TALENT_HEAVY_REPERCUSSIONS      = 95048,    // Lourdes répercussions
    TALENT_NEVER_SURRENDER_1        = 95049,    // Ne jamais se rendre
    TALENT_NEVER_SURRENDER_2        = 95050,
    TALENT_BOLSTER                  = 95062,    // Renfort
    TALENT_IMMOVABLE_OBJECT         = 95065,    // Objet inamovible
    TALENT_UNSTOPPABLE_FORCE        = 95066,    // Force imparable
};

// The abilities and auras of localTools/warrior/Spells.ps1
enum Spells : uint32
{
    SPELL_HEROIC_LEAP               = 95100,
    SPELL_HEROIC_LEAP_LANDING       = 95101,
    SPELL_THUNDEROUS_ROAR           = 95105,
    SPELL_CHAMPIONS_SPEAR           = 95106,
    SPELL_AVATAR                    = 95104,
    SPELL_COLOSSUS_SMASH            = 95110,
    SPELL_WARBREAKER                = 95111,
    SPELL_SKULLSPLITTER             = 95112,
    SPELL_RAVAGER_ARMS              = 95113,
    SPELL_COLOSSUS_MARK             = 95115,
    SPELL_MARTIAL_PROWESS           = 95116,
    SPELL_EXECUTIONERS_PRECISION    = 95117,
    SPELL_IN_FOR_THE_KILL           = 95118,
    SPELL_BONEGRINDER               = 95119,
    SPELL_RAMPAGE                   = 95120,
    SPELL_RAGING_BLOW               = 95121,
    SPELL_ONSLAUGHT                 = 95122,
    SPELL_ODYNS_FURY                = 95123,
    SPELL_RAVAGER_BLADES            = 95124,
    SPELL_RAGING_BLOW_OFFHAND       = 95125,
    SPELL_RAMPAGE_HIT               = 95126,
    SPELL_RAMPAGE_HIT_OFFHAND       = 95127,
    SPELL_ENRAGE                    = 95128,
    SPELL_MEAT_CLEAVER              = 95129,
    SPELL_MEAT_CLEAVER_HIT          = 95130,
    SPELL_RECKLESS_ABANDON          = 95131,
    SPELL_DANCING_BLADES            = 95132,
    SPELL_PROTECTIVE_RAGE           = 95133,
    SPELL_RAGING_BLOW_CHARGES       = 95134,
    SPELL_SHIELD_CHARGE             = 95140,
    SPELL_RAVAGER_PROTECTION        = 95141,
    SPELL_IGNORE_PAIN               = 95143,
    SPELL_IGNORE_PAIN_ABSORB        = 95144,
    SPELL_SHIELD_CHARGE_HIT         = 95145,
    SPELL_REVENGE_PROC              = 95146,
    SPELL_SHIELD_BLOCK_CHARGES      = 95147,
    SPELL_OVERPOWER_CHARGES         = 95148,

    // The specializations' passives
    SPELL_SPEC_ARMS                 = 95280,
    SPELL_SPEC_FURY                 = 95281,
    SPELL_SPEC_PROTECTION           = 95282,

    // Stock (first ranks)
    SPELL_THUNDER_CLAP_R1           = 6343,
    SPELL_REVENGE_R1                = 6572,
    SPELL_EXECUTE_R1                = 5308,
    SPELL_EXECUTE_DAMAGE            = 20647,
    SPELL_OVERPOWER                 = 7384,
    SPELL_SHIELD_WALL               = 871,
    SPELL_WHIRLWIND                 = 1680,
    SPELL_RECKLESSNESS              = 1719,
    SPELL_SHIELD_BLOCK              = 2565,
    SPELL_LAST_STAND                = 12975,
    SPELL_MORTAL_STRIKE_R1          = 12294,
    SPELL_SHIELD_SLAM_R1            = 23922,
    SPELL_BLOODTHIRST               = 23881,
    SPELL_WHIRLWIND_OFFHAND         = 44949,
    SPELL_BLADESTORM                = 46924,
    SPELL_SHOCKWAVE                 = 46968,
    SPELL_BLADESTORM_WHIRL          = 50622,
    SPELL_SUDDEN_DEATH              = 52437,
};

// --- Tuning (README.md) ----------------------------------------------------------------------------------------------
// Rage a builder gives (in rage points)
constexpr int32 RageBloodthirst = 8;
constexpr int32 RageRagingBlow = 12;
constexpr int32 RageOnslaught = 15;
constexpr int32 RageSkullsplitter = 20;
constexpr int32 RageShieldSlam = 15;
constexpr int32 RageThunderClap = 5;
constexpr int32 RageRavagerTick = 5;
constexpr int32 RageChampionsSpear = 10;
constexpr int32 RageShieldCharge = 20;
constexpr int32 RageRecklessAbandon = 50;
// Execute: below this share of health, or with Sudden Death's proc
constexpr float ExecutePct = 20.0f;
constexpr float MassacrePct = 35.0f;
// Arms
constexpr float ColossusFactor = 1.2f;              // Frappe du colosse and Briseguerre's mark
constexpr float ExecutionersPrecisionPerStack = 0.25f;
constexpr uint8 MartialProwessMax = 2;
constexpr uint8 ExecutionersPrecisionMax = 2;
constexpr int32 BattlelordChance = 25;
constexpr int32 InForTheKillHaste = 10;
constexpr int32 InForTheKillLowHaste = 25;
constexpr int32 BonegrinderBonusMs = 9000;          // Broyeur impitoyable: past the whirl's own duration
constexpr uint32 UnhingedPeriodMs = 2000;
// Fury
constexpr int32 BloodthirstEnrageChance = 30;
constexpr int32 EnrageMs = 4000;
constexpr uint8 MeatCleaverStacks = 2;
constexpr uint8 KeenCleaverStacks = 2;
constexpr uint8 MeatCleaverTargets = 4;
constexpr float MeatCleaverShare = 0.9f;
constexpr float MeatCleaverRange = 8.0f;
constexpr int32 BerserkersTormentMs = 8000;
// Protection
constexpr float IgnorePainAttackPower = 2.0f;       // the absorb, a share of attack power
constexpr int32 IgnorePainCapPct = 30;              // of maximum health
constexpr uint32 IgnorePainHitPct = 50;             // of each hit it takes, until spent
constexpr int32 HeavyRepercussionsMs = 1000;
constexpr uint32 RevengeProcCooldownMs = 3000;
constexpr int32 ImmovableObjectMs = 10000;
constexpr float UnstoppableForceFactor = 1.5f;
constexpr int32 UnstoppableForceCooldownMs = 3000;
constexpr float ShieldChargeRange = 8.0f;
constexpr uint8 ShieldChargeTargets = 5;
// Class tree
constexpr float SecondWindPct = 35.0f;
constexpr uint32 SecondWindPeriodMs = 1000;
constexpr float LeapSpeed = 28.0f;                  // yards a second (Spell::CalculateJumpSpeeds for a player)
// The Ravager: a blow every second for 7 s within 8 yd of its spot
constexpr uint32 RavagerMs = 7000;
constexpr uint32 RavagerPeriodMs = 1000;
constexpr float RavagerRadius = 8.0f;
// Packs (the combat bench, Fire mage as the reference): the area spells reach AreaMaxTargets, each hit whole up to
// AreaFullTargets enemies and sqrt(AreaFullTargets / enemies) of it past them. Twelve rather than the casters' five:
// the Warrior's areas are 8 yd around it, so a big pack is already only partly in reach
constexpr uint8 AreaFullTargets = 12;
constexpr uint8 AreaMaxTargets = 12;
constexpr uint32 AreaCountMs = 300;

constexpr uint32 HoldCheckMs = 250;
constexpr uint32 StaleTalentCheckMs = 2000;

enum ChargeKind : uint8
{
    CHARGE_OVERPOWER,
    CHARGE_RAGING_BLOW,
    CHARGE_SHIELD_BLOCK,
    MAX_CHARGE_KINDS
};

struct ChargeDefinition
{
    uint32 spellId;
    uint32 displayAura;
};

constexpr std::array<ChargeDefinition, MAX_CHARGE_KINDS> ChargeDefinitions = {{
    { SPELL_OVERPOWER, SPELL_OVERPOWER_CHARGES },
    { SPELL_RAGING_BLOW, SPELL_RAGING_BLOW_CHARGES },
    { SPELL_SHIELD_BLOCK, SPELL_SHIELD_BLOCK_CHARGES },
}};

struct Charges
{
    bool initialized = false;
    uint8 count = 0;
    int32 rechargeMs = 0;
    bool clearCooldown = false;     // the cooldown its cast just started is taken back on the next update
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct WarriorState : public DataMap::Base
{
    std::array<Charges, MAX_CHARGE_KINDS> charges;
    // Heroic Leap: where and when the Warrior lands
    bool leapPending = false;
    uint32 leapLandMs = 0;
    // The Ravager's spot and clock
    uint32 ravagerUntilMs = 0;
    uint32 ravagerNextMs = 0;
    float ravagerX = 0.0f;
    float ravagerY = 0.0f;
    float ravagerZ = 0.0f;
    // Bladestorm or the Ravager whirling (Désaxé)
    uint32 whirlUntilMs = 0;
    uint32 unhingedNextMs = 0;
    uint32 secondWindMs = 0;
    uint32 revengeProcMs = 0;
    bool defending = false;
    uint32 rageSpent = 0;           // Tacticien, in tenths
    uint32 staleCheckMs = 0;
    bool shortenThunderClap = false;
    uint32 holdTimer = 0;
    // The enemies an area spell reaches, counted a few times a second
    uint32 areaSpell = 0;
    uint32 areaStamp = 0;
    uint8 areaCount = 1;
};

constexpr char const* StateKey = "WarriorTalentState";

WarriorState* GetState(Player* player)
{
    return player->CustomData.GetDefault<WarriorState>(StateKey);
}

Player* Warrior(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_WARRIOR ? player : nullptr;
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsArms(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_ARMS);
}

bool IsFury(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_FURY);
}

bool IsProtection(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_PROTECTION);
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

// An aura of the Warrior's on a unit, one more stack (up to max), its duration refreshed
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

// An aura held a little longer (Lourdes répercussions, Rage prolongée), or for a set time
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

void AddRage(Player* player, int32 rage)
{
    if (rage)
        player->ModifyPower(POWER_RAGE, rage * 10);
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

// The highest rank of a chain the player knows, 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

// Execute's threshold: 20%, 35% with Massacre
float ExecuteThreshold(Player const* player)
{
    return player->HasAura(TALENT_MASSACRE_ARMS) || player->HasAura(TALENT_MASSACRE_FURY) ? MassacrePct : ExecutePct;
}

// --- Charges ------------------------------------------------------------------------------------------------------

uint8 MaxCharges(Player* player, ChargeKind kind)
{
    switch (kind)
    {
        case CHARGE_OVERPOWER:
            return IsArms(player) ? 2 : 1;
        case CHARGE_RAGING_BLOW:
            return IsFury(player) ? (player->HasAura(TALENT_UNCHECKED_RAGE) ? 3 : 2) : 1;
        case CHARGE_SHIELD_BLOCK:
            return IsProtection(player) ? 2 : 1;
        default:
            return 1;
    }
}

// A spell's cooldown with the Warrior's modifiers
int32 Recharge(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime)) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

void SpendCharge(Player* player, WarriorState* state, ChargeKind kind)
{
    uint8 const max = MaxCharges(player, kind);
    if (max <= 1)
        return;
    Charges& charges = state->charges[kind];
    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = max;
    }
    if (charges.count >= max)
        charges.rechargeMs = Recharge(player, ChargeDefinitions[kind].spellId);
    if (charges.count > 0)
        --charges.count;
    charges.clearCooldown = charges.count > 0;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

// One charge back at once (Tacticien), or the cooldown cleared without charges
void RefundCharge(Player* player, WarriorState* state, ChargeKind kind)
{
    uint8 const max = MaxCharges(player, kind);
    if (max <= 1)
    {
        ClearChainCooldown(player, ChargeDefinitions[kind].spellId);
        return;
    }
    Charges& charges = state->charges[kind];
    if (!charges.initialized || charges.count >= max)
        return;
    if (!charges.count)
        ClearChainCooldown(player, ChargeDefinitions[kind].spellId);
    ++charges.count;
    if (charges.count >= max)
        charges.rechargeMs = 0;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

void UpdateCharges(Player* player, WarriorState* state, ChargeKind kind, uint32 diff)
{
    Charges& charges = state->charges[kind];
    uint8 const max = MaxCharges(player, kind);
    uint32 const display = ChargeDefinitions[kind].displayAura;
    uint32 const spellId = ChargeDefinitions[kind].spellId;
    if (max <= 1 || !player->HasSpell(spellId))
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

// --- Fury: Enrage, Meat Cleaver --------------------------------------------------------------------------------------

void Enrage(Player* player)
{
    Aura* enrage = player->GetAura(SPELL_ENRAGE);
    if (!enrage)
        enrage = player->AddAura(SPELL_ENRAGE, player);
    else
        enrage->RefreshDuration();
    if (uint8 const rank = Rank(player, TALENT_PROTECTIVE_RAGE_1, TALENT_PROTECTIVE_RAGE_2))
    {
        // A custom base point still gets the effect's one die point: one less
        int32 const reduction = -5 * int32(rank) - 1;
        player->RemoveAurasDueToSpell(SPELL_PROTECTIVE_RAGE);
        player->CastCustomSpell(player, SPELL_PROTECTIVE_RAGE, &reduction, nullptr, nullptr, true);
        if (enrage)
            Lengthen(player->GetAura(SPELL_PROTECTIVE_RAGE), enrage->GetDuration());
    }
}

// Rampage's Enrage, longer with Rage prolongée
void RampageEnrage(Player* player)
{
    Enrage(player);
    if (uint8 const rank = Rank(player, TALENT_FRENZIED_ENRAGE_1, TALENT_FRENZIED_ENRAGE_2))
    {
        int32 const duration = EnrageMs + 1000 * int32(rank);
        Lengthen(player->GetAura(SPELL_ENRAGE), duration);
        Lengthen(player->GetAura(SPELL_PROTECTIVE_RAGE), duration);
    }
}

// A single-target attack that Meat Cleaver carries over the pack (its relays, 95130, never count themselves)
bool IsCleaverAttack(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_BLOODTHIRST:
        case SPELL_RAGING_BLOW:
        case SPELL_RAGING_BLOW_OFFHAND:
        case SPELL_RAMPAGE:
        case SPELL_RAMPAGE_HIT:
        case SPELL_RAMPAGE_HIT_OFFHAND:
        case SPELL_ONSLAUGHT:
        case SPELL_EXECUTE_DAMAGE:
            return true;
        default:
            return false;
    }
}

void MeatCleaver(Player* player, Unit* victim, uint32 damage)
{
    if (!player->HasAura(SPELL_MEAT_CLEAVER) || !victim)
        return;
    std::list<Unit*> enemies = EnemiesNear(player, victim, MeatCleaverRange);
    enemies.sort([victim](Unit const* left, Unit const* right)
    {
        return victim->GetExactDist2d(left) < victim->GetExactDist2d(right);
    });
    uint8 hit = 0;
    for (Unit* enemy : enemies)
    {
        if (hit++ >= MeatCleaverTargets)
            break;
        Strike(player, enemy, SPELL_MEAT_CLEAVER_HIT, float(damage) * MeatCleaverShare);
    }
}

void ConsumeCleaver(Player* player)
{
    if (Aura* cleaver = player->GetAura(SPELL_MEAT_CLEAVER))
        cleaver->ModStackAmount(-1);
}

// --- Arms ------------------------------------------------------------------------------------------------------------

void ColossusMark(Player* player, Unit* target)
{
    if (target && target->IsAlive())
        player->AddAura(SPELL_COLOSSUS_MARK, target);
}

// Pour la mise à mort: haste while the mark lasts, more on a target below 35%
void InForTheKill(Player* player, Unit* target)
{
    if (!player->HasAura(TALENT_IN_FOR_THE_KILL))
        return;
    // A custom base point still gets the effect's one die point: one less
    int32 const haste =
        (target && target->GetHealthPct() < MassacrePct ? InForTheKillLowHaste : InForTheKillHaste) - 1;
    player->RemoveAurasDueToSpell(SPELL_IN_FOR_THE_KILL);
    player->CastCustomSpell(player, SPELL_IN_FOR_THE_KILL, &haste, nullptr, nullptr, true);
}

// Bladestorm or the Ravager: Désaxé's Mortal Strikes while it whirls, Broyeur impitoyable after
void StartWhirl(Player* player, WarriorState* state, uint32 durationMs)
{
    state->whirlUntilMs = Now() + durationMs;
    state->unhingedNextMs = Now();
    if (player->HasAura(TALENT_BONEGRINDER))
        if (Aura* bonegrinder = player->AddAura(SPELL_BONEGRINDER, player))
            Lengthen(bonegrinder, int32(durationMs) + BonegrinderBonusMs);
}

// --- Protection ------------------------------------------------------------------------------------------------------

void IgnorePain(Player* player)
{
    float amount = float(player->GetTotalAttackPowerValue(BASE_ATTACK)) * IgnorePainAttackPower;
    // Ne jamais se rendre: up to 50% / 100% more as health is missing
    if (uint8 const rank = Rank(player, TALENT_NEVER_SURRENDER_1, TALENT_NEVER_SURRENDER_2))
        amount *= 1.0f + 0.5f * float(rank) * (1.0f - player->GetHealthPct() / 100.0f);
    int32 const cap = player->CountPctFromMaxHealth(IgnorePainCapPct);
    if (Aura* absorb = player->GetAura(SPELL_IGNORE_PAIN_ABSORB))
        if (AuraEffect* effect = absorb->GetEffect(EFFECT_0))
        {
            effect->ChangeAmount(std::min(cap, effect->GetAmount() + Amount(amount)));
            absorb->RefreshDuration();
            return;
        }
    int32 const absorb = std::min(cap, Amount(amount));
    player->CastCustomSpell(player, SPELL_IGNORE_PAIN_ABSORB, &absorb, nullptr, nullptr, true);
}

void ShieldCharge(Player* player, Unit* target)
{
    AddRage(player, RageShieldCharge);
    if (!target || !target->IsAlive())
        return;
    player->CastSpell(target, SPELL_SHIELD_CHARGE_HIT, true);
    uint8 hit = 1;
    for (Unit* enemy : EnemiesNear(player, target, ShieldChargeRange))
    {
        if (hit++ >= ShieldChargeTargets)
            break;
        player->CastSpell(enemy, SPELL_SHIELD_CHARGE_HIT, true);
    }
}

// Avatar for a while (Tourment du berserker, Objet inamovible): never shortens one already up
void GrantAvatar(Player* player, int32 durationMs)
{
    Aura* avatar = player->GetAura(SPELL_AVATAR);
    if (avatar && avatar->GetDuration() >= durationMs)
        return;
    if (!avatar)
        avatar = player->AddAura(SPELL_AVATAR, player);
    Lengthen(avatar, durationMs);
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

class WarriorTalentSpellScript : public AllSpellScript
{
public:
    WarriorTalentSpellScript() : AllSpellScript("WarriorTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // A spell kept on charges cannot be cast with none left; Execute needs its target low (or Sudden Death)
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Warrior(spell->GetCaster());
        if (!player)
            return;
        uint32 const firstRank = FirstRank(spell->GetSpellInfo());
        WarriorState* state = GetState(player);
        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
        {
            if (ChargeDefinitions[kind].spellId != firstRank || MaxCharges(player, ChargeKind(kind)) <= 1)
                continue;
            Charges const& charges = state->charges[kind];
            if (charges.initialized && !charges.count)
            {
                result = SPELL_FAILED_NOT_READY;
                return;
            }
        }

        if (firstRank == SPELL_EXECUTE_R1)
        {
            Unit* target = spell->m_targets.GetUnitTarget();
            if (target && target->GetHealthPct() >= ExecuteThreshold(player) && !player->HasAura(SPELL_SUDDEN_DEATH))
                result = SPELL_FAILED_TARGET_AURASTATE;
        }
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Warrior(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        WarriorState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        Tactician(player, state, spell, spellInfo);

        switch (id)
        {
            // Class tree
            case SPELL_HEROIC_LEAP:
                if (spell->m_targets.HasDst())
                {
                    float const distance = player->GetExactDist2d(spell->m_targets.GetDstPos());
                    state->leapPending = true;
                    state->leapLandMs = Now() + uint32(distance / LeapSpeed * 1000.0f) + 100;
                }
                return;
            case SPELL_CHAMPIONS_SPEAR:
                AddRage(player, RageChampionsSpear);
                return;
            // Arms
            case SPELL_COLOSSUS_SMASH:
                ColossusMark(player, target);
                InForTheKill(player, target);
                return;
            case SPELL_WARBREAKER:
                // Its marks go on each enemy it hits (OnSpellDamageDone)
                InForTheKill(player, player->GetVictim());
                return;
            case SPELL_SKULLSPLITTER:
                AddRage(player, RageSkullsplitter);
                return;
            case SPELL_RAVAGER_ARMS:
            case SPELL_RAVAGER_PROTECTION:
            {
                Position const spot = spell->m_targets.HasDst() ? Position(*spell->m_targets.GetDstPos()) :
                    Position(target ? *target : *player);
                state->ravagerX = spot.GetPositionX();
                state->ravagerY = spot.GetPositionY();
                state->ravagerZ = spot.GetPositionZ();
                state->ravagerUntilMs = Now() + RavagerMs;
                state->ravagerNextMs = Now() + RavagerPeriodMs / 2;
                StartWhirl(player, state, RavagerMs);
                return;
            }
            case SPELL_BLADESTORM:
                StartWhirl(player, state, 6000);
                return;
            case SPELL_OVERPOWER:
                SpendCharge(player, state, CHARGE_OVERPOWER);
                if (player->HasAura(TALENT_MARTIAL_PROWESS))
                    AddStack(player, player, SPELL_MARTIAL_PROWESS, MartialProwessMax);
                if (player->HasAura(TALENT_BATTLELORD) && roll_chance_i(BattlelordChance))
                    ClearChainCooldown(player, SPELL_MORTAL_STRIKE_R1);
                return;
            // Fury
            case SPELL_RAMPAGE:
                Rampage(player, target);
                return;
            case SPELL_RAGING_BLOW:
                SpendCharge(player, state, CHARGE_RAGING_BLOW);
                AddRage(player, RageRagingBlow);
                if (target && target->IsAlive() && player->HasOffhandWeaponForAttack())
                    player->CastSpell(target, SPELL_RAGING_BLOW_OFFHAND, true);
                ConsumeCleaver(player);
                return;
            case SPELL_ONSLAUGHT:
                AddRage(player, RageOnslaught);
                if (Rank(player, TALENT_RELENTLESS_ONSLAUGHT_1, TALENT_RELENTLESS_ONSLAUGHT_2))
                    Enrage(player);
                ConsumeCleaver(player);
                return;
            case SPELL_ODYNS_FURY:
                Enrage(player);
                if (player->HasAura(TALENT_DANCING_BLADES))
                    player->AddAura(SPELL_DANCING_BLADES, player);
                return;
            case SPELL_BLOODTHIRST:
                AddRage(player, RageBloodthirst);
                ConsumeCleaver(player);
                return;
            case SPELL_WHIRLWIND:
                if (player->HasAura(TALENT_MEAT_CLEAVER))
                    ShowStacks(player, SPELL_MEAT_CLEAVER, MeatCleaverStacks +
                        (player->HasAura(TALENT_KEEN_CLEAVER) ? KeenCleaverStacks : 0));
                return;
            case SPELL_RECKLESSNESS:
                if (player->HasAura(TALENT_RECKLESS_ABANDON))
                {
                    AddRage(player, RageRecklessAbandon);
                    if (Aura* abandon = player->AddAura(SPELL_RECKLESS_ABANDON, player))
                        if (Aura* recklessness = player->GetAura(SPELL_RECKLESSNESS))
                            Lengthen(abandon, recklessness->GetDuration());
                }
                if (player->HasAura(TALENT_BERSERKERS_TORMENT))
                    GrantAvatar(player, BerserkersTormentMs);
                return;
            // Protection
            case SPELL_SHIELD_CHARGE:
                ShieldCharge(player, target);
                return;
            case SPELL_IGNORE_PAIN:
                IgnorePain(player);
                return;
            case SPELL_SHIELD_BLOCK:
                SpendCharge(player, state, CHARGE_SHIELD_BLOCK);
                return;
            case SPELL_SHIELD_WALL:
                if (player->HasAura(TALENT_IMMOVABLE_OBJECT))
                    GrantAvatar(player, ImmovableObjectMs);
                return;
            case SPELL_LAST_STAND:
                if (player->HasAura(TALENT_BOLSTER))
                    player->AddAura(SPELL_SHIELD_BLOCK, player);
                return;
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_MORTAL_STRIKE_R1:
                // Both boosts were in its damage: spent now
                player->RemoveAurasDueToSpell(SPELL_MARTIAL_PROWESS);
                if (target)
                    target->RemoveAurasDueToSpell(SPELL_EXECUTIONERS_PRECISION, player->GetGUID());
                break;
            case SPELL_EXECUTE_R1:
                // Above the threshold it was Sudden Death's: spent
                if (target && target->GetHealthPct() >= ExecuteThreshold(player))
                    player->RemoveAurasDueToSpell(SPELL_SUDDEN_DEATH);
                if (player->HasAura(TALENT_EXECUTIONERS_PRECISION) && target && target->IsAlive())
                    AddStack(player, target, SPELL_EXECUTIONERS_PRECISION, ExecutionersPrecisionMax);
                ConsumeCleaver(player);
                break;
            case SPELL_SHIELD_SLAM_R1:
                AddRage(player, RageShieldSlam);
                // Lourdes répercussions
                if (player->HasAura(TALENT_HEAVY_REPERCUSSIONS))
                    if (Aura* block = player->GetAura(SPELL_SHIELD_BLOCK))
                        Lengthen(block, block->GetDuration() + HeavyRepercussionsMs);
                break;
            case SPELL_THUNDER_CLAP_R1:
                AddRage(player, RageThunderClap);
                // Force imparable: twice as fast during Avatar (the cooldown this cast starts, on the next update)
                if (player->HasAura(TALENT_UNSTOPPABLE_FORCE) && player->HasAura(SPELL_AVATAR))
                    state->shortenThunderClap = true;
                break;
            case SPELL_REVENGE_R1:
                player->RemoveAurasDueToSpell(SPELL_REVENGE_PROC);
                break;
            default:
                break;
        }
    }

private:
    // Rampage: its three other blows, main hand and off hand in turn, then the Enrage; Meat Cleaver carries each of
    // them (OnSpellDamageDone), and is spent once
    static void Rampage(Player* player, Unit* target)
    {
        if (target && target->IsAlive())
        {
            bool const offhand = player->HasOffhandWeaponForAttack();
            for (uint32 blow : { SPELL_RAMPAGE_HIT_OFFHAND, SPELL_RAMPAGE_HIT, SPELL_RAMPAGE_HIT_OFFHAND })
            {
                if (!target->IsAlive())
                    break;
                player->CastSpell(target, blow == SPELL_RAMPAGE_HIT_OFFHAND && !offhand ? SPELL_RAMPAGE_HIT : blow,
                    true);
            }
        }
        RampageEnrage(player);
        ConsumeCleaver(player);
    }

    // Tacticien: every 10 rage spent, a chance to bring Overpower back
    static void Tactician(Player* player, WarriorState* state, Spell* spell, SpellInfo const* spellInfo)
    {
        uint8 const rank = Rank(player, TALENT_TACTICIAN_1, TALENT_TACTICIAN_2);
        if (!rank || spellInfo->PowerType != POWER_RAGE || spell->GetPowerCost() <= 0)
            return;
        state->rageSpent += uint32(spell->GetPowerCost());
        int32 const chance = rank > 1 ? 15 : 8;
        while (state->rageSpent >= 100)
        {
            state->rageSpent -= 100;
            if (roll_chance_i(chance))
                RefundCharge(player, state, CHARGE_OVERPOWER);
        }
    }
};

// --- Damage --------------------------------------------------------------------------------------------------------

// The area spells whose hits fall off past five enemies
bool IsAreaSpell(SpellInfo const* spellInfo)
{
    switch (spellInfo->Id)
    {
        case SPELL_WHIRLWIND:
        case SPELL_WHIRLWIND_OFFHAND:
        case SPELL_BLADESTORM_WHIRL:
        case SPELL_WARBREAKER:
        case SPELL_THUNDEROUS_ROAR:
        case SPELL_ODYNS_FURY:
        case SPELL_CHAMPIONS_SPEAR:
        case SPELL_HEROIC_LEAP_LANDING:
        case SPELL_RAVAGER_BLADES:
        case SPELL_SHOCKWAVE:
            return true;
        default:
            break;
    }
    uint32 const firstRank = FirstRank(spellInfo);
    return firstRank == SPELL_THUNDER_CLAP_R1 || firstRank == SPELL_REVENGE_R1;
}

class WarriorTalentUnitScript : public UnitScript
{
public:
    WarriorTalentUnitScript() : UnitScript("WarriorTalentUnitScript", true, {
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Colossus Smash's mark on white hits
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Player* player = Warrior(attacker);
        if (!player || !target || !damage)
            return;
        if (target->HasAura(SPELL_COLOSSUS_MARK, player->GetGUID()))
            damage = uint32(float(damage) * ColossusFactor);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || damage <= 0)
            return;
        Player* player = Warrior(attacker);
        if (!player)
            return;
        float factor = 1.0f;
        if (target->HasAura(SPELL_COLOSSUS_MARK, player->GetGUID()))
            factor *= ColossusFactor;
        uint32 const firstRank = FirstRank(spellInfo);
        // Précision de l'exécuteur: Mortal Strike on its target
        if (firstRank == SPELL_MORTAL_STRIKE_R1)
            if (uint8 const stacks = Stacks(target, SPELL_EXECUTIONERS_PRECISION, player->GetGUID()))
                factor *= 1.0f + ExecutionersPrecisionPerStack * float(stacks);
        // Force imparable: Thunder Clap during Avatar
        if (firstRank == SPELL_THUNDER_CLAP_R1 && player->HasAura(TALENT_UNSTOPPABLE_FORCE) &&
            player->HasAura(SPELL_AVATAR))
            factor *= UnstoppableForceFactor;
        if (IsAreaSpell(spellInfo))
            factor *= AreaFalloff(AreaCount(player, spellInfo, target));
        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // The Warrior's bleeds and burns: the mark, and the packs' falloff on the area spells'
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !damage)
            return;
        Player* player = Warrior(attacker);
        if (!player)
            return;
        float factor = 1.0f;
        if (target->HasAura(SPELL_COLOSSUS_MARK, player->GetGUID()))
            factor *= ColossusFactor;
        if (spellInfo->Id == SPELL_THUNDEROUS_ROAR || spellInfo->Id == SPELL_ODYNS_FURY)
            factor *= AreaFalloff(AreaCount(player, spellInfo, target));
        if (factor != 1.0f)
            damage = uint32(float(damage) * factor);
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool critical) override
    {
        Player* player = Warrior(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        switch (spellInfo->Id)
        {
            case SPELL_WARBREAKER:
                ColossusMark(player, victim);
                return;
            case SPELL_BLOODTHIRST:
                if (IsFury(player) && (critical || roll_chance_i(BloodthirstEnrageChance)))
                    Enrage(player);
                break;
            default:
                break;
        }
        if (IsCleaverAttack(spellInfo->Id))
            MeatCleaver(player, victim, damage);
    }

private:
    // The enemies around the target an area spell reaches (the target included), counted a few times a second
    static uint8 AreaCount(Player* player, SpellInfo const* spellInfo, Unit* target)
    {
        WarriorState* state = GetState(player);
        uint32 const now = Now();
        if (state->areaSpell == spellInfo->Id && now - state->areaStamp <= AreaCountMs)
            return state->areaCount;
        float radius = spellInfo->Effects[EFFECT_0].CalcRadius(player);
        if (radius <= 0.0f)
            radius = 8.0f;
        state->areaSpell = spellInfo->Id;
        state->areaStamp = now;
        state->areaCount = uint8(std::min<std::size_t>(EnemiesNear(player, target, radius).size() + 1,
            std::numeric_limits<uint8>::max()));
        return state->areaCount;
    }
};

// --- Ignore Pain's absorb: half of each hit, until it is spent (warrior_spells.sql binds it) -------------------------

class spell_warr_ignore_pain_absorb : public AuraScript
{
    PrepareAuraScript(spell_warr_ignore_pain_absorb);

    void Absorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
    {
        absorbAmount = std::min(absorbAmount, CalculatePct(dmgInfo.GetDamage(), IgnorePainHitPct));
    }

    void Register() override
    {
        OnEffectAbsorb += AuraEffectAbsorbFn(spell_warr_ignore_pain_absorb::Absorb, EFFECT_0);
    }
};

// --- Every update: charges, the leap's landing, the Ravager, Revenge's procs, Second Wind ----------------------------

class WarriorTalentPlayerScript : public PlayerScript
{
public:
    WarriorTalentPlayerScript() : PlayerScript("WarriorTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_WARRIOR || !player->IsInWorld())
            return;
        WarriorState* state = GetState(player);

        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
            UpdateCharges(player, state, ChargeKind(kind), diff);

        uint32 const now = Now();
        if (state->leapPending && now >= state->leapLandMs)
        {
            state->leapPending = false;
            if (player->IsAlive())
                player->CastSpell(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
                    SPELL_HEROIC_LEAP_LANDING, true);
        }

        if (state->shortenThunderClap)
        {
            state->shortenThunderClap = false;
            ModifyChainCooldown(player, SPELL_THUNDER_CLAP_R1, -UnstoppableForceCooldownMs);
        }

        UpdateRavager(player, state, now);
        UpdateUnhinged(player, state, now);

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        state->holdTimer = 0;
        UpdateRevengeProc(player, state, now);
        UpdateSecondWind(player, state, now);
        DropStaleTalentAuras(player, state, now);
    }

private:
    // The trees reuse WotLK talent ranks (Deep Wounds, Flurry...). A specialization change takes them off, but a rank's
    // passive aura could outlive it until the next login (a bot taken from Arms to Fury kept Deep Wounds): a WotLK
    // talent aura the Warrior no longer knows the spell of goes
    static void DropStaleTalentAuras(Player* player, WarriorState* state, uint32 now)
    {
        if (now < state->staleCheckMs + StaleTalentCheckMs)
            return;
        state->staleCheckMs = now;
        std::vector<uint32> stale;
        for (auto const& [spellId, aura] : player->GetOwnedAuras())
            if (aura->IsPassive() && aura->GetCasterGUID() == player->GetGUID() && GetTalentSpellPos(spellId) &&
                !player->HasSpell(spellId))
                stale.push_back(spellId);
        for (uint32 spellId : stale)
            player->RemoveOwnedAura(spellId);
    }

    // The Ravager's blades on every enemy near its spot, a blow a second, 5 rage each
    static void UpdateRavager(Player* player, WarriorState* state, uint32 now)
    {
        if (!state->ravagerUntilMs)
            return;
        if (now > state->ravagerUntilMs || !player->IsAlive())
        {
            state->ravagerUntilMs = 0;
            return;
        }
        if (now < state->ravagerNextMs)
            return;
        state->ravagerNextMs += RavagerPeriodMs;
        Position const spot(state->ravagerX, state->ravagerY, state->ravagerZ);
        float const reach = player->GetExactDist(&spot) + RavagerRadius;
        uint8 hit = 0;
        for (Unit* enemy : EnemiesNear(player, player, reach))
        {
            if (hit >= AreaMaxTargets)
                break;
            if (enemy->GetExactDist(&spot) > RavagerRadius + enemy->GetCombatReach())
                continue;
            player->CastSpell(enemy, SPELL_RAVAGER_BLADES, true);
            ++hit;
        }
        if (hit)
            AddRage(player, RageRavagerTick);
    }

    // Désaxé: a free Mortal Strike on the target every 2 s while Bladestorm or the Ravager whirls
    static void UpdateUnhinged(Player* player, WarriorState* state, uint32 now)
    {
        if (!state->whirlUntilMs)
            return;
        if (now > state->whirlUntilMs)
        {
            state->whirlUntilMs = 0;
            return;
        }
        if (!player->HasAura(TALENT_UNHINGED) || now < state->unhingedNextMs)
            return;
        state->unhingedNextMs = now + UnhingedPeriodMs;
        Unit* victim = player->GetVictim();
        uint32 const strike = KnownRank(player, SPELL_MORTAL_STRIKE_R1);
        if (victim && victim->IsAlive() && strike && player->IsWithinMeleeRange(victim))
            player->CastSpell(victim, strike, true);
    }

    // Protection: a dodge, a parry or a block makes the next Revenge free (at most every 3 s)
    static void UpdateRevengeProc(Player* player, WarriorState* state, uint32 now)
    {
        bool const defending = player->HasAuraState(AURA_STATE_DEFENSE);
        bool const fresh = defending && !state->defending;
        state->defending = defending;
        if (!IsProtection(player) || !defending || player->HasAura(SPELL_REVENGE_PROC))
            return;
        if (!fresh && now < state->revengeProcMs + RevengeProcCooldownMs)
            return;
        state->revengeProcMs = now;
        player->AddAura(SPELL_REVENGE_PROC, player);
    }

    // Seconde inspiration: below 35% in combat, a share of maximum health every second
    static void UpdateSecondWind(Player* player, WarriorState* state, uint32 now)
    {
        uint8 const rank = Rank(player, TALENT_SECOND_WIND_1, TALENT_SECOND_WIND_2);
        if (!rank || !player->IsAlive() || !player->IsInCombat() || player->GetHealthPct() >= SecondWindPct)
            return;
        if (now < state->secondWindMs + SecondWindPeriodMs)
            return;
        state->secondWindMs = now;
        SpellInfo const* talent = sSpellMgr->GetSpellInfo(rank > 1 ? TALENT_SECOND_WIND_2 : TALENT_SECOND_WIND_1);
        if (!talent)
            return;
        HealInfo healInfo(player, player, player->CountPctFromMaxHealth(int32(rank)), talent, talent->GetSchoolMask());
        player->HealBySpell(healInfo);
    }
};
}

void AddWarriorTalentScripts()
{
    new WarriorTalentSpellScript();
    new WarriorTalentUnitScript();
    new WarriorTalentPlayerScript();
    RegisterSpellScript(spell_warr_ignore_pain_absorb);
}
