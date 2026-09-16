#ifndef MOD_STAT_GROWTH_PERSONAL_LOOT_SYSTEM_H
#define MOD_STAT_GROWTH_PERSONAL_LOOT_SYSTEM_H

#include "DatabaseEnvFwd.h"
#include "Define.h"
#include "ObjectGuid.h"

#include <array>
#include <string>

class Item;
class Player;
class Unit;

enum class PersonalLootAffix : uint8
{
    None,
    Strength,
    Agility,
    Intellect,
    Spirit,
    Stamina,
    AttackPower,
    SpellPower,
    CriticalStrike,
    Haste,
    Hit,
    Expertise,
    ArmorPenetration,
    Defense,
    Dodge,
    Parry,
    Block,
    ManaRegeneration,
    ExperienceGain,
    ResourceRegeneration,
    MaximumHealth,
    Fortune,
    // Stored in the database by value: only ever append new affixes
    Leech
};

struct PersonalLootAffixRoll
{
    PersonalLootAffix type = PersonalLootAffix::None;
    uint32 value = 0;
};

struct PersonalLootRoll
{
    uint8 itemQuality = 0;
    uint8 rolledClass = 0;
    std::array<PersonalLootAffixRoll, 4> affixes;
};

void LoadPersonalLootRolls();
void DeletePersonalLootRoll(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid);
void TryRollPersonalLoot(Player* player, Item* item);
void ApplyEquippedPersonalLoot(Player* player);
void ApplyPersonalLootItem(Player* player, Item* item);
void RemovePersonalLootItem(Player* player, Item* item);
void ClearPersonalLootPlayerState(Player* player);
void BeginPersonalLootAddonHandshake(Player* player);
void UpdatePersonalLootAddonHandshake(Player* player, uint32 diff);
void HandlePersonalLootAddonMessage(Player* player, uint32 language, std::string const& message);
uint32 GetEquippedPersonalLootBonus(Player const* player, PersonalLootAffix affix);
void ApplyPersonalLootLeech(Unit* attacker, Unit* victim, uint32 damage);

#endif
