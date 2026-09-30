#ifndef MOD_STAT_GROWTH_INFINITE_GOD_LOOT_H
#define MOD_STAT_GROWTH_INFINITE_GOD_LOOT_H

#include "Item.h"

#include <algorithm>
#include <array>

namespace InfiniteGodLoot
{
// The first of each reward's five permanent property enchantments. Preserved by the Forge and item persistence.
constexpr std::array<uint32, 3> BonusEnchants = { 3890, 3895, 3900 };
constexpr uint32 EnchantLines = 5;

inline bool IsReward(Item const* item)
{
    uint32 const first = item->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0);
    if (std::find(BonusEnchants.begin(), BonusEnchants.end(), first) == BonusEnchants.end())
        return false;
    for (uint32 line = 1; line < EnchantLines; ++line)
        if (item->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + line)) != first + line)
            return false;
    return true;
}
}

#endif
