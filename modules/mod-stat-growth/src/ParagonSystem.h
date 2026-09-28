#ifndef MOD_STAT_GROWTH_PARAGON_SYSTEM_H
#define MOD_STAT_GROWTH_PARAGON_SYSTEM_H

#include "DatabaseEnvFwd.h"
#include "Define.h"

#include <string>
#include <string_view>

class Creature;
class Player;
class Spell;
class SpellInfo;
class Unit;

// The paragon board: a second layer of permanent power on top of the essences, spent on a tree rather than
// granted flat. A node is worth several essences at least, so every point is an event; the point cap is what
// keeps that from running away. Prestige raises it (Paragon.PointsPerPrestige each reset).
//
// Points come from bosses, from finished keys and from paragon levels: at the level cap, experience keeps
// filling a bar, and every level of it is a point. The board has three zones, and the outer two are where a
// character stops being merely strong.
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

// Bots own a real board, with the same stats and procs as a player's, planned rather than bought and never saved.
// Its size is the content's: a Mythic+ key's recommended paragon, a challenge tier's (SetParagonInstanceBudget), else
// the average spent by the group's real players on the bot's map; none at all when that is 0. Its shape is the bot's
// role: a tank walks Carapace, a fighter the weapon branch of its main stat, a caster Arcanes, a healer Intellect,
// zone by zone under the player's rules. Looked at again every few seconds out of combat, and at once on a map change.
void UpdateBotParagon(Player* bot, uint32 diff);
void RefreshBotParagon(Player* bot);

// The paragon an instance's content asks of its bots, over what its key level says: the challenge board's tiers
// (mod-playerbots RaidFinder.cpp). 0 forgets the instance.
void SetParagonInstanceBudget(uint32 instanceId, uint32 points);

// The extra threat the board's tank nodes give, in percent (mod-stat-growth's tank aura applies it)
uint32 GetParagonThreatPct(Player* player);

// "Paragon\t..." addon whispers from the frame: OPEN, ALLOC <node>, RESET, and the glyphs' GLYPHSYNC,
// SOCKET <node> <glyph>, UNSOCKET <node>, ABSORB <glyph>.
void HandleParagonAddonMessage(Player* player, uint32 language, std::string const& message);

// Opens the board on the client. The gossip option and the NPC script both come through here.
void SendParagonBoard(Player* player);

// Combat hooks. Each returns immediately for a character with no procs allocated, which is almost all of
// them, so they are cheap enough to sit on the damage path.
void OnParagonDamageTaken(Unit* victim, Unit* attacker, uint32& damage);
// Rancune's count of a hit taken: landed plus absorbed, before the player's damage-taken reductions
void NoteParagonHitTaken(Unit* victim, Unit* attacker, uint32 amount, SpellSchoolMask schoolMask);
void OnParagonDamageDealt(Unit* attacker, Unit* victim, uint32& damage);
void OnParagonKill(Player* player, Unit* killed);
// What is about to deal the next hit (ModifyFinalDamage, ModifyPeriodicDamageAurasTick): no spell for a white swing.
// The damage hook itself is not told, and a branch's effects answer only to weapons or only to spells.
void NoteParagonDamageSource(Unit* attacker, Unit* victim, SpellInfo const* spellInfo, bool periodic);
// The caster side: a spell's direct damage once dealt (echo, arc), and a spell cast (quickening, spell power,
// ward, mana)
void OnParagonSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage, bool critical);
void OnParagonSpellCast(Player* player, Spell* spell);
void UpdateParagonBuffs(Player* player);

// The outer zones' maximum health, applied where the module already adjusts it (OnPlayerAfterUpdateMaxHealth).
void ApplyParagonHealth(Player* player, float& value);

// Experience earned at the level cap fills the paragon bar; each paragon level is a point.
void AddParagonExperience(Player* player, uint32 amount);

// The Pantheon's healing Blessings: Eonar's grows the character's spell heals (ModifyHealReceived), Freya's shares
// out a part of the healing it receives (OnHeal)
void OnParagonHealDone(Unit* healer, Unit* target, uint32& heal, SpellInfo const* spellInfo);
void OnParagonHealReceived(Unit* healer, Unit* receiver, uint32 gain);

// Paragon glyphs. Their experience comes only while socketed, and a drop brings a glyph not yet owned first:
// - a finished key of +10 and up, and its chest's chance of a glyph (10% at +10 up to 35% at +30);
// - a floor of the Infinite Dungeon's gearing ladder from floor 50 on, and a sure glyph every 10 floors past it;
// - a heroic raid boss (every real player in the raid), 15% each.
void OnParagonKeyCompleted(Player* player, uint32 keyLevel);
void OnParagonInfiniteFloor(Player* player, uint32 floor, uint32 floorsDown);
void OnParagonCreatureDeath(Creature* creature);
// A glyph item used from the bags: learnt, or absorbed as experience. False when the item is not a glyph.
bool UseParagonGlyphItem(Player* player, uint32 itemEntry);

void AddParagonScripts();

#endif
