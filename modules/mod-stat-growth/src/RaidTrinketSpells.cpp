#include "Group.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>

// What the raid trinkets' spell data (localTools/raidTrinkets/Spells.ps1) cannot say: Valérian's four
// (RaidTrinkets.cpp, items 17859-17862). Bound to their spells by spell_script_names.
namespace
{
enum RaidTrinketSpells : uint32
{
    SPELL_LITANY_SPEAR      = 94551,    // Lance de la Litanie's spear, through its target
    SPELL_PSALM_VERSE       = 94553,    // Psaume du Zélote's verse, 2% faster casts a stack
    SPELL_PSALM_FULFILLED   = 94554,    // its 8th verse: the next spell instant
    SPELL_CHOIR_ECHO        = 94556,    // Sceau du Chœur's echo of a heal
    SPELL_CHAIN_BURST       = 94558,    // Chaîne brisée's burst
};

// The spear: 120% of the wearer's attack power (a hunter's ranged one if higher), to the target and the enemies up to
// 15 yd behind it within 2 yd of the line from the wearer through it
constexpr float SpearAttackPowerPct = 120.0f;
constexpr float SpearLength = 15.0f;
constexpr float SpearHalfWidth = 2.0f;
// The psalm's verses, the last making the next spell instant
constexpr uint8 PsalmVerses = 8;
// The choir's echo: 35% of a direct heal, on the most hurt member of the group within 40 yd
constexpr uint32 ChoirEchoPct = 35;
constexpr float ChoirRange = 40.0f;
// The chain's shield: 25% of the wearer's maximum health
constexpr int32 ChainShieldHealthPct = 25;

// Lance de la Litanie (94551): effect 0 hits the target, effect 1 the enemies around it, narrowed to the line behind
// it; each hit is the wearer's attack power with their damage done bonuses
class RaidTrinketLitanySpearScript : public SpellScript
{
    PrepareSpellScript(RaidTrinketLitanySpearScript);

    void FilterLine(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        Unit* target = GetExplTargetUnit();
        if (!caster || !target)
        {
            targets.clear();
            return;
        }
        // The line from the wearer through the target (the wearer's facing when they stand on it)
        float const angle = caster->GetExactDist2d(target) < 0.5f ? caster->GetOrientation() : caster->GetAngle(target);
        float const cosine = std::cos(angle);
        float const sine = std::sin(angle);
        targets.remove_if([target, cosine, sine](WorldObject* object)
        {
            if (object == target)
                return true;
            float const dx = object->GetPositionX() - target->GetPositionX();
            float const dy = object->GetPositionY() - target->GetPositionY();
            float const along = dx * cosine + dy * sine;
            float const across = std::fabs(dy * cosine - dx * sine);
            float const reach = object->GetCombatReach();
            return along < 0.0f || along > SpearLength + reach || across > SpearHalfWidth + reach;
        });
    }

    void HandleDamage(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;
        float const attackPower = std::max(caster->GetTotalAttackPowerValue(BASE_ATTACK),
            caster->GetTotalAttackPowerValue(RANGED_ATTACK));
        uint32 const damage = uint32(CalculatePct(attackPower, SpearAttackPowerPct));
        SetHitDamage(caster->SpellDamageBonusDone(target, GetSpellInfo(), damage, SPELL_DIRECT_DAMAGE, effIndex));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(RaidTrinketLitanySpearScript::FilterLine, EFFECT_1,
            TARGET_UNIT_DEST_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(RaidTrinketLitanySpearScript::HandleDamage, EFFECT_ALL,
            SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Psaume du Zélote (94552): each harmful spell cast a verse; the 8th makes the next spell instant, once a psalm. Its
// verses end with it.
class RaidTrinketZealotPsalmScript : public AuraScript
{
    PrepareAuraScript(RaidTrinketZealotPsalmScript);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PSALM_VERSE, SPELL_PSALM_FULFILLED });
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& /*eventInfo*/)
    {
        PreventDefaultAction();
        Unit* target = GetTarget();
        target->CastSpell(target, SPELL_PSALM_VERSE, true, nullptr, aurEff);
        Aura const* verses = target->GetAura(SPELL_PSALM_VERSE);
        if (_fulfilled || !verses || verses->GetStackAmount() < PsalmVerses)
            return;
        _fulfilled = true;
        target->CastSpell(target, SPELL_PSALM_FULFILLED, true, nullptr, aurEff);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->RemoveAurasDueToSpell(SPELL_PSALM_VERSE);
    }

    void Register() override
    {
        OnEffectProc += AuraEffectProcFn(RaidTrinketZealotPsalmScript::HandleProc, EFFECT_0,
            SPELL_AURA_PROC_TRIGGER_SPELL);
        AfterEffectRemove += AuraEffectRemoveFn(RaidTrinketZealotPsalmScript::HandleRemove, EFFECT_0,
            SPELL_AURA_PROC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
    }

    bool _fulfilled = false;
};

// The most hurt member of the healer's group within reach, neither the healer nor the one just healed
Unit* MostHurtAlly(Unit* healer, Unit* healed)
{
    Player* player = healer->ToPlayer();
    Group* group = player ? player->GetGroup() : nullptr;
    if (!group)
        return nullptr;
    Player* mostHurt = nullptr;
    for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
    {
        Player* member = reference->GetSource();
        if (!member || member == player || member == healed || !member->IsAlive() || member->IsFullHealth() ||
            !member->IsInMap(player) || !player->IsWithinDistInMap(member, ChoirRange))
            continue;
        if (!mostHurt || member->GetHealthPct() < mostHurt->GetHealthPct())
            mostHurt = member;
    }
    return mostHurt;
}

// Sceau du Chœur (94555): each direct heal echoed on the most hurt ally for 35% of it
class RaidTrinketChoirSealScript : public AuraScript
{
    PrepareAuraScript(RaidTrinketChoirSealScript);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CHOIR_ECHO });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        HealInfo const* heal = eventInfo.GetHealInfo();
        SpellInfo const* spell = eventInfo.GetSpellInfo();
        return heal && heal->GetHeal() && spell && spell->Id != SPELL_CHOIR_ECHO;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();
        Unit* healer = GetTarget();
        Unit* ally = MostHurtAlly(healer, eventInfo.GetActionTarget());
        if (!ally)
            return;
        int32 const amount = int32(CalculatePct(eventInfo.GetHealInfo()->GetHeal(), ChoirEchoPct));
        if (amount > 0)
            healer->CastCustomSpell(ally, SPELL_CHOIR_ECHO, &amount, nullptr, nullptr, true, nullptr, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(RaidTrinketChoirSealScript::CheckProc);
        OnEffectProc += AuraEffectProcFn(RaidTrinketChoirSealScript::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

// The choir's echo (94556): exactly the amount it is handed, none of the healer's bonuses a second time
class RaidTrinketChoirEchoScript : public SpellScript
{
    PrepareSpellScript(RaidTrinketChoirEchoScript);

    void HandleHeal(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        SetHitHeal(GetEffectValue());
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(RaidTrinketChoirEchoScript::HandleHeal, EFFECT_0, SPELL_EFFECT_HEAL);
    }
};

// Chaîne brisée (94557): a shield of 25% of the wearer's maximum health; when it is broken or runs out, the chain
// bursts on the enemies around them for what it absorbed
class RaidTrinketBrokenChainScript : public AuraScript
{
    PrepareAuraScript(RaidTrinketBrokenChainScript);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CHAIN_BURST });
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;
        if (Unit* owner = GetUnitOwner())
            amount = int32(owner->CountPctFromMaxHealth(ChainShieldHealthPct));
    }

    void HandleAbsorb(AuraEffect* /*aurEff*/, DamageInfo& /*dmgInfo*/, uint32& absorbAmount)
    {
        _absorbed += absorbAmount;
    }

    void HandleRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        AuraRemoveMode const removeMode = GetTargetApplication()->GetRemoveMode();
        if (!_absorbed || (removeMode != AURA_REMOVE_BY_EXPIRE && removeMode != AURA_REMOVE_BY_ENEMY_SPELL))
            return;
        Unit* target = GetTarget();
        int32 const damage = int32(std::min<uint64>(_absorbed, std::numeric_limits<int32>::max()));
        target->CastCustomSpell(target, SPELL_CHAIN_BURST, &damage, nullptr, nullptr, true, nullptr, aurEff);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(RaidTrinketBrokenChainScript::CalculateAmount, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB);
        AfterEffectAbsorb += AuraEffectAbsorbFn(RaidTrinketBrokenChainScript::HandleAbsorb, EFFECT_0);
        AfterEffectRemove += AuraEffectRemoveFn(RaidTrinketBrokenChainScript::HandleRemove, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
    }

    uint64 _absorbed = 0;
};
}

void AddRaidTrinketSpellScripts()
{
    RegisterSpellScript(RaidTrinketLitanySpearScript);
    RegisterSpellScript(RaidTrinketZealotPsalmScript);
    RegisterSpellScript(RaidTrinketChoirSealScript);
    RegisterSpellScript(RaidTrinketChoirEchoScript);
    RegisterSpellScript(RaidTrinketBrokenChainScript);
}
