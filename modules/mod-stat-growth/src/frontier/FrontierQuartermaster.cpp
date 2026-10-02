#include "Frontier.h"

#include "EssenceTierSystem.h"
#include "MythicDungeonSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>

// Le Front du Nord's quartermaster, on Krasus' Landing in Dalaran: three contracts a day per character, each a deed of
// the Front du Nord to do in a tier zone (or a Colosse to slay), paid in shards and a piece of the tier's gear as soon
// as it is done; and the shards' shop: a piece of a tier's gear for the slot chosen, or an essence.
// Plan: .agents/plans/northrend-frontier/northrend-frontier.PLAN.md
//
// The contracts are drawn per character and day (the day turns at ResetHour, server time): the first in tier I or II,
// the second in II or III, the third in III or IV or a Colosse; three different deeds. Kept in the characters
// database (frontier_contract), so a restart keeps the day's progress.
//
// Client: Interface\FrameXML\FrontierQuartermaster.lua, on the "Frontier" addon prefix, whispered to oneself:
//   server  QM <shards> <seconds to the next day> <gear price tier I> <II> <III> <IV> <essence price>
//           QMC <contract 1-3> <deed 1-4> <zone id, 0: any> <tier> <target> <progress> <done 0/1> <shards> <item level>
//           QMEND                      opens (or refreshes) the window
//           QMCLOSE                    walked away from the quartermaster
//   client  QM_BUY <tier 1-4> <equipment slot>, QM_ESSENCE, QM_CLOSED
namespace
{
using Frontier::Deed;

// Her creature (940040, stat_growth_frontier.sql) has this file's script: npc_frontier_quartermaster
constexpr float QuartermasterRange = 12.0f;
constexpr uint32 QuartermasterCheckMs = 500;
constexpr uint8 ResetHour = 6;
constexpr std::string_view Prefix = "Frontier";

// The shop: a piece of a tier's gear, and an essence
constexpr std::array<uint32, Frontier::TierCount> GearPrices = { 30, 45, 60, 80 };
constexpr uint32 EssencePrice = 25;
// The slots it sells for (their group: both rings, both trinkets), as the window lists them
constexpr std::array<uint8, 15> ShopSlots = {
    EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_NECK, EQUIPMENT_SLOT_SHOULDERS, EQUIPMENT_SLOT_BACK, EQUIPMENT_SLOT_CHEST,
    EQUIPMENT_SLOT_WRISTS, EQUIPMENT_SLOT_HANDS, EQUIPMENT_SLOT_WAIST, EQUIPMENT_SLOT_LEGS, EQUIPMENT_SLOT_FEET,
    EQUIPMENT_SLOT_FINGER1, EQUIPMENT_SLOT_TRINKET1, EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND,
    EQUIPMENT_SLOT_RANGED,
};

// A contract's pay, by its tier (and a piece of that tier's gear)
constexpr std::array<uint32, Frontier::TierCount> ContractShards = { 10, 15, 20, 25 };
// How many deeds a contract asks for, by deed (Deed's values)
constexpr std::array<uint8, 5> DeedTargets = { 0, 5, 2, 3, 1 };
constexpr std::size_t ContractCount = 3;

struct Contract
{
    Deed deed = Deed::Elite;
    uint32 zone = 0;            // where its deeds count; 0 anywhere (a Colosse)
    uint8 tier = 1;
    uint8 target = 1;
    uint8 progress = 0;
    bool done = false;
};

struct Contracts
{
    uint32 day = 0;
    std::array<Contract, ContractCount> list;
};

std::mutex Lock;
std::unordered_map<uint32, Contracts> Cache;       // by character (guid counter)

time_t NextDay()
{
    return Acore::Time::GetLocalHourTimestamp(GameTime::GetGameTime().count(), ResetHour, true);
}

uint32 Today()
{
    return uint32(NextDay() / DAY);
}

// The day's contracts of a character: the same whenever they are drawn again that day
Contracts Draw(uint32 guid, uint32 day)
{
    std::mt19937 random(guid * 2654435761u ^ day);
    auto const pick = [&random](uint32 count) { return std::uniform_int_distribution<uint32>(0, count - 1)(random); };

    std::array<Deed, ContractCount> deeds = { Deed::Elite, Deed::Rift, Deed::Chest };
    std::shuffle(deeds.begin(), deeds.end(), random);

    Contracts contracts;
    contracts.day = day;
    for (std::size_t index = 0; index < ContractCount; ++index)
    {
        Contract& contract = contracts.list[index];
        contract.tier = uint8(index + 1 + pick(2));
        if (index == ContractCount - 1 && pick(2) == 0)
            contract.deed = Deed::Colossus;
        else
        {
            contract.deed = deeds[index];
            contract.zone = Frontier::TierZone(contract.tier, uint8(pick(2)));
        }
        contract.target = DeedTargets[uint8(contract.deed)];
    }
    return contracts;
}

Contracts Load(uint32 guid)
{
    Contracts contracts;
    if (QueryResult result = CharacterDatabase.Query("SELECT `slot`, `day`, `deed`, `zone`, `tier`, `target`, "
            "`progress`, `done` FROM `frontier_contract` WHERE `guid` = {}", guid))
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const slot = fields[0].Get<uint8>();
            if (slot >= ContractCount)
                continue;
            contracts.day = fields[1].Get<uint32>();
            Contract& contract = contracts.list[slot];
            contract.deed = Deed(std::clamp<uint8>(fields[2].Get<uint8>(), 1, 4));
            contract.zone = fields[3].Get<uint32>();
            contract.tier = std::clamp<uint8>(fields[4].Get<uint8>(), 1, Frontier::TierCount);
            contract.target = std::max<uint8>(fields[5].Get<uint8>(), 1);
            contract.progress = fields[6].Get<uint8>();
            contract.done = fields[7].Get<uint8>() != 0;
        } while (result->NextRow());
    }
    return contracts;
}

void Save(uint32 guid, Contracts const& contracts)
{
    for (std::size_t slot = 0; slot < ContractCount; ++slot)
    {
        Contract const& contract = contracts.list[slot];
        CharacterDatabase.Execute("REPLACE INTO `frontier_contract` (`guid`, `slot`, `day`, `deed`, `zone`, `tier`, "
            "`target`, `progress`, `done`) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})", guid, slot, contracts.day,
            uint32(contract.deed), contract.zone, uint32(contract.tier), uint32(contract.target),
            uint32(contract.progress), contract.done ? 1 : 0);
    }
}

// A character's contracts of the day, drawn when the day turned. Lock held.
Contracts& ContractsOf(Player* player)
{
    uint32 const guid = player->GetGUID().GetCounter();
    uint32 const today = Today();
    auto found = Cache.find(guid);
    if (found == Cache.end())
        found = Cache.emplace(guid, Load(guid)).first;
    if (found->second.day != today)
    {
        found->second = Draw(guid, today);
        Save(guid, found->second);
    }
    return found->second;
}

std::string ZoneName(Player* player, uint32 zoneId)
{
    AreaTableEntry const* area = sAreaTableStore.LookupEntry(zoneId);
    if (!area)
        return "";
    LocaleConstant const locale = player->GetSession() ? player->GetSession()->GetSessionDbcLocale() : LOCALE_enUS;
    char const* name = area->area_name[locale];
    return name && *name ? name : area->area_name[LOCALE_enUS];
}

std::string Describe(Player* player, Contract const& contract)
{
    bool const french = Frontier::IsFrench(player);
    std::string const zone = ZoneName(player, contract.zone);
    switch (contract.deed)
    {
        case Deed::Elite:
            return Acore::StringFormat(french ? "Abattre {} élites rôdeurs ({})" : "Slay {} roaming elites ({})",
                contract.target, zone);
        case Deed::Rift:
            return Acore::StringFormat(french ? "Refermer {} failles arcaniques ({})" : "Close {} arcane rifts ({})",
                contract.target, zone);
        case Deed::Chest:
            return Acore::StringFormat(french ? "Ouvrir {} coffres du Front ({})" : "Open {} Frontier chests ({})",
                contract.target, zone);
        default:
            return french ? "Abattre un Colosse" : "Slay a Colossus";
    }
}

// The window's data, and the window opened (or refreshed)
void SendWindow(Player* player)
{
    Contracts const& contracts = ContractsOf(player);
    time_t const now = GameTime::GetGameTime().count();
    Frontier::SendAddon(player, Acore::StringFormat("QM\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        player->GetItemCount(Frontier::ITEM_FROST_SHARD), uint32(std::max<time_t>(NextDay() - now, 0)),
        GearPrices[0], GearPrices[1], GearPrices[2], GearPrices[3], EssencePrice));
    for (std::size_t slot = 0; slot < ContractCount; ++slot)
    {
        Contract const& contract = contracts.list[slot];
        Frontier::SendAddon(player, Acore::StringFormat("QMC\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", slot + 1,
            uint32(contract.deed), contract.zone, uint32(contract.tier), uint32(contract.target),
            uint32(contract.progress), contract.done ? 1 : 0, ContractShards[contract.tier - 1],
            Frontier::LootItemLevel(contract.tier)));
    }
    Frontier::SendAddon(player, "QMEND");
}

// The quartermaster whose window a player has open: it closes when the player walks away, and its purchases are only
// taken near her
struct QuartermasterData : public DataMap::Base
{
    ObjectGuid quartermaster;
    uint32 nextCheckMs = 0;
};
constexpr char const* QuartermasterKey = "FrontierQuartermaster";

bool NearQuartermaster(Player* player)
{
    QuartermasterData const* data = player->CustomData.Get<QuartermasterData>(QuartermasterKey);
    Creature* quartermaster = data ? ObjectAccessor::GetCreature(*player, data->quartermaster) : nullptr;
    return quartermaster && player->IsAlive() && player->IsWithinDistInMap(quartermaster, QuartermasterRange);
}

void CloseWindow(Player* player, bool tell)
{
    if (player->CustomData.Erase(QuartermasterKey) && tell)
        Frontier::SendAddon(player, "QMCLOSE");
}

void Say(Player* player, std::string const& french, std::string const& english)
{
    ChatHandler(player->GetSession()).PSendSysMessage("|cffd9b36c{}|r {}",
        Frontier::IsFrench(player) ? "Front du Nord :" : "Northrend Frontier:",
        Frontier::IsFrench(player) ? french : english);
}

// A piece of a tier's gear for the slot chosen: the shards are taken once the piece is given
void BuyGear(Player* player, uint8 tier, uint8 slot)
{
    if (!NearQuartermaster(player) || tier < 1 || tier > Frontier::TierCount ||
        std::ranges::find(ShopSlots, slot) == ShopSlots.end())
        return;
    uint32 const price = GearPrices[tier - 1];
    if (!player->HasItemCount(Frontier::ITEM_FROST_SHARD, price))
    {
        Say(player, "il vous manque des éclats de givre.", "you lack Frost Shards.");
        return;
    }
    if (!GiveMythicLootItemForSlot(player, Frontier::LootItemLevel(tier), slot))
    {
        Say(player, "aucune pièce de ce palier ne vous convient à cet emplacement.",
            "no piece of that tier fits you in that slot.");
        return;
    }
    player->DestroyItemCount(Frontier::ITEM_FROST_SHARD, price, true);
}

void BuyEssence(Player* player)
{
    if (!NearQuartermaster(player))
        return;
    if (!player->HasItemCount(Frontier::ITEM_FROST_SHARD, EssencePrice))
    {
        Say(player, "il vous manque des éclats de givre.", "you lack Frost Shards.");
        return;
    }
    if (GrantEssenceRewards(player, 1, 0))
        player->DestroyItemCount(Frontier::ITEM_FROST_SHARD, EssencePrice, true);
}

class npc_frontier_quartermaster : public CreatureScript
{
public:
    npc_frontier_quartermaster() : CreatureScript("npc_frontier_quartermaster") { }

    // No gossip: the client's own window (FrontierQuartermaster.lua) opens with the data
    bool OnGossipHello(Player* player, Creature* creature) override
    {
        CloseGossipMenuFor(player);
        if (Frontier::IsBot(player))
            return true;
        QuartermasterData* data = player->CustomData.GetDefault<QuartermasterData>(QuartermasterKey);
        data->quartermaster = creature->GetGUID();
        data->nextCheckMs = 0;
        std::lock_guard<std::mutex> guard(Lock);
        SendWindow(player);
        return true;
    }
};

class FrontierQuartermasterPlayerScript : public PlayerScript
{
public:
    FrontierQuartermasterPlayerScript() : PlayerScript("FrontierQuartermasterPlayerScript", {
        PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
    }) { }

    void OnPlayerLogout(Player* player) override
    {
        std::lock_guard<std::mutex> guard(Lock);
        Cache.erase(player->GetGUID().GetCounter());
    }

    // The window closes when the player walks away, like a merchant's
    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        QuartermasterData* data = player->CustomData.Get<QuartermasterData>(QuartermasterKey);
        if (!data)
            return;
        uint32 const now = GameTime::GetGameTimeMS().count();
        if (now < data->nextCheckMs)
            return;
        data->nextCheckMs = now + QuartermasterCheckMs;
        if (!NearQuartermaster(player))
            CloseWindow(player, true);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
        std::string& message) override
    {
        if (language != LANG_ADDON || !message.starts_with(Prefix) || message.size() <= Prefix.size() ||
            message[Prefix.size()] != '\t')
            return;
        std::string_view body(message);
        body.remove_prefix(Prefix.size() + 1);
        std::string_view command = body;
        std::string_view argument;
        if (std::size_t const tab = body.find('\t'); tab != std::string_view::npos)
        {
            command = body.substr(0, tab);
            argument = body.substr(tab + 1);
        }

        std::lock_guard<std::mutex> guard(Lock);
        if (command == "QM_BUY")
        {
            std::size_t const tab = argument.find('\t');
            if (tab == std::string_view::npos)
                return;
            uint32 tier = 0;
            uint32 slot = 0;
            std::from_chars(argument.data(), argument.data() + tab, tier);
            std::from_chars(argument.data() + tab + 1, argument.data() + argument.size(), slot);
            BuyGear(player, uint8(tier), uint8(slot));
        }
        else if (command == "QM_ESSENCE")
            BuyEssence(player);
        else if (command == "QM_CLOSED")
        {
            CloseWindow(player, false);
            return;
        }
        else
            return;
        if (NearQuartermaster(player))
            SendWindow(player);
    }
};
}

namespace Frontier
{
void CountDeed(Player* player, Deed deed, uint32 zoneId)
{
    if (!player || IsBot(player))
        return;
    std::lock_guard<std::mutex> guard(Lock);
    Contracts& contracts = ContractsOf(player);
    bool changed = false;
    for (Contract& contract : contracts.list)
    {
        if (contract.done || contract.deed != deed || (contract.zone && contract.zone != zoneId))
            continue;
        changed = true;
        ++contract.progress;
        if (contract.progress < contract.target)
        {
            Say(player, Acore::StringFormat("contrat « {} » : {} / {}.", Describe(player, contract),
                contract.progress, contract.target), Acore::StringFormat("contract \"{}\": {} / {}.",
                Describe(player, contract), contract.progress, contract.target));
            continue;
        }
        // Done: paid at once
        contract.done = true;
        uint32 const shards = ContractShards[contract.tier - 1];
        player->AddItem(ITEM_FROST_SHARD, shards);
        GiveMythicLootItem(player, LootItemLevel(contract.tier));
        Say(player, Acore::StringFormat("contrat « {} » rempli ! {} éclats de givre et une pièce d'équipement.",
            Describe(player, contract), shards), Acore::StringFormat("contract \"{}\" fulfilled! {} Frost Shards and "
            "a piece of gear.", Describe(player, contract), shards));
    }
    if (!changed)
        return;
    Save(player->GetGUID().GetCounter(), contracts);
    if (NearQuartermaster(player))
        SendWindow(player);
}

std::string DescribeContracts(Player* player)
{
    std::lock_guard<std::mutex> guard(Lock);
    Contracts const& contracts = ContractsOf(player);
    std::string text = Acore::StringFormat("Front du Nord: day {}, next in {} s.", contracts.day,
        uint32(std::max<time_t>(NextDay() - GameTime::GetGameTime().count(), 0)));
    for (Contract const& contract : contracts.list)
        text += Acore::StringFormat("\n  [{}] {}: {} / {} (tier {}, zone {})", contract.done ? "done" : "open",
            Describe(player, contract), contract.progress, contract.target, contract.tier, contract.zone);
    return text;
}
}

void AddFrontierQuartermasterScripts()
{
    new npc_frontier_quartermaster();
    new FrontierQuartermasterPlayerScript();
}
