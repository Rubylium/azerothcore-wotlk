#ifndef ACORE_MYTHIC_DUNGEON_H
#define ACORE_MYTHIC_DUNGEON_H

#include "Define.h"

#include <algorithm>
#include <cmath>

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
// Mythic+ loot rises with the key, without end
constexpr uint32 ItemLevelPerKeyLevel = 4;

// The best items of the game. Above them Mythic+ loot is generated at startup (mod-stat-growth
// MythicItemGeneration.cpp) from those items: variant v is item level MaxItemLevel + 1 + ItemLevelPerKeyLevel * v,
// which a key of +16 and up asks for. The variant is read from the item level wanted, never from the key, so the
// baseline above can move without touching this. Its entry names its base item, which is how the client extension
// (awesome_wotlk GeneratedItems.cpp) draws it with that item's look: entry = GeneratedItemBase * (v + 1) + base
// entry. Real entries stay below GeneratedItemBase. Both sides must agree on these numbers.
constexpr uint32 MaxItemLevel = 284;
constexpr uint32 GeneratedItemBase = 0x10000;
constexpr uint32 GeneratedItemVariants = 128;

// Mythic+ (key level 2 and up) on top of Mythique 0. Up to +10 a key is a matter of gear: health and damage grow 8%
// a level, compounded. Past +10 it is a matter of paragon: every level asks for ParagonPerLevel more points spent on
// the board (+20 asks for 50, +30 for 100: a prestige, 10 points of cap, every two levels), and the creatures grow by what those points are worth to a character -
// about 1% of its power each, compounded (ParagonPointPower) - and by the key's better loot (KeyGearGrowth a level).
// So a character at the recommended paragon meets every key the way it met +10 - the key never looks at a
// character's own paragon, so every point gained still makes the same key easier - and the player reads the ladder as
// "this key wants that much paragon". The client shows the same numbers (MythicPlus.lua, ChallengeBoard.lua).
// Keys have no ceiling; the creature code clamps health to 32 bits.
constexpr int32 GearLevels = 10;
constexpr float CompoundedGrowth = 1.08f;
constexpr uint32 ParagonPerLevel = 5;
constexpr float ParagonPointPower = 1.01f;
constexpr float KeyGearGrowth = 1.01f;

inline uint32 GetRecommendedParagon(int32 level)
{
    return level > GearLevels ? ParagonPerLevel * static_cast<uint32>(level - GearLevels) : 0;
}

inline float GetLevelScaling(int32 level)
{
    if (level <= 0)
        return 1.0f;

    float const gear = std::pow(CompoundedGrowth, static_cast<float>(std::min(level, GearLevels)));
    float const paragon = std::pow(ParagonPointPower, static_cast<float>(GetRecommendedParagon(level)));
    float const loot = std::pow(KeyGearGrowth, static_cast<float>(std::max(level - GearLevels, 0)));
    return gear * paragon * loot;
}

inline uint32 GetItemLevel(int32 level)
{
    return BaseItemLevel + ItemLevelPerKeyLevel * static_cast<uint32>(std::max(level, 0));
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
    return MaxItemLevel + 1 + ItemLevelPerKeyLevel * variant;
}

// The variant of an item level above MaxItemLevel; the highest one past the last variant
inline uint32 GetGeneratedVariant(uint32 itemLevel)
{
    uint32 const above = itemLevel > MaxItemLevel ? itemLevel - MaxItemLevel - 1 : 0;
    return std::min(above / ItemLevelPerKeyLevel, GeneratedItemVariants - 1);
}

// A Mythic+ variant (not an item of the Forge below)
inline bool IsMythicGeneratedItem(uint32 entry)
{
    return entry >= GeneratedItemBase && entry / GeneratedItemBase <= GeneratedItemVariants;
}

// The Forge (mod-forge): a high-end item brought to the blacksmith comes back ForgeItemLevelPerRank item levels
// higher, up to ForgeRanks times. Each rank of a real item is generated at startup too, right after the Mythic+
// variants: entry = GeneratedItemBase * (GeneratedItemVariants + rank) + base entry. A Mythic+ variant needs no
// entries of its own: forged, it becomes the next variant, as many item levels higher. The client extension draws
// GeneratedItemBase * (GeneratedItemVariants + ForgeRanks + 1) - 1 entries; it must agree.
constexpr uint32 ForgeRanks = 8;
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
