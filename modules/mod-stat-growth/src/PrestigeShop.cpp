#include "PrestigeShop.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "ParagonSystem.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldSession.h"

#include <array>
#include <charconv>
#include <mutex>
#include <unordered_map>

// Éclats de prestige, and the heirlooms they buy.
//
// A character that has prestiged levels again from 1, and that second trip is what the éclats reward: every level it
// gains, every PrestigeKillsPerShard enemies it kills for experience (its group's kills included), and reaching the
// maximum level again. They are spent at the prestige keeper, on the heirlooms of the game - the gear that levels with
// its wearer and speeds the next trip up. Heirlooms are bound to the account, so the éclats are the account's: any of
// its characters earns them and any of them spends them.
//
// Messages (addon prefix "Prestige", whispered to oneself; the rest of the protocol is PrestigeSystem.cpp's):
//   server -> client
//     SHARDS <tab> <éclats> <tab> <kill progress> <tab> <kills per éclat> <tab> <gained> <tab> <per level> <tab>
//       <run bonus>                    the account's éclats; gained: how many this message pays (0 for a refresh)
//     SHOP <tab> <item>:<cost>:<usable>,...
//                                      the heirlooms on offer; usable 1 when this character can wear or use it
//     BOUGHT <tab> <item> <tab> <éclats left>
//     BUYFAIL <tab> <code>             1 not enough éclats, 2 no room in the bags, 3 not usable, 4 not on offer
//   client -> server
//     BUY <tab> <item>
namespace
{
constexpr std::string_view Prefix = "Prestige";

// What a trip back to the maximum level pays
constexpr uint32 PrestigeShardsPerLevel = 10;
constexpr uint32 PrestigeKillsPerShard = 20;
constexpr uint32 PrestigeRunBonus = 150;

enum class BuyFailure : uint8
{
    Shards = 1,
    Bags = 2,
    Usable = 3,
    Unknown = 4
};

struct Offer
{
    uint32 item;
    uint32 cost;
};

// Every heirloom of the game, by what it costs: a first trip back to 80 (about 1,200 éclats) buys three or four.
// The armour saves 10% experience each, the rest scales its stats with its wearer.
constexpr std::array<Offer, 38> Shop = { {
    // Shoulders
    { 42949, 300 }, { 42950, 300 }, { 42951, 300 }, { 42952, 300 }, { 42984, 300 }, { 42985, 300 }, { 44099, 300 },
    { 44100, 300 }, { 44101, 300 }, { 44102, 300 }, { 44103, 300 }, { 44105, 300 }, { 44107, 300 },
    // Chests
    { 48677, 350 }, { 48683, 350 }, { 48685, 350 }, { 48687, 350 }, { 48689, 350 }, { 48691, 350 },
    // One-hand weapons and ranged
    { 42944, 350 }, { 44091, 350 }, { 44096, 350 }, { 48716, 350 }, { 42945, 350 }, { 42948, 350 },
    { 42946, 350 }, { 44093, 350 },
    // Two-hand weapons
    { 38691, 500 }, { 42943, 500 }, { 42947, 500 }, { 44092, 500 }, { 44095, 500 }, { 48718, 500 },
    // Trinkets and insignias
    { 42991, 400 }, { 42992, 400 }, { 44097, 250 }, { 44098, 250 },
    // Cold Weather Flying for the next character to reach Northrend
    { 49177, 200 },
} };

struct Shards
{
    uint32 shards = 0;
    uint32 earned = 0;
    uint32 killProgress = 0;
};

// By account. Two characters of one account can be in the world at once, in maps updated on different threads.
std::mutex ShardLock;
std::unordered_map<uint32, Shards> AccountShards;

bool IsRealPlayer(Player* player)
{
    return player && player->GetSession() && !player->GetSession()->IsBot();
}

bool IsFrench(Player* player)
{
    return player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
}

void Send(Player* player, std::string const& body)
{
    if (!IsRealPlayer(player))
        return;

    WorldPacket packet;
    std::string const payload = std::string(Prefix) + "\t" + body;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

void Load(uint32 account)
{
    Shards loaded;
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT shards, earned, kill_progress FROM account_prestige_shards WHERE account = {}", account))
    {
        Field* fields = result->Fetch();
        loaded.shards = fields[0].Get<uint32>();
        loaded.earned = fields[1].Get<uint32>();
        loaded.killProgress = fields[2].Get<uint32>();
    }

    std::lock_guard<std::mutex> guard(ShardLock);
    AccountShards[account] = loaded;
}

Shards Get(uint32 account)
{
    std::lock_guard<std::mutex> guard(ShardLock);
    auto const itr = AccountShards.find(account);
    return itr != AccountShards.end() ? itr->second : Shards();
}

std::string SaveStatement(uint32 account, Shards const& shards)
{
    return Acore::StringFormat("REPLACE INTO account_prestige_shards (account, shards, earned, kill_progress) "
        "VALUES ({}, {}, {}, {})", account, shards.shards, shards.earned, shards.killProgress);
}

// Adds éclats (and kill progress) to the account and saves it
Shards Add(uint32 account, uint32 shards, uint32 kills)
{
    Shards updated;
    {
        std::lock_guard<std::mutex> guard(ShardLock);
        Shards& entry = AccountShards[account];
        entry.killProgress += kills;
        uint32 const fromKills = entry.killProgress / PrestigeKillsPerShard;
        entry.killProgress %= PrestigeKillsPerShard;
        entry.shards += shards + fromKills;
        entry.earned += shards + fromKills;
        updated = entry;
    }
    CharacterDatabase.Execute(SaveStatement(account, updated));
    return updated;
}

// Whether a character is on a trip that pays: it has prestiged, and it is a real player
bool EarnsShards(Player* player)
{
    return IsRealPlayer(player) && GetParagonPrestige(player) > 0;
}

void Pay(Player* player, uint32 shards, uint32 kills)
{
    uint32 const account = player->GetSession()->GetAccountId();
    uint32 const before = Get(account).shards;
    Shards const after = Add(account, shards, kills);
    uint32 const gained = after.shards - before;
    if (!gained && !kills)
        return;

    SendPrestigeShards(player, gained);
}

void Announce(Player* player, uint32 shards, bool runComplete)
{
    ChatHandler chat(player->GetSession());
    if (runComplete)
        chat.PSendSysMessage(IsFrench(player)
            ? "|cffe6cc80Retour au niveau maximum : +{} éclats de prestige.|r"
            : "|cffe6cc80Back at the maximum level: +{} prestige shards.|r", shards);
    else
        chat.PSendSysMessage(IsFrench(player)
            ? "|cffe6cc80+{} éclats de prestige.|r"
            : "|cffe6cc80+{} prestige shards.|r", shards);
}

Offer const* FindOffer(uint32 item)
{
    for (Offer const& offer : Shop)
        if (offer.item == item)
            return &offer;
    return nullptr;
}

void Buy(Player* player, uint32 itemId)
{
    Offer const* offer = FindOffer(itemId);
    ItemTemplate const* proto = offer ? sObjectMgr->GetItemTemplate(itemId) : nullptr;
    if (!proto)
    {
        Send(player, Acore::StringFormat("BUYFAIL\t{}", uint32(BuyFailure::Unknown)));
        return;
    }

    if (player->CanUseItem(proto) != EQUIP_ERR_OK)
    {
        Send(player, Acore::StringFormat("BUYFAIL\t{}", uint32(BuyFailure::Usable)));
        return;
    }

    uint32 const account = player->GetSession()->GetAccountId();
    if (Get(account).shards < offer->cost)
    {
        Send(player, Acore::StringFormat("BUYFAIL\t{}", uint32(BuyFailure::Shards)));
        return;
    }

    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, itemId, 1) != EQUIP_ERR_OK)
    {
        Send(player, Acore::StringFormat("BUYFAIL\t{}", uint32(BuyFailure::Bags)));
        return;
    }

    Shards remaining;
    {
        std::lock_guard<std::mutex> guard(ShardLock);
        Shards& entry = AccountShards[account];
        if (entry.shards < offer->cost)
        {
            Send(player, Acore::StringFormat("BUYFAIL\t{}", uint32(BuyFailure::Shards)));
            return;
        }
        entry.shards -= offer->cost;
        remaining = entry;
    }

    Item* item = player->StoreNewItem(destination, itemId, true);
    if (item)
        player->SendNewItem(item, 1, true, false);

    // The heirloom and the price leave together: a crash cannot keep one without the other
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    player->SaveInventoryAndGoldToDB(trans);
    trans->Append(SaveStatement(account, remaining));
    CharacterDatabase.CommitTransaction(trans);

    LOG_INFO("module", "prestige shop account={} player={} item={} cost={} left={}", account, player->GetName(),
             itemId, offer->cost, remaining.shards);
    Send(player, Acore::StringFormat("BOUGHT\t{}\t{}", itemId, remaining.shards));
}

class PrestigeShopPlayerScript : public PlayerScript
{
public:
    PrestigeShopPlayerScript() : PlayerScript("PrestigeShopPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_GIVE_EXP
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (IsRealPlayer(player))
            Load(player->GetSession()->GetAccountId());
    }

    // Every level gained on the way back up, and the maximum level reached again. The prestige itself, down to
    // level 1, pays nothing.
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!EarnsShards(player) || player->GetLevel() <= oldLevel)
            return;

        uint32 const levels = player->GetLevel() - oldLevel;
        bool const runComplete = player->GetLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
        uint32 const shards = levels * PrestigeShardsPerLevel + (runComplete ? PrestigeRunBonus : 0);
        Pay(player, shards, 0);
        Announce(player, shards, runComplete);
    }

    // An enemy killed for experience, by the character or its group
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        if (!amount || !victim || xpSource != XPSOURCE_KILL || !victim->IsCreature() || !EarnsShards(player))
            return;

        Pay(player, 0, 1);
    }
};
}

void SendPrestigeShards(Player* player, uint32 gained)
{
    if (!IsRealPlayer(player))
        return;

    Shards const shards = Get(player->GetSession()->GetAccountId());
    Send(player, Acore::StringFormat("SHARDS\t{}\t{}\t{}\t{}\t{}\t{}", shards.shards, shards.killProgress,
        PrestigeKillsPerShard, gained, PrestigeShardsPerLevel, PrestigeRunBonus));
}

void SendPrestigeShop(Player* player)
{
    if (!IsRealPlayer(player))
        return;

    std::string list;
    for (Offer const& offer : Shop)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(offer.item);
        if (!proto)
            continue;

        if (!list.empty())
            list += ',';
        list += Acore::StringFormat("{}:{}:{}", offer.item, offer.cost,
            player->CanUseItem(proto) == EQUIP_ERR_OK ? 1 : 0);
    }
    Send(player, "SHOP\t" + list);
}

bool HandlePrestigeShopMessage(Player* player, std::string_view body)
{
    constexpr std::string_view BuyPrefix = "BUY\t";
    if (!body.starts_with(BuyPrefix))
        return false;

    body.remove_prefix(BuyPrefix.size());
    uint32 itemId = 0;
    if (std::from_chars(body.data(), body.data() + body.size(), itemId).ec != std::errc())
        return true;

    Buy(player, itemId);
    return true;
}

void AddPrestigeShopScripts()
{
    new PrestigeShopPlayerScript();
}
