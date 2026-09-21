#ifndef MOD_STAT_GROWTH_TALENT_RESET_SYSTEM_H
#define MOD_STAT_GROWTH_TALENT_RESET_SYSTEM_H

#include "Define.h"

#include <string>

class Player;

// The talent window's reset button (clientPatcher FrameXML/TalentFrameHD.lua) sends "Talents\tRESET" as an addon
// whisper to the player itself: the active spec's talents are reset for free, outside combat. Some classes (the
// Pestiféré) have no trainer to do it.
void HandleTalentResetAddonMessage(Player* player, uint32 language, std::string const& message);

#endif
