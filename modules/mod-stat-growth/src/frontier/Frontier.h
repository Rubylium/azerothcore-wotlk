#ifndef MOD_STAT_GROWTH_FRONTIER_H
#define MOD_STAT_GROWTH_FRONTIER_H

#include "Define.h"

#include <string>

class Player;

// Le Front du Nord (Frontier.cpp: the tiers, the phase, the elites, rifts, chests and Colosses;
// FrontierQuartermaster.cpp: the Dalaran quartermaster, its daily contracts and its shop)
namespace Frontier
{
    constexpr uint32 ITEM_FROST_SHARD = 37711;
    constexpr uint8 TierCount = 4;

    // What a contract asks for, and what counts towards it
    enum class Deed : uint8
    {
        Elite = 1,      // a roaming elite slain, in a zone
        Rift = 2,       // a rift closed, in a zone
        Chest = 3,      // a chest opened, in a zone
        Colossus = 4    // a Colosse slain, anywhere
    };

    // A tier's gear (1-4)
    uint32 LootItemLevel(uint8 tier);
    // A zone's tier, 0 for none
    uint8 ZoneTier(uint32 zoneId);
    // The tier zones, in tier order (two a tier)
    uint32 TierZone(uint8 tier, uint8 which);

    bool IsBot(Player const* player);
    bool IsFrench(Player const* player);
    // Whether a character sees the tier content (the tier phase)
    bool SeesTier(Player const* player);
    // To the player's FrontierUI.lua / FrontierQuartermaster.lua, on the "Frontier" addon prefix
    void SendAddon(Player* player, std::string const& body);

    // A deed done by a player (bots aside): their contracts that ask for it move on
    void CountDeed(Player* player, Deed deed, uint32 zoneId);
    // The player's contracts of the day, for a game master (.frontier contracts)
    std::string DescribeContracts(Player* player);
}

void AddFrontierScripts();
void AddFrontierQuartermasterScripts();

#endif
