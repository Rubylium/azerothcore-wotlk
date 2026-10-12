// The loot collection (.agents/plans/loot-collection): every static piece our own bosses drop, and whether a character
// ever looted it, shown in the N window's Collection page (clientPatcher FrameXML Collection.lua).
//
// A piece is its row (an entry's low word: a set piece or a generated item carries its profile above it, different
// for each player). Collected: the row entered the character's bags, for good (sold, deleted or replaced, it stays).
// Legendaries and Uniques stay hidden until seen: seen is the account's (any of its characters looted one), shown on
// all its characters - seen, not collected. Only static pieces are listed: the Gaol's sets and weapons, the raids'
// trinkets, the legendaries; a source whose gear is generated (L'Infini's, the Hollow Voice's armour, Mythic+) lists
// its static pieces and its legendaries only. What a character is shown is what the loot itself would give it: its
// armour type's set, the weapons its class uses, the trinkets of its role (and anything it already collected).
//
// Protocol (addon whispers to oneself, prefix "Collection", fields separated by tabs; as the Défi board's):
//   client -> server
//     OPEN                     the sources
//     LIST <tab> <source>      a source's pieces
//   server -> client
//     S <tab> <source> <tab> <collected> <tab> <total> <tab> <name> <tab> <icon>     a source, in order (icon: an
//         item whose icon it shows, its first legendary); then E, the last
//     G <tab> <source> <tab> <group> <tab> <name>     a group of a source's pieces (a Mythic+ dungeon)
//     P <tab> <source> <tab> <row> <tab> <entry> <tab> <kind> <tab> <state> <tab> <group> [<tab> <low> <tab> <high>]
//         a piece: entry, the item to show (the profile that suits the character); kind 0 armour, 1 weapon,
//         2 trinket, 3 legendary, 4 Unique; state 0 not collected (a legendary: hidden), 1 seen on the account, 2
//         collected; group 0 or a G line's; a legendary's power window, its bottom's low and its top's high in
//         tenths of a percent (its tooltip without a copy). Then F <tab> <source>, the last.
//     N <tab> <source> <tab> <row>      a piece just collected

#include "Legendary.h"

#include "Bag.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "RaidTrinkets.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace LootCollection
{
namespace
{
constexpr std::string_view Prefix = "Collection";

enum Kind : uint8
{
    KIND_ARMOUR = 0,
    KIND_WEAPON = 1,
    KIND_TRINKET = 2,
    KIND_LEGENDARY = 3,
    KIND_UNIQUE = 4,
};

enum State : uint8
{
    STATE_MISSING = 0,
    STATE_SEEN = 1,
    STATE_COLLECTED = 2,
};

// The Unique quality (localTools/legendary/buildLegendaryItemSql.py: a legendary's above it)
constexpr uint32 QualityUnique = 6;

// The sources, in the page's order: the Gaol's gates, the pinnacle, the god, Mythic+
struct Source
{
    uint8 id;
    uint32 boss;        // its creature entry (0: Mythic+, its dungeons)
};
constexpr uint8 SourceMythic = 5;
constexpr std::array<Source, 6> Sources = { {
    { 1, Legendary::WardenVorhanGate },
    { 2, Legendary::EscapeHunterGate },
    { 6, Legendary::GaolThirdGate },    // Valérian (its id the next free: the others' are kept by the client's choice)
    { 3, 930100 },      // the Hollow Voice
    { 4, 930000 },      // L'Infini
    { SourceMythic, 0 },
} };

struct Piece
{
    uint32 row;
    uint32 entry;
    Kind kind;
    uint32 group;
};

std::mutex Lock;
std::unordered_map<ObjectGuid::LowType, std::set<uint32>> Collected;    // the characters online
std::unordered_map<uint32, std::set<uint32>> Seen;                      // their accounts' legendaries seen

Source const* FindSource(uint8 id)
{
    for (Source const& source : Sources)
        if (source.id == id)
            return &source;
    return nullptr;
}

bool IsLegendaryRow(uint32 row)
{
    return Legendary::GetDefinitionByItem(row) != nullptr;
}

// The source a row's loot is (each row we collect belongs to one), 0 for none
uint8 SourceOfRow(uint32 row)
{
    if (Legendary::Definition const* definition = Legendary::GetDefinitionByItem(row))
    {
        if (definition->sourceDungeon)
            return SourceMythic;
        for (Source const& source : Sources)
            if (source.boss && source.boss == definition->sourceBoss)
                return source.id;
        return 0;
    }
    // A gate of the Gaol (its three bosses each a source)
    if (uint32 const gate = Legendary::GateOfRow(row))
    {
        for (Source const& source : Sources)
            if (source.boss == gate)
                return source.id;
        return 0;
    }
    if (std::optional<RaidTrinkets::Raid> const raid = RaidTrinkets::RaidOf(row))
    {
        switch (*raid)
        {
            case RaidTrinkets::Raid::WardenVorhan: return 1;
            case RaidTrinkets::Raid::EscapeHunter: return 2;
            case RaidTrinkets::Raid::HollowVoice: return 3;
            case RaidTrinkets::Raid::Valerian: return 6;
        }
    }
    return 0;
}

// A gate's own trinkets (RaidTrinkets), if it has any
std::optional<RaidTrinkets::Raid> RaidOfGate(uint32 gate)
{
    if (gate == Legendary::WardenVorhanGate)
        return RaidTrinkets::Raid::WardenVorhan;
    if (gate == Legendary::EscapeHunterGate)
        return RaidTrinkets::Raid::EscapeHunter;
    if (gate == Legendary::GaolThirdGate)
        return RaidTrinkets::Raid::Valerian;
    return std::nullopt;
}

Kind KindOf(uint32 row)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(row);
    if (IsLegendaryRow(row))
        return item && item->Quality == QualityUnique ? KIND_UNIQUE : KIND_LEGENDARY;
    if (RaidTrinkets::RaidOf(row))
        return KIND_TRINKET;
    // A shield and an off-hand with the weapons, where the Gaol keeps them
    bool const weapon = item && (item->Class == ITEM_CLASS_WEAPON || item->InventoryType == INVTYPE_SHIELD ||
        item->InventoryType == INVTYPE_HOLDABLE);
    return weapon ? KIND_WEAPON : KIND_ARMOUR;
}

// A source's pieces for a player: what its loot would give them, and whatever of it they already collected
std::vector<Piece> PiecesOf(Player* player, Source const& source, std::set<uint32> const& collected)
{
    std::vector<uint32> rows;
    if (source.boss == Legendary::WardenVorhanGate || source.boss == Legendary::EscapeHunterGate ||
        source.boss == Legendary::GaolThirdGate)
    {
        rows = Legendary::GateRows(player, source.boss);
        if (std::optional<RaidTrinkets::Raid> const raid = RaidOfGate(source.boss))
        {
            std::vector<uint32> const trinkets = RaidTrinkets::Suited(player, *raid);
            rows.insert(rows.end(), trinkets.begin(), trinkets.end());
        }
    }
    else if (source.id == 3)
        rows = RaidTrinkets::Suited(player, RaidTrinkets::Raid::HollowVoice);
    for (Legendary::Definition const& definition : Legendary::GetDefinitions())
        if (source.id == SourceMythic ? definition.sourceDungeon != 0 :
            (source.boss && definition.sourceBoss == source.boss))
            rows.push_back(definition.baseItem);
    for (uint32 row : collected)
        if (SourceOfRow(row) == source.id && std::ranges::find(rows, row) == rows.end())
            rows.push_back(row);

    std::vector<Piece> pieces;
    for (uint32 row : rows)
    {
        if (!sObjectMgr->GetItemTemplate(row))
            continue;
        Kind const kind = KindOf(row);
        uint32 const entry = kind == KIND_ARMOUR || kind == KIND_WEAPON ? Legendary::SetPieceEntry(player, row) : row;
        uint32 group = 0;
        if (Legendary::Definition const* definition = Legendary::GetDefinitionByItem(row))
            group = definition->sourceDungeon;
        pieces.push_back({ row, entry, kind, group });
    }
    return pieces;
}

void Send(Player* player, std::string const& body)
{
    if (!player || !player->IsInWorld() || player->GetSession()->IsBot())
        return;
    WorldPacket packet;
    std::string const payload = std::string(Prefix) + "\t" + body;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

std::string SourceName(Player* player, Source const& source)
{
    bool const french = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
    if (!source.boss)
        return french ? "Mythique+" : "Mythic+";
    CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(source.boss);
    if (!creature)
        return "?";
    std::string name = creature->Name;
    if (CreatureLocale const* data = sObjectMgr->GetCreatureLocale(source.boss))
        ObjectMgr::GetLocaleString(data->Name, player->GetSession()->GetSessionDbLocaleIndex(), name);
    return name;
}

std::string DungeonName(Player* player, uint32 dungeon)
{
    LFGDungeonEntry const* entry = sLFGDungeonStore.LookupEntry(dungeon);
    if (!entry)
        return "?";
    char const* name = entry->Name[player->GetSession()->GetSessionDbcLocale()];
    return name && *name ? name : entry->Name[LOCALE_enUS];
}

std::pair<std::set<uint32>, std::set<uint32>> Snapshot(Player* player)
{
    std::lock_guard<std::mutex> guard(Lock);
    auto const collected = Collected.find(player->GetGUID().GetCounter());
    auto const seen = Seen.find(player->GetSession()->GetAccountId());
    return { collected != Collected.end() ? collected->second : std::set<uint32>(),
             seen != Seen.end() ? seen->second : std::set<uint32>() };
}

State StateOf(uint32 row, std::set<uint32> const& collected, std::set<uint32> const& seen)
{
    if (collected.contains(row))
        return STATE_COLLECTED;
    return seen.contains(row) ? STATE_SEEN : STATE_MISSING;
}

void SendSources(Player* player)
{
    auto const [collected, seen] = Snapshot(player);
    for (Source const& source : Sources)
    {
        std::vector<Piece> const pieces = PiecesOf(player, source, collected);
        uint32 const owned = uint32(std::ranges::count_if(pieces, [&collected](Piece const& piece)
            { return collected.contains(piece.row); }));
        uint32 icon = 0;
        for (Piece const& piece : pieces)
            if (!icon && (piece.kind == KIND_LEGENDARY || piece.kind == KIND_UNIQUE))
                icon = piece.entry;
        Send(player, Acore::StringFormat("S\t{}\t{}\t{}\t{}\t{}", source.id, owned, pieces.size(),
                                         SourceName(player, source), icon));
    }
    Send(player, "E");
}

void SendPieces(Player* player, uint8 id)
{
    Source const* source = FindSource(id);
    if (!source)
        return;
    auto const [collected, seen] = Snapshot(player);
    std::vector<Piece> const pieces = PiecesOf(player, *source, collected);
    std::set<uint32> groups;
    for (Piece const& piece : pieces)
        if (piece.group && groups.insert(piece.group).second)
            Send(player, Acore::StringFormat("G\t{}\t{}\t{}", id, piece.group, DungeonName(player, piece.group)));
    for (Piece const& piece : pieces)
    {
        std::string window;
        if (Legendary::Definition const* definition = Legendary::GetDefinitionByItem(piece.row))
            window = Acore::StringFormat("\t{}\t{}", uint32(definition->bottomLow * 10.0f + 0.5f),
                                         uint32(definition->topHigh * 10.0f + 0.5f));
        Send(player, Acore::StringFormat("P\t{}\t{}\t{}\t{}\t{}\t{}{}", id, piece.row, piece.entry,
                                         uint32(piece.kind), uint32(StateOf(piece.row, collected, seen)), piece.group,
                                         window));
    }
    Send(player, Acore::StringFormat("F\t{}", id));
}

void Load(Player* player)
{
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    uint32 const account = player->GetSession()->GetAccountId();
    std::set<uint32> collected;
    if (QueryResult result = CharacterDatabase.Query("SELECT piece FROM character_loot_collection WHERE guid = {}",
        guid))
        do
            collected.insert((*result)[0].Get<uint32>());
        while (result->NextRow());
    std::set<uint32> seen;
    if (QueryResult result = CharacterDatabase.Query("SELECT piece FROM account_loot_seen WHERE account = {}",
        account))
        do
            seen.insert((*result)[0].Get<uint32>());
        while (result->NextRow());
    std::lock_guard<std::mutex> guard(Lock);
    Collected[guid] = std::move(collected);
    Seen[account] = std::move(seen);
}

// A row collected now: false if it was already
bool Collect(Player* player, uint32 row, bool legendary)
{
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    uint32 const account = player->GetSession()->GetAccountId();
    bool added = false;
    bool revealed = false;
    {
        std::lock_guard<std::mutex> guard(Lock);
        added = Collected[guid].insert(row).second;
        if (legendary)
            revealed = Seen[account].insert(row).second;
    }
    time_t const now = GameTime::GetGameTime().count();
    if (added)
        CharacterDatabase.Execute("REPLACE INTO character_loot_collection (guid, piece, at) VALUES ({}, {}, {})", guid,
                                  row, uint64(now));
    if (revealed)
        CharacterDatabase.Execute("REPLACE INTO account_loot_seen (account, piece, at) VALUES ({}, {}, {})", account,
                                  row, uint64(now));
    return added;
}

// What the character already has (worn, in its bags, in its bank and its bank's bags): collected, quietly - pieces
// looted before the collection existed, or by a way its hook does not see
void CollectOwned(Player* player)
{
    auto collect = [player](Item* item)
    {
        if (!item)
            return;
        uint32 const row = item->GetEntry() % 0x10000;
        if (SourceOfRow(row))
            Collect(player, row, IsLegendaryRow(row));
    };
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        collect(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        collect(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    auto bagContents = [player, &collect](uint8 bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                collect(bag->GetItemByPos(uint8(slot)));
    };
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        bagContents(bagSlot);
    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
        bagContents(bagSlot);
}

void HandleMessage(Player* player, std::string_view message)
{
    std::vector<std::string_view> const parts = Acore::Tokenize(message, '\t', true);
    if (parts.empty())
        return;
    if (parts[0] == "OPEN")
    {
        CollectOwned(player);
        SendSources(player);
    }
    else if (parts[0] == "LIST" && parts.size() >= 2)
        SendPieces(player, uint8(Acore::StringTo<uint32>(parts[1]).value_or(0)));
}
}

void Note(Player* player, uint32 entry)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;
    uint32 const row = entry % 0x10000;
    uint8 const source = SourceOfRow(row);
    if (!source || !Collect(player, row, IsLegendaryRow(row)))
        return;
    LOG_INFO("module", "LootCollection: {} collected {} ({})", player->GetName(), row, source);
    Send(player, Acore::StringFormat("N\t{}\t{}", source, row));
}

namespace
{
class LootCollectionPlayerScript final : public PlayerScript
{
public:
    LootCollectionPlayerScript() : PlayerScript("LootCollectionPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_STORE_NEW_ITEM,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player->GetSession()->IsBot())
        {
            Load(player);
            CollectOwned(player);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        std::lock_guard<std::mutex> guard(Lock);
        Collected.erase(player->GetGUID().GetCounter());
    }

    // Every way an item enters the bags: the ground loot's pickups, a satchel's, the drops given straight (a bag full
    // mails them: GroundLoot / MythicDungeonSystem call Note there)
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (item)
            Note(player, item->GetEntry());
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32&, uint32& language, std::string& message) override
    {
        if (language != LANG_ADDON || !message.starts_with(Prefix) || message.size() <= Prefix.size() ||
            message[Prefix.size()] != '\t')
            return;
        HandleMessage(player, std::string_view(message).substr(Prefix.size() + 1));
    }
};

using namespace Acore::ChatCommands;

class LootCollectionCommandScript final : public CommandScript
{
public:
    LootCollectionCommandScript() : CommandScript("LootCollectionCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable collectionTable =
        {
            { "list", HandleList, SEC_GAMEMASTER, Console::No },
            { "reset", HandleReset, SEC_GAMEMASTER, Console::No },
            { "add", HandleAdd, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "collection", collectionTable },
        };
        return commandTable;
    }

    // .collection list: the selected character's sources, collected out of shown
    static bool HandleList(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;
        auto const [collected, seen] = Snapshot(target);
        for (Source const& source : Sources)
        {
            std::vector<Piece> const pieces = PiecesOf(target, source, collected);
            uint32 const owned = uint32(std::ranges::count_if(pieces, [&collected](Piece const& piece)
                { return collected.contains(piece.row); }));
            handler->PSendSysMessage("{} : {} / {}", SourceName(target, source), owned, pieces.size());
        }
        handler->PSendSysMessage("Légendaires vus sur le compte : {}", seen.size());
        return true;
    }

    // .collection add <item>: the selected character collects it, as if looted (a test of the page)
    static bool HandleAdd(ChatHandler* handler, uint32 item)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;
        if (!SourceOfRow(item % 0x10000))
        {
            handler->PSendSysMessage("{} n'est pas une pièce de la collection.", item);
            return true;
        }
        Note(target, item);
        handler->PSendSysMessage("{} : {} collecté.", target->GetName(), item);
        return true;
    }

    // .collection reset: forgets the selected character's collection (its account's legendaries seen stay)
    static bool HandleReset(ChatHandler* handler)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
            return false;
        ObjectGuid::LowType const guid = target->GetGUID().GetCounter();
        {
            std::lock_guard<std::mutex> guard(Lock);
            Collected[guid].clear();
        }
        CharacterDatabase.Execute("DELETE FROM character_loot_collection WHERE guid = {}", guid);
        handler->PSendSysMessage("{} : collection effacée.", target->GetName());
        return true;
    }
};
}
}

void AddLootCollectionScripts()
{
    new LootCollection::LootCollectionPlayerScript();
    new LootCollection::LootCollectionCommandScript();
}
