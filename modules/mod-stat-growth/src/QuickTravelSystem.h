#ifndef MOD_STAT_GROWTH_QUICK_TRAVEL_SYSTEM_H
#define MOD_STAT_GROWTH_QUICK_TRAVEL_SYSTEM_H

#include "Define.h"

#include <string>

class Player;

void AddQuickTravelScripts();
void ClearQuickTravel(Player* player);
void HandleQuickTravelAddonMessage(Player* player, uint32 language, std::string const& message);

#endif
