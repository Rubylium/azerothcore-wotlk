#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DynamicObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
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
#include <deque>
#include <limits>
#include <list>
#include <unordered_map>
#include <vector>

// The Paladin's talents and abilities on the retail-style trees (localTools/paladin/talentTree.json) that spell data
// cannot carry, and Holy Power (Puissance sacrée), the resource every specialization now fights with. A talent is its
// rank spell's aura on the Paladin (learned by mod-custom-classes' TalentTree.cpp), read here with HasAura; a
// specialization is its passive (Grâce sacrée, Bastion sacré, Zèle vindicatif). The abilities and auras named below
// are in localTools/paladin/Spells.ps1; the WotLK talents the trees reuse keep their own scripts in the core
// (spell_paladin.cpp).
//
// Holy Power is a stacking aura (up to 5). Builders add to it as they are cast; the finishers (Word of Glory, Light of
// Dawn, Shield of the Righteous, Templar's Verdict, Divine Storm) cannot be cast below 3 unless a proc makes the next
// one free, and spend 3 once cast. It fades 10 s after combat.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_JUSTICE_WRATH            = 92806,    // Courroux de la justice
    TALENT_DIVINE_PURPOSE           = 92814,    // Volonté divine
    TALENT_WINGS                    = 92815,    // Ailes de la vengeance
    TALENT_INFUSION_1               = 92819,
    TALENT_INFUSION_2               = 92820,
    TALENT_HOLY_SHOCK_CHARGES       = 92821,    // Horion en cascade
    TALENT_GLIMMER                  = 92822,    // Lueur de lumière
    TALENT_RADIANT_DAWN_1           = 92823,    // Aube radieuse
    TALENT_RADIANT_DAWN_2           = 92824,
    TALENT_LIGHT_BEARER             = 92825,    // Dispensateur de lumière
    TALENT_DAWN_POWER_1             = 92829,    // Puissance de l'aube
    TALENT_DAWN_POWER_2             = 92830,
    TALENT_FAVOR_OF_LIGHT           = 92831,    // Faveur de la lumière
    TALENT_IMPROVED_AVENGERS_SHIELD = 92832,
    TALENT_CONSECRATED_GROUND       = 92833,    // Sol consacré
    TALENT_BLESSED_SHIELD           = 92835,    // Bouclier béni
    TALENT_RIGHTEOUS_DEFENSE        = 92840,    // Juste défense
    TALENT_BULWARK                  = 92843,    // Rempart de fureur vertueuse
    TALENT_CRUSADERS_FAITH          = 92844,    // Foi du croisé
    TALENT_ART_OF_WAR_1             = 92847,
    TALENT_ART_OF_WAR_2             = 92848,
    TALENT_EMPYREAN_POWER           = 92856,
    TALENT_EXPURGATION              = 92857,
    TALENT_RIGHTEOUS_STORM          = 92861,    // Tempête vertueuse
    TALENT_RADIANT_LIGHT            = 92862,    // Lumière radieuse
};

// The abilities and auras of localTools/paladin/Spells.ps1
enum Spells : uint32
{
    SPELL_HOLY_POWER                = 92900,
    SPELL_WORD_OF_GLORY             = 92901,
    SPELL_DIVINE_PURPOSE            = 92902,
    SPELL_DIVINE_TOLL               = 92904,
    SPELL_DIVINE_TOLL_DAMAGE        = 92905,
    SPELL_DIVINE_TOLL_HEAL          = 92906,
    SPELL_WINGS                     = 92907,
    SPELL_JUSTICE_WRATH             = 92908,
    SPELL_LIGHT_OF_DAWN             = 92910,
    SPELL_LIGHT_OF_DAWN_HEAL        = 92911,
    SPELL_INFUSION_OF_LIGHT         = 92912,
    SPELL_HOLY_SHOCK_CHARGES        = 92913,
    SPELL_GLIMMER                   = 92914,
    SPELL_GLIMMER_HEAL              = 92915,
    SPELL_GLIMMER_DAMAGE            = 92916,
    SPELL_BEACON_OF_FAITH           = 92917,
    SPELL_BEACON_OF_VIRTUE          = 92918,
    SPELL_BEACON_HEAL               = 92919,
    SPELL_AVENGING_CRUSADER         = 92920,
    SPELL_AVENGING_CRUSADER_HEAL    = 92921,
    SPELL_LIGHT_BEARER_HEAL         = 92922,
    SPELL_SHIELD_OF_THE_RIGHTEOUS   = 92930,
    SPELL_RIGHTEOUS_ARMOR           = 92931,
    SPELL_ARDENT_DEFENDER           = 92932,
    SPELL_GUARDIAN                  = 92934,
    SPELL_BASTION_OF_LIGHT          = 92935,
    SPELL_CONSECRATED_GROUND        = 92936,
    SPELL_SHIELD_SILENCE            = 92937,
    SPELL_BULWARK                   = 92938,
    SPELL_TEMPLARS_VERDICT          = 92940,
    SPELL_BLADE_OF_JUSTICE          = 92941,
    SPELL_DIVINE_STORM              = 92942,
    SPELL_WAKE_OF_ASHES             = 92943,
    SPELL_FINAL_RECKONING           = 92944,
    SPELL_FINAL_RECKONING_DAMAGE    = 92945,
    SPELL_FINAL_RECKONING_MARK      = 92946,
    SPELL_EXECUTION_SENTENCE        = 92947,
    SPELL_EXECUTION_SENTENCE_DAMAGE = 92948,
    SPELL_CRUSADE                   = 92949,
    SPELL_CRUSADE_STACKS            = 92950,
    SPELL_SHIELD_OF_VENGEANCE       = 92951,
    SPELL_EMPYREAN_POWER            = 92952,
    SPELL_EXPURGATION               = 92953,
    SPELL_ART_OF_WAR                = 92954,
    SPELL_SHIELD_OF_VENGEANCE_ABSORB = 92955,

    // The specializations' passives
    SPELL_SPEC_HOLY                 = 93000,
    SPELL_SPEC_PROTECTION           = 93001,
    SPELL_SPEC_RETRIBUTION          = 93002,

    // Stock (first ranks)
    SPELL_CRUSADER_STRIKE           = 35395,
    SPELL_JUDGEMENT_OF_LIGHT        = 20271,
    SPELL_JUDGEMENT_OF_WISDOM       = 53408,
    SPELL_JUDGEMENT_OF_JUSTICE      = 53407,
    SPELL_HAMMER_OF_WRATH_R1        = 24275,
    SPELL_HOLY_SHOCK_R1             = 20473,
    SPELL_HOLY_SHOCK_DAMAGE_R1      = 25912,
    SPELL_HAMMER_OF_THE_RIGHTEOUS   = 53595,
    SPELL_AVENGERS_SHIELD_R1        = 31935,
    SPELL_FLASH_OF_LIGHT_R1         = 19750,
    SPELL_HOLY_LIGHT_R1             = 635,
    SPELL_AVENGING_WRATH            = 31884,
    SPELL_BEACON_OF_LIGHT_HEAL_1    = 53652,
    SPELL_BEACON_OF_LIGHT_HEAL_2    = 53654,
};

// Consecration's ranks: the one the Paladin stands in is its dynamic object
constexpr std::array<uint32, 8> ConsecrationRanks = { 26573, 20116, 20922, 20923, 20924, 27173, 48818, 48819 };

constexpr uint8 HolyPowerMax = 5;
constexpr uint8 FinisherCost = 3;
constexpr uint32 HolyPowerFadeMs = 10000;
constexpr uint32 GroundCheckMs = 250;
constexpr uint8 GlimmerMax = 8;
constexpr uint8 BulwarkMax = 5;
constexpr uint8 CrusadeMax = 10;
constexpr int32 RighteousArmorStepMs = 4500;
constexpr int32 RighteousArmorMaxMs = 13500;
constexpr float HealRange = 40.0f;

// A character's state for its talents. Kept on the player, so it dies with the session.
struct PaladinState : public DataMap::Base
{
    // Holy Shock's charges (Horion en cascade, Faveur de la lumière): how many are left, the recharge of the next one,
    // the rank last cast, and whether the cooldown its cast just started must be taken back
    uint8 shockCharges = 3;
    int32 shockRechargeMs = 0;
    uint32 shockSpellId = 0;
    bool shockClearCooldown = false;

    std::deque<ObjectGuid> glimmers;            // Lueur de lumière, oldest first
    ObjectGuid faithTarget;                     // Guide de foi
    std::vector<ObjectGuid> virtueTargets;      // Guide de vertu

    std::unordered_map<ObjectGuid, uint64> sentenced;   // Sentence d'exécution: damage dealt to each marked enemy

    uint32 outOfCombatMs = 0;
    uint32 groundTimer = 0;
};

constexpr char const* StateKey = "PaladinTalentState";

PaladinState* GetState(Player* player)
{
    return player->CustomData.GetDefault<PaladinState>(StateKey);
}

Player* Paladin(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_PALADIN ? player : nullptr;
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsHoly(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_HOLY);
}

bool IsProtection(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_PROTECTION);
}

bool IsFinisher(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_WORD_OF_GLORY:
        case SPELL_LIGHT_OF_DAWN:
        case SPELL_SHIELD_OF_THE_RIGHTEOUS:
        case SPELL_TEMPLARS_VERDICT:
        case SPELL_DIVINE_STORM:
            return true;
        default:
            return false;
    }
}

bool IsJudgement(uint32 spellId)
{
    return spellId == SPELL_JUDGEMENT_OF_LIGHT || spellId == SPELL_JUDGEMENT_OF_WISDOM ||
        spellId == SPELL_JUDGEMENT_OF_JUSTICE;
}

// The heals this module hands out at amounts it works out: a beacon never copies them
bool IsRelayedHeal(uint32 spellId)
{
    return spellId == SPELL_BEACON_HEAL || spellId == SPELL_AVENGING_CRUSADER_HEAL ||
        spellId == SPELL_LIGHT_BEARER_HEAL || spellId == SPELL_BEACON_OF_LIGHT_HEAL_1 ||
        spellId == SPELL_BEACON_OF_LIGHT_HEAL_2;
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

// --- Holy Power ----------------------------------------------------------------------------------------------------

uint8 HolyPower(Player const* player)
{
    return Stacks(player, SPELL_HOLY_POWER);
}

void AddHolyPower(Player* player, uint8 amount)
{
    if (amount)
        ShowStacks(player, SPELL_HOLY_POWER, std::min<uint8>(HolyPowerMax, HolyPower(player) + amount));
}

// The proc that would pay for this finisher instead of Holy Power: Divine Purpose for any, Empyrean Power for Divine
// Storm, Bastion of Light for Shield of the Righteous
uint32 FreeFinisherAura(Player const* player, uint32 spellId)
{
    if (player->HasAura(SPELL_DIVINE_PURPOSE))
        return SPELL_DIVINE_PURPOSE;
    if (spellId == SPELL_DIVINE_STORM && player->HasAura(SPELL_EMPYREAN_POWER))
        return SPELL_EMPYREAN_POWER;
    if (spellId == SPELL_SHIELD_OF_THE_RIGHTEOUS && player->HasAura(SPELL_BASTION_OF_LIGHT))
        return SPELL_BASTION_OF_LIGHT;
    return 0;
}

// Friendly units in the Paladin's group (itself and pets included) within range of center, most injured first
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

// Enemies of the player within range of center, center left out
std::list<Unit*> EnemiesNear(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, targets, check);
    Cell::VisitObjects(center, searcher, range);
    targets.remove_if([player, center](Unit* unit)
    {
        return unit == center || !unit->IsAlive() || !player->IsValidAttackTarget(unit);
    });
    return targets;
}

void Heal(Player* player, Unit* target, uint32 spellId, uint32 amount)
{
    if (!target || !amount || !target->IsAlive())
        return;
    int32 const heal = int32(std::min<uint32>(amount, uint32(std::numeric_limits<int32>::max())));
    player->CastCustomSpell(target, spellId, &heal, nullptr, nullptr, true);
}

bool InOwnConsecration(Player* player)
{
    for (uint32 spellId : ConsecrationRanks)
        if (DynamicObject* dynamic = player->GetDynObject(spellId))
            if (dynamic->GetMapId() == player->GetMapId() && player->GetExactDist2d(dynamic) <= dynamic->GetRadius())
                return true;
    return false;
}

// --- Holy Shock's charges (Holy) -----------------------------------------------------------------------------------

uint8 MaxShockCharges(Player* player)
{
    if (player->HasAura(TALENT_FAVOR_OF_LIGHT))
        return 3;
    return player->HasAura(TALENT_HOLY_SHOCK_CHARGES) ? 2 : 1;
}

// A rank's cooldown with the Paladin's modifiers (Horion sacré rapide): Holy Shock's is a category cooldown
int32 ShockCooldown(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime)) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

// Every rank of Holy Shock the Paladin knows shares the category cooldown
void ClearShockCooldown(Player* player, uint32 spellId)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->RemoveSpellCooldown(rank, true);
}

void SpendShockCharge(Player* player, PaladinState* state, uint32 spellId)
{
    uint8 const max = MaxShockCharges(player);
    if (max <= 1)
        return;
    state->shockSpellId = spellId;
    if (state->shockCharges >= max)
        state->shockRechargeMs = ShockCooldown(player, spellId);
    if (state->shockCharges > 0)
        --state->shockCharges;
    // A charge left: the cooldown the cast started is taken back on the next update
    state->shockClearCooldown = state->shockCharges > 0;
    ShowStacks(player, SPELL_HOLY_SHOCK_CHARGES, state->shockCharges);
}

void UpdateShockCharges(Player* player, PaladinState* state, uint32 diff)
{
    uint8 const max = MaxShockCharges(player);
    if (max <= 1 || !IsHoly(player))
    {
        state->shockCharges = max;
        state->shockRechargeMs = 0;
        state->shockClearCooldown = false;
        player->RemoveAurasDueToSpell(SPELL_HOLY_SHOCK_CHARGES);
        return;
    }

    if (state->shockClearCooldown)
    {
        state->shockClearCooldown = false;
        ClearShockCooldown(player, state->shockSpellId);
    }

    if (state->shockCharges >= max)
    {
        state->shockCharges = max;
        if (Stacks(player, SPELL_HOLY_SHOCK_CHARGES) != max)
            ShowStacks(player, SPELL_HOLY_SHOCK_CHARGES, max);
        return;
    }

    state->shockRechargeMs -= int32(diff);
    if (state->shockRechargeMs > 0)
        return;

    // A charge back: usable again if it was the last one gone
    if (!state->shockCharges && state->shockSpellId)
        ClearShockCooldown(player, state->shockSpellId);
    ++state->shockCharges;
    state->shockRechargeMs = state->shockCharges < max ? ShockCooldown(player, state->shockSpellId) : 0;
    ShowStacks(player, SPELL_HOLY_SHOCK_CHARGES, state->shockCharges);
}

// --- Glimmer of Light (Holy) ---------------------------------------------------------------------------------------

void AddGlimmer(Player* player, PaladinState* state, Unit* target)
{
    if (!target || !target->IsAlive())
        return;
    player->AddAura(SPELL_GLIMMER, target);
    state->glimmers.erase(std::remove(state->glimmers.begin(), state->glimmers.end(), target->GetGUID()),
        state->glimmers.end());
    state->glimmers.push_back(target->GetGUID());
    while (state->glimmers.size() > GlimmerMax)
    {
        if (Unit* oldest = ObjectAccessor::GetUnit(*player, state->glimmers.front()))
            oldest->RemoveAurasDueToSpell(SPELL_GLIMMER, player->GetGUID());
        state->glimmers.pop_front();
    }
}

// Each Holy Shock reaches every glimmer: a heal on an ally, holy damage on an enemy
void PulseGlimmers(Player* player, PaladinState* state)
{
    std::deque<ObjectGuid> kept;
    for (ObjectGuid const& guid : state->glimmers)
    {
        Unit* unit = ObjectAccessor::GetUnit(*player, guid);
        if (!unit || !unit->IsAlive() || !unit->HasAura(SPELL_GLIMMER, player->GetGUID()) ||
            !unit->IsWithinDistInMap(player, HealRange))
            continue;
        kept.push_back(guid);
        if (player->IsValidAttackTarget(unit))
            player->CastSpell(unit, SPELL_GLIMMER_DAMAGE, true);
        else if (!unit->IsFullHealth())
            player->CastSpell(unit, SPELL_GLIMMER_HEAL, true);
    }
    state->glimmers.swap(kept);
}

// --- Finishers -----------------------------------------------------------------------------------------------------

// A finisher was cast: its proc or its Holy Power goes, and what spending Holy Power feeds is fed
void SpendFinisher(Player* player, uint32 spellId)
{
    uint8 spent = 0;
    if (uint32 const proc = FreeFinisherAura(player, spellId))
    {
        if (proc == SPELL_BASTION_OF_LIGHT)
        {
            if (Aura* bastion = player->GetAura(SPELL_BASTION_OF_LIGHT))
                bastion->ModStackAmount(-1);
        }
        else
            player->RemoveAurasDueToSpell(proc);
    }
    else
    {
        uint8 const power = HolyPower(player);
        spent = std::min(power, FinisherCost);
        ShowStacks(player, SPELL_HOLY_POWER, power - spent);
    }

    // Volonté divine: the next one free
    if (player->HasAura(TALENT_DIVINE_PURPOSE) && roll_chance_i(15))
        player->CastSpell(player, SPELL_DIVINE_PURPOSE, true);

    uint8 const worth = spent ? spent : FinisherCost;
    // Croisade: a stack per Holy Power
    if (player->HasAura(SPELL_CRUSADE))
    {
        uint8 const stacks = std::min<uint8>(CrusadeMax, Stacks(player, SPELL_CRUSADE_STACKS) + worth);
        if (Aura* crusade = player->GetAura(SPELL_CRUSADE_STACKS))
        {
            crusade->SetStackAmount(stacks);
            if (Aura* parent = player->GetAura(SPELL_CRUSADE))
                crusade->SetDuration(parent->GetDuration());
        }
        else if (Aura* added = player->AddAura(SPELL_CRUSADE_STACKS, player))
        {
            added->SetStackAmount(stacks);
            if (Aura* parent = player->GetAura(SPELL_CRUSADE))
                added->SetDuration(parent->GetDuration());
        }
    }

    // Ailes de la vengeance: Avenging Wrath lasts a second longer per Holy Power
    if (player->HasAura(TALENT_WINGS))
        if (Aura* wrath = player->GetAura(SPELL_AVENGING_WRATH))
        {
            int32 const duration = wrath->GetDuration() + 1000 * worth;
            if (duration > wrath->GetMaxDuration())
                wrath->SetMaxDuration(duration);
            wrath->SetDuration(duration);
        }

    // Puissance de l'aube: Light of Dawn and Word of Glory may give one back
    if (spellId == SPELL_LIGHT_OF_DAWN || spellId == SPELL_WORD_OF_GLORY)
    {
        int32 const chance = player->HasAura(TALENT_DAWN_POWER_2) ? 30 : player->HasAura(TALENT_DAWN_POWER_1) ? 15 : 0;
        if (chance && roll_chance_i(chance))
            AddHolyPower(player, 1);
    }
}

// Shield of the Righteous: 4.5 s more armor, 13.5 s at most; Rempart's stacks spent; Juste défense brings the
// defensive cooldowns closer
void ShieldOfTheRighteous(Player* player)
{
    if (Aura* armor = player->GetAura(SPELL_RIGHTEOUS_ARMOR))
    {
        int32 const duration = std::min(armor->GetDuration() + RighteousArmorStepMs, RighteousArmorMaxMs);
        armor->SetMaxDuration(std::max(armor->GetMaxDuration(), duration));
        armor->SetDuration(duration);
    }
    else if (Aura* added = player->AddAura(SPELL_RIGHTEOUS_ARMOR, player))
    {
        added->SetMaxDuration(RighteousArmorStepMs);
        added->SetDuration(RighteousArmorStepMs);
    }

    player->RemoveAurasDueToSpell(SPELL_BULWARK);

    if (player->HasAura(TALENT_RIGHTEOUS_DEFENSE))
        for (uint32 spellId : { SPELL_ARDENT_DEFENDER, SPELL_GUARDIAN, SPELL_BASTION_OF_LIGHT })
            if (player->HasSpellCooldown(spellId))
                player->ModifySpellCooldown(spellId, -2000);
}

// Light of Dawn: the most injured allies within 15 yd, 5 of them and one or two more with Aube radieuse
void LightOfDawn(Player* player)
{
    std::size_t const count = 5 + (player->HasAura(TALENT_RADIANT_DAWN_2) ? 2 :
        player->HasAura(TALENT_RADIANT_DAWN_1) ? 1 : 0);
    std::vector<Unit*> const allies = InjuredAllies(player, player, 15.0f);
    for (std::size_t index = 0; index < allies.size() && index < count; ++index)
        player->CastSpell(allies[index], SPELL_LIGHT_OF_DAWN_HEAL, true);
}

// Divine Toll: an enemy target and up to 4 enemies within 10 yd of it, a Holy Power each; in Holy, the 5 most injured
// allies healed, a Holy Power each too (it may then be cast on an ally)
void DivineToll(Player* player, Unit* target)
{
    uint8 struck = 0;
    if (target && target->IsAlive() && player->IsValidAttackTarget(target))
    {
        std::list<Unit*> const around = EnemiesNear(player, target, 10.0f);
        player->CastSpell(target, SPELL_DIVINE_TOLL_DAMAGE, true);
        ++struck;
        for (Unit* enemy : around)
        {
            if (struck >= 5)
                break;
            player->CastSpell(enemy, SPELL_DIVINE_TOLL_DAMAGE, true);
            ++struck;
        }
    }
    if (IsHoly(player))
    {
        std::vector<Unit*> const allies = InjuredAllies(player, player, HealRange);
        for (std::size_t index = 0; index < allies.size() && index < 5; ++index)
        {
            player->CastSpell(allies[index], SPELL_DIVINE_TOLL_HEAL, true);
            ++struck;
        }
    }
    AddHolyPower(player, struck);
}

// Beacon of Virtue: the target and the 3 most injured allies near it
void BeaconOfVirtue(Player* player, PaladinState* state, Unit* target)
{
    state->virtueTargets.clear();
    if (!target)
        return;
    state->virtueTargets.push_back(target->GetGUID());
    uint8 added = 0;
    for (Unit* ally : InjuredAllies(player, target, 30.0f))
    {
        if (added >= 3)
            break;
        if (ally == target)
            continue;
        player->AddAura(SPELL_BEACON_OF_VIRTUE, ally);
        state->virtueTargets.push_back(ally->GetGUID());
        ++added;
    }
}

// Final Reckoning: the target and every enemy within 8 yd of it, struck and marked
void FinalReckoning(Player* player, Unit* target)
{
    if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        return;
    std::list<Unit*> enemies = EnemiesNear(player, target, 8.0f);
    enemies.push_front(target);
    for (Unit* enemy : enemies)
    {
        player->AddAura(SPELL_FINAL_RECKONING_MARK, enemy);
        if (enemy->IsAlive())
            player->CastSpell(enemy, SPELL_FINAL_RECKONING_DAMAGE, true);
    }
}

// --- Spell casts ---------------------------------------------------------------------------------------------------

class PaladinTalentSpellScript : public AllSpellScript
{
public:
    PaladinTalentSpellScript() : AllSpellScript("PaladinTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_PREPARE,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // A finisher needs 3 Holy Power, or a proc that pays for it
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered() || !IsFinisher(spell->GetSpellInfo()->Id))
            return;
        Player* player = Paladin(spell->GetCaster());
        if (player && HolyPower(player) < FinisherCost && !FreeFinisherAura(player, spell->GetSpellInfo()->Id))
            result = SPELL_FAILED_CASTER_AURASTATE;
    }

    // Tempête vertueuse: Divine Storm has no target limit (its core script set one as the spell loaded)
    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* spellInfo) override
    {
        Player* player = Paladin(caster);
        if (player && spellInfo->Id == SPELL_DIVINE_STORM && player->HasAura(TALENT_RIGHTEOUS_STORM))
            spell->SetSpellValue(SPELLVALUE_MAX_TARGETS, 25);
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Paladin(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        PaladinState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        if (IsFinisher(id))
        {
            SpendFinisher(player, id);
            switch (id)
            {
                case SPELL_LIGHT_OF_DAWN:
                    LightOfDawn(player);
                    break;
                case SPELL_SHIELD_OF_THE_RIGHTEOUS:
                    ShieldOfTheRighteous(player);
                    break;
                default:
                    break;
            }
            return;
        }

        switch (id)
        {
            case SPELL_DIVINE_TOLL:
                DivineToll(player, target);
                return;
            case SPELL_BEACON_OF_FAITH:
                if (Unit* previous = ObjectAccessor::GetUnit(*player, state->faithTarget))
                    if (previous != target)
                        previous->RemoveAurasDueToSpell(SPELL_BEACON_OF_FAITH, player->GetGUID());
                state->faithTarget = target ? target->GetGUID() : ObjectGuid::Empty;
                return;
            case SPELL_BEACON_OF_VIRTUE:
                BeaconOfVirtue(player, state, target);
                return;
            case SPELL_BASTION_OF_LIGHT:
                if (Aura* bastion = player->GetAura(SPELL_BASTION_OF_LIGHT))
                    bastion->SetStackAmount(3);
                return;
            case SPELL_BLADE_OF_JUSTICE:
                AddHolyPower(player, 2);
                player->RemoveAurasDueToSpell(SPELL_ART_OF_WAR);
                if (player->HasAura(TALENT_EXPURGATION) && target && target->IsAlive())
                    player->CastSpell(target, SPELL_EXPURGATION, true);
                return;
            case SPELL_WAKE_OF_ASHES:
                AddHolyPower(player, 3);
                if (player->HasAura(TALENT_RADIANT_LIGHT))
                    player->CastSpell(player, SPELL_EMPYREAN_POWER, true);
                return;
            case SPELL_FINAL_RECKONING:
                FinalReckoning(player, target);
                return;
            case SPELL_SHIELD_OF_VENGEANCE:
            {
                int32 const absorb = int32(player->CountPctFromMaxHealth(30));
                player->CastCustomSpell(player, SPELL_SHIELD_OF_VENGEANCE_ABSORB, &absorb, nullptr, nullptr, true);
                return;
            }
            case SPELL_CRUSADE:
                player->RemoveAurasDueToSpell(SPELL_CRUSADE_STACKS);
                return;
            case SPELL_CRUSADER_STRIKE:
                AddHolyPower(player, 1);
                // Puissance empyréenne
                if (player->HasAura(TALENT_EMPYREAN_POWER) && roll_chance_i(15))
                    player->CastSpell(player, SPELL_EMPYREAN_POWER, true);
                return;
            case SPELL_HAMMER_OF_THE_RIGHTEOUS:
                AddHolyPower(player, 1);
                return;
            default:
                break;
        }

        if (IsJudgement(id))
        {
            // Foi du croisé: one more for the Protection Paladin
            AddHolyPower(player, 1 + (IsProtection(player) && player->HasAura(TALENT_CRUSADERS_FAITH) ? 1 : 0));
            return;
        }

        switch (firstRank)
        {
            case SPELL_HAMMER_OF_WRATH_R1:
                AddHolyPower(player, 1);
                break;
            case SPELL_AVENGERS_SHIELD_R1:
                // Bouclier béni
                if (player->HasAura(TALENT_BLESSED_SHIELD))
                    AddHolyPower(player, 1);
                break;
            case SPELL_HOLY_SHOCK_R1:
                if (!IsHoly(player))
                    break;
                AddHolyPower(player, 1);
                SpendShockCharge(player, state, id);
                // Infusion de lumière
                if (int32 const chance = player->HasAura(TALENT_INFUSION_2) ? 30 :
                    player->HasAura(TALENT_INFUSION_1) ? 15 : 0)
                    if (roll_chance_i(chance))
                        player->CastSpell(player, SPELL_INFUSION_OF_LIGHT, true);
                // Lueur de lumière: the target carries one, and every glimmer answers
                if (player->HasAura(TALENT_GLIMMER))
                {
                    AddGlimmer(player, state, target);
                    PulseGlimmers(player, state);
                }
                break;
            case SPELL_FLASH_OF_LIGHT_R1:
            case SPELL_HOLY_LIGHT_R1:
                if (player->HasAura(SPELL_INFUSION_OF_LIGHT))
                {
                    player->RemoveAurasDueToSpell(SPELL_INFUSION_OF_LIGHT);
                    AddHolyPower(player, 1);
                }
                break;
            default:
                break;
        }
    }
};

// --- Damage, heals, auras ------------------------------------------------------------------------------------------

class PaladinTalentUnitScript : public UnitScript
{
public:
    PaladinTalentUnitScript() : UnitScript("PaladinTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_ON_AURA_REMOVE
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!damage || !victim)
            return;

        // Défenseur ardent: the blow that would kill brings the Paladin back to 20% instead
        if (Player* player = Paladin(victim))
        {
            if (damage >= player->GetHealth() && player->HasAura(SPELL_ARDENT_DEFENDER))
            {
                player->RemoveAurasDueToSpell(SPELL_ARDENT_DEFENDER);
                damage = 0;
                uint32 const floor = player->CountPctFromMaxHealth(20);
                if (player->GetHealth() < floor)
                    player->SetHealth(floor);
            }
            return;
        }

        // Sentence d'exécution: what the Paladin deals the marked enemy counts toward the sentence
        Player* player = Paladin(attacker);
        if (player && victim->HasAura(SPELL_EXECUTION_SENTENCE, player->GetGUID()))
            GetState(player)->sentenced[victim->GetGUID()] += damage;
    }

    // L'art de la guerre: an auto attack may bring Blade of Justice back
    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        Player* player = Paladin(attacker);
        if (!player || !damage)
            return;
        int32 const chance = player->HasAura(TALENT_ART_OF_WAR_2) ? 20 : player->HasAura(TALENT_ART_OF_WAR_1) ? 10 : 0;
        if (!chance || !player->HasSpellCooldown(SPELL_BLADE_OF_JUSTICE) || !roll_chance_i(chance))
            return;
        player->RemoveSpellCooldown(SPELL_BLADE_OF_JUSTICE, true);
        player->CastSpell(player, SPELL_ART_OF_WAR, true);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Paladin(attacker);
        if (!player || !target || !spellInfo || damage <= 0)
            return;
        float factor = 1.0f;

        // Jugement final: the finishers hit the marked harder
        if (IsFinisher(spellInfo->Id) && target->HasAura(SPELL_FINAL_RECKONING_MARK, player->GetGUID()))
            factor *= 1.3f;

        // Rempart de fureur vertueuse: 20% a stack gathered by Avenger's Shield
        if (spellInfo->Id == SPELL_SHIELD_OF_THE_RIGHTEOUS)
            factor *= 1.0f + 0.2f * float(Stacks(player, SPELL_BULWARK));

        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // The beacons (Guide de foi, Guide de vertu) and Dispensateur de lumière relay a share of the Paladin's heals. The
    // hook takes (target, healer); its own relayed heals are left alone.
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        Player* player = Paladin(healer);
        if (!player || !target || !heal || !spellInfo || s_relaying || IsRelayedHeal(spellInfo->Id))
            return;
        PaladinState* state = GetState(player);
        uint32 const amount = heal;
        s_relaying = true;

        if (player->HasAura(SPELL_SPEC_HOLY))
        {
            // Guide de foi: half of every heal on someone else
            if (Unit* faith = ObjectAccessor::GetUnit(*player, state->faithTarget))
                if (faith != target && faith->HasAura(SPELL_BEACON_OF_FAITH, player->GetGUID()) &&
                    faith->IsWithinDistInMap(player, 60.0f))
                    Heal(player, faith, SPELL_BEACON_HEAL, amount / 2);

            // Guide de vertu: a heal on one beacon reaches the others at 40%
            bool const onVirtue = target->HasAura(SPELL_BEACON_OF_VIRTUE, player->GetGUID());
            if (onVirtue)
                for (ObjectGuid const& guid : state->virtueTargets)
                    if (Unit* beacon = ObjectAccessor::GetUnit(*player, guid))
                        if (beacon != target && beacon->HasAura(SPELL_BEACON_OF_VIRTUE, player->GetGUID()) &&
                            beacon->IsWithinDistInMap(player, 60.0f))
                            Heal(player, beacon, SPELL_BEACON_HEAL, amount * 2 / 5);
        }

        // Dispensateur de lumière: Word of Glory also heals 2 injured allies near its target, for half
        if (spellInfo->Id == SPELL_WORD_OF_GLORY && player->HasAura(TALENT_LIGHT_BEARER))
        {
            uint8 splashed = 0;
            for (Unit* ally : InjuredAllies(player, target, 10.0f))
            {
                if (splashed >= 2)
                    break;
                if (ally == target)
                    continue;
                Heal(player, ally, SPELL_LIGHT_BEARER_HEAL, amount / 2);
                ++splashed;
            }
        }
        s_relaying = false;
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = Paladin(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        uint32 const firstRank = FirstRank(spellInfo);

        // Avenger's Shield: the improved one silences, and the Bulwark gathers a stack per enemy hit
        if (firstRank == SPELL_AVENGERS_SHIELD_R1)
        {
            if (player->HasAura(TALENT_IMPROVED_AVENGERS_SHIELD) && victim->IsAlive())
                player->CastSpell(victim, SPELL_SHIELD_SILENCE, true);
            if (player->HasAura(TALENT_BULWARK))
            {
                uint8 const stacks = std::min<uint8>(BulwarkMax, Stacks(player, SPELL_BULWARK) + 1);
                ShowStacks(player, SPELL_BULWARK, stacks);
            }
            return;
        }

        // Croisé vengeur: Crusader Strike and Holy Shock's damage heals the 3 most injured allies near, twice over
        if (player->HasAura(SPELL_AVENGING_CRUSADER) &&
            (spellInfo->Id == SPELL_CRUSADER_STRIKE || firstRank == SPELL_HOLY_SHOCK_DAMAGE_R1))
        {
            std::vector<Unit*> const allies = InjuredAllies(player, player, 30.0f);
            for (std::size_t index = 0; index < allies.size() && index < 3; ++index)
                Heal(player, allies[index], SPELL_AVENGING_CRUSADER_HEAL, damage * 2);
        }
    }

    // Sentence d'exécution: the sentence falls as the mark ends
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!unit || !aurApp || aurApp->GetBase()->GetId() != SPELL_EXECUTION_SENTENCE)
            return;
        Player* player = Paladin(aurApp->GetBase()->GetCaster());
        if (!player)
            return;
        PaladinState* state = GetState(player);
        auto const itr = state->sentenced.find(unit->GetGUID());
        uint64 const dealt = itr == state->sentenced.end() ? 0 : itr->second;
        if (itr != state->sentenced.end())
            state->sentenced.erase(itr);
        if (mode != AURA_REMOVE_BY_EXPIRE || !unit->IsAlive() || !player->IsInMap(unit))
            return;
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_EXECUTION_SENTENCE_DAMAGE);
        int32 const damage = int32(std::min<uint64>(uint64(spellInfo ? spellInfo->Effects[EFFECT_0].CalcValue() : 0) +
            dealt / 5, uint64(std::numeric_limits<int32>::max())));
        player->CastCustomSpell(unit, SPELL_EXECUTION_SENTENCE_DAMAGE, &damage, nullptr, nullptr, true);
    }

private:
    static bool s_relaying;
};

bool PaladinTalentUnitScript::s_relaying = false;

// --- Every update: Holy Shock's charges, Holy Power out of combat, Consecration, Avenging Wrath's companions --------

class PaladinTalentPlayerScript : public PlayerScript
{
public:
    PaladinTalentPlayerScript() : PlayerScript("PaladinTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_PALADIN)
            return;
        PaladinState* state = GetState(player);

        UpdateShockCharges(player, state, diff);
        UpdateHolyPower(player, state, diff);
        UpdateGround(player, state, diff);
    }

private:
    // Holy Power fades 10 s after combat
    static void UpdateHolyPower(Player* player, PaladinState* state, uint32 diff)
    {
        if (player->IsInCombat() || !player->HasAura(SPELL_HOLY_POWER))
        {
            state->outOfCombatMs = 0;
            return;
        }
        state->outOfCombatMs += diff;
        if (state->outOfCombatMs < HolyPowerFadeMs)
            return;
        state->outOfCombatMs = 0;
        player->RemoveAurasDueToSpell(SPELL_HOLY_POWER);
    }

    // A few times a second: Sol consacré while the Paladin stands in its own Consecration, Avenging Wrath's
    // companions (Courroux de la justice, Ailes de la vengeance) while it is up, Crusade's stacks gone with it
    static void UpdateGround(Player* player, PaladinState* state, uint32 diff)
    {
        state->groundTimer += diff;
        if (state->groundTimer < GroundCheckMs)
            return;
        state->groundTimer = 0;

        bool const inside = player->IsAlive() && player->HasAura(TALENT_CONSECRATED_GROUND) &&
            InOwnConsecration(player);
        Hold(player, SPELL_CONSECRATED_GROUND, inside);

        bool const wrath = player->HasAura(SPELL_AVENGING_WRATH);
        Hold(player, SPELL_JUSTICE_WRATH, wrath && player->HasAura(TALENT_JUSTICE_WRATH));
        Hold(player, SPELL_WINGS, wrath && player->HasAura(TALENT_WINGS));

        if (!player->HasAura(SPELL_CRUSADE))
            player->RemoveAurasDueToSpell(SPELL_CRUSADE_STACKS);
    }

    static void Hold(Player* player, uint32 spellId, bool wanted)
    {
        if (wanted && !player->HasAura(spellId))
            player->AddAura(spellId, player);
        else if (!wanted)
            player->RemoveAurasDueToSpell(spellId);
    }
};
}

void AddPaladinTalentScripts()
{
    new PaladinTalentSpellScript();
    new PaladinTalentUnitScript();
    new PaladinTalentPlayerScript();
}
