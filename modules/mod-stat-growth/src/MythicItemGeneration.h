#ifndef MOD_STAT_GROWTH_MYTHIC_ITEM_GENERATION_H
#define MOD_STAT_GROWTH_MYTHIC_ITEM_GENERATION_H

#include "Define.h"

struct ItemTemplate;

void AddMythicItemGenerationScripts();

// Whether an item is one the raid and Mythic+ loot is generated from (its variants grown to every item level): what
// other gear systems measure themselves against
bool IsMythicBaseItem(ItemTemplate const& itemTemplate);

// The same among the top tier alone, the bases whose variants go all the way up the ladder (every item level, 485
// too): what mod-legendary's set pieces are made from
bool IsMythicTopBaseItem(ItemTemplate const& itemTemplate);

// A base item grown to an item level as its generated variants are (the power model's growth: primary stats, stamina,
// armour and weapon damage linearly, ratings slower); its entry and set left as they are, for the caller to give
ItemTemplate GrowMythicItem(ItemTemplate const& base, uint32 itemLevel);

#endif
