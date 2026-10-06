#ifndef MOD_FORGE_VISUALS_H
#define MOD_FORGE_VISUALS_H

#include "Define.h"

class Player;

namespace ForgeVisuals
{
struct State
{
    uint32 armourRanks = 0;
    uint32 cooldownMs = 0;
};

void Reset(Player* player, State& state);
void SetArmourRanks(Player* player, State& state, uint32 ranks);
void UpdateTrail(Player* player, State& state, uint32 diff);
}

#endif
