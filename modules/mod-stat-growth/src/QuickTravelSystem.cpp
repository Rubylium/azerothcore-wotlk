#include "QuickTravelSystem.h"

#include "Chat.h"
#include "DBCStores.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

#include <string_view>
#include <unordered_map>

namespace
{
constexpr uint32 QuickTravelSpell = 90019;
constexpr std::string_view TravelPrefix = "PersonalLoot\tTRAVEL\t";

std::unordered_map<ObjectGuid::LowType, uint32> pendingDestinations;

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

        uint32 const nodeId = destination->second;
        pendingDestinations.erase(destination);
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(nodeId);
        if (!node || !CanQuickTravel(player, true, false) || !IsNetworkTaxiNode(nodeId) ||
            !IsAllowedForPlayer(player, node))
            return;

        std::string const destinationName = node->name[0] ? node->name[0] : "flight master";
        if (player->TeleportTo(node->map_id, node->x, node->y, node->z + 1.0f, player->GetOrientation()))
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff66ccffQuick Travel: {}.|r", destinationName);
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

    pendingDestinations[player->GetGUID().GetCounter()] = destination->ID;
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
