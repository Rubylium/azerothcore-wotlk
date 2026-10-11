#ifndef MOD_STAT_GROWTH_RAID_TRINKETS_H
#define MOD_STAT_GROWTH_RAID_TRINKETS_H

#include "Define.h"

#include <optional>
#include <vector>

class Player;
struct ItemTemplate;

// The Défi board's raids' own trinkets: eight of the Hollow Voice (item level 477, MythicDungeonSystem.cpp
// GivePinnacleLootItem) and eight of Gardien-chef Vorhan (485, mod-legendary SetPieces.cpp GiveWardenVorhanLootItem),
// a passive and an active for each role - a fighter, a caster, a healer, a tank; two of the Traqueur d'évadés (485,
// GiveEscapeHunterLootItem), a passive and an active for every damage dealer. Plain items with their own spells
// (item_template, localTools/raidTrinkets/Spells.ps1), sized for their item level as the raid's generated gear is.
namespace RaidTrinkets
{
    enum class Raid : uint8
    {
        HollowVoice,
        WardenVorhan,
        EscapeHunter,
    };

    // A piece of the raid's loot is one of its trinkets ChancePct of the time: one made for the player's role (a tank's,
    // a fighter's, a caster's or a healer's, or a damage dealer's; a tank with all the raid's tank trinkets gets a
    // fighter's) that they have not got yet, else none
    ItemTemplate const* Roll(Player* player, Raid raid);
    // A raid's trinkets that suit the player (the loot collection), and the raid a trinket is of
    std::vector<uint32> Suited(Player* player, Raid raid);
    std::optional<Raid> RaidOf(uint32 entry);
}

#endif
