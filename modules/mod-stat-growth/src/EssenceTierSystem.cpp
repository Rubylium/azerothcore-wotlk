#include "EssenceTierSystem.h"

#include "Creature.h"
#include "LootMgr.h"
#include "Random.h"
#include "StatGrowthConfig.h"

#include <algorithm>
#include <array>
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
