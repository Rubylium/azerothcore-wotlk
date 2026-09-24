#ifndef MOD_STAT_GROWTH_GLADIATOR_STANCE_SYSTEM_H
#define MOD_STAT_GROWTH_GLADIATOR_STANCE_SYSTEM_H

class Player;

void AddGladiatorStanceScripts();
void LearnGladiatorStance(Player* player);
void OnGladiatorLevelChanged(Player* player, uint8 oldLevel);
void UpdateGladiatorStance(Player* player);

#endif
