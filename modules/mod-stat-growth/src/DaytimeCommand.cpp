#include "Chat.h"
#include "GameTime.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <ctime>

namespace
{
using namespace Acore::ChatCommands;

// .daytime <hour> [minute]: the game master's own client set to that time of day, for looking at the world by day or
// by night (the client's post-processing, its lights and darker nights). Only that client's clock moves - the server
// and the other players keep theirs - and until the next login, which sends the real time again. .daytime alone puts
// the real time back now.
class DaytimeCommandScript : public CommandScript
{
public:
    DaytimeCommandScript() : CommandScript("DaytimeCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "daytime", HandleDaytime, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    static bool HandleDaytime(ChatHandler* handler, Optional<uint32> hour, Optional<uint32> minute)
    {
        Player* player = handler->GetPlayer();
        bool const french = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
        if ((hour && *hour > 23) || (minute && *minute > 59))
        {
            handler->SendSysMessage(french ? "Heure de 0 à 23, minutes de 0 à 59." :
                "Hour 0 to 23, minutes 0 to 59.");
            return false;
        }

        // Today at that time, in the server's local time as the login packet sends it (or now, without an hour)
        time_t when = GameTime::GetGameTime().count();
        if (hour)
        {
            std::tm local = Acore::Time::TimeBreakdown(when);
            local.tm_hour = int(*hour);
            local.tm_min = int(minute.value_or(0));
            local.tm_sec = 0;
            when = std::mktime(&local);
        }

        WorldPacket data(SMSG_LOGIN_SETTIMESPEED, 4 + 4 + 4);
        data.AppendPackedTime(when);
        data << float(0.01666667f);     // the game's speed, as at login
        data << uint32(0);
        player->SendDirectMessage(&data);

        if (hour)
            handler->PSendSysMessage(french ?
                "Votre client est réglé sur {:02}h{:02} (jusqu'à la prochaine connexion)." :
                "Your client is set to {:02}:{:02} (until your next login).", *hour, minute.value_or(0));
        else
            handler->SendSysMessage(french ? "Votre client est revenu à l'heure réelle." :
                "Your client is back on the real time.");
        return true;
    }
};
}

void AddDaytimeCommand()
{
    new DaytimeCommandScript();
}
