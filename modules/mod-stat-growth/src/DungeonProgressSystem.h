#ifndef MOD_STAT_GROWTH_DUNGEON_PROGRESS_SYSTEM_H
#define MOD_STAT_GROWTH_DUNGEON_PROGRESS_SYSTEM_H

#include "Define.h"

class Player;
class Unit;

// Dungeon progress tracker: tells the client which bosses an instance holds and which are already defeated.
void SendDungeonProgress(Player* player);
void OnDungeonProgressUnitDeath(Unit* unit);

// Dungeon Finder gear locks: tells the client the average item level each locked dungeon requires
void AddDungeonFinderLockScripts();

#endif
