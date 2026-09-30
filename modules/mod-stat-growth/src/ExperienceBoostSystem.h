#ifndef MOD_STAT_GROWTH_EXPERIENCE_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_EXPERIENCE_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

void ApplyExperienceBoost(Player* player, uint32& amount);
bool GrantExperienceBoost(Player* player, uint32 amount, uint32& totalBonus);
// The experience bonus in percent: the essences' saved points, and what counts once the gear's affix is added and the
// cap applied (EssenceTuning::MaxExperienceBonus)
uint32 GetStoredExperienceBonus(Player* player);
uint32 GetEffectiveExperienceBonus(Player* player);
void TryAddExperienceBoostLoot(Player* player, Creature* killed);
// .xp <percent>: a game master's own experience rate
void AddExperienceRateCommand();

#endif
