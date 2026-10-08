/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "StatOverflow.h"
#include "Creature.h"
#include "Player.h"
#include "SpellInfo.h"
#include "StringFormat.h"
#include "Unit.h"
#include "World.h"

#include <limits>

namespace StatOverflow
{
// The rates (.agents/docs/systems/stat-overflow.md, "The rates"): a point past a cap is worth about 0.4 of a point
// below it. Critique: 0.4% of the critical bonus a point (below the cap, a point of crit adds 1% of it). Precision:
// 0.4% damage a point (below the cap, a point of hit lands 1% more hits). Robustesse: 0.3% less of all damage taken
// a point (below the cap, a point of avoidance stops 1% of the swings, about 0.65% of a tank's intake), at most 20%.
LiveTuning::Knob const CritRate("overflow.crit_rate", 0.4f);
LiveTuning::Knob const PrecisionRate("overflow.precision_rate", 0.4f);
LiveTuning::Knob const RobustnessRate("overflow.robustness_rate", 0.3f);
LiveTuning::Knob const RobustnessCap("overflow.robustness_cap", 20.0f);

namespace
{
template<typename T>
T Scale(T value, float pct)
{
    if (!(value > 0) || pct <= 0.0f)
        return value;
    double const scaled = double(value) * (1.0 + double(pct) / 100.0);
    return static_cast<T>(std::min<double>(scaled, double(std::numeric_limits<T>::max())));
}

float CriticalBonusPctFor(Unit const* attacker, float critChance)
{
    if (!attacker || critChance <= CritCap || !attacker->IsControlledByPlayer())
        return 0.0f;
    return CriticalBonusPct(critChance, CritRate);
}

// Expertise past what the target can still avoid of this attack: its dodge, or its parry where it can parry it
// (Unit::RollMeleeOutcomeAgainst, Unit::MeleeSpellHitResult). The target's own state of the moment (casting,
// stunned) does not count: the bonus is the character's, not the fight's.
float ExpertisePoints(Player const* player, Unit const* victim, SpellInfo const* spellInfo,
    WeaponAttackType attackType, int32 skillDiff)
{
    float const expertise = player->GetExpertiseDodgeOrParryReduction(attackType);
    if (expertise <= 0.0f)
        return 0.0f;

    bool const front = victim->HasInArc(float(M_PI), player) || victim->HasIgnoreHitDirectionAura();
    // Only a player cannot dodge from behind
    bool canDodge = front || !victim->IsPlayer();
    bool canParry = front;
    if (spellInfo)
    {
        if (spellInfo->HasAttribute(SPELL_ATTR0_NO_ACTIVE_DEFENSE))
            canDodge = canParry = false;
        if (spellInfo->HasAttribute(SPELL_ATTR7_NO_ATTACK_DODGE))
            canDodge = false;
        if (spellInfo->HasAttribute(SPELL_ATTR7_NO_ATTACK_PARRY))
            canParry = false;
    }
    if (Creature const* creature = victim->ToCreature())
    {
        if (creature->HasFlagsExtra(CREATURE_FLAG_EXTRA_NO_DODGE))
            canDodge = false;
        if (creature->HasFlagsExtra(CREATURE_FLAG_EXTRA_NO_PARRY))
            canParry = false;
    }

    // The table's 0.04% a point of weapon skill under the target's
    float const skillShift = 0.04f * float(skillDiff);
    float cap = 0.0f;
    if (canDodge)
    {
        float dodge = victim->GetUnitDodgeChance()
            + float(player->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_COMBAT_RESULT_CHANCE, VICTIMSTATE_DODGE));
        dodge *= player->GetTotalAuraMultiplier(SPELL_AURA_MOD_ENEMY_DODGE);
        cap = dodge - skillShift;
    }
    if (canParry)
        cap = std::max(cap, victim->GetUnitParryChance() - skillShift);
    return Excess(expertise, std::max(cap, 0.0f));
}

// A creature's miss chance against a player's attack: 5%, and per point of weapon skill under the creature's
// defence 0.1%, 0.4% past 10 points (Unit::MeleeSpellRawMissChance)
float CreatureMissChance(int32 weaponSkill, int32 defense)
{
    int32 const diff = defense - weaponSkill;
    return 5.0f + (diff > 10 ? 1.0f + float(diff - 10) * 0.4f : float(diff) * 0.1f);
}

// "181.4", "0.35", "100": two decimals at most, no trailing zeros, for the addon message
std::string Compact(float value)
{
    std::string text = Acore::StringFormat("{:.2f}", value);
    while (!text.empty() && text.back() == '0')
        text.pop_back();
    if (!text.empty() && text.back() == '.')
        text.pop_back();
    return text == "-0" ? "0" : text;
}
}

int32 ScaleCriticalBonus(Unit const* attacker, float critChance, int32 bonus)
{
    return Scale(bonus, CriticalBonusPctFor(attacker, critChance));
}

uint32 ScaleCriticalBonus(Unit const* attacker, float critChance, uint32 bonus)
{
    return Scale(bonus, CriticalBonusPctFor(attacker, critChance));
}

float PrecisionPoints(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
    WeaponAttackType attackType, SpellSchoolMask schoolMask)
{
    if (!attacker || !victim || attacker == victim)
        return 0.0f;
    Player const* player = attacker->ToPlayer();
    if (!player)
        return 0.0f;

    float points = 0.0f;
    uint32 const damageClass = spellInfo ? spellInfo->DmgClass : uint32(SPELL_DAMAGE_CLASS_MELEE);
    switch (damageClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
        {
            // As the roll does: a white swing with its own hand, an ability with the main hand or the ranged weapon
            WeaponAttackType type = attackType;
            if (spellInfo)
                type = damageClass == SPELL_DAMAGE_CLASS_RANGED ? RANGED_ATTACK : BASE_ATTACK;
            else if (type != OFF_ATTACK && type != RANGED_ATTACK)
                type = BASE_ATTACK;

            int32 weaponSkill;
            if (spellInfo && damageClass == SPELL_DAMAGE_CLASS_RANGED && !spellInfo->IsRangedWeaponSpell())
                weaponSkill = int32(player->GetLevel()) * 5;
            else
                weaponSkill = int32(player->GetWeaponSkillValue(type, victim));
            int32 const skillDiff = weaponSkill - int32(victim->GetMaxSkillValueForLevel(player));

            points += Excess(-player->MeleeSpellRawMissChance(victim, type, skillDiff, spellInfo ? spellInfo->Id : 0),
                0.0f);
            if (type != RANGED_ATTACK)
                points += ExpertisePoints(player, victim, spellInfo, type, skillDiff);
            break;
        }
        case SPELL_DAMAGE_CLASS_MAGIC:
            points += Excess(float(player->MagicSpellRawHitChance(victim, spellInfo)) / 100.0f, 100.0f);
            break;
        default:
            // No hit roll (Unit::SpellHitResult): nothing to be past
            return 0.0f;
    }

    if (Unit::IsDamageReducedByArmor(schoolMask, spellInfo))
        points += Excess(Unit::GetArmorPenetrationPct(player, spellInfo), ArmorPenetrationCap) * ArmorPenetrationWeight;
    return points;
}

int32 ApplyPrecision(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
    WeaponAttackType attackType, SpellSchoolMask schoolMask, int32 damage)
{
    if (damage <= 0 || !attacker || !attacker->IsPlayer())
        return damage;
    return Scale(damage, PrecisionBonusPct(PrecisionPoints(attacker, victim, spellInfo, attackType, schoolMask),
        PrecisionRate));
}

uint32 ApplyPrecision(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
    WeaponAttackType attackType, SpellSchoolMask schoolMask, uint32 damage)
{
    if (!damage || !attacker || !attacker->IsPlayer())
        return damage;
    return Scale(damage, PrecisionBonusPct(PrecisionPoints(attacker, victim, spellInfo, attackType, schoolMask),
        PrecisionRate));
}

Robustness ComputeRobustness(Player const* player)
{
    Robustness result;
    if (!player)
        return result;

    // The boss's weapon skill against the character's: its swings land 0.6% more past dodge, parry and block at 83
    // against 80 (Unit::RollMeleeOutcomeAgainst's skill bonus), and miss 0.3% less (Unit::MeleeSpellRawMissChance,
    // 0.02% a point above a player victim, 0.04% below)
    int32 const bossSkill = int32(ReferenceLevel) * 5;
    int32 const skillDiff = bossSkill - int32(player->GetMaxSkillValueForLevel());
    float const tableShift = 0.04f * float(skillDiff);

    float const miss = std::clamp(player->GetUnitMissChance(BASE_ATTACK)
        - float(skillDiff) * (skillDiff > 0 ? 0.02f : 0.04f), 0.0f, 60.0f);
    float const dodge = std::max(player->GetUnitDodgeChance() - tableShift, 0.0f);
    float const parry = std::max(player->GetUnitParryChance() - tableShift, 0.0f);
    float const block = std::max(player->GetUnitBlockChance() - tableShift, 0.0f);
    result.avoidance = miss + dodge + parry + block;
    result.avoidanceExcess = Excess(result.avoidance, AvoidanceCap);

    // The boss's crit chance: a creature's 5% and 0.04% a point of its skill above the defence, less resilience
    // (Unit::GetUnitCriticalChance). Below 0, the defence past crit immunity.
    float const resilience = player->GetMeleeCritChanceReduction();
    result.defense = float(player->GetDefenseSkillValue());
    result.defenseCap = float(bossSkill) + std::max(CreatureBaseCrit - resilience, 0.0f) / CritPerSkillPoint;
    float const critTaken = CreatureBaseCrit + float(bossSkill - int32(result.defense)) * CritPerSkillPoint
        - resilience;
    result.critTakenExcess = Excess(-critTaken, 0.0f);

    result.armorReduction = ArmorReductionPct(float(player->GetArmor()), ReferenceLevel);
    result.armorPoints = ArmorPoints(result.armorReduction);

    result.points = result.avoidanceExcess + result.critTakenExcess + result.armorPoints;
    result.reductionPct = RobustnessReductionPct(result.points, RobustnessRate, RobustnessCap);
    return result;
}

uint32 ApplyRobustness(Player const* victim, uint32 damage)
{
    if (!victim || !damage)
        return damage;
    float const reduction = ComputeRobustness(victim).reductionPct;
    if (reduction <= 0.0f)
        return damage;
    return uint32(double(damage) * (1.0 - double(reduction) / 100.0));
}

Summary BuildSummary(Player const* player)
{
    Summary summary;
    if (!player)
        return summary;

    int32 const bossSkill = int32(ReferenceLevel) * 5;
    int32 const maxSkill = int32(player->GetMaxSkillValueForLevel());
    float const critRate = CritRate;
    float const precisionRate = PrecisionRate;

    // Critique: the sheet's chances, less the boss's 0.04% a point of defence above the character's skill (the
    // weapon's own skill is already in the sheet's)
    float const levelCrit = float(maxSkill - bossSkill) * CritPerSkillPoint;
    summary.meleeCrit = player->GetFloatValue(PLAYER_CRIT_PERCENTAGE) + levelCrit;
    summary.rangedCrit = player->GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE) + levelCrit;
    for (uint32 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        summary.spellCrit = std::max(summary.spellCrit,
            player->GetFloatValue(static_cast<uint16>(PLAYER_SPELL_CRIT_PERCENTAGE1) + school));
    summary.meleeCritBonus = CriticalBonusPct(summary.meleeCrit, critRate);
    summary.rangedCritBonus = CriticalBonusPct(summary.rangedCrit, critRate);
    summary.spellCritBonus = CriticalBonusPct(summary.spellCrit, critRate);

    // Precision. Hit: an ability's miss chance (a white swing of two weapons misses 19% more), a spell's 100% less
    // its base chance to hit a creature three levels up (Unit::MagicSpellRawHitChance)
    int32 const mainSkill = int32(player->GetWeaponSkillValue(BASE_ATTACK));
    summary.meleeHitCap = CreatureMissChance(mainSkill, bossSkill);
    summary.meleeHitExcess = Excess(player->m_modMeleeHitChance, summary.meleeHitCap);
    summary.whiteHitCap = summary.meleeHitCap + (player->HasOffhandWeaponForAttack() ? 19.0f : 0.0f);
    summary.rangedHitCap = CreatureMissChance(int32(player->GetWeaponSkillValue(RANGED_ATTACK)), bossSkill);
    summary.rangedHitExcess = Excess(player->m_modRangedHitChance, summary.rangedHitCap);

    int32 const levelDiff = int32(ReferenceLevel) - int32(player->GetLevel());
    int32 const baseHit = levelDiff < 3 ? 96 - levelDiff
        : 94 - (levelDiff - 2) * int32(sWorld->getRate(RATE_MISS_CHANCE_MULTIPLIER_TARGET_CREATURE));
    summary.spellHitCap = float(100 - baseHit);
    int32 schoolHit = 0;
    for (uint32 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        schoolHit = std::max(schoolHit,
            player->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_INCREASES_SPELL_PCT_TO_HIT, 1 << school));
    summary.spellHitExcess = Excess(player->m_modSpellHitChance + float(schoolHit), summary.spellHitCap);

    // Expertise: past a world boss's dodge from behind, its parry in front
    float const skillShift = 0.04f * float(mainSkill - bossSkill);
    summary.expertise = player->GetExpertiseDodgeOrParryReduction(BASE_ATTACK);
    summary.dodgeCap = std::max(ReferenceBossDodge - skillShift, 0.0f);
    summary.parryCap = std::max(ReferenceBossParry - skillShift, summary.dodgeCap);
    float const expertiseBehind = Excess(summary.expertise, summary.dodgeCap);
    float const expertiseFront = Excess(summary.expertise, summary.parryCap);

    summary.armorPenetration = Unit::GetArmorPenetrationPct(player, nullptr);
    summary.armorPenetrationExcess = Excess(summary.armorPenetration, ArmorPenetrationCap);
    float const armorPoints = summary.armorPenetrationExcess * ArmorPenetrationWeight;

    summary.meleePrecision = PrecisionBonusPct(summary.meleeHitExcess + expertiseBehind + armorPoints, precisionRate);
    summary.meleePrecisionFront = PrecisionBonusPct(summary.meleeHitExcess + expertiseFront + armorPoints,
        precisionRate);
    summary.rangedPrecision = PrecisionBonusPct(summary.rangedHitExcess + armorPoints, precisionRate);
    summary.spellPrecision = PrecisionBonusPct(summary.spellHitExcess, precisionRate);

    summary.robustness = ComputeRobustness(player);
    return summary;
}

std::string FormatCritPrecision(Summary const& summary)
{
    float const values[] = {
        CritRate, PrecisionRate,
        summary.meleeCrit, summary.rangedCrit, summary.spellCrit,
        summary.meleeCritBonus, summary.rangedCritBonus, summary.spellCritBonus,
        summary.meleeHitCap, summary.meleeHitExcess, summary.whiteHitCap,
        summary.rangedHitCap, summary.rangedHitExcess,
        summary.spellHitCap, summary.spellHitExcess,
        summary.expertise, summary.dodgeCap, summary.parryCap,
        summary.armorPenetration, summary.armorPenetrationExcess,
        summary.meleePrecision, summary.meleePrecisionFront, summary.rangedPrecision, summary.spellPrecision,
    };
    std::string text = "C";
    for (float const value : values)
        text += "\t" + Compact(value);
    return text;
}

std::string FormatRobustness(Summary const& summary)
{
    Robustness const& robustness = summary.robustness;
    float const values[] = {
        RobustnessRate, RobustnessCap,
        robustness.avoidance, robustness.avoidanceExcess,
        robustness.defense, robustness.defenseCap, robustness.critTakenExcess,
        robustness.armorReduction, robustness.armorPoints,
        robustness.points, robustness.reductionPct,
    };
    std::string text = "R";
    for (float const value : values)
        text += "\t" + Compact(value);
    return text;
}
}
