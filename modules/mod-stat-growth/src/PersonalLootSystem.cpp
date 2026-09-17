#include "PersonalLootSystem.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Creature.h"
#include "FortuneBoostSystem.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "LootMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "Random.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StatGrowthConfig.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr std::string_view AddonPrefix = "PersonalLoot";
// Custom "Vol de vie" spell (localTools/patchSinisterStrike.ps1): names the Leech heal in logs and meters
constexpr uint32 SPELL_GEAR_BONUS_LEECH = 90022;

struct AddonHandshake
{
    uint32 elapsed = 0;
    bool verified = false;
    bool warned = false;
};

std::unordered_map<ObjectGuid::LowType, PersonalLootRoll> personalLootRolls;
std::unordered_map<ObjectGuid::LowType, std::unordered_set<ObjectGuid::LowType>> equippedPersonalLoot;
std::unordered_map<ObjectGuid::LowType, std::array<uint32, 5>> equippedCustomBonuses;
std::unordered_map<ObjectGuid::LowType, AddonHandshake> addonHandshakes;

// Bonuses rolled before an item exists, so the loot window and group roll tooltips can preview them. Rolled per
// class because the winner of a group roll may be of a different class than the player hovering the item.
struct PreRollKey
{
    ObjectGuid lootGuid;
    uint32 lootIndex = 0;
    uint32 itemId = 0;
    uint8 classId = 0;

    bool operator<(PreRollKey const& other) const
    {
        return std::tie(lootGuid, lootIndex, itemId, classId) <
            std::tie(other.lootGuid, other.lootIndex, other.itemId, other.classId);
    }
};

struct PreRoll
{
    PersonalLootRoll roll;
    Seconds createdAt;
};

constexpr Seconds PreRollLifetime = 1h;
std::map<PreRollKey, PreRoll> preRolls;

std::string_view GetAffixName(PersonalLootAffix affix)
{
    switch (affix)
    {
        case PersonalLootAffix::Strength: return "Strength";
        case PersonalLootAffix::Agility: return "Agility";
        case PersonalLootAffix::Intellect: return "Intellect";
        case PersonalLootAffix::Spirit: return "Spirit";
        case PersonalLootAffix::Stamina: return "Stamina";
        case PersonalLootAffix::AttackPower: return "Attack Power";
        case PersonalLootAffix::SpellPower: return "Spell Power";
        case PersonalLootAffix::CriticalStrike: return "Critical Strike Rating";
        case PersonalLootAffix::Haste: return "Haste Rating";
        case PersonalLootAffix::Hit: return "Hit Rating";
        case PersonalLootAffix::Expertise: return "Expertise Rating";
        case PersonalLootAffix::ArmorPenetration: return "Armor Penetration Rating";
        case PersonalLootAffix::Defense: return "Defense Rating";
        case PersonalLootAffix::Dodge: return "Dodge Rating";
        case PersonalLootAffix::Parry: return "Parry Rating";
        case PersonalLootAffix::Block: return "Block Rating";
        case PersonalLootAffix::ManaRegeneration: return "Mana per 5 sec";
        case PersonalLootAffix::ExperienceGain: return "Experience gained";
        case PersonalLootAffix::ResourceRegeneration: return "Primary resource regeneration";
        case PersonalLootAffix::MaximumHealth: return "Maximum health";
        case PersonalLootAffix::Fortune: return "Fortune";
        case PersonalLootAffix::Leech: return "Leech";
        default: return "";
    }
}

bool IsPercentageAffix(PersonalLootAffix affix)
{
    return affix >= PersonalLootAffix::ExperienceGain && affix <= PersonalLootAffix::Leech;
}

uint8 GetCustomBonusIndex(PersonalLootAffix affix)
{
    return static_cast<uint8>(affix) - static_cast<uint8>(PersonalLootAffix::ExperienceGain);
}

std::vector<PersonalLootAffix> GetClassAffixPool(uint8 classId)
{
    std::vector<PersonalLootAffix> pool;
    switch (classId)
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            pool = { PersonalLootAffix::Strength, PersonalLootAffix::Stamina, PersonalLootAffix::AttackPower };
            break;
        case CLASS_PALADIN:
            pool = { PersonalLootAffix::Strength, PersonalLootAffix::Intellect, PersonalLootAffix::Stamina,
                PersonalLootAffix::AttackPower, PersonalLootAffix::SpellPower };
            break;
        case CLASS_HUNTER:
        case CLASS_ROGUE:
            pool = { PersonalLootAffix::Agility, PersonalLootAffix::Stamina, PersonalLootAffix::AttackPower };
            break;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            pool = { PersonalLootAffix::Intellect, PersonalLootAffix::Spirit, PersonalLootAffix::Stamina,
                PersonalLootAffix::SpellPower };
            break;
        case CLASS_SHAMAN:
            pool = { PersonalLootAffix::Strength, PersonalLootAffix::Agility, PersonalLootAffix::Intellect,
                PersonalLootAffix::Stamina, PersonalLootAffix::AttackPower, PersonalLootAffix::SpellPower };
            break;
        case CLASS_DRUID:
            pool = { PersonalLootAffix::Strength, PersonalLootAffix::Agility, PersonalLootAffix::Intellect,
                PersonalLootAffix::Spirit, PersonalLootAffix::Stamina, PersonalLootAffix::AttackPower,
                PersonalLootAffix::SpellPower };
            break;
        default:
            pool = { PersonalLootAffix::Stamina };
            break;
    }

    pool.insert(pool.end(), { PersonalLootAffix::ExperienceGain, PersonalLootAffix::ResourceRegeneration,
        PersonalLootAffix::MaximumHealth, PersonalLootAffix::Fortune, PersonalLootAffix::Leech });
    return pool;
}

float GetBonusChance(uint32 quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootPoorChance);
        case ITEM_QUALITY_NORMAL:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootCommonChance);
        case ITEM_QUALITY_UNCOMMON:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootUncommonChance);
        case ITEM_QUALITY_RARE:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootRareChance);
        case ITEM_QUALITY_EPIC:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootEpicChance);
        default:
            return statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootLegendaryChance);
    }
}

uint8 GetAffixCount(uint32 quality)
{
    if (quality >= ITEM_QUALITY_LEGENDARY)
        return 4;
    if (quality >= ITEM_QUALITY_EPIC)
        return 3;
    if (quality >= ITEM_QUALITY_RARE)
        return 2;
    return 1;
}

float GetQualityMultiplier(uint32 quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_UNCOMMON: return 1.15f;
        case ITEM_QUALITY_RARE: return 1.35f;
        case ITEM_QUALITY_EPIC: return 1.65f;
        case ITEM_QUALITY_LEGENDARY: return 2.0f;
        case ITEM_QUALITY_ARTIFACT:
        case ITEM_QUALITY_HEIRLOOM: return 2.25f;
        default: return 1.0f;
    }
}

float GetFortuneBonusChance(uint32 fortuneBonus)
{
    return static_cast<float>(fortuneBonus) *
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::FortuneGearBonusChancePerPoint);
}

float GetFortuneValueMultiplier(uint32 fortuneBonus)
{
    float const increase = std::min(static_cast<float>(fortuneBonus) *
            statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::FortuneGearBonusValuePerPoint),
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::FortuneMaxGearBonusValueIncrease));
    return 1.0f + increase / 100.0f;
}

uint32 RollAffixValue(ItemTemplate const* itemTemplate, PersonalLootAffix affix, float fortuneMultiplier)
{
    StatGrowthConfigKey scaleKey = StatGrowthConfigKey::PersonalLootAffixScale;
    if (affix == PersonalLootAffix::Leech)
        scaleKey = StatGrowthConfigKey::PersonalLootLeechScale;
    else if (IsPercentageAffix(affix))
        scaleKey = StatGrowthConfigKey::PersonalLootPercentScale;

    float const scale = statGrowthConfig.GetConfigValue<float>(scaleKey);
    float const baseValue = std::max<uint32>(itemTemplate->ItemLevel, 1) * scale *
        GetQualityMultiplier(itemTemplate->Quality) * fortuneMultiplier;
    return std::max<uint32>(1, std::lround(baseValue * frand(0.85f, 1.15f)));
}

std::string BuildAffixText(PersonalLootRoll const& roll)
{
    std::string text;
    for (PersonalLootAffixRoll const& affix : roll.affixes)
    {
        if (affix.type == PersonalLootAffix::None)
            continue;

        if (!text.empty())
            text += ';';
        text += GetAffixName(affix.type);
        text += '=';
        text += std::to_string(affix.value);
        text += IsPercentageAffix(affix.type) ? '%' : '+';
    }
    return text;
}

bool IsEligibleEquipmentTemplate(ItemTemplate const* itemTemplate)
{
    if (!itemTemplate || (itemTemplate->Class != ITEM_CLASS_WEAPON && itemTemplate->Class != ITEM_CLASS_ARMOR))
        return false;

    return itemTemplate->InventoryType != INVTYPE_NON_EQUIP && itemTemplate->InventoryType != INVTYPE_BAG;
}

bool IsEligibleEquipment(Item const* item)
{
    return item && item->GetCount() == 1 && IsEligibleEquipmentTemplate(item->GetTemplate());
}

void ApplyAffix(Player* player, PersonalLootAffixRoll const& affix, bool apply)
{
    uint32 const amount = affix.value;
    switch (affix.type)
    {
        case PersonalLootAffix::Strength:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, amount, apply);
            player->UpdateStatBuffMod(STAT_STRENGTH);
            break;
        case PersonalLootAffix::Agility:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, amount, apply);
            player->UpdateStatBuffMod(STAT_AGILITY);
            break;
        case PersonalLootAffix::Intellect:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, amount, apply);
            player->UpdateStatBuffMod(STAT_INTELLECT);
            break;
        case PersonalLootAffix::Spirit:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, amount, apply);
            player->UpdateStatBuffMod(STAT_SPIRIT);
            break;
        case PersonalLootAffix::Stamina:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, amount, apply);
            player->UpdateStatBuffMod(STAT_STAMINA);
            break;
        case PersonalLootAffix::AttackPower:
            player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, amount, apply);
            player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, amount, apply);
            break;
        case PersonalLootAffix::SpellPower:
            player->ApplySpellPowerBonus(amount, apply);
            break;
        case PersonalLootAffix::CriticalStrike:
            player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
            break;
        case PersonalLootAffix::Haste:
            player->ApplyRatingMod(CR_HASTE_MELEE, amount, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
            break;
        case PersonalLootAffix::Hit:
            player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
            break;
        case PersonalLootAffix::Expertise:
            player->ApplyRatingMod(CR_EXPERTISE, amount, apply);
            break;
        case PersonalLootAffix::ArmorPenetration:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, amount, apply);
            break;
        case PersonalLootAffix::Defense:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, amount, apply);
            break;
        case PersonalLootAffix::Dodge:
            player->ApplyRatingMod(CR_DODGE, amount, apply);
            break;
        case PersonalLootAffix::Parry:
            player->ApplyRatingMod(CR_PARRY, amount, apply);
            break;
        case PersonalLootAffix::Block:
            player->ApplyRatingMod(CR_BLOCK, amount, apply);
            break;
        case PersonalLootAffix::ManaRegeneration:
            player->ApplyManaRegenBonus(amount, apply);
            break;
        case PersonalLootAffix::ExperienceGain:
        case PersonalLootAffix::ResourceRegeneration:
        case PersonalLootAffix::MaximumHealth:
        case PersonalLootAffix::Fortune:
        case PersonalLootAffix::Leech:
        {
            auto& bonuses = equippedCustomBonuses[player->GetGUID().GetCounter()];
            uint32& value = bonuses[GetCustomBonusIndex(affix.type)];
            value = apply ? value + amount : (value > amount ? value - amount : 0);
            break;
        }
        default:
            break;
    }
}

PersonalLootRoll RollBonuses(Player* player, ItemTemplate const* itemTemplate)
{
    PersonalLootRoll roll;
    roll.itemQuality = itemTemplate->Quality;
    roll.rolledClass = player->getClass();

    // Fortune makes bonuses both more likely and stronger
    uint32 const fortuneBonus = GetFortuneBonus(player);
    float const fortuneMultiplier = GetFortuneValueMultiplier(fortuneBonus);
    if (roll_chance_f(std::min(GetBonusChance(itemTemplate->Quality) + GetFortuneBonusChance(fortuneBonus), 100.0f)))
    {
        std::vector<PersonalLootAffix> pool = GetClassAffixPool(roll.rolledClass);
        uint8 const affixCount = std::min<uint8>(GetAffixCount(itemTemplate->Quality), roll.affixes.size());
        for (uint8 index = 0; index < affixCount; ++index)
        {
            uint32 const poolIndex = urand(0, pool.size() - 1);
            roll.affixes[index].type = pool[poolIndex];
            roll.affixes[index].value = RollAffixValue(itemTemplate, pool[poolIndex], fortuneMultiplier);
            pool.erase(pool.begin() + poolIndex);
        }
    }

    return roll;
}

PersonalLootRoll const& GetOrCreatePreRoll(Player* player, ObjectGuid lootGuid, uint32 lootIndex,
    ItemTemplate const* itemTemplate)
{
    Seconds const now = GameTime::GetGameTime();
    std::erase_if(preRolls, [now](auto const& entry) { return now - entry.second.createdAt > PreRollLifetime; });

    PreRollKey const key { lootGuid, lootIndex, itemTemplate->ItemId, player->getClass() };
    auto itr = preRolls.find(key);
    if (itr == preRolls.end())
        itr = preRolls.emplace(key, PreRoll { RollBonuses(player, itemTemplate), now }).first;
    return itr->second.roll;
}

std::optional<PersonalLootRoll> TakePreRoll(Player* player, ObjectGuid lootGuid, int32 lootIndex, uint32 itemId)
{
    if (lootGuid.IsEmpty())
        return std::nullopt;

    for (auto itr = preRolls.begin(); itr != preRolls.end(); ++itr)
    {
        PreRollKey const& key = itr->first;
        if (key.lootGuid == lootGuid && key.itemId == itemId && key.classId == player->getClass() &&
            (lootIndex < 0 || key.lootIndex == uint32(lootIndex)))
        {
            PersonalLootRoll const roll = itr->second.roll;
            preRolls.erase(itr);
            return roll;
        }
    }

    return std::nullopt;
}

Loot* GetOpenLoot(Player* player)
{
    ObjectGuid const lootGuid = player->GetLootGUID();
    if (lootGuid.IsCreatureOrVehicle())
    {
        Creature* creature = ObjectAccessor::GetCreature(*player, lootGuid);
        return creature ? &creature->loot : nullptr;
    }
    if (lootGuid.IsGameObject())
    {
        GameObject* gameObject = ObjectAccessor::GetGameObject(*player, lootGuid);
        return gameObject ? &gameObject->loot : nullptr;
    }
    if (lootGuid.IsItem())
    {
        Item* item = player->GetItemByGuid(lootGuid);
        return item ? &item->loot : nullptr;
    }
    return nullptr;
}

bool ParseNumber(std::string_view value, uint32& result)
{
    auto const conversion = std::from_chars(value.data(), value.data() + value.size(), result);
    return conversion.ec == std::errc() && conversion.ptr == value.data() + value.size();
}

void SendAddonMessage(Player* player, std::string const& payload)
{
    WorldPacket packet;
    std::string const message = std::string(AddonPrefix) + '\t' + payload;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
    player->GetSession()->SendPacket(&packet);
}

std::vector<std::string_view> SplitMessage(std::string_view message)
{
    std::vector<std::string_view> parts;
    while (!message.empty())
    {
        std::size_t const separator = message.find('\t');
        parts.push_back(message.substr(0, separator));
        if (separator == std::string_view::npos)
            break;
        message.remove_prefix(separator + 1);
    }
    return parts;
}

bool ParseByte(std::string_view value, uint8& result)
{
    uint32 parsed = 0;
    auto const conversion = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (conversion.ec != std::errc() || parsed > 255)
        return false;
    result = static_cast<uint8>(parsed);
    return true;
}

void SendItemMetadata(Player* player, uint8 bag, uint8 slot)
{
    uint8 serverBag = bag;
    uint8 serverSlot = slot;
    if (bag == 255 && slot > 0)
    {
        serverBag = INVENTORY_SLOT_BAG_0;
        serverSlot = slot - 1;
    }
    else if (bag == 0 && slot > 0)
    {
        serverBag = INVENTORY_SLOT_BAG_0;
        serverSlot = INVENTORY_SLOT_ITEM_START + slot - 1;
    }
    else if (bag >= 1 && bag <= 4 && slot > 0)
    {
        serverBag = INVENTORY_SLOT_BAG_START + bag - 1;
        serverSlot = slot - 1;
    }
    else if (bag >= 5 && bag <= 11 && slot > 0)
    {
        serverBag = BANK_SLOT_BAG_START + bag - 5;
        serverSlot = slot - 1;
    }
    else if (bag == 254 && slot > 0)
    {
        serverBag = INVENTORY_SLOT_BAG_0;
        serverSlot = BANK_SLOT_ITEM_START + slot - 1;
    }

    Item* item = player->GetItemByPos(serverBag, serverSlot);
    if (!item)
    {
        SendAddonMessage(player, "C\t" + std::to_string(bag) + "\t" + std::to_string(slot));
        return;
    }

    auto const roll = personalLootRolls.find(item->GetGUID().GetCounter());
    if (roll == personalLootRolls.end())
    {
        SendAddonMessage(player, "C\t" + std::to_string(bag) + "\t" + std::to_string(slot));
        return;
    }

    SendAddonMessage(player, "D\t" + std::to_string(bag) + "\t" + std::to_string(slot) + "\t" +
        BuildAffixText(roll->second));
}
}

void LoadPersonalLootRolls()
{
    personalLootRolls.clear();
    QueryResult result = CharacterDatabase.Query(
        "SELECT item_guid, rarity, rolled_class, affix1_type, affix1_value, affix2_type, affix2_value, "
        "affix3_type, affix3_value, affix4_type, affix4_value FROM mod_personal_loot_roll");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        PersonalLootRoll roll;
        ObjectGuid::LowType const itemGuid = fields[0].Get<uint32>();
        roll.itemQuality = fields[1].Get<uint8>();
        roll.rolledClass = fields[2].Get<uint8>();
        for (uint8 index = 0; index < roll.affixes.size(); ++index)
        {
            roll.affixes[index].type = static_cast<PersonalLootAffix>(fields[3 + index * 2].Get<uint8>());
            roll.affixes[index].value = fields[4 + index * 2].Get<uint32>();
        }
        personalLootRolls[itemGuid] = std::move(roll);
    } while (result->NextRow());
}

void DeletePersonalLootRoll(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid)
{
    personalLootRolls.erase(itemGuid);
    transaction->Append("DELETE FROM mod_personal_loot_roll WHERE item_guid = {}", itemGuid);
}

void TryRollPersonalLoot(Player* player, Item* item, ObjectGuid lootGuid, int32 lootIndex)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::PersonalLootEnabled) ||
        !player || !IsEligibleEquipment(item) || personalLootRolls.contains(item->GetGUID().GetCounter()))
        return;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    std::optional<PersonalLootRoll> const preRoll = TakePreRoll(player, lootGuid, lootIndex, itemTemplate->ItemId);
    PersonalLootRoll const roll = preRoll ? *preRoll : RollBonuses(player, itemTemplate);

    ObjectGuid::LowType const itemGuid = item->GetGUID().GetCounter();
    personalLootRolls[itemGuid] = roll;
    CharacterDatabase.Execute(
        "REPLACE INTO mod_personal_loot_roll (item_guid, rarity, custom_name, rolled_class, affix1_type, "
        "affix1_value, affix2_type, affix2_value, affix3_type, affix3_value, affix4_type, affix4_value) "
        "VALUES ({}, {}, '', {}, {}, {}, {}, {}, {}, {}, {}, {})",
        itemGuid, roll.itemQuality, roll.rolledClass,
        static_cast<uint8>(roll.affixes[0].type), roll.affixes[0].value,
        static_cast<uint8>(roll.affixes[1].type), roll.affixes[1].value,
        static_cast<uint8>(roll.affixes[2].type), roll.affixes[2].value,
        static_cast<uint8>(roll.affixes[3].type), roll.affixes[3].value);
}

void ApplyEquippedPersonalLoot(Player* player)
{
    if (!player)
        return;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        ApplyPersonalLootItem(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
}

void ApplyPersonalLootItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    auto const roll = personalLootRolls.find(item->GetGUID().GetCounter());
    if (roll == personalLootRolls.end())
        return;

    auto& equippedItems = equippedPersonalLoot[player->GetGUID().GetCounter()];
    if (!equippedItems.insert(item->GetGUID().GetCounter()).second)
        return;

    bool updateHealth = false;
    for (PersonalLootAffixRoll const& affix : roll->second.affixes)
    {
        ApplyAffix(player, affix, true);
        updateHealth = updateHealth || affix.type == PersonalLootAffix::MaximumHealth;
    }
    if (updateHealth)
        player->UpdateMaxHealth();
}

void RemovePersonalLootItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    auto playerRolls = equippedPersonalLoot.find(player->GetGUID().GetCounter());
    auto roll = personalLootRolls.find(item->GetGUID().GetCounter());
    if (playerRolls == equippedPersonalLoot.end() || roll == personalLootRolls.end() ||
        playerRolls->second.erase(item->GetGUID().GetCounter()) == 0)
        return;

    bool updateHealth = false;
    for (PersonalLootAffixRoll const& affix : roll->second.affixes)
    {
        ApplyAffix(player, affix, false);
        updateHealth = updateHealth || affix.type == PersonalLootAffix::MaximumHealth;
    }
    if (updateHealth)
        player->UpdateMaxHealth();
}

void ClearPersonalLootPlayerState(Player* player)
{
    if (!player)
        return;
    ObjectGuid::LowType const playerGuid = player->GetGUID().GetCounter();
    equippedPersonalLoot.erase(playerGuid);
    equippedCustomBonuses.erase(playerGuid);
    addonHandshakes.erase(playerGuid);
}

void BeginPersonalLootAddonHandshake(Player* player)
{
    if (!player)
        return;

    AddonHandshake& handshake = addonHandshakes[player->GetGUID().GetCounter()];
    handshake = {};
    handshake.verified = !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::PersonalLootRequireAddon);
}

void UpdatePersonalLootAddonHandshake(Player* player, uint32 diff)
{
    if (!player || !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::PersonalLootRequireAddon))
        return;

    AddonHandshake& handshake = addonHandshakes[player->GetGUID().GetCounter()];
    if (handshake.verified)
        return;

    handshake.elapsed += diff;
    uint32 const graceTime = statGrowthConfig.GetConfigValue<uint32>(
        StatGrowthConfigKey::PersonalLootAddonGraceSeconds) * IN_MILLISECONDS;
    if (!handshake.warned && handshake.elapsed >= graceTime / 2)
    {
        handshake.warned = true;
        ChatHandler(player->GetSession()).SendSysMessage(
            "|cffff2020The mandatory Gear Bonuses addon was not detected. Enable it and reconnect.|r");
    }

    if (handshake.elapsed >= graceTime)
    {
        handshake.verified = true;
        player->GetSession()->KickPlayer("Mandatory Gear Bonuses addon handshake missing");
    }
}

void HandlePersonalLootAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON)
        return;

    std::vector<std::string_view> const parts = SplitMessage(message);
    if (parts.size() < 2 || parts[0] != AddonPrefix)
        return;

    if (parts[1] == "HELLO")
    {
        addonHandshakes[player->GetGUID().GetCounter()].verified = true;
        SendAddonMessage(player, "READY\t3");
        return;
    }

    if (parts[1] == "Q" && parts.size() >= 4)
    {
        uint8 bag = 0;
        uint8 slot = 0;
        if (ParseByte(parts[2], bag) && ParseByte(parts[3], slot))
            SendItemMetadata(player, bag, slot);
        return;
    }

    // L <itemId> <lootSlot>: item in the open loot window; R <itemId> <rollId>: item of an active group roll
    if ((parts[1] == "L" || parts[1] == "R") && parts.size() >= 4)
    {
        uint32 itemId = 0;
        uint32 clientSlot = 0;
        if (!ParseNumber(parts[2], itemId) || !ParseNumber(parts[3], clientSlot))
            return;

        bool const isRoll = parts[1] == "R";
        std::string const reference = std::string(parts[1]) + "\t" + std::to_string(clientSlot) + "\t" +
            std::to_string(itemId);
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::PersonalLootEnabled) ||
            !IsEligibleEquipmentTemplate(itemTemplate))
        {
            SendAddonMessage(player, "C" + reference);
            return;
        }

        ObjectGuid lootGuid;
        int32 lootIndex = -1;
        if (isRoll)
        {
            // Prefer the roll whose slot matches the client roll id when the same item is rolled more than once
            if (Group* group = player->GetGroup())
                for (Roll const* roll : group->GetRolls())
                    if (roll && roll->itemid == itemId && roll->playerVote.contains(player->GetGUID()) &&
                        (lootIndex < 0 || roll->itemSlot == clientSlot))
                    {
                        lootGuid = roll->itemGUID;
                        lootIndex = roll->itemSlot;
                    }
        }
        else if (Loot* loot = GetOpenLoot(player))
        {
            // The client loot slot can be offset by the money slot: pick the matching item closest to it
            for (uint32 index = 0; index < loot->items.size(); ++index)
            {
                LootItem const& lootItem = loot->items[index];
                if (lootItem.is_looted || lootItem.itemid != itemId)
                    continue;

                if (lootIndex < 0 || std::abs(int32(index) + 1 - int32(clientSlot)) <
                    std::abs(lootIndex + 1 - int32(clientSlot)))
                    lootIndex = int32(index);
            }
            lootGuid = player->GetLootGUID();
        }

        if (lootGuid.IsEmpty() || lootIndex < 0)
        {
            SendAddonMessage(player, "C" + reference);
            return;
        }

        PersonalLootRoll const& roll = GetOrCreatePreRoll(player, lootGuid, uint32(lootIndex), itemTemplate);
        std::string const affixText = BuildAffixText(roll);
        SendAddonMessage(player, affixText.empty() ? "C" + reference : "D" + reference + "\t" + affixText);
    }
}

uint32 GetEquippedPersonalLootBonus(Player const* player, PersonalLootAffix affix)
{
    if (!player || !IsPercentageAffix(affix))
        return 0;

    auto const playerBonuses = equippedCustomBonuses.find(player->GetGUID().GetCounter());
    return playerBonuses == equippedCustomBonuses.end()
        ? 0
        : playerBonuses->second[GetCustomBonusIndex(affix)];
}

// Leech heals the player for a percentage of the effective damage they deal
void ApplyPersonalLootLeech(Unit* attacker, Unit* victim, uint32 damage)
{
    Player* player = attacker ? attacker->ToPlayer() : nullptr;
    if (!player || !victim || victim == player || damage == 0 || !player->IsAlive() || player->IsFullHealth())
        return;

    uint32 const leech = GetEquippedPersonalLootBonus(player, PersonalLootAffix::Leech);
    if (leech == 0)
        return;

    float const leechPercent = std::min(static_cast<float>(leech),
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::PersonalLootMaxLeech));
    uint32 const effectiveDamage = std::min(damage, victim->GetHealth());
    uint32 const healing = static_cast<uint32>(static_cast<float>(effectiveDamage) * leechPercent / 100.0f);
    if (healing == 0)
        return;

    // A spell heal, not a silent ModifyHealth: it sends the heal log, so the heal shows as floating combat text,
    // in the combat log and in damage meters such as Details
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_GEAR_BONUS_LEECH);
    if (!spellInfo)
    {
        player->ModifyHealth(healing);
        return;
    }

    HealInfo healInfo(player, player, healing, spellInfo, spellInfo->GetSchoolMask());
    player->HealBySpell(healInfo);
}
