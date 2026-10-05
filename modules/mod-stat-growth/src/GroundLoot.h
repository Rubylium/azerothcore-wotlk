#ifndef MOD_STAT_GROWTH_GROUND_LOOT_H
#define MOD_STAT_GROWTH_GROUND_LOOT_H

#include "Define.h"
#include "ObjectGuid.h"

#include <functional>

class Creature;
class Item;
class Player;
struct ItemTemplate;

// Diablo-style loot: a boss of a dungeon (or of a Défi) throws its loot out on the floor a moment after it dies, for
// each player their own, and they walk over it to pick it up. Every real player of the instance sees only their own
// drops - a bag (a pile of gold for money) under a light beam of the item's quality - and the item's tooltip when
// hovering it (client: FrameXML GroundLoot.lua). The corpse's items and gold are shared out among them at the kill
// (personal loot: no rolls), and what the boss gives on top (a mythic item, the Défi's gear) lands with them.
// Anything left on the floor goes to the bags (the mailbox when full) when it fades, or the player leaves.
namespace GroundLoot
{
// At a boss's death: whether it throws its loot (a dungeon boss, a Défi's boss), and if so its corpse's loot shared out
// among the real players of the instance and the burst set up. Called again for the same corpse, nothing more.
bool Open(Creature* corpse);
// A mythic item given to a player right after a boss of theirs died, thrown out with its loot instead of going to the
// bags: false when there is no such kill (the caller gives it as usual). Bots never get theirs on the floor.
bool Throw(Player* player, ItemTemplate const* itemTemplate, std::function<void(Item*)> const& touch);
// Whether the player still has loot on the floor or about to land (mod-playerbots RaidFinder.cpp waits for it before
// the Défi's way home, and declares it itself)
bool HasPending(ObjectGuid player);
}

void AddGroundLootScripts();

#endif
