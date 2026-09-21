#ifndef MOD_STAT_GROWTH_SYSTEM_H
#define MOD_STAT_GROWTH_SYSTEM_H

#include "Define.h"
#include <string_view>

class Creature;
class Player;

enum class PermanentStat : uint8
{
    Strength,
    Agility,
    Stamina,
    Intellect,
    Spirit,
    AttackPower,
    SpellPower,
    Count
};

void ApplyStoredStatGrowth(Player* player);
// Applies, or takes back, a flat permanent stat bonus. Shared with the paragon board, which needs to remove
// what a node gave when it is respecced.
void ApplyPermanentStat(Player* player, PermanentStat stat, uint32 amount, bool apply);
bool GrantRandomStatGrowth(Player* player, uint32 amount, std::string_view& statName);
void TryAddStatGrowthLoot(Player* player, Creature* killed);

#endif
