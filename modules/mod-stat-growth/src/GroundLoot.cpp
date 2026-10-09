#include "GroundLoot.h"
#include "EvolutionsAudio.h"
#include "InfiniteDungeonSystem.h"
#include "MythicDungeonSystem.h"

#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "DataMap.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "MoveSpline.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <vector>

// mod-playerbots (RaidFinder.cpp): the boss entry a Défi's instance is against, 0 when map is no Défi's, and its tier
uint32 GetChallengeBossOf(Map const* map);
uint8 GetChallengeTierOf(Map const* map);

namespace
{
// The drops (stat_growth_ground_loot.sql): a bag, a pile of gold, and the light beam over either, whose display is
// that of the drop's quality (localTools/patchSinisterStrike.ps1: 60004 on, white, green, blue, purple, orange, gold,
// red for a Unique)
constexpr uint32 NPC_GROUND_LOOT = 900120;
constexpr uint32 NPC_GROUND_GOLD = 900121;
constexpr uint32 NPC_GROUND_LOOT_BEAM = 900122;
constexpr uint32 DISPLAY_BEAM_FIRST = 60004;
constexpr uint8 BeamGold = 5;
constexpr uint8 BeamUnique = 6;     // quality 6, the Unique's red (the client extension recolours the quality)

// Their sparkles, landing and picked up (localTools/groundLoot/ascensionVisuals.json)
constexpr uint32 KIT_LAND = 81911;
constexpr uint32 KIT_PICKUP = 81913;
// Diablo IV's loot sounds, played by our own sound engine to the owner alone (EvolutionsAudio.h; the bank:
// client-assets/audio/groundLoot.json): from the drop, a flip as it jumps and a landing - a white item, a green, a
// blue, an epic, an epic with a power of its own (touched by L'Infini or the Hollow Voice) or a Unique, a legendary,
// gold - then its quality's loop as long as it lies there; picked up, an item's or gold's, where the player is.
constexpr char const* SoundFlip = "GroundLoot.Flip";
constexpr char const* SoundLandCommon = "GroundLoot.LandCommon";
constexpr char const* SoundLandMagic = "GroundLoot.LandMagic";
constexpr char const* SoundLand = "GroundLoot.Land";
constexpr char const* SoundLandEpic = "GroundLoot.LandEpic";
constexpr char const* SoundLandUnique = "GroundLoot.LandUnique";
constexpr char const* SoundLandLegendary = "GroundLoot.LandLegendary";
constexpr char const* SoundLandGold = "GroundLoot.LandGold";
constexpr char const* SoundPickup = "GroundLoot.Pickup";
constexpr char const* SoundPickupGold = "GroundLoot.PickupGold";
// By the beam's colour (BeamIndex): white, green, blue, purple, orange, gold, red
constexpr char const* SoundLoops[] = { "GroundLoot.Loop.Normal", "GroundLoot.Loop.Magic", "GroundLoot.Loop.Rare",
                                       "GroundLoot.Loop.Legendary", "GroundLoot.Loop.Legendary",
                                       "GroundLoot.Loop.Gold", "GroundLoot.Loop.Legendary" };

// The burst: this long after the kill the first drop leaves the corpse, the next ones one after the other. Each
// appears inside the corpse and jumps a moment later (the client has it by then), in an arc of that height, flying
// about that long (the jump's own spline says how long exactly: the landing is timed on it), around the corpse past
// its reach. Each drop flips out with its own sound as it jumps, and lands with its own that much ahead of the
// touchdown (the sound reaches the client a moment after): every landing comes right after its own flip, never before.
// (One flip for the whole burst, on the first jump, could be heard after a landing: 2026-10-06, the Hollow Voice.)
constexpr uint64 BurstDelayMs = 2000;
constexpr uint64 ThrowIntervalMs = 300;
constexpr uint64 JumpDelayMs = 250;
constexpr uint64 FlightMs = 900;
constexpr uint64 LandSoundLeadMs = 50;
constexpr float JumpSpeedZ = 10.0f;
constexpr float OriginHeight = 1.0f;
constexpr float MaxCorpseReach = 8.0f;
constexpr float MinThrowDistance = 2.0f;
constexpr float MaxThrowDistance = 5.5f;
// Golden angle: drops thrown one after the other spread evenly all around the corpse
constexpr float ThrowAngleStep = 2.39996f;
constexpr float ThrowAngleJitter = 0.35f;

// A boss that drops little still throws a handful: a player with fewer drops than this from the corpse gets the rest in
// piles of gold, their share of its gold and some more - a Défi's (more at its higher tiers), a dungeon's (more at
// higher keys) - shared out unevenly among the piles
constexpr uint32 MinDropsDefi = 6;
constexpr uint32 MinDropsDungeon = 4;
constexpr uint32 DefiBaseGold = 10 * GOLD;
constexpr uint32 DefiTierGold = 5 * GOLD;
constexpr uint32 DungeonBaseGold = 4 * GOLD;
constexpr uint32 DungeonKeyGold = 1 * GOLD;
constexpr float PileWeightMin = 0.5f;
constexpr float PileWeightMax = 1.5f;

// Walked over within this distance, a drop is picked up; left this long on the floor, it goes to the bags
constexpr float PickupRange = 2.5f;
constexpr float PickupHeight = 3.0f;
constexpr uint64 LifetimeMs = 180000;
// A mythic item given this long after a kill is thrown out with its loot
constexpr uint64 ThrowWindowMs = 20000;
constexpr uint32 UpdateIntervalMs = 50;
constexpr uint64 FullBagsNoticeMs = 5000;

constexpr char const* StateKey = "GroundLoot";
constexpr char const* CorpseKey = "GroundLootCorpse";
// The tooltip of a drop (client: FrameXML GroundLoot.lua): "GLOOT\t<drop guid>\t<item link | gold:<copper> | ->"
constexpr char const* AddonPrefix = "GLOOT";

enum class DropKind : uint8
{
    Item,    // one of the corpse's
    Mythic,  // a mythic item the boss gives on top (MythicDungeonSystem.cpp), made as it is picked up
    Gold,    // a share of the corpse's gold
};

struct Drop
{
    DropKind kind = DropKind::Item;
    uint32 itemId = 0;
    uint32 count = 1;
    int32 randomPropertyId = 0;
    ItemTemplate const* mythic = nullptr;
    std::function<void(Item*)> touch;
    uint32 gold = 0;
    uint8 quality = 0;
    bool unique = false;  // a mythic item touched as it is made: a power of its own
    bool fromCorpse = false;  // one of the corpse's: picked up as looting it would (the loot hooks told)
    bool test = false;  // .groundloot's: looks, sounds and picks up as any, gives nothing
    ObjectGuid corpse;
    Position origin;
    Position landing;
    uint64 throwAt = 0;
    uint64 jumpAt = 0;
    uint64 landAt = 0;
    uint64 fadesAt = 0;
    ObjectGuid bag;
    ObjectGuid beam;
    bool jumped = false;
    bool landSoundPlayed = false;
    bool landed = false;
};

// A player's loot on the floor, and the last kill that threw some (more can join it: Throw)
struct GroundLootState : public DataMap::Base
{
    uint32 mapId = 0;
    uint32 instanceId = 0;
    ObjectGuid corpse;
    Position origin;
    float reach = 0.0f;
    uint64 openedAt = 0;
    uint64 nextThrowAt = 0;
    uint32 thrown = 0;
    uint32 updateTimer = 0;
    uint64 fullNoticeAt = 0;
    std::vector<Drop> drops;
};

struct GroundLootCorpse : public DataMap::Base
{
    bool opened = false;
};

uint64 NowMs()
{
    return static_cast<uint64>(GameTime::GetGameTimeMS().count());
}

bool IsRealPlayer(Player const* player)
{
    return player && player->GetSession() && !player->GetSession()->IsBot();
}

bool Qualifies(Creature const* corpse)
{
    Map const* map = corpse->GetMap();
    if (!map->IsDungeon() || corpse->IsPet() || InfiniteDungeon::IsFloorCreature(corpse))
        return false;
    if (uint32 const boss = GetChallengeBossOf(map))
        return boss == corpse->GetEntry();
    return map->IsNonRaidDungeon() && IsMythicDungeonBoss(corpse);
}

void Queue(GroundLootState& state, Drop drop)
{
    drop.corpse = state.corpse;
    drop.origin = state.origin;
    drop.throwAt = std::max({ NowMs(), state.nextThrowAt, state.openedAt + BurstDelayMs });
    state.nextThrowAt = drop.throwAt + ThrowIntervalMs;
    state.drops.push_back(std::move(drop));
}

void SendKit(Player* player, ObjectGuid target, uint32 kit)
{
    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 8 + 4);
    data << target;
    data << uint32(kit);
    player->SendDirectMessage(&data);
}

void SendTooltip(Player* player, ObjectGuid bag, std::string const& what)
{
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                                 Acore::StringFormat("{}\t{:016X}\t{}", AddonPrefix, bag.GetRawValue(), what));
    player->SendDirectMessage(&packet);
}

std::string Describe(Drop const& drop)
{
    switch (drop.kind)
    {
        case DropKind::Gold:
            return Acore::StringFormat("gold:{}", drop.gold);
        case DropKind::Mythic:
            return Acore::StringFormat("item:{}:0:0:0:0:0:0", drop.mythic->ItemId);
        default:
            return Acore::StringFormat("item:{}:0:0:0:0:0:{}", drop.itemId, drop.randomPropertyId);
    }
}

// The beam's colour: grey and white items white, then green, blue, purple, orange for a legendary, red for a Unique
// (quality 6: it showed orange, as a legendary); gold its own
uint8 BeamIndex(Drop const& drop)
{
    if (drop.kind == DropKind::Gold)
        return BeamGold;
    if (drop.quality >= ITEM_QUALITY_ARTIFACT)
        return BeamUnique;
    if (drop.quality <= ITEM_QUALITY_NORMAL)
        return 0;
    return static_cast<uint8>(std::min<uint32>(drop.quality - ITEM_QUALITY_NORMAL, 4));
}

uint32 DropItemId(Drop const& drop)
{
    return drop.kind == DropKind::Mythic ? drop.mythic->ItemId : drop.itemId;
}

void MailCorpseItem(Player* player, Drop const& drop)
{
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    MailDraft draft("Loot", "Your bags were full: here is what a boss left you.");
    if (Item* item = Item::CreateItem(drop.itemId, drop.count, player))
    {
        if (drop.randomPropertyId)
            item->SetItemRandomProperties(drop.randomPropertyId);
        item->SaveToDB(transaction);
        draft.AddItem(item);
    }
    draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()),
        MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
    CharacterDatabase.CommitTransaction(transaction);
    if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(drop.itemId))
        ChatHandler(player->GetSession()).PSendSysMessage("Your bags are full: {} was sent to your mailbox.",
                                                          itemTemplate->Name1);
}

// The drop, as looting it from the corpse would have given it: into the bags (the mailbox when full), the loot hooks
// told (the essences consumed, the personal loot bonuses rolled), the gold with the "You loot" line
void Give(Player* player, Drop const& drop)
{
    if (drop.test)
        return;
    switch (drop.kind)
    {
        case DropKind::Gold:
        {
            uint32 const gold = player->CalculateMoneyReward(drop.gold);
            player->ModifyMoney(gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, gold);
            WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
            data << uint32(gold);
            data << uint8(1);
            player->SendDirectMessage(&data);
            sScriptMgr->OnPlayerAfterCreatureLootMoney(player);
            sScriptMgr->OnLootMoney(player, drop.gold);
            break;
        }
        case DropKind::Mythic:
            StoreMythicItem(player, drop.mythic, drop.touch);
            break;
        default:
        {
            ItemPosCountVec destination;
            if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, drop.itemId, drop.count) != EQUIP_ERR_OK)
            {
                MailCorpseItem(player, drop);
                break;
            }
            if (Item* item = player->StoreNewItem(destination, drop.itemId, true, drop.randomPropertyId))
            {
                player->SendNewItem(item, drop.count, false, false, true);
                if (drop.fromCorpse)
                {
                    player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, drop.itemId, drop.count);
                    sScriptMgr->OnPlayerLootItem(player, item, drop.count, drop.corpse);
                }
            }
            break;
        }
    }
}

void Despawn(Player* player, Drop const& drop)
{
    if (Creature* bag = drop.bag ? ObjectAccessor::GetCreature(*player, drop.bag) : nullptr)
        EvolutionsAudio::StopOn(player, bag);
    for (ObjectGuid const& guid : { drop.bag, drop.beam })
        if (Creature* creature = guid ? ObjectAccessor::GetCreature(*player, guid) : nullptr)
            creature->DespawnOrUnsummon();
    if (drop.bag)
        SendTooltip(player, drop.bag, "-");
}

// Everything still on the floor or about to be, to the bags: the player left, or logs out. Their bags and beams are
// left to fade (only they could see them).
void GiveAll(Player* player, GroundLootState& state)
{
    for (Drop const& drop : state.drops)
        Give(player, drop);
    state.drops.clear();
}

// The drop leaves the corpse: a bag (or gold) inside it, which jumps out a moment later to a spot around it
void Spawn(Player* player, GroundLootState& state, Drop& drop, uint64 now)
{
    Map* map = player->GetMap();
    float const angle = state.thrown++ * ThrowAngleStep + frand(-ThrowAngleJitter, ThrowAngleJitter);
    float const distance = state.reach + frand(MinThrowDistance, MaxThrowDistance);
    float x = drop.origin.GetPositionX() + distance * std::cos(angle);
    float y = drop.origin.GetPositionY() + distance * std::sin(angle);
    float z = drop.origin.GetPositionZ();
    map->CheckCollisionAndGetValidCoords(player, drop.origin.GetPositionX(), drop.origin.GetPositionY(),
                                         drop.origin.GetPositionZ(), x, y, z, false);
    float const ground = map->GetHeight(player->GetPhaseMask(), x, y, z + 2.0f);
    if (ground > INVALID_HEIGHT)
        z = ground;
    drop.landing.Relocate(x, y, z, angle);

    Position from = drop.origin;
    from.SetOrientation(angle);
    TempSummon* bag = player->SummonCreature(drop.kind == DropKind::Gold ? NPC_GROUND_GOLD : NPC_GROUND_LOOT, from,
                                             TEMPSUMMON_TIMED_DESPAWN, uint32(LifetimeMs * 2), 0, nullptr, true);
    if (!bag)
    {
        // Nowhere to put it: straight to the bags
        Give(player, drop);
        drop.landed = true;
        drop.fadesAt = now;
        return;
    }
    drop.bag = bag->GetGUID();
    drop.jumpAt = now + JumpDelayMs;
    SendTooltip(player, drop.bag, Describe(drop));
}

char const* LandSound(Drop const& drop)
{
    if (drop.kind == DropKind::Gold)
        return SoundLandGold;
    if (drop.quality >= ITEM_QUALITY_ARTIFACT)
        return SoundLandUnique;
    if (drop.quality == ITEM_QUALITY_LEGENDARY)
        return SoundLandLegendary;
    if (drop.unique)
        return SoundLandUnique;
    switch (drop.quality)
    {
        case ITEM_QUALITY_EPIC:     return SoundLandEpic;
        case ITEM_QUALITY_RARE:     return SoundLand;
        case ITEM_QUALITY_UNCOMMON: return SoundLandMagic;
        default:                    return SoundLandCommon;
    }
}

void Land(Player* player, Creature* bag, Drop& drop, uint64 now)
{
    drop.landed = true;
    drop.fadesAt = now + LifetimeMs;
    if (TempSummon* beam = player->SummonCreature(NPC_GROUND_LOOT_BEAM, drop.landing, TEMPSUMMON_TIMED_DESPAWN,
                                                  uint32(LifetimeMs * 2), 0, nullptr, true))
    {
        beam->SetDisplayId(DISPLAY_BEAM_FIRST + BeamIndex(drop));
        drop.beam = beam->GetGUID();
    }
    SendKit(player, bag->GetGUID(), KIT_LAND);
    // On the bag, under its beam: the bag was in the player's sight long before (the beam may not be yet)
    EvolutionsAudio::PlayOn(player, SoundLoops[BeamIndex(drop)], bag);
}

bool HasRoomFor(Player* player, Drop const& drop)
{
    if (drop.kind == DropKind::Gold || drop.test)
        return true;
    ItemPosCountVec destination;
    return player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, DropItemId(drop),
                                   drop.kind == DropKind::Mythic ? 1 : drop.count) == EQUIP_ERR_OK;
}

// Picked up: false when it stays on the floor (bags full - it waits for room, or to fade into the mailbox)
bool PickUp(Player* player, GroundLootState& state, Drop const& drop)
{
    if (!HasRoomFor(player, drop))
    {
        uint64 const now = NowMs();
        if (now >= state.fullNoticeAt)
        {
            state.fullNoticeAt = now + FullBagsNoticeMs;
            player->SendEquipError(EQUIP_ERR_INVENTORY_FULL, nullptr, nullptr, DropItemId(drop));
        }
        return false;
    }
    Give(player, drop);
    SendKit(player, player->GetGUID(), KIT_PICKUP);
    EvolutionsAudio::Play(player, drop.kind == DropKind::Gold ? SoundPickupGold : SoundPickup);
    return true;
}

bool InReach(Player* player, Creature const* bag)
{
    return player->IsAlive() && player->GetExactDist2d(bag) <= PickupRange &&
        std::fabs(player->GetPositionZ() - bag->GetPositionZ()) < PickupHeight;
}

void Update(Player* player, uint32 diff)
{
    GroundLootState* state = player->CustomData.Get<GroundLootState>(StateKey);
    if (!state || state->drops.empty())
        return;
    state->updateTimer += diff;
    if (state->updateTimer < UpdateIntervalMs)
        return;
    state->updateTimer = 0;

    Map* map = player->GetMap();
    if (!player->IsInWorld() || !map || map->GetId() != state->mapId || map->GetInstanceId() != state->instanceId)
    {
        GiveAll(player, *state);
        return;
    }

    uint64 const now = NowMs();
    for (auto itr = state->drops.begin(); itr != state->drops.end();)
    {
        Drop& drop = *itr;
        if (!drop.bag && !drop.landed)
        {
            if (now >= drop.throwAt)
                Spawn(player, *state, drop, now);
            ++itr;
            continue;
        }

        Creature* bag = drop.bag ? ObjectAccessor::GetCreature(*player, drop.bag) : nullptr;
        if (!bag)
        {
            // Gone (or never spawned, and already given): what it held is not lost
            if (drop.bag)
                Give(player, drop);
            Despawn(player, drop);
            itr = state->drops.erase(itr);
            continue;
        }

        if (!drop.jumped && now >= drop.jumpAt)
        {
            drop.jumped = true;
            float const distance = bag->GetExactDist2d(&drop.landing);
            bag->GetMotionMaster()->MoveJump(drop.landing, std::max(distance * 1000.0f / FlightMs, 1.0f),
                                              JumpSpeedZ);
            int32 const flight = bag->movespline ? bag->movespline->Duration() : 0;
            drop.landAt = now + (flight > 0 ? uint64(flight) : FlightMs);
            EvolutionsAudio::PlayOn(player, SoundFlip, bag);
        }
        if (drop.jumped && !drop.landSoundPlayed && now + LandSoundLeadMs >= drop.landAt)
        {
            drop.landSoundPlayed = true;
            EvolutionsAudio::PlayOn(player, LandSound(drop), bag);
        }
        if (drop.jumped && !drop.landed && now >= drop.landAt)
            Land(player, bag, drop, now);

        // Left too long on the floor it goes to the bags, else it waits for its owner to walk over it
        bool taken = false;
        if (drop.landed && now >= drop.fadesAt)
        {
            Give(player, drop);
            taken = true;
        }
        else if (drop.landed && InReach(player, bag))
            taken = PickUp(player, *state, drop);
        if (taken)
        {
            Despawn(player, drop);
            itr = state->drops.erase(itr);
            continue;
        }
        ++itr;
    }
}

// The corpse's items and gold, shared out among the real players it was killed by: each item to one of them (one
// who can use it if any), a free-for-all one to each, the gold in equal shares. What it held for a quest stays on it.
void ShareCorpseLoot(Creature* corpse, std::vector<Player*> const& players)
{
    Group const* group = corpse->GetLootRecipientGroup();
    Player const* recipient = corpse->GetLootRecipient();
    std::vector<Player*> owners;
    for (Player* player : players)
        if (player == recipient || (group && player->GetGroup() == group))
            owners.push_back(player);
    if (owners.empty())
        return;

    Loot& loot = corpse->loot;
    for (uint32 index = 0; index < loot.items.size(); ++index)
    {
        LootItem& item = loot.items[index];
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.itemid);
        if (item.is_looted || item.needs_quest || !itemTemplate)
            continue;

        std::vector<Player*> allowed;
        for (Player* owner : owners)
            if (item.AllowedForPlayer(owner, corpse->GetGUID()))
                allowed.push_back(owner);
        if (allowed.empty())
            continue;

        std::vector<Player*> takers;
        if (item.freeforall)
            takers = allowed;
        else
        {
            std::vector<Player*> users;
            for (Player* owner : allowed)
                if (owner->CanUseItem(itemTemplate) == EQUIP_ERR_OK)
                    users.push_back(owner);
            takers.push_back(Acore::Containers::SelectRandomContainerElement(users.empty() ? allowed : users));
        }

        for (Player* taker : takers)
        {
            Drop drop;
            drop.kind = DropKind::Item;
            drop.fromCorpse = true;
            drop.itemId = item.itemid;
            drop.count = item.count;
            drop.randomPropertyId = item.randomPropertyId;
            drop.quality = itemTemplate->Quality;
            Queue(*taker->CustomData.GetDefault<GroundLootState>(StateKey), std::move(drop));
        }

        item.is_looted = true;
        if (loot.unlootedCount)
            --loot.unlootedCount;
        for (auto const& [guid, list] : loot.GetPlayerFFAItems())
            for (QuestItem& entry : *list)
                if (entry.index == index)
                    entry.is_looted = true;
    }

    if (uint32 const share = loot.gold / owners.size())
        for (Player* owner : owners)
        {
            Drop drop;
            drop.kind = DropKind::Gold;
            drop.gold = share;
            Queue(*owner->CustomData.GetDefault<GroundLootState>(StateKey), std::move(drop));
        }
    loot.gold = 0;

    if (loot.isLooted())
    {
        loot.clear();
        corpse->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
    }
}

uint32 BonusGold(Map const* map)
{
    if (uint8 const tier = GetChallengeTierOf(map))
        return DefiBaseGold + DefiTierGold * tier;
    return DungeonBaseGold + DungeonKeyGold * uint32(std::max(map->GetMythicLevel(), 0));
}

// A player left with fewer drops than the boss's minimum: piles of gold make up the rest (see MinDropsDefi), their
// corpse gold share melted into them
void TopUpWithGold(Creature* corpse, Player* player)
{
    GroundLootState* state = player->CustomData.Get<GroundLootState>(StateKey);
    if (!state)
        return;
    uint32 const minimum = GetChallengeBossOf(corpse->GetMap()) ? MinDropsDefi : MinDropsDungeon;
    uint32 gold = BonusGold(corpse->GetMap());
    uint32 others = 0;
    for (auto itr = state->drops.begin(); itr != state->drops.end();)
    {
        if (itr->corpse != corpse->GetGUID() || itr->bag)
            ++itr;
        else if (itr->kind == DropKind::Gold)
        {
            gold += itr->gold;
            itr = state->drops.erase(itr);
        }
        else
        {
            ++others;
            ++itr;
        }
    }
    uint32 const piles = others < minimum ? minimum - others : 1;
    std::vector<float> weights(piles);
    float total = 0.0f;
    for (float& weight : weights)
        total += weight = frand(PileWeightMin, PileWeightMax);
    uint32 given = 0;
    for (uint32 index = 0; index < piles; ++index)
    {
        Drop drop;
        drop.kind = DropKind::Gold;
        drop.gold = index + 1 == piles ? gold - given : uint32(gold * weights[index] / total);
        given += drop.gold;
        if (drop.gold)
            Queue(*state, std::move(drop));
    }
}

class GroundLootUnitScript : public UnitScript
{
public:
    GroundLootUnitScript() : UnitScript("GroundLootUnitScript", true, { UNITHOOK_ON_UNIT_DEATH }) { }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        if (Creature* creature = unit->ToCreature())
            GroundLoot::Open(creature);
    }
};

class GroundLootPlayerScript : public PlayerScript
{
public:
    GroundLootPlayerScript() : PlayerScript("GroundLootPlayerScript", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_LOGOUT
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (IsRealPlayer(player))
            Update(player, diff);
    }

    // Before the character is saved: what is left on the floor goes to the bags with it
    void OnPlayerBeforeLogout(Player* player) override
    {
        if (GroundLootState* state = player->CustomData.Get<GroundLootState>(StateKey))
            GiveAll(player, *state);
    }
};

// A drop clicked (it shows the speech cursor): picked up as walking over it would, from the interaction range
class npc_ground_loot : public CreatureScript
{
public:
    npc_ground_loot() : CreatureScript("npc_ground_loot") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        CloseGossipMenuFor(player);
        GroundLootState* state = player->CustomData.Get<GroundLootState>(StateKey);
        if (!state)
            return true;
        auto const drop = std::ranges::find_if(state->drops, [creature](Drop const& entry)
        {
            return entry.bag == creature->GetGUID() && entry.landed;
        });
        if (drop != state->drops.end() && player->IsAlive() && PickUp(player, *state, *drop))
        {
            Despawn(player, *drop);
            state->drops.erase(drop);
        }
        return true;
    }
};
}

namespace GroundLoot
{
bool Open(Creature* corpse)
{
    if (!corpse || !corpse->IsInWorld() || !Qualifies(corpse))
        return false;
    GroundLootCorpse* mark = corpse->CustomData.GetDefault<GroundLootCorpse>(CorpseKey);
    if (mark->opened)
        return true;
    mark->opened = true;

    Map* map = corpse->GetMap();
    std::vector<Player*> players;
    map->DoForAllPlayers([&players](Player* player)
    {
        if (IsRealPlayer(player))
            players.push_back(player);
    });
    if (players.empty())
        return false;

    uint64 const now = NowMs();
    Position origin = corpse->GetPosition();
    origin.m_positionZ += OriginHeight;
    for (Player* player : players)
    {
        GroundLootState* state = player->CustomData.GetDefault<GroundLootState>(StateKey);
        state->mapId = map->GetId();
        state->instanceId = map->GetInstanceId();
        state->corpse = corpse->GetGUID();
        state->origin = origin;
        state->reach = std::min(corpse->GetCombatReach(), MaxCorpseReach);
        state->openedAt = now;
        state->nextThrowAt = now + BurstDelayMs;
        state->thrown = 0;
    }
    ShareCorpseLoot(corpse, players);
    for (Player* player : players)
        TopUpWithGold(corpse, player);
    LOG_DEBUG("module", "ground loot: {} thrown for {} players", corpse->GetName(), players.size());
    return true;
}

// The player's loot of a kill just made, still taking more (ThrowWindowMs), nullptr when there is none
GroundLootState* OpenBurst(Player* player)
{
    if (!IsRealPlayer(player) || !player->IsInWorld())
        return nullptr;
    GroundLootState* state = player->CustomData.Get<GroundLootState>(StateKey);
    if (!state || !state->openedAt || NowMs() > state->openedAt + ThrowWindowMs ||
        player->GetMapId() != state->mapId || player->GetInstanceId() != state->instanceId)
        return nullptr;
    return state;
}

bool Throw(Player* player, ItemTemplate const* itemTemplate, std::function<void(Item*)> const& touch)
{
    GroundLootState* state = itemTemplate ? OpenBurst(player) : nullptr;
    if (!state)
        return false;

    Drop drop;
    drop.kind = DropKind::Mythic;
    drop.mythic = itemTemplate;
    drop.touch = touch;
    drop.unique = static_cast<bool>(touch);
    drop.quality = itemTemplate->Quality;
    // Its record now, for its tooltip on the floor
    SendMythicItemRecord(player, itemTemplate->ItemId);
    Queue(*state, std::move(drop));
    return true;
}

bool ThrowItem(Player* player, uint32 itemId, uint32 count, uint8 quality)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    GroundLootState* state = itemTemplate ? OpenBurst(player) : nullptr;
    if (!state)
        return false;

    Drop drop;
    drop.kind = DropKind::Item;
    drop.itemId = itemId;
    drop.count = count;
    drop.quality = std::max<uint8>(quality, uint8(itemTemplate->Quality));
    Queue(*state, std::move(drop));
    return true;
}

bool HasPending(ObjectGuid guid)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(guid);
    GroundLootState const* state = player ? player->CustomData.Get<GroundLootState>(StateKey) : nullptr;
    return state && !state->drops.empty();
}
}

namespace
{
using namespace Acore::ChatCommands;

// .groundloot [count]: a boss's burst thrown out of the game master, for testing its looks and sounds. With no count,
// one drop of each kind - a white, a green, a blue, an epic, an epic with a power of its own, a legendary, a Unique,
// two piles of gold - else that many at random. Real items' tooltips, but picking them up gives nothing.
class GroundLootCommandScript : public CommandScript
{
public:
    GroundLootCommandScript() : CommandScript("GroundLootCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "groundloot", HandleGroundLoot, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    // Equipment of each quality, at or above the level-80 dungeons' item level, to show on the floor
    static ItemTemplate const* SampleItem(uint8 quality)
    {
        uint32 const minimumLevel = quality >= ITEM_QUALITY_EPIC ? 200 : quality >= ITEM_QUALITY_UNCOMMON ? 150 : 0;
        std::vector<ItemTemplate const*> found;
        for (auto const& [entry, itemTemplate] : *sObjectMgr->GetItemTemplateStore())
            if (itemTemplate.Quality == quality && itemTemplate.ItemLevel >= minimumLevel && entry < 0x10000 &&
                (itemTemplate.Class == ITEM_CLASS_WEAPON || itemTemplate.Class == ITEM_CLASS_ARMOR))
                found.push_back(&itemTemplate);
        return found.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(found);
    }

    static Drop TestDrop(uint8 kind)
    {
        Drop drop;
        drop.test = true;
        if (kind >= 7)
        {
            drop.kind = DropKind::Gold;
            drop.gold = urand(5 * GOLD, 25 * GOLD);
            return drop;
        }
        uint8 const quality = kind == 0 ? ITEM_QUALITY_NORMAL : kind == 1 ? ITEM_QUALITY_UNCOMMON :
            kind == 2 ? ITEM_QUALITY_RARE : kind <= 4 ? ITEM_QUALITY_EPIC : kind == 5 ? ITEM_QUALITY_LEGENDARY :
            ITEM_QUALITY_ARTIFACT;
        if (ItemTemplate const* itemTemplate = SampleItem(quality))
        {
            drop.itemId = itemTemplate->ItemId;
            drop.quality = quality;
            drop.unique = kind == 4;
        }
        else
        {
            drop.kind = DropKind::Gold;
            drop.gold = urand(5 * GOLD, 25 * GOLD);
        }
        return drop;
    }

    static bool HandleGroundLoot(ChatHandler* handler, Optional<uint32> count)
    {
        Player* player = handler->GetPlayer();
        if (!player || !player->IsInWorld())
            return false;
        Map* map = player->GetMap();
        GroundLootState* state = player->CustomData.GetDefault<GroundLootState>(StateKey);
        uint64 const now = NowMs();
        state->mapId = map->GetId();
        state->instanceId = map->GetInstanceId();
        state->corpse = player->GetGUID();
        state->origin = player->GetPosition();
        state->origin.m_positionZ += OriginHeight;
        // Thrown past where the game master stands, so they do not pick them up as they land
        state->reach = 3.0f;
        state->openedAt = now - BurstDelayMs;
        state->nextThrowAt = now;
        state->thrown = 0;

        std::vector<uint8> kinds;
        if (count)
            for (uint32 index = 0; index < std::min<uint32>(*count, 30); ++index)
                kinds.push_back(uint8(urand(0, 8)));
        else
            kinds = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        for (uint8 kind : kinds)
            Queue(*state, TestDrop(kind));
        handler->PSendSysMessage("Ground loot: {} drops thrown (a test: picking them up gives nothing).",
                                 kinds.size());
        return true;
    }
};
}

void AddGroundLootScripts()
{
    new GroundLootCommandScript();
    new GroundLootUnitScript();
    new GroundLootPlayerScript();
    new npc_ground_loot();
}
