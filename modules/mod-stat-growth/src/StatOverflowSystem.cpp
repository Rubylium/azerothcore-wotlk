#include "Chat.h"
#include "DataMap.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StatOverflow.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <string>
#include <string_view>

// Stat overflow (core src/server/game/Combat/StatOverflow.h, .agents/docs/systems/stat-overflow.md): the core grows
// critical hits and precision on every hit; this takes Robustesse off every hit a player takes, and sends each player
// its summary for the character sheet (FrameXML's DragonUI sidebar override, "Overflow" addon messages), when it
// changes and when the sheet asks ("Q").
namespace
{
constexpr std::string_view AddonPrefix = "Overflow";
// How often a player's summary is checked for a change: stats move with gear, buffs and stances
constexpr uint32 CheckIntervalMs = 1000;

struct OverflowFeedState : public DataMap::Base
{
    std::string sent;
    uint32 sinceCheck = 0;
};

void SendAddon(Player* player, std::string const& body)
{
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(AddonPrefix) + "\t" + body);
    player->GetSession()->SendPacket(&packet);
}

void SendSummary(Player* player, bool force)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;

    StatOverflow::Summary const summary = StatOverflow::BuildSummary(player);
    std::string const critPrecision = StatOverflow::FormatCritPrecision(summary);
    std::string const robustness = StatOverflow::FormatRobustness(summary);

    OverflowFeedState* state = player->CustomData.GetDefault<OverflowFeedState>("StatOverflowFeed");
    std::string combined = critPrecision + "\n" + robustness;
    if (!force && combined == state->sent)
        return;
    state->sent = std::move(combined);

    SendAddon(player, critPrecision);
    SendAddon(player, robustness);
}
}

class StatOverflowPlayerScript : public PlayerScript
{
public:
    StatOverflowPlayerScript() : PlayerScript("StatOverflowPlayerScript", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player->IsInWorld() || !player->GetSession() || player->GetSession()->IsBot())
            return;
        OverflowFeedState* state = player->CustomData.GetDefault<OverflowFeedState>("StatOverflowFeed");
        state->sinceCheck += diff;
        if (state->sinceCheck < CheckIntervalMs)
            return;
        state->sinceCheck = 0;
        SendSummary(player, false);
    }

    // The sheet asks once it loads (a /reload forgets what was sent)
    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
        std::string& message) override
    {
        if (language != LANG_ADDON || message != std::string(AddonPrefix) + "\tQ")
            return;
        SendSummary(player, true);
    }
};

class StatOverflowUnitScript : public UnitScript
{
public:
    StatOverflowUnitScript() : UnitScript("StatOverflowUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    // Registered before StatGrowthUnitScript: the paragon board's reduction, leech and procs see the hit after it
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || !attacker || attacker == victim || !damage)
            return;
        damage = StatOverflow::ApplyRobustness(player, damage);
    }
};

void AddStatOverflowScripts()
{
    new StatOverflowPlayerScript();
    new StatOverflowUnitScript();
}
