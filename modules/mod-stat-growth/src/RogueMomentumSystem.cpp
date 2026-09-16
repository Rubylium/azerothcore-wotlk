#include "RogueMomentumSystem.h"

#include "Chat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <list>
#include <string>
#include <unordered_map>

namespace
{
constexpr uint32 SinisterStrikeBase = 1752;
constexpr uint32 QuickCut = 90010;
constexpr uint32 ShadowLunge = 90011;
constexpr uint32 Riposte = 90012;
constexpr uint32 SanguineVeil = 90013;
constexpr uint32 OpeningAura = 90014;
constexpr uint32 BattleTempoAura = 90015;
constexpr uint32 KillingMomentumAura = 90016;
constexpr uint32 CrimsonSweep = 90017;
constexpr uint32 CrimsonWounds = 90018;
constexpr uint32 GuardedRhythm = 90020;
constexpr uint32 LegacyQuickCut = 16511;
constexpr uint32 LegacyShadowLunge = 36554;
constexpr uint32 LegacyRiposte = 14278;
constexpr uint32 EviscerateBase = 2098;
constexpr uint32 SinisterEnergyGain = 45;
constexpr uint32 MomentumDuration = 6000;
constexpr uint32 BattleTempoDuration = 8000;
constexpr uint32 SanguineVeilHealingPercent = 50;
constexpr std::array<uint32, 2> VanguardRhythmRanks = { 13732, 13863 };
constexpr std::array<uint32, 5> AggressionRanks = { 18427, 18428, 18429, 61330, 61331 };
constexpr std::array<uint32, 2> CrimsonReachRanks = { 5952, 51679 };
constexpr uint32 RiposteMastery = 14251;

struct RogueState
{
    bool opening = false;
    uint8 battleTempoStacks = 0;
    uint32 battleTempoRemaining = 0;
    uint8 killingMomentumStacks = 0;
    uint32 killingMomentumRemaining = 0;
    float movementMultiplier = 1.0f;
};

std::unordered_map<ObjectGuid::LowType, RogueState> rogueStates;

bool IsRogue(Player const* player)
{
    return player && player->getClass() == CLASS_ROGUE;
}

RogueState& GetState(Player* player)
{
    return rogueStates[player->GetGUID().GetCounter()];
}

template <std::size_t Size>
uint8 GetTalentRank(Player const* player, std::array<uint32, Size> const& ranks)
{
    for (std::size_t index = ranks.size(); index > 0; --index)
        if (player->HasSpell(ranks[index - 1]))
            return uint8(index);
    return 0;
}

void SetDisplayAura(Player* player, uint32 spellId, uint32 duration, uint8 stacks = 1)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);

    if (!aura)
        return;

    if (aura->GetMaxDuration() != int32(duration))
        aura->SetMaxDuration(int32(duration));
    aura->SetDuration(int32(duration));
    if (aura->GetStackAmount() != stacks)
        aura->SetStackAmount(stacks);
}

void ApplyGuardedRhythm(Player* player)
{
    if (GetTalentRank(player, VanguardRhythmRanks) > 0)
        SetDisplayAura(player, GuardedRhythm, 4000);
}

void SetMovementMultiplier(Player* player, RogueState& state, float multiplier)
{
    if (state.movementMultiplier == multiplier)
        return;

    float const currentRate = player->GetSpeedRate(MOVE_RUN);
    float const baseRate = state.movementMultiplier > 0.0f ? currentRate / state.movementMultiplier : currentRate;
    state.movementMultiplier = multiplier;
    player->SetSpeed(MOVE_RUN, baseRate * multiplier, true);
}

void ClearBattleTempo(Player* player, RogueState& state)
{
    if (state.battleTempoStacks > 0)
    {
        float const bonus = float(state.battleTempoStacks * 3);
        player->ApplyAttackTimePercentMod(BASE_ATTACK, bonus, false);
        player->ApplyAttackTimePercentMod(OFF_ATTACK, bonus, false);
    }

    state.battleTempoStacks = 0;
    state.battleTempoRemaining = 0;
    player->RemoveAura(BattleTempoAura);
}

void ApplyBattleTempo(Player* player, RogueState& state, uint8 comboPoints)
{
    ClearBattleTempo(player, state);
    state.battleTempoStacks = std::clamp<uint8>(comboPoints, 1, 5);
    state.battleTempoRemaining = BattleTempoDuration;
    float const bonus = float(state.battleTempoStacks * 3);
    player->ApplyAttackTimePercentMod(BASE_ATTACK, bonus, true);
    player->ApplyAttackTimePercentMod(OFF_ATTACK, bonus, true);
    SetDisplayAura(player, BattleTempoAura, BattleTempoDuration, state.battleTempoStacks);
}

void HandleSinisterStrike(Player* player)
{
    if (!IsRogue(player))
        return;

    RogueState& state = GetState(player);
    player->EnergizeBySpell(player, SinisterStrikeBase, SinisterEnergyGain, POWER_ENERGY);
    if (!state.opening)
    {
        state.opening = true;
        SetDisplayAura(player, OpeningAura, uint32(-1));
    }
}

void HandleQuickCut(Player* player, Unit* target)
{
    if (!IsRogue(player))
        return;

    RogueState& state = GetState(player);
    if (!state.opening)
        return;

    state.opening = false;
    player->RemoveAura(OpeningAura);
    player->AddComboPoints(target, 1);
}

void HandleShadowLunge(Player* player, Unit* target)
{
    if (!IsRogue(player))
        return;

    if (target)
        player->AddComboPoints(target, 1);
}

void HandleRiposte(Player* player)
{
    if (!IsRogue(player))
        return;

    uint32 const energy = player->HasSpell(RiposteMastery) ? 40 : 20;
    player->EnergizeBySpell(player, Riposte, energy, POWER_ENERGY);
}

uint8 EnsureRogueAbilities(Player* player)
{
    if (player->HasSpell(LegacyQuickCut))
        player->removeSpell(LegacyQuickCut, 1, false);
    if (player->HasSpell(LegacyShadowLunge))
        player->removeSpell(LegacyShadowLunge, 1, false);
    if (player->HasSpell(LegacyRiposte))
        player->removeSpell(LegacyRiposte, 1, false);

    uint8 learned = 0;
    if (player->GetLevel() >= 4 && !player->HasSpell(QuickCut))
    {
        player->learnSpell(QuickCut, false);
        ++learned;
    }
    if (player->GetLevel() >= 8 && !player->HasSpell(ShadowLunge))
    {
        player->learnSpell(ShadowLunge, false);
        ++learned;
    }
    if (player->GetLevel() >= 10 && !player->HasSpell(Riposte))
    {
        player->learnSpell(Riposte, false);
        ++learned;
    }
    if (player->GetLevel() >= 10 && !player->HasSpell(SanguineVeil))
    {
        player->learnSpell(SanguineVeil, false);
        ++learned;
    }
    if (player->GetLevel() >= 18 && !player->HasSpell(CrimsonSweep))
    {
        player->learnSpell(CrimsonSweep, false);
        ++learned;
    }
    return learned;
}

class RogueSanguineVeilUnitScript : public UnitScript
{
public:
    RogueSanguineVeilUnitScript() : UnitScript("RogueSanguineVeilUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (Player* defendingRogue = victim ? victim->ToPlayer() : nullptr;
            IsRogue(defendingRogue) && damage > 0)
        {
            uint32 reductionPercent = 0;
            if (defendingRogue->HasAura(GuardedRhythm))
                reductionPercent += uint32(GetTalentRank(defendingRogue, VanguardRhythmRanks)) * 3;
            if (defendingRogue->HasSpell(RiposteMastery) && defendingRogue->HasAura(Riposte))
                reductionPercent += 10;
            if (reductionPercent > 0)
                damage = uint32(uint64(damage) * (100 - std::min<uint32>(reductionPercent, 90)) / 100);
        }

        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!IsRogue(player) || !victim || victim == player || damage == 0 ||
            !player->HasAura(SanguineVeil) || !player->IsAlive() || player->IsFullHealth())
            return;

        uint32 const effectiveDamage = std::min(damage, victim->GetHealth());
        uint32 const healing = std::max<uint32>(1,
            uint64(effectiveDamage) * SanguineVeilHealingPercent / 100);
        HealInfo healInfo(player, player, healing, sSpellMgr->GetSpellInfo(SanguineVeil), SPELL_SCHOOL_MASK_SHADOW);
        player->HealBySpell(healInfo);
    }
};

class SinisterStrikeEnergyScript : public SpellScript
{
    PrepareSpellScript(SinisterStrikeEnergyScript);

    void HandleAfterCast()
    {
        HandleSinisterStrike(GetCaster()->ToPlayer());
    }

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player) || !GetHitUnit())
            return;

        uint8 const rank = GetTalentRank(player, VanguardRhythmRanks);
        if (rank > 0)
            SetHitDamage(int32(float(GetHitDamage()) * (1.0f + float(rank) * 0.10f)));
        ApplyGuardedRhythm(player);
    }

    void Register() override
    {
        OnHit += SpellHitFn(SinisterStrikeEnergyScript::HandleHit);
        AfterCast += SpellCastFn(SinisterStrikeEnergyScript::HandleAfterCast);
    }
};

class RogueQuickCutScript : public SpellScript
{
    PrepareSpellScript(RogueQuickCutScript);

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player))
            return;

        RogueState& state = GetState(player);
        if (state.opening)
        {
            uint8 const rank = GetTalentRank(player, VanguardRhythmRanks);
            float const multiplier = 1.75f * (1.0f + float(rank) * 0.10f);
            SetHitDamage(int32(float(GetHitDamage()) * multiplier));
            ApplyGuardedRhythm(player);
            HandleQuickCut(player, GetHitUnit());
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(RogueQuickCutScript::HandleHit);
    }
};

class RogueShadowLungeScript : public SpellScript
{
    PrepareSpellScript(RogueShadowLungeScript);

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!IsRogue(player) || !target)
            return;

        uint8 const aggressionRank = GetTalentRank(player, AggressionRanks);
        if (aggressionRank > 0)
            SetHitDamage(int32(float(GetHitDamage()) * (1.0f + float(aggressionRank) * 0.03f)));
        HandleShadowLunge(player, target);
    }

    void Register() override
    {
        OnHit += SpellHitFn(RogueShadowLungeScript::HandleHit);
    }
};

class RogueRiposteScript : public SpellScript
{
    PrepareSpellScript(RogueRiposteScript);

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player))
            return;

        float multiplier = 1.30f;
        if (player->HasSpell(RiposteMastery))
            multiplier *= 1.30f;
        multiplier *= 1.0f + float(GetTalentRank(player, AggressionRanks)) * 0.03f;
        SetHitDamage(int32(float(GetHitDamage()) * multiplier));
        HandleRiposte(player);
    }

    void Register() override
    {
        OnHit += SpellHitFn(RogueRiposteScript::HandleHit);
    }
};

class RogueEviscerateScript : public SpellScript
{
    PrepareSpellScript(RogueEviscerateScript);

    uint8 comboPoints = 0;
    bool killedTarget = false;

    void HandleBeforeCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player))
            return;

        comboPoints = player->GetComboPoints();
    }

    void HandleAfterHit()
    {
        if (Unit* target = GetHitUnit())
            killedTarget = !target->IsAlive();
    }

    void HandleHit()
    {
        if (comboPoints > 0)
            SetHitDamage(int32(float(GetHitDamage()) * (1.10f + float(comboPoints) * 0.05f)));
    }

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player) || comboPoints == 0)
            return;

        RogueState& state = GetState(player);
        if (killedTarget)
            player->EnergizeBySpell(player, EviscerateBase, comboPoints * 10, POWER_ENERGY);
        ApplyBattleTempo(player, state, comboPoints);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(RogueEviscerateScript::HandleBeforeCast);
        OnHit += SpellHitFn(RogueEviscerateScript::HandleHit);
        AfterHit += SpellHitFn(RogueEviscerateScript::HandleAfterHit);
        AfterCast += SpellCastFn(RogueEviscerateScript::HandleAfterCast);
    }
};

class RogueCrimsonSweepScript : public SpellScript
{
    PrepareSpellScript(RogueCrimsonSweepScript);

    bool empoweredByOpening = false;
    bool comboPointGranted = false;

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player))
            return;

        float const radius = 8.0f + float(GetTalentRank(player, CrimsonReachRanks)) * 2.0f;
        targets.remove_if([player, radius](WorldObject* target)
        {
            return !target || !player->IsWithinDistInMap(target, radius);
        });
    }

    void HandleBeforeCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!IsRogue(player))
            return;

        RogueState& state = GetState(player);
        empoweredByOpening = state.opening;
        if (empoweredByOpening)
        {
            state.opening = false;
            player->RemoveAura(OpeningAura);
        }
    }

    void HandleHit()
    {
        Player* player = GetCaster()->ToPlayer();
        Unit* target = GetHitUnit();
        if (!IsRogue(player) || !target || !player->IsValidAttackTarget(target))
            return;

        float const openingMultiplier = empoweredByOpening ? 1.5f : 1.0f;
        float const aggressionMultiplier =
            1.0f + float(GetTalentRank(player, AggressionRanks)) * 0.03f;
        float const reachMultiplier =
            1.0f + float(GetTalentRank(player, CrimsonReachRanks)) * 0.15f;
        uint32 const strikeDamage = std::max<uint32>(1,
            uint32(float(GetHitDamage()) * 2.5f * openingMultiplier * aggressionMultiplier));
        int32 const bleedDamage = std::max<int32>(1,
            int32(float(strikeDamage) * 0.30f * reachMultiplier));

        SetHitDamage(strikeDamage);
        player->CastCustomSpell(CrimsonWounds, SPELLVALUE_BASE_POINT0, bleedDamage, target, true);
        if (Aura* aura = target->GetAura(CrimsonWounds, player->GetGUID()))
        {
            int32 const duration = empoweredByOpening ? 10000 : 6000;
            aura->SetMaxDuration(duration);
            aura->SetDuration(duration);
        }

        if (empoweredByOpening && !comboPointGranted)
        {
            player->AddComboPoints(target, 1);
            comboPointGranted = true;
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(RogueCrimsonSweepScript::HandleBeforeCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            RogueCrimsonSweepScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(RogueCrimsonSweepScript::HandleHit);
    }
};

class RogueCrimsonWoundsAuraScript : public AuraScript
{
    PrepareAuraScript(RogueCrimsonWoundsAuraScript);

    void HandlePeriodic(AuraEffect const*)
    {
        Player* player = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (IsRogue(player) && GetTarget() && GetTarget()->IsAlive())
            player->EnergizeBySpell(player, CrimsonWounds, 3, POWER_ENERGY);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(
            RogueCrimsonWoundsAuraScript::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};
}

void LearnRogueMomentumAbilities(Player* player)
{
    if (!IsRogue(player))
        return;

    uint8 const learned = EnsureRogueAbilities(player);
    if (learned > 0)
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffb048f8Rogue Momentum awakened: learned {} new {}.|r",
            learned, learned == 1 ? "ability" : "abilities");

    GetState(player);
}

void OnRogueMomentumKill(Player* player)
{
    if (!IsRogue(player))
        return;

    RogueState& state = GetState(player);
    state.killingMomentumStacks = std::min<uint8>(5, state.killingMomentumStacks + 1);
    state.killingMomentumRemaining = MomentumDuration;
    SetMovementMultiplier(player, state, 1.0f + float(state.killingMomentumStacks) * 0.05f);
    player->RemoveSpellCooldown(ShadowLunge, true);
    SetDisplayAura(player, KillingMomentumAura, MomentumDuration, state.killingMomentumStacks);
}

void UpdateRogueMomentum(Player* player, uint32 diff)
{
    if (!IsRogue(player))
        return;

    uint8 const learned = EnsureRogueAbilities(player);
    if (learned > 0)
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffb048f8Rogue Momentum repaired: learned {} missing {}.|r",
            learned, learned == 1 ? "ability" : "abilities");

    RogueState& state = GetState(player);
    if (state.battleTempoRemaining > 0)
    {
        if (diff >= state.battleTempoRemaining)
        {
            ClearBattleTempo(player, state);
        }
        else
            state.battleTempoRemaining -= diff;
    }

    if (state.killingMomentumRemaining > 0)
    {
        if (diff >= state.killingMomentumRemaining)
        {
            state.killingMomentumRemaining = 0;
            state.killingMomentumStacks = 0;
            SetMovementMultiplier(player, state, 1.0f);
            player->RemoveAura(KillingMomentumAura);
        }
        else
            state.killingMomentumRemaining -= diff;
    }

    if (state.killingMomentumStacks == 5)
        player->RemoveSpellCooldown(ShadowLunge, true);

    if (state.opening && !player->HasAura(OpeningAura))
        SetDisplayAura(player, OpeningAura, uint32(-1));
    if (state.battleTempoStacks > 0 && !player->HasAura(BattleTempoAura))
        SetDisplayAura(player, BattleTempoAura, state.battleTempoRemaining, state.battleTempoStacks);
    if (state.killingMomentumStacks > 0 && !player->HasAura(KillingMomentumAura))
        SetDisplayAura(player, KillingMomentumAura, state.killingMomentumRemaining, state.killingMomentumStacks);
}

void ApplyRogueMomentumRegeneration(Player* player, Powers power, float& amount)
{
    if (!IsRogue(player) || power != POWER_ENERGY)
        return;

    RogueState& state = GetState(player);
    amount *= 1.0f + float(state.killingMomentumStacks) * 0.05f;
}

void ClearRogueMomentum(Player* player)
{
    if (!IsRogue(player))
        return;

    auto const iterator = rogueStates.find(player->GetGUID().GetCounter());
    if (iterator == rogueStates.end())
        return;

    ClearBattleTempo(player, iterator->second);
    SetMovementMultiplier(player, iterator->second, 1.0f);
    player->RemoveAura(OpeningAura);
    player->RemoveAura(KillingMomentumAura);
    rogueStates.erase(iterator);
}

void AddRogueMomentumScripts()
{
    new RogueSanguineVeilUnitScript();
    RegisterSpellScript(SinisterStrikeEnergyScript);
    RegisterSpellScript(RogueQuickCutScript);
    RegisterSpellScript(RogueShadowLungeScript);
    RegisterSpellScript(RogueRiposteScript);
    RegisterSpellScript(RogueEviscerateScript);
    RegisterSpellScript(RogueCrimsonSweepScript);
    RegisterSpellScript(RogueCrimsonWoundsAuraScript);
}
