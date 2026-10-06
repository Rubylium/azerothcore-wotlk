#ifndef MOD_LEGENDARY_H
#define MOD_LEGENDARY_H

#include "Define.h"

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

enum Power : uint32
{
    POWER_INQUISITOR_BRAND = 1,     // Marque de l'Inquisiteur: direct damage brands the target, X% of it burning
};

struct Definition
{
    uint32 id;
    uint32 baseItem;                // the template (its look, slot, quality); ids below 65536, in Item.dbc
    Power power;
    // The power's window at the floor item level and at TopItemLevel, in percent
    float bottomLow, bottomHigh, topLow, topHigh;
    uint32 floorItemLevel;          // the lowest item level it drops at (its window's bottom)
    uint32 referenceArmor;          // its slot's armour at item level 284 (the reference cloak's)
    uint32 sourceDungeon;           // where it drops: the Dungeon Finder dungeon a Mythic+ key of completes
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
