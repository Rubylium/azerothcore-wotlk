#ifndef MOD_STAT_GROWTH_ESSENCE_TIER_SYSTEM_H
#define MOD_STAT_GROWTH_ESSENCE_TIER_SYSTEM_H

#include "Define.h"

#include <string_view>

class Creature;
class Player;

enum class EssenceFamily : uint8
{
    Growth,
    Experience,
    Resource,
    Vitality,
    Fortune
};

enum class EssenceTier : uint8
{
    Faint,
    Greater,
    Ascendant
};

void AddTieredEssenceLoot(Creature* killed, EssenceFamily family);
EssenceTier GetEssenceTier(uint32 itemEntry);
std::string_view GetEssenceTierName(EssenceTier tier);
uint32 GetTieredEssenceBonus(uint32 baseAmount, uint32 itemEntry);
bool IsEssenceItem(uint32 itemEntry);
bool TryGetEssenceFamily(uint32 itemEntry, EssenceFamily& family);
// A random essence of any family; its tier is the best of that many tier rolls
uint32 RollEssenceEntry(uint32 tierRolls);
// How many essences a Mythic+ clear of that key level pays
uint32 GetMythicEssenceReward(uint32 level);
// Grants that many essences at once, each rolled for its own family and tier, reporting the lot in one line
// (StatGrowthScripts.cpp). Returns how many were granted.
uint32 GrantEssenceRewards(Player* player, uint32 count, uint32 tierRolls);

#endif
