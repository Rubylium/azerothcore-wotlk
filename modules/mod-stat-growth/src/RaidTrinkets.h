#ifndef MOD_STAT_GROWTH_RAID_TRINKETS_H
#define MOD_STAT_GROWTH_RAID_TRINKETS_H

#include "Define.h"

class Player;
struct ItemTemplate;

// The Défi board's last two raids' own trinkets: eight of the Hollow Voice (item level 477, MythicDungeonSystem.cpp
// GivePinnacleLootItem) and eight of Gardien-chef Vorhan (485, mod-legendary SetPieces.cpp GiveWardenVorhanLootItem),
// a passive and an active for each role - a fighter, a caster, a healer, a tank. Plain items with their own spells
// (item_template, localTools/raidTrinkets/Spells.ps1), sized for their item level as the raid's generated gear is.
namespace RaidTrinkets
{
    enum class Raid : uint8
    {
        HollowVoice,
        WardenVorhan,
    };

    // A piece of the raid's loot is one of its trinkets ChancePct of the time: one made for the player's role (a tank's,
    // a fighter's, a caster's or a healer's; a tank with both of theirs gets a fighter's) that they have not got yet,
    // else none
    ItemTemplate const* Roll(Player* player, Raid raid);
}

#endif
