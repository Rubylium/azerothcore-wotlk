// The module exports its include directory when enabled; core-only builds omit these tests.
#if __has_include("InfiniteDungeonScaling.h")
#include "InfiniteDungeonScaling.h"
#include "gtest/gtest.h"

#include <limits>

TEST(InfiniteDungeonProgression, EntryGearReachesCapAtFloorOneHundred)
{
    EXPECT_EQ(InfiniteDungeon::GetItemLevel(0), 200u);
    EXPECT_EQ(InfiniteDungeon::GetItemLevel(1), 200u);
    EXPECT_LT(InfiniteDungeon::GetItemLevel(99), 310u);
    EXPECT_EQ(InfiniteDungeon::GetItemLevel(100), 310u);
    EXPECT_EQ(InfiniteDungeon::GetItemLevel(std::numeric_limits<uint32>::max()), 310u);
    for (uint32 floor = 2; floor <= 100; ++floor)
        EXPECT_GE(InfiniteDungeon::GetItemLevel(floor), InfiniteDungeon::GetItemLevel(floor - 1));
}

TEST(InfiniteDungeonProgression, FreshCharacterBaselineNeverRequiresParagon)
{
    auto const ladder = InfiniteDungeon::Ladder::Gearing;
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetGearingReferenceHealth(1), 12000.0f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1), 0.65f);
    EXPECT_LT(InfiniteDungeon::GetGearingReferenceHealth(1), Mythic::GetDamageReference(0.0f));
    for (uint32 floor : { 1u, 50u, 100u, 101u, 1000u, std::numeric_limits<uint32>::max() })
        EXPECT_EQ(InfiniteDungeon::GetRecommendedParagon(ladder, floor), 0u);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1000),
        InfiniteDungeon::GetFloorScaling(ladder, 100));
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetGearingReferenceHealth(1000),
        InfiniteDungeon::GetGearingReferenceHealth(100));
}

TEST(InfiniteDungeonProgression, LevellingRequiresNoGearOrEssenceScaling)
{
    auto const ladder = InfiniteDungeon::Ladder::Levelling;
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1), 1.0f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 21), 1.2f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1000), 1.2f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorHealthScaling(ladder, 1), 0.6f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorHealthScaling(ladder, 21), 0.72f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorDamageScaling(ladder, 1), 0.45f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorDamageScaling(ladder, 21), 0.54f);
    for (uint8 level : { 15, 60, 70, 79, 80 })
        EXPECT_FLOAT_EQ(InfiniteDungeon::GetLevelHealthFactor(ladder, level), 1.0f);
}

TEST(InfiniteDungeonProgression, GearingKeepsItsExistingLevelEightyBudget)
{
    auto const ladder = InfiniteDungeon::Ladder::Gearing;
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorHealthScaling(ladder, 1), 0.65f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorDamageScaling(ladder, 1), 1.0f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorHealthScaling(ladder, 100),
        InfiniteDungeon::GetFloorScaling(ladder, 100));
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorDamageScaling(ladder, 100), 1.0f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetLevelHealthFactor(ladder, 80), 2.5f);
}
#endif
