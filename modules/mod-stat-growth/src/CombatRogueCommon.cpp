#include "CombatRogue.h"
#include "CombatRogueCommon.h"

#include "CellImpl.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "Random.h"
#include "SpellAuras.h"
#include "SpellInfo.h"

#include <algorithm>

namespace CombatRogue
{
namespace
{
// Rank spells of each reworked talent, indexed by Talent (WotLK Combat talent spell ids, rank 1 first)
constexpr std::array<std::array<uint32, 5>, uint8(Talent::Count)> TalentRankSpells = { {
    { 13741, 13793, 13792 },                // Keen Openings (Improved Gouge slot)
    { 13732, 13863 },                       // Honed Blades (Improved Sinister Strike slot)
    { 13715, 13848, 13849, 13851, 13852 },  // Crimson Discipline (Dual Wield Specialization slot)
    { 14165, 14166 },                       // Tempo Mastery (Improved Slice and Dice slot)
    { 13713, 13853, 13854 },                // Parry Instinct (Deflection slot)
    { 13705, 13832, 13843, 13844, 13845 },  // Surgical Precision (Precision slot)
    { 13742, 13872 },                       // Fleet Footwork (Endurance slot)
    { 14251 },                              // Counter Rhythm (Riposte slot)
    { 13706, 13804, 13805, 13806, 13807 },  // Deep Cuts (Close Quarters Combat slot)
    { 13754, 13867 },                       // Opportunist (Improved Kick slot)
    { 13743, 13875 },                       // Relentless Pursuit (Improved Sprint slot)
    { 13712, 13788, 13789 },                // Sanguine Instinct (Lightning Reflexes slot)
    { 18427, 18428, 18429, 61330, 61331 },  // Lethal Edge (Aggression slot)
    { 13709, 13800, 13801, 13802, 13803 },  // Twin Cuts (Mace Specialization slot)
    { 13877 },                              // Flowing Strikes (Blade Flurry slot)
    { 13960, 13961, 13962, 13963, 13964 },  // Rending Arc (Hack and Slash slot)
    { 30919, 30920 },                       // Relentless Flow (Weapon Expertise slot)
    { 31124, 31126 },                       // Widening Arcs (Blade Twisting slot)
    { 31122, 31123, 61329 },                // Tempo Echo (Vitality slot)
    { 13750 },                              // Adrenaline Flow (Adrenaline Rush slot)
    { 31130, 31131 },                       // Sanguine Resolve (Nerves of Steel slot)
    { 5952, 51679 },                        // Relentless Tempo (Throwing Specialization slot)
    { 35541, 35550, 35551, 35552, 35553 },  // Hemorrhaging Blades (Combat Potency slot)
    { 51672, 51674 },                       // Riposte Echo (Unfair Advantage slot)
    { 32601 },                              // Waltz of Blades (Surprise Attacks slot)
    { 51682, 58413 },                       // Crimson Frenzy (Savage Combat slot)
    { 51685, 51686, 51687, 51688, 51689 },  // Executioner's Tempo (Prey on the Weak slot)
    { 51690 }                               // Crimson Cadence (Killing Spree slot)
} };

constexpr uint32 MaxBleedEnergyPerWindow = 8;
constexpr Milliseconds BleedEnergyWindow = 2s;
constexpr Milliseconds DefenseWindow = 5s;
constexpr Milliseconds CounterRhythmCooldown = 6s;
}

bool IsRogue(Player const* player)
{
    return player && player->getClass() == CLASS_ROGUE;
}

RogueData& GetData(Player* player)
{
    return *player->CustomData.GetDefault<RogueData>("CombatRogue");
}

uint8 GetTalentRank(Player const* player, Talent talent)
{
    auto const& ranks = TalentRankSpells[uint8(talent)];
    uint8 const spec = player->GetActiveSpec();
    for (uint8 rank = uint8(ranks.size()); rank > 0; --rank)
        if (ranks[rank - 1] && player->HasTalent(ranks[rank - 1], spec))
            return rank;
    return 0;
}

bool HasEvolution(Player const* player, Evolution evolution)
{
    return player->GetLevel() >= uint8(evolution);
}

void GrantOpening(Player* player)
{
    uint8 const maxStacks = HasEvolution(player, EVOLUTION_OPENING_STACKS) ? 2 : 1;
    if (Aura* aura = player->GetAura(SPELL_OPENING))
    {
        if (aura->GetStackAmount() < maxStacks)
            aura->ModStackAmount(1);
        else
            aura->RefreshDuration();
        return;
    }

    player->AddAura(SPELL_OPENING, player);
}

bool ConsumeOpening(Player* player)
{
    Aura* aura = player->GetAura(SPELL_OPENING);
    if (!aura)
        return false;

    aura->ModStackAmount(-1);
    return true;
}

void TryOpeningProc(Player* player)
{
    int32 const chance = (HasEvolution(player, EVOLUTION_KEENER_OPENINGS) ? 35 : 25) +
        5 * GetTalentRank(player, Talent::KeenOpenings);
    if (roll_chance_i(chance))
        GrantOpening(player);
}

uint32 TakeOverflowEnergy(Player* player, uint32 baseCost)
{
    uint32 const energy = player->GetPower(POWER_ENERGY);
    uint32 const overflow = std::min<uint32>(30, energy > baseCost ? energy - baseCost : 0);
    if (overflow)
        player->ModifyPower(POWER_ENERGY, -int32(overflow));
    return overflow;
}

float GetOverflowMultiplier(uint32 overflowEnergy)
{
    // 30 extra Energy = +50%
    return 1.0f + float(overflowEnergy) / 60.0f;
}

void ApplyBattleTempo(Player* player, uint8 comboPoints)
{
    if (!comboPoints)
        return;

    Aura* aura = player->GetAura(SPELL_BATTLE_TEMPO);
    if (!aura)
        aura = player->AddAura(SPELL_BATTLE_TEMPO, player);
    if (!aura)
        return;

    int32 const duration = 10000 + 2000 * GetTalentRank(player, Talent::TempoMastery);
    aura->SetStackAmount(std::min<uint8>(comboPoints, 5));
    aura->SetMaxDuration(duration);
    aura->SetDuration(duration);
}

void ExtendBattleTempo(Player* player, uint32 milliseconds)
{
    Aura* aura = player->GetAura(SPELL_BATTLE_TEMPO);
    if (!aura)
        return;

    int32 const duration = aura->GetDuration() + int32(milliseconds);
    aura->SetMaxDuration(std::max(aura->GetMaxDuration(), duration));
    aura->SetDuration(duration);
}

void OnFinisherCast(Player* player, uint8 comboPoints, Unit* target, bool daggerfall)
{
    if (!comboPoints)
        return;

    // Crimson Daggerfall feeds on the other finishers: each one shortens its cooldown, and a full one may rain a
    // free Daggerfall (a triggered cast: 5 combo points, no energy, and its cooldown is left alone)
    if (!daggerfall && player->HasSpell(SPELL_CRIMSON_DAGGERFALL))
    {
        player->ModifySpellCooldown(SPELL_CRIMSON_DAGGERFALL,
            -int32(DAGGERFALL_COOLDOWN_PER_COMBO_POINT_MS * comboPoints));

        if (comboPoints >= 5 && roll_chance_i(DAGGERFALL_FREE_CAST_CHANCE) &&
            !GetEnemiesInRange(player, GetAoeRadius(player, false)).empty())
            player->CastSpell(player, SPELL_CRIMSON_DAGGERFALL, TRIGGERED_FULL_MASK);
    }

    if (uint8 const relentlessTempo = GetTalentRank(player, Talent::RelentlessTempo))
        if (roll_chance_i(std::min(100, 10 * relentlessTempo * comboPoints)))
            GrantOpening(player);

    // Combo points are already cleared when the after-cast hooks run, so the refund is not lost
    if (GetTalentRank(player, Talent::AdrenalineFlow) && roll_chance_i(20))
        player->AddComboPoints(target, 1);

    if (!GetTalentRank(player, Talent::CrimsonCadence))
        return;

    RogueData& data = GetData(player);
    if (++data.finisherCount % 5)
        return;

    if (GetEnemiesInRange(player, GetAoeRadius(player, true)).size() >= 2)
        player->CastSpell(player, SPELL_BLOOD_WALTZ, TRIGGERED_FULL_MASK);
    else if (target && data.lastEviscerateDamage)
        CastBladeEcho(player, target, data.lastEviscerateDamage / 2, 300ms);
}

float GetAoeRadius(Player const* player, bool bloodWaltz)
{
    float radius = 8.0f + 2.0f * GetTalentRank(player, Talent::WideningArcs);
    if (bloodWaltz && HasEvolution(player, EVOLUTION_BLOOD_WALTZ_RADIUS))
        radius += 2.0f;
    return radius;
}

void ApplyCrimsonWounds(Player* player, Unit* target, uint32 durationMs)
{
    float const weaponDamage =
        (player->GetFloatValue(UNIT_FIELD_MINDAMAGE) + player->GetFloatValue(UNIT_FIELD_MAXDAMAGE)) / 2.0f;
    float const deepCuts = 1.0f + 0.06f * GetTalentRank(player, Talent::DeepCuts);
    int32 const tickDamage = std::max<int32>(1, int32(weaponDamage * 0.20f * deepCuts));

    player->CastCustomSpell(SPELL_CRIMSON_WOUNDS, SPELLVALUE_BASE_POINT0, tickDamage, target, TRIGGERED_FULL_MASK);

    Aura* aura = target->GetAura(SPELL_CRIMSON_WOUNDS, player->GetGUID());
    if (!aura)
        return;

    uint8 const maxStacks = HasEvolution(player, EVOLUTION_CRIMSON_WOUNDS_STACKS) ? 3 : 1;
    if (aura->GetStackAmount() > maxStacks)
        aura->SetStackAmount(maxStacks);
    aura->SetMaxDuration(int32(durationMs));
    aura->SetDuration(int32(durationMs));
}

void RefreshCrimsonWounds(Player* player, Unit* target, uint32 extendMs)
{
    Aura* aura = target->GetAura(SPELL_CRIMSON_WOUNDS, player->GetGUID());
    if (!aura)
        return;

    if (extendMs)
        aura->SetDuration(std::min(aura->GetDuration() + int32(extendMs), aura->GetMaxDuration()));
    else
        aura->RefreshDuration();
}

void OnCrimsonWoundsTick(Player* player)
{
    RogueData& data = GetData(player);
    Milliseconds const now = GameTime::GetGameTimeMS();
    if (now - data.bleedEnergyWindowStart >= BleedEnergyWindow)
    {
        data.bleedEnergyWindowStart = now;
        data.bleedEnergyGranted = 0;
    }

    if (data.bleedEnergyGranted < MaxBleedEnergyPerWindow)
    {
        uint32 const energy = std::min<uint32>(2, MaxBleedEnergyPerWindow - data.bleedEnergyGranted);
        data.bleedEnergyGranted += energy;
        EnergizeRogue(player, SPELL_CRIMSON_WOUNDS, energy);
    }

    if (!GetTalentRank(player, Talent::CrimsonFrenzy))
        return;

    Aura* frenzy = player->GetAura(SPELL_CRIMSON_FRENZY);
    if (!frenzy)
        frenzy = player->AddAura(SPELL_CRIMSON_FRENZY, player);
    else if (frenzy->GetStackAmount() < 5)
        frenzy->ModStackAmount(1);

    if (frenzy)
    {
        frenzy->SetMaxDuration(5000);
        frenzy->SetDuration(5000);
    }
}

std::list<Unit*> GetEnemiesInRange(Player* player, float range)
{
    std::list<Unit*> enemies;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(player, enemies, check);
    Cell::VisitObjects(player, searcher, range);
    enemies.remove_if([player](Unit* enemy)
    {
        return !enemy->IsAlive() || !player->IsValidAttackTarget(enemy);
    });
    return enemies;
}

float GetExecutionerMultiplier(Player const* player, Unit const* target)
{
    uint8 const rank = GetTalentRank(player, Talent::ExecutionersTempo);
    return rank && target && target->HealthBelowPct(35) ? 1.0f + 0.04f * rank : 1.0f;
}

void CastBladeEcho(Player* player, Unit* target, uint32 damage, Milliseconds delay)
{
    if (!target || !damage)
        return;

    if (delay == 0ms)
    {
        player->CastCustomSpell(SPELL_BLADE_ECHO, SPELLVALUE_BASE_POINT0, int32(damage), target, TRIGGERED_FULL_MASK);
        return;
    }

    // The event belongs to the player, so it can never outlive it
    ObjectGuid const targetGuid = target->GetGUID();
    player->m_Events.AddEventAtOffset([player, targetGuid, damage]()
    {
        Unit* echoTarget = ObjectAccessor::GetUnit(*player, targetGuid);
        if (player->IsAlive() && echoTarget && echoTarget->IsAlive())
            player->CastCustomSpell(SPELL_BLADE_ECHO, SPELLVALUE_BASE_POINT0, int32(damage), echoTarget,
                TRIGGERED_FULL_MASK);
    }, delay);
}

void EnergizeRogue(Player* player, uint32 spellId, uint32 amount)
{
    if (amount)
        player->EnergizeBySpell(player, spellId, amount, POWER_ENERGY);
}

void OnRogueDefended(Player* player)
{
    RogueData& data = GetData(player);
    Milliseconds const now = GameTime::GetGameTimeMS();
    data.lastDefense = now;

    if (uint8 const parryInstinct = GetTalentRank(player, Talent::ParryInstinct))
        EnergizeRogue(player, SPELL_CRIMSON_DUELIST, 3 * parryInstinct);

    if (GetTalentRank(player, Talent::CounterRhythm) && now >= data.counterRhythmReadyAt)
    {
        data.counterRhythmReadyAt = now + CounterRhythmCooldown;
        player->RemoveSpellCooldown(SPELL_RIPOSTE, true);
    }
}

bool DefendedRecently(Player* player)
{
    RogueData const& data = GetData(player);
    return data.lastDefense > 0ms && GameTime::GetGameTimeMS() - data.lastDefense <= DefenseWindow;
}

void OnKillingBlow(Player* player)
{
    Aura* momentum = player->GetAura(SPELL_KILLING_MOMENTUM);
    if (!momentum)
        momentum = player->AddAura(SPELL_KILLING_MOMENTUM, player);
    else if (momentum->GetStackAmount() < 5)
        momentum->ModStackAmount(1);

    if (momentum)
    {
        momentum->SetMaxDuration(6000);
        momentum->SetDuration(6000);
    }

    if (player->HasSpellCooldown(SPELL_SHADOW_LUNGE))
    {
        player->RemoveSpellCooldown(SPELL_SHADOW_LUNGE, true);
        GetData(player).shadowLungeWasReset = true;
    }

    if (uint8 const executioner = GetTalentRank(player, Talent::ExecutionersTempo))
        EnergizeRogue(player, SPELL_KILLING_MOMENTUM, 3 * executioner);
}
}

using namespace CombatRogue;

namespace
{
constexpr char ReworkSettingsSource[] = "mod_stat_growth_combat_rogue";
constexpr uint32 ReworkVersionSetting = 0;
constexpr uint32 ReworkVersion = 1;

// Spells a very early version of the rework taught directly; they are real Subtlety talents, so only strip them
// when the talent is not actually taken
constexpr std::array<uint32, 3> LegacyTalentSpells = { 16511, 36554, 14278 };

struct EvolutionMessage
{
    Evolution evolution;
    char const* text;
};

constexpr std::array<EvolutionMessage, 15> EvolutionMessages = { {
    { EVOLUTION_SINISTER_STRIKE_ENERGY, "Sinister Strike now generates 12 Energy." },
    { EVOLUTION_EVISCERATE_OPENING, "Eviscerate at 5 combo points now grants Opening." },
    { EVOLUTION_QUICK_CUT_BOTH_WEAPONS, "Quick Cut now strikes with both weapons." },
    { EVOLUTION_SHADOW_LUNGE_OPENING, "Shadow Lunge now grants Opening." },
    { EVOLUTION_CRIMSON_WOUNDS_STACKS, "Crimson Wounds now stacks up to 3 times." },
    { EVOLUTION_CRESCENT_SLASH_DAMAGE, "Crescent Slash now deals 125% weapon damage." },
    { EVOLUTION_RIPOSTE_DODGE, "Riposte now increases your dodge chance by 15% for 7 sec." },
    { EVOLUTION_KEENER_OPENINGS,
        "Sinister Strike grants Opening more often, and Blood Waltz at 5 combo points grants Opening." },
    { EVOLUTION_OPENING_STACKS, "Opening now stacks up to 2 times." },
    { EVOLUTION_LUNGE_ENERGY_AND_VEIL,
        "Shadow Lunge restores 10 Energy after a reset, and Sanguine Veil heals for 20% of damage dealt." },
    { EVOLUTION_CRESCENT_SLASH_COMBO, "Crescent Slash grants a second combo point when it hits 3 or more enemies." },
    { EVOLUTION_CRIMSON_SWEEP_COOLDOWN, "Crimson Sweep cooldown reduced to 4 sec." },
    { EVOLUTION_DAGGERFALL_DAMAGE, "Crimson Daggerfall now deals 70% weapon damage per combo point." },
    { EVOLUTION_DAGGERFALL_DOT_BONUS,
        "Crimson Daggerfall now deals 50% more damage to enemies suffering from one of your damage-over-time effects." },
    { EVOLUTION_BLOOD_WALTZ_RADIUS, "Blood Waltz radius increased by 2 yards." }
} };

uint32 LearnUnlockedAbilities(Player* player)
{
    uint32 learned = 0;
    for (AbilityUnlock const& unlock : AbilityUnlocks)
    {
        if (player->GetLevel() < unlock.level || player->HasSpell(unlock.spellId))
            continue;

        player->learnSpell(unlock.spellId, false);
        if (player->HasSpell(unlock.spellId) && unlock.spellId != SPELL_CRIMSON_DUELIST)
            ++learned;
    }
    return learned;
}

void AnnounceLearned(Player* player, uint32 learned)
{
    if (learned)
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffb048f8Crimson Duelist: learned {} new {}.|r", learned, learned == 1 ? "ability" : "abilities");
}

void ApplyReworkTalentReset(Player* player)
{
    if (player->GetPlayerSetting(ReworkSettingsSource, ReworkVersionSetting).value >= ReworkVersion)
        return;

    if (player->resetTalents(true))
    {
        player->SendTalentsInfoData(false);
        ChatHandler(player->GetSession()).SendSysMessage(
            "|cffb048f8The Combat talent tree has been reworked: your talents were reset for free.|r");
    }

    PlayerSettingVector settings(1);
    settings[ReworkVersionSetting].value = ReworkVersion;
    player->UpdatePlayerSetting(ReworkSettingsSource, ReworkVersionSetting, ReworkVersion);
    CharacterDatabase.Execute(PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), ReworkSettingsSource, settings));
}
}

void OnCombatRogueLogin(Player* player)
{
    if (!IsRogue(player))
        return;

    for (uint32 spellId : LegacyTalentSpells)
        if (player->HasSpell(spellId) && !player->HasTalent(spellId, player->GetActiveSpec()))
            player->removeSpell(spellId, SPEC_MASK_ALL, false);

    AnnounceLearned(player, LearnUnlockedAbilities(player));
    ApplyReworkTalentReset(player);
}

void OnCombatRogueLevelChanged(Player* player, uint8 oldLevel)
{
    if (!IsRogue(player))
        return;

    if (player->GetLevel() < oldLevel)
    {
        for (AbilityUnlock const& unlock : AbilityUnlocks)
            if (unlock.level > player->GetLevel() && player->HasSpell(unlock.spellId))
                player->removeSpell(unlock.spellId, SPEC_MASK_ALL, false);
        LearnUnlockedAbilities(player);
        return;
    }

    if (player->GetLevel() <= oldLevel)
        return;

    AnnounceLearned(player, LearnUnlockedAbilities(player));

    for (EvolutionMessage const& message : EvolutionMessages)
        if (oldLevel < uint8(message.evolution) && HasEvolution(player, message.evolution))
            ChatHandler(player->GetSession()).PSendSysMessage("|cffb048f8Crimson Duelist evolves:|r {}", message.text);
}

void OnCombatRogueKill(Player* player, Unit* /*killed*/)
{
    if (IsRogue(player))
        OnKillingBlow(player);
}
