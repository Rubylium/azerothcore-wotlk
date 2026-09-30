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
// its damage with none: measured too, grows by ParagonDpsPerPoint a point, compounded.
constexpr float ParagonDpsPerPoint = 1.0065f;

inline float ParagonDpsIndex(float paragonPoints)
{
    return std::pow(ParagonDpsPerPoint, std::max(paragonPoints, 0.0f));
}

// A character's damage at an item level and a number of paragon points active, over the reference damage dealer's
// at ReferenceItemLevel with none: THE power index content is sized with
inline float PowerIndex(float itemLevel, float paragonPoints = 0.0f)
{
    return DpsIndex(itemLevel) * ParagonDpsIndex(paragonPoints);
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

// A DPS check: the health a target needs to last `seconds` against `damageDealers` damage dealers of that item level
// and paragon. A tank or a healer counts for about a third of a damage dealer.
inline float DpsCheckHealth(float itemLevel, float paragonPoints, float damageDealers, float seconds,
                            bool pack = false)
{
    return ExpectedDps(itemLevel, paragonPoints, pack) * damageDealers * seconds;
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
constexpr float EssenceStaminaPerKey = 8.0f;
constexpr float VitalityHealthPerKey = 25.0f * 40.0f;
constexpr float ParagonHealthPctPerPoint = 0.04f;
constexpr float BuffsAndTalents = 1.1f;

// How far a player of an item level has progressed, in Mythic+ keys (their essences and Vitality come with it): the
// key whose loot is that item level (Mythic::GetItemLevel: 223 at Mythique 0, 2.45 item levels a key), one key on
constexpr float ProgressBaseItemLevel = 223.0f;
constexpr float ProgressItemLevelsPerKey = 147.0f / 60.0f;

inline float ProgressKeys(float itemLevel)
{
    return std::clamp((itemLevel - ProgressBaseItemLevel) / ProgressItemLevelsPerKey + 1.0f, 0.0f, 99.0f);
}

inline float ExpectedPlayerHealth(float itemLevel, float paragonPoints, float progressKeys)
{
    float const stamina = PlayerBaseStamina + GearStamina(itemLevel) + AffixStaminaPerItemLevel * itemLevel +
        EssenceStaminaPerKey * progressKeys;
    float health = PlayerBaseHealth + HealthPerStamina * stamina;
    health *= 1.0f + AffixHealthPctPerItemLevel * itemLevel / 100.0f;
    health += VitalityHealthPerKey * progressKeys;
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
