#include "TalentResetSystem.h"

#include "Chat.h"
#include "Player.h"
#include "SharedDefines.h"

#include <string_view>

namespace
{
constexpr std::string_view ResetMessage = "Talents\tRESET";
}

void HandleTalentResetAddonMessage(Player* player, uint32 language, std::string const& message)
{
    if (!player || language != LANG_ADDON || message != ResetMessage)
        return;

    ChatHandler chat(player->GetSession());
    if (player->IsInCombat())
    {
        chat.SendSysMessage("Vous ne pouvez pas réinitialiser vos talents en combat.");
        return;
    }

    if (!player->resetTalents(true))
    {
        chat.SendSysMessage("Vous n'avez aucun talent à réinitialiser.");
        return;
    }

    player->SendTalentsInfoData(false);
    chat.SendSysMessage("Vos talents ont été réinitialisés.");
}
