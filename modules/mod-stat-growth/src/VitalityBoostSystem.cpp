#include "VitalityBoostSystem.h"

#include "CharacterDatabase.h"
#include "Creature.h"
#include "EssenceTierSystem.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "PersonalLootSystem.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include <algorithm>
#include <limits>

namespace
{
constexpr char VitalitySettingsSource[] = "mod_stat_growth_vitality";
constexpr uint32 VitalityBonusSetting = 0;

bool IsEligibleCreature(Creature const* killed)
{
    return killed && !killed->IsPet() && !killed->IsTotem() &&
        killed->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}

uint32 GetStoredVitalityBonus(Player* player)
{
    return player->GetPlayerSetting(VitalitySettingsSource, VitalityBonusSetting).value;
}

uint32 GetVitalityBonus(Player* player)
{
    uint64 const bonus = static_cast<uint64>(GetStoredVitalityBonus(player)) +
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::MaximumHealth);
    return static_cast<uint32>(std::min<uint64>(bonus, std::numeric_limits<uint32>::max()));
}
}

void ApplyVitalityBoost(Player* player, float& maxHealth)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player)
        return;

    uint32 const bonusPercent = GetVitalityBonus(player);
    double const boostedHealth = static_cast<double>(maxHealth) * (100.0 + bonusPercent) / 100.0;
    maxHealth = static_cast<float>(std::min<double>(boostedHealth, std::numeric_limits<uint32>::max()));
}

bool GrantVitalityBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = GetStoredVitalityBonus(player);
    if (amount > std::numeric_limits<uint32>::max() - currentBonus)
        return false;

    totalBonus = currentBonus + amount;
    PlayerSettingVector settings(1);
    settings[VitalityBonusSetting].value = totalBonus;
    player->UpdatePlayerSetting(VitalitySettingsSource, VitalityBonusSetting, totalBonus);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), VitalitySettingsSource, settings);
    CharacterDatabase.Execute(statement);
    player->UpdateMaxHealth();
    return true;
}

void TryAddVitalityBoostLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player ||
        !IsEligibleCreature(killed))
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::VitalityDropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Vitality);
}
