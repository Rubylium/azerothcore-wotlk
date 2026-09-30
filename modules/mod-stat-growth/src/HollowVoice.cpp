#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"

#include "CellImpl.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ModelIgnoreFlags.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PowerScaling.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// The Hollow Voice, the pinnacle of the Défi board: a 10-player fight of the board's own in Sunwell Plateau's M'uru
// chamber, for item level 460 and 650 paragon (plan: .agents/plans/hollow-voice). Like L'Infini (InfiniteGod.cpp) it
// runs on a fixed timeline set by its two tracks, played as one (patch-Z Sound\Music\Evolutions\HollowVoice.mp3,
// SoundEntries 30112; localTools/hollowVoice/buildMusic.py joins them):
//
// 0:00 Archbishop Aldric Dawnmantle (his track, up to 2:00.0 at the end of its fade). His health stops at 1%.
//      0:00-0:43 Judgement on his tank (Condemned: holy damage taken), Light of Dawn (a cone), Consecrated Aisles
//      (every other lane burns), Blessed Hammers (three hammers spiralling out), Wrath of the Pulpit (rings of holy
//      fire from his feet), Holy Radiance on everyone.
//      0:43.3 the first climax: Choir of the Faithful, twice - three golden towers for two players each, and a Bastion
//      tower only a tank may hold.
//      1:00.7 the break: Prayer of Absolution - he kneels under an aegis; break it in 9 s or he heals.
//      1:09.6 the second build: Execution Sentence (a sentence shared by those standing with the marked), and at 1:25.5
//      Verdict of the Faithful - a giant hammer a tank must take.
//      1:36 Seraphim: his wings, everything faster, Wake of Ashes (cones of light sweeping in front of him).
//      1:57.5 the verdict: not at 1%, Last Rites - a holy judgement kills the group on his track's last bars, and the
//      demon is never seen nor heard; at 1%, he falls as his track fades (1:59.7).
// 2:00.0 Vel'thazar's track (5:30) follows; on its first hit (2:01.5) Vel'thazar, the Hollow Voice, tears out of the
//      Archbishop, who stays hidden. Killing the demon frees and kills the Archbishop: his kill is the board's win
//      (RaidFinder follows the board's boss, 930100).
//      2:03-3:03 Vampiric Brand on his tank (he feeds on a tank marked three times), Carrion Swarm (a cone), Hollow
//      Echo (the Archbishop's aisles and rings come back inverted), and on the 0:47 hit Whisper of Doubt (three
//      carried circles).
//      3:03 Last Light: the void pulses, three shrinking pools of the Archbishop's light are the only shelter.
//      3:17.6 Nightmare Lances (a void lance to each of two marked), and on the 1:43.5 hit two Dread Infernals crash
//      (each impact shared) and fight on.
//      4:01 Inhale of the Void: he draws everyone in while void falls; on the drop (4:07.8) all near him are struck.
//      4:07.8 his true form: bigger, the chamber's edge devoured for the rest of the fight; three swarms spin round
//      him.
//      4:41 Aldric's Last Prayer: the Archbishop fights from inside - the demon is imprisoned, four lights of his
//      stand in the chamber: carried into the demon, each cracks the prison and blesses its carrier. Not broken by
//      the climax: a void blast on everyone for each light left.
//      5:12.4 the loudest climax: Hollow Sermon (void towers, a Bastion tower), crosses of void, the swarms.
//      6:15 the dark section: Voice of Ruin on the three stabs (6:27, 6:44, 6:59) - near death for everyone, halved
//      under Aldric's last Aegis, held by a tank.
// 7:17.1 / 7:17.7 / 7:18.1 (the track's BAM BAM BAM, 5:17.1-5:18.1): the hard enrage, three blasts of three times
//      everyone's health, then a pulse every second that kills whatever protects them.
//
// Every avoidable hit is drawn first (GroundIndicators, the red; a sigil of the user's under a soak) and resolved on
// the very area drawn; bots read the same areas, soak what asks for soakers, and send a tank to a Bastion. Damage is
// a share of the health of a damage dealer at the profile (Reference), the Défi tier's factor on top.
//
// Only in a challenge's instance: the static spawn hides at once and goes from any other Sunwell
// (IsChallengeInstanceFor, mod-playerbots RaidFinder.cpp), and clears M'uru's chamber of its own occupants.

// mod-playerbots (RaidFinder.cpp), built into the same modules library
bool IsChallengeInstanceFor(Map const* map, uint32 bossEntry);
float GetChallengeDamageFactorOf(Unit* attacker);

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// The profile it is made for (mod-playerbots ChallengeTiers.h BossProfiles): hits are shares of the health of a
// damage dealer at it (Power::ExpectedPlayerHealth, about 221 000; a tank has about 321 000), the Défi tier's damage
// factor on top
constexpr float ProfileItemLevel = 460.0f;
constexpr float ProfileParagon = 650.0f;
constexpr float AldricMeleeFloorPct = 10.0f;    // his melee on a player: at least this, whatever their armour
constexpr float VelthazarMeleeFloorPct = 14.0f;
constexpr float InfernalMeleeFloorPct = 9.0f;
constexpr float HoldHealthPct = 1.0f;           // the Archbishop's health stops here
constexpr float EnrageBlastHealthPct = 300.0f;  // each of the three blasts, of the target's maximum health
constexpr uint32 EnragePulseMs = 1000;          // then a pulse that kills, whatever protects them
constexpr uint32 FinishPulses = 3;              // the bots left alone: pulses, the last one kills them
constexpr uint32 FinishPulseMs = 1000;
constexpr Milliseconds WipeLinger = 8s;         // a wipe: it stands this long, the music on, before it goes
constexpr Seconds WipeRespawnDelay = 5s;
constexpr uint32 DefiGraceMs = 8000;            // players are waited for this long before a non-challenge's goes

// The Archbishop's
constexpr float RadiancePct = 8.0f;             // Holy Radiance on everyone...
constexpr float RadianceSeraphimPct = 12.0f;    // ... with his wings
constexpr float JudgementPct = 60.0f;           // on his tank, before Condemned
constexpr float CondemnedPerStackPct = 15.0f;   // holy damage taken, a stack
constexpr uint32 CondemnedMs = 16000;
constexpr uint32 CondemnedMaxStacks = 5;
constexpr float LightOfDawnPct = 70.0f;
constexpr float HammerPct = 45.0f;              // a blessed hammer crossing someone (once a hammer)
constexpr float PulpitPct = 70.0f;              // a ring of Wrath of the Pulpit
constexpr float AislePct = 80.0f;               // a burning aisle
constexpr float TowerSharedPct = 120.0f;        // a Choir tower, split between its soakers (two or more)...
constexpr float TowerFailPct = 45.0f;           // ... one held by fewer: everyone, for each such tower
constexpr float BastionTankPct = 60.0f;         // the Bastion tower on the tank holding it...
constexpr float BastionOtherPct = 100.0f;       // ... on anyone else in it...
constexpr float BastionFailPct = 70.0f;         // ... no tank in it: everyone
constexpr float AbsolutionShieldPct = 5.0f;     // Prayer of Absolution: the aegis, of his maximum health...
constexpr float AbsolutionHealPct = 8.0f;       // ... not broken in time: he heals this
constexpr float SentencePct = 240.0f;           // Execution Sentence, split between all in its circle
constexpr float VerdictTankPct = 110.0f;        // Verdict of the Faithful on the tank taking it...
constexpr float VerdictOtherPct = 200.0f;       // ... on anyone else in it...
constexpr float VerdictFailPct = 100.0f;        // ... no tank in it: everyone
constexpr float WakePct = 75.0f;                // a cone of Wake of Ashes

// Vel'thazar's
constexpr float HollowPulsePct = 8.0f;          // Hollow Pulse on everyone...
constexpr float HollowPulseTruePct = 10.0f;     // ... from his true form
constexpr float RevealPulsePct = 10.0f;         // tearing out of the Archbishop
constexpr float BrandPct = 70.0f;               // Vampiric Brand on his tank, a tenth more a stack
constexpr float BrandPerStackPct = 10.0f;
constexpr uint32 BrandMs = 18000;
constexpr uint32 BrandFeedStacks = 3;           // from this many he feeds on the hit...
constexpr float BrandHealPct = 1.0f;            // ... healing this share of his maximum health
constexpr float SwarmPct = 80.0f;               // Carrion Swarm
constexpr float EchoPct = 75.0f;                // Hollow Echo's inverted aisles and rings
constexpr float WhisperCarrierPct = 20.0f;      // Whisper of Doubt, on the marked...
constexpr float WhisperOtherPct = 90.0f;        // ... on anyone else in their circle
constexpr float LastLightOutsidePct = 30.0f;    // Last Light, every 2 s, out of the pools...
constexpr float LastLightInsidePct = 3.0f;      // ... in one
constexpr float LancePct = 80.0f;               // Nightmare Lances, on anyone on the line...
constexpr float LanceMarkedPct = 25.0f;         // ... on the marked
constexpr float InfernalSharedPct = 150.0f;     // a Dread Infernal's impact, split between its soakers (two or more)...
constexpr float InfernalFailPct = 50.0f;        // ... fewer: everyone
constexpr float InhaleVoidPct = 50.0f;          // the void falling while he inhales
constexpr float InhaleBlastPct = 150.0f;        // the drop, on anyone near him
constexpr float TrueFormPulsePct = 15.0f;
constexpr float EdgePct = 30.0f;                // each second on the devoured edge
constexpr float SpinSwarmPct = 60.0f;           // a spinning swarm crossing someone (every 1.5 s at most)
constexpr float PrayerPulsePct = 6.0f;          // Aldric's Last Prayer, every 3 s
constexpr float PrayerFailPct = 50.0f;          // the prison not broken: everyone, for each light left
constexpr float SermonSharedPct = 130.0f;       // a void tower of the Hollow Sermon, split...
constexpr float SermonFailPct = 50.0f;          // ... held by fewer than two: everyone, for each
constexpr float VoidCrossPct = 70.0f;
constexpr float RuinPct = 140.0f;               // Voice of Ruin...
constexpr float RuinAegisShare = 0.5f;          // ... under the Aegis a tank holds

// Sizes (yards) and warnings (ms)
constexpr float LightOfDawnRadius = 40.0f;
constexpr float LightOfDawnArc = 60.0f;
constexpr uint32 LightOfDawnWarningMs = 2500;
constexpr uint32 HammerCount = 3;
constexpr uint32 HammerMs = 8000;
constexpr float HammerRadius = 2.5f;            // a hammer hits whoever it passes this close to
constexpr float HammerStart = 7.0f;             // from his feet (past his melee: a first touch nobody could dodge)...
constexpr float HammerGrowth = 4.4f;            // ... outward, yards a second...
constexpr float HammerTurn = 0.85f;             // ... turning, radians a second
constexpr float HammerHeight = 1.4f;
constexpr uint32 PulpitWaveMs = 1500;
constexpr uint32 PulpitWarningMs = 2500;
// Its rings, all drawn at once and landing one after the other, with a safe band between each (in the drawn ring
// proportions, GroundIndicators ShowRing): under 8.4 yards (his melee), 14-20.8 and 26-32. Drawn one at a time, two
// of them overlapped and the bots stepping out of one walked into the next.
constexpr std::array<std::pair<float, float>, 3> PulpitRings = { { { 14.0f, 8.4f }, { 26.0f, 20.8f },
                                                                   { 40.0f, 32.0f } } };
constexpr float EchoCoreRadius = 9.0f;          // the inverted rings: the core and all past EchoOuterInner burn
constexpr float EchoOuterInner = 18.0f;
constexpr uint32 EchoWarningMs = 3000;
constexpr float AisleWidth = 8.0f;
constexpr uint32 AisleCount = 10;
constexpr float AisleLength = 90.0f;
constexpr uint32 AisleWarningMs = 3500;
constexpr float TowerRadius = 4.5f;
constexpr uint32 TowerMs = 7000;
constexpr uint32 AbsolutionMs = 9000;
constexpr float AbsolutionSigilRadius = 8.0f;
constexpr uint32 SentenceMs = 6000;
constexpr float SentenceRadius = 7.0f;
constexpr uint32 SentenceSoakersBots = 4;
constexpr uint32 VerdictMs = 5000;
constexpr float VerdictRadius = 5.0f;
constexpr uint32 WakeCones = 3;
constexpr uint32 WakeStepMs = 1000;
constexpr uint32 WakeWarningMs = 2000;
constexpr float WakeRadius = 40.0f;
constexpr float WakeArc = 60.0f;
constexpr float SwarmRadius = 40.0f;
constexpr float SwarmArc = 50.0f;
constexpr uint32 SwarmWarningMs = 2500;
constexpr uint32 WhisperCarriers = 3;
constexpr uint32 WhisperMs = 6000;
constexpr float WhisperRadius = 8.0f;
constexpr uint32 LastLightPools = 3;
constexpr float LastLightPoolDistance = 20.0f;
constexpr std::array<float, 3> LastLightPoolRadii = { 7.0f, 5.5f, 4.5f };
constexpr uint32 LastLightShrinkMs = 5000;
constexpr uint32 LanceMarked = 2;
constexpr uint32 LanceMs = 5000;
constexpr float LanceLength = 60.0f;
constexpr float LanceWidth = 4.0f;
constexpr uint32 InfernalCount = 2;
constexpr uint32 InfernalWarningMs = 4000;
constexpr float InfernalRadius = 6.0f;
constexpr uint32 InfernalSoakersBots = 3;
constexpr float InhaleBlastRadius = 15.0f;
constexpr float InhalePullSpeed = 6.0f;
constexpr float InhaleVoidRadius = 5.0f;
constexpr uint32 InhaleVoidWarningMs = 1500;
constexpr float TrueFormScale = 1.35f;
constexpr float EdgeOuterRadius = 50.0f;        // the devoured edge, from the true form on
constexpr float EdgeInnerRadius = 30.0f;
constexpr uint32 SpinSwarmArms = 3;
constexpr uint32 SpinSwarmMs = 8000;
constexpr float SpinSwarmLength = 36.0f;
constexpr float SpinSwarmWidth = 7.0f;
constexpr float SpinSwarmTurn = 0.4f;           // radians a second
constexpr float SpinSwarmCore = 5.0f;           // under his model: the swarms pour out past it
constexpr uint32 SpinSwarmHitEveryMs = 1500;
constexpr uint32 LaserTickMs = 100;
constexpr uint32 PrayerLights = 4;
constexpr float PrayerLightDistance = 24.0f;
constexpr float PrayerLightRadius = 3.0f;
constexpr uint32 PrayerCarryMs = 2500;          // a light picked up reaches the demon this long after
constexpr float CrossLength = 42.0f;
constexpr float CrossWidth = 6.0f;
constexpr uint32 CrossWaves = 2;
constexpr uint32 CrossWaveMs = 2300;
constexpr uint32 CrossWarningMs = 2500;
constexpr uint32 RuinWarningMs = 7000;
constexpr float RuinAegisRadius = 8.0f;

// M'uru's chamber: round, 39 yards to its walls round the Archbishop's spot (.hollow floor), its floor 69.6 in the
// middle and 71.2 at the edge
constexpr float ArenaReach = 45.0f;
constexpr float ArenaWalkRadius = 36.0f;        // random spots stay this close to the middle
constexpr float MusicReach = 120.0f;
constexpr float ChamberClearRadius = 60.0f;
// What is painted on the chamber's floor is carried at its mid height: a painting shows 2 yards above and below its
// carrier, and the floor goes from 69.6 (the middle) to 71.2 (the edge). Carried at the ground found under its start,
// a lane starting past the walls found another floor below and was never seen.
constexpr float ChamberFloorZ = 70.4f;     // the chamber's own occupants this close go (M'uru, its guards)

// --- Timeline (ms from the pull), on the tracks' beats (measured on their loudness) ---------------------------------
// The verdict: a group not at 1% by then gets Last Rites, and the track is ended there - the client fades a track
// out over a few seconds, and the demon's part (AtSecondTrack) must not be heard in that fade. At 1%, he falls on
// the fade (AtFall).
constexpr uint32 AtLastRites = 117500;
constexpr uint32 AtFall = 119700;               // the Archbishop's track fades out
// Vel'thazar's track follows at the end of his fade (1:59.7-2:00.0), in the one file. Sent as a second music, it
// never played through: the first reaching its own end stopped it (sent at 2:03.45: not heard; at 2:02.0: cut a few
// seconds in, 2026-09-30). At 2:00.0 rather than 2:02.0 (as asked): no gap of silence between the two.
constexpr uint32 AtSecondTrack = 120000;
constexpr uint32 RevealTestStartMs = 110000;    // a pull armed with .hollow reveal starts here, the Archbishop at 1%
// A wipe before the demon's part ends the track at least this long before it: the client fades a track out over a
// few seconds, and the demon's part must not be heard in that fade (it would spoil him)
constexpr uint32 TrackEndMarginMs = 5000;
constexpr uint32 AtReveal = AtSecondTrack + 1500;               // its first hit
// The second track's sections, from its start
constexpr uint32 T2(uint32 ms) { return AtSecondTrack + ms; }
constexpr uint32 AtAbsolution = 60700;
constexpr uint32 AtAbsolutionEnd = AtAbsolution + AbsolutionMs;
constexpr uint32 AtVerdict = 85500;             // the hammer lands on the hit
constexpr uint32 AtSeraphim = 96000;
constexpr uint32 AtWhisper = T2(47000);
constexpr uint32 AtLastLight = T2(61000);
constexpr uint32 AtLastLightEnd = T2(75600);
constexpr uint32 AtInfernals = T2(103500);      // the impacts land on the hit
constexpr uint32 AtInhale = T2(119000);
constexpr uint32 AtTrueForm = T2(125800);       // the big drop
constexpr uint32 AtLastPrayer = T2(159000);
constexpr uint32 AtLastPrayerEnd = T2(190400);  // the loudest climax
constexpr uint32 AtRuinSection = T2(253000);
constexpr std::array<uint32, 3> AtRuins = { T2(265000), T2(282000), T2(297000) };
constexpr std::array<uint32, 3> AtEnrageBlasts = { T2(317090), T2(317650), T2(318100) };

constexpr uint32 NPC_ALDRIC = 930100;
constexpr uint32 NPC_VELTHAZAR = 930101;
constexpr uint32 NPC_INFERNAL = 930102;
constexpr uint32 NPC_STALKER = 900104;          // GroundIndicators' invisible stalker: kits played on the ground
constexpr uint32 NPC_HOVER_STALKER = 15214;     // a stock one that flies

// SoundEntries (localTools/patchSinisterStrike.ps1): the fight's track, the same from 1:50 (a pull armed with .hollow
// reveal), the two alone (.hollow music), L'Infini's silence to end them, the abilities' sounds. A music sent over
// another fades out a few seconds in: the fight sends one track, and a test starting late its own.
constexpr uint32 MUSIC_FIGHT = 30112;
constexpr uint32 MUSIC_FIGHT_FROM_REVEAL = 30113;
constexpr uint32 MUSIC_ALDRIC = 30110;
constexpr uint32 MUSIC_VELTHAZAR = 30111;
constexpr uint32 MUSIC_SILENCE = 30101;
enum Sounds : uint32
{
    SOUND_JUDGEMENT = 30120,
    SOUND_BLESSED_HAMMERS,
    SOUND_WRATH_OF_THE_PULPIT,
    SOUND_CONSECRATED_AISLES,
    SOUND_LIGHT_OF_DAWN,
    SOUND_CHOIR,
    SOUND_ABSOLUTION,
    SOUND_EXECUTION_SENTENCE,
    SOUND_VERDICT,
    SOUND_SERAPHIM,
    SOUND_WAKE_OF_ASHES,
    SOUND_LAST_RITES,
    SOUND_REVEAL,
    SOUND_CARRION_SWARM,
    SOUND_VAMPIRIC_BRAND,
    SOUND_HOLLOW_ECHO,
    SOUND_NIGHTMARE_LANCES,
    SOUND_VOICE_OF_RUIN,
    SOUND_HARD_ENRAGE,
};

// The combat log's names: the fight's own spell once the patch has it (localTools/hollowVoice/Spells.ps1), a stock
// spell of the same school until then
struct NamedSpell
{
    uint32 custom;
    uint32 stock;
};
constexpr uint32 STOCK_HOLY = 48817;            // Holy Wrath
constexpr uint32 STOCK_SHADOW = 47809;          // Shadow Bolt
constexpr NamedSpell SPELL_JUDGEMENT = { 94000, STOCK_HOLY };
constexpr NamedSpell SPELL_HAMMERS = { 94002, STOCK_HOLY };
constexpr NamedSpell SPELL_PULPIT = { 94003, STOCK_HOLY };
constexpr NamedSpell SPELL_AISLES = { 94004, STOCK_HOLY };
constexpr NamedSpell SPELL_LIGHT_OF_DAWN = { 94005, STOCK_HOLY };
constexpr NamedSpell SPELL_CHOIR = { 94006, STOCK_HOLY };
constexpr NamedSpell SPELL_SENTENCE = { 94008, STOCK_HOLY };
constexpr NamedSpell SPELL_VERDICT = { 94009, STOCK_HOLY };
constexpr NamedSpell SPELL_WAKE = { 94011, STOCK_HOLY };
constexpr NamedSpell SPELL_LAST_RITES = { 94012, STOCK_HOLY };
constexpr NamedSpell SPELL_RADIANCE = { 94013, STOCK_HOLY };
constexpr NamedSpell SPELL_SWARM = { 94020, STOCK_SHADOW };
constexpr NamedSpell SPELL_BRAND = { 94021, STOCK_SHADOW };
constexpr NamedSpell SPELL_ECHO = { 94023, STOCK_SHADOW };
constexpr NamedSpell SPELL_WHISPER = { 94024, STOCK_SHADOW };
constexpr NamedSpell SPELL_LAST_LIGHT = { 94025, STOCK_SHADOW };
constexpr NamedSpell SPELL_LANCES = { 94026, STOCK_SHADOW };
constexpr NamedSpell SPELL_INFERNAL = { 94027, STOCK_SHADOW };
constexpr NamedSpell SPELL_INHALE = { 94028, STOCK_SHADOW };
constexpr NamedSpell SPELL_EDGE = { 94029, STOCK_SHADOW };
constexpr NamedSpell SPELL_SERMON = { 94033, STOCK_SHADOW };
constexpr NamedSpell SPELL_RUIN = { 94034, STOCK_SHADOW };
constexpr NamedSpell SPELL_HOLLOW_PULSE = { 94036, STOCK_SHADOW };
constexpr NamedSpell SPELL_SILENCE = { 94037, STOCK_SHADOW };
constexpr NamedSpell SPELL_VOID_CROSS = { 94038, STOCK_SHADOW };
constexpr NamedSpell SPELL_TEAR = { 94039, STOCK_SHADOW };
constexpr NamedSpell SPELL_SPIN_SWARM = { 94040, STOCK_SHADOW };
// What the players and the bosses wear: dummy auras (nothing without the patch)
constexpr uint32 SPELL_CONDEMNED = 94001;
constexpr uint32 SPELL_ABSOLUTION_AURA = 94007;
constexpr uint32 SPELL_SERAPHIM_AURA = 94010;
constexpr uint32 SPELL_SENTENCE_MARK = 94014;
constexpr uint32 SPELL_HAMMER_FX = 94015;
constexpr uint32 SPELL_LIGHT_FX = 94016;
constexpr uint32 SPELL_BRAND_STACKS = 94022;
constexpr uint32 SPELL_PRISON_AURA = 94030;
constexpr uint32 SPELL_BLESSING = 94031;
constexpr uint32 SPELL_CARRY_LIGHT = 94032;
constexpr uint32 SPELL_TRUE_FORM = 94041;
// Its abilities painted on the ground in place of the red (localTools/hollowVoice/textures, shapes.json kind texture:
// each painting warped to the area it marks). A warning breathes; its "hit" twin, at full strength, flashes where it
// lands (StrikeMs). The rings and round ones turn slowly.
enum Paint : uint32
{
    PAINT_AISLES = 90769, PAINT_AISLES_HIT,
    PAINT_PULPIT60, PAINT_PULPIT60_HIT,
    PAINT_PULPIT80, PAINT_PULPIT80_HIT,
    PAINT_DAWN, PAINT_DAWN_HIT,
    PAINT_WAKE, PAINT_WAKE_HIT,
    PAINT_VERDICT,
    PAINT_SENTENCE,
    PAINT_HOLY_SCORCH,
    PAINT_ECHO_AISLES, PAINT_ECHO_AISLES_HIT,
    PAINT_ECHO_RING, PAINT_ECHO_RING_HIT,
    PAINT_ECHO_CORE, PAINT_ECHO_CORE_HIT,
    PAINT_CARRION, PAINT_CARRION_HIT,
    PAINT_LANCE, PAINT_LANCE_HIT,
    PAINT_CROSS, PAINT_CROSS_HIT,
    PAINT_SWARM,
    PAINT_EDGE,
    PAINT_INHALE,
    PAINT_INFERNAL,
    PAINT_VOID_SCORCH,
};
// A tower's looks (shapes.json): the sigils lit once held, the gems round its rim (empty, lit), one a soaker
constexpr uint32 LOOK_SIGIL_RADIANT_LIT = 90673;
constexpr uint32 LOOK_SIGIL_VOID_LIT = 90674;
constexpr uint32 LOOK_SIGIL_BASTION_LIT = 90675;
constexpr uint32 LOOK_PIP_HOLY_EMPTY = 90676;
constexpr uint32 LOOK_PIP_HOLY_LIT = 90677;
constexpr uint32 LOOK_PIP_VOID_EMPTY = 90678;
constexpr uint32 LOOK_PIP_VOID_LIT = 90679;
constexpr float PipRadius = 0.8f;
constexpr float PipGap = 1.2f;                  // past the tower's edge
constexpr uint32 TowerCheckMs = 250;
constexpr uint32 StrikeMs = 900;
constexpr uint32 ScorchMs = 3000;
// Stock: a red reticle over the head (Mark of Rimefang), a purple beam (a dummy channel)
constexpr uint32 SPELL_FX_MARK = 69275;
constexpr uint32 SPELL_FX_PURPLE_BEAM = 28309;

// Stock spell visual kits (SpellVisualKit.dbc), from the paladin's and the demons' own spells. Nothing that flashes
// over the whole screen.
enum Kits : uint32
{
    KIT_HOLY_WRATH_CAST     = 329,
    KIT_HOLY_WRATH_HIT      = 211,
    KIT_CONSECRATION_HIT    = 121,
    KIT_DIVINE_STORM_CAST   = 11088,
    KIT_DIVINE_STORM_HIT    = 11089,
    KIT_HAMMER_CAST         = 11015,
    KIT_HAMMER_HIT          = 11014,
    KIT_WRATH_HAMMER_HIT    = 6359,
    KIT_EXORCISM_HIT        = 487,
    KIT_JUDGEMENT_HIT       = 6849,
    KIT_HOLY_NOVA_CAST      = 3154,
    KIT_HYMN_CAST           = 10780,
    KIT_AVENGING_WRATH      = 6839,
    KIT_METAMORPHOSIS       = 11228,
    KIT_METAMORPHOSIS_PRE   = 6778,
    KIT_SHADOW_NOVA_CAST    = 6817,
    KIT_SHADOW_NOVA_HIT     = 8593,
    KIT_CARRION_CAST        = 6831,
    KIT_CARRION_HIT         = 6819,
    KIT_VAMPIRIC_HIT        = 3109,
    KIT_SHADOWFURY_HIT      = 2350,
    KIT_SHADOW_CRASH_CAST   = 12583,
    KIT_INFERNO_HIT         = 117,
    KIT_THOUSAND_SOULS      = 9552,
    KIT_DARKNESS            = 8717,
    KIT_SHADOWFLAME_CAST    = 10386,
    KIT_VOID_BLAST_HIT      = 6706,
    KIT_FEAR_HIT            = 498,
};

enum AldricTexts : uint8
{
    SAY_ALDRIC_AGGRO = 0,
    SAY_ALDRIC_KILL = 1,
    SAY_ALDRIC_FALLS = 2,
    SAY_ALDRIC_LAST_RITES = 3,
    SAY_ALDRIC_RELEASED = 4,
};

enum VelthazarTexts : uint8
{
    SAY_VELTHAZAR_REVEAL = 0,
    SAY_VELTHAZAR_KILL = 1,
    SAY_VELTHAZAR_ENRAGE = 2,
    SAY_VELTHAZAR_DEATH = 3,
};

enum class Phase : uint8
{
    None,
    Aldric,                                     // 0:00-1:59.7 (the verdict at 1:57.5)
    Fallen,                                     // at 1%, his track fading: the demon is about to show
    Hollow,                                     // Vel'thazar
    Over,                                       // a kill or a wipe
};

enum class Ability : uint8
{
    // The Archbishop's
    Radiance,
    Judgement,
    LightOfDawn,
    Hammers,
    Pulpit,
    Aisles,
    Choir,
    AbsolutionStart,
    AbsolutionEnd,
    Sentence,
    Verdict,
    Seraphim,
    Wake,
    // Vel'thazar's
    HollowPulse,
    Brand,
    Swarm,
    EchoAisles,
    EchoPulpit,
    Whisper,
    LastLightStart,
    LastLightPulse,
    LastLightEnd,
    Lances,
    Infernals,
    InhaleStart,
    TrueForm,
    SpinSwarm,
    LastPrayerStart,
    LastPrayerPulse,
    LastPrayerEnd,
    Sermon,
    VoidCross,
    Ruin,
};

// Steps that change the fight's state: a skip (.hollow skip) still runs them. Not the Prayer of Absolution: run over
// in a skip it started and ended at once, and he healed 8%.
bool IsStageStep(Ability ability)
{
    switch (ability)
    {
        case Ability::Seraphim:
        case Ability::LastLightStart:
        case Ability::LastLightEnd:
        case Ability::InhaleStart:
        case Ability::TrueForm:
        case Ability::LastPrayerStart:
        case Ability::LastPrayerEnd:
            return true;
        default:
            return false;
    }
}

// The demon's steps: they wait for him (a group at 1% too late never sees them)
bool IsVelthazarStep(Ability ability)
{
    return ability >= Ability::HollowPulse;
}

struct Step
{
    uint32 at;
    Ability what;
};

// The whole fight, in order: set on the two tracks' sections and hits
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps;
    auto once = [&steps](Ability what, uint32 at) { steps.push_back({ at, what }); };
    auto every = [&steps](Ability what, uint32 first, uint32 period, uint32 until)
    {
        for (uint32 at = first; at < until; at += period)
            steps.push_back({ at, what });
    };

    // --- The Archbishop: the opening and the steady part (0:00-0:43), rotation I
    once(Ability::Radiance, 1500);              // the orchestra's first hit
    for (uint32 at : { 4000u, 16000u, 28000u, 40000u, 53000u, 72000u, 84000u, 96000u, 104000u, 112000u })
        once(Ability::Judgement, at);
    for (uint32 at : { 7000u, 26000u, 47000u, 88000u, 103000u, 116000u })
        once(Ability::LightOfDawn, at);
    for (uint32 at : { 11600u, 36000u, 70000u, 110000u })
        once(Ability::Aisles, at);               // on the break (0:11.6), a swell (0:36), the build (1:09.6)
    for (uint32 at : { 19000u, 77000u, 106000u })
        once(Ability::Hammers, at);
    for (uint32 at : { 29000u, 91000u, 114000u })
        once(Ability::Pulpit, at);
    for (uint32 at : { 22000u, 34000u, 89000u })
        once(Ability::Radiance, at);
    // The first climax: two waves of the Choir
    once(Ability::Choir, 43300);
    once(Ability::Choir, 51500);
    // The break: he kneels
    once(Ability::AbsolutionStart, AtAbsolution);
    once(Ability::AbsolutionEnd, AtAbsolutionEnd);
    // The second build: the sentences and the Verdict on the hit
    once(Ability::Sentence, 74000);
    once(Ability::Verdict, AtVerdict - VerdictMs);
    // The final climax: his wings
    once(Ability::Seraphim, AtSeraphim);
    every(Ability::Radiance, AtSeraphim + 1000, 7000, AtLastRites - 1000);
    once(Ability::Wake, 98000);
    once(Ability::Sentence, 101000);
    once(Ability::Wake, 108000);

    // --- Vel'thazar: the reveal (2:01.5) to Last Light
    uint32 const start = AtReveal + 1000;
    every(Ability::HollowPulse, AtReveal + 6500, 12000, AtLastLight - 2000);
    every(Ability::Brand, AtReveal + 4500, 10000, AtLastLight);
    for (uint32 at : { AtReveal + 9500, AtReveal + 23500, AtReveal + 37500, AtReveal + 51500 })
        once(Ability::Swarm, at);
    once(Ability::EchoAisles, start + 14500);
    once(Ability::EchoPulpit, start + 31500);
    once(Ability::Whisper, AtWhisper);
    // Last Light: the shelters
    once(Ability::LastLightStart, AtLastLight);
    // The first pulse once the pools can be reached from across the chamber
    every(Ability::LastLightPulse, AtLastLight + 5000, 2000, AtLastLightEnd);
    once(Ability::LastLightEnd, AtLastLightEnd);
    // Phase 3: the lances, the infernals on the hit
    every(Ability::HollowPulse, AtLastLightEnd + 6500, 12000, AtInhale - 1000);
    every(Ability::Brand, AtLastLightEnd + 2500, 10000, AtInhale);
    for (uint32 at : { AtLastLightEnd + 5500, AtLastLightEnd + 19500, AtLastLightEnd + 35500 })
        once(Ability::Swarm, at);
    once(Ability::Lances, AtLastLightEnd + 10500);
    once(Ability::Lances, AtLastLightEnd + 33500);
    once(Ability::EchoPulpit, AtLastLightEnd + 15500);
    once(Ability::Infernals, AtInfernals - InfernalWarningMs);
    // Inhale of the Void in the near silence, the true form on the drop
    once(Ability::InhaleStart, AtInhale);
    once(Ability::TrueForm, AtTrueForm);
    // Phase 4: the spinning swarms
    every(Ability::HollowPulse, AtTrueForm + 4000, 10000, AtLastPrayer - 1000);
    every(Ability::Brand, AtTrueForm + 2000, 9000, AtLastPrayer);
    once(Ability::SpinSwarm, AtTrueForm + 7000);
    once(Ability::EchoAisles, AtTrueForm + 17000);
    once(Ability::Lances, AtTrueForm + 23000);
    once(Ability::SpinSwarm, AtTrueForm + 24500);
    // Aldric's Last Prayer, in the quiet verse
    once(Ability::LastPrayerStart, AtLastPrayer);
    every(Ability::LastPrayerPulse, AtLastPrayer + 3000, 3000, AtLastPrayerEnd);
    once(Ability::EchoAisles, AtLastPrayer + 11000);
    once(Ability::LastPrayerEnd, AtLastPrayerEnd);
    // Phase 5, the loudest climax: the Hollow Sermon, the crosses, the swarms
    every(Ability::HollowPulse, AtLastPrayerEnd + 5000, 10000, AtRuinSection);
    every(Ability::Brand, AtLastPrayerEnd + 2000, 8000, AtRuinSection);
    once(Ability::Sermon, AtLastPrayerEnd + 3600);
    once(Ability::VoidCross, AtLastPrayerEnd + 13600);
    once(Ability::SpinSwarm, AtLastPrayerEnd + 19600);
    once(Ability::Sermon, AtLastPrayerEnd + 31600);
    once(Ability::Lances, AtLastPrayerEnd + 42600);
    once(Ability::SpinSwarm, AtLastPrayerEnd + 48600);
    once(Ability::VoidCross, AtLastPrayerEnd + 55600);
    // The dark section: Voice of Ruin on the three stabs, the Aegis shown before each
    every(Ability::HollowPulse, AtRuinSection + 4000, 10000, AtEnrageBlasts[0] - 3000);
    every(Ability::Brand, AtRuinSection + 2000, 9000, AtEnrageBlasts[0] - 2000);
    for (uint32 at : AtRuins)
        once(Ability::Ruin, at - RuinWarningMs);
    once(Ability::Swarm, AtRuins[0] + 5000);
    once(Ability::EchoPulpit, AtRuins[1] + 4000);
    once(Ability::Swarm, AtRuins[2] + 5000);
    once(Ability::Lances, AtRuins[2] + 9000);

    std::stable_sort(steps.begin(), steps.end(), [](Step const& left, Step const& right)
    {
        if (left.at != right.at)
            return left.at < right.at;
        return IsStageStep(left.what) && !IsStageStep(right.what);
    });
    return steps;
}

uint32 SpellOf(NamedSpell const& spell)
{
    return sSpellMgr->GetSpellInfo(spell.custom) ? spell.custom : spell.stock;
}

float Reference()
{
    return Power::ExpectedPlayerHealth(ProfileItemLevel, ProfileParagon);
}

// .hollow botsonly: the fight fought by its bots alone, a game master watching (the fight leaves game masters out):
// the bots count as players - no finishing them off - and the report says how they did (testing the bots)
bool BotsOnly = false;

bool IsRealPlayer(Player const* player)
{
    return player->GetSession() && (!player->GetSession()->IsBot() || BotsOnly);
}

// --- The report: how each ability went (.hollow report, and in the log at the end of every fight) -----------------
struct AbilityStats
{
    uint32 hits = 0;                            // players hit
    uint32 red = 0;                             // of them, standing in the red
    uint32 deaths = 0;                          // players it killed
    uint32 resolved = 0;                        // soaks and the like resolved...
    uint32 failed = 0;                          // ... and failed
};

char const* AbilityLabel(uint32 spellId)
{
    switch (spellId)
    {
        case 94000: return "Judgement";
        case 94002: return "Blessed Hammers";
        case 94003: return "Wrath of the Pulpit";
        case 94004: return "Consecrated Aisles";
        case 94005: return "Light of Dawn";
        case 94006: return "Choir of the Faithful";
        case 94008: return "Execution Sentence";
        case 94009: return "Verdict of the Faithful";
        case 94011: return "Wake of Ashes";
        case 94012: return "Last Rites";
        case 94013: return "Holy Radiance";
        case 94020: return "Carrion Swarm";
        case 94021: return "Vampiric Brand";
        case 94023: return "Hollow Echo";
        case 94024: return "Whisper of Doubt";
        case 94025: return "Last Light";
        case 94026: return "Nightmare Lances";
        case 94027: return "Dread Infernal impact";
        case 94028: return "Inhale of the Void";
        case 94029: return "Devoured edge";
        case 94033: return "Hollow Sermon";
        case 94034: return "Voice of Ruin";
        case 94036: return "Hollow Pulse";
        case 94037: return "Silence";
        case 94038: return "Crosses of void";
        case 94039: return "Tearing out";
        case 94040: return "Spinning swarms";
        default:    return "melee";
    }
}

// Throttled kill yells, as L'Infini's
bool KillYellReady(uint32& lastMs)
{
    uint32 const now = getMSTime();
    if (lastMs && getMSTimeDiff(lastMs, now) < 10000)
        return false;
    lastMs = now;
    return true;
}

// A player stands in a line from start along facing, length long and width wide
bool InLine(Position const& start, float facing, float length, float width, Position const& at)
{
    float const dx = at.GetPositionX() - start.GetPositionX();
    float const dy = at.GetPositionY() - start.GetPositionY();
    float const along = dx * std::cos(facing) + dy * std::sin(facing);
    float const across = -dx * std::sin(facing) + dy * std::cos(facing);
    return along >= 0.0f && along <= length && std::fabs(across) <= width / 2.0f;
}

// A line from start along facing, length long and width wide, for the bots (nothing drawn)
GroundIndicators::Area LineArea(Position const& start, float facing, float length, float width)
{
    GroundIndicators::Area area;
    area.kind = GroundIndicators::Area::Kind::Rectangle;
    area.origin = Position(start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), facing);
    area.radius = length;
    area.width = width;
    return area;
}

GroundIndicators::Area CircleArea(Position const& center, float radius)
{
    GroundIndicators::Area area;
    area.kind = GroundIndicators::Area::Kind::Circle;
    area.origin = center;
    area.radius = radius;
    return area;
}

GroundIndicators::Area ConeArea(Position const& apex, float facing, float radius, float arcDegrees)
{
    GroundIndicators::Area area;
    area.kind = GroundIndicators::Area::Kind::Cone;
    area.origin = Position(apex.GetPositionX(), apex.GetPositionY(), apex.GetPositionZ(), facing);
    area.radius = radius;
    area.arc = arcDegrees * float(M_PI) / 180.0f;
    return area;
}

GroundIndicators::Area RingArea(Position const& center, float outer, float inner)
{
    GroundIndicators::Area area;
    area.kind = GroundIndicators::Area::Kind::Ring;
    area.origin = center;
    area.radius = outer;
    area.inner = inner;
    return area;
}

// A creature of the fight's that melees: at least a share of the reference, whatever the target's armour
void MeleeFloor(Unit* attacker, Unit* victim, uint32& damage, DamageEffectType type, float percent)
{
    if (type == DIRECT_DAMAGE && victim && victim->IsPlayer())
        damage = uint32(std::max(float(damage), Reference() * percent / 100.0f *
                                 std::max(GetChallengeDamageFactorOf(attacker), 1.0f)));
}

// A player killed by a creature of the fight: counted in the Archbishop's report as melee, unless an ability of
// the fight's did it (it counts its own). Defined after the Archbishop's AI.
void NoteKill(Creature* killer, Unit* victim);

struct boss_hollow_voice_velthazar : public ScriptedAI
{
    boss_hollow_voice_velthazar(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }

    // The Archbishop ends the fight: a wipe is his to see (boss_hollow_voice_aldric Wipe)
    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void KilledUnit(Unit* victim) override
    {
        NoteKill(me, victim);
        if (victim->IsPlayer() && !_enraged && KillYellReady(_lastKillYellMs))
            Talk(SAY_VELTHAZAR_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_VELTHAZAR_DEATH);
    }

    // Imprisoned by Aldric's Last Prayer, nothing reaches him
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_imprisoned)
            damage = 0;
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        MeleeFloor(me, victim, damage, type, VelthazarMeleeFloorPct);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        if (!_held)
            DoMeleeAttackIfReady();
    }

    bool _enraged = false;
    bool _held = false;                         // an intermission: no melee, no chase
    bool _imprisoned = false;

private:
    uint32 _lastKillYellMs = 0;
};

// Dread Infernal: crashes down in phase 3 and fights on until killed
struct npc_hollow_voice_infernal : public ScriptedAI
{
    npc_hollow_voice_infernal(Creature* creature) : ScriptedAI(creature) { }

    void KilledUnit(Unit* victim) override
    {
        NoteKill(me, victim);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        MeleeFloor(me, victim, damage, type, InfernalMeleeFloorPct);
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }
};

struct boss_hollow_voice_aldric : public ScriptedAI
{
    boss_hollow_voice_aldric(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void InitializeAI() override
    {
        // Hidden until its instance is known to be a challenge's (UpdateDefi)
        if (!_defiConfirmed)
            me->SetVisible(false);
        ScriptedAI::InitializeAI();
    }

    void Reset() override
    {
        ResetFight();
        _lingering = false;
        // He never regenerates (his hold at 1%), so he would keep his spawn's stored health: full on every reset
        me->SetFullHealth();
        if (_defiConfirmed)
            me->SetVisible(true);
        // The board holds him until the group pulls (RaidFinder HoldChallengeBoss)
        me->SetReactState(REACT_PASSIVE);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (_lingering)
            return;
        // Fallen or hidden he has no one to fight: the players standing decide the wipe (UpdateStanding), not combat
        if (_phase == Phase::Fallen || _phase == Phase::Hollow)
            return;
        if (_phase == Phase::Aldric)
        {
            Wipe();
            return;
        }
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (_phase != Phase::None)
            return;
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, ArenaReach);
        _pullMs = getMSTime();
        _phase = Phase::Aldric;
        _stats.clear();
        _timeline = BuildTimeline();
        _next = 0;
        uint32 track = MUSIC_FIGHT;
        if (_revealArmed)
        {
            // .hollow reveal: the fight from 1:50, the Archbishop at 1%, its track from there
            _revealArmed = false;
            _pullMs -= RevealTestStartMs;
            me->SetHealth(HoldHealth());
            track = MUSIC_FIGHT_FROM_REVEAL;
            while (_next < _timeline.size() && _timeline[_next].at <= RevealTestStartMs)
            {
                Ability const what = _timeline[_next++].what;
                if (IsStageStep(what))
                    Execute(what);
            }
        }
        else
            Talk(SAY_ALDRIC_AGGRO);
        _fightListeners.clear();
        for (Player* player : Listeners())
        {
            SendMusic(player, track);
            _fightListeners.insert(player->GetGUID());
        }
        LOG_INFO("module.hollowvoice", "The Hollow Voice pulled instance={} health={} tier factor={}",
                 me->GetInstanceId(), me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        NoteKill(me, victim);
        if (victim->IsPlayer() && _phase == Phase::Aldric && KillYellReady(_lastKillYellMs))
            Talk(SAY_ALDRIC_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        EndTrack(MUSIC_SILENCE);
        LOG_INFO("module.hollowvoice", "The Hollow Voice killed instance={} elapsed={}ms", me->GetInstanceId(),
                 Elapsed());
        LogReport();
        // The demon's corpse stays with his
        scheduler.CancelAll();
        ClearPlayerAuras();
        GroundIndicators::ClearAreasOf(me);
        _phase = Phase::Over;
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
    }

    // The demon dies: the Archbishop is freed, seen again, and dies with it - his kill is the board's win
    void SummonedCreatureDies(Creature* summon, Unit* killer) override
    {
        if (summon->GetEntry() != NPC_VELTHAZAR || _phase != Phase::Hollow)
            return;
        _phase = Phase::Over;
        EndTrack(MUSIC_SILENCE);
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        for (ObjectGuid const& guid : _summons)
            if (Creature* infernal = ObjectAccessor::GetCreature(*me, guid);
                infernal && infernal->GetEntry() == NPC_INFERNAL && infernal->IsAlive())
                Unit::Kill(infernal, infernal);
        me->SetVisible(true);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        Talk(SAY_ALDRIC_RELEASED);
        Unit* credit = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        if (!credit)
        {
            std::vector<Player*> const players = ArenaPlayers();
            credit = players.empty() ? static_cast<Unit*>(me) : players.front();
        }
        Unit::Kill(credit, me);
    }

    // His health stops at 1%; fallen or hidden, nothing reaches him. Kneeling in prayer, his aegis takes it first.
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_phase == Phase::Fallen || _phase == Phase::Hollow)
        {
            damage = 0;
            return;
        }
        if (_phase != Phase::Aldric)
            return;
        if (_absolutionShield)
        {
            uint32 const absorbed = std::min(damage, _absolutionShield);
            _absolutionShield -= absorbed;
            damage -= absorbed;
            if (!_absolutionShield)
                BreakAbsolution();
        }
        uint32 const floor = HoldHealth();
        uint32 const health = uint32(me->GetHealth());
        if (health <= floor)
            damage = 0;
        else if (damage >= health - floor)
            damage = health - floor;
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        MeleeFloor(me, victim, damage, type, AldricMeleeFloorPct);
    }

    void UpdateAI(uint32 diff) override
    {
        // Fallen, then hidden: he runs the clock with no one to fight
        if (_phase == Phase::Fallen || _phase == Phase::Hollow)
        {
            scheduler.Update(diff);
            UpdateChamber(diff);
            UpdateTowers();
            UpdateClock(false);
            UpdateStanding();
            return;
        }

        if (!UpdateVictim())
        {
            UpdateOutOfCombat(diff);
            return;
        }

        scheduler.Update(diff);
        UpdateChamber(diff);
        UpdateTowers();
        UpdateClock(false);
        UpdateStanding();
        if (_phase == Phase::Aldric && !_kneeling && !_windup)
            DoMeleeAttackIfReady();
    }

    // A mechanic resolved: held (soakers enough, a tank in it, lights carried) or failed
    void Resolved(char const* ability, bool held, std::string const& detail)
    {
        AbilityStats& stats = _stats[ability];
        ++stats.resolved;
        if (!held)
            ++stats.failed;
        LOG_INFO("module.hollowvoice", "hv {} {} {} at={:.1f}s", held ? "held" : "FAILED", ability, detail,
                 Elapsed() / 1000.0f);
    }

    // A player killed by a boss's or an infernal's melee
    void MeleeDeath(Unit* victim)
    {
        if (_inHit || !victim->IsPlayer() || _phase == Phase::None || _phase == Phase::Over)
            return;
        ++_stats["melee"].deaths;
        LOG_INFO("module.hollowvoice", "hv death {} by=melee at={:.1f}s", victim->GetName(), Elapsed() / 1000.0f);
    }

    std::string Report() const
    {
        std::string text = Acore::StringFormat("Hollow Voice report at {:.1f}s:", Elapsed() / 1000.0f);
        for (auto const& [ability, stats] : _stats)
            text += Acore::StringFormat("\n  {}: hits {}, in the red {}, deaths {}{}", ability, stats.hits, stats.red,
                stats.deaths, stats.resolved ? Acore::StringFormat(", held {}/{}", stats.resolved - stats.failed,
                stats.resolved) : std::string());
        return text;
    }

    void LogReport() const
    {
        LOG_INFO("module.hollowvoice", "{}", Report());
    }

    // --- The game master's commands --------------------------------------------------------------------------------
    std::string Describe() const
    {
        Creature* demon = Velthazar();
        std::vector<Player*> const players = ArenaPlayers();
        uint64 health = 0;
        for (Player* player : players)
            health += player->GetMaxHealth();
        return Acore::StringFormat("The Hollow Voice: phase {} at {:.1f}s, Aldric {}/{} ({:.1f}%), Vel'thazar {}, "
                                   "tier damage x{:.2f}, reference {:.0f}, next step {}/{}, {} standing (health {} on "
                                   "average)", uint32(_phase), Elapsed() / 1000.0f, me->GetHealth(), me->GetMaxHealth(),
                                   me->GetHealthPct(), demon ? Acore::StringFormat("{}/{}", demon->GetHealth(),
                                   demon->GetMaxHealth()) : std::string("-"), GetChallengeDamageFactorOf(me),
                                   Reference(), _next, _timeline.size(), players.size(),
                                   players.empty() ? 0 : health / players.size());
    }

    // Jumps the fight forward (testing): the steps passed over are dropped, the stage changes among them still run.
    // The music cannot follow (a second one would fade out): it plays on where it was.
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        EndWindup();
        if (_kneeling)
            StandUp();
        _pullMs -= seconds * IN_MILLISECONDS;
        UpdateClock(true);
        return true;
    }

    // The next pull starts at 1:50, the Archbishop at 1%, with its own track from there (testing the reveal)
    bool ArmReveal()
    {
        if (_phase != Phase::None)
            return false;
        _revealArmed = true;
        return true;
    }

    // .hollow cast: one ability now, the fight going on
    bool CastNow(std::string const& what)
    {
        static std::map<std::string, Ability> const names = {
            { "radiance", Ability::Radiance }, { "judgement", Ability::Judgement },
            { "dawn", Ability::LightOfDawn }, { "hammers", Ability::Hammers }, { "pulpit", Ability::Pulpit },
            { "aisles", Ability::Aisles }, { "choir", Ability::Choir }, { "absolution", Ability::AbsolutionStart },
            { "sentence", Ability::Sentence }, { "verdict", Ability::Verdict }, { "seraphim", Ability::Seraphim },
            { "wake", Ability::Wake }, { "pulse", Ability::HollowPulse }, { "brand", Ability::Brand },
            { "swarm", Ability::Swarm }, { "echoaisles", Ability::EchoAisles }, { "echopulpit", Ability::EchoPulpit },
            { "whisper", Ability::Whisper }, { "lastlight", Ability::LastLightStart },
            { "lances", Ability::Lances }, { "infernals", Ability::Infernals }, { "inhale", Ability::InhaleStart },
            { "trueform", Ability::TrueForm }, { "spin", Ability::SpinSwarm },
            { "prayer", Ability::LastPrayerStart }, { "sermon", Ability::Sermon }, { "cross", Ability::VoidCross },
            { "ruin", Ability::Ruin },
        };
        auto const found = names.find(what);
        if (found == names.end() || _phase == Phase::None || _phase == Phase::Over)
            return false;
        if (IsVelthazarStep(found->second) != (_phase == Phase::Hollow))
            return false;
        Execute(found->second);
        // An intermission started by hand ends by itself
        if (found->second == Ability::LastLightStart)
        {
            for (uint32 pulse = 0; pulse < 6; ++pulse)
                scheduler.Schedule(Milliseconds(5000 + pulse * 2000), [this](TaskContext) { LastLightPulse(); });
            scheduler.Schedule(Milliseconds(AtLastLightEnd - AtLastLight), [this](TaskContext) { EndLastLight(); });
        }
        else if (found->second == Ability::LastPrayerStart)
            scheduler.Schedule(Milliseconds(AtLastPrayerEnd - AtLastPrayer), [this](TaskContext) { EndLastPrayer(); });
        else if (found->second == Ability::AbsolutionStart)
            scheduler.Schedule(Milliseconds(AbsolutionMs), [this](TaskContext) { EndAbsolution(); });
        return true;
    }

private:
    uint32 Elapsed() const
    {
        return _phase == Phase::None ? 0 : getMSTimeDiff(_pullMs, getMSTime());
    }

    uint32 HoldHealth() const
    {
        return std::max<uint32>(1, uint32(float(me->GetMaxHealth()) * HoldHealthPct / 100.0f));
    }

    Creature* Velthazar() const
    {
        for (ObjectGuid const& guid : _summons)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, guid);
                summon && summon->GetEntry() == NPC_VELTHAZAR && summon->IsAlive())
                return summon;
        return nullptr;
    }

    boss_hollow_voice_velthazar* VelthazarAI() const
    {
        Creature* demon = Velthazar();
        return demon ? dynamic_cast<boss_hollow_voice_velthazar*>(demon->AI()) : nullptr;
    }

    // Who the abilities come from: the Archbishop, then the demon
    Unit* Caster() const
    {
        if (_phase == Phase::Hollow)
            if (Creature* demon = Velthazar())
                return demon;
        return me;
    }

    Position Center() const
    {
        return me->GetHomePosition();
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
        else if (!_lingering)
            ClearChamber();
    }

    // Every second of the fight too: what the chamber's stock fight sends in later goes as it comes
    void UpdateChamber(uint32 diff)
    {
        _checkTimer += diff;
        if (_checkTimer < 1000)
            return;
        _checkTimer = 0;
        if (_defiConfirmed && !_lingering)
            ClearChamber();
        if (_edge)
            EdgeTick();
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
            me->SetFullHealth();
            ClearChamber();
            LOG_INFO("module.hollowvoice", "The Hollow Voice appears for a challenge instance={}", me->GetInstanceId());
            return;
        }

        _defiWaitMs += 1000;
        if (_defiWaitMs >= DefiGraceMs)
        {
            LOG_INFO("module.hollowvoice", "The Hollow Voice leaves an instance that is no challenge's instance={}",
                     me->GetInstanceId());
            me->DespawnOrUnsummon(0ms, Seconds(7 * DAY));
        }
    }

    // M'uru's chamber is his: M'uru, the guards around it and anything its fight summoned go (triggers stay). The
    // board's own clearing (RaidFinder ClearChallengeTrash) leaves what stands in the boss's room, the adds a stock
    // fight is made of.
    // A player landing in the chamber can engage M'uru before it goes: its encounter is left in progress, and a raid
    // lets nobody in while one is (Map::CannotEnter, ZONE_IN_COMBAT) - the bots were kept out. No stock encounter is
    // fought in his instance: any left in progress is set back.
    void ClearChamber()
    {
        std::list<Creature*> found;
        Acore::AllWorldObjectsInRange check(me, ChamberClearRadius);
        Acore::CreatureListSearcher<Acore::AllWorldObjectsInRange> searcher(me, found, check);
        Cell::VisitObjects(me, searcher, ChamberClearRadius);
        for (Creature* creature : found)
            if (creature != me && creature->IsAlive() && !creature->IsTrigger() &&
                creature->GetEntry() != NPC_ALDRIC && creature->GetEntry() != NPC_VELTHAZAR &&
                creature->GetEntry() != NPC_INFERNAL && creature->GetEntry() != NPC_STALKER &&
                creature->GetEntry() != NPC_HOVER_STALKER && !creature->IsCharmedOwnedByPlayerOrPlayer() &&
                creature->IsHostileToPlayers())
                creature->DespawnOrUnsummon(0ms, Seconds(DAY));

        if (InstanceScript* instance = me->GetInstanceScript())
            for (uint32 boss = 0; boss < instance->GetEncounterCount(); ++boss)
                if (instance->GetBossState(boss) == IN_PROGRESS)
                {
                    instance->SetBossState(boss, NOT_STARTED);
                    LOG_INFO("module.hollowvoice", "The Hollow Voice: stock encounter {} set back instance={}", boss,
                             me->GetInstanceId());
                }
    }

    // --- Fight state -----------------------------------------------------------------------------------------------
    void ResetFight()
    {
        scheduler.CancelAll();
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        ClearPlayerAuras();
        _phase = Phase::None;
        _timeline.clear();
        _next = 0;
        _blasts = 0;
        _nextPulseMs = 0;
        _finishing = false;
        _nextStandingCheckMs = 0;
        _kneeling = false;
        _absolutionShield = 0;
        _seraphim = false;
        _windup = false;
        _edge = false;
        _prisonCracks = 0;
        _prisonLights.clear();
        _towers.clear();
        _condemned.clear();
        _brand.clear();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->RemoveAurasDueToSpell(SPELL_ABSOLUTION_AURA);
        me->RemoveAurasDueToSpell(SPELL_SERAPHIM_AURA);
    }

    void ClearPlayerAuras()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                for (uint32 spell : { SPELL_CONDEMNED, SPELL_BRAND_STACKS, SPELL_SENTENCE_MARK, SPELL_CARRY_LIGHT })
                    player->RemoveAurasDueToSpell(spell);
    }

    // The players the fight hits: alive in the chamber, not game masters
    std::vector<Player*> ArenaPlayers() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || player->IsGameMaster() || !player->IsAlive() ||
                player->GetExactDist2d(&me->GetHomePosition()) > ArenaReach)
                continue;
            players.push_back(player);
        }
        return players;
    }

    // Everyone but the tanks and whoever the caster is fighting
    std::vector<Player*> NonTanks() const
    {
        Unit* victim = Caster()->GetVictim();
        std::vector<Player*> players = ArenaPlayers();
        std::erase_if(players, [victim](Player* player) { return player == victim || IsGroupTank(player); });
        if (players.empty())
            players = ArenaPlayers();
        return players;
    }

    // count of them, as far apart as can be: the first at random, then each the farthest from those picked
    std::vector<Player*> SpreadPick(std::vector<Player*> players, uint32 count) const
    {
        std::vector<Player*> picked;
        if (players.empty())
            return picked;
        Acore::Containers::RandomShuffle(players);
        picked.push_back(players.front());
        while (picked.size() < count && picked.size() < players.size())
        {
            Player* best = nullptr;
            float bestDistance = -1.0f;
            for (Player* player : players)
            {
                if (std::find(picked.begin(), picked.end(), player) != picked.end())
                    continue;
                float nearest = 1000.0f;
                for (Player* other : picked)
                    nearest = std::min(nearest, player->GetExactDist2d(other));
                if (nearest > bestDistance)
                {
                    bestDistance = nearest;
                    best = player;
                }
            }
            if (!best)
                break;
            picked.push_back(best);
        }
        return picked;
    }

    std::vector<Player*> PlayersIn(GroundIndicators::Area const& area) const
    {
        std::vector<Player*> players;
        for (Player* player : ArenaPlayers())
            if (area.Contains(*player))
                players.push_back(player);
        return players;
    }

    // The players who hear its music: everyone near the chamber, dead or a game master too
    std::vector<Player*> Listeners() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource();
                player && player->GetExactDist2d(&me->GetHomePosition()) <= MusicReach)
                players.push_back(player);
        return players;
    }

    Position Ground(Position const& at) const
    {
        float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), at.GetPositionX(), at.GetPositionY(),
                                                 Center().GetPositionZ() + 5.0f, true, 15.0f);
        return Position(at.GetPositionX(), at.GetPositionY(), z > INVALID_HEIGHT ? z : Center().GetPositionZ(),
                        at.GetOrientation());
    }

    // A spot on the chamber's floor between minRadius and maxRadius from its middle
    Position ArenaSpot(float minRadius, float maxRadius) const
    {
        Position const center = Center();
        for (uint32 attempt = 0; attempt < 8; ++attempt)
        {
            float const angle = frand(0.0f, 2.0f * float(M_PI));
            float const distance = frand(minRadius, maxRadius);
            Position const spot = Ground(Position(center.GetPositionX() + std::cos(angle) * distance,
                                                  center.GetPositionY() + std::sin(angle) * distance,
                                                  center.GetPositionZ()));
            if (std::fabs(spot.GetPositionZ() - center.GetPositionZ()) < 3.0f)
                return spot;
        }
        return center;
    }

    Position AtAngle(Position const& from, float angle, float distance) const
    {
        return Ground(Position(from.GetPositionX() + std::cos(angle) * distance,
                               from.GetPositionY() + std::sin(angle) * distance, from.GetPositionZ(), angle));
    }

    // --- Music: sent as stock encounters send theirs (SMSG_PLAY_MUSIC); only a music ends a music -------------------
    void EndTrack(uint32 soundId)
    {
        if (!_fightListeners.empty())
            LOG_INFO("module.hollowvoice", "hv music ended ({}) at={:.1f}s", soundId, Elapsed() / 1000.0f);
        for (ObjectGuid const& guid : _fightListeners)
            if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                SendMusic(player, soundId);
        _fightListeners.clear();
    }

    void SendMusic(Player* player, uint32 soundId)
    {
        if (player && player->IsInWorld() && IsRealPlayer(player))
            me->PlayDirectMusic(soundId, player);
    }

    // --- Effects ---------------------------------------------------------------------------------------------------
    void Sound(uint32 soundId)
    {
        Caster()->PlayDirectSound(soundId);
    }

    // A stock kit on the ground, on a short-lived invisible stalker, a moment after it exists for the clients
    void PlayOnGround(Position const& where, uint32 kit)
    {
        TempSummon* stalker = me->SummonCreature(NPC_STALKER, Ground(where), TEMPSUMMON_TIMED_DESPAWN, 4000);
        if (!stalker)
            return;
        ObjectGuid const guid = stalker->GetGUID();
        scheduler.Schedule(250ms, [this, guid, kit](TaskContext)
        {
            if (Creature* found = me->GetMap()->GetCreature(guid))
                found->SendPlaySpellVisual(kit);
        });
    }

    // A stalker that lives lasts ms, held where it is put
    Creature* FxStalker(Position const& at, uint32 lasts, uint32 entry = NPC_STALKER)
    {
        Creature* stalker = me->SummonCreature(entry, at, TEMPSUMMON_TIMED_DESPAWN, lasts);
        if (stalker)
            stalker->SetDisableGravity(true);
        return stalker;
    }

    void ShowStacks(Player* player, uint32 spellId, uint32 stacks, uint32 durationMs)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
            return;
        Aura* aura = player->GetAura(spellId);
        if (!aura)
            aura = me->AddAura(spellId, player);
        if (!aura)
            return;
        aura->SetStackAmount(uint8(std::min<uint32>(stacks, 255)));
        aura->SetMaxDuration(int32(durationMs));
        aura->SetDuration(int32(durationMs));
    }

    static Position OnFloor(Position const& at)
    {
        return Position(at.GetPositionX(), at.GetPositionY(), ChamberFloorZ, at.GetOrientation());
    }

    // A painted warning (GroundIndicators::ShowPainted), carried at the chamber's floor height, staying StrikeMs past
    // its warning for Strike
    GroundIndicators::Area Paint(GroundIndicators::Area area, uint32 look, uint32 durationMs,
                                 GroundIndicators::Theme theme = GroundIndicators::Theme::None)
    {
        area.origin = OnFloor(area.origin);
        return GroundIndicators::ShowPainted(me, area, look, durationMs, theme, 0, nullptr, StrikeMs);
    }

    // A picture on the floor (a sigil, a scorch), at the chamber's floor height
    void Decal(Position const& at, float orientation, float radius, uint32 durationMs, uint32 look)
    {
        GroundIndicators::ShowDecal(me, OnFloor(at), orientation, radius, durationMs, look);
    }

    // Where a painted area lands: its own carrier turns to the full-strength look (hitLook, its warning's twin: the
    // warning's id + 1) until it goes. A carrier of its own would fade in as the client shows any new unit: the flash
    // appeared half-way and went.
    void Strike(GroundIndicators::Area const& area, uint32 hitLook)
    {
        uint32 const warning = hitLook - 1;
        std::list<Creature*> stalkers;
        me->GetCreatureListWithEntryInGrid(stalkers, NPC_STALKER, 60.0f);
        for (Creature* stalker : stalkers)
            // By place, look and size: the Pulpit's rings share a middle, two of them a look
            if (stalker->HasAura(warning) && stalker->GetExactDist2d(&area.origin) < 0.5f &&
                std::fabs(stalker->GetObjectScale() - area.radius) < 0.05f)
            {
                stalker->RemoveAurasDueToSpell(warning);
                stalker->AddAura(hitLook, stalker);
                return;
            }
    }

    // What a landing leaves on the floor a while (a hammer's, a crash's)
    void Scorch(Position const& where, float radius, uint32 look)
    {
        Decal(where, frand(0.0f, 2.0f * float(M_PI)), radius, ScorchMs, look);
    }

    // A look of the ground (shapes.json) on an invisible stalker of its own, radius yards: kept by guid, so it can
    // change looks (a tower waiting, held)
    Creature* Look(Position const& at, float orientation, float radius, uint32 lasts, uint32 look)
    {
        TempSummon* stalker = me->SummonCreature(NPC_STALKER, Position(at.GetPositionX(), at.GetPositionY(),
            at.GetPositionZ(), orientation), TEMPSUMMON_TIMED_DESPAWN, lasts);
        if (!stalker)
            return nullptr;
        stalker->SetObjectScale(radius);
        stalker->AddAura(look, stalker);
        return stalker;
    }

    void SwapLook(ObjectGuid guid, uint32 from, uint32 to)
    {
        if (Creature* stalker = guid ? me->GetMap()->GetCreature(guid) : nullptr)
        {
            stalker->RemoveAurasDueToSpell(from);
            stalker->AddAura(to, stalker);
        }
    }

    void ShowTower(Position const& spot, float orientation, float radius, uint32 lasts, uint32 needed, bool tanksOnly,
                   bool holy, uint32 waiting, uint32 held, bool pillar = true)
    {
        Tower tower;
        tower.center = OnFloor(spot);
        tower.radius = radius;
        tower.needed = needed;
        tower.tanksOnly = tanksOnly;
        tower.pillar = pillar;
        tower.waiting = waiting;
        tower.held = held;
        tower.pipEmpty = holy ? LOOK_PIP_HOLY_EMPTY : LOOK_PIP_VOID_EMPTY;
        tower.pipLit = holy ? LOOK_PIP_HOLY_LIT : LOOK_PIP_VOID_LIT;
        tower.endsAt = getMSTime() + lasts;
        if (Creature* sigil = Look(tower.center, orientation, radius, lasts, waiting))
            tower.sigil = sigil->GetGUID();
        // The gems in an arc over the tower's far side from the middle of the chamber, a gem's width apart
        float const facing = Center().GetAngle(&tower.center);
        float const step = 2.0f * (PipRadius + 0.3f) / (radius + PipGap);
        for (uint32 index = 0; index < needed; ++index)
        {
            float const angle = facing + (float(index) - float(needed - 1) / 2.0f) * step;
            if (Creature* pip = Look(AtAngle(tower.center, angle, radius + PipGap), 0.0f, PipRadius, lasts,
                tower.pipEmpty))
                tower.pips.push_back(pip->GetGUID());
        }
        _towers.push_back(tower);
    }

    // Every TowerCheckMs: who stands in each tower, its gems and sigil following
    void UpdateTowers()
    {
        uint32 const now = getMSTime();
        if (_towers.empty() || getMSTimeDiff(_nextTowerCheck, now) > 0x80000000u)
            return;
        _nextTowerCheck = now + TowerCheckMs;
        std::erase_if(_towers, [now](Tower const& tower) { return getMSTimeDiff(tower.endsAt, now) < 0x80000000u; });
        for (Tower& tower : _towers)
        {
            uint32 standing = 0;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(&tower.center) <= tower.radius && (!tower.tanksOnly || IsGroupTank(player)))
                    ++standing;
            uint32 const lit = std::min(standing, tower.needed);
            for (uint32 index = 0; index < tower.pips.size(); ++index)
            {
                bool const was = index < tower.lit;
                bool const is = index < lit;
                if (was != is)
                    SwapLook(tower.pips[index], was ? tower.pipLit : tower.pipEmpty,
                             is ? tower.pipLit : tower.pipEmpty);
            }
            tower.lit = lit;
            bool const full = standing >= tower.needed;
            if (full == tower.full)
                continue;
            tower.full = full;
            if (tower.held != tower.waiting)
                SwapLook(tower.sigil, full ? tower.waiting : tower.held, full ? tower.held : tower.waiting);
            if (!tower.pillar)
                continue;
            if (full)
            {
                uint32 const left = getMSTimeDiff(now, tower.endsAt);
                if (Creature* light = FxStalker(tower.center, std::max<uint32>(left, 100), NPC_HOVER_STALKER))
                {
                    light->AddAura(SPELL_LIGHT_FX, light);
                    tower.light = light->GetGUID();
                }
                PlayOnGround(tower.center, KIT_HOLY_WRATH_HIT);
            }
            else if (Creature* light = tower.light ? me->GetMap()->GetCreature(tower.light) : nullptr)
            {
                light->DespawnOrUnsummon();
                tower.light = ObjectGuid::Empty;
            }
        }
    }

    // An aura for a set time (a mark, a light carried)
    void AddTimedAura(Unit* target, uint32 spellId, uint32 durationMs)
    {
        if (!target || !sSpellMgr->GetSpellInfo(spellId))
            return;
        if (Aura* aura = me->AddAura(spellId, target))
        {
            aura->SetMaxDuration(int32(durationMs));
            aura->SetDuration(int32(durationMs));
        }
    }

    // --- Hits --------------------------------------------------------------------------------------------------------
    // Holy damage taken, from Condemned
    float TakenFactor(Player const* player, bool holy) const
    {
        if (!holy)
            return 1.0f;
        auto const condemned = _condemned.find(player->GetGUID());
        if (condemned == _condemned.end() || getMSTimeDiff(condemned->second.second, getMSTime()) < 0x80000000u)
            return 1.0f;
        return 1.0f + CondemnedPerStackPct / 100.0f * float(condemned->second.first);
    }

    // A share of the reference health, as spell: the tier's factor applies on the way (ChallengeTierUnitScript), then
    // the player's defences. An avoidable one (it stood in the red) adds Imprudence, as in a key.
    void Hit(Player* player, NamedSpell const& spell, float percent, bool avoidable)
    {
        if (!player || !player->IsAlive())
            return;
        AbilityStats& stats = _stats[AbilityLabel(spell.custom)];
        ++stats.hits;
        if (avoidable)
        {
            ++stats.red;
            LOG_INFO("module.hollowvoice", "hv red {} ability={} at={:.1f}s", player->GetName(),
                     AbilityLabel(spell.custom), Elapsed() / 1000.0f);
        }
        bool const holy = spell.stock == STOCK_HOLY;
        float const amount = Reference() * percent / 100.0f * TakenFactor(player, holy);
        _inHit = true;
        MythicTuning::DealAbilityDamage(Caster(), player, SpellOf(spell), uint32(std::max(1.0f, amount)));
        _inHit = false;
        if (avoidable)
            MythicTuning::ApplyImprudence(player);
        if (!player->IsAlive())
        {
            ++stats.deaths;
            LOG_INFO("module.hollowvoice", "hv death {} by={} at={:.1f}s", player->GetName(),
                     AbilityLabel(spell.custom), Elapsed() / 1000.0f);
        }
    }

    void HitEveryone(NamedSpell const& spell, float percent)
    {
        for (Player* player : ArenaPlayers())
            Hit(player, spell, percent, false);
    }

    // --- Windups: the caster held still, facing where it strikes --------------------------------------------------
    void BeginWindup(float facing)
    {
        Unit* caster = Caster();
        _windup = true;
        caster->StopMoving();
        caster->SetControlled(true, UNIT_STATE_ROOT);
        caster->SetFacingTo(facing);
    }

    void EndWindup()
    {
        if (!_windup)
            return;
        _windup = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
        if (Creature* demon = Velthazar())
            demon->SetControlled(false, UNIT_STATE_ROOT);
    }

    // The demon held still, no melee: an intermission
    void HoldDemon(bool held)
    {
        Creature* demon = Velthazar();
        boss_hollow_voice_velthazar* ai = VelthazarAI();
        if (!demon || !ai)
            return;
        ai->_held = held;
        if (held)
        {
            demon->AttackStop();
            demon->StopMoving();
            demon->GetMotionMaster()->Clear();
            demon->GetMotionMaster()->MoveIdle();
            demon->SetReactState(REACT_PASSIVE);
            return;
        }
        demon->ClearEmoteState();
        demon->SetReactState(REACT_AGGRESSIVE);
        if (Unit* target = demon->GetThreatMgr().GetCurrentVictim())
            demon->AI()->AttackStart(target);
        else
            DoZoneInCombat(demon, ArenaReach);
    }

    // --- The clock -------------------------------------------------------------------------------------------------
    void UpdateClock(bool skipping)
    {
        uint32 const elapsed = Elapsed();
        if (_phase == Phase::Aldric && elapsed >= AtLastRites && me->GetHealth() > HoldHealth())
            LastRites();
        if (_phase == Phase::Aldric && elapsed >= AtFall)
            Judge();
        if (_phase == Phase::Fallen && elapsed >= AtReveal)
            Reveal();
        while (_next < _timeline.size() && _timeline[_next].at <= elapsed && _phase != Phase::Over)
        {
            Ability const what = _timeline[_next++].what;
            if (!skipping || IsStageStep(what))
                Execute(what);
        }
        if (_phase != Phase::Hollow)
            return;
        while (_blasts < AtEnrageBlasts.size() && elapsed >= AtEnrageBlasts[_blasts])
            EnrageBlast(_blasts++);
        if (_blasts == AtEnrageBlasts.size() && elapsed >= _nextPulseMs)
        {
            _nextPulseMs = elapsed + EnragePulseMs;
            EnragePulse();
        }
    }

    void Execute(Ability what)
    {
        // Each boss's own: the Archbishop's wait for him standing, the demon's for the demon
        bool const demonStep = IsVelthazarStep(what);
        if (demonStep != (_phase == Phase::Hollow) || (!demonStep && _phase != Phase::Aldric))
            return;
        switch (what)
        {
            case Ability::Radiance:         Radiance(); break;
            case Ability::Judgement:        Judgement(); break;
            case Ability::LightOfDawn:      LightOfDawn(); break;
            case Ability::Hammers:          BlessedHammers(); break;
            case Ability::Pulpit:           WrathOfThePulpit(); break;
            case Ability::Aisles:           Aisles(false); break;
            case Ability::Choir:            Towers(false); break;
            case Ability::AbsolutionStart:  StartAbsolution(); break;
            case Ability::AbsolutionEnd:    EndAbsolution(); break;
            case Ability::Sentence:         ExecutionSentence(); break;
            case Ability::Verdict:          Verdict(); break;
            case Ability::Seraphim:         Seraphim(); break;
            case Ability::Wake:             WakeOfAshes(); break;
            case Ability::HollowPulse:      HollowPulse(); break;
            case Ability::Brand:            VampiricBrand(); break;
            case Ability::Swarm:            CarrionSwarm(); break;
            case Ability::EchoAisles:       Aisles(true); break;
            case Ability::EchoPulpit:       EchoPulpit(); break;
            case Ability::Whisper:          WhisperOfDoubt(); break;
            case Ability::LastLightStart:   StartLastLight(); break;
            case Ability::LastLightPulse:   LastLightPulse(); break;
            case Ability::LastLightEnd:     EndLastLight(); break;
            case Ability::Lances:           NightmareLances(); break;
            case Ability::Infernals:        DreadInfernals(); break;
            case Ability::InhaleStart:      Inhale(); break;
            case Ability::TrueForm:         TrueForm(); break;
            case Ability::SpinSwarm:        SpinningSwarms(); break;
            case Ability::LastPrayerStart:  StartLastPrayer(); break;
            case Ability::LastPrayerPulse:  HitEveryone(SPELL_HOLLOW_PULSE, PrayerPulsePct); break;
            case Ability::LastPrayerEnd:    EndLastPrayer(); break;
            case Ability::Sermon:           Towers(true); break;
            case Ability::VoidCross:        VoidCrosses(); break;
            case Ability::Ruin:             VoiceOfRuin(); break;
        }
    }

    // --- The Archbishop ----------------------------------------------------------------------------------------------
    // Holy Radiance: everyone, the healers' rhythm
    void Radiance()
    {
        me->SendPlaySpellVisual(KIT_HOLY_NOVA_CAST);
        HitEveryone(SPELL_RADIANCE, _seraphim ? RadianceSeraphimPct : RadiancePct);
    }

    // Judgement: his tank, and Condemned on them (holy damage taken, CondemnedMs): two tanks trading him keep it low
    void Judgement()
    {
        Player* tank = me->GetVictim() ? me->GetVictim()->ToPlayer() : nullptr;
        if (!tank || _kneeling)
            return;
        Sound(SOUND_JUDGEMENT);
        tank->SendPlaySpellVisual(KIT_JUDGEMENT_HIT);
        Hit(tank, SPELL_JUDGEMENT, JudgementPct, false);
        auto& condemned = _condemned[tank->GetGUID()];
        bool const lapsed = getMSTimeDiff(condemned.second, getMSTime()) < 0x80000000u;
        condemned.first = std::min(CondemnedMaxStacks, (lapsed ? 0u : condemned.first) + 1);
        condemned.second = getMSTime() + CondemnedMs;
        ShowStacks(tank, SPELL_CONDEMNED, condemned.first, CondemnedMs);
    }

    // Light of Dawn: a wide cone at someone who is no tank
    void LightOfDawn()
    {
        std::vector<Player*> const players = NonTanks();
        if (players.empty() || _kneeling)
            return;
        Player* target = Acore::Containers::SelectRandomContainerElement(players);
        Position const apex = Ground(me->GetPosition());
        float const facing = apex.GetAngle(target);
        BeginWindup(facing);
        me->SendPlaySpellVisual(KIT_DIVINE_STORM_CAST);
        GroundIndicators::Area const area = Paint(ConeArea(apex, facing,
            LightOfDawnRadius, LightOfDawnArc), PAINT_DAWN, LightOfDawnWarningMs, GroundIndicators::Theme::Holy);
        scheduler.Schedule(Milliseconds(LightOfDawnWarningMs), [this, area](TaskContext)
        {
            Sound(SOUND_LIGHT_OF_DAWN);
            Strike(area, PAINT_DAWN_HIT);
            for (Player* player : PlayersIn(area))
                Hit(player, SPELL_LIGHT_OF_DAWN, LightOfDawnPct, true);
            EndWindup();
        });
    }

    // Blessed Hammers: three hammers spiral out from his feet across the chamber; each hits whoever it passes
    void BlessedHammers()
    {
        Position const center = Ground(me->GetPosition());
        float const base = frand(0.0f, 2.0f * float(M_PI));
        float const turn = roll_chance_i(50) ? HammerTurn : -HammerTurn;
        Sound(SOUND_BLESSED_HAMMERS);
        me->SendPlaySpellVisual(KIT_HAMMER_CAST);
        auto hammerAt = [center, base, turn](uint32 hammer, float seconds)
        {
            float const angle = base + float(hammer) * 2.0f * float(M_PI) / float(HammerCount) + turn * seconds;
            float const radius = HammerStart + HammerGrowth * seconds;
            return Position(center.GetPositionX() + std::cos(angle) * radius,
                            center.GetPositionY() + std::sin(angle) * radius, center.GetPositionZ());
        };
        // The hammers: a floating hammer each, flown along its spiral
        for (uint32 hammer = 0; hammer < HammerCount; ++hammer)
        {
            Position const first = hammerAt(hammer, 0.0f);
            Creature* stalker = FxStalker(Position(first.GetPositionX(), first.GetPositionY(),
                first.GetPositionZ() + HammerHeight), HammerMs + 500, NPC_HOVER_STALKER);
            if (!stalker)
                continue;
            stalker->AddAura(SPELL_HAMMER_FX, stalker);
            Movement::MoveSplineInit init(stalker);
            Movement::PointsArray path;
            path.push_back(G3D::Vector3(first.GetPositionX(), first.GetPositionY(),
                                        first.GetPositionZ() + HammerHeight));
            float length = 0.0f;
            Position last = first;
            for (uint32 point = 1; point <= 24; ++point)
            {
                Position const at = hammerAt(hammer, float(HammerMs) / 1000.0f * float(point) / 24.0f);
                length += last.GetExactDist2d(&at);
                last = at;
                path.push_back(G3D::Vector3(at.GetPositionX(), at.GetPositionY(), at.GetPositionZ() + HammerHeight));
            }
            init.MovebyPath(path);
            init.SetSmooth();
            init.SetFly();
            init.SetVelocity(length / (float(HammerMs) / 1000.0f));
            init.Launch();
        }
        // The hits, and the bots shown where the hammers go next
        auto hit = std::make_shared<std::set<std::pair<uint32, ObjectGuid>>>();
        for (uint32 at = 0; at <= HammerMs; at += LaserTickMs)
            scheduler.Schedule(Milliseconds(at), [this, at, hammerAt, hit](TaskContext)
            {
                float const seconds = float(at) / 1000.0f;
                for (uint32 hammer = 0; hammer < HammerCount; ++hammer)
                {
                    Position const now = hammerAt(hammer, seconds);
                    // The bots see where each hammer goes in the next 1.5 s
                    if (at % 300 == 0)
                        for (float ahead : { 0.0f, 0.3f, 0.6f, 0.9f, 1.2f, 1.5f })
                            GroundIndicators::WatchArea(me, CircleArea(hammerAt(hammer, seconds + ahead),
                                HammerRadius + 1.0f), 400);
                    for (Player* player : ArenaPlayers())
                        if (player->GetExactDist2d(&now) <= HammerRadius &&
                            hit->insert({ hammer, player->GetGUID() }).second)
                        {
                            player->SendPlaySpellVisual(KIT_HAMMER_HIT);
                            Hit(player, SPELL_HAMMERS, HammerPct, true);
                        }
                }
            });
    }

    // Wrath of the Pulpit: three rings of holy fire from his feet, one after the other: step across, in or out
    void WrathOfThePulpit()
    {
        Position const center = Ground(me->GetPosition());
        me->SendPlaySpellVisual(KIT_HOLY_WRATH_CAST);
        for (uint32 wave = 0; wave < PulpitRings.size(); ++wave)
        {
            auto const [outer, inner] = PulpitRings[wave];
            uint32 const lands = PulpitWarningMs + wave * PulpitWaveMs;
            // Painted as two proportions: 0.6 (the first) and 0.8
            bool const wide = inner / outer < 0.7f;
            GroundIndicators::Area const ring = Paint(RingArea(center, outer, inner),
                wide ? PAINT_PULPIT60 : PAINT_PULPIT80, lands, GroundIndicators::Theme::Holy);
            scheduler.Schedule(Milliseconds(lands), [this, ring, wave, wide](TaskContext)
            {
                if (wave == 0)
                    Sound(SOUND_WRATH_OF_THE_PULPIT);
                Strike(ring, wide ? PAINT_PULPIT60_HIT : PAINT_PULPIT80_HIT);
                for (Player* player : PlayersIn(ring))
                    Hit(player, SPELL_PULPIT, PulpitPct, true);
            });
        }
    }

    // Hollow Echo's rings: the Pulpit inverted - his core and everything past a band burn, the band is safe
    void EchoPulpit()
    {
        Unit* caster = Caster();
        Position const center = Ground(caster->GetPosition());
        Sound(SOUND_HOLLOW_ECHO);
        caster->SendPlaySpellVisual(KIT_SHADOW_CRASH_CAST);
        std::vector<GroundIndicators::Area> areas;
        areas.push_back(Paint(CircleArea(center, EchoCoreRadius), PAINT_ECHO_CORE,
            EchoWarningMs, GroundIndicators::Theme::Shadow));
        areas.push_back(Paint(RingArea(center, ArenaReach, EchoOuterInner),
            PAINT_ECHO_RING, EchoWarningMs, GroundIndicators::Theme::Shadow));
        scheduler.Schedule(Milliseconds(EchoWarningMs), [this, areas](TaskContext)
        {
            Strike(areas[0], PAINT_ECHO_CORE_HIT);
            Strike(areas[1], PAINT_ECHO_RING_HIT);
            for (Player* player : ArenaPlayers())
                if (std::ranges::any_of(areas, [player](GroundIndicators::Area const& area)
                    { return area.Contains(*player); }))
                    Hit(player, SPELL_ECHO, EchoPct, true);
        });
    }

    // Consecrated Aisles: the chamber in lanes across a random way, every other one burning; Hollow Echo's inverted
    // ones (the demon's) burn on the lanes the Archbishop's left
    void Aisles(bool echo)
    {
        Position const center = Center();
        float const facing = frand(0.0f, float(M_PI));
        float const across = facing + float(M_PI) / 2.0f;
        uint32 const burning = echo ? 1u : 0u;
        Sound(echo ? SOUND_HOLLOW_ECHO : SOUND_CONSECRATED_AISLES);
        std::vector<GroundIndicators::Area> lanes;
        for (uint32 lane = burning; lane < AisleCount; lane += 2)
        {
            float const offset = (float(lane) - float(AisleCount - 1) / 2.0f) * AisleWidth;
            Position const middle(center.GetPositionX() + std::cos(across) * offset,
                                  center.GetPositionY() + std::sin(across) * offset, center.GetPositionZ());
            Position const start = Ground(Position(middle.GetPositionX() - std::cos(facing) * AisleLength / 2.0f,
                middle.GetPositionY() - std::sin(facing) * AisleLength / 2.0f, center.GetPositionZ()));
            lanes.push_back(Paint(LineArea(start, facing, AisleLength, AisleWidth),
                echo ? PAINT_ECHO_AISLES : PAINT_AISLES, AisleWarningMs));
        }
        scheduler.Schedule(Milliseconds(AisleWarningMs), [this, lanes, echo](TaskContext)
        {
            for (GroundIndicators::Area const& lane : lanes)
                Strike(lane, echo ? PAINT_ECHO_AISLES_HIT : PAINT_AISLES_HIT);
            for (Player* player : ArenaPlayers())
                if (std::ranges::any_of(lanes, [player](GroundIndicators::Area const& lane)
                    { return lane.Contains(*player); }))
                    Hit(player, echo ? SPELL_ECHO : SPELL_AISLES, echo ? EchoPct : AislePct, true);
        });
    }

    // Choir of the Faithful (the Archbishop) and the Hollow Sermon (the demon): towers for two players each, their
    // sigil on the floor, and a Bastion tower only a tank may hold. A tower held by fewer bursts on everyone.
    void Towers(bool sermon)
    {
        Unit* caster = Caster();
        Position const center = Center();
        // Three towers of two and a Bastion: six soakers and a tank, what a group of ten has with its tank on the
        // boss and a healer to spare (four towers asked for every one of them: a single death failed one)
        uint32 const count = 3;
        float const base = frand(0.0f, 2.0f * float(M_PI));
        Sound(sermon ? SOUND_HOLLOW_ECHO : SOUND_CHOIR);
        caster->SendPlaySpellVisual(sermon ? KIT_DARKNESS : KIT_HYMN_CAST);
        std::vector<Position> towers;
        for (uint32 index = 0; index < count; ++index)
        {
            float const angle = base + float(index) * 2.0f * float(M_PI) / float(count) + frand(-0.25f, 0.25f);
            Position const spot = AtAngle(center, angle, frand(17.0f, 26.0f));
            towers.push_back(spot);
            ShowTower(spot, angle, TowerRadius, TowerMs, 2, false, !sermon,
                sermon ? GroundIndicators::SPELL_SIGIL_VOID : GroundIndicators::SPELL_SIGIL_RADIANT,
                sermon ? LOOK_SIGIL_VOID_LIT : LOOK_SIGIL_RADIANT_LIT);
            GroundIndicators::ShowSoak(Caster(), spot, TowerRadius, TowerMs, 2,
                sermon ? GroundIndicators::Theme::Shadow : GroundIndicators::Theme::Holy);
        }
        // The Bastion: between two towers, nearer the middle; the tank not on the boss goes
        float const bastionAngle = base + float(M_PI) / float(count);
        Position const bastion = AtAngle(center, bastionAngle, 12.0f);
        ShowTower(bastion, bastionAngle, TowerRadius, TowerMs, 1, true, true, GroundIndicators::SPELL_SIGIL_BASTION,
            LOOK_SIGIL_BASTION_LIT);
        GroundIndicators::SetOffTankSpot(Caster(), bastion, TowerMs, true);

        scheduler.Schedule(Milliseconds(TowerMs), [this, towers, bastion, sermon](TaskContext)
        {
            NamedSpell const& spell = sermon ? SPELL_SERMON : SPELL_CHOIR;
            uint32 failed = 0;
            for (Position const& tower : towers)
            {
                PlayOnGround(tower, sermon ? KIT_SHADOW_NOVA_HIT : KIT_HOLY_WRATH_HIT);
                std::vector<Player*> const soakers = PlayersIn(CircleArea(tower, TowerRadius));
                Resolved(sermon ? "Hollow Sermon" : "Choir of the Faithful", soakers.size() >= 2,
                         Acore::StringFormat("tower soakers={}", soakers.size()));
                if (soakers.size() >= 2)
                {
                    float const shared = (sermon ? SermonSharedPct : TowerSharedPct) / float(soakers.size());
                    for (Player* player : soakers)
                        Hit(player, spell, shared, false);
                }
                else
                {
                    ++failed;
                    for (Player* player : soakers)
                        Hit(player, spell, (sermon ? SermonSharedPct : TowerSharedPct) / 2.0f, false);
                }
            }
            PlayOnGround(bastion, KIT_WRATH_HAMMER_HIT);
            std::vector<Player*> const inBastion = PlayersIn(CircleArea(bastion, TowerRadius));
            bool const held = std::ranges::any_of(inBastion, [](Player* player) { return IsGroupTank(player); });
            Resolved(sermon ? "Hollow Sermon" : "Choir of the Faithful", held,
                     Acore::StringFormat("bastion inside={}", inBastion.size()));
            for (Player* player : inBastion)
                Hit(player, spell, IsGroupTank(player) ? BastionTankPct : BastionOtherPct, !IsGroupTank(player));
            float burst = float(failed) * (sermon ? SermonFailPct : TowerFailPct);
            if (!held)
                burst += BastionFailPct;
            if (burst > 0.0f)
            {
                Caster()->SendPlaySpellVisual(sermon ? KIT_SHADOW_NOVA_CAST : KIT_HOLY_NOVA_CAST);
                HitEveryone(spell, burst);
            }
        });
    }

    // Prayer of Absolution (the break): he kneels under an aegis; broken in time, he stands; not, he heals
    void StartAbsolution()
    {
        EndWindup();
        _kneeling = true;
        _absolutionShield = uint32(float(me->GetMaxHealth()) * AbsolutionShieldPct / 100.0f);
        Sound(SOUND_ABSOLUTION);
        me->AttackStop();
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        AddTimedAura(me, SPELL_ABSOLUTION_AURA, AbsolutionMs);
        Decal(Ground(me->GetPosition()), me->GetOrientation(), AbsolutionSigilRadius,
            AbsolutionMs, GroundIndicators::SPELL_SIGIL_AEGIS);
    }

    void BreakAbsolution()
    {
        GroundIndicators::Burst(me, Ground(me->GetPosition()), GroundIndicators::Theme::Holy);
        StandUp();
    }

    void EndAbsolution()
    {
        if (!_kneeling)
            return;
        Resolved("Prayer of Absolution", _absolutionShield == 0, Acore::StringFormat("aegis left={}",
                 _absolutionShield));
        if (_absolutionShield)
        {
            // Not broken: he rises healed
            me->SendPlaySpellVisual(KIT_HOLY_NOVA_CAST);
            me->ModifyHealth(int32(float(me->GetMaxHealth()) * AbsolutionHealPct / 100.0f));
        }
        StandUp();
    }

    void StandUp()
    {
        _kneeling = false;
        _absolutionShield = 0;
        me->RemoveAurasDueToSpell(SPELL_ABSOLUTION_AURA);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        if (Unit* target = me->GetThreatMgr().GetCurrentVictim())
            AttackStart(target);
    }

    // Execution Sentence: two marked who are no tanks, apart; after SentenceMs it falls on the spot they were marked
    // on, split between everyone standing in its circle - stand with them
    void ExecutionSentence()
    {
        for (Player* marked : SpreadPick(NonTanks(), 2))
        {
            Position const spot = Ground(marked->GetPosition());
            AddTimedAura(marked, SPELL_SENTENCE_MARK, SentenceMs);
            AddTimedAura(marked, SPELL_FX_MARK, SentenceMs);
            ShowTower(spot, 0.0f, SentenceRadius, SentenceMs, 3, false, true, PAINT_SENTENCE, PAINT_SENTENCE);
            GroundIndicators::ShowSoak(Caster(), spot, SentenceRadius, SentenceMs, SentenceSoakersBots);
            scheduler.Schedule(Milliseconds(SentenceMs), [this, spot](TaskContext)
            {
                Sound(SOUND_EXECUTION_SENTENCE);
                PlayOnGround(spot, KIT_WRATH_HAMMER_HIT);
                Scorch(spot, SentenceRadius, PAINT_HOLY_SCORCH);
                std::vector<Player*> const soakers = PlayersIn(CircleArea(spot, SentenceRadius));
                Resolved("Execution Sentence", soakers.size() >= 3, Acore::StringFormat("soakers={}", soakers.size()));
                if (soakers.empty())
                    return;
                float const shared = SentencePct / float(soakers.size());
                for (Player* player : soakers)
                    Hit(player, SPELL_SENTENCE, shared, false);
            });
        }
    }

    // Verdict of the Faithful (on the 1:25.5 hit): a giant hammer marks a spot; a tank takes it, anyone else in it is
    // crushed, and nobody holding it strikes everyone
    void Verdict()
    {
        Unit* victim = me->GetVictim();
        float const away = victim ? me->GetAngle(victim) + float(M_PI) : frand(0.0f, 2.0f * float(M_PI));
        Position const spot = AtAngle(me->GetPosition(), away + frand(-0.6f, 0.6f), 11.0f);
        Decal(spot, away, VerdictRadius, VerdictMs, PAINT_VERDICT);
        GroundIndicators::SetOffTankSpot(Caster(), spot, VerdictMs, true);
        me->SendPlaySpellVisual(KIT_HOLY_WRATH_CAST);
        scheduler.Schedule(Milliseconds(VerdictMs), [this, spot](TaskContext)
        {
            Sound(SOUND_VERDICT);
            PlayOnGround(spot, KIT_WRATH_HAMMER_HIT);
            GroundIndicators::Burst(me, spot, GroundIndicators::Theme::Holy);
            Scorch(spot, VerdictRadius * 1.3f, PAINT_HOLY_SCORCH);
            std::vector<Player*> const inside = PlayersIn(CircleArea(spot, VerdictRadius));
            bool const held = std::ranges::any_of(inside, [](Player* player) { return IsGroupTank(player); });
            Resolved("Verdict of the Faithful", held, Acore::StringFormat("inside={}", inside.size()));
            for (Player* player : inside)
                Hit(player, SPELL_VERDICT, IsGroupTank(player) ? VerdictTankPct : VerdictOtherPct,
                    !IsGroupTank(player));
            if (!held)
                HitEveryone(SPELL_VERDICT, VerdictFailPct);
        });
    }

    // Seraphim (the final climax): his wings; the rotation quickens (the timeline) and Radiance grows
    void Seraphim()
    {
        EndAbsolution();
        _seraphim = true;
        Sound(SOUND_SERAPHIM);
        me->SendPlaySpellVisual(KIT_AVENGING_WRATH);
        if (sSpellMgr->GetSpellInfo(SPELL_SERAPHIM_AURA))
            me->AddAura(SPELL_SERAPHIM_AURA, me);
    }

    // Wake of Ashes: three cones of light, one after the other, turning across the side the group stands on
    void WakeOfAshes()
    {
        std::vector<Player*> const players = NonTanks();
        if (players.empty() || _kneeling)
            return;
        float x = 0.0f;
        float y = 0.0f;
        for (Player* player : players)
        {
            x += player->GetPositionX();
            y += player->GetPositionY();
        }
        Position const apex = Ground(me->GetPosition());
        float const toward = apex.GetAngle(x / float(players.size()), y / float(players.size()));
        float const turn = roll_chance_i(50) ? 1.0f : -1.0f;
        BeginWindup(toward);
        Sound(SOUND_WAKE_OF_ASHES);
        for (uint32 cone = 0; cone < WakeCones; ++cone)
        {
            float const facing = Position::NormalizeOrientation(toward + turn *
                (float(cone) - 1.0f) * WakeArc * float(M_PI) / 180.0f);
            scheduler.Schedule(Milliseconds(cone * WakeStepMs), [this, apex, facing, cone](TaskContext)
            {
                GroundIndicators::Area const area = Paint(ConeArea(apex, facing,
                    WakeRadius, WakeArc), PAINT_WAKE, WakeWarningMs, GroundIndicators::Theme::Holy);
                scheduler.Schedule(Milliseconds(WakeWarningMs), [this, area, cone](TaskContext)
                {
                    me->SetFacingTo(area.origin.GetOrientation());
                    Strike(area, PAINT_WAKE_HIT);
                    for (Player* player : PlayersIn(area))
                        Hit(player, SPELL_WAKE, WakePct, true);
                    if (cone + 1 == WakeCones)
                        EndWindup();
                });
            });
        }
    }

    // --- The verdict and the fall, the reveal -----------------------------------------------------------------------
    // 1:59.7, his track fading: at 1% he falls, the demon about to show (above it the verdict, 1:57.5, has already
    // ended it: this is a safeguard)
    void Judge()
    {
        if (me->GetHealth() > HoldHealth())
        {
            LastRites();
            return;
        }
        StandUp();
        EndWindup();
        scheduler.CancelAll();
        _phase = Phase::Fallen;
        Talk(SAY_ALDRIC_FALLS);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        me->RemoveAurasDueToSpell(SPELL_SERAPHIM_AURA);
        me->GetMotionMaster()->Clear();
        me->StopMoving();
        GroundIndicators::ClearAreasOf(me);
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                player->RemoveAurasDueToSpell(SPELL_CONDEMNED);
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Aldric falls instance={}", me->GetInstanceId());
    }

    // Too slow: a holy judgement on everyone in the chamber, and the demon is never seen
    void LastRites()
    {
        _phase = Phase::Over;
        scheduler.CancelAll();
        Talk(SAY_ALDRIC_LAST_RITES);
        // The track ends now, 2.5 s before the demon's part (2:00): its fade is over, the group never hears him
        EndTrack(MUSIC_SILENCE);
        me->PlayDirectSound(SOUND_LAST_RITES);
        me->SendPlaySpellVisual(KIT_HOLY_NOVA_CAST);
        for (Player* player : ArenaPlayers())
        {
            player->SendPlaySpellVisual(KIT_HOLY_WRATH_HIT);
            MythicTuning::DealAbilityDamage(me, player, SpellOf(SPELL_LAST_RITES), player->GetMaxHealth());
            if (player->IsAlive())
                Unit::Kill(me, player);
        }
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Last Rites (too slow) instance={} health={:.1f}%",
                 me->GetInstanceId(), me->GetHealthPct());
        Wipe();
    }

    // 2:01.5, the second track's first hit: Vel'thazar tears out of the Archbishop, who is no longer seen
    void Reveal()
    {
        _phase = Phase::Hollow;
        me->PlayDirectSound(SOUND_REVEAL);
        me->SendPlaySpellVisual(KIT_METAMORPHOSIS_PRE);
        Position const at = me->GetPosition();
        Creature* demon = me->SummonCreature(NPC_VELTHAZAR, at, TEMPSUMMON_MANUAL_DESPAWN);
        me->SetVisible(false);
        if (!demon)
        {
            LOG_ERROR("module.hollowvoice", "The Hollow Voice: Vel'thazar could not be summoned instance={}",
                      me->GetInstanceId());
            return;
        }
        demon->SendPlaySpellVisual(KIT_METAMORPHOSIS);
        demon->AI()->Talk(SAY_VELTHAZAR_REVEAL);
        demon->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(demon, ArenaReach);
        for (Player* player : ArenaPlayers())
        {
            player->SendPlaySpellVisual(KIT_SHADOW_NOVA_HIT);
            Hit(player, SPELL_TEAR, RevealPulsePct, false);
        }
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Vel'thazar revealed instance={} health={}",
                 me->GetInstanceId(), demon->GetMaxHealth());
    }

    // --- Vel'thazar -------------------------------------------------------------------------------------------------
    void HollowPulse()
    {
        Caster()->SendPlaySpellVisual(KIT_SHADOW_NOVA_CAST);
        HitEveryone(SPELL_HOLLOW_PULSE, _trueForm ? HollowPulseTruePct : HollowPulsePct);
    }

    // Vampiric Brand: his tank, and a stack on them; from BrandFeedStacks he feeds on the hit
    void VampiricBrand()
    {
        Creature* demon = Velthazar();
        Player* tank = demon && demon->GetVictim() ? demon->GetVictim()->ToPlayer() : nullptr;
        if (!tank || (VelthazarAI() && VelthazarAI()->_held))
            return;
        auto& brand = _brand[tank->GetGUID()];
        bool const lapsed = getMSTimeDiff(brand.second, getMSTime()) < 0x80000000u;
        uint32 const stacks = lapsed ? 0u : brand.first;
        Sound(SOUND_VAMPIRIC_BRAND);
        tank->SendPlaySpellVisual(KIT_VAMPIRIC_HIT);
        Hit(tank, SPELL_BRAND, BrandPct * (1.0f + BrandPerStackPct / 100.0f * float(stacks)), false);
        if (stacks >= BrandFeedStacks)
            demon->ModifyHealth(int32(float(demon->GetMaxHealth()) * BrandHealPct / 100.0f));
        brand.first = std::min(stacks + 1, 5u);
        brand.second = getMSTime() + BrandMs;
        ShowStacks(tank, SPELL_BRAND_STACKS, brand.first, BrandMs);
    }

    // Carrion Swarm: a cone of swarming void at someone who is no tank
    void CarrionSwarm()
    {
        Creature* demon = Velthazar();
        std::vector<Player*> const players = NonTanks();
        if (!demon || players.empty() || VelthazarAI()->_held)
            return;
        Player* target = Acore::Containers::SelectRandomContainerElement(players);
        Position const apex = Ground(demon->GetPosition());
        float const facing = apex.GetAngle(target);
        BeginWindup(facing);
        demon->SendPlaySpellVisual(KIT_CARRION_CAST);
        GroundIndicators::Area const area = Paint(ConeArea(apex, facing, SwarmRadius,
            SwarmArc), PAINT_CARRION, SwarmWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(SwarmWarningMs), [this, area](TaskContext)
        {
            Sound(SOUND_CARRION_SWARM);
            Strike(area, PAINT_CARRION_HIT);
            for (Player* player : PlayersIn(area))
                Hit(player, SPELL_SWARM, SwarmPct, true);
            EndWindup();
        });
    }

    // Whisper of Doubt (on the 0:47 hit): three marked carry a circle; whoever else is in one when it lands takes it
    void WhisperOfDoubt()
    {
        for (Player* carrier : SpreadPick(NonTanks(), WhisperCarriers))
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, WhisperRadius,
                WhisperMs);
            ObjectGuid const guid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(WhisperMs), [this, area, guid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                    return;
                carrier->SendPlaySpellVisual(KIT_FEAR_HIT);
                Hit(carrier, SPELL_WHISPER, WhisperCarrierPct, false);
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                        Hit(player, SPELL_WHISPER, WhisperOtherPct, true);
            });
        }
    }

    // Last Light: the void pulses; three pools of the Archbishop's light, shrinking, are the only shelter
    void StartLastLight()
    {
        EndWindup();
        HoldDemon(true);
        Sound(SOUND_VOICE_OF_RUIN);
        if (Creature* demon = Velthazar())
        {
            demon->SendPlaySpellVisual(KIT_DARKNESS);
            demon->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        }
        _pools.clear();
        float const base = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 pool = 0; pool < LastLightPools; ++pool)
            _pools.push_back(AtAngle(Center(), base + float(pool) * 2.0f * float(M_PI) / float(LastLightPools),
                LastLightPoolDistance));
        _poolRadius = LastLightPoolRadii[0];
        for (uint32 step = 0; step < LastLightPoolRadii.size(); ++step)
            scheduler.Schedule(Milliseconds(step * LastLightShrinkMs), [this, step](TaskContext)
            {
                _poolRadius = LastLightPoolRadii[step];
                uint32 const lasts = step + 1 == LastLightPoolRadii.size() ?
                    (AtLastLightEnd - AtLastLight) - step * LastLightShrinkMs : LastLightShrinkMs;
                for (Position const& pool : _pools)
                {
                    Decal(pool, 0.0f, _poolRadius, lasts,
                        GroundIndicators::SPELL_SIGIL_AEGIS);
                    GroundIndicators::ShowSoak(Caster(), pool, _poolRadius, lasts, 4, GroundIndicators::Theme::Holy,
                        true);
                }
            });
    }

    void LastLightPulse()
    {
        if (_pools.empty())
            return;
        Caster()->SendPlaySpellVisual(KIT_SHADOW_NOVA_CAST);
        uint32 out = 0;
        std::string names;
        for (Player* player : ArenaPlayers())
            if (std::ranges::none_of(_pools, [this, player](Position const& pool)
                { return player->GetExactDist2d(&pool) <= _poolRadius; }))
            {
                ++out;
                names += Acore::StringFormat(" {}({}{})", player->GetName(), IsGroupTank(player) ? "tank " : "",
                    uint32(player->GetExactDist2d(&_pools.front())));
            }
        Resolved("Last Light", out == 0, Acore::StringFormat("outside={}{}", out, names));
        for (Player* player : ArenaPlayers())
        {
            bool const sheltered = std::ranges::any_of(_pools, [this, player](Position const& pool)
                { return player->GetExactDist2d(&pool) <= _poolRadius; });
            if (!sheltered)
                player->SendPlaySpellVisual(KIT_SHADOWFURY_HIT);
            Hit(player, SPELL_LAST_LIGHT, sheltered ? LastLightInsidePct : LastLightOutsidePct, false);
        }
    }

    void EndLastLight()
    {
        _pools.clear();
        HoldDemon(false);
    }

    // Nightmare Lances: a void lance from where he stands toward each of two marked, fixed where they stood when marked
    // and painted on the ground, a purple beam along it; when it lands it pierces the whole line - step off it. The
    // marked are struck through whatever they do. (A line following its marked had the marked dodge their own line and
    // swing it through the group: nine hits in the red a test.)
    void NightmareLances()
    {
        Creature* demon = Velthazar();
        if (!demon)
            return;
        Position const source = Ground(demon->GetPosition());
        Sound(SOUND_NIGHTMARE_LANCES);
        for (Player* marked : SpreadPick(NonTanks(), LanceMarked))
        {
            float const facing = source.GetAngle(marked);
            GroundIndicators::Area const line = Paint(LineArea(source, facing,
                LanceLength, LanceWidth), PAINT_LANCE, LanceMs);
            Creature* from = FxStalker(Position(source.GetPositionX(), source.GetPositionY(),
                source.GetPositionZ() + 2.0f), LanceMs + 300, NPC_HOVER_STALKER);
            Position const lineEnd = AtAngle(source, facing, LanceLength);
            Creature* to = FxStalker(Position(lineEnd.GetPositionX(), lineEnd.GetPositionY(),
                lineEnd.GetPositionZ() + 1.0f),
                LanceMs + 300);
            if (from && to)
            {
                to->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                from->CastSpell(to, SPELL_FX_PURPLE_BEAM, true);
            }
            AddTimedAura(marked, SPELL_FX_MARK, LanceMs);
            ObjectGuid const guid = marked->GetGUID();
            scheduler.Schedule(Milliseconds(LanceMs), [this, line, guid](TaskContext)
            {
                Strike(line, PAINT_LANCE_HIT);
                Player* marked = ObjectAccessor::GetPlayer(*me, guid);
                if (marked && marked->IsAlive())
                    Hit(marked, SPELL_LANCES, LanceMarkedPct, false);
                for (Player* player : PlayersIn(line))
                    if (player != marked)
                        Hit(player, SPELL_LANCES, LancePct, true);
            });
        }
    }

    // Dread Infernals (the 1:43.5 hit): two impacts to share, then two infernals an off-tank holds
    void DreadInfernals()
    {
        std::vector<Position> spots;
        for (uint32 index = 0; index < InfernalCount; ++index)
        {
            Position spot = ArenaSpot(12.0f, 24.0f);
            for (uint32 attempt = 0; attempt < 6 && !spots.empty() && spot.GetExactDist2d(&spots.front()) < 18.0f;
                 ++attempt)
                spot = ArenaSpot(12.0f, 24.0f);
            spots.push_back(spot);
            ShowTower(spot, 0.0f, InfernalRadius, InfernalWarningMs, 2, false, false, PAINT_INFERNAL, PAINT_INFERNAL,
                false);
            GroundIndicators::ShowSoak(Caster(), spot, InfernalRadius, InfernalWarningMs, InfernalSoakersBots,
                GroundIndicators::Theme::Fire);
        }
        Caster()->SendPlaySpellVisual(KIT_SHADOW_CRASH_CAST);
        scheduler.Schedule(Milliseconds(InfernalWarningMs), [this, spots](TaskContext)
        {
            uint32 failed = 0;
            for (Position const& spot : spots)
            {
                PlayOnGround(spot, KIT_INFERNO_HIT);
                GroundIndicators::Burst(me, spot, GroundIndicators::Theme::Fire);
                Scorch(spot, InfernalRadius * 1.2f, PAINT_VOID_SCORCH);
                std::vector<Player*> const soakers = PlayersIn(CircleArea(spot, InfernalRadius));
                Resolved("Dread Infernal impact", soakers.size() >= 2, Acore::StringFormat("soakers={}",
                         soakers.size()));
                if (soakers.size() >= 2)
                    for (Player* player : soakers)
                        Hit(player, SPELL_INFERNAL, InfernalSharedPct / float(soakers.size()), false);
                else
                    ++failed;
                if (Creature* infernal = me->SummonCreature(NPC_INFERNAL, spot, TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                    10000))
                {
                    infernal->SetReactState(REACT_AGGRESSIVE);
                    DoZoneInCombat(infernal, ArenaReach);
                }
            }
            if (failed)
                HitEveryone(SPELL_INFERNAL, InfernalFailPct * float(failed));
        });
    }

    // Inhale of the Void (the near silence): he draws everyone in while void falls; on the drop, all near him struck
    void Inhale()
    {
        Creature* demon = Velthazar();
        if (!demon)
            return;
        EndWindup();
        HoldDemon(true);
        demon->SendPlaySpellVisual(KIT_THOUSAND_SOULS);
        demon->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        Position const center = Ground(demon->GetPosition());
        uint32 const lasts = AtTrueForm - AtInhale;
        _inhaleCore = Paint(CircleArea(center, InhaleBlastRadius), PAINT_INHALE, lasts,
            GroundIndicators::Theme::Shadow);
        for (uint32 at = 1000; at < lasts; at += 1000)
            scheduler.Schedule(Milliseconds(at), [this, center](TaskContext)
            {
                for (Player* player : ArenaPlayers())
                    if (player->GetExactDist2d(&center) > 3.0f)
                        // A knockback from the far side of the player: towards him (bots take no negative speed)
                        player->KnockbackFrom(2.0f * player->GetPositionX() - center.GetPositionX(),
                            2.0f * player->GetPositionY() - center.GetPositionY(), InhalePullSpeed, 1.5f);
            });
        for (uint32 at = 500; at + InhaleVoidWarningMs < lasts; at += 1500)
            scheduler.Schedule(Milliseconds(at), [this](TaskContext)
            {
                for (uint32 index = 0; index < 3; ++index)
                {
                    GroundIndicators::Area const area = GroundIndicators::ShowPainted(me,
                        CircleArea(ArenaSpot(InhaleBlastRadius + 2.0f, ArenaWalkRadius), InhaleVoidRadius),
                        PAINT_ECHO_CORE, InhaleVoidWarningMs, GroundIndicators::Theme::Shadow);
                    scheduler.Schedule(Milliseconds(InhaleVoidWarningMs), [this, area](TaskContext)
                    {
                        PlayOnGround(area.origin, KIT_SHADOWFURY_HIT);
                        Strike(area, PAINT_ECHO_CORE_HIT);
                        Scorch(area.origin, area.radius, PAINT_VOID_SCORCH);
                        for (Player* player : PlayersIn(area))
                            Hit(player, SPELL_INHALE, InhaleVoidPct, true);
                    });
                }
            });
    }

    // The big drop: the inhale's blast, then his true form - bigger, the chamber's edge devoured for good
    void TrueForm()
    {
        Creature* demon = Velthazar();
        if (demon && _inhaleCore.radius > 0.0f)
        {
            GroundIndicators::Burst(me, _inhaleCore.origin, GroundIndicators::Theme::Shadow);
            for (Player* player : PlayersIn(_inhaleCore))
                Hit(player, SPELL_INHALE, InhaleBlastPct, true);
        }
        _inhaleCore = GroundIndicators::Area();
        _trueForm = true;
        me->PlayDirectSound(SOUND_REVEAL);
        if (demon)
        {
            demon->SetObjectScale(TrueFormScale);
            demon->SendPlaySpellVisual(KIT_METAMORPHOSIS);
            if (sSpellMgr->GetSpellInfo(SPELL_TRUE_FORM))
                demon->AddAura(SPELL_TRUE_FORM, demon);
        }
        HitEveryone(SPELL_TEAR, TrueFormPulsePct);
        _edgeArea = Paint(RingArea(Center(), EdgeOuterRadius, EdgeInnerRadius),
            PAINT_EDGE, AtEnrageBlasts[2] + 30000 - Elapsed(), GroundIndicators::Theme::Shadow);
        _edge = true;
        HoldDemon(false);
    }

    void EdgeTick()
    {
        for (Player* player : PlayersIn(_edgeArea))
            Hit(player, SPELL_EDGE, EdgePct, true);
    }

    // Three swarms spinning round him: lines from where he stands, turning; run ahead of them, never through
    void SpinningSwarms()
    {
        Creature* demon = Velthazar();
        if (!demon || VelthazarAI()->_held)
            return;
        Position const center = Ground(demon->GetPosition());
        float const base = frand(0.0f, 2.0f * float(M_PI));
        float const turn = roll_chance_i(50) ? SpinSwarmTurn : -SpinSwarmTurn;
        Sound(SOUND_CARRION_SWARM);
        demon->SendPlaySpellVisual(KIT_SHADOWFLAME_CAST);
        demon->SetControlled(true, UNIT_STATE_ROOT);
        _windup = true;
        for (uint32 arm = 0; arm < SpinSwarmArms; ++arm)
            GroundIndicators::ShowSweepingRectangle(me, center, base + float(arm) * 2.0f * float(M_PI) /
                float(SpinSwarmArms), turn, SpinSwarmLength, SpinSwarmWidth, SpinSwarmMs, 0, PAINT_SWARM);
        auto lastHit = std::make_shared<std::map<ObjectGuid, uint32>>();
        for (uint32 at = 0; at <= SpinSwarmMs; at += LaserTickMs)
            scheduler.Schedule(Milliseconds(at), [this, at, center, base, turn, lastHit](TaskContext)
            {
                for (Player* player : ArenaPlayers())
                {
                    auto const last = lastHit->find(player->GetGUID());
                    if (last != lastHit->end() && at < last->second + SpinSwarmHitEveryMs)
                        continue;
                    for (uint32 arm = 0; arm < SpinSwarmArms; ++arm)
                    {
                        float const angle = base + float(arm) * 2.0f * float(M_PI) / float(SpinSwarmArms) +
                            turn * float(at) / 1000.0f;
                        if (player->GetExactDist2d(&center) > SpinSwarmCore &&
                            InLine(center, angle, SpinSwarmLength, SpinSwarmWidth, *player))
                        {
                            (*lastHit)[player->GetGUID()] = at;
                            player->SendPlaySpellVisual(KIT_CARRION_HIT);
                            Hit(player, SPELL_SPIN_SWARM, SpinSwarmPct, true);
                            break;
                        }
                    }
                }
            });
        scheduler.Schedule(Milliseconds(SpinSwarmMs + 100), [this](TaskContext) { EndWindup(); });
    }

    // Aldric's Last Prayer (the quiet verse): the demon imprisoned; four lights of the Archbishop's in the chamber,
    // each picked up and carried into him cracks the prison and blesses its carrier
    void StartLastPrayer()
    {
        Creature* demon = Velthazar();
        if (!demon)
            return;
        EndWindup();
        HoldDemon(true);
        VelthazarAI()->_imprisoned = true;
        Sound(SOUND_ABSOLUTION);
        demon->SendPlaySpellVisual(KIT_HOLY_WRATH_CAST);
        if (sSpellMgr->GetSpellInfo(SPELL_PRISON_AURA))
            demon->AddAura(SPELL_PRISON_AURA, demon);
        _prisonCracks = 0;
        _prisonLights.clear();
        uint32 const lasts = AtLastPrayerEnd - AtLastPrayer;
        float const base = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 index = 0; index < PrayerLights; ++index)
        {
            Position const spot = AtAngle(Center(), base + float(index) * 2.0f * float(M_PI) / float(PrayerLights),
                PrayerLightDistance);
            Creature* light = FxStalker(spot, lasts, NPC_HOVER_STALKER);
            if (!light)
                continue;
            light->AddAura(SPELL_LIGHT_FX, light);
            Decal(spot, 0.0f, PrayerLightRadius, lasts,
                GroundIndicators::SPELL_SIGIL_AEGIS);
            GroundIndicators::ShowSoak(Caster(), spot, PrayerLightRadius, lasts, 1);
            _prisonLights.push_back(light->GetGUID());
        }
        for (uint32 at = 250; at < lasts; at += 250)
            scheduler.Schedule(Milliseconds(at), [this](TaskContext) { UpdatePrayerLights(); });
    }

    // A light with a player on it is picked up; it reaches the demon PrayerCarryMs later
    void UpdatePrayerLights()
    {
        for (ObjectGuid& guid : _prisonLights)
        {
            Creature* light = guid ? me->GetMap()->GetCreature(guid) : nullptr;
            if (!light)
                continue;
            Player* carrier = nullptr;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(light) <= PrayerLightRadius)
                {
                    carrier = player;
                    break;
                }
            if (!carrier)
                continue;
            guid = ObjectGuid::Empty;
            light->DespawnOrUnsummon();
            AddTimedAura(carrier, SPELL_CARRY_LIGHT, PrayerCarryMs);
            carrier->SendPlaySpellVisual(KIT_HOLY_WRATH_HIT);
            ObjectGuid const carrierGuid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(PrayerCarryMs), [this, carrierGuid](TaskContext)
            {
                Creature* demon = Velthazar();
                if (!demon || !VelthazarAI()->_imprisoned)
                    return;
                demon->SendPlaySpellVisual(KIT_HOLY_WRATH_HIT);
                if (Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid))
                    if (carrier->IsAlive())
                        AddTimedAura(carrier, SPELL_BLESSING, 20000);
                if (++_prisonCracks >= PrayerLights)
                    BreakPrison();
            });
        }
    }

    // The prison broken before the climax: he fights on, the lights' carriers blessed
    void BreakPrison()
    {
        Creature* demon = Velthazar();
        if (!demon)
            return;
        VelthazarAI()->_imprisoned = false;
        demon->RemoveAurasDueToSpell(SPELL_PRISON_AURA);
        GroundIndicators::Burst(me, Ground(demon->GetPosition()), GroundIndicators::Theme::Holy);
        HoldDemon(false);
    }

    void EndLastPrayer()
    {
        Creature* demon = Velthazar();
        uint32 const left = PrayerLights - std::min(_prisonCracks, PrayerLights);
        Resolved("Aldric's Last Prayer", left == 0, Acore::StringFormat("lights carried={}", _prisonCracks));
        for (ObjectGuid const& guid : _prisonLights)
            if (Creature* light = guid ? me->GetMap()->GetCreature(guid) : nullptr)
                light->DespawnOrUnsummon();
        _prisonLights.clear();
        if (demon && VelthazarAI()->_imprisoned)
        {
            // Not broken: the void bursts out, once for each light left
            Sound(SOUND_VOICE_OF_RUIN);
            demon->SendPlaySpellVisual(KIT_SHADOW_NOVA_CAST);
            HitEveryone(SPELL_HOLLOW_PULSE, PrayerFailPct * float(left));
            BreakPrison();
        }
    }

    // Crosses of void through where he stands, two waves, the second turned 45 degrees
    void VoidCrosses()
    {
        Creature* demon = Velthazar();
        if (!demon)
            return;
        Position const center = Ground(demon->GetPosition());
        float const first = frand(0.0f, float(M_PI) / 2.0f);
        for (uint32 wave = 0; wave < CrossWaves; ++wave)
        {
            float const facing = first + float(wave) * float(M_PI) / 4.0f;
            scheduler.Schedule(Milliseconds(wave * CrossWaveMs), [this, center, facing](TaskContext)
            {
                std::vector<GroundIndicators::Area> lines;
                for (float arm : { facing, facing + float(M_PI) / 2.0f })
                {
                    Position const start = Ground(Position(center.GetPositionX() - std::cos(arm) * CrossLength,
                        center.GetPositionY() - std::sin(arm) * CrossLength, center.GetPositionZ()));
                    lines.push_back(Paint(LineArea(start, arm, CrossLength * 2.0f,
                        CrossWidth), PAINT_CROSS, CrossWarningMs));
                }
                scheduler.Schedule(Milliseconds(CrossWarningMs), [this, lines](TaskContext)
                {
                    for (GroundIndicators::Area const& line : lines)
                        Strike(line, PAINT_CROSS_HIT);
                    for (Player* player : ArenaPlayers())
                        if (std::ranges::any_of(lines, [player](GroundIndicators::Area const& line)
                            { return line.Contains(*player); }))
                            Hit(player, SPELL_VOID_CROSS, VoidCrossPct, true);
                });
            });
        }
    }

    // Voice of Ruin (the three stabs): near death for everyone; under Aldric's last Aegis, which a tank must hold,
    // half. The Aegis is laid on his tank, the group stacks in it.
    void VoiceOfRuin()
    {
        Creature* demon = Velthazar();
        Unit* tank = demon ? demon->GetVictim() : nullptr;
        Position const spot = Ground(tank ? tank->GetPosition() : AtAngle(Center(), 0.0f, 8.0f));
        Decal(spot, 0.0f, RuinAegisRadius, RuinWarningMs,
            GroundIndicators::SPELL_SIGIL_AEGIS);
        GroundIndicators::ShowSoak(Caster(), spot, RuinAegisRadius, RuinWarningMs, 10, GroundIndicators::Theme::Holy,
            true);
        scheduler.Schedule(Milliseconds(RuinWarningMs - 2000), [this](TaskContext)
        {
            if (Creature* demon = Velthazar())
                demon->SendPlaySpellVisual(KIT_THOUSAND_SOULS);
        });
        scheduler.Schedule(Milliseconds(RuinWarningMs), [this, spot](TaskContext)
        {
            Sound(SOUND_VOICE_OF_RUIN);
            GroundIndicators::Area const aegis = CircleArea(spot, RuinAegisRadius);
            std::vector<Player*> const inside = PlayersIn(aegis);
            bool const held = std::ranges::any_of(inside, [](Player* player) { return IsGroupTank(player); });
            Resolved("Voice of Ruin", held && inside.size() + 1 >= ArenaPlayers().size(),
                     Acore::StringFormat("inside={} of {} tank={}", inside.size(), ArenaPlayers().size(), held));
            GroundIndicators::Burst(me, spot, GroundIndicators::Theme::Holy);
            for (Player* player : ArenaPlayers())
            {
                bool const sheltered = held && aegis.Contains(*player);
                player->SendPlaySpellVisual(KIT_SHADOW_NOVA_HIT);
                Hit(player, SPELL_RUIN, RuinPct * (sheltered ? RuinAegisShare : 1.0f), false);
            }
        });
    }

    // 7:17.1 / 7:17.7 / 7:18.1: three blasts of three times everyone's health (an immunity still holds)...
    void EnrageBlast(uint32 index)
    {
        Creature* demon = Velthazar();
        Unit* source = demon ? static_cast<Unit*>(demon) : me;
        if (demon && index == 0)
        {
            if (boss_hollow_voice_velthazar* ai = VelthazarAI())
                ai->_enraged = true;
            demon->AI()->Talk(SAY_VELTHAZAR_ENRAGE);
        }
        source->PlayDirectSound(SOUND_HARD_ENRAGE);
        source->SendPlaySpellVisual(KIT_SHADOW_NOVA_CAST);
        _inHit = true;
        for (Player* player : ArenaPlayers())
        {
            MythicTuning::DealAbilityDamage(source, player, SpellOf(SPELL_SILENCE),
                                            uint32(float(player->GetMaxHealth()) * EnrageBlastHealthPct / 100.0f));
            if (!player->IsAlive())
                ++_stats["Silence"].deaths;
        }
        _inHit = false;
    }

    // ... then a pulse every second that kills, whatever protects them
    void EnragePulse()
    {
        Creature* demon = Velthazar();
        Unit* source = demon ? static_cast<Unit*>(demon) : me;
        _inHit = true;
        for (Player* player : ArenaPlayers())
        {
            Unit::Kill(source, player);
            ++_stats["Silence"].deaths;
        }
        _inHit = false;
    }

    // --- Wipes -----------------------------------------------------------------------------------------------------
    // Every half second: nobody standing is a wipe; bots alone are finished off (RaidFinder already counts the wipe
    // from the last player down), so the players do not watch their bots fight on
    void UpdateStanding()
    {
        if (_phase == Phase::Over || _finishing)
            return;
        uint32 const elapsed = Elapsed();
        if (elapsed < _nextStandingCheckMs)
            return;
        _nextStandingCheckMs = elapsed + 500;

        std::vector<Player*> const standing = ArenaPlayers();
        if (standing.empty())
        {
            Wipe();
            return;
        }
        if (std::ranges::any_of(standing, [](Player* player) { return IsRealPlayer(player); }))
            return;

        _finishing = true;
        for (uint32 pulse = 0; pulse < FinishPulses; ++pulse)
            scheduler.Schedule(Milliseconds(pulse * FinishPulseMs), [this, pulse](TaskContext)
            {
                Creature* demon = Velthazar();
                Unit* source = demon ? static_cast<Unit*>(demon) : me;
                source->PlayDirectSound(demon ? SOUND_VOICE_OF_RUIN : SOUND_LAST_RITES);
                if (pulse + 1 == FinishPulses)
                    for (Player* player : ArenaPlayers())
                        Unit::Kill(source, player);
            });
    }

    // The end is seen: everything stands WipeLinger, the music playing on, then the track ends for those who heard it
    // and the Archbishop goes, back at his spot a few seconds later
    void Wipe()
    {
        if (_lingering)
            return;
        _lingering = true;
        _phase = Phase::Over;
        scheduler.CancelAll();
        EndWindup();
        GroundIndicators::ClearAreasOf(me);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        if (Creature* demon = Velthazar())
        {
            demon->AttackStop();
            demon->SetReactState(REACT_PASSIVE);
        }
        uint32 const elapsed = Elapsed();
        LOG_INFO("module.hollowvoice", "The Hollow Voice wipe instance={} elapsed={}ms", me->GetInstanceId(), elapsed);
        LogReport();
        // Before the demon's part of the track: it ends first, so a wipe never lets it be heard (Last Rites, a wipe at
        // the end of the Archbishop's phase)
        if (elapsed < AtSecondTrack && elapsed + uint32(WipeLinger.count()) + TrackEndMarginMs > AtSecondTrack)
        {
            uint32 const before = AtSecondTrack > elapsed + TrackEndMarginMs ?
                AtSecondTrack - elapsed - TrackEndMarginMs : 0;
            me->m_Events.AddEventAtOffset([this]() { EndTrack(MUSIC_SILENCE); }, Milliseconds(before));
        }
        me->m_Events.AddEventAtOffset([this]()
        {
            EndTrack(MUSIC_SILENCE);
            _summons.DespawnAll();
            ClearPlayerAuras();
            me->CombatStop(true);
            me->SetVisible(true);
            me->DespawnOnEvade(WipeRespawnDelay);
        }, WipeLinger);
    }

    SummonList _summons;
    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    std::size_t _next = 0;
    uint32 _pullMs = 0;
    bool _defiConfirmed = false;
    bool _lingering = false;
    bool _revealArmed = false;
    bool _finishing = false;
    bool _kneeling = false;                     // Prayer of Absolution
    bool _seraphim = false;
    bool _windup = false;
    bool _trueForm = false;
    bool _edge = false;
    uint32 _absolutionShield = 0;
    uint32 _defiWaitMs = 0;
    uint32 _checkTimer = 0;
    uint32 _nextStandingCheckMs = 0;
    uint32 _blasts = 0;
    uint32 _nextPulseMs = 0;
    uint32 _lastKillYellMs = 0;
    uint32 _prisonCracks = 0;
    float _poolRadius = 0.0f;
    std::vector<Position> _pools;
    std::vector<ObjectGuid> _prisonLights;
    GroundIndicators::Area _inhaleCore;
    GroundIndicators::Area _edgeArea;
    std::set<ObjectGuid> _fightListeners;
    // A soak drawn as a tower (FFXIV's): waiting - its sigil breathing, an empty gem round its rim for each soaker it
    // asks for - the gems lighting up as players stand in it, then held - its sigil lit and turning fast, a pillar
    // of light over it - once it has them all (for a Bastion: a tank). Someone stepping out takes it back.
    struct Tower
    {
        Position center;
        float radius = 0.0f;
        uint32 needed = 0;
        bool tanksOnly = false;
        bool pillar = true;
        uint32 waiting = 0;                     // the sigil's looks
        uint32 held = 0;
        uint32 pipEmpty = 0;
        uint32 pipLit = 0;
        ObjectGuid sigil;
        ObjectGuid light;
        std::vector<ObjectGuid> pips;
        uint32 lit = 0;
        bool full = false;
        uint32 endsAt = 0;                      // getMSTime
    };
    std::vector<Tower> _towers;
    uint32 _nextTowerCheck = 0;
    std::map<std::string, AbilityStats> _stats;
    bool _inHit = false;                        // an ability's damage being dealt: a kill now is its own
    // Stacks by player: how many, and when they lapse (getMSTime)
    std::map<ObjectGuid, std::pair<uint32, uint32>> _condemned;
    std::map<ObjectGuid, std::pair<uint32, uint32>> _brand;
};

void NoteKill(Creature* killer, Unit* victim)
{
    Creature* aldric = killer->GetEntry() == NPC_ALDRIC ? killer : nullptr;
    if (!aldric)
        if (TempSummon* summon = killer->ToTempSummon())
            aldric = summon->GetSummonerCreatureBase();
    if (auto* ai = aldric ? dynamic_cast<boss_hollow_voice_aldric*>(aldric->AI()) : nullptr)
        ai->MeleeDeath(victim);
}

boss_hollow_voice_aldric* FindAldric(Player* player)
{
    if (!player || !player->IsInWorld())
        return nullptr;
    Creature* aldric = player->FindNearestCreature(NPC_ALDRIC, 250.0f, true);
    return aldric ? dynamic_cast<boss_hollow_voice_aldric*>(aldric->AI()) : nullptr;
}

using namespace Acore::ChatCommands;

// .hollow info | skip <seconds> | reveal | pull | cast <ability> | music <aldric|velthazar|stop> | floor: for game
// masters trying the fight. The fight itself starts from the board: .defi start 930100 (hidden from the boards until
// it is revealed).
class HollowVoiceCommandScript final : public CommandScript
{
public:
    HollowVoiceCommandScript() : CommandScript("HollowVoiceCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable hollowTable = {
            { "info",   HandleInfo,   SEC_GAMEMASTER, Console::No },
            { "skip",   HandleSkip,   SEC_GAMEMASTER, Console::No },
            { "reveal", HandleReveal, SEC_GAMEMASTER, Console::No },
            { "pull",   HandlePull,   SEC_GAMEMASTER, Console::No },
            { "cast",   HandleCast,   SEC_GAMEMASTER, Console::No },
            { "music",  HandleMusic,  SEC_GAMEMASTER, Console::No },
            { "floor",  HandleFloor,  SEC_GAMEMASTER, Console::No },
            { "botsonly", HandleBotsOnly, SEC_GAMEMASTER, Console::Yes },
            { "report", HandleReport, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "hollow", hollowTable },
        };
        return commandTable;
    }

    static bool Fail(ChatHandler* handler, char const* text)
    {
        handler->SendSysMessage(text);
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        handler->SendSysMessage(aldric ? aldric->Describe() : std::string("The Archbishop is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        if (!aldric || !aldric->Skip(seconds))
            return Fail(handler, "The Hollow Voice is not fighting within 250 yards.");
        handler->SendSysMessage(aldric->Describe());
        return true;
    }

    // .hollow reveal, before the pull: the next pull starts at 1:50, the Archbishop at 1%, with its track from there
    static bool HandleReveal(ChatHandler* handler)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        if (!aldric || !aldric->ArmReveal())
            return Fail(handler, "No Archbishop out of combat within 250 yards: .hollow reveal arms the next pull.");
        handler->SendSysMessage("The next pull starts at 1:50, the Archbishop at 1%.");
        return true;
    }

    // .hollow cast <ability>: one of the fighting boss's abilities now (radiance judgement dawn hammers pulpit aisles
    // choir absolution sentence verdict seraphim wake | pulse brand swarm echoaisles echopulpit whisper lastlight
    // lances infernals inhale trueform spin prayer sermon cross ruin)
    static bool HandleCast(ChatHandler* handler, std::string what)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        if (!aldric || !aldric->CastNow(what))
            return Fail(handler, "No such ability for the boss fighting now, or no fight within 250 yards.");
        return true;
    }

    // .hollow pull: the Archbishop engages the game master (a test pull without walking up to him)
    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* aldric = player ? player->FindNearestCreature(NPC_ALDRIC, 150.0f) : nullptr;
        if (!aldric || !aldric->IsAlive())
            return Fail(handler, "No living Archbishop within 150 yards.");
        aldric->SetReactState(REACT_AGGRESSIVE);
        aldric->AI()->AttackStart(player);
        handler->SendSysMessage("The Archbishop pulled.");
        return true;
    }

    // .hollow floor: the chamber's floor round the Archbishop's spot, logged (module.hollowvoice): along each of 32
    // directions, how far the floor goes on at his height in his sight, and the height found every 3 yards
    static bool HandleFloor(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* aldric = player ? player->FindNearestCreature(NPC_ALDRIC, 250.0f) : nullptr;
        if (!aldric)
            return Fail(handler, "No Archbishop within 250 yards.");
        Position const home = aldric->GetHomePosition();
        Map* map = aldric->GetMap();
        for (uint32 direction = 0; direction < 32; ++direction)
        {
            float const angle = float(direction) * 2.0f * float(M_PI) / 32.0f;
            float reach = 0.0f;
            std::string heights;
            for (float distance = 3.0f; distance <= 72.0f; distance += 3.0f)
            {
                float const x = home.GetPositionX() + std::cos(angle) * distance;
                float const y = home.GetPositionY() + std::sin(angle) * distance;
                float const z = map->GetHeight(aldric->GetPhaseMask(), x, y, home.GetPositionZ() + 5.0f, true, 15.0f);
                bool const sight = map->isInLineOfSight(home.GetPositionX(), home.GetPositionY(),
                    home.GetPositionZ() + 2.0f, x, y, z + 2.0f, aldric->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                    VMAP::ModelIgnoreFlags::Nothing);
                heights += Acore::StringFormat(" {:.1f}{}", z, sight ? "" : "x");
                if (reach == distance - 3.0f && sight && std::fabs(z - home.GetPositionZ()) < 2.0f)
                    reach = distance;
            }
            LOG_INFO("module.hollowvoice", "floor angle={:.2f} reach={:.0f}:{}", angle, reach, heights);
        }
        handler->SendSysMessage("Floor logged (module.hollowvoice).");
        return true;
    }

    // .hollow botsonly <on|off>: the bots fight alone, the game master watching (testing the bots)
    static bool HandleBotsOnly(ChatHandler* handler, bool enable)
    {
        BotsOnly = enable;
        handler->SendSysMessage(enable ? "The Hollow Voice: bots only (the bots count as players)." :
                                         "The Hollow Voice: players and bots.");
        return true;
    }

    static bool HandleReport(ChatHandler* handler)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        if (!aldric)
            return Fail(handler, "The Archbishop is not within 250 yards.");
        handler->SendSysMessage(aldric->Report());
        return true;
    }

    static bool HandleMusic(ChatHandler* handler, std::string track)
    {
        uint32 const soundId = track == "aldric" ? MUSIC_ALDRIC : track == "velthazar" ? MUSIC_VELTHAZAR :
            track == "stop" ? MUSIC_SILENCE : 0;
        if (!soundId)
            return Fail(handler, ".hollow music <aldric|velthazar|stop>");
        Player* player = handler->GetPlayer();
        player->PlayDirectMusic(soundId, player);
        handler->SendSysMessage(Acore::StringFormat("SMSG_PLAY_MUSIC {} sent.", soundId));
        return true;
    }
};
}

void AddHollowVoiceScripts()
{
    RegisterCreatureAI(boss_hollow_voice_aldric);
    RegisterCreatureAI(boss_hollow_voice_velthazar);
    RegisterCreatureAI(npc_hollow_voice_infernal);
    new HollowVoiceCommandScript();
}
