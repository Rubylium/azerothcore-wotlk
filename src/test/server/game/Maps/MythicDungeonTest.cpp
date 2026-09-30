#include "MythicDungeon.h"
#include "gtest/gtest.h"

#include <limits>

TEST(MythicProgression, LootReachesCapOnlyAtKeySixty)
{
    EXPECT_EQ(Mythic::GetItemLevel(-1), 223u);
    EXPECT_EQ(Mythic::GetItemLevel(0), 223u);
    EXPECT_LT(Mythic::GetItemLevel(59), 370u);
    EXPECT_EQ(Mythic::GetItemLevel(60), 370u);
    EXPECT_EQ(Mythic::GetItemLevel(99), 370u);
    EXPECT_EQ(Mythic::GetItemLevel(std::numeric_limits<int32>::max()), 370u);
    for (int32 key = 1; key <= 99; ++key)
        EXPECT_GE(Mythic::GetItemLevel(key), Mythic::GetItemLevel(key - 1));
}

TEST(MythicProgression, GeneratedLootHonorsExactCaps)
{
    for (uint32 cap : { 310u, 370u, 460u })
    {
        EXPECT_EQ(Mythic::GetGeneratedItemLevel(Mythic::GetGeneratedVariant(cap)), cap);
        EXPECT_LT(Mythic::GetGeneratedItemLevel(Mythic::GetGeneratedVariant(cap - 1)), cap);
    }
    for (uint32 requested = 285; requested <= 800; ++requested)
    {
        uint32 const variant = Mythic::GetGeneratedVariant(requested);
        EXPECT_TRUE(variant < Mythic::GeneratedItemVariants ||
            (variant >= Mythic::FirstCapVariant && variant < Mythic::FirstCapVariant + Mythic::CapVariants));
        EXPECT_LE(Mythic::GetGeneratedItemLevel(variant), std::min(requested, 460u));
    }
}

TEST(MythicProgression, ExistingTemplatesAndForgeBlocksRemainUnchanged)
{
    for (uint32 variant = 0; variant < 128; ++variant)
        EXPECT_EQ(Mythic::GetGeneratedItemLevel(variant), 285u + 4u * variant);
    for (uint32 rank = 1; rank <= 8; ++rank)
    {
        uint32 const entry = Mythic::GetForgeItemEntry(50730, rank);
        EXPECT_EQ(entry, (128u + rank) * 65536u + 50730u);
        EXPECT_EQ(Mythic::GetForgeRank(entry), rank);
        EXPECT_FALSE(Mythic::IsMythicGeneratedItem(entry));
    }
    for (uint32 variant = Mythic::FirstCapVariant; variant < Mythic::FirstCapVariant + Mythic::CapVariants; ++variant)
    {
        uint32 const entry = Mythic::GetGeneratedItemEntry(50730, variant);
        EXPECT_TRUE(Mythic::IsMythicGeneratedItem(entry));
        EXPECT_EQ(Mythic::GetForgeRank(entry), 0u);
        EXPECT_EQ(Mythic::GetBaseItemEntry(entry), 50730u);
    }
}

TEST(MythicProgression, BonusKeysKeepGettingHarderWithoutBetterGear)
{
    EXPECT_FLOAT_EQ(Mythic::GetExpectedItemLevel(61.0f), 370.0f);
    EXPECT_FLOAT_EQ(Mythic::GetExpectedItemLevel(99.0f), 370.0f);
    EXPECT_GT(Mythic::GetLevelScaling(99), Mythic::GetLevelScaling(60));
    EXPECT_GT(Mythic::GetDamageReference(99.0f), Mythic::GetDamageReference(60.0f));
    EXPECT_EQ(Mythic::GetRecommendedParagon(99), 445u);
    EXPECT_EQ(Mythic::GetRecommendedParagon(1000), 445u);
    EXPECT_FLOAT_EQ(Mythic::GetLevelScaling(1000), Mythic::GetLevelScaling(99));
    EXPECT_FLOAT_EQ(Mythic::GetDamageReference(1000.0f), Mythic::GetDamageReference(99.0f));
}
