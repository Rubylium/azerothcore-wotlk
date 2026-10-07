#include "GroundIndicators.h"
#include "FightMusic.h"
#include "LiveTuning.h"
#include "MythicDungeonSystem.h"
#include "MythicTuning.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GameTime.h"
#include "Group.h"
#include "LFG.h"
#include "Log.h"
#include "Map.h"
#include "MoveSplineInit.h"
#include "MythicDungeon.h"
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
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// L'Infini (the Infinite), the Défi board's god fight: a 10-player boss of the board's own in Ulduar's Celestial
// Planetarium, on Algalon's platform (plan: .agents/plans/infinite-boss). The fight runs on a fixed timeline set by its
// music (Sound\Music\Evolutions\LInfini.mp3 in patch-Z, 5:06), not on the boss's health: phases,
// intermissions and abilities come at set times from the pull, the Big Bang on the track's first drop, the god's
// true form on the second, the end of times as it fades, and at 5:05, in the silence, everyone dies.
//
// 0:00 Phase 1 "Voici ce que vous affrontez": Double fauchage cosmique (two cones, one on each tank), Pluie
//      d'étoiles (circles under players), Onde de gravité (raid-wide); 0:23 the god rises in the break, 0:28.2 Big Bang
//      on the drop: everything but its feet.
// 0:46.8 Intermission 1: it is exposed (+50% damage taken); two Fragments d'éternité walk to it (kill or slow them,
//      they heal it and burst on the raid when they reach it); a steady light raid damage.
// 1:12.5 Phase 2: phase 1 faster, plus Singularité (a black hole pulling players into its pools) and Étoile effondrée
//      (a star to share, three players or more).
// 1:54.8 Intermission 2 "Les cieux se déchirent": it channels in the middle, Constellation lines fire across the
//      platform, and Rayons stellaires mark three players with a star of four rays that follows them (its rays keep
//      their own direction): they keep it away from the others.
// 2:19 Setup: the platform's edge turns deadly, meteor waves. 2:32.9 the break: it rises, "Contemplez l'infini.", and
//      on the drop at 2:36.9 shows its true form (bigger, glowing, the Planetarium's floor turns to stars).
// 2:36.9 Phase 3: everything at once and faster, the cleave alternating with Frappes jumelles (a buster on each
//      tank), Supernova (all but three lanes), Jugement divin (carried circles: spread), Poids de l'éternité (+3%
//      damage taken every 10 s).
// 4:55 La Fin des Temps: a pulse every 2 s for 45% of everyone's maximum health, each one adding Fin imminente
//      (+20% damage taken). 5:05: everyone in the Planetarium dies, whatever protects them.
//
// Against chance (the rework of 2026-10-08): the fight stays hard, but no one hit and no random mark decides it.
// - A first mistake wounds instead of killing: Fêlure du temps, +50% damage taken for 15 s; a second one then kills.
// - Marked mechanics go to whoever can carry them (ranged first, then healers, melee last, never a tank; the Étoile
//   déchue to a real player first), each in turn, never to someone already carrying one.
// - Phases 2 and 3 run one big ability at a time, in the same order every pull, the small ones in the gaps between.
// - Supernova's lanes, the crosses, the constellation's lines, the black hole and the star come back to the same spots
//   in the same order: they can be learnt.
// - Résurgence: each intermission brings the fallen back, the god healing 5% for each one.
// - Fracture de l'éternité: each mechanic the whole group handles cleanly makes the god take 1% more (20 at most), a
//   failed one takes two back; with none it takes 5% less. The result follows the group's play.
// - Every death is logged with what killed it (module.infinite), to tune on evidence.
//
// No tank swap: two tanks, the boss aims a cone at each. Every avoidable hit is drawn in red first (GroundIndicators)
// and resolved against the very area drawn; bots step out of it, soak the star and keep the off-tank on the boss's
// side (GroundIndicators::FindGoal). Damage is a share of the reference health (below), and the Défi tier scales it.
//
// The god only exists in a challenge's instance: its static spawn hides at once and goes from any other Ulduar
// instance (IsChallengeInstanceFor, mod-playerbots RaidFinder.cpp). Its music is sent the way stock encounters send
// theirs (SMSG_PLAY_MUSIC, below).

// mod-playerbots (RaidFinder.cpp), built into the same modules library
bool IsChallengeInstanceFor(Map const* map, uint32 bossEntry);
float GetChallengeDamageFactorOf(Unit* attacker);
uint8 GetChallengeTierOf(Map const* map);

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// Its hits are shares of the health of a player of its Défi I profile (ChallengeTiers::BossProfiles: item level 300,
// 100 paragon), on the power model: 102 000. It was 122 000, set by play when its bots were scaled on the player they
// came with; they come at the profile now (a little under: 96 500), and every hit was a fifth heavier than meant.
constexpr float ProfileItemLevel = 300.0f;
constexpr float ProfileParagon = 100.0f;

constexpr float CleaveTankPct = 110.0f;         // Double fauchage cosmique, on each tank a cone is aimed at
constexpr float CleaveOtherPct = 160.0f;        // ... on anyone else in a cone
constexpr float TwinStrikePct = 130.0f;         // Frappes jumelles, on each tank (phase 3)
constexpr float StarfallPct = 55.0f;            // Pluie d'étoiles, each circle
constexpr float SweepPct = 60.0f;               // Rayon cosmique, the cone from the god
constexpr float FallenStarCarrierPct = 25.0f;   // Étoile déchue: the one marked...
constexpr float FallenStarMaxPct = 250.0f;      // ... everyone else, next to them (death), fading to nothing at
constexpr float FallenStarReach = 40.0f;        //     this distance
constexpr float FallenStarKeepAway = 25.0f;     // where its bot carrier stands from the others (35% of the hit)
// The off-tank's spot stays put until its tank has gone this far round the god (UpdateOffTankSpot)
constexpr float OffTankRebaseAngle = float(M_PI) / 3.0f;
constexpr float CrossWavePct = 70.0f;           // Croix céleste, each wave's lines
constexpr float OrbLaserPct = 80.0f;            // Lances de l'orbe, each pass of a laser (out, then back)
constexpr float SpinLaserPct = 90.0f;           // Rayon du Gardien, the laser swept round
constexpr float GravityPct[3] = { 24.0f, 30.0f, 18.0f };   // Onde de gravité, by phase (1, 2, 3)
constexpr float MeleeFloorPct = 8.0f;           // its melee on a player: at least this, whatever their armour
constexpr float TankArcanePct = 20.0f;          // Fracture stellaire: arcane on its target, every TankArcaneMs
constexpr uint32 TankArcaneMs = 6000;
// Each tier above Défi I: avoidable hits this much harder (on top of the tier's own damage), and more to dodge -
// Pluie d'étoiles a circle more every two tiers, Constellation a line more every three, Rayons stellaires a carrier
// more at Défi IV and VII, four Fragments d'éternité from Défi II
constexpr float AvoidablePctPerTier = 10.0f;
constexpr float BigBangPct = 140.0f;            // anyone off the god's feet
constexpr float RaidTickPct = 3.0f;             // intermissions, every 2 s
constexpr float FragmentBurstPct = 25.0f;       // a fragment reaching the god: the raid...
constexpr float FragmentHealPct = 4.0f;         // ... and it heals this share of its maximum health
constexpr float SingularityPoolPct = 12.0f;     // each second in the black hole's pool
constexpr float StarSharedPct = 180.0f;         // Étoile effondrée shared by 3 or more: split between them
constexpr float StarFailPct = 70.0f;            // ... fewer than 3: everyone
constexpr float ConstellationPct = 60.0f;       // a star line
constexpr float EdgePct = 25.0f;                // each second on the deadly edge (from 2:19)
constexpr float MeteorPct = 50.0f;              // a meteor of the setup's waves
constexpr float RevealPulsePct = 10.0f;         // the true form bursting out
constexpr float SupernovaPct = 150.0f;          // anywhere but the three lanes
constexpr float JudgementOtherPct = 90.0f;      // Jugement divin, to everyone else in a carried circle
constexpr float JudgementCarrierPct = 20.0f;    // ... to the one carrying it
constexpr float StarRaysOtherPct = 75.0f;       // Rayons stellaires, to everyone else in a marked star's rays
constexpr float StarRaysCarrierPct = 15.0f;     // ... to the one marked
constexpr float WeightPerStackPct = 3.0f;       // Poids de l'éternité: damage taken, every stack
constexpr float EndPulseHealthPct = 45.0f;      // La Fin des Temps, of the target's maximum health (not tier-scaled)
constexpr float DoomPerStackPct = 20.0f;        // Fin imminente: damage taken, every stack
constexpr float ExposedDamageTakenPct = 50.0f;  // intermission 1

// Against chance. A first avoidable hit (Hit) is held back to at most WoundHitPct of the player's maximum health and
// never takes them under WoundFloorPct; it leaves Fêlure du temps (WoundTakenPct more damage taken) for WoundMs, and
// an avoidable hit while it lasts is the whole hit. Hits within WoundGraceMs of the wounding one are the same mistake
// (two circles landing together), held back too.
LiveTuning::Knob const WoundHitPct("infini.wound_hit_pct", 75.0f);
LiveTuning::Knob const WoundTakenPct("infini.wound_taken_pct", 50.0f);
constexpr float WoundFloorPct = 10.0f;
constexpr uint32 WoundMs = 15000;
constexpr uint32 WoundGraceMs = 1000;
// Étoile déchue: a share of its hit above this on the others is theirs to avoid (nearer than FallenStarKeepAway)
constexpr float FallenStarAvoidablePct = 35.0f;
// Résurgence, each intermission: the fallen come back with this share of their health, the god healing for each
LiveTuning::Knob const ResurgenceHealPct("infini.resurgence_heal_pct", 5.0f);
constexpr float ResurgenceHealthPct = 50.0f;
// Fracture de l'éternité: damage the god takes, a share at no stack and more for each (each clean mechanic one, a
// failed one FractureLostOnFail back)
LiveTuning::Knob const FractureBasePct("infini.fracture_base_pct", 95.0f);
LiveTuning::Knob const FracturePerStackPct("infini.fracture_per_stack_pct", 1.0f);
constexpr uint32 FractureMaxStacks = 20;
constexpr uint32 FractureLostOnFail = 2;
// Phases 2 and 3: the gap after each big ability, where a small one (a cleave, a rain of stars) lands alone
constexpr uint32 SequenceGapMs = 3500;
// The learnt patterns: each Supernova's lanes this far round from the last one's, each constellation line too
constexpr float SupernovaTurn = 2.0f * float(M_PI) / 9.0f;
constexpr float ConstellationTurn = 0.96f;
constexpr float ConstellationOffset = 10.0f;   // the lines pass this far from the middle, by turns either side
constexpr float SingularityDistance = 15.0f;   // the black hole and the star, a third of a turn round each time
constexpr float StarDistance = 17.0f;

// Sizes (yards) and warnings (ms)
constexpr float CleaveRadius = 35.0f;
constexpr float CleaveArc = 60.0f;
constexpr uint32 CleaveWarningMs = 3000;          // the cones turn with their tanks until they land
constexpr float StarfallRadius = 5.0f;
constexpr float SweepRadius = 38.0f;            // Rayon cosmique: a cone from the god at a player who is no tank
constexpr float SweepArc = 50.0f;
constexpr uint32 SweepWarningMs = 2500;
// Étoile déchue (phase 1): a player who is no tank is marked, a star falls on them; the closer the others, the harder
constexpr uint32 FallenStarMs = 5500;
constexpr float FallenStarLethalRadius = 10.0f; // drawn red around the marked one: where it kills
// Croix céleste (phase 1): crosses through the god's feet one after the other, each turned 45 degrees from the last
constexpr uint32 CrossWaves = 4;
constexpr uint32 CrossWaveMs = 2300;            // the next wave drawn just before the last one lands
constexpr uint32 CrossWarningMs = 2500;         // time to read it and step out
constexpr float CrossLength = 42.0f;            // each arm, from the middle
constexpr float CrossWidth = 6.0f;
// Lances de l'orbe (phase 2): an orb over the god, then four lasers from it out across the platform and back
constexpr uint32 OrbChargeMs = 3500;
constexpr uint32 OrbLaserOutMs = 1600;
constexpr uint32 OrbLaserBackMs = 1600;
constexpr float OrbHeight = 16.0f;
constexpr float OrbScale = 0.6f;               // the sphere's size: its model at 1 filled the view
constexpr float OrbLaserReach = 44.0f;
constexpr float BeamEndDrop = 2.5f;            // a beam's far end under the floor: the beam lands on the ground
constexpr float OrbLaserWidth = 4.0f;
// Rayon du Gardien (phase 2): a laser straight ahead, then swept a full turn
constexpr uint32 SpinWarningMs = 2500;
constexpr float SpinSourceHeight = 1.0f;       // the beam's start just over the floor: a flat line
constexpr uint32 SpinMs = 9000;
constexpr float SpinReach = 46.0f;
constexpr float SpinWidth = 5.0f;
constexpr uint32 SpinHitEveryMs = 1500;         // one player is hit at most this often by it
constexpr uint32 LaserTickMs = 100;
// The hovering orb's stalker (a stock one that flies)
constexpr uint32 NPC_HOVER_STALKER = 15214;

// Their effects: stock spells and kits, harmless as they are used (0: none, the red on the ground alone)
constexpr uint32 SPELL_FX_BEAM = 64367;         // Algalon Event Beam: a thick blue beam, a dummy aura, channelled
// Unstable Sphere Passive: the Oculus's blue sphere, a dummy aura (Light Essence's white sphere, tried first, washed
// the screen out)
constexpr uint32 SPELL_FX_ORB = 50756;
constexpr uint32 SPELL_FX_MARK = 69275;         // Mark of Rimefang: a red reticle over the head, a dummy aura
constexpr uint32 KIT_FX_STAR_FALL = 11742;      // Malygos's platform breaking: a huge blue column and burst
constexpr uint32 StarfallWarningMs = 2000;
constexpr float BigBangSafeRadius = 9.0f;
constexpr float BigBangRadius = 45.0f;          // the ring reaches past the platform's edge
constexpr float SingularityRadius = 7.0f;
constexpr float SingularityPullReach = 35.0f;
constexpr float SingularityPullSpeed = 5.0f;
constexpr uint32 SingularityWarningMs = 1500;
constexpr uint32 SingularityTicks = 6;
constexpr float StarRadius = 6.0f;
constexpr uint32 StarSoakMs = 7000;
constexpr uint32 StarSoakers = 3;               // what the star asks for...
constexpr uint32 StarSoakersBots = 5;           // ... and how many the bots send, tanks and slow ones counted in
constexpr float ConstellationLength = 80.0f;
constexpr float ConstellationWidth = 6.0f;
constexpr uint32 ConstellationWarningMs = 2500;
constexpr float EdgeInnerRadius = 36.0f;
constexpr float EdgeOuterRadius = 60.0f;
constexpr float MeteorRadius = 6.0f;
constexpr uint32 MeteorWarningMs = 2000;
constexpr uint32 MeteorsPerWave = 4;
constexpr float SupernovaCoreRadius = 12.0f;
constexpr float SupernovaRadius = 60.0f;
constexpr float SupernovaArc = 90.0f;           // three cones, 120° apart: three 30° lanes between them
constexpr float SupernovaLaneDistance = 22.0f;  // where the lanes' golden marks stand
constexpr uint32 SupernovaWarningMs = 4000;
constexpr float JudgementRadius = 8.0f;
constexpr uint32 JudgementMs = 6000;
constexpr uint32 JudgementCarriers = 2;
constexpr uint32 StarRaysMs = 5000;
constexpr uint32 StarRaysCarriers = 3;
constexpr float RevealScale = 1.35f;
constexpr float LiftHeight = 6.0f;
constexpr uint32 FragmentCount = 2;             // at Défi I; FragmentCountHigh from Défi II
constexpr uint32 FragmentCountHigh = 4;
constexpr uint32 FinishPulses = 3;              // the bots left alone: pulses, the last one kills them
constexpr uint32 FinishPulseMs = 1000;
constexpr Milliseconds WipeLinger = 8s;         // a wipe: the god stands this long, the music on, before it goes
constexpr Seconds WipeRespawnDelay = 5s;       // a wipe despawns the god; it is back this long after
constexpr float FragmentReach = 1.5f;           // a fragment this close to the god's middle merges with it
constexpr float FragmentSpawnDistance = 34.0f;
// They walk: 1.5 yd/s (MOVE_WALK 2.5 x 0.6), about 22 s to the god's middle - the window to kill or slow them
// (running, they reached it in 5 s)
constexpr float FragmentWalkSpeedRate = 0.6f;

// The Planetarium: the middle of Algalon's platform (boss_algalon_the_observer.cpp), players this close are in it
Position const ArenaCenter = { 1632.668f, -302.7656f, 417.3211f, 1.53f };
constexpr float ArenaReach = 90.0f;
constexpr float ArenaFloorZ = 417.3211f;
constexpr float ArenaWalkRadius = 34.0f;        // random spots stay inside the deadly edge

// --- Timeline (ms from the pull), on the music's own beats (measured on the track's loudness) -----------------------
constexpr uint32 AtBigBangRise = 23000;         // the break begins
constexpr uint32 AtBigBangYell = 24000;
constexpr uint32 AtBigBang = 28200;             // the drop
constexpr uint32 AtIntermission1 = 46800;
constexpr uint32 AtFragments = 48500;
constexpr uint32 AtIntermission1Yell = 66000;
constexpr uint32 AtPhase2 = 72500;
constexpr uint32 AtIntermission2 = 114800;
constexpr uint32 AtSetup = 139000;
constexpr uint32 AtRevealRise = 152900;         // the second break
constexpr uint32 AtRevealYell = 153400;
constexpr uint32 AtPhase3 = 156900;             // the drop: the true form
constexpr uint32 AtFinal = 295000;
constexpr uint32 AtHardEnrage = 305000;
constexpr uint32 TrackLengthMs = 306000;

constexpr uint32 NPC_INFINI = 930000;
constexpr uint32 NPC_FRAGMENT = 930001;
constexpr uint32 NPC_COLLAPSING_STAR = 930002;
constexpr uint32 NPC_SINGULARITY = 930003;
constexpr uint32 NPC_STALKER = 900104;          // GroundIndicators' invisible stalker: kits played on the ground

// A recoloured Algalon (localTools/infiniteBoss): its own display once the client patch and the server's DBCs have
// it, Algalon's own until then
constexpr uint32 DISPLAY_INFINI = 60001;

// The Planetarium's doodads, drawn as doors of Algalon's (instance_ulduar.cpp): its resting look, and its fight's
// (the floor turned to stars, the globe), shown with the true form
constexpr uint32 GO_UNIVERSE_FLOOR_01 = 194715;
constexpr uint32 GO_UNIVERSE_FLOOR_02 = 194716;
constexpr uint32 GO_UNIVERSE_GLOBE = 194148;
constexpr uint32 GO_ALGALON_TRAPDOOR = 194253;

// The combat log names: the custom spell once the patch has it (localTools/infiniteBoss/Spells.ps1), a stock one of
// Algalon's until then
struct NamedSpell
{
    uint32 custom;
    uint32 stock;
};
constexpr NamedSpell SPELL_DOUBLE_CLEAVE = { 90740, 64395 };    // Quantum Strike
constexpr NamedSpell SPELL_TWIN_STRIKES = { 90741, 64412 };     // Phase Punch
constexpr NamedSpell SPELL_STARFALL = { 90742, 64596 };         // Cosmic Smash
constexpr NamedSpell SPELL_GRAVITY = { 90743, 64443 };          // Big Bang
constexpr NamedSpell SPELL_BIG_BANG = { 90744, 64443 };
constexpr NamedSpell SPELL_SINGULARITY = { 90745, 64122 };      // Black Hole Explosion
constexpr NamedSpell SPELL_COLLAPSING_STAR = { 90746, 64122 };
constexpr NamedSpell SPELL_CONSTELLATION = { 90747, 64596 };
constexpr NamedSpell SPELL_DEVOURING_VOID = { 90748, 64122 };
constexpr NamedSpell SPELL_METEORS = { 90749, 64596 };
constexpr NamedSpell SPELL_SUPERNOVA = { 90750, 64443 };
constexpr NamedSpell SPELL_JUDGEMENT = { 90751, 64443 };
constexpr NamedSpell SPELL_END_OF_TIMES = { 90752, 64487 };     // Ascend to the Heavens
constexpr NamedSpell SPELL_ETERNITY_SHARD = { 90755, 64443 };
constexpr NamedSpell SPELL_STAR_RAYS = { 90756, 64596 };
constexpr NamedSpell SPELL_SWEEP = { 90764, 64596 };            // Rayon cosmique
constexpr NamedSpell SPELL_FALLEN_STAR = { 90765, 64596 };      // Étoile déchue
constexpr NamedSpell SPELL_CROSS = { 90766, 64596 };            // Croix céleste
constexpr NamedSpell SPELL_ORB_LASER = { 90767, 64596 };        // Lances de l'orbe
constexpr NamedSpell SPELL_SPIN_LASER = { 90768, 64596 };       // Rayon du Gardien
constexpr NamedSpell SPELL_TANK_ARCANE = { 90763, 64412 };      // Fracture stellaire (Phase Punch)
// Debuffs the players see: dummy auras, the script does what they say
constexpr uint32 SPELL_DOOM = 90753;            // Fin imminente
constexpr uint32 SPELL_WEIGHT = 90754;          // Poids de l'éternité
constexpr uint32 SPELL_FELURE = 90790;          // Fêlure du temps
constexpr uint32 SPELL_FRACTURE = 90791;        // Fracture de l'éternité, on the god
// The true form: stock visual-only auras
constexpr uint32 SPELL_REVEAL_PARTICLES = 31954;    // Spirit Particles, super big
constexpr uint32 SPELL_REVEAL_GLOW = 49411;         // Arcane Power State

// Stock spell visual kits (SpellVisualKit.dbc), from Algalon's own spells
enum Kits : uint32
{
    KIT_BIG_BANG_GATHER     = 12688,
    KIT_BIG_BANG_BLAST      = 12211,    // Sunwell's beam, a flash over the whole screen: the hard enrage alone
    KIT_BIG_BANG_HIT        = 9168,
    KIT_ARRIVAL             = 12817,
    KIT_ASCEND_CAST         = 12706,
    KIT_ASCEND_HIT          = 9813,
    KIT_BLACK_HOLE_CAST     = 6995,
    KIT_BLACK_HOLE_HIT      = 2350,
    KIT_QUANTUM_STRIKE      = 12677,
    KIT_PHASE_PUNCH_CAST    = 449,
    KIT_PHASE_PUNCH_HIT     = 1005,
    KIT_COSMIC_SMASH        = 11835,
    KIT_REORIGINATION       = 12816,
    KIT_SINGULARITY         = 7775,
};

// creature_text of NPC_INFINI (stat_growth_infinite_god.sql), French in creature_text_locale
// Its lines only: the mechanics are never announced on screen, the players read them from the fight
enum Texts : uint8
{
    SAY_AGGRO               = 0,
    SAY_BIG_BANG            = 2,
    SAY_INTERMISSION_1      = 3,
    SAY_INTERMISSION_1_END  = 5,
    SAY_INTERMISSION_2      = 8,
    SAY_SETUP               = 9,
    SAY_REVEAL              = 11,
    SAY_FINAL               = 14,
    SAY_HARD_ENRAGE         = 15,
    SAY_KILL                = 16,
    SAY_DEATH               = 17,
};

enum class Phase : uint8
{
    None,
    One,
    Intermission1,
    Two,
    Intermission2,
    Setup,
    Three,
    Final,
    Over,
};

enum class Ability : uint8
{
    Cleave,
    TwinStrikes,
    Starfall,
    Gravity,
    TankArcane,
    Sweep,
    FallenStar,
    CrossWaves,
    OrbLasers,
    SpinLaser,
    BigBangRise,
    BigBangYell,
    BigBang,
    Intermission1,
    Fragments,
    Intermission1Yell,
    Phase2,
    RaidTick,
    Singularity,
    CollapsingStar,
    Intermission2,
    Constellation,
    StarRays,
    Setup,
    Meteors,
    RevealRise,
    RevealYell,
    Phase3,
    Supernova,
    Judgement,
    Weight,
    Final,
    EndPulse,
    HardEnrage,
};

// Steps that change the fight's state: a skip (.infini skip) still runs them
bool IsPhaseStep(Ability ability)
{
    switch (ability)
    {
        case Ability::Intermission1:
        case Ability::Phase2:
        case Ability::Intermission2:
        case Ability::Setup:
        case Ability::Phase3:
        case Ability::Final:
            return true;
        default:
            return false;
    }
}

struct Step
{
    uint32 at;
    Ability what;
};

// How long a big ability of phases 2 and 3 holds the stage, until it is over: the next one waits for it
uint32 BigBusyMs(Ability what)
{
    switch (what)
    {
        case Ability::Supernova:        return SupernovaWarningMs + 1000;
        case Ability::OrbLasers:        return OrbChargeMs + OrbLaserOutMs + OrbLaserBackMs + 500;
        case Ability::SpinLaser:        return SpinWarningMs + SpinMs + 500;
        case Ability::Singularity:      return SingularityWarningMs + SingularityTicks * 1000 + 500;
        case Ability::CollapsingStar:   return StarSoakMs + 500;
        case Ability::Judgement:        return JudgementMs + 500;
        default:                        return 3000;
    }
}

// The whole fight, in order. Repeating abilities are laid out here too, so the timeline reads as one table.
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps;
    auto every = [&steps](Ability what, uint32 first, uint32 period, uint32 until)
    {
        for (uint32 at = first; at < until; at += period)
            steps.push_back({ at, what });
    };
    auto once = [&steps](Ability what, uint32 at) { steps.push_back({ at, what }); };
    // One big ability at a time, each waiting for the last to be over, and in the gap after each a small one in turn:
    // never two orders at once. Stacked, a bot torn between leaving a cone and keeping its circle away from the
    // others lived or died on whichever won the moment. The same order every pull, so it can be learnt; `cycle` goes
    // round the list until `until`. `twinStrikes`: Frappes jumelles (nothing to dodge) during each big one.
    auto sequence = [&steps](std::vector<Ability> const& bigs, bool cycle, std::vector<Ability> const& smalls,
        uint32 from, uint32 until, bool twinStrikes)
    {
        uint32 at = from;
        for (std::size_t index = 0, gap = 0; cycle || index < bigs.size(); ++index)
        {
            Ability const big = bigs[index % bigs.size()];
            if (at + BigBusyMs(big) > until)
                break;
            steps.push_back({ at, big });
            if (twinStrikes)
                steps.push_back({ at + 1500, Ability::TwinStrikes });
            at += BigBusyMs(big);
            if (at + SequenceGapMs > until)
                break;
            steps.push_back({ at, smalls[gap++ % smalls.size()] });
            at += SequenceGapMs;
        }
    };

    // Phase 1: the Big Bang's break (23-29.5 s) keeps clear of everything else
    for (uint32 at : { 7000u, 18000u, 35000u })
        once(Ability::Cleave, at);
    // Phase 1: fewer circles, the fallen star and the crosses between them
    for (uint32 at : { 9000u, 20500u, 39000u })
        once(Ability::Starfall, at);
    for (uint32 at : { 14500u, 41500u })
        once(Ability::FallenStar, at);
    for (uint32 at : { 2500u, 30500u })
        once(Ability::CrossWaves, at);
    for (uint32 at : { 10000u, 20000u, 31500u, 40000u })
        once(Ability::Gravity, at);
    every(Ability::TankArcane, 4000, TankArcaneMs, AtIntermission1);
    once(Ability::Sweep, 12500);
    once(Ability::BigBangRise, AtBigBangRise);
    once(Ability::BigBangYell, AtBigBangYell);
    once(Ability::BigBang, AtBigBang);

    // Intermission 1
    once(Ability::Intermission1, AtIntermission1);
    once(Ability::Fragments, AtFragments);
    every(Ability::RaidTick, AtIntermission1 + 1200, 2000, AtPhase2 - 1000);
    once(Ability::Intermission1Yell, AtIntermission1Yell);

    // Phase 2: the orb's lasers, the black hole and the star one after the other, a cleave or a rain of stars between
    once(Ability::Phase2, AtPhase2);
    every(Ability::Gravity, AtPhase2 + 7500, 10000, AtIntermission2);
    every(Ability::TankArcane, AtPhase2 + 2000, TankArcaneMs, AtIntermission2);
    sequence({ Ability::OrbLasers, Ability::Singularity, Ability::CollapsingStar, Ability::OrbLasers }, false,
        { Ability::Cleave, Ability::Starfall }, AtPhase2 + 1500, AtIntermission2, false);

    // Intermission 2: the heavens tear
    once(Ability::Intermission2, AtIntermission2);
    every(Ability::Constellation, AtIntermission2 + 3000, 2500, AtSetup - 1500);
    every(Ability::StarRays, AtIntermission2 + 4000, 7000, AtSetup - StarRaysMs);
    every(Ability::RaidTick, AtIntermission2 + 1200, 2000, AtSetup - 1000);

    // Setup: the edge, meteor waves, then the break and the reveal
    once(Ability::Setup, AtSetup);
    every(Ability::Meteors, AtSetup + 1500, 2500, AtRevealRise - 1500);
    once(Ability::RevealRise, AtRevealRise);
    once(Ability::RevealYell, AtRevealYell);

    // Phase 3: the big ones in turn, round and round - Supernova, the black hole, Jugement divin, the orb's lasers, the
    // star, the Gardien's ray - Frappes jumelles during each, a rain of stars or a cleave in the gaps
    once(Ability::Phase3, AtPhase3);
    every(Ability::Gravity, AtPhase3 + 6000, 15000, AtFinal);
    every(Ability::Weight, AtPhase3 + 10000, 10000, AtFinal);
    every(Ability::TankArcane, AtPhase3 + 2000, TankArcaneMs, AtFinal);
    sequence({ Ability::Supernova, Ability::Singularity, Ability::Judgement, Ability::OrbLasers,
        Ability::CollapsingStar, Ability::SpinLaser }, true, { Ability::Starfall, Ability::Cleave }, AtPhase3 + 3000,
        AtFinal - 500, true);

    // The end of times, then silence
    once(Ability::Final, AtFinal);
    for (uint32 at = AtFinal; at < AtHardEnrage; at += 2000)
        once(Ability::EndPulse, at);
    once(Ability::HardEnrage, AtHardEnrage);

    std::stable_sort(steps.begin(), steps.end(), [](Step const& left, Step const& right)
    {
        // At the same time, the phase changes first
        if (left.at != right.at)
            return left.at < right.at;
        return IsPhaseStep(left.what) && !IsPhaseStep(right.what);
    });
    return steps;
}

// --- Music: SoundEntries sent with SMSG_PLAY_MUSIC (PlayDirectMusic), as stock encounters and world events do ------
// - L'Infini's track: 30100 (localTools/patchSinisterStrike.ps1, a copy of Algalon's own fight music entry 15877), on
//   the pull, once, to the players in the Planetarium then (sent again, this client starts it over; one arriving late
//   would hear it off the fight's timeline). It plays once (5:06).
// - A music is only ended by sending another: 30101, two seconds of silence, on a wipe and on the kill. The client
//   fades the track out, and the zone's own music comes back by itself later.
// - Nothing else is ever sent: a music sent while another music this server sent still plays dies a few seconds later,
//   once the old one's fade is over (measured: an arena's music after a wipe killed the next pull's track 7 s in).
// Out of combat nothing is sent: the Planetarium's own zone music (WMOAreaTable ZoneMusic 514, 15842) plays by itself
// and is the arena's tense music. Never send a zone's stock music entry (15842 was, as the arena's music): the
// client's zone music then stops whatever music plays about 20 s later, which cut the track a few seconds into every
// pull (measured on a live client with the speakers recorded, 2026-09-29: cut 8 s in with it sent, 83 s unbroken
// without).
constexpr uint32 MUSIC_FIGHT = 30100;
constexpr uint32 MUSIC_SILENCE = 30101;
constexpr float MusicReach = 120.0f;

uint32 SpellOf(NamedSpell const& spell)
{
    return sSpellMgr->GetSpellInfo(spell.custom) ? spell.custom : spell.stock;
}

float Reference()
{
    return Power::ExpectedPlayerHealth(ProfileItemLevel, ProfileParagon);
}

struct boss_infinite_god : public ScriptedAI
{
    boss_infinite_god(Creature* creature) : ScriptedAI(creature), _summons(creature) { }

    void InitializeAI() override
    {
        if (sCreatureDisplayInfoStore.LookupEntry(DISPLAY_INFINI))
        {
            me->SetDisplayId(DISPLAY_INFINI);
            me->SetNativeDisplayId(DISPLAY_INFINI);
        }
        // Hidden until its instance is known to be a challenge's (UpdateDefi)
        if (!_defiConfirmed)
            me->SetVisible(false);
        ScriptedAI::InitializeAI();
    }

    void Reset() override
    {
        ResetFight();
        // The board holds it until the group pulls (RaidFinder HoldChallengeBoss): nobody pulls it by landing
        me->SetReactState(REACT_PASSIVE);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (_lingering)
            return;
        bool const wiped = _phase != Phase::None && _phase != Phase::Over;
        if (wiped)
            LogSummary("wipe");
        ResetFight();
        if (wiped)
        {
            // The end is seen: the god stands WipeLinger, the music playing on (the hard enrage's silence, the track's
            // last bars), then the track fades out for those who heard it and it goes, back at its spot a few seconds
            // later - nothing to walk back to or reset in front of them
            _lingering = true;
            me->CombatStop(true);
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->m_Events.AddEventAtOffset([this]()
            {
                EndTrack(MUSIC_SILENCE);
                me->DespawnOnEvade(WipeRespawnDelay);
            }, WipeLinger);
            return;
        }
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, ArenaReach);
        _timeline = BuildTimeline();
        _next = 0;
        _pullMs = getMSTime();
        _phase = Phase::One;
        _nextOffTankMs = 0;
        _nextEdgeMs = 0;
        ResetRework();
        Talk(SAY_AGGRO);
        _fightListeners.clear();
        for (Player* player : Listeners())
        {
            SendMusic(player, MUSIC_FIGHT);
            _fightListeners.insert(player->GetGUID());
            FightMusic::Claim(player->GetGUID(), me->GetGUID());
        }
        LOG_INFO("module.infinite", "L'Infini pulled instance={} health={} tier factor={}", me->GetInstanceId(),
                 me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            RecordDeath(victim->ToPlayer());
        if (!victim->IsPlayer() || _phase == Phase::Final || _phase == Phase::Over)
            return;
        uint32 const now = getMSTime();
        if (_lastKillYellMs && getMSTimeDiff(_lastKillYellMs, now) < 10000)
            return;
        _lastKillYellMs = now;
        Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // The track stops with the god
        Talk(SAY_DEATH);
        EndTrack(MUSIC_SILENCE);
        LOG_INFO("module.infinite", "L'Infini killed instance={} elapsed={}ms", me->GetInstanceId(), Elapsed());
        LogSummary("kill");
        ResetFight();
        _phase = Phase::Over;
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        if (summon->GetEntry() != NPC_FRAGMENT)
            return;

        summon->SetReactState(REACT_PASSIVE);
        summon->SetWalk(true);
        DoZoneInCombat(summon, ArenaReach);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (summon->GetEntry() == NPC_FRAGMENT)
            MarkFragment();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return;
        float factor = (float(FractureBasePct) + float(FracturePerStackPct) * float(_fracture)) / 100.0f;
        if (_phase == Phase::Intermission1)
            factor *= 1.0f + ExposedDamageTakenPct / 100.0f;
        damage = uint32(float(damage) * factor);
    }

    // Its melee follows the players' Poids de l'éternité and Fin imminente, as its abilities do
    // Never less than MeleeFloorPct of the reference hit, whatever the target's armour: a tank has to be healed
    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (!victim || !victim->IsPlayer())
            return;
        if (type == DIRECT_DAMAGE)
        {
            float const floor = Reference() * MeleeFloorPct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
            damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
            _lastHit[victim->GetGUID()] = { 0, Elapsed(), false };
        }
        // A first mistake (Hit): held back to WoundHitPct of their health, and never under WoundFloorPct of it
        if (victim->GetGUID() == _sparing)
        {
            float const maximum = float(victim->GetMaxHealth());
            damage = std::min(damage, uint32(maximum * float(WoundHitPct) / 100.0f));
            uint32 const floor = uint32(maximum * WoundFloorPct / 100.0f);
            damage = std::min(damage, victim->GetHealth() > floor ? victim->GetHealth() - floor : 0u);
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_CENTER && _phase == Phase::Intermission2)
        {
            me->SetFacingTo(ArenaCenter.GetOrientation());
            me->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            UpdateOutOfCombat(diff);
            return;
        }

        scheduler.Update(diff);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && _timeline[_next].at <= elapsed && _phase != Phase::Over)
            Execute(_timeline[_next++].what);

        if (elapsed >= _nextOffTankMs)
        {
            _nextOffTankMs = elapsed + 1000;
            UpdateOffTankSpot();
        }
        if (_edge && elapsed >= _nextEdgeMs)
        {
            _nextEdgeMs = elapsed + 1000;
            EdgeTick();
        }
        if (_phase == Phase::Intermission1)
            UpdateFragments();
        if (elapsed >= _nextStandingCheckMs && !_finishing)
        {
            _nextStandingCheckMs = elapsed + 500;
            CheckPlayersStanding();
        }
        if (CanMelee())
            DoMeleeAttackIfReady();
    }

    // --- Music, and the game master's commands -----------------------------------------------------------------
    // What ends the track, to each player it was sent to, wherever they are now (a wipe's players may be home already,
    // the track still playing for them), unless they went on to another fight's track since (FightMusic.h)
    void EndTrack(uint32 soundId)
    {
        for (ObjectGuid const& guid : _fightListeners)
            if (FightMusic::Release(guid, me->GetGUID()))
                if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                    SendMusic(player, soundId);
        _fightListeners.clear();
    }

    void SendMusic(Player* player, uint32 soundId)
    {
        if (player && player->IsInWorld() && player->GetSession() && !player->GetSession()->IsBot())
            me->PlayDirectMusic(soundId, player);
    }

    std::string Describe() const
    {
        return Acore::StringFormat("L'Infini: phase {} at {:.1f}s, health {}/{} ({:.1f}%), tier damage x{:.2f}, "
                                   "reference {:.0f}, next step {}/{}, fracture {} (clean {}, failed {}), revived {}",
                                   uint32(_phase), Elapsed() / 1000.0f, me->GetHealth(), me->GetMaxHealth(),
                                   me->GetHealthPct(), GetChallengeDamageFactorOf(me), Reference(), _next,
                                   _timeline.size(), _fracture, _cleanCount, _failCount, _revived);
    }

    // Jumps the fight forward (testing): the steps passed over are dropped, the phase changes among them still run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        EndWindup();
        // A lift's landing was among what the scheduler dropped
        if (_lifted)
        {
            _lifted = false;
            me->SetDisableGravity(false);
            me->GetMotionMaster()->MoveLand(POINT_LAND, _liftFrom.GetPositionX(), _liftFrom.GetPositionY(),
                                            _liftFrom.GetPositionZ());
            Resume();
        }
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

private:
    static constexpr uint32 POINT_CENTER = 1;
    static constexpr uint32 POINT_LIFT = 2;
    static constexpr uint32 POINT_LAND = 3;
    // Players are waited for this long in an instance before a god not confirmed as a challenge's goes
    static constexpr uint32 DefiGraceMs = 8000;

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
        {
            UpdateDefi();
            return;
        }

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
            LOG_INFO("module.infinite", "L'Infini appears for a challenge instance={}", me->GetInstanceId());
            return;
        }

        _defiWaitMs += 1000;
        if (_defiWaitMs >= DefiGraceMs)
        {
            LOG_INFO("module.infinite", "L'Infini leaves an instance that is no challenge's instance={}",
                     me->GetInstanceId());
            me->DespawnOrUnsummon(0ms, Seconds(7 * DAY));
        }
    }

    // --- Fight state -----------------------------------------------------------------------------------------------
    void ResetFight()
    {
        scheduler.CancelAll();
        EndWindup();
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        _timeline.clear();
        _next = 0;
        _phase = Phase::None;
        _edge = false;
        _exposed = false;
        _finishing = false;
        _nextStandingCheckMs = 0;
        _offTankSide = 0.0f;
        _offTankBase = 0.0f;
        _mainTank.Clear();
        _offTank.Clear();
        _fragments.clear();
        _lifted = false;
        me->ClearEmoteState();
        me->SetDisableGravity(false);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetObjectScale(1.0f);
        me->RemoveAurasDueToSpell(SPELL_REVEAL_PARTICLES);
        me->RemoveAurasDueToSpell(SPELL_REVEAL_GLOW);
        ShowStarryRoom(false);
        for (auto const& [guid, stacks] : _weight)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                player->RemoveAurasDueToSpell(SPELL_WEIGHT);
        for (auto const& [guid, stacks] : _doom)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                player->RemoveAurasDueToSpell(SPELL_DOOM);
        _weight.clear();
        _doom.clear();
        ResetRework();
    }

    // --- Against chance: wounds, carriers, the god's fracture, the fallen brought back, the deaths logged ------------
    void ResetRework()
    {
        for (auto const& [guid, wound] : _wounds)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                player->RemoveAurasDueToSpell(SPELL_FELURE);
        me->RemoveAurasDueToSpell(SPELL_FRACTURE);
        _wounds.clear();
        _lastHit.clear();
        _deaths.clear();
        _lastCarried.clear();
        _carryingUntil.clear();
        _sparing.Clear();
        _killLabel = nullptr;
        _fracture = 0;
        _bestFracture = 0;
        _cleanCount = 0;
        _failCount = 0;
        _revived = 0;
        _supernovas = 0;
        _singularities = 0;
        _stars = 0;
        _crosses = 0;
        _constellationLines = 0;
    }

    // Not wounded, or wounded by this very mistake a moment ago (WoundGraceMs): the hit is held back
    bool IsSpared(ObjectGuid guid, uint32 now) const
    {
        auto const wound = _wounds.find(guid);
        return wound == _wounds.end() || now >= wound->second.until || now < wound->second.since + WoundGraceMs;
    }

    bool IsWounded(ObjectGuid guid, uint32 now) const
    {
        auto const wound = _wounds.find(guid);
        return wound != _wounds.end() && now < wound->second.until;
    }

    void Wound(Player* player, uint32 now)
    {
        Wounded& wound = _wounds[player->GetGUID()];
        if (now < wound.until)
            return;
        wound = { now, now + WoundMs };
        if (!sSpellMgr->GetSpellInfo(SPELL_FELURE))
            return;
        player->RemoveAurasDueToSpell(SPELL_FELURE);
        if (Aura* aura = me->AddAura(SPELL_FELURE, player))
        {
            aura->SetMaxDuration(int32(WoundMs));
            aura->SetDuration(int32(WoundMs));
        }
    }

    // Who carries a marked mechanic: no tank, nobody carrying one already; whoever can best keep it from the others -
    // a real player first when `playerFirst` (the hardest ones: the player is the hero, the bots the steady support),
    // then the ranged, the healers, the melee last (a melee stuck in the god's feet drew its circle over the others) -
    // and among them whoever carried one the longest ago, so each takes a turn
    std::vector<Player*> PickCarriers(uint32 count, bool playerFirst)
    {
        uint32 const now = Elapsed();
        std::vector<Player*> players = ArenaPlayers();
        std::erase_if(players, [this, now](Player* player)
        {
            auto const carrying = _carryingUntil.find(player->GetGUID());
            return IsGroupTank(player) || (carrying != _carryingUntil.end() && now < carrying->second);
        });
        auto const rank = [playerFirst](Player* player) -> uint32
        {
            if (playerFirst && player->GetSession() && !player->GetSession()->IsBot())
                return 0;
            if (IsHealerPlayer(player))
                return 2;
            return FightsAtRange(player) ? 1 : 3;
        };
        auto const last = [this](Player* player)
        {
            auto const found = _lastCarried.find(player->GetGUID());
            return found == _lastCarried.end() ? 0u : found->second + 1;
        };
        std::stable_sort(players.begin(), players.end(), [&rank, &last](Player* left, Player* right)
        {
            uint32 const leftRank = rank(left);
            uint32 const rightRank = rank(right);
            return leftRank != rightRank ? leftRank < rightRank : last(left) < last(right);
        });
        if (players.size() > count)
            players.resize(count);
        for (Player* player : players)
        {
            _lastCarried[player->GetGUID()] = now;
            _carryingUntil[player->GetGUID()] = now + 1000;
        }
        return players;
    }

    // How long the carrier picked keeps its mark: no other one is given to them meanwhile
    void Carries(Player* player, uint32 lasts)
    {
        _carryingUntil[player->GetGUID()] = Elapsed() + lasts + 500;
    }

    static bool IsHealerPlayer(Player* player)
    {
        if (Group* group = player->GetGroup())
            for (Group::MemberSlot const& member : group->GetMemberSlots())
                if (member.guid == player->GetGUID() && member.roles)
                    return member.roles & lfg::PLAYER_ROLE_HEALER;
        return player->HasHealSpec();
    }

    static bool FightsAtRange(Player* player)
    {
        return player->getClass() == CLASS_HUNTER || player->HasCasterSpec() || IsHealerPlayer(player);
    }

    static char const* RoleName(Player* player)
    {
        if (IsGroupTank(player))
            return "tank";
        if (IsHealerPlayer(player))
            return "healer";
        return FightsAtRange(player) ? "ranged" : "melee";
    }

    // A mechanic's resolution, over its parts (a rain of stars' circles, Jugement's two carriers): clean when no part
    // hit anyone who could have kept out of it
    struct Outcome
    {
        uint32 left;
        bool failed = false;
    };

    std::shared_ptr<Outcome> Expect(uint32 parts)
    {
        return std::make_shared<Outcome>(Outcome{ std::max(parts, 1u) });
    }

    void Settle(std::shared_ptr<Outcome> const& outcome, bool failed)
    {
        outcome->failed = outcome->failed || failed;
        if (!outcome->left || --outcome->left)
            return;
        Resolve(!outcome->failed);
    }

    // Fracture de l'éternité: a stack more for a clean mechanic, FractureLostOnFail fewer for a failed one
    void Resolve(bool clean)
    {
        if (_phase == Phase::None || _phase == Phase::Over || _phase == Phase::Final)
            return;
        ++(clean ? _cleanCount : _failCount);
        uint32 const before = _fracture;
        _fracture = clean ? std::min(_fracture + 1, FractureMaxStacks) :
            (_fracture > FractureLostOnFail ? _fracture - FractureLostOnFail : 0u);
        _bestFracture = std::max(_bestFracture, _fracture);
        if (_fracture == before || !sSpellMgr->GetSpellInfo(SPELL_FRACTURE))
            return;
        if (!_fracture)
        {
            me->RemoveAurasDueToSpell(SPELL_FRACTURE);
            return;
        }
        Aura* aura = me->GetAura(SPELL_FRACTURE);
        if (!aura)
            aura = me->AddAura(SPELL_FRACTURE, me);
        if (aura)
            aura->SetStackAmount(uint8(_fracture));
    }

    // Résurgence: the fight's fallen stand again where they fell, the god healing ResurgenceHealPct for each
    void Resurgence()
    {
        uint32 revived = 0;
        for (ObjectGuid const& guid : _fightListeners)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, guid);
            if (!player || player->IsAlive() || player->IsGameMaster() ||
                player->GetExactDist2d(&ArenaCenter) > ArenaReach)
                continue;
            player->ResurrectPlayer(ResurgenceHealthPct / 100.0f);
            player->SpawnCorpseBones();
            player->RemoveAurasDueToSpell(SPELL_FELURE);
            _wounds.erase(guid);
            ++revived;
        }
        if (!revived)
            return;
        _revived += revived;
        me->ModifyHealth(int32(float(me->GetMaxHealth()) * float(ResurgenceHealPct) / 100.0f * float(revived)));
        LOG_INFO("module.infinite", "L'Infini Résurgence instance={} at={:.1f}s revived={} health={:.1f}%",
                 me->GetInstanceId(), Elapsed() / 1000.0f, revived, me->GetHealthPct());
    }

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

    // What killed a player, logged for tuning on evidence: the last hit they took from the god
    void RecordDeath(Player* player)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return;
        uint32 const now = Elapsed();
        auto const last = _lastHit.find(player->GetGUID());
        std::string const what = _killLabel ? std::string(_killLabel) :
            last == _lastHit.end() ? std::string("unknown") : SpellName(last->second.spellId);
        bool const avoidable = !_killLabel && last != _lastHit.end() && last->second.avoidable;
        ++_deaths[what];
        LOG_INFO("module.infinite", "L'Infini death instance={} at={:.1f}s phase={} {} ({}, {}): {}{}{}",
                 me->GetInstanceId(), now / 1000.0f, uint32(_phase), player->GetName(),
                 player->GetSession() && player->GetSession()->IsBot() ? "bot" : "player", RoleName(player), what,
                 avoidable ? " (avoidable)" : "", IsWounded(player->GetGUID(), now) ? " while wounded" : "");
    }

    void LogSummary(char const* outcome)
    {
        std::string deaths;
        for (auto const& [what, count] : _deaths)
            deaths += Acore::StringFormat("{}{} x{}", deaths.empty() ? "" : ", ", what, count);
        LOG_INFO("module.infinite", "L'Infini {} instance={} at={:.1f}s phase={} health={:.1f}% deaths=[{}] "
                 "revived={} clean={} failed={} fracture={} (best {})", outcome, me->GetInstanceId(),
                 Elapsed() / 1000.0f, uint32(_phase), me->GetHealthPct(), deaths, _revived, _cleanCount, _failCount,
                 _fracture, _bestFracture);
    }

    // A spot of the learnt patterns: at a fixed angle round the Planetarium's middle
    Position FixedSpot(float angle, float distance) const
    {
        return Ground(Position(ArenaCenter.GetPositionX() + std::cos(angle) * distance,
            ArenaCenter.GetPositionY() + std::sin(angle) * distance, ArenaFloorZ));
    }

    bool CanMelee() const
    {
        if (_windup || _lifted || me->HasReactState(REACT_PASSIVE))
            return false;
        return _phase == Phase::One || _phase == Phase::Two || _phase == Phase::Setup || _phase == Phase::Three;
    }

    // The players the fight hits: alive in the Planetarium, not game masters
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

    // The players who hear its music: everyone near the Planetarium, dead or a game master too
    std::vector<Player*> Listeners() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->GetExactDist2d(&ArenaCenter) <= MusicReach)
                players.push_back(player);
        return players;
    }

    // What a player takes on top of a hit: Poids de l'éternité, Fin imminente and Fêlure du temps
    float TakenFactor(Unit const* victim) const
    {
        float factor = 1.0f;
        if (IsWounded(victim->GetGUID(), Elapsed()))
            factor *= 1.0f + float(WoundTakenPct) / 100.0f;
        if (auto const weight = _weight.find(victim->GetGUID()); weight != _weight.end())
            factor *= 1.0f + WeightPerStackPct * float(weight->second) / 100.0f;
        if (auto const doom = _doom.find(victim->GetGUID()); doom != _doom.end())
            factor *= 1.0f + DoomPerStackPct * float(doom->second) / 100.0f;
        return factor;
    }

    // A share of the reference health, as spell: the tier's factor applies on the way (ChallengeTierUnitScript), then
    // the player's defences. An avoidable one (it stood in the red) adds Imprudence, as in a key.
    // Défi tiers above I (0 at Défi I, and outside a challenge)
    uint32 TierAbove() const
    {
        uint8 const tier = GetChallengeTierOf(me->GetMap());
        return tier > 1 ? tier - 1u : 0u;
    }

    // An avoidable one is a mistake: the first wounds rather than kills (held back in DamageDealt, then Fêlure du
    // temps), a second one while wounded is the whole hit.
    void Hit(Player* player, NamedSpell const& spell, float percent, bool avoidable)
    {
        if (!player || !player->IsAlive())
            return;
        if (avoidable)
            percent *= 1.0f + AvoidablePctPerTier / 100.0f * float(TierAbove());
        float const amount = Reference() * percent / 100.0f * TakenFactor(player);
        uint32 const now = Elapsed();
        ObjectGuid const guid = player->GetGUID();
        bool const spared = avoidable && IsSpared(guid, now);
        _lastHit[guid] = { SpellOf(spell), now, avoidable };
        if (spared)
            _sparing = guid;
        MythicTuning::DealAbilityDamage(me, player, SpellOf(spell), uint32(std::max(1.0f, amount)));
        _sparing.Clear();
        if (!avoidable)
            return;
        MythicTuning::ApplyImprudence(player);
        if (spared && player->IsAlive())
            Wound(player, now);
    }

    std::vector<Player*> PlayersIn(GroundIndicators::Area const& area) const
    {
        std::vector<Player*> players;
        for (Player* player : ArenaPlayers())
            if (area.Contains(*player))
                players.push_back(player);
        return players;
    }

    // A spot on the platform's floor, between minRadius and maxRadius from its middle
    Position ArenaSpot(float minRadius, float maxRadius) const
    {
        for (uint32 attempt = 0; attempt < 8; ++attempt)
        {
            float const angle = frand(0.0f, 2.0f * float(M_PI));
            float const distance = frand(minRadius, maxRadius);
            float const x = ArenaCenter.GetPositionX() + std::cos(angle) * distance;
            float const y = ArenaCenter.GetPositionY() + std::sin(angle) * distance;
            float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), x, y, ArenaFloorZ + 5.0f, true, 15.0f);
            if (z > INVALID_HEIGHT && std::fabs(z - ArenaFloorZ) < 4.0f)
                return Position(x, y, z);
        }
        return ArenaCenter;
    }

    Position Ground(Position const& at) const
    {
        float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), at.GetPositionX(), at.GetPositionY(),
                                                 ArenaFloorZ + 5.0f, true, 15.0f);
        return Position(at.GetPositionX(), at.GetPositionY(), z > INVALID_HEIGHT ? z : ArenaFloorZ);
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

    // --- Windups and lifts -----------------------------------------------------------------------------------------
    void BeginWindup(float facing)
    {
        _windup = true;
        me->StopMoving();
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->SetTarget();
        me->SetFacingTo(facing);
    }

    void EndWindup()
    {
        if (!_windup)
            return;
        _windup = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
        if (Unit* victim = me->GetVictim())
            me->SetTarget(victim->GetGUID());
    }

    // Held still, no melee, no chase: an intermission, a lift, the end
    void Hold()
    {
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
    }

    void Resume()
    {
        me->ClearEmoteState();
        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0, 0.0f, true))
            AttackStart(target);
    }

    void Lift()
    {
        Hold();
        _lifted = true;
        _liftFrom = me->GetPosition();
        me->SetDisableGravity(true);
        me->GetMotionMaster()->MoveTakeoff(POINT_LIFT, _liftFrom.GetPositionX(), _liftFrom.GetPositionY(),
                                           _liftFrom.GetPositionZ() + LiftHeight);
    }

    void Land(bool resume)
    {
        if (!_lifted)
            return;
        _lifted = false;
        me->GetMotionMaster()->MoveLand(POINT_LAND, _liftFrom.GetPositionX(), _liftFrom.GetPositionY(),
                                        _liftFrom.GetPositionZ());
        scheduler.Schedule(1500ms, [this, resume](TaskContext)
        {
            me->SetDisableGravity(false);
            if (resume)
                Resume();
        });
    }

    // The Planetarium's floor turned to stars, as Algalon's own fight shows it, or back to rest
    void ShowStarryRoom(bool starry)
    {
        // Never in an instance that is not a challenge's: a raid fighting Algalon there has these doodads in hand
        if (!_defiConfirmed)
            return;
        auto set = [this](uint32 entry, GOState state)
        {
            if (GameObject* go = me->FindNearestGameObject(entry, 150.0f))
                go->SetGoState(state);
        };
        set(GO_UNIVERSE_FLOOR_01, starry ? GO_STATE_READY : GO_STATE_ACTIVE);
        set(GO_UNIVERSE_FLOOR_02, starry ? GO_STATE_ACTIVE : GO_STATE_READY);
        set(GO_UNIVERSE_GLOBE, starry ? GO_STATE_ACTIVE : GO_STATE_READY);
        set(GO_ALGALON_TRAPDOOR, starry ? GO_STATE_ACTIVE : GO_STATE_READY);
    }

    // --- The timeline ----------------------------------------------------------------------------------------------
    void Execute(Ability what)
    {
        switch (what)
        {
            case Ability::Cleave:           DoubleCleave(); break;
            case Ability::TwinStrikes:      TwinStrikes(); break;
            case Ability::Starfall:         Starfall(4 + TierAbove() / 2); break;
            case Ability::TankArcane:       TankArcane(); break;
            case Ability::Sweep:            Sweep(); break;
            case Ability::FallenStar:       FallenStar(); break;
            case Ability::CrossWaves:       CrossWavesStart(); break;
            case Ability::OrbLasers:        OrbLasers(); break;
            case Ability::SpinLaser:        SpinLaser(); break;
            case Ability::Gravity:          Gravity(); break;
            case Ability::BigBangRise:      BigBangRise(); break;
            case Ability::BigBangYell:      Talk(SAY_BIG_BANG); break;
            case Ability::BigBang:          BigBang(); break;
            case Ability::Intermission1:    EnterIntermission1(); break;
            case Ability::Fragments:        SummonFragments(); break;
            case Ability::Intermission1Yell: Talk(SAY_INTERMISSION_1_END); break;
            case Ability::Phase2:           EnterPhase2(); break;
            case Ability::RaidTick:         RaidTick(); break;
            case Ability::Singularity:      Singularity(); break;
            case Ability::CollapsingStar:   CollapsingStar(); break;
            case Ability::Intermission2:    EnterIntermission2(); break;
            case Ability::Constellation:    Constellation(); break;
            case Ability::StarRays:         StarRays(); break;
            case Ability::Setup:            EnterSetup(); break;
            case Ability::Meteors:          Meteors(); break;
            case Ability::RevealRise:       Lift(); break;
            case Ability::RevealYell:       Talk(SAY_REVEAL); break;
            case Ability::Phase3:           EnterPhase3(); break;
            case Ability::Supernova:        Supernova(); break;
            case Ability::Judgement:        Judgement(); break;
            case Ability::Weight:           Weight(); break;
            case Ability::Final:            EnterFinal(); break;
            case Ability::EndPulse:         EndPulse(); break;
            case Ability::HardEnrage:       HardEnrage(); break;
        }
    }

    // The two its tank busters go to: the group's tanks (MythicDungeonSystem IsGroupTank: the tank role, else a tank
    // stance) standing in the Planetarium, the most threat first; while fewer than two stand, the highest on its threat
    // make up the pair.
    std::vector<Unit*> BusterTargets()
    {
        std::vector<Unit*> tanks;
        for (Player* player : ArenaPlayers())
            if (IsGroupTank(player))
                tanks.push_back(player);
        std::sort(tanks.begin(), tanks.end(), [this](Unit* a, Unit* b)
        {
            return me->GetThreatMgr().GetThreat(a) > me->GetThreatMgr().GetThreat(b);
        });
        if (tanks.size() > 2)
            tanks.resize(2);
        for (uint32 position = 0; tanks.size() < 2 && position < 10; ++position)
        {
            Unit* next = SelectTarget(SelectTargetMethod::MaxThreat, position, 0.0f, true);
            if (!next)
                break;
            if (std::find(tanks.begin(), tanks.end(), next) == tanks.end())
                tanks.push_back(next);
        }
        return tanks;
    }

    // Double fauchage cosmique: a cone at each of the two tanks (BusterTargets), at once, each turning with its tank
    // until it lands. It lands on the one it is aimed at wherever that one stands; anyone else in a cone takes far
    // more - the two tanks stacked take each other's and die (tanks: stand apart, cones away from the group).
    void DoubleCleave()
    {
        std::vector<Unit*> const targets = BusterTargets();
        Unit* first = targets.empty() ? nullptr : targets[0];
        if (!first || _lifted)
            return;
        Unit* second = targets.size() > 1 ? targets[1] : nullptr;

        BeginWindup(me->GetAngle(first));
        me->HandleEmoteCommand(EMOTE_ONESHOT_SPELL_CAST_OMNI);
        std::vector<std::pair<ObjectGuid, GroundIndicators::Area>> cones;
        for (Unit* aimed : { first, second })
            if (aimed)
                cones.emplace_back(aimed->GetGUID(), GroundIndicators::ShowTrackingCone(me, me->GetPosition(),
                    CleaveRadius, CleaveArc, CleaveWarningMs, aimed));

        scheduler.Schedule(Milliseconds(CleaveWarningMs), [this, cones](TaskContext)
        {
            me->SendPlaySpellVisual(KIT_QUANTUM_STRIKE);
            bool failed = false;
            for (Player* player : ArenaPlayers())
            {
                float percent = 0.0f;
                bool stoodIn = false;
                for (auto const& [aimed, area] : cones)
                {
                    if (player->GetGUID() == aimed)
                        percent = std::max(percent, CleaveTankPct);
                    else if (GroundIndicators::CurrentCone(ObjectAccessor::GetUnit(*me, aimed), area)
                        .Contains(*player))
                    {
                        percent = std::max(percent, CleaveOtherPct);
                        stoodIn = true;
                    }
                }
                if (percent > 0.0f)
                    Hit(player, SPELL_DOUBLE_CLEAVE, percent, stoodIn);
                failed = failed || stoodIn;
            }
            Resolve(!failed);
            EndWindup();
        });
    }

    // Frappes jumelles: a buster on each of the two tanks (BusterTargets), nothing to dodge
    void TwinStrikes()
    {
        std::vector<ObjectGuid> targets;
        for (Unit* target : BusterTargets())
            targets.push_back(target->GetGUID());
        if (targets.empty())
            return;

        me->SendPlaySpellVisual(KIT_PHASE_PUNCH_CAST);
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK2HTIGHT);
        scheduler.Schedule(1s, [this, targets](TaskContext)
        {
            for (ObjectGuid const& guid : targets)
                if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                {
                    player->SendPlaySpellVisual(KIT_PHASE_PUNCH_HIT);
                    Hit(player, SPELL_TWIN_STRIKES, TwinStrikePct, false);
                }
        });
    }

    // Pluie d'étoiles: a circle under a few players, where they stand now
    void Starfall(uint32 count)
    {
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomResize(players, count);
        auto const outcome = Expect(uint32(players.size()));
        for (Player* target : players)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, Ground(*target), StarfallRadius,
                StarfallWarningMs, GroundIndicators::Theme::Arcane);
            scheduler.Schedule(Milliseconds(StarfallWarningMs), [this, area, outcome](TaskContext)
            {
                PlayOnGround(area.origin, KIT_COSMIC_SMASH);
                std::vector<Player*> const hit = PlayersIn(area);
                for (Player* player : hit)
                    Hit(player, SPELL_STARFALL, StarfallPct, true);
                Settle(outcome, !hit.empty());
            });
        }
    }

    // Rayon cosmique: a cone from the god at a player who is no tank (phases 1 and 2)
    void Sweep()
    {
        if (_lifted)
            return;
        std::vector<Player*> const carriers = PickCarriers(1, false);
        if (carriers.empty())
            return;
        Player* target = carriers.front();
        Carries(target, SweepWarningMs);
        Position const apex = Ground(me->GetPosition());
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, apex, apex.GetAngle(target), SweepRadius,
            SweepArc, SweepWarningMs, GroundIndicators::Theme::Arcane);
        scheduler.Schedule(Milliseconds(SweepWarningMs), [this, area](TaskContext)
        {
            float const facing = area.origin.GetOrientation();
            for (float along = 8.0f; along < area.radius; along += 10.0f)
                PlayOnGround(Position(area.origin.GetPositionX() + std::cos(facing) * along,
                    area.origin.GetPositionY() + std::sin(facing) * along, area.origin.GetPositionZ()),
                    KIT_COSMIC_SMASH);
            std::vector<Player*> const hit = PlayersIn(area);
            for (Player* player : hit)
                Hit(player, SPELL_SWEEP, SweepPct, true);
            Resolve(hit.empty());
        });
    }

public:
    // .infini cast: one attack now, the fight going on
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat())
            return false;
        if (what == "star")
            FallenStar();
        else if (what == "crosses")
            CrossWavesStart();
        else if (what == "orb")
            OrbLasers();
        else if (what == "spin")
            SpinLaser();
        else if (what == "sweep")
            Sweep();
        else if (what == "cleave")
            DoubleCleave();
        else if (what == "twin")
            TwinStrikes();
        else
            return false;
        return true;
    }

private:

    // --- Patterns: the fallen star, the crosses, the orb's lasers, the swept laser ---------------------------------

    // A player stands in a line from start along facing, length long and width wide
    static bool InLine(Position const& start, float facing, float length, float width, Position const& at)
    {
        float const dx = at.GetPositionX() - start.GetPositionX();
        float const dy = at.GetPositionY() - start.GetPositionY();
        float const along = dx * std::cos(facing) + dy * std::sin(facing);
        float const across = -dx * std::sin(facing) + dy * std::cos(facing);
        return along >= 0.0f && along <= length && std::fabs(across) <= width / 2.0f;
    }

    // A stalker that lives lasts ms, an effect on it or a beam's end
    Creature* FxStalker(Position const& at, uint32 lasts, uint32 entry = NPC_STALKER)
    {
        // Held where it is put: a beam's end past the platform's edge does not fall
        Creature* stalker = me->SummonCreature(entry, at, TEMPSUMMON_TIMED_DESPAWN, lasts);
        if (stalker)
            stalker->SetDisableGravity(true);
        return stalker;
    }

    // A beam's end flown straight to where, at speed yards a second (not walked: it is under the floor, or past
    // the platform's edge)
    void FlyTo(Creature* stalker, Position const& where, float speed)
    {
        Movement::MoveSplineInit init(stalker);
        init.MoveTo(where.GetPositionX(), where.GetPositionY(), where.GetPositionZ(), false);
        init.SetFly();
        init.SetVelocity(std::max(speed, 1.0f));
        init.Launch();
    }

    // A line from start along facing, length long and width wide, for the bots (nothing drawn)
    static GroundIndicators::Area LineArea(Position const& start, float facing, float length, float width)
    {
        GroundIndicators::Area area;
        area.kind = GroundIndicators::Area::Kind::Rectangle;
        area.origin = Position(start.GetPositionX(), start.GetPositionY(), start.GetPositionZ(), facing);
        area.radius = length;
        area.width = width;
        return area;
    }

    // A beam from one stalker to another, for lasts ms (nothing without an effect for it). A spell cannot take a unit
    // that cannot be selected as its target, and the stalkers are made so: the beam's end is made selectable (it has
    // no model to click).
    void Beam(Creature* from, Creature* to, uint32 lasts)
    {
        if (!SPELL_FX_BEAM || !from || !to)
            return;
        to->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        from->CastSpell(to, SPELL_FX_BEAM, true);
        ObjectGuid const guid = from->GetGUID();
        scheduler.Schedule(Milliseconds(lasts), [this, guid](TaskContext)
        {
            if (Creature* source = me->GetMap()->GetCreature(guid))
                source->InterruptNonMeleeSpells(false);
        });
    }

    // Étoile déchue: a mark on a player who is no tank, then a star falls on them - a little for them, for the others
    // all the more the closer they stand: next to them it kills, FallenStarReach away it is nothing. The marked one
    // runs from the group.
    void FallenStar()
    {
        std::vector<Player*> const carriers = PickCarriers(1, true);
        if (carriers.empty())
            return;
        Player* marked = carriers.front();
        Carries(marked, FallenStarMs);
        // Its carrier keeps FallenStarKeepAway from the others: the hit falls off to a third there
        GroundIndicators::ShowCarriedCircle(me, marked, FallenStarLethalRadius, FallenStarMs, 0, FallenStarKeepAway);
        if (SPELL_FX_MARK)
            if (Aura* aura = me->AddAura(SPELL_FX_MARK, marked))
            {
                aura->SetMaxDuration(int32(FallenStarMs));
                aura->SetDuration(int32(FallenStarMs));
            }
        ObjectGuid const guid = marked->GetGUID();
        scheduler.Schedule(Milliseconds(FallenStarMs), [this, guid](TaskContext)
        {
            Player* target = ObjectAccessor::GetPlayer(*me, guid);
            if (!target || !target->IsAlive())
                return;
            Position const at = Ground(*target);
            PlayOnGround(at, KIT_FX_STAR_FALL ? KIT_FX_STAR_FALL : KIT_BIG_BANG_HIT);
            Hit(target, SPELL_FALLEN_STAR, FallenStarCarrierPct, false);
            bool failed = false;
            for (Player* player : ArenaPlayers())
            {
                if (player == target)
                    continue;
                float const distance = player->GetExactDist2d(&at);
                float const share = std::max(0.0f, 1.0f - distance / FallenStarReach);
                float const percent = FallenStarMaxPct * share * share;
                // Nearer than its keep-away distance it was theirs to avoid (it was only so inside the red: a step
                // past it still killed, as an unavoidable hit)
                bool const avoidable = percent > FallenStarAvoidablePct;
                if (share > 0.02f)
                    Hit(player, SPELL_FALLEN_STAR, percent, avoidable);
                failed = failed || avoidable;
            }
            Resolve(!failed);
        });
    }

    // Croix céleste: CrossWaves crosses through the god's feet, each drawn as the last lands and turned 45 degrees
    // from it: the gaps move every wave
    void CrossWavesStart()
    {
        Position const center = Ground(me->GetPosition());
        // Learnt: the first cross of each turned an eighth from the last one's
        float const first = float(_crosses++) * float(M_PI) / 8.0f;
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
                    lines.push_back(GroundIndicators::ShowRectangle(me, start, arm, CrossLength * 2.0f, CrossWidth,
                        CrossWarningMs, GroundIndicators::Theme::Arcane));
                }
                scheduler.Schedule(Milliseconds(CrossWarningMs), [this, lines, center, facing](TaskContext)
                {
                    // Smashes down the arms, as the constellation's lines: the line bursts, not the whole platform
                    for (uint32 arm = 0; arm < 4; ++arm)
                    {
                        float const angle = facing + float(arm) * float(M_PI) / 2.0f;
                        for (float along = 6.0f; along < CrossLength; along += 12.0f)
                            PlayOnGround(Position(center.GetPositionX() + std::cos(angle) * along,
                                center.GetPositionY() + std::sin(angle) * along, center.GetPositionZ()),
                                KIT_COSMIC_SMASH);
                    }
                    bool failed = false;
                    for (Player* player : ArenaPlayers())
                        if (std::ranges::any_of(lines, [player](GroundIndicators::Area const& line)
                            { return line.Contains(*player); }))
                        {
                            Hit(player, SPELL_CROSS, CrossWavePct, true);
                            failed = true;
                        }
                    Resolve(!failed);
                });
            });
        }
    }

    // Lances de l'orbe: an orb gathers over the god, then four lasers from it - ahead, behind, to either side - sweep
    // out across the platform and back; each pass (out, back) hits whoever it crosses
    void OrbLasers()
    {
        Position const center = Ground(me->GetPosition());
        float const facing = me->GetOrientation();
        uint32 const lasts = OrbChargeMs + OrbLaserOutMs + OrbLaserBackMs + 500;
        Position const orbAt(center.GetPositionX(), center.GetPositionY(), center.GetPositionZ() + OrbHeight);
        Creature* orb = FxStalker(orbAt, lasts, NPC_HOVER_STALKER);
        if (orb)
        {
            orb->SetDisableGravity(true);
            // The sphere's model is drawn at the stalker's scale: at 1 it filled the view
            orb->SetObjectScale(OrbScale);
            if (SPELL_FX_ORB)
                orb->AddAura(SPELL_FX_ORB, orb);
        }
        // No red on the ground: the orb and its beams are the warning. Each beam from a stalker of its own in the orb
        // (a unit channels one beam at a time) to a far end sent out and back along its arm, just under the floor so
        // the beam grazes it
        Position const low(center.GetPositionX(), center.GetPositionY(), center.GetPositionZ() - BeamEndDrop);
        std::vector<std::pair<ObjectGuid, ObjectGuid>> beams;
        for (uint32 arm = 0; arm < 4; ++arm)
        {
            Creature* source = FxStalker(orbAt, lasts, NPC_HOVER_STALKER);
            Creature* end = FxStalker(low, lasts);
            if (source && end)
                beams.emplace_back(source->GetGUID(), end->GetGUID());
        }
        std::vector<ObjectGuid> ends;
        for (auto const& beam : beams)
            ends.push_back(beam.second);
        scheduler.Schedule(Milliseconds(OrbChargeMs), [this, beams, low, facing](TaskContext)
        {
            for (std::size_t arm = 0; arm < beams.size(); ++arm)
                if (Creature* end = me->GetMap()->GetCreature(beams[arm].second))
                {
                    float const angle = facing + float(arm) * float(M_PI) / 2.0f;
                    Position const farEnd(low.GetPositionX() + std::cos(angle) * OrbLaserReach,
                        low.GetPositionY() + std::sin(angle) * OrbLaserReach, low.GetPositionZ());
                    FlyTo(end, farEnd, OrbLaserReach / (float(OrbLaserOutMs) / 1000.0f));
                    Beam(me->GetMap()->GetCreature(beams[arm].first), end, OrbLaserOutMs + OrbLaserBackMs);
                }
        });
        // Back to the god's feet, wherever it stands now
        scheduler.Schedule(Milliseconds(OrbChargeMs + OrbLaserOutMs), [this, ends](TaskContext)
        {
            Position const feet = Ground(me->GetPosition());
            Position const back(feet.GetPositionX(), feet.GetPositionY(), feet.GetPositionZ() - BeamEndDrop);
            for (ObjectGuid const& guid : ends)
                if (Creature* end = me->GetMap()->GetCreature(guid))
                    FlyTo(end, back, end->GetExactDist(&back) / (float(OrbLaserBackMs) / 1000.0f));
        });
        // The bots see the four lines, nothing drawn
        for (uint32 arm = 0; arm < 4; ++arm)
            GroundIndicators::WatchArea(me, LineArea(center, facing + float(arm) * float(M_PI) / 2.0f, OrbLaserReach,
                OrbLaserWidth), lasts - 500);

        // The hits, pass by pass: a laser reaches out to its length of the moment
        auto hitOut = std::make_shared<std::set<ObjectGuid>>();
        auto hitBack = std::make_shared<std::set<ObjectGuid>>();
        for (uint32 at = 0; at <= OrbLaserOutMs + OrbLaserBackMs; at += LaserTickMs)
            scheduler.Schedule(Milliseconds(OrbChargeMs + at), [this, at, center, facing, hitOut, hitBack](TaskContext)
            {
                bool const out = at <= OrbLaserOutMs;
                float const reach = out ? OrbLaserReach * float(at) / float(OrbLaserOutMs) :
                    OrbLaserReach * (1.0f - float(at - OrbLaserOutMs) / float(OrbLaserBackMs));
                auto& hit = out ? *hitOut : *hitBack;
                for (Player* player : ArenaPlayers())
                {
                    if (hit.contains(player->GetGUID()))
                        continue;
                    for (uint32 arm = 0; arm < 4; ++arm)
                        if (InLine(center, facing + float(arm) * float(M_PI) / 2.0f, reach, OrbLaserWidth, *player))
                        {
                            hit.insert(player->GetGUID());
                            Hit(player, SPELL_ORB_LASER, OrbLaserPct, true);
                            break;
                        }
                }
                if (at + LaserTickMs > OrbLaserOutMs + OrbLaserBackMs)
                    Resolve(hitOut->empty() && hitBack->empty());
            });
    }

    // Rayon du Gardien: the god holds still and a laser shoots straight ahead, then it turns a full circle with it,
    // one way or the other: run ahead of it, never through it
    void SpinLaser()
    {
        if (_lifted)
            return;
        float const facing = me->GetOrientation();
        float const turn = (roll_chance_i(50) ? 1.0f : -1.0f) * 2.0f * float(M_PI) / (float(SpinMs) / 1000.0f);
        Position const center = Ground(me->GetPosition());
        BeginWindup(facing);
        // No red on the ground: the beam shows the line, held straight ahead for the warning, then swept (the bots
        // read both from the registry)
        GroundIndicators::WatchArea(me, LineArea(center, facing, SpinReach, SpinWidth), SpinWarningMs);

        uint32 const lasts = SpinWarningMs + SpinMs + 300;
        // On the platform's level (past its edge the ground found is the floor far below), from a stalker just over
        // the floor (not the god: its own casts would cut the channel): a line flat along the ground
        Creature* end = FxStalker(Position(center.GetPositionX() + std::cos(facing) * SpinReach,
            center.GetPositionY() + std::sin(facing) * SpinReach, center.GetPositionZ()), lasts);
        ObjectGuid const endGuid = end ? end->GetGUID() : ObjectGuid::Empty;
        Creature* source = FxStalker(Position(center.GetPositionX(), center.GetPositionY(),
            center.GetPositionZ() + SpinSourceHeight), lasts, NPC_HOVER_STALKER);
        Beam(source, end, SpinWarningMs + SpinMs);
        scheduler.Schedule(Milliseconds(SpinWarningMs), [this, center, facing, turn, endGuid](TaskContext)
        {
            GroundIndicators::WatchSweepingRectangle(me, center, facing, turn, SpinReach, SpinWidth, SpinMs);
            if (Creature* end = me->GetMap()->GetCreature(endGuid))
            {
                // The beam's end round the circle, along a smooth path the client flies itself
                Movement::MoveSplineInit init(end);
                Movement::PointsArray path;
                path.push_back(G3D::Vector3(end->GetPositionX(), end->GetPositionY(), end->GetPositionZ()));
                for (uint32 step = 1; step <= 36; ++step)
                {
                    float const angle = facing + turn * (float(SpinMs) / 1000.0f) * float(step) / 36.0f;
                    path.push_back(G3D::Vector3(center.GetPositionX() + std::cos(angle) * SpinReach,
                        center.GetPositionY() + std::sin(angle) * SpinReach, center.GetPositionZ()));
                }
                init.MovebyPath(path);
                init.SetSmooth();
                init.SetFly();
                init.SetVelocity(2.0f * float(M_PI) * SpinReach / (float(SpinMs) / 1000.0f));
                init.Launch();
            }
        });

        auto lastHit = std::make_shared<std::map<ObjectGuid, uint32>>();
        for (uint32 at = 0; at <= SpinMs; at += LaserTickMs)
            scheduler.Schedule(Milliseconds(SpinWarningMs + at), [this, at, center, facing, turn, lastHit](TaskContext)
            {
                float const angle = facing + turn * float(at) / 1000.0f;
                me->SetFacingTo(Position::NormalizeOrientation(angle));
                for (Player* player : ArenaPlayers())
                {
                    auto const last = lastHit->find(player->GetGUID());
                    if (last != lastHit->end() && at < last->second + SpinHitEveryMs)
                        continue;
                    if (InLine(center, angle, SpinReach, SpinWidth, *player))
                    {
                        (*lastHit)[player->GetGUID()] = at;
                        Hit(player, SPELL_SPIN_LASER, SpinLaserPct, true);
                    }
                }
                if (at + LaserTickMs > SpinMs)
                    Resolve(lastHit->empty());
            });
        scheduler.Schedule(Milliseconds(SpinWarningMs + SpinMs + 100), [this](TaskContext) { EndWindup(); });
    }

    // Fracture stellaire: arcane on whoever it is fighting, armour no help
    void TankArcane()
    {
        if (!CanMelee())
            return;
        if (Player* victim = me->GetVictim() ? me->GetVictim()->ToPlayer() : nullptr)
        {
            victim->SendPlaySpellVisual(KIT_PHASE_PUNCH_HIT);
            Hit(victim, SPELL_TANK_ARCANE, TankArcanePct, false);
        }
    }

    // Onde de gravité: the whole raid, the healers' rhythm
    void Gravity()
    {
        float const percent = GravityPct[_phase == Phase::Three ? 2 : _phase == Phase::Two ? 1 : 0];
        me->SendPlaySpellVisual(KIT_BLACK_HOLE_CAST);
        for (Player* player : ArenaPlayers())
            Hit(player, SPELL_GRAVITY, percent, false);
    }

    // The music breaks: the god rises and the whole platform but its feet turns red, the ring closing in from the edge
    void BigBangRise()
    {
        EndWindup();
        Lift();
        me->SendPlaySpellVisual(KIT_BIG_BANG_GATHER);
        Position const center = Ground(_liftFrom);
        uint32 const warning = AtBigBang - AtBigBangRise;
        _bigBang = GroundIndicators::ShowRing(me, center, BigBangRadius, BigBangSafeRadius, warning,
            GroundIndicators::Theme::Arcane);
        // The red deepens from the edge inward, a ring more every beat
        std::array<float, 3> const sweep = { 0.8f, 0.6f, 0.4f };
        for (std::size_t index = 0; index < sweep.size(); ++index)
        {
            uint32 const at = 1000 * uint32(index + 1);
            float const inner = BigBangRadius * sweep[index];
            scheduler.Schedule(Milliseconds(at), [this, center, inner, warning, at](TaskContext)
            {
                GroundIndicators::ShowRing(me, center, BigBangRadius, inner, warning - at);
            });
        }
    }

    void BigBang()
    {
        // One bright moment in the fight: the true form's arrival (phase 3); the blast here stays on the ground
        GroundIndicators::Burst(me, Ground(_liftFrom), GroundIndicators::Theme::Arcane);
        std::vector<Player*> const hit = PlayersIn(_bigBang);
        for (Player* player : hit)
        {
            player->SendPlaySpellVisual(KIT_BIG_BANG_HIT);
            Hit(player, SPELL_BIG_BANG, BigBangPct, true);
        }
        Resolve(hit.empty());
        Land(true);
    }

    void EnterIntermission1()
    {
        _phase = Phase::Intermission1;
        EndWindup();
        Land(false);
        Talk(SAY_INTERMISSION_1);
        Hold();
        me->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        _exposed = true;
        Resurgence();
    }

    // Two Fragments d'éternité from the edge, walking to the god: the first one marked with the skull, which bots kill
    // first (DpsTargetValue)
    void SummonFragments()
    {
        _fragments.clear();
        float const base = frand(0.0f, 2.0f * float(M_PI));
        uint32 const count = TierAbove() > 0 ? FragmentCountHigh : FragmentCount;
        for (uint32 index = 0; index < count; ++index)
        {
            float const angle = base + float(M_PI) * 2.0f * float(index) / float(count);
            Position const spawn = Ground(Position(me->GetPositionX() + std::cos(angle) * FragmentSpawnDistance,
                me->GetPositionY() + std::sin(angle) * FragmentSpawnDistance, ArenaFloorZ));
            if (TempSummon* fragment = me->SummonCreature(NPC_FRAGMENT, spawn, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000))
            {
                fragment->SetFacingToObject(me);
                fragment->SetWalk(true);
                fragment->SetSpeed(MOVE_WALK, FragmentWalkSpeedRate);
                _fragments.push_back(fragment->GetGUID());
            }
        }
        MarkFragment();
    }

    void MarkFragment()
    {
        Creature* next = nullptr;
        for (ObjectGuid const& guid : _fragments)
            if (Creature* fragment = me->GetMap()->GetCreature(guid); fragment && fragment->IsAlive())
            {
                next = fragment;
                break;
            }
        if (!next)
            return;
        for (Player* player : Listeners())
            if (Group* group = player->GetGroup())
            {
                group->SetTargetIcon(7, ObjectGuid::Empty, next->GetGUID());
                break;
            }
    }

    // The fragments walk to the god; one reaching it merges: it heals the god and bursts on the raid
    void UpdateFragments()
    {
        for (ObjectGuid const& guid : _fragments)
        {
            Creature* fragment = me->GetMap()->GetCreature(guid);
            if (!fragment || !fragment->IsAlive())
                continue;
            // Its middle at the god's: the god's large hitbox leaves no time to kill it otherwise
            if (fragment->GetExactDist2d(me) <= FragmentReach)
            {
                MergeFragment(fragment);
                continue;
            }
            if (fragment->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                fragment->GetMotionMaster()->MovePoint(1, me->GetPosition());
        }
    }

    void MergeFragment(Creature* fragment)
    {
        fragment->SendPlaySpellVisual(KIT_BLACK_HOLE_HIT);
        me->ModifyHealth(int32(float(me->GetMaxHealth()) * FragmentHealPct / 100.0f));
        for (Player* player : ArenaPlayers())
            Hit(player, SPELL_ETERNITY_SHARD, FragmentBurstPct, false);
        fragment->DespawnOrUnsummon(500ms);
        MarkFragment();
    }

    void EnterPhase2()
    {
        // Fragments still standing reach it as the fight resumes
        for (ObjectGuid const& guid : _fragments)
            if (Creature* fragment = me->GetMap()->GetCreature(guid); fragment && fragment->IsAlive())
                MergeFragment(fragment);
        _fragments.clear();
        _phase = Phase::Two;
        _exposed = false;
        Resume();
    }

    // The steady damage of the intermissions: the healers' mana break
    void RaidTick()
    {
        for (Player* player : ArenaPlayers())
            Hit(player, SPELL_ETERNITY_SHARD, RaidTickPct, false);
    }

    // Singularité: a black hole in the platform, pulling everyone near it into its pool
    void Singularity()
    {
        // Learnt: a third of a turn round from the last one
        float const turn = float(_singularities++) * 2.0f * float(M_PI) / 3.0f;
        Position const hole = FixedSpot(ArenaCenter.GetOrientation() + turn, SingularityDistance);
        uint32 const lasts = SingularityWarningMs + SingularityTicks * 1000;
        me->SummonCreature(NPC_SINGULARITY, hole, TEMPSUMMON_TIMED_DESPAWN, lasts);
        // No red: the black hole's own look says where its pool is. The bots still keep out of it.
        GroundIndicators::Area pool;
        pool.kind = GroundIndicators::Area::Kind::Circle;
        pool.origin = hole;
        pool.radius = SingularityRadius;
        GroundIndicators::WatchArea(me, pool, lasts);
        PlayOnGround(hole, KIT_SINGULARITY);
        auto const failed = std::make_shared<bool>(false);
        for (uint32 tick = 1; tick <= SingularityTicks; ++tick)
        {
            scheduler.Schedule(Milliseconds(SingularityWarningMs + tick * 1000), [this, pool, tick, failed](TaskContext)
            {
                if (tick == SingularityTicks)
                    scheduler.Schedule(10ms, [this, failed](TaskContext) { Resolve(!*failed); });
                for (Player* player : ArenaPlayers())
                {
                    float const distance = player->GetExactDist2d(&pool.origin);
                    if (pool.Contains(*player))
                    {
                        Hit(player, SPELL_SINGULARITY, SingularityPoolPct, true);
                        *failed = true;
                    }
                    else if (distance <= SingularityPullReach)
                        // A knockback from the far side of the player: towards the hole (bots take no negative speed)
                        player->KnockbackFrom(2.0f * player->GetPositionX() - pool.origin.GetPositionX(),
                            2.0f * player->GetPositionY() - pool.origin.GetPositionY(), SingularityPullSpeed, 1.5f);
                }
            });
        }
    }

    // Étoile effondrée: a star to share, three players or more in its circle, or it bursts on everyone
    void CollapsingStar()
    {
        // Learnt: between the black hole's spots, a third of a turn round from the last one
        Position const where = FixedSpot(ArenaCenter.GetOrientation() + float(M_PI) / 3.0f +
            float(_stars++) * 2.0f * float(M_PI) / 3.0f, StarDistance);
        me->SummonCreature(NPC_COLLAPSING_STAR, where, TEMPSUMMON_TIMED_DESPAWN, StarSoakMs + 500);
        GroundIndicators::ShowSoak(me, where, StarRadius, StarSoakMs, StarSoakersBots);
        GroundIndicators::Area soak;
        soak.kind = GroundIndicators::Area::Kind::Circle;
        soak.origin = where;
        soak.radius = StarRadius;
        scheduler.Schedule(Milliseconds(StarSoakMs), [this, soak](TaskContext)
        {
            PlayOnGround(soak.origin, KIT_BLACK_HOLE_HIT);
            std::vector<Player*> const soakers = PlayersIn(soak);
            if (soakers.size() >= StarSoakers)
            {
                for (Player* player : soakers)
                    Hit(player, SPELL_COLLAPSING_STAR, StarSharedPct / float(soakers.size()), false);
                Resolve(true);
                return;
            }
            Resolve(false);
            for (Player* player : ArenaPlayers())
                Hit(player, SPELL_COLLAPSING_STAR, StarFailPct, false);
        });
    }

    void EnterIntermission2()
    {
        _phase = Phase::Intermission2;
        EndWindup();
        Talk(SAY_INTERMISSION_2);
        Hold();
        Resurgence();
        me->GetMotionMaster()->MovePoint(POINT_CENTER, ArenaCenter);
        me->SendPlaySpellVisual(KIT_REORIGINATION);
    }

    // Constellation: two star lines across the platform, one after the other
    void Constellation()
    {
        me->SendPlaySpellVisual(KIT_REORIGINATION);
        uint32 const lines = 2 + TierAbove() / 3;
        auto const outcome = Expect(lines);
        for (uint32 line = 0; line < lines; ++line)
        {
            // Learnt: each line turned ConstellationTurn from the last, passing by turns left of the middle, through
            // it, right of it
            uint32 const index = _constellationLines++;
            float const facing = Position::NormalizeOrientation(ArenaCenter.GetOrientation() +
                float(index) * ConstellationTurn);
            float const offset = (float(index % 3) - 1.0f) * ConstellationOffset;
            float const half = ConstellationLength / 2.0f;
            Position const start = Ground(Position(
                ArenaCenter.GetPositionX() - std::cos(facing) * half - std::sin(facing) * offset,
                ArenaCenter.GetPositionY() - std::sin(facing) * half + std::cos(facing) * offset, ArenaFloorZ));
            GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, start, facing,
                ConstellationLength, ConstellationWidth, ConstellationWarningMs, GroundIndicators::Theme::Arcane);
            scheduler.Schedule(Milliseconds(ConstellationWarningMs), [this, area, start, facing, outcome](TaskContext)
            {
                for (float along = 8.0f; along < ConstellationLength; along += 16.0f)
                    PlayOnGround(Position(start.GetPositionX() + std::cos(facing) * along,
                        start.GetPositionY() + std::sin(facing) * along, ArenaFloorZ), KIT_COSMIC_SMASH);
                std::vector<Player*> const hit = PlayersIn(area);
                for (Player* player : hit)
                    Hit(player, SPELL_CONSTELLATION, ConstellationPct, true);
                Settle(outcome, !hit.empty());
            });
        }
    }

    // The edge turns deadly for the rest of the fight
    void EnterSetup()
    {
        _phase = Phase::Setup;
        Talk(SAY_SETUP);
        Resume();
        uint32 const lasts = AtHardEnrage - Elapsed() + 2000;
        _edgeArea = GroundIndicators::ShowRing(me, ArenaCenter, EdgeOuterRadius, EdgeInnerRadius, lasts,
            GroundIndicators::Theme::Shadow);
        _edge = true;
        _nextEdgeMs = Elapsed() + 2000;
    }

    void EdgeTick()
    {
        for (Player* player : PlayersIn(_edgeArea))
            Hit(player, SPELL_DEVOURING_VOID, EdgePct, true);
    }

    void Meteors()
    {
        auto const outcome = Expect(MeteorsPerWave);
        for (uint32 index = 0; index < MeteorsPerWave; ++index)
        {
            Position const where = ArenaSpot(4.0f, ArenaWalkRadius - 2.0f);
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, where, MeteorRadius, MeteorWarningMs,
                GroundIndicators::Theme::Fire);
            scheduler.Schedule(Milliseconds(MeteorWarningMs), [this, area, outcome](TaskContext)
            {
                PlayOnGround(area.origin, KIT_COSMIC_SMASH);
                std::vector<Player*> const hit = PlayersIn(area);
                for (Player* player : hit)
                    Hit(player, SPELL_METEORS, MeteorPct, true);
                Settle(outcome, !hit.empty());
            });
        }
    }

    // The drop after "Contemplez l'infini.": the true form
    void EnterPhase3()
    {
        _phase = Phase::Three;
        me->SetObjectScale(RevealScale);
        me->AddAura(SPELL_REVEAL_PARTICLES, me);
        me->AddAura(SPELL_REVEAL_GLOW, me);
        me->SendPlaySpellVisual(KIT_ARRIVAL);
        ShowStarryRoom(true);
        for (Player* player : ArenaPlayers())
            Hit(player, SPELL_ETERNITY_SHARD, RevealPulsePct, false);
        bool const lifted = _lifted;
        Land(true);
        if (!lifted)
            Resume();
    }

    // Supernova: everything around the god but three lanes, marked in gold
    void Supernova()
    {
        Position const center = Ground(me->GetPosition());
        // Learnt: the lanes turn SupernovaTurn from the last Supernova's, always the same way
        float const base = ArenaCenter.GetOrientation() + float(_supernovas++) * SupernovaTurn;
        std::vector<GroundIndicators::Area> areas;
        areas.push_back(GroundIndicators::ShowCircle(me, center, SupernovaCoreRadius, SupernovaWarningMs,
            GroundIndicators::Theme::Holy));
        for (uint32 index = 0; index < 3; ++index)
        {
            float const facing = Position::NormalizeOrientation(base + index * 2.0f * float(M_PI) / 3.0f);
            areas.push_back(GroundIndicators::ShowCone(me, center, facing, SupernovaRadius, SupernovaArc,
                SupernovaWarningMs, GroundIndicators::Theme::Holy));
            // The lane after this cone: golden, nothing red
            float const lane = facing + float(M_PI) / 3.0f;
            GroundIndicators::Area mark;
            mark.origin = Ground(Position(center.GetPositionX() + std::cos(lane) * SupernovaLaneDistance,
                center.GetPositionY() + std::sin(lane) * SupernovaLaneDistance, ArenaFloorZ));
            mark.radius = 3.0f;
            GroundIndicators::ShowParticles(me, mark, GroundIndicators::Theme::Holy, SupernovaWarningMs);
        }
        me->SendPlaySpellVisual(KIT_BIG_BANG_GATHER);
        scheduler.Schedule(Milliseconds(SupernovaWarningMs), [this, areas](TaskContext)
        {
            // Bursts over the core and along each cone, on the ground: no flash over the whole screen
            for (GroundIndicators::Area const& area : areas)
            {
                if (area.kind != GroundIndicators::Area::Kind::Cone)
                {
                    GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Holy);
                    continue;
                }
                float const facing = area.origin.GetOrientation();
                for (float distance : { 20.0f, 35.0f, 50.0f })
                    GroundIndicators::Burst(me, Position(area.origin.GetPositionX() + std::cos(facing) * distance,
                        area.origin.GetPositionY() + std::sin(facing) * distance, ArenaFloorZ),
                        GroundIndicators::Theme::Holy);
            }
            bool failed = false;
            for (Player* player : ArenaPlayers())
                if (std::ranges::any_of(areas, [player](GroundIndicators::Area const& area)
                    { return area.Contains(*player); }))
                {
                    Hit(player, SPELL_SUPERNOVA, SupernovaPct, true);
                    failed = true;
                }
            Resolve(!failed);
        });
    }

    // Jugement divin: two players carry a circle; whoever else is in one when it lands takes it
    void Judgement()
    {
        std::vector<Player*> const carriers = PickCarriers(JudgementCarriers, false);
        auto const outcome = Expect(uint32(carriers.size()));
        for (Player* carrier : carriers)
        {
            Carries(carrier, JudgementMs);
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, JudgementRadius,
                JudgementMs);
            ObjectGuid const guid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(JudgementMs), [this, area, guid, outcome](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                {
                    Settle(outcome, false);
                    return;
                }
                carrier->SendPlaySpellVisual(KIT_ASCEND_HIT);
                Hit(carrier, SPELL_JUDGEMENT, JudgementCarrierPct, false);
                bool failed = false;
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                    {
                        Hit(player, SPELL_JUDGEMENT, JudgementOtherPct, true);
                        failed = true;
                    }
                Settle(outcome, failed);
            });
        }
    }

    // Rayons stellaires: three players carry a star of four rays that follows them, its rays pointing a way of their
    // own; when it lands whoever else stands in a ray takes it. Kept away from the others, it only costs its carrier
    // a little.
    void StarRays()
    {
        std::vector<Player*> const carriers = PickCarriers(StarRaysCarriers + (TierAbove() >= 3) + (TierAbove() >= 6),
            false);
        auto const outcome = Expect(uint32(carriers.size()));
        for (Player* carrier : carriers)
        {
            Carries(carrier, StarRaysMs);
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedStar(me, carrier, StarRaysMs);
            ObjectGuid const guid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(StarRaysMs), [this, area, guid, outcome](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                {
                    Settle(outcome, false);
                    return;
                }
                GroundIndicators::Area const rays = GroundIndicators::CurrentArea(carrier, area);
                // The rays flare along their length
                for (uint32 arm = 0; arm < 4; ++arm)
                {
                    float const facing = rays.origin.GetOrientation() + arm * float(M_PI) / 2.0f;
                    for (float along : { 4.0f, 8.5f })
                        GroundIndicators::Burst(me, Position(rays.origin.GetPositionX() + std::cos(facing) * along,
                            rays.origin.GetPositionY() + std::sin(facing) * along, rays.origin.GetPositionZ()),
                            GroundIndicators::Theme::Arcane);
                }
                Hit(carrier, SPELL_STAR_RAYS, StarRaysCarrierPct, false);
                bool failed = false;
                for (Player* player : PlayersIn(rays))
                    if (player != carrier)
                    {
                        Hit(player, SPELL_STAR_RAYS, StarRaysOtherPct, true);
                        failed = true;
                    }
                Settle(outcome, failed);
            });
        }
    }

    // Poids de l'éternité: a stack on everyone, every 10 s of phase 3
    void Weight()
    {
        for (Player* player : ArenaPlayers())
        {
            uint32 const stacks = ++_weight[player->GetGUID()];
            ShowStacks(player, SPELL_WEIGHT, stacks);
        }
    }

    void ShowStacks(Player* player, uint32 spellId, uint32 stacks)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
            return;
        Aura* aura = player->GetAura(spellId);
        if (!aura)
            aura = me->AddAura(spellId, player);
        if (aura)
        {
            aura->SetStackAmount(uint8(std::min<uint32>(stacks, 255)));
            aura->RefreshDuration();
        }
    }

    void EnterFinal()
    {
        _phase = Phase::Final;
        EndWindup();
        Talk(SAY_FINAL);
        Hold();
        me->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        me->SendPlaySpellVisual(KIT_ASCEND_CAST);
    }

    // La Fin des Temps: a share of everyone's own maximum health, the tier left out (it already is everyone's
    // health), and Fin imminente piling up on it
    void EndPulse()
    {
        me->SendPlaySpellVisual(KIT_ASCEND_CAST);
        float const tier = std::max(GetChallengeDamageFactorOf(me), 0.01f);
        uint32 const spellId = SpellOf(SPELL_END_OF_TIMES);
        for (Player* player : ArenaPlayers())
        {
            player->SendPlaySpellVisual(KIT_ASCEND_HIT);
            float const amount = float(player->GetMaxHealth()) * EndPulseHealthPct / 100.0f / tier *
                TakenFactor(player);
            MythicTuning::DealAbilityDamage(me, player, spellId, uint32(amount));
            uint32 const stacks = ++_doom[player->GetGUID()];
            if (player->IsAlive())
                ShowStacks(player, SPELL_DOOM, stacks);
        }
    }

    // No player stands, only bots: the fight is lost (RaidFinder counts the wipe from the last player), so the god ends
    // it - pulses of the end of times, the last one killing what still stands - and the players do not watch their
    // bots fight on before they are brought home
    void CheckPlayersStanding()
    {
        bool botStanding = false;
        for (Player* player : ArenaPlayers())
        {
            if (!player->GetSession() || !player->GetSession()->IsBot())
                return;
            botStanding = true;
        }
        if (!botStanding)
            return;
        _finishing = true;
        for (uint32 pulse = 0; pulse < FinishPulses; ++pulse)
            scheduler.Schedule(Milliseconds(pulse * FinishPulseMs), [this, pulse](TaskContext)
            {
                me->SendPlaySpellVisual(KIT_ASCEND_CAST);
                _killLabel = "no player standing";
                for (Player* player : ArenaPlayers())
                {
                    player->SendPlaySpellVisual(KIT_ASCEND_HIT);
                    if (pulse + 1 == FinishPulses)
                        Unit::Kill(me, player);
                }
                _killLabel = nullptr;
            });
    }

    // 5:05, the silence: everyone in the Planetarium dies, whatever protects them, and the wipe counts
    void HardEnrage()
    {
        Talk(SAY_HARD_ENRAGE);
        me->SendPlaySpellVisual(KIT_BIG_BANG_BLAST);
        _killLabel = "hard enrage";
        for (Player* player : ArenaPlayers())
            Unit::Kill(me, player);
        _killLabel = nullptr;
    }

    // The off-tank's spot: at the god's side, a quarter turn from its target, on the side with fewer players, so the
    // two cones point away from each other and from the group
    // The two tanks, named once: the first the god hits is its main tank, the other its off-tank, whoever holds it
    // a moment. Read from its target, the off-tank's spot jumped to whichever tank had just lost the god (a taunt war
    // between bots): the two walked through each other, and through the group, with a cone each in tow.
    void NameTanks(Unit* victim)
    {
        auto const isTank = [this](Unit* unit)
        {
            return unit && unit->IsAlive() && unit->IsInMap(me) && unit->IsPlayer() && IsGroupTank(unit->ToPlayer());
        };
        Unit* main = ObjectAccessor::GetUnit(*me, _mainTank);
        if (!isTank(main))
        {
            // The off-tank takes over when the main one is gone; at the pull, the god's target if a tank pulled, else
            // the tank it holds most against
            Unit* off = ObjectAccessor::GetUnit(*me, _offTank);
            main = isTank(off) ? off : isTank(victim) ? victim : nullptr;
            if (!main)
                for (Unit* tank : BusterTargets())
                    if (isTank(tank))
                    {
                        main = tank;
                        break;
                    }
            _offTank.Clear();
            if (!main)
            {
                _mainTank.Clear();
                return;
            }
            _mainTank = main->GetGUID();
        }
        Unit* off = ObjectAccessor::GetUnit(*me, _offTank);
        if (!off || !off->IsAlive() || !off->IsInMap(me) || off == main)
        {
            _offTank.Clear();
            for (Unit* tank : BusterTargets())
                if (tank != main && isTank(tank))
                {
                    _offTank = tank->GetGUID();
                    break;
                }
        }
    }

    void UpdateOffTankSpot()
    {
        Unit* victim = me->GetVictim();
        if (!victim || !CanMelee())
            return;
        NameTanks(victim);
        Unit* main = ObjectAccessor::GetUnit(*me, _mainTank);
        Unit* off = ObjectAccessor::GetUnit(*me, _offTank);
        if (!main || !off)
            return;

        // The spot keeps its angle while the main tank moves a little: it only follows it round once it has gone
        // OffTankRebaseAngle round the god. The off-tank used to chase every turn.
        float const toVictim = me->GetAngle(main);
        if (_offTankSide != 0.0f &&
            std::fabs(std::remainder(toVictim - _offTankBase, 2.0f * float(M_PI))) > OffTankRebaseAngle)
            _offTankBase = toVictim;
        if (_offTankSide == 0.0f)
        {
            _offTankBase = toVictim;
            uint32 left = 0;
            uint32 right = 0;
            for (Player* player : ArenaPlayers())
            {
                if (player == main)
                    continue;
                float const relative = std::remainder(me->GetAngle(player) - toVictim, 2.0f * float(M_PI));
                ++(relative > 0.0f ? left : right);
            }
            _offTankSide = left <= right ? 1.0f : -1.0f;
        }

        float const angle = _offTankBase + _offTankSide * float(M_PI) / 2.0f;
        float const distance = me->GetCombatReach() + 2.0f;
        // Held: the named off-tank stands there and stays, rather than following the god about
        GroundIndicators::SetOffTankSpot(me, Ground(Position(me->GetPositionX() + std::cos(angle) * distance,
            me->GetPositionY() + std::sin(angle) * distance, ArenaFloorZ)), 2500, true, off);
    }

    SummonList _summons;
    std::vector<Step> _timeline;
    std::size_t _next = 0;
    uint32 _pullMs = 0;
    Phase _phase = Phase::None;
    bool _windup = false;
    bool _finishing = false;                    // no player standing: the bots are being finished off
    uint32 _nextStandingCheckMs = 0;
    bool _lingering = false;                    // a wipe's WipeLinger: standing, before it goes
    bool _lifted = false;
    bool _exposed = false;
    bool _edge = false;
    bool _defiConfirmed = false;
    uint32 _defiWaitMs = 0;
    uint32 _checkTimer = 0;
    uint32 _nextOffTankMs = 0;
    uint32 _nextEdgeMs = 0;
    uint32 _lastKillYellMs = 0;
    float _offTankSide = 0.0f;
    ObjectGuid _mainTank;                       // NameTanks
    ObjectGuid _offTank;
    float _offTankBase = 0.0f;                  // the angle of the tank round the god the spot is set from
    Position _liftFrom;
    GroundIndicators::Area _bigBang;
    GroundIndicators::Area _edgeArea;
    std::vector<ObjectGuid> _fragments;
    std::set<ObjectGuid> _fightListeners;

    std::map<ObjectGuid, uint32> _weight;
    std::map<ObjectGuid, uint32> _doom;

    // Against chance (ResetRework)
    struct Wounded
    {
        uint32 since = 0;
        uint32 until = 0;
    };
    struct LastHit
    {
        uint32 spellId = 0;                     // 0: its melee
        uint32 at = 0;
        bool avoidable = false;
    };
    std::map<ObjectGuid, Wounded> _wounds;
    std::map<ObjectGuid, LastHit> _lastHit;
    std::map<std::string, uint32> _deaths;
    std::map<ObjectGuid, uint32> _lastCarried;
    std::map<ObjectGuid, uint32> _carryingUntil;
    ObjectGuid _sparing;                        // the player the hit being dealt wounds rather than kills
    char const* _killLabel = nullptr;           // what the deaths being dealt are logged as (the ends)
    uint32 _fracture = 0;
    uint32 _bestFracture = 0;
    uint32 _cleanCount = 0;
    uint32 _failCount = 0;
    uint32 _revived = 0;
    uint32 _supernovas = 0;
    uint32 _singularities = 0;
    uint32 _stars = 0;
    uint32 _crosses = 0;
    uint32 _constellationLines = 0;
};

boss_infinite_god* FindGod(Player* player)
{
    if (!player || !player->IsInWorld())
        return nullptr;
    Creature* god = player->FindNearestCreature(NPC_INFINI, 250.0f, true);
    return god ? dynamic_cast<boss_infinite_god*>(god->AI()) : nullptr;
}

using namespace Acore::ChatCommands;

// .infini info | skip <seconds> | music <pull|arena|wipe>: for game masters trying the fight; the music goes to the
// game master alone, as the fight sends it (pull: the track, arena and wipe: the arena's)
class InfiniteGodCommandScript final : public CommandScript
{
public:
    InfiniteGodCommandScript() : CommandScript("InfiniteGodCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable infiniTable = {
            { "info",  HandleInfo,  SEC_GAMEMASTER, Console::No },
            { "skip",  HandleSkip,  SEC_GAMEMASTER, Console::No },
            { "music", HandleMusic, SEC_GAMEMASTER, Console::No },
            { "pull",  HandlePull,  SEC_GAMEMASTER, Console::No },
            { "gear",  HandleGear,  SEC_GAMEMASTER, Console::No },
            { "wmo",   HandleWmo,   SEC_GAMEMASTER, Console::No },
            { "cast",  HandleCast,  SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "infini", infiniTable },
        };
        return commandTable;
    }

    // .infini gear [item level] [plain]: a piece of the god's gear for the game master (304 unless given), as a win
    // gives it; plain: the same Mythic+ piece without the god's touch, to compare
    static bool HandleGear(ChatHandler* handler, Optional<uint32> itemLevel, Optional<std::string> plain)
    {
        if (plain && *plain == "plain")
            GiveMythicLootItem(handler->GetPlayer(), itemLevel.value_or(304));
        else
            GiveInfiniteGodLootItem(handler->GetPlayer(), itemLevel.value_or(304));
        return true;
    }

    // .infini cast <star|crosses|orb|spin|sweep|cleave|twin>: one of its attacks now, in a fight (to try one out)
    static bool HandleCast(ChatHandler* handler, std::string what)
    {
        boss_infinite_god* god = FindGod(handler->GetPlayer());
        if (!god || !god->CastNow(what))
        {
            handler->SendSysMessage("No fighting L'Infini within 250 yards, or no such attack "
                "(star, crosses, orb, spin, sweep, cleave, twin).");
            handler->SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    // .infini wmo: the WMO area under the game master (root and group ids, as WMOAreaTable.dbc names them), and its
    // WMOAreaTable row: where a room's zone music comes from. Logged too (module.infinite).
    static bool HandleWmo(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        uint32 flags = 0;
        int32 adtId = 0;
        int32 rootId = 0;
        int32 groupId = 0;
        bool const found = player->GetMap()->GetAreaInfo(player->GetPhaseMask(), player->GetPositionX(),
            player->GetPositionY(), player->GetPositionZ(), flags, adtId, rootId, groupId);
        WMOAreaTableEntry const* row = found ? GetWMOAreaTableEntryByTripple(rootId, adtId, groupId) : nullptr;
        std::string const text = Acore::StringFormat("map {} at {:.1f} {:.1f} {:.1f}: vmap {} root {} adt {} group {} "
            "flags {:#x}, WMOAreaTable {}", player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
            player->GetPositionZ(), found ? "found" : "none", rootId, adtId, groupId, flags, row ? row->Id : 0);
        LOG_INFO("module.infinite", "infini wmo: {}", text);
        handler->SendSysMessage(text);
        return true;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_infinite_god* god = FindGod(handler->GetPlayer());
        handler->SendSysMessage(god ? god->Describe() : std::string("L'Infini is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_infinite_god* god = FindGod(handler->GetPlayer());
        if (!god || !god->Skip(seconds))
        {
            handler->SendSysMessage("L'Infini is not fighting within 250 yards.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        handler->SendSysMessage(god->Describe());
        return true;
    }

    static bool HandleMusic(ChatHandler* handler, std::string event)
    {
        Player* player = handler->GetPlayer();
        uint32 const soundId = event == "pull" ? MUSIC_FIGHT : (event == "stop" || event == "wipe") ? MUSIC_SILENCE : 0;
        if (!soundId)
        {
            handler->SendSysMessage(".infini music <pull|wipe|stop>");
            handler->SetSentErrorMessage(true);
            return false;
        }
        player->PlayDirectMusic(soundId, player);
        handler->SendSysMessage(Acore::StringFormat("SMSG_PLAY_MUSIC {} sent.", soundId));
        return true;
    }

    // .infini pull: the god engages the game master (a test pull without walking up to it)
    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* god = player ? player->FindNearestCreature(NPC_INFINI, 150.0f) : nullptr;
        if (!god || !god->IsAlive())
        {
            handler->SendSysMessage("No living L'Infini within 150 yards.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        god->SetReactState(REACT_AGGRESSIVE);
        god->AI()->AttackStart(player);
        handler->SendSysMessage("L'Infini pulled.");
        return true;
    }
};
}

void AddInfiniteGodScripts()
{
    RegisterCreatureAI(boss_infinite_god);
    new InfiniteGodCommandScript();
}
