#ifndef MOD_STAT_GROWTH_LOOT_FIT_H
#define MOD_STAT_GROWTH_LOOT_FIT_H

#include "Define.h"
#include "ObjectGuid.h"

#include <string>
#include <string_view>

class Player;
struct ItemTemplate;

// What gear fits a player, for every loot path (smart loot, Mythic+ rewards, the ground loot's shares, the Défi
// satchel, legendaries, set pieces, personal loot bonuses): judged on the player's role, never its class alone, and on
// everything the item gives - its listed stats and its spells (a caster trinket lists only hit, crit or haste rating:
// its spell power is in its equip or proc spell).
namespace LootFit
{
    enum class Role : uint8
    {
        Tank,
        Strength,       // a strength fighter: warriors, paladins, death knights (and the classes built on them)
        Agility,        // an agility fighter: rogues, hunters, feral druids, enhancement shamans
        Caster,
        Healer,
    };

    // The talent tree's specialization first (mod-custom-classes GetTalentSpecRole: tank, healer, caster or fighter),
    // else the stock spec checks; a tank's role picked in the group too (a bear druid shares its tree with the cat).
    // A fighter is a strength or an agility one as the class it is built on.
    Role RoleOf(Player* player);
    char const* RoleName(Role role);

    // Whether an item's stats and spells suit the role: a fighter never gets spell power or intellect, a caster or a
    // healer never strength, agility or attack power, a damage dealer never a tank's defences, a tank never a caster's
    // stats. Items giving only shared ratings (hit, crit, haste) or nothing recognised suit everyone.
    bool Fits(Player* player, ItemTemplate const& item);
    // How good the item is for the role: its stats and spells weighted as the role values them (a proc's or a use
    // effect's half as much as a stat always there)
    int32 Score(Player* player, ItemTemplate const& item);
    // The primary stat a role's generated gear takes: ITEM_MOD_STRENGTH, ITEM_MOD_AGILITY or ITEM_MOD_INTELLECT
    uint32 PrimaryStat(Player* player);

    // The same for a role given (agilityTank: a tank built on an agility class, a bear), without a player
    bool FitsRole(Role role, bool agilityTank, ItemTemplate const& item);
    int32 ScoreRole(Role role, bool agilityTank, ItemTemplate const& item);
    bool RoleByName(std::string_view name, Role& role);
    // What an item was read as giving (its stats and spells), for .lootfit
    std::string Describe(ItemTemplate const& item);

    // One drop's role: a tank's loot is a damage dealer's now and then (loot.tank_offspec_pct), its offspec - the
    // fighter it would be (RoleOf) - so a tank gets both. Drawn once for one pick, so that every candidate of it is
    // judged alike: while one lives, RoleOf answers the drawn role for its player. The outermost draws; one made
    // inside it keeps that draw. A real player's only: a bot tank keeps its tank's gear.
    class DrawnRole
    {
    public:
        explicit DrawnRole(Player* player);
        ~DrawnRole();
        DrawnRole(DrawnRole const&) = delete;
        DrawnRole& operator=(DrawnRole const&) = delete;

    private:
        bool _owner = false;
    };
}

#endif
