#ifndef MOD_STAT_GROWTH_FORTUNE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_FORTUNE_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

// Earned money only; transfers, refunds and vendor transactions must not call this.
void ApplyFortuneGoldBoost(Player* player, uint32& amount);
uint32 GetFortuneBonus(Player* player);
bool GrantFortuneBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddFortuneBoostLoot(Player* player, Creature* killed);

#endif
