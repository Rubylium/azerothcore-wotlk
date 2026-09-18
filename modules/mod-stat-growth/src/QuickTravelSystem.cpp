#include "QuickTravelSystem.h"

#include "Chat.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

#include <charconv>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
constexpr uint32 QuickTravelSpell = 90019;
constexpr std::string_view TravelPrefix = "PersonalLoot\tTRAVEL\t";
constexpr std::string_view InstancePrefix = "PersonalLoot\tINSTANCE\t";

struct PendingDestination
{
    WorldLocation location;
    std::string name;
};

std::unordered_map<ObjectGuid::LowType, PendingDestination> pendingDestinations;

bool IsNetworkTaxiNode(uint32 nodeId)
{
    uint8 const field = uint8((nodeId - 1) / 32);
    uint32 const mask = 1u << ((nodeId - 1) % 32);
    return field < TaxiMaskSize && (sTaxiNodesMask[field] & mask) != 0;
}

bool IsAllowedForPlayer(Player const* player, TaxiNodesEntry const* node)
{
    uint8 const teamIndex = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 0;
    return node->MountCreatureID[teamIndex] != 0 || node->MountCreatureID[0] == 32981;
}

TaxiNodesEntry const* FindDestination(Player const* player, std::string_view name)
{
    for (uint32 nodeId = 1; nodeId < sTaxiNodesStore.GetNumRows(); ++nodeId)
    {
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(nodeId);
        MapEntry const* map = node ? sMapStore.LookupEntry(node->map_id) : nullptr;
        if (!node || !map || map->Instanceable() || !node->name[0] || name != node->name[0] ||
            !IsNetworkTaxiNode(nodeId) || !IsAllowedForPlayer(player, node))
            continue;

        return node;
    }
    return nullptr;
}

bool CanQuickTravel(Player* player, bool reportError, bool checkCasting = true)
{
    char const* error = nullptr;
    if (!player->IsAlive())
        error = "You must be alive to use Quick Travel.";
    else if (player->IsInCombat())
        error = "You cannot use Quick Travel while in combat.";
    else if (player->IsInFlight() || player->IsBeingTeleported())
        error = "You are already travelling.";
    else if (player->GetMap()->Instanceable())
        error = "Quick Travel cannot be used inside an instance or battleground.";
    else if (checkCasting && player->IsNonMeleeSpellCast(false))
        error = "You are already casting another spell.";

    if (error && reportError)
        ChatHandler(player->GetSession()).SendSysMessage(error);
    return error == nullptr;
}

class QuickTravelSpellScript : public SpellScript
{
    PrepareSpellScript(QuickTravelSpellScript);

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        auto const destination = pendingDestinations.find(player->GetGUID().GetCounter());
        if (destination == pendingDestinations.end())
            return;

        PendingDestination const pending = destination->second;
        pendingDestinations.erase(destination);
        if (!CanQuickTravel(player, true, false))
            return;

        if (player->TeleportTo(pending.location))
            ChatHandler(player->GetSession()).PSendSysMessage("|cff66ccffQuick Travel: {}.|r", pending.name);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(QuickTravelSpellScript::HandleAfterCast);
    }
};
}

void HandleQuickTravelAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON || !message.starts_with(TravelPrefix))
        return;

    std::string_view const destinationName(message.data() + TravelPrefix.size(), message.size() - TravelPrefix.size());
    if (destinationName.empty() || destinationName.size() > 120 || !CanQuickTravel(player, true))
        return;

    TaxiNodesEntry const* destination = FindDestination(player, destinationName);
    if (!destination)
    {
        ChatHandler(player->GetSession()).SendSysMessage(
            "That flight master is not a valid destination for your faction.");
        return;
    }

    pendingDestinations[player->GetGUID().GetCounter()] = {
        WorldLocation(destination->map_id, destination->x, destination->y, destination->z + 1.0f,
            player->GetOrientation()),
        destination->name[0] ? destination->name[0] : "flight master" };
    player->CastSpell(player, QuickTravelSpell, false);
}

// Travel to a dungeon or raid entrance: the outside area its exit portal leads back to, which is where the
// meeting stone stands. The client sends the LFGDungeons id of the map pin that was clicked.
void HandleInstanceTravelAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON || !message.starts_with(InstancePrefix))
        return;

    std::string_view const argument(message.data() + InstancePrefix.size(), message.size() - InstancePrefix.size());
    uint32 dungeonId = 0;
    auto const parsed = std::from_chars(argument.data(), argument.data() + argument.size(), dungeonId);
    if (parsed.ec != std::errc() || !dungeonId || !CanQuickTravel(player, true))
        return;

    LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(dungeonId);
    AreaTriggerTeleport const* entrance = dungeon ? sObjectMgr->GetGoBackTrigger(dungeon->MapID) : nullptr;
    if (!entrance)
    {
        ChatHandler(player->GetSession()).SendSysMessage("That destination has no known entrance.");
        return;
    }

    pendingDestinations[player->GetGUID().GetCounter()] = {
        WorldLocation(entrance->target_mapId, entrance->target_X, entrance->target_Y, entrance->target_Z,
            entrance->target_Orientation),
        dungeon->Name[0] ? dungeon->Name[0] : "instance entrance" };
    player->CastSpell(player, QuickTravelSpell, false);
}

void ClearQuickTravel(Player* player)
{
    if (player)
        pendingDestinations.erase(player->GetGUID().GetCounter());
}

void AddQuickTravelScripts()
{
    RegisterSpellScript(QuickTravelSpellScript);
}
