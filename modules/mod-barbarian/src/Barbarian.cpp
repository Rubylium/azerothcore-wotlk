#include "AllSpellScript.h"
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
#include "SpellScript.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>

// The Barbarian (class 14): what its spell data (localTools/barbarian/Spells.ps1) cannot carry. Its abilities taught by
// level, its proficiencies, Carnage (a bleed of up to 10 stacks most strikes add to), the Brutalité enrage on auto
// attacks, and the talents its trees mark as dummies (localTools/barbarian/talentTree.json): a talent is its rank
// spell's aura on the Barbarian, learned by mod-custom-classes' TalentTree.cpp.
//
// Enrage is the dispel type of the enrage auras (Spell.dbc field 2 = 9): the core then raises AURA_STATE_ENRAGE, which
// Fracas, Déchaînement and Outrage ask for (field 20), so the client greys them too.
namespace
{
constexpr uint8 CLASS_BARBARIAN = 14;
constexpr uint32 SPELL_FAMILY_BARBARIAN = 20;

enum Spells : uint32
{
    SPELL_BARBARIC_STRIKE = 97100,
    SPELL_BARBARIC_WHIRL = 97102,
    SPELL_UNBRIDLED_RAGE = 97103,
    SPELL_WRIST_SNAP = 97104,
    SPELL_IMPALING_RUSH = 97105,
    SPELL_WHIRLING_ADVANCE = 97106,
    SPELL_WHIRLING_ADVANCE_HIT = 97113,
    SPELL_CARNAGE = 97114,
    SPELL_BLOODTHIRSTY_RAGE = 97115,
    SPELL_BORN_IN_BLOOD_HEAL = 97116,
    SPELL_ANGER_MANAGEMENT_AURA = 97117,
    SPELL_SMASH = 97120,
    SPELL_RAMPAGE = 97121,
    SPELL_BRUTAL_SWING = 97122,
    SPELL_DECAPITATE = 97124,
    SPELL_STORM_OF_STEEL = 97127,
    SPELL_IMPALE = 97129,
    SPELL_STORM_OF_STEEL_HIT = 97130,
    SPELL_SPEC_BRUTALITY = 97290,
    // Talent ranks the C++ reads (localTools/barbarian/talentTree.json)
    TALENT_BORN_IN_BLOOD_1 = 97314,
    TALENT_BORN_IN_BLOOD_2 = 97315,
    TALENT_ANGER_MANAGEMENT = 97338,
    TALENT_BRUTAL_CARNAGE = 97361,
    TALENT_UNBRIDLED_WRATH_1 = 97362,
    TALENT_THIRST_FOR_BLOOD = 97377,
    TALENT_HONED_CLEAVER = 97390,
    TALENT_UNCONTROLLED_RAGE = 97398,
    TALENT_PROTECTIVE_RAGE_1 = 97401,
    TALENT_PROTECTIVE_RAGE_2 = 97402,
    TALENT_BLOODLETTING_1 = 97407,
    TALENT_BLOODLETTING_2 = 97408,
};

// The abilities every Barbarian holds, by level (the others come from its talent trees and specialization)
struct AbilityUnlock
{
    uint32 spellId;
    uint8 level;
};

constexpr std::array<AbilityUnlock, 4> AbilityUnlocks = { {
    { SPELL_BARBARIC_STRIKE, 1 }, { SPELL_BARBARIC_WHIRL, 3 }, { SPELL_UNBRIDLED_RAGE, 5 }, { SPELL_WRIST_SNAP, 7 }
} };

// The proficiencies (PlayerStart.CustomSpells is off, so playercreateinfo_spell_custom teaches none of them): cloth,
// leather, mail, the axes, maces, polearms, swords, thrown, shields, dual wield, parry and block
constexpr std::array<uint32, 17> ProficiencySpells = { 9078, 9077, 8737, 196, 197, 198, 199, 200, 201, 202, 2567, 9116,
    674, 3127, 107, 204, 81 };
// Their skills, at full value for the weapons
constexpr std::array<uint32, 8> WeaponSkills = { SKILL_AXES, SKILL_2H_AXES, SKILL_MACES, SKILL_2H_MACES, SKILL_POLEARMS,
    SKILL_SWORDS, SKILL_2H_SWORDS, SKILL_THROWN };
constexpr std::array<uint32, 2> ArmorSkills = { SKILL_MAIL, SKILL_SHIELD };

// Balance, live with .tune (LiveTuning.h)
LiveTuning::Knob const CarnageTickAp("barbarian.carnage_tick_ap", 0.012f);          // per stack, every 3 s
LiveTuning::KnobUInt const CarnageMaxStacks("barbarian.carnage_max_stacks", 10);
LiveTuning::KnobInt const BloodthirstyRageChance("barbarian.bloodthirsty_rage_chance", 15);
LiveTuning::KnobUInt const DecapitateMaxExtraEnergy("barbarian.decapitate_extra_energy", 50);
LiveTuning::Knob const DecapitatePerEnergy("barbarian.decapitate_per_energy", 0.02f);
LiveTuning::Knob const ImpaleLowHealthFactor("barbarian.impale_low_health_factor", 1.625f);
// Intercept's rush runs at SPEED_CHARGE (42 yd/s); the blow lands this long after the Barbarian arrives
LiveTuning::KnobUInt const WhirlingAdvanceLandMs("barbarian.whirling_advance_land_ms", 100);

constexpr char const* StateKey = "BarbarianState";

struct BarbarianState : public DataMap::Base
{
    // The energy a Décapitation burns on top of its cost: measured as it hits (the hit comes before the cast hook,
    // after the cost was paid), then spent by the cast hook
    uint32 decapitateExtra = 0;
};

Player* Barbarian(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_BARBARIAN ? player : nullptr;
}

BarbarianState* GetState(Player* player)
{
    return player->CustomData.GetDefault<BarbarianState>(StateKey);
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

// Courroux débridé's five ranks: 0-5
uint8 RankOfFive(Unit const* unit, uint32 first)
{
    for (uint8 rank = 5; rank; --rank)
        if (unit->HasAura(first + rank - 1))
            return rank;
    return 0;
}

bool IsEnraged(Unit const* unit)
{
    return unit->HasAuraState(AURA_STATE_ENRAGE);
}

// Carnage on the target: `stacks` more, up to the cap, its duration refreshed
void AddCarnage(Player* player, Unit* target, uint8 stacks)
{
    if (!target || !stacks || !target->IsAlive())
        return;
    Aura* aura = target->GetAura(SPELL_CARNAGE, player->GetGUID());
    if (!aura)
    {
        aura = player->AddAura(SPELL_CARNAGE, target);
        if (!aura)
            return;
        --stacks;
    }
    uint8 const total = uint8(std::min<uint32>(CarnageMaxStacks, aura->GetStackAmount() + stacks));
    if (total != aura->GetStackAmount())
        aura->SetStackAmount(total);
    aura->RefreshDuration();
}

void RestoreTraining(Player* player)
{
    for (uint32 spellId : ProficiencySpells)
        if (!player->HasSpell(spellId))
            player->learnSpell(spellId, false);
    for (uint32 skill : WeaponSkills)
        if (!player->GetSkillValue(skill))
            player->SetSkill(skill, 0, 1, player->GetMaxSkillValueForLevel());
    for (uint32 skill : ArmorSkills)
        if (!player->GetSkillValue(skill))
            player->SetSkill(skill, 0, 1, 1);
}

void LearnUnlockedAbilities(Player* player)
{
    for (AbilityUnlock const& unlock : AbilityUnlocks)
    {
        if (player->GetLevel() >= unlock.level)
        {
            if (!player->HasSpell(unlock.spellId))
                player->learnSpell(unlock.spellId, false);
        }
        else if (player->HasSpell(unlock.spellId))
            player->removeSpell(unlock.spellId, SPEC_MASK_ALL, false);
    }
}

// Avancée tourbillonnante: the blow around the Barbarian as it lands
void ScheduleAdvanceLanding(Player* player, Unit* target)
{
    ObjectGuid const guid = player->GetGUID();
    uint32 const travelMs = target ? uint32(player->GetExactDist(target) / 42.0f * 1000.0f) : 0;
    player->m_Events.AddEventAtOffset([guid]()
    {
        if (Player* barbarian = ObjectAccessor::FindPlayer(guid))
            if (barbarian->IsAlive())
                barbarian->CastSpell(barbarian, SPELL_WHIRLING_ADVANCE_HIT, true);
    }, Milliseconds(travelMs + WhirlingAdvanceLandMs));
}

class BarbarianSpellScript : public AllSpellScript
{
public:
    BarbarianSpellScript() : AllSpellScript("BarbarianSpellScript", { ALLSPELLHOOK_ON_CAST }) { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Barbarian(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        switch (spellInfo->Id)
        {
            case SPELL_UNBRIDLED_RAGE:
                // Maîtrise de la colère
                if (player->HasAura(TALENT_ANGER_MANAGEMENT))
                    player->CastSpell(player, SPELL_ANGER_MANAGEMENT_AURA, true);
                break;
            case SPELL_WHIRLING_ADVANCE:
                ScheduleAdvanceLanding(player, spell->m_targets.GetUnitTarget());
                break;
            case SPELL_SMASH:
                // Rage incontrôlée: a Fracas now and then leaves no cooldown behind
                if (player->HasAura(TALENT_UNCONTROLLED_RAGE) && roll_chance_i(25))
                    player->m_Events.AddEventAtOffset([guid = player->GetGUID()]()
                    {
                        if (Player* barbarian = ObjectAccessor::FindPlayer(guid))
                            barbarian->RemoveSpellCooldown(SPELL_SMASH, true);
                    }, Milliseconds(1));
                break;
            case SPELL_DECAPITATE:
            {
                // The energy the blow burned (none when it missed)
                BarbarianState* state = GetState(player);
                player->ModifyPower(POWER_ENERGY, -int32(state->decapitateExtra));
                state->decapitateExtra = 0;
                break;
            }
            default:
                break;
        }
    }
};

class BarbarianUnitScript : public UnitScript
{
public:
    BarbarianUnitScript() : UnitScript("BarbarianUnitScript", true, {
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Auto attacks: Brutalité's enrage, and Ruée empalante's impaling blow
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Player* player = Barbarian(attacker);
        if (!player || !target || !damage)
            return;
        ConsumeImpalingRush(player, target);
        if (player->HasAura(SPELL_SPEC_BRUTALITY) &&
            roll_chance_i(BloodthirstyRageChance + RankOfFive(player, TALENT_UNBRIDLED_WRATH_1)))
            player->CastSpell(player, SPELL_BLOODTHIRSTY_RAGE, true);
    }

    // Décapitation's burned energy; Empaler on a target below 35%
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Barbarian(attacker);
        if (!player || !target || !spellInfo || damage <= 0)
            return;
        if (spellInfo->Id == SPELL_DECAPITATE)
        {
            // Every point of energy left after its cost, up to the cap, burned into the blow
            uint32 const extra = std::min<uint32>(DecapitateMaxExtraEnergy, player->GetPower(POWER_ENERGY));
            GetState(player)->decapitateExtra = extra;
            damage = int32(float(damage) * (1.0f + DecapitatePerEnergy * float(extra)));
        }
        else if (spellInfo->Id == SPELL_IMPALE && target->GetHealthPct() < 35.0f)
            damage = int32(float(damage) * ImpaleLowHealthFactor);
    }

    // What a strike leaves on each enemy it hit: Carnage, and the heals that ride on it
    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = Barbarian(caster);
        if (!player || !victim || !spellInfo || !damage || victim == caster)
            return;
        switch (spellInfo->Id)
        {
            case SPELL_BARBARIC_STRIKE:
                AddCarnage(player, victim, 2);
                // Soif de sang
                if (player->HasAura(TALENT_THIRST_FOR_BLOOD))
                    player->ModifyHealth(int32(player->CountPctFromMaxHealth(2)));
                break;
            case SPELL_BARBARIC_WHIRL:
                // Fendoir affûté: two instead of one
                AddCarnage(player, victim, player->HasAura(TALENT_HONED_CLEAVER) ? 2 : 1);
                break;
            case SPELL_RAMPAGE:
                // Saignée: one or two more
                AddCarnage(player, victim, 3 + Rank(player, TALENT_BLOODLETTING_1, TALENT_BLOODLETTING_2));
                break;
            case SPELL_BRUTAL_SWING:
                // Carnage brutal
                if (player->HasAura(TALENT_BRUTAL_CARNAGE))
                    AddCarnage(player, victim, 2);
                break;
            case SPELL_IMPALE:
                if (victim->GetHealthPct() < 35.0f)
                    AddCarnage(player, victim, 5);
                break;
            default:
                break;
        }
        if (spellInfo->SpellFamilyName == SPELL_FAMILY_BARBARIAN && spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
            ConsumeImpalingRush(player, victim);
    }

    // The Barbarian hit (every unit script hears this one): Rage protectrice while enraged, and Né dans le sang as it
    // falls under 35%
    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType /*type*/) override
    {
        Player* player = Barbarian(victim);
        if (!player || !damage || attacker == victim || !player->IsAlive())
            return damage;
        if (uint8 const rank = Rank(player, TALENT_PROTECTIVE_RAGE_1, TALENT_PROTECTIVE_RAGE_2))
            if (IsEnraged(player))
                damage = uint32(CalculatePct(damage, 100 - 3 * rank));
        if (uint8 const rank = Rank(player, TALENT_BORN_IN_BLOOD_1, TALENT_BORN_IN_BLOOD_2))
        {
            uint64 const threshold = player->CountPctFromMaxHealth(35);
            if (player->GetHealth() >= threshold && damage < player->GetHealth() &&
                player->GetHealth() - damage < threshold && !player->HasSpellCooldown(TALENT_BORN_IN_BLOOD_1))
            {
                player->AddSpellCooldown(TALENT_BORN_IN_BLOOD_1, 0, 60000);
                // 10% (20%) of maximum health over the heal's 6 ticks
                int32 const perTick = int32(player->CountPctFromMaxHealth(10 * rank) / 6);
                player->CastCustomSpell(SPELL_BORN_IN_BLOOD_HEAL, SPELLVALUE_BASE_POINT0, perTick, player, true);
            }
        }
        return damage;
    }

private:
    // Ruée empalante: the next blow after it impales its target
    static void ConsumeImpalingRush(Player* player, Unit* target)
    {
        if (!player->HasAura(SPELL_IMPALING_RUSH))
            return;
        player->RemoveAurasDueToSpell(SPELL_IMPALING_RUSH);
        AddCarnage(player, target, 3);
    }
};

class BarbarianPlayerScript : public PlayerScript
{
public:
    BarbarianPlayerScript() : PlayerScript("BarbarianPlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!Barbarian(player))
            return;
        RestoreTraining(player);
        LearnUnlockedAbilities(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!Barbarian(player))
            return;
        RestoreTraining(player);
        LearnUnlockedAbilities(player);
    }
};

// Carnage's tick: a share of the Barbarian's attack power per stack (the core multiplies by the stacks)
class spell_barbarian_carnage : public AuraScript
{
    PrepareAuraScript(spell_barbarian_carnage);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;
        amount = std::max<int32>(1, int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * CarnageTickAp));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_barbarian_carnage::CalculateAmount, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// Tempête d'acier: a blow at the Barbarian's target every half second, when it is in reach
class spell_barbarian_storm_of_steel : public AuraScript
{
    PrepareAuraScript(spell_barbarian_storm_of_steel);

    void Strike(AuraEffect const* /*aurEff*/)
    {
        Player* player = Barbarian(GetTarget());
        if (!player)
            return;
        Unit* target = player->GetVictim();
        if (target && target->IsAlive() && player->IsWithinMeleeRange(target))
            player->CastSpell(target, SPELL_STORM_OF_STEEL_HIT, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_barbarian_storm_of_steel::Strike, EFFECT_0,
            SPELL_AURA_PERIODIC_DUMMY);
    }
};
}

void AddBarbarianScripts()
{
    new BarbarianSpellScript();
    new BarbarianUnitScript();
    new BarbarianPlayerScript();
    RegisterSpellScript(spell_barbarian_carnage);
    RegisterSpellScript(spell_barbarian_storm_of_steel);
}
