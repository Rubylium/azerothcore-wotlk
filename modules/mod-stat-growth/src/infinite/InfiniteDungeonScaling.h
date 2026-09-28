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
// stops when its guardian falls; there is no fail timer. Its par is a minute, alone or as two: cleared in 30 seconds
// or less, the portal leads two floors down; in 20 seconds or less, three. The floors passed over still count:
// their checkpoint is kept (its chest stands beside the portal) and their gear piece given, while the gold and the
// essence chance stay one floor's.
constexpr uint32 ParMs = 60 * 1000;
constexpr uint32 TwoFloorsMs = 30 * 1000;           // InfiniteDungeon.lua TWO_FLOORS_SHARE (a share of the par)
constexpr uint32 ThreeFloorsMs = 20 * 1000;         // InfiniteDungeon.lua THREE_FLOORS_SHARE
constexpr uint32 MaxFloorsDown = 3;

// Where the portal of a floor cleared in `clearMs` leads: 1, 2 or 3 floors down
inline uint32 GetFloorsDown(uint32 clearMs)
{
    if (clearMs <= ThreeFloorsMs)
        return 3;
    if (clearMs <= TwoFloorsMs)
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

// The gearing ladder's loot follows the Mythic+ key of the same difficulty, ItemLevelKeysBelow keys under it: a step
// of the gear part (the first GearingGearSteps) stands for a key level (+0 to +10), and past it a step asks
// ParagonPerStep paragon where a key asks Mythic::ParagonPerLevel, so it stands for that share of a key. At any
// recommended paragon the Infinite Dungeon drops 8 item levels under the key that recommends it: Mythic+ stays the
// better gear for the harder content. Above the game's best items it is a generated variant (MythicDungeon.h).
constexpr uint32 ItemLevelKeysBelow = 2;

inline uint32 GetItemLevel(uint32 floor)
{
    uint32 const step = GetStep(floor);
    uint32 const gearSteps = std::min(step, GearingGearSteps);
    uint32 const paragonSteps = step - gearSteps;
    uint32 const itemLevel = Mythic::BaseItemLevel + Mythic::ItemLevelPerKeyLevel * gearSteps +
        Mythic::ItemLevelPerKeyLevel * ParagonPerStep * paragonSteps / Mythic::ParagonPerLevel;
    return itemLevel - Mythic::ItemLevelPerKeyLevel * ItemLevelKeysBelow;
}

// The gearing ladder's hits are measured on the Mythic+ yardstick, the health a player ready for the key of the same
// difficulty has (Mythic::GetDamageReference: a model of a real character's health at that key's gear and paragon,
// 66 000 at +10), ItemLevelKeysBelow keys under it as its loot. A step of the gear part stands for a key level, one
// past it for ParagonPerStep / ParagonPerLevel of one. The ordinary level-80 player the levelling ladder measures on
// (GetReferenceHealthFactor) has about a quarter of what a character deep in the ladder carries, and its hits grazed
// them. It used to grow with the creatures' health (Mythic::GetLevelScaling), which outruns any player's health:
// deep floors one-shot.
// A floor creature's melee swing at the level cap, as a share of that health (a tank taking it; lighter untanked)
constexpr float TrashMeleeShare = 0.025f;
constexpr float EliteMeleeShare = 0.05f;
constexpr float BossMeleeShare = 0.08f;

inline float GetKeyEquivalent(uint32 floor)
{
    uint32 const step = GetStep(floor);
    float const key = static_cast<float>(std::min(step, GearingGearSteps)) +
        static_cast<float>(step > GearingGearSteps ? step - GearingGearSteps : 0) * ParagonPerStep /
        static_cast<float>(Mythic::ParagonPerLevel);
    return std::max(0.0f, key - static_cast<float>(ItemLevelKeysBelow));
}

inline float GetGearingReferenceHealth(uint32 floor)
{
    return Mythic::GetDamageReference(GetKeyEquivalent(floor));
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

// Rewards. Experience comes from the kills, a normal kill's (the stock creature's own), through the essence boosts
// like any kill.
// Gold: 12 * level^2 copper (48 silver at 20, 7.7 gold at 80).
constexpr uint32 FloorGoldPerLevelSquared = 12;
constexpr uint32 FloorEssenceChance = 10;               // percent, one essence
constexpr uint32 CheckpointEssences = 2;                // and one more every 50 floors
// Paragon: one point at the level cap on every CheckpointParagonEvery-th checkpoint only (floors 30, 60, 90...)
constexpr uint32 CheckpointParagonPoints = 1;
constexpr uint32 CheckpointParagonEvery = 3;

// Healing hearts: the chance a creature leaves one, and what one gives back
constexpr uint32 TrashHeartChance = 15;
constexpr uint32 EliteHeartChance = 50;
constexpr uint32 HeartHealthPct = 45;
constexpr uint32 HeartManaPct = 30;
// Hearts also rise during the fight: every CombatHeartMinMs-CombatHeartMaxMs of combat on the floor one appears a few
// yards from the most hurt player (on the ground they stand on, in their sight, away from the guardians). The clock
// runs twice as fast while a player is under CombatHeartLowHealthPct. None rises while CombatHeartCap hearts already
// lie on the floor (kill drops count), and one that rose in the fight fades after CombatHeartLifetimeMs.
constexpr uint32 CombatHeartMinMs = 12 * 1000;
constexpr uint32 CombatHeartMaxMs = 20 * 1000;
constexpr float CombatHeartLowHealthPct = 50.0f;
constexpr uint32 CombatHeartLowHealthSpeed = 2;
constexpr std::size_t CombatHeartCap = 3;
constexpr uint32 CombatHeartLifetimeMs = 45 * 1000;

// Gear on every floor: each member's clear of any floor rolls FloorGearChance percent for one smart-loot piece (the
// floor's item level), on top of the sure piece of every GearFloors-th floor. InfiniteDungeon.lua and
// InfiniteDungeonKeeper.lua show the same numbers.
constexpr uint32 FloorGearChance = 20;

// A long, hard floor: cleared after more than HardFightMinMs (three pars) with the run's members having taken, on
// average, at least HardFightMinDamage of their maximum health from the floor's creatures (so a floor left to run
// out while nobody fights does not count), the gear chance becomes HardFightChance, plus HardFightChancePerMinute
// for each full minute past HardFightMinMs, up to HardFightMaxChance. Past HardFightSureMs with HardFightSureDamage
// taken, the piece is sure.
constexpr uint32 HardFightMinMs = ParMs * 3;
constexpr float HardFightMinDamage = 1.5f;
constexpr uint32 HardFightChance = 40;
constexpr uint32 HardFightChancePerMinute = 10;
constexpr uint32 HardFightMaxChance = 70;
constexpr uint32 HardFightSureMs = ParMs * 6;
constexpr float HardFightSureDamage = 3.0f;

// The gear chance a hard floor earns (0: the floor was not one). damageTaken: the members' average, in maximum healths.
inline uint32 GetHardFightChance(uint32 clearMs, float damageTaken)
{
    if (clearMs <= HardFightMinMs || damageTaken < HardFightMinDamage)
        return 0;
    if (clearMs >= HardFightSureMs && damageTaken >= HardFightSureDamage)
        return 100;
    uint32 const minutes = (clearMs - HardFightMinMs) / (60 * 1000);
    return std::min(HardFightMaxChance, HardFightChance + HardFightChancePerMinute * minutes);
}

// Floor variants: every floor is drawn a shape (never the one of the floor before). Each keeps about the same health
// budget as the standard floor, counted in a lone trash creature's health (standard: solo 2 trash + 1 trash + an elite
// (3) + the boss (10) = 16; duo 3 + 2 + 3 + 10 = 18) and about the same damage at any moment, so the par time (a
// minute, +2 in 30 s, +3 in 20 s) stays fair for all of them. The layouts are in InfiniteDungeonSystem.cpp
// (SetupFloor); InfiniteDungeon.lua names them.
enum class FloorVariant : uint8
{
    Standard = 0,   // two packs (the second led by an elite), the guardian
    Guardian = 1,   // the guardian alone, x1.6 health (16) and x1.15 damage
    Horde = 2,      // three packs of 3 (duo 4) trash at x0.65 health and damage (5.9 + 10; duo 7.8 + 10)
    ElitePair = 3,  // two elites (duo: each with one trash), then the guardian (3 + 3 + 10; duo 8 + 10)
    Gauntlet = 4,   // four small packs spread from the circle to the guardian, the third an elite (6 + 10; duo 8 + 10)
    Ambush = 5,     // one pack of 2, then the guardian; at 75, 50 and 25% of its health a wave of 1 (duo 2) trash
                    // joins the fight (2 + 3 + 10 = 15; duo 2 + 6 + 10 = 18)
    Twins = 6,      // one pack, two guardians at x0.7 health and damage each (2 + 14; duo 3 + 14)
    Treasure = 7,   // one pack and a guardian at x0.6 health (about half the floor): +TreasureGearBonus percent gear
                    // chance and double gold. Rare.
    Count
};

struct VariantTuning
{
    uint32 weight;              // how often it is drawn, against the others
    float trashHealth;
    float trashDamage;
    float eliteHealth;
    float eliteDamage;
    float bossHealth;
    float bossDamage;
};

constexpr VariantTuning VariantTunings[static_cast<std::size_t>(FloorVariant::Count)] = {
    { 30, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },     // Standard
    { 10, 1.0f, 1.0f, 1.0f, 1.0f, 1.6f, 1.15f },    // Guardian
    { 12, 0.65f, 0.65f, 1.0f, 1.0f, 1.0f, 1.0f },   // Horde
    { 12, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },     // ElitePair
    { 12, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },     // Gauntlet
    { 12, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },     // Ambush
    { 12, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f, 0.7f },     // Twins
    { 5, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f, 1.0f },      // Treasure
};

inline VariantTuning const& GetVariantTuning(FloorVariant variant)
{
    std::size_t const index = static_cast<std::size_t>(variant);
    return VariantTunings[index < static_cast<std::size_t>(FloorVariant::Count) ? index : 0];
}

// The ambush's waves join at these health shares of the guardian
constexpr uint32 AmbushWaves = 3;
constexpr float AmbushWaveHealthPct[AmbushWaves] = { 75.0f, 50.0f, 25.0f };
constexpr uint32 TreasureGearBonus = 40;
constexpr uint32 TreasureGoldFactor = 2;

// The gear chance of a clear, in percent: the base or the hard fight's, whichever is higher, and the treasure floor's
// bonus on top
inline uint32 GetFloorGearChance(FloorVariant variant, uint32 hardFightChance)
{
    uint32 const chance = std::max(FloorGearChance, hardFightChance) +
        (variant == FloorVariant::Treasure ? TreasureGearBonus : 0);
    return std::min<uint32>(chance, 100);
}
}

#endif
