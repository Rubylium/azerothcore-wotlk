#ifndef MOD_STAT_GROWTH_MYTHIC_DUNGEON_SYSTEM_H
#define MOD_STAT_GROWTH_MYTHIC_DUNGEON_SYSTEM_H

#include "Define.h"

class Creature;
class Player;
class SpellInfo;
class Unit;

// The factor a creature spell's damage is multiplied by in a mythic dungeon (the key's scaling, the spell's level
// catch-up and its tuning multiplier); 1 outside one
float GetMythicSpellFactor(Unit const* caster, SpellInfo const* spellInfo);

// A creature of a mythic instance whose kill gives no loot: trash, and in Mythic+ every creature (its loot comes at
// the end of the dungeon, see MythicDungeonSystem.cpp)
bool IsMythicLootless(Creature const* creature);
bool IsMythicCreature(Creature const* creature);

// Keeps the tank of a Mythic+ group immune to being taken off the pack, and takes it away on the way out; in any
// dungeon or raid, the tank's presence (more threat) and everyone else's discretion (less). The first looks now
// (a map change), the second every couple of seconds from the player's update.
void UpdateMythicTankResolve(Player* player);
void UpdateMythicTankResolve(Player* player, uint32 diff);

// Whether the character is its group's tank: a tank role from the Dungeon Finder or the group, else a tank stance
bool IsGroupTank(Player* player);

// One epic of that item level fitted to the player's class, into their bags (or their mailbox when full); above the
// game's best items, the generated variant of that item level. The Mythic+ reward, also the Infinite Dungeon's.
void GiveMythicLootItem(Player* player, uint32 itemLevel);
// The same for one slot chosen (an equipment slot: its group, both rings or both trinkets); false, nothing given, when
// no item of that item level fits the player there (an off hand beside a two-hander)
bool GiveMythicLootItemForSlot(Player* player, uint32 itemLevel, uint8 equipmentSlot);
// The same, touched by L'Infini (the challenge board's god, InfiniteGod.cpp): a bonus of its own on top, drawn at
// random among its three whatever the piece or its wearer, what it does and a line of its lore, in the item's five random property
// enchantment slots (a generated item has no random property). They stay on it for good, forged too.
void GiveInfiniteGodLootItem(Player* player, uint32 itemLevel);
// Every generated item the player carries, its record sent to the client again (at login: their stats may have
// changed since the client cached them)
void SendGeneratedItemRecords(Player* player);

// The combat bench's dummies (mod-playerbots Script/CombatBench.cpp, which declares it itself): a creature outside any
// mythic instance brought to what a creature of its template is in a Mythic+ key of that level (0: Mythique 0), in
// that role (MythicTuning::CreatureRole) - level, health and weapon damage, the numbers ScaleCreature gives the real
// ones. Call it on a creature freshly spawned at its template's level: the rank's health rate is read from it.
void ApplyMythicBenchScaling(Creature* creature, int32 keyLevel, uint8 role);

void AddMythicDungeonScripts();

#endif
