#include "MythicDungeon.h"
#include "PowerScaling.h"
#include "gtest/gtest.h"

TEST(PowerScaling, GeneratedGearGrowsLinearlyAndRatingsSlower)
{
    EXPECT_FLOAT_EQ(Power::StatGrowth(284.0f, 284.0f), 1.0f);
    EXPECT_NEAR(Power::StatGrowth(284.0f, 426.0f), 1.5f, 1e-5f);
    EXPECT_NEAR(Power::StatGrowth(284.0f, 426.0f, true), std::sqrt(1.5f), 1e-5f);
    EXPECT_TRUE(Power::IsRatingStat(32));   // crit rating
    EXPECT_TRUE(Power::IsRatingStat(36));   // haste rating
    EXPECT_TRUE(Power::IsRatingStat(44));   // armour penetration rating
    EXPECT_FALSE(Power::IsRatingStat(7));   // stamina
    EXPECT_FALSE(Power::IsRatingStat(38));  // attack power
    EXPECT_FALSE(Power::IsRatingStat(45));  // spell power
}

TEST(PowerScaling, DpsIndexIsOneAtReferenceAndClimbs)
{
    EXPECT_FLOAT_EQ(Power::DpsIndex(Power::ReferenceItemLevel), 1.0f);
    for (auto const& point : Power::DpsCurve)
        EXPECT_FLOAT_EQ(Power::DpsIndex(point.itemLevel), point.index);
    for (float itemLevel = 200.0f; itemLevel < 800.0f; itemLevel += 1.0f)
        EXPECT_GT(Power::DpsIndex(itemLevel + 1.0f), Power::DpsIndex(itemLevel));
}

TEST(PowerScaling, ParagonCompoundsAndPowerIndexCombines)
{
    EXPECT_FLOAT_EQ(Power::ParagonDpsIndex(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(Power::ParagonDpsIndex(-5.0f), 1.0f);
    EXPECT_NEAR(Power::ParagonDpsIndex(100.0f), std::pow(Power::ParagonDpsPerPoint, 100.0f), 1e-4f);
    EXPECT_FLOAT_EQ(Power::PowerIndex(370.0f, 100.0f), Power::DpsIndex(370.0f) * Power::ParagonDpsIndex(100.0f));
    EXPECT_FLOAT_EQ(Power::DpsCheckHealth(284.0f, 0.0f, 3.0f, 60.0f), Power::ReferenceSingleTargetDps * 3.0f * 60.0f);
}

TEST(PowerScaling, GearStaminaIsContinuousAtTheGamesBestItems)
{
    EXPECT_NEAR(Power::GearStamina(284.0f), Power::GearStamina(284.001f), 0.1f);
    EXPECT_NEAR(Power::GearStamina(568.0f), 2.0f * Power::GearStamina(284.0f), 0.1f);
}

TEST(PowerScaling, MythicKeysUseThePowerModel)
{
    // A key's player is the power model's player at that key's gear, paragon and progress
    for (float key : { 0.0f, 10.0f, 30.0f, 52.0f, 99.0f })
        EXPECT_FLOAT_EQ(Mythic::GetExpectedPlayerHealth(key),
            Power::ExpectedPlayerHealth(Mythic::GetExpectedItemLevel(key),
                float(Mythic::GetRecommendedParagon(int32(key))), key));
    // Past +10 creature health grows by the power the key asks for over +10's
    float const atTen = Power::PowerIndex(Mythic::GetExpectedItemLevel(10.0f), 0.0f);
    float const atThirty = Power::PowerIndex(Mythic::GetExpectedItemLevel(30.0f),
        float(Mythic::GetRecommendedParagon(30)));
    EXPECT_NEAR(Mythic::GetLevelScaling(30) / Mythic::GetLevelScaling(10), atThirty / atTen, 1e-3f);
}
