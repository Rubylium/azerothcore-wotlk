#ifndef MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H
#define MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H

#include "Define.h"

class Creature;
class Player;
struct ItemTemplate;
struct LootItem;

void ImproveBaseEquipmentLoot(Player* player, Creature* killed);
ItemTemplate const* SelectMythicLootItem(Player* player, uint32 itemLevel);

#endif
