#include "EvolutionsAudio.h"

#include "Chat.h"
#include "Object.h"
#include "Player.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
// "EVA\t<command>\t...": an addon whisper to the player, the client splitting the prefix at the tab
void Send(Player* player, std::string const& message)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, "EVA\t" + message);
    player->SendDirectMessage(&packet);
}
}

namespace EvolutionsAudio
{
void Play(Player* player, std::string_view key)
{
    Send(player, Acore::StringFormat("P\t{}", key));
}

void PlayOn(Player* player, std::string_view key, WorldObject const* source)
{
    if (source)
        Send(player, Acore::StringFormat("P\t{}\tG{:016X}", key, source->GetGUID().GetRawValue()));
}

void PlayAt(Player* player, std::string_view key, Position const& where)
{
    Send(player, Acore::StringFormat("P\t{}\tX{:.2f},{:.2f},{:.2f}", key, where.GetPositionX(), where.GetPositionY(),
                                     where.GetPositionZ()));
}

void StopOn(Player* player, WorldObject const* source)
{
    if (source)
        Send(player, Acore::StringFormat("S\t{:016X}", source->GetGUID().GetRawValue()));
}
}
