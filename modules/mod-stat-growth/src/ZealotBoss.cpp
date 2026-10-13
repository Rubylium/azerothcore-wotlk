#include "DefiBoss.h"
#include "EvolutionsAudio.h"
#include "GroundIndicators.h"
#include "GroundLoot.h"

#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "LFG.h"
#include "Random.h"
#include "StringFormat.h"

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>

// Valérian, le Zélote: the Hellfire Gaol's third gate, a Défi board boss in the Blood Furnace, in Keli'dan the Breaker's
// hall - Vrogar's arena, the second gate's (plan: .agents/plans/gaol-third). A fanatic of the Light locked in the Gaol for years: he has some light, he wants all
// of it. 8 players - 2 tanks, 2 healers, 4 damage dealers - tuned as the Gaol's other gates (477, 650 paragon).
//
// Phase 1 (this file; phase 2, the god, to come): a walled arena on his music (p1, 2:17), he stands at its middle all
// fight. Fast patterns, a dance of placement - each a surprise the first time, read the second:
// - Revers sacré: a cone on his tank, then the same cone behind him a beat later (the melee step through him);
// - Colère du juste: half the arena, then the other half - or the same half again (his raised fist tells);
// - Chaîne brisée: his chain sweeps all round him but a gap, the gap turning a little each strike: follow it;
// - Ferveur: three marked players spread - the second time the marks turn to a stack at the last moment;
// - Rage contenue: the inside, then the outside - or the other way round (his glow tells);
// - Lances de lumière: a grid of lines wall to wall, then the grid shifted half a square;
// - La Litanie: a numeral I to VIII over every head, a spear of light thrown at each verse in turn, from him through
//   it to the wall - striking anyone else whose verse is still to come is a profanation: everyone dies. Solved by
//   standing round him in number order, as on the sigil under him (a clock: I at its top); later two verses at once.
// **The lights**: every few seconds a soak of light falls somewhere in the arena (the retail soak's look: it shows when
// someone stands in it). Whoever stands in it as it lands takes its light (Lumière dérobée: 3% damage and healing a
// stack - the temptation); left alone, the light flies to him and fills his power bar. At the end of the track, only if
// every light reached him does he ascend (phase 2); one taken, and the jailers come for him: he is beaten, and
// leaves 1 gold on the floor, no gear. A light left alone hits everyone for LightReturnPct of their health.
// Phase 2 (to come) is for now a bare fight: no mechanic, its own health, its music - a win as any other.
// Health paced (Defi::BossAI StartPace): half his bar (HoldPct) only at the end; above it then, the light floods the
// arena.
// Every area drawn in his own painted looks (localTools/valerian/valerianArt.py: holy fire, gold light, scorched stone);
// nothing stays on the floor once a pattern has struck (the room is read fast: its sounds mark the hits).
//
// .valerian lab: in the FX lab (map 451) he stands at its middle in rehearsal - every pattern drawn for real by
// .valerian cast, striking nobody, each strike logging how much of the arena it left safe.

// mod-playerbots RaidFinder (the same modules library): the kill counted as lost on the board
void DenyChallengeClear(Map const* map);

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As the Gaol's other gates (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can
// be hit the whole phase (to its end, beat JudgementBeat), part of that time on him (OnHimShare: the dance)
constexpr float OnHimShare = 0.7f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, 127.4f * OnHimShare, 1.095f / 1.18f };
// Phase 2: hittable from his blow (track 0:34.5) to the earliest he may die (P2EarliestDeathMs), part of it on him;
// and a wall: AscendedHealthScale over the profile's raid, what a group of the profile brings only with its
// legendaries (+10% damage each, several of them)
Defi::Sizing const AscendedSizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, (265.0f - 34.5f) * OnHimShare,
    1.095f / 1.18f };
LiveTuning::Knob const HealthScale("valerian.health_scale", 0.85f);
// (Measured on the first real kill, 2026-10-12: two players with their legendaries and bots took phase 2's health at
// 0.26% of the bar a second at 1.8125 - dead by 3:49; 3.75 made it the wall. Phase 3 came, the wall moved there: the
// user and bots, 2026-10-13, took 0.076% of the bar a second at 3.75 - at 1.2 that dies by 4:05, held to 4:25)
LiveTuning::Knob const AscendedHealthScale("valerian.ascended_health_scale", 1.2f);
// Phase 3 (Le Refus): hittable from its drop (5.25 s on its music) to its earliest death (3:19.25), as phase 2's pace.
// The wall: at 2.9 the kill's group (above) leaves him 20% at Lumière totale; the user and bots alone, half
Defi::Sizing const RewoundSizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, (199.25f - 5.25f) * OnHimShare,
    1.095f / 1.18f };
LiveTuning::Knob const RewoundHealthScale("valerian.rewound_health_scale", 2.9f);
// One bar for both phases: each phase's health (its sizing) is half of it - phase 1 paced from full down to HoldPct
// (half: a boss at half health, not one that drops to nothing as if a second phase were to come), phase 2 from there
// to nothing. What a group deals in each phase is its sizing's, as before.
constexpr float HoldPct = 50.0f;

// Keli'dan's hall in the Blood Furnace, its middle (ForgeMaster.cpp's Room: the same arena); the arena round it: a ring
// of WallPieces of Vorhan's wall pieces (GroundIndicators::WardenWallPieceLength each), ArenaRadius from the middle
// reach: the whole hall (a group standing back at the landing spot is drawn in by the wall); its music, the instance
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 1000.0f, 60.0f };
constexpr uint32 WallPieces = 16;
float const ArenaRadius = GroundIndicators::WardenWallPieceLength / (2.0f * std::sin(float(M_PI) / float(WallPieces)));
// His god's blow pushes the wall back: WallPiecesAscended of them from then on (AscendedArenaRadius, about 24 yd: the
// rim of Keli'dan's round platform) - phase 2's arena, the wall still deadly. A wall lasts until it is pushed or the
// fight ends (WallLastsMs: longer than any fight)
constexpr uint32 WallPiecesAscended = 24;
float const AscendedArenaRadius = GroundIndicators::WardenWallPieceLength /
    (2.0f * std::sin(float(M_PI) / float(WallPiecesAscended)));
constexpr uint32 WallLastsMs = 30 * MINUTE * IN_MILLISECONDS;
constexpr uint32 WallPushRiseMs = 300;      // the pushed wall rises this long after the blow (the bursts first)
constexpr float WallMargin = 0.6f;          // past ArenaRadius less this: in the wall
constexpr uint32 WallShowBeats = 8;         // the wall rises this long after the pull
constexpr uint32 WallRiseBeats = 8;         // and kills this long after it rose (time to step in)
constexpr uint32 WallDrawInGraceMs = 1500;  // ... and this long after those still outside are drawn in

// The FX lab (map 451, FxLab.cpp): his rehearsal's place
constexpr uint32 MapFxLab = 451;
Position const LabMiddle(2933.333f, 800.0f, 0.0f, 0.0f);

// --- The music ------------------------------------------------------------------------------------------------------
// p1 (Music.ValerianZealot): a pulse a second from 1.37 s, felt at 120 in half time - the fight counts half-seconds, a
// beat each. Its hits (s -> beat): 11.3 (20), 14.2, 15.2, 18.2 (34), 19.2, 24.1, 27.1, 31.0, 32.0, 36.0 (69), 42.9 (83),
// 46.9 (91), 62.7 (123), 69.6 (136), 73.5 (144), 129.3 (256: the end of the phase); loud to ~120 s, fading from 124 s.
// The track ends at MusicEndMs.
constexpr Defi::BeatGrid Grid = { 1370.0f, 500.0f };
constexpr uint32 MusicEndMs = 137100;
constexpr uint32 JudgementBeat = 256;

// --- His blows ------------------------------------------------------------------------------------------------------
// Profile shares (a damage dealer of the profile): his blows on the tank, the group hits
constexpr float MeleeFloorPct = 10.0f;
constexpr float ReversTankPct = 50.0f;      // Revers sacré on his tank, and a stack of Zèle brûlant
constexpr float ZealTakenPct = 15.0f;       // Zèle brûlant: more taken a stack, for ZealMs; at SwapStacks the swap
constexpr uint32 ZealMs = 30000;
constexpr uint8 SwapStacks = 2;
constexpr float BurstGroupPct = 22.0f;      // Éclat de zèle, on the music's hits
constexpr float FervorCarrierPct = 25.0f;   // Ferveur on its own carrier
constexpr float FervorStackPct = 120.0f;    // Ferveur partagée: shared by those under it
constexpr float AscendLeftPct = 5.0f;       // his first blow as a god leaves everyone at this share of their health
// The victim's own health: a dodge missed
constexpr float DodgePct = 75.0f;
// A mistake: Ébloui for DazzleMs - a second mistake under it kills
constexpr uint32 DazzleMs = 10000;

// The patterns' sizes (yards) and times (beats: half-seconds)
constexpr uint32 WarnBeats = 6;             // a pattern's first red: this long before it strikes
constexpr float ReversArc = 90.0f;
constexpr uint32 ReversBackBeats = 2;       // the cone behind, after the one in front
constexpr uint32 WrathFollowBeats = 4;      // Colère du juste's second half after its first
constexpr float ChainGapDegrees = 90.0f;
constexpr float ChainTurnDegrees = 30.0f;   // the gap's turn each strike (its leading 60 degrees stay safe)
constexpr uint32 ChainStrikes = 5;
constexpr uint32 ChainStepBeats = 2;
constexpr uint32 FervorMarks = 3;
constexpr float FervorRadius = 5.0f;
constexpr uint32 FervorBeats = 8;           // the marks explode this long after they show
constexpr uint32 FervorFlipBeats = 5;       // ... and turn to a stack this long before
constexpr float RageInnerShare = 0.4f;      // Rage contenue: the inside out to this share of the arena (a ring's own)
constexpr uint32 RageFollowBeats = 3;
constexpr float LanceWidth = 3.5f;
constexpr float LanceSpacing = 8.0f;
constexpr uint32 LanceShiftBeats = 3;
constexpr uint32 BurstCastBeats = 3;
// La Litanie: the numerals and the sigil from its start, the first spear LitanyFirstBeats later, then one every
// LitanyEveryBeats (two verses at once: four throws); each spear's line watched (not drawn) LitanyWarnBeats before. The
// sigil LitanySigilRadius yards across its radius, its numerals at LitanyRing yards (where each player stands), I at
// its top (north). A verse struck: LitanyVersePct of its holder's profile health. He is warded meanwhile (nothing
// reaches him: SPELL_LITANY_WARD).
constexpr uint32 LitanyFirstBeats = 10;
constexpr uint32 LitanyEveryBeats = 2;
constexpr uint32 LitanyWarnBeats = 2;
constexpr uint32 LitanyBotsBeats = 4;       // the bots go to their numerals this long before the first spear
constexpr uint32 LitanyVerses = 8;
constexpr float LitanySigilRadius = 13.5f;
constexpr float LitanyRing = LitanySigilRadius * 0.8f;     // where its painting puts the numerals (sigil_litany.png)
constexpr float LitanyVersePct = 25.0f;
constexpr float LitanySigilFacing = float(M_PI) / 2.0f;    // the sigil turned so its I points north (VerseSpot's)
// Each spear thrown: Odyn's spear of light flying from his hand to the wall through its verse at LitanySpearSpeed (the
// spell's own, Spells.ps1), a lightning streak of the Light's gold flying with it; thrown so it reaches the verse's
// numeral on the beat it strikes, aimed LitanySpearHeight over the floor at the wall
constexpr float LitanySpearSpeed = 45.0f;
// When a pattern that wards him ends (La Litanie), his guard down: everyone deals OpeningPct more damage for OpeningMs
constexpr uint32 OpeningMs = 15000;
constexpr float LitanySpearHeight = 1.2f;

// The bots' places (Guide: a spot of its own each, out of a pattern's safe ones - they cannot read his tells): how far
// from his middle they stand close to him (in his reach: his size makes it wide), out in the ring, and Ferveur's marks
// far apart
constexpr float GuideNear = 4.0f;
constexpr float GuideRing = 10.5f;
constexpr float GuideMarks = 11.5f;
constexpr float GuideSlack = 1.0f;

// The lights: a soak every LightEveryBeats from LightFirstBeat to LightLastBeat, shown LightShownBeats before it lands,
// LightRadius yards, between LightNear and the wall's LightWallGap from his middle, never two in a row on the same side.
// Whoever stands in it as it lands takes it; left alone, its light flies to him at LightFlight yards a second.
constexpr uint32 LightFirstBeat = 20;
constexpr uint32 LightLastBeat = 236;
constexpr uint32 LightEveryBeats = 12;
constexpr uint32 LightShownBeats = 8;
constexpr float LightRadius = 3.0f;
constexpr float LightNear = 6.0f;
constexpr float LightWallGap = 3.0f;
constexpr float LightFlight = 12.0f;
constexpr float LightHeight = 1.5f;
constexpr uint32 LightCheckMs = 200;
// Its look: retail's holy soak (display 60020), its ring LightModelRadius yards at scale 1; the model's own states
// played by emotes - someone steps in (the wall sinks, then holds sunk), everyone left (it rises), it lands (a flash)
constexpr float LightModelRadius = 4.12f;
constexpr uint32 EMOTE_SOAK_SINK = 402;
constexpr uint32 EMOTE_STATE_SOAK_SUNK = 417;
constexpr uint32 EMOTE_SOAK_RISE = 404;
constexpr uint32 EMOTE_SOAK_LAND = 405;
constexpr uint32 SoakSinkMs = 500;
constexpr uint32 SoakLandMs = 1200;
constexpr uint32 StolenMs = 10 * MINUTE * IN_MILLISECONDS;
constexpr uint32 LightCount = (LightLastBeat - LightFirstBeat) / LightEveryBeats + 1;
// A light left alone: as it flies to him, everyone takes this share of their own health
constexpr float LightReturnPct = 5.0f;
constexpr float LabLightDistance = 8.0f;    // .valerian cast light: east of him in the lab

// The end of the phase: the jailers walk in, then the mission ends
constexpr uint32 Jailers = 3;
constexpr uint32 JailersInMs = 2000;
constexpr uint32 JailerLineMs = 7000;
constexpr uint32 DeniedMs = 11000;      // the jailers' line said: he is beaten
// His apotheosis, on p2's calm opening (Music.ValerianAscended, from the judgement on): standing, untouchable, the
// light gathering (AscendRiseMs), then rising slowly AscendHeight yards over AscendRiseTakesMs and hovering, glowing
// brighter and growing (AscendGlowEveryMs, to AscendScale); down as the track breaks (AscendLandMs) and his blow on its
// hit (36.9 s: AscendHitMs) - then phase 2. P2's track ends at AscendedMusicEndMs after it started.
constexpr uint32 AscendRiseMs = 2500;
constexpr uint32 AscendRiseTakesMs = 12000;
constexpr float AscendHeight = 7.0f;
constexpr uint32 AscendGlowEveryMs = 1500;
constexpr uint32 AscendGlowClearMs = 2500;
constexpr float AscendScale = 1.7f;
// His god's form from his blow on: the Maw's Kyrian made his (localTools/valerian/buildAscendedModel.py: repainted to
// the Light, wings and shroud of light), at AscendedSize - as tall as his human form had grown (the Kyrian stands about
// half as tall again at the same scale)
constexpr uint32 DISPLAY_ASCENDED = 60022;
constexpr float AscendedSize = 3.0f;
constexpr float AscendFromScale = 1.35f;    // ... from the prelude's last
constexpr float AscendSway = 0.35f;         // hovering, he sways up and down this far, once every AscendSwayMs
constexpr uint32 AscendSwayMs = 3200;
constexpr uint32 AscendHitMs = 34500;     // the track's hit, as heard in game
constexpr uint32 AscendLandMs = AscendHitMs - 700;
constexpr uint32 AscendDropMs = AscendHitMs - AscendLandMs;
// While he rises, untouchable: shards of light falling on a few players and spots every ShardsEveryMs (seen ShardsWarnMs
// before they land, ShardRadius yards): something to step out of while there is nothing to hit
constexpr uint32 ShardsEveryMs = 3000;
constexpr uint32 ShardsWarnMs = 2000;
constexpr float ShardRadius = 3.0f;
constexpr uint32 ShardsPlayers = 3;
constexpr uint32 ShardsSpots = 2;
constexpr float ShardPct = 20.0f;
// Before it all, the light he gathered: his words, and light streaming into him from the wall (one every PreludeLightMs,
// flying at PreludeLightSpeed) for PreludeMs
constexpr uint32 PreludeMs = 9000;
constexpr uint32 PreludeSecondLineMs = 4500;
constexpr uint32 PreludeLightMs = 150;
constexpr float PreludeLightSpeed = 20.0f;
// His size (the display's scale, creature_template_model: twice a man's), every look of his grown from it
constexpr float BaseScale = 2.6f;
constexpr uint32 AscendedMusicEndMs = 281100;

// --- Phase 2 (p2's track from his apotheosis: track ms, measured - .agents/plans/gaol-third PLAN) -----------------------
// His health paced from his blow (AscendHitMs) to nothing at P2EarliestDeathMs (4:25, after his last mechanic): he
// dies then at the soonest, however hard the group hits; Lumière totale (a cast from 4:16) kills everyone at 4:35.0 if
// he still stands. The music's moments: Écho on the rewind (1:06 -> 1:11.0, its loudest), Éclipse on the wrong
// chords (3:43.0 -> 3:48, 3:51.0 -> 3:55).
constexpr uint32 P2EarliestDeathMs = 265000;
constexpr uint32 TotalLightCastMs = 256000;
constexpr uint32 TotalLightMs = 275000;
float const P2Radius = 24.0f;               // phase 2's arena (the pushed wall's, AscendedArenaRadius)
constexpr uint32 P2TellMs = 3000;           // a pattern's red this long before it strikes
// Ailes séraphiques: six tendrils from him to the wall, AilesWidth wide, sweeping AilesTurnDeg over AilesSweepMs; the
// reversed one AilesTurnDeg / 2 one way, then AilesTurnDeg back the other
constexpr uint32 AilesTendrils = 6;
constexpr float AilesWidth = 4.5f;          // drawn as wide (its painting repeated along it at its own proportions)
constexpr float AilesTurnDeg = 60.0f;
constexpr uint32 AilesSweepMs = 2000;
constexpr float AilesGapAt = 11.0f;          // where the bots stand in a gap: 5 yd clear, the gap moving 4 yd/s
constexpr uint32 AilesGuideStepMs = 300;    // their spot moved along this often while the tendrils turn
constexpr uint32 AilesGuideLeadMs = 200;
// The split (the later sets): each tendril, once it has burst, splits; its halves slide AilesSplitDeg each way into the
// gaps (AilesSplitAfterMs after it stopped, over AilesSplitSlideMs) and explode AilesSplitHoldMs after they land
constexpr float AilesSplitDeg = 30.0f;
// ... and their sweep turns AilesSplitSweepDeg at the same pace: not a whole gap, so the tendrils stop off the lines they
// started on and the halves land off both those and the first gaps - nothing learnt at the tell holds
constexpr float AilesSplitSweepDeg = 75.0f;
constexpr float AilesSplitWidth = AilesWidth; // drawn with the tendril's own look: as wide
constexpr uint32 AilesSplitAfterMs = 200;
constexpr uint32 AilesSplitSlideMs = 600;
constexpr uint32 AilesSplitHoldMs = 1000;    // ... where the gap will be this long after (a caster is slow to go)
// Pluie d'astres: three stars carried to three beacons on the wall (StarsBeaconAt yd, 120 degrees apart), dropped where
// the carriers stand at StarsDropMs, bursting at StarsBurstMs: a star on its beacon as its third of the arena from the
// wall to StarsSafe yd of him, off it as a StarsMissRadius circle where it fell
constexpr uint32 StarsCount = 3;
constexpr float StarsBeaconAt = 22.0f;
constexpr float StarsOnBeacon = 3.0f;
constexpr float StarsSafe = 10.0f;          // his reach (7.8 yd at his god's size): melee stands within it
constexpr float StarsMissRadius = 16.0f;
constexpr float StarsCarried = 3.0f;
constexpr uint32 StarsDropMs = 6500;
constexpr uint32 StarsBurstMs = 10500;    // 4 s back to him from the beacon
// Regard divin: a 90 degree cone on its target that follows them GazeFollowMs, holds GazeFreezeMs, then strikes: the
// target takes GazeTargetPct, anyone else in it the dodge's price
constexpr float GazeArc = 90.0f;
constexpr uint32 GazeFollowMs = 3000;
constexpr uint32 GazeFreezeMs = 1000;
constexpr float GazeTargetPct = 35.0f;
// Sentence: two halves (or four quadrants), numbered, struck in order SentenceStepMs apart from P2TellMs on
constexpr uint32 SentenceStepMs = 1500;    // between its counts
constexpr uint32 SentenceCounts = 4;
constexpr uint32 SpreadMs = 2000;           // its closing spread: a FervorRadius circle on everyone, bursting then
constexpr float SentenceAt = 3.5f;          // the bots stand this close to him: a crossing through him in time
constexpr uint32 SentenceWatchMs = 1000;   // a part an area for the bots only its last second
// Le Chœur: seals on everyone, eight wedges light and shadow; in verses of ChoirVerseMs (a cast bar each), the floor
// turning ChoirTurnBeforeMs before each singing; the bell (the swap) ChoirBellBeforeMs before its verse's turn
constexpr uint32 ChoirFirstCastMs = 1000;   // its first verse's cast bar after the seals
constexpr uint32 ChoirVerseMs = 5000;       // a verse: its cast bar (CAST_CHOIR), the choir singing at its end
constexpr uint32 ChoirFastVerseMs = 4000;   // ... the second Chœur's (CAST_CHOIR_FAST)
constexpr uint32 ChoirTurnBeforeMs = 2000;  // the floor turns this long before the choir sings
constexpr uint32 ChoirTurnMs = 800;         // ... over this long
constexpr uint32 ChoirBellBeforeMs = 1500;  // the bell (the swap) this long before its verse's turn
constexpr float ChoirRing = 10.0f;          // where the bots stand in their wedge
constexpr float ChoirNeighbour = 9.0f;      // a real player this near another is swapped with them
// Écho: every player's after-image (EchoRadius) bursts EchoMs after the rewind begins, then everyone left at
// EchoLeftPct; nothing hurts for EchoCalmMs after it (no pattern, no melee: the healers' time)
constexpr float EchoRadius = 5.0f;
constexpr uint32 EchoMs = 6000;            // on the music's drop (1:12.0)
constexpr float EchoLeftPct = 5.0f;
constexpr uint32 EchoCalmMs = 10000;
// Ascension: a pizza (six of eight slices burn, AscensionTellMs warned), orbs coming in along the two left, OrbSpeed
// yd/s; an orb taken gives Surcharge (OverloadMs: a second taken under it bursts, OrbBurstPct); one that reaches him
// heals him OrbHealPct of his phase 2 health
constexpr uint32 AscensionTellMs = 3000;
constexpr uint32 AscensionLastsMs = 21000;  // the pizza on the floor, one drawing, three waves
constexpr uint32 AscensionWaveMs = 7000;
constexpr uint32 AscensionTurnMs = 2000;    // it turns a slice this long before each wave after the first
constexpr uint32 AscensionTickMs = 500;     // its slices burn whoever stands in them, checked this often
constexpr uint32 AscensionGraceMs = 1500;   // ... a player hit not again before this
constexpr float OrbSpeed = 4.0f;
constexpr float OrbFrom = 22.0f;
constexpr float OrbReach = 1.6f;
constexpr float AscensionAside = 2.6f;      // the waiting bots this far beside a lane
constexpr float OrbHealPct = 5.0f;           // (3 first: bots took them all, nobody minded)
constexpr uint32 OverloadMs = 6000;
constexpr float OrbBurstPct = 60.0f;
// Éclipse: the dark everywhere but its holes of light (EclipseHole yd), its burst EclipseMs (EclipseSecondMs the
// second time) after it falls; nothing else meanwhile, no melee
constexpr float EclipseHole = 4.0f;
constexpr float EclipseHoleAt = 14.0f;
constexpr uint32 EclipseMs = 5000;
constexpr uint32 EclipseSecondMs = 4000;
constexpr float EclipseHitPct = 60.0f;      // the light breaking on those in it: of their health (a healer check)
constexpr float EclipseTurnDeg = 90.0f;     // its holes going round him meanwhile, either way (the second the other)
constexpr uint32 KIT_HOLY_GLOW = 83922;     // Holy_Precast_High_Base, played on him (stock 7005 without its tints:
                                            // they stayed on him, and on the god after, yellow and see-through)

// --- Phase 3, Le Refus (Music.ValerianRewind: p2's track again from its 65.75 s; ms on its own clock, P3Ms) ----------
// At nothing in phase 2 he refuses to die: the music cut, the user's rewind (a big clock's machinery and bells: the
// start of phase 3's own track) as time stands still (RefusStillMs: everyone held) and runs back, his health
// filling, everyone pulled back to where they stood RefusBackMs before (their trail) at RefusPullMs, their
// after-image there; phase 2's music again RefusMusicMs after the cut, fading in under the rewind's last clockwork
// (P3Ms counts from there). On its drop (RefusDropMs) the after-images burst and everyone is left at RefusLeftPct;
// then RefusCalmMs of calm.
constexpr uint32 RefusStillMs = 1500;
constexpr uint32 RefusPullMs = 6000;
constexpr uint32 RefusMusicMs = 6500;
constexpr uint32 RefusBackMs = 4000;
constexpr uint32 TrailEveryMs = 250;
constexpr uint32 RefusDropMs = 5250;
constexpr float RefusLeftPct = 20.0f;
constexpr uint32 RefusCalmMs = 10000;
// His health paced from the drop to nothing at P3EarliestDeathMs; Lumière totale as in phase 2, on the same music
// (p2's times less 65.75 s): its cast from P3TotalLightCastMs, everyone dead at P3TotalLightMs if he stands. Its sigil
// covers the arena only once the last Prémonition has struck (P3TotalLightSigilMs): over its shapes, it hid them.
constexpr uint32 P3EarliestDeathMs = 199250;
constexpr uint32 P3TotalLightCastMs = 190250;
constexpr uint32 P3TotalLightMs = 209250;
constexpr uint32 P3TotalLightSigilMs = 14500;  // after the cast's start
constexpr uint32 P3MusicEndMs = 215360;

// Les Sabliers: six seals on the wall (HourglassAt yd from him, 60 degrees apart), in volleys of three. A sun fires a
// SablierNearArc cone SablierNearLength long at the player it marked, to be taken along the wall beside it; a crescent
// a SablierFarArc cone across the arena, its player at the far wall; the cone follows them until a second before it
// fires, then holds. Its target takes SablierTargetPct; anyone else in it dies.
constexpr uint32 Hourglasses = 6;
constexpr float HourglassAt = 23.0f;
constexpr float SablierNearArc = 30.0f;
constexpr float SablierNearLength = 30.0f;
constexpr float SablierFarArc = 20.0f;
constexpr float SablierFarLength = 47.0f;
constexpr float SablierReach = 60.0f;
constexpr uint32 SablierFreezeMs = 1000;
// Each seal of a volley marks a player SablierMarkLeadMs before its cone shows (real players first, tanks too; bots
// fill in): its seal over their head and its beam to them until it fires - the cone follows them, they place it.
// The nearest (farthest) only if they die. Nobody marked for two volleys at once.
constexpr uint32 SablierMarkLeadMs = 2000;
constexpr float SablierSealHeight = 2.4f;   // the seal's middle over the floor (shapes.json VL_SablierSeal*)
constexpr float SablierTargetPct = 40.0f;
// Prémonition: visions of his strikes shown one after another (PremonitionShowMs each), then struck in reverse order
// (PremonitionGapMs apart, each drawn that long before it strikes): his light close (PremonitionInner yd: the ring
// looks' 0.4), the ring beyond it, the half on his left, the half on his right (of where he faced as he began)
constexpr float PremonitionInner = 10.0f;
constexpr uint32 PremonitionFirstMs = 500;
constexpr uint32 PremonitionPauseMs = 1600;
// Chaînes du temps: ChainsMs after he chains them (never a tank), each chain breaks - death on anyone but a tank, who
// takes ChainBreakPct (a profile hit: their defences count) for each they hold. A tank within ChainTakeReach of a chained
// player takes their chain, as many as their share (the chains over the tanks there). Its line burns whoever else stands
// on it (ChainLineWidth wide, past ChainLineFrom: his own reach is the tanks'), from ChainLineGraceMs on.
constexpr uint32 ChainsMs = 10000;
constexpr float ChainTakeReach = 3.0f;
constexpr float ChainBreakPct = 40.0f;
constexpr float ChainLineFrom = 8.0f;
constexpr float ChainLineWidth = 1.2f;
constexpr uint32 ChainLineGraceMs = 2000;
constexpr uint32 ChainCheckMs = 200;
constexpr uint32 ChainBurnAgainMs = 2000;

// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

// His painted looks (localTools/groundIndicators/shapes.json VL_*, from the paintings in localTools/valerian/art through
// valerianArt.py): every area he lays drawn in them rather than the red, the same Area registered (the bots, the hits)
enum Looks : uint32
{
    LOOK_RADIANCE_CONE_180      = 98600,    // Colère du juste's halves: radiant flagstones
    LOOK_RADIANCE_GLOW_180      = 98601,
    LOOK_RADIANCE_CONE_270      = 98602,    // Chaîne brisée's sweep (all but its gap)
    LOOK_RADIANCE_GLOW_270      = 98603,
    LOOK_RAGE_CIRCLE            = 98604,    // Rage contenue: the inside
    LOOK_RAGE_CRACKS_CIRCLE     = 98605,
    LOOK_RAGE_RING_40           = 98606,    // ... the outside (inner 0.4 of outer)
    LOOK_RAGE_CRACKS_RING_40    = 98607,
    LOOK_REVERS_CONE_90         = 98608,    // Revers sacré
    LOOK_REVERS_FLARE           = 98609,
    LOOK_LANCE_TILE             = 98610,    // a lance's tile: LanceTileRatio as long as it is wide
    LOOK_FERVOR_SPREAD          = 98611,    // carried, built to FervorRadius
    LOOK_FERVOR_STACK           = 98612,    // carried, built to FervorRadius
    LOOK_MARK_FERVOR            = 98613,    // over the marked's heads
    LOOK_MARK_FERVOR_STACK      = 98614,
    LOOK_WALL_BARS_FIRST        = 98615,    // the wall's bars, its two pieces each half of their band
    LOOK_WALL_LIGHT             = 98617,    // the light behind them, rising
    LOOK_WALL_MOTES             = 98618,    // the motes in front
    LOOK_WALL_RISE              = 98619,
    LOOK_SIGIL                  = 98620,    // under him for his apotheosis
    LOOK_SIGIL_RUNES            = 98621,
    LOOK_MARK_VERSE_FIRST       = 98622,    // La Litanie's numerals I-VIII over the heads (+ the verse's index)
    LOOK_LITANY_SIGIL           = 98630,    // its clock under him: I at its top, then clockwise
    LOOK_LITANY_SIGIL_GLOW      = 98631,
    // Phase 3's (the brief: gaol-third.ASSETS-p3.md; 98632 up, their fading twins 60 on)
    // Les Sabliers drawn as Le Chœur's seals standing at the wall (painted hourglasses did not match phase 2):
    LOOK_SABLIER_LIGHT          = 98632,    // the sun seal: it fires at the nearest
    LOOK_SABLIER_SHADOW         = 98633,    // the crescent seal: at the farthest
    LOOK_SABLIER_SWAP_LS        = 98646,    // ... burning into the other as it fires (the seals' flipbook, SealSwapMs)
    LOOK_SABLIER_SWAP_SL        = 98647,
    LOOK_SABLIER_GOLD           = 98634,    // ... a gold one's cone
    LOOK_SABLIER_GOLD_GLOW      = 98635,
    LOOK_SABLIER_VIOLET         = 98636,    // ... a violet one's
    LOOK_SABLIER_VIOLET_WISPS   = 98637,
    LOOK_VISION_CIRCLE          = 98638,    // Prémonition's visions: his light close, the ring, a half
    LOOK_VISION_RING_40         = 98639,
    LOOK_VISION_HALF            = 98640,
    LOOK_DIVINE_CIRCLE          = 98641,    // ... and its strikes (a half: LOOK_DIVINE_CONE_180)
    LOOK_DIVINE_GLOW_CIRCLE     = 98642,
    LOOK_DIVINE_RING_40         = 98643,
    LOOK_DIVINE_GLOW_RING_40    = 98644,
    LOOK_MARK_CHAIN             = 98645,    // over a chained player's head
    LOOK_VISION_GLOW_CIRCLE     = 98648,    // the visions' glow
    LOOK_VISION_GLOW_RING_40    = 98649,
    LOOK_VISION_GLOW_HALF       = 98650,
    LOOK_REFUS_SIGIL            = 98651,    // Le Refus: a clock under him
    LOOK_REFUS_HANDS            = 98652,    // ... its hands, spun backwards
    // Phase 2's (the brief: gaol-third.ASSETS-p2.md; 98800 up, the Faucheur's ranks below)
    LOOK_DIVINE_CONE_45         = 98800,    // Ascension's burning slices
    LOOK_DIVINE_GLOW_45         = 98801,
    LOOK_DIVINE_CONE_180        = 98802,    // Sentence's halves
    LOOK_DIVINE_GLOW_180        = 98803,
    LOOK_DIVINE_CONE_90         = 98804,    // ... its quadrants
    LOOK_DIVINE_GLOW_90         = 98805,
    LOOK_STARFALL_THIRD         = 98806,    // a star on its beacon: its third, from StarsSafe to the wall
    LOOK_STARFALL_GLOW_THIRD    = 98807,
    LOOK_STARFALL_CIRCLE        = 98808,    // a star dropped off its beacon
    LOOK_STARFALL_GLOW_CIRCLE   = 98809,
    LOOK_CHOIR_LIGHT            = 98810,    // Le Chœur's wedges, light and shadow
    LOOK_CHOIR_LIGHT_GLOW       = 98811,
    LOOK_CHOIR_SHADOW           = 98812,
    LOOK_CHOIR_SHADOW_WISPS     = 98813,
    LOOK_ECLIPSE_VEIL_TWO       = 98814,    // the dark, its holes cut (valerianArt.py eclipse_veil: facing between them)
    LOOK_ECLIPSE_VEIL_ONE       = 98815,    // ... its one hole behind it
    LOOK_TENDRIL                = 98816,    // Ailes séraphiques' tendril: 24 x 4.5 yd, its painting repeated along it
    LOOK_GAZE_CONE              = 98817,    // Regard divin
    LOOK_GAZE_FLARE             = 98818,    // ... as it freezes
    LOOK_ORB_LANE               = 98819,    // an orb's path to him: 8 to 1
    LOOK_BEACON                 = 98820,    // a star's beacon
    LOOK_BEACON_RAYS            = 98821,
    LOOK_STAR_CARRIED           = 98822,    // carried, built to StarsCarried
    LOOK_ECHO                   = 98823,    // an after-image
    LOOK_ECHO_GLOW              = 98824,
    LOOK_ECLIPSE_HOLE           = 98825,
    LOOK_ECLIPSE_HOLE_RIM       = 98826,
    LOOK_SENTENCE_QUARTER_FIRST = 98827,    // Sentence's quadrants, numerals I-IV painted in (+ the part's step)
    LOOK_TOTAL_LIGHT_SIGIL      = 98831,    // under him for Lumière totale, growing
    LOOK_TOTAL_LIGHT_RUNES      = 98832,
    LOOK_MARK_SEAL_LIGHT        = 98833,    // over the heads: the seals, the carriers, the gaze, the orbs
    LOOK_MARK_SEAL_SHADOW       = 98834,
    LOOK_MARK_STAR              = 98835,
    LOOK_MARK_GAZE              = 98836,
    LOOK_MARK_ORB_FREE          = 98837,
    LOOK_MARK_ORB_FULL          = 98838,
    LOOK_SEAL_SWAP_LS           = 98839,    // a seal burning into the other (a flipbook, SealSwapMs)
    LOOK_SEAL_SWAP_SL           = 98840,
    LOOK_ECLIPSE_LIGHT_TWO      = 98845,    // the Éclipse's pools of light where its veil's holes are, turning with it
    LOOK_ECLIPSE_LIGHT_ONE      = 98846,
    LOOK_TENDRIL_CURTAIN        = 98847,    // a tendril standing up (a curtain, 3D) in shadow, solid: over Lumière
                                            // totale's holy sigil (gold on gold did not read; a decal over it, or a
                                            // see-through wall past the middle, flickered: drawn with the solids)
    LOOK_SENTENCE_HALF_FIRST    = 98841,    // Sentence's halves, I and III painted in one, II and IV the other
    LOOK_LANCE_FLARE            = 98843,    // a lance's tile flashing as it strikes (added, its own alpha keys)
    LOOK_TENDRIL_FLARE          = 98844,    // a tendril (or a split half) flashing as it strikes
};

// The layer shown over a look (0: none)
constexpr uint32 LayerOf(uint32 look)
{
    switch (look)
    {
        case LOOK_RADIANCE_CONE_180: return LOOK_RADIANCE_GLOW_180;
        case LOOK_RADIANCE_CONE_270: return LOOK_RADIANCE_GLOW_270;
        case LOOK_RAGE_CIRCLE: return LOOK_RAGE_CRACKS_CIRCLE;
        case LOOK_RAGE_RING_40: return LOOK_RAGE_CRACKS_RING_40;
        case LOOK_REVERS_CONE_90: return LOOK_REVERS_FLARE;
        case LOOK_DIVINE_CONE_45: return LOOK_DIVINE_GLOW_45;
        case LOOK_DIVINE_CONE_180: return LOOK_DIVINE_GLOW_180;
        case LOOK_DIVINE_CONE_90: return LOOK_DIVINE_GLOW_90;
        case LOOK_STARFALL_THIRD: return LOOK_STARFALL_GLOW_THIRD;
        case LOOK_STARFALL_CIRCLE: return LOOK_STARFALL_GLOW_CIRCLE;
        case LOOK_ECHO: return LOOK_ECHO_GLOW;
        case LOOK_SENTENCE_QUARTER_FIRST: case LOOK_SENTENCE_QUARTER_FIRST + 1: case LOOK_SENTENCE_QUARTER_FIRST + 2:
        case LOOK_SENTENCE_QUARTER_FIRST + 3: return LOOK_DIVINE_GLOW_90;
        case LOOK_SENTENCE_HALF_FIRST: case LOOK_SENTENCE_HALF_FIRST + 1: return LOOK_DIVINE_GLOW_180;
        case LOOK_DIVINE_CIRCLE: return LOOK_DIVINE_GLOW_CIRCLE;
        case LOOK_DIVINE_RING_40: return LOOK_DIVINE_GLOW_RING_40;
        default: return 0;
    }
}
constexpr uint32 WallBarsLooks = 2;
constexpr float WallLightOut = 1.006f;      // the light's ring a little behind the bars, the motes before them
constexpr float WallMotesIn = 0.994f;
constexpr uint32 WallRiseMs = 1400;
constexpr float LanceTileRatio = 4.0f;
constexpr uint32 LineFlareMs = 700;         // a line's flare (shapes.json VL_LanceFlare / VL_TendrilFlare alphaKeys)
constexpr float SigilRadius = 8.0f;
constexpr uint32 SealSwapMs = 600;          // a seal's swap flipbook (six frames, 100 ms each)
constexpr float EclipseSpread = 1.22f;      // the first Éclipse's holes this far each side of its veil's facing

// His impacts: retail holy effects Ascension converted (localTools/valerian/ascensionVisuals.json, importedValerian),
// and Dragonflight's holy impact rebuilt to his forms (localTools/shapedEffects: played from the form's origin, turned a
// quarter left of its axis - ShapedImpact)
enum Kits : uint32
{
    KIT_LIGHTS_JUDGEMENT    = 83903,    // a ring of golden sparks bursting (the Vindicaar's Light's Judgement)
    KIT_HOLY_IMPACT         = 83906,    // a column of light and its ground flash (Dragonflight)
    KIT_HOLY_BLAST          = 83911,    // a wide golden flash (the Lightforged siege weapon's)
    KIT_SPEAR_IN_HAND       = 83920,    // Odyn's spear of light in his right hand (unused: it read as a ball of light)
    SHAPED_HOLY_CONE_90     = 83950,    // a quarter of the arena, 17 yd
    SHAPED_HOLY_RING_7_17   = 83951,    // Rage contenue's outside
    SHAPED_HOLY_LINE_10X2   = 83952,    // a lance's tile: 10 yd along, 2.5 across
};
constexpr uint32 ImpactMs = 4000;
// His spear: Odyn's Spear of Light (retail, localTools/retailImport display 70118: gold and light), carried by item
// 18882 - in his hands while he throws spears of light (La Litanie, Lances de lumière)
constexpr uint32 ITEM_SPEAR_OF_LIGHT = 18882;
// How far each burst reaches from its middle at scale 1 (yards, seen in the FX lab): sized from it to its circle
constexpr float BlastReach = 3.0f;
constexpr float JudgementReach = 3.5f;

enum Spells : uint32
{
    SPELL_REVERS            = 94630,    // Revers sacré
    SPELL_WRATH             = 94631,    // Colère du juste
    SPELL_CHAIN             = 94632,    // Chaîne brisée
    SPELL_FERVOR            = 94633,    // Ferveur
    SPELL_FERVOR_SHARED     = 94634,    // Ferveur partagée
    SPELL_RAGE              = 94635,    // Rage contenue
    SPELL_LANCES            = 94636,    // Lances de lumière
    SPELL_WALL              = 94637,    // Mur de lumière
    SPELL_BURST             = 94638,    // Éclat de zèle
    SPELL_BLINDING          = 94639,    // Lumière aveuglante (the check missed)
    SPELL_ASCEND            = 94640,    // Jugement du Zélote (his rising)
    SPELL_LIGHT_RETURN      = 94641,    // Lumière rendue (a light left alone, on everyone)
    SPELL_SHARDS            = 94642,    // Éclats de lumière (while he rises)
    SPELL_VERSE             = 94643,    // Verset (a spear on its own verse)
    SPELL_PROFANATION       = 94644,    // Profanation (a spear struck another: everyone dies)
    SPELL_VERSE_SPEAR       = 94645,    // La Litanie's spear in flight (a missile, nothing more)
    SPELL_VERSE_LIGHTNING   = 94646,    // ... and the golden lightning flying with it
    SPELL_LITANY_WARD       = 94669,    // Litanie (nothing reaches him while he recites)
    SPELL_OUT_OF_REACH      = 94670,    // Hors d'atteinte (his defeat and his ascension: nothing reaches him)
    // Phase 2's (localTools/valerian/Spells.ps1)
    SPELL_AILES             = 94680,    // Ailes séraphiques
    SPELL_STARS             = 94681,    // Pluie d'astres
    SPELL_GAZE              = 94682,    // Regard divin
    SPELL_SENTENCE          = 94683,    // Sentence
    SPELL_DISSONANCE        = 94684,    // Dissonance (the choir: a seal on the wrong wedge, everyone dies)
    SPELL_ECHO              = 94685,    // Écho (an after-image bursting)
    SPELL_CATALYST          = 94686,    // Catalyse (everyone left at 5%)
    SPELL_ECLIPSE           = 94687,    // Éclipse (caught in the dark)
    SPELL_TOTAL_LIGHT       = 94688,    // Lumière totale (he still stood at 4:35)
    SPELL_OVERLOAD          = 94689,    // Surcharge (an orb taken: a second under it bursts)
    SPELL_ORB_BURST         = 94690,    // Lumière brûlante (an orb taken under Surcharge)
    CAST_CHOIR              = 94691,    // Le Chœur (8 s)
    CAST_ECHO               = 94692,    // Écho (5 s)
    CAST_TOTAL_LIGHT        = 94693,
    CAST_CHOIR_FAST         = 94694,    // Le Chœur's verse, the second's (4 s)    // Lumière totale (19 s)
    // Phase 3's (Spells.ps1: in the gaps)
    SPELL_TIME_STILL        = 94671,    // Temps suspendu (everyone held as he refuses)
    SPELL_TIME_CHAIN        = 94672,    // Chaîne du temps (its holder's debuff, a stack a chain)
    SPELL_TIME_CHAIN_BEAM   = 94673,    // ... the chain from him (a beam)
    SPELL_SABLIER           = 94674,    // Les Sabliers
    SPELL_CHAIN_BREAK       = 94675,    // Rupture du temps
    SPELL_PREMONITION       = 94676,    // Prémonition
    SPELL_CHAIN_LINE        = 94677,    // Lien brûlant (standing on a chain)
    SPELL_REFUS             = 94678,    // Le Refus (an after-image bursting)
    SPELL_SABLIER_LINK_SUN  = 94698,    // a seal's beam to the player it marked: a sun's (gold)
    SPELL_SABLIER_LINK_MOON = 94699,    // ... a crescent's (purple)
    CAST_REFUS              = 94695,
    CAST_SABLIERS           = 94696,
    CAST_PREMONITION        = 94697,
    SPELL_OPENING           = 94654,    // Garde brisée (+50% damage done, after a ward: Spells.ps1 holds the share)
    SPELL_STOLEN            = 94650,    // Lumière dérobée (3% damage and healing a stack)
    SPELL_DAZZLED           = 94651,    // Ébloui (a mistake)
    SPELL_ZEAL              = 94652,    // Zèle brûlant (his tank's stacks)
    SPELL_FERVOR_MARK       = 94653,    // Marque de ferveur
    CAST_REVERS             = 94660,
    CAST_WRATH              = 94661,
    CAST_CHAIN              = 94662,
    CAST_FERVOR             = 94663,
    CAST_RAGE               = 94664,
    CAST_LANCES             = 94665,
    CAST_BURST              = 94666,
    CAST_JUDGEMENT          = 94667,
    CAST_LITANY             = 94668,
};

enum Npcs : uint32
{
    NPC_VALERIAN            = 930500,
    NPC_ORB                 = 930501,   // a light flying to him (only seen)
    NPC_JAILER              = 930502,
    NPC_LIGHT               = 930503,   // a light soak (its look)
};

enum Says : uint8
{
    SAY_AGGRO               = 0,
    SAY_KILL                = 1,
    SAY_STOLEN              = 2,
    SAY_ASCEND              = 4,
    SAY_DEATH               = 6,
    SAY_PRELUDE_FIRST       = 7,        // every light in him: his words before he rises
    SAY_PRELUDE_SECOND      = 8,
    SAY_JAILER              = 0,        // the jailer's own
};

enum class Phase : uint8
{
    None,
    Zealot,         // phase 1
    Chained,        // the light stolen: he kneels, the jailers come
    Ascending,      // every light in him: he rises
    Ascended,       // phase 2
    Refusing,       // at nothing in phase 2: time stopped and wound back (Le Refus)
    Rewound,        // phase 3
    Over,
    Rehearsal,      // the FX lab
};

enum class Event : uint8
{
    Revers,
    Wrath,          // arg: 0 the other half, 1 at random
    Chain,
    Fervor,         // arg: 0 spread, 1 turned to a stack, 2 at random
    Rage,           // arg: 0 in then out, 1 out then in, 2 at random
    Lances,
    Burst,
    Judgement,
    Litany,         // arg: 0 one verse at a time, 1 two at once
};

char const* EventName(Event what)
{
    switch (what)
    {
        case Event::Revers: return "revers";
        case Event::Wrath: return "wrath";
        case Event::Chain: return "chain";
        case Event::Fervor: return "fervor";
        case Event::Rage: return "rage";
        case Event::Lances: return "lances";
        case Event::Burst: return "burst";
        case Event::Judgement: return "judgement";
        case Event::Litany: return "litany";
    }
    return "?";
}

struct Step
{
    uint32 beat;    // where it starts
    Event what;
    uint32 arg = 0;
};

// Phase 1 on p1 (beats: half-seconds). Each pattern ends before the next starts; strikes on the music's hits where they
// fall (beats 20, 34, 69, 83, 91, 123, 136, 144, 256).
std::vector<Step> BuildTimeline()
{
    return {
        { 14, Event::Revers },                  // lands on the 11.3 s hit
        { 24, Event::Wrath, 0 },                // its second half on the 18.2 s hit
        { 37, Event::Litany, 0 },               // one verse at a time, early: its spears from beat 47 to 61
        { 63, Event::Revers },                  // on the 36.0 s hit
        { 77, Event::Lances },                  // on the 42.9 s hit
        { 87, Event::Chain },
        { 102, Event::Wrath, 1 },
        { 115, Event::Fervor, 0 },              // a spread, on the 62.7 s hit
        { 130, Event::Rage, 0 },                // on the 69.6 s hit, in then out
        { 141, Event::Burst },                  // on the 73.5 s hit
        { 146, Event::Lances },
        { 157, Event::Chain },
        { 173, Event::Fervor, 1 },              // turned to a stack
        { 183, Event::Litany, 1 },              // two verses at once: its spears from beat 193 to 199
        { 200, Event::Rage, 2 },
        { 212, Event::Wrath, 1 },
        { 225, Event::Chain },
        { 241, Event::Revers },
        { 250, Event::Judgement },              // on the 129.3 s hit
    };
}

// What a mistake costs (a share of the victim's own health; 100: death), and its name in the log
// What a strike looks like as it lands, laid out from its area (Impact): its holy impact cut to its form
enum class Look : uint8
{
    None,
    Shaped,     // a cone in quarters, a ring, a lance in tiles
    Blast,      // a circle: the golden flash sized to it
};

// sound: played as it strikes (client-assets/audio/valerian.json), from the area nearest each listener
struct Rule
{
    uint32 spell;
    float percent;
    char const* name;
    char const* sound = nullptr;
    Look look = Look::None;
    bool dazzles = true;            // a mistake under it leaves Ébloui (a second one kills); the shards' do not
};

Rule const RuleRevers = { SPELL_REVERS, DodgePct, "revers", "Valerian.Revers", Look::Shaped };
Rule const RuleWrath = { SPELL_WRATH, DodgePct, "wrath", "Valerian.Wrath", Look::Shaped };
Rule const RuleChain = { SPELL_CHAIN, DodgePct, "chain", "Valerian.Chain", Look::Shaped };
Rule const RuleFervor = { SPELL_FERVOR, DodgePct, "fervor" };
Rule const RuleRage = { SPELL_RAGE, DodgePct, "rage", "Valerian.Rage", Look::Shaped };
Rule const RuleLances = { SPELL_LANCES, DodgePct, "lances", "Valerian.Lances", Look::Shaped };
Rule const RuleShards = { SPELL_SHARDS, ShardPct, "shards", "Valerian.Fervor", Look::Shaped, false };
Rule const RuleAiles = { SPELL_AILES, DodgePct, "ailes", "Valerian.Lances" };
Rule const RuleStars = { SPELL_STARS, DodgePct, "stars", "Valerian.Rage", Look::Shaped };
Rule const RuleGaze = { SPELL_GAZE, DodgePct, "gaze", "Valerian.Revers", Look::Shaped };
Rule const RuleSentence = { SPELL_SENTENCE, DodgePct, "sentence", "Valerian.Wrath", Look::Shaped };
Rule const RuleEcho = { SPELL_ECHO, DodgePct, "echo", "Valerian.Fervor", Look::Blast, false };
Rule const RulePizza = { SPELL_STARS, DodgePct, "pizza", "Valerian.Chain", Look::Shaped };
Rule const RuleEclipse = { SPELL_ECLIPSE, 100.0f, "eclipse" };
Rule const RuleSpread = { SPELL_SENTENCE, DodgePct, "spread", "Valerian.Fervor", Look::None };
Rule const RuleRefus = { SPELL_REFUS, DodgePct, "refus", "Valerian.Fervor", Look::Blast, false };
Rule const RuleSablier = { SPELL_SABLIER, 100.0f, "sablier", "Valerian.Revers" };
Rule const RulePremonition = { SPELL_PREMONITION, DodgePct, "premonition", nullptr, Look::Shaped };
Rule const RuleChainLine = { SPELL_CHAIN_LINE, DodgePct, "chainline" };

// Phase 2 on p2's track (ms from his apotheosis), each pattern ending before the next starts (3-5 s between); after
// Écho's hit and the choir's swap, nothing that hurts. arg: a pattern's version (0 its first, 1 its twist, ...)
enum class P2Event : uint8
{
    Ailes,          // 1: reversed halfway; 2, 3: the same, then the split
    Stars,
    Gaze,           // arg: the eyes (1 or 2)
    Sentence,       // 0: two halves, 1: four quadrants (each closed by a spread), 2: two halves, no spread
    Choir,          // 0: four verses, the bell before the 2nd and the 4th; 1: five, faster, bells before 2, 3, 5
    Echo,
    Ascension,      // the pizza and its three waves
    Eclipse,        // 0: two holes, 1: one, on the other side
    TotalLight,
};

struct P2Step
{
    uint32 trackMs;
    P2Event what;
    uint32 arg = 0;
};

std::vector<P2Step> BuildP2Timeline()
{
    return {
        { 41000, P2Event::Ailes, 0 }, { 52000, P2Event::Stars }, { 66000, P2Event::Echo },
        { 82000, P2Event::Choir, 0 }, { 106000, P2Event::Ascension },
        { 131000, P2Event::Gaze, 1 }, { 138000, P2Event::Ailes, 3 }, { 147000, P2Event::Stars },
        { 158000, P2Event::Sentence, 0 }, { 171000, P2Event::Gaze, 2 }, { 179000, P2Event::Choir, 1 },
        { 202000, P2Event::Ailes, 2 }, { 211000, P2Event::Stars },
        { 223000, P2Event::Eclipse, 0 }, { 231000, P2Event::Eclipse, 1 },
        { 240000, P2Event::Sentence, 1 }, { 252000, P2Event::Gaze, 2 },
        { TotalLightCastMs, P2Event::TotalLight }, { 258000, P2Event::Ailes, 3 }, { 266000, P2Event::Sentence, 2 },
    };
}

// Phase 3 on its own music (P3Ms: 0 where it starts, after the rewind), each pattern ending before the next starts;
// never two at once (the chains above all: who is chained has nothing else to do). arg: a pattern's version.
enum class P3Event : uint8
{
    Sabliers,       // 0: gold then violet; 1: each flipping as it fires (four volleys); 2: so, faster; 3: violet first
    Premonition,    // 0: four visions; 1: five, mirrored as they strike; 2: six, faster; 3: five, during the last cast
    Chains,         // 0: four chains; 1: six
    Eclipse,        // 2: its holes go round and back; 3: one hole, round and back the other way
    TotalLight,
};

struct P3Step
{
    uint32 ms;
    P3Event what;
    uint32 arg = 0;
};

std::vector<P3Step> BuildP3Timeline()
{
    return {
        { 18000, P3Event::Sabliers, 0 }, { 34000, P3Event::Premonition, 0 }, { 50000, P3Event::Chains, 0 },
        { 66000, P3Event::Sabliers, 1 }, { 86000, P3Event::Premonition, 1 }, { 103000, P3Event::Chains, 1 },
        { 123000, P3Event::Premonition, 2 }, { 140000, P3Event::Sabliers, 2 },
        { 157250, P3Event::Eclipse, 2 }, { 165250, P3Event::Eclipse, 3 }, { 172000, P3Event::Sabliers, 3 },
        { P3TotalLightCastMs, P3Event::TotalLight }, { 193000, P3Event::Premonition, 3 },
    };
}

// When nothing may hurt and he does not swing (track ms): after Écho's hit, the two Éclipses
struct Calm
{
    uint32 fromMs;
    uint32 toMs;
};
Calm const P2Calms[] = { { 66000, 66000 + EchoMs + EchoCalmMs }, { 220000, 236000 } };
// Phase 3's (P3Ms): the drop's (healing up), the two Éclipses
Calm const P3Calms[] = { { 0, RefusDropMs + RefusCalmMs }, { 154000, 170250 } };

bool IsTank(Player* player)
{
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() && member.roles)
                return member.roles & lfg::PLAYER_ROLE_TANK;
    return player->HasTankSpec();
}

using Areas = std::vector<GroundIndicators::Area>;

struct boss_valerian : public Defi::BossAI
{
    boss_valerian(Creature* creature) : Defi::BossAI(creature, Room, "module.valerian")
    {
        // Held at the arena's middle all fight (HoldMiddle): his tanks come to him
        me->SetCombatMovement(false);
    }

    void Reset() override
    {
        if (_phase == Phase::Rehearsal)
            return;
        // A fight still on is a wipe: the board resets him once the players are down
        if (_phase == Phase::Zealot)
        {
            WatchDeaths();
            LOG_INFO(_logName, "Valerian wipe instance={} at={:.1f}s true health={:.1f}% lights absorbed={} taken={} "
                     "deaths={}", me->GetInstanceId(), Elapsed() / 1000.0f, TrueHealthPct(), _absorbed, _taken,
                     DeathsText());
        }
        EndMusic();
        ResetFight();
        if (DefiConfirmed())
        {
            me->SetVisible(true);
            SetHalfBar(true);
        }
        // The board holds him until the group pulls (RaidFinder HoldChallengeBoss)
        me->SetReactState(REACT_PASSIVE);
    }

    // Combat's own evade never ends the fight while a player stands in the arena; the board's reset (a wipe:
    // EVADE_REASON_OTHER) always does
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (_phase == Phase::Rehearsal)
            return;
        if (why != EVADE_REASON_OTHER && Fighting() && !ArenaPlayers().empty())
            return;
        Defi::BossAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (_phase != Phase::None)
            return;
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, GetRoom().reach);
        SetHalfBar(true);
        StartPace(HoldPct);
        GroundIndicators::DrawInstantly(me);
        _timeline = BuildTimeline();
        _next = 0;
        StartClock();
        _phase = Phase::Zealot;
        RaiseArena();
        StartLights(LightFirstBeat, LightLastBeat);
        Talk(SAY_AGGRO);
        StartMusic("Music.ValerianZealot");
        LOG_INFO(_logName, "Valerian pulled instance={} health={} arena={:.2f} yd lights={} tier factor={}",
                 me->GetInstanceId(), me->GetMaxHealth(), ArenaRadius, LightCount, GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && _phase == Phase::Zealot && KillYellReady())
            Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        WatchDeaths();
        LOG_INFO(_logName, "Valerian killed instance={} at={:.1f}s lights absorbed={} taken={} deaths={}",
                 me->GetInstanceId(), Elapsed() / 1000.0f, _absorbed, _taken, DeathsText());
        EndMusic();
        ResetFight();
        _phase = Phase::Over;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_phase == Phase::Ascended)
        {
            P2Damage(damage);
            return;
        }
        if (_phase == Phase::Rewound)
        {
            P3Damage(damage);
            return;
        }
        if (_phase != Phase::Zealot || me->HasAura(SPELL_LITANY_WARD))
        {
            damage = 0;
            return;
        }
        PaceDamage(damage, Elapsed(), Grid.At(JudgementBeat));
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type != DIRECT_DAMAGE || !victim || !victim->IsPlayer())
            return;
        float const floor = Reference() * MeleeFloorPct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
        damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
    }

    void UpdateAI(uint32 diff) override
    {
        KeepSize();
        if (_phase == Phase::Rehearsal)
        {
            scheduler.Update(diff);
            UpdateLights(diff);
            UpdateAscent();
            UpdateOrbs(diff);
            return;
        }
        if (Fighting())
            WatchDeaths();
        // Kneeling, rising, refusing: no one to hit, his steps still go on (the jailers, the end; phase 2, phase 3)
        if (_phase == Phase::Chained || _phase == Phase::Ascending || _phase == Phase::Refusing)
        {
            scheduler.Update(diff);
            UpdateAscent();
            return;
        }
        if (!UpdateVictim())
        {
            if (!Fighting())
                UpdateOutOfCombat(diff);
            return;
        }

        scheduler.Update(diff);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed && _phase == Phase::Zealot)
        {
            Step const step = _timeline[_next++];
            Execute(step);
        }
        if (_ascendFrom && elapsed >= _ascendFrom + AscendedMusicEndMs && !_ascendedMusicOver)
        {
            _ascendedMusicOver = true;
            EndMusic();
        }
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            EndMusic();
        }
        if (_phase == Phase::Ascended && elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 200;
            CheckWall();
        }
        if (_phase == Phase::Ascended)
        {
            RecordTrail(elapsed);
            while (_p2Next < _p2Timeline.size() && _p2Timeline[_p2Next].trackMs <= TrackMs())
                ExecuteP2(_p2Timeline[_p2Next++], 0);
            UpdateP2(diff);
            if (_phase == Phase::Ascended && !me->HasUnitState(UNIT_STATE_CASTING) && !InCalm())
                DoMeleeAttackIfReady();
        }
        if (_phase == Phase::Rewound)
        {
            if (elapsed >= _nextCheckMs)
            {
                _nextCheckMs = elapsed + 200;
                CheckWall();
            }
            if (_p3From && elapsed >= _p3From + P3MusicEndMs && !_p3MusicOver)
            {
                _p3MusicOver = true;
                EndMusic();
            }
            while (_p3Next < _p3Timeline.size() && _p3Timeline[_p3Next].ms <= P3Ms())
                ExecuteP3(_p3Timeline[_p3Next++]);
            UpdateP3();
            if (_phase == Phase::Rewound && !me->HasUnitState(UNIT_STATE_CASTING) && !InCalmP3())
                DoMeleeAttackIfReady();
        }
        if (_phase != Phase::Zealot)
            return;
        UpdatePace(elapsed, Grid.At(JudgementBeat));
        UpdateLights(diff);
        HoldMiddle();
        if (elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 200;
            CheckWall();
            UpdateTanks();
        }
        if (!CastingBar())
            DoMeleeAttackIfReady();
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        uint32 const elapsed = Elapsed();
        return Acore::StringFormat("Valerian: phase {} at {:.1f}s (beat {:.1f}), shown health {:.1f}%, true {:.1f}%, "
            "next step {}/{}, lights shown {} absorbed {} taken {} on the floor {}", uint32(_phase), elapsed / 1000.0f,
            Grid.BeatOf(elapsed), me->GetHealthPct(), TrueHealthPct(), _next, _timeline.size(), _spawned, _absorbed,
            _taken, _lights.size());
    }

    // The landing's floor round the middle, every 10 degrees clockwise from north (Defi::BossAI MeasureRoom)
    std::string Measure() const
    {
        std::vector<float> const reach = MeasureRoom(36, 60.0f);
        std::string text = "Valerian's floor (yd, from north clockwise):";
        for (float yards : reach)
            text += Acore::StringFormat(" {:.0f}", yards);
        return text;
    }

    // Jumps the fight forward (the music is not: it goes on from where it was)
    bool Skip(uint32 seconds)
    {
        if (_phase != Phase::Zealot)
            return false;
        scheduler.CancelAll();
        EndCastBar();
        AdvanceClock(seconds * IN_MILLISECONDS);
        // The lights still to come, from where the fight is now
        uint32 const beat = CurrentBeat(Grid) + 1;
        uint32 const from = beat <= LightFirstBeat ? LightFirstBeat :
            LightFirstBeat + ((beat - LightFirstBeat + LightEveryBeats - 1) / LightEveryBeats) * LightEveryBeats;
        _lights.clear();
        if (from <= LightLastBeat)
            StartLights(from, LightLastBeat);
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= Elapsed())
            ++_next;
        return true;
    }

    // One pattern now (in a fight, or in the lab's rehearsal)
    // .valerian look's size: kept as his own (SetSize)
    void LookSize(float scale) { SetSize(scale); }

    bool CastNow(std::string const& what)
    {
        // In phase 2: Le Refus now, as if his health had reached nothing (straight to phase 3: a bot run)
        if (what == "refus" && _phase == Phase::Ascended)
        {
            StartRefus();
            return true;
        }
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return false;
        uint32 const now = CurrentBeat(Grid) + 1;
        // His apotheosis and phase 2 now, as if every light had reached him (in the lab: the apotheosis alone, harmless,
        // back to rehearsing after his blow)
        if (what == "ascend")
        {
            _ascendRehearsed = _phase == Phase::Rehearsal;
            scheduler.CancelAll();
            EndCastBar();
            Prelude();
            return true;
        }
        // In the lab: one light LabLightDistance east of him (a game master standing in it counts there)
        if (what == "light")
        {
            ShowLight(now + LightShownBeats, OnArena(0.0f, LabLightDistance));
            return true;
        }
        if (what == "lights")
        {
            StartLights(now, now + 60);
            return true;
        }
        // In the lab: one lance from him eastward, its flash 1.5 s later (the flash against the drawing)
        if (what == "line" && _phase == Phase::Rehearsal)
        {
            Lance(Middle(), 0.0f, 20.0f, 3000);
            scheduler.Schedule(Milliseconds(1500), [this](TaskContext) { ShapedLance(Middle(), 0.0f, 20.0f); });
            return true;
        }
        static std::map<std::string, P2Step> const p2 = {
            { "ailes", { 0, P2Event::Ailes, 0 } }, { "ailes2", { 0, P2Event::Ailes, 1 } },
            { "ailes3", { 0, P2Event::Ailes, 2 } }, { "ailes4", { 0, P2Event::Ailes, 3 } },
            { "sentence3", { 0, P2Event::Sentence, 2 } },
            { "stars", { 0, P2Event::Stars, 0 } }, { "gaze", { 0, P2Event::Gaze, 1 } },
            { "gaze2", { 0, P2Event::Gaze, 2 } }, { "sentence", { 0, P2Event::Sentence, 0 } },
            { "sentence2", { 0, P2Event::Sentence, 1 } }, { "choir", { 0, P2Event::Choir, 0 } },
            { "choir2", { 0, P2Event::Choir, 1 } }, { "echo", { 0, P2Event::Echo } },
            { "ascension", { 0, P2Event::Ascension, 0 } }, { "eclipse", { 0, P2Event::Eclipse, 0 } },
            { "eclipse2", { 0, P2Event::Eclipse, 1 } },
        };
        static std::map<std::string, P3Step> const p3 = {
            { "sabliers", { 0, P3Event::Sabliers, 0 } }, { "sabliers2", { 0, P3Event::Sabliers, 1 } },
            { "sabliers3", { 0, P3Event::Sabliers, 2 } }, { "sabliers4", { 0, P3Event::Sabliers, 3 } },
            { "premonition", { 0, P3Event::Premonition, 0 } }, { "premonition2", { 0, P3Event::Premonition, 1 } },
            { "premonition3", { 0, P3Event::Premonition, 2 } }, { "premonition4", { 0, P3Event::Premonition, 3 } },
            { "chains", { 0, P3Event::Chains, 0 } }, { "chains2", { 0, P3Event::Chains, 1 } },
            { "eclipse3", { 0, P3Event::Eclipse, 2 } }, { "eclipse4", { 0, P3Event::Eclipse, 3 } },
        };
        if (auto const found = p3.find(what); found != p3.end())
        {
            if (_phase != Phase::Rehearsal)
                return false;
            ExecuteP3(found->second);
            return true;
        }
        // In the lab: Lumière totale's sigil, the Ailes cast meanwhile drawn as under it (ailes4: its last set)
        if (what == "totallight" && _phase == Phase::Rehearsal)
        {
            _totalLightRehearsedUntil = Elapsed() + (TotalLightMs - TotalLightCastMs);
            TotalLight(0);
            return true;
        }
        // In the lab: the change, harmless (its sound, the music, the after-images round whoever is there)
        if (what == "refus" && _phase == Phase::Rehearsal)
        {
            StartRefus();
            return true;
        }
        if (auto const found = p2.find(what); found != p2.end())
        {
            if (_phase != Phase::Rehearsal)
                return false;
            ExecuteP2(found->second, 0);
            return true;
        }
        static std::map<std::string, Step> const steps = {
            { "revers", { 0, Event::Revers } }, { "wrath", { 0, Event::Wrath, 0 } },
            { "wrath2", { 0, Event::Wrath, 1 } }, { "chain", { 0, Event::Chain } },
            { "fervor", { 0, Event::Fervor, 0 } }, { "fervor2", { 0, Event::Fervor, 1 } },
            { "rage", { 0, Event::Rage, 0 } }, { "rage2", { 0, Event::Rage, 1 } },
            { "lances", { 0, Event::Lances } }, { "burst", { 0, Event::Burst } },
            { "judgement", { 0, Event::Judgement } }, { "litany", { 0, Event::Litany, 0 } },
            { "litany2", { 0, Event::Litany, 1 } },
        };
        auto const found = steps.find(what);
        if (found == steps.end())
            return false;
        Step step = found->second;
        step.beat = now;
        AtBeat(Grid, step.beat, [this, step]() { Execute(step); });
        return true;
    }

    // The FX lab: he stands at its middle in rehearsal, his arena up, harmless
    void Rehearse()
    {
        _phase = Phase::Rehearsal;
        StartRehearsal(LabMiddle);
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToAll(true);
        me->NearTeleportTo(LabMiddle.GetPositionX(), LabMiddle.GetPositionY(), LabMiddle.GetPositionZ(), 0.0f);
        GroundIndicators::DrawInstantly(me);
        StartClock();
        RaiseArena();
    }

protected:
    Defi::Sizing const& GetSizing() const override
    {
        return _phase == Phase::Ascended ? AscendedSizing : Rewinding() ? RewoundSizing : Sizing;
    }
    float HealthScale() const override
    {
        return _phase == Phase::Ascended ? float(::AscendedHealthScale) : Rewinding() ? float(::RewoundHealthScale) :
            float(::HealthScale);
    }
    bool IsOwn(Creature const* creature) const override
    {
        return creature->GetEntry() == NPC_ORB || creature->GetEntry() == NPC_JAILER ||
            creature->GetEntry() == NPC_LIGHT;
    }

    float TakenFactor(Unit const* victim) const override
    {
        float factor = 1.0f;
        if (uint8 const stacks = StacksOf(victim, SPELL_ZEAL))
            factor *= 1.0f + ZealTakenPct / 100.0f * float(stacks);
        return factor;
    }

private:
    bool Fighting() const
    {
        return _phase == Phase::Zealot || _phase == Phase::Chained || _phase == Phase::Ascending ||
            _phase == Phase::Ascended || Rewinding();
    }

    bool Rewinding() const { return _phase == Phase::Refusing || _phase == Phase::Rewound; }

    // --- The timeline ----------------------------------------------------------------------------------------------
    void Execute(Step const& step)
    {
        uint32 const elapsed = Elapsed();
        LOG_INFO(_logName, "Valerian {} instance={} at={:.2f}s beat={:.1f} shown={:.1f}% true={:.1f}% lights={}/{}",
                 EventName(step.what), me->GetInstanceId(), elapsed / 1000.0f, Grid.BeatOf(elapsed),
                 me->GetHealthPct(), TrueHealthPct(), _absorbed, _spawned);
        uint32 const at = step.beat;
        switch (step.what)
        {
            case Event::Revers: Revers(at); break;
            case Event::Wrath: Wrath(at, step.arg); break;
            case Event::Chain: Chain(at); break;
            case Event::Fervor: Fervor(at, step.arg); break;
            case Event::Rage: Rage(at, step.arg); break;
            case Event::Lances: Lances(at); break;
            case Event::Burst: Burst(at); break;
            case Event::Judgement: Judgement(at); break;
            case Event::Litany: Litany(at, step.arg != 0); break;
        }
    }

    // --- Waves: red drawn on a beat, struck on a later one ----------------------------------------------------------
    void Wave(uint32 shown, uint32 lands, std::function<Areas(uint32)> draw, Rule const& rule)
    {
        AtBeat(Grid, shown, [this, lands, draw, &rule]()
        {
            Areas const areas = draw(MsTo(lands));
            AtBeat(Grid, lands - 1, [this, areas, &rule]() { Probe(areas, rule.name); });
            AtBeat(Grid, lands, [this, areas, &rule]() { Strike(areas, rule); });
        });
    }

    uint32 MsTo(uint32 beat) const
    {
        uint32 const at = Grid.At(beat);
        uint32 const now = Elapsed();
        return at > now ? at - now : 0;
    }

    // Whoever stands in any of the areas: struck by rule. In rehearsal: nobody, and the share of the arena left safe
    // logged (its floor sampled a yard apart)
    void Strike(Areas const& areas, Rule const& rule)
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal && _phase != Phase::Ascending &&
            _phase != Phase::Ascended && !Rewinding())
            return;
        std::vector<Position> spots;
        for (GroundIndicators::Area const& area : areas)
        {
            spots.push_back(BurstSpot(area));
            Impact(rule.look, area);
        }
        if (rule.sound)
            SoundNearest(rule.sound, spots);
        if (_phase == Phase::Rehearsal || _ascendRehearsed)
        {
            LogSafe(rule.name, areas);
            return;
        }
        for (Player* player : ArenaPlayers())
            for (GroundIndicators::Area const& area : areas)
                if (area.Contains(player->GetPosition()))
                {
                    Mistake(player, rule);
                    break;
                }
    }

    // A beat before a strike: every bot still in it, what it is doing and whether it sees a way out
    void Probe(Areas const& areas, char const* name)
    {
        if (_phase != Phase::Zealot)
            return;
        Position const middle = Middle();
        for (Player* player : ArenaPlayers())
            for (GroundIndicators::Area const& area : areas)
                if (area.Contains(player->GetPosition()))
                {
                    Position escape;
                    bool const escaping = GroundIndicators::FindEscape(player, escape, IsTank(player));
                    LOG_INFO(_logName, "Valerian probe instance={} at={:.1f}s {}: {} class={} tank={} "
                             "at={:.1f}yd/{:.0f}deg moving={} escape={} to={:.1f}yd/{:.0f}deg", me->GetInstanceId(),
                             Elapsed() / 1000.0f, name, player->GetName(), player->getClass(), IsTank(player),
                             player->GetExactDist2d(&middle), middle.GetAbsoluteAngle(player) * 180.0f / float(M_PI),
                             player->isMoving(), escaping, escaping ? escape.GetExactDist2d(&middle) : -1.0f,
                             escaping ? middle.GetAbsoluteAngle(&escape) * 180.0f / float(M_PI) : 0.0f);
                    break;
                }
    }

    void LogSafe(char const* name, Areas const& areas) const
    {
        Position const middle = Middle();
        uint32 total = 0;
        uint32 safe = 0;
        for (float x = -ArenaRadius; x <= ArenaRadius; x += 1.0f)
            for (float y = -ArenaRadius; y <= ArenaRadius; y += 1.0f)
            {
                if (x * x + y * y > (ArenaRadius - WallMargin) * (ArenaRadius - WallMargin))
                    continue;
                Position const at(middle.GetPositionX() + x, middle.GetPositionY() + y, middle.GetPositionZ());
                ++total;
                bool hit = false;
                for (GroundIndicators::Area const& area : areas)
                    if (area.Contains(at))
                        hit = true;
                if (!hit)
                    ++safe;
            }
        LOG_INFO(_logName, "Valerian rehearsal {}: {} of {} square yards safe ({:.0f}%)", name, safe, total,
                 total ? 100.0f * float(safe) / float(total) : 0.0f);
    }

    Position Middle() const { return Ground(GetRoom().center); }

    // --- His impacts --------------------------------------------------------------------------------------------------
    // A strike's look laid out from its area (Look)
    void Impact(Look look, GroundIndicators::Area const& area)
    {
        if (look == Look::None)
            return;
        Position const& origin = area.origin;
        float const facing = origin.GetOrientation();
        switch (area.kind)
        {
            case GroundIndicators::Area::Kind::Cone:
                ShapedCone(origin, facing, area.arc);
                break;
            case GroundIndicators::Area::Kind::Ring:
                ShapedImpact(SHAPED_HOLY_RING_7_17, origin, 0.0f);
                break;
            case GroundIndicators::Area::Kind::Rectangle:
                ShapedLance(origin, facing, area.radius);
                break;
            case GroundIndicators::Area::Kind::Circle:
                Blast(origin, area.radius);
                break;
            default:
                break;
        }
    }

    // A shaped kit (Kits) from origin along axis: its forms run along the model's +Y, a quarter turn left of the way it
    // faces
    void ShapedImpact(uint32 kit, Position const& origin, float axis)
    {
        Position spot = origin;
        spot.SetOrientation(Position::NormalizeOrientation(axis - float(M_PI) / 2.0f));
        GroundIndicators::PlayKit(me, spot, kit, 1.0f, 0, ImpactMs);
    }

    // A cone from apex along facing, arc wide (radians): its quarters side by side
    void ShapedCone(Position const& apex, float facing, float arc)
    {
        float const quarter = float(M_PI) / 2.0f;
        uint32 const quarters = std::max<uint32>(1, uint32(arc / quarter + 0.5f));
        float edge = facing - float(quarters) * quarter / 2.0f;
        for (uint32 index = 0; index < quarters; ++index, edge += quarter)
            ShapedImpact(SHAPED_HOLY_CONE_90, apex, edge + quarter / 2.0f);
    }

    // A lance from origin along facing, length long: its tiles end to end, the last flush with its end
    void ShapedLance(Position const& origin, float facing, float length)
    {
        ShapedLine(origin, facing, length, LanceWidth);
    }

    // A line's flash from origin along facing: its own painting flaring over it (a lance in its tiles, a tendril whole).
    // The 10 x 2.5 kit was played for it first: it ignored its carrier's facing (seen in the FX lab, every flash ran
    // the same way across its line - the grid's flashes read as shifted)
    void ShapedLine(Position const& origin, float facing, float length, float width)
    {
        if (width > LanceWidth)
        {
            GroundIndicators::ShowDecal(me, origin, facing, length, LineFlareMs, LOOK_TENDRIL_FLARE);
            return;
        }
        float const tile = LanceWidth * LanceTileRatio;
        float done = 0.0f;
        for (; length - done >= tile; done += tile)
            GroundIndicators::ShowDecal(me, Ground(AtAngle(origin, facing, done)), facing, tile, LineFlareMs,
                LOOK_LANCE_FLARE);
        if (length - done > 0.5f)
            GroundIndicators::ShowDecal(me, Ground(AtAngle(origin, facing, std::max(0.0f, length - tile))), facing,
                std::min(tile, length), LineFlareMs, LOOK_LANCE_FLARE);
    }

    // A circle radius yards: the golden flash sized to it, Light's Judgement's sparks round its edge
    void Blast(Position const& at, float radius)
    {
        GroundIndicators::PlayKit(me, at, KIT_HOLY_BLAST, std::max(0.3f, radius / BlastReach), 0, ImpactMs);
        GroundIndicators::PlayKit(me, at, KIT_LIGHTS_JUDGEMENT, std::max(0.3f, radius / JudgementReach), 0, ImpactMs);
    }

    // One of his sounds (client-assets/audio/valerian.json) at where, for everyone hearing the fight
    void Sound(char const* key, Position const& where)
    {
        for (Player* player : Listeners())
            EvolutionsAudio::PlayAt(player, key, where);
    }

    // A sound with several sources (the marks, the lines of a grid): each listener hears it once, from the nearest
    void SoundNearest(char const* key, std::vector<Position> const& wheres)
    {
        if (wheres.empty())
            return;
        for (Player* player : Listeners())
        {
            Position const* nearest = &wheres.front();
            for (Position const& where : wheres)
                if (player->GetExactDist2d(&where) < player->GetExactDist2d(nearest))
                    nearest = &where;
            EvolutionsAudio::PlayAt(player, key, *nearest);
        }
    }
    Position OnArena(float angle, float distance) const { return AtAngle(GetRoom().center, angle, distance); }

    // Where a strike's light bursts: a circle's middle, a cone's or a line's halfway along it, inside the wall
    Position BurstSpot(GroundIndicators::Area const& area) const
    {
        float const facing = area.origin.GetOrientation();
        switch (area.kind)
        {
            case GroundIndicators::Area::Kind::Rectangle:
            case GroundIndicators::Area::Kind::Cone:
                return Ground(Position(area.origin.GetPositionX() + std::cos(facing) * area.radius / 2.0f,
                    area.origin.GetPositionY() + std::sin(facing) * area.radius / 2.0f, area.origin.GetPositionZ()));
            default:
                return area.origin;
        }
    }

    // Players at random, the tanks left out
    std::vector<Player*> Targets(uint32 count)
    {
        std::vector<Player*> others;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player))
                others.push_back(player);
        Acore::Containers::RandomShuffle(others);
        if (others.size() > count)
            others.resize(count);
        return others;
    }

    // A cone from the middle to the wall, arcDegrees wide (as look is painted), around facing; aimedAt: the tank it is
    // aimed at
    GroundIndicators::Area Cone(float facing, float arcDegrees, uint32 look, uint32 ms, Unit* aimedAt = nullptr)
    {
        GroundIndicators::Area area = MakeCircle(Middle(), ArenaRadius + 1.0f);
        area.kind = GroundIndicators::Area::Kind::Cone;
        area.origin.SetOrientation(facing);
        area.arc = arcDegrees * float(M_PI) / 180.0f;
        return Painted(area, look, ms, aimedAt);
    }

    // His areas drawn with his looks (Looks) and the layer moving over each (LayerOf), registered as the red ones are
    GroundIndicators::Area Painted(GroundIndicators::Area const& area, uint32 look, uint32 ms, Unit* aimedAt = nullptr)
    {
        if (uint32 const layer = LayerOf(look))
            GroundIndicators::ShowDecal(me, area.origin, area.origin.GetOrientation(), area.radius, ms, layer);
        return GroundIndicators::ShowPainted(me, area, look, ms, GroundIndicators::Theme::None, MistakeDamage, nullptr,
            0, aimedAt);
    }

    static GroundIndicators::Area MakeCircle(Position const& center, float radius)
    {
        GroundIndicators::Area area;
        area.kind = GroundIndicators::Area::Kind::Circle;
        area.origin = center;
        area.radius = radius;
        return area;
    }

    // A lance from start along facing, length long, LanceWidth wide: registered whole, drawn in tiles of the lance
    // (LanceTileRatio times its width each), the last one flush with its end
    // painted false: watched (the bots keep out of it) but not drawn (La Litanie: the spear in flight is its look)
    GroundIndicators::Area Lance(Position const& start, float facing, float length, uint32 ms, bool painted = true)
    {
        GroundIndicators::Area area = MakeCircle(start, length);
        area.kind = GroundIndicators::Area::Kind::Rectangle;
        area.origin.SetOrientation(facing);
        area.width = LanceWidth;
        GroundIndicators::WatchArea(me, area, ms, MistakeDamage);
        if (!painted)
            return area;
        float const tile = LanceWidth * LanceTileRatio;
        float done = 0.0f;
        for (; length - done >= tile; done += tile)
            GroundIndicators::ShowDecal(me, Ground(AtAngle(start, facing, done)), facing, tile, ms, LOOK_LANCE_TILE);
        if (length - done > 0.5f)
            GroundIndicators::ShowDecal(me, Ground(AtAngle(start, facing, std::max(0.0f, length - tile))), facing,
                std::min(tile, length), ms, LOOK_LANCE_TILE);
        return area;
    }

    // --- The arena ---------------------------------------------------------------------------------------------------
    // Its wall rises round the middle (Vorhan's pieces, WallPieces of them: ArenaRadius) and kills from WallRiseBeats on.
    // For the bots, everything past it is one area to keep out of, all fight.
    void RaiseArena()
    {
        Position const middle = Middle();
        uint32 const shows = _phase == Phase::Rehearsal ? 0 : WallShowBeats;
        _wallRadius = ArenaRadius;
        AtBeat(Grid, shows, [this, middle]()
        {
            GroundIndicators::ShowDecal(me, middle, 0.0f, ArenaRadius, WallRiseMs, LOOK_WALL_RISE);
            ShowWall(WallPieces);
            Sound("Valerian.WallRise", middle);
        });
        _wallFrom = Grid.At(shows + WallRiseBeats) + WallDrawInGraceMs;
        AtBeat(Grid, shows + WallRiseBeats, [this]() { DrawIn(); });
        KeepBotsIn(WallLastsMs);
    }

    // The wall's pieces round the middle (its light behind, its bars, its motes in front), until pushed or the end
    void ShowWall(uint32 pieces)
    {
        Position const middle = Middle();
        float const piece = GroundIndicators::WardenWallPieceLength;
        GroundIndicators::ShowCurtainRing(me, middle, LOOK_WALL_LIGHT, 1, pieces, piece * WallLightOut, WallLastsMs);
        GroundIndicators::ShowCurtainRing(me, middle, LOOK_WALL_BARS_FIRST, WallBarsLooks, pieces, piece, WallLastsMs);
        GroundIndicators::ShowCurtainRing(me, middle, LOOK_WALL_MOTES, 1, pieces, piece * WallMotesIn, WallLastsMs);
    }

    // His god's blow pushes the wall back: the old one bursting where it stood and fading, the new one rising at the
    // rim of the platform (AscendedArenaRadius) - still deadly, the bots kept inside it
    void PushWall()
    {
        Position const middle = Middle();
        float const was = _wallRadius;
        GroundIndicators::FadeLooksOf(me, LOOK_WALL_BARS_FIRST, LOOK_WALL_MOTES);
        for (uint32 index = 0; index < WallPieces; ++index)
            GroundIndicators::Burst(me, OnArena(float(index) * 2.0f * float(M_PI) / float(WallPieces), was),
                GroundIndicators::Theme::Holy);
        _wallRadius = AscendedArenaRadius;
        // Anyone past the new wall drawn inside it, the wall deadly only once they are (as at the pull)
        DrawIn();
        _wallFrom = Elapsed() + WallDrawInGraceMs;
        GroundIndicators::ShowDecal(me, middle, 0.0f, _wallRadius, WallRiseMs, LOOK_WALL_RISE);
        scheduler.Schedule(Milliseconds(WallPushRiseMs), [this, middle](TaskContext)
        {
            ShowWall(WallPiecesAscended);
            Sound("Valerian.WallRise", middle);
        });
        if (_phase != Phase::Rehearsal)
        {
            // The old wall's ring for the bots ended first (left on, phase 1's 16 yd held them in there: the beacons
            // at 22 yd were past it)
            GroundIndicators::ClearAreasOf(me);
            KeepBotsIn(WallLastsMs);
        }
    }

    void KeepBotsIn(uint32 lastsMs)
    {
        GroundIndicators::Area outside;
        outside.kind = GroundIndicators::Area::Kind::Ring;
        outside.origin = Middle();
        outside.radius = 60.0f;
        outside.inner = _wallRadius - WallMargin - 0.5f;
        GroundIndicators::WatchArea(me, outside, lastsMs, MistakeDamage);
    }

    // Everyone past the wall set just inside it, where they stood round it
    void DrawIn()
    {
        if (_phase != Phase::Zealot && _phase != Phase::Ascending && _phase != Phase::Ascended)
            return;
        Position const middle = Middle();
        for (Player* player : ArenaPlayers())
            if (player->GetExactDist2d(&middle) > _wallRadius - WallMargin)
            {
                Position const in = AtAngle(middle, middle.GetAngle(player), _wallRadius - 3.0f);
                player->NearTeleportTo(in.GetPositionX(), in.GetPositionY(), in.GetPositionZ() + 0.5f,
                    player->GetOrientation());
            }
    }

    // Past the wall: death (from WallRiseBeats on, everyone drawn in then)
    void CheckWall()
    {
        if (Elapsed() < _wallFrom)
            return;
        Position const middle = Middle();
        for (Player* player : ArenaPlayers())
            if (player->GetExactDist2d(&middle) > _wallRadius - WallMargin)
            {
                LOG_INFO(_logName, "Valerian wall instance={} at={:.1f}s {} at={:.1f}yd/{:.0f}deg moving={}",
                         me->GetInstanceId(), Elapsed() / 1000.0f, player->GetName(), player->GetExactDist2d(&middle),
                         middle.GetAbsoluteAngle(player) * 180.0f / float(M_PI), player->isMoving());
                Doom(player, SPELL_WALL);
            }
    }

    // The middle: he stands there all fight, rooted; anything moving him (a knockback, a grip) has him leap back
    void HoldMiddle()
    {
        Position const middle = Middle();
        if (me->GetExactDist2d(&middle) > 1.5f)
        {
            if (!me->HasUnitState(UNIT_STATE_JUMPING))
            {
                me->SetControlled(false, UNIT_STATE_ROOT);
                me->GetMotionMaster()->MoveJump(middle, 20.0f, 4.0f);
            }
            return;
        }
        if (!me->HasUnitState(UNIT_STATE_ROOT))
            me->SetControlled(true, UNIT_STATE_ROOT);
    }

    // The tanks: his tank in front, the other at his side (out of both of Revers sacré's cones); the swap at
    // SwapStacks of Zèle brûlant
    void UpdateTanks()
    {
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (!tank)
            return;
        GroundIndicators::SetOffTankSpot(me, AtAngle(me->GetPosition(), me->GetAngle(tank) + float(M_PI) / 2.0f,
            4.0f), 1000);
        // Never between Revers sacré's two cones: the boss turned to the new tank, and the old one stood in the back cone
        if (StacksOf(tank, SPELL_ZEAL) < SwapStacks || Elapsed() < _swapHeldUntil)
            return;
        for (Player* player : ArenaPlayers())
            if (player != tank && IsTank(player) && StacksOf(player, SPELL_ZEAL) < SwapStacks)
            {
                GroundIndicators::SetBossHolder(me, player, 8000);
                return;
            }
    }

    // --- The bots' places -------------------------------------------------------------------------------------------
    // The bots of the arena (a real player is never moved), tanks left out unless withTanks; except: one left alone (his
    // tank on Revers sacré)
    std::vector<Player*> Bots(bool withTanks, ObjectGuid except = ObjectGuid::Empty) const
    {
        std::vector<Player*> bots;
        for (Player* player : ArenaPlayers())
            if (player->GetSession() && player->GetSession()->IsBot() && player->GetGUID() != except &&
                (withTanks || !IsTank(player)))
                bots.push_back(player);
        return bots;
    }

    // Each bot to a spot of its own among spots (the nearest one still free; more bots than spots: shared), for ms,
    // stopping within slack yards of it
    void Guide(std::vector<Player*> const& bots, std::vector<Position> const& spots, uint32 ms,
               float slack = GuideSlack)
    {
        if (spots.empty() || _phase == Phase::Rehearsal)
            return;
        std::vector<bool> taken(spots.size(), false);
        for (Player* bot : bots)
        {
            if (std::find(taken.begin(), taken.end(), false) == taken.end())
                std::fill(taken.begin(), taken.end(), false);
            size_t best = spots.size();
            for (size_t index = 0; index < spots.size(); ++index)
                if (!taken[index] && (best == spots.size() ||
                        bot->GetExactDist2d(&spots[index]) < bot->GetExactDist2d(&spots[best])))
                    best = index;
            taken[best] = true;
            GroundIndicators::SetUnitSpot(me, bot, spots[best], slack, ms);
        }
    }

    // count spots on the circle radius yards from his middle, spread over arcDegrees around towards
    std::vector<Position> Fan(float towards, float arcDegrees, float radius, uint32 count) const
    {
        std::vector<Position> spots;
        float const arc = arcDegrees * float(M_PI) / 180.0f;
        for (uint32 index = 0; index < count; ++index)
        {
            float const share = count > 1 ? float(index) / float(count - 1) - 0.5f : 0.0f;
            spots.push_back(OnArena(towards + share * arc, radius));
        }
        return spots;
    }

    // --- The lights ------------------------------------------------------------------------------------------------------
    struct Light
    {
        Position at;
        uint32 lands;           // its beat
        bool occupied = false;  // someone stood in it at the last check
        ObjectGuid look;        // its soak (NPC_LIGHT)
    };

    // A soak every LightEveryBeats from beat from to beat to (each landing there), shown LightShownBeats before
    void StartLights(uint32 from, uint32 to)
    {
        if (_phase != Phase::Rehearsal && _phase != Phase::Zealot)
            return;
        for (uint32 beat = from; beat <= to; beat += LightEveryBeats)
            AtBeat(Grid, beat > LightShownBeats ? beat - LightShownBeats : 0, [this, beat]() { ShowLight(beat); });
    }

    // Somewhere in the arena, away from him and from the wall, on another side than the last one (or at, if given)
    void ShowLight(uint32 lands, std::optional<Position> at = {})
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return;
        float const angle = Position::NormalizeOrientation(_lastLightAngle + frand(float(M_PI) / 2.0f,
            3.0f * float(M_PI) / 2.0f));
        _lastLightAngle = angle;
        Light light;
        light.at = at ? *at : OnArena(angle, frand(LightNear, ArenaRadius - LightWallGap));
        light.lands = lands;
        ++_spawned;
        uint32 const ms = MsTo(lands);
        if (Creature* soak = me->SummonCreature(NPC_LIGHT, light.at, TEMPSUMMON_TIMED_DESPAWN, ms + SoakLandMs))
        {
            soak->SetObjectScale(LightRadius / LightModelRadius);
            soak->SetReactState(REACT_PASSIVE);
            light.look = soak->GetGUID();
        }
        // The bots leave it alone: never theirs to take
        GroundIndicators::Area area;
        area.kind = GroundIndicators::Area::Kind::Circle;
        area.origin = light.at;
        area.radius = LightRadius + 0.5f;
        GroundIndicators::WatchArea(me, area, ms, MistakeDamage);
        _lights.push_back(light);
        AtBeat(Grid, lands, [this, lands]() { LandLight(lands); });
    }

    // Its look: someone in it (its wall sinks and holds sunk), or no one any more (it rises again)
    void LightLook(Light const& light, bool occupied)
    {
        Creature* soak = light.look ? me->GetMap()->GetCreature(light.look) : nullptr;
        if (!soak)
            return;
        if (!occupied)
        {
            soak->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
            soak->HandleEmoteCommand(EMOTE_SOAK_RISE);
            return;
        }
        soak->HandleEmoteCommand(EMOTE_SOAK_SINK);
        ObjectGuid const guid = light.look;
        scheduler.Schedule(Milliseconds(SoakSinkMs), [this, guid](TaskContext)
        {
            for (Light const& still : _lights)
                if (still.look == guid && still.occupied)
                    if (Creature* sunk = me->GetMap()->GetCreature(guid))
                        sunk->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_SOAK_SUNK);
        });
    }

    std::vector<Player*> InLight(Light const& light) const
    {
        std::vector<Player*> in;
        if (_phase == Phase::Rehearsal)
        {
            // The lab: its game master tries the look (counted, never struck: LandLight strikes in a fight only)
            for (auto const& ref : me->GetMap()->GetPlayers())
                if (Player* player = ref.GetSource(); player && player->GetExactDist2d(&light.at) <= LightRadius)
                    in.push_back(player);
            return in;
        }
        for (Player* player : ArenaPlayers())
            if (player->GetExactDist2d(&light.at) <= LightRadius)
                in.push_back(player);
        return in;
    }

    // Every LightCheckMs: a light someone stands in shows it (and stops showing it once they leave)
    void UpdateLights(uint32 diff)
    {
        _lightTimer += diff;
        if (_lightTimer < LightCheckMs)
            return;
        _lightTimer = 0;
        for (Light& light : _lights)
        {
            bool const occupied = !InLight(light).empty();
            if (occupied != light.occupied)
            {
                light.occupied = occupied;
                LightLook(light, occupied);
            }
        }
    }

    // It lands: taken by whoever stands in it, or its light flies to him
    void LandLight(uint32 lands)
    {
        auto const found = std::ranges::find(_lights, lands, &Light::lands);
        if (found == _lights.end())
            return;
        Light const light = *found;
        _lights.erase(found);
        if (Creature* soak = light.look ? me->GetMap()->GetCreature(light.look) : nullptr)
        {
            soak->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
            soak->HandleEmoteCommand(EMOTE_SOAK_LAND);
            soak->DespawnOrUnsummon(Milliseconds(SoakLandMs));
        }
        std::vector<Player*> const in = InLight(light);
        if (!in.empty() && _phase == Phase::Zealot)
        {
            ++_taken;
            Sound("Valerian.OrbTaken", light.at);
            for (Player* player : in)
            {
                AddTimedAura(player, SPELL_STOLEN, StolenMs, uint8(std::min(99, StacksOf(player, SPELL_STOLEN) + 1)));
                LOG_INFO(_logName, "Valerian light taken instance={} at={:.1f}s by {} bot={} taken={}",
                         me->GetInstanceId(), Elapsed() / 1000.0f, player->GetName(),
                         player->GetSession() && player->GetSession()->IsBot(), _taken);
            }
            return;
        }
        // Its light flies to him (only seen: it is his already)
        Position from = light.at;
        from.m_positionZ += LightHeight;
        if (Creature* orb = me->SummonCreature(NPC_ORB, from, TEMPSUMMON_TIMED_DESPAWN,
                uint32(light.at.GetExactDist2d(Middle()) / LightFlight * 1000.0f) + 200))
        {
            orb->SetCanFly(true);
            orb->SetDisableGravity(true);
            orb->SetReactState(REACT_PASSIVE);
            Position to = Middle();
            to.m_positionZ += LightHeight;
            orb->GetMotionMaster()->MovePoint(1, to, FORCED_MOVEMENT_NONE, LightFlight, false);
        }
        ++_absorbed;
        Sound("Valerian.OrbAbsorb", Middle());
        ShowAbsorbed();
        if (_phase == Phase::Zealot)
            for (Player* player : ArenaPlayers())
                HitOwnHealth(player, SPELL_LIGHT_RETURN, LightReturnPct);
    }

    // His bar: twice the phase's health (SetModelHealth: its sizing's), full (phase 1) or at half (phase 2: on from
    // where phase 1 left it) - each phase's own health is half the bar
    void SetHalfBar(bool full)
    {
        SetModelHealth();
        uint32 const phase = me->GetMaxHealth();
        me->SetCreateHealth(phase * 2);
        me->SetMaxHealth(phase * 2);
        me->SetHealth(full ? phase * 2 : phase);
    }

    // His size, kept: an aura changing a unit's size (players put many on him) makes the core recompute it from his
    // template's (Unit::RecalculateObjectScale), which dropped his growth - put back on his next update
    void SetSize(float scale)
    {
        _size = scale;
        me->SetObjectScale(scale);
    }

    void KeepSize()
    {
        if (_size > 0.0f && std::fabs(me->GetObjectScale() - _size) > 0.001f)
            me->SetObjectScale(_size);
    }

    void ShowAbsorbed()
    {
        float const share = std::min(1.0f, float(_absorbed) / float(LightCount));
        me->SetMaxPower(POWER_MANA, 100);
        me->SetPower(POWER_MANA, uint32(share * 100.0f));
        SetSize(BaseScale * (1.0f + 0.25f * share));
    }

    void ClearLights()
    {
        for (Light const& light : _lights)
            if (Creature* soak = light.look ? me->GetMap()->GetCreature(light.look) : nullptr)
                soak->DespawnOrUnsummon();
        _lights.clear();
    }

    // --- Revers sacré: a cone on his tank, then the same behind him ----------------------------------------------------
    void Revers(uint32 start)
    {
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        CastBar(CAST_REVERS);
        Pose(EMOTE_STATE_READY1H);
        Sound("Valerian.ReversCast", me->GetPosition());
        float const facing = tank ? me->GetAngle(tank) : me->GetOrientation();
        if (tank)
            me->SetFacingToObject(tank);
        ObjectGuid const tankGuid = tank ? tank->GetGUID() : ObjectGuid::Empty;
        uint32 const lands = start + WarnBeats;
        // His tank takes the front; everyone else at his sides, out of both cones, until the back one lands
        _swapHeldUntil = Grid.At(lands + ReversBackBeats) + 500;
        std::vector<Position> sides = Fan(facing + float(M_PI) / 2.0f, 40.0f, GuideNear + 1.0f, 4);
        for (Position const& spot : Fan(facing - float(M_PI) / 2.0f, 40.0f, GuideNear + 1.0f, 4))
            sides.push_back(spot);
        Guide(Bots(true, tankGuid), sides, MsTo(lands + ReversBackBeats) + 300);
        GroundIndicators::Area const front = Cone(facing, ReversArc, LOOK_REVERS_CONE_90, MsTo(lands), tank);
        AtBeat(Grid, lands - 1, [this, front]() { Probe({ front }, "revers"); });
        AtBeat(Grid, lands, [this, front, tankGuid]()
        {
            EndCastBar();
            Gesture(EMOTE_ONESHOT_ATTACK1H);
            Sound("Valerian.Revers", BurstSpot(front));
            Impact(Look::Shaped, front);
            if (_phase == Phase::Rehearsal)
            {
                LogSafe("revers front", { front });
                return;
            }
            for (Player* player : ArenaPlayers())
            {
                if (player->GetGUID() == tankGuid)
                {
                    Hit(player, SPELL_REVERS, ReversTankPct);
                    AddTimedAura(player, SPELL_ZEAL, ZealMs, uint8(std::min(10, StacksOf(player, SPELL_ZEAL) + 1)));
                }
                else if (front.Contains(player->GetPosition()))
                    Mistake(player, RuleRevers);
            }
        });
        // The same cone behind him, shown as the first strikes: his backhand as it lands
        AtBeat(Grid, lands + ReversBackBeats, [this]() { Gesture(EMOTE_ONESHOT_SPECIALATTACK1H); });
        float const back = Position::NormalizeOrientation(facing + float(M_PI));
        Wave(lands, lands + ReversBackBeats, [this, back](uint32 ms)
        {
            return Areas{ Cone(back, ReversArc, LOOK_REVERS_CONE_90, ms) };
        }, RuleRevers);
    }

    // --- Colère du juste: half the arena, then the other half - or the same one again -----------------------------------
    // twist 0: the other half; 1: either, at random (his fist raised again over the half it will be)
    void Wrath(uint32 start, uint32 twist)
    {
        CastBar(CAST_WRATH);
        uint32 const first = start + WarnBeats;
        AtBeat(Grid, first, [this]() { EndCastBar(); });
        float const facing = frand(0.0f, 2.0f * float(M_PI));
        bool const same = twist && urand(0, 1);
        float const second = same ? facing : Position::NormalizeOrientation(facing + float(M_PI));
        // The bots: close to him in the half that stays safe, then in the one that does (across him, or not at all)
        Guide(Bots(true), Fan(facing + float(M_PI), 120.0f, GuideNear, 6), MsTo(first) + 200);
        AtBeat(Grid, first, [this, second, first]()
        {
            Guide(Bots(true), Fan(second + float(M_PI), 120.0f, GuideNear, 6), MsTo(first + WrathFollowBeats) + 200);
        });
        // His fist raised over the half: its light where it will strike
        AtBeat(Grid, start, [this, facing]() { Fist(facing); });
        AtBeat(Grid, first, [this, second]() { Fist(second); });
        AtBeat(Grid, first + WrathFollowBeats, [this]() { Gesture(EMOTE_ONESHOT_SPELL_CAST_OMNI); });
        Wave(start, first, [this, facing](uint32 ms)
        {
            return Areas{ Cone(facing, 180.0f, LOOK_RADIANCE_CONE_180, ms) };
        }, RuleWrath);
        Wave(first, first + WrathFollowBeats, [this, second](uint32 ms)
        {
            return Areas{ Cone(second, 180.0f, LOOK_RADIANCE_CONE_180, ms) };
        }, RuleWrath);
    }

    void Fist(float towards)
    {
        me->SetFacingTo(towards);
        me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
        Sound("Valerian.WrathCast", AtAngle(Middle(), towards, 3.0f));
    }

    // --- Chaîne brisée: all round him but a gap, the gap turning a little each strike ------------------------------------
    void Chain(uint32 start)
    {
        CastBar(CAST_CHAIN);
        Pose(EMOTE_STATE_SPELL_CHANNEL_DIRECTED);
        Sound("Valerian.ChainSpin", me->GetPosition());
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const turn = way * ChainTurnDegrees * float(M_PI) / 180.0f;
        float gap = frand(0.0f, 2.0f * float(M_PI));
        uint32 shown = start;
        uint32 lands = start + WarnBeats;
        for (uint32 strike = 0; strike < ChainStrikes; ++strike, gap += turn)
        {
            // The bots: close to him in the part of this gap the next one keeps (its leading side)
            float const keeps = gap + turn / 2.0f;
            uint32 const until = lands;
            AtBeat(Grid, shown, [this, keeps, until]()
            {
                Guide(Bots(true), Fan(keeps, 40.0f, GuideNear, 4), MsTo(until) + 200);
            });
            Wave(shown, lands, [this, gap](uint32 ms) { return Sweep(gap, ms); }, RuleChain);
            bool const last = strike + 1 == ChainStrikes;
            AtBeat(Grid, lands, [this, last]() { Gesture(EMOTE_ONESHOT_ATTACK1H, last); });
            shown = lands;
            lands += ChainStepBeats;
        }
        AtBeat(Grid, shown, [this]() { EndCastBar(); });
    }

    // Everything round him but ChainGapDegrees around gap: one 270 degree cone, turned away from the gap
    Areas Sweep(float gap, uint32 ms)
    {
        static_assert(ChainGapDegrees == 90.0f, "the sweep is a 270 degree cone");
        return Areas{ Cone(Position::NormalizeOrientation(gap + float(M_PI)), 360.0f - ChainGapDegrees,
            LOOK_RADIANCE_CONE_270, ms) };
    }

    // --- Ferveur: three marked to spread - turned to a stack at the last moment ------------------------------------------
    // twist 0: a spread; 1: turned to a stack FervorFlipBeats before it explodes; 2: either, at random
    void Fervor(uint32 start, uint32 twist)
    {
        CastBar(CAST_FERVOR);
        Gesture(EMOTE_ONESHOT_POINT, false);
        Pose(EMOTE_STATE_READY_SPELL_OMNI);
        uint32 const lands = start + FervorBeats;
        uint32 const flips = lands - FervorFlipBeats;
        bool const stack = twist == 1 || (twist == 2 && urand(0, 1));
        AtBeat(Grid, flips, [this]() { EndCastBar(); });
        std::vector<ObjectGuid> marks;
        std::vector<Player*> carriers = Targets(FervorMarks);
        for (Player* carrier : carriers)
        {
            marks.push_back(carrier->GetGUID());
            AddTimedAura(carrier, SPELL_FERVOR_MARK, MsTo(lands));
            uint32 const spreadMs = MsTo(stack ? flips : lands);
            GroundIndicators::ShowCarriedLook(carrier, LOOK_FERVOR_SPREAD, spreadMs);
            GroundIndicators::ShowCarriedLook(carrier, LOOK_MARK_FERVOR, spreadMs);
            GroundIndicators::WatchCarriedCircle(me, carrier, FervorRadius, spreadMs, MistakeDamage);
        }
        std::vector<Position> marked;
        for (Player* carrier : carriers)
            marked.push_back(carrier->GetPosition());
        SoundNearest("Valerian.FervorMark", marked);
        // The bots: a marked one to a point of its own far from the others (a third of the circle apart), everyone else
        // close to him, between them
        {
            float const first = frand(0.0f, 2.0f * float(M_PI));
            std::vector<Position> apart;
            for (uint32 index = 0; index < FervorMarks; ++index)
                apart.push_back(OnArena(first + float(index) * 2.0f * float(M_PI) / float(FervorMarks), GuideMarks));
            std::vector<Player*> markedBots;
            std::vector<Player*> others;
            for (Player* bot : Bots(true))
                (std::find(carriers.begin(), carriers.end(), bot) != carriers.end() ? markedBots : others)
                    .push_back(bot);
            uint32 const ms = MsTo(stack ? flips : lands) + 200;
            Guide(markedBots, apart, ms);
            Guide(others, Fan(first + float(M_PI) / float(FervorMarks), 300.0f, GuideNear - 1.0f, 6), ms);
        }
        if (_phase == Phase::Rehearsal && carriers.empty())
            LogSafe("fervor (no carrier in the lab)", {});
        if (stack)
            AtBeat(Grid, flips, [this, marks, lands]()
            {
                // The marks turn: a soak on each, its own colour, the others to come under them
                Sound("Valerian.FervorFlip", Middle());
                for (ObjectGuid const& guid : marks)
                    if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                    {
                        GroundIndicators::ClearCarriedLook(carrier, LOOK_FERVOR_SPREAD);
                        GroundIndicators::ClearCarriedLook(carrier, LOOK_MARK_FERVOR);
                        GroundIndicators::ShowCarriedLook(carrier, LOOK_FERVOR_STACK, MsTo(lands));
                        GroundIndicators::ShowCarriedLook(carrier, LOOK_MARK_FERVOR_STACK, MsTo(lands));
                        GroundIndicators::ShowSoak(me, carrier->GetPosition(), FervorRadius, MsTo(lands),
                            uint32(ArenaPlayers().size() / FervorMarks), GroundIndicators::Theme::Holy, false,
                            false);
                    }
                FollowStacks(marks, lands);
            });
        AtBeat(Grid, lands, [this, marks, stack]()
        {
            std::vector<Position> bursts;
            for (ObjectGuid const& guid : marks)
                if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                    bursts.push_back(carrier->GetPosition());
            SoundNearest("Valerian.Fervor", bursts);
            Gesture(EMOTE_ONESHOT_SPELL_CAST_OMNI);
            for (Position const& at : bursts)
                Blast(at, FervorRadius);
            for (ObjectGuid const& guid : marks)
                if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                {
                    carrier->RemoveAurasDueToSpell(SPELL_FERVOR_MARK);
                    if (stack)
                        StrikeStack(carrier);
                    else
                        StrikeSpread(carrier);
                }
        });
    }

    // Turned to a stack: the marked bots hold still where they are; every other bot to a mark, shared out evenly (the
    // nearest mark with room: as many under each), followed as it moves, until it lands
    void FollowStacks(std::vector<ObjectGuid> const& marks, uint32 lands)
    {
        if (CurrentBeat(Grid) >= lands || _phase == Phase::Over)
            return;
        uint32 const ms = MsTo(lands);
        std::vector<Player*> carriers;
        for (ObjectGuid const& guid : marks)
            if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid); carrier && carrier->IsAlive())
                carriers.push_back(carrier);
        if (carriers.empty())
            return;
        std::vector<Player*> others;
        for (Player* bot : Bots(true))
        {
            if (std::find(carriers.begin(), carriers.end(), bot) != carriers.end())
                GroundIndicators::SetUnitSpot(me, bot, bot->GetPosition(), GuideSlack, ms);
            else
                others.push_back(bot);
        }
        // Everyone (players too) counts towards a mark's room: the bots fill what the players leave
        size_t const players = ArenaPlayers().size();
        size_t const room = (players - carriers.size() + carriers.size() - 1) / carriers.size();
        std::vector<size_t> under(carriers.size(), 0);
        for (Player* player : ArenaPlayers())
            if (!player->GetSession() || !player->GetSession()->IsBot())
                for (size_t index = 0; index < carriers.size(); ++index)
                    if (player != carriers[index] && player->GetExactDist2d(carriers[index]) <= FervorRadius)
                        ++under[index];
        std::sort(others.begin(), others.end(), [&carriers](Player* left, Player* right)
        {
            auto nearest = [&carriers](Player* player)
            {
                float best = 1000.0f;
                for (Player* carrier : carriers)
                    best = std::min(best, player->GetExactDist2d(carrier));
                return best;
            };
            return nearest(left) < nearest(right);
        });
        for (Player* bot : others)
        {
            size_t best = carriers.size();
            for (size_t index = 0; index < carriers.size(); ++index)
                if (under[index] < room && (best == carriers.size() ||
                        bot->GetExactDist2d(carriers[index]) < bot->GetExactDist2d(carriers[best])))
                    best = index;
            if (best == carriers.size())
                best = 0;
            ++under[best];
            GroundIndicators::SetUnitSpot(me, bot, carriers[best]->GetPosition(), FervorRadius - 2.5f, ms);
        }
        scheduler.Schedule(400ms, [this, marks, lands](TaskContext) { FollowStacks(marks, lands); });
    }

    // A spread's mark: its carrier hit, everyone else under it a mistake
    void StrikeSpread(Player* carrier)
    {
        if (_phase == Phase::Rehearsal)
            return;
        Hit(carrier, SPELL_FERVOR, FervorCarrierPct);
        for (Player* player : ArenaPlayers())
            if (player != carrier && player->GetExactDist2d(carrier) <= FervorRadius)
                Mistake(player, RuleFervor);
    }

    // A stack's mark: FervorStackPct shared by all under it
    void StrikeStack(Player* carrier)
    {
        std::vector<Player*> in;
        for (Player* player : ArenaPlayers())
            if (player->GetExactDist2d(carrier) <= FervorRadius)
                in.push_back(player);
        LOG_INFO(_logName, "Valerian stack instance={} at={:.1f}s on={} shared by {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, carrier->GetName(), in.size());
        if (_phase == Phase::Rehearsal || in.empty())
            return;
        float const share = FervorStackPct / float(in.size());
        for (Player* player : in)
            Hit(player, SPELL_FERVOR_SHARED, share);
    }

    // --- Rage contenue: the inside, then the outside - or the other way round ----------------------------------------
    // order 0: in then out; 1: out then in; 2: either, at random. His glow tells: gathered close round him (in first),
    // or flared out at the wall (out first)
    void Rage(uint32 start, uint32 order)
    {
        CastBar(CAST_RAGE);
        Pose(EMOTE_STATE_SPELL_PRECAST);
        AtBeat(Grid, start + WarnBeats, [this]() { Gesture(EMOTE_ONESHOT_STOMP); });
        AtBeat(Grid, start + WarnBeats + RageFollowBeats, [this]() { Gesture(EMOTE_ONESHOT_STOMP); });
        Sound("Valerian.RageCast", me->GetPosition());
        uint32 const first = start + WarnBeats;
        AtBeat(Grid, first, [this]() { EndCastBar(); });
        bool const inFirst = order == 0 || (order == 2 && urand(0, 1));
        float const outer = ArenaRadius + 1.0f;
        float const inner = outer * RageInnerShare;
        GroundIndicators::Area glow;
        glow.kind = inFirst ? GroundIndicators::Area::Kind::Circle : GroundIndicators::Area::Kind::Ring;
        glow.origin = Middle();
        glow.radius = inFirst ? 4.0f : ArenaRadius;
        glow.inner = inFirst ? 0.0f : ArenaRadius - 3.0f;
        GroundIndicators::ShowParticles(me, glow, GroundIndicators::Theme::Holy, MsTo(first));
        auto in = [this, inner](uint32 ms)
        {
            return Areas{ Painted(MakeCircle(Middle(), inner), LOOK_RAGE_CIRCLE, ms) };
        };
        auto out = [this, outer, inner](uint32 ms)
        {
            GroundIndicators::Area ring = MakeCircle(Middle(), outer);
            ring.kind = GroundIndicators::Area::Kind::Ring;
            ring.inner = inner;
            return Areas{ Painted(ring, LOOK_RAGE_RING_40, ms) };
        };
        // The bots: out in the ring while the inside strikes, close to him while the outside does
        float const spread = frand(0.0f, 2.0f * float(M_PI));
        Guide(Bots(true), Fan(spread, 315.0f, inFirst ? GuideRing : GuideNear, 8), MsTo(first) + 200);
        AtBeat(Grid, first, [this, spread, inFirst, first]()
        {
            Guide(Bots(true), Fan(spread, 315.0f, inFirst ? GuideNear : GuideRing, 8),
                MsTo(first + RageFollowBeats) + 200);
        });
        Wave(start, first, inFirst ? std::function<Areas(uint32)>(in) : std::function<Areas(uint32)>(out), RuleRage);
        Wave(first, first + RageFollowBeats, inFirst ? std::function<Areas(uint32)>(out) :
            std::function<Areas(uint32)>(in), RuleRage);
    }

    // --- Lances de lumière: a grid of lines wall to wall, then the grid shifted half a square ----------------------------
    void Lances(uint32 start)
    {
        CastBar(CAST_LANCES);
        Sound("Valerian.LancesCast", Middle());
        uint32 const first = start + WarnBeats;
        // Channelling, a spear of light in his hand while the grid shows; he throws it to the sky a beat before it lands
        // (a column of light rising from him), takes another for the shifted grid, and throws that one too
        Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        HoldSpear(true);
        AtBeat(Grid, first + LanceShiftBeats, [this]() { HoldSpear(false); });
        AtBeat(Grid, first - 1, [this]() { ThrowSpear(false); });
        AtBeat(Grid, first + LanceShiftBeats - 1, [this]() { ThrowSpear(true); });
        AtBeat(Grid, first, [this]() { EndCastBar(); });
        float const turn = frand(0.0f, float(M_PI) / 2.0f);
        // The bots: on points neither grid strikes (a quarter of the spacing off every line of both: the grids' lines
        // fall every half spacing between them), so the shift needs no step
        {
            std::vector<Position> safe;
            Position const middle = Middle();
            float const quarter = LanceSpacing / 4.0f;
            for (float u : { -3.0f * quarter, -quarter, quarter, 3.0f * quarter })
                for (float v : { -3.0f * quarter, -quarter, quarter, 3.0f * quarter })
                {
                    float const x = u * std::cos(turn) - v * std::sin(turn);
                    float const y = u * std::sin(turn) + v * std::cos(turn);
                    safe.push_back(Ground(Position(middle.GetPositionX() + x, middle.GetPositionY() + y,
                        middle.GetPositionZ())));
                }
            // (0.75 yd clear of the nearest lines: they stop close)
            Guide(Bots(true), safe, MsTo(first + LanceShiftBeats) + 200, 0.5f);
        }
        Wave(start, first, [this, turn](uint32 ms) { return Grille(turn, LanceSpacing / 2.0f, ms); }, RuleLances);
        Wave(first, first + LanceShiftBeats, [this, turn](uint32 ms) { return Grille(turn, 0.0f, ms); }, RuleLances);
    }

    // His spear in his hands (or not): Odyn's Spear of Light
    void HoldSpear(bool held)
    {
        me->SetVirtualItem(0, held ? ITEM_SPEAR_OF_LIGHT : 0);
    }

    void ThrowSpear(bool last)
    {
        Gesture(EMOTE_ONESHOT_ATTACK_THROWN, last);
        if (!last)
            Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        GroundIndicators::PlayKit(me, Middle(), KIT_HOLY_IMPACT, 1.5f, 0, ImpactMs);
    }

    // Lines LanceSpacing apart both ways (turned by turn), the first offset from the middle: each from wall to wall
    Areas Grille(float turn, float offset, uint32 ms)
    {
        Areas lines;
        Position const middle = Middle();
        // Wall to wall: no further (past the wall is death already, and drawn there they showed through it)
        float const reach = ArenaRadius - 0.3f;
        for (uint32 axis = 0; axis < 2; ++axis)
        {
            float const along = turn + float(axis) * float(M_PI) / 2.0f;
            float const across = along + float(M_PI) / 2.0f;
            for (float at = offset - 2.0f * LanceSpacing; at <= reach; at += LanceSpacing)
            {
                if (std::fabs(at) >= ArenaRadius - 1.0f)
                    continue;
                float const half = std::sqrt(reach * reach - at * at);
                Position const crossing(middle.GetPositionX() + std::cos(across) * at,
                    middle.GetPositionY() + std::sin(across) * at, middle.GetPositionZ());
                Position startAt = Ground(Position(crossing.GetPositionX() - std::cos(along) * half,
                    crossing.GetPositionY() - std::sin(along) * half, crossing.GetPositionZ()));
                startAt.SetOrientation(along);
                lines.push_back(Lance(startAt, along, 2.0f * half, ms));
            }
        }
        return lines;
    }

    // --- La Litanie: a numeral on every head, a spear at each verse in turn ------------------------------------------
    // twoAtOnce: verses I and V together, then II and VI... (the clock must be exact: I and V opposite)
    void Litany(uint32 start, bool twoAtOnce)
    {
        CastBar(CAST_LITANY);
        Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        HoldSpear(true);
        me->AddAura(SPELL_LITANY_WARD, me);
        Sound("Valerian.LitanyStart", Middle());
        // The numerals dealt at random, I to as many as there are players (eight at most)
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomShuffle(players);
        if (players.size() > LitanyVerses)
            players.resize(LitanyVerses);
        std::vector<ObjectGuid> verses;
        uint32 const firstStrike = start + LitanyFirstBeats;
        uint32 const throws = twoAtOnce ? std::min<uint32>(4, uint32(players.size())) : uint32(players.size());
        uint32 const lastStrike = firstStrike + (throws ? throws - 1 : 0) * LitanyEveryBeats;
        for (uint32 index = 0; index < players.size(); ++index)
        {
            verses.push_back(players[index]->GetGUID());
            GroundIndicators::ShowCarriedLook(players[index], LOOK_MARK_VERSE_FIRST + index, MsTo(lastStrike) + 500);
        }
        // The clock under him: its numerals where each verse stands
        GroundIndicators::ShowDecal(me, Middle(), LitanySigilFacing, LitanySigilRadius, MsTo(lastStrike) + 600,
            LOOK_LITANY_SIGIL);
        GroundIndicators::ShowDecal(me, Middle(), LitanySigilFacing, LitanySigilRadius, MsTo(lastStrike) + 600,
            LOOK_LITANY_SIGIL_GLOW);
        _litanyStruck.clear();
        _litanyProfaned = false;
        // The bots: to their numeral's place on the clock, LitanyBotsBeats before the first spear (never earlier: the
        // players are not to read the answer off them), each until its own verse is struck
        AtBeat(Grid, firstStrike - LitanyBotsBeats, [this, verses, firstStrike, twoAtOnce]()
        {
            for (uint32 index = 0; index < verses.size(); ++index)
            {
                Player* player = ObjectAccessor::GetPlayer(*me, verses[index]);
                if (!player || !player->GetSession() || !player->GetSession()->IsBot())
                    continue;
                uint32 const throwIndex = twoAtOnce ? index % 4 : index;
                GroundIndicators::SetUnitSpot(me, player, VerseSpot(index), 0.6f,
                    MsTo(firstStrike + throwIndex * LitanyEveryBeats) + 300);
            }
        });
        // The spears: each one's line shown LitanyWarnBeats before it strikes, aimed where its verse stands then
        for (uint32 throwIndex = 0; throwIndex < throws; ++throwIndex)
        {
            uint32 const strikes = firstStrike + throwIndex * LitanyEveryBeats;
            std::vector<uint32> group;
            if (twoAtOnce)
            {
                group.push_back(throwIndex);
                if (throwIndex + 4 < verses.size())
                    group.push_back(throwIndex + 4);
            }
            else
                group.push_back(throwIndex);
            AtBeat(Grid, strikes - LitanyWarnBeats, [this, verses, group, strikes]()
            {
                Sound("Valerian.VerseTell", Middle());
                std::vector<std::pair<uint32, GroundIndicators::Area>> lines;
                std::vector<float> aims;
                for (uint32 verse : group)
                    if (Player* holder = ObjectAccessor::GetPlayer(*me, verses[verse]); holder && holder->IsAlive())
                    {
                        aims.push_back(Middle().GetAngle(holder));
                        lines.emplace_back(verse, Lance(Middle(), aims.back(), ArenaRadius - 0.3f, MsTo(strikes),
                            false));
                    }
                // The spears leave his hand so they reach the numerals on the beat
                uint32 const flightMs = uint32(LitanyRing / LitanySpearSpeed * 1000.0f);
                scheduler.Schedule(Milliseconds(MsTo(strikes) - std::min(MsTo(strikes), flightMs)),
                    [this, aims](TaskContext) { ThrowVerseSpears(aims); });
                AtBeat(Grid, strikes, [this, verses, lines]() { StrikeVerses(verses, lines); });
            });
        }
        AtBeat(Grid, lastStrike, [this]()
        {
            EndCastBar();
            Pose(0);
            HoldSpear(false);
            me->RemoveAurasDueToSpell(SPELL_LITANY_WARD);
            GuardDown();
        });
        AtBeat(Grid, lastStrike + 1, [this]()
        {
            if (!_litanyProfaned)
                Sound("Valerian.LitanyEnd", Middle());
        });
    }

    // His ward gone: everyone alive strikes harder for a while (Garde brisée)
    void GuardDown()
    {
        if (_litanyProfaned)
            return;
        for (Player* player : ArenaPlayers())
            if (player->IsAlive())
                AddTimedAura(player, SPELL_OPENING, OpeningMs);
    }

    // The spears in flight: from his hand to the wall along each aim, the throw's gesture with them
    void ThrowVerseSpears(std::vector<float> const& aims)
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return;
        Gesture(EMOTE_ONESHOT_ATTACK_THROWN, false);
        for (float aim : aims)
        {
            Position const wall = Ground(OnArena(aim, ArenaRadius - 0.3f));
            float const z = wall.GetPositionZ() + LitanySpearHeight;
            me->CastSpell(wall.GetPositionX(), wall.GetPositionY(), z, SPELL_VERSE_SPEAR, true);
            me->CastSpell(wall.GetPositionX(), wall.GetPositionY(), z, SPELL_VERSE_LIGHTNING, true);
        }
    }

    // Where verse index stands on the clock: I at its top (north), then clockwise
    Position VerseSpot(uint32 index) const
    {
        return OnArena(float(M_PI) / 2.0f - float(index) * 2.0f * float(M_PI) / float(LitanyVerses), LitanyRing);
    }

    // Spears land: each on its verse; anyone else on a line whose verse is still to come profanes the litany
    void StrikeVerses(std::vector<ObjectGuid> const& verses, std::vector<std::pair<uint32, GroundIndicators::Area>> const&
                      lines)
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return;
        std::vector<Position> spots;
        for (auto const& [verse, line] : lines)
        {
            _litanyStruck.insert(verses[verse]);
            spots.push_back(BurstSpot(line));
        }
        SoundNearest("Valerian.VerseStrike", spots);
        if (_phase == Phase::Rehearsal || _litanyProfaned)
            return;
        Player* profaner = nullptr;
        uint32 profanedBy = 0;
        for (auto const& [verse, line] : lines)
            for (Player* player : ArenaPlayers())
            {
                if (player->GetGUID() == verses[verse])
                {
                    Hit(player, SPELL_VERSE, LitanyVersePct);
                    continue;
                }
                bool const waiting = std::find(verses.begin(), verses.end(), player->GetGUID()) != verses.end() &&
                    !_litanyStruck.contains(player->GetGUID());
                if (waiting && line.Contains(player->GetPosition()))
                {
                    profaner = player;
                    profanedBy = verse;
                }
            }
        if (!profaner)
            return;
        // Profanation: the light bursts, everyone dies
        _litanyProfaned = true;
        LOG_INFO(_logName, "Valerian profanation instance={} at={:.1f}s verse {} struck {} bot={}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, profanedBy + 1, profaner->GetName(),
                 profaner->GetSession() && profaner->GetSession()->IsBot());
        Sound("Valerian.Profanation", Middle());
        Blast(Middle(), ArenaRadius);
        for (Player* player : ArenaPlayers())
            Doom(player, SPELL_PROFANATION);
    }

    // --- Éclat de zèle: a group hit on the music's hit ------------------------------------------------------------------
    void Burst(uint32 start)
    {
        uint32 const lands = start + BurstCastBeats;
        CastBar(CAST_BURST);
        Pose(EMOTE_STATE_SPELL_PRECAST);
        GroundIndicators::WarnGroupDamage(me, MsTo(lands));
        AtBeat(Grid, lands, [this]()
        {
            EndCastBar();
            Sound("Valerian.Burst", Middle());
            Gesture(EMOTE_ONESHOT_BATTLE_ROAR);
            Blast(Middle(), 8.0f);
            if (_phase == Phase::Zealot)
                HitEveryone(SPELL_BURST, BurstGroupPct);
        });
    }

    // --- The end of the phase: the check, then the lights' verdict -------------------------------------------------------
    void Judgement(uint32 start)
    {
        uint32 const strikes = std::max(start + 2, JudgementBeat);
        CastBar(CAST_JUDGEMENT);
        Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        Sound("Valerian.JudgementCast", Middle());
        GroundIndicators::WarnGroupDamage(me, MsTo(strikes));
        AtBeat(Grid, strikes, [this]()
        {
            EndCastBar();
            if (_phase == Phase::Rehearsal)
            {
                LOG_INFO(_logName, "Valerian rehearsal judgement: lights shown {} absorbed {} taken {}", _spawned,
                         _absorbed, _taken);
                return;
            }
            bool const passed = AtPaceFloor();
            bool const whole = _taken == 0 && _absorbed >= LightCount;
            LOG_INFO(_logName, "Valerian judgement instance={} at={:.1f}s true={:.1f}% alive={} passed={} lights "
                     "spawned={} absorbed={} taken={}", me->GetInstanceId(), Elapsed() / 1000.0f, TrueHealthPct(),
                     ArenaPlayers().size(), passed, _spawned, _absorbed, _taken);
            if (!passed)
            {
                GroundIndicators::ShowCircle(me, Middle(), ArenaRadius + 1.0f, 2000);
                for (Player* player : ArenaPlayers())
                    Doom(player, SPELL_BLINDING);
                return;
            }
            if (whole)
                Prelude();
            else
                Chained();
        });
    }

    // Some light was stolen from him: he falls to his knees, the jailers come, he is beaten - no gear, 1 gold on the floor
    // (GroundLoot::DenyLoot), then home as from any kill
    void Chained()
    {
        _phase = Phase::Chained;
        EndPace();
        ClearLights();
        GroundIndicators::ClearAreasOf(me);
        // His wall comes down (it no longer kills: the jailers walking in are seen)
        GroundIndicators::FadeLooksOf(me, LOOK_WALL_BARS_FIRST, LOOK_WALL_MOTES);
        EndMusic();
        me->AddAura(SPELL_OUT_OF_REACH, me);
        Talk(SAY_STOLEN);
        Sound("Valerian.Chained", Middle());
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        // Still in combat (never SetImmuneToAll: an immune boss left combat, and the board counted down a new pull):
        // an aura makes him immune instead (Hors d'atteinte, from his defeat to phase 2), DamageTaken takes nothing
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        me->SetPower(POWER_MANA, 0);
        scheduler.Schedule(Milliseconds(JailersInMs), [this](TaskContext)
        {
            Position const middle = Middle();
            float const first = frand(0.0f, 2.0f * float(M_PI));
            for (uint32 index = 0; index < Jailers; ++index)
            {
                float const angle = first + float(index) * 2.0f * float(M_PI) / float(Jailers);
                Position at = OnArena(angle, ArenaRadius - 1.0f);
                at.SetOrientation(Position::NormalizeOrientation(angle + float(M_PI)));
                if (Creature* jailer = me->SummonCreature(NPC_JAILER, at, TEMPSUMMON_MANUAL_DESPAWN))
                {
                    _jailers.push_back(jailer->GetGUID());
                    jailer->SetWalk(true);
                    jailer->GetMotionMaster()->MovePoint(1, AtAngle(middle, angle, 2.5f));
                }
            }
        });
        scheduler.Schedule(Milliseconds(JailerLineMs), [this](TaskContext)
        {
            if (!_jailers.empty())
                if (Creature* jailer = me->GetMap()->GetCreature(_jailers.front()))
                    if (jailer->AI())
                        jailer->AI()->Talk(SAY_JAILER);
        });
        scheduler.Schedule(Milliseconds(DeniedMs), [this](TaskContext)
        {
            LOG_INFO(_logName, "Valerian chained instance={}: beaten, no gear", me->GetInstanceId());
            GroundLoot::DenyLoot(me->GetMap());
            // ... and no clear on the board: lost, once the group is home
            DenyChallengeClear(me->GetMap());
            me->SetImmuneToAll(false);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            std::vector<Player*> const players = ArenaPlayers();
            Unit* credit = players.empty() ? nullptr : players.front();
            Unit::Kill(credit ? credit : me, me);
        });
    }

    // Every light in him: he rises off the floor, the light pours out of him (everyone thrown back), then phase 2 - bare
    // for now: his own health, his music, his melee, no mechanic
    // Every light in him, the phase beaten: his words, the light streaming into him from all round the wall, then his
    // apotheosis on p2's opening (Ascend)
    void Prelude()
    {
        _phase = Phase::Ascending;
        ClearLights();
        GroundIndicators::ClearAreasOf(me);
        KeepBotsIn(WallLastsMs);    // (the wall still stands: the bots kept inside it)
        EndPace();
        // Phase 1's track ends here, fading (it is in its own fade by now): every track loops, and left to play to
        // its end it started over for a few seconds before MusicEndMs stopped it
        _musicOver = true;
        EndMusic();
        me->AttackStop();
        me->AddAura(SPELL_OUT_OF_REACH, me);
        me->SetReactState(REACT_PASSIVE);
        // Still in combat (never SetImmuneToAll: an immune boss left combat, and the board counted down a new pull):
        // an aura makes him immune instead (Hors d'atteinte, from his defeat to phase 2), DamageTaken takes nothing
        Talk(SAY_PRELUDE_FIRST);
        Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        uint32 const lights = PreludeMs / PreludeLightMs;
        for (uint32 index = 0; index < lights; ++index)
            scheduler.Schedule(Milliseconds(index * PreludeLightMs), [this, index, lights](TaskContext)
            {
                StreamLight();
                SetSize(BaseScale * (1.25f + 0.1f * float(index) / float(lights)));
                if (index % 10 == 0)
                    me->SendPlaySpellVisual(KIT_HOLY_GLOW);
            });
        scheduler.Schedule(Milliseconds(PreludeSecondLineMs), [this](TaskContext) { Talk(SAY_PRELUDE_SECOND); });
        scheduler.Schedule(Milliseconds(PreludeMs), [this](TaskContext) { Ascend(); });
    }

    // A light flying from a random point of the wall into him
    void StreamLight()
    {
        float const angle = frand(0.0f, 2.0f * float(M_PI));
        Position from = OnArena(angle, ArenaRadius - 1.0f);
        from.m_positionZ += LightHeight + frand(0.0f, 3.0f);
        uint32 const flies = uint32((ArenaRadius - 1.0f) / PreludeLightSpeed * 1000.0f);
        if (Creature* orb = me->SummonCreature(NPC_ORB, from, TEMPSUMMON_TIMED_DESPAWN, flies + 100))
        {
            orb->SetCanFly(true);
            orb->SetDisableGravity(true);
            orb->SetReactState(REACT_PASSIVE);
            Position to = Middle();
            to.m_positionZ += LightHeight * 2.0f;
            orb->GetMotionMaster()->MovePoint(1, to, FORCED_MOVEMENT_NONE, PreludeLightSpeed, false);
        }
    }

    void Ascend()
    {
        _phase = Phase::Ascending;
        ClearLights();
        GroundIndicators::ClearAreasOf(me);
        KeepBotsIn(WallLastsMs);    // (the wall still stands: the bots kept inside it)
        // p2's calm opening is his apotheosis: p1's own end no longer silences anything (it cut p2 short)
        _musicOver = true;
        _ascendFrom = Elapsed();
        StartMusic("Music.ValerianAscended");
        Talk(SAY_ASCEND);
        Sound("Valerian.Ascend", Middle());
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        // Still in combat (never SetImmuneToAll: an immune boss left combat, and the board counted down a new pull):
        // an aura makes him immune instead (Hors d'atteinte, from his defeat to phase 2), DamageTaken takes nothing
        Position const middle = Middle();
        GroundIndicators::ShowDecal(me, middle, 0.0f, SigilRadius, AscendHitMs, LOOK_SIGIL);
        GroundIndicators::ShowDecal(me, middle, 0.0f, SigilRadius, AscendHitMs, LOOK_SIGIL_RUNES);
        // The light gathering on him, brighter and brighter, until his blow
        // (stopping AscendGlowClearMs before he lands: the last ones lingered over the god, washing him yellow)
        for (uint32 at = 0; at + AscendGlowClearMs < AscendLandMs; at += AscendGlowEveryMs)
            scheduler.Schedule(Milliseconds(at), [this](TaskContext) { me->SendPlaySpellVisual(KIT_HOLY_GLOW); });
        // Up off the floor, slowly, levitating (arms raised, channelling), and down as the music breaks: his height set
        // every update (UpdateAscent), never moved - any movement plays a movement animation on his model (flying, a
        // human's fell back to swimming; hovering, a hunched lean)
        _ascentFrom = Elapsed();
        _dropFrom = -1.0f;
        scheduler.Schedule(Milliseconds(AscendRiseMs), [this](TaskContext)
        {
            me->SetControlled(false, UNIT_STATE_ROOT);
            me->SetDisableGravity(true);
            Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        });
        // Shards of light to step out of meanwhile
        for (uint32 at = AscendRiseMs + 1000; at + ShardsWarnMs < AscendLandMs; at += ShardsEveryMs)
            scheduler.Schedule(Milliseconds(at), [this](TaskContext) { Shards(); });
        // His blow on its hit: everyone left at AscendLeftPct of their health, then phase 2
        scheduler.Schedule(Milliseconds(AscendHitMs), [this](TaskContext)
        {
            Position const at = Middle();
            Sound("Valerian.FirstBlow", at);
            GroundIndicators::Burst(me, at, GroundIndicators::Theme::Holy);
            PushWall();
            for (Player* player : _ascendRehearsed ? std::vector<Player*>() : ArenaPlayers())
            {
                uint32 const left = uint32(float(player->GetMaxHealth()) * AscendLeftPct / 100.0f);
                if (player->GetHealth() > left)
                    HitOwnHealth(player, SPELL_ASCEND,
                        float(player->GetHealth() - left) * 100.0f / float(player->GetMaxHealth()));
            }
            // The god, as his blow lands: upright, his wings of light half spread (the form's spell-ready stance; its
            // stand folds them on his back)
            me->SetDisplayId(DISPLAY_ASCENDED);
            SetSize(AscendedSize);
            Pose(EMOTE_STATE_READY_SPELL_OMNI);
            if (_ascendRehearsed)
            {
                // The lab: back to rehearsing, the god's form kept to look at (.valerian look 0 2.6: the man again)
                _ascendRehearsed = false;
                _phase = Phase::Rehearsal;
                me->RemoveAurasDueToSpell(SPELL_OUT_OF_REACH);
                me->SetCanFly(false);
                me->SetDisableGravity(false);
                EndMusic();
                return;
            }
            StartAscended();
        });
    }

    // His pose while a pattern builds (an emote state: 0, none) - every mechanic is seen thrown by him
    void Pose(uint32 state)
    {
        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, state);
    }

    // A gesture as a pattern strikes, his pose let go after it
    void Gesture(uint32 emote, bool release = true)
    {
        if (release)
            Pose(0);
        me->HandleEmoteCommand(emote);
    }

    // His apotheosis now: his height still, then rising eased over AscendRiseTakesMs, hovering (a slow sway of
    // AscendSway yards), then dropping (eased in) to the floor by his blow; his size growing all along to AscendScale.
    // Set every update: an unchanged height is never sent again, and the clients let him fall
    void UpdateAscent()
    {
        if (!_ascentFrom)
            return;
        uint32 const at = Elapsed() - _ascentFrom;
        // (from his blow on he is the god, its own size: his blow's step has set it)
        if (at < AscendHitMs)
            SetSize(BaseScale * (AscendFromScale + (AscendScale - AscendFromScale) *
                std::min(1.0f, float(at) / float(AscendLandMs))));
        float height = 0.0f;
        if (at >= AscendHitMs)
            _ascentFrom = 0;
        else if (at >= AscendLandMs)
        {
            if (_dropFrom < 0.0f)
                _dropFrom = _height;
            float const t = float(at - AscendLandMs) / float(AscendDropMs);
            height = _dropFrom * (1.0f - t * t);
        }
        else if (at > AscendRiseMs)
        {
            float const t = std::min(1.0f, float(at - AscendRiseMs) / float(AscendRiseTakesMs));
            float const eased = t * t * (3.0f - 2.0f * t);
            float const sway = std::sin(2.0f * float(M_PI) * float(at - AscendRiseMs) / float(AscendSwayMs));
            height = AscendHeight * eased + AscendSway * eased * sway;
        }
        if (std::fabs(height - _height) < 0.005f && _ascentFrom)
            return;
        _height = height;
        SetHeight(height);
    }

    // Shards of light on a few players and spots while he rises (ShardsWarnMs warned)
    void Shards()
    {
        Areas areas;
        for (Player* player : Targets(ShardsPlayers))
            areas.push_back(Painted(MakeCircle(Ground(player->GetPosition()), ShardRadius), LOOK_RAGE_CIRCLE,
                ShardsWarnMs));
        for (uint32 index = 0; index < ShardsSpots; ++index)
            areas.push_back(Painted(MakeCircle(OnArena(frand(0.0f, 2.0f * float(M_PI)), frand(3.0f,
                ArenaRadius - ShardRadius - 1.0f)), ShardRadius), LOOK_RAGE_CIRCLE, ShardsWarnMs));
        scheduler.Schedule(Milliseconds(ShardsWarnMs), [this, areas](TaskContext) { Strike(areas, RuleShards); });
    }

    // His height over the middle's floor, set at once (no movement: see Ascend)
    void SetHeight(float height)
    {
        Position at = Middle();
        at.m_positionZ += height;
        me->NearTeleportTo(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), me->GetOrientation());
    }

    // Phase 2, bare: on the floor at the middle, his health anew (its music already on since his apotheosis)
    void StartAscended()
    {
        EndPace();
        _phase = Phase::Ascended;
        me->RemoveAurasDueToSpell(SPELL_OUT_OF_REACH);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        Position const middle = Middle();
        if (me->GetExactDist(&middle) > 1.0f)
            me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(), me->GetOrientation());
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->SetImmuneToAll(false);
        SetHalfBar(false);
        _p2Pool = uint32(me->GetHealth());
        _p2True = _p2Pool;
        _p2Timeline = BuildP2Timeline();
        _p2Next = 0;
        _starsBase = frand(0.0f, 2.0f * float(M_PI));
        _pizzaBase = frand(0.0f, 2.0f * float(M_PI));
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, GetRoom().reach);
        LOG_INFO(_logName, "Valerian ascended instance={} health={}", me->GetInstanceId(), me->GetMaxHealth());
    }

    // --- Phase 2 ------------------------------------------------------------------------------------------------------
    // The track's time (from his apotheosis): phase 2's clock
    uint32 TrackMs() const { return _ascendFrom && Elapsed() > _ascendFrom ? Elapsed() - _ascendFrom : 0; }

    bool P2Active() const { return _phase == Phase::Ascended || _phase == Phase::Rehearsal || Rewinding(); }

    bool InCalm() const
    {
        uint32 const now = TrackMs();
        for (Calm const& calm : P2Calms)
            if (now >= calm.fromMs && now < calm.toMs)
                return true;
        return now >= TotalLightCastMs;
    }

    // fn delayMs from now, as long as phase 2 (or the lab) goes on
    void After(uint32 delayMs, std::function<void()> fn)
    {
        scheduler.Schedule(Milliseconds(delayMs), [this, fn](TaskContext)
        {
            if (P2Active())
                fn();
        });
    }

    Position P2At(float angle, float distance) const { return Ground(OnArena(angle, distance)); }

    bool IsBotPlayer(Player* player) const { return player->GetSession() && player->GetSession()->IsBot(); }

    void ExecuteP2(P2Step const& step, uint32 /*unused*/)
    {
        LOG_INFO(_logName, "Valerian p2 {} instance={} track={:.2f}s shown={:.1f}% true={:.1f}%", uint32(step.what),
                 me->GetInstanceId(), TrackMs() / 1000.0f, me->GetHealthPct(),
                 _p2Pool ? 50.0f * float(_p2True) / float(_p2Pool) : 0.0f);
        switch (step.what)
        {
            case P2Event::Ailes: Ailes(step.arg); break;
            case P2Event::Stars: Stars(); break;
            case P2Event::Gaze: Gaze(std::max<uint32>(1, step.arg)); break;
            case P2Event::Sentence: Sentence(step.arg); break;
            case P2Event::Choir: Choir(step.arg == 1); break;
            case P2Event::Echo: Echo(); break;
            case P2Event::Ascension: Ascension(); break;
            case P2Event::Eclipse: Eclipse(step.arg); break;
            case P2Event::TotalLight: TotalLight(0); break;
        }
    }

    // --- His health in phase 2: paced to nothing at P2EarliestDeathMs, never before ----------------------------------
    uint32 P2Shown() const
    {
        uint32 const now = TrackMs();
        float const share = now <= AscendHitMs ? 1.0f : now >= P2EarliestDeathMs ? 0.0f :
            1.0f - float(now - AscendHitMs) / float(P2EarliestDeathMs - AscendHitMs);
        return std::max(_p2True, uint32(float(_p2Pool) * share));
    }

    void P2Damage(uint32& damage)
    {
        _p2True = _p2True > damage ? _p2True - damage : 0;
        uint32 const shown = P2Shown();
        uint32 const health = uint32(me->GetHealth());
        // At nothing (the pace past its end, the group's damage done): he keeps a point of it - he refuses to die
        // (UpdateP2: Le Refus)
        uint32 const floor = std::max<uint32>(1, shown);
        damage = health > floor ? health - floor : 0;
    }

    void UpdateP2(uint32 diff)
    {
        UpdateOrbs(diff);
        uint32 const shown = P2Shown();
        if (shown == 0)
        {
            // Past his earliest death with nothing left: he refuses to die (phase 3)
            StartRefus();
            return;
        }
        // Both ways: down with the pace, up when an orb reaches him
        if (me->GetHealth() != shown)
            me->SetHealth(shown);
    }

    // --- Ailes séraphiques: six tendrils from him to the wall, swept round ---------------------------------------------
    // Each tendril one drawing from its tell to the end of its sweep: still, then turning (and back, reversed)
    void Ailes(uint32 version)
    {
        bool const reversed = version % 2 == 1;
        bool const split = version >= 2;
        Position const middle = Middle();
        float const base = frand(0.0f, 2.0f * float(M_PI));
        float const sweepDeg = split ? AilesSplitSweepDeg : AilesTurnDeg;
        float const turn = sweepDeg * float(M_PI) / 180.0f;
        uint32 const sweepMs = uint32(float(AilesSweepMs) * sweepDeg / AilesTurnDeg);
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        std::vector<GroundIndicators::Turn> turns;
        uint32 total = P2TellMs + sweepMs;
        if (!reversed)
            turns.push_back({ P2TellMs, sweepMs, way * turn });
        else
        {
            // AilesTurnDeg / 2 one way, then the sweep back
            uint32 const half = AilesSweepMs / 2;
            turns.push_back({ P2TellMs, half, way * AilesTurnDeg * float(M_PI) / 360.0f });
            turns.push_back({ P2TellMs + half, sweepMs, -way * turn });
            total = P2TellMs + half + sweepMs;
            // Its tell: a flash on the side it turns back to, a beat before
            After(P2TellMs + half - 600, [this, base, way]()
            {
                GroundIndicators::PlayKit(me, P2At(base - way * float(M_PI) / 2.0f, 3.0f), KIT_HOLY_IMPACT, 1.0f, 0,
                    ImpactMs);
            });
        }
        for (uint32 index = 0; index < AilesTendrils; ++index)
            GroundIndicators::ShowTurningRectangle(me, middle,
                base + float(index) * 2.0f * float(M_PI) / float(AilesTendrils), P2Radius, AilesWidth, total, turns,
                MistakeDamage, TendrilLook(), TendrilCurtain());
        // The bots: each in a gap's middle, moved along with the gaps as they turn (a little ahead of them)
        float const gap = float(M_PI) / float(AilesTendrils);
        auto gapsAt = [this, base, gap, turns](uint32 t)
        {
            std::vector<Position> spots;
            float const turned = GroundIndicators::TurnedBy(turns, t);
            for (uint32 index = 0; index < AilesTendrils; ++index)
                spots.push_back(P2At(base + turned + gap + float(index) * 2.0f * gap, AilesGapAt));
            return spots;
        };
        GuideP2(gapsAt(P2TellMs), P2TellMs + 300);
        for (uint32 t = P2TellMs; t < total; t += AilesGuideStepMs)
            After(t, [this, gapsAt, t]() { GuideP2(gapsAt(t + AilesGuideLeadMs), AilesGuideStepMs + 300); });
        // Struck from the tell's end, where the tendrils are each moment
        auto hit = std::make_shared<std::set<ObjectGuid>>();
        for (uint32 t = P2TellMs; t <= total; t += 200)
            After(t, [this, base, turns, t, hit]() { StrikeTendrils(base + GroundIndicators::TurnedBy(turns, t), hit); });
        // Where they stop: a flash along each (its strike seen)
        float const end = base + GroundIndicators::TurnedBy(turns, total);
        After(total, [this, end]()
        {
            // (a flare is a floor drawing: none over Lumière totale's sigil)
            for (uint32 index = 0; index < AilesTendrils && !TendrilCurtain(); ++index)
                ShapedLine(Middle(), end + float(index) * 2.0f * float(M_PI) / float(AilesTendrils), P2Radius,
                    AilesWidth);
            SoundNearest("Valerian.Lances", { Middle() });
        });
        if (split)
            After(total, [this, end]() { SplitTendrils(end); });
    }

    // The split: each tendril, burst where it stopped (end: the first's facing), splits; its halves slide into the gaps
    // either side and explode there - the line it burst on is the way out
    void SplitTendrils(float end)
    {
        float const slide = AilesSplitDeg * float(M_PI) / 180.0f;
        uint32 const lands = AilesSplitAfterMs + AilesSplitSlideMs;
        uint32 const blows = lands + AilesSplitHoldMs;
        Areas halves;
        for (uint32 index = 0; index < AilesTendrils; ++index)
        {
            float const from = end + float(index) * 2.0f * float(M_PI) / float(AilesTendrils);
            for (float side : { -1.0f, 1.0f })
            {
                GroundIndicators::Area area = GroundIndicators::ShowTurningRectangle(me, Middle(), from, P2Radius,
                    AilesSplitWidth, blows, { { AilesSplitAfterMs, AilesSplitSlideMs, side * slide } }, MistakeDamage,
                    TendrilLook(), TendrilCurtain());
                area.origin.SetOrientation(Position::NormalizeOrientation(from + side * slide));
                halves.push_back(area);
            }
        }
        // The bots: onto the lines the tendrils burst on
        std::vector<Position> lines;
        for (uint32 index = 0; index < AilesTendrils; ++index)
            lines.push_back(P2At(end + float(index) * 2.0f * float(M_PI) / float(AilesTendrils), AilesGapAt));
        GuideP2(lines, blows + 300);
        After(blows, [this, halves]()
        {
            Strike(halves, RuleAiles);
            for (GroundIndicators::Area const& half : halves)
                if (!TendrilCurtain())
                    ShapedLine(Middle(), half.origin.GetOrientation(), P2Radius, AilesSplitWidth);
        });
    }

    // Under Lumière totale's sigil (the whole arena's floor) the tendrils stand up in shadow, nothing on the floor
    bool UnderTotalLight() const { return TrackMs() >= TotalLightCastMs || Elapsed() < _totalLightRehearsedUntil; }
    uint32 TendrilCurtain() const { return UnderTotalLight() ? LOOK_TENDRIL_CURTAIN : 0; }
    uint32 TendrilLook() const { return UnderTotalLight() ? GroundIndicators::CurtainOnly : LOOK_TENDRIL; }

    // Everyone on a tendril now (angle: the first's) struck, once a sweep
    void StrikeTendrils(float angle, std::shared_ptr<std::set<ObjectGuid>> hit)
    {
        Position const middle = Middle();
        for (Player* player : ArenaPlayers())
        {
            if (hit->count(player->GetGUID()))
                continue;
            float const distance = player->GetExactDist2d(&middle);
            if (distance > P2Radius + 1.0f)
                continue;
            float const at = middle.GetAbsoluteAngle(player);
            for (uint32 index = 0; index < AilesTendrils; ++index)
            {
                float const off = Position::NormalizeOrientation(at - angle -
                    float(index) * 2.0f * float(M_PI) / float(AilesTendrils));
                float const delta = off > float(M_PI) ? off - 2.0f * float(M_PI) : off;
                if (std::cos(delta) > 0.0f && std::fabs(std::sin(delta)) * distance <= AilesWidth / 2.0f)
                {
                    hit->insert(player->GetGUID());
                    if (_phase != Phase::Rehearsal)
                        Mistake(player, RuleAiles);
                    break;
                }
            }
        }
    }

    // Every bot of the room to the spots (in turn, one each), for ms
    void GuideP2(std::vector<Position> const& spots, uint32 ms)
    {
        if (spots.empty())
            return;
        uint32 index = 0;
        for (Player* player : SortedBots())
            GroundIndicators::SetUnitSpot(me, player, spots[index++ % spots.size()], 1.0f, ms);
    }

    // The room's bots, always in the same order (by guid): a spot handed out again goes to the same bot (the map's own
    // order changes, and bots were sent across the arena to another's spot as a sweep turned)
    std::vector<Player*> SortedBots() const
    {
        std::vector<Player*> bots;
        for (Player* player : ArenaPlayers())
            if (player->GetSession() && player->GetSession()->IsBot())
                bots.push_back(player);
        std::sort(bots.begin(), bots.end(), [](Player* a, Player* b) { return a->GetGUID() < b->GetGUID(); });
        return bots;
    }

    // --- Pluie d'astres: three stars to three beacons on the wall --------------------------------------------------------
    void Stars()
    {
        _starsBase += frand(0.6f, 1.4f);
        float const base = _starsBase;
        // The carriers: the real players first, a tank as any other (a tank friend never carried one); then bots, their
        // tanks last
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomShuffle(players);
        std::stable_sort(players.begin(), players.end(), [this](Player* a, Player* b)
        {
            auto rank = [this](Player* p) { return IsBotPlayer(p) ? (IsTank(p) ? 2 : 1) : 0; };
            return rank(a) < rank(b);
        });
        if (players.size() > StarsCount)
            players.resize(StarsCount);
        std::vector<ObjectGuid> carriers;
        for (uint32 index = 0; index < players.size(); ++index)
        {
            Player* carrier = players[index];
            carriers.push_back(carrier->GetGUID());
            // (no keep-away: a carrier keeping away chose its own place and held it, not the beacon)
            GroundIndicators::WatchCarriedCircle(me, carrier, StarsCarried, StarsDropMs, MistakeDamage);
            GroundIndicators::ShowCarriedLook(carrier, LOOK_STAR_CARRIED, StarsDropMs);
            GroundIndicators::ShowCarriedLook(carrier, LOOK_MARK_STAR, StarsDropMs);
            if (IsBotPlayer(carrier))
                GroundIndicators::SetUnitSpot(me, carrier, P2At(base + float(index) * 2.0f * float(M_PI) / 3.0f,
                    StarsBeaconAt), 1.0f, StarsDropMs);
        }
        // The beacons: painted where a star counts (StarsOnBeacon), until the drop
        for (uint32 index = 0; index < StarsCount; ++index)
        {
            Position const beacon = P2At(base + float(index) * 2.0f * float(M_PI) / 3.0f, StarsBeaconAt);
            GroundIndicators::ShowDecal(me, beacon, 0.0f, StarsOnBeacon, StarsDropMs, LOOK_BEACON);
            GroundIndicators::ShowDecal(me, beacon, 0.0f, StarsOnBeacon, StarsDropMs, LOOK_BEACON_RAYS);
        }
        // The others under him meanwhile
        std::vector<Position> underHim;
        for (uint32 index = 0; index < 8; ++index)
            underHim.push_back(P2At(float(index) * float(M_PI) / 4.0f, StarsSafe - 3.5f));
        for (Player* player : ArenaPlayers())
            if (IsBotPlayer(player) && std::find(carriers.begin(), carriers.end(), player->GetGUID()) == carriers.end())
                GroundIndicators::SetUnitSpot(me, player, underHim[urand(0, 7)], 1.0f, StarsDropMs);
        After(StarsDropMs, [this, carriers, base, underHim]()
        {
            Areas areas;
            std::vector<bool> used(StarsCount, false);
            for (ObjectGuid const& guid : carriers)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier)
                    continue;
                Position const star = Ground(carrier->GetPosition());
                int32 beacon = -1;
                for (uint32 index = 0; index < StarsCount; ++index)
                {
                    Position const at = P2At(base + float(index) * 2.0f * float(M_PI) / 3.0f, StarsBeaconAt);
                    if (!used[index] && star.GetExactDist2d(&at) <= StarsOnBeacon)
                    {
                        beacon = int32(index);
                        used[index] = true;
                        break;
                    }
                }
                float nearest = 1000.0f;
                for (uint32 index = 0; index < StarsCount; ++index)
                {
                    Position const at = P2At(base + float(index) * 2.0f * float(M_PI) / 3.0f, StarsBeaconAt);
                    nearest = std::min(nearest, star.GetExactDist2d(&at));
                }
                Position const middle = Middle();
                LOG_INFO(_logName, "Valerian star instance={} {} bot={} at={:.1f}yd/{:.0f}deg beacon={} nearest={:.1f}yd",
                         me->GetInstanceId(), carrier->GetName(), IsBotPlayer(carrier), star.GetExactDist2d(&middle),
                         middle.GetAbsoluteAngle(&star) * 180.0f / float(M_PI), beacon, nearest);
                if (beacon >= 0)
                {
                    // Its third from StarsSafe out (painted hollowed to StarsSafe / (P2Radius + 1): 0.4)
                    GroundIndicators::Area third = MakeCircle(Middle(), P2Radius + 1.0f);
                    third.kind = GroundIndicators::Area::Kind::Cone;
                    third.origin.SetOrientation(base + float(beacon) * 2.0f * float(M_PI) / 3.0f);
                    third.arc = 2.0f * float(M_PI) / 3.0f;
                    third.inner = StarsSafe;
                    areas.push_back(Painted(third, LOOK_STARFALL_THIRD, StarsBurstMs - StarsDropMs));
                }
                else
                    areas.push_back(Painted(MakeCircle(star, StarsMissRadius), LOOK_STARFALL_CIRCLE,
                        StarsBurstMs - StarsDropMs));
            }
            // Everyone under him
            for (Player* player : ArenaPlayers())
                if (IsBotPlayer(player))
                    GroundIndicators::SetUnitSpot(me, player, underHim[urand(0, 7)], 1.0f, StarsBurstMs - StarsDropMs + 300);
            After(StarsBurstMs - StarsDropMs, [this, areas]() { Strike(areas, RuleStars); });
        });
    }

    // --- Regard divin: a wide cone that follows its target, freezes, strikes --------------------------------------------
    void Gaze(uint32 eyes)
    {
        std::vector<Player*> targets = Targets(eyes);
        if (targets.empty())
            return;
        // The bots: the targets drag their cones to one side, the others gather on the other
        float const side = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 index = 0; index < targets.size(); ++index)
            if (IsBotPlayer(targets[index]))
                GroundIndicators::SetUnitSpot(me, targets[index], P2At(side + (targets.size() > 1 ?
                    (index ? 0.45f : -0.45f) : 0.0f), 18.0f), 1.0f, GazeFollowMs + GazeFreezeMs + 300);
        std::vector<Position> others = Fan(side + float(M_PI), 80.0f, 8.0f, 6);
        uint32 next = 0;
        for (Player* player : ArenaPlayers())
            if (IsBotPlayer(player) && std::find(targets.begin(), targets.end(), player) == targets.end())
                GroundIndicators::SetUnitSpot(me, player, others[next++ % others.size()], 1.0f,
                    GazeFollowMs + GazeFreezeMs + 300);
        for (Player* target : targets)
        {
            GroundIndicators::Area const tracking = GroundIndicators::ShowTrackingCone(me, Middle(), P2Radius + 1.0f,
                GazeArc, GazeFollowMs + GazeFreezeMs, target, MistakeDamage, false, GazeFollowMs, LOOK_GAZE_CONE);
            GroundIndicators::ShowCarriedLook(target, LOOK_MARK_GAZE, GazeFollowMs + GazeFreezeMs);
            ObjectGuid const guid = target->GetGUID();
            After(GazeFollowMs, [this, tracking, guid]()
            {
                Player* target = ObjectAccessor::GetPlayer(*me, guid);
                // Held where it points now (its drawing stops following: the same one)
                GroundIndicators::Area const frozen = target ? GroundIndicators::CurrentCone(target, tracking) : tracking;
                GroundIndicators::WatchArea(me, frozen, GazeFreezeMs, MistakeDamage);
                GroundIndicators::ShowDecal(me, frozen.origin, frozen.origin.GetOrientation(), frozen.radius,
                    GazeFreezeMs, LOOK_GAZE_FLARE);
                After(GazeFreezeMs, [this, frozen, guid]()
                {
                    Strike(Areas{ frozen }, RuleGaze, guid);
                    if (Player* target = ObjectAccessor::GetPlayer(*me, guid); target && _phase != Phase::Rehearsal)
                        Hit(target, SPELL_GAZE, GazeTargetPct);
                });
            });
        }
    }

    // A strike as Strike, one player spared (the gaze's target takes its own hit)
    void Strike(Areas const& areas, Rule const& rule, ObjectGuid spared)
    {
        if (_phase == Phase::Rehearsal)
        {
            Strike(areas, rule);
            return;
        }
        std::vector<Position> spots;
        for (GroundIndicators::Area const& area : areas)
        {
            spots.push_back(BurstSpot(area));
            Impact(rule.look, area);
        }
        if (rule.sound)
            SoundNearest(rule.sound, spots);
        for (Player* player : ArenaPlayers())
            if (player->GetGUID() != spared)
                for (GroundIndicators::Area const& area : areas)
                    if (area.Contains(player->GetPosition()))
                    {
                        Mistake(player, rule);
                        break;
                    }
    }

    // --- Sentence: halves (or quadrants) numbered, struck in their order ----------------------------------------------
    // version 0: two halves, 1: four quadrants (each closed by a spread), 2: two halves, no spread. At each count every
    // part but that count's strikes (its numeral painted in it): the group moves every count
    void Sentence(uint32 version)
    {
        bool const quadrants = version == 1;
        uint32 const parts = quadrants ? 4 : 2;
        float const arc = 360.0f / float(parts);
        float const base = frand(0.0f, 2.0f * float(M_PI));
        auto angleOf = [base, parts](uint32 part) { return base + float(part) * 2.0f * float(M_PI) / float(parts); };
        // The part safe at each count: the halves in turn (I, III on one, II, IV on the other), the quadrants in a
        // random order
        std::vector<uint32> safeAt = { 0, 1, 0, 1 };
        if (quadrants)
        {
            safeAt = { 0, 1, 2, 3 };
            Acore::Containers::RandomShuffle(safeAt);
        }
        me->SetFacingTo(angleOf(safeAt[0]));
        uint32 const lastMs = P2TellMs + (SentenceCounts - 1) * SentenceStepMs;
        std::vector<GroundIndicators::Area> areas(parts);
        for (uint32 part = 0; part < parts; ++part)
        {
            uint32 const count = uint32(std::find(safeAt.begin(), safeAt.end(), part) - safeAt.begin());
            uint32 const look = quadrants ? LOOK_SENTENCE_QUARTER_FIRST + count : LOOK_SENTENCE_HALF_FIRST + part;
            areas[part] = GroundIndicators::ShowTurningCone(me, Middle(), angleOf(part), P2Radius + 1.0f, arc,
                lastMs + 300, {}, MistakeDamage, false, look, LayerOf(look));
        }
        for (uint32 count = 0; count < SentenceCounts; ++count)
        {
            uint32 const lands = P2TellMs + count * SentenceStepMs;
            Areas striking;
            for (uint32 part = 0; part < parts; ++part)
                if (part != safeAt[count])
                    striking.push_back(areas[part]);
            // An area for the bots its last SentenceWatchMs only (the part they wait on would be red to them too)
            After(lands - SentenceWatchMs, [this, striking]()
            {
                for (GroundIndicators::Area const& area : striking)
                    GroundIndicators::WatchArea(me, area, SentenceWatchMs, MistakeDamage);
            });
            After(lands, [this, striking]() { Strike(striking, RuleSentence); });
            // The bots: into the count's part as the last one struck, close to him
            uint32 const from = count ? lands - SentenceStepMs + 100 : 0;
            float const safe = angleOf(safeAt[count]);
            After(from, [this, safe, arc, lands, from]()
            {
                GuideP2(Fan(safe, arc * 0.5f, SentenceAt, 8), lands - from + 300);
            });
        }
        if (version != 2)
            After(lastMs + 100, [this]() { Spread(); });
    }

    // A FervorRadius circle on everyone, bursting SpreadMs later: whoever stands in another's takes its hit
    void Spread()
    {
        std::vector<ObjectGuid> marked;
        for (Player* player : ArenaPlayers())
        {
            marked.push_back(player->GetGUID());
            GroundIndicators::ShowCarriedLook(player, LOOK_FERVOR_SPREAD, SpreadMs);
            GroundIndicators::WatchCarriedCircle(me, player, FervorRadius, SpreadMs, MistakeDamage);
        }
        // The bots: round the arena, far apart
        GuideP2(Fan(frand(0.0f, 2.0f * float(M_PI)), 315.0f, 15.0f, 8), SpreadMs + 300);
        After(SpreadMs, [this, marked]()
        {
            std::set<ObjectGuid> hit;
            for (ObjectGuid const& guid : marked)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                    continue;
                Blast(Ground(carrier->GetPosition()), FervorRadius);
                for (Player* other : ArenaPlayers())
                    if (other != carrier && !hit.count(other->GetGUID()) && other->GetExactDist2d(carrier) <= FervorRadius)
                        hit.insert(other->GetGUID());
            }
            Sound("Valerian.Fervor", Middle());
            if (_phase == Phase::Rehearsal)
                return;
            for (ObjectGuid const& guid : hit)
                if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                    Mistake(player, RuleSpread);
        });
    }

    // --- Le Chœur: seals, eight wedges light and shadow -------------------------------------------------------------------
    // Verses: each the floor turns (verses[k] steps, either way), then the choir sings; two steps bring a colour back
    // under whoever stands on it; a bell before a verse swaps the real players' seals. The first: four verses, the bell
    // before the 2nd and the 4th; the second: five, a verse every ChoirFastVerseMs, bells before the 2nd, 3rd and 5th
    void Choir(bool second)
    {
        float const offset = frand(0.0f, 2.0f * float(M_PI));
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const step = float(M_PI) / 4.0f;
        std::vector<int32> const verses = second ? std::vector<int32>{ 1, -1, 2, 1, -1 } :
            std::vector<int32>{ 1, -1, 2, -1 };
        std::set<uint32> const bells = second ? std::set<uint32>{ 1, 2, 4 } : std::set<uint32>{ 1, 3 };
        uint32 const verseMs = second ? ChoirFastVerseMs : ChoirVerseMs;
        uint32 const cast = second ? CAST_CHOIR_FAST : CAST_CHOIR;
        uint32 const count = uint32(verses.size());
        auto singAt = [verseMs](uint32 verse) { return ChoirFirstCastMs + verseMs * (verse + 1); };
        uint32 const total = singAt(count - 1);
        // The wedges after each verse's turn (where wedge 0 starts)
        std::vector<float> after(count);
        std::vector<GroundIndicators::Turn> turns;
        float rotation = offset;
        for (uint32 verse = 0; verse < count; ++verse)
        {
            float const by = way * float(verses[verse]) * step;
            turns.push_back({ singAt(verse) - ChoirTurnBeforeMs, ChoirTurnMs, by });
            rotation += by;
            after[verse] = rotation;
        }
        // The seals: half light (0), half shadow (1)
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomShuffle(players);
        auto seals = std::make_shared<std::map<ObjectGuid, uint32>>();
        for (uint32 index = 0; index < players.size(); ++index)
        {
            uint32 const seal = index % 2;
            (*seals)[players[index]->GetGUID()] = seal;
            GroundIndicators::ShowCarriedLook(players[index], seal ? LOOK_MARK_SEAL_SHADOW : LOOK_MARK_SEAL_LIGHT,
                total + 500);
        }
        DrawWedges(offset, turns, total);
        After(ChoirFirstCastMs, [this, cast]() { CastBar(cast); });
        // The bots: each to the nearest wedge of its colour (wedge k: light if k even)
        // within: where in its wedge (0 its start, 1 its end, counterclockwise): before a turn by the edge it turns
        // towards, after it just past the new wedge's edge they come from - a short step (a long arc was late)
        // (Handed out in turn first: a bell's swap changed how many bots carried each colour, every bot after the
        // swapped one got another wedge - some across the arena - and one was late for the singing: a real wipe)
        auto place = [this, seals, step](float wedgesAt, float within, uint32 ms)
        {
            Position const middle = Middle();
            for (Player* player : SortedBots())
            {
                auto const found = seals->find(player->GetGUID());
                if (found == seals->end())
                    continue;
                float const at = middle.GetAbsoluteAngle(player);
                float best = 0.0f;
                float bestOff = 10.0f;
                for (uint32 wedge = found->second; wedge < 8; wedge += 2)
                {
                    float const spot = wedgesAt + (float(wedge) + within) * step;
                    float off = Position::NormalizeOrientation(spot - at);
                    off = std::min(off, 2.0f * float(M_PI) - off);
                    if (off < bestOff)
                    {
                        bestOff = off;
                        best = spot;
                    }
                }
                GroundIndicators::SetUnitSpot(me, player, P2At(best, ChoirRing), 1.0f, ms);
            }
        };
        for (uint32 verse = 0; verse < count; ++verse)
        {
            uint32 const turnsAt = singAt(verse) - ChoirTurnBeforeMs;
            uint32 const from = verse ? singAt(verse - 1) + 100 : 0;
            float const wedgesAt = verse ? after[verse - 1] : offset;
            bool const forward = way * float(verses[verse]) > 0.0f;
            bool const two = std::abs(verses[verse]) == 2;
            float const before = forward ? 0.8f : 0.2f;
            float const behind = forward ? 0.25f : 0.75f;
            // Before its turn (a two-step turn: held there to the singing - the colour comes back)
            After(from, [place, wedgesAt, before, turnsAt, from, two, verse, singAt]()
            {
                place(wedgesAt, before, (two ? singAt(verse) : turnsAt) - from + 300);
            });
            // The bell: the seals swap before the turn
            if (bells.count(verse))
                After(turnsAt - ChoirBellBeforeMs, [this, seals, place, wedgesAt, before, turnsAt, total]()
                {
                    SwapSeals(*seals, total - (turnsAt - ChoirBellBeforeMs) + 500);
                    place(wedgesAt, before, ChoirBellBeforeMs + 300);
                });
            if (!two)
                After(turnsAt, [place, turned = after[verse], behind, verse, singAt, turnsAt]()
                {
                    place(turned, behind, singAt(verse) - turnsAt + 300);
                });
            After(singAt(verse), [this, seals, step, cast, turned = after[verse], last = verse + 1 == count]()
            {
                EndCastBar();
                if (!last)
                    CastBar(cast);
                Position const middle = Middle();
                Player* wrong = nullptr;
                for (Player* player : ArenaPlayers())
                {
                    auto const found = seals->find(player->GetGUID());
                    if (found == seals->end())
                        continue;
                    float const at = Position::NormalizeOrientation(middle.GetAbsoluteAngle(player) - turned);
                    uint32 const wedge = uint32(at / step) % 8;
                    if (wedge % 2 != found->second)
                        wrong = player;
                }
                Sound("Valerian.LitanyEnd", middle);
                if (!wrong)
                    return;
                LOG_INFO(_logName, "Valerian dissonance instance={} {} bot={}", me->GetInstanceId(), wrong->GetName(),
                         IsBotPlayer(wrong));
                Sound("Valerian.Profanation", middle);
                Blast(middle, P2Radius);
                if (_phase != Phase::Rehearsal)
                    for (Player* player : ArenaPlayers())
                        Doom(player, SPELL_DISSONANCE);
            });
        }
    }

    // The shadow wedges (odd k from offset; the light ones are the gaps between), drawn once for the whole choir and
    // turning their step in place at ChoirTurnMs (red until their art); drawn only: a colour to stand on, not a hit (as
    // areas the bots of the shadow seal fled them)
    void DrawWedges(float offset, std::vector<GroundIndicators::Turn> const& turns, uint32 lastsMs)
    {
        // Both colours painted (wedge k light if k even), drawn only: the floor's colour to stand on, not a hit
        for (uint32 wedge = 0; wedge < 8; ++wedge)
        {
            bool const light = wedge % 2 == 0;
            GroundIndicators::ShowTurningCone(me, Middle(), offset + (float(wedge) + 0.5f) * float(M_PI) / 4.0f,
                P2Radius + 1.0f, 45.0f, lastsMs, turns, MistakeDamage, false,
                light ? LOOK_CHOIR_LIGHT : LOOK_CHOIR_SHADOW, light ? LOOK_CHOIR_LIGHT_GLOW : LOOK_CHOIR_SHADOW_WISPS);
        }
    }

    // The swap (a bell): every real player swaps seals with a neighbour of the other seal - a real player beside them if
    // there is one, else the nearest bot of the other seal
    // holdMs: how long the new seals are worn (to the last singing)
    void SwapSeals(std::map<ObjectGuid, uint32>& seals, uint32 holdMs)
    {
        Sound("Valerian.VerseTell", Middle());
        std::vector<Player*> players = ArenaPlayers();
        std::set<ObjectGuid> swapped;
        auto doSwap = [this, &seals, &swapped, holdMs](Player* a, Player* b)
        {
            std::swap(seals[a->GetGUID()], seals[b->GetGUID()]);
            swapped.insert(a->GetGUID());
            swapped.insert(b->GetGUID());
            // The old seal burnt into the new (its flipbook), then the new one held to the singing
            for (Player* who : { a, b })
            {
                bool const shadow = seals[who->GetGUID()] != 0;
                GroundIndicators::ClearCarriedLook(who, LOOK_MARK_SEAL_LIGHT);
                GroundIndicators::ClearCarriedLook(who, LOOK_MARK_SEAL_SHADOW);
                GroundIndicators::ShowCarriedLook(who, shadow ? LOOK_SEAL_SWAP_LS : LOOK_SEAL_SWAP_SL, SealSwapMs);
                ObjectGuid const guid = who->GetGUID();
                After(SealSwapMs, [this, guid, shadow, holdMs]()
                {
                    if (Player* swapped = ObjectAccessor::GetPlayer(*me, guid))
                        GroundIndicators::ShowCarriedLook(swapped, shadow ? LOOK_MARK_SEAL_SHADOW : LOOK_MARK_SEAL_LIGHT,
                            holdMs > SealSwapMs ? holdMs - SealSwapMs : 0);
                });
            }
        };
        for (Player* player : players)
        {
            if (IsBotPlayer(player) || swapped.count(player->GetGUID()) || !seals.count(player->GetGUID()))
                continue;
            Player* partner = nullptr;
            float best = 1000.0f;
            // A real player of the other seal beside them first
            for (Player* other : players)
                if (other != player && !IsBotPlayer(other) && !swapped.count(other->GetGUID()) &&
                    seals.count(other->GetGUID()) && seals[other->GetGUID()] != seals[player->GetGUID()] &&
                    player->GetExactDist2d(other) <= ChoirNeighbour && player->GetExactDist2d(other) < best)
                {
                    partner = other;
                    best = player->GetExactDist2d(other);
                }
            if (!partner)
                for (Player* other : players)
                    if (other != player && IsBotPlayer(other) && !swapped.count(other->GetGUID()) &&
                        seals.count(other->GetGUID()) && seals[other->GetGUID()] != seals[player->GetGUID()] &&
                        player->GetExactDist2d(other) < best)
                    {
                        partner = other;
                        best = player->GetExactDist2d(other);
                    }
            if (partner)
                doSwap(player, partner);
        }
        // In a group of bots alone (the lab, a bot test), two of them anyway
        if (swapped.empty())
            for (Player* a : players)
                for (Player* b : players)
                    if (swapped.empty() && a != b && seals.count(a->GetGUID()) && seals.count(b->GetGUID()) &&
                        seals[a->GetGUID()] != seals[b->GetGUID()])
                        doSwap(a, b);
    }

    // --- Écho: the rewind, the after-images, the catalyst -----------------------------------------------------------------
    void Echo()
    {
        CastBar(CAST_ECHO);
        Areas echoes;
        for (Player* player : ArenaPlayers())
            echoes.push_back(Painted(MakeCircle(Ground(player->GetPosition()), EchoRadius), LOOK_ECHO, EchoMs));
        // The light flowing back into him: his apotheosis' streams
        for (uint32 at = 0; at < EchoMs; at += 250)
            After(at, [this]() { StreamLight(); });
        After(EchoMs, [this, echoes]()
        {
            EndCastBar();
            Strike(echoes, RuleEcho);
            Sound("Valerian.FirstBlow", Middle());
            GroundIndicators::Burst(me, Middle(), GroundIndicators::Theme::Holy);
            if (_phase == Phase::Rehearsal)
                return;
            for (Player* player : ArenaPlayers())
            {
                uint32 const left = uint32(float(player->GetMaxHealth()) * EchoLeftPct / 100.0f);
                if (player->GetHealth() > left)
                    HitOwnHealth(player, SPELL_CATALYST,
                        float(player->GetHealth() - left) * 100.0f / float(player->GetMaxHealth()));
            }
        });
    }

    // --- Ascension: a pizza, its safe slices where the orbs come in ---------------------------------------------------------
    // The pizza (six of eight slices burning), one drawing for its three waves, turning a slice before the second and
    // the third; burning whoever stands in it all along (from its tell); the orbs come down its two safe slices
    void Ascension()
    {
        float const slice = float(M_PI) / 4.0f;
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        std::vector<GroundIndicators::Turn> turns;
        for (uint32 wave = 1; wave < 3; ++wave)
            turns.push_back({ wave * AscensionWaveMs - AscensionTurnMs, AscensionTurnMs, way * slice });
        Areas burning;
        for (uint32 index = 1; index < 8; ++index)
            if (index != 4)
                // Drawn only (as areas all along, the bots refused their spots in the safe slices and fell back to him):
                // the bots are guided along the safe slices (GuideAscension), the burning checked below
                burning.push_back(GroundIndicators::ShowTurningCone(me, Middle(), _pizzaBase + float(index) * slice,
                    P2Radius + 1.0f, 45.0f, AscensionLastsMs, turns, MistakeDamage, false, LOOK_DIVINE_CONE_45,
                    LOOK_DIVINE_GLOW_45));
        // Each bot its place for the whole pizza (its lane, its side of it, how far): a place handed out again from a
        // list as the catchers changed sent bots across the arena, through the burning slices by him
        _ascensionSlots.clear();
        uint32 slot = 0;
        for (Player* bot : SortedBots())
            _ascensionSlots[bot->GetGUID()] = slot++;
        auto lastHit = std::make_shared<std::map<ObjectGuid, uint32>>();
        for (uint32 t = AscensionTellMs; t < AscensionLastsMs; t += AscensionTickMs)
            After(t, [this, burning, turns, t, lastHit]()
            {
                float const turned = GroundIndicators::TurnedBy(turns, t);
                for (Player* player : ArenaPlayers())
                {
                    auto const before = lastHit->find(player->GetGUID());
                    if (before != lastHit->end() && t - before->second < AscensionGraceMs)
                        continue;
                    for (GroundIndicators::Area area : burning)
                    {
                        area.origin.SetOrientation(Position::NormalizeOrientation(area.origin.GetOrientation() + turned));
                        if (!area.Contains(player->GetPosition()))
                            continue;
                        (*lastHit)[player->GetGUID()] = t;
                        Blast(Ground(player->GetPosition()), 2.0f);
                        if (_phase != Phase::Rehearsal)
                            Mistake(player, RulePizza);
                        break;
                    }
                }
            });
        for (uint32 wave = 0; wave < 3; ++wave)
            After(wave * AscensionWaveMs, [this, wave, way, slice]()
            {
                AscensionWave(_pizzaBase + float(wave) * way * slice, wave % 2);
            });
        // The bots: in the safe slices, along with them as they turn
        for (uint32 t = 0; t < AscensionLastsMs; t += AilesGuideStepMs)
            After(t, [this, turns, t]() { GuideAscension(_pizzaBase + GroundIndicators::TurnedBy(turns, t + 300)); });
    }

    // A wave: its orbs down the safe slices (safe: one's middle; the other opposite), who may take one marked. One
    // lane (playersLane: 0 safe's, 1 the other) is the real players': no bot takes its orb (they took every one, the
    // players never had to)
    void AscensionWave(float safe, uint32 playersLane)
    {
        uint32 const markMs = uint32(OrbFrom / OrbSpeed * 1000.0f) + 1000;
        _catchers.clear();
        for (Player* player : ArenaPlayers())
        {
            GroundIndicators::ClearCarriedLook(player, LOOK_MARK_ORB_FREE);
            GroundIndicators::ClearCarriedLook(player, LOOK_MARK_ORB_FULL);
            if (Aura const* overload = player->GetAura(SPELL_OVERLOAD))
                GroundIndicators::ShowCarriedLook(player, LOOK_MARK_ORB_FULL, uint32(overload->GetDuration()));
            else
                GroundIndicators::ShowCarriedLook(player, LOOK_MARK_ORB_FREE, markMs);
        }
        // The orbs: one in each safe slice, from the wall to him, along a lane painted to him
        for (float lane : { safe, safe + float(M_PI) })
        {
            GroundIndicators::ShowDecal(me, P2At(lane, OrbFrom), lane + float(M_PI), OrbFrom - 2.0f, markMs,
                LOOK_ORB_LANE);
            Position from = P2At(lane, OrbFrom);
            from.m_positionZ += LightHeight;
            if (Creature* orb = me->SummonCreature(NPC_ORB, from, TEMPSUMMON_TIMED_DESPAWN,
                uint32((OrbFrom / OrbSpeed + 2.0f) * 1000.0f)))
            {
                orb->SetCanFly(true);
                orb->SetDisableGravity(true);
                orb->SetReactState(REACT_PASSIVE);
                Position to = P2At(lane, 2.5f);
                to.m_positionZ += LightHeight;
                orb->GetMotionMaster()->MovePoint(1, to, FORCED_MOVEMENT_NONE, OrbSpeed, false);
                _orbs.push_back({ orb->GetGUID(), lane, OrbFrom });
            }
        }
        // Its catcher: on the bots' lane a bot already beside it (its place on that lane), free of Surcharge
        _catchers.assign(2, ObjectGuid::Empty);
        for (Player* bot : SortedBots())
        {
            auto const slot = _ascensionSlots.find(bot->GetGUID());
            if (slot == _ascensionSlots.end() || slot->second % 2 == playersLane || bot->HasAura(SPELL_OVERLOAD) ||
                IsTank(bot))
                continue;
            ObjectGuid& catcher = _catchers[slot->second % 2];
            if (catcher.IsEmpty())
                catcher = bot->GetGUID();
        }
    }

    // The bots in the safe slices (safe: one's middle), each at its place (AscensionAside yd off its lane, near or far),
    // a wave's catcher on its lane
    void GuideAscension(float safe)
    {
        for (Player* bot : SortedBots())
        {
            auto const found = _ascensionSlots.find(bot->GetGUID());
            uint32 const slot = found != _ascensionSlots.end() ? found->second : 0;
            float const lane = safe + (slot % 2 ? float(M_PI) : 0.0f);
            bool const catcher = _catchers.size() == 2 && _catchers[slot % 2] == bot->GetGUID();
            float const d = (slot / 4) % 2 ? 17.0f : 12.0f;
            float const side = (slot / 2) % 2 ? 1.0f : -1.0f;
            Position const spot = catcher ? P2At(lane, 10.0f) : P2At(lane + side * std::asin(AscensionAside / d), d);
            GroundIndicators::SetUnitSpot(me, bot, spot, catcher ? 0.8f : 1.0f, AilesGuideStepMs + 400);
        }
    }

    // The orbs in flight: taken by whoever they touch (Surcharge; a second under it bursts), absorbed at him (healed)
    void UpdateOrbs(uint32 /*diff*/)
    {
        if (_orbs.empty())
            return;
        Position const middle = Middle();
        for (auto it = _orbs.begin(); it != _orbs.end();)
        {
            Creature* orb = ObjectAccessor::GetCreature(*me, it->guid);
            if (!orb)
            {
                it = _orbs.erase(it);
                continue;
            }
            Player* taker = nullptr;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(orb) <= OrbReach)
                {
                    taker = player;
                    break;
                }
            if (taker)
            {
                if (_phase != Phase::Rehearsal)
                {
                    if (taker->HasAura(SPELL_OVERLOAD))
                    {
                        Mistake(taker, Rule{ SPELL_ORB_BURST, OrbBurstPct, "orb burst" });
                        Blast(Ground(taker->GetPosition()), 3.0f);
                    }
                    else
                    {
                        AddTimedAura(taker, SPELL_OVERLOAD, OverloadMs);
                        GroundIndicators::ClearCarriedLook(taker, LOOK_MARK_ORB_FREE);
                        GroundIndicators::ShowCarriedLook(taker, LOOK_MARK_ORB_FULL, OverloadMs);
                    }
                }
                Sound("Valerian.OrbTaken", orb->GetPosition());
                orb->DespawnOrUnsummon();
                it = _orbs.erase(it);
                continue;
            }
            if (orb->GetExactDist2d(&middle) <= 3.0f)
            {
                // It reaches him: he heals
                if (_phase == Phase::Ascended)
                    _p2True = std::min(_p2Pool, _p2True + uint32(float(_p2Pool) * OrbHealPct / 100.0f));
                Sound("Valerian.OrbAbsorb", middle);
                me->SendPlaySpellVisual(KIT_HOLY_GLOW);
                orb->DespawnOrUnsummon();
                it = _orbs.erase(it);
                continue;
            }
            ++it;
        }
    }

    // --- Éclipse: the dark everywhere but its holes of light ----------------------------------------------------------------
    // version 0: two holes going round; 1: one, behind, the other way; phase 3's 2: two holes going round, then back
    // halfway; 3: one, round and back the other way
    void Eclipse(uint32 version)
    {
        bool const second = version % 2 == 1;
        Position const middle = Middle();
        std::vector<Position> holes;
        // The veil faces between the first two holes (EclipseSpread each side: its picture symmetric, so a mirrored
        // projection cannot move a hole); the second's one is straight behind
        if (!second)
        {
            _eclipseAngle = frand(0.0f, 2.0f * float(M_PI));
            holes.push_back(P2At(_eclipseAngle - EclipseSpread, EclipseHoleAt));
            holes.push_back(P2At(_eclipseAngle + EclipseSpread, EclipseHoleAt));
        }
        else
            holes.push_back(P2At(_eclipseAngle + float(M_PI), EclipseHoleAt + 1.0f));
        uint32 const lasts = second ? EclipseSecondMs : EclipseMs;
        // The holes go round him all along (the second the other way): one turn of the veil and its light about him
        if (!second)
            _eclipseWay = urand(0, 1) ? 1.0f : -1.0f;
        float const way = second ? -_eclipseWay : _eclipseWay;
        float const turn = EclipseTurnDeg * float(M_PI) / 180.0f;
        std::vector<GroundIndicators::Turn> turns = { { 0, lasts, way * turn } };
        // (phase 3's: round and back, half as far each way - a whole 90 degrees in half the time ran the holes at
        // 9-12 yd/s, faster than anyone runs: the bots were caught)
        if (version == 2 || version == 3)
            turns = { { 0, lasts / 2, way * turn / 2.0f }, { lasts / 2, lasts / 2, -way * turn / 2.0f } };
        Sound("Valerian.Profanation", middle);
        // The dark, its holes cut (drawn only - as an area, it had the bots refuse the holes inside it), and the holes'
        // light painted where they are, both turning together
        // (a decal's picture lies a quarter turn on from its carrier's facing: its x along facing + pi / 2 - seen in the
        // FX lab, the holes a quarter turn off their light)
        float const facing = _eclipseAngle - float(M_PI) / 2.0f;
        GroundIndicators::ShowTurningDecal(me, middle, facing, P2Radius, lasts,
            second ? LOOK_ECLIPSE_VEIL_ONE : LOOK_ECLIPSE_VEIL_TWO, turns);
        GroundIndicators::ShowTurningDecal(me, middle, facing, P2Radius, lasts,
            second ? LOOK_ECLIPSE_LIGHT_ONE : LOOK_ECLIPSE_LIGHT_TWO, turns);
        // Where the holes are after t ms
        auto holesAt = [this, holes, middle, turns](uint32 t)
        {
            float const turned = GroundIndicators::TurnedBy(turns, t);
            std::vector<Position> at;
            for (Position const& hole : holes)
                at.push_back(P2At(middle.GetAbsoluteAngle(&hole) + turned, middle.GetExactDist2d(&hole)));
            return at;
        };
        // The bots: split between the holes, along with them
        for (uint32 t = 0; t < lasts; t += AilesGuideStepMs)
            After(t, [this, holesAt, t]()
            {
                std::vector<Position> spots;
                for (Position const& hole : holesAt(t + 300))
                    for (uint32 index = 0; index < 4; ++index)
                        spots.push_back(Ground(AtAngle(hole, float(index) * float(M_PI) / 2.0f, 1.5f)));
                GuideP2(spots, AilesGuideStepMs + 400);
            });
        holes = holesAt(lasts);
        After(lasts, [this, holes]()
        {
            Blast(Middle(), P2Radius);
            for (Player* player : ArenaPlayers())
            {
                bool lit = false;
                for (Position const& hole : holes)
                    if (player->GetExactDist2d(&hole) <= EclipseHole)
                        lit = true;
                if (!lit)
                {
                    LOG_INFO(_logName, "Valerian eclipse instance={} caught {} bot={}", me->GetInstanceId(),
                             player->GetName(), IsBotPlayer(player));
                    if (_phase != Phase::Rehearsal)
                        Doom(player, SPELL_ECLIPSE);
                }
                // In the light: it breaks on them all the same (the healers' check)
                else if (_phase != Phase::Rehearsal)
                    HitOwnHealth(player, SPELL_ECLIPSE, EclipseHitPct);
            }
        });
    }

    // --- Lumière totale: his last cast, the end at 4:35.0 -----------------------------------------------------------------
    // sigilAfterMs: its sigil only that long into the cast (phase 3: once the last Prémonition has struck)
    void TotalLight(uint32 sigilAfterMs)
    {
        CastBar(CAST_TOTAL_LIGHT);
        // The whole arena (it lies over the last patterns' decals, drawn as the client sorts them: the tendrils then
        // stand up as curtains, TendrilCurtain)
        After(sigilAfterMs, [this, sigilAfterMs]()
        {
            uint32 const lasts = TotalLightMs - TotalLightCastMs - sigilAfterMs;
            GroundIndicators::ShowDecal(me, Middle(), 0.0f, P2Radius, lasts, LOOK_TOTAL_LIGHT_SIGIL);
            GroundIndicators::ShowDecal(me, Middle(), 0.0f, P2Radius, lasts, LOOK_TOTAL_LIGHT_RUNES);
        });
        After(TotalLightMs - TotalLightCastMs, [this]()
        {
            EndCastBar();
            Blast(Middle(), P2Radius);
            Sound("Valerian.FirstBlow", Middle());
            for (Player* player : ArenaPlayers())
                Doom(player, SPELL_TOTAL_LIGHT);
        });
    }

    // --- Phase 3: Le Refus -------------------------------------------------------------------------------------------
    // A Sablier volley: the hourglasses of one parity (every other one) firing at once, strikesMs into the pattern
    struct Volley
    {
        uint32 strikesMs;
        uint32 parity;
        bool farthest;
        float side = 1.0f;      // the side of its hourglass its baiters stand (the bots: SablierBait)
    };

    enum class Vision : uint8
    {
        In,         // his light, close to him
        Out,        // the ring beyond it
        Left,       // the half on his left (of where he faced as he began)
        Right,
    };

    struct TimeChain
    {
        ObjectGuid holder;
        ObjectGuid tether;
    };

    // Phase 3's clock: from its music's start
    uint32 P3Ms() const { return _p3From && Elapsed() > _p3From ? Elapsed() - _p3From : 0; }

    bool InCalmP3() const
    {
        uint32 const now = P3Ms();
        for (Calm const& calm : P3Calms)
            if (now >= calm.fromMs && now < calm.toMs)
                return true;
        return now >= P3TotalLightCastMs;
    }

    // Where everyone has been in phase 2: a step every TrailEveryMs, back to RefusBackMs ago
    void RecordTrail(uint32 elapsed)
    {
        if (elapsed < _nextTrailMs)
            return;
        _nextTrailMs = elapsed + TrailEveryMs;
        for (Player* player : ArenaPlayers())
        {
            auto& trail = _trail[player->GetGUID()];
            trail.push_back({ elapsed, Ground(player->GetPosition()) });
            while (trail.size() > 1 && trail[1].first + RefusBackMs <= elapsed)
                trail.pop_front();
        }
    }

    // Where player stood RefusBackMs ago (as far back as their trail goes)
    Position PastSpot(Player* player) const
    {
        auto const found = _trail.find(player->GetGUID());
        if (found == _trail.end() || found->second.empty())
            return Ground(player->GetPosition());
        return found->second.front().second;
    }

    // At nothing in phase 2: time stops and runs back (in the lab: its sound, the music, harmless)
    void StartRefus()
    {
        bool const rehearsed = _phase == Phase::Rehearsal;
        if (!rehearsed)
        {
            _phase = Phase::Refusing;
            scheduler.CancelAll();
            EndCastBar();
            _p2Next = _p2Timeline.size();
            GroundIndicators::ClearAreasOf(me);
            KeepBotsIn(WallLastsMs);
            GroundIndicators::FadeLooksOf(me, LOOK_DIVINE_CONE_45, LOOK_TENDRIL_CURTAIN);
            _ascendedMusicOver = true;
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->AddAura(SPELL_OUT_OF_REACH, me);
            // His health anew (phase 3's), filled as time runs back
            SetModelHealth();
            _p3Pool = uint32(me->GetMaxHealth());
            _p3True = _p3Pool;
            me->SetHealth(1);
        }
        // The music cut: phase 3's track starts with the rewind itself (as loud as the music, on its channel), and
        // its clock over everyone's screen (RefusOverlay.lua, on the same channel)
        EndMusic();
        StartMusic("Music.ValerianRewind");
        for (Player* player : Listeners())
            EvolutionsAudio::Command(player, "R");
        CastBar(CAST_REFUS);
        // A clock under him while time stands still and runs back, its hands spun backwards
        GroundIndicators::ShowDecal(me, Middle(), 0.0f, SigilRadius, RefusMusicMs, LOOK_REFUS_SIGIL);
        GroundIndicators::ShowDecal(me, Middle(), 0.0f, SigilRadius, RefusMusicMs, LOOK_REFUS_HANDS);
        Pose(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        LOG_INFO(_logName, "Valerian refuses instance={} at={:.1f}s health={}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, _p3Pool);
        // Where everyone goes back to; held meanwhile
        std::vector<std::pair<ObjectGuid, Position>> back;
        for (Player* player : ArenaPlayers())
        {
            back.push_back({ player->GetGUID(), PastSpot(player) });
            if (!rehearsed)
                AddTimedAura(player, SPELL_TIME_STILL, RefusPullMs);
        }
        _trail.clear();
        uint32 const drop = RefusMusicMs + RefusDropMs;
        auto echoes = std::make_shared<Areas>();
        After(RefusStillMs, [this, back, drop, echoes]()
        {
            for (auto const& [guid, spot] : back)
                echoes->push_back(Painted(MakeCircle(spot, EchoRadius), LOOK_ECHO, drop - RefusStillMs));
        });
        for (uint32 at = RefusStillMs; at <= RefusPullMs; at += 100)
            After(at, [this, at, rehearsed]()
            {
                if (at % 300 == 0)
                    StreamLight();
                if (!rehearsed)
                    me->SetHealth(std::max<uint32>(1, uint32(uint64(_p3Pool) * (at - RefusStillMs) /
                        (RefusPullMs - RefusStillMs))));
            });
        After(RefusPullMs, [this, back, rehearsed]()
        {
            for (auto const& [guid, spot] : back)
                if (Player* player = ObjectAccessor::GetPlayer(*me, guid); player && player->IsAlive())
                {
                    GroundIndicators::PlayKit(me, Ground(player->GetPosition()), KIT_HOLY_IMPACT, 0.6f, 0, ImpactMs);
                    if (!rehearsed)
                        player->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ() + 0.5f,
                            player->GetOrientation());
                }
        });
        After(RefusMusicMs, [this, rehearsed]() { StartRewound(rehearsed); });
        After(drop, [this, echoes, rehearsed]()
        {
            Strike(*echoes, RuleRefus);
            Sound("Valerian.FirstBlow", Middle());
            GroundIndicators::Burst(me, Middle(), GroundIndicators::Theme::Holy);
            if (rehearsed)
                return;
            for (Player* player : ArenaPlayers())
            {
                uint32 const left = uint32(float(player->GetMaxHealth()) * RefusLeftPct / 100.0f);
                if (player->GetHealth() > left)
                    HitOwnHealth(player, SPELL_CATALYST,
                        float(player->GetHealth() - left) * 100.0f / float(player->GetMaxHealth()));
            }
        });
    }

    // Phase 3, its music in (under the rewind's end: its track started with the rewind)
    void StartRewound(bool rehearsed)
    {
        _p3From = Elapsed();
        if (rehearsed)
            return;
        _phase = Phase::Rewound;
        EndCastBar();
        Pose(EMOTE_STATE_READY_SPELL_OMNI);
        me->RemoveAurasDueToSpell(SPELL_OUT_OF_REACH);
        me->SetHealth(_p3Pool);
        _p3Timeline = BuildP3Timeline();
        _p3Next = 0;
        _p3MusicOver = false;
        _nextCheckMs = 0;
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, GetRoom().reach);
        LOG_INFO(_logName, "Valerian rewound instance={} health={}", me->GetInstanceId(), me->GetMaxHealth());
    }

    void ExecuteP3(P3Step const& step)
    {
        LOG_INFO(_logName, "Valerian p3 {} instance={} at={:.2f}s shown={:.1f}% true={:.1f}%", uint32(step.what),
                 me->GetInstanceId(), P3Ms() / 1000.0f, me->GetHealthPct(),
                 _p3Pool ? 100.0f * float(_p3True) / float(_p3Pool) : 0.0f);
        switch (step.what)
        {
            case P3Event::Sabliers: Sabliers(step.arg); break;
            case P3Event::Premonition: Premonition(step.arg); break;
            case P3Event::Chains: Chains(step.arg); break;
            case P3Event::Eclipse: Eclipse(step.arg); break;
            case P3Event::TotalLight: TotalLight(P3TotalLightSigilMs); break;
        }
    }

    // His health in phase 3: paced from the drop to nothing at P3EarliestDeathMs, never before
    uint32 P3Shown() const
    {
        uint32 const now = P3Ms();
        float const share = now <= RefusDropMs ? 1.0f : now >= P3EarliestDeathMs ? 0.0f :
            1.0f - float(now - RefusDropMs) / float(P3EarliestDeathMs - RefusDropMs);
        return std::max(_p3True, uint32(float(_p3Pool) * share));
    }

    void P3Damage(uint32& damage)
    {
        _p3True = _p3True > damage ? _p3True - damage : 0;
        uint32 const shown = P3Shown();
        uint32 const health = uint32(me->GetHealth());
        damage = shown == 0 ? health : health > shown ? health - shown : 0;
    }

    void UpdateP3()
    {
        uint32 const shown = P3Shown();
        if (shown == 0)
        {
            // Past his earliest death with nothing left: he falls at last (the one who holds him the killer)
            Unit* killer = me->GetVictim();
            if (!killer)
                for (Player* player : ArenaPlayers())
                    killer = player;
            if (killer)
                Unit::Kill(killer, me);
            return;
        }
        if (me->GetHealth() != shown)
            me->SetHealth(shown);
    }

    // --- Les Sabliers: seals firing at the players they marked (violet) ---------------------
    std::vector<Volley> SablierVolleys(uint32 version) const
    {
        switch (version)
        {
            case 0: return { { 6000, 0, false }, { 11000, 1, true } };
            case 1: return { { 5000, 0, false }, { 9000, 1, true }, { 13000, 1, false }, { 17000, 0, true } };
            case 2: return { { 4000, 0, false }, { 7000, 1, true }, { 10000, 1, false }, { 13000, 0, true } };
            default: return { { 4000, 1, true }, { 7000, 0, false }, { 10000, 0, true }, { 13000, 1, false } };
        }
    }

    void Sabliers(uint32 version)
    {
        CastBar(CAST_SABLIERS);
        float const base = frand(0.0f, 2.0f * float(M_PI));
        std::vector<Volley> volleys = SablierVolleys(version);
        uint32 const tellMs = version >= 2 ? 3000 : 4000;
        // The marked bots' side of each seal, volley by volley: a short walk from the spots two volleys before, clear
        // of the volley before when it can be (they set off as they are marked, while it still shows)
        for (uint32 index = 1; index < volleys.size(); ++index)
        {
            float best = 1.0f;
            float bestCost = 0.0f;
            for (float side : { 1.0f, -1.0f })
            {
                volleys[index].side = side;
                float cost = 0.0f;
                for (float angle : VolleyAngles(base, volleys[index]))
                {
                    Position const bait = SablierBait(angle, volleys[index]);
                    if (index >= 2)
                    {
                        float nearest = 100.0f;
                        for (float before : VolleyAngles(base, volleys[index - 2]))
                        {
                            Position const was = SablierBait(before, volleys[index - 2]);
                            nearest = std::min(nearest, bait.GetExactDist2d(&was));
                        }
                        cost += nearest;
                    }
                    for (float before : VolleyAngles(base, volleys[index - 1]))
                        if (PredictedSablier(before, volleys[index - 1]).Contains(bait, 2.0f))
                            cost += 30.0f;
                }
                if (side > 0.0f || cost < bestCost)
                {
                    best = side;
                    bestCost = cost;
                }
            }
            volleys[index].side = best;
        }
        // Each hourglass in the colour of its next volley: it flips as it fires
        for (uint32 index = 0; index < Hourglasses; ++index)
        {
            Position const at = P2At(base + float(index) * float(M_PI) / 3.0f, HourglassAt);
            uint32 from = 0;
            for (Volley const& volley : volleys)
            {
                if (volley.parity != index % 2)
                    continue;
                // Its seal until it fires; then, if it fires again, burning into the other
                uint32 const seal = volley.farthest ? LOOK_SABLIER_SHADOW : LOOK_SABLIER_LIGHT;
                if (volley.strikesMs > from)
                    After(from, [this, at, seal, lasts = volley.strikesMs - from]()
                    {
                        GroundIndicators::ShowBillboard(me, at, seal, lasts, nullptr);
                    });
                from = volley.strikesMs;
                bool const again = std::any_of(volleys.begin(), volleys.end(), [&volley, index](Volley const& later)
                {
                    return later.parity == index % 2 && later.strikesMs > volley.strikesMs;
                });
                if (again)
                {
                    uint32 const swap = volley.farthest ? LOOK_SABLIER_SWAP_SL : LOOK_SABLIER_SWAP_LS;
                    After(from, [this, at, swap]()
                    {
                        GroundIndicators::ShowBillboard(me, at, swap, SealSwapMs, nullptr);
                    });
                    from += SealSwapMs;
                }
            }
        }
        // Each volley: its marks first, then its cones on them
        for (Volley const& volley : volleys)
        {
            uint32 const tellAt = volley.strikesMs - tellMs;
            uint32 const markAt = tellAt > SablierMarkLeadMs ? tellAt - SablierMarkLeadMs : 0;
            auto marks = std::make_shared<std::vector<ObjectGuid>>();
            After(markAt, [this, base, volley, marks, lasts = volley.strikesMs - markAt]()
            {
                MarkSabliers(base, volley, lasts, *marks);
            });
            After(tellAt, [this, base, volley, tellMs, marks]() { SablierVolley(base, volley, tellMs, *marks); });
        }
        After(volleys.front().strikesMs, [this]() { EndCastBar(); });
    }

    // The hourglasses firing in a volley (every other one)
    std::vector<float> VolleyAngles(float base, Volley const& volley) const
    {
        std::vector<float> angles;
        for (uint32 index = volley.parity; index < Hourglasses; index += 2)
            angles.push_back(base + float(index) * float(M_PI) / 3.0f);
        return angles;
    }

    // Where a marked bot takes its seal's cone: close in front of it, a little aside, so that it runs along the wall
    // (a sun); across the arena, aside again, the three cones a pinwheel round him (a crescent)
    Position SablierBait(float angle, Volley const& volley) const
    {
        return volley.farthest ? P2At(angle + float(M_PI) + 0.8f * volley.side, 19.0f) :
            P2At(angle + 0.35f * volley.side, 19.0f);
    }

    // Its cone if its baiter stands there (a little wider: the bots keep clear of it)
    GroundIndicators::Area PredictedSablier(float angle, Volley const& volley) const
    {
        bool const farthest = volley.farthest;
        Position const apex = P2At(angle, HourglassAt);
        Position const bait = SablierBait(angle, volley);
        GroundIndicators::Area cone = MakeCircle(apex, farthest ? SablierFarLength : SablierNearLength);
        cone.kind = GroundIndicators::Area::Kind::Cone;
        cone.origin.SetOrientation(apex.GetAngle(&bait));
        cone.arc = ((farthest ? SablierFarArc : SablierNearArc) + 10.0f) * float(M_PI) / 180.0f;
        return cone;
    }

    // A volley's marks: a player for each of its seals - the real ones first (tanks too), then the bots nearest the
    // seals' spots - none already marked for a volley still to fire; the seal over their head, its beam to them. A
    // marked bot goes to its spot at once (SablierBait)
    void MarkSabliers(float base, Volley const& volley, uint32 lastsMs, std::vector<ObjectGuid>& marks)
    {
        uint32 const now = Elapsed();
        std::vector<Player*> reals;
        std::vector<Player*> bots;
        for (Player* player : ArenaPlayers())
            if (auto const busy = _sablierMarked.find(player->GetGUID()); busy == _sablierMarked.end() ||
                busy->second <= now)
                (IsBotPlayer(player) ? bots : reals).push_back(player);
        Acore::Containers::RandomShuffle(reals);
        std::vector<float> const angles = VolleyAngles(base, volley);
        Position const middle = Middle();
        marks.clear();
        for (uint32 index = 0; index < angles.size(); ++index)
        {
            Position const bait = SablierBait(angles[index], volley);
            // A real player first; else the bot nearest its spot (a random one walked 36 yd and was late: a tank only
            // if need be, never the one holding him)
            Player* player = nullptr;
            if (!reals.empty())
            {
                player = reals.back();
                reals.pop_back();
            }
            else
            {
                auto const cost = [this, &bait](Player* bot)
                {
                    return bot->GetExactDist2d(&bait) + (IsTank(bot) ? 40.0f : 0.0f) +
                        (bot == me->GetVictim() ? 1000.0f : 0.0f);
                };
                auto const nearest = std::min_element(bots.begin(), bots.end(), [&cost](Player* a, Player* b)
                {
                    return cost(a) < cost(b);
                });
                if (nearest == bots.end())
                    break;
                player = *nearest;
                bots.erase(nearest);
            }
            marks.push_back(player->GetGUID());
            _sablierMarked[player->GetGUID()] = now + lastsMs;
            GroundIndicators::ShowCarriedLook(player, volley.farthest ? LOOK_MARK_SEAL_SHADOW : LOOK_MARK_SEAL_LIGHT,
                lastsMs);
            Position seal = P2At(angles[index], HourglassAt);
            seal.m_positionZ += SablierSealHeight;
            GroundIndicators::ShowTetherFrom(me, seal, player, volley.farthest ? SPELL_SABLIER_LINK_MOON :
                SPELL_SABLIER_LINK_SUN, lastsMs);
            if (IsBotPlayer(player))
                GroundIndicators::SetUnitSpot(me, player, bait, 1.0f, lastsMs + 300);
            LOG_INFO(_logName, "Valerian sablier instance={} {} marked {} bot={} from {:.1f}yd of its spot at "
                     "{:.1f}yd/{:.0f}deg", me->GetInstanceId(), volley.farthest ? "crescent" : "sun", player->GetName(),
                     IsBotPlayer(player), player->GetExactDist2d(&bait), middle.GetExactDist2d(&bait),
                     middle.GetAbsoluteAngle(&bait) * 180.0f / float(M_PI));
        }
    }

    // The bots: each to the nearest of spots (a spot taken costs more: spread over them)
    void GuideNearest(std::vector<Player*> const& bots, std::vector<Position> const& spots, uint32 ms)
    {
        if (spots.empty())
            return;
        std::vector<uint32> taken(spots.size(), 0);
        for (Player* bot : bots)
        {
            size_t best = 0;
            float bestCost = 0.0f;
            for (size_t index = 0; index < spots.size(); ++index)
            {
                float const cost = bot->GetExactDist2d(&spots[index]) + float(taken[index]) * 6.0f;
                if (index == 0 || cost < bestCost)
                {
                    best = index;
                    bestCost = cost;
                }
            }
            ++taken[best];
            GroundIndicators::SetUnitSpot(me, bot, spots[best], 1.0f, ms);
        }
    }

    void SablierVolley(float base, Volley const& volley, uint32 tellMs, std::vector<ObjectGuid> const& marks)
    {
        float const arc = volley.farthest ? SablierFarArc : SablierNearArc;
        float const length = volley.farthest ? SablierFarLength : SablierNearLength;
        uint32 const look = volley.farthest ? LOOK_SABLIER_VIOLET : LOOK_SABLIER_GOLD;
        uint32 const layer = volley.farthest ? LOOK_SABLIER_VIOLET_WISPS : LOOK_SABLIER_GOLD_GLOW;
        bool const farthest = volley.farthest;
        struct SealCone
        {
            ObjectGuid stalker;
            Position apex;
            ObjectGuid marked;
        };
        auto cones = std::make_shared<std::vector<SealCone>>();
        std::vector<float> const angles = VolleyAngles(base, volley);
        for (uint32 index = 0; index < angles.size(); ++index)
        {
            Position const apex = P2At(angles[index], HourglassAt);
            ObjectGuid const marked = index < marks.size() ? marks[index] : ObjectGuid::Empty;
            ObjectGuid placed;
            GroundIndicators::ShowSeekingCone(me, apex, length, arc, tellMs, tellMs - SablierFreezeMs, farthest,
                SablierReach, look, layer, &placed, marked);
            cones->push_back({ placed, apex, marked });
        }
        Sound("Valerian.ReversCast", Middle());
        // Busy: every bot marked for a volley still to fire
        std::set<ObjectGuid> busy;
        for (auto const& [guid, until] : _sablierMarked)
            if (until > Elapsed())
                busy.insert(guid);
        GuideSabliers(angles, volley, busy, tellMs);
        // Held a second before it fires: where it points then, and at whom
        auto frozen = std::make_shared<std::vector<std::pair<GroundIndicators::Area, ObjectGuid>>>();
        After(tellMs - SablierFreezeMs, [this, cones, frozen, arc, length, farthest]()
        {
            Position const middle = Middle();
            for (auto const& [stalker, apex, marked] : *cones)
            {
                // Its marked player, while they live; else the nearest (farthest)
                Player* target = marked.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*me, marked);
                if (!target || !target->IsAlive())
                    target = GroundIndicators::SeekTarget(me, apex, farthest, SablierReach);
                Creature* drawn = me->GetMap()->GetCreature(stalker);
                float const facing = drawn ? drawn->GetOrientation() : target ? apex.GetAngle(target) :
                    apex.GetAngle(&middle);
                GroundIndicators::Area area = MakeCircle(apex, length);
                area.kind = GroundIndicators::Area::Kind::Cone;
                area.origin.SetOrientation(facing);
                area.arc = arc * float(M_PI) / 180.0f;
                GroundIndicators::WatchArea(me, area, SablierFreezeMs, MistakeDamage);
                if (target)
                    LOG_INFO(_logName, "Valerian sablier instance={} {} cone on {} bot={} at={:.1f}yd/{:.0f}deg",
                             me->GetInstanceId(), farthest ? "far" : "near", target->GetName(), IsBotPlayer(target),
                             middle.GetExactDist2d(target), middle.GetAbsoluteAngle(target) * 180.0f / float(M_PI));
                frozen->push_back({ area, target && !target->IsGameMaster() ? target->GetGUID() : ObjectGuid::Empty });
            }
        });
        After(tellMs, [this, frozen]()
        {
            std::vector<Position> spots;
            Areas areas;
            for (auto const& [area, target] : *frozen)
            {
                spots.push_back(BurstSpot(area));
                areas.push_back(area);
                ConeBlasts(area);
            }
            SoundNearest("Valerian.Revers", spots);
            if (_phase == Phase::Rehearsal)
            {
                LogSafe("sablier", areas);
                return;
            }
            for (auto const& [area, target] : *frozen)
                if (Player* player = ObjectAccessor::GetPlayer(*me, target))
                    Hit(player, SPELL_SABLIER, SablierTargetPct);
            for (Player* player : ArenaPlayers())
                for (auto const& [area, target] : *frozen)
                    if (player->GetGUID() != target && area.Contains(player->GetPosition()))
                    {
                        Mistake(player, RuleSablier);
                        break;
                    }
        });
    }

    // A narrow cone's strike: bursts along its axis, each as wide as the cone there (it never leaves its shape)
    void ConeBlasts(GroundIndicators::Area const& area)
    {
        float const facing = area.origin.GetOrientation();
        for (float along = area.radius * 0.15f; along < area.radius; along += area.radius * 0.22f)
            Blast(Ground(AtAngle(area.origin, facing, along)), std::max(1.5f, along * std::tan(area.arc / 2.0f)));
    }

    // The bots not baiting (nor walking to bait the next volley): where none of this volley's cones reaches, the tanks
    // with him
    void GuideSabliers(std::vector<float> const& angles, Volley const& volley, std::set<ObjectGuid> const& busy,
                       uint32 tellMs)
    {
        bool const farthest = volley.farthest;
        Areas predicted;
        for (float angle : angles)
            predicted.push_back(PredictedSablier(angle, volley));
        std::vector<Position> safe;
        for (float radius : { 3.0f, 5.0f, 8.0f, 11.0f })
            for (uint32 step = 0; step < 24; ++step)
            {
                Position const spot = P2At(float(step) * float(M_PI) / 12.0f, radius);
                bool reached = false;
                for (GroundIndicators::Area const& cone : predicted)
                    if (cone.Contains(spot, 2.0f) || (!farthest && spot.GetExactDist2d(&cone.origin) < 12.0f))
                        reached = true;
                if (!reached)
                    safe.push_back(spot);
            }
        std::vector<Player*> others;
        for (Player* bot : SortedBots())
            if (!IsTank(bot) && !busy.count(bot->GetGUID()))
                others.push_back(bot);
        GuideNearest(others, safe, tellMs + 300);
    }

    // --- Prémonition: visions of his strikes, then the strikes in reverse order --------------------------------------
    void Premonition(uint32 version)
    {
        uint32 const count = version == 0 ? 4 : version == 2 ? 6 : 5;
        uint32 const showMs = version == 2 ? 900 : 1100;
        uint32 const gapMs = version >= 2 ? 1100 : 1300;
        bool const mirrored = version == 1;
        float const facing = me->GetOrientation();
        // Each kind once, then more, never the same twice in a row
        std::vector<Vision> visions = { Vision::In, Vision::Out, Vision::Left, Vision::Right };
        Acore::Containers::RandomShuffle(visions);
        while (visions.size() < count)
        {
            Vision next = Vision(urand(0, 3));
            while (next == visions.back())
                next = Vision(urand(0, 3));
            visions.push_back(next);
        }
        CastBar(CAST_PREMONITION);
        for (uint32 index = 0; index < count; ++index)
        {
            Vision const vision = visions[index];
            After(PremonitionFirstMs + index * showMs, [this, vision, facing, showMs]()
            {
                ShowVision(vision, facing, showMs - 100);
                Sound("Valerian.VerseTell", Middle());
            });
        }
        uint32 const firstStrike = PremonitionFirstMs + count * showMs + PremonitionPauseMs;
        After(firstStrike - gapMs, [this]() { EndCastBar(); });
        // The strikes: the visions backwards (mirrored, the second time)
        std::vector<Vision> strikes;
        for (uint32 step = 0; step < count; ++step)
        {
            Vision vision = visions[count - 1 - step];
            if (mirrored && (vision == Vision::Left || vision == Vision::Right))
                vision = vision == Vision::Left ? Vision::Right : Vision::Left;
            strikes.push_back(vision);
        }
        // The bots: where the first strike will not reach as soon as the visions end (they cannot remember them), then
        // at each strike's tell where neither it nor the next reaches when there is such a place (1.3 s is short for
        // them: a whole half crossed in it caught them on the way)
        After(firstStrike - gapMs - PremonitionPauseMs, [this, strikes, facing, gapMs]()
        {
            GuidePremonition(strikes[0], strikes.size() > 1 ? &strikes[1] : nullptr, facing,
                PremonitionPauseMs + gapMs);
        });
        for (uint32 step = 0; step < count; ++step)
        {
            Vision const vision = strikes[step];
            std::optional<Vision> const next = step + 1 < count ? std::optional<Vision>(strikes[step + 1]) :
                std::nullopt;
            After(firstStrike + step * gapMs - gapMs, [this, vision, next, facing, gapMs]()
            {
                GroundIndicators::Area const area = PremonitionArea(vision, facing, gapMs);
                GuidePremonition(vision, next ? &*next : nullptr, facing, gapMs);
                After(gapMs, [this, area, vision]()
                {
                    Strike(Areas{ area }, RulePremonition);
                    Sound(vision == Vision::In || vision == Vision::Out ? "Valerian.Rage" : "Valerian.Wrath", Middle());
                });
            });
        }
    }

    float HalfFacing(Vision vision, float facing) const
    {
        return Position::NormalizeOrientation(facing + (vision == Vision::Left ? 1.0f : -1.0f) * float(M_PI) / 2.0f);
    }

    // A vision: drawn only (nothing to leave yet)
    void ShowVision(Vision vision, float facing, uint32 ms)
    {
        Position const middle = Middle();
        switch (vision)
        {
            case Vision::In:
                GroundIndicators::ShowDecal(me, middle, 0.0f, PremonitionInner, ms, LOOK_VISION_CIRCLE);
                GroundIndicators::ShowDecal(me, middle, 0.0f, PremonitionInner, ms, LOOK_VISION_GLOW_CIRCLE);
                break;
            case Vision::Out:
                GroundIndicators::ShowDecal(me, middle, 0.0f, P2Radius + 1.0f, ms, LOOK_VISION_RING_40);
                GroundIndicators::ShowDecal(me, middle, 0.0f, P2Radius + 1.0f, ms, LOOK_VISION_GLOW_RING_40);
                break;
            default:
                GroundIndicators::ShowDecal(me, middle, HalfFacing(vision, facing), P2Radius + 1.0f, ms,
                    LOOK_VISION_HALF);
                GroundIndicators::ShowDecal(me, middle, HalfFacing(vision, facing), P2Radius + 1.0f, ms,
                    LOOK_VISION_GLOW_HALF);
                break;
        }
    }

    // A strike as it comes: drawn and registered ms before it lands
    GroundIndicators::Area PremonitionArea(Vision vision, float facing, uint32 ms)
    {
        Position const middle = Middle();
        if (vision == Vision::In)
            return Painted(MakeCircle(middle, PremonitionInner), LOOK_DIVINE_CIRCLE, ms);
        GroundIndicators::Area area = MakeCircle(middle, P2Radius + 1.0f);
        if (vision == Vision::Out)
        {
            area.kind = GroundIndicators::Area::Kind::Ring;
            area.inner = PremonitionInner;
            return Painted(area, LOOK_DIVINE_RING_40, ms);
        }
        area.kind = GroundIndicators::Area::Kind::Cone;
        area.origin.SetOrientation(HalfFacing(vision, facing));
        area.arc = float(M_PI);
        return Painted(area, LOOK_DIVINE_CONE_180, ms);
    }

    // Whether a spot is margin yards clear of a strike
    bool ClearOf(Vision vision, float facing, Position const& spot, float margin) const
    {
        Position const middle = Middle();
        float const distance = middle.GetExactDist2d(&spot);
        switch (vision)
        {
            case Vision::In: return distance > PremonitionInner + margin;
            case Vision::Out: return distance < PremonitionInner - margin;
            default:
            {
                // Along the half's middle: negative on the other side
                float const half = HalfFacing(vision, facing);
                float const along = (spot.GetPositionX() - middle.GetPositionX()) * std::cos(half) +
                    (spot.GetPositionY() - middle.GetPositionY()) * std::sin(half);
                return along < -margin;
            }
        }
    }

    // The bots: spots clear of this strike and the next one if any such are, else of this one alone nearest to where
    // the next leaves free (round the edge between them), spread round him in a fixed order
    void GuidePremonition(Vision vision, Vision const* next, float facing, uint32 ms)
    {
        std::vector<Position> both;
        std::vector<std::pair<float, Position>> alone;
        for (float radius : { 3.5f, 5.0f, 7.0f, 8.0f, 12.0f, 13.5f, 16.0f })
            for (uint32 step = 0; step < 24; ++step)
            {
                Position const spot = P2At(float(step) * float(M_PI) / 12.0f, radius);
                if (!ClearOf(vision, facing, spot, 1.5f))
                    continue;
                if (!next || ClearOf(*next, facing, spot, 1.5f))
                    both.push_back(spot);
                else
                {
                    // How far it stands from where the next leaves free: round the edge of it
                    float cost = 0.0f;
                    for (float margin = 1.5f; margin > -20.0f && !ClearOf(*next, facing, spot, margin); margin -= 1.0f)
                        cost += 1.0f;
                    alone.push_back({ cost, spot });
                }
            }
        std::vector<Position> spots = both;
        if (spots.empty())
        {
            std::stable_sort(alone.begin(), alone.end(), [](auto const& a, auto const& b)
            {
                return a.first < b.first;
            });
            for (uint32 index = 0; index < alone.size() && index < 12; ++index)
                spots.push_back(alone[index].second);
        }
        GuideNearest(SortedBots(), spots, ms + 300);
    }

    // --- Chaînes du temps: chains the tanks must take before they break ----------------------------------------------
    void Chains(uint32 version)
    {
        uint32 const count = version ? 6 : 4;
        EndChains();
        uint32 tanks = 0;
        for (Player* player : ArenaPlayers())
            if (IsTank(player))
                ++tanks;
        _chainShare = tanks ? (count + tanks - 1) / tanks : count;
        for (Player* player : Targets(count))
        {
            _chains.push_back({ player->GetGUID(), GroundIndicators::ShowTether(me, me, player, SPELL_TIME_CHAIN_BEAM,
                ChainsMs) });
            AddTimedAura(player, SPELL_TIME_CHAIN, ChainsMs);
            GroundIndicators::ShowCarriedLook(player, LOOK_MARK_CHAIN, ChainsMs);
        }
        Sound("Valerian.Chained", Middle());
        for (uint32 at = ChainCheckMs; at < ChainsMs; at += ChainCheckMs)
            After(at, [this, at]() { UpdateChains(at); });
        After(ChainsMs, [this]() { BreakChains(); });
    }

    uint32 ChainsHeldBy(ObjectGuid guid) const
    {
        uint32 held = 0;
        for (TimeChain const& chain : _chains)
            if (chain.holder == guid)
                ++held;
        return held;
    }

    // A tank touching a chained player takes their chain (up to their share); the chained bots walk to a tank with room
    void UpdateChains(uint32 at)
    {
        uint32 const left = ChainsMs > at ? ChainsMs - at : 0;
        std::vector<Player*> tanks;
        for (Player* player : ArenaPlayers())
            if (IsTank(player))
                tanks.push_back(player);
        for (TimeChain& chain : _chains)
        {
            Player* holder = ObjectAccessor::GetPlayer(*me, chain.holder);
            if (!holder || !holder->IsAlive() || IsTank(holder))
                continue;
            Player* taker = nullptr;
            Player* nearest = nullptr;
            for (Player* tank : tanks)
            {
                if (ChainsHeldBy(tank->GetGUID()) >= _chainShare)
                    continue;
                if (tank->GetExactDist2d(holder) <= ChainTakeReach)
                    taker = tank;
                if (!nearest || tank->GetExactDist2d(holder) < nearest->GetExactDist2d(holder))
                    nearest = tank;
            }
            if (taker)
            {
                GroundIndicators::EndTether(me, chain.tether);
                holder->RemoveAurasDueToSpell(SPELL_TIME_CHAIN);
                GroundIndicators::ClearCarriedLook(holder, LOOK_MARK_CHAIN);
                chain.holder = taker->GetGUID();
                chain.tether = GroundIndicators::ShowTether(me, me, taker, SPELL_TIME_CHAIN_BEAM, left);
                AddTimedAura(taker, SPELL_TIME_CHAIN, left, uint8(ChainsHeldBy(taker->GetGUID())));
                LOG_INFO(_logName, "Valerian chain instance={} taken by {} from {}", me->GetInstanceId(),
                         taker->GetName(), holder->GetName());
                continue;
            }
            if (nearest && IsBotPlayer(holder))
                GroundIndicators::SetUnitSpot(me, holder, Ground(nearest->GetPosition()), 1.0f, ChainCheckMs * 3);
        }
        if (at >= ChainLineGraceMs && _phase != Phase::Rehearsal)
            BurnChainLines();
    }

    // Whoever else stands on a chain (past his own reach) is burnt
    void BurnChainLines()
    {
        Position const middle = Middle();
        uint32 const now = Elapsed();
        for (Player* player : ArenaPlayers())
        {
            if (ChainsHeldBy(player->GetGUID()))
                continue;
            if (auto const burned = _chainBurned.find(player->GetGUID()); burned != _chainBurned.end() &&
                burned->second > now)
                continue;
            for (TimeChain const& chain : _chains)
            {
                Player* holder = ObjectAccessor::GetPlayer(*me, chain.holder);
                if (!holder)
                    continue;
                float const dx = holder->GetPositionX() - middle.GetPositionX();
                float const dy = holder->GetPositionY() - middle.GetPositionY();
                float const length = std::sqrt(dx * dx + dy * dy);
                if (length < ChainLineFrom + 1.0f)
                    continue;
                float const px = player->GetPositionX() - middle.GetPositionX();
                float const py = player->GetPositionY() - middle.GetPositionY();
                float const along = (px * dx + py * dy) / length;
                float const across = std::fabs(px * dy - py * dx) / length;
                if (along > ChainLineFrom && along < length - 1.0f && across <= ChainLineWidth / 2.0f)
                {
                    _chainBurned[player->GetGUID()] = now + ChainBurnAgainMs;
                    Mistake(player, RuleChainLine);
                    break;
                }
            }
        }
    }

    void BreakChains()
    {
        Sound("Valerian.Chain", Middle());
        for (TimeChain const& chain : _chains)
        {
            Player* holder = ObjectAccessor::GetPlayer(*me, chain.holder);
            if (!holder || !holder->IsAlive())
                continue;
            GroundIndicators::PlayKit(me, Ground(holder->GetPosition()), KIT_HOLY_IMPACT, 0.8f, 0, ImpactMs);
            if (_phase == Phase::Rehearsal)
                continue;
            if (IsTank(holder))
                Hit(holder, SPELL_CHAIN_BREAK, ChainBreakPct);
            else
            {
                LOG_INFO(_logName, "Valerian chain instance={} broke on {} bot={}", me->GetInstanceId(),
                         holder->GetName(), IsBotPlayer(holder));
                Doom(holder, SPELL_CHAIN_BREAK);
            }
        }
        EndChains();
    }

    void EndChains()
    {
        for (TimeChain const& chain : _chains)
        {
            GroundIndicators::EndTether(me, chain.tether);
            if (Player* holder = ObjectAccessor::GetPlayer(*me, chain.holder))
            {
                holder->RemoveAurasDueToSpell(SPELL_TIME_CHAIN);
                GroundIndicators::ClearCarriedLook(holder, LOOK_MARK_CHAIN);
            }
        }
        _chains.clear();
        _chainBurned.clear();
    }

    // --- Mistakes ----------------------------------------------------------------------------------------------------
    // A share of the victim's own health (100: death); a second under Ébloui is death
    void Mistake(Player* player, Rule const& rule)
    {
        if (!player || !player->IsAlive())
            return;
        if (rule.percent >= 100.0f || (rule.dazzles && player->HasAura(SPELL_DAZZLED)))
            Doom(player, rule.spell);
        else
        {
            HitOwnHealth(player, rule.spell, rule.percent);
            if (player->IsAlive() && rule.dazzles)
                AddTimedAura(player, SPELL_DAZZLED, DazzleMs);
        }
        Position const middle = Middle();
        LOG_INFO(_logName, "Valerian rule broken instance={} at={:.1f}s {}: {} bot={} class={} tank={} "
                 "at={:.1f}yd/{:.0f}deg moving={}", me->GetInstanceId(), Elapsed() / 1000.0f, rule.name,
                 player->GetName(), player->GetSession() && player->GetSession()->IsBot(), player->getClass(),
                 IsTank(player), player->GetExactDist2d(&middle), middle.GetAbsoluteAngle(player) * 180.0f / float(M_PI),
                 player->isMoving());
    }

    void WatchDeaths()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && !player->IsGameMaster())
            {
                bool const dead = !player->IsAlive();
                if (dead && !_fallen.count(player->GetGUID()))
                {
                    ++_deaths[player->GetName()];
                    LOG_INFO(_logName, "Valerian death instance={} at={:.1f}s {}", me->GetInstanceId(),
                             Elapsed() / 1000.0f, player->GetName());
                }
                if (dead)
                    _fallen.insert(player->GetGUID());
                else
                    _fallen.erase(player->GetGUID());
            }
    }

    std::string DeathsText() const
    {
        std::string text;
        for (auto const& [name, count] : _deaths)
            text += Acore::StringFormat("{}{}x{}", text.empty() ? "" : " ", name, count);
        return text.empty() ? "none" : text;
    }

    void ResetFight()
    {
        GroundIndicators::DrawInstantly(me, false);
        scheduler.CancelAll();
        EndCastBar();
        EndPace();
        me->SetControlled(false, UNIT_STATE_ROOT);
        _summons.DespawnAll();
        _lights.clear();
        _jailers.clear();
        GroundIndicators::ClearAreasOf(me);
        _timeline.clear();
        _deaths.clear();
        _fallen.clear();
        _next = 0;
        _nextCheckMs = 0;
        _wallFrom = 0;
        _musicOver = false;
        _ascendFrom = 0;
        _ascendedMusicOver = false;
        EndChains();
        _sablierMarked.clear();
        _p3From = 0;
        _p3Timeline.clear();
        _p3Next = 0;
        _p3Pool = 0;
        _p3True = 0;
        _p3MusicOver = false;
        _trail.clear();
        _nextTrailMs = 0;
        _spawned = 0;
        _absorbed = 0;
        _taken = 0;
        _phase = Phase::None;
        me->SetVisible(DefiConfirmed());
        me->SetImmuneToAll(false);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
        HoldSpear(false);
        me->RemoveAurasDueToSpell(SPELL_LITANY_WARD);
        me->RemoveAurasDueToSpell(SPELL_OUT_OF_REACH);
        _wallRadius = ArenaRadius;
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        me->SetDisplayId(me->GetNativeDisplayId());
        SetSize(BaseScale);
        me->setPowerType(POWER_MANA);
        me->SetMaxPower(POWER_MANA, 100);
        me->SetPower(POWER_MANA, 0);
        StopClock();
    }

    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    size_t _next = 0;
    uint32 _nextCheckMs = 0;
    uint32 _wallFrom = 0;
    bool _musicOver = false;
    uint32 _ascendFrom = 0;             // when his apotheosis began (Elapsed): p2's track from then
    bool _ascendRehearsed = false;      // his apotheosis played in the lab
    uint32 _ascentFrom = 0;             // when his apotheosis' rise began (Elapsed), 0 once he is down
    float _height = 0.0f;
    float _dropFrom = -1.0f;            // his height as his drop began (-1: not yet)
    float _wallRadius = ArenaRadius;    // the wall now: phase 1's, then pushed back by his god's blow
    // Phase 2
    std::vector<P2Step> _p2Timeline;
    size_t _p2Next = 0;
    uint32 _p2Pool = 0;                 // his phase 2 health (half his bar)
    uint32 _p2True = 0;                 // what of it the group has not dealt yet (the bar shows the pace above it)
    float _starsBase = 0.0f;
    float _pizzaBase = 0.0f;
    float _eclipseWay = 1.0f;               // the first Éclipse's holes' way round (the second goes the other)
    std::vector<ObjectGuid> _catchers;      // Ascension's wave: the bots taking its orbs
    std::map<ObjectGuid, uint32> _ascensionSlots;   // Ascension: each bot's place (GuideAscension)
    float _eclipseAngle = 0.0f;         // the first Éclipse's holes, the second's on the other side
    uint32 _totalLightAt = 0;
    uint32 _totalLightRehearsedUntil = 0;   // the lab's Lumière totale (.valerian cast totallight): until then
    struct Orb
    {
        ObjectGuid guid;
        float angle;
        float distance;
    };
    std::vector<Orb> _orbs;
    float _size = 0.0f;                 // his size as set (SetSize), kept against the core's
    uint32 _swapHeldUntil = 0;          // no tank swap before then (Revers sacré's back cone)
    std::set<ObjectGuid> _litanyStruck;  // La Litanie: the verses struck so far (free to move)
    bool _litanyProfaned = false;
    bool _ascendedMusicOver = false;
    std::vector<Light> _lights;
    std::vector<ObjectGuid> _jailers;
    uint32 _lightTimer = 0;
    float _lastLightAngle = 0.0f;
    uint32 _spawned = 0;
    uint32 _absorbed = 0;
    uint32 _taken = 0;
    std::map<std::string, uint32> _deaths;
    std::set<ObjectGuid> _fallen;
    // Phase 3
    uint32 _p3From = 0;                 // when its music started (Elapsed): P3Ms from then
    std::vector<P3Step> _p3Timeline;
    size_t _p3Next = 0;
    uint32 _p3Pool = 0;                 // his phase 3 health (the whole bar)
    uint32 _p3True = 0;
    bool _p3MusicOver = false;
    std::map<ObjectGuid, std::deque<std::pair<uint32, Position>>> _trail;  // phase 2: where everyone has been
    uint32 _nextTrailMs = 0;
    std::vector<TimeChain> _chains;
    uint32 _chainShare = 1;             // the chains a tank may hold
    std::map<ObjectGuid, uint32> _chainBurned;  // standing on a chain: not burnt again before then
    std::map<ObjectGuid, uint32> _sablierMarked;    // Les Sabliers: marked until (Elapsed)
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .valerian info | skip <seconds> | cast <pattern> | lab [off] | pull | measure | look <display> [scale] | kit <id> |
// pose <emote state>
class ValerianCommandScript final : public CommandScript
{
public:
    ValerianCommandScript() : CommandScript("ValerianCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable valerianTable = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "skip", HandleSkip, SEC_GAMEMASTER, Console::No },
            { "cast", HandleCast, SEC_GAMEMASTER, Console::No },
            { "lab", HandleLab, SEC_GAMEMASTER, Console::No },
            { "pull", HandlePull, SEC_GAMEMASTER, Console::No },
            { "measure", HandleMeasure, SEC_GAMEMASTER, Console::No },
            { "look", HandleLook, SEC_GAMEMASTER, Console::No },
            { "kit", HandleKit, SEC_GAMEMASTER, Console::No },
            { "pose", HandlePose, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "valerian", valerianTable },
        };
        return commandTable;
    }

    static boss_valerian* FindValerian(Player* player)
    {
        Creature* valerian = player ? player->FindNearestCreature(NPC_VALERIAN, 250.0f) : nullptr;
        return valerian ? CAST_AI(boss_valerian, valerian->AI()) : nullptr;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_valerian* valerian = FindValerian(handler->GetPlayer());
        handler->SendSysMessage(valerian ? valerian->Describe() : std::string("Valerian is not within 250 yards."));
        return true;
    }

    // .valerian look <display> [scale]: him in another look (0: his own) and size, to compare (the FX lab)
    static bool HandleLook(ChatHandler* handler, uint32 display, Optional<float> scale)
    {
        Player* player = handler->GetPlayer();
        Creature* valerian = player ? player->FindNearestCreature(NPC_VALERIAN, 250.0f) : nullptr;
        if (!valerian)
        {
            handler->SendErrorMessage("Valerian is not within 250 yards.");
            return false;
        }
        valerian->SetDisplayId(display ? display : valerian->GetNativeDisplayId());
        if (scale)
            CAST_AI(boss_valerian, valerian->AI())->LookSize(*scale);
        return true;
    }

    // .valerian pose <emote state>: him held in that pose (0: none), to compare (the FX lab)
    static bool HandlePose(ChatHandler* handler, uint32 state)
    {
        Player* player = handler->GetPlayer();
        Creature* valerian = player ? player->FindNearestCreature(NPC_VALERIAN, 250.0f) : nullptr;
        if (!valerian)
        {
            handler->SendErrorMessage("Valerian is not within 250 yards.");
            return false;
        }
        valerian->SetUInt32Value(UNIT_NPC_EMOTESTATE, state);
        return true;
    }

    // .valerian kit <SpellVisualKit id>: a kit played on him (a candidate look, compared in the FX lab)
    static bool HandleKit(ChatHandler* handler, uint32 kit)
    {
        Player* player = handler->GetPlayer();
        Creature* valerian = player ? player->FindNearestCreature(NPC_VALERIAN, 250.0f) : nullptr;
        if (!valerian)
        {
            handler->SendErrorMessage("Valerian is not within 250 yards.");
            return false;
        }
        valerian->SendPlaySpellVisual(kit);
        return true;
    }

    static bool HandleMeasure(ChatHandler* handler)
    {
        boss_valerian* valerian = FindValerian(handler->GetPlayer());
        handler->SendSysMessage(valerian ? valerian->Measure() : std::string("Valerian is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_valerian* valerian = FindValerian(handler->GetPlayer());
        if (!valerian || !valerian->Skip(seconds))
        {
            handler->SendErrorMessage("Valerian is not fighting within 250 yards.");
            return false;
        }
        handler->SendSysMessage(valerian->Describe());
        return true;
    }

    // .valerian cast <revers|wrath|wrath2|chain|fervor|fervor2|rage|rage2|lances|burst|judgement|litany|litany2|lights|
// light|ascend>
    static bool HandleCast(ChatHandler* handler, std::string step)
    {
        boss_valerian* valerian = FindValerian(handler->GetPlayer());
        if (!valerian || !valerian->CastNow(step))
        {
            handler->SendErrorMessage("No such pattern, or Valerian is neither fighting nor rehearsing within 250 "
                                      "yards.");
            return false;
        }
        return true;
    }

    // .valerian lab: in the FX lab, Valérian at its middle in rehearsal (his arena up, harmless); .valerian lab off: gone
    static bool HandleLab(ChatHandler* handler, Optional<std::string> off)
    {
        Player* player = handler->GetPlayer();
        if (!player || player->GetMapId() != MapFxLab)
        {
            handler->SendErrorMessage("In the FX lab only (.fxlab).");
            return false;
        }
        if (Creature* existing = player->FindNearestCreature(NPC_VALERIAN, 250.0f))
            existing->DespawnOrUnsummon(0ms);
        if (off && *off == "off")
            return true;
        TempSummon* valerian = player->SummonCreature(NPC_VALERIAN, LabMiddle, TEMPSUMMON_MANUAL_DESPAWN);
        if (!valerian)
            return false;
        if (boss_valerian* ai = CAST_AI(boss_valerian, valerian->AI()))
            ai->Rehearse();
        handler->SendSysMessage("Valerian rehearses at the lab's middle: .valerian cast <pattern>.");
        return true;
    }

    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* valerian = player ? player->FindNearestCreature(NPC_VALERIAN, 150.0f) : nullptr;
        if (!valerian || !valerian->IsAlive())
        {
            handler->SendErrorMessage("No living Valerian within 150 yards.");
            return false;
        }
        valerian->SetReactState(REACT_AGGRESSIVE);
        valerian->AI()->AttackStart(player);
        handler->SendSysMessage("Valerian pulled.");
        return true;
    }
};
}

void AddZealotBossScripts()
{
    RegisterCreatureAI(boss_valerian);
    new ValerianCommandScript();
}
