#include "Bag.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "GlobalScript.h"
#include "Item.h"
#include "Log.h"
#include "MythicDungeon.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellAuras.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "Tokenize.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The Forge: a piece of high-end gear brought to the blacksmith, and gold paid for his work, comes back
// ForgeItemLevelPerRank item levels higher, up to ForgeRanks times (MythicDungeon.h). The window is the client's
// ItemForge.lua, opened by the master smith at his anvil in each capital (npc_forge_master) or with .forge.
//
// It is meant to feel like an event. At the anvil, the smith strikes the piece three times, in time with the
// window's hammer, and says a word when it comes out of the quench. Forged gear shows it: a weapon glows more with
// every rank, and the ranks forged on everything worn make the character smoulder, then burn. And the gold feels well
// spent: every piece keeps what it cost, and the smith remembers his customers - the more gold he has been paid, the
// better his price, and the likelier a masterwork (two ranks for the price of one).
//
// Protocol, addon messages under the prefix "Forge", fields split by tabs:
//
//   client -> server   L                                       the list again
//                      U <bag> <slot> <entry>                  forge the item there (still that entry)
//   server -> client   O                                       a list follows
//                      I <bag> <slot> <entry> <rank> <itemLevel> <nextEntry> <nextItemLevel> <cost> <invested>
//                                                              one item that can be forged (nextEntry 0 at the top)
//                      E <open> <maxRank> <spent> <standing>   end of the list; open 1: show the window
//                      D <bag> <slot> <entry> <rank> <itemLevel> <masterwork> <newStanding>
//                                                              the smith is done with it (newStanding 0: no change)
//                      X <error>                               ForgeError
//
// Money is in copper. A real item's rank is its entry (its forged copies are generated at startup, like the Mythic+
// variants); a Mythic+ variant becomes the next variant instead, so its rank is kept in character_item_forge, with
// what every forged item cost. character_forge_patron keeps what each character has paid the smith.
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
// for its first rank, 3 700 for its last, 10 900 in all), before the smith's discount.
constexpr float GoldAtBaseLevel = 130.0f;
constexpr float RankGrowth = 1.45f;

// The smith remembers his customers: the gold a character has paid him in all earns a standing, and each standing
// a discount and a chance that a forge is a masterwork. The client's ItemForge.lua names them; change them together.
struct Standing
{
    uint32 gold;
    uint32 discount;        // % off the price
    uint32 masterwork;      // % chance a forge gives two ranks
};
constexpr std::array<Standing, 5> Standings = { {
    { 0, 0, 0 },            // Client de passage
    { 5000, 5, 0 },         // Client régulier
    { 15000, 5, 5 },        // Client estimé
    { 35000, 10, 5 },       // Ami de la forge
    { 75000, 10, 10 },      // Légende de l'enclume
} };

// The smith and what he does (localTools/forge/Spells.ps1)
constexpr uint32 SPELL_HAMMER_STRIKE = 92403;
constexpr std::array<Milliseconds, 3> HammerStrikes = { 250ms, 670ms, 1090ms };     // ItemForge.lua's hammer
constexpr Milliseconds QuenchLine = 1450ms;
constexpr float SmithReach = 40.0f;

// Worn forged gear smoulders: the ranks forged on everything worn, and the aura each count reaches
struct Embers
{
    uint32 ranks;
    uint32 spell;
};
constexpr std::array<Embers, 3> EmberTiers = { { { 24, 92400 }, { 64, 92401 }, { 112, 92402 } } };

// A forged weapon glows more with every rank: the look of a stock enchantment, shown in the visible item's temporary
// enchantment (a real temporary enchantment, a poison or an oil, keeps its own). Sharpened's shine, Fiery Weapon's
// embers, Flametongue's flames, then Crusader's golden radiance for a masterpiece.
uint32 GlowFor(uint32 rank)
{
    if (rank >= Mythic::ForgeRanks)
        return 1900;
    if (rank >= 5)
        return 5;
    if (rank >= 3)
        return 803;
    return rank >= 1 ? 13 : 0;
}

constexpr uint32 VisualUpdateMs = 2000;

enum ForgeError : uint8
{
    ERROR_ITEM_CHANGED = 1,     // not there any more, or not the same item
    ERROR_NOT_FORGEABLE = 2,
    ERROR_MAX_RANK = 3,
    ERROR_NO_GOLD = 4,
    ERROR_IN_COMBAT = 5,
    ERROR_DEAD = 6
};

// What the smith says: greeting a customer, and handing a piece back
std::array<char const*, 4> const Greetings = {
    "Ah, un client ! Posez donc ça sur l'enclume, qu'on voie de quel métal c'est fait.",
    "Le feu est chaud et le marteau est prêt. Qu'est-ce que je vous forge aujourd'hui ?",
    "Montrez-moi ça. Il y a toujours moyen d'en tirer un peu plus.",
    "La Confrérie du thorium forge pour qui paie bien. Voyons votre équipement.",
};
std::array<char const*, 4> const Finished = {
    "Et voilà ! Plus solide qu'à l'arrivée.",
    "Du beau travail, si je puis me permettre.",
    "Refroidi, affûté, prêt à servir.",
    "Encore un passage et il fera pâlir les forges de Rochenoire.",
};
char const* const MasterworkLine = "Un coup de maître ! Deux rangs pour le prix d'un, ne le dites à personne.";
char const* const MasterpieceLine = "Un chef-d'œuvre. Je n'ai plus rien à y apprendre... et vous non plus.";

struct ForgeRecord
{
    uint32 rank = 0;            // a Mythic+ variant's forged ranks (a real item's is its entry)
    uint32 invested = 0;        // copper paid to forge it
};

// A character's dealings with the Forge. Kept on the player, so it dies with the session.
struct ForgeState : public DataMap::Base
{
    std::unordered_map<ObjectGuid::LowType, ForgeRecord> items;
    uint64 spent = 0;           // copper paid to the smith, in all
    ObjectGuid smith;           // the smith the window was opened at
    uint32 visualTimer = 0;
};

constexpr char const* StateKey = "ForgeState";

ForgeState* GetState(Player* player)
{
    return player->CustomData.GetDefault<ForgeState>(StateKey);
}

void LoadState(Player* player)
{
    ForgeState* state = GetState(player);
    state->items.clear();
    uint32 const guid = player->GetGUID().GetCounter();
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT f.item_guid, f.forge_rank, f.money_invested FROM character_item_forge f "
            "JOIN item_instance i ON i.guid = f.item_guid WHERE i.owner_guid = {}", guid))
        do
        {
            Field* field = result->Fetch();
            state->items[field[0].Get<uint32>()] = { field[1].Get<uint32>(), field[2].Get<uint32>() };
        } while (result->NextRow());

    QueryResult spent = CharacterDatabase.Query("SELECT money_spent FROM character_forge_patron WHERE guid = {}", guid);
    state->spent = spent ? spent->Fetch()[0].Get<uint64>() : 0;
}

uint32 StandingOf(uint64 spent)
{
    uint32 standing = 0;
    for (uint32 index = 0; index < Standings.size(); ++index)
        if (spent / GOLD >= Standings[index].gold)
            standing = index;
    return standing;
}

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

// An item's forged ranks
uint32 RankOf(ForgeState const* state, Item const* item)
{
    uint32 const entry = item->GetEntry();
    if (!Mythic::IsMythicGeneratedItem(entry))
        return Mythic::GetForgeRank(entry);
    auto const record = state->items.find(item->GetGUID().GetCounter());
    return record != state->items.end() ? record->second.rank : 0;
}

// The entry `ranks` ranks above an item's, 0 when there is none
uint32 EntryAbove(ForgeState const* state, Item const* item, uint32 ranks)
{
    uint32 const entry = item->GetEntry();
    uint32 const rank = RankOf(state, item) + ranks;
    if (rank > Mythic::ForgeRanks)
        return 0;

    uint32 next = 0;
    if (Mythic::IsMythicGeneratedItem(entry))
    {
        uint32 const variant = entry / Mythic::GeneratedItemBase - 1 + ranks;
        if (variant < Mythic::GeneratedItemVariants)
            next = Mythic::GetGeneratedItemEntry(Mythic::GetBaseItemEntry(entry), variant);
    }
    else
        next = Mythic::GetForgeItemEntry(Mythic::GetBaseItemEntry(entry), rank);
    return next && sObjectMgr->GetItemTemplate(next) ? next : 0;
}

// Whether the Forge takes an item at all: gear, and a real item the generator made ranks for or a Mythic+ variant
bool IsForgeable(Item const* item)
{
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR) ||
        proto->InventoryType == INVTYPE_NON_EQUIP)
        return false;
    return Mythic::IsMythicGeneratedItem(proto->ItemId) ||
        sObjectMgr->GetItemTemplate(Mythic::GetForgeItemEntry(Mythic::GetBaseItemEntry(proto->ItemId), 1));
}

uint32 GetCost(ForgeState const* state, ItemTemplate const* proto, uint32 rank)
{
    float const level = static_cast<float>(proto->ItemLevel) / static_cast<float>(MinItemLevel);
    float const gold = GoldAtBaseLevel * level * level * std::pow(RankGrowth, static_cast<float>(rank)) *
        (100.0f - static_cast<float>(Standings[StandingOf(state->spent)].discount)) / 100.0f;
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

void SendItem(Player* player, ForgeState const* state, Item* item)
{
    ItemTemplate const* proto = item->GetTemplate();
    uint32 const rank = RankOf(state, item);
    ItemTemplate const* next = sObjectMgr->GetItemTemplate(EntryAbove(state, item, 1));
    if (next)
        TeachItem(player, next->ItemId);
    auto const record = state->items.find(item->GetGUID().GetCounter());
    Send(player, Acore::StringFormat("I\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", item->GetBagSlot(), item->GetSlot(),
        proto->ItemId, rank, proto->ItemLevel, next ? next->ItemId : 0, next ? next->ItemLevel : 0,
        next ? GetCost(state, proto, rank) : 0, record != state->items.end() ? record->second.invested : 0));
}

// Every piece of the player's gear the Forge can take, worn first, then the bags
void SendList(Player* player, bool open)
{
    ForgeState* state = GetState(player);
    Send(player, "O");
    auto consider = [player, state](Item* item)
    {
        if (item && IsForgeable(item))
            SendItem(player, state, item);
    };

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        consider(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* container = player->GetBagByPos(bag))
            for (uint32 slot = 0; slot < container->GetBagSize(); ++slot)
                consider(container->GetItemByPos(uint8(slot)));

    Send(player, Acore::StringFormat("E\t{}\t{}\t{}\t{}", open ? 1 : 0, Mythic::ForgeRanks, state->spent,
        StandingOf(state->spent)));
}

void SendError(Player* player, ForgeError error)
{
    Send(player, Acore::StringFormat("X\t{}", uint32(error)));
}

// --- What forged gear looks like -----------------------------------------------------------------------------------

bool IsWeaponSlot(uint8 slot)
{
    return slot == EQUIPMENT_SLOT_MAINHAND || slot == EQUIPMENT_SLOT_OFFHAND || slot == EQUIPMENT_SLOT_RANGED;
}

void ShowGlow(Player* player, ForgeState const* state, uint8 slot, Item const* item)
{
    if (!item || item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return;
    uint32 const field = PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + slot * 2;
    uint16 const glow = uint16(GlowFor(RankOf(state, item)));
    if (player->GetUInt16Value(field, 1) != glow)
        player->SetUInt16Value(field, 1, glow);
}

// The weapons' glow, again (a poison or an oil wearing off clears it), and the embers the worn ranks reach
void UpdateVisuals(Player* player, ForgeState const* state)
{
    uint32 ranks = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            ranks += RankOf(state, item);
            if (IsWeaponSlot(slot))
                ShowGlow(player, state, slot, item);
        }

    uint32 wanted = 0;
    for (Embers const& tier : EmberTiers)
        if (ranks >= tier.ranks)
            wanted = tier.spell;
    for (Embers const& tier : EmberTiers)
        if (tier.spell != wanted && player->HasAura(tier.spell))
            player->RemoveAurasDueToSpell(tier.spell);
    if (wanted && !player->HasAura(wanted) && player->IsAlive())
        player->AddAura(wanted, player);
}

// --- The smith at work ---------------------------------------------------------------------------------------------

Creature* SmithNear(Player* player, ForgeState const* state)
{
    if (state->smith.IsEmpty())
        return nullptr;
    Creature* smith = ObjectAccessor::GetCreature(*player, state->smith);
    return smith && smith->IsAlive() && smith->IsWithinDistInMap(player, SmithReach) ? smith : nullptr;
}

// Three strikes of the hammer, in time with the window's, and a word once the piece is out of the quench
void SmithWorks(Creature* smith, char const* line)
{
    for (Milliseconds when : HammerStrikes)
        smith->m_Events.AddEventAtOffset([smith]()
        {
            smith->CastSpell(smith, SPELL_HAMMER_STRIKE, true);
        }, when);
    std::string const text = line;
    smith->m_Events.AddEventAtOffset([smith, text]()
    {
        smith->Say(text, LANG_UNIVERSAL);
    }, QuenchLine);
}

// --- Forging -------------------------------------------------------------------------------------------------------

// The item becomes its new rank where it lies: its enchants, gems and binding stay; worn, its stats go and come back
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
    if (!IsForgeable(item))
        return SendError(player, ERROR_NOT_FORGEABLE);

    ForgeState* state = GetState(player);
    uint32 const rank = RankOf(state, item);
    uint32 const nextEntry = EntryAbove(state, item, 1);
    if (!nextEntry)
        return SendError(player, ERROR_MAX_RANK);

    ItemTemplate const* proto = item->GetTemplate();
    uint32 const cost = GetCost(state, proto, rank);
    if (!player->HasEnoughMoney(cost))
        return SendError(player, ERROR_NO_GOLD);

    // A masterwork: the smith strikes true, and the piece takes two ranks for the price of one
    uint32 const standingBefore = StandingOf(state->spent);
    uint32 const masterworkEntry = EntryAbove(state, item, 2);
    bool const masterwork = masterworkEntry && roll_chance_i(int32(Standings[standingBefore].masterwork));
    ItemTemplate const* next = sObjectMgr->GetItemTemplate(masterwork ? masterworkEntry : nextEntry);
    uint32 const newRank = rank + (masterwork ? 2 : 1);

    player->ModifyMoney(-int32(cost));
    TeachItem(player, next->ItemId);
    Reforge(player, item, next);

    ObjectGuid::LowType const itemGuid = item->GetGUID().GetCounter();
    ForgeRecord& record = state->items[itemGuid];
    record.rank = newRank;
    record.invested += cost;
    state->spent += cost;
    CharacterDatabase.Execute(
        "REPLACE INTO character_item_forge (item_guid, forge_rank, money_invested) VALUES ({}, {}, {})",
        itemGuid, record.rank, record.invested);
    CharacterDatabase.Execute("REPLACE INTO character_forge_patron (guid, money_spent) VALUES ({}, {})",
        player->GetGUID().GetCounter(), state->spent);
    // Saved now: the gold is gone, the item must not come back unforged after a crash
    player->SaveToDB(false, false);
    UpdateVisuals(player, state);

    uint32 const standing = StandingOf(state->spent);
    LOG_INFO("module", "Forge: {} forged item {} ({} -> {}, rank {}{}) for {} copper", player->GetName(), itemGuid,
        proto->ItemId, next->ItemId, newRank, masterwork ? ", masterwork" : "", cost);

    if (Creature* smith = SmithNear(player, state))
        SmithWorks(smith, newRank >= Mythic::ForgeRanks ? MasterpieceLine :
            masterwork ? MasterworkLine : Finished[urand(0, Finished.size() - 1)]);

    Send(player, Acore::StringFormat("D\t{}\t{}\t{}\t{}\t{}\t{}\t{}", bag, slot, next->ItemId, newRank,
        next->ItemLevel, masterwork ? 1 : 0, standing != standingBefore ? standing : 0));
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

// --- Scripts -------------------------------------------------------------------------------------------------------

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
    ForgePlayerScript() : PlayerScript("ForgePlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_AFTER_SET_VISIBLE_ITEM_SLOT,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        LoadState(player);
        UpdateVisuals(player, GetState(player));
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        ForgeState* state = GetState(player);
        if (state->visualTimer > diff)
        {
            state->visualTimer -= diff;
            return;
        }
        state->visualTimer = VisualUpdateMs;
        UpdateVisuals(player, state);
    }

    // A weapon put on, or its visible enchantments rewritten: its glow goes back on
    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override
    {
        if (IsWeaponSlot(slot) && player->IsInWorld())
            ShowGlow(player, GetState(player), slot, item);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        if (language != LANG_ADDON || message.size() <= Prefix.size() || !message.starts_with(Prefix) ||
            message[Prefix.size()] != '\t')
            return;
        HandleMessage(player, std::string_view(message).substr(Prefix.size() + 1));
    }
};

// A deleted item takes its record with it
class ForgeGlobalScript : public GlobalScript
{
public:
    ForgeGlobalScript() : GlobalScript("ForgeGlobalScript", { GLOBALHOOK_ON_ITEM_DEL_FROM_DB }) { }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        transaction->Append("DELETE FROM character_item_forge WHERE item_guid = {}", itemGuid);
    }
};

// The master smith at his anvil: talking to him opens the Forge, and he is the one who works the piece
class npc_forge_master : public CreatureScript
{
public:
    npc_forge_master() : CreatureScript("npc_forge_master") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        CloseGossipMenuFor(player);
        GetState(player)->smith = creature->GetGUID();
        creature->Say(Greetings[urand(0, Greetings.size() - 1)], LANG_UNIVERSAL, player);
        SendList(player, true);
        return true;
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

    // Away from the anvil: the window, without the smith
    static bool HandleForgeCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        GetState(player)->smith.Clear();
        SendList(player, true);
        return true;
    }
};
}

void AddForgeScripts()
{
    new ForgeWorldScript();
    new ForgePlayerScript();
    new ForgeGlobalScript();
    new npc_forge_master();
    new ForgeCommandScript();
}
