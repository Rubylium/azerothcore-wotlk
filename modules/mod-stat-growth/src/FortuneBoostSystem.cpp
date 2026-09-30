#include "FortuneBoostSystem.h"

#include "CharacterDatabase.h"
#include "Creature.h"
#include "EssenceTierSystem.h"
#include "EssenceTuning.h"
#include "LootMgr.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "PersonalLootSystem.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include <algorithm>

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

    return EssenceTuning::CombinedBonus(GetStoredFortuneBonus(player),
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::Fortune), 0, EssenceTuning::MaxFortuneBonus);
}

void ApplyFortuneGoldBoost(Player* player, uint32& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || !amount)
        return;

    uint32 const bonusPercent = GetFortuneBonus(player);
    uint64 const boostedAmount = static_cast<uint64>(amount) + static_cast<uint64>(amount) * bonusPercent / 100;
    amount = static_cast<uint32>(std::min<uint64>(boostedAmount, MAX_MONEY_AMOUNT));
}

bool GrantFortuneBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = GetStoredFortuneBonus(player);
    amount = EssenceTuning::GrantAmount(std::max(currentBonus, GetFortuneBonus(player)), amount,
        EssenceTuning::MaxFortuneBonus);
    if (!amount)
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
