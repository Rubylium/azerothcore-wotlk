// Gardien-chef Vorhan's sets as generated items, the raid's own kind (see Legendary.h and README.md).
//
// A set piece is a raid item of its slot and armour type - a top tier base the Mythic+ and raid loot is generated
// from (mod-stat-growth MythicItemGeneration.cpp) - grown to the set's item level exactly as that loot is, and worn by
// the set's own row: its name, look, icon and set. Its stats, armour, sockets, socket bonus, durability and "Heroic"
// line are the raid item's, in the item's own template, so the client knows them as it knows a raid item's (the
// tooltip, the comparison, the character sheet, the average item level) and its gear bonuses roll at its item level.
// Each raid item a piece is made from is a profile, one entry each (Mythic::GetSetPieceItemEntry): the looter gets
// the one that suits them, as a Mythic+ reward is chosen (SmartLootSystem.cpp SelectSuitedItem).
//
// The profiles are numbered once and kept (legendary_set_profile): an item's entry names its profile for good,
// whatever raid items are added later. Copies rolled by the legendary engine before (character_legendary, on the
// set's row) are turned into the profile nearest their rolls at startup.

#include "Legendary.h"

#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "GroundLoot.h"
#include "Log.h"
#include "MythicDungeon.h"
#include "MythicDungeonSystem.h"
#include "MythicItemGeneration.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RaidTrinkets.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SmartLootSystem.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "Tokenize.h"
#include "WorldScript.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace Legendary
{
namespace
{
// Gardien-chef Vorhan's gear's item level (mod-playerbots ChallengeBoard.cpp gives it): above the Hollow Voice's 477
constexpr uint32 SetItemLevel = 485;

// His sets' rows (localTools/legendary/buildLegendaryItemSql.py: names and looks, no stats), head to feet, by armour
// type (ArmorType: cloth, leather, mail, plate), and the three pieces every wearer shares
constexpr std::array<std::array<uint32, 8>, 4> ArmourSets = { {
    { 13688, 13689, 13690, 13691, 13692, 13693, 13694, 13695 },   // Atours du Lieur de sceaux
    { 13680, 13681, 13682, 13683, 13684, 13685, 13686, 13687 },   // Cuirs du Traqueur d'évadés
    { 13672, 13673, 13674, 13675, 13676, 13677, 13678, 13679 },   // Mailles du Porte-chaînes
    { 13710, 13711, 13712, 13713, 13714, 13715, 13716, 13717 },   // Harnois du Gardien-chef
} };
constexpr std::array<uint32, 3> SharedPieces = { 13696, 13697, 12187 };   // Clé de cellule, Anneau de matricule, Cape

std::vector<uint32> AllRows()
{
    std::vector<uint32> rows;
    for (auto const& set : ArmourSets)
        rows.insert(rows.end(), set.begin(), set.end());
    rows.insert(rows.end(), SharedPieces.begin(), SharedPieces.end());
    return rows;
}

struct Profile
{
    uint32 profile;
    uint32 donor;                   // the raid item it is made from
    ItemTemplate const* item;
};

// Per row: its profiles, by number. Made before the world opens and never changed after: read without a lock.
std::unordered_map<uint32, std::vector<Profile>> Pieces;

bool HasStats(ItemTemplate const& itemTemplate)
{
    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        if (itemTemplate.ItemStat[index].ItemStatType && itemTemplate.ItemStat[index].ItemStatValue)
            return true;
    return false;
}

int32 StatOf(ItemTemplate const& itemTemplate, uint32 type)
{
    int32 value = 0;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        if (itemTemplate.ItemStat[index].ItemStatType == type)
            value += itemTemplate.ItemStat[index].ItemStatValue;
    return value;
}

// What a piece is made from: its class, its slot (a robe is a chest) and, in a slot made of an armour type, that type
std::tuple<uint32, uint32, uint32> PeerKey(ItemTemplate const& itemTemplate)
{
    uint32 const slot = itemTemplate.InventoryType == INVTYPE_ROBE ? uint32(INVTYPE_CHEST) : itemTemplate.InventoryType;
    bool armorType = false;
    switch (itemTemplate.InventoryType)
    {
        case INVTYPE_HEAD:
        case INVTYPE_SHOULDERS:
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
        case INVTYPE_WAIST:
        case INVTYPE_LEGS:
        case INVTYPE_FEET:
        case INVTYPE_WRISTS:
        case INVTYPE_HANDS:
            armorType = itemTemplate.Class == ITEM_CLASS_ARMOR;
            break;
        default:
            break;
    }
    return { itemTemplate.Class, slot, armorType ? itemTemplate.SubClass : 0u };
}

// An item's primary stat: the largest of strength, agility and intellect (ITEM_MOD_*), 0 with none
uint32 PrimaryOf(ItemTemplate const& itemTemplate)
{
    int32 const strength = StatOf(itemTemplate, ITEM_MOD_STRENGTH);
    int32 const agility = StatOf(itemTemplate, ITEM_MOD_AGILITY);
    int32 const intellect = StatOf(itemTemplate, ITEM_MOD_INTELLECT);
    if (strength <= 0 && agility <= 0 && intellect <= 0)
        return 0;
    if (intellect > strength && intellect > agility)
        return ITEM_MOD_INTELLECT;
    return agility > strength ? ITEM_MOD_AGILITY : ITEM_MOD_STRENGTH;
}

// The piece: the raid item grown to the set's item level as its own variants are (GrowMythicItem), in the set's row.
// The client draws a generated entry with its base row (its Item.dbc row: class, subclass, slot, look), so what the
// row says it is stays the row's.
ItemTemplate MakePiece(ItemTemplate const& row, ItemTemplate const& donor, uint32 profile)
{
    ItemTemplate item = GrowMythicItem(donor, SetItemLevel);
    item.ItemId = Mythic::GetSetPieceItemEntry(row.ItemId, profile);
    item.Class = row.Class;
    item.SubClass = row.SubClass;
    item.SoundOverrideSubclass = row.SoundOverrideSubclass;
    item.InventoryType = row.InventoryType;
    item.Name1 = row.Name1;
    item.Description = row.Description;
    item.DisplayInfoID = row.DisplayInfoID;
    item.Quality = row.Quality;
    item.Material = row.Material;
    item.Sheath = row.Sheath;
    item.ItemSet = row.ItemSet;
    return item;
}

void BuildPieces()
{
    uint32 const startTime = getMSTime();

    // The raid items a piece can be made from, per slot and armour type, in entry order
    std::map<std::tuple<uint32, uint32, uint32>, std::vector<uint32>> donors;
    for (auto const& [entry, candidate] : *sObjectMgr->GetItemTemplateStore())
        if (!Mythic::IsGeneratedItem(entry) && IsMythicTopBaseItem(candidate) && HasStats(candidate))
            donors[PeerKey(candidate)].push_back(entry);
    for (auto& [key, entries] : donors)
        std::sort(entries.begin(), entries.end());

    // The profiles numbered already; a raid item with none yet gets the first free number, for good
    std::map<uint32, std::map<uint32, uint32>> numbered;    // row -> profile -> donor
    if (QueryResult result = CharacterDatabase.Query("SELECT item, profile, donor FROM legendary_set_profile"))
    {
        do
        {
            Field* fields = result->Fetch();
            numbered[fields[0].Get<uint32>()][fields[1].Get<uint32>()] = fields[2].Get<uint32>();
        } while (result->NextRow());
    }

    Pieces.clear();
    uint32 made = 0;
    for (uint32 rowEntry : AllRows())
    {
        ItemTemplate const* row = sObjectMgr->GetItemTemplate(rowEntry);
        if (!row)
        {
            LOG_ERROR("module", "Legendary: set piece row {} has no item template", rowEntry);
            continue;
        }
        std::map<uint32, uint32>& profiles = numbered[rowEntry];
        std::set<uint32> taken;
        for (auto const& [profile, donor] : profiles)
            taken.insert(donor);
        for (uint32 donor : donors[PeerKey(*row)])
        {
            if (taken.count(donor))
                continue;
            uint32 profile = 0;
            while (profiles.count(profile))
                ++profile;
            if (profile >= Mythic::SetPieceProfiles)
            {
                LOG_WARN("module", "Legendary: set piece {} has no profile left for raid item {}", rowEntry, donor);
                break;
            }
            profiles[profile] = donor;
            taken.insert(donor);
            CharacterDatabase.DirectExecute("INSERT IGNORE INTO legendary_set_profile (item, profile, donor) "
                "VALUES ({}, {}, {})", rowEntry, profile, donor);
        }

        std::vector<Profile>& pieces = Pieces[rowEntry];
        for (auto const& [profile, donor] : profiles)
        {
            ItemTemplate const* from = sObjectMgr->GetItemTemplate(donor);
            if (!from || profile >= Mythic::SetPieceProfiles)
            {
                LOG_ERROR("module", "Legendary: set piece {} profile {}: raid item {} is gone", rowEntry, profile,
                    donor);
                continue;
            }
            // Named as the row: AddGeneratedItemTemplate reads the row's names for the piece's entry
            ItemTemplate const* item = sObjectMgr->AddGeneratedItemTemplate(MakePiece(*row, *from, profile), rowEntry);
            pieces.push_back({ profile, donor, item });
            ++made;
        }
        if (pieces.empty())
            LOG_ERROR("module", "Legendary: set piece {} has no raid item to be made from", rowEntry);
    }
    LOG_INFO("server.loading", ">> Made {} set pieces (Gardien-chef Vorhan's {} rows, item level {}) in {} ms", made,
        Pieces.size(), SetItemLevel, GetMSTimeDiffToNow(startTime));
}

std::string StatsText(ItemTemplate const& item)
{
    std::string text = Acore::StringFormat("armour {}", item.Armor);
    for (uint32 index = 0; index < MAX_ITEM_PROTO_STATS; ++index)
        if (item.ItemStat[index].ItemStatType && item.ItemStat[index].ItemStatValue)
            text += Acore::StringFormat(", {}={}", item.ItemStat[index].ItemStatType,
                item.ItemStat[index].ItemStatValue);
    return text;
}

// A copy the legendary engine rolled: the profile nearest it - one its owner's class may wear, of its primary stat,
// with the most of its secondaries
Profile const* NearestProfile(uint32 rowEntry, uint8 playerClass, std::string_view stats)
{
    auto const found = Pieces.find(rowEntry);
    if (found == Pieces.end() || found->second.empty())
        return nullptr;

    uint32 primary = 0;
    std::set<uint32> secondaries;
    for (std::string_view pair : Acore::Tokenize(stats, ',', false))
    {
        std::vector<std::string_view> parts = Acore::Tokenize(pair, '=', false);
        if (parts.size() != 2)
            continue;
        Optional<uint32> type = Acore::StringTo<uint32>(parts[0]);
        if (!type)
            continue;
        if (*type == ITEM_MOD_STRENGTH || *type == ITEM_MOD_AGILITY || *type == ITEM_MOD_INTELLECT)
            primary = *type;
        else if (*type != ITEM_MOD_STAMINA)
            secondaries.insert(*type);
    }

    uint32 const classMask = playerClass ? 1u << (playerClass - 1) : 0u;
    Profile const* best = nullptr;
    int32 bestScore = -1;
    for (Profile const& profile : found->second)
    {
        ItemTemplate const& item = *profile.item;
        int32 score = 0;
        if (!classMask || (item.AllowableClass & classMask))
            score += 1000;
        if (primary && PrimaryOf(item) == primary)
            score += 100;
        for (uint32 type : secondaries)
            if (StatOf(item, type) > 0)
                score += 10;
        if (score > bestScore)
        {
            best = &profile;
            bestScore = score;
        }
    }
    return best;
}

// Copies of the set's rows (the legendary engine's, rolled per item in character_legendary; or none, a bare row):
// each item turned into the nearest profile, its rolls dropped. Its gear bonuses (mod_personal_loot_roll) are kept by
// its guid. Once done, no item is left on a row: nothing to do at the next start.
void MigrateCopies()
{
    std::string rows;
    for (uint32 row : AllRows())
        rows += Acore::StringFormat("{}{}", rows.empty() ? "" : ",", row);
    QueryResult result = CharacterDatabase.Query("SELECT i.guid, i.itemEntry, c.class, l.stats FROM item_instance i "
        "LEFT JOIN characters c ON c.guid = i.owner_guid LEFT JOIN character_legendary l ON l.item_guid = i.guid "
        "WHERE i.itemEntry IN ({})", rows);
    if (!result)
        return;

    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    uint32 migrated = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 const guid = fields[0].Get<uint32>();
        uint32 const rowEntry = fields[1].Get<uint32>();
        uint8 const playerClass = fields[2].IsNull() ? 0 : fields[2].Get<uint8>();
        std::string const stats = fields[3].IsNull() ? std::string() : fields[3].Get<std::string>();
        Profile const* profile = NearestProfile(rowEntry, playerClass, stats);
        if (!profile)
        {
            LOG_ERROR("module", "Legendary: set piece item {} ({}) has no profile to become", guid, rowEntry);
            continue;
        }
        transaction->Append("UPDATE item_instance SET itemEntry = {} WHERE guid = {} AND itemEntry = {}",
            profile->item->ItemId, guid, rowEntry);
        transaction->Append("DELETE FROM character_legendary WHERE item_guid = {}", guid);
        LOG_INFO("module", "Legendary: set piece item {} ({}, rolled {}) is now {} (raid item {}: {})", guid,
            rowEntry, stats.empty() ? "nothing" : stats, profile->item->ItemId, profile->donor,
            StatsText(*profile->item));
        ++migrated;
    } while (result->NextRow());
    CharacterDatabase.DirectCommitTransaction(transaction);
    LOG_INFO("server.loading", ">> Turned {} rolled set piece copies into set pieces", migrated);
}

// The piece of a row for a player: a profile of their favoured primary stat that suits them, else any that suits
// them, else the best scored for them
ItemTemplate const* FitPiece(Player* player, uint32 rowEntry)
{
    auto const found = Pieces.find(rowEntry);
    if (found == Pieces.end())
        return nullptr;
    std::vector<ItemTemplate const*> all;
    std::vector<ItemTemplate const*> favoured;
    uint32 const primary = FavouredPrimary(player);
    for (Profile const& profile : found->second)
    {
        all.push_back(profile.item);
        if (PrimaryOf(*profile.item) == primary)
            favoured.push_back(profile.item);
    }
    if (ItemTemplate const* item = SelectSuitedItem(player, favoured, false))
        return item;
    return SelectSuitedItem(player, all, true);
}

// One of the player's armour type's eight pieces or a shared one, at random
uint32 DrawRow(Player* player)
{
    uint32 const pick = urand(0, uint32(ArmourSets[0].size() + SharedPieces.size() - 1));
    return pick < ArmourSets[0].size() ? ArmourSets[ArmorType(player)][pick] :
        SharedPieces[pick - ArmourSets[0].size()];
}

class SetPieceWorldScript : public WorldScript
{
public:
    SetPieceWorldScript() : WorldScript("LegendarySetPieceWorldScript", { WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED }) { }

    // Before anyone logs in, as the raid's own generated items: the item store never changes under the map threads
    void OnBeforeWorldInitialized() override
    {
        BuildPieces();
        MigrateCopies();
    }
};

using namespace Acore::ChatCommands;

class SetPieceCommandScript : public CommandScript
{
public:
    SetPieceCommandScript() : CommandScript("LegendarySetPieceCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable setPieceTable =
        {
            { "list", HandleList, SEC_GAMEMASTER, Console::Yes },
            { "give", HandleGive, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "setpiece", setPieceTable },
        };
        return commandTable;
    }

    // .setpiece list <row>: a row's profiles, each beside the Hollow Voice's item of the same raid item (477)
    static bool HandleList(ChatHandler* handler, uint32 rowEntry)
    {
        auto const found = Pieces.find(rowEntry);
        if (found == Pieces.end())
        {
            handler->PSendSysMessage("Pas une pièce d'ensemble : {}.", rowEntry);
            return false;
        }
        uint32 const pinnacle = Mythic::GetGeneratedVariant(Mythic::MaxPinnacleItemLevel);
        for (Profile const& profile : found->second)
        {
            handler->PSendSysMessage("{} profil {} ({}, de {}) : niveau {}, {}", rowEntry, profile.profile,
                profile.item->ItemId, profile.donor, profile.item->ItemLevel, StatsText(*profile.item));
            if (ItemTemplate const* raid = sObjectMgr->GetItemTemplate(
                    Mythic::GetGeneratedItemEntry(profile.donor, pinnacle)))
                handler->PSendSysMessage("    raid {} : niveau {}, {}", raid->ItemId, raid->ItemLevel,
                    StatsText(*raid));
        }
        return true;
    }

    // .setpiece give [row]: a piece of Vorhan's sets for the selected player (or yourself) as his loot gives it
    static bool HandleGive(ChatHandler* handler, Optional<uint32> rowEntry)
    {
        Player* target = handler->getSelectedPlayerOrSelf();
        uint32 const row = rowEntry.value_or(DrawRow(target));
        ItemTemplate const* item = FitPiece(target, row);
        if (!item)
        {
            handler->PSendSysMessage("Pas une pièce d'ensemble : {}.", row);
            return false;
        }
        StoreMythicItem(target, item, {});
        handler->PSendSysMessage("{} : {} ({}), {}.", target->GetName(), item->Name1, item->ItemId,
            StatsText(*item));
        return true;
    }
};
}
}

// A piece of Gardien-chef Vorhan's sets for a player (mod-playerbots ChallengeBoard.cpp: his win): one of their armour
// type's eight or a shared piece, at random, of the profile that suits them; thrown on the floor with his loot
// (GroundLoot) as any raid item, in the bags when it cannot be. Its item level is the set's (the board gives 485).
// Sometimes one of his own trinkets instead (mod-stat-growth RaidTrinkets.cpp), of the player's role.
void GiveWardenVorhanLootItem(Player* player, uint32 /*itemLevel*/)
{
    using namespace Legendary;
    if (!player)
        return;
    if (ItemTemplate const* trinket = RaidTrinkets::Roll(player, RaidTrinkets::Raid::WardenVorhan))
    {
        if (!GroundLoot::Throw(player, trinket, {}))
            StoreMythicItem(player, trinket, {});
        return;
    }
    uint32 const row = DrawRow(player);
    ItemTemplate const* item = FitPiece(player, row);
    if (!item)
    {
        LOG_ERROR("module", "Legendary: no set piece of {} for {}", row, player->GetName());
        return;
    }
    if (!GroundLoot::Throw(player, item, {}))
        StoreMythicItem(player, item, {});
}

void AddLegendarySetPieceScripts()
{
    new Legendary::SetPieceWorldScript();
    new Legendary::SetPieceCommandScript();
}
