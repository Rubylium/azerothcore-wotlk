#ifndef MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H
#define MOD_STAT_GROWTH_SMART_LOOT_SYSTEM_H

class Creature;
class Player;
struct LootItem;

void ImproveBaseEquipmentLoot(Player* player, Creature* killed);
bool UpgradeLootItemQuality(Player* player, Creature* killed, LootItem& lootItem);

#endif
