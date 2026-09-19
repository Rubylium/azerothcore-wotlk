#include "ResourceBoostSystem.h"

#include "CharacterDatabase.h"
#include "Creature.h"
#include "DataMap.h"
#include "EssenceTierSystem.h"
#include "GameTime.h"
#include "Group.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "PersonalLootSystem.h"
#include "Random.h"
#include "StatGrowthConfig.h"
#include "WorldSession.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr char ResourceSettingsSource[] = "mod_stat_growth_resource";
constexpr uint32 ResourceBonusSetting = 0;

bool IsEligibleCreature(Creature const* killed)
{
    return killed && !killed->IsPet() && !killed->IsTotem() &&
        killed->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}

uint32 GetStoredResourceBonus(Player* player)
{
    return player->GetPlayerSetting(ResourceSettingsSource, ResourceBonusSetting).value;
}

uint32 GetOwnResourceBonus(Player* player)
{
    uint64 const bonus = static_cast<uint64>(GetStoredResourceBonus(player)) +
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::ResourceRegeneration);
    return static_cast<uint32>(std::min<uint64>(bonus, std::numeric_limits<uint32>::max()));
}

// A bot has no essences of its own. In a group with real players (a Dungeon or Raid Finder run) it carries about
// what they carry: their average bonus, give or take 10% per bot, so bots keep pace with the players they fill for.
// Regeneration asks for it every tick, so the value is kept for a few seconds.
struct MirroredResourceBonus : public DataMap::Base
{
    uint32 value = 0;
    Milliseconds refreshedAt = 0ms;
};

constexpr Milliseconds MirrorRefreshInterval = 5s;

uint32 GetMirroredResourceBonus(Player* bot)
{
    MirroredResourceBonus* mirror = bot->CustomData.GetDefault<MirroredResourceBonus>("StatGrowthResourceMirror");
    Milliseconds const now = GameTime::GetGameTimeMS();
    if (mirror->refreshedAt != 0ms && now - mirror->refreshedAt < MirrorRefreshInterval)
        return mirror->value;

    mirror->refreshedAt = now;
    mirror->value = 0;

    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint64 total = 0;
    uint32 players = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && !member->GetSession()->IsBot())
        {
            total += GetOwnResourceBonus(member);
            ++players;
        }

    if (!players)
        return 0;

    // A fixed factor per bot between 0.90 and 1.10
    float const variation = 0.9f + float((bot->GetGUID().GetCounter() * 2654435761u) % 21) / 100.0f;
    mirror->value = uint32(float(total / players) * variation);
    return mirror->value;
}

uint32 GetResourceBonus(Player* player)
{
    return player->GetSession()->IsBot() ? GetMirroredResourceBonus(player) : GetOwnResourceBonus(player);
}

bool IsActivePrimaryResource(Player const* player, Powers power)
{
    return player->getPowerType() == power;
}
}

void ApplyResourceRegenerationBoost(Player* player, Powers power, float& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || amount <= 0.0f ||
        !IsActivePrimaryResource(player, power))
        return;

    uint32 const bonusPercent = GetResourceBonus(player);
    amount *= 1.0f + static_cast<float>(bonusPercent) / 100.0f;
}

void ApplyResourceGenerationBoost(Player* player, Powers power, int32& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || amount <= 0 ||
        !IsActivePrimaryResource(player, power) || (power != POWER_RAGE && power != POWER_RUNIC_POWER))
        return;

    uint32 const bonusPercent = GetResourceBonus(player);
    double const boostedAmount = static_cast<double>(amount) * (100.0 + bonusPercent) / 100.0;
    amount = static_cast<int32>(std::min<double>(std::round(boostedAmount), std::numeric_limits<int32>::max()));
}

bool GrantResourceBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = GetStoredResourceBonus(player);
    if (amount > std::numeric_limits<uint32>::max() - currentBonus)
        return false;

    totalBonus = currentBonus + amount;
    PlayerSettingVector settings(1);
    settings[ResourceBonusSetting].value = totalBonus;
    player->UpdatePlayerSetting(ResourceSettingsSource, ResourceBonusSetting, totalBonus);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), ResourceSettingsSource, settings);
    CharacterDatabase.Execute(statement);
    return true;
}

void TryAddResourceBoostLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player ||
        !IsEligibleCreature(killed))
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::ResourceDropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Resource);
}
