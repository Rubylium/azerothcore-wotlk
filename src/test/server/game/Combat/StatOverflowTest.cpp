#include "StatOverflow.h"
#include "gtest/gtest.h"

TEST(StatOverflow, NothingBelowTheCaps)
{
    EXPECT_FLOAT_EQ(StatOverflow::Excess(99.0f, 100.0f), 0.0f);
    EXPECT_FLOAT_EQ(StatOverflow::CriticalBonusPct(100.0f, 0.4f), 0.0f);
    EXPECT_FLOAT_EQ(StatOverflow::PrecisionBonusPct(-3.0f, 0.4f), 0.0f);
    EXPECT_FLOAT_EQ(StatOverflow::RobustnessReductionPct(0.0f, 0.3f, 20.0f), 0.0f);
    EXPECT_FLOAT_EQ(StatOverflow::ArmorPoints(74.9f), 0.0f);
}

TEST(StatOverflow, CritPastTheCapGrowsTheCriticalBonus)
{
    // 281% melee crit at 0.4: the bonus 72.4% larger, a melee crit x2.724 instead of x2
    EXPECT_NEAR(StatOverflow::CriticalBonusPct(281.0f, 0.4f), 72.4f, 1e-3f);
    // 165% spell crit: a spell crit x1.5 becomes x1.63
    EXPECT_NEAR(1.0f + 0.5f * (1.0f + StatOverflow::CriticalBonusPct(165.0f, 0.4f) / 100.0f), 1.63f, 1e-3f);
}

TEST(StatOverflow, RobustnessIsCapped)
{
    EXPECT_NEAR(StatOverflow::RobustnessReductionPct(10.0f, 0.3f, 20.0f), 3.0f, 1e-4f);
    EXPECT_FLOAT_EQ(StatOverflow::RobustnessReductionPct(500.0f, 0.3f, 20.0f), 20.0f);
}

TEST(StatOverflow, ArmorMatchesTheGameFormula)
{
    // Against a level-83 attacker the cap (75%) is reached at 3 x 16 635 armour
    EXPECT_NEAR(StatOverflow::ArmorReductionPct(16635.0f, 83), 50.0f, 1e-3f);
    EXPECT_NEAR(StatOverflow::ArmorReductionPct(3.0f * 16635.0f, 83), 75.0f, 1e-3f);
    // Three times the cap's armour: 90% uncapped, a tenth let through instead of a quarter, 60 points
    float const tripled = StatOverflow::ArmorReductionPct(9.0f * 16635.0f, 83);
    EXPECT_NEAR(tripled, 90.0f, 1e-3f);
    EXPECT_NEAR(StatOverflow::ArmorPoints(tripled), 60.0f, 1e-2f);
}
