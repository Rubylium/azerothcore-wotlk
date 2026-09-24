#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"

#include <array>
#include <utility>

// What the client shows as wearable must be what the server lets a character equip.
//
// The two decide it from different things. The server equips on the skill (Player::CanEquipItem: an armor or weapon
// type is usable when its skill has a value), while the client colours an item red from the proficiency masks the
// server sends it, and the core only sends those when a proficiency spell is cast (Spell::EffectProficiency). Those
// casts happen while the character is still loading, and whatever the client misses there it never hears again - so
// a warrior with the Mail skill saw every mail item in red, in the default bags and DragonUI's alike, and could
// equip it all the same.
//
// So the masks are rebuilt from the skills themselves, the very rule the server equips by, and sent once the
// character is in the world, and again whenever a skill is gained or lost. The client can then only ever show what
// the server allows.
namespace
{
// Armor type (ITEM_SUBCLASS_ARMOR_*) and the skill that lets it be worn
constexpr std::array<std::pair<uint8, uint32>, 5> ArmorSkills = { {
    { ITEM_SUBCLASS_ARMOR_CLOTH,   SKILL_CLOTH },
    { ITEM_SUBCLASS_ARMOR_LEATHER, SKILL_LEATHER },
    { ITEM_SUBCLASS_ARMOR_MAIL,    SKILL_MAIL },
    { ITEM_SUBCLASS_ARMOR_PLATE,   SKILL_PLATE_MAIL },
    { ITEM_SUBCLASS_ARMOR_SHIELD,  SKILL_SHIELD },
} };

// Weapon type (ITEM_SUBCLASS_WEAPON_*) and the skill that lets it be wielded
constexpr std::array<std::pair<uint8, uint32>, 16> WeaponSkills = { {
    { ITEM_SUBCLASS_WEAPON_AXE,          SKILL_AXES },
    { ITEM_SUBCLASS_WEAPON_AXE2,         SKILL_2H_AXES },
    { ITEM_SUBCLASS_WEAPON_BOW,          SKILL_BOWS },
    { ITEM_SUBCLASS_WEAPON_GUN,          SKILL_GUNS },
    { ITEM_SUBCLASS_WEAPON_MACE,         SKILL_MACES },
    { ITEM_SUBCLASS_WEAPON_MACE2,        SKILL_2H_MACES },
    { ITEM_SUBCLASS_WEAPON_POLEARM,      SKILL_POLEARMS },
    { ITEM_SUBCLASS_WEAPON_SWORD,        SKILL_SWORDS },
    { ITEM_SUBCLASS_WEAPON_SWORD2,       SKILL_2H_SWORDS },
    { ITEM_SUBCLASS_WEAPON_STAFF,        SKILL_STAVES },
    { ITEM_SUBCLASS_WEAPON_FIST,         SKILL_FIST_WEAPONS },
    { ITEM_SUBCLASS_WEAPON_DAGGER,       SKILL_DAGGERS },
    { ITEM_SUBCLASS_WEAPON_THROWN,       SKILL_THROWN },
    { ITEM_SUBCLASS_WEAPON_CROSSBOW,     SKILL_CROSSBOWS },
    { ITEM_SUBCLASS_WEAPON_WAND,         SKILL_WANDS },
    { ITEM_SUBCLASS_WEAPON_FISHING_POLE, SKILL_FISHING },
} };

// Every skill above: a change to any of them changes a mask
bool IsProficiencySkill(uint32 skill)
{
    for (auto const& [subclass, armorSkill] : ArmorSkills)
        if (armorSkill == skill)
            return true;
    for (auto const& [subclass, weaponSkill] : WeaponSkills)
        if (weaponSkill == skill)
            return true;
    return false;
}

void SyncProficiency(Player* player)
{
    // Relics (librams, idols, totems, sigils) have no skill: they come from the class's own proficiency spells, and
    // are kept as those gave them. Miscellaneous armor and weapons are anyone's.
    uint32 constexpr relics = (1u << ITEM_SUBCLASS_ARMOR_LIBRAM) | (1u << ITEM_SUBCLASS_ARMOR_IDOL) |
        (1u << ITEM_SUBCLASS_ARMOR_TOTEM) | (1u << ITEM_SUBCLASS_ARMOR_SIGIL);
    uint32 armor = (player->GetArmorProficiency() & relics) | (1u << ITEM_SUBCLASS_ARMOR_MISC);
    for (auto const& [subclass, skill] : ArmorSkills)
        if (player->GetSkillValue(skill))
            armor |= 1u << subclass;

    uint32 weapon = 1u << ITEM_SUBCLASS_WEAPON_MISC;
    for (auto const& [subclass, skill] : WeaponSkills)
        if (player->GetSkillValue(skill))
            weapon |= 1u << subclass;

    // Kept on the player too, so a proficiency the core sends later (a new proficiency spell) carries these bits
    player->AddArmorProficiency(armor);
    player->AddWeaponProficiency(weapon);
    player->SendProficiency(ITEM_CLASS_ARMOR, armor);
    player->SendProficiency(ITEM_CLASS_WEAPON, weapon);
}

class ProficiencySyncPlayerScript final : public PlayerScript
{
public:
    ProficiencySyncPlayerScript() : PlayerScript("ProficiencySyncPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_SET_SKILL
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        SyncProficiency(player);
    }

    // A skill gained or lost once in the world (a trainer, a level, a prestige); the ones set while loading are
    // covered by the login
    void OnPlayerSetSkill(Player* player, uint32 skillId, uint32 /*value*/, uint32 /*max*/, uint32 /*step*/,
                          uint32 /*newValue*/) override
    {
        if (player->IsInWorld() && IsProficiencySkill(skillId))
            SyncProficiency(player);
    }
};
}

void AddProficiencySyncScripts()
{
    new ProficiencySyncPlayerScript();
}
