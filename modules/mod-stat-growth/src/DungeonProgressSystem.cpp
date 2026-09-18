#include "DungeonProgressSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
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
