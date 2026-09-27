#ifndef MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H
#define MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H

#include "Define.h"

class Creature;
class Player;
struct ItemTemplate;
struct LootItem;

void ImproveBaseEquipmentLoot(Player* player, Creature* killed);
ItemTemplate const* SelectMythicLootItem(Player* player, uint32 itemLevel, uint32 givenItemLevel = 0);
// An item of that quality for the player's own level (required level at most theirs, as close to it as the game has),
// fitted to its class and slots the same way: the Infinite Dungeon's gear while levelling
ItemTemplate const* SelectLevelLootItem(Player* player, uint32 quality);

#endif
