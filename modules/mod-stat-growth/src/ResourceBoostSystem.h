#ifndef MOD_STAT_GROWTH_RESOURCE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_RESOURCE_BOOST_SYSTEM_H

#include "Define.h"
#include "SharedDefines.h"

class Creature;
class Player;

void ApplyResourceRegenerationBoost(Player* player, Powers power, float& amount);
void ApplyResourceGenerationBoost(Player* player, Powers power, int32& amount);
bool GrantResourceBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddResourceBoostLoot(Player* player, Creature* killed);

#endif
