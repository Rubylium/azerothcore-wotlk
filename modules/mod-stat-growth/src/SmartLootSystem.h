#ifndef MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H
#define MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H

#include "Define.h"

#include <vector>

class Creature;
class Player;
struct ItemTemplate;
struct LootItem;

void ImproveBaseEquipmentLoot(Player* player, Creature* killed);
// equipmentSlot: only for the slot group holding that equipment slot (a purchase for a slot chosen), else NULL_SLOT
ItemTemplate const* SelectMythicLootItem(Player* player, uint32 itemLevel, uint32 givenItemLevel = 0,
    uint8 equipmentSlot = 255);
// One of these items (one set piece's profiles: mod-legendary SetPieces.cpp) for the player, judged as a Mythic+
// reward's pick: one that suits them, of about the best stat score; when none does, the best scored if `anyway`
ItemTemplate const* SelectSuitedItem(Player* player, std::vector<ItemTemplate const*> const& items, bool anyway);
// An item of that quality for the player's own level (required level at most theirs, as close to it as the game has),
// fitted to its class and slots the same way: the Infinite Dungeon's gear while levelling
ItemTemplate const* SelectLevelLootItem(Player* player, uint32 quality);
// .lootdebug: the slot groups a player is behind in and what a Mythic+ reward would give them
void AddSmartLootScripts();

#endif
