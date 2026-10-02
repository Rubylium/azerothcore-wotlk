#include "EssenceTierSystem.h"
#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Containers.h"
#include "CreatureScript.h"
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

#include <algorithm>
#include <array>
#include <cmath>
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
// Milestone 1 (this file): the tiers, the phase, the zone banner and the roaming elites. Each tier is sized on its
// profile with the power model (.agents/docs/systems/power-scaling.md), one damage dealer a participant:
//   Palier I   Borean Tundra, Howling Fjord     187 / 0   drops 200
//   Palier II  Dragonblight, Grizzly Hills      200 / 0   drops 213
//   Palier III Zul'Drak, Sholazar Basin         213 / 0   drops 219
//   Palier IV  Storm Peaks, Icecrown            223 / 0   drops 226 (Mythique 0's profile)
//
// Client: Interface\FrameXML\FrontierUI.lua, on the "Frontier" addon prefix, whispered to the player:
//   ZONE <tier 0-4> <zone id> <loot item level>     entering (or leaving: tier 0) a tier zone
//   PINS <zone id> <kind>:<x>:<y>,...               the zone's content to pin on the maps (kind E: a roaming elite)

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
    uint32 eliteShards;         // Éclats de givre a roaming elite gives
};

// Tiers I-IV (index 0-3)
constexpr std::array<Tier, 4> Tiers = { {
    { 187.0f, 0.0f, 200, 2 },
    { 200.0f, 0.0f, 213, 3 },
    { 213.0f, 0.0f, 219, 4 },
    { 223.0f, 0.0f, 226, 5 },
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

// Who the reference model counts below Mythique 0: its measured curve starts at 223, and its straight line below
// that drops a fresh 80 to a third of what it deals. Item level 223's damage, scaled by the square of the item level
// ratio (gear's stats grow about so) - to re-measure on the combat bench.
constexpr float ModelFloorItemLevel = 223.0f;

float TierDps(Tier const& tier)
{
    float const ratio = std::min(tier.itemLevel / ModelFloorItemLevel, 1.0f);
    return Power::ExpectedDps(std::max(tier.itemLevel, ModelFloorItemLevel), tier.paragon) * ratio * ratio;
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

void ScaleElite(Creature* elite, Tier const& tier, float dealers)
{
    uint32 const maxHealth = uint32(std::max(1.0f, TierDps(tier) * dealers * EliteSeconds));
    float const pct = elite->GetHealthPct();
    elite->SetCreateHealth(maxHealth);
    elite->SetMaxHealth(maxHealth);
    elite->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(maxHealth));
    float const swing = TierPlayerHealth(tier) * EliteSwingPct / 100.0f * ArmourMargin;
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
    {
        elite->SetBaseWeaponDamage(attackType, MINDAMAGE, swing * 0.85f);
        elite->SetBaseWeaponDamage(attackType, MAXDAMAGE, swing * 1.15f);
    }
    elite->UpdateAllStats();
    elite->SetHealth(uint32(maxHealth * pct / 100.0f));
    elite->ResetPlayerDamageReq();
}

// --- The zones' state ----------------------------------------------------------------------------------------------
struct ZoneState
{
    std::vector<Position> spots;        // its creatures' own spawn points: valid ground, built once
    bool spotsBuilt = false;
    std::vector<ObjectGuid> elites;
    std::vector<uint32> respawns;       // a slain elite's place, the time left before it is taken again
    uint32 idleMs = 0;
    uint8 nextEntry = 0;
};

std::array<ZoneState, Zones.size()> States;

void BuildSpots(Zone const& zone, ZoneState& state)
{
    state.spotsBuilt = true;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        if (data.mapid == MAP_NORTHREND && (data.phaseMask & PHASEMASK_NORMAL) &&
            data.posX >= zone.xMin && data.posX <= zone.xMax && data.posY >= zone.yMin && data.posY <= zone.yMax)
            state.spots.emplace_back(data.posX, data.posY, data.posZ, frand(0.0f, 2.0f * float(M_PI)));
    LOG_INFO("module.frontier", "Front du Nord: zone {} has {} spawn spots", zone.id, state.spots.size());
}

Creature* SpawnElite(Map* map, Zone const& zone, ZoneState& state, std::vector<Player*> const& players)
{
    if (!state.spotsBuilt)
        BuildSpots(zone, state);
    if (state.spots.empty())
        return nullptr;

    for (uint32 attempt = 0; attempt < EliteSpawnTries; ++attempt)
    {
        Position const& spot = Acore::Containers::SelectRandomContainerElement(state.spots);
        if (std::ranges::any_of(players, [&spot](Player* player)
            { return player->GetExactDist2d(&spot) < EliteSpawnClearance; }))
            continue;

        uint32 const entry = zone.elites[state.nextEntry % zone.elites.size()];
        TempSummon* elite = map->SummonCreature(entry, spot);
        if (!elite)
            continue;
        // The spawn rectangles overlap their neighbours: the zone is checked where the creature stands
        if (elite->GetZoneId() != zone.id || elite->IsInWater())
        {
            elite->DespawnOrUnsummon();
            continue;
        }
        elite->SetPhaseMask(FrontierPhaseMask, true);
        elite->SetHomePosition(spot);
        ScaleElite(elite, TierOf(zone), 1.0f);
        ++state.nextEntry;
        return elite;
    }
    return nullptr;
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
    for (ObjectGuid const& guid : state.elites)
        if (Creature* elite = map->GetCreature(guid))
            if (elite->IsAlive())
                pins += Acore::StringFormat("{}E:{:.0f}:{:.0f}", pins.empty() ? "" : ",", elite->GetPositionX(),
                    elite->GetPositionY());
    return pins;
}

// Every few seconds: each tier zone with a tier player in it keeps its roaming elites; an empty one loses them
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

            std::string const pins = Pins(map, state);
            for (Player* player : present[index])
                if (!IsBot(player))
                    SendAddon(player, Acore::StringFormat("PINS\t{}\t{}", zone.id, pins));
        }
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

class FrontierPlayerScript : public PlayerScript
{
public:
    FrontierPlayerScript() : PlayerScript("FrontierPlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_UPDATE_ZONE, PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override { RefreshPhase(player); }
    void OnPlayerUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override { RefreshPhase(player); }
    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override { RefreshPhase(player); }
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
}
