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

TEST(InfiniteDungeonProgression, LevellingCurveIsUnchanged)
{
    auto const ladder = InfiniteDungeon::Ladder::Levelling;
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1), 1.0f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 16), 1.75f);
    EXPECT_FLOAT_EQ(InfiniteDungeon::GetFloorScaling(ladder, 1000), 1.75f);
}
#endif
