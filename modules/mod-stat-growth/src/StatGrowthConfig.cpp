#include "StatGrowthConfig.h"

StatGrowthConfig statGrowthConfig;

StatGrowthConfig::StatGrowthConfig() : ConfigValueCache(StatGrowthConfigKey::Count) { }

void StatGrowthConfig::BuildConfigCache()
{
    SetConfigValue<bool>(StatGrowthConfigKey::Enabled, "StatGrowth.Enabled", true);
    SetConfigValue<bool>(StatGrowthConfigKey::AutoLearnClassSpells, "StatGrowth.AutoLearnClassSpells", true);
    SetConfigValue<bool>(StatGrowthConfigKey::PersonalLootEnabled, "PersonalLoot.Enabled", true);
    SetConfigValue<bool>(StatGrowthConfigKey::PersonalLootRequireAddon, "PersonalLoot.RequireAddon", true);
    SetConfigValue<uint32>(StatGrowthConfigKey::PersonalLootAddonGraceSeconds,
        "PersonalLoot.AddonGraceSeconds", 20, Reloadable::Yes,
        [](uint32 value) { return value >= 10 && value <= 120; }, "10 through 120");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootPoorChance, "GearBonus.PoorChance", 5.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootCommonChance, "GearBonus.CommonChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootUncommonChance, "GearBonus.UncommonChance", 25.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootRareChance, "GearBonus.RareChance", 45.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootEpicChance, "GearBonus.EpicChance", 70.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootLegendaryChance,
        "GearBonus.LegendaryChance", 100.0f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootAffixScale, "GearBonus.AttributeScale", 0.10f,
        Reloadable::Yes, [](float value) { return value > 0.0f && value <= 1.0f; }, "greater than 0 through 1");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootPercentScale, "GearBonus.PercentScale", 0.015f,
        Reloadable::Yes, [](float value) { return value > 0.0f && value <= 1.0f; }, "greater than 0 through 1");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootLeechScale, "GearBonus.LeechScale", 0.005f,
        Reloadable::Yes, [](float value) { return value > 0.0f && value <= 1.0f; }, "greater than 0 through 1");
    SetConfigValue<float>(StatGrowthConfigKey::PersonalLootMaxLeech, "GearBonus.MaxLeech", 25.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<bool>(StatGrowthConfigKey::SmartLootEnabled, "SmartLoot.Enabled", true);
    SetConfigValue<uint32>(StatGrowthConfigKey::SmartLootLevelWindow, "SmartLoot.LevelWindow", 5,
        Reloadable::Yes, [](uint32 value) { return value >= 2 && value <= 10; }, "2 through 10");
    SetConfigValue<uint32>(StatGrowthConfigKey::SmartLootCreatureLevelTolerance,
        "SmartLoot.CreatureLevelTolerance", 3, Reloadable::Yes,
        [](uint32 value) { return value <= 10; }, "0 through 10");
    SetConfigValue<float>(StatGrowthConfigKey::DropChance, "StatGrowth.DropChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::BonusPerUse, "StatGrowth.BonusPerUse", 1,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::ItemEntry, "StatGrowth.ItemEntry", 42590,
        Reloadable::No, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::ExperienceDropChance, "StatGrowth.ExperienceDropChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::ExperienceBonusPerUse, "StatGrowth.ExperienceBonusPerUse", 10,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::ExperienceItemEntry, "StatGrowth.ExperienceItemEntry", 39163,
        Reloadable::No, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::ResourceDropChance, "StatGrowth.ResourceDropChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::ResourceBonusPerUse, "StatGrowth.ResourceBonusPerUse", 1,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::ResourceItemEntry, "StatGrowth.ResourceItemEntry", 21238,
        Reloadable::No, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::VitalityDropChance, "StatGrowth.VitalityDropChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::VitalityBonusPerUse, "StatGrowth.VitalityBonusPerUse", 1,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::VitalityHealthPerLevel, "StatGrowth.VitalityHealthPerLevel", 0.5f,
        Reloadable::Yes, [](float value) { return value >= 0.0f; }, "zero or greater");
    SetConfigValue<uint32>(StatGrowthConfigKey::VitalityItemEntry, "StatGrowth.VitalityItemEntry", 41606,
        Reloadable::No, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::FortuneDropChance, "StatGrowth.FortuneDropChance", 10.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::FortuneBonusPerUse, "StatGrowth.FortuneBonusPerUse", 1,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::FortuneItemEntry, "StatGrowth.FortuneItemEntry", 23656,
        Reloadable::No, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::GreaterTierChance, "StatGrowth.GreaterTierChance", 18.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::AscendantTierChance, "StatGrowth.AscendantTierChance", 2.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<uint32>(StatGrowthConfigKey::GreaterTierMultiplier, "StatGrowth.GreaterTierMultiplier", 3,
        Reloadable::Yes, [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::AscendantTierMultiplier,
        "StatGrowth.AscendantTierMultiplier", 10, Reloadable::Yes,
        [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::MythicEssenceBaseline,
        "StatGrowth.MythicEssenceBaseline", 60, Reloadable::Yes,
        [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<float>(StatGrowthConfigKey::MythicEssenceBonusPerKeyLevel,
        "StatGrowth.MythicEssenceBonusPerKeyLevel", 10.0f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<bool>(StatGrowthConfigKey::ParagonEnabled, "Paragon.Enabled", true, Reloadable::Yes);
    SetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointCap, "Paragon.PointCap", 50, Reloadable::Yes,
        [](uint32 value) { return value > 0; }, "greater than zero");
    SetConfigValue<uint32>(StatGrowthConfigKey::ParagonPointsPerPrestige, "Paragon.PointsPerPrestige", 10,
        Reloadable::Yes, [](uint32) { return true; }, "zero or more");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonNormalChance, "Paragon.NormalBossChance", 3.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonHeroicChance, "Paragon.HeroicBossChance", 8.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonMythicZeroChance, "Paragon.MythicZeroBossChance", 15.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonMythicChance, "Paragon.MythicBossChance", 15.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonMythicChancePerLevel,
        "Paragon.MythicBossChancePerKeyLevel", 3.0f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonRaidChance, "Paragon.RaidBossChance", 25.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::ParagonRaidHeroicChance, "Paragon.RaidHeroicBossChance", 40.0f,
        Reloadable::Yes, [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::FortuneGearBonusChancePerPoint,
        "Fortune.GearBonusChancePerPoint", 0.5f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::FortuneGearBonusValuePerPoint,
        "Fortune.GearBonusValuePerPoint", 0.5f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 100.0f; }, "0 through 100");
    SetConfigValue<float>(StatGrowthConfigKey::FortuneMaxGearBonusValueIncrease,
        "Fortune.MaxGearBonusValueIncrease", 100.0f, Reloadable::Yes,
        [](float value) { return value >= 0.0f && value <= 1000.0f; }, "0 through 1000");
}
