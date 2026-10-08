#ifndef MOD_STAT_GROWTH_MYTHIC_ITEM_GENERATION_H
#define MOD_STAT_GROWTH_MYTHIC_ITEM_GENERATION_H

struct ItemTemplate;

void AddMythicItemGenerationScripts();

// Whether an item is one the raid and Mythic+ loot is generated from (its variants grown to every item level): what
// other gear systems measure themselves against (mod-legendary's set pieces take their sockets and durability)
bool IsMythicBaseItem(ItemTemplate const& itemTemplate);

#endif
