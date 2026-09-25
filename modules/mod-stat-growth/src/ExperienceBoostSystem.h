#ifndef MOD_STAT_GROWTH_EXPERIENCE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_EXPERIENCE_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

void ApplyExperienceBoost(Player* player, uint32& amount);
bool GrantExperienceBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddExperienceBoostLoot(Player* player, Creature* killed);
// .xp <percent>: a game master's own experience rate
void AddExperienceRateCommand();

#endif
