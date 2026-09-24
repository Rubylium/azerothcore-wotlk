#ifndef MOD_STAT_GROWTH_PRESTIGE_SYSTEM_H
#define MOD_STAT_GROWTH_PRESTIGE_SYSTEM_H

#include "Define.h"

class Player;

// Resets a max-level character to level 1 and raises their paragon cap. Essences and the paragon allocation
// stay. The frame (FrameXML/Prestige.lua) is the confirmation; this only acts on PRESTIGE.

void SendPrestigeWindow(Player* player);
void HandlePrestigeAddonMessage(Player* player, uint32 language, std::string const& message);

void AddPrestigeScripts();

#endif
