#ifndef MOD_STAT_GROWTH_ROGUE_MOMENTUM_SYSTEM_H
#define MOD_STAT_GROWTH_ROGUE_MOMENTUM_SYSTEM_H

#include "SharedDefines.h"

class Player;

void AddRogueMomentumScripts();
void ApplyRogueMomentumRegeneration(Player* player, Powers power, float& amount);
void ClearRogueMomentum(Player* player);
void LearnRogueMomentumAbilities(Player* player);
void OnRogueMomentumKill(Player* player);
void UpdateRogueMomentum(Player* player, uint32 diff);

#endif
