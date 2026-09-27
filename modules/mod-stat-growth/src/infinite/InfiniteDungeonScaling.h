#ifndef MOD_STAT_GROWTH_INFINITE_DUNGEON_SCALING_H
#define MOD_STAT_GROWTH_INFINITE_DUNGEON_SCALING_H

#include "Define.h"
#include "MythicDungeon.h"

#include <algorithm>
#include <cmath>

// The Infinite Dungeon's numbers: how hard a floor is, and what it pays. A gearing system, not hardcore content: the
// curve is gentle and a run should feel rewarding. The client shows the same step and recommended paragon
// (InfiniteDungeon.lua); change them together.
namespace InfiniteDungeon
{
// Two ladders, each with its own checkpoint and record per character: the levelling ladder is climbed below the level
// cap, the gearing ladder at it. A run of a player at the cap with one below it climbs the levelling ladder.
enum class Ladder : uint8
{
    Levelling = 0,
    Gearing = 1
};

constexpr uint8 MinPlayerLevel = 15;
constexpr uint32 CheckpointFloors = 10;
constexpr uint32 GearFloors = 5;
constexpr uint32 StepFloors = 5;

// While levelling each floor adds 5% to the monsters' health and damage, up to +75% (floor 16 and deeper). The ladder
// is climbed for hundreds of floors while a character levels (the monsters follow its level), so an uncapped 5% a
// floor would make the deep floors impossible at any level; past the cap the gear the ladder gives makes it easier.
constexpr float LevellingFloorGrowth = 0.05f;
constexpr uint32 LevellingFloorCap = 15;

// At the level cap the difficulty climbs once every StepFloors floors: 5% a step (compounded) for the first
// GearingGearSteps steps, a matter of gear, then a matter of paragon: each step asks for ParagonPerStep more points,
// and the monsters grow by what those points and the step's better loot are worth - the Mythic+ curve
// (MythicDungeon.h), gentler: 5% instead of 8% a step, 3 paragon a step instead of 5. A character's own paragon never
// raises the difficulty.
constexpr float GearingStepGrowth = 1.05f;
constexpr uint32 GearingGearSteps = 10;
constexpr uint32 ParagonPerStep = 3;

inline uint32 GetStep(uint32 floor)
{
    return floor > 0 ? (floor - 1) / StepFloors : 0;
}

inline bool IsCheckpointFloor(uint32 floor)
{
    return floor > 0 && floor % CheckpointFloors == 0;
}

inline bool IsGearFloor(uint32 floor)
{
    return floor > 0 && floor % GearFloors == 0;
}

// Speed skips, the Mythic+ key upgrades' way. A floor's clock starts when the players step out of the bubble and
// stops when its guardian falls; there is no fail timer. Its par time is 20 seconds a foe (trash and elite) and a
// minute for the guardian, a quarter more for a duo (its foes have more health): about 2:20 alone, 3:45 as two.
// Cleared within 70% of it, the portal leads two floors down; within 45%, three. The floors passed over still count:
// their checkpoint is kept (its chest stands beside the portal) and their gear piece given, while the gold and the
// essence chance stay one floor's.
constexpr uint32 ParSecondsPerFoe = 20;
constexpr uint32 ParSecondsGuardian = 60;
constexpr uint32 DuoParPct = 125;
constexpr uint32 TwoFloorsParPct = 70;
constexpr uint32 ThreeFloorsParPct = 45;
constexpr uint32 MaxFloorsDown = 3;

inline uint32 GetParMs(uint32 foes, bool duo)
{
    uint32 const seconds = ParSecondsPerFoe * foes + ParSecondsGuardian;
    return seconds * 1000 * (duo ? DuoParPct : 100) / 100;
}

inline uint32 GetParShareMs(uint32 parMs, uint32 pct)
{
    return static_cast<uint32>(static_cast<uint64>(parMs) * pct / 100);
}

// Where the portal of a floor cleared in `clearMs` leads: 1, 2 or 3 floors down
inline uint32 GetFloorsDown(uint32 clearMs, uint32 parMs)
{
    if (!parMs)
        return 1;
    if (clearMs <= GetParShareMs(parMs, ThreeFloorsParPct))
        return 3;
    if (clearMs <= GetParShareMs(parMs, TwoFloorsParPct))
        return 2;
    return 1;
}

// The checkpoint a clear of `floor` reaches, the floor itself or one the portal passes over (the floor it leads to
// is played, and reached as usual); 0 when none. A jump covers at most three floors, so one checkpoint at most.
inline uint32 GetCheckpointReached(uint32 floor, uint32 floorsDown)
{
    for (uint32 passed = floor; passed < floor + floorsDown; ++passed)
        if (IsCheckpointFloor(passed))
            return passed;
    return 0;
}

inline uint32 GetRecommendedParagon(Ladder ladder, uint32 floor)
{
    uint32 const step = GetStep(floor);
    return ladder == Ladder::Gearing && step > GearingGearSteps ? ParagonPerStep * (step - GearingGearSteps) : 0;
}

inline float GetFloorScaling(Ladder ladder, uint32 floor)
{
    if (ladder == Ladder::Levelling)
        return 1.0f + LevellingFloorGrowth * static_cast<float>(std::min(floor > 0 ? floor - 1 : 0, LevellingFloorCap));

    uint32 const step = GetStep(floor);
    float const gear = std::pow(GearingStepGrowth, static_cast<float>(std::min(step, GearingGearSteps)));
    float const paragon = std::pow(Mythic::ParagonPointPower,
        static_cast<float>(GetRecommendedParagon(ladder, floor)));
    float const loot = std::pow(Mythic::KeyGearGrowth,
        static_cast<float>(step > GearingGearSteps ? step - GearingGearSteps : 0));
    return gear * paragon * loot;
}

// The gearing ladder's loot: a heroic's item level (200) on the first step, 4 more each step, without end (above the
// game's best items it is a generated variant, see MythicDungeon.h)
constexpr uint32 GearingBaseItemLevel = 200;

inline uint32 GetItemLevel(uint32 floor)
{
    return GearingBaseItemLevel + Mythic::ItemLevelPerKeyLevel * GetStep(floor);
}

// Roles: the monsters' health follows the damage the run can deal (a damage dealer in full, a tank for about 60%, a
// healer for about 40%), their damage who takes the hits (a tank: real tank damage; without one, lower, the hearts
// being the healing)
constexpr float DamageDealerWeight = 1.0f;
constexpr float TankWeight = 0.6f;
constexpr float HealerWeight = 0.4f;
constexpr float TankedDamage = 1.0f;
constexpr float UntankedDuoDamage = 0.6f;
constexpr float UntankedSoloDamage = 0.5f;

// The expansion column of the creature base stats a level is read from: its own era's
inline uint8 GetStatsExpansion(uint8 level)
{
    return level >= 70 ? 2 : level >= 60 ? 1 : 0;
}

// Health and weapon damage of a floor's creature for one damage dealer, on top of the base stats of its level:
// - health: about seven seconds of a solo damage dealer's damage for trash, three times that for an elite, ten for a
//   boss. WotLK gear outgrows the base stats from 70 on, hence the level factor.
// - damage: a dungeon elite's (the modifier WotLK and older templates carry), trash lighter, bosses heavier.
constexpr float TrashHealth[3] = { 0.8f, 1.0f, 1.2f };
constexpr float EliteDamage[3] = { 2.0f, 3.0f, 6.0f };
constexpr float EliteHealthRank = 3.0f;
constexpr float BossHealthRank = 10.0f;
constexpr float TrashDamageRank = 0.6f;
constexpr float BossDamageRank = 1.5f;

inline float GetLevelHealthFactor(uint8 level)
{
    return 1.0f + 1.5f * std::clamp((static_cast<float>(level) - 70.0f) / 10.0f, 0.0f, 1.0f);
}

// What a player of that level has for health in ordinary gear, as a share of a warrior creature's base health: the
// yardstick of the telegraphed abilities (a share of it, whatever the level)
inline float GetReferenceHealthFactor(uint8 level)
{
    if (level < 60)
        return 1.4f;
    if (level < 70)
        return 1.2f;
    return 1.3f + 0.05f * static_cast<float>(std::min<uint8>(level, 80) - 70);
}

// Rewards. Experience comes from the kills (the floor creatures' ExperienceModifier, stat_growth_infinite_dungeon_
// creatures.sql: trash 4, elite 3 and boss 8, twice again as elites), so it goes through the boosts like any kill.
// Gold: 12 * level^2 copper (48 silver at 20, 7.7 gold at 80).
constexpr uint32 FloorGoldPerLevelSquared = 12;
constexpr uint32 FloorEssenceChance = 10;               // percent, one essence
constexpr uint32 CheckpointEssences = 2;                // and one more every 50 floors
constexpr uint32 CheckpointParagonPoints = 1;           // at the level cap, and one more every 50 floors

// Healing hearts: the chance a creature leaves one, and what one gives back
constexpr uint32 TrashHeartChance = 15;
constexpr uint32 EliteHeartChance = 50;
constexpr uint32 HeartHealthPct = 45;
constexpr uint32 HeartManaPct = 30;
}

#endif
