#include "PrestigeSystem.h"

#include "AutoLearnSpellsSystem.h"
#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Mail.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ParagonSystem.h"
#include "PersonalLootSystem.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StatGrowthConfig.h"
#include "StringFormat.h"
#include "Trainer.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
constexpr std::string_view Prefix = "Prestige";
constexpr uint32 PrestigeKeeperEntry = 900103;
constexpr uint8 LevelOne = 1;

enum class PrestigeBlock : uint8
{
    None = 0,
    Level = 1,
    Combat = 2,
    Dead = 3,
    Instance = 4
};

bool IsFrench(Player* player)
{
    return player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
}

void Send(Player* player, std::string const& body)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;

    WorldPacket packet;
    std::string const payload = std::string(Prefix) + "\t" + body;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

PrestigeBlock BlockReason(Player* player)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot() || !player->IsInWorld())
        return PrestigeBlock::Level;

    if (player->GetLevel() < sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
        return PrestigeBlock::Level;
    if (!player->IsAlive())
        return PrestigeBlock::Dead;
    if (player->IsInCombat())
        return PrestigeBlock::Combat;
    if (player->IsBeingTeleported() || (player->GetMap() && player->GetMap()->Instanceable()))
        return PrestigeBlock::Instance;
    return PrestigeBlock::None;
}

void SendState(Player* player)
{
    uint32 const prestige = GetParagonPrestige(player);
    uint32 const cap = GetParagonPointCap(player);
    uint32 const per = statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointsPerPrestige);
    uint32 const nextCap = per > (std::numeric_limits<uint32>::max() - cap) ? cap : cap + per;
    PrestigeBlock const block = BlockReason(player);
    Send(player, Acore::StringFormat("STATE\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
        prestige, cap, nextCap, GetParagonEarned(player), GetParagonSpent(player),
        player->GetLevel(), block == PrestigeBlock::None ? 1 : 0, static_cast<uint8>(block)));
}

bool ShouldStrip(Item const* item)
{
    ItemTemplate const* proto = item ? item->GetTemplate() : nullptr;
    if (!proto || proto->Quality == ITEM_QUALITY_HEIRLOOM)
        return false;
    return proto->RequiredLevel > LevelOne;
}

// Bags first. What does not fit is mailed in the same transaction as the level save, in batches of 12.
uint32 StoreOrCollect(Player* player, std::vector<Item*>& overflow)
{
    uint32 mailed = 0;
    bool moved = true;
    while (moved)
    {
        moved = false;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!ShouldStrip(item))
                continue;

            ItemPosCountVec destination;
            if (player->CanStoreItem(NULL_BAG, NULL_SLOT, destination, item, false) != EQUIP_ERR_OK)
                continue;

            RemovePersonalLootItem(player, item);
            player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
            player->StoreItem(destination, item, true);
            moved = true;
        }
    }

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!ShouldStrip(item))
            continue;
        RemovePersonalLootItem(player, item);
        overflow.push_back(item);
        ++mailed;
    }
    return mailed;
}

void AttachToMail(Player* player, CharacterDatabaseTransaction trans, MailDraft& draft, Item* item)
{
    item->SetNotRefundable(player, true, &trans);
    player->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
    item->DeleteFromInventoryDB(trans);
    if (item->GetState() == ITEM_UNCHANGED)
        item->FSetState(ITEM_CHANGED);
    item->SetOwnerGUID(player->GetGUID());
    item->SaveToDB(trans);
    draft.AddItem(item);
}

void MailOverflow(Player* player, CharacterDatabaseTransaction trans, std::vector<Item*> const& overflow)
{
    bool const french = IsFrench(player);
    std::string const subject = french ? "Prestige" : "Prestige";
    std::string const body = french
        ? "Cet équipement demandait un niveau que vous n'avez plus. Il vous attend ici."
        : "This gear required a level you no longer have. It is waiting here.";

    for (std::size_t start = 0; start < overflow.size(); start += MAX_MAIL_ITEMS)
    {
        MailDraft draft(subject, body);
        std::size_t const end = std::min(overflow.size(), start + MAX_MAIL_ITEMS);
        for (std::size_t index = start; index < end; ++index)
            AttachToMail(player, trans, draft, overflow[index]);
        draft.SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()),
            MailSender(MAIL_CREATURE, PrestigeKeeperEntry), MAIL_CHECK_MASK_HAS_BODY);
    }
}

void RemoveTrainerSpellsAbove(Player* player, uint8 level)
{
    std::unordered_set<uint32> remove;
    for (Trainer::Trainer const* trainer : sObjectMgr->GetClassTrainers(player->getClass()))
    {
        if (!trainer || !trainer->IsTrainerValidForPlayer(player))
            continue;

        for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
        {
            if (trainerSpell.ReqLevel <= level || !player->HasSpell(trainerSpell.SpellId))
                continue;

            remove.insert(trainerSpell.SpellId);
            SpellInfo const* info = sSpellMgr->GetSpellInfo(trainerSpell.SpellId);
            if (!info)
                continue;
            for (SpellEffectInfo const& effect : info->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && effect.TriggerSpell)
                    remove.insert(effect.TriggerSpell);
        }
    }

    for (uint32 spellId : remove)
        player->removeSpell(spellId, SPEC_MASK_ALL, false);
}

void ClearGlyphs(Player* player)
{
    for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
    {
        uint32 const glyph = player->GetGlyph(slot);
        if (!glyph)
            continue;
        if (GlyphPropertiesEntry const* entry = sGlyphPropertiesStore.LookupEntry(glyph))
            player->RemoveAurasDueToSpell(entry->SpellId);
        player->SetGlyph(slot, 0, true);
    }
}

void ResetSpecs(Player* player)
{
    uint8 const original = player->GetActiveSpec();
    uint8 const specs = std::min<uint8>(player->GetSpecsCount(), MAX_TALENT_SPECS);
    for (uint8 spec = 0; spec < specs; ++spec)
    {
        if (player->GetActiveSpec() != spec)
            player->ActivateSpec(spec);
        player->resetTalents(true);
        ClearGlyphs(player);
    }
    if (specs && player->GetActiveSpec() != original)
        player->ActivateSpec(original);
}

// What a character actually opens the game with.
//
// AutoLearnClassSpells only gives back what a class trainer teaches, and a character's first abilities are
// not on any trainer's list: Attack, the class's opening strike, and its weapon and armour proficiencies all
// come from the creation tables. So a prestiged character came out of it with an empty spellbook - it had
// been stripped of everything above level 1 and handed back only the handful a level 1 trainer offers.
//
// The gear is the same story from the other side. Prestige leaves the character holding equipment it is now
// far too low to wear, so it has no weapon at all, and most abilities need one. These are the same items a
// freshly made character of this race and class is given.
uint32 GrantOpeningKit(Player* player)
{
    player->LearnDefaultSkills();
    player->LearnCustomSpells();

    PlayerInfo const* info = sObjectMgr->GetPlayerInfo(player->getRace(), player->getClass());
    if (!info)
        return 0;

    uint32 granted = 0;
    for (PlayerCreateInfoItem const& item : info->item)
        if (player->StoreNewItemInBestSlots(item.item_id, item.item_amount))
            ++granted;

    return granted;
}

void Announce(Player* player, uint32 cap, uint32 available, uint32 mailed)
{
    ChatHandler chat(player->GetSession());
    bool const french = IsFrench(player);
    chat.PSendSysMessage(french
        ? "|cffa335eePrestige {}.|r |cff00ff00Niveau 1. Plafond de parangon : {}.|r"
        : "|cffa335eePrestige {}.|r |cff00ff00Level 1. Paragon cap: {}.|r",
        GetParagonPrestige(player), cap);
    if (available)
        chat.PSendSysMessage(french
            ? "|cff00ff00{} point(s) de parangon sont maintenant disponibles.|r"
            : "|cff00ff00{} paragon point(s) are now available.|r", available);
    if (mailed)
        chat.SendSysMessage(french
            ? "|cff888888L'équipement qui ne rentrait pas dans vos sacs a été envoyé par courrier.|r"
            : "|cff888888Gear that did not fit in your bags was sent to your mailbox.|r");
}

// Paragon points handed over for the run that got the character here, on top of anything it banked.
constexpr uint32 PRESTIGE_PARAGON_REWARD = 5;

bool PerformPrestige(Player* player)
{
    if (BlockReason(player) != PrestigeBlock::None)
        return false;

    // Off while the armour the percent was taken from is still what the character is wearing.
    SuspendParagon(player);

    std::vector<Item*> overflow;
    uint32 const mailed = StoreOrCollect(player, overflow);
    RemoveTrainerSpellsAbove(player, LevelOne);
    ResetSpecs(player);

    SetParagonPrestige(player, GetParagonPrestige(player) + 1);
    player->GiveLevel(LevelOne);
    if (player->GetLevel() != LevelOne)
    {
        SetParagonPrestige(player, GetParagonPrestige(player) - 1);
        RestoreParagon(player);
        return false;
    }

    player->SetUInt32Value(PLAYER_XP, 0);
    player->SetRestBonus(0);
    RestoreParagon(player);
    AutoLearnClassSpells(player);
    GrantOpeningKit(player);
    player->UpdateAllStats();

    // Paid before the numbers below are read, so the frame and the message that follow already count them
    AwardParagonPoints(player, PRESTIGE_PARAGON_REWARD, "prestige");

    uint32 const cap = GetParagonPointCap(player);
    uint32 const earned = GetParagonEarned(player);
    uint32 const spent = GetParagonSpent(player);
    uint32 const usable = std::min(earned, cap);
    uint32 const available = usable > spent ? usable - spent : 0;

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    MailOverflow(player, trans, overflow);
    SaveParagonPoints(player, trans);
    player->SaveToDB(trans, false, false);
    CharacterDatabase.CommitTransaction(trans);

    Announce(player, cap, available, mailed);
    Send(player, Acore::StringFormat("DONE\t{}\t{}", GetParagonPrestige(player), cap));
    return true;
}

class npc_stat_growth_prestige_keeper : public CreatureScript
{
public:
    npc_stat_growth_prestige_keeper() : CreatureScript("npc_stat_growth_prestige_keeper") { }

    bool OnGossipHello(Player* player, Creature* /*creature*/) override
    {
        CloseGossipMenuFor(player);
        SendPrestigeWindow(player);
        return true;
    }
};

class PrestigePlayerScript : public PlayerScript
{
public:
    PrestigePlayerScript() : PlayerScript("PrestigePlayerScript", {
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_CAN_GIVE_MAIL_REWARD_AT_GIVE_LEVEL
    }) { }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32&, uint32& language, std::string& message) override
    {
        HandlePrestigeAddonMessage(player, language, message);
    }

    // The level 20, 40, 60 and 70 letters already paid on the first trip to 80.
    bool OnPlayerCanGiveMailRewardAtGiveLevel(Player* player, uint8) override
    {
        return GetParagonPrestige(player) == 0;
    }
};
}

void SendPrestigeWindow(Player* player)
{
    SendState(player);
}

void HandlePrestigeAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON || !message.starts_with(Prefix))
        return;

    std::string_view body(message);
    body.remove_prefix(Prefix.size());
    if (body.empty() || body.front() != '\t')
        return;
    body.remove_prefix(1);

    if (body == "OPEN")
    {
        SendState(player);
        return;
    }

    if (body != "PRESTIGE")
        return;

    PrestigeBlock const block = BlockReason(player);
    if (block != PrestigeBlock::None)
    {
        Send(player, Acore::StringFormat("ERROR\t{}", static_cast<uint8>(block)));
        return;
    }

    if (!PerformPrestige(player))
        Send(player, Acore::StringFormat("ERROR\t{}", static_cast<uint8>(PrestigeBlock::Level)));
}

void AddPrestigeScripts()
{
    new npc_stat_growth_prestige_keeper();
    new PrestigePlayerScript();
}
