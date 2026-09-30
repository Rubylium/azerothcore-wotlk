#ifndef ACORE_MYTHIC_DUNGEON_H
#define ACORE_MYTHIC_DUNGEON_H

#include "Define.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

// Mythic dungeons: a dungeon played at level 80, a step above a WotLK heroic, with its own loot. The level of a
// run (0 for Mythique 0, the key level for Mythic+) lives on the group (Group::GetMythicLevel) and is handed to the
// instance the group creates (Map::GetMythicLevel), before its creatures load.
//  - mod-playerbots (Script/RaidFinder.cpp) lists, queues and forms the runs, bots included
//  - mod-stat-growth (MythicDungeonSystem.cpp) scales the creatures and hands out the loot
namespace Mythic
{
// Level every creature of a mythic instance is brought to; bosses stand above it
constexpr uint8 CreatureLevel = 80;
constexpr uint8 BossLevel = 82;

// On top of the level-80 WotLK creature stats: a WotLK heroic sits around 1.4x its normal mode, Mythique 0 a
// step above
constexpr float HealthMultiplier = 1.6f;
constexpr float DamageMultiplier = 1.4f;

// Item level of the loot a Mythique 0 boss gives, a clear tier above WotLK heroics (200): a mythic pull hits
// for 1.4x a heroic's and has 1.6x its health before the key even counts, and 213 did not pay for that
constexpr uint32 BaseItemLevel = 223;
// Reward progression is independent of the generated templates' four-level spacing.
constexpr uint32 MaxLootItemLevel = 370;
constexpr uint32 MaxRaidItemLevel = 460;
constexpr uint32 MaxInfiniteItemLevel = 310;
constexpr int32 MaxLootKeyLevel = 60;
constexpr uint32 MaxKeyLevel = 99;
constexpr uint32 ItemLevelPerKeyLevel = 4;

// Stock items stop at 284. Higher rewards use generated copies of those templates (MythicItemGeneration.cpp).
// Existing entry blocks and the client extension's decoding stay stable: base + block * GeneratedItemBase.
// Legacy variants and Forge blocks never change: existing items retain their levels and stats.
// Three new blocks after the Forge provide exact progression caps without rewriting existing templates.
constexpr uint32 MaxItemLevel = 284;
constexpr uint32 GeneratedItemBase = 0x10000;
constexpr uint32 GeneratedItemVariants = 128;
constexpr uint32 ForgeRanks = 8;
constexpr uint32 FirstCapVariant = GeneratedItemVariants + ForgeRanks;
constexpr uint32 CapVariants = 3;

// Up to +10 difficulty grows with gear. Beyond that it also asks for five more paragon points per key.
// Loot's contribution follows the slower reward curve and stops at +60; paragon and pressure keep growing to +99.
// Creature health follows damage output; creature damage follows the expected player health below.
// A player's own paragon never changes the difficulty. Client paragon recommendations use these same constants.
constexpr int32 GearLevels = 10;
constexpr float CompoundedGrowth = 1.08f;
constexpr uint32 ParagonPerLevel = 5;
constexpr float ParagonPointPower = 1.015f;
// Approximately 2% power per four item levels, scaled to 147 item levels over 60 keys.
constexpr float KeyGearGrowth = 1.0122f;

inline uint32 GetRecommendedParagon(int32 level)
{
    level = std::min(level, static_cast<int32>(MaxKeyLevel));
    return level > GearLevels ? ParagonPerLevel * static_cast<uint32>(level - GearLevels) : 0;
}

inline float GetLevelScaling(int32 level)
{
    level = std::min(level, static_cast<int32>(MaxKeyLevel));
    if (level <= 0)
        return 1.0f;

    float const gear = std::pow(CompoundedGrowth, static_cast<float>(std::min(level, GearLevels)));
    float const paragon = std::pow(ParagonPointPower, static_cast<float>(GetRecommendedParagon(level)));
    float const loot = std::pow(KeyGearGrowth,
        static_cast<float>(std::max(std::min(level, MaxLootKeyLevel) - GearLevels, 0)));
    return gear * paragon * loot;
}

inline uint32 GetItemLevel(int32 level)
{
    return BaseItemLevel + (MaxLootItemLevel - BaseItemLevel) *
        static_cast<uint32>(std::clamp(level, 0, MaxLootKeyLevel)) / MaxLootKeyLevel;
}

// GetLevelScaling is the creatures' HEALTH: a character's damage multiplies (gear, paragon, essences all compound),
// and so does what it takes to kill a pack. A character's health does not: it grows with the stamina of its gear,
// its essences and a few board nodes, far slower. Creature DAMAGE follows that health instead (GetDamageScaling), so
// a hit that takes half of a player's health at +10 takes about half of it at +40 for a player geared for the key.
// The model (.agents/plans/mplus-scaling/mplus-scaling.ANALYSIS.md) is a damage dealer without a tank's bonuses,
// in gear one key under the key's own loot, at its recommended paragon, with a typical player's essences and gear
// bonuses, calibrated on real characters:
//  - class base health and stamina at 80 (player_class_stats, averaged), 10 health a point of stamina
//  - the gear's stamina: a real full set has 2036 at item level 298, and generated and forged items grow their
//    stats with the square of the item level (MythicItemGeneration.cpp), so the set's does too
//  - personal loot bonuses (fortune capped, a third of the items rolling each): stamina and a maximum health
//    percentage, both a share of the item level
//  - essences gathered along the way: permanent stamina and Vitality points (40 health each at 80), per key reached
//  - the board's maximum health nodes, a damage dealer's path taking a few
//  - group buffs and talents, about a tenth
// The model at Mythique 0 is close to the original 41 700 health yardstick. Past +10 a key hits harder each level
// on top (DamagePressurePerKey, up to MaxDamagePressure): the players'
// own paragon and essences outgrow the model, and a higher key should still ask for more. Tested at +37: at 1% a
// level (up to +15%) a geared rogue took 20% standing in the fire and the tank next to nothing.
constexpr float GearKeysBehind = 1.0f;
constexpr float PlayerBaseHealth = 7300.0f;
constexpr float PlayerBaseStamina = 110.0f;
constexpr float HealthPerStamina = 10.0f;
constexpr float GearStaminaPerSquaredItemLevel = 0.0229f;
constexpr float AffixStaminaPerItemLevel = 1.87f;
constexpr float AffixHealthPctPerItemLevel = 0.278f;
constexpr float EssenceStaminaPerKey = 8.0f;
constexpr float VitalityHealthPerKey = 25.0f * 40.0f;
constexpr float ParagonHealthPctPerPoint = 0.04f;
constexpr float BuffsAndTalents = 1.1f;
constexpr float DamagePressurePerKey = 0.025f;
constexpr float MaxDamagePressure = 1.6f;
// Creature melee in a key hits this much harder again (on top of the pressure): the tanks' armour, presence and
// Mythic+ resolve took most of it
constexpr float MeleePressure = 1.5f;

// Item level of the gear a player brings to a key, one key behind its reward and capped at the loot ceiling.
inline float GetExpectedItemLevel(float key)
{
    float const rewardKey = std::clamp(key - GearKeysBehind, 0.0f, static_cast<float>(MaxLootKeyLevel));
    return static_cast<float>(BaseItemLevel) + static_cast<float>(MaxLootItemLevel - BaseItemLevel) *
        rewardKey / static_cast<float>(MaxLootKeyLevel);
}

// Maximum health of a damage dealer ready for that key
inline float GetExpectedPlayerHealth(float key)
{
    key = std::clamp(key, 0.0f, static_cast<float>(MaxKeyLevel));
    float const itemLevel = GetExpectedItemLevel(key);
    float const stamina = PlayerBaseStamina + GearStaminaPerSquaredItemLevel * itemLevel * itemLevel +
        AffixStaminaPerItemLevel * itemLevel + EssenceStaminaPerKey * key;
    float const paragon = static_cast<float>(ParagonPerLevel) * std::max(key - static_cast<float>(GearLevels), 0.0f);
    float health = PlayerBaseHealth + HealthPerStamina * stamina;
    health *= 1.0f + AffixHealthPctPerItemLevel * itemLevel / 100.0f;
    health += VitalityHealthPerKey * key;
    health *= 1.0f + ParagonHealthPctPerPoint * paragon / 100.0f;
    return health * BuffsAndTalents;
}

inline float GetDamagePressure(float key)
{
    return std::min(1.0f + DamagePressurePerKey * std::max(key - static_cast<float>(GearLevels), 0.0f),
        MaxDamagePressure);
}

// The yardstick of a key's damage: a share of it is a share of a ready player's health (MythicTuning.h)
inline float GetDamageReference(float key)
{
    return GetExpectedPlayerHealth(key) * GetDamagePressure(key);
}

// What the creatures' damage is multiplied by at a key, over Mythique 0's
inline float GetDamageScaling(float key)
{
    return GetDamageReference(key) / GetDamageReference(0.0f);
}

inline bool IsGeneratedItem(uint32 entry)
{
    return entry >= GeneratedItemBase;
}

inline uint32 GetGeneratedItemEntry(uint32 baseEntry, uint32 variant)
{
    return GeneratedItemBase * (variant + 1) + baseEntry;
}

inline uint32 GetGeneratedItemLevel(uint32 variant)
{
    switch (variant)
    {
        case FirstCapVariant: return MaxInfiniteItemLevel;
        case FirstCapVariant + 1: return MaxLootItemLevel;
        case FirstCapVariant + 2: return MaxRaidItemLevel;
        default: return MaxItemLevel + 1 + ItemLevelPerKeyLevel * variant;
    }
}

// Best generated reward at or below the requested level, capped for new drops; legacy templates remain intact.
inline uint32 GetGeneratedVariant(uint32 itemLevel)
{
    itemLevel = std::min(itemLevel, MaxRaidItemLevel);
    uint32 const above = itemLevel > MaxItemLevel ? itemLevel - MaxItemLevel - 1 : 0;
    uint32 variant = std::min(above / ItemLevelPerKeyLevel, GeneratedItemVariants - 1);
    for (uint32 capVariant = FirstCapVariant; capVariant < FirstCapVariant + CapVariants; ++capVariant)
        if (GetGeneratedItemLevel(capVariant) <= itemLevel &&
            GetGeneratedItemLevel(capVariant) > GetGeneratedItemLevel(variant))
            variant = capVariant;
    return variant;
}

// A Mythic+ variant (not an item of the Forge below)
inline bool IsMythicGeneratedItem(uint32 entry)
{
    uint32 const block = entry / GeneratedItemBase;
    return block > 0 && (block <= GeneratedItemVariants ||
        (block > FirstCapVariant && block <= FirstCapVariant + CapVariants));
}

// The Forge (mod-forge): a high-end item brought to the blacksmith comes back up to ForgeItemLevelPerRank item levels
// higher, up to ForgeRanks times. Each rank of a real item is generated at startup too, right after the Mythic+
// variants: entry = GeneratedItemBase * (GeneratedItemVariants + rank) + base entry. Generated loot instead selects
// its next template by item level, respecting the source's ceiling. The client extension draws
// GeneratedItemBase * (GeneratedItemVariants + ForgeRanks + CapVariants + 1) - 1 entries; it must agree.
constexpr uint32 ForgeItemLevelPerRank = ItemLevelPerKeyLevel;

inline uint32 GetForgeItemEntry(uint32 baseEntry, uint32 rank)
{
    return GeneratedItemBase * (GeneratedItemVariants + rank) + baseEntry;
}

// The rank of a forged real item, 0 for anything else
inline uint32 GetForgeRank(uint32 entry)
{
    uint32 const block = entry / GeneratedItemBase;
    return block > GeneratedItemVariants && block <= GeneratedItemVariants + ForgeRanks ?
        block - GeneratedItemVariants : 0;
}

// The real item any generated one is a copy of
inline uint32 GetBaseItemEntry(uint32 entry)
{
    return entry % GeneratedItemBase;
}
}

#endif
