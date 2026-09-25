#ifndef MOD_STAT_GROWTH_PRESTIGE_SHOP_H
#define MOD_STAT_GROWTH_PRESTIGE_SHOP_H

#include "Define.h"
#include <string_view>

class Player;

// Éclats de prestige and the heirloom shop of the prestige keeper (PrestigeShop.cpp). The messages share the
// "Prestige" addon prefix of PrestigeSystem.cpp, which hands the shop's to HandlePrestigeShopMessage.

// The account's éclats and the shop, for the keeper's window
void SendPrestigeShards(Player* player, uint32 gained = 0);
void SendPrestigeShop(Player* player);
// BUY <item>: true when the message was the shop's
bool HandlePrestigeShopMessage(Player* player, std::string_view body);

void AddPrestigeShopScripts();

#endif
