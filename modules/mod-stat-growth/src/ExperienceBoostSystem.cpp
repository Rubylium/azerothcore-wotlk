#include "ExperienceBoostSystem.h"

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
constexpr char ExperienceSettingsSource[] = "mod_stat_growth_xp";
constexpr uint32 ExperienceBonusSetting = 0;

bool IsEligibleCreature(Creature const* killed)
{
    return killed && !killed->IsPet() && !killed->IsTotem() &&
        killed->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}
}

void ApplyExperienceBoost(Player* player, uint32& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || amount == 0)
        return;

    uint64 const bonusPercent = static_cast<uint64>(player->GetPlayerSetting(
        ExperienceSettingsSource, ExperienceBonusSetting).value) +
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::ExperienceGain);
    uint64 const boostedAmount = static_cast<uint64>(amount) +
        static_cast<uint64>(amount) * bonusPercent / 100;
    amount = static_cast<uint32>(std::min<uint64>(boostedAmount, std::numeric_limits<uint32>::max()));
}

bool GrantExperienceBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = player->GetPlayerSetting(
        ExperienceSettingsSource, ExperienceBonusSetting).value;
    if (amount > std::numeric_limits<uint32>::max() - currentBonus)
        return false;

    totalBonus = currentBonus + amount;
    PlayerSettingVector settings(1);
    settings[ExperienceBonusSetting].value = totalBonus;
    player->UpdatePlayerSetting(ExperienceSettingsSource, ExperienceBonusSetting, totalBonus);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), ExperienceSettingsSource, settings);
    CharacterDatabase.Execute(statement);
    return true;
}

void TryAddExperienceBoostLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player ||
        !IsEligibleCreature(killed))
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::ExperienceDropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Experience);
}
