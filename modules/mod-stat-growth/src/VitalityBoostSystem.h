#ifndef MOD_STAT_GROWTH_VITALITY_BOOST_SYSTEM_H
#define MOD_STAT_GROWTH_VITALITY_BOOST_SYSTEM_H

#include "Define.h"

class Creature;
class Player;

void ApplyVitalityBoost(Player* player, float& maxHealth);
// Maximum health that many (effective) Vitality points give at the player's level
uint32 GetVitalityHealth(Player const* player, uint32 points);
// What that many saved Vitality points count for, after the diminishing returns (EssenceTuning::VitalityCeiling)
uint32 GetEffectiveVitalityPoints(uint32 points);
// The Vitality points the character's essences have given (a bot's mirror of its players is BotEssenceSystem.h's)
uint32 GetStoredVitalityPoints(Player* player);
bool GrantVitalityBoost(Player* player, uint32 amount, uint32& totalBonus);
void TryAddVitalityBoostLoot(Player* player, Creature* killed);

#endif
