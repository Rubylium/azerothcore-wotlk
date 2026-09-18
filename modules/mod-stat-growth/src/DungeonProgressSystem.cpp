#include "DungeonProgressSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "InstanceScript.h"
#include "LFGMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
constexpr std::string_view AddonPrefix = "DungeonProgress";

// Encounters defeated per instance, for maps whose script does not track them itself (most classic dungeons).
// Merged with the instance script's own mask when the state is sent.
std::unordered_map<uint32 /*instanceId*/, uint32 /*encounter bit mask*/> trackedProgress;

void SendProgressMessage(Player* player, std::string const& payload)
{
    WorldPacket packet;
    std::string const message = std::string(AddonPrefix) + '\t' + payload;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
    player->GetSession()->SendPacket(&packet);
}

DungeonEncounterList const* GetEncounters(Map const* map)
{
    if (!map || !map->IsDungeon())
        return nullptr;

    return sObjectMgr->GetDungeonEncounterList(map->GetId(), map->GetDifficulty());
}

uint32 GetCompletedMask(Map* map)
{
    auto const tracked = trackedProgress.find(map->GetInstanceId());
    uint32 mask = tracked == trackedProgress.end() ? 0 : tracked->second;
    InstanceMap const* instance = map->ToInstanceMap();
    if (InstanceScript const* script = instance ? instance->GetInstanceScript() : nullptr)
        mask |= script->GetCompletedEncounterMask();
    return mask;
}

std::string BuildStatePayload(Map* map, DungeonEncounterList const& encounters)
{
    uint32 const mask = GetCompletedMask(map);
    std::vector<DungeonEncounter const*> ordered(encounters.begin(), encounters.end());
    // DungeonEncounter.dbc order index is not loaded by the core; the encounter bit follows the same order
    std::sort(ordered.begin(), ordered.end(), [](DungeonEncounter const* left, DungeonEncounter const* right)
    {
        return left->dbcEntry->encounterIndex < right->dbcEntry->encounterIndex;
    });

    std::string payload = "STATE";
    for (DungeonEncounter const* encounter : ordered)
    {
        payload += '\t';
        payload += std::to_string(encounter->dbcEntry->id);
        payload += ':';
        payload += (mask & (1u << encounter->dbcEntry->encounterIndex)) ? '1' : '0';
    }
    return payload;
}
}

// Sent when the player enters a map and on login: the boss list of the instance they are in, with the ones
// already defeated marked, or NONE outside an instance so the client restores the normal quest tracker.
void SendDungeonProgress(Player* player)
{
    if (!player || !player->GetSession())
        return;

    Map* map = player->GetMap();
    DungeonEncounterList const* encounters = GetEncounters(map);
    if (!encounters || encounters->empty())
    {
        SendProgressMessage(player, "NONE");
        return;
    }

    SendProgressMessage(player, BuildStatePayload(map, *encounters));
}

void OnDungeonProgressUnitDeath(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    Map* map = creature ? creature->GetMap() : nullptr;
    DungeonEncounterList const* encounters = GetEncounters(map);
    if (!encounters)
        return;

    bool defeated = false;
    for (DungeonEncounter const* encounter : *encounters)
    {
        if (encounter->creditType == ENCOUNTER_CREDIT_KILL_CREATURE && encounter->creditEntry == creature->GetEntry())
        {
            uint32& mask = trackedProgress[map->GetInstanceId()];
            uint32 const bit = 1u << encounter->dbcEntry->encounterIndex;
            defeated = defeated || (mask & bit) == 0;
            mask |= bit;
        }
    }

    if (!defeated)
        return;

    std::string const payload = BuildStatePayload(map, *encounters);
    map->DoForAllPlayers([&payload](Player* player)
    {
        if (player && player->GetSession())
            SendProgressMessage(player, payload);
    });
}

// -----------------------------------------------------------------------------------------------------------------
// Dungeon Finder gear locks
// -----------------------------------------------------------------------------------------------------------------

namespace
{
// The client only learns that a dungeon is locked and a reason code: a gear lock reads "get better gear" in a
// tooltip, with no number, and a random dungeon is never locked itself - it only fails when joined, once the server
// finds every dungeon in it locked. This tells the client's Dungeon Finder (DungeonFinderLocks.lua) both up front:
//   G <tab> <average item level> <tab> <dungeon id>:<required average>,...
//       the player's average as the Dungeon Finder computes it, and the requirement of every dungeon locked for
//       gear; "g" continues the list when it does not fit one message
//   R <tab> <random dungeon id> <tab> <lock reason>:<value>,...
//       a random dungeon with no dungeon left to join, and why its dungeons are locked (the value is the lowest
//       average item level required for a gear lock, 0 otherwise)
constexpr std::string_view LockPrefix = "DungeonFinderLocks";
constexpr std::size_t LockMessageMaxLength = 220;

struct DungeonLock
{
    uint32 reason;
    uint32 requiredItemLevel;
};

// Locks found while the Dungeon Finder evaluates a player, sent once it is done
std::unordered_map<ObjectGuid, std::unordered_map<uint32 /*dungeon id*/, DungeonLock>> pendingLocks;

void SendLockMessage(Player* player, std::string const& payload)
{
    WorldPacket packet;
    std::string const message = std::string(LockPrefix) + '	' + payload;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message);
    player->GetSession()->SendPacket(&packet);
}

void SendGearLocks(Player* player, std::unordered_map<uint32, DungeonLock> const& locks)
{
    // Always sent, even empty: it also clears what an earlier evaluation reported
    std::string const average = std::to_string(uint32(player->GetAverageItemLevelForDF()));
    std::string payload = "G	" + average + '	';
    std::size_t listed = 0;
    for (auto const& [dungeonId, lock] : locks)
    {
        if (lock.reason != lfg::LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE)
            continue;

        std::string const entry = std::to_string(dungeonId) + ':' + std::to_string(lock.requiredItemLevel);
        if (listed && payload.size() + entry.size() + 1 > LockMessageMaxLength)
        {
            SendLockMessage(player, payload);
            payload = "g	" + average + '	';
            listed = 0;
        }

        payload += (listed ? "," : "") + entry;
        ++listed;
    }
    SendLockMessage(player, payload);
}

// A random dungeon the player may pick but cannot join: every dungeon it draws from is locked
void SendBlockedRandomDungeons(Player* player, std::unordered_map<uint32, DungeonLock> const& locks)
{
    for (uint32 entry : sLFGMgr->GetRandomAndSeasonalDungeons(player->GetLevel(), player->GetSession()->Expansion()))
    {
        // The set holds packed entries (dungeon id | type << 24): the lookups and the client use the plain id
        uint32 const randomId = entry & 0x00FFFFFF;
        lfg::LfgDungeonSet const& dungeons = sLFGMgr->GetDungeonsByRandom(randomId);
        if (dungeons.empty())
            continue;

        // Lock reason -> lowest average item level required (gear locks only)
        std::map<uint32, uint32> reasons;
        bool joinable = false;
        for (uint32 dungeonId : dungeons)
        {
            auto const lock = locks.find(dungeonId);
            if (lock == locks.end())
            {
                joinable = true;
                break;
            }

            auto const [reason, inserted] = reasons.emplace(lock->second.reason, lock->second.requiredItemLevel);
            if (!inserted && lock->second.requiredItemLevel < reason->second)
                reason->second = lock->second.requiredItemLevel;
        }

        if (joinable)
            continue;

        std::string payload = "R	" + std::to_string(randomId) + '	';
        bool first = true;
        for (auto const& [reason, requiredItemLevel] : reasons)
        {
            payload += (first ? "" : ",") + std::to_string(reason) + ':' + std::to_string(requiredItemLevel);
            first = false;
        }
        SendLockMessage(player, payload);
    }
}

class DungeonFinderLockGlobalScript : public GlobalScript
{
public:
    DungeonFinderLockGlobalScript() : GlobalScript("DungeonFinderLockGlobalScript", {
        GLOBALHOOK_ON_INITIALIZE_LOCKED_DUNGEONS,
        GLOBALHOOK_ON_AFTER_INITIALIZE_LOCKED_DUNGEONS
    }) { }

    void OnInitializeLockedDungeons(Player* player, uint8& /*level*/, uint32& lockData,
        lfg::LFGDungeonData const* dungeon) override
    {
        if (!player || !dungeon || !lockData)
            return;

        DungeonLock lock{ lockData, 0 };
        if (lockData == lfg::LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE)
        {
            // The same requirement LFGMgr::InitializeLockedDungeons just compared the player's average against
            DungeonProgressionRequirements const* requirements =
                sObjectMgr->GetAccessRequirement(dungeon->map, Difficulty(dungeon->difficulty));
            lock.requiredItemLevel = requirements ? requirements->reqItemLevel : 0;
        }

        pendingLocks[player->GetGUID()][dungeon->id] = lock;
    }

    void OnAfterInitializeLockedDungeons(Player* player) override
    {
        if (!player || !player->GetSession())
            return;

        std::unordered_map<uint32, DungeonLock> locks;
        if (auto node = pendingLocks.extract(player->GetGUID()))
            locks = std::move(node.mapped());

        SendGearLocks(player, locks);
        SendBlockedRandomDungeons(player, locks);
    }
};
}

void AddDungeonFinderLockScripts()
{
    new DungeonFinderLockGlobalScript();
}
