#include "FortuneBoostSystem.h"

#include "CharacterDatabase.h"
#include "Creature.h"
#include "EssenceTierSystem.h"
#include "LootMgr.h"
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
constexpr char FortuneSettingsSource[] = "mod_stat_growth_fortune";
constexpr uint32 FortuneBonusSetting = 0;

bool IsEligibleCreature(Creature const* killed)
{
    return killed && !killed->IsPet() && !killed->IsTotem() &&
        killed->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}

uint32 GetStoredFortuneBonus(Player* player)
{
    return player->GetPlayerSetting(FortuneSettingsSource, FortuneBonusSetting).value;
}

}

uint32 GetFortuneBonus(Player* player)
{
    if (!player || !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled))
        return 0;

    uint64 const bonus = static_cast<uint64>(GetStoredFortuneBonus(player)) +
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::Fortune);
    return static_cast<uint32>(std::min<uint64>(bonus, std::numeric_limits<uint32>::max()));
}

void ApplyFortuneGoldBoost(Player* player, int32& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || amount <= 0)
        return;

    uint32 const bonusPercent = GetFortuneBonus(player);
    int64 const boostedAmount = static_cast<int64>(amount) + static_cast<int64>(amount) * bonusPercent / 100;
    amount = static_cast<int32>(std::min<int64>(boostedAmount, std::numeric_limits<int32>::max()));
}

bool GrantFortuneBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = GetStoredFortuneBonus(player);
    if (amount > std::numeric_limits<uint32>::max() - currentBonus)
        return false;

    totalBonus = currentBonus + amount;
    PlayerSettingVector settings(1);
    settings[FortuneBonusSetting].value = totalBonus;
    player->UpdatePlayerSetting(FortuneSettingsSource, FortuneBonusSetting, totalBonus);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), FortuneSettingsSource, settings);
    CharacterDatabase.Execute(statement);
    return true;
}

void TryAddFortuneBoostLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player ||
        !IsEligibleCreature(killed))
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::FortuneDropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Fortune);
}
