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

#ifndef ACORE_STAT_OVERFLOW_H
#define ACORE_STAT_OVERFLOW_H

#include "Define.h"
#include "LiveTuning.h"
#include "SharedDefines.h"

#include <algorithm>
#include <string>

class Player;
class SpellInfo;
class Unit;
enum WeaponAttackType : uint8;

// Stat overflow: past a stock cap a stat converts into a second bonus instead of being lost. Paragon's flat primary
// stats put a character far past the caps (an agility role at 600 points holds ~280% melee crit, a healer ~165%
// spell crit, ratings and a tank's armour several times their caps). The caps stay; what lies past them is worth a
// share of what a point below them is. Documented in .agents/docs/systems/stat-overflow.md (formulas, the rates'
// maths, the character sheet).
//
//  - Critique: a crit chance past 100% (the chance the roll used, against that target) grows the critical bonus:
//    CritRate % of the bonus per 1% past the cap (x2 melee becomes x2.4 at 200%).
//  - Precision: hit past the cap against the target (melee or spell, as the attack rolls), expertise past the dodge
//    cap (the parry cap where the target can parry it) and armour penetration past 100% (weighted
//    ArmorPenetrationWeight) are points; each is PrecisionRate % more damage.
//  - Robustesse: against a level-83 boss, avoidance past a full combat table (miss + dodge + parry + block over 100%),
//    defence past crit immunity (its crit chance below 0) and armour past the 75% cap (the share of physical damage
//    the excess would still have stopped) are points; each is RobustnessRate % less damage taken, at most
//    RobustnessCap % (it multiplies with the paragon board's own reduction, capped at 25%: 40% at most together).
namespace StatOverflow
{
    // The rates and the cap, live (.tune set overflow.<name> <value>)
    extern LiveTuning::Knob const CritRate;
    extern LiveTuning::Knob const PrecisionRate;
    extern LiveTuning::Knob const RobustnessRate;
    extern LiveTuning::Knob const RobustnessCap;

    // The stock caps
    constexpr float CritCap = 100.0f;
    constexpr float AvoidanceCap = 100.0f;
    constexpr float ArmorReductionCap = 75.0f;              // Unit::CalcArmorReducedDamage
    constexpr float ArmorPenetrationCap = 100.0f;
    // A point of armour penetration below its cap is worth about half a point of hit against a boss's armour
    constexpr float ArmorPenetrationWeight = 0.5f;
    // A creature's crit chance per point of weapon skill above the defence (Unit::GetUnitCriticalChance)
    constexpr float CritPerSkillPoint = 0.04f;
    constexpr float CreatureBaseCrit = 5.0f;

    // The reference the character sheet and Robustesse are computed against: a level-83 boss (a world boss's dodge
    // and parry, Unit::GetUnitDodgeChance / GetUnitParryChance)
    constexpr uint8 ReferenceLevel = 83;
    constexpr float ReferenceBossDodge = 5.85f;
    constexpr float ReferenceBossParry = 13.4f;

    // --- The maths, pure -----------------------------------------------------------------------------------------
    [[nodiscard]] constexpr float Excess(float value, float cap)
    {
        return value > cap ? value - cap : 0.0f;
    }

    // Percent added to the critical bonus by a crit chance past 100%
    [[nodiscard]] constexpr float CriticalBonusPct(float critChance, float rate)
    {
        return Excess(critChance, CritCap) * std::max(rate, 0.0f);
    }

    // Percent more damage from precision points
    [[nodiscard]] constexpr float PrecisionBonusPct(float points, float rate)
    {
        return std::max(points, 0.0f) * std::max(rate, 0.0f);
    }

    // Percent less damage taken from robustness points
    [[nodiscard]] constexpr float RobustnessReductionPct(float points, float rate, float cap)
    {
        return std::clamp(std::max(points, 0.0f) * rate, 0.0f, std::max(cap, 0.0f));
    }

    // The armour reduction against an attacker of that level, in percent and not held to the 75% cap (the formula of
    // Unit::CalcArmorReducedDamage)
    [[nodiscard]] constexpr float ArmorReductionPct(float armor, uint8 attackerLevel)
    {
        float level = float(attackerLevel);
        if (level > 59.0f)
            level += 4.5f * (level - 59.0f);
        float const ratio = 0.1f * std::max(armor, 0.0f) / (8.5f * level + 40.0f);
        return 100.0f * ratio / (1.0f + ratio);
    }

    // Armour past the cap as robustness points: the share of the physical damage the cap lets through (25%) that the
    // armour past it would still have stopped, in percent. Three times the cap's armour: 60.
    [[nodiscard]] constexpr float ArmorPoints(float uncappedReductionPct)
    {
        if (uncappedReductionPct <= ArmorReductionCap)
            return 0.0f;
        float const cappedTaken = 100.0f - ArmorReductionCap;
        return 100.0f * (1.0f - (100.0f - std::min(uncappedReductionPct, 100.0f)) / cappedTaken);
    }

    // --- Per hit -------------------------------------------------------------------------------------------------
    // A critical bonus (what a crit adds over the normal hit) grown by the crit chance past 100%. Players' and their
    // pets', totems' and guardians' hits; anything else's is returned as it is.
    [[nodiscard]] int32 ScaleCriticalBonus(Unit const* attacker, float critChance, int32 bonus);
    [[nodiscard]] uint32 ScaleCriticalBonus(Unit const* attacker, float critChance, uint32 bonus);

    // A player's precision points on this hit against this target (spellInfo null: a white swing)
    [[nodiscard]] float PrecisionPoints(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
        WeaponAttackType attackType, SpellSchoolMask schoolMask);
    [[nodiscard]] int32 ApplyPrecision(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
        WeaponAttackType attackType, SpellSchoolMask schoolMask, int32 damage);
    [[nodiscard]] uint32 ApplyPrecision(Unit const* attacker, Unit const* victim, SpellInfo const* spellInfo,
        WeaponAttackType attackType, SpellSchoolMask schoolMask, uint32 damage);

    // --- Robustesse ----------------------------------------------------------------------------------------------
    struct Robustness
    {
        float avoidance = 0.0f;         // miss + dodge + parry + block against the reference boss
        float avoidanceExcess = 0.0f;
        float defense = 0.0f;           // defence skill
        float defenseCap = 0.0f;        // the defence its crit chance reaches 0 at
        float critTakenExcess = 0.0f;   // the boss's crit chance below 0, in percent
        float armorReduction = 0.0f;    // uncapped
        float armorPoints = 0.0f;
        float points = 0.0f;
        float reductionPct = 0.0f;      // less damage taken, capped
    };
    [[nodiscard]] Robustness ComputeRobustness(Player const* player);
    // A hit a player takes, less its Robustesse
    [[nodiscard]] uint32 ApplyRobustness(Player const* victim, uint32 damage);

    // --- The character sheet's summary, against the reference boss -----------------------------------------------
    struct Summary
    {
        float meleeCrit = 0.0f, rangedCrit = 0.0f, spellCrit = 0.0f;
        float meleeCritBonus = 0.0f, rangedCritBonus = 0.0f, spellCritBonus = 0.0f;
        float meleeHitCap = 0.0f, meleeHitExcess = 0.0f, whiteHitCap = 0.0f;
        float rangedHitCap = 0.0f, rangedHitExcess = 0.0f;
        float spellHitCap = 0.0f, spellHitExcess = 0.0f;
        float expertise = 0.0f, dodgeCap = 0.0f, parryCap = 0.0f;
        float armorPenetration = 0.0f, armorPenetrationExcess = 0.0f;
        float meleePrecision = 0.0f;        // from behind (the dodge cap)
        float meleePrecisionFront = 0.0f;   // in front (the parry cap)
        float rangedPrecision = 0.0f, spellPrecision = 0.0f;
        Robustness robustness;
    };
    [[nodiscard]] Summary BuildSummary(Player const* player);
    // The client's two addon messages' bodies ("C\t..." critique and precision, "R\t..." robustesse), tab separated:
    // FrameXML's DragonUI sidebar override reads them in this order
    [[nodiscard]] std::string FormatCritPrecision(Summary const& summary);
    [[nodiscard]] std::string FormatRobustness(Summary const& summary);
}

#endif
