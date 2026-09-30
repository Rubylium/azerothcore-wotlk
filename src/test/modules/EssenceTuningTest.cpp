#if __has_include("EssenceTuning.h")
#include "EssenceTuning.h"
#include "gtest/gtest.h"

#include <limits>

TEST(EssenceTuning, ResourceCapIncludesEquipmentAndMirroredBonuses)
{
    EXPECT_EQ(EssenceTuning::CombinedBonus(450, 40, 100, EssenceTuning::MaxResourceBonus), 500u);
    EXPECT_EQ(EssenceTuning::CombinedBonus(400, 40, 30, EssenceTuning::MaxResourceBonus), 470u);
    uint32 const maximum = std::numeric_limits<uint32>::max();
    EXPECT_EQ(EssenceTuning::CombinedBonus(maximum, maximum, maximum, EssenceTuning::MaxResourceBonus), 500u);
}

TEST(EssenceTuning, FinalGrantFitsRemainingRoomAndOldTotalsCannotGrow)
{
    EXPECT_EQ(EssenceTuning::GrantAmount(498, 10, EssenceTuning::MaxResourceBonus), 2u);
    EXPECT_EQ(EssenceTuning::GrantAmount(500, 10, EssenceTuning::MaxResourceBonus), 0u);
    EXPECT_EQ(EssenceTuning::GrantAmount(10000, 10, EssenceTuning::MaxResourceBonus), 0u);
    EXPECT_EQ(EssenceTuning::GrantAmount(99, 10, EssenceTuning::MaxFortuneBonus), 1u);
}

TEST(EssenceTuning, CapsExcludeOnlyTheSaturatedFamilies)
{
    EXPECT_FALSE(EssenceTuning::IsFamilyCapped(EssenceFamily::Resource, 499, 100, 0));
    EXPECT_TRUE(EssenceTuning::IsFamilyCapped(EssenceFamily::Resource, 500, 100, 0));
    EXPECT_FALSE(EssenceTuning::IsFamilyCapped(EssenceFamily::Fortune, 500, 99, 0));
    EXPECT_TRUE(EssenceTuning::IsFamilyCapped(EssenceFamily::Fortune, 500, 100, 0));
    EXPECT_FALSE(EssenceTuning::IsFamilyCapped(EssenceFamily::Experience, 0, 0, 199));
    EXPECT_TRUE(EssenceTuning::IsFamilyCapped(EssenceFamily::Experience, 0, 0, 200));
    for (EssenceFamily family : { EssenceFamily::Growth, EssenceFamily::Vitality })
        EXPECT_FALSE(EssenceTuning::IsFamilyCapped(family, 10000, 10000, 10000));
}

TEST(EssenceTuning, GrowthAndVitalityDiminishTowardsTheirCeiling)
{
    EXPECT_EQ(EssenceTuning::Diminished(0, EssenceTuning::GrowthCeiling), 0u);
    EXPECT_EQ(EssenceTuning::Diminished(10, EssenceTuning::GrowthCeiling), 10u);   // early points count in full
    EXPECT_NEAR(EssenceTuning::Diminished(1175, EssenceTuning::GrowthCeiling), 515.0, 1.0);
    EXPECT_NEAR(EssenceTuning::Diminished(3510, EssenceTuning::VitalityCeiling), 1175.0, 1.0);
    EXPECT_LT(EssenceTuning::Diminished(1000000, EssenceTuning::VitalityCeiling), 1251u);
    for (uint32 points = 0; points < 5000; points += 50)
        EXPECT_LE(EssenceTuning::Diminished(points, EssenceTuning::GrowthCeiling),
            EssenceTuning::Diminished(points + 50, EssenceTuning::GrowthCeiling));
}

TEST(EssenceTuning, FortuneCannotBypassEconomyLimitsThroughGearOrConfiguration)
{
    EXPECT_EQ(EssenceTuning::CombinedBonus(95, 20, 0, EssenceTuning::MaxFortuneBonus), 100u);
    EXPECT_EQ(EssenceTuning::ItemFortune(12), 2u);
    EXPECT_EQ(EssenceTuning::ItemFortune(1), 1u);
    EXPECT_FLOAT_EQ(EssenceTuning::AffixChanceBonus(100, 0.5f), 10.0f);
    EXPECT_FLOAT_EQ(EssenceTuning::AffixValueBonus(100, 0.5f, 100.0f), 10.0f);
    EXPECT_FLOAT_EQ(EssenceTuning::AffixValueBonus(100, 0.5f, 5.0f), 5.0f);
    EXPECT_FLOAT_EQ(EssenceTuning::AffixChanceBonus(50, 0.1f), 5.0f);
}
#endif
