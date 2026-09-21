#ifndef MOD_STAT_GROWTH_CONFIG_H
#define MOD_STAT_GROWTH_CONFIG_H

#include "ConfigValueCache.h"

enum class StatGrowthConfigKey : uint8
{
    Enabled,
    AutoLearnClassSpells,
    PersonalLootEnabled,
    PersonalLootRequireAddon,
    PersonalLootAddonGraceSeconds,
    PersonalLootPoorChance,
    PersonalLootCommonChance,
    PersonalLootUncommonChance,
    PersonalLootRareChance,
    PersonalLootEpicChance,
    PersonalLootLegendaryChance,
    PersonalLootAffixScale,
    PersonalLootPercentScale,
    PersonalLootLeechScale,
    PersonalLootMaxLeech,
    SmartLootEnabled,
    SmartLootLevelWindow,
    SmartLootCreatureLevelTolerance,
    DropChance,
    BonusPerUse,
    ItemEntry,
    ExperienceDropChance,
    ExperienceBonusPerUse,
    ExperienceItemEntry,
    ResourceDropChance,
    ResourceBonusPerUse,
    ResourceItemEntry,
    VitalityDropChance,
    VitalityBonusPerUse,
    VitalityHealthPerLevel,
    VitalityItemEntry,
    FortuneDropChance,
    FortuneBonusPerUse,
    FortuneItemEntry,
    GreaterTierChance,
    AscendantTierChance,
    GreaterTierMultiplier,
    AscendantTierMultiplier,
    MythicEssenceBaseline,
    MythicEssenceBonusPerKeyLevel,
    ParagonEnabled,
    ParagonPointCap,
    ParagonNormalChance,
    ParagonHeroicChance,
    ParagonMythicZeroChance,
    ParagonMythicChance,
    ParagonMythicChancePerLevel,
    ParagonRaidChance,
    ParagonRaidHeroicChance,
    FortuneGearBonusChancePerPoint,
    FortuneGearBonusValuePerPoint,
    FortuneMaxGearBonusValueIncrease,
    Count
};

class StatGrowthConfig : public ConfigValueCache<StatGrowthConfigKey>
{
public:
    StatGrowthConfig();

    void BuildConfigCache() override;
};

extern StatGrowthConfig statGrowthConfig;

#endif
