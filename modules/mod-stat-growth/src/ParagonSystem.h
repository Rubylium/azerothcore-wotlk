#ifndef MOD_STAT_GROWTH_PARAGON_SYSTEM_H
#define MOD_STAT_GROWTH_PARAGON_SYSTEM_H

#include "DatabaseEnvFwd.h"
#include "Define.h"

#include <string>
#include <string_view>

class Creature;
class Player;
class Unit;

// The paragon board: a second layer of permanent power on top of the essences, spent on a tree rather than
// granted flat. A node is worth several essences at least, so every point is an event; the point cap is what
// keeps that from running away. Prestige raises it (Paragon.PointsPerPrestige each reset).
//
// The board itself is data (paragon_node, paragon_node_link) and the client draws whatever the server sends,
// so reshaping the tree is a rerun of localTools/paragon/buildParagonTree.py, not a rebuild.

void LoadParagonBoard();

// Re-applies a character's allocated nodes. Called on login, and after anything that changes the allocation.
void ApplyStoredParagon(Player* player);
void LoadParagonForPlayer(Player* player);
void ForgetParagonForPlayer(Player* player);

// Takes the board's modifiers off, and puts them back. Prestige does this around the level change: armour
// percent is a snapshot of the armour it was applied to, so it has to come off before the gear does and go
// back on afterwards. Flat stats come off and on as the same amounts.
void SuspendParagon(Player* player);
void RestoreParagon(Player* player);

uint32 GetParagonPrestige(Player* player);
uint32 GetParagonEarned(Player* player);
uint32 GetParagonSpent(Player* player);
uint32 GetParagonPointCap(Player* player);
void SetParagonPrestige(Player* player, uint32 prestige);

// Writes earned and prestige. Pass a transaction to commit them with the rest of a prestige.
void SaveParagonPoints(Player* player, CharacterDatabaseTransaction trans);

// A boss died: roll each real player in the group for a point, at a chance set by the difficulty it was killed
// on. Rolled per player, so nobody is competing for it. A Mythic+ boss never rolls - the run itself pays.
void TryAwardParagonPoint(Player* player, Creature* killed);

// Hands points over outright, with a short reason for the message. For awards that are earned rather than
// rolled for, such as finishing a key.
void AwardParagonPoints(Player* player, uint32 count, std::string_view reason);

// Bots do not own a board. They are handed the stat a real board of the group's average size would be worth,
// on their own best stat, so a bot party keeps pace with the player filling theirs.
void ApplyBotParagon(Player* bot);

// "Paragon\t..." addon whispers from the frame: OPEN, ALLOC <node>, RESET.
void HandleParagonAddonMessage(Player* player, uint32 language, std::string const& message);

// Opens the board on the client. The gossip option and the NPC script both come through here.
void SendParagonBoard(Player* player);

// Combat hooks. Each returns immediately for a character with no procs allocated, which is almost all of
// them, so they are cheap enough to sit on the damage path.
void OnParagonDamageTaken(Unit* victim, Unit* attacker, uint32& damage);
void OnParagonDamageDealt(Unit* attacker, Unit* victim, uint32& damage);
void OnParagonKill(Player* player, Unit* killed);
void UpdateParagonBuffs(Player* player);

void AddParagonScripts();

#endif
