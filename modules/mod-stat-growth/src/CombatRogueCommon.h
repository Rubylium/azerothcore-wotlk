#ifndef MOD_STAT_GROWTH_COMBAT_ROGUE_COMMON_H
#define MOD_STAT_GROWTH_COMBAT_ROGUE_COMMON_H

#include "DataMap.h"
#include "Define.h"
#include "Duration.h"

#include <array>
#include <list>

class Player;
class Unit;

namespace CombatRogue
{
enum Spells : uint32
{
    SPELL_SINISTER_STRIKE   = 1752,
    SPELL_EVISCERATE        = 2098,
    SPELL_SLICE_AND_DICE    = 5171,
    SPELL_KICK              = 1766,
    SPELL_QUICK_CUT         = 90010,
    SPELL_SHADOW_LUNGE      = 90011,
    SPELL_RIPOSTE           = 90012,
    SPELL_SANGUINE_VEIL     = 90013,
    SPELL_OPENING           = 90014,
    SPELL_BATTLE_TEMPO      = 90015,
    SPELL_KILLING_MOMENTUM  = 90016,
    SPELL_CRIMSON_SWEEP     = 90017,
    SPELL_CRIMSON_WOUNDS    = 90018,
    SPELL_CRESCENT_SLASH    = 90100,
    SPELL_BLOOD_WALTZ       = 90101,
    SPELL_BLADE_ECHO        = 90102,
    SPELL_CRIMSON_FRENZY    = 90103,
    SPELL_CRIMSON_DUELIST   = 90104,
    SPELL_CRIMSON_DAGGERFALL = 90105
};

// Crimson Daggerfall: every other finisher takes this much off its cooldown per combo point spent, and one spending
// 5 combo points has this % chance to rain a free Daggerfall
constexpr uint32 DAGGERFALL_COOLDOWN_PER_COMBO_POINT_MS = 1000;
constexpr int32 DAGGERFALL_FREE_CAST_CHANCE = 20;

// Caster aura state not used by any class: set while Opening is up and required by Quick Cut in the client data, so the
// Quick Cut button lights up like a reactive ability
constexpr uint32 OPENING_AURA_STATE = 9;

// Spells granted to every rogue, in learn order
struct AbilityUnlock
{
    uint32 spellId;
    uint8 level;
};

constexpr std::array<AbilityUnlock, 9> AbilityUnlocks = { {
    { SPELL_CRIMSON_DUELIST, 1 },
    { SPELL_QUICK_CUT, 4 },
    { SPELL_SHADOW_LUNGE, 8 },
    { SPELL_RIPOSTE, 10 },
    { SPELL_CRESCENT_SLASH, 14 },
    { SPELL_SANGUINE_VEIL, 16 },
    { SPELL_CRIMSON_SWEEP, 18 },
    { SPELL_CRIMSON_DAGGERFALL, 20 },
    { SPELL_BLOOD_WALTZ, 26 }
} };

// Automatic upgrades of the kit while levelling (no spell, checked by level)
enum Evolution : uint8
{
    EVOLUTION_SINISTER_STRIKE_ENERGY    = 20,
    EVOLUTION_EVISCERATE_OPENING        = 25,
    EVOLUTION_QUICK_CUT_BOTH_WEAPONS    = 30,
    EVOLUTION_SHADOW_LUNGE_OPENING      = 35,
    EVOLUTION_CRIMSON_WOUNDS_STACKS     = 40,
    EVOLUTION_CRESCENT_SLASH_DAMAGE     = 42,
    EVOLUTION_RIPOSTE_DODGE             = 45,
    EVOLUTION_KEENER_OPENINGS           = 50,
    EVOLUTION_OPENING_STACKS            = 55,
    EVOLUTION_LUNGE_ENERGY_AND_VEIL     = 60,
    EVOLUTION_CRESCENT_SLASH_COMBO      = 62,
    EVOLUTION_CRIMSON_SWEEP_COOLDOWN    = 65,
    EVOLUTION_DAGGERFALL_DAMAGE         = 45,
    EVOLUTION_DAGGERFALL_DOT_BONUS      = 70,
    EVOLUTION_BLOOD_WALTZ_RADIUS        = 75
};

// All Combat talents are reworked; they reuse the WotLK talent spell ids (see the patcher)
enum class Talent : uint8
{
    KeenOpenings,
    HonedBlades,
    CrimsonDiscipline,
    TempoMastery,
    ParryInstinct,
    SurgicalPrecision,
    FleetFootwork,
    CounterRhythm,
    DeepCuts,
    Opportunist,
    RelentlessPursuit,
    SanguineInstinct,
    LethalEdge,
    TwinCuts,
    FlowingStrikes,
    RendingArc,
    RelentlessFlow,
    WideningArcs,
    TempoEcho,
    AdrenalineFlow,
    SanguineResolve,
    RelentlessTempo,
    HemorrhagingBlades,
    RiposteEcho,
    WaltzOfBlades,
    CrimsonFrenzy,
    ExecutionersTempo,
    CrimsonCadence,
    Count
};

// Per player state, stored in Player::CustomData
struct RogueData : public DataMap::Base
{
    uint32 sinisterStrikeCount = 0;
    uint32 finisherCount = 0;
    uint32 lastEviscerateDamage = 0;
    bool shadowLungeWasReset = false;
    Milliseconds lastDefense = 0ms;
    Milliseconds counterRhythmReadyAt = 0ms;
    Milliseconds bleedEnergyWindowStart = 0ms;
    uint32 bleedEnergyGranted = 0;
};

bool IsRogue(Player const* player);
RogueData& GetData(Player* player);
uint8 GetTalentRank(Player const* player, Talent talent);
bool HasEvolution(Player const* player, Evolution evolution);

// Opening proc (also lights up Quick Cut through a caster aura state)
void GrantOpening(Player* player);
bool ConsumeOpening(Player* player);
void TryOpeningProc(Player* player);

// Finishers
uint32 TakeOverflowEnergy(Player* player, uint32 baseCost);
float GetOverflowMultiplier(uint32 overflowEnergy);
void ApplyBattleTempo(Player* player, uint8 comboPoints);
void ExtendBattleTempo(Player* player, uint32 milliseconds);
// `daggerfall` is set by Crimson Daggerfall itself, which does not feed its own cooldown or free casts
void OnFinisherCast(Player* player, uint8 comboPoints, Unit* target, bool daggerfall = false);

// Bleeds and AoE
float GetAoeRadius(Player const* player, bool bloodWaltz);
void ApplyCrimsonWounds(Player* player, Unit* target, uint32 durationMs);
void RefreshCrimsonWounds(Player* player, Unit* target, uint32 extendMs = 0);
void OnCrimsonWoundsTick(Player* player);
std::list<Unit*> GetEnemiesInRange(Player* player, float range);

// Damage helpers
float GetExecutionerMultiplier(Player const* player, Unit const* target);
void CastBladeEcho(Player* player, Unit* target, uint32 damage, Milliseconds delay = 0ms);
void EnergizeRogue(Player* player, uint32 spellId, uint32 amount);

// Defensive procs from the hidden Crimson Duelist passive
void OnRogueDefended(Player* player);
bool DefendedRecently(Player* player);

// Killing Momentum
void OnKillingBlow(Player* player);
}

#endif
