#ifndef MOD_STAT_GROWTH_MYTHIC_APPEARANCE_H
#define MOD_STAT_GROWTH_MYTHIC_APPEARANCE_H

#include "Define.h"

class Item;
class Player;

namespace MythicAppearance
{
// The enchantment a generated weapon worn by this player shows as its glow (MythicAppearance.cpp), 0 when it shows
// none: an item that is not generated, below the first tier, or a player who turned the looks off. mod-forge leaves
// its own glow off such a weapon, so the two never fight over the client's visible enchantments.
uint32 GetWeaponGlow(Player* player, Item const* item);
}

void AddMythicAppearanceScripts();

#endif
