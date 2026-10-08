#ifndef MOD_LEGENDARY_H
#define MOD_LEGENDARY_H

#include "Define.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

class Item;
class Player;

// Legendary items (.agents/plans/legendary-items/legendary-items.DESIGN.md): a base item gives the look, the slot and
// the quality; every copy rolls its own item level, power strength and stats, kept per item guid in
// character_legendary and applied by the server when it is worn.
namespace Legendary
{
// Mythic+ loot's cap (+60): a power's window is at its top there, and above (raids)
constexpr uint32 TopItemLevel = 370;

// What a power does: a mechanic, written once in Legendary.cpp, that a legendary takes with its own spells, numbers
// and theme. Its rolled value is always a percentage (of what, the mechanic says).
enum Kind : uint32
{
    KIND_NONE = 0,              // no power: a set piece (gear), rolled as a legendary is
    KIND_BRAND = 1,             // direct damage burns the target: X% of it over the spell's duration (its school)
    KIND_OATH,                  // a killing blow leaves 1 health and heals X% of it over 4 sec; then rests
    KIND_GROUND,                // every everyMs in combat, ground where the wearer stands: X% of AP or SP a second
    KIND_THORNS,                // struck in melee: X% of the blow back at the attacker
    KIND_SURGE,                 // direct hits may (chance, cooldown) hasten the wearer by X% for the buff's length
    KIND_ECHO,                  // every count-th direct hit strikes again: X% of AP or SP
    KIND_LAST_STAND,            // below 35% health, X% less damage taken
    KIND_KILL_FRENZY,           // a kill: X% more damage done for the buff's length
    KIND_CLEAVE,                // X% of direct damage to up to count other enemies within radius of the target
    KIND_DOT_FEAST,             // damage over time deals X% more
    KIND_LEECH,                 // X% of direct damage dealt heals the wearer (once a second)
    KIND_EXECUTE,               // X% more damage to enemies below 35% health
    KIND_KILL_NOVA,             // a kill blows up: X% of AP or SP to enemies within radius of it
    KIND_RENEW_ALLIES,          // every everyMs in combat, the most hurt ally within radius healed for X% of AP or SP
    KIND_OVERHEAL_SHIELD,       // X% of a direct heal's overhealing shields its target (up to 20% of its health)
    KIND_HEAL_SPLASH,           // X% of a direct heal also heals the most hurt other ally within radius
    KIND_KILL_HEAL,             // a kill heals the wearer for X% of their health over 4 sec
    KIND_CHAIN,                 // a direct hit (cooldown) leaps to up to count other enemies within radius: X% of it
    KIND_PULSE,                 // every everyMs in combat, a nova around the wearer: X% of AP or SP within radius
    KIND_BULWARK,               // dropping below 50% health shields X% of the health; then rests (spent spell)
    KIND_COOLDOWN_ECHO,         // an ability with a cooldown of 20 sec or more used: every other ability's cooldown
                                // loses X% of what it has left
    KIND_SUPERNOVA,             // X% of the damage and healing done feeds a star; every everyMs in combat it collapses:
                                // the damage on the target and the enemies within radius of it, the healing on the
                                // count most hurt allies around, each shared between them
};

// Which blows a damage power takes: every direct one, weapon blows only, or spells only
enum Filter : uint8
{
    FILTER_ANY = 0,
    FILTER_WEAPON,
    FILTER_SPELL,
};

// A slot's stats at a reference item level, as stock items carry them for each kind of wearer; a copy's grow from them
// with the power model (PowerScaling.h). A strength or agility copy has its primary stat and as much stamina, an
// agility one attack power besides (as stock agility gear does; strength gear never has it); an intellect copy has its
// intellect, as much stamina and spell power.
struct Budget
{
    float itemLevel;
    int32 physical, attackPower;    // strength or agility; the attack power an agility copy adds
    int32 intellect, spellPower;
    int32 secondary;                // each of the two secondaries
    std::array<int32, 4> armor;     // by the looter's armour type: cloth, leather, mail, plate
};

// A power's own numbers, besides its rolled value (0 where the mechanic has no use for one)
struct Tuning
{
    uint32 spell = 0;               // what it casts (its damage, heal, shield or buff: the combat log's line)
    uint32 spell2 = 0;              // a second one (a visual, a ground's pulse, a spent debuff)
    uint32 spell3 = 0;              // a third (a ground's heal on allies)
    uint32 everyMs = 0;             // how often (in combat)
    uint32 count = 0;               // targets, hits, ticks
    float radius = 0.0f;
    float chance = 0.0f;            // percent
    uint32 cooldownMs = 0;
    Filter filter = FILTER_ANY;
};

struct Definition
{
    uint32 id;
    uint32 baseItem;                // the template (its look, slot, quality); ids below 65536, in Item.dbc
    Kind kind;
    // The power's window at the floor item level and at TopItemLevel, in percent
    float bottomLow, bottomHigh, topLow, topHigh;
    uint32 floorItemLevel;          // the lowest item level it drops at (its window's bottom)
    Budget budget;
    uint32 sourceDungeon;           // where it drops: the Dungeon Finder dungeon a Mythic+ key of completes (0: none)
    Tuning tuning;
    uint32 sourceBoss = 0;          // or the boss whose death drops it (its creature entry)
    bool gear = false;              // a set piece (Vorhan's): no power and a stock epic's stats, no premium
};

struct Copy
{
    uint32 legendary = 0;
    uint32 itemLevel = 0;
    float power = 0.0f;             // the rolled strength, percent
    int32 armor = 0;
    std::vector<std::pair<uint32, int32>> stats;    // ITEM_MOD_* and value
};

Definition const* GetDefinition(uint32 id);
Definition const* GetDefinitionByItem(uint32 baseItem);

// The copy an item is, if it is a legendary's; thread safe
std::optional<Copy> GetCopy(Item const* item);

// The power's window for a copy dropping at itemLevel
std::pair<float, float> PowerWindow(Definition const& definition, uint32 itemLevel);

// Rolls a copy of a legendary for a player (its primary stat follows theirs), stores it and gives it; the item, or
// nullptr when their bags are full. powerOverride forces the power's value (a GM's test).
Item* GiveLegendary(Player* player, uint32 legendary, uint32 itemLevel, std::optional<float> powerOverride = {});

// Makes an item just created of a legendary's base item a rolled copy (the ground loot's, as it lands in the bags)
void MakeCopy(Player* player, Item* item, uint32 legendary, uint32 itemLevel);
}

#endif
