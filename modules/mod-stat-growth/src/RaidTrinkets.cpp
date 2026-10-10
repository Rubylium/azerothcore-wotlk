#include "RaidTrinkets.h"

#include "Containers.h"
#include "LootFit.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"

#include <algorithm>
#include <array>
#include <vector>

namespace RaidTrinkets
{
namespace
{
// How often a piece of a raid's loot is one of its trinkets: a little more than a trinket slot's share of the
// pieces, so its sixteen come round
constexpr float ChancePct = 20.0f;

enum class Kind : uint8
{
    Fighter,
    Caster,
    Healer,
    Tank,
    Damage,     // a fighter's and a caster's alike
};

struct Trinket
{
    uint32 entry;
    Raid raid;
    Kind kind;
};

constexpr std::array<Trinket, 18> Trinkets = { {
    { 17836, Raid::HollowVoice, Kind::Fighter },    // Éclat du Marteau béni: attack power on a blow
    { 17837, Raid::HollowVoice, Kind::Fighter },    // Penne du Séraphin: haste, used
    { 17838, Raid::HollowVoice, Kind::Caster },     // Psautier du Néant: spell power on a harmful spell
    { 17839, Raid::HollowVoice, Kind::Caster },     // Souffle du Néant: spell power, used
    { 17840, Raid::HollowVoice, Kind::Healer },     // Chapelet de l'Archevêque: spell power on a heal
    { 17841, Raid::HollowVoice, Kind::Healer },     // Reliquaire d'Aldric: 20% more healing, used
    { 17842, Raid::HollowVoice, Kind::Tank },       // Pierre du Bastion: 15% less damage taken on a blow taken
    { 17843, Raid::HollowVoice, Kind::Tank },       // Cierge de la Dernière lumière: 25% more health, used
    { 17844, Raid::WardenVorhan, Kind::Fighter },   // Pierre à aiguiser du bourreau: attack power stacked by blows
    { 17845, Raid::WardenVorhan, Kind::Fighter },   // Cadran du couvre-feu: attack power, used
    { 17846, Raid::WardenVorhan, Kind::Caster },    // Registre d'écrou: haste on a harmful spell
    { 17847, Raid::WardenVorhan, Kind::Caster },    // Œil du Gardien-chef: 20% faster casts, used
    { 17848, Raid::WardenVorhan, Kind::Healer },    // Lettre de grâce: spell power stacked by heals
    { 17851, Raid::WardenVorhan, Kind::Healer },    // Tampon de libération: spell power, used
    { 17852, Raid::WardenVorhan, Kind::Tank },      // Maillon des fers: less damage taken stacked by blows taken
    { 17853, Raid::WardenVorhan, Kind::Tank },      // Verrou du cachot: 25% less damage taken, used
    { 17856, Raid::EscapeHunter, Kind::Damage },    // Croc du gangrechien: damage done stacked by blows and spells
    { 17857, Raid::EscapeHunter, Kind::Damage },    // Cor de l'hallali: critical strike rating, used
} };

Kind KindOf(LootFit::Role role)
{
    switch (role)
    {
        case LootFit::Role::Tank: return Kind::Tank;
        case LootFit::Role::Caster: return Kind::Caster;
        case LootFit::Role::Healer: return Kind::Healer;
        default: return Kind::Fighter;
    }
}

// The raid's trinkets of a kind the player has not got (worn, in the bags or in the bank) and that fit them
std::vector<ItemTemplate const*> Missing(Player* player, Raid raid, Kind kind)
{
    std::vector<ItemTemplate const*> missing;
    for (Trinket const& trinket : Trinkets)
    {
        bool const ofKind = trinket.kind == kind ||
            (trinket.kind == Kind::Damage && (kind == Kind::Fighter || kind == Kind::Caster));
        if (trinket.raid != raid || !ofKind || player->HasItemCount(trinket.entry, 1, true))
            continue;
        ItemTemplate const* item = sObjectMgr->GetItemTemplate(trinket.entry);
        if (item && LootFit::Fits(player, *item))
            missing.push_back(item);
    }
    return missing;
}

bool HasKind(Raid raid, Kind kind)
{
    return std::ranges::any_of(Trinkets, [raid, kind](Trinket const& trinket)
        { return trinket.raid == raid && trinket.kind == kind; });
}
}

ItemTemplate const* Roll(Player* player, Raid raid)
{
    if (!player || !roll_chance_f(ChancePct))
        return nullptr;
    LootFit::DrawnRole const drawn(player);
    Kind const kind = KindOf(LootFit::RoleOf(player));
    std::vector<ItemTemplate const*> missing = Missing(player, raid, kind);
    // A tank with all the raid's own gets a fighter's; a raid with none for tanks gives them none (it would be all
    // they got from it)
    if (missing.empty() && kind == Kind::Tank && HasKind(raid, Kind::Tank))
        missing = Missing(player, raid, Kind::Fighter);
    return missing.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(missing);
}
}
