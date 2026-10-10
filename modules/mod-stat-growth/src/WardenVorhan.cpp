#include "EvolutionsAudio.h"
#include "FightMusic.h"
#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"

#include "CellImpl.h"
#include "Chat.h"
#include "Containers.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "InstanceScript.h"
#include "LFG.h"
#include "LiveTuning.h"
#include "Log.h"
#include "Map.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PowerScaling.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// Gardien-chef Vorhan, the head warden of la Geôle des Flammes infernales: a Défi board boss in Magtheridon's Lair,
// fought the way a FFXIV Extreme trial is (plan: .agents/plans/warden-vorhan). 8 players - 2 tanks, 2 healers, 4 damage
// dealers - tuned for item level 477 and 650 paragon (the DPS check): the raid after the Hollow Voice, in what it
// pays. About 5:50, the hard enrage at 5:50.
//
// He does not fight to win: he applies the rules, and every rule broken is a sentence. The same script every pull,
// each rule read on his cast bar and on the players' debuffs, solved by assignment:
// - The rules that need them draw inmate numbers, 1-8, shown only while they run (Matricule n: a debuff, and a number
//   over the head): a player's cell, their Roll Call mark (pairs 1-2, 3-4, 5-6, 7-8). The shackles pair players at
//   random, their chain showing who is bound to whom.
// - Sentence: raid-wide, the healers' rhythm.
// - Mise à l'isolement: a heavy blow on his tank that never kills, throwing it far away in solitary (Isolement): 3 s
//   later its seal bursts on everyone, lethal within 20 yd, 80% just past it. The other tank takes him: a forced
//   tank swap.
// - Cellules: eight cells round the room at its clock points (cell n at the n-th, north first, clockwise); each player
//   walks into their own - a distance check, no collision. When the doors slam: an empty cell is an escape (everyone
//   hit, and Évasion: +25% damage taken), two in one cell both die, out of every cell the mark.
// - Menottes: a chain binds players two by two; they have to get 20 yd apart to break it, or it ramps until it
//   kills.
// - Regard du geôlier: an eye opens on him for 6 s; anyone facing him when it opens dies, thrown back.
// - Appel nominal: he stands still and calls the roll; four marks round him read 1-2, 3-4, 5-6, 7-8 (their order
//   changes each call, the same sequence every pull). Alone on a mark, more than two, or on another number's: death.
// - Charge du geôlier: electrified walls rise at the room's edge; 4 s later he throws everyone back 20 yd. From under
//   him it lands short of the walls; farther out, into them: death.
// - Couvre-feu: anyone still moving when it sounds is struck, stunned and marked.
// - Mutinerie (the intermission): the cell doors fail and waves of prisoners pour in on the tanks.
// - Phase 3, "Exécution des peines" (from 3:42), three rules of its own, then each with an old one:
//   - Exécution des peines: eight numbered seats round him; he calls them in an order drawn each time (and always
//     solvable), one a DOOM every 4 s (a clock's tick each second), the last DOOM two at once. Each seat called fires
//     a double cone of 40 degrees along its axis: its number stands on it, everyone else out of both cones and away
//     from his feet; a strike puts a player under Sursis (30 s), a second under it is death. Each cone's floor burns
//     from 6 yd out for the next two DOOMs: the group reads the next seat and moves to the gap that leaves, crossing
//     through the ring at his feet. The curfew sounds with the fourth DOOM.
//   - Mise au cachot: a player who is not a tank is caged for 8 s; the group breaks the cage (a few seconds of its
//     damage, the skull on it for the bots) or the prisoner dies.
//   - Châtiment exemplaire: a cone of its own look on his tank, 4 s: nobody in it is everyone's death, one alone dies
//     and everyone is hit, two or more share it (two tanks live through it, anyone else does not).
//   - His axe grows wilder through the phase: three lines that split, then lines splitting in four with a blade that
//     sweeps a third of the room, all of it faster in Perpétuité.
// - Perpétuité: a Sentence every 4 s, each heavier, until the hard enrage, Peine capitale.
//
// A personal mistake (the gaze faced, moving at curfew, out of a cell, a seal's burst) hits hard and marks (Marque du
// geôlier: +50% damage taken for 15 s); a mistake while marked is the whole hit, which kills. Every hit is a spell of
// its own whose description says what went wrong (localTools/wardenVorhan/Spells.ps1). Bots are told where to stand
// before each rule (GroundIndicators spots), never left to react.
//
// Only in a challenge's instance: the static spawn hides at once and goes from any other Magtheridon's Lair
// (IsChallengeInstanceFor, mod-playerbots RaidFinder.cpp), and clears the lair of Magtheridon and his channelers.

// mod-playerbots (RaidFinder.cpp), built into the same modules library
bool IsChallengeInstanceFor(Map const* map, uint32 bossEntry);
float GetChallengeDamageFactorOf(Unit* attacker);

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// The profile it is made for (mod-playerbots ChallengeTiers.h BossProfiles): hits are shares of the health of a
// damage dealer at it (Power::ExpectedPlayerHealth), the Défi tier's damage factor on top
constexpr float ProfileItemLevel = 477.0f;
constexpr float ProfileParagon = 650.0f;
// The group it is made for: 4 damage dealers, 2 tanks, 2 healers (Power::GroupDamageDealers)
constexpr float GroupDamage = 4.0f;
constexpr float GroupTanks = 2.0f;
constexpr float GroupHealers = 2.0f;
// The DPS check: the seconds the warden can be hit (the fight but the riot), against what a raid group of the profile
// deals there, measured on the simulation bench (Power::RaidBossHealth: the share a group playing well gives his
// health, the rest goes to the rules). It replaced the model's check and a knob raised twice by play (x1.44,
// 2026-10-08) on kills by players past his profile then (450 / 600). The deaths log each player's damage
// (LogSummary). The fight but the riot: 350 s less its 30.
constexpr float UptimeSeconds = 320.0f;
// His group of 8 gets fewer buffs than the 10-player teams Power::RaidCurve was measured on (two healers, no priest,
// two subgroups): 1.095 of the model against 1.18 at 477 / 650 (simBench.ps1 raid -dealers 4, 2026-10-09)
constexpr float EightPlayerShare = 1.095f / 1.18f;
// Raised by play: the first kills at the measured health were too easy (2026-10-09, +15%)
LiveTuning::Knob const HealthScale("vorhan.health_scale", 1.15f);
// The riot's waves: each about this many seconds of the group's pack damage
constexpr float WaveSeconds = 8.0f;

constexpr float MeleeFloorPct = 12.0f;          // his melee on a player: at least this, whatever their armour
constexpr float PrisonerMeleePct = 3.0f;        // a fel orc prisoner's swing
constexpr float AbyssalMeleePct = 5.0f;         // an abyssal's
constexpr float SentencePct[2] = { 45.0f, 60.0f };     // phase 1, phase 2
constexpr float RiotSentencePct = 27.0f;        // during the riot
constexpr float LifeSentencePct = 50.0f;        // Perpétuité's first, each one LifeSentenceGrowth heavier
constexpr float LifeSentenceGrowth = 1.2f;
constexpr float IsolationPct = 120.0f;          // on his tank (a tank has about 1.45 times this health): never lethal
// The seal's burst on everyone else, by their distance from the isolated tank - he carries a bomb: death within
// SealAvoidable (its lethal ring), most of one's health just past it, falling to SealFarPct at SealFar (the room's far
// side) and beyond. Raised by play (2026-10-09): a tank 17-20 yd away, a shield on, read as no damage at all.
constexpr float SealNearPct = 250.0f;
constexpr float SealEdgePct = 80.0f;
constexpr float SealFarPct = 30.0f;
constexpr float SealFar = 45.0f;
constexpr float SealAvoidable = 20.0f;
constexpr float EscapePct = 60.0f;              // an empty cell: everyone
constexpr float EscapeTakenPct = 25.0f;         // ... and Évasion, damage taken a stack
constexpr uint32 EscapeMs = 10000;
constexpr float OutOfCellPct = 80.0f;
// Coup de hache: between his rules, his axe splits the floor along a line - out of it before it lands. The lines
// grow wilder as the fight goes (AxeVolley): one, two, three at once, then one that splits where it lands into four
// more, each with its own warning. A blow (with what it splits into) needs AxeWindowMs clear of the next rule.
constexpr uint32 AxeWarnMs = 2000;              // the cast: its warning
constexpr uint32 AxeLingerMs = 900;             // the blow shown where it landed
constexpr uint32 AxeEveryMs = 6500;             // from a blow to the next, at the soonest
constexpr uint32 AxeWindowMs = 6000;
constexpr float AxeWidth = 5.0f;
// The split: as a line lands, it opens like shears - one swings to its left, one to its right, still from his feet,
// AxeSwingDegrees each way in AxeSwingMs (from AxeSwingStartMs after it lands), stops, and blows AxeSplitMs after the
// line landed. In Perpétuité it opens in four (half and all of AxeSwingDegrees each way).
constexpr float AxeSwingDegrees = 45.0f;
constexpr uint32 AxeSwingStartMs = 250;
constexpr uint32 AxeSwingMs = 1100;
constexpr uint32 AxeSplitMs = 2000;
constexpr uint32 SPELL_AXE_SWING = 94245;       // shapes.json VW_AxeSwing: the whole line on one model, 40 yards
constexpr uint32 SPELL_AXE_SWING_HIT = 94246;
constexpr float AxePct = 65.0f;
// A cell's cage stands this long past the doors' slam
constexpr uint32 CageAfterDoorsMs = 3000;
// Rancœur du détenu: who stood within this of his reach as a rule sent everyone away, and for how long it lasts
constexpr float RallyReach = 6.0f;
constexpr uint32 RallyMs = 10000;
constexpr std::array<float, 6> ChainRampPct = { 6.0f, 12.0f, 24.0f, 48.0f, 96.0f, 192.0f };
constexpr float CurfewPct = 100.0f;

// --- Phase 3 ---------------------------------------------------------------------------------------------------------
// Exécution des peines: eight numbered seats round him at the clock (seat n at (n - 1) x 45 degrees from north), a
// DOOM every ExecutionEveryMs (three ticks of the clock, then the DOOM as its cones strike). Each DOOM calls seats -
// one at a time, two at once on the last - in an order drawn each time among those that can be solved (every seat
// called off the burning floors, the group a gap of ExecutionMinGapDegrees at every DOOM). A seat called fires a
// double cone along its axis (through it and the seat opposite); its number must stand on it (spared by its own
// cones), everyone else out of them; anyone at his feet as it strikes is struck too. Each cone's floor burns from
// ExecutionSafe out for the next two DOOMs (BurntLastsMs): the ring at his feet is the way across, never a place to
// stand. The curfew sounds with the fourth DOOM.
constexpr uint32 ExecutionCastMs = 4000;
constexpr uint32 ExecutionEveryMs = 4000;
constexpr std::array<uint8, 7> ExecutionDooms = { 1, 1, 1, 1, 1, 1, 2 };   // the seats called at each DOOM
constexpr uint8 ExecutionCurfewDoom = 3;        // its index: the curfew's bell with that DOOM
constexpr float ExecutionArcDegrees = 40.0f;    // VW_BurntFloor's painting (the warning is the stock 45)
constexpr float ExecutionReach = 40.0f;
constexpr int32 ExecutionMinGapDegrees = 15;
constexpr int32 ExecutionGroupGapDegrees = 30;  // a gap the whole group stands in comfortably (bots: where it is sent)
constexpr float ExecutionSafe = 6.0f;           // his feet: no floor burns there, the DOOM strikes there
constexpr uint32 ExecutionFeetWarnMs = 1500;
constexpr float ExecutionSeatDistance = 10.0f;
constexpr float ExecutionSeatRadius = 2.5f;
constexpr float ExecutionOffSeatPct = 80.0f;    // a number off its seat as it is executed
constexpr float ExecutionGroupDistance = 12.0f; // bots: the group, in its gap
// The seat called, made plain: its runes flare again on every tick of the clock until it strikes, its number hangs
// over it as the inmate numbers hang over heads, SeatNumberScale times their size, and it is announced in the middle
// of every player's screen (its number's player told apart)
constexpr float SeatNumberScale = 2.5f;
constexpr uint32 SeatFlareEveryMs = 1000;
constexpr uint32 BurntDelayMs = 1500;           // the floor burns this long after the strike: the time to step off
constexpr uint32 BurntLastsMs = 2 * ExecutionEveryMs + 500;  // then goes out, two seconds before the next-but-two DOOM
constexpr uint32 BurntEveryMs = 1000;           // a player on it is struck at most this often
constexpr float ExecutionPct = 60.0f;
constexpr float BurntPct = 40.0f;
constexpr uint32 SursisMs = 30000;
constexpr float SursisTakenPct = 50.0f;
// Mise au cachot: the cage holds CageMs; its health is CageBreakSeconds of the group's damage
constexpr uint32 CageCastMs = 1500;
constexpr uint32 CageMs = 8000;
constexpr float CageBreakSeconds = 3.0f;
constexpr uint32 CageCurfewMs = 5000;           // with the curfew: it sounds this long after the cage shuts
// Châtiment exemplaire: shared by whoever stands in it (two tanks: 110% each, which a tank lives through)
constexpr uint32 PunishmentCastMs = 4000;
constexpr float PunishmentArcDegrees = 60.0f;   // VW_Punishment's painting
constexpr float PunishmentReach = 14.0f;
constexpr float PunishmentPct = 220.0f;
constexpr float PunishmentAlonePct = 70.0f;     // one alone dies, and everyone takes this
constexpr float PunishmentOffTankDistance = 5.0f;
// Faux du geôlier: the axe's blade swept from his feet through SweepDegrees in SweepMs, the third of the room it
// sweeps warned as a cone first
constexpr float SweepDegrees = 120.0f;
constexpr uint32 SweepMs = 2000;
constexpr uint32 SweepCheckMs = 100;
// The axe's pace: phase 3 and Perpétuité blow more often
constexpr uint32 AxeEveryPhase3Ms = 5000;
constexpr uint32 AxeEveryFrenzyMs = 4000;

// The warden's mark: a first mistake is held back to MarkHitPct of the player's health and never takes them under
// MarkFloorPct (DamageDealt), then marks them; a mistake while marked is the whole hit. Hits within MarkGraceMs of
// the marking one are the same mistake.
LiveTuning::Knob const MarkHitPct("vorhan.mark_hit_pct", 75.0f);
constexpr float MarkTakenPct = 50.0f;
constexpr float MarkFloorPct = 10.0f;
constexpr uint32 MarkMs = 15000;
constexpr uint32 MarkGraceMs = 1000;
// Mise à l'isolement on the tank: whatever it takes, it leaves the tank this share of its health
constexpr float IsolationFloorPct = 10.0f;

// --- The room (Magtheridon's Lair, measured: the channelers at 35-40 yd, the walls at 46-49) ------------------------
Position const ArenaCenter = { -18.7f, 2.2f, -0.3f, 0.0f };
constexpr float ArenaReach = 50.0f;
constexpr float MusicReach = 120.0f;
constexpr float ClearRadius = 60.0f;
constexpr float CellDistance = 18.0f;
constexpr float CellRadius = 3.0f;              // 6 yd across, as its seal is painted
constexpr float MarkDistance = 10.0f;
constexpr float MarkRadius = 2.0f;              // 4 yd across
constexpr float MarkReach = 2.5f;               // standing on a mark: within this of its middle
constexpr float WallRadius = 30.0f;              // the walls: thirty flat pieces round it, their middles at 29.8 yd
constexpr float WallKillRadius = 29.8f;
constexpr float ChargeSafeRadius = 6.0f;        // from within this of the warden, his throw lands short of the walls
constexpr float ChainBreakDistance = 20.0f;
constexpr float ChainSpotDistance = 15.0f;      // partners each go this far out, opposite ways: 30 yd apart
constexpr float SealKeepAway = 28.0f;           // bots: the isolated tank keeps this far from the others
constexpr float TankSpotSlack = 6.0f;

// Throws: a knockback's flight lasts 2 x speedZ / 19.29 s; its distance is speedXY times that
constexpr float ThrowSpeedZ = 7.0f;
constexpr float ThrowDistance = 18.0f;          // the charge: from within 6 yd lands within 24, short of the walls
constexpr float IsolationThrowDistance = 25.0f;

// His eye, drawn by the players' interface over his torso (FrameXML WardenGaze.lua: over the world and the nameplates,
// which hid it as a model in the world): this high over his feet, this wide (yards)
constexpr std::string_view GazePrefix = "WardenVorhan";
constexpr float GazeHeight = 4.0f;
constexpr float GazeWidth = 5.0f;

void SendGaze(Player* player, ObjectGuid const& on, uint32 durationMs, float height, float width)
{
    if (!player || !player->GetSession() || player->GetSession()->IsBot())
        return;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        Acore::StringFormat("{}\tGAZE\t0x{:016X}\t{}\t{:.2f}\t{:.2f}", GazePrefix, on.GetRawValue(), durationMs,
                            height, width));
    player->GetSession()->SendPacket(&packet);
}
constexpr float GazeThrowSpeedXY = 15.0f;
constexpr float GazeThrowSpeedZ = 6.0f;
float ThrowSpeedXY(float distance)
{
    return distance / (2.0f * ThrowSpeedZ / 19.2911f);
}

// --- Timeline (ms from the pull) ------------------------------------------------------------------------------------
constexpr uint32 AtRiot = 95000;
constexpr uint32 AtPhase2 = 125000;
constexpr uint32 AtPhase3 = 222000;
constexpr uint32 AtSweepFromMs = 277000;        // the blade sweeps from the end of the execution
constexpr uint32 AtLifeSentence = 330000;
constexpr uint32 AtHardEnrage = 350000;
constexpr uint32 LifeSentenceEveryMs = 4000;

// Rules' timings
constexpr uint32 SentenceCastMs = 2000;
constexpr uint32 IsolationCastMs = 1500;
constexpr uint32 IsolationBurstMs = 3000;
constexpr uint32 CellsMs = 8000;                // the cells drawn, then the doors slam
constexpr uint32 CurfewAfterCellsMs = 1000;     // with the cells: the curfew a second after the doors
constexpr uint32 ChainCastMs = 1000;
constexpr uint32 ChainMaxMs = 8000;
constexpr uint32 ChainGraceMs = 3000;           // the chain only tightens after this: the time to walk apart
constexpr uint32 GazeMs = 6000;
constexpr uint32 RollCallMs = 6000;
constexpr uint32 ChargeWallsMs = 4000;          // the walls up, then the throw
constexpr uint32 ChargeLandMs = 850;            // where they are this long after it: just landed (a 0.73 s flight)
constexpr uint32 WallsLastMs = 7000;
constexpr uint32 CurfewMs = 7000;               // with the gaze: a second after it opens
constexpr uint32 LookAwayLeadMs = 1500;         // the bots turn their backs this long before the eye opens
constexpr uint32 CurfewHoldLeadMs = 1500;       // the bots stand still this long before the curfew sounds
constexpr uint32 RiotWaveMs[3] = { 2000, 11000, 20000 };   // from the riot's start
constexpr uint32 PrisonersPerWave = 4;
constexpr uint32 DefiGraceMs = 8000;

constexpr uint32 NPC_VORHAN = 930200;
constexpr uint32 NPC_PRISONER = 930201;
constexpr uint32 NPC_ABYSSAL = 930202;
constexpr uint32 NPC_CAGE = 930203;
constexpr uint32 NPC_STALKER = 900104;          // GroundIndicators' invisible stalker

// What hits (localTools/wardenVorhan/Spells.ps1): never cast, named in the log and the death recap
enum Hits : uint32
{
    SPELL_SENTENCE              = 94400,
    SPELL_ISOLATION             = 94401,
    SPELL_SEAL                  = 94402,
    SPELL_ESCAPE                = 94403,
    SPELL_OVERCROWDED           = 94404,
    SPELL_OUT_OF_CELL           = 94405,
    SPELL_SHACKLES              = 94406,
    SPELL_GAZE                  = 94407,
    SPELL_ABSENT                = 94408,
    SPELL_GATHERING             = 94409,
    SPELL_WRONG_NUMBER          = 94410,
    SPELL_BARRIER               = 94411,
    SPELL_CURFEW_VIOLATION      = 94412,
    SPELL_LIFE_SENTENCE         = 94413,
    SPELL_CAPITAL               = 94414,
    SPELL_AXE                   = 94415,
    SPELL_EXECUTION             = 94416,
    SPELL_BURNT                 = 94417,
    SPELL_CAGE_DEATH            = 94418,
    SPELL_PUNISHMENT            = 94419,
    SPELL_SWEEP                 = 94452,
    SPELL_OFF_SEAT              = 94455,
};

// The debuffs, shown on the players (and the warden's eye on him)
enum Marks : uint32
{
    SPELL_MARK                  = 94420,
    SPELL_ISOLATED              = 94421,
    SPELL_SHACKLED              = 94422,
    SPELL_CURFEW                = 94423,
    SPELL_ESCAPED               = 94424,
    SPELL_OPENING_EYE           = 94425,
    SPELL_CURFEW_STUN           = 94426,
    SPELL_RALLY                 = 94427,        // Rancœur du détenu: the melee's damage back after a rule
    SPELL_SURSIS                = 94428,        // an execution's strike taken: the next one is death
    SPELL_CAGED                 = 94429,        // Au cachot: rooted in the cage
    SPELL_NUMBER_FIRST          = 94430,        // Matricule 1, ... 94437 Matricule 8
};

// His cast bars
enum Casts : uint32
{
    CAST_AXE                    = 94438,
    CAST_SENTENCE               = 94440,
    CAST_ISOLATION              = 94441,
    CAST_CELLS                  = 94442,
    CAST_SHACKLES               = 94443,
    CAST_GAZE                   = 94444,
    CAST_ROLL_CALL              = 94445,
    CAST_CHARGE                 = 94446,
    CAST_CURFEW                 = 94447,
    CAST_RIOT                   = 94448,
    CAST_LIFE_SENTENCE          = 94449,
    CAST_EXECUTION              = 94439,
    CAST_CAGE                   = 94453,
    CAST_PUNISHMENT             = 94454,
};

constexpr uint32 SPELL_CHAIN_BEAM = 94450;
constexpr uint32 SPELL_CAGE = 94451;

// Stock spell visual kits
enum Kits : uint32
{
    KIT_SHOUT                   = 389,          // Intimidating Shout's: the warden bellowing his sentence
    KIT_STRIKE                  = 2370,         // Hurtful Strike's (Gruul): a heavy blow
    KIT_SEAL                    = 2350,         // Shadowfury's impact: a dark burst, the seal
    KIT_SHOCKWAVE               = 9854,         // Shockwave's: the warden's charge
};

// creature_text of NPC_VORHAN (stat_growth_warden_vorhan.sql)
enum Texts : uint8
{
    SAY_AGGRO                   = 0,
    SAY_KILL                    = 1,
    SAY_RIOT                    = 2,
    SAY_PHASE_2                 = 3,
    SAY_LIFE_SENTENCE           = 4,
    SAY_HARD_ENRAGE             = 5,
    SAY_DEATH                   = 6,
    SAY_PHASE_3                 = 7,
};

enum class Phase : uint8
{
    None,
    One,
    Riot,
    Two,
    Three,
    LifeSentence,
    Over,
};

enum class Ability : uint8
{
    Sentence,
    Isolation,
    Cells,
    CellsCurfew,
    Shackles,
    ShacklesGaze,
    Gaze,
    GazeCurfew,
    RollCall,
    Charge,
    ChargeRollCall,
    Riot,
    RiotWave,
    RiotSentence,
    Phase2,
    Phase3,
    Cage,
    CageCurfew,
    Execution,
    Punishment,
    LifeSentence,
    LifeSentenceHit,
    HardEnrage,
};

bool IsPhaseStep(Ability ability)
{
    return ability == Ability::Riot || ability == Ability::Phase2 || ability == Ability::Phase3 ||
           ability == Ability::LifeSentence;
}

struct Step
{
    uint32 at;
    Ability what;
};

// The whole fight, in order: each rule alone in phase 1, together in phase 2, phase 3's own then with the old ones
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps = {
        // Phase 1, "Le règlement"
        { 5000, Ability::Sentence },
        { 12000, Ability::Isolation },
        { 20000, Ability::Cells },
        { 34000, Ability::Sentence },
        { 40000, Ability::Shackles },
        { 52000, Ability::Isolation },
        { 60000, Ability::Gaze },
        { 72000, Ability::RollCall },
        { 84000, Ability::Charge },
        { 92000, Ability::Sentence },
        // The riot
        { AtRiot, Ability::Riot },
        { AtRiot + 15000, Ability::RiotSentence },
        // Phase 2, "Tolérance zéro"
        { AtPhase2, Ability::Phase2 },
        { 128000, Ability::Isolation },
        { 135000, Ability::CellsCurfew },
        { 152000, Ability::Sentence },
        { 158000, Ability::ShacklesGaze },
        { 175000, Ability::Isolation },
        { 182000, Ability::ChargeRollCall },
        { 198000, Ability::Sentence },
        { 204000, Ability::GazeCurfew },
        { 215000, Ability::Isolation },
        // Phase 3, "Exécution des peines"
        { AtPhase3, Ability::Phase3 },
        { 226000, Ability::Cage },
        { 240000, Ability::Execution },         // its cast and seven DOOMs: over at 4:32
        { 279000, Ability::Sentence },
        { 285000, Ability::Punishment },
        { 295000, Ability::CageCurfew },
        { 309000, Ability::Isolation },
        { 316000, Ability::Punishment },
        { 324000, Ability::Sentence },
        // Perpétuité, then the end
        { AtLifeSentence, Ability::LifeSentence },
        { AtHardEnrage, Ability::HardEnrage },
    };
    for (uint32 wave = 0; wave < std::size(RiotWaveMs); ++wave)
        steps.push_back({ AtRiot + RiotWaveMs[wave], Ability::RiotWave });
    for (uint32 at = AtLifeSentence + 1000; at < AtHardEnrage; at += LifeSentenceEveryMs)
        steps.push_back({ at, Ability::LifeSentenceHit });
    std::stable_sort(steps.begin(), steps.end(), [](Step const& left, Step const& right)
    {
        if (left.at != right.at)
            return left.at < right.at;
        return IsPhaseStep(left.what) && !IsPhaseStep(right.what);
    });
    return steps;
}

// Coup de hache's lines (shapes.json VW_AxeSeg*, VW_AxeHitSeg*: four pieces 10 yards long, built to size): the whole
// line, 40 yards, and the split's branches, its first two pieces (20 yards)
constexpr GroundIndicators::PaintedLine AxeFullLine { 94237, 4, 2.0f, true, 0 };
constexpr GroundIndicators::PaintedLine AxeFullLineHit { 94241, 4, 2.0f, true, 0 };
constexpr GroundIndicators::PaintedLine AxeHalfLine { 94237, 2, 2.0f, true, 0 };
constexpr GroundIndicators::PaintedLine AxeHalfLineHit { 94241, 2, 2.0f, true, 0 };
// Two lines from then in phase 1; in phase 2 three, then from AxeSplitFromMs one that splits
constexpr uint32 AxeTwoFromMs = 50000;
constexpr uint32 AxeSplitFromMs = 175000;

// The Roll Call's four marks, at the cardinal points (north, east, south, west), and which pair each holds at each
// call: the same sequence every pull, so it can be learnt
constexpr std::array<std::array<uint8, 4>, 3> RollCallOrders = { {
    { 0, 1, 2, 3 },     // north 1-2, east 3-4, south 5-6, west 7-8
    { 2, 0, 3, 1 },     // north 5-6, east 1-2, south 7-8, west 3-4
    { 1, 3, 0, 2 },     // north 3-4, east 7-8, south 1-2, west 5-6
} };

// .vorhan botsonly: the fight fought by its bots alone, a game master watching (the fight leaves game masters out): the
// bots count as players, for testing them (e2e/local/vorhan). The board's wipe counts them so on its own: a group whose
// players all watch is fought by its bots (RaidFinder IsChallengeFighterStanding).
bool BotsOnly = false;

float Reference()
{
    return Power::ExpectedPlayerHealth(ProfileItemLevel, ProfileParagon);
}

// What a broken rule's area is told to the bots it does (GroundIndicators hitDamage): nothing known, so always left.
// A bot stood in an area whose hit left it above 30% of its health (SurvivableHit), but here a hit is a mistake: it
// marks, and the next one kills. Bots stood in the axe's lines at full health and died on the next rule (2026-10-10).
constexpr uint32 MistakeDamage = 0;

// North is +x; the clock turns the way the sun does over a map seen from above: towards -y
float ClockAngle(float steps, float perTurn)
{
    return Position::NormalizeOrientation(-steps * 2.0f * float(M_PI) / perTurn);
}

bool IsHealerPlayer(Player* player)
{
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() && member.roles)
                return member.roles & lfg::PLAYER_ROLE_HEALER;
    return player->HasHealSpec();
}

struct boss_warden_vorhan : public ScriptedAI
{
    boss_warden_vorhan(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void InitializeAI() override
    {
        // Hidden until its instance is known to be a challenge's (UpdateDefi)
        if (!_defiConfirmed)
            me->SetVisible(false);
        ScriptedAI::InitializeAI();
    }

    void Reset() override
    {
        // A fight still on is a wipe: the board resets him once the players are down (RaidFinder UpdateChallengeWipe),
        // and the group is raised once he stands reset at his post
        if (_phase != Phase::None && _phase != Phase::Over)
        {
            LogSummary("wipe");
            EndMusic();
        }
        ResetFight();
        if (_defiConfirmed)
        {
            me->SetVisible(true);
            SetModelHealth();
        }
        // The board holds him until the group pulls (RaidFinder HoldChallengeBoss)
        me->SetReactState(REACT_PASSIVE);
    }

    // A wipe is the players', never combat's: while one stands in the lair he fights on (the riot holds him away from
    // them, untargetable, and the core would evade him). The wipe itself is the board's (RaidFinder
    // UpdateChallengeWipe): he is reset in place, at full health (Reset), and nobody is raised before.
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        bool const fighting = _phase != Phase::None && _phase != Phase::Over;
        if (fighting && RealPlayerStanding())
            return;
        ScriptedAI::EnterEvadeMode(why);
    }

    // A player standing in the lair (the bots too with .vorhan botsonly)
    bool RealPlayerStanding() const
    {
        for (Player* player : ArenaPlayers())
            if (player->GetSession() && (!player->GetSession()->IsBot() || BotsOnly))
                return true;
        return false;
    }

    // The fight ended by him (the hard enrage, nobody left in the lair): out of combat, reset at his post (Reset)
    void Wipe()
    {
        ScriptedAI::EnterEvadeMode(EVADE_REASON_OTHER);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (_phase != Phase::None)
            return;
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, ArenaReach);
        _timeline = BuildTimeline();
        _next = 0;
        _pullMs = getMSTime();
        _phase = Phase::One;
        Talk(SAY_AGGRO);
        _fightListeners.clear();
        for (Player* player : Listeners())
        {
            if (player->GetSession() && !player->GetSession()->IsBot())
                EvolutionsAudio::PlayMusic(player, "Music.WardenVorhan");
            _fightListeners.insert(player->GetGUID());
            FightMusic::Claim(player->GetGUID(), me->GetGUID());
        }
        LOG_INFO("module.vorhan", "Vorhan pulled instance={} health={} tier factor={}", me->GetInstanceId(),
                 me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        if (Player* player = victim->ToPlayer())
            RecordDeath(player);
        if (!victim->IsPlayer() || _phase == Phase::Over)
            return;
        uint32 const now = getMSTime();
        if (_lastKillYellMs && getMSTimeDiff(_lastKillYellMs, now) < 10000)
            return;
        _lastKillYellMs = now;
        Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        LOG_INFO("module.vorhan", "Vorhan killed instance={} elapsed={}ms", me->GetInstanceId(), Elapsed());
        LogSummary("kill");
        EndMusic();
        ResetFight();
        _phase = Phase::Over;
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        _summons.Despawn(summon);
    }

    // A prisoner's blow on a player: at least its share of the reference, whatever their armour (an area damage moment
    // for the healers, on the tanks)
    void PrisonerDamage(Creature* prisoner, Unit* victim, uint32& damage)
    {
        if (!victim || !victim->IsPlayer())
            return;
        float const pct = prisoner->GetEntry() == NPC_ABYSSAL ? AbyssalMeleePct : PrisonerMeleePct;
        float const floor = Reference() * pct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
        damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
    }

    // Who dealt what (a pet's to its owner), for the summary: the check is tuned on it
    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
            _dealt[player->GetGUID()] += damage;
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player)
            return;
        if (type == DIRECT_DAMAGE)
        {
            float const floor = Reference() * MeleeFloorPct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
            damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
            _lastHit[player->GetGUID()] = { 0, Elapsed(), false };
        }
        float const maximum = float(player->GetMaxHealth());
        // A first mistake (Hit): held back to MarkHitPct of their health, and never under MarkFloorPct of it
        if (player->GetGUID() == _sparing)
        {
            damage = std::min(damage, uint32(maximum * float(MarkHitPct) / 100.0f));
            uint32 const floor = uint32(maximum * MarkFloorPct / 100.0f);
            damage = std::min(damage, player->GetHealth() > floor ? player->GetHealth() - floor : 0u);
        }
        // Mise à l'isolement never kills its tank
        if (player->GetGUID() == _isolating)
        {
            uint32 const floor = uint32(maximum * IsolationFloorPct / 100.0f);
            damage = std::min(damage, player->GetHealth() > floor ? player->GetHealth() - floor : 0u);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        bool const fighting = _phase != Phase::None && _phase != Phase::Over;
        // The riot: no victim (he stands apart, untargetable), the fight going on
        if (!fighting || _phase != Phase::Riot)
            if (!UpdateVictim())
            {
                if (!fighting)
                    UpdateOutOfCombat(diff);
                return;
            }

        scheduler.Update(diff);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && _timeline[_next].at <= elapsed && _phase != Phase::Over)
            Execute(_timeline[_next++].what);

        if (elapsed >= _nextSecondMs)
        {
            _nextSecondMs = elapsed + 1000;
            EverySecond();
        }
        if (elapsed >= _nextStandingCheckMs)
        {
            _nextStandingCheckMs = elapsed + 500;
            CheckPlayersStanding();
            CheckBurntFloor();
        }
        if (AxeFree(elapsed))
            AxeVolley();
        if (CanMelee())
            DoMeleeAttackIfReady();
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        std::string numbers;
        for (uint8 number = 1; number <= 8; ++number)
            if (Player* player = PlayerOf(number))
                numbers += Acore::StringFormat(" {}={}", number, player->GetName());
        return Acore::StringFormat("Vorhan: phase {} at {:.1f}s, health {}/{} ({:.1f}%), reference {:.0f}, next step "
                                   "{}/{}, numbers:{}", uint32(_phase), Elapsed() / 1000.0f, me->GetHealth(),
                                   me->GetMaxHealth(), me->GetHealthPct(), Reference(), _next, _timeline.size(),
                                   numbers.empty() ? " none" : numbers);
    }

    // Jumps the fight forward: the steps passed over are dropped, the phase changes among them still run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        EndCast();
        _pullMs -= seconds * IN_MILLISECONDS;
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && _timeline[_next].at <= elapsed)
        {
            Ability const what = _timeline[_next++].what;
            if (IsPhaseStep(what))
                Execute(what);
        }
        return true;
    }

    // One rule now, the fight going on
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat())
            return false;
        static std::map<std::string, Ability> const rules = {
            { "sentence", Ability::Sentence }, { "isolation", Ability::Isolation }, { "cells", Ability::Cells },
            { "cellscurfew", Ability::CellsCurfew }, { "shackles", Ability::Shackles },
            { "shacklesgaze", Ability::ShacklesGaze }, { "gaze", Ability::Gaze },
            { "gazecurfew", Ability::GazeCurfew }, { "rollcall", Ability::RollCall }, { "charge", Ability::Charge },
            { "chargerollcall", Ability::ChargeRollCall }, { "wave", Ability::RiotWave },
            { "cage", Ability::Cage }, { "cagecurfew", Ability::CageCurfew }, { "execution", Ability::Execution },
            { "punishment", Ability::Punishment },
        };
        auto const found = rules.find(what);
        if (found == rules.end())
            return false;
        Execute(found->second);
        return true;
    }

private:
    uint32 Elapsed() const
    {
        return _phase == Phase::None ? 0 : getMSTimeDiff(_pullMs, getMSTime());
    }

    // --- The challenge's instance ------------------------------------------------------------------------------------
    void UpdateOutOfCombat(uint32 diff)
    {
        _checkTimer += diff;
        if (_checkTimer < 1000)
            return;
        _checkTimer = 0;
        if (!_defiConfirmed)
            UpdateDefi();
        else
            ClearLair();
    }

    void UpdateDefi()
    {
        Map* map = me->GetMap();
        if (!map || !map->HavePlayers())
        {
            _defiWaitMs = 0;
            return;
        }
        if (IsChallengeInstanceFor(map, me->GetEntry()))
        {
            _defiConfirmed = true;
            me->SetVisible(true);
            SetModelHealth();
            ClearLair();
            LOG_INFO("module.vorhan", "Vorhan appears for a challenge instance={} health={}", me->GetInstanceId(),
                     me->GetMaxHealth());
            return;
        }
        _defiWaitMs += 1000;
        if (_defiWaitMs >= DefiGraceMs)
        {
            LOG_INFO("module.vorhan", "Vorhan leaves an instance that is no challenge's instance={}",
                     me->GetInstanceId());
            me->DespawnOrUnsummon(0ms, Seconds(7 * DAY));
        }
    }

    // His health from the power model: a raid group of the profile over the seconds he can be hit
    // (Power::RaidBossHealth), and the live knob over it
    void SetModelHealth()
    {
        float const dealers = Power::GroupDamageDealers(GroupDamage, GroupTanks, GroupHealers);
        float const health = Power::RaidBossHealth(ProfileItemLevel, ProfileParagon, dealers, UptimeSeconds) *
            EightPlayerShare * float(HealthScale);
        me->SetCreateHealth(uint32(health));
        me->SetMaxHealth(uint32(health));
        me->SetFullHealth();
    }

    // The lair is his: Magtheridon, his channelers and their cubes go (triggers stay); no stock encounter is fought in
    // his instance, and one left in progress is set back (a raid lets nobody in while one is)
    void ClearLair()
    {
        std::list<Creature*> found;
        Acore::AllWorldObjectsInRange check(me, ClearRadius);
        Acore::CreatureListSearcher<Acore::AllWorldObjectsInRange> searcher(me, found, check);
        Cell::VisitObjects(me, searcher, ClearRadius);
        for (Creature* creature : found)
            if (creature != me && creature->IsAlive() && !creature->IsTrigger() &&
                creature->GetEntry() != NPC_PRISONER && creature->GetEntry() != NPC_ABYSSAL &&
                creature->GetEntry() != NPC_STALKER && !creature->IsCharmedOwnedByPlayerOrPlayer() &&
                creature->IsHostileToPlayers())
                creature->DespawnOrUnsummon(0ms, Seconds(DAY));

        if (InstanceScript* instance = me->GetInstanceScript())
            for (uint32 boss = 0; boss < instance->GetEncounterCount(); ++boss)
                if (instance->GetBossState(boss) == IN_PROGRESS)
                    instance->SetBossState(boss, NOT_STARTED);
    }

    // --- Fight state -----------------------------------------------------------------------------------------------
    void ResetFight()
    {
        scheduler.CancelAll();
        EndCast();
        _awayFromHim.clear();
        _shacklesOn = false;
        _nextAxeMs = 0;
        _busyUntil = 0;
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        ClearPlayerAuras();
        ClearNumbers();
        _timeline.clear();
        _next = 0;
        _phase = Phase::None;
        _nextStandingCheckMs = 0;
        _nextSecondMs = 0;
        _numbers.clear();
        _marks.clear();
        _escapes.clear();
        _lastHit.clear();
        _deaths.clear();
        _dealt.clear();
        _chains.clear();
        _sparing.Clear();
        _isolating.Clear();
        _rollCalls = 0;
        _isolations = 0;
        _lifeSentences = 0;
        _rulesKept = 0;
        _rulesBroken = 0;
        _placingUntil = 0;
        _executionOn = false;
        _burnt.clear();
        _burntStruck.clear();
        _sursis.clear();
        ClearCage();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->ClearEmoteState();
        me->RemoveAurasDueToSpell(SPELL_OPENING_EYE);
    }

    void ClearPlayerAuras()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
            {
                for (uint32 spell : { uint32(SPELL_MARK), uint32(SPELL_ISOLATED), uint32(SPELL_SHACKLED),
                    uint32(SPELL_CURFEW), uint32(SPELL_ESCAPED), uint32(SPELL_CURFEW_STUN), SPELL_CHAIN_BEAM,
                    uint32(SPELL_SURSIS), uint32(SPELL_CAGED) })
                    player->RemoveAurasDueToSpell(spell);
                for (uint32 number = 0; number < 8; ++number)
                    player->RemoveAurasDueToSpell(SPELL_NUMBER_FIRST + number);
            }
    }

    bool CanMelee() const
    {
        if (_casting || me->HasReactState(REACT_PASSIVE) || me->HasUnitState(UNIT_STATE_CASTING))
            return false;
        return _phase == Phase::One || _phase == Phase::Two || _phase == Phase::Three ||
               _phase == Phase::LifeSentence;
    }

    // The players the fight hits: alive in the lair, not game masters
    std::vector<Player*> ArenaPlayers() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || player->IsGameMaster() || !player->IsAlive() ||
                player->GetExactDist2d(&ArenaCenter) > ArenaReach)
                continue;
            players.push_back(player);
        }
        return players;
    }

    // The players who hear his music: everyone near the lair, dead or a game master too
    std::vector<Player*> Listeners() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->GetExactDist2d(&ArenaCenter) <= MusicReach)
                players.push_back(player);
        return players;
    }

    Position Ground(Position const& at) const
    {
        float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), at.GetPositionX(), at.GetPositionY(),
                                                 ArenaCenter.GetPositionZ() + 5.0f, true, 15.0f);
        return Position(at.GetPositionX(), at.GetPositionY(), z > INVALID_HEIGHT ? z : ArenaCenter.GetPositionZ(),
                        at.GetOrientation());
    }

    Position AtAngle(Position const& from, float angle, float distance) const
    {
        return Ground(Position(from.GetPositionX() + std::cos(angle) * distance,
                               from.GetPositionY() + std::sin(angle) * distance, from.GetPositionZ(), angle));
    }

    Position CellSpot(uint8 number) const
    {
        return AtAngle(ArenaCenter, ClockAngle(float(number - 1), 8.0f), CellDistance);
    }

    // The Roll Call's cardinal marks: 0 north, 1 east, 2 south, 3 west
    Position MarkSpot(uint8 cardinal) const
    {
        return AtAngle(ArenaCenter, ClockAngle(float(cardinal), 4.0f), MarkDistance);
    }

    // --- The inmate numbers ------------------------------------------------------------------------------------------
    // Drawn for each rule that needs them (the cells, the shackles, the roll call), shown over the heads and as a
    // debuff only while it runs: a new draw every time, and every pull. Rules back to back keep the same draw (the
    // charge's and its roll call).
    void AssignNumbers(uint32 durationMs)
    {
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomShuffle(players);
        ClearNumbers();
        _numbers.clear();
        uint8 number = 0;
        for (Player* player : players)
        {
            if (++number > 8)
                break;
            _numbers[player->GetGUID()] = number;
            ShowNumber(player, number, durationMs);
        }
        std::string drawn;
        for (uint8 shown = 1; shown <= std::min<uint8>(number, 8); ++shown)
            if (Player* player = PlayerOf(shown))
                drawn += Acore::StringFormat(" {}={}", shown, player->GetName());
        LOG_INFO("module.vorhan", "Vorhan numbers instance={} at={:.1f}s:{}", me->GetInstanceId(), Elapsed() / 1000.0f,
                 drawn);
    }

    uint8 NumberOf(Player const* player) const
    {
        auto const found = _numbers.find(player->GetGUID());
        return found == _numbers.end() ? 0 : found->second;
    }

    Player* PlayerOf(uint8 number) const
    {
        for (auto const& [guid, held] : _numbers)
            if (held == number)
                return ObjectAccessor::GetPlayer(*me, guid);
        return nullptr;
    }

    // The Roll Call pair a number is in: 0 for 1-2, ... 3 for 7-8
    static uint8 PairOf(uint8 number)
    {
        return uint8((number - 1) / 2);
    }

    // --- What a player takes: the mark, the escapes ------------------------------------------------------------------
    float TakenFactor(Unit const* victim) const
    {
        uint32 const now = Elapsed();
        float factor = 1.0f;
        if (auto const mark = _marks.find(victim->GetGUID()); mark != _marks.end() && now < mark->second.until)
            factor *= 1.0f + MarkTakenPct / 100.0f;
        if (auto const escape = _escapes.find(victim->GetGUID());
            escape != _escapes.end() && now < escape->second.until)
            factor *= 1.0f + EscapeTakenPct / 100.0f * float(escape->second.stacks);
        if (auto const sursis = _sursis.find(victim->GetGUID()); sursis != _sursis.end() && now < sursis->second)
            factor *= 1.0f + SursisTakenPct / 100.0f;
        return factor;
    }

    // Not marked, or marked by this very mistake a moment ago: the hit is held back
    bool IsSpared(ObjectGuid guid, uint32 now) const
    {
        auto const mark = _marks.find(guid);
        return mark == _marks.end() || now >= mark->second.until || now < mark->second.since + MarkGraceMs;
    }

    void MarkPlayer(Player* player, uint32 now)
    {
        Marked& mark = _marks[player->GetGUID()];
        if (now < mark.until)
            return;
        mark = { now, now + MarkMs };
        AddTimedAura(player, SPELL_MARK, MarkMs);
    }

    void AddTimedAura(Unit* target, uint32 spellId, uint32 durationMs, uint8 stacks = 1)
    {
        if (!target || !sSpellMgr->GetSpellInfo(spellId))
            return;
        Aura* aura = target->GetAura(spellId);
        if (!aura)
            aura = me->AddAura(spellId, target);
        if (!aura)
            return;
        aura->SetMaxDuration(int32(durationMs));
        aura->SetDuration(int32(durationMs));
        if (stacks > 1)
            aura->SetStackAmount(stacks);
    }

    // A share of the reference health, as spell: the tier's factor applies on the way, then the player's defences. An
    // avoidable one is a mistake: the first is held back and marks, a second while marked is the whole hit.
    void Hit(Player* player, uint32 spellId, float percent, bool avoidable)
    {
        if (!player || !player->IsAlive())
            return;
        uint32 const now = Elapsed();
        ObjectGuid const guid = player->GetGUID();
        float const amount = Reference() * percent / 100.0f * TakenFactor(player);
        bool const spared = avoidable && IsSpared(guid, now);
        _lastHit[guid] = { spellId, now, avoidable };
        if (spared)
            _sparing = guid;
        MythicTuning::DealAbilityDamage(me, player, spellId, uint32(std::max(1.0f, amount)));
        _sparing.Clear();
        if (avoidable && spared && player->IsAlive())
            MarkPlayer(player, now);
    }

    void HitEveryone(uint32 spellId, float percent)
    {
        for (Player* player : ArenaPlayers())
            Hit(player, spellId, percent, false);
    }

    // A rule broken where the rule is binary (the Roll Call, two in a cell, the wall): death, whatever protects them,
    // the spell named in the log so the death recap says why
    void Doom(Player* player, uint32 spellId)
    {
        if (!player || !player->IsAlive())
            return;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        _lastHit[player->GetGUID()] = { spellId, Elapsed(), true };
        if (info)
        {
            SpellNonMeleeDamage log(me, player, info, info->GetSchoolMask());
            log.damage = player->GetHealth();
            me->SendSpellNonMeleeDamageLog(&log);
        }
        Unit::Kill(me, player, true, BASE_ATTACK, info);
    }

    void Kept(char const* rule)
    {
        ++_rulesKept;
        LOG_DEBUG("module.vorhan", "Vorhan rule kept: {}", rule);
    }

    void Broken(char const* rule, std::string const& who)
    {
        ++_rulesBroken;
        LOG_INFO("module.vorhan", "Vorhan rule broken instance={} at={:.1f}s {}: {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, rule, who);
    }

    // --- His casts ---------------------------------------------------------------------------------------------------
    // A rule on his cast bar: he stops and casts for as long as the rule takes, rooted - chasing his tank broke the
    // cast and the bar went while the rule still came
    void Cast(uint32 castSpell)
    {
        _casting = true;
        me->StopMoving();
        me->SetControlled(true, UNIT_STATE_ROOT);
        _rooted = true;
        if (sSpellMgr->GetSpellInfo(castSpell))
            me->CastSpell(me, castSpell, false);
    }

    void EndCast()
    {
        _casting = false;
        if (_rooted)
        {
            _rooted = false;
            me->SetControlled(false, UNIT_STATE_ROOT);
        }
    }

    // --- The timeline ----------------------------------------------------------------------------------------------
    void Execute(Ability what)
    {
        switch (what)
        {
            case Ability::Sentence:         Sentence(SentencePct[_phase == Phase::One ? 0 : 1]); break;
            case Ability::Isolation:        Isolation(); break;
            case Ability::Cells:            Cells(false); break;
            case Ability::CellsCurfew:      Cells(true); break;
            case Ability::Shackles:         Shackles(false); break;
            case Ability::ShacklesGaze:     Shackles(true); break;
            case Ability::Gaze:             Gaze(false); break;
            case Ability::GazeCurfew:       Gaze(true); break;
            case Ability::RollCall:         RollCall(false); break;
            case Ability::Charge:           Charge(false); break;
            case Ability::ChargeRollCall:   Charge(true); break;
            case Ability::Riot:             EnterRiot(); break;
            case Ability::RiotWave:         RiotWave(); break;
            case Ability::RiotSentence:     HitEveryone(SPELL_SENTENCE, RiotSentencePct); break;
            case Ability::Phase2:           EnterPhase2(); break;
            case Ability::Phase3:           EnterPhase3(); break;
            case Ability::Cage:             Cage(false); break;
            case Ability::CageCurfew:       Cage(true); break;
            case Ability::Execution:        Execution(); break;
            case Ability::Punishment:       Punishment(); break;
            case Ability::LifeSentence:     EnterLifeSentence(); break;
            case Ability::LifeSentenceHit:  LifeSentenceHit(); break;
            case Ability::HardEnrage:       HardEnrage(); break;
        }
    }

    void EverySecond()
    {
        // His tank keeps him in the middle: the rules are laid round it (not while a rule places everyone: a chained
        // tank goes to its own side)
        if (CanMelee() && Elapsed() >= _placingUntil)
            GroundIndicators::SetTankSpot(me, ArenaCenter, 2000, TankSpotSlack);
        UpdateChains();
        if (_defiConfirmed)
            ClearLair();
    }

    // Sentence: the whole group, the healers' rhythm (their defensives warned)
    void Sentence(float percent)
    {
        GroundIndicators::WarnGroupDamage(me, SentenceCastMs);
        Cast(CAST_SENTENCE);
        scheduler.Schedule(Milliseconds(SentenceCastMs), [this, percent](TaskContext)
        {
            EndCast();
            me->SendPlaySpellVisual(KIT_SHOUT);
            Sound("Vorhan.Sentence");
            HitEveryone(SPELL_SENTENCE, percent);
        });
    }

    // Mise à l'isolement: a blow on his tank that never kills and throws it far off, in solitary: its seal bursts 3 s
    // later on everyone, by their distance from it - death next to it, a raid-wide hit from far off. Nothing on the
    // ground says it: the tank knows the rule, or reads its debuff (the bots are told, unseen). The other tank takes
    // him (his tank's threat is gone, and the bots are told).
    void Isolation()
    {
        Player* tank = me->GetVictim() ? me->GetVictim()->ToPlayer() : nullptr;
        if (!tank)
            return;
        ObjectGuid const guid = tank->GetGUID();
        _busyUntil = Elapsed() + IsolationCastMs + IsolationBurstMs + 500;
        Cast(CAST_ISOLATION);
        if (Player* other = OtherTank(tank))
            GroundIndicators::SetBossHolder(me, other, IsolationCastMs + IsolationBurstMs + 3000);
        scheduler.Schedule(Milliseconds(IsolationCastMs), [this, guid](TaskContext)
        {
            EndCast();
            Player* tank = ObjectAccessor::GetPlayer(*me, guid);
            if (!tank || !tank->IsAlive())
                return;
            ++_isolations;
            tank->SendPlaySpellVisual(KIT_STRIKE);
            Sound("Vorhan.Isolation", tank);
            // The blow seen: a cone of fire from his fist through the tank, and the furrows of its throw
            float const blow = me->GetAngle(tank);
            GroundIndicators::ShowWardenStrike(me, me->GetPosition(), blow);
            GroundIndicators::ShowWardenPushTrail(me, tank->GetPosition(), blow);
            _isolating = guid;
            Hit(tank, SPELL_ISOLATION, IsolationPct, false);
            _isolating.Clear();
            LOG_INFO("module.vorhan", "Vorhan isolation instance={} at={:.1f}s tank={} {}", me->GetInstanceId(),
                     Elapsed() / 1000.0f, tank->GetName(), tank->IsAlive() ? "thrown" : "dead");
            if (!tank->IsAlive())
                return;
            me->GetThreatMgr().ResetThreat(tank);
            Throw(tank, ThrowSpeedXY(IsolationThrowDistance), ThrowSpeedZ);
            AddTimedAura(tank, SPELL_ISOLATED, IsolationBurstMs);
            // For the bots only: the tank keeps away, the others keep out of its lethal reach (KeepsAway)
            GroundIndicators::WatchCarriedCircle(me, tank, SealAvoidable + 1.0f, IsolationBurstMs,
                uint32(Reference() * SealNearPct / 100.0f), SealKeepAway);
            scheduler.Schedule(Milliseconds(IsolationBurstMs), [this, guid](TaskContext)
            {
                Player* tank = ObjectAccessor::GetPlayer(*me, guid);
                if (!tank)
                    return;
                tank->SendPlaySpellVisual(KIT_SEAL);
                Sound("Vorhan.SealBurst", tank);
                GroundIndicators::ShowWardenSealBurst(me, tank->GetPosition());
                std::string tooClose;
                std::string burst;
                for (Player* player : ArenaPlayers())
                {
                    if (player == tank)
                        continue;
                    float const distance = player->GetExactDist2d(tank);
                    float const share = std::clamp((distance - SealAvoidable) / (SealFar - SealAvoidable), 0.0f,
                                                   1.0f);
                    float const pct = distance < SealAvoidable ? SealNearPct :
                        SealEdgePct + (SealFarPct - SealEdgePct) * share;
                    uint32 const before = player->GetHealth();
                    Hit(player, SPELL_SEAL, pct, distance < SealAvoidable);
                    burst += Acore::StringFormat(" {} {:.1f}yd {:.0f}%:{}", player->GetName(), distance, pct,
                        player->IsAlive() ? int64(before) - int64(player->GetHealth()) : -1);
                    if (distance < SealAvoidable)
                        tooClose += Acore::StringFormat(" {} ({:.1f} yd{})", player->GetName(), distance,
                            player->GetSession() && player->GetSession()->IsBot() ? ", bot" : "");
                }
                LOG_INFO("module.vorhan", "Vorhan seal instance={} at={:.1f}s tank={}:{}", me->GetInstanceId(),
                         Elapsed() / 1000.0f, tank->GetName(), burst);
                if (tooClose.empty())
                    Kept("isolation");
                else
                    Broken("isolation seal", tooClose);
            });
        });
    }

    Player* OtherTank(Player const* tank) const
    {
        for (Player* player : ArenaPlayers())
            if (player != tank && IsGroupTank(player))
                return player;
        return nullptr;
    }

    // Cellules: eight cells, each player into their number's; the doors slam at the end. With the curfew: a second
    // after the doors, still in their cell.
    void Cells(bool curfew)
    {
        MarkAway();
        Cast(CAST_CELLS);
        uint32 const lasts = CellsMs + (curfew ? CurfewAfterCellsMs : 0);
        AssignNumbers(lasts + 500);
        Placing(lasts);
        Sound("Vorhan.CellsOpen");
        for (uint8 number = 1; number <= 8; ++number)
        {
            ShowCell(number, CellsMs);
            if (Player* player = PlayerOf(number); player && player->IsAlive())
                GroundIndicators::SetUnitSpot(me, player, CellSpot(number), 1.0f, lasts + 500);
        }
        if (curfew)
            Curfew(CellsMs + CurfewAfterCellsMs);
        HoldAxe(lasts);
        scheduler.Schedule(Milliseconds(CellsMs), [this](TaskContext)
        {
            EndCast();
            SlamDoors();
        });
    }

    void SlamDoors()
    {
        Sound("Vorhan.CellDoors");
        Rally();
        // Their numbers go a moment after the doors
        scheduler.Schedule(1500ms, [this](TaskContext) { ClearNumbers(); });
        std::array<std::vector<Player*>, 9> inCell;
        std::vector<Player*> outside;
        for (Player* player : ArenaPlayers())
        {
            uint8 cell = 0;
            for (uint8 number = 1; number <= 8 && !cell; ++number)
                if (Position const spot = CellSpot(number); player->GetExactDist2d(&spot) <= CellRadius)
                    cell = number;
            if (cell)
                inCell[cell].push_back(player);
            else
                outside.push_back(player);
        }

        bool kept = true;
        uint32 empty = 0;
        for (uint8 number = 1; number <= 8; ++number)
        {
            DropCage(number);
            // A cell with nobody's number on it (fewer players than cells) waits for no one
            if (inCell[number].empty() && PlayerOf(number) && PlayerOf(number)->IsAlive())
                ++empty;
            if (inCell[number].size() > 1)
            {
                kept = false;
                std::string who;
                for (Player* player : inCell[number])
                {
                    who += " " + player->GetName();
                    Doom(player, SPELL_OVERCROWDED);
                }
                Broken("overcrowded cell", who);
            }
        }
        for (Player* player : outside)
        {
            kept = false;
            uint8 const number = NumberOf(player);
            Position const own = CellSpot(number ? number : 1);
            Hit(player, SPELL_OUT_OF_CELL, OutOfCellPct, true);
            Broken("out of cell", Acore::StringFormat("{} number {} {:.1f} yd from its cell{}", player->GetName(),
                number, player->GetExactDist2d(&own), player->GetSession() && player->GetSession()->IsBot() ?
                    " (bot)" : ""));
        }
        for (uint32 escape = 0; escape < empty; ++escape)
        {
            kept = false;
            for (Player* player : ArenaPlayers())
            {
                Hit(player, SPELL_ESCAPE, EscapePct, false);
                Escaped& state = _escapes[player->GetGUID()];
                uint32 const now = Elapsed();
                state.stacks = now < state.until ? state.stacks + 1 : 1;
                state.until = now + EscapeMs;
                AddTimedAura(player, SPELL_ESCAPED, EscapeMs, uint8(state.stacks));
            }
        }
        if (empty)
            Broken("empty cells", std::to_string(empty));
        if (kept)
            Kept("cells");
    }

    // Couvre-feu: when it sounds (in `inMs`), anyone moving is struck, stunned and marked. The bots stand still from a
    // little before (GroundIndicators hold).
    void Curfew(uint32 inMs)
    {
        Sound("Vorhan.CurfewTick");
        GroundIndicators::ShowWardenCurfew(me, Ground(ArenaCenter), inMs);
        for (Player* player : ArenaPlayers())
        {
            AddTimedAura(player, SPELL_CURFEW, inMs);
            GroundIndicators::ShowCarriedLook(player, GroundIndicators::SPELL_WARDEN_CURFEW_MARK, inMs);
            HoldStill(player, inMs > CurfewHoldLeadMs ? inMs - CurfewHoldLeadMs : 0, CurfewHoldLeadMs + 1000);
        }
        scheduler.Schedule(Milliseconds(inMs), [this](TaskContext)
        {
            Sound("Vorhan.CurfewBell");
            std::string moved;
            for (Player* player : ArenaPlayers())
            {
                if (!player->isMoving())
                    continue;
                Hit(player, SPELL_CURFEW_VIOLATION, CurfewPct, true);
                Sound("Vorhan.Violation", player);
                if (player->IsAlive())
                    AddTimedAura(player, SPELL_CURFEW_STUN, 3000);
                moved += " " + player->GetName();
            }
            if (moved.empty())
                Kept("curfew");
            else
                Broken("curfew", moved);
        });
    }

    // Menottes: each pair of partners chained; 20 yd apart breaks it, until then it ramps every second. With the gaze:
    // break it facing away.
    void Shackles(bool gaze)
    {
        MarkAway();
        Cast(CAST_SHACKLES);
        Placing(ChainCastMs + ChainMaxMs);
        scheduler.Schedule(Milliseconds(ChainCastMs), [this, gaze](TaskContext)
        {
            EndCast();
            _chains.clear();
            _shacklesOn = true;
            // Pairs drawn at random, no number shown: the chain itself says who is bound to whom
            std::vector<Player*> players = ArenaPlayers();
            Acore::Containers::RandomShuffle(players);
            for (uint8 pair = 0; pair < 4 && 2u * pair + 1 < players.size(); ++pair)
            {
                Player* first = players[2 * pair];
                Player* second = players[2 * pair + 1];
                _chains.push_back({ first->GetGUID(), second->GetGUID(), 0, Elapsed() + ChainMaxMs, Elapsed() });
                AddTimedAura(first, SPELL_SHACKLED, ChainMaxMs);
                AddTimedAura(second, SPELL_SHACKLED, ChainMaxMs);
                // The chain drawn between them, by stalkers following each (cast by a player, the beam was a channel
                // that held a bot in place)
                ObjectGuid const tether = GroundIndicators::ShowTether(me, first, second, SPELL_CHAIN_BEAM, ChainMaxMs);
                _chains.back().tether = tether;
                Sound("Vorhan.Chains", first);
                // The bots each to their own side, the pairs spread round the room, until it breaks
                GroundIndicators::SetUnitSpot(me, first, AtAngle(ArenaCenter, ClockAngle(float(pair), 8.0f),
                    ChainSpotDistance), 1.5f, ChainMaxMs);
                GroundIndicators::SetUnitSpot(me, second, AtAngle(ArenaCenter, ClockAngle(float(pair + 4), 8.0f),
                    ChainSpotDistance), 1.5f, ChainMaxMs);
            }
            if (gaze)
                Gaze(false);
        });
    }

    struct Chain
    {
        ObjectGuid first;
        ObjectGuid second;
        uint32 ticks;
        uint32 until;
        uint32 since;                           // when it was put on
        ObjectGuid tether = ObjectGuid::Empty;  // the chain's look (GroundIndicators::ShowTether)
    };

    // --- Coup de hache ------------------------------------------------------------------------------------------
    // Free for a blow: not casting nor placing anyone, no rule going off, the next one far enough, not in the riot
    bool AxeFree(uint32 elapsed) const
    {
        if (_phase != Phase::One && _phase != Phase::Two && _phase != Phase::Three && _phase != Phase::LifeSentence)
            return false;
        if (_casting || elapsed < _placingUntil || elapsed < _busyUntil || elapsed < _nextAxeMs || !me->GetVictim())
            return false;
        if (_shacklesOn || _executionOn)
            return false;
        // Perpétuité's hits place nobody: the blows go on between them
        std::size_t next = _next;
        while (next < _timeline.size() && _timeline[next].what == Ability::LifeSentenceHit)
            ++next;
        return next >= _timeline.size() || _timeline[next].at >= elapsed + AxeWindowMs;
    }

    // How many lines, and whether they split, as the fight goes
    void AxeVolley()
    {
        uint32 const elapsed = Elapsed();
        _nextAxeMs = elapsed + (_phase == Phase::LifeSentence ? AxeEveryFrenzyMs :
                                _phase == Phase::Three ? AxeEveryPhase3Ms : AxeEveryMs);
        // parts: 0 a line that only lands; 2 a line that splits in two as it lands; 4 in four. sweep: a blade swept
        // through a third of the room as they land.
        uint32 lines = 1;
        uint8 parts = 0;
        bool sweep = false;
        if (_phase == Phase::LifeSentence)
        {
            lines = 3;
            parts = 4;
            sweep = true;
        }
        else if (_phase == Phase::Three)
        {
            sweep = elapsed >= AtSweepFromMs;
            lines = sweep ? 2 : 3;
            parts = sweep ? 4 : 2;
        }
        else if (_phase == Phase::Two)
        {
            lines = elapsed >= AxeSplitFromMs ? 1 : 3;
            parts = elapsed >= AxeSplitFromMs ? 2 : 0;
        }
        else if (elapsed >= AxeTwoFromMs)
            lines = 2;

        // At players other than his tanks, each a different one where it can
        std::vector<Player*> targets;
        for (Player* player : ArenaPlayers())
            if (player->IsAlive() && !IsGroupTank(player))
                targets.push_back(player);
        if (targets.empty())
            targets = ArenaPlayers();
        if (targets.empty())
            return;
        Acore::Containers::RandomShuffle(targets);

        Cast(CAST_AXE);
        Position const from = Ground(me->GetPosition());
        std::vector<float> directions;
        for (uint32 line = 0; line < lines; ++line)
            directions.push_back(me->GetAngle(targets[line % targets.size()]));
        me->SetFacingTo(directions.front());
        std::vector<GroundIndicators::Area> areas;
        for (float direction : directions)
            areas.push_back(AxeLine(from, direction, AxeFullLine, AxeWarnMs));
        if (sweep)
            Sweep(from, me->GetAngle(targets[lines % targets.size()]));
        scheduler.Schedule(Milliseconds(AxeWarnMs), [this, areas, parts](TaskContext)
        {
            EndCast();
            me->SendPlaySpellVisual(KIT_STRIKE);
            Sound("Vorhan.Isolation");
            for (GroundIndicators::Area const& area : areas)
            {
                AxeLands(area, AxeFullLine);
                if (parts)
                    AxeSplit(area, parts);
            }
        });
    }

    // The line opening as it lands, like shears: lines swing out from his feet to either side (in four, half as far
    // too), stop, blow
    void AxeSplit(GroundIndicators::Area const& area, uint8 parts)
    {
        float const full = AxeSwingDegrees * float(M_PI) / 180.0f;
        std::vector<float> turns = { full, -full };
        if (parts >= 4)
            turns = { full / 2.0f, -full / 2.0f, full, -full };
        auto const swings = std::make_shared<std::vector<std::pair<GroundIndicators::Area, ObjectGuid>>>();
        for (float turn : turns)
        {
            ObjectGuid placed;
            GroundIndicators::Area const stop = GroundIndicators::ShowSwingingLine(me, area, SPELL_AXE_SWING, turn,
                AxeSwingStartMs, AxeSwingMs, AxeSplitMs, MistakeDamage, AxeLingerMs, &placed);
            swings->emplace_back(stop, placed);
        }
        scheduler.Schedule(Milliseconds(AxeSplitMs), [this, swings](TaskContext)
        {
            Sound("Vorhan.Isolation");
            for (auto const& [stop, placed] : *swings)
            {
                if (Creature* line = me->GetMap()->GetCreature(placed))
                {
                    line->RemoveAurasDueToSpell(SPELL_AXE_SWING);
                    line->AddAura(SPELL_AXE_SWING_HIT, line);
                }
                AxeStrikes(stop);
            }
        });
    }

    // A line's warning, painted (the bots leave it: it is registered as any red area)
    GroundIndicators::Area AxeLine(Position const& from, float direction, GroundIndicators::PaintedLine const& look,
                                   uint32 warnMs)
    {
        GroundIndicators::Area area;
        area.kind = GroundIndicators::Area::Kind::Rectangle;
        area.origin = Position(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ(), direction);
        area.radius = float(look.count) * look.ratio * AxeWidth;
        area.width = AxeWidth;
        Position const center = Ground(ArenaCenter);
        return GroundIndicators::ShowPaintedLine(me, area, look, warnMs, GroundIndicators::Theme::None,
            MistakeDamage, AxeLingerMs, &center, WallRadius + 2.0f);
    }

    // The blow: the line torn open, whoever stands in it hit
    void AxeLands(GroundIndicators::Area const& area, GroundIndicators::PaintedLine const& look)
    {
        // The blow's look: the warning's pieces, four spells on (VW_AxeHitSeg*)
        GroundIndicators::PaintedLine hit = look;
        hit.firstSpell = look.firstSpell + 4;
        GroundIndicators::RepaintLine(me, area, look, hit);
        AxeStrikes(area);
    }

    void AxeStrikes(GroundIndicators::Area const& area)
    {
        std::string struck;
        for (Player* player : ArenaPlayers())
            if (area.Contains(player->GetPosition()))
            {
                Hit(player, SPELL_AXE, AxePct, true);
                struck += " " + player->GetName();
            }
        if (struck.empty())
            Kept("axe");
        else
            Broken("axe", struck);
    }

    static Position AtAngleFrom(Position const& from, float angle, float distance)
    {
        return Position(from.GetPositionX() + distance * std::cos(angle),
                        from.GetPositionY() + distance * std::sin(angle), from.GetPositionZ());
    }

    // Who stands at his side as a rule sends everyone away: the melee, whose damage it costs
    void MarkAway()
    {
        for (Player* player : ArenaPlayers())
            if (player->IsAlive() && player->GetExactDist2d(me) <= me->GetCombatReach() + RallyReach)
                _awayFromHim.insert(player->GetGUID());
    }

    // The rule over: those it kept off him get their damage back for a while (Rancœur du détenu)
    void Rally()
    {
        for (ObjectGuid const& guid : _awayFromHim)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid); player && player->IsAlive())
                AddTimedAura(player, SPELL_RALLY, RallyMs);
        _awayFromHim.clear();
    }

    void BreakChain(Chain const& chain)
    {
        GroundIndicators::EndTether(me, chain.tether);
        for (ObjectGuid const& guid : { chain.first, chain.second })
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
            {
                player->RemoveAurasDueToSpell(SPELL_SHACKLED);
                GroundIndicators::EndUnitSpot(me, player);
            }
    }

    void UpdateChains()
    {
        uint32 const now = Elapsed();
        for (auto chain = _chains.begin(); chain != _chains.end();)
        {
            Player* first = ObjectAccessor::GetPlayer(*me, chain->first);
            Player* second = ObjectAccessor::GetPlayer(*me, chain->second);
            bool const broken = !first || !second || !first->IsAlive() || !second->IsAlive() ||
                first->GetExactDist2d(second) > ChainBreakDistance;
            if (broken || now >= chain->until)
            {
                if (broken && first && second && first->IsAlive() && second->IsAlive())
                {
                    Kept("shackles");
                    Sound("Vorhan.ChainBreak", first);
                }
                BreakChain(*chain);
                chain = _chains.erase(chain);
                continue;
            }
            // Time to walk apart before it bites (ChainGraceMs)
            if (now < chain->since + ChainGraceMs)
            {
                ++chain;
                continue;
            }
            float const pct = ChainRampPct[std::min<std::size_t>(chain->ticks, ChainRampPct.size() - 1)];
            ++chain->ticks;
            if (chain->ticks >= 2)
                Broken("shackles held", Acore::StringFormat("{} {} {:.1f} yd apart", first->GetName(),
                    second->GetName(), first->GetExactDist2d(second)));
            Hit(first, SPELL_SHACKLES, pct, false);
            Hit(second, SPELL_SHACKLES, pct, false);
            Sound("Vorhan.ChainTighten", first);
            ++chain;
        }
        // The last chain gone: the shackles are over
        if (_shacklesOn && _chains.empty())
        {
            _shacklesOn = false;
            Rally();
        }
    }

    // Regard du geôlier: his eye opens over 6 s; whoever faces him then dies, thrown back. With the curfew: still,
    // facing away.
    void Gaze(bool curfew)
    {
        Cast(CAST_GAZE);
        Sound("Vorhan.GazeOpen");
        AddTimedAura(me, SPELL_OPENING_EYE, GazeMs);
        ShowGaze(GazeMs);
        LookAway(GazeMs);
        if (curfew)
        {
            Curfew(CurfewMs);
            HoldAxe(CurfewMs);
        }
        scheduler.Schedule(Milliseconds(GazeMs), [this](TaskContext)
        {
            EndCast();
            me->RemoveAurasDueToSpell(SPELL_OPENING_EYE);
            Sound("Vorhan.GazeBurst");
            std::string faced;
            for (Player* player : ArenaPlayers())
            {
                if (!player->isInFront(me, float(M_PI)))
                    continue;
                // How far off its back to him it stood (0: turned right away, 180: facing him)
                float const off = std::fabs(std::remainder(player->GetOrientation() - player->GetAngle(me) -
                    float(M_PI), 2.0f * float(M_PI))) * 180.0f / float(M_PI);
                LOG_INFO("module.vorhan", "Vorhan gaze instance={} {} faced him, {:.0f} degrees off{}",
                         me->GetInstanceId(), player->GetName(), off, player->isMoving() ? ", moving" : "");
                Throw(player, GazeThrowSpeedXY, GazeThrowSpeedZ);
                Doom(player, SPELL_GAZE);
                faced += " " + player->GetName();
            }
            if (faced.empty())
                Kept("gaze");
            else
                Broken("gaze", faced);
        });
    }

    // Appel nominal: he stands still and calls the roll; each pair to its mark. Alone, more than two, or on another
    // number's mark: death. A player whose partner is dead stands alone legitimately.
    void RollCall(bool drawn = false)
    {
        // Drawn by the charge: who it sent away is already known
        if (!drawn)
            MarkAway();
        std::array<uint8, 4> const order = RollCallOrders[_rollCalls++ % RollCallOrders.size()];
        Cast(CAST_ROLL_CALL);
        if (!drawn)
            AssignNumbers(RollCallMs + 500);
        Placing(RollCallMs);
        Sound("Vorhan.RollCall");
        std::array<uint8, 4> pairOnMark = {};
        for (uint8 cardinal = 0; cardinal < 4; ++cardinal)
        {
            pairOnMark[cardinal] = order[cardinal];
            ShowRollCallMark(cardinal, order[cardinal], RollCallMs);
        }
        for (auto const& [guid, number] : _numbers)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid); player && player->IsAlive())
                for (uint8 cardinal = 0; cardinal < 4; ++cardinal)
                    if (pairOnMark[cardinal] == PairOf(number))
                        GroundIndicators::SetUnitSpot(me, player, MarkSpot(cardinal), 0.8f, RollCallMs + 500);
        // The call answered: his blow at each of them, seen and harmless, growing out of him to its full size as the
        // call resolves - a pair well placed lays two cones on its mark, one out of place a cone of its own across
        // the others
        scheduler.Schedule(Milliseconds(RollCallMs - GroundIndicators::WardenRollCallBlowGrowMs), [this](TaskContext)
        {
            Position const from = me->GetPosition();
            for (Player* player : ArenaPlayers())
                if (player->IsAlive())
                    GroundIndicators::ShowWardenRollCallBlow(me, from, me->GetAngle(player));
            me->SendPlaySpellVisual(KIT_STRIKE);
        });
        scheduler.Schedule(Milliseconds(RollCallMs), [this, pairOnMark](TaskContext)
        {
            EndCast();
            Sound("Vorhan.RollCallEnd");
            ResolveRollCall(pairOnMark);
            ClearNumbers();
            Rally();
        });
    }

    void ResolveRollCall(std::array<uint8, 4> const& pairOnMark)
    {
        std::array<std::vector<Player*>, 4> onMark;
        std::vector<Player*> nowhere;
        for (Player* player : ArenaPlayers())
        {
            int32 found = -1;
            for (uint8 cardinal = 0; cardinal < 4 && found < 0; ++cardinal)
                if (Position const spot = MarkSpot(cardinal); player->GetExactDist2d(&spot) <= MarkReach)
                    found = cardinal;
            if (found < 0)
                nowhere.push_back(player);
            else
                onMark[found].push_back(player);
        }

        std::vector<std::pair<Player*, uint32>> doomed;
        for (uint8 cardinal = 0; cardinal < 4; ++cardinal)
        {
            std::vector<Player*> const& here = onMark[cardinal];
            if (here.size() > 2)
            {
                for (Player* player : here)
                    doomed.emplace_back(player, SPELL_GATHERING);
                continue;
            }
            for (Player* player : here)
            {
                uint8 const number = NumberOf(player);
                if (!number || PairOf(number) != pairOnMark[cardinal])
                {
                    doomed.emplace_back(player, SPELL_WRONG_NUMBER);
                    continue;
                }
                // Alone: unless the partner of the pair is dead (a body answers no roll call)
                if (here.size() == 1)
                {
                    uint8 const other = (number % 2) ? number + 1 : number - 1;
                    Player* partner = PlayerOf(other);
                    if (partner && partner->IsAlive())
                        doomed.emplace_back(player, SPELL_ABSENT);
                }
            }
        }
        for (Player* player : nowhere)
            if (NumberOf(player))
            {
                doomed.emplace_back(player, SPELL_ABSENT);
                for (uint8 cardinal = 0; cardinal < 4; ++cardinal)
                    if (Position const spot = MarkSpot(cardinal); pairOnMark[cardinal] == PairOf(NumberOf(player)))
                        LOG_INFO("module.vorhan", "Vorhan roll call instance={} {} number {} {:.1f} yd from its mark{}",
                                 me->GetInstanceId(), player->GetName(), NumberOf(player),
                                 player->GetExactDist2d(&spot), player->GetSession() &&
                                 player->GetSession()->IsBot() ? " (bot)" : "");
            }

        if (doomed.empty())
        {
            Kept("roll call");
            return;
        }
        std::string who;
        for (auto const& [player, spell] : doomed)
        {
            who += Acore::StringFormat(" {}({})", player->GetName(), spell);
            Doom(player, spell);
        }
        Broken("roll call", who);
    }

    // Charge du geôlier: he goes to the middle and the walls rise at the edge; 4 s later he throws everyone back. From
    // under him it lands short of the walls; farther, into them. With the Roll Call: called as they land.
    void Charge(bool rollCall)
    {
        MarkAway();
        Cast(CAST_CHARGE);
        me->NearTeleportTo(ArenaCenter.GetPositionX(), ArenaCenter.GetPositionY(), Ground(ArenaCenter).GetPositionZ(),
                           me->GetOrientation());
        if (rollCall)
            AssignNumbers(ChargeWallsMs + ChargeLandMs + RollCallMs + 1000);
        Placing(ChargeWallsMs);
        // Through its landing and its roll call: an axe fitted in the gap between the throw and the call landed on
        // the marks (2026-10-09)
        HoldAxe(ChargeWallsMs + ChargeLandMs + (rollCall ? RollCallMs : 0));
        ShowWalls(WallsLastMs);
        // The walls for the bots: the band past them is death, never to be walked into once landed
        GroundIndicators::Area walls;
        walls.kind = GroundIndicators::Area::Kind::Ring;
        walls.origin = Ground(ArenaCenter);
        walls.radius = WallRadius + 15.0f;
        walls.inner = WallKillRadius - 2.0f;
        GroundIndicators::WatchArea(me, walls, WallsLastMs, uint32(Reference() * 10.0f));
        Sound("Vorhan.Walls");
        std::vector<Player*> players = ArenaPlayers();
        for (std::size_t index = 0; index < players.size(); ++index)
        {
            // Under him, each on a side of their own (their number's way): they land spread out round the room
            uint8 const number = NumberOf(players[index]);
            float const angle = ClockAngle(float(number ? number - 1 : index), 8.0f);
            GroundIndicators::SetUnitSpot(me, players[index], AtAngle(ArenaCenter, angle, ChargeSafeRadius / 2.0f),
                1.0f, ChargeWallsMs + 200);
        }
        scheduler.Schedule(Milliseconds(ChargeWallsMs), [this, rollCall](TaskContext)
        {
            EndCast();
            me->SendPlaySpellVisual(KIT_SHOCKWAVE);
            Sound("Vorhan.Charge");
            auto const thrownFrom = std::make_shared<std::map<ObjectGuid, float>>();
            for (Player* player : ArenaPlayers())
            {
                GroundIndicators::EndUnitSpot(me, player);
                (*thrownFrom)[player->GetGUID()] = player->GetExactDist2d(me);
                Throw(player, ThrowSpeedXY(ThrowDistance), ThrowSpeedZ);
            }
            scheduler.Schedule(Milliseconds(ChargeLandMs), [this, rollCall, thrownFrom](TaskContext)
            {
                std::string walled;
                for (Player* player : ArenaPlayers())
                    if (player->GetExactDist2d(&ArenaCenter) >= WallKillRadius)
                    {
                        Doom(player, SPELL_BARRIER);
                        auto const from = thrownFrom->find(player->GetGUID());
                        walled += Acore::StringFormat(" {} (thrown from {:.1f} yd{})", player->GetName(),
                            from == thrownFrom->end() ? -1.0f : from->second, player->GetSession() &&
                            player->GetSession()->IsBot() ? ", bot" : "");
                    }
                if (walled.empty())
                    Kept("charge");
                else
                    Broken("charge", walled);
                if (rollCall)
                    RollCall(true);
                else
                    Rally();
            });
        });
    }

    // --- The riot ----------------------------------------------------------------------------------------------------
    void EnterRiot()
    {
        _phase = Phase::Riot;
        EndCast();
        Talk(SAY_RIOT);
        me->AttackStop();
        me->StopMoving();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(0, Ground(ArenaCenter));
        // Out of reach, still in combat with them (immune to them, he left it and evaded mid-fight)
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        Sound("Vorhan.Riot");
        if (sSpellMgr->GetSpellInfo(CAST_RIOT))
            me->CastSpell(me, CAST_RIOT, false);
    }

    // A wave of prisoners from the lair's edge, each straight on a tank (the fel orcs on one, the abyssal on the
    // other): the area damage moment. Sized on the group's pack damage: a wave lasts about WaveSeconds.
    void RiotWave()
    {
        std::vector<Player*> tanks;
        for (Player* player : ArenaPlayers())
            if (IsGroupTank(player))
                tanks.push_back(player);
        if (tanks.empty())
            tanks = ArenaPlayers();
        if (tanks.empty())
            return;
        float const dealers = Power::GroupDamageDealers(GroupDamage, GroupTanks, GroupHealers);
        float const waveHealth = Power::ExpectedDps(ProfileItemLevel, ProfileParagon, true) * dealers * WaveSeconds;
        // Shares: each fel orc one, the abyssal two
        float const share = waveHealth / float(PrisonersPerWave + 2);
        float const base = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 index = 0; index <= PrisonersPerWave; ++index)
        {
            bool const abyssal = index == PrisonersPerWave;
            float const angle = base + float(index) * 2.0f * float(M_PI) / float(PrisonersPerWave + 1);
            Position const spawn = AtAngle(ArenaCenter, angle, 34.0f);
            TempSummon* prisoner = me->SummonCreature(abyssal ? NPC_ABYSSAL : NPC_PRISONER, spawn,
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            if (!prisoner)
                continue;
            uint32 const health = uint32(share * (abyssal ? 2.0f : 1.0f));
            prisoner->SetCreateHealth(health);
            prisoner->SetMaxHealth(health);
            prisoner->SetFullHealth();
            Player* tank = tanks[abyssal ? tanks.size() - 1 : 0];
            prisoner->SetInCombatWithZone();
            prisoner->GetThreatMgr().AddThreat(tank, 1000000.0f);
            prisoner->AI()->AttackStart(tank);
        }
    }

    // --- Phase 3, "Exécution des peines" ------------------------------------------------------------------------
    void EnterPhase3()
    {
        _phase = Phase::Three;
        Talk(SAY_PHASE_3);
    }

    // An angle given in degrees clockwise from north
    static float ClockDegrees(float degrees)
    {
        return ClockAngle(degrees, 360.0f);
    }

    // Where a spot is round him, in the clock's degrees (clockwise from north): ClockDegrees' other way
    float DegreesOf(Position const& spot) const
    {
        float const angle = Ground(ArenaCenter).GetAngle(&spot);
        return std::fmod(360.0f - angle * 180.0f / float(M_PI), 360.0f);
    }

    // Seat n at the clock: (n - 1) x 45 degrees clockwise from north; its cones' axis the same modulo 180
    static float SeatDegrees(uint8 seat)
    {
        return float((seat - 1) * 45 % 360);
    }

    Position SeatSpot(uint8 seat) const
    {
        return AtAngle(ArenaCenter, ClockDegrees(SeatDegrees(seat)), ExecutionSeatDistance);
    }

    // The degrees round him (1-degree bins) the double cones along these seats' axes cover
    static std::array<bool, 360> CoveredDegrees(std::vector<uint8> const& seats)
    {
        std::array<bool, 360> covered = {};
        int32 const half = int32(ExecutionArcDegrees / 2.0f);
        for (uint8 seat : seats)
            for (int32 end : { int32(SeatDegrees(seat)), int32(SeatDegrees(seat)) + 180 })
                for (int32 offset = -half; offset <= half; ++offset)
                    covered[((end + offset) % 360 + 360) % 360] = true;
        return covered;
    }

    // The gaps of a cover: their middle and width, in degrees
    static std::vector<std::pair<float, int32>> GapsOf(std::array<bool, 360> const& covered)
    {
        std::vector<std::pair<float, int32>> gaps;
        for (int32 start = 0; start < 360; ++start)
        {
            if (covered[start] || !covered[(start + 359) % 360])
                continue;
            int32 length = 0;
            while (length < 360 && !covered[(start + length) % 360])
                ++length;
            gaps.emplace_back(std::fmod(float(start) + float(length) / 2.0f, 360.0f), length);
        }
        return gaps;
    }

    // The seats whose floors burn at a DOOM (the two before it), and with its own the cones then
    std::vector<uint8> BurningSeats(std::vector<std::vector<uint8>> const& calls, uint8 doom) const
    {
        std::vector<uint8> seats;
        for (uint8 earlier = doom >= 2 ? uint8(doom - 2) : uint8(0); earlier < doom; ++earlier)
            seats.insert(seats.end(), calls[earlier].begin(), calls[earlier].end());
        return seats;
    }

    // An order of the seats cut into DOOMs that can be solved: every seat called off the burning floors, the group a
    // gap of ExecutionMinGapDegrees at every DOOM (288 of the 40320 orders; scratch seatOrders.py). Drawn at random
    // until one is; a known one if none turns up.
    std::vector<std::vector<uint8>> DrawExecutionOrder() const
    {
        auto cut = [](std::vector<uint8> const& order)
        {
            std::vector<std::vector<uint8>> calls;
            std::size_t index = 0;
            for (uint8 count : ExecutionDooms)
            {
                calls.emplace_back(order.begin() + index, order.begin() + index + count);
                index += count;
            }
            return calls;
        };
        auto solvable = [this](std::vector<std::vector<uint8>> const& calls)
        {
            for (uint8 doom = 0; doom < calls.size(); ++doom)
            {
                std::vector<uint8> seats = BurningSeats(calls, doom);
                std::array<bool, 360> const burning = CoveredDegrees(seats);
                for (uint8 seat : calls[doom])
                    if (burning[int32(SeatDegrees(seat))])
                        return false;
                seats.insert(seats.end(), calls[doom].begin(), calls[doom].end());
                bool gap = false;
                for (auto const& [middle, width] : GapsOf(CoveredDegrees(seats)))
                    gap = gap || width >= ExecutionMinGapDegrees;
                if (!gap)
                    return false;
            }
            return true;
        };
        std::vector<uint8> order = { 1, 2, 3, 4, 5, 6, 7, 8 };
        for (uint32 attempt = 0; attempt < 5000; ++attempt)
        {
            Acore::Containers::RandomShuffle(order);
            std::vector<std::vector<uint8>> calls = cut(order);
            if (solvable(calls))
                return calls;
        }
        return cut({ 1, 2, 5, 6, 3, 4, 7, 8 });
    }

    // Where the group stands at a DOOM, in degrees: the middle of a gap outside its cones and the floors burning then,
    // the gap nearest where the group stood (the widest at the first) of those wide enough to hold it -
    // ExecutionGroupGapDegrees when there is one, else ExecutionMinGapDegrees (every DOOM of a drawn order has one).
    // The nearest of all once sent the group to a sliver of 5 degrees between two cones, and the DOOM struck it.
    float ExecutionGroupDegrees(uint8 doom) const
    {
        std::vector<uint8> seats = BurningSeats(_executionCalls, doom);
        seats.insert(seats.end(), _executionCalls[doom].begin(), _executionCalls[doom].end());
        std::vector<std::pair<float, int32>> gaps = GapsOf(CoveredDegrees(seats));
        // The floor of three DOOMs back still burns as the group is sent off, for its first two seconds: kept clear of
        // too when a gap wide enough is left (the bots held their spot on it, and it struck them twice)
        if (doom >= 3)
        {
            std::vector<uint8> stricter = seats;
            stricter.insert(stricter.end(), _executionCalls[doom - 3].begin(), _executionCalls[doom - 3].end());
            std::vector<std::pair<float, int32>> const clear = GapsOf(CoveredDegrees(stricter));
            if (std::any_of(clear.begin(), clear.end(),
                    [](auto const& gap) { return gap.second >= ExecutionGroupGapDegrees; }))
                gaps = clear;
        }
        for (int32 wide : { ExecutionGroupGapDegrees, ExecutionMinGapDegrees })
            if (std::any_of(gaps.begin(), gaps.end(), [wide](auto const& gap) { return gap.second >= wide; }))
            {
                std::erase_if(gaps, [wide](auto const& gap) { return gap.second < wide; });
                break;
            }
        float best = _executionGroupDegrees;
        float bestScore = 1.0e9f;
        for (auto const& [middle, width] : gaps)
        {
            float const score = doom == 0 ? -float(width) :
                std::fabs(std::remainder(middle - _executionGroupDegrees, 360.0f));
            if (score < bestScore)
            {
                bestScore = score;
                best = middle;
            }
        }
        return best;
    }

    GroundIndicators::Area ExecutionCone(float way) const
    {
        GroundIndicators::Area cone;
        cone.kind = GroundIndicators::Area::Kind::Cone;
        cone.origin = Ground(ArenaCenter);
        cone.origin.SetOrientation(Position::NormalizeOrientation(way));
        cone.radius = ExecutionReach;
        cone.arc = ExecutionArcDegrees * float(M_PI) / 180.0f;
        return cone;
    }

    // Exécution des peines: the seats shown round him, an order of them drawn; a clock ticks each second, the DOOM
    // on each call's cones
    void Execution()
    {
        MarkAway();
        Cast(CAST_EXECUTION);
        me->NearTeleportTo(ArenaCenter.GetPositionX(), ArenaCenter.GetPositionY(), Ground(ArenaCenter).GetPositionZ(),
                           me->GetOrientation());
        uint32 const dooms = uint32(ExecutionDooms.size());
        uint32 const lasts = ExecutionCastMs + dooms * ExecutionEveryMs;
        AssignNumbers(lasts + 1000);
        Placing(lasts);
        HoldAxe(lasts);
        _executionEndMs = Elapsed() + lasts + 500;
        _executionOn = true;
        _burnt.clear();
        _burntStruck.clear();
        _executionCalls = DrawExecutionOrder();
        std::string drawn;
        for (std::vector<uint8> const& call : _executionCalls)
        {
            drawn += " ";
            for (uint8 seat : call)
                drawn += std::to_string(seat);
        }
        LOG_INFO("module.vorhan", "Vorhan execution instance={} order:{}", me->GetInstanceId(), drawn);
        Sound("Vorhan.RollCall");
        for (uint8 seat = 1; seat <= 8; ++seat)
        {
            Position const spot = SeatSpot(seat);
            GroundIndicators::ShowCell(me, spot, seat, ArenaCenter.GetAngle(&spot), lasts, ExecutionSeatRadius);
        }
        // The group to its first gap during the cast
        _executionGroupDegrees = 0.0f;
        _executionGroupDegrees = ExecutionGroupDegrees(0);
        for (Player* player : ArenaPlayers())
            RouteTo(player, _executionGroupDegrees, ExecutionGroupDistance, 2.0f, ExecutionCastMs + 500);
        // The clock: a tick each second, but on the DOOMs
        for (uint32 second = 1; second * 1000 <= dooms * ExecutionEveryMs; ++second)
        {
            if (second % (ExecutionEveryMs / 1000) == 0)
                continue;
            scheduler.Schedule(Milliseconds(ExecutionCastMs + second * 1000), [this, second](TaskContext)
            {
                if (_executionOn)
                    Sound(second % 2 ? "Vorhan.ExecutionTick" : "Vorhan.ExecutionTock");
            });
        }
        scheduler.Schedule(Milliseconds(ExecutionCastMs), [this](TaskContext) { ExecutionCall(0); });
    }

    // A bot's spot for this DOOM (degrees round him, at distance). The way there is the bots' own: round every floor
    // burning or about to, through the middle when the floors close both sides (GroundIndicators::RouteAround). A
    // timed detour of the script's, in at his feet and round, went wrong whenever a bot was a moment late.
    void RouteTo(Player* player, float degrees, float distance, float radius, uint32 durationMs)
    {
        GroundIndicators::SetUnitSpot(me, player, AtAngle(Ground(ArenaCenter), ClockDegrees(degrees), distance), radius,
                                      durationMs);
    }

    // A seat called, until its DOOM: its runes flaring on every tick, its number hanging big over it
    void ShowSeatCall(uint8 seat, uint32 durationMs)
    {
        Position const spot = SeatSpot(seat);
        for (uint32 at = 0; at < durationMs; at += SeatFlareEveryMs)
            scheduler.Schedule(Milliseconds(at), [this, spot](TaskContext)
            {
                if (_executionOn)
                    GroundIndicators::FlareCell(me, spot, ExecutionSeatRadius);
            });
        if (Creature* sign = me->SummonCreature(NPC_STALKER, spot, TEMPSUMMON_TIMED_DESPAWN, durationMs + 500))
        {
            sign->SetObjectScale(SeatNumberScale);
            GroundIndicators::ShowCarriedNumber(sign, seat, durationMs);
        }
    }

    // A line in the middle of the players' screens (the raid boss emote frame), in their language; whisper: to one
    // player alone, in its own colour
    void Announce(Player* player, std::string const& french, std::string const& english, bool whisper = false)
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBot())
            return;
        bool const isFrench = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
        WorldPacket packet;
        ChatHandler::BuildChatPacket(packet, whisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_RAID_BOSS_EMOTE,
            LANG_UNIVERSAL, me, player, isFrench ? french : english);
        player->GetSession()->SendPacket(&packet);
    }

    void AnnounceSeats(std::vector<uint8> const& seats)
    {
        std::string numbers = std::to_string(seats.front());
        std::string french = "Siège " + numbers;
        std::string english = "Seat " + numbers;
        if (seats.size() > 1)
        {
            french = "Sièges " + numbers + " et " + std::to_string(seats[1]);
            english = "Seats " + numbers + " and " + std::to_string(seats[1]);
        }
        for (Player* player : Listeners())
        {
            uint8 const number = NumberOf(player);
            if (std::find(seats.begin(), seats.end(), number) != seats.end())
                Announce(player, Acore::StringFormat("|cffff3b1fVOTRE SIÈGE : {} !|r", number),
                    Acore::StringFormat("|cffff3b1fYOUR SEAT: {}!|r", number), true);
            else
                Announce(player, "|cffffb020" + french + " !|r", "|cffffb020" + english + "!|r");
        }
    }

    // A DOOM's seats called: their numbers' players to them, their cones shown along the seats' axes, the group to its
    // gap; his feet warned just before it strikes
    void ExecutionCall(uint8 doom)
    {
        if (!_executionOn)
            return;
        std::vector<uint8> const& seats = _executionCalls[doom];
        _executionGroupDegrees = ExecutionGroupDegrees(doom);
        Position const center = Ground(ArenaCenter);
        uint32 const damage = MistakeDamage;
        std::vector<ObjectGuid> called;
        for (uint8 seat : seats)
        {
            // Each cone aimed at the number whose seat it covers, when that number is called: the bots hold their
            // seat in their own cone (a cone aimed at them) and everyone else leaves it
            for (float end : { SeatDegrees(seat), SeatDegrees(seat) + 180.0f })
            {
                Player* holder = nullptr;
                for (uint8 called : seats)
                    if (int32(SeatDegrees(called)) == int32(std::fmod(end, 360.0f)))
                        if (Player* number = PlayerOf(called); number && number->IsAlive())
                            holder = number;
                // His feet clear for the bots until their own warning (ExecutionFeetWarnMs before the DOOM): the way
                // across when the floors close both sides of a bot
                if (holder)
                    GroundIndicators::ShowAimedCone(me, center, ClockDegrees(end), ExecutionReach, ExecutionArcDegrees,
                        ExecutionEveryMs, holder, GroundIndicators::Theme::None, damage, ExecutionSafe);
                else
                    GroundIndicators::ShowCone(me, center, ClockDegrees(end), ExecutionReach, ExecutionArcDegrees,
                        ExecutionEveryMs, GroundIndicators::Theme::None, damage, ExecutionSafe);
            }
            ShowSeatCall(seat, ExecutionEveryMs);
            if (Player* player = PlayerOf(seat); player && player->IsAlive())
            {
                called.push_back(player->GetGUID());
                RouteTo(player, SeatDegrees(seat), ExecutionSeatDistance, 1.0f, ExecutionEveryMs + 500);
            }
        }
        for (Player* player : ArenaPlayers())
            if (std::find(called.begin(), called.end(), player->GetGUID()) == called.end())
                RouteTo(player, _executionGroupDegrees, ExecutionGroupDistance, 2.0f, ExecutionEveryMs + 500);
        AnnounceSeats(seats);
        if (doom == ExecutionCurfewDoom)
            Curfew(ExecutionEveryMs);
        scheduler.Schedule(Milliseconds(ExecutionEveryMs - ExecutionFeetWarnMs), [this](TaskContext)
        {
            if (_executionOn)
                GroundIndicators::ShowCircle(me, Ground(ArenaCenter), ExecutionSafe, ExecutionFeetWarnMs,
                    GroundIndicators::Theme::Fire, MistakeDamage);
        });
        scheduler.Schedule(Milliseconds(ExecutionEveryMs), [this, doom](TaskContext) { ExecutionStrikes(doom); });
    }

    // The DOOM: each seat's double cone along its axis; everyone in one struck but the seat's own number on it (two at
    // once: Sursis, then death), whoever stands at his feet struck too, a number off its seat punished. The floors
    // burn a moment later, from ExecutionSafe out.
    void ExecutionStrikes(uint8 doom)
    {
        if (!_executionOn)
            return;
        Position const center = Ground(ArenaCenter);
        std::vector<uint8> const& seats = _executionCalls[doom];
        me->SetFacingTo(ClockDegrees(SeatDegrees(seats.front())));
        me->SendPlaySpellVisual(KIT_STRIKE);
        Sound("Vorhan.ExecutionDoom");

        std::string struck;
        for (uint8 seat : seats)
        {
            Player* owner = PlayerOf(seat);
            Position const spot = SeatSpot(seat);
            if (owner && owner->IsAlive() && owner->GetExactDist2d(&spot) > ExecutionSeatRadius)
            {
                Hit(owner, SPELL_OFF_SEAT, ExecutionOffSeatPct, true);
                Broken("execution seat", Acore::StringFormat("{} number {} {:.1f} yd from it", owner->GetName(), seat,
                    owner->GetExactDist2d(&spot)));
            }
            for (float end : { SeatDegrees(seat), SeatDegrees(seat) + 180.0f })
            {
                GroundIndicators::Area const cone = ExecutionCone(ClockDegrees(end));
                GroundIndicators::ShowWardenRollCallBlow(me, center, cone.origin.GetOrientation());
                for (Player* player : ArenaPlayers())
                    if (player->IsAlive() && !SparedBy(player, seat, seats) && cone.Contains(player->GetPosition()))
                    {
                        ExecutionStrike(player, SPELL_EXECUTION, ExecutionPct);
                        // Where it stood: degrees round him (his clock's) and yards from him
                        struck += Acore::StringFormat(" {}@{:.0f}deg/{:.1f}yd", player->GetName(),
                            DegreesOf(player->GetPosition()), player->GetExactDist2d(&center));
                    }
                GroundIndicators::Area floor = cone;
                floor.inner = ExecutionSafe;
                uint32 const now = Elapsed();
                uint32 const until = std::min(now + BurntDelayMs + BurntLastsMs, _executionEndMs);
                _burnt.push_back({ floor, now + BurntDelayMs, until });
                // For the bots from now on, as a floor about to burn: they step off it before it does, and walk round
                // it on their way (GroundIndicators::RouteAround). It is drawn once it burns.
                GroundIndicators::WatchArea(me, floor, until - now, MistakeDamage);
                scheduler.Schedule(Milliseconds(BurntDelayMs), [this, floor, until](TaskContext)
                {
                    uint32 const now = Elapsed();
                    if (!_executionOn || now >= until)
                        return;
                    GroundIndicators::ShowWardenMark(me, floor.origin, floor.origin.GetOrientation(),
                                                     GroundIndicators::SPELL_WARDEN_BURNT_FLOOR, until - now);
                });
            }
        }
        // His feet: never there when the DOOM strikes
        for (Player* player : ArenaPlayers())
            if (player->IsAlive() && player->GetExactDist2d(&center) < ExecutionSafe)
            {
                ExecutionStrike(player, SPELL_EXECUTION, ExecutionPct);
                struck += " " + player->GetName() + "(feet)";
            }
        if (struck.empty())
            Kept("execution");
        else
            Broken("execution", Acore::StringFormat("doom {} (group to {:.0f}deg):{}", doom + 1,
                _executionGroupDegrees, struck));

        if (doom + 1u < ExecutionDooms.size())
            ExecutionCall(uint8(doom + 1));
        else
            scheduler.Schedule(Milliseconds(_executionEndMs > Elapsed() ? _executionEndMs - Elapsed() : 0),
                               [this](TaskContext) { EndExecution(); });
    }

    // A number called at this DOOM is spared by the cones along its own seat's axis (two seats called opposite each
    // other share one: each stands in the other's)
    bool SparedBy(Player const* player, uint8 seat, std::vector<uint8> const& seats) const
    {
        uint8 const number = NumberOf(player);
        return number && std::find(seats.begin(), seats.end(), number) != seats.end() &&
               int32(SeatDegrees(number)) % 180 == int32(SeatDegrees(seat)) % 180;
    }

    // An execution's strike (a cone, his feet, or a step on its floor): the first puts the player under Sursis, a
    // second under it is death
    void ExecutionStrike(Player* player, uint32 spellId, float percent)
    {
        uint32 const now = Elapsed();
        uint32& until = _sursis[player->GetGUID()];
        if (now < until)
        {
            Doom(player, spellId);
            return;
        }
        Hit(player, spellId, percent, false);
        if (player->IsAlive())
        {
            until = now + SursisMs;
            AddTimedAura(player, SPELL_SURSIS, SursisMs);
        }
    }

    // Whoever stands on a burning floor is struck, at most once a second; floors gone out are forgotten
    void CheckBurntFloor()
    {
        if (!_executionOn || _burnt.empty())
            return;
        uint32 const now = Elapsed();
        std::erase_if(_burnt, [now](BurntFloor const& floor) { return now >= floor.until; });
        for (Player* player : ArenaPlayers())
        {
            bool on = false;
            for (BurntFloor const& floor : _burnt)
                if (now >= floor.from && floor.area.Contains(player->GetPosition()))
                {
                    on = true;
                    break;
                }
            if (!on)
                continue;
            uint32& last = _burntStruck[player->GetGUID()];
            if (last && now < last + BurntEveryMs)
                continue;
            last = now;
            ExecutionStrike(player, SPELL_BURNT, BurntPct);
            Broken("burnt floor", Acore::StringFormat("{}@{:.0f}deg/{:.1f}yd{} (group to {:.0f}deg)", player->GetName(),
                DegreesOf(player->GetPosition()), player->GetExactDist2d(&ArenaCenter),
                player->isMoving() ? " moving" : "", _executionGroupDegrees));
        }
    }

    void EndExecution()
    {
        if (!_executionOn)
            return;
        _executionOn = false;
        _burnt.clear();
        _burntStruck.clear();
        _executionCalls.clear();
        EndCast();
        ClearNumbers();
        for (Player* player : ArenaPlayers())
            GroundIndicators::EndUnitSpot(me, player);
        Rally();
    }

    // Mise au cachot: a player who is not a tank caged where they stand; the group breaks the cage or they die. With
    // the curfew: it sounds while they break it.
    void Cage(bool curfew)
    {
        std::vector<Player*> candidates;
        for (Player* player : ArenaPlayers())
            if (!IsGroupTank(player))
                candidates.push_back(player);
        if (candidates.empty() || !_cage.IsEmpty())
            return;
        Player* prisoner = Acore::Containers::SelectRandomContainerElement(candidates);
        ObjectGuid const guid = prisoner->GetGUID();
        Cast(CAST_CAGE);
        HoldAxe(CageCastMs + (curfew ? CageCurfewMs : 0));
        if (curfew)
            Curfew(CageCastMs + CageCurfewMs);
        scheduler.Schedule(Milliseconds(CageCastMs), [this, guid](TaskContext)
        {
            EndCast();
            Player* prisoner = ObjectAccessor::GetPlayer(*me, guid);
            if (!prisoner || !prisoner->IsAlive())
                return;
            TempSummon* cage = me->SummonCreature(NPC_CAGE, prisoner->GetPosition(), TEMPSUMMON_MANUAL_DESPAWN);
            if (!cage)
                return;
            float const dealers = Power::GroupDamageDealers(GroupDamage, GroupTanks, GroupHealers);
            uint32 const health = uint32(Power::ExpectedDps(ProfileItemLevel, ProfileParagon) * dealers *
                                         CageBreakSeconds);
            cage->SetCreateHealth(health);
            cage->SetMaxHealth(health);
            cage->SetFullHealth();
            if (sSpellMgr->GetSpellInfo(SPELL_CAGE))
                cage->AddAura(SPELL_CAGE, cage);
            cage->SetInCombatWithZone();
            prisoner->StopMoving();
            AddTimedAura(prisoner, SPELL_CAGED, CageMs);
            Sound("Vorhan.Isolation", prisoner);
            _cage = cage->GetGUID();
            _caged = guid;
            // The skull on it: the bots break it first
            if (Group* group = prisoner->GetGroup())
                group->SetTargetIcon(7, ObjectGuid::Empty, _cage);
            scheduler.Schedule(Milliseconds(CageMs), [this](TaskContext) { ResolveCage(false); });
        });
    }

    // The cage broken (its death, npc_warden_vorhan_cage), or its time over
    void ResolveCage(bool broken)
    {
        if (_cage.IsEmpty())
            return;
        Player* prisoner = ObjectAccessor::GetPlayer(*me, _caged);
        if (prisoner)
        {
            prisoner->RemoveAurasDueToSpell(SPELL_CAGED);
            if (broken)
                Kept("cage");
            else if (prisoner->IsAlive())
            {
                Doom(prisoner, SPELL_CAGE_DEATH);
                Broken("cage", prisoner->GetName());
            }
        }
        ClearCage();
    }

    void ClearCage()
    {
        if (_cage.IsEmpty())
            return;
        Creature* cage = me->GetMap()->GetCreature(_cage);
        if (Player* prisoner = ObjectAccessor::GetPlayer(*me, _caged))
        {
            prisoner->RemoveAurasDueToSpell(SPELL_CAGED);
            if (Group* group = prisoner->GetGroup(); group && group->GetTargetIcon(7) == _cage)
                group->SetTargetIcon(7, ObjectGuid::Empty, ObjectGuid::Empty);
        }
        _cage.Clear();
        _caged.Clear();
        if (cage && cage->IsAlive())
            cage->DespawnOrUnsummon(0ms);
    }

public:
    void CageBroken(Creature* cage)
    {
        if (cage->GetGUID() == _cage)
            ResolveCage(true);
    }

private:
    // Châtiment exemplaire: a cone of its own look on his tank; whoever stands in it shares it. Nobody: everyone dies;
    // one alone dies, and everyone is hit; two or more share it.
    void Punishment()
    {
        Player* tank = me->GetVictim() ? me->GetVictim()->ToPlayer() : nullptr;
        if (!tank)
            return;
        Cast(CAST_PUNISHMENT);
        _busyUntil = std::max(_busyUntil, Elapsed() + PunishmentCastMs + 500);
        Position const from = Ground(me->GetPosition());
        float const way = me->GetAngle(tank);
        me->SetFacingTo(way);
        GroundIndicators::Area cone;
        cone.kind = GroundIndicators::Area::Kind::Cone;
        cone.origin = from;
        cone.origin.SetOrientation(way);
        cone.radius = PunishmentReach;
        cone.arc = PunishmentArcDegrees * float(M_PI) / 180.0f;
        GroundIndicators::ShowWardenMark(me, from, way, GroundIndicators::SPELL_WARDEN_PUNISHMENT,
                                         PunishmentCastMs + 300);
        // The bots: the tanks stay in it (the other one goes in beside the first), everyone else leaves it
        GroundIndicators::WatchArea(me, cone, PunishmentCastMs, uint32(Reference() * PunishmentPct / 200.0f), true);
        if (Player* other = OtherTank(tank))
            GroundIndicators::SetOffTankSpot(me, AtAngleFrom(from, way, PunishmentOffTankDistance),
                                             PunishmentCastMs + 500, true, other);
        Sound("Vorhan.Chains");
        scheduler.Schedule(Milliseconds(PunishmentCastMs), [this, cone](TaskContext)
        {
            EndCast();
            me->SendPlaySpellVisual(KIT_STRIKE);
            Sound("Vorhan.ExecutionDoom");
            GroundIndicators::ShowWardenStrike(me, cone.origin, cone.origin.GetOrientation());
            std::vector<Player*> inside;
            for (Player* player : ArenaPlayers())
                if (cone.Contains(player->GetPosition()))
                    inside.push_back(player);
            if (inside.empty())
            {
                for (Player* player : ArenaPlayers())
                    Doom(player, SPELL_PUNISHMENT);
                Broken("punishment", "nobody in it");
                return;
            }
            if (inside.size() == 1)
            {
                Doom(inside.front(), SPELL_PUNISHMENT);
                HitEveryone(SPELL_PUNISHMENT, PunishmentAlonePct);
                Broken("punishment", inside.front()->GetName() + " alone");
                return;
            }
            for (Player* player : inside)
                Hit(player, SPELL_PUNISHMENT, PunishmentPct / float(inside.size()), false);
            Kept("punishment");
        });
    }

    // Faux du geôlier: the third of the room it sweeps warned as a cone with the volley's lines, then the blade swept
    // through it from one side to the other as they land: whoever it passes is struck, once
    void Sweep(Position const& from, float towards)
    {
        float const turn = SweepDegrees * float(M_PI) / 180.0f * (urand(0, 1) ? 1.0f : -1.0f);
        float const start = Position::NormalizeOrientation(towards - turn / 2.0f);
        uint32 const damage = MistakeDamage;
        GroundIndicators::ShowCone(me, from, towards, ExecutionReach, SweepDegrees, AxeWarnMs,
                                   GroundIndicators::Theme::None, damage);
        float const perSecond = turn * 1000.0f / float(SweepMs);
        scheduler.Schedule(Milliseconds(AxeWarnMs), [this, from, start, perSecond](TaskContext)
        {
            Position origin = from;
            origin.SetOrientation(start);
            GroundIndicators::Area const blade = GroundIndicators::ShowSweepingLine(me, origin, start, perSecond,
                ExecutionReach, AxeWidth, SweepMs, SPELL_AXE_SWING_HIT, MistakeDamage);
            auto const struck = std::make_shared<std::set<ObjectGuid>>();
            for (uint32 at = 0; at <= SweepMs; at += SweepCheckMs)
                scheduler.Schedule(Milliseconds(at), [this, blade, perSecond, at, struck](TaskContext)
                {
                    GroundIndicators::Area const now = GroundIndicators::CurrentSweep(blade, perSecond, at);
                    for (Player* player : ArenaPlayers())
                        if (!struck->count(player->GetGUID()) && now.Contains(player->GetPosition()))
                        {
                            struck->insert(player->GetGUID());
                            Hit(player, SPELL_SWEEP, AxePct, true);
                            Broken("sweep", player->GetName());
                        }
                });
        });
    }

    void EnterPhase2()
    {
        _phase = Phase::Two;
        Talk(SAY_PHASE_2);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->ClearEmoteState();
        me->InterruptNonMeleeSpells(false);
        me->SetReactState(REACT_AGGRESSIVE);
        Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0, 0.0f, true);
        if (!target)
            for (Player* player : ArenaPlayers())
                if (IsGroupTank(player) || !target)
                    target = player;
        if (target)
            AttackStart(target);
    }

    // --- The end -----------------------------------------------------------------------------------------------------
    void EnterLifeSentence()
    {
        _phase = Phase::LifeSentence;
        Talk(SAY_LIFE_SENTENCE);
        if (sSpellMgr->GetSpellInfo(CAST_LIFE_SENTENCE))
            me->CastSpell(me, CAST_LIFE_SENTENCE, true);
    }

    void LifeSentenceHit()
    {
        float const percent = LifeSentencePct * std::pow(LifeSentenceGrowth, float(_lifeSentences++));
        GroundIndicators::WarnGroupDamage(me, 1000);
        me->SendPlaySpellVisual(KIT_SHOUT);
        Sound("Vorhan.LifeSentence");
        HitEveryone(SPELL_LIFE_SENTENCE, percent);
    }

    void HardEnrage()
    {
        Talk(SAY_HARD_ENRAGE);
        me->SendPlaySpellVisual(KIT_SHOUT);
        Sound("Vorhan.LifeSentence");
        for (Player* player : ArenaPlayers())
            Doom(player, SPELL_CAPITAL);
        Wipe();
    }

    // Nobody left in the lair at all: the fight is over (combat would not tell during the riot). Players down with bots
    // still standing are the board's wipe (RaidFinder UpdateChallengeWipe): it stops the bots and resets him.
    void CheckPlayersStanding()
    {
        if (ArenaPlayers().empty())
            Wipe();
    }

    // --- The music: our sound engine's looping track, to the players near the lair at the pull (FightMusic.h: a later
    // attempt's music is never ended by this one's end) -------------------------------------------------------------
    void EndMusic()
    {
        for (ObjectGuid const& guid : _fightListeners)
            if (FightMusic::Release(guid, me->GetGUID()))
                if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                    if (player->GetSession() && !player->GetSession()->IsBot())
                        EvolutionsAudio::StopMusic(player);
        _fightListeners.clear();
    }

    // --- The visuals: the painted marks (GroundIndicators), the cage, the bots' orders -------------------------------
    // The number, for its rule: its debuff, and painted over the head
    void ShowNumber(Player* player, uint8 number, uint32 durationMs)
    {
        AddTimedAura(player, SPELL_NUMBER_FIRST + number - 1, durationMs);
        GroundIndicators::ShowCarriedNumber(player, number, durationMs);
    }

    void ClearNumbers()
    {
        for (auto const& [guid, number] : _numbers)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
            {
                GroundIndicators::ClearCarriedNumber(player);
                player->RemoveAurasDueToSpell(SPELL_NUMBER_FIRST + number - 1);
            }
    }

    // Thrown back from him: a player's client flies the knockback, a bot is flown by the server (its client-less
    // session took the knockback's packet late, if at all: bots were never thrown)
    void Throw(Player* player, float speedXY, float speedZ)
    {
        if (player->GetSession() && player->GetSession()->IsBot())
        {
            player->StopMoving();
            player->GetMotionMaster()->Clear();
            player->GetMotionMaster()->MoveKnockbackFromForPlayer(me->GetPositionX(), me->GetPositionY(), speedXY,
                                                                  speedZ);
        }
        else
            player->KnockbackFrom(me->GetPositionX(), me->GetPositionY(), speedXY, speedZ);
    }

    // No axe blow lands while a rule goes on for this long (its placement, its curfew, its roll call): a blow in a
    // rule asks the players to be in two places at once. A blow starting just after it is over is free.
    void HoldAxe(uint32 durationMs)
    {
        _busyUntil = std::max(_busyUntil, Elapsed() + durationMs + 1000);
    }

    // A rule places everyone for this long: his tank does not pull him back to the middle meanwhile
    void Placing(uint32 durationMs)
    {
        _placingUntil = std::max(_placingUntil, Elapsed() + durationMs + 500);
    }

    // A rule's sound, to the players near the lair, heard from where it happens (our sound engine)
    void Sound(std::string_view key, WorldObject const* from = nullptr)
    {
        Position const where = (from ? from : me)->GetPosition();
        for (Player* player : Listeners())
            if (player->GetSession() && !player->GetSession()->IsBot())
                EvolutionsAudio::PlayAt(player, key, where);
    }

    // A cell's seal: its iron, its turning runes and its number, the number's top towards the cell's way out from the
    // middle (upright to someone standing there)
    void ShowCell(uint8 number, uint32 durationMs)
    {
        Position const spot = CellSpot(number);
        GroundIndicators::ShowCell(me, spot, number, ArenaCenter.GetAngle(&spot), durationMs, CellRadius);
        // The cage low while they find their cells; it rises as the doors slam (DropCage)
        GroundIndicators::ShowWardenCellBars(me, Ground(spot), durationMs, false);
    }

    // The doors slam: the runes flare, a cage's bars round each cell for a moment
    void DropCage(uint8 number)
    {
        Position const spot = CellSpot(number);
        GroundIndicators::FlareCell(me, spot, CellRadius);
        GroundIndicators::ShowWardenCellBars(me, Ground(spot), CageAfterDoorsMs, true);
        if (Creature* stalker = me->SummonCreature(NPC_STALKER, spot, TEMPSUMMON_TIMED_DESPAWN, 3000))
            if (sSpellMgr->GetSpellInfo(SPELL_CAGE))
                stalker->AddAura(SPELL_CAGE, stalker);
    }

    void ShowRollCallMark(uint8 cardinal, uint8 pair, uint32 durationMs)
    {
        Position const spot = MarkSpot(cardinal);
        GroundIndicators::ShowRollCallMark(me, spot, pair, ArenaCenter.GetAngle(&spot), durationMs, MarkRadius);
    }

    // The electrified walls: a shockwave from the warden out to the room's edge, and the walls of lightning between
    // iron bars rising behind it as it arrives, a circle round the room; they fade out at the end
    void ShowWalls(uint32 durationMs)
    {
        Position const center = Ground(ArenaCenter);
        GroundIndicators::ShowWardenShockwave(me, center);
        GroundIndicators::ShowWardenWall(me, center, durationMs, GroundIndicators::WardenWallRiseDelayMs);
    }

    // His eye on his torso, opening for the whole cast (64 frames, a red burst as it opens), drawn by each player's
    // interface over everything
    void ShowGaze(uint32 durationMs)
    {
        for (Player* player : Listeners())
            SendGaze(player, me->GetGUID(), durationMs, GazeHeight, GazeWidth);
    }

    // The bots: their backs to him from LookAwayLeadMs before the eye opens (in `opensInMs`) until it has: they stop
    // attacking and casting meanwhile, so turned away no longer than it takes
    void LookAway(uint32 opensInMs)
    {
        uint32 const from = opensInMs > LookAwayLeadMs ? opensInMs - LookAwayLeadMs : 0;
        scheduler.Schedule(Milliseconds(from), [this, opensInMs, from](TaskContext)
        {
            GroundIndicators::SetLookAway(me, opensInMs - from + 300);
        });
    }

    // The bots: still from `fromMs` for `forMs`
    void HoldStill(Player* player, uint32 fromMs, uint32 forMs)
    {
        ObjectGuid const guid = player->GetGUID();
        scheduler.Schedule(Milliseconds(fromMs), [this, guid, forMs](TaskContext)
        {
            if (Player* still = ObjectAccessor::GetPlayer(*me, guid))
                GroundIndicators::SetHoldStill(me, still, forMs);
        });
    }

    // --- The deaths, logged for tuning on evidence -------------------------------------------------------------------
    static std::string SpellName(uint32 spellId)
    {
        if (!spellId)
            return "melee";
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
            for (char const* name : info->SpellName)
                if (name && *name)
                    return name;
        return std::to_string(spellId);
    }

    void RecordDeath(Player* player)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return;
        auto const last = _lastHit.find(player->GetGUID());
        std::string const what = last == _lastHit.end() ? std::string("unknown") : SpellName(last->second.spellId);
        ++_deaths[what];
        LOG_INFO("module.vorhan", "Vorhan death instance={} at={:.1f}s phase={} {} number {} ({}): {}",
                 me->GetInstanceId(), Elapsed() / 1000.0f, uint32(_phase), player->GetName(), NumberOf(player),
                 player->GetSession() && player->GetSession()->IsBot() ? "bot" : "player", what);
    }

    void LogSummary(char const* outcome)
    {
        std::string deaths;
        for (auto const& [what, count] : _deaths)
            deaths += Acore::StringFormat("{}{} x{}", deaths.empty() ? "" : ", ", what, count);
        // Each player's damage on him and its share of the time he could be hit (the riot aside)
        float const seconds = std::max(1.0f, (Elapsed() - (Elapsed() > AtPhase2 ? AtPhase2 - AtRiot : 0)) / 1000.0f);
        std::vector<std::pair<uint64, ObjectGuid>> dealt;
        for (auto const& [guid, amount] : _dealt)
            dealt.emplace_back(amount, guid);
        std::sort(dealt.rbegin(), dealt.rend());
        for (auto const& [amount, guid] : dealt)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            LOG_INFO("module.vorhan", "Vorhan damage instance={} {}{}: {} ({:.0f} a second)", me->GetInstanceId(),
                     player ? player->GetName() : guid.ToString(), player && player->GetSession() &&
                     player->GetSession()->IsBot() ? " (bot)" : "", amount, float(amount) / seconds);
        }
        LOG_INFO("module.vorhan", "Vorhan {} instance={} at={:.1f}s phase={} health={:.1f}% deaths=[{}] kept={} "
                 "broken={}", outcome, me->GetInstanceId(), Elapsed() / 1000.0f, uint32(_phase), me->GetHealthPct(),
                 deaths, _rulesKept, _rulesBroken);
    }

    struct Marked
    {
        uint32 since = 0;
        uint32 until = 0;
    };
    struct Escaped
    {
        uint32 stacks = 0;
        uint32 until = 0;
    };
    struct LastHit
    {
        uint32 spellId = 0;                     // 0: his melee
        uint32 at = 0;
        bool avoidable = false;
    };

    SummonList _summons;
    std::vector<Step> _timeline;
    std::size_t _next = 0;
    uint32 _pullMs = 0;
    Phase _phase = Phase::None;
    bool _casting = false;
    bool _rooted = false;
    bool _defiConfirmed = false;
    uint32 _defiWaitMs = 0;
    uint32 _checkTimer = 0;
    uint32 _nextStandingCheckMs = 0;
    uint32 _nextSecondMs = 0;
    uint32 _lastKillYellMs = 0;
    uint32 _rollCalls = 0;
    uint32 _isolations = 0;
    uint32 _lifeSentences = 0;
    uint32 _rulesKept = 0;
    uint32 _rulesBroken = 0;
    uint32 _placingUntil = 0;                   // a rule placing everyone until then (Placing)
    std::map<ObjectGuid, uint8> _numbers;
    std::map<ObjectGuid, Marked> _marks;
    std::map<ObjectGuid, Escaped> _escapes;
    std::map<ObjectGuid, LastHit> _lastHit;
    std::map<std::string, uint32> _deaths;
    std::map<ObjectGuid, uint64> _dealt;
    std::vector<Chain> _chains;
    // Who stood at his side when a rule sent everyone away (MarkAway): Rally() makes it up to them once it is over
    std::set<ObjectGuid> _awayFromHim;
    bool _shacklesOn = false;
    uint32 _nextAxeMs = 0;                      // Coup de hache: no blow before then
    uint32 _busyUntil = 0;                      // a rule still going off after its cast (the isolation's seal)
    std::set<ObjectGuid> _fightListeners;
    ObjectGuid _sparing;                        // the player the hit being dealt marks rather than kills
    ObjectGuid _isolating;                      // the tank Mise à l'isolement never kills
    // Phase 3
    bool _executionOn = false;
    uint32 _executionEndMs = 0;
    struct BurntFloor
    {
        GroundIndicators::Area area;
        uint32 from;                                           // it burns from then (a moment after its DOOM)
        uint32 until;
    };
    std::vector<BurntFloor> _burnt;                            // the execution's floors, burning or about to
    std::map<ObjectGuid, uint32> _burntStruck;                 // when each player was last struck on one
    std::vector<std::vector<uint8>> _executionCalls;           // the seats called at each DOOM, drawn each time
    float _executionGroupDegrees = 0.0f;                       // where the group's gap was at the last DOOM
    std::map<ObjectGuid, uint32> _sursis;                      // until when each player is under Sursis
    ObjectGuid _cage;                                          // the cage standing, and who is in it
    ObjectGuid _caged;
};

// Mise au cachot's cage: it stands where it was shut and fights nothing; broken, it frees its prisoner
struct npc_warden_vorhan_cage : public ScriptedAI
{
    npc_warden_vorhan_cage(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void JustDied(Unit* /*killer*/) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            if (Creature* warden = summon->GetSummonerCreatureBase())
                if (boss_warden_vorhan* ai = dynamic_cast<boss_warden_vorhan*>(warden->AI()))
                    ai->CageBroken(me);
        me->DespawnOrUnsummon(1000ms);
    }

    void UpdateAI(uint32 /*diff*/) override { }
};

// The riot's prisoners: on their tank from the moment they come, blows sized by the warden (PrisonerDamage)
struct npc_warden_vorhan_prisoner : public ScriptedAI
{
    npc_warden_vorhan_prisoner(Creature* creature) : ScriptedAI(creature) { }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->DespawnOrUnsummon(0ms);
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type != DIRECT_DAMAGE)
            return;
        if (TempSummon* summon = me->ToTempSummon())
            if (Creature* warden = summon->GetSummonerCreatureBase())
                if (boss_warden_vorhan* ai = dynamic_cast<boss_warden_vorhan*>(warden->AI()))
                    ai->PrisonerDamage(me, victim, damage);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }
};

boss_warden_vorhan* FindWarden(Player* player)
{
    if (!player || !player->IsInWorld())
        return nullptr;
    Creature* warden = player->FindNearestCreature(NPC_VORHAN, 250.0f, true);
    return warden ? dynamic_cast<boss_warden_vorhan*>(warden->AI()) : nullptr;
}

using namespace Acore::ChatCommands;

// .vorhan info | skip <seconds> | cast <rule> | pull | botsonly <on|off>: for game masters trying the fight
class WardenVorhanCommandScript final : public CommandScript
{
public:
    WardenVorhanCommandScript() : CommandScript("WardenVorhanCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable vorhanTable = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "skip", HandleSkip, SEC_GAMEMASTER, Console::No },
            { "cast", HandleCast, SEC_GAMEMASTER, Console::No },
            { "pull", HandlePull, SEC_GAMEMASTER, Console::No },
            { "botsonly", HandleBotsOnly, SEC_GAMEMASTER, Console::Yes },
            { "fx", HandleFx, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "vorhan", vorhanTable },
        };
        return commandTable;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_warden_vorhan* warden = FindWarden(handler->GetPlayer());
        handler->SendSysMessage(warden ? warden->Describe() : std::string("Vorhan is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_warden_vorhan* warden = FindWarden(handler->GetPlayer());
        if (!warden || !warden->Skip(seconds))
        {
            handler->SendErrorMessage("Vorhan is not fighting within 250 yards.");
            return false;
        }
        handler->SendSysMessage(warden->Describe());
        return true;
    }

    // .vorhan cast <sentence|isolation|cells|cellscurfew|shackles|shacklesgaze|gaze|gazecurfew|rollcall|charge|
    // chargerollcall|wave|cage|cagecurfew|execution|punishment>: one rule now, in a fight
    static bool HandleCast(ChatHandler* handler, std::string what)
    {
        boss_warden_vorhan* warden = FindWarden(handler->GetPlayer());
        if (!warden || !warden->CastNow(what))
        {
            handler->SendErrorMessage("No fighting Vorhan within 250 yards, or no such rule (sentence, isolation, "
                "cells, cellscurfew, shackles, shacklesgaze, gaze, gazecurfew, rollcall, charge, chargerollcall, "
                "wave, cage, cagecurfew, execution, punishment).");
            return false;
        }
        return true;
    }

    // .vorhan fx <gaze|isolation>: a rule's visuals and sounds alone, anywhere, on the game master (or the unit they
    // target), without a fight or any harm - to look at them. gaze: his eye on the target's torso (on one's own when
    // targeting nothing), opening over 6 s. isolation: the blow from 6 yards in front of the game master through them,
    // the furrows of a throw behind them, and 3 s later the seal's burst where they stand.
    static bool HandleFx(ChatHandler* handler, std::string what)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        if (what == "gaze")
        {
            Unit* target = handler->getSelectedUnit();
            Unit* on = target ? target : player;
            // A warden's torso, or a player's chest
            bool const warden = on->GetTypeId() == TYPEID_UNIT && on->ToCreature()->GetEntry() == NPC_VORHAN;
            SendGaze(player, on->GetGUID(), GazeMs, warden ? GazeHeight : 1.3f, warden ? GazeWidth : 2.0f);
            EvolutionsAudio::PlayAt(player, "Vorhan.GazeOpen", on->GetPosition());
            ObjectGuid const guid = player->GetGUID();
            Position const where = on->GetPosition();
            player->m_Events.AddEventAtOffset([guid, where]()
            {
                if (Player* player = ObjectAccessor::FindPlayer(guid))
                    EvolutionsAudio::PlayAt(player, "Vorhan.GazeBurst", where);
            }, Milliseconds(GazeMs));
            return true;
        }
        if (what == "isolation")
        {
            float const facing = player->GetOrientation();
            Position const fist(player->GetPositionX() + 6.0f * std::cos(facing),
                                player->GetPositionY() + 6.0f * std::sin(facing), player->GetPositionZ());
            float const blow = Position::NormalizeOrientation(facing + float(M_PI));
            player->SendPlaySpellVisual(KIT_STRIKE);
            EvolutionsAudio::PlayAt(player, "Vorhan.Isolation", player->GetPosition());
            GroundIndicators::ShowWardenStrike(player, fist, blow);
            GroundIndicators::ShowWardenPushTrail(player, player->GetPosition(), blow);
            ObjectGuid const guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([guid]()
            {
                Player* player = ObjectAccessor::FindPlayer(guid);
                if (!player)
                    return;
                player->SendPlaySpellVisual(KIT_SEAL);
                EvolutionsAudio::PlayAt(player, "Vorhan.SealBurst", player->GetPosition());
                GroundIndicators::ShowWardenSealBurst(player, player->GetPosition());
            }, Milliseconds(IsolationBurstMs));
            return true;
        }
        if (what == "cell")
        {
            // A cell round the game master, its number 1, its cage rising; the doors' flare 5 s later
            Position const at = player->GetPosition();
            GroundIndicators::ShowCell(player, at, 1, player->GetOrientation(), 8000, CellRadius);
            GroundIndicators::ShowWardenCellBars(player, at, 5000, false);
            EvolutionsAudio::PlayAt(player, "Vorhan.CellsOpen", at);
            ObjectGuid const guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([guid, at]()
            {
                if (Player* player = ObjectAccessor::FindPlayer(guid))
                {
                    GroundIndicators::FlareCell(player, at, CellRadius);
                    GroundIndicators::ShowWardenCellBars(player, at, CageAfterDoorsMs, true);
                    EvolutionsAudio::PlayAt(player, "Vorhan.CellDoors", at);
                }
            }, 5s);
            return true;
        }
        if (what == "curfew")
        {
            // The dial round the game master, the hourglass over their head, the bell at its end
            GroundIndicators::ShowWardenCurfew(player, player->GetPosition(), CurfewMs);
            GroundIndicators::ShowCarriedLook(player, GroundIndicators::SPELL_WARDEN_CURFEW_MARK, CurfewMs);
            EvolutionsAudio::PlayAt(player, "Vorhan.CurfewTick", player->GetPosition());
            ObjectGuid const guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([guid]()
            {
                if (Player* player = ObjectAccessor::FindPlayer(guid))
                    EvolutionsAudio::PlayAt(player, "Vorhan.CurfewBell", player->GetPosition());
            }, Milliseconds(CurfewMs));
            return true;
        }
        if (what == "axe")
        {
            // A blow from the game master's feet ahead of them, opening as it lands like shears
            Position const from(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
                                player->GetOrientation());
            GroundIndicators::Area area;
            area.kind = GroundIndicators::Area::Kind::Rectangle;
            area.origin = from;
            area.radius = float(AxeFullLine.count) * AxeFullLine.ratio * AxeWidth;
            area.width = AxeWidth;
            GroundIndicators::ShowPaintedLine(player, area, AxeFullLine, AxeWarnMs, GroundIndicators::Theme::None, 0,
                AxeLingerMs);
            ObjectGuid const guid = player->GetGUID();
            player->m_Events.AddEventAtOffset([guid, area]()
            {
                Player* player = ObjectAccessor::FindPlayer(guid);
                if (!player)
                    return;
                GroundIndicators::RepaintLine(player, area, AxeFullLine, AxeFullLineHit);
                player->SendPlaySpellVisual(KIT_STRIKE);
                EvolutionsAudio::PlayAt(player, "Vorhan.Isolation", area.origin);
                // Opening as it lands, like shears: a line swings out to either side from the start, stops, blows
                std::vector<ObjectGuid> swung;
                float const full = AxeSwingDegrees * float(M_PI) / 180.0f;
                for (float turn : { full, -full })
                {
                    ObjectGuid placed;
                    GroundIndicators::ShowSwingingLine(player, area, SPELL_AXE_SWING, turn, AxeSwingStartMs,
                        AxeSwingMs, AxeSplitMs, 0, AxeLingerMs, &placed);
                    swung.push_back(placed);
                }
                player->m_Events.AddEventAtOffset([guid, swung]()
                {
                    if (Player* player = ObjectAccessor::FindPlayer(guid))
                        for (ObjectGuid const& placed : swung)
                            if (Creature* line = player->GetMap()->GetCreature(placed))
                            {
                                line->RemoveAurasDueToSpell(SPELL_AXE_SWING);
                                line->AddAura(SPELL_AXE_SWING_HIT, line);
                            }
                }, Milliseconds(AxeSplitMs));
            }, Milliseconds(AxeWarnMs));
            return true;
        }
        if (what == "rollcall")
        {
            // The roll call's blows from the game master: one to each cardinal point, two ahead (a pair on its mark)
            Position const from = player->GetPosition();
            float const facing = player->GetOrientation();
            for (float turn : { 0.0f, 0.0f, float(M_PI) / 2.0f, float(M_PI), -float(M_PI) / 2.0f })
                GroundIndicators::ShowWardenRollCallBlow(player, from, Position::NormalizeOrientation(facing + turn));
            player->SendPlaySpellVisual(KIT_STRIKE);
            return true;
        }
        if (what == "seat")
        {
            // Seat 6 ahead of the game master, called: its seal, its runes flaring on every tick, its number big over
            // it, the call in the middle of the screen, the clock and the DOOM
            Position const at(player->GetPositionX() + 8.0f * std::cos(player->GetOrientation()),
                              player->GetPositionY() + 8.0f * std::sin(player->GetOrientation()), player->GetPositionZ());
            GroundIndicators::ShowCell(player, at, 6, player->GetAngle(&at), 6000, ExecutionSeatRadius);
            for (uint32 tick = 0; tick < 4; ++tick)
            {
                ObjectGuid const guid = player->GetGUID();
                player->m_Events.AddEventAtOffset([guid, at, tick]()
                {
                    if (Player* player = ObjectAccessor::FindPlayer(guid))
                    {
                        GroundIndicators::FlareCell(player, at, ExecutionSeatRadius);
                        EvolutionsAudio::PlayAt(player, tick == 3 ? "Vorhan.ExecutionDoom" :
                            (tick % 2 ? "Vorhan.ExecutionTock" : "Vorhan.ExecutionTick"), at);
                    }
                }, Milliseconds(tick * 1000 + 1000));
            }
            if (Creature* sign = player->SummonCreature(NPC_STALKER, at, TEMPSUMMON_TIMED_DESPAWN, 5000))
            {
                sign->SetObjectScale(SeatNumberScale);
                GroundIndicators::ShowCarriedNumber(sign, 6, 4500);
            }
            WorldPacket packet;
            ChatHandler::BuildChatPacket(packet, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, player, player,
                "|cffffb020Siège 6 !|r");
            player->GetSession()->SendPacket(&packet);
            return true;
        }
        if (what == "burnt" || what == "punishment")
        {
            // Phase 3's marks from the game master ahead of them: an execution's burnt floor (its strike, its clock's
            // DOOM), or the tanks' punishment cone, for 8 s
            float const facing = player->GetOrientation();
            bool const burnt = what == "burnt";
            if (burnt)
            {
                GroundIndicators::ShowWardenRollCallBlow(player, player->GetPosition(), facing);
                EvolutionsAudio::PlayAt(player, "Vorhan.ExecutionDoom", player->GetPosition());
            }
            GroundIndicators::ShowWardenMark(player, player->GetPosition(), facing, burnt ?
                GroundIndicators::SPELL_WARDEN_BURNT_FLOOR : GroundIndicators::SPELL_WARDEN_PUNISHMENT, 8000);
            return true;
        }
        handler->SendErrorMessage(
            "Usage: .vorhan fx <gaze|isolation|cell|curfew|axe|rollcall|burnt|punishment|seat>");
        return false;
    }

    // .vorhan botsonly <on|off>: the bots fight alone, the game master watching (testing the bots)
    static bool HandleBotsOnly(ChatHandler* handler, bool enable)
    {
        BotsOnly = enable;
        handler->SendSysMessage(enable ? "Vorhan: bots only (the bots count as players)." :
                                         "Vorhan: players and bots.");
        return true;
    }

    // .vorhan pull: he engages the game master (a test pull without walking up to him)
    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* warden = player ? player->FindNearestCreature(NPC_VORHAN, 150.0f) : nullptr;
        if (!warden || !warden->IsAlive())
        {
            handler->SendErrorMessage("No living Vorhan within 150 yards.");
            return false;
        }
        warden->SetReactState(REACT_AGGRESSIVE);
        warden->AI()->AttackStart(player);
        handler->SendSysMessage("Vorhan pulled.");
        return true;
    }
};
}

void AddWardenVorhanScripts()
{
    RegisterCreatureAI(boss_warden_vorhan);
    RegisterCreatureAI(npc_warden_vorhan_prisoner);
    RegisterCreatureAI(npc_warden_vorhan_cage);
    new WardenVorhanCommandScript();
}
