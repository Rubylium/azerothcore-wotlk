#include "EssenceTierSystem.h"

#include "Creature.h"
#include "EssenceTuning.h"
#include "ExperienceBoostSystem.h"
#include "BotEssenceSystem.h"
#include "Chat.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StatGrowthSystem.h"
#include "StringConvert.h"
#include "VitalityBoostSystem.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "FortuneBoostSystem.h"
#include "LootMgr.h"
#include "Random.h"
#include "ResourceBoostSystem.h"
#include "StatGrowthConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
struct EssenceEntries
{
    uint32 faint;
    uint32 greater;
    StatGrowthConfigKey ascendantConfigKey;
};

constexpr std::array EssenceItems = {
    EssenceEntries { 1533, 1612, StatGrowthConfigKey::ItemEntry },
    EssenceEntries { 3338, 1267, StatGrowthConfigKey::ExperienceItemEntry },
    EssenceEntries { 2461, 3441, StatGrowthConfigKey::ResourceItemEntry },
    EssenceEntries { 1704, 3507, StatGrowthConfigKey::VitalityItemEntry },
    EssenceEntries { 2050, 1950, StatGrowthConfigKey::FortuneItemEntry }
};

EssenceEntries const& GetEssenceEntries(EssenceFamily family)
{
    return EssenceItems[static_cast<uint8>(family)];
}

uint32 GetEssenceEntry(EssenceFamily family, EssenceTier tier)
{
    EssenceEntries const& entries = GetEssenceEntries(family);
    switch (tier)
    {
        case EssenceTier::Faint:
            return entries.faint;
        case EssenceTier::Greater:
            return entries.greater;
        case EssenceTier::Ascendant:
            return statGrowthConfig.GetConfigValue<uint32>(entries.ascendantConfigKey);
    }

    return entries.faint;
}

EssenceTier RollEssenceTier()
{
    float const ascendantChance = statGrowthConfig.GetConfigValue<float>(
        StatGrowthConfigKey::AscendantTierChance);
    float const greaterChance = std::min(
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::GreaterTierChance),
        100.0f - ascendantChance);
    float const roll = frand(0.0f, 100.0f);

    if (roll < ascendantChance)
        return EssenceTier::Ascendant;
    if (roll < ascendantChance + greaterChance)
        return EssenceTier::Greater;
    return EssenceTier::Faint;
}

uint32 GetTierMultiplier(EssenceTier tier)
{
    switch (tier)
    {
        case EssenceTier::Faint:
            return 1;
        case EssenceTier::Greater:
            return statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::GreaterTierMultiplier);
        case EssenceTier::Ascendant:
            return statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::AscendantTierMultiplier);
    }

    return 1;
}
}

void AddTieredEssenceLoot(Creature* killed, EssenceFamily family)
{
    EssenceTier const tier = RollEssenceTier();
    uint32 const itemEntry = GetEssenceEntry(family, tier);
    LootStoreItem const essence(itemEntry, 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, 1, 1);
    killed->loot.AddItem(essence);

    // In a group nobody has to roll on it: the players roll Need automatically (Group::AutoRoll)
    for (auto item = killed->loot.items.rbegin(); item != killed->loot.items.rend(); ++item)
        if (item->itemid == itemEntry)
        {
            item->auto_roll = true;
            break;
        }
}

EssenceTier GetEssenceTier(uint32 itemEntry)
{
    for (uint8 familyIndex = 0; familyIndex < EssenceItems.size(); ++familyIndex)
    {
        EssenceFamily const family = static_cast<EssenceFamily>(familyIndex);
        for (uint8 tierIndex = 0; tierIndex <= static_cast<uint8>(EssenceTier::Ascendant); ++tierIndex)
        {
            EssenceTier const tier = static_cast<EssenceTier>(tierIndex);
            if (GetEssenceEntry(family, tier) == itemEntry)
                return tier;
        }
    }

    return EssenceTier::Faint;
}

// What a Mythic+ clear pays in essences.
//
// A key drops none of its own: every creature in one is stripped of its loot (MythicDungeonSystem.cpp), which is
// the point of it -- nobody stops to loot a corpse against a timer. That leaves the end of the run paying for the
// whole dungeon, and it was paying a single essence while the same dungeon on heroic paid one for every other
// kill, across five families dropping at ten percent each.
//
// The rate is read back from those drop chances instead of written down again, so tuning them moves this with
// them. The baseline it multiplies is a fixed number of kills rather than the run's real one: a key is routed
// around the packs it can skip, and paying per kill would push groups into fighting the timer for essences.
uint32 GetMythicEssenceReward(uint32 level)
{
    float const perKill =
        (statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::DropChance) +
         statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::ExperienceDropChance) +
         statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::ResourceDropChance) +
         statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::VitalityDropChance) +
         statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::FortuneDropChance)) / 100.0f;

    float const keyBonus = 1.0f + level *
        statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::MythicEssenceBonusPerKeyLevel) / 100.0f;
    float const reward = perKill * keyBonus *
        statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::MythicEssenceBaseline);

    // A cleared key always pays something, however the chances above are tuned
    return std::max<uint32>(1, static_cast<uint32>(std::lround(reward)));
}

uint32 RollEssenceEntry(uint32 tierRolls)
{
    EssenceTier tier = EssenceTier::Faint;
    for (uint32 roll = 0; roll < std::max<uint32>(tierRolls, 1); ++roll)
        tier = std::max(tier, RollEssenceTier());

    EssenceFamily const family = static_cast<EssenceFamily>(urand(0, static_cast<uint32>(EssenceItems.size() - 1)));
    return GetEssenceEntry(family, tier);
}

std::string_view GetEssenceTierName(EssenceTier tier)
{
    switch (tier)
    {
        case EssenceTier::Faint:
            return "Faint";
        case EssenceTier::Greater:
            return "Greater";
        case EssenceTier::Ascendant:
            return "Ascendant";
    }

    return "Faint";
}

uint32 GetTieredEssenceBonus(uint32 baseAmount, uint32 itemEntry)
{
    uint64 const scaledAmount = static_cast<uint64>(baseAmount) * GetTierMultiplier(GetEssenceTier(itemEntry));
    return static_cast<uint32>(std::min<uint64>(scaledAmount, std::numeric_limits<uint32>::max()));
}

bool IsEssenceItem(uint32 itemEntry)
{
    EssenceFamily family;
    return TryGetEssenceFamily(itemEntry, family);
}

bool TryGetEssenceFamily(uint32 itemEntry, EssenceFamily& family)
{
    for (uint8 familyIndex = 0; familyIndex < EssenceItems.size(); ++familyIndex)
    {
        EssenceFamily const candidate = static_cast<EssenceFamily>(familyIndex);
        if (itemEntry == GetEssenceEntry(candidate, EssenceTier::Faint) ||
            itemEntry == GetEssenceEntry(candidate, EssenceTier::Greater) ||
            itemEntry == GetEssenceEntry(candidate, EssenceTier::Ascendant))
        {
            family = candidate;
            return true;
        }
    }

    return false;
}

uint32 ResolveEssenceEntry(Player* player, uint32 itemEntry)
{
    EssenceFamily family;
    if (!player || !TryGetEssenceFamily(itemEntry, family))
        return itemEntry;

    uint32 const resource = GetResourceBonus(player);
    uint32 const fortune = GetFortuneBonus(player);
    uint32 const experience = GetEffectiveExperienceBonus(player);
    auto const capped = [resource, fortune, experience](EssenceFamily candidate)
    {
        return EssenceTuning::IsFamilyCapped(candidate, resource, fortune, experience);
    };
    if (!capped(family))
        return itemEntry;

    std::array<EssenceFamily, EssenceItems.size()> available;
    uint32 count = 0;
    for (uint32 index = 0; index < EssenceItems.size(); ++index)
    {
        EssenceFamily const candidate = static_cast<EssenceFamily>(index);
        if (!capped(candidate))
            available[count++] = candidate;
    }
    return count ? GetEssenceEntry(available[urand(0, count - 1)], GetEssenceTier(itemEntry)) : itemEntry;
}

void HandleEssenceAddonMessage(Player* player, uint32 language, std::string const& message)
{
    constexpr std::string_view Prefix = "Essences\tQ\t";
    if (!player || language != LANG_ADDON || !message.starts_with(Prefix))
        return;

    uint32 const low = Acore::StringTo<uint32>(std::string_view(message).substr(Prefix.size())).value_or(0);
    Player* target = low ? ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(low)) : nullptr;
    if (!target || !target->IsInWorld())
        return;

    bool const bot = target->GetSession() && target->GetSession()->IsBot();
    uint32 growth = GetStoredStatGrowthTotal(target);
    uint32 growthEffective = GetEffectiveStatGrowthTotal(target);
    uint32 vitality = GetStoredVitalityPoints(target);
    uint32 resource = GetResourceBonus(target);
    if (bot)
    {
        uint32 statPoints = 0;
        uint32 mirroredVitality = 0;
        uint32 mirroredResource = 0;
        uint32 players = 0;
        if (GetBotEssenceSummary(target, statPoints, mirroredVitality, mirroredResource, players))
        {
            growth += statPoints;
            growthEffective += GetBotEssenceEffectiveStats(target);
            vitality += mirroredVitality;
            resource = std::max(resource, mirroredResource);
        }
    }
    uint32 const vitalityEffective = GetEffectiveVitalityPoints(vitality);

    std::string const reply = Acore::StringFormat("Essences\tS\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", low, growth,
        growthEffective, vitality, vitalityEffective, GetVitalityHealth(target, vitalityEffective),
        GetEffectiveExperienceBonus(target), GetFortuneBonus(target), resource, bot ? 1 : 0);
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, reply);
    player->SendDirectMessage(&packet);
}
