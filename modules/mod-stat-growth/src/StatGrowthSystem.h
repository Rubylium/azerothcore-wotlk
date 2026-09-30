#ifndef MOD_STAT_GROWTH_SYSTEM_H
#define MOD_STAT_GROWTH_SYSTEM_H

#include "Define.h"
#include <span>
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
// The stats a class's Essences of Growth pick from, each equally likely: a character of that class spreads its points
// evenly over them in the long run
std::span<PermanentStat const> GetClassPermanentStats(uint8 classId);
// Every point the character's Essences of Growth have given, all stats together
uint32 GetStoredStatGrowthTotal(Player* player);
// A stat's saved points, and what all of them give after the diminishing returns (EssenceTuning.h)
uint32 GetStoredStatGrowth(Player* player, PermanentStat stat);
uint32 GetEffectiveStatGrowthTotal(Player* player);
bool GrantRandomStatGrowth(Player* player, uint32 amount, std::string_view& statName);
void TryAddStatGrowthLoot(Player* player, Creature* killed);

#endif
