#ifndef MOD_STAT_GROWTH_FORTUNE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_FORTUNE_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

void ApplyFortuneGoldBoost(Player* player, int32& amount);
void ApplyFortuneLootBoost(Player* player, Creature* killed);
uint32 GetFortuneBonus(Player* player);
bool GrantFortuneBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddFortuneBoostLoot(Player* player, Creature* killed);

#endif
