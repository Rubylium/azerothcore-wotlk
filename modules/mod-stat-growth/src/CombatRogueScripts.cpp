#include "CombatRogue.h"
#include "CombatRogueCommon.h"
#include "CombatRogueDaggerfall.h"

#include "Player.h"
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

using namespace CombatRogue;

namespace
{
Player* GetRogue(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return IsRogue(player) ? player : nullptr;
}

void FilterByRadius(Player* player, std::list<WorldObject*>& targets, float radius)
{
    targets.remove_if([player, radius](WorldObject* target)
    {
        return !target || !player->IsWithinDistInMap(target, radius);
    });
}

// ---------------------------------------------------------------------------------------------------------------
// Builders
// ---------------------------------------------------------------------------------------------------------------

// -1752 Sinister Strike: free, generates Energy, chance to grant Opening, drives Flowing Strikes
class CombatRogueSinisterStrikeScript : public SpellScript
{
    PrepareSpellScript(CombatRogueSinisterStrikeScript);

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float const multiplier = (1.0f + 0.05f * GetTalentRank(player, Talent::HonedBlades)) *
            GetExecutionerMultiplier(player, target);
        SetHitDamage(int32(GetHitDamage() * multiplier));
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        uint32 const energy = (HasEvolution(player, EVOLUTION_SINISTER_STRIKE_ENERGY) ? 12 : 10) +
            2 * GetTalentRank(player, Talent::HonedBlades);
        EnergizeRogue(player, GetSpellInfo()->Id, energy);

        // Critical strikes always grant Opening through the Crimson Duelist passive
        TryOpeningProc(player);

        if (!GetTalentRank(player, Talent::FlowingStrikes))
            return;

        uint8 const relentlessFlow = GetTalentRank(player, Talent::RelentlessFlow);
        RogueData& data = GetData(player);
        Unit* target = GetExplTargetUnit();
        if (++data.sinisterStrikeCount % (relentlessFlow ? 3 : 4) || !target || !target->IsAlive())
            return;

        player->CastSpell(target, SPELL_QUICK_CUT, TRIGGERED_FULL_MASK);
        if (relentlessFlow >= 2)
            EnergizeRogue(player, SPELL_QUICK_CUT, 10);
    }

    void Register() override
    {
        OnHit += SpellHitFn(CombatRogueSinisterStrikeScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueSinisterStrikeScript::HandleAfterCast);
    }
};

// 90010 Quick Cut: spends Opening for a heavy strike and 2 combo points
class CombatRogueQuickCutScript : public SpellScript
{
    PrepareSpellScript(CombatRogueQuickCutScript);

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        int32 const damage = int32(GetHitDamage() * 1.75f * GetExecutionerMultiplier(player, target));
        SetHitDamage(damage);

        if (HasEvolution(player, EVOLUTION_QUICK_CUT_BOTH_WEAPONS) && player->haveOffhandWeapon())
            CastBladeEcho(player, target, uint32(std::max(damage, 0)) / 2, 200ms);
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        player->AddComboPoints(GetExplTargetUnit(), 1);

        // Flowing Strikes performs a free Quick Cut that neither needs nor consumes Opening
        if (GetSpell()->IsTriggered())
            return;

        uint8 const twinCuts = GetTalentRank(player, Talent::TwinCuts);
        if (!twinCuts || !roll_chance_i(4 * twinCuts))
            ConsumeOpening(player);
    }

    void Register() override
    {
        OnHit += SpellHitFn(CombatRogueQuickCutScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueQuickCutScript::HandleAfterCast);
    }
};

// 90011 Shadow Lunge: gap closer, resets on kill
class CombatRogueShadowLungeScript : public SpellScript
{
    PrepareSpellScript(CombatRogueShadowLungeScript);

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (player && target)
            SetHitDamage(int32(GetHitDamage() * GetExecutionerMultiplier(player, target)));
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        player->AddComboPoints(GetExplTargetUnit(), 1);
        if (HasEvolution(player, EVOLUTION_SHADOW_LUNGE_OPENING))
            GrantOpening(player);

        RogueData& data = GetData(player);
        if (data.shadowLungeWasReset && HasEvolution(player, EVOLUTION_LUNGE_ENERGY_AND_VEIL))
            EnergizeRogue(player, SPELL_SHADOW_LUNGE, 10);
        data.shadowLungeWasReset = false;

        // The cooldown is already started when after-cast hooks run
        if (uint8 const fleetFootwork = GetTalentRank(player, Talent::FleetFootwork))
            player->ModifySpellCooldown(SPELL_SHADOW_LUNGE, -2000 * int32(fleetFootwork));

        Aura const* momentum = player->GetAura(SPELL_KILLING_MOMENTUM);
        if (momentum && momentum->GetStackAmount() >= 5)
            player->RemoveSpellCooldown(SPELL_SHADOW_LUNGE, true);
    }

    void Register() override
    {
        OnHit += SpellHitFn(CombatRogueShadowLungeScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueShadowLungeScript::HandleAfterCast);
    }
};

// 90012 Riposte: counterattack, always critical after a dodge or parry
class CombatRogueRiposteScript : public SpellScript
{
    PrepareSpellScript(CombatRogueRiposteScript);

    bool _empowered = false;
    int32 _hitDamage = 0;

    void HandleBeforeCast()
    {
        if (Player* player = GetRogue(GetCaster()))
            _empowered = DefendedRecently(player);
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float multiplier = 1.30f * GetExecutionerMultiplier(player, target);
        if (_empowered)
            multiplier *= 2.0f;
        _hitDamage = int32(GetHitDamage() * multiplier);
        SetHitDamage(_hitDamage);
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        Unit* target = GetExplTargetUnit();
        if (!HasEvolution(player, EVOLUTION_RIPOSTE_DODGE))
            player->RemoveAurasDueToSpell(SPELL_RIPOSTE);

        if (_empowered)
        {
            player->AddComboPoints(target, 1);
            if (GetTalentRank(player, Talent::CounterRhythm))
                GrantOpening(player);
        }

        uint8 const echoes = GetTalentRank(player, Talent::RiposteEcho);
        if (!echoes || !target || _hitDamage <= 0)
            return;

        uint8 hits = 0;
        for (Unit* enemy : GetEnemiesInRange(player, 5.0f))
        {
            if (hits >= echoes)
                break;
            if (enemy == target || !player->HasInArc(float(M_PI), enemy))
                continue;

            CastBladeEcho(player, enemy, uint32(_hitDamage));
            ++hits;
        }
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueRiposteScript::HandleBeforeCast);
        OnHit += SpellHitFn(CombatRogueRiposteScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueRiposteScript::HandleAfterCast);
    }
};

// 90100 Crescent Slash: spammable pure-damage AoE that pays for itself on packs
class CombatRogueCrescentSlashScript : public SpellScript
{
    PrepareSpellScript(CombatRogueCrescentSlashScript);

    uint8 _targetsHit = 0;

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (Player* player = GetRogue(GetCaster()))
            FilterByRadius(player, targets, GetAoeRadius(player, false));
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float multiplier = (HasEvolution(player, EVOLUTION_CRESCENT_SLASH_DAMAGE) ? 125.0f / 110.0f : 1.0f) *
            (1.0f + 0.03f * GetTalentRank(player, Talent::CrimsonDiscipline)) *
            GetExecutionerMultiplier(player, target);

        bool const bleeding = target->HasAura(SPELL_CRIMSON_WOUNDS, player->GetGUID());
        uint8 const rendingArc = GetTalentRank(player, Talent::RendingArc);
        if (rendingArc && bleeding)
        {
            multiplier *= 1.0f + 0.04f * rendingArc;
            if (rendingArc >= 5)
                RefreshCrimsonWounds(player, target, 2000);
        }

        SetHitDamage(int32(GetHitDamage() * multiplier));

        uint8 const hemorrhaging = GetTalentRank(player, Talent::HemorrhagingBlades);
        if (hemorrhaging && !bleeding && roll_chance_i(4 * hemorrhaging))
            ApplyCrimsonWounds(player, target, 6000);

        ++_targetsHit;
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        EnergizeRogue(player, SPELL_CRESCENT_SLASH, std::min<uint32>(8, 2 * _targetsHit));
        uint8 const comboPoints =
            HasEvolution(player, EVOLUTION_CRESCENT_SLASH_COMBO) && _targetsHit >= 3 ? 2 : 1;
        player->AddComboPoints(nullptr, comboPoints);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            CombatRogueCrescentSlashScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(CombatRogueCrescentSlashScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueCrescentSlashScript::HandleAfterCast);
    }
};

// 90017 Crimson Sweep: lower-damage AoE that bleeds the whole pack
class CombatRogueCrimsonSweepScript : public SpellScript
{
    PrepareSpellScript(CombatRogueCrimsonSweepScript);

    bool _empoweredByOpening = false;

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (Player* player = GetRogue(GetCaster()))
            FilterByRadius(player, targets, GetAoeRadius(player, false));
    }

    void HandleBeforeCast()
    {
        if (Player* player = GetRogue(GetCaster()))
            _empoweredByOpening = ConsumeOpening(player);
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float const multiplier = (1.0f + 0.03f * GetTalentRank(player, Talent::CrimsonDiscipline)) *
            GetExecutionerMultiplier(player, target);
        SetHitDamage(int32(GetHitDamage() * multiplier));
        ApplyCrimsonWounds(player, target, _empoweredByOpening ? 10000 : 6000);
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        player->AddComboPoints(nullptr, _empoweredByOpening ? 2 : 1);
        if (HasEvolution(player, EVOLUTION_CRIMSON_SWEEP_COOLDOWN))
            player->ModifySpellCooldown(SPELL_CRIMSON_SWEEP, -2000);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueCrimsonSweepScript::HandleBeforeCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            CombatRogueCrimsonSweepScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(CombatRogueCrimsonSweepScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueCrimsonSweepScript::HandleAfterCast);
    }
};

// -1766 Kick: Opportunist grants Opening and Energy on a successful interrupt
class CombatRogueKickScript : public SpellScript
{
    PrepareSpellScript(CombatRogueKickScript);

    bool _interrupting = false;

    void HandleBeforeHit(SpellMissInfo missInfo)
    {
        Unit* target = GetHitUnit();
        _interrupting = missInfo == SPELL_MISS_NONE && target && target->IsNonMeleeSpellCast(false);
    }

    void HandleAfterHit()
    {
        Player* player = GetRogue(GetCaster());
        if (!player || !_interrupting)
            return;

        if (uint8 const opportunist = GetTalentRank(player, Talent::Opportunist))
        {
            GrantOpening(player);
            EnergizeRogue(player, GetSpellInfo()->Id, 10 * opportunist);
        }
    }

    void Register() override
    {
        BeforeHit += BeforeSpellHitFn(CombatRogueKickScript::HandleBeforeHit);
        AfterHit += SpellHitFn(CombatRogueKickScript::HandleAfterHit);
    }
};

// ---------------------------------------------------------------------------------------------------------------
// Finishers: all burn up to 30 extra Energy for up to +50% damage or duration
// ---------------------------------------------------------------------------------------------------------------

class CombatRogueFinisherScript : public SpellScript
{
protected:
    uint8 _comboPoints = 0;
    uint32 _overflowEnergy = 0;

    void PrepareFinisher()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        _comboPoints = player->GetComboPoints();
        _overflowEnergy = TakeOverflowEnergy(player, GetSpellInfo()->ManaCost);
    }

    float GetFinisherMultiplier() const
    {
        return GetOverflowMultiplier(_overflowEnergy);
    }
};

// -2098 Eviscerate: damage, Battle Tempo, Tempo Echo
class CombatRogueEviscerateScript : public CombatRogueFinisherScript
{
    PrepareSpellScript(CombatRogueEviscerateScript);

    void HandleBeforeCast()
    {
        PrepareFinisher();
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float const multiplier = (1.10f + 0.05f * _comboPoints) * GetFinisherMultiplier() *
            GetExecutionerMultiplier(player, target);
        int32 const damage = int32(GetHitDamage() * multiplier);
        SetHitDamage(damage);
        GetData(player).lastEviscerateDamage = uint32(std::max(damage, 0));

        if (uint8 const tempoEcho = GetTalentRank(player, Talent::TempoEcho))
            if (roll_chance_i(10 * tempoEcho))
                CastBladeEcho(player, target, uint32(std::max(damage, 0)) * 2 / 5, 500ms);
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        ApplyBattleTempo(player, _comboPoints);
        if (_comboPoints >= 5 && HasEvolution(player, EVOLUTION_EVISCERATE_OPENING))
            GrantOpening(player);
        OnFinisherCast(player, _comboPoints, GetExplTargetUnit());
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueEviscerateScript::HandleBeforeCast);
        OnHit += SpellHitFn(CombatRogueEviscerateScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueEviscerateScript::HandleAfterCast);
    }
};

// -5171 Slice and Dice: Energy overflow extends the duration
class CombatRogueSliceAndDiceScript : public CombatRogueFinisherScript
{
    PrepareSpellScript(CombatRogueSliceAndDiceScript);

    void HandleBeforeCast()
    {
        PrepareFinisher();
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        if (Aura* aura = player->GetAura(GetSpellInfo()->Id); aura && _overflowEnergy)
        {
            int32 const duration = int32(aura->GetDuration() * GetFinisherMultiplier());
            aura->SetMaxDuration(duration);
            aura->SetDuration(duration);
        }
        OnFinisherCast(player, _comboPoints, GetExplTargetUnit());
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueSliceAndDiceScript::HandleBeforeCast);
        AfterCast += SpellCastFn(CombatRogueSliceAndDiceScript::HandleAfterCast);
    }
};

// 90013 Sanguine Veil: maintained lifesteal buff, 6 sec per combo point
class CombatRogueSanguineVeilScript : public CombatRogueFinisherScript
{
    PrepareSpellScript(CombatRogueSanguineVeilScript);

    void HandleBeforeCast()
    {
        PrepareFinisher();
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        if (Aura* aura = player->GetAura(SPELL_SANGUINE_VEIL))
        {
            int32 const duration = int32(6000.0f * std::max<uint8>(_comboPoints, 1) * GetFinisherMultiplier());
            aura->SetMaxDuration(duration);
            aura->SetDuration(duration);
        }
        OnFinisherCast(player, _comboPoints, nullptr);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueSanguineVeilScript::HandleBeforeCast);
        AfterCast += SpellCastFn(CombatRogueSanguineVeilScript::HandleAfterCast);
    }
};

// Sanguine Resolve: damage taken reduction while the veil is up
class CombatRogueSanguineVeilAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueSanguineVeilAuraScript);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Player* player = GetRogue(GetCaster());
        amount = player ? -3 * int32(GetTalentRank(player, Talent::SanguineResolve)) : 0;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CombatRogueSanguineVeilAuraScript::CalculateAmount, EFFECT_1,
            SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN);
    }
};

// 90101 Blood Waltz: AoE finisher, also released for free by Crimson Cadence
class CombatRogueBloodWaltzScript : public CombatRogueFinisherScript
{
    PrepareSpellScript(CombatRogueBloodWaltzScript);

    bool _cadenceEcho = false;
    uint8 _targetsHit = 0;

    void HandleBeforeCast()
    {
        _cadenceEcho = GetSpell()->IsTriggered();
        if (_cadenceEcho)
            _comboPoints = 5;
        else
            PrepareFinisher();
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        if (Player* player = GetRogue(GetCaster()))
            FilterByRadius(player, targets, GetAoeRadius(player, true));
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float const multiplier = float(std::max<uint8>(_comboPoints, 1)) * GetFinisherMultiplier() *
            GetExecutionerMultiplier(player, target);
        SetHitDamage(int32(GetHitDamage() * multiplier));
        RefreshCrimsonWounds(player, target);
        ExtendBattleTempo(player, 1000);
        ++_targetsHit;
    }

    void HandleAfterCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player || _cadenceEcho)
            return;

        if (_comboPoints >= 5 && HasEvolution(player, EVOLUTION_KEENER_OPENINGS))
            GrantOpening(player);
        OnFinisherCast(player, _comboPoints, nullptr);

        if (GetTalentRank(player, Talent::WaltzOfBlades) && _targetsHit)
            player->AddComboPoints(nullptr, std::min<uint8>(_targetsHit, 3));
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueBloodWaltzScript::HandleBeforeCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            CombatRogueBloodWaltzScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(CombatRogueBloodWaltzScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueBloodWaltzScript::HandleAfterCast);
    }
};

// ---------------------------------------------------------------------------------------------------------------
// State auras
// ---------------------------------------------------------------------------------------------------------------

// 90014 Opening: lights up Quick Cut through a caster aura state
class CombatRogueOpeningAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueOpeningAuraScript);

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->ModifyAuraState(AuraStateType(OPENING_AURA_STATE), true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->ModifyAuraState(AuraStateType(OPENING_AURA_STATE), false);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(CombatRogueOpeningAuraScript::HandleApply, EFFECT_0, SPELL_AURA_DUMMY,
            AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(CombatRogueOpeningAuraScript::HandleRemove, EFFECT_0,
            SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 90015 Battle Tempo: attack speed per stack, Tempo Mastery adds to every stack
class CombatRogueBattleTempoAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueBattleTempoAuraScript);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Player* player = GetRogue(GetCaster()))
            amount += GetTalentRank(player, Talent::TempoMastery);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CombatRogueBattleTempoAuraScript::CalculateAmount, EFFECT_0,
            SPELL_AURA_MOD_MELEE_HASTE);
    }
};

// 90016 Killing Momentum: movement speed and Energy regeneration per stack, Relentless Pursuit adds to both
class CombatRogueKillingMomentumAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueKillingMomentumAuraScript);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Player* player = GetRogue(GetCaster()))
            amount += 2 * GetTalentRank(player, Talent::RelentlessPursuit);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CombatRogueKillingMomentumAuraScript::CalculateAmount, EFFECT_0,
            SPELL_AURA_MOD_INCREASE_SPEED);
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CombatRogueKillingMomentumAuraScript::CalculateAmount, EFFECT_1,
            SPELL_AURA_MOD_POWER_REGEN_PERCENT);
    }
};

// 90018 Crimson Wounds: each tick restores Energy and feeds Crimson Frenzy
class CombatRogueCrimsonWoundsAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueCrimsonWoundsAuraScript);

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        Player* player = GetRogue(GetCaster());
        if (player && GetTarget()->IsAlive())
            OnCrimsonWoundsTick(player);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(CombatRogueCrimsonWoundsAuraScript::HandlePeriodic, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 90103 Crimson Frenzy: 1% or 2% attack speed per stack depending on the talent rank
class CombatRogueCrimsonFrenzyAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueCrimsonFrenzyAuraScript);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Player* player = GetRogue(GetCaster()))
            amount = std::max<int32>(1, GetTalentRank(player, Talent::CrimsonFrenzy));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(CombatRogueCrimsonFrenzyAuraScript::CalculateAmount, EFFECT_0,
            SPELL_AURA_MOD_MELEE_HASTE);
    }
};

// 90104 Crimson Duelist (hidden passive): dodges and parries (Riposte, Parry Instinct, Counter Rhythm) and
// Sinister Strike critical strikes (guaranteed Opening)
class CombatRogueCrimsonDuelistAuraScript : public AuraScript
{
    PrepareAuraScript(CombatRogueCrimsonDuelistAuraScript);

    static bool IsDefense(ProcEventInfo const& eventInfo)
    {
        uint32 const takenMelee = PROC_FLAG_TAKEN_MELEE_AUTO_ATTACK | PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS;
        return (eventInfo.GetTypeMask() & takenMelee) && (eventInfo.GetHitMask() & (PROC_HIT_DODGE | PROC_HIT_PARRY));
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!GetRogue(GetTarget()))
            return false;

        if (IsDefense(eventInfo))
            return true;

        SpellInfo const* spellInfo = eventInfo.GetSpellInfo();
        return (eventInfo.GetTypeMask() & PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS) &&
            (eventInfo.GetHitMask() & PROC_HIT_CRITICAL) && spellInfo &&
            spellInfo->GetFirstRankSpell()->Id == SPELL_SINISTER_STRIKE;
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        Player* player = GetRogue(GetTarget());
        if (!player)
            return;

        if (IsDefense(eventInfo))
            OnRogueDefended(player);
        else
            GrantOpening(player);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(CombatRogueCrimsonDuelistAuraScript::CheckProc);
        OnProc += AuraProcFn(CombatRogueCrimsonDuelistAuraScript::HandleProc);
    }
};

// Sanguine Veil lifesteal: heals for a share of effective damage dealt by any source
class CombatRogueUnitScript : public UnitScript
{
public:
    CombatRogueUnitScript() : UnitScript("CombatRogueUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        Player* player = GetRogue(attacker);
        if (!player || !victim || victim == player || !damage || !player->IsAlive() || player->IsFullHealth() ||
            !player->HasAura(SPELL_SANGUINE_VEIL))
            return;

        uint32 const percent = (HasEvolution(player, EVOLUTION_LUNGE_ENERGY_AND_VEIL) ? 20 : 15) +
            2 * GetTalentRank(player, Talent::SanguineInstinct);
        uint32 const effectiveDamage = std::min(damage, victim->GetHealth());
        uint32 const healing = std::max<uint32>(1, uint64(effectiveDamage) * percent / 100);
        HealInfo healInfo(player, player, healing, sSpellMgr->GetSpellInfo(SPELL_SANGUINE_VEIL),
            SPELL_SCHOOL_MASK_SHADOW);
        player->HealBySpell(healInfo);
    }
};
}

void AddCombatRogueScripts()
{
    new CombatRogueUnitScript();
    AddCombatRogueDaggerfallScripts();
    RegisterSpellScript(CombatRogueSinisterStrikeScript);
    RegisterSpellScript(CombatRogueQuickCutScript);
    RegisterSpellScript(CombatRogueShadowLungeScript);
    RegisterSpellScript(CombatRogueRiposteScript);
    RegisterSpellScript(CombatRogueCrescentSlashScript);
    RegisterSpellScript(CombatRogueCrimsonSweepScript);
    RegisterSpellScript(CombatRogueKickScript);
    RegisterSpellScript(CombatRogueEviscerateScript);
    RegisterSpellScript(CombatRogueSliceAndDiceScript);
    RegisterSpellAndAuraScriptPair(CombatRogueSanguineVeilScript, CombatRogueSanguineVeilAuraScript);
    RegisterSpellScript(CombatRogueBloodWaltzScript);
    RegisterSpellScript(CombatRogueOpeningAuraScript);
    RegisterSpellScript(CombatRogueBattleTempoAuraScript);
    RegisterSpellScript(CombatRogueKillingMomentumAuraScript);
    RegisterSpellScript(CombatRogueCrimsonWoundsAuraScript);
    RegisterSpellScript(CombatRogueCrimsonFrenzyAuraScript);
    RegisterSpellScript(CombatRogueCrimsonDuelistAuraScript);
}
