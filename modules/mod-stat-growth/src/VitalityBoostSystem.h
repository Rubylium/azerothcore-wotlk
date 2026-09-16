#ifndef MOD_STAT_GROWTH_VITALITY_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_VITALITY_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

void ApplyVitalityBoost(Player* player, float& maxHealth);
bool GrantVitalityBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddVitalityBoostLoot(Player* player, Creature* killed);

#endif
