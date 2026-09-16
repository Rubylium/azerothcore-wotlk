#ifndef MOD_STAT_GROWTH_ESSENCE_TIER_SYSTEM_H
#define MOD_STAT_GROWTH_ESSENCE_TIER_SYSTEM_H

#include "Define.h"

#include <string_view>

class Creature;

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

#endif
