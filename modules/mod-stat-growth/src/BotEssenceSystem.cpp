#include "BotEssenceSystem.h"

#include "DataMap.h"
#include "EssenceTuning.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PowerScaling.h"
#include "ResourceBoostSystem.h"
#include "StatGrowthConfig.h"
#include "StatGrowthSystem.h"
#include "VitalityBoostSystem.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <limits>

namespace
{
constexpr char StateKey[] = "StatGrowthBotEssences";
constexpr char TimerKey[] = "StatGrowthBotEssencesTimer";

// How often a bot's group and its players' totals are looked at again
constexpr int32 CheckMs = 3000;

constexpr uint32 StatCount = static_cast<uint32>(PermanentStat::Count);

struct BotEssenceState : public DataMap::Base
{
    std::array<uint32, StatCount> appliedStats = {};   // exactly what is on the bot, to take off again
    uint32 statPoints = 0;                              // the average the stats were spread from
    uint32 vitality = 0;
    uint32 resource = 0;
    uint32 players = 0;
};

struct BotEssenceTimer : public DataMap::Base
{
    int32 left = 0;
};

// SetBotEssenceFloor: the least the bot carries in that instance, in saved points (the diminishing returns apply on
// top, as for a player's)
constexpr char FloorKey[] = "StatGrowthBotEssenceFloor";
struct BotEssenceFloor : public DataMap::Base
{
    uint32 instanceId = 0;
    uint32 statPointsPerStat = 0;
    uint32 vitality = 0;
};

struct GroupEssences
{
    uint32 statPoints = 0;
    uint32 vitality = 0;
    uint32 resource = 0;
    uint32 players = 0;
};

bool IsBot(Player* player)
{
    return player && player->GetSession() && player->GetSession()->IsBot();
}

// The average essences of the group's real players on the bot's map. Only that map is read: another map updates on
// another thread.
GroupEssences ReadGroupEssences(Player* bot)
{
    GroupEssences result;
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) ||
        !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::BotEssencesEnabled))
        return result;

    Group* group = bot->GetGroup();
    if (!group)
        return result;

    uint64 statPoints = 0;
    uint64 vitality = 0;
    uint64 resource = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && member->GetSession() && !member->GetSession()->IsBot() &&
            member->IsInMap(bot))
        {
            statPoints += GetStoredStatGrowthTotal(member);
            vitality += GetStoredVitalityPoints(member);
            resource += GetStoredResourcePoints(member);
            ++result.players;
        }

    if (!result.players)
        return result;

    result.statPoints = static_cast<uint32>(std::min<uint64>(statPoints / result.players,
        std::numeric_limits<uint32>::max()));
    result.vitality = static_cast<uint32>(std::min<uint64>(vitality / result.players,
        std::numeric_limits<uint32>::max()));
    if (BotEssenceFloor const* floor = bot->CustomData.Get<BotEssenceFloor>(FloorKey);
        floor && floor->instanceId && floor->instanceId == bot->GetInstanceId())
    {
        uint32 const statCount = static_cast<uint32>(GetClassPermanentStats(bot->getClass()).size());
        result.statPoints = std::max(result.statPoints, floor->statPointsPerStat * statCount);
        result.vitality = std::max(result.vitality, floor->vitality);
    }
    result.resource = static_cast<uint32>(std::min<uint64>(resource / result.players,
        std::numeric_limits<uint32>::max()));
    return result;
}

void StripStats(Player* bot, BotEssenceState* state)
{
    for (uint32 index = 0; index < StatCount; ++index)
        if (uint32 const amount = state->appliedStats[index])
            ApplyPermanentStat(bot, static_cast<PermanentStat>(index), amount, false);
    state->appliedStats.fill(0);
    state->statPoints = 0;
}

// A player's Essence of Growth picks one of its class's stats at random, so a player's points end up spread evenly
// over them: the bot takes the average total spread the same way (the remainder to the first stats)
void ApplyStats(Player* bot, BotEssenceState* state, uint32 statPoints)
{
    std::span<PermanentStat const> const classStats = GetClassPermanentStats(bot->getClass());
    state->statPoints = statPoints;
    if (classStats.empty() || !statPoints)
        return;

    uint32 const share = statPoints / classStats.size();
    uint32 remainder = statPoints % classStats.size();
    for (PermanentStat stat : classStats)
    {
        uint32 amount = share;
        if (remainder)
        {
            ++amount;
            --remainder;
        }
        if (!amount)
            continue;
        // Through the players' diminishing returns, as their own points are
        amount = EssenceTuning::Diminished(amount, EssenceTuning::GrowthCeiling);
        state->appliedStats[static_cast<uint32>(stat)] += amount;
        ApplyPermanentStat(bot, stat, amount, true);
    }
}

// Looks at the bot's group again and rebuilds the mirror if the players' averages moved. Never in combat, like the
// bot's paragon board: stats and health are not pulled from under a fight.
// force: on a map change, whatever the combat flag it carried over
void RefreshBot(Player* bot, bool force = false)
{
    if ((bot->IsInCombat() && !force) || !bot->IsInWorld())
        return;

    GroupEssences const wanted = ReadGroupEssences(bot);
    BotEssenceState* state = bot->CustomData.Get<BotEssenceState>(StateKey);
    if (!wanted.players)
    {
        if (!state)
            return;
        StripStats(bot, state);
        bool const hadVitality = state->vitality != 0;
        bot->CustomData.Erase(StateKey);
        if (hadVitality)
            bot->UpdateMaxHealth();
        LOG_DEBUG("module", "BotEssences: {} lost its mirror", bot->GetName());
        return;
    }

    if (!state)
        state = bot->CustomData.GetDefault<BotEssenceState>(StateKey);

    state->players = wanted.players;
    bool changed = false;
    if (state->statPoints != wanted.statPoints)
    {
        StripStats(bot, state);
        ApplyStats(bot, state, wanted.statPoints);
        changed = true;
    }

    if (state->resource != wanted.resource)
    {
        state->resource = wanted.resource;
        changed = true;
    }

    if (state->vitality != wanted.vitality)
    {
        state->vitality = wanted.vitality;
        bot->UpdateMaxHealth();
        changed = true;
    }

    if (changed)
        LOG_DEBUG("module", "BotEssences: {} mirrors {} player(s): {} stat points, {} Vitality, {}% resource",
            bot->GetName(), wanted.players, wanted.statPoints, wanted.vitality, wanted.resource);
}
}

void RefreshBotEssences(Player* bot)
{
    if (!IsBot(bot))
        return;

    RefreshBot(bot, true);
    bot->CustomData.GetDefault<BotEssenceTimer>(TimerKey)->left = CheckMs;
}

void UpdateBotEssences(Player* bot, uint32 diff)
{
    if (!IsBot(bot))
        return;

    BotEssenceTimer* timer = bot->CustomData.GetDefault<BotEssenceTimer>(TimerKey);
    timer->left -= static_cast<int32>(diff);
    if (timer->left > 0)
        return;
    timer->left = CheckMs;
    RefreshBot(bot);
}

uint32 GetBotEssenceVitality(Player* player)
{
    if (!IsBot(player))
        return 0;
    BotEssenceState const* state = player->CustomData.Get<BotEssenceState>(StateKey);
    return state ? state->vitality : 0;
}

uint32 GetBotEssenceResource(Player* player)
{
    if (!IsBot(player))
        return 0;
    BotEssenceState const* state = player->CustomData.Get<BotEssenceState>(StateKey);
    return state ? state->resource : 0;
}

void SetBotEssenceFloor(Player* bot, uint32 instanceId, float itemLevel)
{
    if (!IsBot(bot))
        return;
    float const keys = Power::ProgressKeys(itemLevel);
    BotEssenceFloor* floor = bot->CustomData.GetDefault<BotEssenceFloor>(FloorKey);
    floor->instanceId = instanceId;
    floor->statPointsPerStat = static_cast<uint32>(Power::EssenceGrowthPerKey * keys);
    floor->vitality = static_cast<uint32>(Power::EssenceVitalityPerKey * keys);
}

uint32 GetBotEssenceEffectiveStats(Player* bot)
{
    BotEssenceState const* state = IsBot(bot) ? bot->CustomData.Get<BotEssenceState>(StateKey) : nullptr;
    uint32 total = 0;
    if (state)
        for (uint32 amount : state->appliedStats)
            total += amount;
    return total;
}

bool GetBotEssenceSummary(Player* bot, uint32& statPoints, uint32& vitality, uint32& resource, uint32& players)
{
    statPoints = vitality = resource = players = 0;
    if (!IsBot(bot))
        return false;
    BotEssenceState const* state = bot->CustomData.Get<BotEssenceState>(StateKey);
    if (!state)
        return false;
    statPoints = state->statPoints;
    vitality = state->vitality;
    resource = state->resource;
    players = state->players;
    return true;
}
