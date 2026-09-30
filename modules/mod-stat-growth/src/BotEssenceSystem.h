#ifndef MOD_STAT_GROWTH_BOT_ESSENCE_SYSTEM_H
#define MOD_STAT_GROWTH_BOT_ESSENCE_SYSTEM_H

#include "Define.h"

class Player;

// Bots loot no essences worth the name, while the players they fill for carry hundreds. A bot in a group with real
// players carries what they carry: the average essences of the group's real players on its map, through the same code
// paths as a player's own - Growth as flat stats spread over the bot's class stats the way a player of that class
// receives them (StatGrowthSystem.cpp), Vitality as maximum health, Resource as regeneration. Experience and Fortune
// do nothing in a fight and are left out.
//
// Mirrors ParagonSystem.cpp's bot board: looked at every few seconds out of combat and at once on a map change, only
// players on the bot's own map are read (another map updates on another thread), taken off cleanly when the group,
// the map or the players' totals change, and never saved - the state lives in the bot's CustomData, its own
// character_settings are untouched.
void UpdateBotEssences(Player* bot, uint32 diff);
void RefreshBotEssences(Player* bot);

// What the mirror currently adds, 0 for a real player or a bot without one
uint32 GetBotEssenceVitality(Player* player);
uint32 GetBotEssenceResource(Player* player);
// The mirror as applied (.botcatchup): the stat points spread over the class, Vitality, Resource, players averaged
// What a bot's mirrored Growth points give, all stats together, after the diminishing returns
uint32 GetBotEssenceEffectiveStats(Player* bot);
bool GetBotEssenceSummary(Player* bot, uint32& statPoints, uint32& vitality, uint32& resource, uint32& players);

#endif
