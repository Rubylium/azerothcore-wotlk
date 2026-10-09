#ifndef ACORE_POWER_SCALING_H
#define ACORE_POWER_SCALING_H

#include "Define.h"

#include <algorithm>
#include <cmath>
#include <iterator>

// The power model every piece of content is tuned against: how strong a character is at an item level, and so how
// much health and damage content made "for item level X" should have. Documented, with its measures and how to use
// it, in .agents/docs/systems/power-scaling.md - keep the two together.
//
//  - Gear above the game's best items (284) is generated (MythicItemGeneration.cpp, mod-forge's Forge.cpp): its
//    primary stats, stamina, weapon damage, armour and block grow linearly with the item level, its secondary ratings
//    with the square root of it, so each upgrade adds a steady few percent and crit, haste and hit never run into
//    their caps.
//  - What that gear is worth is measured, not derived: DpsCurve is the combat bench's reference damage dealer at
//    each item level (localTools/combatBench/runBench.ps1), GearStamina a real set's stamina.
//  - Content sizes itself from those: a DPS check's health from the damage of the group it is made for, a hit from
//    the health of a player of that item level (Mythic::GetExpectedPlayerHealth for the Mythic+ keys).
namespace Power
{
// The game's best items (Mythic::MaxItemLevel): power 1, the base of every generated item
constexpr float ReferenceItemLevel = 284.0f;

// Generated gear's growth over its base item, by the item level ratio: primary stats, stamina, spell and attack
// power, weapon damage, armour, block value
constexpr float StatExponent = 1.0f;
// ... and its secondary ratings (hit, crit, haste, expertise, armour penetration, defence, dodge, parry, block,
// resilience): slower, as they turn into percentages that cap
constexpr float RatingExponent = 0.5f;

// ITEM_MOD_* (ItemTemplate.h): the ratings, which grow with RatingExponent
inline bool IsRatingStat(uint32 statType)
{
    return (statType >= 12 && statType <= 37) || statType == 44;
}

inline float StatGrowth(float fromItemLevel, float toItemLevel, bool rating = false)
{
    if (fromItemLevel <= 0.0f)
        return 1.0f;
    return std::pow(toItemLevel / fromItemLevel, rating ? RatingExponent : StatExponent);
}

// Damage of the reference damage dealer (the combat bench's Fire mage, a tuned class, single target on a Mythic+
// boss dummy) at an item level, over its damage at ReferenceItemLevel. Measured points, joined linearly and carried
// on past the last ones by their slope. Below 284 the gear is the game's own items.
struct DpsPoint
{
    float itemLevel;
    float index;
};

// Measured 2026-09-30 (power-scaling.md, "Measures"); past 284 smoothed to (I / 284)^0.85, which fits the measures
constexpr DpsPoint DpsCurve[] = {
    { 223.0f, 0.48f },
    { 264.0f, 0.82f },
    { 284.0f, 1.00f },
    { 330.0f, 1.14f },
    { 370.0f, 1.25f },
    { 460.0f, 1.51f },
};

inline float DpsIndex(float itemLevel)
{
    constexpr std::size_t count = std::size(DpsCurve);
    std::size_t upper = 1;
    while (upper < count - 1 && itemLevel > DpsCurve[upper].itemLevel)
        ++upper;
    DpsPoint const& low = DpsCurve[upper - 1];
    DpsPoint const& high = DpsCurve[upper];
    float const t = (itemLevel - low.itemLevel) / (high.itemLevel - low.itemLevel);
    return std::max(0.1f, low.index + (high.index - low.index) * t);
}

// Damage of the reference damage dealer with paragon points active (its board spent as a bench bot spends it), over
// its damage with none. Measured on the combat bench 2026-09-30 (a Fire mage at item levels 280 and 460, 60 s on one
// target, power-scaling.md "Measures"): it grows about as 1.0065 a point compounded up to some 250 points, then
// flattens - past about 425 points what is left of the board is defence and other roles' nodes, and a damage dealer
// gains nothing more. The compounded rule it replaces put 650 points at x67 (measured: about x9.6).
struct ParagonPoint
{
    float points;
    float index;
};

constexpr ParagonPoint ParagonCurve[] = {
    { 0.0f, 1.0f },
    { 85.0f, 2.0f },
    { 212.0f, 3.7f },
    { 255.0f, 5.4f },
    { 340.0f, 7.8f },
    { 425.0f, 9.6f },
};

inline float ParagonDpsIndex(float paragonPoints)
{
    constexpr std::size_t count = std::size(ParagonCurve);
    float const points = std::max(paragonPoints, 0.0f);
    if (points >= ParagonCurve[count - 1].points)
        return ParagonCurve[count - 1].index;
    std::size_t upper = 1;
    while (points > ParagonCurve[upper].points)
        ++upper;
    ParagonPoint const& low = ParagonCurve[upper - 1];
    ParagonPoint const& high = ParagonCurve[upper];
    float const t = (points - low.points) / (high.points - low.points);
    return low.index + (high.index - low.index) * t;
}

// Essences (mod-stat-growth EssenceTuning.h): Growth (a stat of the class a point) and Vitality (40 health a point
// at 80) count with diminishing returns, ceiling * (1 - e^(-points / ceiling)), per stat for Growth. A typical
// player gathers EssenceGrowthPerKey points of each stat and EssenceVitalityPerKey Vitality a key of progress.
// At the Growth ceiling its stats add EssenceDpsAtCeiling to its damage.
constexpr float EssenceGrowthCeiling = 600.0f;
constexpr float EssenceVitalityCeiling = 1250.0f;
constexpr float EssenceGrowthPerKey = 8.0f;
constexpr float EssenceVitalityPerKey = 25.0f;
constexpr float EssenceDpsAtCeiling = 0.15f;

inline float EssenceDiminished(float points, float ceiling)
{
    return ceiling * (1.0f - std::exp(-std::max(points, 0.0f) / ceiling));
}

// How far a player of an item level has progressed, in Mythic+ keys (their essences and Vitality come with it): the
// key whose loot is that item level (Mythic::GetItemLevel: 223 at Mythique 0, 2.45 item levels a key), one key on
constexpr float ProgressBaseItemLevel = 223.0f;
constexpr float ProgressItemLevelsPerKey = 147.0f / 60.0f;

inline float ProgressKeys(float itemLevel)
{
    return std::clamp((itemLevel - ProgressBaseItemLevel) / ProgressItemLevelsPerKey + 1.0f, 0.0f, 99.0f);
}

// The damage a typical player's essences add at that progress
inline float EssenceDpsIndex(float progressKeys)
{
    return 1.0f + EssenceDpsAtCeiling *
        EssenceDiminished(EssenceGrowthPerKey * progressKeys, EssenceGrowthCeiling) / EssenceGrowthCeiling;
}

// A character's damage at an item level and a number of paragon points active, with the essences that item level
// comes with, over the reference damage dealer's at ReferenceItemLevel with none: THE power index content is sized
// with
inline float PowerIndex(float itemLevel, float paragonPoints = 0.0f)
{
    return DpsIndex(itemLevel) * ParagonDpsIndex(paragonPoints) * EssenceDpsIndex(ProgressKeys(itemLevel));
}

// The reference damage dealer's damage per second at ReferenceItemLevel and no paragon, measured on the bench (60 s,
// Mythic+ dummies, no group buffs): on one target, and on a pack of five. A group of N damage dealers of item level
// I and paragon P deals about N * ReferenceDps * PowerIndex(I, P).
constexpr float ReferenceSingleTargetDps = 6400.0f;
constexpr float ReferencePackDps = 15900.0f;

inline float ExpectedDps(float itemLevel, float paragonPoints = 0.0f, bool pack = false)
{
    return (pack ? ReferencePackDps : ReferenceSingleTargetDps) * PowerIndex(itemLevel, paragonPoints);
}

// What a tank and a healer deal next to a damage dealer of the same profile. A healer heals: measured 2026-10-01 in
// L'Infini, a group's healers dealt 0-3 000 next to its damage dealers' 12 000-13 000 (counted a third of one, as a
// tank, a group was sized for damage it never had).
constexpr float TankDpsShare = 1.0f / 3.0f;
constexpr float HealerDpsShare = 0.1f;

// A group, in damage dealers (DpsCheckHealth): a 10-player group (2 tanks, 3 healers, 5 damage dealers, the raid
// finder's) is worth 5.97
inline float GroupDamageDealers(float damageDealers, float tanks, float healers)
{
    return damageDealers + tanks * TankDpsShare + healers * HealerDpsShare;
}

// A DPS check: the health a target needs to last `seconds` against `damageDealers` damage dealers of that item level
// and paragon (a group counted with GroupDamageDealers).
inline float DpsCheckHealth(float itemLevel, float paragonPoints, float damageDealers, float seconds,
                            bool pack = false)
{
    return ExpectedDps(itemLevel, paragonPoints, pack) * damageDealers * seconds;
}

// A raid group's damage over the model's (DpsCheckHealth), measured on the simulation bench's raid layout
// (localTools/simBench/simBench.ps1 raid: 10-player teams - 2 tanks, 3 healers, 5 damage dealers, every damage spec in
// turn - in one phase with their buffs, auras and totems, on a boss that hits its tanks and pulses the group, each spec
// on its single-target build and tuned to the Fire mage; a raid's bots take that build too). By the profile's power
// index, between the points measured. Measured 2026-10-09 (raid-2026-10-09_13-15-58: 7 teams a profile, every damage
// spec, 2 runs of 120 s): the team's mean damage dealer x5 and its tanks and healers, over 5.97 x ExpectedDps.
struct RaidPoint
{
    float itemLevel;
    float paragon;
    float factor;
};

constexpr RaidPoint RaidCurve[] = {
    { 300.0f, 100.0f, 1.048f },     // L'Infini, Défi I: 97k
    { 340.0f, 300.0f, 0.911f },     // Défi V: 290k (the mid-paragon sag)
    { 390.0f, 550.0f, 1.000f },     // Défi X: 523k
    { 450.0f, 600.0f, 1.087f },     // Vorhan's: 653k
    { 460.0f, 650.0f, 1.105f },     // the Hollow Voice's: 678k
    { 477.0f, 650.0f, 1.180f },     // its loot (Vorhan's profile): 748k; 8 players (2 / 2 / 4) 1.095, 567k
};

inline float RaidDpsFactor(float itemLevel, float paragonPoints)
{
    constexpr std::size_t count = std::size(RaidCurve);
    float const power = PowerIndex(itemLevel, paragonPoints);
    auto const at = [](RaidPoint const& point) { return PowerIndex(point.itemLevel, point.paragon); };
    if (power <= at(RaidCurve[0]))
        return RaidCurve[0].factor;
    if (power >= at(RaidCurve[count - 1]))
        return RaidCurve[count - 1].factor;
    std::size_t upper = 1;
    while (power > at(RaidCurve[upper]))
        ++upper;
    float const low = at(RaidCurve[upper - 1]);
    float const t = (power - low) / (at(RaidCurve[upper]) - low);
    return RaidCurve[upper - 1].factor + (RaidCurve[upper].factor - RaidCurve[upper - 1].factor) * t;
}

// A raid boss of the board's own (the Hollow Voice, Vorhan, L'Infini) is cleared just before its enrage, no one dead,
// by a group at its profile: its health is what that group deals over the seconds it can be hit, of which a group
// playing well gives RaidClearShare - the rest goes to mechanics and movement (the bench's group stands still).
constexpr float RaidClearShare = 0.85f;

inline float RaidBossHealth(float itemLevel, float paragonPoints, float damageDealers, float seconds)
{
    return DpsCheckHealth(itemLevel, paragonPoints, damageDealers, seconds) * RaidDpsFactor(itemLevel, paragonPoints) *
        RaidClearShare;
}

// Stamina of a full set of gear at an item level. The game's own items (up to 284) grow about with the square of
// their item level (a real set: 2036 stamina at item level 298); generated ones linearly from there (StatExponent).
constexpr float GearStaminaPerSquaredItemLevel = 0.0229f;

inline float GearStamina(float itemLevel)
{
    if (itemLevel <= ReferenceItemLevel)
        return GearStaminaPerSquaredItemLevel * itemLevel * itemLevel;
    return GearStaminaPerSquaredItemLevel * ReferenceItemLevel * ReferenceItemLevel *
        StatGrowth(ReferenceItemLevel, itemLevel);
}

// Maximum health of a damage dealer (no tank bonuses; a tank has about 1.45 times it), calibrated on real
// characters (.agents/docs/systems/power-scaling.md): class base health and stamina at 80, its gear's stamina, its
// personal loot bonuses (stamina, maximum health), the essences and Vitality gathered while progressing, the
// board's health nodes, group buffs and talents.
constexpr float PlayerBaseHealth = 7300.0f;
constexpr float PlayerBaseStamina = 110.0f;
constexpr float HealthPerStamina = 10.0f;
constexpr float AffixStaminaPerItemLevel = 1.87f;
constexpr float AffixHealthPctPerItemLevel = 0.278f;
constexpr float VitalityHealthPerPoint = 40.0f;
constexpr float ParagonHealthPctPerPoint = 0.04f;
constexpr float BuffsAndTalents = 1.1f;

inline float ExpectedPlayerHealth(float itemLevel, float paragonPoints, float progressKeys)
{
    float const stamina = PlayerBaseStamina + GearStamina(itemLevel) + AffixStaminaPerItemLevel * itemLevel +
        EssenceDiminished(EssenceGrowthPerKey * progressKeys, EssenceGrowthCeiling);
    float health = PlayerBaseHealth + HealthPerStamina * stamina;
    health *= 1.0f + AffixHealthPctPerItemLevel * itemLevel / 100.0f;
    health += VitalityHealthPerPoint * EssenceDiminished(EssenceVitalityPerKey * progressKeys, EssenceVitalityCeiling);
    health *= 1.0f + ParagonHealthPctPerPoint * std::max(paragonPoints, 0.0f) / 100.0f;
    return health * BuffsAndTalents;
}

// ... with the progress that item level usually comes with: the health content's hits are sized on
inline float ExpectedPlayerHealth(float itemLevel, float paragonPoints)
{
    return ExpectedPlayerHealth(itemLevel, paragonPoints, ProgressKeys(itemLevel));
}
}

#endif
