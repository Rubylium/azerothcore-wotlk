#ifndef MOD_ROGUE_SHARED_H
#define MOD_ROGUE_SHARED_H

#include "Define.h"

class Player;

// What RogueTalents.cpp and RogueOutlaw.cpp share.

// Lames sans repos (the class tree's choice 92220, built into Hors-la-loi): Vanish, Sprint, Evasion, Blind, Cloak of
// Shadows and Shadowstep come back this much sooner (RogueTalents.cpp)
void ReduceRogueUtilityCooldowns(Player* player, int32 milliseconds);

#endif
