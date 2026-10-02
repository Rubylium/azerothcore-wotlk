#include "EssenceTierSystem.h"
#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "PowerScaling.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Le Front du Nord (the Northrend Frontier): open-world content at level 80. Northrend's zones are ranked in four
// tiers; in a tier zone a level-80 character (bots included) sees the tier's content, in a phase of its own, and the
// higher the tier the better the rewards: the bridge from quest gear to Mythique 0. Every activity is sized for one
// player and grows with the players taking part, so a group meets the same challenge per head.
// Plan: .agents/plans/northrend-frontier/northrend-frontier.PLAN.md
//
// Here: the tiers, the phase, the zone banner, the roaming elites, the rifts and the Colosses (world bosses). Each
// tier is sized on its profile with the power model (.agents/docs/systems/power-scaling.md), one damage dealer a
// participant:
//   Palier I   Borean Tundra, Howling Fjord     187 / 0   drops 200
//   Palier II  Dragonblight, Grizzly Hills      200 / 0   drops 213
//   Palier III Zul'Drak, Sholazar Basin         213 / 0   drops 219
//   Palier IV  Storm Peaks, Icecrown            223 / 0   drops 226 (Mythique 0's profile)
//
// Client: Interface\FrameXML\FrontierUI.lua, on the "Frontier" addon prefix, whispered to the player:
//   ZONE <tier 0-4> <zone id> <loot item level>     entering (or leaving: tier 0) a tier zone
//   PINS <zone id> <kind>:<x>:<y>,...               the zone's content to pin on the maps (E a roaming elite, R a rift)
//   RIFT <stage> <wave> <waves> <alive> <total> <s>  a rift near the player (stage: 1 waiting, 2 waves, 3 guardian,
//                                                   4 closed, 5 collapsed; s: its time left)
//   COLOSSUS <state> <colossus 1-4> <zone id> <x> <y> <s> <loot item level>
//                                                   to every level-80 player: state 1 coming (s: until it
//                                                   comes), 2 here (s: until it leaves), 3 slain, 4 gone unfought

namespace
{
constexpr uint32 MAP_NORTHREND = 571;
// The tier phase: everything the Front du Nord spawns is in it alone. The players see it through SPELL_PHASE.
constexpr uint32 FrontierPhaseMask = 0x4000;
// localTools/frontier/Spells.ps1: the hidden phase aura (phase 1 plus the tier phase) and the elites' blow's name
constexpr uint32 SPELL_PHASE = 97600;
constexpr uint32 SPELL_DEVASTATING_BLOW = 97601;
constexpr uint32 SPELL_DEVASTATING_BLOW_STOCK = 59706;     // until the server's Spell.dbc has 97601
constexpr uint32 ITEM_FROST_SHARD = 37711;
constexpr std::string_view AddonPrefix = "Frontier";

struct Tier
{
    float itemLevel;            // the profile the tier is sized for
    float paragon;
    uint32 lootItemLevel;       // the gear it drops
    uint32 eliteShards;         // Éclats de givre a roaming elite gives...
    uint32 riftShards;          // ... a closed rift
    uint32 colossusShards;      // ... and the tier's Colosse
};

// Tiers I-IV (index 0-3)
constexpr std::array<Tier, 4> Tiers = { {
    { 187.0f, 0.0f, 200, 2, 6, 15 },
    { 200.0f, 0.0f, 213, 3, 8, 20 },
    { 213.0f, 0.0f, 219, 4, 10, 25 },
    { 223.0f, 0.0f, 226, 5, 12, 30 },
} };

struct Zone
{
    uint32 id;
    uint8 tier;                         // 1-4
    float xMin, xMax, yMin, yMax;       // WorldMapArea.dbc: its top/bottom are x, its left/right y
    std::array<uint32, 2> elites;       // its roaming elites' entries (stat_growth_frontier.sql)
};

constexpr std::array<Zone, 8> Zones = { {
    { 3537, 1, 1054.0f, 4898.0f, 2806.0f, 8571.0f, { 940000, 940001 } },     // Borean Tundra
    { 495, 1, -915.0f, 3117.0f, -7444.0f, -1398.0f, { 940002, 940003 } },    // Howling Fjord
    { 65, 2, 1835.0f, 5575.0f, -1981.0f, 3627.0f, { 940004, 940005 } },      // Dragonblight
    { 394, 2, 2017.0f, 5517.0f, -6360.0f, -1110.0f, { 940006, 940007 } },    // Grizzly Hills
    { 66, 3, 4340.0f, 7669.0f, -5594.0f, -600.0f, { 940008, 940009 } },      // Zul'Drak
    { 3711, 3, 4383.0f, 7287.0f, 2573.0f, 6929.0f, { 940010, 940011 } },     // Sholazar Basin
    { 67, 4, 5456.0f, 10198.0f, -5271.0f, 1842.0f, { 940012, 940013 } },     // Storm Peaks
    { 210, 4, 5246.0f, 9427.0f, -827.0f, 5444.0f, { 940014, 940015 } },      // Icecrown
} };

// --- The roaming elites ------------------------------------------------------------------------------------------
constexpr uint32 ElitesPerZone = 3;
constexpr uint32 EliteRespawnMs = 90000;            // a slain elite's place is taken this long after
constexpr uint32 ZoneIdleMs = 5 * 60 * 1000;        // a zone with no tier player this long loses its elites
constexpr uint32 ManagerTickMs = 5000;
constexpr float EliteSpawnClearance = 60.0f;        // never this close to a player when it appears
constexpr uint32 EliteSpawnTries = 12;
// A solo fight of about this long against one damage dealer of the tier's profile
constexpr float EliteSeconds = 25.0f;
// Its melee: a swing at this share of a player's health, before armour (ArmourMargin makes up for it)
constexpr float EliteSwingPct = 3.0f;
constexpr float ArmourMargin = 1.3f;
// Coup dévastateur: a circle under a fighter, then a hit at this share of a player's health
constexpr float BlowPct = 35.0f;
constexpr float BlowRadius = 6.0f;
constexpr uint32 BlowWarningMs = 2500;
constexpr Milliseconds BlowFirst = 6s;
constexpr Milliseconds BlowEveryMin = 10s;
constexpr Milliseconds BlowEveryMax = 13s;
// Who took part: the engaging player's group within this reach (a tank counts as a third of a damage dealer)
constexpr float ParticipantReach = 50.0f;
constexpr float TankShare = 1.0f / 3.0f;
// Rewards, to every player who fought it (bots take nothing)
constexpr float EliteGearChance = 12.0f;
constexpr float EliteEssenceChance = 5.0f;
constexpr float RewardReach = 80.0f;
constexpr Seconds CorpseDespawn = 60s;

// --- The rifts (Failles) -----------------------------------------------------------------------------------------
// One a zone while a tier player is in it: a portal at a random spot, opened by whoever comes near; three waves come
// out of it, then its guardian. Sized on the participants when it opens (a tank a third of a damage dealer).
constexpr uint32 NPC_RIFT = 940020;
constexpr std::array<uint32, 2> RiftCreatures = { 940021, 940022 };
constexpr uint32 NPC_RIFT_GUARDIAN = 940023;
constexpr uint32 RiftFirstMs = 20000;               // a zone just entered gets its rift this soon
constexpr uint32 RiftCooldownMs = 2 * 60 * 1000;    // the next one, this long after the last closed
constexpr uint32 RiftIdleMs = 10 * 60 * 1000;       // a rift nobody opens closes after this long
constexpr uint32 RiftLimitMs = 4 * 60 * 1000;       // an opened rift collapses unless closed within this
constexpr float RiftSpawnClearance = 80.0f;
constexpr float RiftStartReach = 15.0f;             // a tier player this close opens it
constexpr float RiftReach = 45.0f;                  // its participants, and who it hits
constexpr float RiftLeaveReach = 80.0f;             // nobody alive this close: it collapses
constexpr uint32 RiftAbandonMs = 20000;
constexpr uint32 RiftWaves = 3;
constexpr uint32 RiftWaveGapMs = 4000;
constexpr float RiftWaveSeconds = 15.0f;            // a wave, against the participants' area damage
constexpr float RiftGuardianSeconds = 35.0f;        // the guardian, against their single-target damage
constexpr float RiftCreatureSwingPct = 2.0f;
constexpr float RiftGuardianSwingPct = 4.0f;
// The guardian's: Éclat de cristal (a circle under a fighter) and Nova arcanique (a circle around itself: get out)
constexpr float CrystalPct = 35.0f;
constexpr float CrystalRadius = 5.0f;
constexpr uint32 CrystalWarningMs = 2500;
constexpr float NovaPct = 45.0f;
constexpr float NovaRadius = 10.0f;
constexpr uint32 NovaWarningMs = 3000;
constexpr float RiftGearChance = 50.0f;
constexpr float RiftEssenceChance = 30.0f;
constexpr uint32 SPELL_CRYSTAL_SHARD = 97602;
constexpr uint32 SPELL_ARCANE_NOVA = 97603;
constexpr uint32 SPELL_ARCANE_STOCK = 59706;

// --- The Colosses (world bosses) ---------------------------------------------------------------------------------
// One at a time, the four in turn (tier I's, then II's, III's and IV's), at a spot of one of their tier's two zones.
// Every level-80 player hears of one ColossusLeadMs before it comes; it stays ColossusStayMs unless fought. Sized for
// whoever fights it (one damage dealer at least, more as players join), its abilities all on the ground indicators,
// its rewards to every player who fought it: many shards and the tier's gear for certain.
constexpr std::array<uint32, 4> NPC_COLOSSI = { 940030, 940031, 940032, 940033 };
constexpr uint32 ColossusFirstMs = 5 * 60 * 1000;    // the first one comes this long after the server starts
constexpr uint32 ColossusEveryMs = 15 * 60 * 1000;   // then one this often: each tier's once an hour
constexpr uint32 ColossusLeadMs = 10 * 60 * 1000;
constexpr uint32 ColossusStayMs = 12 * 60 * 1000;
constexpr float ColossusSeconds = 60.0f;             // a fight of about this long, against its participants
constexpr float ColossusSwingPct = 2.5f;             // its melee is light: what kills is standing in its abilities
constexpr float ColossusReach = 60.0f;               // its participants, and who it hits
constexpr float ColossusRewardReach = 100.0f;
constexpr float ColossusEssenceChance = 50.0f;
constexpr float TownClearance = 60.0f;               // never this close to a friendly creature's spawn
// Its abilities: a cone toward its target, a circle under every fighter (spread out), a circle around itself (get
// out), and at two thirds and a third of its health everything but the ground at its feet (come in)
constexpr float ConePct = 50.0f;
constexpr float ConeRadius = 22.0f;
constexpr float ConeArc = 90.0f;
constexpr uint32 ConeWarningMs = 3000;
constexpr float SpreadPct = 35.0f;
constexpr float SpreadRadius = 5.0f;
constexpr uint32 SpreadWarningMs = 3000;
constexpr float StompPct = 50.0f;
constexpr float StompRadius = 12.0f;
constexpr uint32 StompWarningMs = 3500;
constexpr float RingPct = 60.0f;
constexpr float RingOuter = 45.0f;
constexpr float RingInner = 9.0f;
constexpr uint32 RingWarningMs = 5000;
// localTools/frontier/Spells.ps1: each Colosse's four (cone, spread, stomp, ring), from SPELL_COLOSSUS_FIRST
constexpr uint32 SPELL_COLOSSUS_FIRST = 97604;

struct ColossusKind
{
    GroundIndicators::Theme cone, spread, stomp, ring;
};

constexpr std::array<ColossusKind, 4> ColossusKinds = { {
    // Gorroth Grandes-Défenses: his tusks, falling ice, his stomp, an avalanche
    { GroundIndicators::Theme::None, GroundIndicators::Theme::Frost, GroundIndicators::Theme::None,
      GroundIndicators::Theme::Frost },
    // Vyskarn: frost breath, freezing rain, his wings, a blizzard
    { GroundIndicators::Theme::Frost, GroundIndicators::Theme::Frost, GroundIndicators::Theme::None,
      GroundIndicators::Theme::Frost },
    // Zul'Gath: his ritual axe, a voodoo curse, the loa's fury, its serpent spirits
    { GroundIndicators::Theme::None, GroundIndicators::Theme::Shadow, GroundIndicators::Theme::Nature,
      GroundIndicators::Theme::Nature },
    // The Saronite Juggernaut: its fists, plague, a saronite wave, its cauldron's mist
    { GroundIndicators::Theme::None, GroundIndicators::Theme::Nature, GroundIndicators::Theme::Shadow,
      GroundIndicators::Theme::Shadow },
} };

// Who the reference model counts below Mythique 0: its measured curve starts at 223, and its straight line below
// that drops a fresh 80 to a third of what it deals. Item level 223's damage, scaled by the square of the item level
// ratio (gear's stats grow about so) - to re-measure on the combat bench.
constexpr float ModelFloorItemLevel = 223.0f;

float TierDps(Tier const& tier, bool pack = false)
{
    float const ratio = std::min(tier.itemLevel / ModelFloorItemLevel, 1.0f);
    return Power::ExpectedDps(std::max(tier.itemLevel, ModelFloorItemLevel), tier.paragon, pack) * ratio * ratio;
}

float TierPlayerHealth(Tier const& tier)
{
    return Power::ExpectedPlayerHealth(tier.itemLevel, tier.paragon);
}

Zone const* FindZone(uint32 zoneId)
{
    for (Zone const& zone : Zones)
        if (zone.id == zoneId)
            return &zone;
    return nullptr;
}

Tier const& TierOf(Zone const& zone)
{
    return Tiers[zone.tier - 1];
}

uint32 SpellOrStock(uint32 spellId, uint32 stock)
{
    return sSpellMgr->GetSpellInfo(spellId) ? spellId : stock;
}

bool IsBot(Player const* player)
{
    return player->GetSession() && player->GetSession()->IsBot();
}

void SendAddon(Player* player, std::string const& body)
{
    if (!player || !player->GetSession())
        return;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(AddonPrefix) + "\t" + body);
    player->GetSession()->SendPacket(&packet);
}

// --- The phase ---------------------------------------------------------------------------------------------------
// The zone a character's tier phase is for, or none: level 80, in a tier zone of Northrend, in no quest phase of its
// own (phases add up: phase 1 would show through the quest's phasing)
Zone const* TierZoneOf(Player* player)
{
    if (!player->IsInWorld() || player->GetMapId() != MAP_NORTHREND || player->GetLevel() < DEFAULT_MAX_LEVEL)
        return nullptr;
    Zone const* zone = FindZone(player->GetZoneId());
    if (!zone)
        return nullptr;
    for (AuraEffect const* effect : player->GetAuraEffectsByType(SPELL_AURA_PHASE))
        if (effect->GetId() != SPELL_PHASE)
            return nullptr;
    return zone;
}

void RefreshPhase(Player* player)
{
    Zone const* zone = TierZoneOf(player);
    bool const has = player->HasAura(SPELL_PHASE);
    if (zone && !has && sSpellMgr->GetSpellInfo(SPELL_PHASE))
        player->AddAura(SPELL_PHASE, player);
    else if (!zone && has)
        player->RemoveAurasDueToSpell(SPELL_PHASE);

    if (IsBot(player))
        return;
    if (zone)
        SendAddon(player, Acore::StringFormat("ZONE\t{}\t{}\t{}", zone->tier, zone->id, TierOf(*zone).lootItemLevel));
    else
        SendAddon(player, "ZONE\t0\t0\t0");
}

bool SeesTier(Player* player)
{
    return (player->GetPhaseMask() & FrontierPhaseMask) != 0;
}

// --- Scaling -----------------------------------------------------------------------------------------------------
// The damage dealers fighting it: the engaging player and their group within reach, players and bots
float Participants(Creature* elite, Unit* engager)
{
    Player* player = engager ? engager->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (!player)
        return 1.0f;
    auto const share = [](Player* member) { return IsGroupTank(member) ? TankShare : 1.0f; };
    Group* group = player->GetGroup();
    if (!group)
        return share(player);
    float dealers = 0.0f;
    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        if (Player* member = itr->GetSource())
            if (member->IsAlive() && member->IsInMap(elite) && member->GetDistance(elite) <= ParticipantReach)
                dealers += share(member);
    return std::max(dealers, share(player));
}

// A creature's health and melee, its health kept at the same share
void ScaleUnit(Creature* creature, float health, float swing)
{
    uint32 const maxHealth = uint32(std::max(1.0f, health));
    float const pct = creature->GetHealthPct();
    creature->SetCreateHealth(maxHealth);
    creature->SetMaxHealth(maxHealth);
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(maxHealth));
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
    {
        creature->SetBaseWeaponDamage(attackType, MINDAMAGE, swing * ArmourMargin * 0.85f);
        creature->SetBaseWeaponDamage(attackType, MAXDAMAGE, swing * ArmourMargin * 1.15f);
    }
    creature->UpdateAllStats();
    creature->SetHealth(uint32(maxHealth * pct / 100.0f));
    creature->ResetPlayerDamageReq();
}

void ScaleElite(Creature* elite, Tier const& tier, float dealers)
{
    ScaleUnit(elite, TierDps(tier) * dealers * EliteSeconds, TierPlayerHealth(tier) * EliteSwingPct / 100.0f);
}

// A player's or a bot's share of a group's damage
float DealerShare(Player* player)
{
    return IsGroupTank(player) ? TankShare : 1.0f;
}

// The tier players and bots alive within reach of a spot
std::vector<Player*> TierPlayersNear(WorldObject* center, float reach)
{
    std::vector<Player*> players;
    for (auto const& ref : center->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource(); player && player->IsAlive() && player->IsInWorld() &&
            (player->GetPhaseMask() & FrontierPhaseMask) && player->GetDistance(center) <= reach)
            players.push_back(player);
    return players;
}

// Rewards to these players (bots take nothing): shards, and chances of the tier's gear and an essence
void RewardPlayers(std::set<Player*> const& players, Tier const& tier, uint32 shards, float gearChance,
    float essenceChance)
{
    for (Player* player : players)
    {
        if (IsBot(player))
            continue;
        player->AddItem(ITEM_FROST_SHARD, shards);
        if (roll_chance_f(gearChance))
            GiveMythicLootItem(player, tier.lootItemLevel);
        if (roll_chance_f(essenceChance))
            GrantEssenceRewards(player, 1, 0);
    }
}

// --- The zones' state ----------------------------------------------------------------------------------------------
struct ZoneState
{
    std::vector<Position> spots;        // its hostile and wild creatures' spawn points: valid ground, built once
    std::vector<Position> towns;        // its friendly creatures' (quest givers, guards, vendors): kept away from
    bool spotsBuilt = false;
    std::vector<ObjectGuid> elites;
    std::vector<uint32> respawns;       // a slain elite's place, the time left before it is taken again
    uint32 idleMs = 0;
    uint8 nextEntry = 0;
    ObjectGuid rift;
    uint32 riftInMs = RiftFirstMs;
};

std::array<ZoneState, Zones.size()> States;

void BuildSpots(Zone const& zone, ZoneState& state)
{
    state.spotsBuilt = true;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
    {
        if (data.mapid != MAP_NORTHREND || !(data.phaseMask & PHASEMASK_NORMAL) || data.posX < zone.xMin ||
            data.posX > zone.xMax || data.posY < zone.yMin || data.posY > zone.yMax)
            continue;
        CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(data.id);
        FactionTemplateEntry const* faction = info ? sFactionTemplateStore.LookupEntry(info->faction) : nullptr;
        bool const wild = info && !info->npcflag && faction &&
            (faction->IsHostileToPlayers() || faction->IsNeutralToAll());
        (wild ? state.spots : state.towns).emplace_back(data.posX, data.posY, data.posZ,
            frand(0.0f, 2.0f * float(M_PI)));
    }
    LOG_INFO("module.frontier", "Front du Nord: zone {} has {} spawn spots, {} friendly", zone.id, state.spots.size(),
        state.towns.size());
}

bool NearTown(ZoneState const& state, Position const& spot)
{
    return std::ranges::any_of(state.towns, [&spot](Position const& town)
        { return town.GetExactDist2d(&spot) < TownClearance; });
}

// A creature of the tier phase at one of the zone's spawn spots, away from every player
Creature* SummonAtSpot(Map* map, Zone const& zone, ZoneState& state, std::vector<Player*> const& players,
    uint32 entry, float clearance)
{
    if (!state.spotsBuilt)
        BuildSpots(zone, state);
    if (state.spots.empty())
        return nullptr;

    for (uint32 attempt = 0; attempt < EliteSpawnTries; ++attempt)
    {
        Position const& spot = Acore::Containers::SelectRandomContainerElement(state.spots);
        if (NearTown(state, spot) || std::ranges::any_of(players, [&spot, clearance](Player* player)
            { return player->GetExactDist2d(&spot) < clearance; }))
            continue;

        TempSummon* creature = map->SummonCreature(entry, spot);
        if (!creature)
            continue;
        // The spawn rectangles overlap their neighbours: the zone is checked where the creature stands
        if (creature->GetZoneId() != zone.id || creature->IsInWater())
        {
            creature->DespawnOrUnsummon();
            continue;
        }
        creature->SetPhaseMask(FrontierPhaseMask, true);
        creature->SetHomePosition(spot);
        return creature;
    }
    return nullptr;
}

Creature* SpawnElite(Map* map, Zone const& zone, ZoneState& state, std::vector<Player*> const& players)
{
    Creature* elite = SummonAtSpot(map, zone, state, players, zone.elites[state.nextEntry % zone.elites.size()],
        EliteSpawnClearance);
    if (!elite)
        return nullptr;
    ScaleElite(elite, TierOf(zone), 1.0f);
    ++state.nextEntry;
    return elite;
}

void DespawnElites(Map* map, ZoneState& state)
{
    for (ObjectGuid const& guid : state.elites)
        if (Creature* elite = map->GetCreature(guid))
            if (!elite->IsInCombat())
                elite->DespawnOrUnsummon();
    state.elites.clear();
}

std::string Pins(Map* map, ZoneState const& state)
{
    std::string pins;
    auto const add = [&pins](char kind, Creature const* creature)
    {
        pins += Acore::StringFormat("{}{}:{:.0f}:{:.0f}", pins.empty() ? "" : ",", kind, creature->GetPositionX(),
            creature->GetPositionY());
    };
    for (ObjectGuid const& guid : state.elites)
        if (Creature* elite = map->GetCreature(guid); elite && elite->IsAlive())
            add('E', elite);
    if (Creature* rift = map->GetCreature(state.rift))
        add('R', rift);
    return pins;
}

// --- The Colosses' schedule ----------------------------------------------------------------------------------------
enum class ColossusNews : uint8
{
    Coming = 1,
    Here = 2,
    Slain = 3,
    Gone = 4
};

// A Colosse and its spot, in one of its tier's zones
struct ColossusSpot
{
    uint8 index = 0;            // 0-3: its tier's, less one
    std::size_t zone = 0;       // in Zones
    Position spot;
};

struct ColossusSchedule
{
    uint8 next = 0;
    uint32 inMs = ColossusFirstMs;          // until the next one comes
    std::optional<ColossusSpot> coming;     // heard of, not come yet
    std::optional<ColossusSpot> here;       // come: on its spot, summoned whenever a player is in its zone
    uint32 stayMs = 0;
    ObjectGuid guid;
    bool slain = false;
};

ColossusSchedule Colossi;

std::string ColossusMessage(ColossusNews news, ColossusSpot const& colossus, uint32 seconds)
{
    return Acore::StringFormat("COLOSSUS\t{}\t{}\t{}\t{:.0f}\t{:.0f}\t{}\t{}", uint32(news), colossus.index + 1,
        Zones[colossus.zone].id, colossus.spot.GetPositionX(), colossus.spot.GetPositionY(), seconds,
        Tiers[colossus.index].lootItemLevel);
}

// Every level-80 player in the world (bots aside) hears of the Colosses
void TellEveryone(std::string const& body)
{
    for (auto const& [accountId, session] : sWorldSessionMgr->GetAllSessions())
        if (Player* player = session ? session->GetPlayer() : nullptr; player && player->IsInWorld() &&
            !IsBot(player) && player->GetLevel() >= DEFAULT_MAX_LEVEL)
            SendAddon(player, body);
}

// What a player coming into the world should know of
void TellColossi(Player* player)
{
    if (IsBot(player) || player->GetLevel() < DEFAULT_MAX_LEVEL)
        return;
    if (Colossi.coming)
        SendAddon(player, ColossusMessage(ColossusNews::Coming, *Colossi.coming, Colossi.inMs / 1000));
    if (Colossi.here && !Colossi.slain)
        SendAddon(player, ColossusMessage(ColossusNews::Here, *Colossi.here, Colossi.stayMs / 1000));
}

// A spot for a Colosse in one of its tier's zones: a hostile or wild creature's spawn point away from the towns,
// outdoors and on dry ground (an invisible trigger stands there a moment to tell)
std::optional<ColossusSpot> FindColossusSpot(Map* map, uint8 index)
{
    std::vector<std::size_t> zones;
    for (std::size_t zoneIndex = 0; zoneIndex < Zones.size(); ++zoneIndex)
        if (Zones[zoneIndex].tier == index + 1)
            zones.push_back(zoneIndex);
    std::size_t const zoneIndex = Acore::Containers::SelectRandomContainerElement(zones);
    Zone const& zone = Zones[zoneIndex];
    ZoneState& state = States[zoneIndex];
    if (!state.spotsBuilt)
        BuildSpots(zone, state);
    if (state.spots.empty())
        return std::nullopt;

    for (uint32 attempt = 0; attempt < EliteSpawnTries * 2; ++attempt)
    {
        Position const& spot = Acore::Containers::SelectRandomContainerElement(state.spots);
        if (NearTown(state, spot))
            continue;
        TempSummon* probe = map->SummonCreature(WORLD_TRIGGER, spot);
        if (!probe)
            continue;
        bool const fits = probe->GetZoneId() == zone.id && !probe->IsInWater() && probe->IsOutdoors();
        probe->DespawnOrUnsummon();
        if (fits)
            return ColossusSpot{ index, zoneIndex, spot };
    }
    return std::nullopt;
}

Creature* SummonColossus(Map* map, ColossusSpot const& colossus)
{
    TempSummon* creature = map->SummonCreature(NPC_COLOSSI[colossus.index], colossus.spot);
    if (!creature)
        return nullptr;
    creature->SetPhaseMask(FrontierPhaseMask, true);
    creature->SetHomePosition(colossus.spot);
    return creature;
}

// Every manager tick: the Colosse on its spot stays its time (longer while fought), the next one is heard of, then
// comes once the one before has left
void UpdateColossi(Map* map, std::array<std::vector<Player*>, Zones.size()> const& present, uint32 elapsed)
{
    ColossusSchedule& schedule = Colossi;
    Creature* creature = schedule.guid.IsEmpty() ? nullptr : map->GetCreature(schedule.guid);
    bool const fighting = creature && creature->IsAlive() && creature->IsInCombat();

    if (schedule.here)
    {
        schedule.stayMs = schedule.stayMs > elapsed ? schedule.stayMs - elapsed : 0;
        if (schedule.slain || (!schedule.stayMs && !fighting))
        {
            if (!schedule.slain)
            {
                if (creature)
                    creature->DespawnOrUnsummon();
                TellEveryone(ColossusMessage(ColossusNews::Gone, *schedule.here, 0));
                LOG_INFO("module.frontier", "Front du Nord: Colosse {} left zone {} unfought", schedule.here->index + 1,
                    Zones[schedule.here->zone].id);
            }
            schedule.here.reset();
            schedule.guid.Clear();
        }
        // Summoned when a player is in its zone, and again if its grid was unloaded with it
        else if (!creature && std::ranges::any_of(present[schedule.here->zone], [](Player* player)
            { return !IsBot(player); }))
        {
            if (Creature* summoned = SummonColossus(map, *schedule.here))
                schedule.guid = summoned->GetGUID();
        }
    }

    schedule.inMs = schedule.inMs > elapsed ? schedule.inMs - elapsed : 0;
    if (!schedule.coming && schedule.inMs <= ColossusLeadMs)
    {
        schedule.coming = FindColossusSpot(map, schedule.next);
        if (schedule.coming)
        {
            TellEveryone(ColossusMessage(ColossusNews::Coming, *schedule.coming, schedule.inMs / 1000));
            LOG_INFO("module.frontier", "Front du Nord: Colosse {} coming to zone {} ({:.0f}, {:.0f}) in {} s",
                schedule.coming->index + 1, Zones[schedule.coming->zone].id, schedule.coming->spot.GetPositionX(),
                schedule.coming->spot.GetPositionY(), schedule.inMs / 1000);
        }
    }
    if (schedule.coming && !schedule.inMs && !schedule.here)
    {
        schedule.here = schedule.coming;
        schedule.coming.reset();
        schedule.stayMs = ColossusStayMs;
        schedule.slain = false;
        schedule.guid.Clear();
        schedule.next = uint8((schedule.next + 1) % NPC_COLOSSI.size());
        schedule.inMs = ColossusEveryMs;
        TellEveryone(ColossusMessage(ColossusNews::Here, *schedule.here, schedule.stayMs / 1000));
    }
}

// Every few seconds: each tier zone with a tier player in it keeps its roaming elites and its rift; an empty one loses
// its elites. The Colosses keep their schedule.
class FrontierWorldScript : public WorldScript
{
public:
    FrontierWorldScript() : WorldScript("FrontierWorldScript", { WORLDHOOK_ON_UPDATE }) { }

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;
        if (_timer < ManagerTickMs)
            return;
        uint32 const elapsed = _timer;
        _timer = 0;

        Map* map = sMapMgr->FindMap(MAP_NORTHREND, 0);
        if (!map)
            return;

        std::array<std::vector<Player*>, Zones.size()> present;
        for (auto const& ref : map->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->IsInWorld() && SeesTier(player))
                for (std::size_t index = 0; index < Zones.size(); ++index)
                    if (Zones[index].id == player->GetZoneId())
                        present[index].push_back(player);

        for (std::size_t index = 0; index < Zones.size(); ++index)
        {
            Zone const& zone = Zones[index];
            ZoneState& state = States[index];
            std::size_t const before = state.elites.size();
            std::erase_if(state.elites, [map](ObjectGuid const& guid)
            {
                Creature* elite = map->GetCreature(guid);
                return !elite || !elite->IsAlive();
            });
            // Each slain (or unloaded) elite's place is taken EliteRespawnMs later
            state.respawns.insert(state.respawns.end(), before - state.elites.size(), EliteRespawnMs);

            if (present[index].empty())
            {
                state.idleMs += elapsed;
                if (state.idleMs >= ZoneIdleMs && !state.elites.empty())
                {
                    DespawnElites(map, state);
                    state.respawns.clear();
                }
                continue;
            }
            state.idleMs = 0;

            for (uint32& left : state.respawns)
                left = left > elapsed ? left - elapsed : 0;
            // One a tick: a zone just entered fills quickly; a slain elite's place waits for its respawn
            auto const due = std::ranges::find(state.respawns, 0u);
            bool const free = state.elites.size() + state.respawns.size() < ElitesPerZone;
            if (state.elites.size() < ElitesPerZone && (free || due != state.respawns.end()))
                if (Creature* elite = SpawnElite(map, zone, state, present[index]))
                {
                    state.elites.push_back(elite->GetGUID());
                    if (!free)
                        state.respawns.erase(due);
                }

            // Its rift: a new one RiftCooldownMs after the last one closed
            if (!state.rift.IsEmpty() && !map->GetCreature(state.rift))
            {
                state.rift.Clear();
                state.riftInMs = RiftCooldownMs;
            }
            if (state.rift.IsEmpty())
            {
                state.riftInMs = state.riftInMs > elapsed ? state.riftInMs - elapsed : 0;
                if (!state.riftInMs)
                    if (Creature* rift = SummonAtSpot(map, zone, state, present[index], NPC_RIFT, RiftSpawnClearance))
                        state.rift = rift->GetGUID();
            }

            std::string const pins = Pins(map, state);
            for (Player* player : present[index])
                if (!IsBot(player))
                    SendAddon(player, Acore::StringFormat("PINS\t{}\t{}", zone.id, pins));
        }

        UpdateColossi(map, present, elapsed);
    }

private:
    uint32 _timer = 0;
};

// A roaming elite: its melee and a telegraphed blow, sized for whoever fights it; its rewards to every player who did
struct npc_frontier_elite : public ScriptedAI
{
    explicit npc_frontier_elite(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        scheduler.CancelAll();
    }

    void JustEngagedWith(Unit* who) override
    {
        if (Tier const* tier = MyTier())
            ScaleElite(me, *tier, Participants(me, who));
        scheduler.Schedule(BlowFirst, [this](TaskContext context)
        {
            DevastatingBlow();
            context.Repeat(BlowEveryMin, BlowEveryMax);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        scheduler.Update(diff, [this] { DoMeleeAttackIfReady(); });
    }

    void JustDied(Unit* killer) override
    {
        scheduler.CancelAll();
        if (Tier const* tier = MyTier())
            Reward(*tier, killer);
        me->DespawnOrUnsummon(CorpseDespawn);
    }

private:
    Tier const* MyTier() const
    {
        Zone const* zone = FindZone(me->GetZoneId());
        return zone ? &TierOf(*zone) : nullptr;
    }

    // A circle under one of its fighters; whoever stands in it when it lands is hit hard
    void DevastatingBlow()
    {
        Tier const* tier = MyTier();
        std::vector<Unit*> targets;
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
            if (Unit* victim = ref->GetVictim(); victim && victim->IsPlayer() && me->GetDistance(victim) <= 30.0f)
                targets.push_back(victim);
        if (!tier || targets.empty())
            return;
        Unit* target = Acore::Containers::SelectRandomContainerElement(targets);
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK2HTIGHT);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, target->GetPosition(), BlowRadius,
            BlowWarningMs);
        uint32 const amount = uint32(TierPlayerHealth(*tier) * BlowPct / 100.0f);
        scheduler.Schedule(Milliseconds(BlowWarningMs), [this, area, amount](TaskContext)
        {
            uint32 const spellId = SpellOrStock(SPELL_DEVASTATING_BLOW, SPELL_DEVASTATING_BLOW_STOCK);
            for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
                if (Unit* victim = ref->GetVictim(); victim && victim->IsPlayer() && victim->IsAlive() &&
                    area.Contains(victim->GetPosition()))
                {
                    MythicTuning::DealAbilityDamage(me, victim, spellId, amount);
                    MythicTuning::ApplyImprudence(victim);
                }
        });
    }

    // Every player who fought it, and the killer's group around it: shards, a chance of the tier's gear and of an
    // essence. Bots take nothing.
    void Reward(Tier const& tier, Unit* killer)
    {
        std::set<Player*> players;
        for (ThreatReference const* ref : me->GetThreatMgr().GetUnsortedThreatList())
            if (Unit* victim = ref->GetVictim())
                if (Player* player = victim->GetCharmerOrOwnerPlayerOrPlayerItself())
                    players.insert(player);
        if (Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
        {
            players.insert(player);
            if (Group* group = player->GetGroup())
                for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
                    if (Player* member = itr->GetSource())
                        players.insert(member);
        }

        for (Player* player : players)
        {
            if (IsBot(player) || !player->IsInMap(me) || player->GetDistance(me) > RewardReach)
                continue;
            player->AddItem(ITEM_FROST_SHARD, tier.eliteShards);
            if (roll_chance_f(EliteGearChance))
                GiveMythicLootItem(player, tier.lootItemLevel);
            if (roll_chance_f(EliteEssenceChance))
                GrantEssenceRewards(player, 1, 0);
        }
    }
};

// --- The rifts ---------------------------------------------------------------------------------------------------
enum class RiftStage : uint8
{
    Waiting = 1,        // open, nobody near
    Waves = 2,
    Guardian = 3,
    Closed = 4,         // the guardian slain: rewards given
    Collapsed = 5       // its time ran out, or everyone left or died
};

Tier const* TierAt(WorldObject const* object)
{
    Zone const* zone = FindZone(object->GetZoneId());
    return zone ? &TierOf(*zone) : nullptr;
}

// The nearest tier player or bot to a spot, within reach
Unit* NearestFighter(WorldObject* center, float reach)
{
    Player* nearest = nullptr;
    for (Player* player : TierPlayersNear(center, reach))
        if (!nearest || center->GetDistance(player) < center->GetDistance(nearest))
            nearest = player;
    return nearest;
}

// The rift: opened by the first tier player near it, it sends three waves then its guardian, sized on whoever was
// there when it opened, and rewards everyone near when the guardian falls. The client's tracker follows it (RIFT).
struct npc_frontier_rift : public ScriptedAI
{
    explicit npc_frontier_rift(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void InitializeAI() override
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void JustSummoned(Creature* summon) override { _summons.Summon(summon); }
    void SummonedCreatureDespawn(Creature* summon) override { _summons.Despawn(summon); }

    void UpdateAI(uint32 diff) override
    {
        _tickMs += diff;
        if (_tickMs < 1000)
            return;
        uint32 const elapsed = _tickMs;
        _tickMs = 0;
        _ageMs += elapsed;

        Tier const* tier = TierAt(me);
        if (!tier)
        {
            me->DespawnOrUnsummon();
            return;
        }

        switch (_stage)
        {
            case RiftStage::Waiting:
                if (_ageMs >= RiftIdleMs)
                {
                    me->DespawnOrUnsummon();
                    return;
                }
                if (!TierPlayersNear(me, RiftStartReach).empty())
                    Open(*tier);
                break;
            case RiftStage::Waves:
            case RiftStage::Guardian:
                if (_ageMs - _openedMs >= RiftLimitMs)
                {
                    Collapse();
                    break;
                }
                _abandonMs = TierPlayersNear(me, RiftLeaveReach).empty() ? _abandonMs + elapsed : 0;
                if (_abandonMs >= RiftAbandonMs)
                {
                    Collapse();
                    break;
                }
                if (Alive() == 0)
                {
                    if (_stage == RiftStage::Guardian)
                        Close(*tier);
                    else if ((_gapMs += elapsed) >= RiftWaveGapMs)
                    {
                        _gapMs = 0;
                        NextWave(*tier);
                    }
                }
                break;
            case RiftStage::Closed:
            case RiftStage::Collapsed:
                if ((_endMs += elapsed) >= 8000)
                {
                    me->DespawnOrUnsummon();
                    return;
                }
                break;
        }
        SendTracker();
    }

private:
    uint32 Alive() const
    {
        uint32 alive = 0;
        for (ObjectGuid const& guid : _current)
            if (Creature* creature = ObjectAccessor::GetCreature(*me, guid); creature && creature->IsAlive())
                ++alive;
        return alive;
    }

    void Open(Tier const& tier)
    {
        float dealers = 0.0f;
        for (Player* player : TierPlayersNear(me, RiftReach))
            dealers += DealerShare(player);
        _dealers = std::max(dealers, 1.0f);
        _openedMs = _ageMs;
        _stage = RiftStage::Waves;
        LOG_INFO("module.frontier", "Front du Nord: rift opened in zone {} for {:.2f} damage dealers", me->GetZoneId(),
            _dealers);
        NextWave(tier);
    }

    TempSummon* SummonFighter(uint32 entry, float distance, float health, float swing)
    {
        float const angle = frand(0.0f, 2.0f * float(M_PI));
        float x = me->GetPositionX() + std::cos(angle) * distance;
        float y = me->GetPositionY() + std::sin(angle) * distance;
        float z = me->GetPositionZ();
        me->UpdateGroundPositionZ(x, y, z);
        TempSummon* creature = me->SummonCreature(entry, Position(x, y, z, angle + float(M_PI)),
            TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
        if (!creature)
            return nullptr;
        creature->SetPhaseMask(FrontierPhaseMask, true);
        ScaleUnit(creature, health, swing);
        if (Unit* target = NearestFighter(me, RiftReach))
            creature->AI()->AttackStart(target);
        _current.push_back(creature->GetGUID());
        return creature;
    }

    // A wave: as many creatures as two plus the damage dealers (3-8), sharing the wave's health; after the last one,
    // the guardian
    void NextWave(Tier const& tier)
    {
        _current.clear();
        if (++_wave > RiftWaves)
        {
            _stage = RiftStage::Guardian;
            SummonFighter(NPC_RIFT_GUARDIAN, 3.0f, TierDps(tier) * _dealers * RiftGuardianSeconds,
                TierPlayerHealth(tier) * RiftGuardianSwingPct / 100.0f);
            _total = 1;
            return;
        }
        uint32 const count = std::clamp<uint32>(uint32(std::lround(2.0f + _dealers)), 3, 8);
        float const health = TierDps(tier, true) * _dealers * RiftWaveSeconds / float(count);
        float const swing = TierPlayerHealth(tier) * RiftCreatureSwingPct / 100.0f;
        for (uint32 index = 0; index < count; ++index)
            SummonFighter(RiftCreatures[index % RiftCreatures.size()], frand(5.0f, 8.0f), health, swing);
        _total = count;
    }

    void Close(Tier const& tier)
    {
        _stage = RiftStage::Closed;
        std::set<Player*> players;
        for (Player* player : TierPlayersNear(me, RiftLeaveReach))
            players.insert(player);
        RewardPlayers(players, tier, tier.riftShards, RiftGearChance, RiftEssenceChance);
        LOG_INFO("module.frontier", "Front du Nord: rift closed in zone {}, {} player(s) near", me->GetZoneId(),
            players.size());
    }

    void Collapse()
    {
        _stage = RiftStage::Collapsed;
        _summons.DespawnAll();
        _current.clear();
    }

    // RIFT <stage> <wave> <waves> <alive> <total> <seconds left>, to the players near it; the client hides the
    // tracker when it stops hearing of it
    void SendTracker()
    {
        bool const running = _stage == RiftStage::Waves || _stage == RiftStage::Guardian;
        uint32 const left = running ? (RiftLimitMs - std::min(RiftLimitMs, _ageMs - _openedMs)) / 1000 : 0;
        std::string const body = Acore::StringFormat("RIFT\t{}\t{}\t{}\t{}\t{}\t{}", uint32(_stage),
            std::min(_wave, RiftWaves), RiftWaves, Alive(), _total, left);
        float const reach = _stage == RiftStage::Waiting ? 40.0f : RiftLeaveReach;
        for (Player* player : TierPlayersNear(me, reach))
            if (!IsBot(player))
                SendAddon(player, body);
    }

    SummonList _summons;
    std::vector<ObjectGuid> _current;
    RiftStage _stage = RiftStage::Waiting;
    uint32 _tickMs = 0;
    uint32 _ageMs = 0;
    uint32 _openedMs = 0;
    uint32 _abandonMs = 0;
    uint32 _gapMs = 0;
    uint32 _endMs = 0;
    uint32 _wave = 0;
    uint32 _total = 0;
    float _dealers = 1.0f;
};

// A rift's wave creature: melee only
struct npc_frontier_rift_creature : public ScriptedAI
{
    explicit npc_frontier_rift_creature(Creature* creature) : ScriptedAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }
};

// A rift's guardian: its melee, Éclat de cristal under a fighter and Nova arcanique around itself
struct npc_frontier_rift_guardian : public ScriptedAI
{
    explicit npc_frontier_rift_guardian(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        scheduler.CancelAll();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        scheduler.Schedule(6s, [this](TaskContext context)
        {
            CrystalShard();
            context.Repeat(8s, 11s);
        });
        scheduler.Schedule(12s, [this](TaskContext context)
        {
            ArcaneNova();
            context.Repeat(15s, 18s);
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        scheduler.Update(diff, [this] { DoMeleeAttackIfReady(); });
    }

private:
    // A hit on whoever stands in area when it lands
    void Land(GroundIndicators::Area const& area, uint32 spellId, uint32 stock, float pct)
    {
        Tier const* tier = TierAt(me);
        if (!tier)
            return;
        uint32 const amount = uint32(TierPlayerHealth(*tier) * pct / 100.0f);
        for (Player* player : TierPlayersNear(me, RiftReach + NovaRadius))
            if (area.Contains(player->GetPosition()))
            {
                MythicTuning::DealAbilityDamage(me, player, SpellOrStock(spellId, stock), amount);
                MythicTuning::ApplyImprudence(player);
            }
    }

    void CrystalShard()
    {
        std::vector<Player*> players = TierPlayersNear(me, RiftReach);
        if (players.empty())
            return;
        Player* target = Acore::Containers::SelectRandomContainerElement(players);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, target->GetPosition(), CrystalRadius,
            CrystalWarningMs, GroundIndicators::Theme::Arcane);
        scheduler.Schedule(Milliseconds(CrystalWarningMs), [this, area](TaskContext)
        {
            Land(area, SPELL_CRYSTAL_SHARD, SPELL_ARCANE_STOCK, CrystalPct);
        });
    }

    void ArcaneNova()
    {
        me->HandleEmoteCommand(EMOTE_ONESHOT_SPELL_CAST_OMNI);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), NovaRadius,
            NovaWarningMs, GroundIndicators::Theme::Arcane);
        scheduler.Schedule(Milliseconds(NovaWarningMs), [this, area](TaskContext)
        {
            Land(area, SPELL_ARCANE_NOVA, SPELL_ARCANE_STOCK, NovaPct);
        });
    }
};

// --- The Colosses --------------------------------------------------------------------------------------------------
// A Colosse: a light melee and four abilities on the ground indicators, the same for the four but in their own colours
// and names. It grows as players join its fight, and rewards every player who fought it.
struct npc_frontier_colossus : public ScriptedAI
{
    explicit npc_frontier_colossus(Creature* creature) : ScriptedAI(creature),
        _index(uint8(std::min<uint32>(creature->GetEntry() - NPC_COLOSSI[0], NPC_COLOSSI.size() - 1))) { }

    void Reset() override
    {
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        me->SetControlled(false, UNIT_STATE_ROOT);
        _dealers = 1.0f;
        _rings = 0;
        _rescaleMs = 0;
        Scale();
    }

    void JustEngagedWith(Unit* who) override
    {
        _dealers = std::max(Participants(me, who), DealersNear());
        Scale();
        scheduler.Schedule(8s, GroupAbilities, [this](TaskContext context)
        {
            Cone();
            context.Repeat(12s, 15s);
        });
        scheduler.Schedule(14s, GroupAbilities, [this](TaskContext context)
        {
            Spread();
            context.Repeat(18s, 22s);
        });
        scheduler.Schedule(20s, GroupAbilities, [this](TaskContext context)
        {
            Stomp();
            context.Repeat(22s, 26s);
        });
    }

    // At two thirds and a third of its health, its ring (the second one waits for the first to land)
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        if (!me->IsInCombat() || me->HasUnitState(UNIT_STATE_ROOT))
            return;
        uint8 const due = me->HealthBelowPctDamaged(33, damage) ? 2 : me->HealthBelowPctDamaged(66, damage) ? 1 : 0;
        if (due > _rings)
        {
            _rings = due;
            Ring();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;
        // Players joining the fight make it bigger, never smaller
        if ((_rescaleMs += diff) >= ManagerTickMs)
        {
            _rescaleMs = 0;
            if (float const dealers = DealersNear(); dealers > _dealers)
            {
                _dealers = dealers;
                Scale();
            }
        }
        scheduler.Update(diff, [this] { DoMeleeAttackIfReady(); });
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        Tier const& tier = Tiers[_index];
        std::set<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->IsInWorld() && SeesTier(player) &&
                player->GetDistance(me) <= ColossusRewardReach)
                players.insert(player);
        RewardPlayers(players, tier, tier.colossusShards, 100.0f, ColossusEssenceChance);
        LOG_INFO("module.frontier", "Front du Nord: Colosse {} slain in zone {} by {:.2f} damage dealers, {} player(s) "
            "near", _index + 1, me->GetZoneId(), _dealers, players.size());
        if (Colossi.here && me->GetGUID() == Colossi.guid)
        {
            Colossi.slain = true;
            TellEveryone(ColossusMessage(ColossusNews::Slain, *Colossi.here, 0));
        }
        me->DespawnOrUnsummon(2min);
    }

private:
    static constexpr uint32 GroupAbilities = 1;

    enum Ability : uint8
    {
        AbilityCone,
        AbilitySpread,
        AbilityStomp,
        AbilityRing
    };

    ColossusKind const& Kind() const { return ColossusKinds[_index]; }

    void Scale()
    {
        Tier const& tier = Tiers[_index];
        ScaleUnit(me, TierDps(tier) * _dealers * ColossusSeconds, TierPlayerHealth(tier) * ColossusSwingPct / 100.0f);
    }

    float DealersNear() const
    {
        float dealers = 0.0f;
        for (Player* player : TierPlayersNear(me, ColossusReach))
            dealers += DealerShare(player);
        return std::max(dealers, 1.0f);
    }

    uint32 Amount(float pct) const
    {
        return uint32(TierPlayerHealth(Tiers[_index]) * pct / 100.0f);
    }

    // A hit on every player standing in area when it lands
    void Land(GroundIndicators::Area const& area, Ability ability, float pct)
    {
        uint32 const spellId = SpellOrStock(SPELL_COLOSSUS_FIRST + _index * 4 + ability, SPELL_DEVASTATING_BLOW_STOCK);
        for (Player* player : TierPlayersNear(me, ColossusReach + RingOuter))
            if (area.Contains(player->GetPosition()))
            {
                MythicTuning::DealAbilityDamage(me, player, spellId, Amount(pct));
                MythicTuning::ApplyImprudence(player);
            }
    }

    // Toward its target, wherever they stand: step aside
    void Cone()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;
        float const facing = me->GetAngle(victim);
        me->SetFacingTo(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing, ConeRadius,
            ConeArc, ConeWarningMs, Kind().cone, Amount(ConePct));
        scheduler.Schedule(Milliseconds(ConeWarningMs), [this, area](TaskContext)
        {
            Land(area, AbilityCone, ConePct);
        });
    }

    // A circle under every fighter: spread out, each circle hits whoever stands in it
    void Spread()
    {
        std::vector<GroundIndicators::Area> areas;
        for (Player* player : TierPlayersNear(me, ColossusReach))
            areas.push_back(GroundIndicators::ShowCircle(me, player->GetPosition(), SpreadRadius, SpreadWarningMs,
                Kind().spread, Amount(SpreadPct)));
        scheduler.Schedule(Milliseconds(SpreadWarningMs), [this, areas](TaskContext)
        {
            for (GroundIndicators::Area const& area : areas)
                Land(area, AbilitySpread, SpreadPct);
        });
    }

    // A circle around itself: get out, its tank too
    void Stomp()
    {
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK2HTIGHT);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), StompRadius,
            StompWarningMs, Kind().stomp, Amount(StompPct));
        scheduler.Schedule(Milliseconds(StompWarningMs), [this, area](TaskContext)
        {
            Land(area, AbilityStomp, StompPct);
        });
    }

    // Everything but the ground at its feet: come in. It holds still, and its other abilities wait for it.
    void Ring()
    {
        scheduler.DelayGroup(GroupAbilities, Milliseconds(RingWarningMs + 2000));
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->HandleEmoteCommand(EMOTE_ONESHOT_SPELL_CAST_OMNI);
        me->TextEmote(Acore::StringFormat("{} se déchaîne : rapprochez-vous de lui !", me->GetName()), nullptr, true);
        GroundIndicators::Area const area = GroundIndicators::ShowRing(me, me->GetPosition(), RingOuter, RingInner,
            RingWarningMs, Kind().ring, Amount(RingPct));
        scheduler.Schedule(Milliseconds(RingWarningMs), [this, area](TaskContext)
        {
            Land(area, AbilityRing, RingPct);
            me->SetControlled(false, UNIT_STATE_ROOT);
        });
    }

    uint8 _index;
    float _dealers = 1.0f;
    uint8 _rings = 0;
    uint32 _rescaleMs = 0;
};

class FrontierPlayerScript : public PlayerScript
{
public:
    FrontierPlayerScript() : PlayerScript("FrontierPlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_UPDATE_ZONE, PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        RefreshPhase(player);
        TellColossi(player);
    }

    void OnPlayerUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override { RefreshPhase(player); }
    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        RefreshPhase(player);
        TellColossi(player);
    }
};

// .frontier: where the Front du Nord stands for a game master
using namespace Acore::ChatCommands;

class FrontierCommandScript : public CommandScript
{
public:
    FrontierCommandScript() : CommandScript("FrontierCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable frontier = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "elite", HandleElite, SEC_GAMEMASTER, Console::No },
            { "kill", HandleKill, SEC_GAMEMASTER, Console::No },
            { "rift", HandleRift, SEC_GAMEMASTER, Console::No },
            { "advance", HandleAdvance, SEC_GAMEMASTER, Console::No },
            { "colossus", HandleColossus, SEC_GAMEMASTER, Console::No },
            { "soon", HandleSoon, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commands = {
            { "frontier", frontier },
        };
        return commands;
    }

    // .frontier info: the zone's tier, the phase, the zone's elites
    static bool HandleInfo(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Zone const* zone = FindZone(player->GetZoneId());
        if (!zone)
        {
            handler->SendSysMessage("Front du Nord: not a tier zone.");
            return true;
        }
        Tier const& tier = TierOf(*zone);
        std::size_t const index = std::size_t(zone - Zones.data());
        handler->PSendSysMessage("Front du Nord: zone {} tier {} (profile {:.0f} / {:.0f}, loot {}), phase {}, "
            "{} elite(s), {} spot(s); elite health {:.0f} solo, swing {:.0f}, blow {:.0f}; {} shard(s) owned",
            zone->id, zone->tier, tier.itemLevel, tier.paragon, tier.lootItemLevel, SeesTier(player) ? "on" : "off",
            States[index].elites.size(), States[index].spots.size(), TierDps(tier) * EliteSeconds,
            TierPlayerHealth(tier) * EliteSwingPct / 100.0f, TierPlayerHealth(tier) * BlowPct / 100.0f,
            player->GetItemCount(ITEM_FROST_SHARD));
        handler->PSendSysMessage("Front du Nord: Colosse {} next in {} s{}; {}", Colossi.next + 1, Colossi.inMs / 1000,
            Colossi.coming ? Acore::StringFormat(" (heard of, zone {})", Zones[Colossi.coming->zone].id) : "",
            Colossi.here ? Acore::StringFormat("Colosse {} in zone {}, {} for {} s more", Colossi.here->index + 1,
                Zones[Colossi.here->zone].id, Colossi.slain ? "slain" : Colossi.guid.IsEmpty() ? "waiting" : "here",
                Colossi.stayMs / 1000) : "none here");
        return true;
    }

    // .frontier colossus [1-4]: that Colosse (the next one otherwise) in front of the game master, now
    static bool HandleColossus(ChatHandler* handler, Optional<uint8> which)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Zone const* zone = FindZone(player->GetZoneId());
        if (!zone || player->GetMapId() != MAP_NORTHREND)
        {
            handler->SendSysMessage("Front du Nord: not a tier zone.");
            return false;
        }
        Map* map = player->GetMap();
        if (Creature* old = Colossi.guid.IsEmpty() ? nullptr : map->GetCreature(Colossi.guid))
            old->DespawnOrUnsummon();
        Position spot = player->GetPosition();
        player->MovePosition(spot, 25.0f, 0.0f);
        uint8 const index = which ? uint8(std::clamp<uint8>(*which, 1, 4) - 1) : Colossi.next;
        Colossi.here = ColossusSpot{ index, std::size_t(zone - Zones.data()), spot };
        Colossi.stayMs = ColossusStayMs;
        Colossi.slain = false;
        Colossi.guid.Clear();
        if (Creature* colossus = SummonColossus(map, *Colossi.here))
        {
            Colossi.guid = colossus->GetGUID();
            handler->PSendSysMessage("Front du Nord: {} here, {} health for one damage dealer.", colossus->GetName(),
                colossus->GetMaxHealth());
        }
        TellEveryone(ColossusMessage(ColossusNews::Here, *Colossi.here, Colossi.stayMs / 1000));
        return true;
    }

    // .frontier soon: the next Colosse in 30 seconds (heard of at the next tick)
    static bool HandleSoon(ChatHandler* handler)
    {
        Colossi.inMs = std::min<uint32>(Colossi.inMs, 30000);
        handler->PSendSysMessage("Front du Nord: Colosse {} in {} s.", Colossi.next + 1, Colossi.inMs / 1000);
        return true;
    }

    // .frontier kill: the zone's nearest roaming elite, slain by the game master (its rewards, for testing)
    static bool HandleKill(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Zone const* zone = FindZone(player->GetZoneId());
        if (!zone)
            return false;
        Creature* nearest = nullptr;
        for (ObjectGuid const& guid : States[std::size_t(zone - Zones.data())].elites)
            if (Creature* elite = player->GetMap()->GetCreature(guid); elite && elite->IsAlive() &&
                (!nearest || player->GetDistance(elite) < player->GetDistance(nearest)))
                nearest = elite;
        if (!nearest)
        {
            handler->SendSysMessage("Front du Nord: no elite in this zone.");
            return false;
        }
        handler->PSendSysMessage("Front du Nord: {} slain at {:.0f} yd.", nearest->GetName(),
            player->GetDistance(nearest));
        nearest->NearTeleportTo(player->GetPositionX() + 3.0f, player->GetPositionY(), player->GetPositionZ(), 0.0f);
        Unit::Kill(player, nearest);
        return true;
    }

    // .frontier rift: a rift here, the zone's own (it opens when a tier player comes near)
    static bool HandleRift(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Zone const* zone = FindZone(player->GetZoneId());
        if (!zone || player->GetMapId() != MAP_NORTHREND)
        {
            handler->SendSysMessage("Front du Nord: not a tier zone.");
            return false;
        }
        Position spot = player->GetPosition();
        player->MovePosition(spot, 25.0f, 0.0f);
        ZoneState& state = States[std::size_t(zone - Zones.data())];
        if (Creature* old = player->GetMap()->GetCreature(state.rift))
            old->DespawnOrUnsummon();
        if (TempSummon* rift = player->GetMap()->SummonCreature(NPC_RIFT, spot))
        {
            rift->SetPhaseMask(FrontierPhaseMask, true);
            state.rift = rift->GetGUID();
        }
        return true;
    }

    // .frontier advance: every rift creature near the game master slain by them (a wave, or the guardian)
    static bool HandleAdvance(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        std::list<Creature*> creatures;
        for (uint32 entry : { RiftCreatures[0], RiftCreatures[1], NPC_RIFT_GUARDIAN, NPC_COLOSSI[0], NPC_COLOSSI[1],
            NPC_COLOSSI[2], NPC_COLOSSI[3] })
            player->GetCreatureListWithEntryInGrid(creatures, entry, 80.0f);
        uint32 slain = 0;
        for (Creature* creature : creatures)
            if (creature->IsAlive())
            {
                Unit::Kill(player, creature);
                ++slain;
            }
        handler->PSendSysMessage("Front du Nord: {} rift creature(s) or Colosse slain.", slain);
        return true;
    }

    // .frontier elite: one of the zone's roaming elites, here
    static bool HandleElite(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        Zone const* zone = FindZone(player->GetZoneId());
        if (!zone || player->GetMapId() != MAP_NORTHREND)
        {
            handler->SendSysMessage("Front du Nord: not a tier zone.");
            return false;
        }
        std::size_t const index = std::size_t(zone - Zones.data());
        Position spot = player->GetPosition();
        player->MovePosition(spot, 15.0f, 0.0f);
        if (TempSummon* elite = player->GetMap()->SummonCreature(zone->elites[urand(0, 1)], spot))
        {
            elite->SetPhaseMask(FrontierPhaseMask, true);
            elite->SetHomePosition(spot);
            ScaleElite(elite, TierOf(*zone), 1.0f);
            States[index].elites.push_back(elite->GetGUID());
        }
        return true;
    }
};
}

void AddFrontierScripts()
{
    new FrontierWorldScript();
    new FrontierPlayerScript();
    new FrontierCommandScript();
    RegisterCreatureAI(npc_frontier_elite);
    RegisterCreatureAI(npc_frontier_rift);
    RegisterCreatureAI(npc_frontier_rift_creature);
    RegisterCreatureAI(npc_frontier_rift_guardian);
    RegisterCreatureAI(npc_frontier_colossus);
}
