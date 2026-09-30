#ifndef MOD_STAT_GROWTH_RESOURCE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_RESOURCE_BOOST_SYSTEM_H

#include "Define.h"
#include "SharedDefines.h"

class Creature;
class Player;

void ApplyResourceRegenerationBoost(Player* player, Powers power, float& amount);
void ApplyResourceGenerationBoost(Player* player, Powers power, int32& amount);
// Effective regeneration bonus, including equipment and mirrored bot essences, capped at 500%.
uint32 GetResourceBonus(Player* player);
// The regeneration percent the character's Essences of Resource have given
uint32 GetStoredResourcePoints(Player* player);
bool GrantResourceBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddResourceBoostLoot(Player* player, Creature* killed);

#endif
