#include "InfiniteDungeonSystem.h"

#include "InfiniteDungeonArenas.h"
#include "InfiniteDungeonScaling.h"

#include "EssenceTierSystem.h"
#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"
#include "ParagonSystem.h"
#include "SmartLootSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Formulas.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "InfiniteDungeon.h"
#include "InstanceSaveMgr.h"
#include "Item.h"
#include "Mail.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The Infinite Dungeon (Donjon infini): an endless ladder of short floors for one or two real players. The design is
// .agents/plans/infinite-dungeon/infinite-dungeon.DESIGN.md; the numbers are in InfiniteDungeonScaling.h.
//
// A run is started at a keeper (920000, in every capital) alone or with a group partner. Each floor is a fresh
// instance of a dungeon map (an arena of InfiniteDungeonArenas.cpp, picked by the run's level): the players are
// unbound from that map and teleported with a new instance, where they and everything of the run live in their own
// phase (InfiniteDungeon.h), so the dungeon's own creatures stay where they are and never meet them. The floor is
// built when the first player arrives: a ring on the ground around the arrival point (the bubble: nothing can attack
// the players in it, nor they anything), two trash packs and the boss. Stepping out of the ring starts the floor for
// good. The boss down, a portal opens to the next floor, and every tenth floor keeps the progress (a checkpoint) and
// leaves a chest.
//
// A run lives in the world thread's gossip and hooks and in its floor map's thread: every access to the runs holds
// the lock. The floor itself (spawns, the bubble, the hearts, deaths) is driven from its map's update.
namespace
{
using namespace InfiniteDungeon;
// The core has an Arena of its own (arena battlegrounds)
using ArenaInfo = InfiniteDungeon::Arena;

constexpr std::string_view Prefix = "Infinite";

// The keeper is 920000 (npc_infinite_dungeon_keeper); the floor creatures (stat_growth_infinite_dungeon_creatures.sql)
constexpr uint32 FirstFloorCreature = 920010;
constexpr uint32 LastFloorCreature = 920299;
// The ground indicators' invisible stalker (GroundIndicators.cpp): the floor's summoner, and the bubble's ring when it
// wears the indicator circle at the bubble's size
constexpr uint32 NPC_STALKER = 900104;
constexpr uint32 SPELL_RING = 90700;
constexpr uint32 GO_PORTAL = 920100;
constexpr uint32 GO_HEART = 920101;
constexpr uint32 GO_CHEST = 920102;
constexpr uint32 GO_PORTAL_LIGHT = 920103;
constexpr uint32 NPC_TEXT_KEEPER = 920000;
constexpr uint32 NPC_TEXT_PORTAL = 920001;

// Spells the telegraphed abilities are named as in the combat log, by the look of their area
constexpr uint32 SPELL_LOG_PHYSICAL = 845;      // Cleave
constexpr uint32 SPELL_LOG_SHADOW = 686;        // Shadow Bolt
constexpr uint32 SPELL_LOG_FIRE = 2120;         // Flamestrike
constexpr uint32 SPELL_LOG_FROST = 120;         // Cone of Cold
constexpr uint32 SPELL_LOG_NATURE = 421;        // Chain Lightning
constexpr uint32 SPELL_LOG_ARCANE = 1449;       // Arcane Explosion
constexpr uint32 SPELL_LOG_HOLY = 26573;        // Consecration

constexpr float BubbleRadius = 6.0f;
constexpr float HeartPickupRange = 2.0f;
constexpr float PartnerRange = 40.0f;
// The portal stands this far off the boss spot, and at least this far from the guardian's body; the checkpoint chest
// this far beside it
constexpr float PortalOffset = 7.0f;
constexpr float PortalCorpseClearance = 5.0f;
constexpr float ChestBesidePortal = 5.0f;
constexpr float AbilityReach = 60.0f;
constexpr float TargetRange = 30.0f;
// Two packs stand at least this far apart, and trash spots this far from the bubble
constexpr float PackSpacing = 12.0f;
constexpr float PackBubbleClearance = 12.0f;
constexpr uint32 TravelTimeoutMs = 90 * IN_MILLISECONDS;
constexpr uint32 FailDelayMs = 4 * IN_MILLISECONDS;
constexpr uint32 StrayCheckMs = 1000;
constexpr uint32 PendingResetMaxMs = 10 * MINUTE * IN_MILLISECONDS;

enum class MobRole : uint8
{
    Trash,
    Elite,
    Boss
};

enum class FloorState : uint8
{
    Travelling,     // the players are on their way to the floor's instance
    Bubble,         // built; nobody has left the ring yet
    Fighting,
    Cleared,        // the boss is down, the portal open
    Failed          // everyone is dead: the run ends in a moment
};

enum class Role : uint8
{
    Damage,
    Tank,
    Healer
};

enum class EndReason : uint8
{
    Left = 0,       // through the portal, or a command
    Fallen = 1,     // everyone died
    Lost = 2        // nobody reached the floor
};

enum KeeperAction : uint32
{
    ACTION_CONTINUE = 1,
    ACTION_RESTART,
    ACTION_CONTINUE_DUO,
    ACTION_RESTART_DUO,
    ACTION_RECORDS,
    ACTION_LEADERBOARD
};

enum PortalAction : uint32
{
    ACTION_DESCEND = 1,
    ACTION_LEAVE
};

// A character's progress on both ladders
struct Progress
{
    std::array<uint32, 2> checkpoint{};
    std::array<uint32, 2> best{};
};

struct Member
{
    ObjectGuid guid;
    WorldLocation home;
    Role role = Role::Damage;
    bool chestClaimed = false;
    // What the run gave it so far, for the summary at the end (SUMMARY)
    uint32 floorsCleared = 0;
    uint32 gold = 0;
    uint32 experience = 0;
    uint32 essences = 0;
    uint32 items = 0;
    uint32 paragon = 0;
    // The last PROG line it was sent: a line is only sent again when it changes
    std::string lastProgress;
};

struct Run
{
    uint32 id = 0;
    Ladder ladder = Ladder::Levelling;
    uint32 floor = 1;
    uint32 startFloor = 1;
    uint32 deaths = 0;              // every fall of a member during the run
    uint32 heartsTaken = 0;
    uint8 level = MinPlayerLevel;
    std::vector<Member> members;
    std::size_t arena = 0;
    std::optional<std::size_t> previousArena;
    std::optional<std::size_t> forcedArena;
    uint32 mapId = 0;
    uint32 instanceId = 0;          // the floor's instance, once someone arrived
    FloorState state = FloorState::Travelling;
    uint32 stateMs = 0;
    std::set<uint32> usedMaps;

    // The floor
    ObjectGuid anchor;
    ObjectGuid ring;
    ObjectGuid boss;
    std::vector<std::pair<ObjectGuid, MobRole>> creatures;
    std::set<ObjectGuid> fallen;
    std::set<ObjectGuid> deadMembers;   // members dead right now, to count each fall once
    std::vector<ObjectGuid> hearts;
    float healthFactor = 1.0f;      // the roles' and the floor's
    float damageFactor = 1.0f;
    float referenceHealth = 1.0f;
};

// Where a player in a run goes back to, on the player itself: a player that finds it on itself with no run (the run
// ended while it was elsewhere) cleans up and goes home
struct HomeData : public DataMap::Base
{
    WorldLocation home;
    uint32 nextCheckMs = 0;
};
constexpr char const* HomeKey = "InfiniteDungeonHome";

// What a floor creature is and what its abilities hit for
struct FloorCreature : public DataMap::Base
{
    MobRole role = MobRole::Trash;
    float abilityUnit = 0.0f;       // an ability's damage per percent of its share
};
constexpr char const* FloorCreatureKey = "InfiniteDungeonCreature";

struct PendingReset
{
    uint32 mapId = 0;
    uint32 instanceId = 0;
    uint32 ageMs = 0;
};

std::recursive_mutex Lock;
std::unordered_map<uint32, std::unique_ptr<Run>> Runs;
std::unordered_map<ObjectGuid, uint32> RunByPlayer;
std::unordered_map<uint32, uint32> RunByInstance;
std::unordered_map<ObjectGuid::LowType, Progress> ProgressByGuid;
std::vector<PendingReset> PendingResets;
std::atomic<uint32> ActiveRuns{ 0 };
uint32 NextRunId = 0;

// The creature being spawned for a floor: its level and scaling are set as it is created (the level hooks below)
struct SpawnContext
{
    uint8 level;
    MobRole role;
    float healthFactor;
    float damageFactor;
    float referenceHealth;
};
thread_local SpawnContext const* CurrentSpawn = nullptr;

// -----------------------------------------------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------------------------------------------

bool IsFrench(Player const* player)
{
    return player && player->GetSession() && player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
}

std::string Text(Player const* player, std::string_view english, std::string_view french)
{
    return std::string(IsFrench(player) ? french : english);
}

void SendAddon(Player* player, std::string const& body)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;

    WorldPacket packet;
    std::string const payload = std::string(Prefix) + "\t" + body;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

void Say(Player* player, std::string const& text)
{
    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage("|cffffd24d" + text + "|r");
}

uint64 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

bool IsAtLevelCap(Player const* player)
{
    return player->GetLevel() >= sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);
}

ArenaInfo const& ArenaOf(Run const& run)
{
    return GetArenas()[run.arena];
}

std::string ArenaName(Player const* player, ArenaInfo const& arena)
{
    return IsFrench(player) ? arena.nameFr : arena.nameEn;
}

Run* RunOf(ObjectGuid guid)
{
    auto const itr = RunByPlayer.find(guid);
    if (itr == RunByPlayer.end())
        return nullptr;
    auto const run = Runs.find(itr->second);
    return run == Runs.end() ? nullptr : run->second.get();
}

Member* MemberOf(Run& run, ObjectGuid guid)
{
    for (Member& member : run.members)
        if (member.guid == guid)
            return &member;
    return nullptr;
}

Progress& ProgressOf(Player const* player)
{
    return ProgressByGuid[player->GetGUID().GetCounter()];
}

void SaveProgress(Player const* player)
{
    Progress const& progress = ProgressOf(player);
    CharacterDatabase.Execute(
        "INSERT INTO character_infinite_dungeon (guid, class, checkpoint, best, checkpoint_max, best_max) "
        "VALUES ({}, {}, {}, {}, {}, {}) ON DUPLICATE KEY UPDATE class = VALUES(class), "
        "checkpoint = VALUES(checkpoint), best = VALUES(best), checkpoint_max = VALUES(checkpoint_max), "
        "best_max = VALUES(best_max)",
        player->GetGUID().GetCounter(), player->getClass(), progress.checkpoint[0], progress.best[0],
        progress.checkpoint[1], progress.best[1]);
}

void SaveRunState(Player const* player, bool inRun, WorldLocation const& home)
{
    CharacterDatabase.Execute(
        "INSERT INTO character_infinite_dungeon (guid, class, in_run, return_map, return_x, return_y, return_z, "
        "return_o) VALUES ({}, {}, {}, {}, {}, {}, {}, {}) ON DUPLICATE KEY UPDATE in_run = VALUES(in_run), "
        "return_map = VALUES(return_map), return_x = VALUES(return_x), return_y = VALUES(return_y), "
        "return_z = VALUES(return_z), return_o = VALUES(return_o)",
        player->GetGUID().GetCounter(), player->getClass(), inRun ? 1 : 0, home.GetMapId(), home.GetPositionX(),
        home.GetPositionY(), home.GetPositionZ(), home.GetOrientation());
}

void LoadProgress(Player const* player)
{
    Progress progress;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT checkpoint, best, checkpoint_max, best_max FROM character_infinite_dungeon WHERE guid = {}",
            player->GetGUID().GetCounter()))
    {
        Field* fields = result->Fetch();
        progress.checkpoint[0] = fields[0].Get<uint32>();
        progress.best[0] = fields[1].Get<uint32>();
        progress.checkpoint[1] = fields[2].Get<uint32>();
        progress.best[1] = fields[3].Get<uint32>();
    }
    std::lock_guard<std::recursive_mutex> guard(Lock);
    ProgressByGuid[player->GetGUID().GetCounter()] = progress;
}

Role RoleOf(Player* player)
{
    if (IsGroupTank(player) || player->HasTankSpec())
        return Role::Tank;
    if (player->HasHealSpec())
        return Role::Healer;
    return Role::Damage;
}

// The run's members that are in this map right now
std::vector<Player*> PresentMembers(Run const& run, Map const* map)
{
    std::vector<Player*> present;
    for (Member const& member : run.members)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(member.guid); player && player->IsInWorld() &&
            player->FindMap() == map)
            present.push_back(player);
    return present;
}

void EnsurePhase(Player* player)
{
    if (!player->IsGameMaster() && player->GetPhaseMask() != PhaseMask)
        player->SetPhaseMask(PhaseMask, true);
}

// The bubble: nothing can attack them, nor they anything (UNIT_FLAG_IMMUNE_TO_NPC works both ways), pets included
void SetSheltered(Player* player, bool apply)
{
    if (player->IsImmuneToNPC() != apply)
        player->SetImmuneToNPC(apply);
    for (Unit* controlled : player->m_Controlled)
        if (controlled && controlled->IsImmuneToNPC() != apply)
            controlled->SetImmuneToNPC(apply);
}

void Unbind(ObjectGuid guid, uint32 mapId, Player* player)
{
    if (InstancePlayerBind* bind = sInstanceSaveMgr->PlayerGetBoundInstance(guid, mapId, DUNGEON_DIFFICULTY_NORMAL);
        bind && !bind->perm)
        sInstanceSaveMgr->PlayerUnbindInstance(guid, mapId, DUNGEON_DIFFICULTY_NORMAL, true, player);
}

void QueueReset(uint32 mapId, uint32 instanceId)
{
    if (instanceId)
        PendingResets.push_back({ mapId, instanceId, 0 });
}

// Takes everything of the run off a player: the pass, the phase, the bubble
void RestorePlayer(Player* player)
{
    player->CustomData.Erase(PassKey);
    player->CustomData.Erase(HomeKey);
    SetSheltered(player, false);
    if (!player->IsGameMaster() && (player->GetPhaseMask() & PhaseMask))
        player->SetPhaseMask(PHASEMASK_NORMAL, true);
}

void SendToHome(Player* player, WorldLocation const& home)
{
    if (!player->IsAlive())
    {
        player->ResurrectPlayer(1.0f);
        player->SpawnCorpseBones();
    }
    if (home.GetMapId() != MAPID_INVALID && (home.GetPositionX() != 0.0f || home.GetPositionY() != 0.0f))
        player->TeleportTo(home);
    else
        player->TeleportTo(player->m_homebindMapId, player->m_homebindX, player->m_homebindY, player->m_homebindZ,
            player->GetOrientation());
}

// -----------------------------------------------------------------------------------------------------------------
// Client messages (InfiniteDungeon.lua)
// -----------------------------------------------------------------------------------------------------------------

uint32 LastCheckpoint(uint32 floor)
{
    return floor > 0 ? (floor - 1) / CheckpointFloors * CheckpointFloors : 0;
}

// The messages, prefix "Infinite", tab-separated (InfiniteDungeon.lua reads them):
//   HUD <floor> <checkpoint> <ladder> <step> <paragon> <state> <arena> <level> <players> <best>
//   ARRIVE <floor> <arena> <ladder> <step> <paragon>
//   PROG <foes down> <foes> <boss down 0/1> <hearts on the ground> <hearts taken> <deaths> <partner> <partner state>
//        partner state: 0 no partner, 1 alive on the floor, 2 dead, 3 not on the floor
//   REWARD <floor> <gold copper> <experience> <essences> <gear 0/1>        a floor cleared, before CLEAR
//   CLEAR <floor> <checkpoint reached 0/1>
//   CHEST <essences> <paragon points>                                    the checkpoint chest opened
//   SUMMARY <ladder> <start floor> <floor> <floors cleared> <best> <checkpoint> <gold> <experience> <essences>
//           <items> <paragon>                                             the run is over for it, before END
//   END <reason> <floor>
// and from the client: STATE (entering the world: the panel again, or END 0 0 out of a run)
// HUD floor checkpoint ladder step paragon state arena level players best
void SendHud(Player* player, Run const& run)
{
    SendAddon(player, Acore::StringFormat("HUD\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", run.floor,
        LastCheckpoint(run.floor), static_cast<uint32>(run.ladder), GetStep(run.floor),
        GetRecommendedParagon(run.ladder, run.floor), static_cast<uint32>(run.state), ArenaName(player, ArenaOf(run)),
        run.level, run.members.size(), ProgressOf(player).best[static_cast<std::size_t>(run.ladder)]));
}

void SendHudToAll(Run const& run, Map const* map)
{
    for (Player* player : PresentMembers(run, map))
        SendHud(player, run);
}

// The floor's progress for the tracker: foes down, the guardian, the hearts, the falls and the partner. Sent only
// when it changed since the last line that member got.
void SendProgress(Run& run, Map const* map)
{
    uint32 foes = 0;
    uint32 foesDown = 0;
    bool bossDown = run.state == FloorState::Cleared;
    for (auto const& [guid, role] : run.creatures)
    {
        bool const down = run.fallen.count(guid) != 0;
        if (role == MobRole::Boss)
        {
            bossDown = bossDown || down;
            continue;
        }
        ++foes;
        if (down)
            ++foesDown;
    }

    for (Member& member : run.members)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
        if (!player || !player->IsInWorld() || player->FindMap() != map)
            continue;

        std::string partnerName;
        uint32 partnerState = 0;
        for (Member const& other : run.members)
        {
            if (other.guid == member.guid)
                continue;
            Player* partner = ObjectAccessor::FindConnectedPlayer(other.guid);
            if (!partner)
                continue;
            partnerName = partner->GetName();
            partnerState = partner->IsInWorld() && partner->FindMap() == map ? (partner->IsAlive() ? 1 : 2) : 3;
        }

        std::string line = Acore::StringFormat("PROG\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", foesDown, foes,
            bossDown ? 1 : 0, run.hearts.size(), run.heartsTaken, run.deaths, partnerName, partnerState);
        if (line == member.lastProgress)
            continue;
        member.lastProgress = line;
        SendAddon(player, line);
    }
}

// SUMMARY ladder start floor cleared best checkpoint gold experience essences items paragon
void SendSummary(Player* player, Run const& run, Member const& member)
{
    Progress const& progress = ProgressOf(player);
    std::size_t const ladder = static_cast<std::size_t>(run.ladder);
    SendAddon(player, Acore::StringFormat("SUMMARY\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", ladder, run.startFloor,
        run.floor, member.floorsCleared, progress.best[ladder], progress.checkpoint[ladder], member.gold,
        member.experience, member.essences, member.items, member.paragon));
}

// -----------------------------------------------------------------------------------------------------------------
// Scaling
// -----------------------------------------------------------------------------------------------------------------

float ReferenceHealthAt(uint8 level)
{
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, CLASS_WARRIOR);
    return static_cast<float>(stats->BaseHealth[GetStatsExpansion(level)]) * GetReferenceHealthFactor(level);
}

// The run's factors: the roles in it and the floor
void ComputeFactors(Run& run)
{
    float weight = 0.0f;
    bool tank = false;
    for (Member const& member : run.members)
    {
        switch (member.role)
        {
            case Role::Tank:
                weight += TankWeight;
                tank = true;
                break;
            case Role::Healer:
                weight += HealerWeight;
                break;
            default:
                weight += DamageDealerWeight;
                break;
        }
    }

    float const floorScaling = GetFloorScaling(run.ladder, run.floor);
    float const damage = tank ? TankedDamage : run.members.size() > 1 ? UntankedDuoDamage : UntankedSoloDamage;
    run.healthFactor = std::max(weight, HealerWeight) * floorScaling;
    run.damageFactor = damage * floorScaling;
    run.referenceHealth = ReferenceHealthAt(run.level);
}

void ApplyScaling(Creature* creature, MobRole role, uint8 level, float healthFactor, float damageFactor,
    float referenceHealth)
{
    CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, cinfo->unit_class);
    uint8 const expansion = GetStatsExpansion(level);

    float const healthRank = role == MobRole::Boss ? BossHealthRank : role == MobRole::Elite ? EliteHealthRank : 1.0f;
    double const health = static_cast<double>(stats->BaseHealth[expansion]) * TrashHealth[expansion] *
        GetLevelHealthFactor(level) * healthRank * healthFactor;
    uint32 const maxHealth = static_cast<uint32>(std::clamp<double>(health, 1.0, std::numeric_limits<int32>::max()));
    creature->SetCreateHealth(maxHealth);
    creature->SetMaxHealth(maxHealth);
    creature->SetHealth(maxHealth);
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, static_cast<float>(maxHealth));
    creature->ResetPlayerDamageReq();

    float const damageRank = role == MobRole::Boss ? BossDamageRank : role == MobRole::Elite ? 1.0f : TrashDamageRank;
    float const damage = stats->BaseDamage[expansion] * EliteDamage[expansion] * damageRank * damageFactor;
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
    {
        creature->SetBaseWeaponDamage(attackType, MINDAMAGE, damage);
        creature->SetBaseWeaponDamage(attackType, MAXDAMAGE, damage * 1.5f);
    }
    creature->UpdateAllStats();

    FloorCreature* data = creature->CustomData.GetDefault<FloorCreature>(FloorCreatureKey);
    data->role = role;
    data->abilityUnit = referenceHealth * damageFactor / 100.0f;
}

// Level and stats of a floor creature as it is created: the run's level, its role's scaling
class InfiniteDungeonCreatureLevelScript : public AllCreatureScript
{
public:
    InfiniteDungeonCreatureLevelScript() : AllCreatureScript("InfiniteDungeonCreatureLevelScript") { }

    void OnBeforeCreatureSelectLevel(CreatureTemplate const* /*cinfo*/, Creature* /*creature*/, uint8& level) override
    {
        if (CurrentSpawn)
            level = CurrentSpawn->level;
    }

    void OnCreatureSelectLevel(CreatureTemplate const* /*cinfo*/, Creature* creature) override
    {
        if (CurrentSpawn)
            ApplyScaling(creature, CurrentSpawn->role, CurrentSpawn->level, CurrentSpawn->healthFactor,
                CurrentSpawn->damageFactor, CurrentSpawn->referenceHealth);
    }
};

// -----------------------------------------------------------------------------------------------------------------
// Building a floor
// -----------------------------------------------------------------------------------------------------------------

Position SpotPosition(ArenaSpot const& spot)
{
    return Position(spot.x, spot.y, spot.z, spot.o);
}

// A point on the ground near `near`, at `height` or the floor found close to it
Position GroundPoint(Map* map, float x, float y, float height, float orientation)
{
    float z = map->GetHeight(PhaseMask, x, y, height + 3.0f, true, 10.0f);
    if (z < height - 6.0f || z > height + 3.0f)
        z = height;
    return Position(x, y, z + 0.1f, orientation);
}

TempSummon* SpawnMob(Run const& run, Creature* anchor, uint32 entry, MobRole role, Position const& position)
{
    SpawnContext const context{ run.level, role, run.healthFactor, run.damageFactor, run.referenceHealth };
    CurrentSpawn = &context;
    TempSummon* creature = anchor->SummonCreature(entry, position, TEMPSUMMON_MANUAL_DESPAWN);
    CurrentSpawn = nullptr;
    return creature;
}

// The spots the packs stand on: the arena's trash spots the boss spot sees, away from the bubble, and else points
// on the way from the bubble to the boss
std::vector<Position> PackSpots(ArenaInfo const& arena, Creature* anchor)
{
    Position const entry = SpotPosition(arena.entry);
    Position const boss = SpotPosition(arena.boss);
    std::vector<Position> spots;
    for (uint32 index = 0; index < arena.trashSpots && index < MaxTrashSpots; ++index)
    {
        ArenaSpot const& spot = arena.trash[index];
        Position const position(spot.x, spot.y, spot.z, 0.0f);
        if (entry.GetExactDist2d(&position) < PackBubbleClearance)
            continue;
        if (!anchor->IsWithinLOS(spot.x, spot.y, spot.z + 2.0f))
            continue;
        spots.push_back(position);
    }
    Acore::Containers::RandomShuffle(spots);

    std::vector<Position> chosen;
    for (Position const& spot : spots)
    {
        if (std::any_of(chosen.begin(), chosen.end(),
                [&spot](Position const& other) { return other.GetExactDist2d(&spot) < PackSpacing; }))
            continue;
        chosen.push_back(spot);
        if (chosen.size() == 2)
            break;
    }

    // Too few: the way from the bubble to the boss
    for (float fraction : { 0.45f, 0.72f })
    {
        if (chosen.size() >= 2)
            break;
        float const x = entry.GetPositionX() + (boss.GetPositionX() - entry.GetPositionX()) * fraction;
        float const y = entry.GetPositionY() + (boss.GetPositionY() - entry.GetPositionY()) * fraction;
        float const z = entry.GetPositionZ() + (boss.GetPositionZ() - entry.GetPositionZ()) * fraction;
        chosen.push_back(GroundPoint(anchor->GetMap(), x, y, z, 0.0f));
    }
    return chosen;
}

void SpawnPack(Run& run, Creature* anchor, Position const& spot, std::vector<uint32> const& entries, bool elite)
{
    ArenaInfo const& arena = ArenaOf(run);
    Position const entry = SpotPosition(arena.entry);
    std::size_t const count = entries.size() + (elite ? 1 : 0);
    for (std::size_t index = 0; index < count; ++index)
    {
        bool const isElite = elite && index == 0;
        float const angle = static_cast<float>(index) * 2.0f * float(M_PI) / static_cast<float>(count);
        float const radius = count > 1 ? 2.5f : 0.0f;
        float const x = spot.GetPositionX() + radius * std::cos(angle);
        float const y = spot.GetPositionY() + radius * std::sin(angle);
        Position position = GroundPoint(anchor->GetMap(), x, y, spot.GetPositionZ(), 0.0f);
        position.SetOrientation(position.GetAbsoluteAngle(&entry));

        uint32 const creatureEntry = isElite ? arena.eliteEntry : entries[elite ? index - 1 : index];
        if (TempSummon* creature = SpawnMob(run, anchor, creatureEntry, isElite ? MobRole::Elite : MobRole::Trash,
                position))
            run.creatures.emplace_back(creature->GetGUID(), isElite ? MobRole::Elite : MobRole::Trash);
    }
}

void SetupFloor(Run& run, Map* map, std::vector<Player*> const& present)
{
    ArenaInfo const& arena = ArenaOf(run);
    Position const entry = SpotPosition(arena.entry);
    Position const boss = SpotPosition(arena.boss);

    for (Player* player : present)
        if (Member* member = MemberOf(run, player->GetGUID()))
            member->role = RoleOf(player);
    ComputeFactors(run);

    TempSummon* anchor = map->SummonCreature(NPC_STALKER, boss);
    if (!anchor)
    {
        LOG_ERROR("module", "Infinite Dungeon: could not build floor {} in {}", run.floor, arena.nameEn);
        return;
    }
    anchor->SetPhaseMask(PhaseMask, true);
    run.anchor = anchor->GetGUID();

    if (TempSummon* ring = anchor->SummonCreature(NPC_STALKER, entry, TEMPSUMMON_MANUAL_DESPAWN))
    {
        ring->SetObjectScale(BubbleRadius);
        ring->AddAura(SPELL_RING, ring);
        run.ring = ring->GetGUID();
    }

    // Trash entries of the arena, drawn at random for each spot of a pack
    std::vector<uint32> trashEntries;
    for (uint32 creatureEntry : arena.trashEntries)
        if (creatureEntry)
            trashEntries.push_back(creatureEntry);
    auto draw = [&trashEntries](std::size_t count)
    {
        std::vector<uint32> drawn;
        for (std::size_t index = 0; index < count && !trashEntries.empty(); ++index)
            drawn.push_back(Acore::Containers::SelectRandomContainerElement(trashEntries));
        return drawn;
    };

    // Alone, packs of two; with a partner, of three. The second pack is led by the elite.
    bool const duo = run.members.size() > 1;
    std::vector<Position> const spots = PackSpots(arena, anchor);
    if (!spots.empty())
        SpawnPack(run, anchor, spots[0], draw(duo ? 3 : 2), false);
    if (spots.size() > 1)
        SpawnPack(run, anchor, spots[1], draw(duo ? 2 : 1), true);

    Position bossPosition = boss;
    bossPosition.SetOrientation(boss.GetAbsoluteAngle(&entry));
    if (TempSummon* creature = SpawnMob(run, anchor, arena.bossEntry, MobRole::Boss, bossPosition))
    {
        run.boss = creature->GetGUID();
        run.creatures.emplace_back(creature->GetGUID(), MobRole::Boss);
    }

    if (!anchor->IsWithinLOS(entry.GetPositionX(), entry.GetPositionY(), entry.GetPositionZ() + 2.0f))
        LOG_WARN("module", "Infinite Dungeon: the boss spot of {} does not see its bubble", arena.nameEn);
}

// The floor starts: the ring goes, the players can be reached, and the monsters are sized for who is really there
void DropBubble(Run& run, Map* map, std::vector<Player*> const& present)
{
    if (Creature* ring = map->GetCreature(run.ring))
        ring->DespawnOrUnsummon();
    run.ring.Clear();

    for (Player* player : present)
    {
        SetSheltered(player, false);
        if (Member* member = MemberOf(run, player->GetGUID()))
            member->role = RoleOf(player);
    }

    ComputeFactors(run);
    for (auto const& [guid, role] : run.creatures)
        if (Creature* creature = map->GetCreature(guid); creature && creature->IsAlive() && !creature->IsInCombat())
            ApplyScaling(creature, role, run.level, run.healthFactor, run.damageFactor, run.referenceHealth);

    run.state = FloorState::Fighting;
    run.stateMs = 0;
    SendHudToAll(run, map);
}

// -----------------------------------------------------------------------------------------------------------------
// Hearts, rewards
// -----------------------------------------------------------------------------------------------------------------

void DropHeart(Run& run, Map* map, Position const& where)
{
    Creature* anchor = map->GetCreature(run.anchor);
    if (!anchor)
        return;
    if (GameObject* heart = anchor->SummonGameObject(GO_HEART, where.GetPositionX(), where.GetPositionY(),
            where.GetPositionZ(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, GO_SUMMON_TIMED_DESPAWN))
        run.hearts.push_back(heart->GetGUID());
}

// Creatures that died since the last look: each may leave a heart where it fell
void UpdateFallen(Run& run, Map* map)
{
    for (auto const& [guid, role] : run.creatures)
    {
        if (run.fallen.count(guid))
            continue;
        Creature* creature = map->GetCreature(guid);
        if (creature && creature->IsAlive())
            continue;

        run.fallen.insert(guid);
        uint32 const chance = role == MobRole::Boss ? 100 : role == MobRole::Elite ? EliteHeartChance :
            TrashHeartChance;
        if (creature && roll_chance_i(static_cast<int32>(chance)))
            DropHeart(run, map, creature->GetPosition());
    }
}

void PickUpHearts(Run& run, Map* map, std::vector<Player*> const& present)
{
    for (auto itr = run.hearts.begin(); itr != run.hearts.end();)
    {
        GameObject* heart = map->GetGameObject(*itr);
        if (!heart || !heart->isSpawned())
        {
            itr = run.hearts.erase(itr);
            continue;
        }

        Player* taker = nullptr;
        for (Player* player : present)
            if (player->IsAlive() && player->GetExactDist2d(heart) <= HeartPickupRange &&
                std::fabs(player->GetPositionZ() - heart->GetPositionZ()) < 3.0f)
            {
                taker = player;
                break;
            }
        if (!taker)
        {
            ++itr;
            continue;
        }

        taker->ModifyHealth(static_cast<int32>(CalculatePct(taker->GetMaxHealth(), HeartHealthPct)));
        if (uint32 const maxMana = taker->GetMaxPower(POWER_MANA))
            taker->ModifyPower(POWER_MANA, static_cast<int32>(CalculatePct(maxMana, HeartManaPct)));
        GroundIndicators::Burst(taker, taker->GetPosition(), GroundIndicators::Theme::Holy);
        heart->DespawnOrUnsummon();
        ++run.heartsTaken;
        itr = run.hearts.erase(itr);
    }
}

void MailItem(Player* player, ItemTemplate const* itemTemplate)
{
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    MailDraft draft(Text(player, "Infinite Dungeon", "Donjon infini"),
        Text(player, "Your bags were full: here is what a guardian of the Infinite Dungeon gave you.",
            "Vos sacs étaient pleins : voici ce qu'un gardien du Donjon infini vous a donné."));
    if (Item* item = Item::CreateItem(itemTemplate->ItemId, 1, player))
    {
        item->SaveToDB(transaction);
        draft.AddItem(item);
    }
    draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()),
        MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
    CharacterDatabase.CommitTransaction(transaction);
    Say(player, Text(player, "Your bags are full: the item was sent to your mailbox.",
        "Vos sacs sont pleins : l'objet vous a été envoyé par la poste."));
}

void GiveItem(Player* player, ItemTemplate const* itemTemplate)
{
    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, itemTemplate->ItemId, 1) != EQUIP_ERR_OK)
    {
        MailItem(player, itemTemplate);
        return;
    }
    if (Item* item = player->StoreNewItem(destination, itemTemplate->ItemId, true,
            Item::GenerateItemRandomPropertyId(itemTemplate->ItemId)))
        player->SendNewItem(item, 1, true, false, true);
}

// Every fifth floor's guardian: one piece fitted to the player's class and slots, at its level while levelling, of
// the step's item level at the cap
bool GiveFloorGear(Run const& run, Player* player)
{
    if (IsAtLevelCap(player))
    {
        GiveMythicLootItem(player, run.ladder == Ladder::Gearing ? GetItemLevel(run.floor) : GearingBaseItemLevel);
        return true;
    }

    ItemTemplate const* itemTemplate = SelectLevelLootItem(player, ITEM_QUALITY_RARE);
    if (!itemTemplate)
        itemTemplate = SelectLevelLootItem(player, ITEM_QUALITY_UNCOMMON);
    if (!itemTemplate)
        return false;
    GiveItem(player, itemTemplate);
    return true;
}

ContentLevels ContentFor(uint8 level)
{
    return level >= 70 ? CONTENT_71_80 : level >= 60 ? CONTENT_61_70 : CONTENT_1_60;
}

// The floor's rewards, told to the client (REWARD) and kept for the run's summary
void RewardFloor(Run const& run, Player* player, Member& member)
{
    uint8 const level = player->GetLevel();
    uint32 experience = 0;
    if (!IsAtLevelCap(player))
    {
        experience = FloorExperienceKills * Acore::XP::BaseGain(level, level, ContentFor(level));
        player->GiveXP(experience, nullptr);
    }

    uint32 const gold = FloorGoldPerLevelSquared * level * level;
    player->ModifyMoney(static_cast<int32>(gold));
    ChatHandler(player->GetSession()).PSendSysMessage(
        IsFrench(player) ? "|cffffd24dÉtage {} franchi :|r {}." : "|cffffd24dFloor {} cleared:|r {}.", run.floor,
        Acore::StringFormat("{}g {}s {}c", gold / GOLD, (gold % GOLD) / SILVER, gold % SILVER));

    uint32 essences = 0;
    if (roll_chance_i(static_cast<int32>(FloorEssenceChance)))
        essences = GrantEssenceRewards(player, 1, 1);
    bool const gear = run.floor % GearFloors == 0 && GiveFloorGear(run, player);

    ++member.floorsCleared;
    member.gold += gold;
    member.experience += experience;
    member.essences += essences;
    member.items += gear ? 1 : 0;
    SendAddon(player, Acore::StringFormat("REWARD\t{}\t{}\t{}\t{}\t{}", run.floor, gold, experience, essences,
        gear ? 1 : 0));

    Progress& progress = ProgressOf(player);
    std::size_t const ladder = static_cast<std::size_t>(run.ladder);
    progress.best[ladder] = std::max(progress.best[ladder], run.floor);
    if (run.floor % CheckpointFloors == 0 && run.floor > progress.checkpoint[ladder])
    {
        progress.checkpoint[ladder] = run.floor;
        Say(player, IsFrench(player) ?
            Acore::StringFormat("Point de passage atteint : vous reprendrez à l'étage {}.", run.floor + 1) :
            Acore::StringFormat("Checkpoint reached: you will start again from floor {}.", run.floor + 1));
    }
    SaveProgress(player);
}

// Where the portal opens: off the guardian's body (it fell among the others), a few yards from the boss spot
// towards the way in or to a side of it, on ground the boss spot sees
Position PortalSpot(Run const& run, Map* map, Creature* anchor)
{
    ArenaInfo const& arena = ArenaOf(run);
    Position const boss = SpotPosition(arena.boss);
    Position const entry = SpotPosition(arena.entry);
    Creature const* corpse = map->GetCreature(run.boss);
    Position const body = corpse ? corpse->GetPosition() : boss;
    float const towardEntry = boss.GetAbsoluteAngle(&entry);
    float const reach = std::min(PortalOffset, boss.GetExactDist2d(&entry) * 0.5f);

    std::vector<Position> candidates;
    auto add = [&](float angle, float distance)
    {
        candidates.push_back(GroundPoint(map, boss.GetPositionX() + distance * std::cos(angle),
            boss.GetPositionY() + distance * std::sin(angle), boss.GetPositionZ(), towardEntry));
    };
    add(towardEntry, reach);
    add(towardEntry + float(M_PI) / 2.0f, PortalOffset);
    add(towardEntry - float(M_PI) / 2.0f, PortalOffset);
    add(towardEntry, reach * 1.8f);
    candidates.push_back(boss);

    for (Position const& spot : candidates)
        if (spot.GetExactDist2d(&body) >= PortalCorpseClearance &&
            anchor->IsWithinLOS(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ() + 2.0f))
            return spot;
    return candidates.front();
}

void OnFloorCleared(Run& run, Map* map, std::vector<Player*> const& present)
{
    run.state = FloorState::Cleared;
    run.stateMs = 0;

    if (Creature* anchor = map->GetCreature(run.anchor))
    {
        Position const portal = PortalSpot(run, map, anchor);
        Position const entry = SpotPosition(ArenaOf(run).entry);
        float const facing = portal.GetAbsoluteAngle(&entry);
        anchor->SummonGameObject(GO_PORTAL, portal.GetPositionX(), portal.GetPositionY(), portal.GetPositionZ(),
            facing, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, GO_SUMMON_TIMED_DESPAWN);
        // A pillar of light over it, seen from across the room
        anchor->SummonGameObject(GO_PORTAL_LIGHT, portal.GetPositionX(), portal.GetPositionY(),
            portal.GetPositionZ(), facing, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, GO_SUMMON_TIMED_DESPAWN);
        GroundIndicators::Burst(anchor, portal, GroundIndicators::Theme::Holy);
        if (run.floor % CheckpointFloors == 0)
        {
            // Beside the portal, facing the way in
            float const side = facing + float(M_PI) / 2.0f;
            Position const chest = GroundPoint(map, portal.GetPositionX() + ChestBesidePortal * std::cos(side),
                portal.GetPositionY() + ChestBesidePortal * std::sin(side), portal.GetPositionZ(), facing);
            anchor->SummonGameObject(GO_CHEST, chest.GetPositionX(), chest.GetPositionY(), chest.GetPositionZ(),
                facing, 0.0f, 0.0f, 0.0f, 0.0f, 0, true, GO_SUMMON_TIMED_DESPAWN);
        }
    }

    for (Player* player : present)
    {
        if (Member* member = MemberOf(run, player->GetGUID()))
            RewardFloor(run, player, *member);
        SendAddon(player, Acore::StringFormat("CLEAR\t{}\t{}", run.floor, run.floor % CheckpointFloors == 0 ? 1 : 0));
        SendHud(player, run);
    }
    SendProgress(run, map);
}

// -----------------------------------------------------------------------------------------------------------------
// Runs
// -----------------------------------------------------------------------------------------------------------------

void DestroyRun(Run& run)
{
    for (Member const& member : run.members)
        RunByPlayer.erase(member.guid);
    if (run.instanceId)
    {
        RunByInstance.erase(run.instanceId);
        QueueReset(run.mapId, run.instanceId);
    }
    uint32 const id = run.id;
    Runs.erase(id);
    ActiveRuns = static_cast<uint32>(Runs.size());
}

// Ends the run for everyone in `map` (its floor): home, alive, out of the phase. Members elsewhere clean up on their
// own when they next look (HomeData). Destroys the run.
void EndRun(Run& run, Map* map, EndReason reason)
{
    for (Member const& member : run.members)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
        for (uint32 mapId : run.usedMaps)
            Unbind(member.guid, mapId, player);
        if (!player || !map || player->FindMap() != map)
            continue;

        RestorePlayer(player);
        SaveRunState(player, false, member.home);
        SendSummary(player, run, member);
        SendAddon(player, Acore::StringFormat("END\t{}\t{}", static_cast<uint32>(reason), run.floor));
        if (reason == EndReason::Fallen)
            Say(player, IsFrench(player) ?
                Acore::StringFormat("Le Donjon infini vous rejette à l'étage {}.", run.floor) :
                Acore::StringFormat("The Infinite Dungeon casts you out on floor {}.", run.floor));
        SendToHome(player, member.home);
    }
    DestroyRun(run);
}

// A member leaves the run on its own (it logged out, or went somewhere else); the run goes on for the other
void LeaveRun(Player* player, bool logout)
{
    Run* run = RunOf(player->GetGUID());
    if (!run)
        return;

    WorldLocation home;
    Member left;
    auto const member = std::find_if(run->members.begin(), run->members.end(),
        [player](Member const& m) { return m.guid == player->GetGUID(); });
    if (member != run->members.end())
    {
        home = member->home;
        left = *member;
        run->members.erase(member);
    }
    RunByPlayer.erase(player->GetGUID());
    for (uint32 mapId : run->usedMaps)
        Unbind(player->GetGUID(), mapId, player);

    // Logged out: in_run stays, so the next login sends it home
    if (!logout)
    {
        RestorePlayer(player);
        SaveRunState(player, false, home);
        if (!left.guid.IsEmpty())
            SendSummary(player, *run, left);
        SendAddon(player, Acore::StringFormat("END\t{}\t{}", static_cast<uint32>(EndReason::Left), run->floor));
    }

    if (run->members.empty())
        DestroyRun(*run);
}

std::optional<std::size_t> PickArena(Run const& run)
{
    std::vector<ArenaInfo> const& arenas = GetArenas();
    if (run.forcedArena && *run.forcedArena < arenas.size())
        return run.forcedArena;

    std::vector<std::size_t> candidates;
    for (std::size_t index = 0; index < arenas.size(); ++index)
        if (run.level >= arenas[index].minLevel && run.level <= arenas[index].maxLevel &&
            (!run.previousArena || *run.previousArena != index))
            candidates.push_back(index);
    if (candidates.empty())
        for (std::size_t index = 0; index < arenas.size(); ++index)
            if (run.level >= arenas[index].minLevel && run.level <= arenas[index].maxLevel)
                candidates.push_back(index);
    if (candidates.empty())
        return std::nullopt;
    return Acore::Containers::SelectRandomContainerElement(candidates);
}

// Sends every member to a fresh instance of the next floor's arena. Called from `from`, the map the members are in
// (the keeper's, or the floor's): a member somewhere else (still loading, gone astray) is left out of the run, and its
// own update cleans it up (HomeData), as players are only touched from their own map's thread.
void BeginFloor(Run& run, uint32 floor, Map const* from)
{
    for (auto itr = run.members.begin(); itr != run.members.end();)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(itr->guid);
        if (player && player->FindMap() == from)
        {
            ++itr;
            continue;
        }
        RunByPlayer.erase(itr->guid);
        itr = run.members.erase(itr);
    }

    run.floor = std::max<uint32>(floor, 1);
    run.level = MinPlayerLevel;
    for (Member const& member : run.members)
        if (Player* player = ObjectAccessor::FindConnectedPlayer(member.guid))
            run.level = std::max(run.level, player->GetLevel());

    std::optional<std::size_t> const arena = PickArena(run);
    if (!arena)
        return;
    run.forcedArena.reset();
    run.previousArena = arena;
    run.arena = *arena;

    // Leave the old floor behind: its instance is reset once empty
    if (run.instanceId)
    {
        RunByInstance.erase(run.instanceId);
        QueueReset(run.mapId, run.instanceId);
    }

    ArenaInfo const& target = ArenaOf(run);
    run.mapId = target.mapId;
    run.instanceId = 0;
    run.state = FloorState::Travelling;
    run.stateMs = 0;
    run.anchor.Clear();
    run.ring.Clear();
    run.boss.Clear();
    run.creatures.clear();
    run.fallen.clear();
    run.deadMembers.clear();
    run.hearts.clear();
    run.usedMaps.insert(target.mapId);
    for (Member& member : run.members)
    {
        member.chestClaimed = false;
        member.lastProgress.clear();
    }

    for (Member const& member : run.members)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
        if (!player)
            continue;

        if (!player->IsAlive())
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        Unbind(member.guid, target.mapId, player);
        if (Group* group = player->GetGroup(); group && group->GetLeaderGUID() != member.guid)
            Unbind(group->GetLeaderGUID(), target.mapId, ObjectAccessor::FindConnectedPlayer(group->GetLeaderGUID()));

        EnsurePhase(player);
        player->CombatStop(true);
        player->TeleportTo(target.mapId, target.entry.x, target.entry.y, target.entry.z + 0.5f, target.entry.o, 0,
            nullptr, true);
    }
}

// Every member of a floor must land in the same instance: the first to arrive binds the others to it (a group's
// leader is bound by the core already)
void BindOthers(Run const& run, Map* map, Player* arrived)
{
    InstanceSave* save = sInstanceSaveMgr->GetInstanceSave(map->GetInstanceId());
    if (!save)
        return;
    for (Member const& member : run.members)
    {
        if (member.guid == arrived->GetGUID())
            continue;
        Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
        if (!player)
            continue;
        InstancePlayerBind* bind = sInstanceSaveMgr->PlayerGetBoundInstance(member.guid, save->GetMapId(),
            save->GetDifficulty());
        if (bind && bind->perm)
            continue;
        sInstanceSaveMgr->PlayerBindToInstance(member.guid, save, false, player);
    }
}

void OnArrival(Run& run, Player* player)
{
    EnsurePhase(player);
    if (run.state == FloorState::Travelling || run.state == FloorState::Bubble)
        SetSheltered(player, true);
    if (Member* member = MemberOf(run, player->GetGUID()))
        member->role = RoleOf(player);

    ArenaInfo const& arena = ArenaOf(run);
    SendHud(player, run);
    SendAddon(player, Acore::StringFormat("ARRIVE\t{}\t{}\t{}\t{}\t{}", run.floor, ArenaName(player, arena),
        static_cast<uint32>(run.ladder), GetStep(run.floor), GetRecommendedParagon(run.ladder, run.floor)));
    if (Member* member = MemberOf(run, player->GetGUID()))
        member->lastProgress.clear();
}

// Why a player cannot start a run, or empty
std::string StartBlock(Player* player)
{
    if (player->GetSession()->IsBot())
        return "bot";
    if (player->GetLevel() < MinPlayerLevel)
        return Text(player, "The Infinite Dungeon opens at level 15.", "Le Donjon infini s'ouvre au niveau 15.");
    if (!player->IsAlive() || player->IsInCombat() || player->IsBeingTeleported())
        return Text(player, "Not now: you must be alive and out of combat.",
            "Pas maintenant : il faut être en vie et hors combat.");
    if (Map* map = player->GetMap(); map && map->Instanceable())
        return Text(player, "Leave your instance first.", "Quittez d'abord votre instance.");
    if (RunOf(player->GetGUID()))
        return Text(player, "Already in a run.", "Déjà dans une descente.");
    if (Group* group = player->GetGroup())
        if (group->isLFGGroup() || group->IsRaidFinder() || group->GetMythicLevel() >= 0 || group->isBGGroup())
            return Text(player, "Leave your Dungeon Finder, Raid Finder or Mythic+ group first.",
                "Quittez d'abord votre groupe de donjon, de raid ou de Mythique+.");
    return {};
}

// The group partner a duo run can be started with: a real player of the group standing near
Player* FindPartner(Player* player)
{
    Group* group = player->GetGroup();
    if (!group)
        return nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && member != player && !member->GetSession()->IsBot() &&
            member->IsInMap(player) && member->IsWithinDistInMap(player, PartnerRange))
            return member;
    return nullptr;
}

Ladder LadderFor(std::vector<Player*> const& players)
{
    for (Player* player : players)
        if (!IsAtLevelCap(player))
            return Ladder::Levelling;
    return Ladder::Gearing;
}

// The floor a run starts on: the lowest checkpoint of its players (nobody is pulled deeper than their own progress)
uint32 StartFloor(std::vector<Player*> const& players, Ladder ladder)
{
    uint32 checkpoint = std::numeric_limits<uint32>::max();
    for (Player* player : players)
        checkpoint = std::min(checkpoint, ProgressOf(player).checkpoint[static_cast<std::size_t>(ladder)]);
    return (checkpoint == std::numeric_limits<uint32>::max() ? 0 : checkpoint) + 1;
}

bool StartRun(std::vector<Player*> const& players, std::optional<uint32> floor,
    std::optional<std::size_t> arena = std::nullopt)
{
    for (Player* player : players)
    {
        std::string const block = StartBlock(player);
        if (!block.empty())
        {
            for (Player* other : players)
                Say(other, block == "bot" ? Text(other, "Bots cannot enter the Infinite Dungeon.",
                    "Les compagnons ne peuvent pas entrer dans le Donjon infini.") : block);
            return false;
        }
    }

    auto run = std::make_unique<Run>();
    run->id = ++NextRunId;
    run->ladder = LadderFor(players);
    uint32 const startFloor = floor ? *floor : StartFloor(players, run->ladder);
    run->startFloor = std::max<uint32>(startFloor, 1);

    for (Player* player : players)
    {
        Member member;
        member.guid = player->GetGUID();
        member.home.WorldRelocate(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
            player->GetPositionZ(), player->GetOrientation());
        member.role = RoleOf(player);
        run->members.push_back(member);

        // Normal difficulty: a heroic lockout of the arena's dungeon would send the player into its own instance
        if (player->GetDungeonDifficulty() != DUNGEON_DIFFICULTY_NORMAL)
        {
            player->SetDungeonDifficulty(DUNGEON_DIFFICULTY_NORMAL);
            player->SendDungeonDifficulty(player->GetGroup() != nullptr);
        }
        if (Group* group = player->GetGroup(); group && group->GetDungeonDifficulty() != DUNGEON_DIFFICULTY_NORMAL)
            group->SetDungeonDifficulty(DUNGEON_DIFFICULTY_NORMAL);

        player->CustomData.GetDefault<Pass>(PassKey);
        HomeData* home = player->CustomData.GetDefault<HomeData>(HomeKey);
        home->home = member.home;
        SaveRunState(player, true, member.home);
        RunByPlayer[player->GetGUID()] = run->id;
    }

    run->forcedArena = arena;
    Run& started = *run;
    Runs[run->id] = std::move(run);
    ActiveRuns = static_cast<uint32>(Runs.size());
    BeginFloor(started, startFloor, players.front()->FindMap());
    return true;
}

// Runs whose players never reached their floor (a disconnect, a failed teleport): given up after a while
void AbandonLostRuns(uint32 diff)
{
    std::vector<uint32> lost;
    for (auto& [id, run] : Runs)
    {
        if (run->state != FloorState::Travelling || run->instanceId)
            continue;
        run->stateMs += diff;
        if (run->stateMs >= TravelTimeoutMs)
            lost.push_back(id);
    }
    for (uint32 id : lost)
        if (auto const itr = Runs.find(id); itr != Runs.end())
        {
            LOG_INFO("module", "Infinite Dungeon: run {} never reached floor {}, given up", id, itr->second->floor);
            DestroyRun(*itr->second);
        }
}

void ProcessPendingResets(uint32 diff)
{
    for (auto itr = PendingResets.begin(); itr != PendingResets.end();)
    {
        itr->ageMs += diff;
        Map* map = sMapMgr->FindMap(itr->mapId, itr->instanceId);
        if (!map || !map->IsDungeon())
        {
            itr = PendingResets.erase(itr);
            continue;
        }
        if (!map->HavePlayers())
        {
            map->ToInstanceMap()->Reset(INSTANCE_RESET_ALL);
            itr = PendingResets.erase(itr);
            continue;
        }
        if (itr->ageMs >= PendingResetMaxMs)
            itr = PendingResets.erase(itr);
        else
            ++itr;
    }
}

// Counts each fall of a member once, for the tracker
void CountDeaths(Run& run, std::vector<Player*> const& present)
{
    for (Player* player : present)
    {
        if (player->IsAlive())
            run.deadMembers.erase(player->GetGUID());
        else if (run.deadMembers.insert(player->GetGUID()).second)
            ++run.deaths;
    }
}

// The floor's life, from its map's update. Returns false once the run is gone.
bool UpdateRun(Run& run, Map* map, uint32 diff)
{
    run.stateMs += diff;
    std::vector<Player*> const present = PresentMembers(run, map);
    for (Player* player : present)
        EnsurePhase(player);

    switch (run.state)
    {
        case FloorState::Travelling:
            if (!present.empty())
            {
                SetupFloor(run, map, present);
                run.state = FloorState::Bubble;
                run.stateMs = 0;
                SendHudToAll(run, map);
                SendProgress(run, map);
            }
            return true;
        case FloorState::Bubble:
        {
            Position const entry = SpotPosition(ArenaOf(run).entry);
            bool left = false;
            for (Player* player : present)
            {
                SetSheltered(player, true);
                if (player->IsAlive() && player->GetExactDist2d(&entry) > BubbleRadius)
                    left = true;
            }
            if (left)
                DropBubble(run, map, present);
            SendProgress(run, map);
            return true;
        }
        case FloorState::Fighting:
        case FloorState::Cleared:
        {
            UpdateFallen(run, map);
            PickUpHearts(run, map, present);
            if (run.state == FloorState::Fighting)
            {
                Creature* boss = map->GetCreature(run.boss);
                if (!run.boss.IsEmpty() && (!boss || !boss->IsAlive()))
                    OnFloorCleared(run, map, present);
            }

            CountDeaths(run, present);
            // When the last living player dies, the run ends
            if (!present.empty() && std::none_of(present.begin(), present.end(),
                    [](Player* player) { return player->IsAlive(); }))
            {
                run.state = FloorState::Failed;
                run.stateMs = 0;
                SendHudToAll(run, map);
            }
            SendProgress(run, map);
            return true;
        }
        case FloorState::Failed:
            if (run.stateMs >= FailDelayMs)
            {
                EndRun(run, map, EndReason::Fallen);
                return false;
            }
            return true;
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------
// The keeper
// -----------------------------------------------------------------------------------------------------------------

std::string FloorLabel(Player const* player, Ladder ladder, uint32 floor)
{
    if (ladder == Ladder::Gearing)
    {
        uint32 const paragon = GetRecommendedParagon(ladder, floor);
        if (paragon)
            return IsFrench(player) ?
                Acore::StringFormat("étage {} (palier {}, parangon conseillé {})", floor, GetStep(floor) + 1,
                    paragon) :
                Acore::StringFormat("floor {} (step {}, recommended paragon {})", floor, GetStep(floor) + 1, paragon);
        return IsFrench(player) ? Acore::StringFormat("étage {} (palier {})", floor, GetStep(floor) + 1) :
            Acore::StringFormat("floor {} (step {})", floor, GetStep(floor) + 1);
    }
    return IsFrench(player) ? Acore::StringFormat("étage {}", floor) : Acore::StringFormat("floor {}", floor);
}

void ShowRecords(Player* player)
{
    Progress const& progress = ProgressOf(player);
    bool const french = IsFrench(player);
    Say(player, french ? "Donjon infini - vos records :" : "Infinite Dungeon - your records:");
    Say(player, french ?
        Acore::StringFormat("  Échelle de progression : étage le plus profond {}, point de passage {}.",
            progress.best[0], progress.checkpoint[0]) :
        Acore::StringFormat("  Levelling ladder: deepest floor {}, checkpoint {}.", progress.best[0],
            progress.checkpoint[0]));
    Say(player, french ?
        Acore::StringFormat("  Échelle d'équipement (niveau 80) : étage le plus profond {}, point de passage {}.",
            progress.best[1], progress.checkpoint[1]) :
        Acore::StringFormat("  Gearing ladder (level 80): deepest floor {}, checkpoint {}.", progress.best[1],
            progress.checkpoint[1]));
    if (IsAtLevelCap(player))
    {
        uint32 const next = progress.checkpoint[1] + 1;
        Say(player, french ?
            Acore::StringFormat("  Prochaine descente : {}, objets de niveau {}.", FloorLabel(player, Ladder::Gearing,
                next), GetItemLevel(next)) :
            Acore::StringFormat("  Next descent: {}, item level {}.", FloorLabel(player, Ladder::Gearing, next),
                GetItemLevel(next)));
    }
}

// The deepest floor at the level cap, per class
void ShowLeaderboard(Player* player)
{
    bool const french = IsFrench(player);
    Say(player, french ? "Donjon infini - le plus profond au niveau 80, par classe :" :
        "Infinite Dungeon - deepest at level 80, by class:");
    QueryResult result = CharacterDatabase.Query(
        "SELECT d.class, c.name, d.best_max FROM character_infinite_dungeon d JOIN characters c ON c.guid = d.guid "
        "WHERE d.best_max > 0 ORDER BY d.best_max DESC LIMIT 500");
    if (!result)
    {
        Say(player, french ? "  Personne encore." : "  Nobody yet.");
        return;
    }

    std::set<uint8> shown;
    do
    {
        Field* fields = result->Fetch();
        uint8 const classId = fields[0].Get<uint8>();
        if (!shown.insert(classId).second)
            continue;
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(classId);
        std::string const className = classEntry ?
            classEntry->name[player->GetSession()->GetSessionDbcLocale()] : std::to_string(classId);
        Say(player, Acore::StringFormat("  {} : {} ({} {})", className, fields[1].Get<std::string>(),
            french ? "étage" : "floor", fields[2].Get<uint32>()));
    } while (result->NextRow());
}

class npc_infinite_dungeon_keeper : public CreatureScript
{
public:
    npc_infinite_dungeon_keeper() : CreatureScript("npc_infinite_dungeon_keeper") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        if (player->GetLevel() >= MinPlayerLevel)
        {
            std::lock_guard<std::recursive_mutex> guard(Lock);
            bool const french = IsFrench(player);
            std::vector<Player*> const solo = { player };
            Ladder const ladder = LadderFor(solo);
            uint32 const start = StartFloor(solo, ladder);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, french ?
                Acore::StringFormat("Descendre seul : {}.", FloorLabel(player, ladder, start)) :
                Acore::StringFormat("Go down alone: {}.", FloorLabel(player, ladder, start)), 0, ACTION_CONTINUE);
            if (start > 1)
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, french ? "Recommencer seul depuis l'étage 1." :
                    "Start over alone from floor 1.", 0, ACTION_RESTART);

            if (Player* partner = FindPartner(player); partner && partner->GetLevel() >= MinPlayerLevel)
            {
                std::vector<Player*> const duo = { player, partner };
                Ladder const duoLadder = LadderFor(duo);
                uint32 const duoStart = StartFloor(duo, duoLadder);
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, french ?
                    Acore::StringFormat("Descendre avec {} : {}.", partner->GetName(),
                        FloorLabel(player, duoLadder, duoStart)) :
                    Acore::StringFormat("Go down with {}: {}.", partner->GetName(),
                        FloorLabel(player, duoLadder, duoStart)), 0, ACTION_CONTINUE_DUO);
                if (duoStart > 1)
                    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, french ?
                        Acore::StringFormat("Recommencer avec {} depuis l'étage 1.", partner->GetName()) :
                        Acore::StringFormat("Start over with {} from floor 1.", partner->GetName()), 0,
                        ACTION_RESTART_DUO);
            }
        }
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, "My records.", "Mes records."), 0, ACTION_RECORDS);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            Text(player, "The deepest, by class.", "Les plus profonds, par classe."), 0, ACTION_LEADERBOARD);
        SendGossipMenuFor(player, NPC_TEXT_KEEPER, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        std::lock_guard<std::recursive_mutex> guard(Lock);
        switch (action)
        {
            case ACTION_CONTINUE:
                StartRun({ player }, std::nullopt);
                break;
            case ACTION_RESTART:
                StartRun({ player }, 1u);
                break;
            case ACTION_CONTINUE_DUO:
            case ACTION_RESTART_DUO:
                if (Player* partner = FindPartner(player))
                    StartRun({ player, partner }, action == ACTION_RESTART_DUO ? std::optional<uint32>(1u) :
                        std::nullopt);
                else
                    Say(player, Text(player, "Your partner must stand with you.",
                        "Votre partenaire doit se tenir près de vous."));
                break;
            case ACTION_RECORDS:
                ShowRecords(player);
                break;
            case ACTION_LEADERBOARD:
                ShowLeaderboard(player);
                break;
            default:
                break;
        }
        return true;
    }
};

// -----------------------------------------------------------------------------------------------------------------
// The portal and the chest
// -----------------------------------------------------------------------------------------------------------------

class go_infinite_dungeon_portal : public GameObjectScript
{
public:
    go_infinite_dungeon_portal() : GameObjectScript("go_infinite_dungeon_portal") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Run* run = RunOf(player->GetGUID());
        if (!run || run->state != FloorState::Cleared)
            return true;

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TAXI, IsFrench(player) ?
            Acore::StringFormat("Descendre à l'{}.", FloorLabel(player, run->ladder, run->floor + 1)) :
            Acore::StringFormat("Go down to {}.", FloorLabel(player, run->ladder, run->floor + 1)), 0,
            ACTION_DESCEND);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text(player, "Leave the Infinite Dungeon (your checkpoint stays).",
            "Quitter le Donjon infini (votre point de passage reste)."), 0, ACTION_LEAVE);
        SendGossipMenuFor(player, NPC_TEXT_PORTAL, go->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* go, uint32 /*sender*/, uint32 action) override
    {
        CloseGossipMenuFor(player);
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Run* run = RunOf(player->GetGUID());
        if (!run || run->state != FloorState::Cleared)
            return true;

        if (action == ACTION_DESCEND)
            BeginFloor(*run, run->floor + 1, go->GetMap());
        else if (action == ACTION_LEAVE)
            EndRun(*run, go->GetMap(), EndReason::Left);
        return true;
    }
};

// Every tenth floor: essences, and paragon points at the level cap. Each member opens it once.
class go_infinite_dungeon_chest : public GameObjectScript
{
public:
    go_infinite_dungeon_chest() : GameObjectScript("go_infinite_dungeon_chest") { }

    bool OnGossipHello(Player* player, GameObject* /*go*/) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Run* run = RunOf(player->GetGUID());
        Member* member = run ? MemberOf(*run, player->GetGUID()) : nullptr;
        if (!member || run->state != FloorState::Cleared)
            return true;
        if (member->chestClaimed)
        {
            Say(player, Text(player, "You already opened this chest.", "Vous avez déjà ouvert ce coffre."));
            return true;
        }

        member->chestClaimed = true;
        uint32 const bonus = run->floor / 50;
        uint32 const essences = GrantEssenceRewards(player, CheckpointEssences + bonus, 1 + run->floor / 30);
        uint32 paragon = 0;
        if (IsAtLevelCap(player))
        {
            paragon = CheckpointParagonPoints + bonus;
            AwardParagonPoints(player, paragon, IsFrench(player) ? "Donjon infini" : "Infinite Dungeon");
        }
        member->essences += essences;
        member->paragon += paragon;
        SendAddon(player, Acore::StringFormat("CHEST\t{}\t{}", essences, paragon));
        return true;
    }
};

// -----------------------------------------------------------------------------------------------------------------
// The floor creatures
// -----------------------------------------------------------------------------------------------------------------

enum class Shape : uint8
{
    AroundSelf,
    UnderTarget,
    ConeAtVictim,
    LineAtVictim
};

struct Ability
{
    Shape shape;
    float size;         // radius, cone radius or line length
    float width;        // line width, cone arc in degrees
    uint32 warnMs;
    uint32 cooldownMs;
    uint32 firstMs;
    float percent;      // of the reference health, to each player hit, before the run's damage factor
};

// Trash: a caster drops a circle under someone, a fighter swings a cone. An elite slams around itself (and a caster
// one also drops circles). A boss has all three shapes: a nova around itself, circles under the players, and a line
// through its victim that the tank must step out of.
constexpr Ability CasterTrash = { Shape::UnderTarget, 5.0f, 0.0f, 2500, 15000, 6000, 20.0f };
constexpr Ability FighterTrash = { Shape::ConeAtVictim, 8.0f, 60.0f, 2000, 16000, 8000, 18.0f };
constexpr Ability EliteSlam = { Shape::AroundSelf, 8.0f, 0.0f, 2500, 16000, 7000, 30.0f };
constexpr Ability EliteBolt = { Shape::UnderTarget, 6.0f, 0.0f, 2500, 13000, 11000, 25.0f };
constexpr Ability BossNova = { Shape::AroundSelf, 10.0f, 0.0f, 3000, 22000, 12000, 40.0f };
constexpr Ability BossRain = { Shape::UnderTarget, 6.0f, 0.0f, 2500, 13000, 6000, 25.0f };
constexpr Ability BossLine = { Shape::LineAtVictim, 25.0f, 5.0f, 2500, 17000, 9000, 35.0f };

// The Mythic+ boss rules: no hard crowd control on a boss
constexpr std::array<Mechanics, 13> BossImmunities = {
    MECHANIC_CHARM, MECHANIC_DISORIENTED, MECHANIC_FEAR, MECHANIC_SLEEP, MECHANIC_STUN, MECHANIC_FREEZE,
    MECHANIC_KNOCKOUT, MECHANIC_POLYMORPH, MECHANIC_BANISH, MECHANIC_SHACKLE, MECHANIC_TURN, MECHANIC_HORROR,
    MECHANIC_SAPPED
};
constexpr std::array<AuraType, 4> BossAuraImmunities = {
    SPELL_AURA_MOD_STUN, SPELL_AURA_MOD_FEAR, SPELL_AURA_MOD_CONFUSE, SPELL_AURA_MOD_CHARM
};

bool IsCaster(CreatureTemplate const* cinfo)
{
    return cinfo->unit_class == CLASS_MAGE || cinfo->unit_class == CLASS_PALADIN;
}

GroundIndicators::Theme ThemeFor(CreatureTemplate const* cinfo)
{
    if (cinfo->dmgschool > SPELL_SCHOOL_NORMAL && cinfo->dmgschool < MAX_SPELL_SCHOOL)
        return GroundIndicators::ThemeOf(1u << cinfo->dmgschool);
    if (IsCaster(cinfo))
        return cinfo->type == CREATURE_TYPE_UNDEAD ? GroundIndicators::Theme::Shadow : GroundIndicators::Theme::Arcane;
    return GroundIndicators::Theme::None;
}

uint32 LogSpellFor(GroundIndicators::Theme theme)
{
    switch (theme)
    {
        case GroundIndicators::Theme::Shadow: return SPELL_LOG_SHADOW;
        case GroundIndicators::Theme::Fire: return SPELL_LOG_FIRE;
        case GroundIndicators::Theme::Frost: return SPELL_LOG_FROST;
        case GroundIndicators::Theme::Nature: return SPELL_LOG_NATURE;
        case GroundIndicators::Theme::Arcane: return SPELL_LOG_ARCANE;
        case GroundIndicators::Theme::Holy: return SPELL_LOG_HOLY;
        default: return SPELL_LOG_PHYSICAL;
    }
}

struct npc_infinite_dungeon_creature : public ScriptedAI
{
    explicit npc_infinite_dungeon_creature(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();

        FloorCreature const* data = me->CustomData.Get<FloorCreature>(FloorCreatureKey);
        MobRole const role = data ? data->role : MobRole::Trash;
        CreatureTemplate const* cinfo = me->GetCreatureTemplate();
        _theme = ThemeFor(cinfo);
        _spellId = LogSpellFor(_theme);
        if (!sSpellMgr->GetSpellInfo(_spellId))
            _spellId = SPELL_LOG_PHYSICAL;

        _kit.clear();
        switch (role)
        {
            case MobRole::Trash:
                _kit.push_back(IsCaster(cinfo) ? CasterTrash : FighterTrash);
                break;
            case MobRole::Elite:
                _kit.push_back(EliteSlam);
                if (IsCaster(cinfo))
                    _kit.push_back(EliteBolt);
                break;
            case MobRole::Boss:
                _kit = { BossNova, BossRain, BossLine };
                for (Mechanics mechanic : BossImmunities)
                    me->ApplySpellImmune(0, IMMUNITY_MECHANIC, mechanic, true);
                for (AuraType aura : BossAuraImmunities)
                    me->ApplySpellImmune(0, IMMUNITY_STATE, aura, true);
                break;
        }
    }

    void Reset() override
    {
        Release();
        _readyAt.clear();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        uint64 const now = NowMs();
        _readyAt.clear();
        for (Ability const& ability : _kit)
            _readyAt.push_back(now + ability.firstMs + urand(0, ability.firstMs / 3));
    }

    void JustDied(Unit* /*killer*/) override
    {
        Release();
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;

        uint64 const now = NowMs();
        if (_pending)
        {
            if (now >= _resolveAt)
                Resolve();
        }
        else if (!me->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_STUNNED | UNIT_STATE_CONFUSED |
                     UNIT_STATE_FLEEING) && _readyAt.size() == _kit.size())
        {
            for (std::size_t index = 0; index < _kit.size(); ++index)
            {
                if (now < _readyAt[index])
                    continue;
                Ability const& ability = _kit[index];
                _readyAt[index] = now + ability.cooldownMs + urand(0, ability.cooldownMs / 5);
                if (Begin(index))
                    break;
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    std::vector<Player*> PlayersNear(float range) const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->IsAlive() && !player->IsGameMaster() &&
                me->IsWithinDistInMap(player, range) && me->IsValidAttackTarget(player))
                players.push_back(player);
        return players;
    }

    bool Begin(std::size_t index)
    {
        Ability const& ability = _kit[index];
        Unit* victim = me->GetVictim();
        bool hold = true;
        switch (ability.shape)
        {
            case Shape::AroundSelf:
                _area = GroundIndicators::ShowCircle(me, me->GetPosition(), ability.size, ability.warnMs, _theme);
                break;
            case Shape::UnderTarget:
            {
                std::vector<Player*> const players = PlayersNear(TargetRange);
                if (players.empty())
                    return false;
                Player* target = Acore::Containers::SelectRandomContainerElement(players);
                _area = GroundIndicators::ShowCircle(me, target->GetPosition(), ability.size, ability.warnMs,
                    _theme);
                hold = false;
                break;
            }
            case Shape::ConeAtVictim:
            case Shape::LineAtVictim:
            {
                if (!victim)
                    return false;
                float const facing = me->GetAngle(victim);
                me->SetFacingTo(facing);
                if (ability.shape == Shape::ConeAtVictim)
                    _area = GroundIndicators::ShowCone(me, me->GetPosition(), facing, ability.size, ability.width,
                        ability.warnMs, _theme);
                else
                    _area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing, ability.size,
                        ability.width, ability.warnMs, _theme);
                break;
            }
        }

        _pending = true;
        _pendingAbility = index;
        _resolveAt = NowMs() + ability.warnMs;
        if (hold)
        {
            me->SetControlled(true, UNIT_STATE_ROOT);
            _rooted = true;
        }
        return true;
    }

    void Resolve()
    {
        Ability const& ability = _kit[_pendingAbility];
        Release();

        FloorCreature const* data = me->CustomData.Get<FloorCreature>(FloorCreatureKey);
        float const unit = data ? data->abilityUnit : 0.0f;
        GroundIndicators::Burst(me, _area.origin, _theme);
        uint32 const amount = static_cast<uint32>(std::max(1.0f, ability.percent * unit));
        for (Player* player : PlayersNear(AbilityReach))
            if (_area.Contains(player->GetPosition()))
                MythicTuning::DealAbilityDamage(me, player, _spellId, amount);
    }

    void Release()
    {
        if (_rooted)
        {
            me->SetControlled(false, UNIT_STATE_ROOT);
            _rooted = false;
        }
        _pending = false;
    }

    std::vector<Ability> _kit;
    std::vector<uint64> _readyAt;
    GroundIndicators::Theme _theme = GroundIndicators::Theme::None;
    uint32 _spellId = SPELL_LOG_PHYSICAL;
    bool _pending = false;
    bool _rooted = false;
    std::size_t _pendingAbility = 0;
    uint64 _resolveAt = 0;
    GroundIndicators::Area _area;
};

// -----------------------------------------------------------------------------------------------------------------
// Hooks
// -----------------------------------------------------------------------------------------------------------------

// A player that carries the run's marks with no run behind them (the run ended while it was elsewhere, or it logged
// in after one): back to normal, and home when it is still in a dungeon
void CleanupStray(Player* player)
{
    HomeData const* data = player->CustomData.Get<HomeData>(HomeKey);
    WorldLocation const home = data ? data->home : WorldLocation();
    bool const hadMarks = data || HasPass(player->CustomData) || (player->GetPhaseMask() & PhaseMask);
    if (!hadMarks)
        return;

    RestorePlayer(player);
    SaveRunState(player, false, home);
    SendAddon(player, "END\t0\t0");
    if (Map* map = player->GetMap(); map && map->IsDungeon())
        SendToHome(player, home);
}

class InfiniteDungeonPlayerScript : public PlayerScript
{
public:
    InfiniteDungeonPlayerScript() : PlayerScript("InfiniteDungeonPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_MAP_CHANGED,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_CAN_REPOP_AT_GRAVEYARD
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (player->GetSession()->IsBot())
            return;
        LoadProgress(player);

        // Logged out during a run: back where the run started
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT return_map, return_x, return_y, return_z, return_o FROM character_infinite_dungeon "
                "WHERE guid = {} AND in_run = 1", player->GetGUID().GetCounter()))
        {
            Field* fields = result->Fetch();
            WorldLocation const home(fields[0].Get<uint16>(), fields[1].Get<float>(), fields[2].Get<float>(),
                fields[3].Get<float>(), fields[4].Get<float>());
            RestorePlayer(player);
            SaveRunState(player, false, home);
            SendToHome(player, home);
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        LeaveRun(player, true);
        ProgressByGuid.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerMapChanged(Player* player) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Run* run = RunOf(player->GetGUID());
        if (!run)
        {
            CleanupStray(player);
            return;
        }

        Map* map = player->GetMap();
        if (map && map->IsDungeon() && map->GetId() == run->mapId)
        {
            if (!run->instanceId && run->state == FloorState::Travelling)
            {
                run->instanceId = map->GetInstanceId();
                RunByInstance[run->instanceId] = run->id;
                BindOthers(*run, map, player);
            }
            if (map->GetInstanceId() == run->instanceId)
            {
                OnArrival(*run, player);
                return;
            }
        }

        // On the way to the next floor, still in the old one: nothing to do
        if (run->state == FloorState::Travelling && player->IsBeingTeleportedFar())
            return;

        // Anywhere else (a hearthstone, a summon): the player left the run
        LeaveRun(player, false);
    }

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        HomeData* data = player->CustomData.Get<HomeData>(HomeKey);
        if (!data && !(player->GetPhaseMask() & PhaseMask))
            return;
        uint32 const now = static_cast<uint32>(NowMs());
        if (data && now < data->nextCheckMs)
            return;
        if (data)
            data->nextCheckMs = now + StrayCheckMs;

        std::lock_guard<std::recursive_mutex> guard(Lock);
        if (!RunOf(player->GetGUID()) && !player->IsBeingTeleported())
            CleanupStray(player);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
        std::string& message) override
    {
        if (language != LANG_ADDON || !message.starts_with(Prefix))
            return;
        std::string_view body(message);
        body.remove_prefix(Prefix.size());
        if (body.empty() || body.front() != '\t')
            return;
        body.remove_prefix(1);
        if (body != "STATE")
            return;

        std::lock_guard<std::recursive_mutex> guard(Lock);
        if (Run* run = RunOf(player->GetGUID()); run && run->instanceId && player->FindMap() &&
            player->FindMap()->GetInstanceId() == run->instanceId)
        {
            SendHud(player, *run);
            if (Member* member = MemberOf(*run, player->GetGUID()))
                member->lastProgress.clear();
            SendProgress(*run, player->FindMap());
        }
        else
            SendAddon(player, "END\t0\t0");
    }

    // A ghost of a run waits where it fell: it comes back on the next floor, or the run ends
    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        return RunOf(player->GetGUID()) == nullptr;
    }
};

class InfiniteDungeonMapScript : public AllMapScript
{
public:
    InfiniteDungeonMapScript() : AllMapScript("InfiniteDungeonMapScript", { ALLMAPHOOK_ON_MAP_UPDATE }) { }

    void OnMapUpdate(Map* map, uint32 diff) override
    {
        if (!ActiveRuns.load() || !map->IsDungeon())
            return;

        std::lock_guard<std::recursive_mutex> guard(Lock);
        auto const itr = RunByInstance.find(map->GetInstanceId());
        if (itr == RunByInstance.end())
            return;
        auto const run = Runs.find(itr->second);
        if (run == Runs.end() || run->second->mapId != map->GetId() || run->second->instanceId != map->GetInstanceId())
            return;
        UpdateRun(*run->second, map, diff);
    }
};

class InfiniteDungeonWorldScript : public WorldScript
{
public:
    InfiniteDungeonWorldScript() : WorldScript("InfiniteDungeonWorldScript", { WORLDHOOK_ON_UPDATE }) { }

    void OnUpdate(uint32 diff) override
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        if (!Runs.empty())
            AbandonLostRuns(diff);
        if (!PendingResets.empty())
            ProcessPendingResets(diff);
    }
};

// -----------------------------------------------------------------------------------------------------------------
// Commands, to test: .infinite start [floor], floor <n>, arena <index>, clear, leave, checkpoint <n>, info
// -----------------------------------------------------------------------------------------------------------------

using namespace Acore::ChatCommands;

class InfiniteDungeonCommandScript : public CommandScript
{
public:
    InfiniteDungeonCommandScript() : CommandScript("InfiniteDungeonCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable infiniteTable =
        {
            { "start",      HandleStart,      SEC_GAMEMASTER, Console::No },
            { "floor",      HandleFloor,      SEC_GAMEMASTER, Console::No },
            { "arena",      HandleArena,      SEC_GAMEMASTER, Console::No },
            { "clear",      HandleClear,      SEC_GAMEMASTER, Console::No },
            { "leave",      HandleLeave,      SEC_GAMEMASTER, Console::No },
            { "checkpoint", HandleCheckpoint, SEC_GAMEMASTER, Console::No },
            { "info",       HandleInfo,       SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "infinite", infiniteTable },
        };
        return commandTable;
    }

    // Starts a solo run at the checkpoint, or at the floor given
    static bool HandleStart(ChatHandler* handler, Optional<uint32> floor)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        std::optional<uint32> start;
        if (floor)
            start = std::max<uint32>(*floor, 1);
        return StartRun({ handler->GetPlayer() }, start);
    }

    // In a run: goes to that floor now (a fresh instance). Out of one: starts a solo run there.
    static bool HandleFloor(ChatHandler* handler, uint32 floor)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        if (Run* run = RunOf(player->GetGUID()))
        {
            BeginFloor(*run, std::max<uint32>(floor, 1), player->FindMap());
            return true;
        }
        return StartRun({ player }, std::max<uint32>(floor, 1));
    }

    // The current floor again, in arena <index> (0 to the number of arenas - 1): to look at every arena
    static bool HandleArena(ChatHandler* handler, uint32 index)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        if (index >= GetArenas().size())
        {
            handler->PSendSysMessage("Arena index 0 to {}.", GetArenas().size() - 1);
            return false;
        }
        if (Run* run = RunOf(player->GetGUID()))
        {
            run->forcedArena = index;
            BeginFloor(*run, run->floor, player->FindMap());
        }
        else if (!StartRun({ player }, 1u, index))
            return false;
        handler->PSendSysMessage("Arena {}: {} (map {}).", index, GetArenas()[index].nameEn, GetArenas()[index].mapId);
        return true;
    }

    // Kills every creature of the floor
    static bool HandleClear(ChatHandler* handler)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        Run* run = RunOf(player->GetGUID());
        if (!run || !player->FindMap() || player->FindMap()->GetInstanceId() != run->instanceId)
            return false;
        for (auto const& [guid, role] : run->creatures)
            if (Creature* creature = player->GetMap()->GetCreature(guid); creature && creature->IsAlive())
                Unit::Kill(player, creature);
        return true;
    }

    static bool HandleLeave(ChatHandler* handler)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        Run* run = RunOf(player->GetGUID());
        if (!run)
            return false;
        EndRun(*run, player->FindMap(), EndReason::Left);
        return true;
    }

    // Sets the checkpoint of the ladder the character climbs now
    static bool HandleCheckpoint(ChatHandler* handler, uint32 checkpoint)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        Progress& progress = ProgressOf(player);
        std::size_t const ladder = static_cast<std::size_t>(IsAtLevelCap(player) ? Ladder::Gearing : Ladder::Levelling);
        progress.checkpoint[ladder] = checkpoint / CheckpointFloors * CheckpointFloors;
        progress.best[ladder] = std::max(progress.best[ladder], progress.checkpoint[ladder]);
        SaveProgress(player);
        handler->PSendSysMessage("Checkpoint {} on the {} ladder.", progress.checkpoint[ladder],
            ladder ? "gearing" : "levelling");
        return true;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        std::lock_guard<std::recursive_mutex> guard(Lock);
        Player* player = handler->GetPlayer();
        Run const* run = RunOf(player->GetGUID());
        if (!run)
        {
            handler->SendSysMessage("Not in a run.");
            ShowRecords(player);
            return true;
        }
        ArenaInfo const& arena = ArenaOf(*run);
        handler->PSendSysMessage("Run {}: floor {} ({} ladder, step {}, paragon {}), level {}, arena {} ({}) map {} "
            "instance {}, state {}, {} creatures ({} down), health x{:.2f}, damage x{:.2f}, reference {:.0f}.",
            run->id, run->floor, run->ladder == Ladder::Gearing ? "gearing" : "levelling", GetStep(run->floor),
            GetRecommendedParagon(run->ladder, run->floor), run->level, run->arena, arena.nameEn, run->mapId,
            run->instanceId, static_cast<uint32>(run->state), run->creatures.size(), run->fallen.size(),
            run->healthFactor, run->damageFactor, run->referenceHealth);
        return true;
    }
};
}

namespace InfiniteDungeon
{
bool IsInRun(Player const* player)
{
    if (!player || !ActiveRuns.load())
        return false;
    std::lock_guard<std::recursive_mutex> guard(Lock);
    return RunByPlayer.count(player->GetGUID()) != 0;
}

bool IsFloorCreature(Creature const* creature)
{
    return creature && creature->GetEntry() >= FirstFloorCreature && creature->GetEntry() <= LastFloorCreature;
}

bool IsFloorMap(Map const* map)
{
    if (!map || !ActiveRuns.load() || !map->IsDungeon())
        return false;
    std::lock_guard<std::recursive_mutex> guard(Lock);
    return RunByInstance.count(map->GetInstanceId()) != 0;
}
}

void AddInfiniteDungeonScripts()
{
    new InfiniteDungeonCreatureLevelScript();
    new npc_infinite_dungeon_keeper();
    new go_infinite_dungeon_portal();
    new go_infinite_dungeon_chest();
    RegisterCreatureAI(npc_infinite_dungeon_creature);
    new InfiniteDungeonPlayerScript();
    new InfiniteDungeonMapScript();
    new InfiniteDungeonWorldScript();
    new InfiniteDungeonCommandScript();
}
