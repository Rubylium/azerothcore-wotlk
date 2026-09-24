#ifndef MOD_STAT_GROWTH_MYTHIC_DUNGEON_SYSTEM_H
#define MOD_STAT_GROWTH_MYTHIC_DUNGEON_SYSTEM_H

class Creature;

// A creature of a mythic instance whose kill gives no loot: trash, and in Mythic+ every creature (its loot comes at
// the end of the dungeon, see MythicDungeonSystem.cpp)
bool IsMythicLootless(Creature const* creature);
bool IsMythicCreature(Creature const* creature);

// Keeps the tank of a Mythic+ group immune to being taken off the pack, and takes it away on the way out
void UpdateMythicTankResolve(Player* player);

void AddMythicDungeonScripts();

#endif
