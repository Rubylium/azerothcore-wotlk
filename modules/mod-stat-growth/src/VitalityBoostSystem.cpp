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

}

void ApplyVitalityBoost(Player* player, float& maxHealth)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player)
        return;

    // Gear bonuses (the maximum-health affix) stay a percentage; the essences' Vitality points are a flat amount
    // per level, added after it, so they never multiply gear or other bonuses
    uint32 const affixPercent = GetEquippedPersonalLootBonus(player, PersonalLootAffix::MaximumHealth);
    double const boostedHealth = static_cast<double>(maxHealth) * (100.0 + affixPercent) / 100.0 +
        GetVitalityHealth(player, GetStoredVitalityBonus(player));
    maxHealth = static_cast<float>(std::min<double>(boostedHealth, std::numeric_limits<uint32>::max()));
}

uint32 GetVitalityHealth(Player const* player, uint32 points)
{
    double const perPoint = static_cast<double>(player->GetLevel()) *
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::VitalityHealthPerLevel);
    return static_cast<uint32>(std::min<double>(perPoint * points, std::numeric_limits<uint32>::max()));
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
