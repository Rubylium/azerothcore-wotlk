#include "Bag.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "GlobalScript.h"
#include "Item.h"
#include "Log.h"
#include "MythicDungeon.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

// The Forge: a piece of high-end gear brought to the blacksmith, and gold paid for his work, comes back
// ForgeItemLevelPerRank item levels higher, up to ForgeRanks times (MythicDungeon.h). The window is the client's
// ItemForge.lua, opened with .forge for now. Protocol, addon messages under the prefix "Forge", fields split by tabs:
//
//   client -> server   L                                       the list again
//                      U <bag> <slot> <entry>                  forge the item there (still that entry)
//   server -> client   O                                       a list follows
//                      I <bag> <slot> <entry> <rank> <itemLevel> <nextEntry> <nextItemLevel> <cost>
//                                                              one item that can be forged (nextEntry 0 at the top)
//                      E <open> <maxRank>                      end of the list; open 1: show the window
//                      D <bag> <slot> <entry> <rank> <itemLevel>   the blacksmith is done with it
//                      X <error>                               ForgeError
//
// A real item's rank is its entry (its forged copies are generated at startup, like the Mythic+ variants); a Mythic+
// variant becomes the next variant instead, so its rank is kept in character_item_forge.
namespace
{
constexpr std::string_view Prefix = "Forge";

// The gear the Forge takes: the epics of the endgame, from the heroic dungeons' 200 up
constexpr uint32 MinItemLevel = 200;

// Stats, weapon damage and block grow with the square of the item level ratio and armor with the ratio itself,
// as the Mythic+ variants do (mod-stat-growth MythicItemGeneration.cpp)
constexpr float StatGrowthExponent = 2.0f;

// The blacksmith's price, in gold: GoldAtBaseLevel for the first rank of an item level 200 piece, growing with the
// square of the item level and RankGrowth times with every rank already forged. Essences boost the gold the game
// gives: the last ranks are the sink, 3 000 to 5 000 gold for a raid or Mythic+ piece's eighth (a 264 piece: 227 gold
// for its first rank, 3 700 for its last, 10 900 in all).
constexpr float GoldAtBaseLevel = 130.0f;
constexpr float RankGrowth = 1.45f;

enum ForgeError : uint8
{
    ERROR_ITEM_CHANGED = 1,     // not there any more, or not the same item
    ERROR_NOT_FORGEABLE = 2,
    ERROR_MAX_RANK = 3,
    ERROR_NO_GOLD = 4,
    ERROR_IN_COMBAT = 5,
    ERROR_DEAD = 6
};

bool IsForgeBase(ItemTemplate const& itemTemplate)
{
    return (itemTemplate.Quality == ITEM_QUALITY_EPIC || itemTemplate.Quality == ITEM_QUALITY_LEGENDARY) &&
        !Mythic::IsGeneratedItem(itemTemplate.ItemId) &&
        (itemTemplate.Class == ITEM_CLASS_WEAPON || itemTemplate.Class == ITEM_CLASS_ARMOR) &&
        itemTemplate.InventoryType != INVTYPE_NON_EQUIP && itemTemplate.ItemLevel >= MinItemLevel &&
        itemTemplate.RandomProperty == 0 && itemTemplate.RandomSuffix == 0 && itemTemplate.Map == 0 &&
        itemTemplate.Area == 0 && !itemTemplate.HasFlag(ITEM_FLAG_DEPRECATED);
}

int32 Grow(int32 value, float factor)
{
    return static_cast<int32>(std::lround(value * factor));
}

ItemTemplate MakeForged(ItemTemplate const& base, uint32 rank)
{
    ItemTemplate item = base;
    item.ItemId = Mythic::GetForgeItemEntry(base.ItemId, rank);
    item.ItemLevel = base.ItemLevel + Mythic::ForgeItemLevelPerRank * rank;
    // The set is kept: the server counts a set's pieces by set, not by entry

    float const growth = static_cast<float>(item.ItemLevel) / static_cast<float>(base.ItemLevel);
    float const statGrowth = std::pow(growth, StatGrowthExponent);

    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        item.ItemStat[index].ItemStatValue = Grow(item.ItemStat[index].ItemStatValue, statGrowth);
    for (uint32 index = 0; index < MAX_ITEM_PROTO_DAMAGES; ++index)
    {
        item.Damage[index].DamageMin *= statGrowth;
        item.Damage[index].DamageMax *= statGrowth;
    }

    item.Armor = static_cast<uint32>(Grow(static_cast<int32>(item.Armor), growth));
    item.Block = static_cast<uint32>(Grow(static_cast<int32>(item.Block), statGrowth));
    for (int32* resistance : { &item.HolyRes, &item.FireRes, &item.NatureRes, &item.FrostRes, &item.ShadowRes,
                               &item.ArcaneRes })
        *resistance = Grow(*resistance, growth);
    return item;
}

// Where an item can go next at the Forge
struct ForgeStep
{
    uint32 rank = 0;            // ranks forged so far
    uint32 nextEntry = 0;       // 0: nothing more to forge
};

// A Mythic+ variant's rank, kept by item
uint32 StoredRank(Item const* item)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT forge_rank FROM character_item_forge WHERE item_guid = {}", item->GetGUID().GetCounter());
    return result ? result->Fetch()[0].Get<uint32>() : 0;
}

// Nothing when the item is not for the Forge at all
Optional<ForgeStep> GetStep(Item const* item)
{
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR) ||
        proto->InventoryType == INVTYPE_NON_EQUIP)
        return std::nullopt;

    uint32 const entry = proto->ItemId;
    ForgeStep step;
    if (Mythic::IsMythicGeneratedItem(entry))
    {
        step.rank = StoredRank(item);
        uint32 const variant = entry / Mythic::GeneratedItemBase - 1;
        if (step.rank < Mythic::ForgeRanks && variant + 1 < Mythic::GeneratedItemVariants)
            step.nextEntry = Mythic::GetGeneratedItemEntry(Mythic::GetBaseItemEntry(entry), variant + 1);
    }
    else
    {
        step.rank = Mythic::GetForgeRank(entry);
        uint32 const base = Mythic::GetBaseItemEntry(entry);
        // A real item the generator made no ranks for is not the Forge's
        if (!sObjectMgr->GetItemTemplate(Mythic::GetForgeItemEntry(base, 1)))
            return std::nullopt;
        if (step.rank < Mythic::ForgeRanks)
            step.nextEntry = Mythic::GetForgeItemEntry(base, step.rank + 1);
    }

    if (step.nextEntry && !sObjectMgr->GetItemTemplate(step.nextEntry))
        step.nextEntry = 0;
    return step;
}

uint32 GetCost(ItemTemplate const* proto, uint32 rank)
{
    float const level = static_cast<float>(proto->ItemLevel) / static_cast<float>(MinItemLevel);
    float const gold = GoldAtBaseLevel * level * level * std::pow(RankGrowth, static_cast<float>(rank));
    return static_cast<uint32>(std::lround(gold)) * GOLD;
}

void Send(Player* player, std::string const& body)
{
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(Prefix) + "\t" + body);
    player->GetSession()->SendPacket(&packet);
}

// The client learns an item's template from the server; the ones the window will preview are sent now, so they are
// there when it opens
void TeachItem(Player* player, uint32 entry)
{
    WorldPacket query(CMSG_ITEM_QUERY_SINGLE, 4);
    query << entry;
    player->GetSession()->HandleItemQuerySingleOpcode(query);
}

void SendItem(Player* player, Item* item, ForgeStep const& step)
{
    ItemTemplate const* proto = item->GetTemplate();
    ItemTemplate const* next = step.nextEntry ? sObjectMgr->GetItemTemplate(step.nextEntry) : nullptr;
    if (next)
        TeachItem(player, next->ItemId);
    Send(player, Acore::StringFormat("I\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", item->GetBagSlot(), item->GetSlot(),
        proto->ItemId, step.rank, proto->ItemLevel, next ? next->ItemId : 0, next ? next->ItemLevel : 0,
        next ? GetCost(proto, step.rank) : 0));
}

// Every piece of the player's gear the Forge can take, worn first, then the bags
void SendList(Player* player, bool open)
{
    Send(player, "O");
    auto consider = [player](Item* item)
    {
        if (item)
            if (Optional<ForgeStep> step = GetStep(item))
                SendItem(player, item, *step);
    };

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* container = player->GetBagByPos(bag))
            for (uint32 slot = 0; slot < container->GetBagSize(); ++slot)
                consider(container->GetItemByPos(uint8(slot)));

    Send(player, Acore::StringFormat("E\t{}\t{}", open ? 1 : 0, Mythic::ForgeRanks));
}

void SendError(Player* player, ForgeError error)
{
    Send(player, Acore::StringFormat("X\t{}", uint32(error)));
}

// The item becomes its next rank where it lies: its enchants, gems and binding stay; worn, its stats go and come back
void Reforge(Player* player, Item* item, ItemTemplate const* next)
{
    if (item->IsRefundable())
        item->SetNotRefundable(player);
    if (item->IsBOPTradable())
        item->ClearSoulboundTradeable(player);

    bool const equipped = item->IsEquipped();
    uint8 const slot = item->GetSlot();
    if (equipped)
        player->_ApplyItemMods(item, slot, false);

    uint32 const maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    uint32 const durability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    item->SetEntry(next->ItemId);
    item->SetUInt32Value(ITEM_FIELD_MAXDURABILITY, next->MaxDurability);
    item->SetUInt32Value(ITEM_FIELD_DURABILITY, maxDurability ?
        uint32(uint64(durability) * next->MaxDurability / maxDurability) : next->MaxDurability);
    item->SetState(ITEM_CHANGED, player);

    if (equipped)
    {
        player->_ApplyItemMods(item, slot, true);
        player->SetVisibleItemSlot(slot, item);
    }
}

void HandleForge(Player* player, uint8 bag, uint8 slot, uint32 entry)
{
    if (!player->IsAlive())
        return SendError(player, ERROR_DEAD);
    if (player->IsInCombat())
        return SendError(player, ERROR_IN_COMBAT);

    Item* item = player->GetItemByPos(bag, slot);
    if (!item || item->GetEntry() != entry || item->GetOwnerGUID() != player->GetGUID())
        return SendError(player, ERROR_ITEM_CHANGED);

    Optional<ForgeStep> step = GetStep(item);
    if (!step)
        return SendError(player, ERROR_NOT_FORGEABLE);
    if (!step->nextEntry)
        return SendError(player, ERROR_MAX_RANK);

    ItemTemplate const* proto = item->GetTemplate();
    uint32 const cost = GetCost(proto, step->rank);
    if (!player->HasEnoughMoney(cost))
        return SendError(player, ERROR_NO_GOLD);

    ItemTemplate const* next = sObjectMgr->GetItemTemplate(step->nextEntry);
    player->ModifyMoney(-int32(cost));
    TeachItem(player, next->ItemId);
    Reforge(player, item, next);

    uint32 const rank = step->rank + 1;
    if (Mythic::IsMythicGeneratedItem(next->ItemId))
        CharacterDatabase.Execute("REPLACE INTO character_item_forge (item_guid, forge_rank) VALUES ({}, {})",
            item->GetGUID().GetCounter(), rank);
    // Saved now: the gold is gone, the item must not come back unforged after a crash
    player->SaveToDB(false, false);

    LOG_INFO("module", "Forge: {} forged item {} ({} -> {}, rank {}) for {} copper", player->GetName(),
        item->GetGUID().GetCounter(), proto->ItemId, next->ItemId, rank, cost);

    Send(player, Acore::StringFormat("D\t{}\t{}\t{}\t{}\t{}", bag, slot, next->ItemId, rank, next->ItemLevel));
    SendList(player, false);
}

void HandleMessage(Player* player, std::string_view body)
{
    std::vector<std::string_view> fields = Acore::Tokenize(body, '\t', true);
    if (fields.empty())
        return;

    if (fields[0] == "L")
        SendList(player, false);
    else if (fields[0] == "U" && fields.size() >= 4)
    {
        Optional<uint8> bag = Acore::StringTo<uint8>(fields[1]);
        Optional<uint8> slot = Acore::StringTo<uint8>(fields[2]);
        Optional<uint32> entry = Acore::StringTo<uint32>(fields[3]);
        if (bag && slot && entry)
            HandleForge(player, *bag, *slot, *entry);
    }
}

// Each forgeable item's ranks, made at startup after the Mythic+ variants, before anyone logs in: the item store
// never changes under the map threads, and forged items always exist again after a restart
class ForgeWorldScript : public WorldScript
{
public:
    ForgeWorldScript() : WorldScript("ForgeWorldScript", { WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED }) { }

    void OnBeforeWorldInitialized() override
    {
        uint32 const startTime = getMSTime();

        // The store grows while generating: the bases are taken first
        std::vector<ItemTemplate const*> bases;
        for (auto const& [entry, itemTemplate] : *sObjectMgr->GetItemTemplateStore())
            if (IsForgeBase(itemTemplate))
                bases.push_back(&itemTemplate);

        for (ItemTemplate const* base : bases)
            for (uint32 rank = 1; rank <= Mythic::ForgeRanks; ++rank)
                sObjectMgr->AddGeneratedItemTemplate(MakeForged(*base, rank), base->ItemId);

        LOG_INFO("server.loading", ">> Generated {} Forge ranks for {} items in {} ms",
            bases.size() * Mythic::ForgeRanks, bases.size(), GetMSTimeDiffToNow(startTime));
    }
};

class ForgePlayerScript : public PlayerScript
{
public:
    ForgePlayerScript() : PlayerScript("ForgePlayerScript", { PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE }) { }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        if (language != LANG_ADDON || message.size() <= Prefix.size() || !message.starts_with(Prefix) ||
            message[Prefix.size()] != '\t')
            return;
        HandleMessage(player, std::string_view(message).substr(Prefix.size() + 1));
    }
};

// A deleted item takes its rank with it
class ForgeGlobalScript : public GlobalScript
{
public:
    ForgeGlobalScript() : GlobalScript("ForgeGlobalScript", { GLOBALHOOK_ON_ITEM_DEL_FROM_DB }) { }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        transaction->Append("DELETE FROM character_item_forge WHERE item_guid = {}", itemGuid);
    }
};

using namespace Acore::ChatCommands;

class ForgeCommandScript : public CommandScript
{
public:
    ForgeCommandScript() : CommandScript("ForgeCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "forge", HandleForgeCommand, SEC_PLAYER, Console::No }
        };
        return commandTable;
    }

    static bool HandleForgeCommand(ChatHandler* handler)
    {
        SendList(handler->GetSession()->GetPlayer(), true);
        return true;
    }
};
}

void AddForgeScripts()
{
    new ForgeWorldScript();
    new ForgePlayerScript();
    new ForgeGlobalScript();
    new ForgeCommandScript();
}
