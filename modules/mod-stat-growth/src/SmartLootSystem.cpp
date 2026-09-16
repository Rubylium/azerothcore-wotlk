#include "SmartLootSystem.h"

#include "Creature.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "StatGrowthConfig.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
struct SmartLootCandidate
{
    ItemTemplate const* itemTemplate;
    int32 score;
};

std::vector<ItemTemplate const*> equipmentCatalog;
bool catalogBuilt = false;

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

bool IsCatalogEquipment(ItemTemplate const& itemTemplate)
{
    if ((itemTemplate.Class != ITEM_CLASS_WEAPON && itemTemplate.Class != ITEM_CLASS_ARMOR) ||
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
    return itemTemplate.RandomProperty == 0 && itemTemplate.RandomSuffix == 0;
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
    switch (player->getClass())
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

    switch (classId)
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

bool IsPhysicalClass(uint8 classId)
{
    return classId == CLASS_WARRIOR || classId == CLASS_ROGUE || classId == CLASS_HUNTER ||
        classId == CLASS_DEATH_KNIGHT;
}

bool IsCasterClass(uint8 classId)
{
    return classId == CLASS_MAGE || classId == CLASS_PRIEST || classId == CLASS_WARLOCK;
}

int32 GetClassStatScore(ItemTemplate const& itemTemplate, Player const* player)
{
    int32 const physicalScore = GetPhysicalStatScore(itemTemplate, player->getClass());
    int32 const casterScore = GetCasterStatScore(itemTemplate);

    if (IsPhysicalClass(player->getClass()))
        return physicalScore - casterScore * 4;
    if (IsCasterClass(player->getClass()))
        return casterScore - physicalScore * 3;
    return std::max(physicalScore, casterScore);
}

bool HasClassAppropriateStats(ItemTemplate const& itemTemplate, Player const* player)
{
    int32 const physicalScore = GetPhysicalStatScore(itemTemplate, player->getClass());
    int32 const casterScore = GetCasterStatScore(itemTemplate);

    if (IsPhysicalClass(player->getClass()) && casterScore > 0 && physicalScore == 0)
        return false;
    if (IsCasterClass(player->getClass()) && physicalScore > 0 && casterScore == 0)
        return false;
    return true;
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

uint32 GetWeakestEquippedItemLevel(Player const* player, ItemTemplate const& candidate)
{
    std::array<uint8, 2> const slots = GetEquipmentSlots(candidate.InventoryType);
    uint32 weakestItemLevel = std::numeric_limits<uint32>::max();
    bool hasSlot = false;
    for (uint8 slot : slots)
    {
        if (slot == NULL_SLOT)
            continue;
        if (slot == EQUIPMENT_SLOT_OFFHAND && candidate.InventoryType == INVTYPE_WEAPON &&
            !player->CanDualWield())
            continue;

        hasSlot = true;
        Item const* equipped = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!equipped)
            return 0;
        weakestItemLevel = std::min(weakestItemLevel, equipped->GetTemplate()->ItemLevel);
    }
    return hasSlot ? weakestItemLevel : std::numeric_limits<uint32>::max();
}

bool IsInCorpseLoot(Loot const& loot, uint32 itemId)
{
    return std::any_of(loot.items.begin(), loot.items.end(), [itemId](LootItem const& lootItem)
    {
        return !lootItem.is_looted && lootItem.itemid == itemId;
    });
}

bool IsUsableCandidate(Player* player, Loot const& loot, ItemTemplate const& candidate, uint32 progressionLevel,
    uint32 minimumRequiredLevel, uint32 quality)
{
    if (candidate.RequiredLevel > progressionLevel || candidate.RequiredLevel < minimumRequiredLevel ||
        candidate.Quality != quality ||
        player->CanUseItem(&candidate) != EQUIP_ERR_OK || !HasClassAppropriateStats(candidate, player))
        return false;

    if (candidate.Class == ITEM_CLASS_ARMOR && UsesArmorSubclass(candidate.InventoryType) &&
        candidate.SubClass != GetPreferredArmorSubclass(player))
        return false;
    if (GetWeakestEquippedItemLevel(player, candidate) == std::numeric_limits<uint32>::max())
        return false;

    // Never hand out a copy of something already owned (bags and bank included) or already on this corpse
    return !IsInCorpseLoot(loot, candidate.ItemId) && !player->HasItemCount(candidate.ItemId, 1, true);
}

int32 ScoreCandidate(Player* player, ItemTemplate const& candidate, uint32 progressionLevel)
{
    uint32 const equippedItemLevel = GetWeakestEquippedItemLevel(player, candidate);
    int32 const upgrade = static_cast<int32>(candidate.ItemLevel) - static_cast<int32>(equippedItemLevel);
    int32 const missingSlotBonus = equippedItemLevel == 0 ? 1200 : 0;
    int32 const levelFit = 40 - static_cast<int32>(std::abs(
        static_cast<int32>(progressionLevel) - static_cast<int32>(candidate.RequiredLevel))) * 5;
    return missingSlotBonus + upgrade * 45 + static_cast<int32>(candidate.Quality) * 80 +
        levelFit + GetClassStatScore(candidate, player);
}

void AddCandidates(Player* player, Loot const& loot, uint32 progressionLevel, uint32 minimumRequiredLevel,
    uint32 quality, std::vector<SmartLootCandidate>& candidates)
{
    for (ItemTemplate const* candidate : equipmentCatalog)
    {
        if (!IsUsableCandidate(player, loot, *candidate, progressionLevel, minimumRequiredLevel, quality))
            continue;

        uint32 const equippedItemLevel = GetWeakestEquippedItemLevel(player, *candidate);
        if (equippedItemLevel == 0 || candidate->ItemLevel > equippedItemLevel)
            candidates.push_back({ candidate, ScoreCandidate(player, *candidate, progressionLevel) });
    }
}

ItemTemplate const* SelectSmartReplacement(Player* player, Creature const* killed, uint32 quality)
{
    uint32 const sourceTolerance = statGrowthConfig.GetConfigValue<uint32>(
        StatGrowthConfigKey::SmartLootCreatureLevelTolerance);
    uint32 const levelWindow = statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::SmartLootLevelWindow);
    uint32 const progressionLevel = std::min<uint32>(player->GetLevel(), killed->GetLevel() + sourceTolerance);
    uint32 const minimumRequiredLevel = progressionLevel > levelWindow ? progressionLevel - levelWindow : 1;

    std::vector<SmartLootCandidate> candidates;
    AddCandidates(player, killed->loot, progressionLevel, minimumRequiredLevel, quality, candidates);
    if (candidates.empty())
        return nullptr;

    std::sort(candidates.begin(), candidates.end(), [](SmartLootCandidate const& left, SmartLootCandidate const& right)
    {
        return left.score > right.score;
    });

    size_t const topPoolSize = std::min<size_t>(candidates.size(), 8);
    return candidates[urand(0, static_cast<uint32>(topPoolSize - 1))].itemTemplate;
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
}

void ImproveBaseEquipmentLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) ||
        !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::SmartLootEnabled) || !player || !killed ||
        killed->IsPet() || killed->IsTotem() || killed->GetCreatureTemplate()->type == CREATURE_TYPE_CRITTER)
        return;

    if (Player* lootRecipient = killed->GetLootRecipient())
        player = lootRecipient;

    BuildEquipmentCatalog();
    for (LootItem& lootItem : killed->loot.items)
    {
        // Poor and common equipment stays as dropped; only uncommon or better drops are made class-appropriate
        ItemTemplate const* original = GetEquipmentLootTemplate(lootItem);
        if (!original || original->Quality < ITEM_QUALITY_UNCOMMON || original->Quality > ITEM_QUALITY_EPIC)
            continue;

        if (ItemTemplate const* replacement = SelectSmartReplacement(player, killed, original->Quality))
            ReplaceLootItem(lootItem, *replacement);
    }
}

bool UpgradeLootItemQuality(Player* player, Creature* killed, LootItem& lootItem)
{
    if (!player || !killed)
        return false;

    ItemTemplate const* original = GetEquipmentLootTemplate(lootItem);
    if (!original)
        return false;

    // Poor and common equipment upgrades straight to uncommon, anything better moves up one quality up to epic
    uint32 const targetQuality = original->Quality < ITEM_QUALITY_UNCOMMON
        ? uint32(ITEM_QUALITY_UNCOMMON)
        : uint32(original->Quality) + 1;
    if (targetQuality > ITEM_QUALITY_EPIC)
        return false;

    BuildEquipmentCatalog();
    ItemTemplate const* replacement = SelectSmartReplacement(player, killed, targetQuality);
    if (!replacement)
        return false;

    ReplaceLootItem(lootItem, *replacement);
    return true;
}
