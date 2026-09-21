#ifndef MOD_STAT_GROWTH_PARAGON_SYSTEM_H
#define MOD_STAT_GROWTH_PARAGON_SYSTEM_H

#include "Define.h"

#include <string>

class Creature;
class Player;

// The paragon board: a second layer of permanent power on top of the essences, spent on a tree rather than
// granted flat. A node is worth several essences at least, so every point is an event; the 50 point cap is what
// keeps that from running away, and it is meant to be raised later by its own system.
//
// The board itself is data (paragon_node, paragon_node_link) and the client draws whatever the server sends,
// so reshaping the tree is a rerun of localTools/paragon/buildParagonTree.py, not a rebuild.

void LoadParagonBoard();

// Re-applies a character's allocated nodes. Called on login, and after anything that changes the allocation.
void ApplyStoredParagon(Player* player);
void LoadParagonForPlayer(Player* player);
void ForgetParagonForPlayer(Player* player);

// A boss died: roll each real player in the group for a point, at a chance set by the difficulty it was killed
// on. Rolled per player, so nobody is competing for it.
void TryAwardParagonPoint(Player* player, Creature* killed);

// Bots do not own a board. They are handed the stat a real board of the group's average size would be worth,
// on their own best stat, so a bot party keeps pace with the player filling theirs.
void ApplyBotParagon(Player* bot);

// "Paragon\t..." addon whispers from the frame: OPEN, ALLOC <node>, RESET.
void HandleParagonAddonMessage(Player* player, uint32 language, std::string const& message);

// Opens the board on the client. The gossip option and the NPC script both come through here.
void SendParagonBoard(Player* player);

void AddParagonScripts();

#endif
