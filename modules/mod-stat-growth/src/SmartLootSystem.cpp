#include "SmartLootSystem.h"

#include "Bag.h"
#include "Chat.h"
#include "Creature.h"
#include "Group.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "LootMgr.h"
#include "MythicDungeon.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "StatGrowthConfig.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

// mod-custom-classes (TalentTree.cpp): the specialization as an index among the class's spec trees
int8 GetTalentSpecializationIndex(Player* player);

namespace
{
std::vector<ItemTemplate const*> equipmentCatalog;
bool catalogBuilt = false;

// The specializations on the talent trees (mod-custom-classes) that fight with a one-hander and a shield
constexpr int8 WarriorProtectionSpec = 2;
constexpr int8 WarriorGladiatorSpec = 3;    // the Gladiateur (mod-warrior): a damage dealer
constexpr int8 PaladinProtectionSpec = 1;

int8 SpecIndex(Player const* player)
{
    return GetTalentSpecializationIndex(const_cast<Player*>(player));
}

// A one-hander and a shield is how this player fights, whatever it holds right now (a Warrior just moved from Arms to
// the Gladiateur still wields its two-hander)
bool FightsWithShield(Player const* player)
{
    int8 const spec = SpecIndex(player);
    switch (player->getClass())
    {
        case CLASS_WARRIOR:
            return spec == WarriorProtectionSpec || spec == WarriorGladiatorSpec;
        case CLASS_PALADIN:
            return spec == PaladinProtectionSpec;
        default:
            return false;
    }
}

bool IsEquipmentInventoryType(uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_NECK:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_FINGER:
        case INVTYPE_TRINKET:
        case INVTYPE_WEAPON:
        case INVTYPE_SHIELD:
        case INVTYPE_RANGED:
        case INVTYPE_CLOAK:
        case INVTYPE_2HWEAPON:
        case INVTYPE_ROBE:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_RELIC:
            return true;
        default:
            return false;
    }
}

// Worn, an item that gives nothing: no stat, armour, block, weapon damage or spell. The rows another system dresses
// (mod-legendary's base items and its sets' rows: epics of item level 227 with no stats of their own) - as loot, an
// empty item.
bool GivesNothing(ItemTemplate const& itemTemplate)
{
    if (itemTemplate.Armor || itemTemplate.Block)
        return false;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        if (itemTemplate.ItemStat[index].ItemStatType && itemTemplate.ItemStat[index].ItemStatValue)
            return false;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_DAMAGES; ++index)
        if (itemTemplate.Damage[index].DamageMax > 0.0f)
            return false;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_SPELLS; ++index)
        if (itemTemplate.Spells[index].SpellId > 0)
            return false;
    return true;
}

bool IsCatalogEquipment(ItemTemplate const& itemTemplate)
{
    // Generated Mythic+ items are handed out as variants of their base item, never picked themselves
    if (Mythic::IsGeneratedItem(itemTemplate.ItemId) ||
        (itemTemplate.Class != ITEM_CLASS_WEAPON && itemTemplate.Class != ITEM_CLASS_ARMOR) ||
        !IsEquipmentInventoryType(itemTemplate.InventoryType) || itemTemplate.ItemLevel == 0 ||
        itemTemplate.Quality < ITEM_QUALITY_NORMAL || itemTemplate.Quality > ITEM_QUALITY_EPIC ||
        itemTemplate.Bonding == BIND_QUEST_ITEM || itemTemplate.Bonding == BIND_QUEST_ITEM1 ||
        itemTemplate.StartQuest != 0 || itemTemplate.Area != 0 || itemTemplate.Map != 0 ||
        itemTemplate.HolidayId != 0 || itemTemplate.RequiredReputationFaction != 0 ||
        itemTemplate.RequiredSkill != 0 || itemTemplate.RequiredSpell != 0 ||
        itemTemplate.HasFlag(ITEM_FLAG_NO_PICKUP) || itemTemplate.HasFlag(ITEM_FLAG_DEPRECATED) ||
        itemTemplate.HasFlag2(ITEM_FLAG2_INTERNAL_ITEM))
        return false;

    // Fixed-stat templates avoid turning a smart physical drop into a random caster suffix.
    return itemTemplate.RandomProperty == 0 && itemTemplate.RandomSuffix == 0 && !GivesNothing(itemTemplate);
}

void BuildEquipmentCatalog()
{
    if (catalogBuilt)
        return;

    catalogBuilt = true;
    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    equipmentCatalog.reserve(itemTemplates->size() / 4);
    for (auto const& [entry, itemTemplate] : *itemTemplates)
    {
        (void)entry;
        if (IsCatalogEquipment(itemTemplate))
            equipmentCatalog.push_back(&itemTemplate);
    }
}

uint32 GetPreferredArmorSubclass(Player const* player)
{
    if (player->getClass() == 10) // Oathblade: knight armor, despite Rogue combat formulas.
        return player->GetLevel() >= 40 ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;
    if (player->getClass() == 14) // Barbarian: mail from level 1, despite Rogue combat formulas (its agility stays)
        return ITEM_SUBCLASS_ARMOR_MAIL;

    // A custom class wears the armor of the class it is built on (see mod-custom-classes)
    switch (sObjectMgr->GetClassFormulaTemplate(player->getClass()))
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return ITEM_SUBCLASS_ARMOR_CLOTH;
        case CLASS_ROGUE:
        case CLASS_DRUID:
            return ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return player->GetLevel() >= 40 ? ITEM_SUBCLASS_ARMOR_MAIL : ITEM_SUBCLASS_ARMOR_LEATHER;
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
            return player->GetLevel() >= 40 ? ITEM_SUBCLASS_ARMOR_PLATE : ITEM_SUBCLASS_ARMOR_MAIL;
        case CLASS_DEATH_KNIGHT:
            return ITEM_SUBCLASS_ARMOR_PLATE;
        default:
            return ITEM_SUBCLASS_ARMOR_MISC;
    }
}

bool UsesArmorSubclass(uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
        case INVTYPE_ROBE:
            return true;
        default:
            return false;
    }
}

int32 GetStatValue(ItemTemplate const& itemTemplate, ItemModType statType)
{
    int32 value = 0;
    for (uint32 index = 0; index < itemTemplate.StatsCount; ++index)
        if (itemTemplate.ItemStat[index].ItemStatType == statType)
            value += itemTemplate.ItemStat[index].ItemStatValue;
    return value;
}

int32 GetPhysicalStatScore(ItemTemplate const& itemTemplate, uint8 classId)
{
    int32 const agility = GetStatValue(itemTemplate, ITEM_MOD_AGILITY);
    int32 const strength = GetStatValue(itemTemplate, ITEM_MOD_STRENGTH);
    int32 const stamina = GetStatValue(itemTemplate, ITEM_MOD_STAMINA);
    int32 const attackPower = GetStatValue(itemTemplate, ITEM_MOD_ATTACK_POWER);
    int32 const rangedAttackPower = GetStatValue(itemTemplate, ITEM_MOD_RANGED_ATTACK_POWER);
    int32 score = stamina * 2 + attackPower + rangedAttackPower;

    // A custom class values the stats of the class it is built on (see mod-custom-classes)
    switch (classId == 10 ? CLASS_WARRIOR : sObjectMgr->GetClassFormulaTemplate(classId))
    {
        case CLASS_ROGUE:
        case CLASS_HUNTER:
            score += agility * 6 + strength;
            break;
        default:
            score += strength * 6 + agility * 2;
            break;
    }

    score += GetStatValue(itemTemplate, ITEM_MOD_HIT_RATING) * 3;
    score += GetStatValue(itemTemplate, ITEM_MOD_CRIT_RATING) * 3;
    score += GetStatValue(itemTemplate, ITEM_MOD_HASTE_RATING) * 3;
    score += GetStatValue(itemTemplate, ITEM_MOD_EXPERTISE_RATING) * 3;
    score += GetStatValue(itemTemplate, ITEM_MOD_ARMOR_PENETRATION_RATING) * 3;
    return score;
}

int32 GetCasterStatScore(ItemTemplate const& itemTemplate)
{
    return GetStatValue(itemTemplate, ITEM_MOD_INTELLECT) * 5 +
        GetStatValue(itemTemplate, ITEM_MOD_SPIRIT) * 3 +
        GetStatValue(itemTemplate, ITEM_MOD_SPELL_POWER) * 4 +
        GetStatValue(itemTemplate, ITEM_MOD_MANA_REGENERATION) * 4 +
        GetStatValue(itemTemplate, ITEM_MOD_HIT_SPELL_RATING) * 3 +
        GetStatValue(itemTemplate, ITEM_MOD_CRIT_SPELL_RATING) * 3 +
        GetStatValue(itemTemplate, ITEM_MOD_HASTE_SPELL_RATING) * 3;
}

// A custom class is physical or caster as the class it is built on (see mod-custom-classes)
bool IsPhysicalClass(uint8 classId)
{
    uint8 const baseClass = sObjectMgr->GetClassFormulaTemplate(classId);
    return baseClass == CLASS_WARRIOR || baseClass == CLASS_ROGUE || baseClass == CLASS_HUNTER ||
        baseClass == CLASS_DEATH_KNIGHT;
}

bool IsCasterClass(uint8 classId)
{
    uint8 const baseClass = sObjectMgr->GetClassFormulaTemplate(classId);
    return baseClass == CLASS_MAGE || baseClass == CLASS_PRIEST || baseClass == CLASS_WARLOCK;
}

// What a shield fighter's defensive stats are worth: a tank's on every piece; the Gladiateur's on its shield only,
// whose defense, dodge, parry and block ratings become critical strike rating and whose block value its weapon blows
// share (mod-warrior)
int32 GetShieldStatScore(ItemTemplate const& itemTemplate, Player const* player)
{
    if (!FightsWithShield(player))
        return 0;
    bool const gladiator = player->getClass() == CLASS_WARRIOR && SpecIndex(player) == WarriorGladiatorSpec;
    if (gladiator && itemTemplate.InventoryType != INVTYPE_SHIELD)
        return 0;
    int32 const ratings = GetStatValue(itemTemplate, ITEM_MOD_DEFENSE_SKILL_RATING) +
        GetStatValue(itemTemplate, ITEM_MOD_DODGE_RATING) + GetStatValue(itemTemplate, ITEM_MOD_PARRY_RATING) +
        GetStatValue(itemTemplate, ITEM_MOD_BLOCK_RATING);
    return ratings * 3 + GetStatValue(itemTemplate, ITEM_MOD_BLOCK_VALUE) +
        (itemTemplate.InventoryType == INVTYPE_SHIELD ? int32(itemTemplate.Block) : 0);
}

int32 GetClassStatScore(ItemTemplate const& itemTemplate, Player const* player)
{
    int32 const physicalScore = GetPhysicalStatScore(itemTemplate, player->getClass()) +
        GetShieldStatScore(itemTemplate, player);
    int32 const casterScore = GetCasterStatScore(itemTemplate);

    if (IsPhysicalClass(player->getClass()))
        return physicalScore - casterScore * 4;
    if (IsCasterClass(player->getClass()))
        return casterScore - physicalScore * 3;
    return std::max(physicalScore, casterScore);
}

bool HasClassAppropriateStats(ItemTemplate const& itemTemplate, Player const* player)
{
    // Held in the off-hand (orbs, tomes) is caster gear, whatever its stats
    if (itemTemplate.InventoryType == INVTYPE_HOLDABLE && IsPhysicalClass(player->getClass()))
        return false;

    // Judged on the primary stats only. Stamina suits every class, and hit, crit and haste rating are shared by
    // casters and fighters in this version: a spell power cloak with crit rating is still a caster's cloak.
    int32 const spellPower = GetStatValue(itemTemplate, ITEM_MOD_SPELL_POWER);
    int32 const intellect = GetStatValue(itemTemplate, ITEM_MOD_INTELLECT);
    int32 const physicalPrimary = GetStatValue(itemTemplate, ITEM_MOD_STRENGTH) +
        GetStatValue(itemTemplate, ITEM_MOD_AGILITY) + GetStatValue(itemTemplate, ITEM_MOD_ATTACK_POWER) +
        GetStatValue(itemTemplate, ITEM_MOD_RANGED_ATTACK_POWER) +
        GetStatValue(itemTemplate, ITEM_MOD_EXPERTISE_RATING) +
        GetStatValue(itemTemplate, ITEM_MOD_ARMOR_PENETRATION_RATING);

    // A fighter never takes spell power, nor intellect without a fighter's stat beside it (a hunter's agility and
    // intellect mail is theirs; a caster's intellect cloak is not)
    if (IsPhysicalClass(player->getClass()) && (spellPower > 0 || (intellect > 0 && physicalPrimary == 0)))
        return false;
    // A caster never takes strength, agility or attack power without spell power or intellect beside it
    if (IsCasterClass(player->getClass()) && physicalPrimary > 0 && spellPower == 0 && intellect == 0)
        return false;
    return true;
}

bool HasEquipmentProficiency(ItemTemplate const& itemTemplate, Player const* player)
{
    uint32 const requiredSkill = itemTemplate.GetSkill();

    // Every equippable weapon subclass must map to a real learned weapon skill.
    // CanUseItem only checks RequiredSkill, which is normally zero on weapons.
    if (itemTemplate.Class == ITEM_CLASS_WEAPON)
        return requiredSkill != 0 && player->GetSkillValue(requiredSkill) > 0;

    // Armor without a proficiency (rings, necklaces, cloaks, trinkets and
    // class-restricted relics) is validated by BotCanUseItem below.
    return requiredSkill == 0 || player->GetSkillValue(requiredSkill) > 0;
}

std::array<uint8, 2> GetEquipmentSlots(uint32 inventoryType)
{
    switch (inventoryType)
    {
        case INVTYPE_HEAD: return { EQUIPMENT_SLOT_HEAD, NULL_SLOT };
        case INVTYPE_NECK: return { EQUIPMENT_SLOT_NECK, NULL_SLOT };
        case INVTYPE_SHOULDERS: return { EQUIPMENT_SLOT_SHOULDERS, NULL_SLOT };
        case INVTYPE_CHEST:
        case INVTYPE_ROBE: return { EQUIPMENT_SLOT_CHEST, NULL_SLOT };
        case INVTYPE_WAIST: return { EQUIPMENT_SLOT_WAIST, NULL_SLOT };
        case INVTYPE_LEGS: return { EQUIPMENT_SLOT_LEGS, NULL_SLOT };
        case INVTYPE_FEET: return { EQUIPMENT_SLOT_FEET, NULL_SLOT };
        case INVTYPE_WRISTS: return { EQUIPMENT_SLOT_WRISTS, NULL_SLOT };
        case INVTYPE_HANDS: return { EQUIPMENT_SLOT_HANDS, NULL_SLOT };
        case INVTYPE_FINGER: return { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 };
        case INVTYPE_TRINKET: return { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 };
        case INVTYPE_CLOAK: return { EQUIPMENT_SLOT_BACK, NULL_SLOT };
        case INVTYPE_WEAPON: return { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND };
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_2HWEAPON: return { EQUIPMENT_SLOT_MAINHAND, NULL_SLOT };
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_SHIELD:
        case INVTYPE_HOLDABLE: return { EQUIPMENT_SLOT_OFFHAND, NULL_SLOT };
        case INVTYPE_RANGED:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_RELIC: return { EQUIPMENT_SLOT_RANGED, NULL_SLOT };
        default: return { NULL_SLOT, NULL_SLOT };
    }
}

bool IsOneHandWeapon(uint32 inventoryType)
{
    return inventoryType == INVTYPE_WEAPON || inventoryType == INVTYPE_WEAPONMAINHAND ||
        inventoryType == INVTYPE_WEAPONOFFHAND;
}

bool IsHandHeld(uint32 inventoryType)
{
    return IsOneHandWeapon(inventoryType) || inventoryType == INVTYPE_2HWEAPON || inventoryType == INVTYPE_SHIELD ||
        inventoryType == INVTYPE_HOLDABLE;
}

// Weapons follow how the player fights: a two-hander wielder gets two-handers (nothing for the off hand), a one-hander
// wielder no two-handers, and the off hand gets more of what it already holds (a shield, a weapon, a held item). An
// empty off hand beside a one-hander takes a weapon when the player can dual wield, a shield or held item otherwise.
// Without this, the empty off hand of a two-hander wielder scored as a missing slot and off-hands flooded the loot.
bool FitsWeaponStyle(Player const* player, ItemTemplate const& candidate)
{
    uint32 const type = candidate.InventoryType;
    // Oathblade's techniques require a one-handed sword. Do not offer two-handers,
    // daggers, axes or shields simply because the inherited rogue proficiencies allow them.
    if (player->getClass() == 10 && (candidate.Class == ITEM_CLASS_WEAPON || IsHandHeld(type)))
        return candidate.Class == ITEM_CLASS_WEAPON &&
            candidate.SubClass == ITEM_SUBCLASS_WEAPON_SWORD && IsOneHandWeapon(type);
    if (!IsHandHeld(type))
        return true;
    // A shield fighter: one-handers and shields, whatever it holds now
    if (FightsWithShield(player))
        return type == INVTYPE_WEAPON || type == INVTYPE_WEAPONMAINHAND || type == INVTYPE_SHIELD;

    Item const* mainHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand)
        return true;

    // Titan's Grip wields two-handers in both hands: a two-hander fits either way
    bool const titanGrip = player->CanTitanGrip();
    if (mainHand->GetTemplate()->InventoryType == INVTYPE_2HWEAPON && !titanGrip)
        return type == INVTYPE_2HWEAPON;

    if (type == INVTYPE_2HWEAPON)
        return titanGrip;
    if (type == INVTYPE_WEAPON || type == INVTYPE_WEAPONMAINHAND)
        return true;

    // What goes in the off hand only
    Item const* offHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    uint32 const held = offHand ? offHand->GetTemplate()->InventoryType : INVTYPE_NON_EQUIP;
    if (held == INVTYPE_SHIELD || held == INVTYPE_HOLDABLE)
        return type == held;
    if (IsOneHandWeapon(held) || held == INVTYPE_2HWEAPON)
        return type == INVTYPE_WEAPONOFFHAND;
    return player->CanDualWield() ? type == INVTYPE_WEAPONOFFHAND : type != INVTYPE_WEAPONOFFHAND;
}

bool IsInCorpseLoot(Loot const& loot, uint32 itemId)
{
    return std::any_of(loot.items.begin(), loot.items.end(), [itemId](LootItem const& lootItem)
    {
        return !lootItem.is_looted && lootItem.itemid == itemId;
    });
}

// A replacement keeps the item level of the item that dropped: at most this many item levels below it, never
// above. Which raid or dungeon it came from still decides how good the loot is; only the class fit changes.
constexpr uint32 ReplacementItemLevelWindow = 6;

// The stock item a generated one (a Mythic+ variant, a forged rank) was made from; the entry itself for a stock one
uint32 BaseItemEntry(uint32 entry)
{
    return Mythic::IsGeneratedItem(entry) ? entry % Mythic::GeneratedItemBase : entry;
}

// Whether the item is the candidate, as itself or as a generated variant of it, at the item level the player would get
// or better: a copy the loot would not improve on
bool IsSameItem(Item const* item, uint32 baseEntry, uint32 givenItemLevel)
{
    return item && BaseItemEntry(item->GetEntry()) == baseEntry && item->GetTemplate()->ItemLevel >= givenItemLevel;
}

// Whether the player owns the candidate already at the item level it would be given at or better, as itself or as any
// generated variant of it (worn, in the bags or the bank), or could not wear it beside what it wears (a unique ring or
// trinket, a unique category). A lower copy does not count: a slot with a single item for the class (a rogue's leather
// agility boots near the top: Frostbitten Fur Boots alone) was never offered its better variants once the stock item
// was worn, and stayed the weakest slot for good while the loot went elsewhere. A unique item with a lower copy worn
// takes that copy's place (it is put on in its slot, which the unique rule allows): any copy owned used to rule it out,
// and a player wearing a trinket was never given it again, however far above it the loot was.
bool OwnsOrCannotWear(Player* player, ItemTemplate const& candidate, uint32 givenItemLevel)
{
    uint32 const baseEntry = BaseItemEntry(candidate.ItemId);
    uint8 wornCopySlot = NULL_SLOT;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            item && BaseItemEntry(item->GetEntry()) == baseEntry)
            wornCopySlot = slot;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (IsSameItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), baseEntry, givenItemLevel))
            return true;
    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        if (IsSameItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot), baseEntry, givenItemLevel))
            return true;
    std::array<std::pair<uint8, uint8>, 2> const bagRanges = { {
        { static_cast<uint8>(INVENTORY_SLOT_BAG_START), static_cast<uint8>(INVENTORY_SLOT_BAG_END) },
        { static_cast<uint8>(BANK_SLOT_BAG_START), static_cast<uint8>(BANK_SLOT_BAG_END) } } };
    for (auto const& [start, end] : bagRanges)
    {
        for (uint8 bagIndex = start; bagIndex < end; ++bagIndex)
            if (Bag const* bag = player->GetBagByPos(bagIndex))
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                    if (IsSameItem(bag->GetItemByPos(static_cast<uint8>(slot)), baseEntry, givenItemLevel))
                        return true;
    }
    return player->CanEquipUniqueItem(&candidate, wornCopySlot) != EQUIP_ERR_OK;
}

// Whether the player could and would wear the candidate: its class, armor, stats, weapon skill and way of fighting,
// and not something it owns already at the item level it would get (givenItemLevel) or better. The item level, quality
// and required level are each selector's own.
bool SuitsPlayer(Player* player, ItemTemplate const& candidate, uint32 givenItemLevel)
{
    if (player->BotCanUseItem(&candidate) != EQUIP_ERR_OK || !HasEquipmentProficiency(candidate, player) ||
        !HasClassAppropriateStats(candidate, player) || !FitsWeaponStyle(player, candidate))
        return false;
    if (candidate.Class == ITEM_CLASS_ARMOR && UsesArmorSubclass(candidate.InventoryType) &&
        candidate.SubClass != GetPreferredArmorSubclass(player))
        return false;
    return !OwnsOrCannotWear(player, candidate, givenItemLevel);
}

// Loot is fitted slot first: the slot the player is furthest behind in is chosen, then the best item for it. Picking
// among the best items of every slot at once let a slot with few items at that item level (agility cloaks near the
// top) lose to one with many (trinkets), and a player with 300+ trinkets and a 272 cloak was given a trinket.
// Rings, trinkets and the two hands are replaced at their weakest: a group of slots, behind by its weakest member.
struct SlotGroup
{
    char const* name;
    std::array<uint8, 2> slots;
    uint32 weakestItemLevel; // 0 when a slot of it is empty
};

// Groups this many item levels or less apart are equally behind (less than one key level): which of them comes first
// is drawn at random, so loot does not always go to the same slot when several are about as behind
constexpr uint32 SlotTieItemLevels = 3;

std::vector<SlotGroup> BuildSlotGroups(Player const* player)
{
    std::vector<SlotGroup> groups = {
        { "Head", { EQUIPMENT_SLOT_HEAD, NULL_SLOT }, 0 },
        { "Neck", { EQUIPMENT_SLOT_NECK, NULL_SLOT }, 0 },
        { "Shoulders", { EQUIPMENT_SLOT_SHOULDERS, NULL_SLOT }, 0 },
        { "Back", { EQUIPMENT_SLOT_BACK, NULL_SLOT }, 0 },
        { "Chest", { EQUIPMENT_SLOT_CHEST, NULL_SLOT }, 0 },
        { "Wrists", { EQUIPMENT_SLOT_WRISTS, NULL_SLOT }, 0 },
        { "Hands", { EQUIPMENT_SLOT_HANDS, NULL_SLOT }, 0 },
        { "Waist", { EQUIPMENT_SLOT_WAIST, NULL_SLOT }, 0 },
        { "Legs", { EQUIPMENT_SLOT_LEGS, NULL_SLOT }, 0 },
        { "Feet", { EQUIPMENT_SLOT_FEET, NULL_SLOT }, 0 },
        { "Rings", { EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_FINGER2 }, 0 },
        { "Trinkets", { EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_TRINKET2 }, 0 },
        { "Main hand", { EQUIPMENT_SLOT_MAINHAND, NULL_SLOT }, 0 },
        { "Ranged", { EQUIPMENT_SLOT_RANGED, NULL_SLOT }, 0 },
    };

    // No off hand beside a two-hander, unless Titan's Grip wields a second one (as FitsWeaponStyle), or the player
    // fights with a shield (its shield is still to come)
    Item const* mainHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!mainHand || mainHand->GetTemplate()->InventoryType != INVTYPE_2HWEAPON || player->CanTitanGrip() ||
        FightsWithShield(player))
        groups.push_back({ "Off hand", { EQUIPMENT_SLOT_OFFHAND, NULL_SLOT }, 0 });

    for (SlotGroup& group : groups)
    {
        uint32 weakest = std::numeric_limits<uint32>::max();
        for (uint8 slot : group.slots)
        {
            if (slot == NULL_SLOT)
                continue;
            Item const* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            weakest = std::min(weakest, equipped ? equipped->GetTemplate()->ItemLevel : 0u);
        }
        group.weakestItemLevel = weakest;
    }
    return groups;
}

// Whether an item of that inventory type goes in the group's slots
bool GroupTakes(Player const* player, SlotGroup const& group, uint32 inventoryType)
{
    switch (group.slots[0])
    {
        case EQUIPMENT_SLOT_MAINHAND:
            return inventoryType == INVTYPE_WEAPON || inventoryType == INVTYPE_WEAPONMAINHAND ||
                inventoryType == INVTYPE_2HWEAPON;
        case EQUIPMENT_SLOT_OFFHAND:
            // A shield fighter's off hand takes a shield only
            if (FightsWithShield(player))
                return inventoryType == INVTYPE_SHIELD;
            return inventoryType == INVTYPE_WEAPONOFFHAND || inventoryType == INVTYPE_SHIELD ||
                inventoryType == INVTYPE_HOLDABLE || (inventoryType == INVTYPE_WEAPON && player->CanDualWield()) ||
                (inventoryType == INVTYPE_2HWEAPON && player->CanTitanGrip());
        default:
            return GetEquipmentSlots(inventoryType)[0] == group.slots[0];
    }
}

// The groups, the furthest behind first; groups about as behind (SlotTieItemLevels) in a random order
std::vector<size_t> OrderSlotGroups(std::vector<SlotGroup> const& groups)
{
    std::vector<size_t> remaining(groups.size());
    for (size_t index = 0; index < groups.size(); ++index)
        remaining[index] = index;

    std::vector<size_t> order;
    order.reserve(groups.size());
    while (!remaining.empty())
    {
        uint32 weakest = std::numeric_limits<uint32>::max();
        for (size_t index : remaining)
            weakest = std::min(weakest, groups[index].weakestItemLevel);

        std::vector<size_t> tied;
        for (size_t index : remaining)
            if (groups[index].weakestItemLevel <= weakest + SlotTieItemLevels)
                tied.push_back(index);

        size_t const chosen = tied[urand(0, static_cast<uint32>(tied.size() - 1))];
        order.push_back(chosen);
        remaining.erase(std::find(remaining.begin(), remaining.end(), chosen));
    }
    return order;
}

struct SlotCandidate
{
    ItemTemplate const* itemTemplate; // what the selector returns (for a Mythic+ reward, the base of the variant)
    uint32 givenItemLevel;            // the item level the player gets
    int32 score;                      // the class stat score of what the player gets
    uint32 tier;                      // the narrowest search window it is in: the first ones are tried first
    uint32 groupMask;                 // the slot groups it goes in
};

// Every catalog item the selector's `tierOf` accepts (its item level, quality and required level windows: the
// narrowest one it is in, or a negative value) and that suits the player, with the slot groups it goes in.
// `givenOf` returns what the player really gets of it (a generated variant for a Mythic+ reward above the game's
// best items), nullptr when it cannot be given.
template <typename TierOf, typename GivenOf>
std::vector<SlotCandidate> CollectSlotCandidates(Player* player, std::vector<SlotGroup> const& groups, TierOf tierOf,
    GivenOf givenOf)
{
    std::vector<SlotCandidate> candidates;
    for (ItemTemplate const* candidate : equipmentCatalog)
    {
        int32 const tier = tierOf(*candidate);
        if (tier < 0)
            continue;

        uint32 groupMask = 0;
        for (size_t index = 0; index < groups.size(); ++index)
            if (GroupTakes(player, groups[index], candidate->InventoryType))
                groupMask |= 1u << index;
        if (!groupMask)
            continue;

        ItemTemplate const* given = givenOf(*candidate);
        if (!given || !SuitsPlayer(player, *candidate, given->ItemLevel))
            continue;

        candidates.push_back({ candidate, given->ItemLevel, GetClassStatScore(*given, player),
            static_cast<uint32>(tier), groupMask });
    }
    return candidates;
}

// Any of the items of about the best stat score (within a tenth of it): variety, never an item clearly worse for the
// class (a tank's strength cloak beside an agility one for a rogue). The top three alone made a slot drop the same
// few items over and over.
SlotCandidate const* PickBestCandidate(std::vector<SlotCandidate const*>& pool)
{
    std::sort(pool.begin(), pool.end(), [](SlotCandidate const* left, SlotCandidate const* right)
    {
        return left->score > right->score;
    });

    int32 const best = pool.front()->score;
    int32 const floor = best - std::abs(best) / 10;
    size_t size = 1;
    while (size < pool.size() && pool[size]->score >= floor)
        ++size;
    return pool[urand(0, static_cast<uint32>(size - 1))];
}

struct SlotPick
{
    ItemTemplate const* itemTemplate = nullptr;
    size_t group = 0;
    bool upgrade = false;
};

// Upgrades first: the groups in order, each searched in its narrowest window then wider ones before going to the
// next group, and only items above what the group's weakest slot wears. Nothing is an upgrade anywhere (a player
// better geared than the loot), unless `upgradesOnly`: something of the narrowest window, for the slot least ahead.
SlotPick PickBySlot(std::vector<SlotGroup> const& groups, std::vector<size_t> const& order,
    std::vector<SlotCandidate> const& candidates, uint32 tierCount, bool upgradesOnly)
{
    std::vector<SlotCandidate const*> pool;
    auto fill = [&](size_t group, uint32 tier, bool upgrade)
    {
        pool.clear();
        for (SlotCandidate const& candidate : candidates)
            if ((candidate.groupMask & (1u << group)) && candidate.tier <= tier &&
                (!upgrade || candidate.givenItemLevel > groups[group].weakestItemLevel))
                pool.push_back(&candidate);
        return !pool.empty();
    };

    for (size_t group : order)
        for (uint32 tier = 0; tier < tierCount; ++tier)
            if (fill(group, tier, true))
                return { PickBestCandidate(pool)->itemTemplate, group, true };

    if (upgradesOnly)
        return {};

    for (uint32 tier = 0; tier < tierCount; ++tier)
        for (size_t group : order)
            if (fill(group, tier, false))
                return { PickBestCandidate(pool)->itemTemplate, group, false };
    return {};
}

ItemTemplate const* SelectSmartReplacement(Player* player, Creature const* killed, ItemTemplate const& dropped)
{
    uint32 const quality = dropped.Quality;
    uint32 const sourceTolerance = statGrowthConfig.GetConfigValue<uint32>(
        StatGrowthConfigKey::SmartLootCreatureLevelTolerance);
    uint32 const levelWindow = statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::SmartLootLevelWindow);
    uint32 const progressionLevel = std::min<uint32>(player->GetLevel(), killed->GetLevel() + sourceTolerance);
    uint32 const minimumRequiredLevel = progressionLevel > levelWindow ? progressionLevel - levelWindow : 1;

    std::vector<SlotGroup> const groups = BuildSlotGroups(player);
    std::vector<size_t> const order = OrderSlotGroups(groups);
    std::vector<SlotCandidate> const candidates = CollectSlotCandidates(player, groups,
        [&](ItemTemplate const& candidate)
        {
            bool const fits = candidate.Quality == quality && candidate.ItemLevel <= dropped.ItemLevel &&
                candidate.ItemLevel + ReplacementItemLevelWindow >= dropped.ItemLevel &&
                candidate.RequiredLevel <= progressionLevel && candidate.RequiredLevel >= minimumRequiredLevel &&
                !IsInCorpseLoot(killed->loot, candidate.ItemId);
            return fits ? 0 : -1;
        },
        [](ItemTemplate const& candidate) { return &candidate; });

    // No upgrade: the drop stays as it is, unless it does not suit the owner - the wrong stats (a spell power cloak
    // for a rogue), armor they do not wear, or how they fight (an off-hand for a two-hander wielder) - or its stats
    // are a random suffix, which may be anyone's. It is then swapped for the best item that does suit them.
    bool const suits = dropped.RandomProperty == 0 && dropped.RandomSuffix == 0 &&
        HasClassAppropriateStats(dropped, player) && FitsWeaponStyle(player, dropped) &&
        player->BotCanUseItem(&dropped) == EQUIP_ERR_OK && HasEquipmentProficiency(dropped, player) &&
        !(dropped.Class == ITEM_CLASS_ARMOR && UsesArmorSubclass(dropped.InventoryType) &&
            dropped.SubClass != GetPreferredArmorSubclass(player));
    return PickBySlot(groups, order, candidates, 1, suits).itemTemplate;
}

void ReplaceLootItem(LootItem& lootItem, ItemTemplate const& replacement)
{
    lootItem.itemid = replacement.ItemId;
    lootItem.randomSuffix = GenerateEnchSuffixFactor(replacement.ItemId);
    lootItem.randomPropertyId = Item::GenerateItemRandomPropertyId(replacement.ItemId);
    lootItem.count = 1;
    lootItem.conditions.clear();
    lootItem.allowedGUIDs.clear();
    lootItem.rollWinnerGUID = ObjectGuid::Empty;
    lootItem.freeforall = replacement.HasFlag(ITEM_FLAG_MULTI_DROP);
    lootItem.follow_loot_rules = replacement.HasFlagCu(ITEM_FLAGS_CU_FOLLOW_LOOT_RULES);
    lootItem.needs_quest = false;
}

ItemTemplate const* GetEquipmentLootTemplate(LootItem const& lootItem)
{
    if (lootItem.is_looted || lootItem.needs_quest)
        return nullptr;

    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(lootItem.itemid);
    if (!itemTemplate || (itemTemplate->Class != ITEM_CLASS_WEAPON && itemTemplate->Class != ITEM_CLASS_ARMOR) ||
        !IsEquipmentInventoryType(itemTemplate->InventoryType))
        return nullptr;

    return itemTemplate;
}

// Whom the drop is fitted to: whoever tagged the creature, except that bots are never dressed by it. In a group
// holding real players, one of those near the creature (a random one per item when several are there) takes it,
// or a group with bots would loot shields and off-hands made for its bot tank.
Player* PickLootOwner(Player* player, Creature* killed)
{
    Player* owner = killed->GetLootRecipient() ? killed->GetLootRecipient() : player;
    Group* group = killed->GetLootRecipientGroup() ? killed->GetLootRecipientGroup() : owner->GetGroup();
    if (!group)
        return owner;

    std::vector<Player*> players;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && !member->GetSession()->IsBot() && member->IsInMap(killed))
            players.push_back(member);

    return players.empty() ? owner : players[urand(0, static_cast<uint32>(players.size() - 1))];
}

// The Mythic+ reward's item level windows below the run's (capped) item level, the narrowest first: the slot the
// player most needs is searched down to the widest before another slot is
constexpr std::array<uint32, 4> MythicItemLevelWindows = { ReplacementItemLevelWindow, 13, 26, 60 };

SlotPick SelectMythicLoot(Player* player, uint32 itemLevel, uint32 givenItemLevel, std::vector<SlotGroup>& groups,
    std::vector<size_t>& order, uint8 equipmentSlot = NULL_SLOT)
{
    BuildEquipmentCatalog();
    uint32 const progressionLevel = player->GetLevel();
    // Above the game's best items the player gets the generated variant of the pick (GiveMythicItem): only items that
    // have one can be given, or the player would get the stock item, far below the reward
    bool const generated = givenItemLevel > itemLevel;
    uint32 const variant = Mythic::GetGeneratedVariant(givenItemLevel);

    groups = BuildSlotGroups(player);
    order = OrderSlotGroups(groups);
    // A slot chosen (the Front du Nord's quartermaster): its group alone, upgrade or not
    if (equipmentSlot != NULL_SLOT)
        std::erase_if(order, [&groups, equipmentSlot](size_t group)
            { return std::ranges::find(groups[group].slots, equipmentSlot) == groups[group].slots.end(); });
    std::vector<SlotCandidate> const candidates = CollectSlotCandidates(player, groups,
        [&](ItemTemplate const& candidate)
        {
            if (candidate.Quality != ITEM_QUALITY_EPIC || candidate.ItemLevel > itemLevel ||
                candidate.RequiredLevel > progressionLevel)
                return -1;
            // A generated reward is grown to its own item level from any base (MythicItemGeneration): every base is
            // as good, none comes first. The windows put the top tier first, and it was all that ever dropped.
            if (generated)
                return 0;
            for (size_t tier = 0; tier < MythicItemLevelWindows.size(); ++tier)
                if (candidate.ItemLevel + MythicItemLevelWindows[tier] >= itemLevel)
                    return static_cast<int32>(tier);
            return -1;
        },
        [&](ItemTemplate const& candidate) -> ItemTemplate const*
        {
            if (!generated)
                return &candidate;
            return sObjectMgr->GetItemTemplate(Mythic::GetGeneratedItemEntry(candidate.ItemId, variant));
        });
    return PickBySlot(groups, order, candidates, static_cast<uint32>(MythicItemLevelWindows.size()), false);
}

using namespace Acore::ChatCommands;

// .lootdebug [item level] [name] [slot]: the named player's (else the selected one's, else your own) slot groups, the
// furthest behind first, and what a Mythic+ reward of that item level (the game's best by default) would give them -
// for that equipment slot when one is given (15 main hand, 16 off hand), as .testprofile picks - without giving it
class SmartLootCommandScript : public CommandScript
{
public:
    SmartLootCommandScript() : CommandScript("SmartLootCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "lootdebug", HandleLootDebug, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    static bool HandleLootDebug(ChatHandler* handler, Optional<uint32> rewardItemLevel,
                                Optional<PlayerIdentifier> target, Optional<uint8> equipmentSlot)
    {
        Player* player = target ? target->GetConnectedPlayer() : handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;

        // As GiveMythicItem (MythicDungeonSystem.cpp)
        uint32 const reward = rewardItemLevel.value_or(Mythic::MaxItemLevel);
        uint32 const itemLevel = std::min(reward, Mythic::MaxItemLevel);
        uint32 const given = reward > Mythic::MaxItemLevel ?
            Mythic::GetGeneratedItemLevel(Mythic::GetGeneratedVariant(reward)) : 0;

        std::vector<SlotGroup> groups;
        std::vector<size_t> order;
        SlotPick const pick = equipmentSlot ?
            SelectMythicLoot(player, itemLevel, given, groups, order, *equipmentSlot) :
            SelectMythicLoot(player, itemLevel, given, groups, order);

        uint32 const reference = given ? given : itemLevel;
        handler->PSendSysMessage("Loot debug for {}: reward item level {} (catalog up to {}, given at {}).",
            player->GetName(), reward, itemLevel, reference);
        for (size_t rank = 0; rank < order.size(); ++rank)
        {
            SlotGroup const& group = groups[order[rank]];
            handler->PSendSysMessage("  {}. {}: weakest {}, deficit {}", rank + 1, group.name,
                group.weakestItemLevel, static_cast<int32>(reference) - static_cast<int32>(group.weakestItemLevel));
        }

        if (!pick.itemTemplate)
        {
            handler->SendSysMessage("  Pick: nothing fits.");
            return true;
        }

        handler->PSendSysMessage("  Pick: {} ({}, item level {}) for {}, given at {}{}.", pick.itemTemplate->Name1,
            pick.itemTemplate->ItemId, pick.itemTemplate->ItemLevel, groups[pick.group].name,
            given ? given : pick.itemTemplate->ItemLevel, pick.upgrade ? "" : " (no upgrade anywhere)");
        return true;
    }
};
}

void ImproveBaseEquipmentLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) ||
        !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::SmartLootEnabled) || !player || !killed ||
        killed->IsPet() || killed->IsTotem() || killed->GetCreatureTemplate()->type == CREATURE_TYPE_CRITTER)
        return;

    BuildEquipmentCatalog();
    for (LootItem& lootItem : killed->loot.items)
    {
        // Poor and common equipment stays as dropped; only uncommon or better drops are made class-appropriate
        ItemTemplate const* original = GetEquipmentLootTemplate(lootItem);
        if (!original || original->Quality < ITEM_QUALITY_UNCOMMON || original->Quality > ITEM_QUALITY_EPIC)
            continue;

        if (ItemTemplate const* replacement =
                SelectSmartReplacement(PickLootOwner(player, killed), killed, *original))
            ReplaceLootItem(lootItem, *replacement);
    }
}

// A mythic boss gives each real player one epic of the run's item level, fitted to their class the same way as
// Smart Loot replacements: for the slot they are furthest behind in (PickBySlot), from just below that item level
// (wider when the game has no items there for that slot, as between raid tiers). `givenItemLevel` is what the player
// really gets (above the game's best items, the generated variant of the pick), which is what "behind" is measured
// against. A player whose gear is already better everywhere still gets something of the right item level.
ItemTemplate const* SelectMythicLootItem(Player* player, uint32 itemLevel, uint32 givenItemLevel, uint8 equipmentSlot)
{
    if (!player)
        return nullptr;

    std::vector<SlotGroup> groups;
    std::vector<size_t> order;
    return SelectMythicLoot(player, itemLevel, givenItemLevel, groups, order, equipmentSlot).itemTemplate;
}

// One of the items given (the profiles of one set piece, mod-legendary SetPieces.cpp: each a raid item of one slot
// grown to the set's item level) for the player, judged as a Mythic+ reward's pick is: the ones that suit them - their
// class, proficiency, stats and way of fighting - and among them any of about the best stat score (PickBestCandidate).
// When none suits them, the best scored one if `anyway`, else nullptr.
ItemTemplate const* SelectSuitedItem(Player* player, std::vector<ItemTemplate const*> const& items, bool anyway)
{
    if (!player || items.empty())
        return nullptr;

    std::vector<SlotCandidate> suited;
    std::vector<SlotCandidate> all;
    for (ItemTemplate const* item : items)
    {
        if (!item)
            continue;
        SlotCandidate const candidate = { item, item->ItemLevel, GetClassStatScore(*item, player), 0, 1 };
        all.push_back(candidate);
        // Of one armour type already, the caller's choice (a set of the looter's own)
        if (player->BotCanUseItem(item) == EQUIP_ERR_OK && HasEquipmentProficiency(*item, player) &&
            HasClassAppropriateStats(*item, player) && FitsWeaponStyle(player, *item))
            suited.push_back(candidate);
    }
    std::vector<SlotCandidate> const& from = suited.empty() ? all : suited;
    if (from.empty() || (suited.empty() && !anyway))
        return nullptr;

    std::vector<SlotCandidate const*> pool;
    for (SlotCandidate const& candidate : from)
        pool.push_back(&candidate);
    return PickBestCandidate(pool)->itemTemplate;
}

// The Infinite Dungeon's gear while levelling (InfiniteDungeonSystem.cpp): an item of the quality asked for, made for
// the player's level - the closest required levels first, widening only when the game has nothing there - fitted to
// its class, armour and weapon style like a Smart Loot replacement, for the slot it is furthest behind in.
ItemTemplate const* SelectLevelLootItem(Player* player, uint32 quality)
{
    if (!player)
        return nullptr;

    BuildEquipmentCatalog();
    uint32 const level = player->GetLevel();
    constexpr std::array<uint32, 3> requiredLevelWindows = { 4, 8, 16 };

    std::vector<SlotGroup> const groups = BuildSlotGroups(player);
    std::vector<size_t> const order = OrderSlotGroups(groups);
    std::vector<SlotCandidate> const candidates = CollectSlotCandidates(player, groups,
        [&](ItemTemplate const& candidate)
        {
            if (candidate.Quality != quality || candidate.RequiredLevel == 0 || candidate.RequiredLevel > level)
                return -1;
            for (size_t tier = 0; tier < requiredLevelWindows.size(); ++tier)
                if (candidate.RequiredLevel + requiredLevelWindows[tier] >= level)
                    return static_cast<int32>(tier);
            return -1;
        },
        [](ItemTemplate const& candidate) { return &candidate; });
    return PickBySlot(groups, order, candidates, static_cast<uint32>(requiredLevelWindows.size()), false)
        .itemTemplate;
}

void AddSmartLootScripts()
{
    new SmartLootCommandScript();
}
