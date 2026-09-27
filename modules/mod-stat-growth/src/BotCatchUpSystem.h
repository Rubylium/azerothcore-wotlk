#ifndef MOD_STAT_GROWTH_BOT_CATCH_UP_SYSTEM_H
#define MOD_STAT_GROWTH_BOT_CATCH_UP_SYSTEM_H

#include "Define.h"

class Player;
class Unit;

// A stopgap until every class's bot has a proper rotation: in a dungeon or a raid, a damage-dealing bot in a group
// with real players deals more damage the further it trails them. Each character's damage (its pets' and guardians'
// included) is summed per second and smoothed over about 30 seconds of combat; every 5 seconds a bot moves its
// multiplier towards BotCatchUp.TargetShare of the real damage dealers' average over its own, 10% at a time, between
// 1 and BotCatchUp.MaxMultiplier. Out of combat it fades slowly towards 1 (the next pull in the same instance starts
// near where the last one ended); leaving the instance, a tank or healer role, or no real player left ends it at once.
// Classes whose bot AI was reworked (BotCatchUp.ExcludedClasses) never get it.
//
// Shown on the bot as Chaotic Charge (41033, a stock dummy aura stacking to 200): one stack per percent of bonus.
// Everything runs on the map's own thread: a character's damage is written to its owner's CustomData by the hit, a
// bot only reads the group members on its own map. Nothing is logged or saved.

// Reads BotCatchUp.ExcludedClasses; call after the module's config is (re)loaded
void LoadBotCatchUpConfig();

// A hit landed (UnitScript::OnDamage): counted for the attacker's owning player, never logged
void RecordBotCatchUpDamage(Unit* attacker, Unit* victim, uint32 damage);

// The damage multiplier the attacker's owning bot currently has against the victim; 1 for anything else
float GetBotCatchUpMultiplier(Unit* attacker, Unit* victim);

// Samples the character's damage every second and, for a bot, adjusts its multiplier every 5 seconds
void UpdateBotCatchUp(Player* player, uint32 diff);

// Login and logout: a bot's display aura is restored from the database with its other auras
void ClearBotCatchUp(Player* player);

void AddBotCatchUpScripts();

#endif
