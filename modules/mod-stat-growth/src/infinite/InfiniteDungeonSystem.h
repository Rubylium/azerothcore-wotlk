#ifndef MOD_STAT_GROWTH_INFINITE_DUNGEON_SYSTEM_H
#define MOD_STAT_GROWTH_INFINITE_DUNGEON_SYSTEM_H

#include "Define.h"

class Creature;
class Map;
class Player;

// The Infinite Dungeon (Donjon infini): an endless ladder of short floors for one or two real players, entered from a
// keeper in every capital. See InfiniteDungeonSystem.cpp, the module README and
// .agents/plans/infinite-dungeon/infinite-dungeon.DESIGN.md. The core side is src/server/game/Maps/InfiniteDungeon.h.
namespace InfiniteDungeon
{
// Whether the player is in a run (on a floor, or on its way to one)
bool IsInRun(Player const* player);
void OnRunExperience(Player* player, uint32 amount);
// Whether the map is the instance of a floor under way
bool IsFloorMap(Map const* map);
// A creature of a floor (a copy of a stock one, 920010-920299): it drops nothing, the floor pays
bool IsFloorCreature(Creature const* creature);
}

void AddInfiniteDungeonScripts();

#endif
