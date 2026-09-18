// Dungeon Finder roles for custom classes: the server side of it.
//
// The client decides which roles a class may queue as from a table compiled into Wow.exe. The patcher writes
// the custom classes into that table (clientPatcher/template/Patch-WowExe.ps1, generated from
// localTools/customClasses/classes.json), so a patched client offers, stores and sends their roles itself.
//
// What the server still owns is the rule: a client is not trusted to only claim roles its class has. This
// keeps the roles of a custom class within `custom_class`.`Roles`, for a join and for a role check alike.

#include "DatabaseEnv.h"
#include "LFG.h"
#include "Log.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

#include <unordered_map>

namespace
{
constexpr uint8 ClassRoleMask = lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE;

// Roles each custom class may take, from `custom_class`.`Roles`
std::unordered_map<uint8 /*classId*/, uint8 /*roles*/> allowedRoles;

void LoadAllowedRoles()
{
    allowedRoles.clear();

    QueryResult result = WorldDatabase.Query("SELECT ClassId, Roles FROM custom_class");
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        allowedRoles[fields[0].Get<uint8>()] = fields[1].Get<uint8>() & ClassRoleMask;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded Dungeon Finder roles for {} custom classes", allowedRoles.size());
}

class CustomClassRolesWorldScript final : public WorldScript
{
public:
    CustomClassRolesWorldScript() : WorldScript("CustomClassRolesWorldScript", {
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnStartup() override
    {
        LoadAllowedRoles();
    }
};

class CustomClassRolesPlayerScript final : public PlayerScript
{
public:
    CustomClassRolesPlayerScript() : PlayerScript("CustomClassRolesPlayerScript", {
        PLAYERHOOK_ON_LFG_ROLES
    }) { }

    // The leader flag is not a class matter, so it passes through untouched
    void OnPlayerLfgRoles(Player* player, uint8& roles) override
    {
        if (!player)
            return;

        auto const itr = allowedRoles.find(player->getClass());
        if (itr != allowedRoles.end())
            roles &= lfg::PLAYER_ROLE_LEADER | itr->second;
    }
};
}

void AddCustomClassRolesScripts()
{
    new CustomClassRolesWorldScript();
    new CustomClassRolesPlayerScript();
}
