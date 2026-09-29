#include "GroundIndicators.h"
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
#include "Log.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "ObjectAccessor.h"
#include "Player.h"
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
//      platform, and Rayons stellaires mark three players with a star of four rays that follows them and turns with
//      their facing: they hold still and aim it away from the others.
// 2:19 Setup: the platform's edge turns deadly, meteor waves. 2:32.9 the break: it rises, "Contemplez l'infini.", and
//      on the drop at 2:36.9 shows its true form (bigger, glowing, the Planetarium's floor turns to stars).
// 2:36.9 Phase 3: everything at once and faster, the cleave alternating with Frappes jumelles (a buster on each
//      tank), Supernova (all but three lanes), Jugement divin (carried circles: spread), Poids de l'éternité (+3%
//      damage taken every 10 s).
// 4:55 La Fin des Temps: a pulse every 2 s for 45% of everyone's maximum health, each one adding Fin imminente
//      (+20% damage taken). 5:05: everyone in the Planetarium dies, whatever protects them.
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
// The super high-end boss: Défi I is tuned on a group in item level 300 gear with the paragon that goes with it
// (mod-playerbots ChallengeTiers::BossProfiles, shown on the board's card). In the key curves of MythicDungeon.h that
// is the gear of a key of 20.25 (one key under its loot, 219 + 4 x key), whose recommended paragon is 51 (5 x 10.25).
// Damage is a share of that key's reference health (Mythic::GetDamageReference(20.25): a damage dealer's 97 300
// health times the key's pressure 1.26, about 122 300); a tank has about 1.45 of a damage dealer's health. Every tier
// above multiplies the god's damage (ChallengeTierUnitScript: x1.12 a tier, x2.7 at Défi X) and health (x1.22 a tier,
// x6.1 at Défi X) and asks for 15 paragon more (66 at Défi II, 186 at Défi X), the players' own health growing slower
// than its damage. Its health: HealthModifier in stat_growth_infinite_god.sql (47.6 million at Défi I, a tight damage
// check for damage dealers at 20 000-30 000 on one target).
constexpr float ReferenceKey = 20.25f;

constexpr float CleaveTankPct = 110.0f;         // Double fauchage cosmique, on each tank a cone is aimed at
constexpr float CleaveOtherPct = 160.0f;        // ... on anyone else in a cone
constexpr float TwinStrikePct = 130.0f;         // Frappes jumelles, on each tank (phase 3)
constexpr float StarfallPct = 55.0f;            // Pluie d'étoiles, each circle
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

// Sizes (yards) and warnings (ms)
constexpr float CleaveRadius = 35.0f;
constexpr float CleaveArc = 60.0f;
constexpr uint32 CleaveWarningMs = 3000;          // the cones turn with their tanks until they land
constexpr float StarfallRadius = 5.0f;
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
constexpr NamedSpell SPELL_TANK_ARCANE = { 90763, 64412 };      // Fracture stellaire (Phase Punch)
// Debuffs the players see: dummy auras, the script does what they say
constexpr uint32 SPELL_DOOM = 90753;            // Fin imminente
constexpr uint32 SPELL_WEIGHT = 90754;          // Poids de l'éternité
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
enum Texts : uint8
{
    SAY_AGGRO               = 0,
    EMOTE_BIG_BANG          = 1,
    SAY_BIG_BANG            = 2,
    SAY_INTERMISSION_1      = 3,
    EMOTE_EXPOSED           = 4,
    SAY_INTERMISSION_1_END  = 5,
    EMOTE_COLLAPSING_STAR   = 6,
    EMOTE_SINGULARITY       = 7,
    SAY_INTERMISSION_2      = 8,
    SAY_SETUP               = 9,
    EMOTE_EDGE              = 10,
    SAY_REVEAL              = 11,
    EMOTE_SUPERNOVA         = 12,
    EMOTE_JUDGEMENT         = 13,
    SAY_FINAL               = 14,
    SAY_HARD_ENRAGE         = 15,
    SAY_KILL                = 16,
    SAY_DEATH               = 17,
    EMOTE_FRAGMENT          = 18,
    EMOTE_STAR_RAYS         = 19,
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

    // Phase 1: the Big Bang's break (23-29.5 s) keeps clear of everything else
    for (uint32 at : { 7000u, 18000u, 35000u })
        once(Ability::Cleave, at);
    for (uint32 at : { 3000u, 9000u, 15000u, 20500u, 33500u, 39000u })
        once(Ability::Starfall, at);
    for (uint32 at : { 10000u, 20000u, 31500u, 40000u })
        once(Ability::Gravity, at);
    every(Ability::TankArcane, 4000, TankArcaneMs, AtIntermission1);
    once(Ability::BigBangRise, AtBigBangRise);
    once(Ability::BigBangYell, AtBigBangYell);
    once(Ability::BigBang, AtBigBang);

    // Intermission 1
    once(Ability::Intermission1, AtIntermission1);
    once(Ability::Fragments, AtFragments);
    every(Ability::RaidTick, AtIntermission1 + 1200, 2000, AtPhase2 - 1000);
    once(Ability::Intermission1Yell, AtIntermission1Yell);

    // Phase 2: phase 1 a fifth faster, the black hole and the star
    once(Ability::Phase2, AtPhase2);
    every(Ability::Cleave, AtPhase2 + 3000, 15000, AtIntermission2 - 4000);
    every(Ability::Starfall, AtPhase2 + 1500, 5000, AtIntermission2 - 2500);
    every(Ability::Gravity, AtPhase2 + 7500, 10000, AtIntermission2);
    every(Ability::TankArcane, AtPhase2 + 2000, TankArcaneMs, AtIntermission2);
    every(Ability::Singularity, AtPhase2 + 5500, 15000, AtIntermission2 - 8000);
    every(Ability::CollapsingStar, AtPhase2 + 10500, 20000, AtIntermission2 - StarSoakMs);

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

    // Phase 3: the cleave and the twin strikes alternating every 10 s, the black hole and the star alternating,
    // Supernova every 30 s and Jugement divin every 25 s kept 3 s off the big ones
    once(Ability::Phase3, AtPhase3);
    for (uint32 at = AtPhase3 + 3000, index = 0; at < AtFinal - 1500; at += 10000, ++index)
        once(index % 2 ? Ability::TwinStrikes : Ability::Cleave, at);
    every(Ability::Starfall, AtPhase3 + 1500, 5000, AtFinal - 2000);
    every(Ability::Gravity, AtPhase3 + 6000, 15000, AtFinal);
    every(Ability::Weight, AtPhase3 + 10000, 10000, AtFinal);
    every(Ability::TankArcane, AtPhase3 + 2000, TankArcaneMs, AtFinal);
    std::vector<Step> big;
    for (uint32 at = AtPhase3 + 8000, index = 0; at < AtFinal - StarSoakMs; at += 12500, ++index)
        big.push_back({ at, index % 2 ? Ability::CollapsingStar : Ability::Singularity });
    for (uint32 at = AtPhase3 + 14000; at < AtFinal - SupernovaWarningMs; at += 30000)
        big.push_back({ at, Ability::Supernova });
    for (uint32 at = AtPhase3 + 26000; at < AtFinal - JudgementMs; at += 25000)
        big.push_back({ at, Ability::Judgement });
    std::sort(big.begin(), big.end(), [](Step const& left, Step const& right) { return left.at < right.at; });
    uint32 previous = 0;
    for (Step step : big)
    {
        if (previous && step.at < previous + 3000)
            step.at = previous + 3000;
        if (step.at >= AtFinal - 2000)
            continue;
        steps.push_back(step);
        previous = step.at;
    }

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
    return Mythic::GetDamageReference(ReferenceKey);
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
        Talk(SAY_AGGRO);
        _fightListeners.clear();
        for (Player* player : Listeners())
        {
            SendMusic(player, MUSIC_FIGHT);
            _fightListeners.insert(player->GetGUID());
        }
        LOG_INFO("module.infinite", "L'Infini pulled instance={} health={} tier factor={}", me->GetInstanceId(),
                 me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
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
        if (_phase == Phase::Intermission1)
            damage = uint32(float(damage) * (1.0f + ExposedDamageTakenPct / 100.0f));
    }

    // Its melee follows the players' Poids de l'éternité and Fin imminente, as its abilities do
    // Never less than MeleeFloorPct of the reference hit, whatever the target's armour: a tank has to be healed
    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type == DIRECT_DAMAGE && victim && victim->IsPlayer())
        {
            float const floor = Reference() * MeleeFloorPct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
            damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
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
        if (CanMelee())
            DoMeleeAttackIfReady();
    }

    // --- Music, and the game master's commands -----------------------------------------------------------------
    // What ends the track, to each player it was sent to and still in the instance
    // Wherever they are now: a wipe's players may be home already, the track still playing for them
    void EndTrack(uint32 soundId)
    {
        for (ObjectGuid const& guid : _fightListeners)
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
                                   "reference {:.0f}, next step {}/{}", uint32(_phase), Elapsed() / 1000.0f,
                                   me->GetHealth(), me->GetMaxHealth(), me->GetHealthPct(),
                                   GetChallengeDamageFactorOf(me), Reference(), _next, _timeline.size());
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
        _offTankSide = 0.0f;
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

    // What a player takes on top of a hit: Poids de l'éternité and Fin imminente
    float TakenFactor(Unit const* victim) const
    {
        float factor = 1.0f;
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

    void Hit(Player* player, NamedSpell const& spell, float percent, bool avoidable)
    {
        if (!player || !player->IsAlive())
            return;
        if (avoidable)
            percent *= 1.0f + AvoidablePctPerTier / 100.0f * float(TierAbove());
        float const amount = Reference() * percent / 100.0f * TakenFactor(player);
        MythicTuning::DealAbilityDamage(me, player, SpellOf(spell), uint32(std::max(1.0f, amount)));
        if (avoidable)
            MythicTuning::ApplyImprudence(player);
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
            }
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
        for (Player* target : players)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, Ground(*target), StarfallRadius,
                StarfallWarningMs, GroundIndicators::Theme::Arcane);
            scheduler.Schedule(Milliseconds(StarfallWarningMs), [this, area](TaskContext)
            {
                PlayOnGround(area.origin, KIT_COSMIC_SMASH);
                for (Player* player : PlayersIn(area))
                    Hit(player, SPELL_STARFALL, StarfallPct, true);
            });
        }
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
        Talk(EMOTE_BIG_BANG);
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
        for (Player* player : PlayersIn(_bigBang))
        {
            player->SendPlaySpellVisual(KIT_BIG_BANG_HIT);
            Hit(player, SPELL_BIG_BANG, BigBangPct, true);
        }
        Land(true);
    }

    void EnterIntermission1()
    {
        _phase = Phase::Intermission1;
        EndWindup();
        Land(false);
        Talk(SAY_INTERMISSION_1);
        Talk(EMOTE_EXPOSED);
        Hold();
        me->SetEmoteState(EMOTE_STATE_SPELL_CHANNEL_OMNI);
        _exposed = true;
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
        Talk(EMOTE_FRAGMENT);
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
        Talk(EMOTE_SINGULARITY);
        Position const hole = ArenaSpot(10.0f, 20.0f);
        uint32 const lasts = SingularityWarningMs + SingularityTicks * 1000;
        me->SummonCreature(NPC_SINGULARITY, hole, TEMPSUMMON_TIMED_DESPAWN, lasts);
        GroundIndicators::Area const pool = GroundIndicators::ShowCircle(me, hole, SingularityRadius, lasts,
            GroundIndicators::Theme::Shadow);
        PlayOnGround(hole, KIT_SINGULARITY);
        for (uint32 tick = 1; tick <= SingularityTicks; ++tick)
        {
            scheduler.Schedule(Milliseconds(SingularityWarningMs + tick * 1000), [this, pool](TaskContext)
            {
                for (Player* player : ArenaPlayers())
                {
                    float const distance = player->GetExactDist2d(&pool.origin);
                    if (pool.Contains(*player))
                        Hit(player, SPELL_SINGULARITY, SingularityPoolPct, true);
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
        Talk(EMOTE_COLLAPSING_STAR);
        Position const where = ArenaSpot(12.0f, 22.0f);
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
                return;
            }
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
        me->GetMotionMaster()->MovePoint(POINT_CENTER, ArenaCenter);
        me->SendPlaySpellVisual(KIT_REORIGINATION);
    }

    // Constellation: two star lines across the platform, one after the other
    void Constellation()
    {
        me->SendPlaySpellVisual(KIT_REORIGINATION);
        for (uint32 line = 0; line < 2 + TierAbove() / 3; ++line)
        {
            float const from = frand(0.0f, 2.0f * float(M_PI));
            Position const start = Ground(Position(ArenaCenter.GetPositionX() + std::cos(from) * 40.0f,
                ArenaCenter.GetPositionY() + std::sin(from) * 40.0f, ArenaFloorZ));
            Position const through = ArenaSpot(0.0f, 15.0f);
            float const facing = start.GetAngle(&through);
            GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, start, facing,
                ConstellationLength, ConstellationWidth, ConstellationWarningMs, GroundIndicators::Theme::Arcane);
            scheduler.Schedule(Milliseconds(ConstellationWarningMs), [this, area, start, facing](TaskContext)
            {
                for (float along = 8.0f; along < ConstellationLength; along += 16.0f)
                    PlayOnGround(Position(start.GetPositionX() + std::cos(facing) * along,
                        start.GetPositionY() + std::sin(facing) * along, ArenaFloorZ), KIT_COSMIC_SMASH);
                for (Player* player : PlayersIn(area))
                    Hit(player, SPELL_CONSTELLATION, ConstellationPct, true);
            });
        }
    }

    // The edge turns deadly for the rest of the fight
    void EnterSetup()
    {
        _phase = Phase::Setup;
        Talk(SAY_SETUP);
        Talk(EMOTE_EDGE);
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
        for (uint32 index = 0; index < MeteorsPerWave; ++index)
        {
            Position const where = ArenaSpot(4.0f, ArenaWalkRadius - 2.0f);
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, where, MeteorRadius, MeteorWarningMs,
                GroundIndicators::Theme::Fire);
            scheduler.Schedule(Milliseconds(MeteorWarningMs), [this, area](TaskContext)
            {
                PlayOnGround(area.origin, KIT_COSMIC_SMASH);
                for (Player* player : PlayersIn(area))
                    Hit(player, SPELL_METEORS, MeteorPct, true);
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
        Talk(EMOTE_SUPERNOVA);
        Position const center = Ground(me->GetPosition());
        float const base = frand(0.0f, 2.0f * float(M_PI));
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
            for (Player* player : ArenaPlayers())
                if (std::ranges::any_of(areas, [player](GroundIndicators::Area const& area)
                    { return area.Contains(*player); }))
                    Hit(player, SPELL_SUPERNOVA, SupernovaPct, true);
        });
    }

    // Jugement divin: two players carry a circle; whoever else is in one when it lands takes it
    void Judgement()
    {
        Talk(EMOTE_JUDGEMENT);
        Unit* tank = me->GetVictim();
        std::vector<Player*> players = ArenaPlayers();
        std::erase_if(players, [tank](Player* player) { return player == tank; });
        Acore::Containers::RandomResize(players, JudgementCarriers);
        for (Player* carrier : players)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, JudgementRadius,
                JudgementMs);
            ObjectGuid const guid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(JudgementMs), [this, area, guid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                    return;
                carrier->SendPlaySpellVisual(KIT_ASCEND_HIT);
                Hit(carrier, SPELL_JUDGEMENT, JudgementCarrierPct, false);
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                        Hit(player, SPELL_JUDGEMENT, JudgementOtherPct, true);
            });
        }
    }

    // Rayons stellaires: three players carry a star of four rays that follows them and turns with their facing; when
    // it lands whoever else stands in a ray takes it. Held still and aimed away from the others, it only costs its
    // carrier a little.
    void StarRays()
    {
        Talk(EMOTE_STAR_RAYS);
        std::vector<Player*> players = ArenaPlayers();
        Acore::Containers::RandomResize(players, StarRaysCarriers + (TierAbove() >= 3) + (TierAbove() >= 6));
        for (Player* carrier : players)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedStar(me, carrier, StarRaysMs);
            ObjectGuid const guid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(StarRaysMs), [this, area, guid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, guid);
                if (!carrier || !carrier->IsAlive())
                    return;
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
                for (Player* player : PlayersIn(rays))
                    if (player != carrier)
                        Hit(player, SPELL_STAR_RAYS, StarRaysOtherPct, true);
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

    // 5:05, the silence: everyone in the Planetarium dies, whatever protects them, and the wipe counts
    void HardEnrage()
    {
        Talk(SAY_HARD_ENRAGE);
        me->SendPlaySpellVisual(KIT_BIG_BANG_BLAST);
        for (Player* player : ArenaPlayers())
            Unit::Kill(me, player);
    }

    // The off-tank's spot: at the god's side, a quarter turn from its target, on the side with fewer players, so the
    // two cones point away from each other and from the group
    void UpdateOffTankSpot()
    {
        Unit* victim = me->GetVictim();
        if (!victim || !CanMelee())
            return;

        float const toVictim = me->GetAngle(victim);
        if (_offTankSide == 0.0f)
        {
            uint32 left = 0;
            uint32 right = 0;
            for (Player* player : ArenaPlayers())
            {
                if (player == victim)
                    continue;
                float const relative = std::remainder(me->GetAngle(player) - toVictim, 2.0f * float(M_PI));
                ++(relative > 0.0f ? left : right);
            }
            _offTankSide = left <= right ? 1.0f : -1.0f;
        }

        float const angle = toVictim + _offTankSide * float(M_PI) / 2.0f;
        float const distance = me->GetCombatReach() + 2.0f;
        GroundIndicators::SetOffTankSpot(me, Ground(Position(me->GetPositionX() + std::cos(angle) * distance,
            me->GetPositionY() + std::sin(angle) * distance, ArenaFloorZ)), 2500);
    }

    SummonList _summons;
    std::vector<Step> _timeline;
    std::size_t _next = 0;
    uint32 _pullMs = 0;
    Phase _phase = Phase::None;
    bool _windup = false;
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
    Position _liftFrom;
    GroundIndicators::Area _bigBang;
    GroundIndicators::Area _edgeArea;
    std::vector<ObjectGuid> _fragments;
    std::set<ObjectGuid> _fightListeners;

    std::map<ObjectGuid, uint32> _weight;
    std::map<ObjectGuid, uint32> _doom;
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
