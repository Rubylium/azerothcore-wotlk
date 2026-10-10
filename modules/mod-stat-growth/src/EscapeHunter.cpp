#include "DefiBoss.h"
#include "GroundIndicators.h"

#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "LFG.h"
#include "Random.h"
#include "StringFormat.h"

#include <array>
#include <functional>
#include <map>
#include <set>
#include <string>

// Le Traqueur d'évadés, the Hellfire Gaol's second gate: a Défi board boss in the Blood Furnace, in Keli'dan the
// Breaker's hall (plan: .agents/plans/escape-hunter). 8 players - 2 tanks, 2 healers, 4 damage dealers - tuned for
// item level 477 and 650 paragon, as Gardien-chef Vorhan, the first gate.
//
// **A dance on his music.** The track is played once from the pull and every step lands on a beat of it - most on the
// snare (the third beat of a bar), the Traque's on the kick: the script holds the beat grid measured on the track
// (155 BPM) and places each step on a beat number, never on a time of its own. Something lands every bar or two:
// patterns drawn in red a bar ahead, each with its own rhythm to learn (surprising once, readable after):
// - Taillade (a cone on his tank, stacking Lacération: the tank swap) and its Revers behind him: stand at his flanks;
// - Volée de flèches: circles under players and around the hall;
// - le Collet: the snare closing around him, a ring then a circle then a ring...: in, out, in, out, a bar each;
// - l'Encerclement: arrows from three sides, the quarter spared turning clockwise a bar at a time;
// - le Rabattage: the hall's lanes swept, the odd ones then the even ones, then the same across;
// - la Charge de la meute: lines through players, one a beat;
// - le Bond: he leaps on the farthest player, then its shockwave everywhere but where he landed;
// - les Pièges, then their Déclenchement: traps under everyone, armed; later they burst one a beat;
// - le Moulinet: his chains turning round him, stopped dead on a beat - they strike where they stopped;
// - le Pistage: marks follow three players, freeze on the snare and strike where they froze;
// - la Battue: strips swept one a beat, a gap in each, sometimes back again;
// - l'Hallali on the peak: quarries away from all, the others under him to share the blow;
// - la Traque on the breakdown: he is gone in the dark - pounces, charges through the hall, then marks under everyone;
//   he comes back on the track's hardest hit, howling;
// - la Curée on the climax: above CureePct of his health, the group dies. The silence at 4:25.5 ends the hunt.
// A mistake marks (Débusqué: more damage taken for a while); each is told to the player it hit, in their language.
//
// What every Défi boss does alike (a challenge's instance only, the hall cleared, its health, its music, its hits, its
// lines to the players) is Defi::BossAI's (DefiBoss.h).

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As Vorhan's profile (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can be hit
// from the pull to the Curée (4:04.0) but during la Traque (2:04.0-2:41.2, he is away), and only part of that time on
// him (OnHimShare: a pattern every bar or two keeps the group on its feet). Measured on two 8-bot groups, 2026-10-10:
// one dealt 34M by the Curée, the other killed him (49.8M) at 3:34 - between them, the Curée's line at 15%.
constexpr float OnHimShare = 0.54f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, (244.0f - 37.2f) * OnHimShare, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("traqueur.health_scale", 1.0f);

// Keli'dan's hall: open floor 55 yd and more round its middle (measured on the navigation mesh, 2026-10-10), an exit
// to the south. The patterns keep within HallRadius of its middle.
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 120.0f, 60.0f };
constexpr float HallRadius = 40.0f;

// --- The music ------------------------------------------------------------------------------------------------------
// prison_first_boss_music_2 (Music.EscapeHunter): 155 BPM, a line fitted to its 684 beats (95% within 42 ms). A bar is
// 4 beats; its snare on the third (beat 4b + 2), in the breakdown its kick on the second. Sections start on bars: the
// build-up on bar 12, section A on 28, the peak on 76, the breakdown on 80, the rebuild on 104 (beat 416: the track's
// hardest hit), section B on 116, the outro on 160. Other hits: beats 55, 468, 517, 533, 575, 630 (the climax). The
// track ends at MusicEndMs.
constexpr Defi::BeatGrid Grid = { 168.2f, 387.09f };
constexpr uint32 MusicEndMs = 265460;

// --- His blows ------------------------------------------------------------------------------------------------------
// Shares of the reference health (a damage dealer of the profile)
constexpr float MeleeFloorPct = 10.0f;      // his swing on a player: at least this
// Taillade: his tank takes TailladeTankPct and a stack of Lacération (LacerationTakenPct more taken a stack, for
// LacerationMs); at SwapStacks the other tank takes him. Its Revers follows behind him two beats later.
constexpr float TailladeTankPct = 50.0f;
constexpr float TailladeRadius = 12.0f;
constexpr float TailladeArc = 120.0f;
constexpr float LacerationTakenPct = 15.0f;
constexpr uint32 LacerationMs = 25000;
constexpr uint8 SwapStacks = 3;
constexpr float HornPct = 15.0f;
constexpr float HowlPct = 35.0f;            // Hurlement: his return from the dark, on everyone
// What a mistake takes, by pattern
constexpr float ConePct = 70.0f;            // Taillade on another than his tank, Revers
constexpr float VolleyPct = 45.0f;
constexpr float PatternPct = 60.0f;         // Collet, Encerclement, Rabattage, Moulinet, Pistage
constexpr float ChargePct = 60.0f;          // la meute, les griffes
constexpr float LeapPct = 70.0f;            // le Bond, its shockwave
constexpr float TrapPct = 40.0f;            // an armed trap stepped in: and held TrapRootMs
constexpr float BurstPct = 55.0f;           // a trap bursting
constexpr float BattuePct = 70.0f;
constexpr float DarkPct = 60.0f;            // la Traque's pounces and marks
// The patterns' sizes
constexpr float VolleyRadius = 5.0f;
constexpr uint32 VolleyAround = 5;          // circles round the hall besides those under players
constexpr uint32 EncircleWaves = 4;
constexpr float SnareRadius = 16.0f;        // le Collet: its circle; its ring from there to the hall's edge
constexpr float LaneWidth = 10.0f;          // le Rabattage: five lanes across the hall
constexpr float ChargeWidth = 7.0f;
constexpr float ChargeLength = 44.0f;
constexpr float LeapRadius = 8.0f;
constexpr uint32 ShockwaveBeats = 8;
constexpr float TrapRadius = 3.0f;
constexpr float BurstRadius = 7.0f;
constexpr uint32 TrapRootMs = 3000;
constexpr float ChainLength = 40.0f;        // le Moulinet: two chains through him, a full turn in ChainTurnBeats
constexpr float ChainWidth = 6.0f;
constexpr uint32 ChainTurnBeats = 8;
constexpr float TrackRadius = 6.0f;         // le Pistage
constexpr uint32 TrackedPlayers = 3;
constexpr float PounceRadius = 7.0f;
constexpr float DarkMarkRadius = 6.0f;
// La Battue: BattueStrips strips across the hall, BattueStripDepth deep, a gap BattueGap wide in each; a strip lands a
// beat after the one before, shown BattueWarnBeats ahead
constexpr uint32 BattueStrips = 9;
constexpr float BattueStripDepth = 8.0f;
constexpr float BattueHalfWidth = 40.0f;
constexpr float BattueGap = 10.0f;
constexpr uint32 BattueWarnBeats = 8;
// L'Hallali: the quarries' circles (HallaliRadius), the group's share (HallaliSoakPct split among those in its circle)
constexpr float HallaliRadius = 8.0f;
constexpr float HallaliQuarryPct = 45.0f;
constexpr float HallaliMistakePct = 80.0f;
constexpr float HallaliSoakRadius = 7.0f;
constexpr float HallaliSoakPct = 160.0f;
constexpr uint32 HallaliMarkBeats = 12;
// A mistake: Débusqué for DebusqueMs, MistakeTakenPct more taken (a second mistake under it hurts all the more)
constexpr uint32 DebusqueMs = 8000;
constexpr float MistakeTakenPct = 50.0f;
// La Curée: above this share of his health as it lands, the group dies; under it, a group hit
constexpr float CureePct = 15.0f;
constexpr float CureeHitPct = 40.0f;
// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

enum Spells : uint32
{
    SPELL_TAILLADE          = 94800,
    SPELL_HORN              = 94801,
    SPELL_LANES             = 94802,
    SPELL_TRAP              = 94803,
    SPELL_VOLLEY            = 94804,
    SPELL_BATTUE            = 94805,
    SPELL_HALLALI           = 94806,
    SPELL_PACK_SHARE        = 94807,
    SPELL_POUNCE            = 94808,
    SPELL_CUREE             = 94809,
    SPELL_HUNT_OVER         = 94810,
    SPELL_REVERS            = 94811,
    SPELL_SNARE             = 94812,
    SPELL_ENCIRCLE          = 94813,
    SPELL_CHARGE            = 94814,
    SPELL_LEAP              = 94815,
    SPELL_SHOCKWAVE         = 94816,
    SPELL_BURST             = 94817,
    SPELL_HOWL              = 94818,
    SPELL_DARK_CHARGE       = 94819,
    SPELL_CHAINS            = 94825,
    SPELL_TRACKED           = 94826,
    SPELL_LACERATION        = 94820,
    SPELL_DEBUSQUE          = 94821,
    SPELL_QUARRY            = 94822,
    SPELL_JAWS              = 94823,
    CAST_TAILLADE           = 94840,
    CAST_HORN               = 94841,
    CAST_BATTUE             = 94842,
    CAST_HALLALI            = 94843,
    CAST_TRAQUE             = 94844,
    CAST_CUREE              = 94845,
    CAST_LEAP               = 94846,
    CAST_SNARE              = 94847,
    CAST_ENCIRCLE           = 94848,
    CAST_LANES              = 94849,
    CAST_CHARGE             = 94850,
    CAST_TRAPS              = 94851,
    CAST_CHAINS             = 94852,
    CAST_TRACKING           = 94853,
};

enum Npcs : uint32
{
    NPC_TRAQUEUR            = 930400,
};

enum Says : uint8
{
    SAY_AGGRO               = 0,
    SAY_KILL                = 1,
    SAY_HORN                = 2,
    SAY_BATTUE              = 3,
    SAY_TRAQUE              = 4,
    SAY_CUREE               = 5,
    SAY_DEATH               = 6,
};

// An invisible model: his, during la Traque (hidden, the players' combat with him would drop)
constexpr uint32 InvisibleDisplay = 11686;

enum class Phase : uint8
{
    None,
    Intro,          // 0:00, bar 0
    Hunt,           // 0:18.7, bar 12: the build-up and section A
    Traque,         // 2:04.0, bar 80: the breakdown, the hunt in the dark
    Rebuild,        // 2:41.2, bar 104
    Frenzy,         // 2:59.8, bar 116: section B
    Outro,          // 4:07.9, bar 160
    Over,
};

// What starts on a beat (each pattern's first red is drawn then; it lands FirstWarn beats later)
enum class Event : uint8
{
    Taillade,
    Volley,
    Snare,          // le Collet
    Encircle,       // l'Encerclement
    Lanes,          // le Rabattage
    Pack,           // la Charge de la meute
    Leap,           // le Bond
    Traps,          // les Pièges
    Burst,          // leur Déclenchement
    Chains,         // le Moulinet
    Tracking,       // le Pistage
    Battue,         // one pass (arg 1: and back)
    Hallali,        // arg: its quarries
    Horn,
    TraqueStart,
    Pounce,         // la Traque: on two players
    DarkCharge,     // la Traque: arg 1: two crossing
    DarkMarks,      // la Traque: under everyone
    TraqueEnd,      // Hurlement
    FrenzyStart,
    Curee,
    Outro,
};

uint32 FirstWarn(Event what)
{
    switch (what)
    {
        case Event::Leap: return 6;
        case Event::Chains: return 8;
        case Event::Tracking: return 6;
        case Event::Burst: return 3;
        case Event::Battue: return BattueWarnBeats;
        case Event::Hallali: return HallaliMarkBeats;
        case Event::Horn: return 3;
        case Event::Pounce: return 4;
        case Event::DarkCharge: return 3;
        case Event::Curee: return 16;
        case Event::TraqueStart:
        case Event::TraqueEnd:
        case Event::FrenzyStart:
        case Event::Outro:
            return 0;
        default:
            return 4;
    }
}

char const* EventName(Event what)
{
    switch (what)
    {
        case Event::Taillade: return "taillade";
        case Event::Volley: return "volley";
        case Event::Snare: return "snare";
        case Event::Encircle: return "encircle";
        case Event::Lanes: return "lanes";
        case Event::Pack: return "pack";
        case Event::Leap: return "leap";
        case Event::Traps: return "traps";
        case Event::Burst: return "burst";
        case Event::Chains: return "chains";
        case Event::Tracking: return "tracking";
        case Event::Battue: return "battue";
        case Event::Hallali: return "hallali";
        case Event::Horn: return "horn";
        case Event::TraqueStart: return "traque start";
        case Event::Pounce: return "pounce";
        case Event::DarkCharge: return "dark charge";
        case Event::DarkMarks: return "dark marks";
        case Event::TraqueEnd: return "traque end";
        case Event::FrenzyStart: return "frenzy";
        case Event::Curee: return "curee";
        case Event::Outro: return "outro";
    }
    return "?";
}

bool IsPhaseStep(Event what)
{
    return what == Event::TraqueStart || what == Event::TraqueEnd || what == Event::FrenzyStart ||
        what == Event::Outro;
}

struct Step
{
    uint32 beat;    // its first red
    Event what;
    uint32 arg = 0;
};

// The fight, on the music. Most patterns are placed by the bar whose snare (beat 4b + 2) their first wave lands on.
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps;
    auto lands = [&steps](uint32 beat, Event what, uint32 arg = 0) { steps.push_back({ beat - FirstWarn(what), what, arg }); };
    auto bar = [&lands](uint32 b, Event what, uint32 arg = 0) { lands(4 * b + 2, what, arg); };

    // Intro (bars 0-11): him alone, learning his blows
    bar(2, Event::Taillade);
    bar(4, Event::Volley);
    bar(6, Event::Taillade);
    bar(8, Event::Tracking);
    bar(10, Event::Leap);
    // Build-up (bars 12-27): his horn on its first beat, then a pattern a bar or two
    lands(48, Event::Horn);
    bar(12, Event::Encircle);
    bar(15, Event::Traps);
    bar(16, Event::Taillade);
    bar(17, Event::Lanes);
    bar(21, Event::Burst);
    bar(23, Event::Taillade);
    bar(24, Event::Pack);
    bar(27, Event::Chains);
    // Section A (bars 28-75), la Battue on its hits (beats 182, 234: the second one back again), the Hallali on the
    // peak (beat 310)
    bar(28, Event::Snare);
    bar(32, Event::Taillade);
    bar(33, Event::Leap);
    bar(35, Event::Volley);
    bar(36, Event::Pack);
    bar(38, Event::Taillade);
    bar(39, Event::Tracking);
    bar(40, Event::Traps);
    bar(41, Event::Encircle);
    lands(182, Event::Battue);
    bar(48, Event::Taillade);
    bar(49, Event::Burst);
    bar(50, Event::Chains);
    bar(52, Event::Taillade);
    bar(53, Event::Lanes);
    lands(234, Event::Battue, 1);
    bar(65, Event::Taillade);
    bar(66, Event::Snare);
    bar(70, Event::Leap);
    bar(72, Event::Taillade);
    bar(73, Event::Volley);
    lands(310, Event::Hallali, 4);
    bar(78, Event::Volley);
    // La Traque (bars 80-103): pounces on the kick (beat 4b + 1) and charges between; in its quiet second half, marks
    // under everyone and crossed charges, a bar each. He comes back howling on beat 416.
    steps.push_back({ 320, Event::TraqueStart });
    for (uint32 b = 81; b < 92; ++b)
    {
        lands(4 * b + 1, Event::Pounce);
        if (b % 2 == 0)
            lands(4 * b + 3, Event::DarkCharge);
    }
    for (uint32 b = 92; b < 104; ++b)
        lands(4 * b + 1, b % 2 ? Event::DarkCharge : Event::DarkMarks, 1);
    steps.push_back({ 416, Event::TraqueEnd });
    // The rebuild (bars 104-115)
    bar(105, Event::Taillade);
    bar(106, Event::Encircle);
    bar(109, Event::Traps);
    bar(110, Event::Pack);
    bar(112, Event::Taillade);
    bar(113, Event::Tracking);
    lands(468, Event::Battue);
    steps.push_back({ 464, Event::FrenzyStart });
    // Section B (bars 116-155): the hits combined (517: horn and pack; 533: the Hallali, two quarries; 562: la Battue;
    // 575: his leap)
    bar(120, Event::Taillade);
    bar(121, Event::Snare);
    bar(125, Event::Leap);
    bar(127, Event::Burst);
    lands(517, Event::Horn);
    lands(517, Event::Pack);
    lands(533, Event::Hallali, 2);
    bar(134, Event::Taillade);
    bar(135, Event::Lanes);
    lands(562, Event::Battue);
    lands(575, Event::Leap);
    bar(145, Event::Taillade);
    bar(146, Event::Chains);
    bar(147, Event::Encircle);
    bar(150, Event::Traps);
    bar(151, Event::Tracking);
    bar(152, Event::Taillade);
    lands(630, Event::Curee);
    bar(154, Event::Burst);
    bar(156, Event::Volley);
    // The outro (bars 160-170): the kill window, still dancing
    steps.push_back({ 640, Event::Outro });
    bar(160, Event::Encircle);
    bar(163, Event::Volley);
    bar(164, Event::Snare);
    bar(168, Event::Chains);
    std::stable_sort(steps.begin(), steps.end(), [](Step const& a, Step const& b) { return a.beat < b.beat; });
    return steps;
}

// What a mistake tells the player it hit (and the log)
struct Rule
{
    uint32 spell;
    float percent;
    char const* name;
    char const* french;
    char const* english;
};

Rule const RuleCone = { SPELL_TAILLADE, ConePct, "taillade",
    "Taillade : tenez-vous sur ses flancs, jamais devant lui si vous ne le tenez pas.",
    "Taillade: stand at his flanks, never in front of him unless you tank him." };
Rule const RuleRevers = { SPELL_REVERS, ConePct, "revers",
    "Revers : après sa taille, il frappe derrière lui. Tenez-vous sur ses flancs.",
    "Backswing: after his slash he strikes behind him. Stand at his flanks." };
Rule const RuleVolley = { SPELL_VOLLEY, VolleyPct, "volley",
    "Volée de flèches : sortez des cercles avant qu'elle tombe.",
    "Arrow Volley: leave the circles before it lands." };
Rule const RuleSnare = { SPELL_SNARE, PatternPct, "snare",
    "Collet : dedans, dehors, dedans, dehors - une mesure chacun.",
    "Snare: in, out, in, out - a bar each." };
Rule const RuleEncircle = { SPELL_ENCIRCLE, PatternPct, "encircle",
    "Encerclement : le quart épargné tourne d'un huitième à chaque mesure, dans le sens des aiguilles d'une montre. "
    "Restez dans sa moitié avant.",
    "Encirclement: the spared quarter turns an eighth every bar, clockwise. Stay in its leading half." };
Rule const RuleLanes = { SPELL_LANES, PatternPct, "lanes",
    "Rabattage : les couloirs impairs, puis les pairs, puis en travers. Changez de couloir à chaque vague.",
    "Beating: the odd lanes, then the even ones, then across. Change lanes every wave." };
Rule const RuleCharge = { SPELL_CHARGE, ChargePct, "pack",
    "Charge de la meute : écartez-vous de la ligne qui passe par vous.",
    "Pack Charge: step off the line running through you." };
Rule const RuleLeap = { SPELL_LEAP, LeapPct, "leap",
    "Bond du Traqueur : quittez le cercle avant qu'il retombe.",
    "Hunter's Leap: leave the circle before he lands." };
Rule const RuleShockwave = { SPELL_SHOCKWAVE, LeapPct, "shockwave",
    "Onde de choc : rejoignez le Traqueur là où il est retombé.",
    "Shockwave: join the Hunter where he landed." };
Rule const RuleTrap = { SPELL_TRAP, TrapPct, "trap",
    "Piège à mâchoires : un piège armé reste au sol. Contournez-les.",
    "Jaw Trap: an armed trap stays on the floor. Walk around them." };
Rule const RuleBurst = { SPELL_BURST, BurstPct, "burst",
    "Déclenchement : les pièges sautent l'un après l'autre. Éloignez-vous d'eux.",
    "Trigger: the traps burst one after the other. Keep away from them." };
Rule const RuleChains = { SPELL_CHAINS, PatternPct, "chains",
    "Moulinet : ses chaînes frappent là où elles s'arrêtent. Regardez où elles se figent.",
    "Chain Whirl: his chains strike where they stop. Watch where they freeze." };
Rule const RuleTracked = { SPELL_TRACKED, PatternPct, "tracking",
    "Pistage : votre marque vous suit, puis se fige. Quittez-la dès qu'elle s'arrête.",
    "Tracking: your mark follows you, then freezes. Leave it as soon as it stops." };
Rule const RuleBattue = { SPELL_BATTUE, BattuePct, "battue",
    "Battue : tenez-vous dans la trouée de votre rangée quand elle passe.",
    "Beat: stand in your row's gap as it sweeps past." };
Rule const RulePounce = { SPELL_POUNCE, DarkPct, "pounce",
    "Bond dans l'ombre : sortez du cercle sous vous.",
    "Pounce from the Dark: leave the circle under you." };
Rule const RuleDarkCharge = { SPELL_DARK_CHARGE, ChargePct, "dark charge",
    "Griffes dans le noir : écartez-vous de la ligne qui passe par vous.",
    "Claws in the Dark: step off the line running through you." };
Rule const RuleHallaliLeap = { SPELL_HALLALI, HallaliMistakePct, "hallali leap",
    "Hallali : éloignez-vous des proies marquées.",
    "Mort: keep away from the marked quarries." };
Rule const RuleHallaliAstray = { SPELL_PACK_SHARE, HallaliMistakePct, "hallali astray",
    "Hallali : sans proie, rejoignez le groupe sous le Traqueur pour partager le coup.",
    "Mort: without a mark, join the group under the Hunter to share the blow." };

bool IsTank(Player* player)
{
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() && member.roles)
                return member.roles & lfg::PLAYER_ROLE_TANK;
    return player->HasTankSpec();
}

using Areas = std::vector<GroundIndicators::Area>;

struct boss_escape_hunter : public Defi::BossAI
{
    boss_escape_hunter(Creature* creature) : Defi::BossAI(creature, Room, "module.traqueur") { }

    void Reset() override
    {
        // A fight still on is a wipe: the board resets him once the players are down
        if (_phase != Phase::None && _phase != Phase::Over)
        {
            WatchDeaths();
            LOG_INFO(_logName, "Traqueur wipe instance={} at={:.1f}s health={:.1f}% deaths={}", me->GetInstanceId(),
                     Elapsed() / 1000.0f, me->GetHealthPct(), DeathsText());
            EndMusic();
        }
        ResetFight();
        if (DefiConfirmed())
        {
            me->SetVisible(true);
            SetModelHealth();
        }
        // The board holds him until the group pulls (RaidFinder HoldChallengeBoss)
        me->SetReactState(REACT_PASSIVE);
    }

    // Combat's own evade (no one left to hit: la Traque holds him away from them) never ends the fight while a player
    // stands in the hall; the board's reset (a wipe: EVADE_REASON_OTHER) always does - refused, his music played on
    // after the wipe
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (why != EVADE_REASON_OTHER && _phase != Phase::None && _phase != Phase::Over && !ArenaPlayers().empty())
            return;
        Defi::BossAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        if (_phase != Phase::None)
            return;
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, GetRoom().reach);
        _timeline = BuildTimeline();
        _next = 0;
        StartClock();
        _phase = Phase::Intro;
        Talk(SAY_AGGRO);
        StartMusic("Music.EscapeHunter");
        LOG_INFO(_logName, "Traqueur pulled instance={} health={} tier factor={}", me->GetInstanceId(),
                 me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && _phase != Phase::Over && KillYellReady())
            Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        WatchDeaths();
        LOG_INFO(_logName, "Traqueur killed instance={} at={:.1f}s deaths={}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, DeathsText());
        EndMusic();
        ResetFight();
        _phase = Phase::Over;
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
        bool const fighting = _phase != Phase::None && _phase != Phase::Over;
        if (fighting)
            WatchDeaths();
        if (!UpdateVictim())
        {
            if (!fighting)
                UpdateOutOfCombat(diff);
            return;
        }

        scheduler.Update(diff);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed && _phase != Phase::Over)
        {
            Step const step = _timeline[_next++];
            Execute(step.what, step.arg);
        }
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            HuntOver();
        }
        if (elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 200;
            CheckTraps();
            UpdateTanks();
        }
        if (_phase != Phase::Traque && !CastingBar())
            DoMeleeAttackIfReady();
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        uint32 const elapsed = Elapsed();
        uint32 onHim = 0;
        for (Player* player : ArenaPlayers())
            if (player->GetVictim() == me)
                ++onHim;
        return Acore::StringFormat("Traqueur: phase {} at {:.1f}s (beat {:.1f}), health {}/{} ({:.1f}%), next step "
            "{}/{}, on him {}, his victim {}, traps {}", uint32(_phase), elapsed / 1000.0f, Grid.BeatOf(elapsed),
            me->GetHealth(), me->GetMaxHealth(), me->GetHealthPct(), _next, _timeline.size(), onHim,
            me->GetVictim() ? me->GetVictim()->GetName() : "none", _traps.size());
    }

    // Jumps the fight forward (the music is not: it goes on from where it was); the phase changes passed over run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        EndCastBar();
        AdvanceClock(seconds * IN_MILLISECONDS);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed)
        {
            Step const step = _timeline[_next++];
            if (IsPhaseStep(step.what))
                Execute(step.what, step.arg);
        }
        return true;
    }

    // One pattern now, the fight going on
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat())
            return false;
        static std::map<std::string, std::pair<Event, uint32>> const steps = {
            { "taillade", { Event::Taillade, 0 } }, { "volley", { Event::Volley, 0 } },
            { "snare", { Event::Snare, 0 } }, { "encircle", { Event::Encircle, 0 } },
            { "lanes", { Event::Lanes, 0 } }, { "pack", { Event::Pack, 0 } }, { "leap", { Event::Leap, 0 } },
            { "traps", { Event::Traps, 0 } }, { "burst", { Event::Burst, 0 } }, { "chains", { Event::Chains, 0 } },
            { "tracking", { Event::Tracking, 0 } }, { "battue", { Event::Battue, 1 } },
            { "hallali", { Event::Hallali, 4 } }, { "horn", { Event::Horn, 0 } }, { "curee", { Event::Curee, 0 } },
            { "pounce", { Event::Pounce, 0 } }, { "darkcharge", { Event::DarkCharge, 1 } },
            { "darkmarks", { Event::DarkMarks, 0 } },
        };
        auto const found = steps.find(what);
        if (found == steps.end())
            return false;
        Execute(found->second.first, found->second.second);
        return true;
    }

protected:
    Defi::Sizing const& GetSizing() const override { return Sizing; }
    float HealthScale() const override { return float(::HealthScale); }

    float TakenFactor(Unit const* victim) const override
    {
        float factor = 1.0f;
        if (uint8 const stacks = StacksOf(victim, SPELL_LACERATION))
            factor *= 1.0f + LacerationTakenPct / 100.0f * float(stacks);
        if (victim->HasAura(SPELL_DEBUSQUE))
            factor *= 1.0f + MistakeTakenPct / 100.0f;
        return factor;
    }

private:
    // --- The timeline ----------------------------------------------------------------------------------------------
    void Execute(Event what, uint32 arg)
    {
        uint32 const elapsed = Elapsed();
        uint32 const start = CurrentBeat(Grid);
        LOG_INFO(_logName, "Traqueur {} instance={} at={:.2f}s beat={:.1f} health={:.1f}%", EventName(what),
                 me->GetInstanceId(), elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealthPct());
        if (_phase == Phase::Intro && what != Event::Taillade && what != Event::Volley && start >= 48)
            _phase = Phase::Hunt;
        switch (what)
        {
            case Event::Taillade: Taillade(start); break;
            case Event::Volley: Volley(start); break;
            case Event::Snare: Snare(start); break;
            case Event::Encircle: Encircle(start); break;
            case Event::Lanes: Lanes(start); break;
            case Event::Pack: Pack(start); break;
            case Event::Leap: Leap(start); break;
            case Event::Traps: Traps(start); break;
            case Event::Burst: Burst(start); break;
            case Event::Chains: Chains(start); break;
            case Event::Tracking: Tracking(start); break;
            case Event::Battue: Battue(start, arg != 0); break;
            case Event::Hallali: Hallali(start, arg); break;
            case Event::Horn: Horn(start); break;
            case Event::TraqueStart: StartTraque(); break;
            case Event::Pounce: Pounce(start); break;
            case Event::DarkCharge: DarkCharge(start, arg != 0); break;
            case Event::DarkMarks: DarkMarks(start); break;
            case Event::TraqueEnd: EndTraque(); break;
            case Event::FrenzyStart: _phase = Phase::Frenzy; break;
            case Event::Curee: Curee(start); break;
            case Event::Outro: _phase = Phase::Outro; break;
        }
    }

    // --- A wave: red drawn on a beat, struck on a later one ----------------------------------------------------------
    // draw(durationMs) draws the wave's areas (the time left to its beat) and returns them as drawn; whoever stands in
    // any of them on the beat it lands is struck by rule (a mistake)
    void Wave(uint32 shown, uint32 lands, std::function<Areas(uint32)> draw, Rule const& rule)
    {
        AtBeat(Grid, shown, [this, lands, draw, &rule]()
        {
            uint32 const at = Grid.At(lands);
            uint32 const now = Elapsed();
            Areas const areas = draw(at > now ? at - now : 0);
            AtBeat(Grid, lands, [this, areas, &rule]() { Strike(areas, rule); });
        });
    }

    void Strike(Areas const& areas, Rule const& rule)
    {
        for (Player* player : ArenaPlayers())
            for (GroundIndicators::Area const& area : areas)
                if (area.Contains(player->GetPosition()))
                {
                    Mistake(player, rule);
                    break;
                }
    }

    Position Middle() const { return Ground(GetRoom().center); }

    // Players at random, the tanks last (a pattern on players: the tanks hold him)
    std::vector<Player*> Targets(uint32 count, bool tanks = false)
    {
        std::vector<Player*> others;
        std::vector<Player*> held;
        for (Player* player : ArenaPlayers())
            (IsTank(player) ? held : others).push_back(player);
        Acore::Containers::RandomShuffle(others);
        if (tanks)
            others.insert(others.end(), held.begin(), held.end());
        if (others.size() > count)
            others.resize(count);
        return others;
    }

    // --- Taillade, its Revers: his tank's cone, then behind him -------------------------------------------------------
    void Taillade(uint32 start)
    {
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (!tank || _phase == Phase::Traque)
            return;
        CastBar(CAST_TAILLADE);
        me->SetFacingToObject(tank);
        float const facing = me->GetAngle(tank);
        Position apex = Ground(me->GetPosition());
        apex.SetOrientation(facing);
        ObjectGuid const tankGuid = tank->GetGUID();
        uint32 const lands = start + 4;
        uint32 const ms = Grid.At(lands) - Elapsed();
        GroundIndicators::Area const cone = GroundIndicators::ShowAimedCone(me, apex, facing, TailladeRadius,
            TailladeArc, ms, tank, GroundIndicators::Theme::None, MistakeDamage);
        // The Revers drawn at once behind him: the flanks are the only place clear of both
        GroundIndicators::Area const back = GroundIndicators::ShowCone(me, apex, facing + float(M_PI), TailladeRadius,
            TailladeArc, Grid.At(lands + 2) - Elapsed(), GroundIndicators::Theme::None, MistakeDamage);
        AtBeat(Grid, lands, [this, cone, tankGuid]()
        {
            EndCastBar();
            for (Player* player : ArenaPlayers())
            {
                if (player->GetGUID() == tankGuid)
                {
                    Hit(player, SPELL_TAILLADE, TailladeTankPct);
                    AddTimedAura(player, SPELL_LACERATION, LacerationMs,
                        uint8(std::min(10, StacksOf(player, SPELL_LACERATION) + 1)));
                }
                else if (cone.Contains(player->GetPosition()))
                    Mistake(player, RuleCone);
            }
        });
        AtBeat(Grid, lands + 2, [this, back]() { Strike({ back }, RuleRevers); });
    }

    // The tanks: him held at the hall's middle, the other tank beside him out of his cones; the swap - the tank with
    // SwapStacks of Lacération leaves him to the other (bots: GroundIndicators::SetBossHolder)
    void UpdateTanks()
    {
        if (_phase == Phase::Traque)
            return;
        GroundIndicators::SetTankSpot(me, Middle(), 1000, 8.0f);
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (!tank)
            return;
        GroundIndicators::SetOffTankSpot(me, AtAngle(me->GetPosition(), me->GetAngle(tank) + float(M_PI) / 2.0f,
            4.0f), 1000);
        if (StacksOf(tank, SPELL_LACERATION) < SwapStacks)
            return;
        for (Player* player : ArenaPlayers())
            if (player != tank && IsTank(player) && StacksOf(player, SPELL_LACERATION) < SwapStacks)
            {
                GroundIndicators::SetBossHolder(me, player, 8000);
                return;
            }
    }

    // --- Volée de flèches: circles under players and round the hall --------------------------------------------------
    void Volley(uint32 start)
    {
        Wave(start, start + 4, [this](uint32 ms)
        {
            Areas areas;
            for (Player* player : Targets(3))
                areas.push_back(GroundIndicators::ShowCircle(me, Ground(player->GetPosition()), VolleyRadius, ms,
                    GroundIndicators::Theme::None, MistakeDamage));
            for (uint32 index = 0; index < VolleyAround; ++index)
                areas.push_back(GroundIndicators::ShowCircle(me, AtAngle(GetRoom().center,
                    frand(0.0f, 2.0f * float(M_PI)), frand(6.0f, HallRadius - 8.0f)), VolleyRadius, ms,
                    GroundIndicators::Theme::None, MistakeDamage));
            return areas;
        }, RuleVolley);
    }

    // --- Le Collet: a ring, then a circle, a ring, a circle - out, in, out, in -----------------------------------------
    // Around him where he stands; each wave drawn as the last one lands, a bar each
    void Snare(uint32 start)
    {
        CastBar(CAST_SNARE);
        AtBeat(Grid, start + 4, [this]() { EndCastBar(); });
        Position const center = Ground(me->GetPosition());
        for (uint32 wave = 0; wave < 4; ++wave)
        {
            uint32 const shown = wave ? start + 4 * wave : start;
            Wave(shown, start + 4 + 4 * wave, [this, center, wave](uint32 ms)
            {
                if (wave % 2 == 0)
                    return Areas{ GroundIndicators::ShowRing(me, center, HallRadius, SnareRadius, ms,
                        GroundIndicators::Theme::None, MistakeDamage) };
                return Areas{ GroundIndicators::ShowCircle(me, center, SnareRadius, ms, GroundIndicators::Theme::None,
                    MistakeDamage) };
            }, RuleSnare);
        }
    }

    // --- L'Encerclement: three quarters of the hall shot at, the quarter spared turning clockwise an eighth a bar --------
    // Each spared quarter shares its clockwise half with the next: who stands there stays safe
    void Encircle(uint32 start)
    {
        CastBar(CAST_ENCIRCLE);
        AtBeat(Grid, start + 4, [this]() { EndCastBar(); });
        Position const center = Middle();
        float const spared = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 wave = 0; wave < EncircleWaves; ++wave)
        {
            // Clockwise: the orientation falls an eighth of a turn a wave; the spared quarter is two eighths from it
            float const safe = spared - float(wave) * float(M_PI) / 4.0f;
            Wave(wave ? start + 4 * wave : start, start + 4 + 4 * wave, [this, center, safe](uint32 ms)
            {
                Areas areas;
                for (uint32 eighth = 2; eighth < 8; ++eighth)
                {
                    Position apex = center;
                    float const facing = safe + (float(eighth) + 0.5f) * float(M_PI) / 4.0f;
                    apex.SetOrientation(facing);
                    areas.push_back(GroundIndicators::ShowCone(me, apex, facing, HallRadius, 45.0f, ms,
                        GroundIndicators::Theme::None, MistakeDamage));
                }
                return areas;
            }, RuleEncircle);
        }
    }

    // --- Le Rabattage: the hall's five lanes swept - the odd ones, the even ones, then across - a bar a wave --------------
    void Lanes(uint32 start)
    {
        CastBar(CAST_LANES);
        AtBeat(Grid, start + 4, [this]() { EndCastBar(); });
        bool const acrossFirst = urand(0, 1);
        for (uint32 wave = 0; wave < 4; ++wave)
        {
            bool const across = (wave >= 2) != acrossFirst;
            bool const odd = wave % 2 == 0;
            Wave(wave ? start + 4 * wave : start, start + 4 + 4 * wave, [this, across, odd](uint32 ms)
            {
                Areas areas;
                Position const center = Middle();
                for (int32 lane = -2; lane <= 2; ++lane)
                {
                    if ((lane % 2 == 0) != odd)
                        continue;
                    // A lane along the hall (north-south: orientation 0) or across it, drawn from the middle out both
                    // ways (a rectangle reaches HallRadius at most)
                    float const along = across ? float(M_PI) / 2.0f : 0.0f;
                    float const offset = float(lane) * LaneWidth;
                    Position const middle(center.GetPositionX() + std::cos(along + float(M_PI) / 2.0f) * offset,
                        center.GetPositionY() + std::sin(along + float(M_PI) / 2.0f) * offset, center.GetPositionZ());
                    for (float facing : { along, along + float(M_PI) })
                        areas.push_back(GroundIndicators::ShowRectangle(me, Ground(middle), facing, HallRadius,
                            LaneWidth, ms, GroundIndicators::Theme::None, MistakeDamage));
                }
                return areas;
            }, RuleLanes);
        }
    }

    // A line ChargeLength long through a point, at an angle: drawn from one end
    Areas Line(Position const& through, float angle, float width, uint32 ms)
    {
        Position const from(through.GetPositionX() - std::cos(angle) * ChargeLength / 2.0f,
            through.GetPositionY() - std::sin(angle) * ChargeLength / 2.0f, through.GetPositionZ());
        return { GroundIndicators::ShowRectangle(me, Ground(from), angle, ChargeLength, width, ms,
            GroundIndicators::Theme::None, MistakeDamage) };
    }

    // --- La Charge de la meute: six lines through players, one a beat ------------------------------------------------
    void Pack(uint32 start)
    {
        CastBar(CAST_CHARGE);
        AtBeat(Grid, start + 4, [this]() { EndCastBar(); });
        for (uint32 charge = 0; charge < 6; ++charge)
            Wave(start + charge, start + 4 + charge, [this](uint32 ms)
            {
                std::vector<Player*> target = Targets(1, true);
                if (target.empty())
                    return Areas{};
                return Line(Ground(target.front()->GetPosition()), frand(0.0f, 2.0f * float(M_PI)), ChargeWidth, ms);
            }, RuleCharge);
    }

    // --- Le Bond: he leaps on the farthest player, then his shockwave everywhere but where he landed -------------------
    void Leap(uint32 start)
    {
        if (_phase == Phase::Traque)
            return;
        Player* prey = nullptr;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player) && (!prey || me->GetExactDist2d(player) > me->GetExactDist2d(prey)))
                prey = player;
        if (!prey)
            return;
        CastBar(CAST_LEAP);
        AnnounceAll(Acore::StringFormat("Le Traqueur va bondir sur {} !", prey->GetName()),
            Acore::StringFormat("The Hunter is about to leap on {}!", prey->GetName()));
        Position const spot = Ground(prey->GetPosition());
        uint32 const lands = start + 6;
        GroundIndicators::Area const circle = GroundIndicators::ShowCircle(me, spot, LeapRadius,
            Grid.At(lands) - Elapsed(), GroundIndicators::Theme::None, MistakeDamage);
        // The jump itself, two beats long, landing on the beat
        AtBeat(Grid, lands - 2, [this, spot]()
        {
            EndCastBar();
            float const jumpSeconds = Grid.beatMs * 2.0f / 1000.0f;
            me->GetMotionMaster()->MoveJump(spot, std::max(10.0f, me->GetExactDist2d(&spot) / jumpSeconds), 8.0f);
        });
        AtBeat(Grid, lands, [this, circle]() { Strike({ circle }, RuleLeap); });
        // Its shockwave two bars later: time to run back to him from wherever the leap was dodged
        Wave(lands, lands + ShockwaveBeats, [this, spot](uint32 ms)
        {
            return Areas{ GroundIndicators::ShowRing(me, spot, HallRadius, LeapRadius, ms,
                GroundIndicators::Theme::None, MistakeDamage) };
        }, RuleShockwave);
    }

    // --- Les Pièges: under everyone but his tanks, armed on the beat, on the floor until they burst ---------------------
    void Traps(uint32 start)
    {
        CastBar(CAST_TRAPS);
        uint32 const armed = start + 4;
        for (Player* player : Targets(8))
        {
            Position const at = Ground(player->GetPosition());
            GroundIndicators::ShowCircle(me, at, TrapRadius, Grid.At(armed) - Elapsed(), GroundIndicators::Theme::None,
                MistakeDamage);
            _traps.push_back({ at, Grid.At(armed) });
        }
        AtBeat(Grid, armed, [this, armed]()
        {
            EndCastBar();
            // Armed: red until they burst (a minute at most)
            for (Trap& trap : _traps)
                if (trap.armedAt == Grid.At(armed))
                    trap.area = GroundIndicators::ShowCircle(me, trap.at, TrapRadius, 60000,
                        GroundIndicators::Theme::Fire, MistakeDamage);
        });
    }

    // An armed trap catches whoever steps in: struck, held
    void CheckTraps()
    {
        uint32 const now = Elapsed();
        for (Trap& trap : _traps)
        {
            if (now < trap.armedAt || trap.sprung)
                continue;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(&trap.at) <= TrapRadius)
                {
                    trap.sprung = true;
                    Mistake(player, RuleTrap);
                    player->SetControlled(true, UNIT_STATE_ROOT);
                    AddTimedAura(player, SPELL_JAWS, TrapRootMs);
                    ObjectGuid const guid = player->GetGUID();
                    scheduler.Schedule(Milliseconds(TrapRootMs), [this, guid](TaskContext)
                    {
                        if (Player* caught = ObjectAccessor::GetPlayer(*me, guid))
                            caught->SetControlled(false, UNIT_STATE_ROOT);
                    });
                    break;
                }
        }
        std::erase_if(_traps, [](Trap const& trap) { return trap.sprung && !trap.bursting; });
    }

    // --- Leur Déclenchement: the traps on the floor burst one a beat, in the order they were laid ---------------------
    void Burst(uint32 start)
    {
        uint32 beat = start;
        for (Trap& trap : _traps)
        {
            if (trap.bursting)
                continue;
            trap.bursting = true;
            Position const at = trap.at;
            Wave(beat, beat + 3, [this, at](uint32 ms)
            {
                return Areas{ GroundIndicators::ShowCircle(me, at, BurstRadius, ms, GroundIndicators::Theme::Fire,
                    MistakeDamage) };
            }, RuleBurst);
            // Gone with its burst
            AtBeat(Grid, beat + 3, [this, at]()
            {
                std::erase_if(_traps, [&at](Trap const& trap) { return trap.at.GetExactDist2d(&at) < 0.1f; });
            });
            ++beat;
        }
    }

    // --- Le Moulinet: two chains through him turning, stopped dead on a beat: they strike where they stopped ----------
    void Chains(uint32 start)
    {
        CastBar(CAST_CHAINS);
        Position const center = Ground(me->GetPosition());
        float const facing = frand(0.0f, 2.0f * float(M_PI));
        // A turn in ChainTurnBeats, either way; it stops a few beats in, somewhere between a quarter and three
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const radiansPerSecond = way * 2.0f * float(M_PI) / (Grid.beatMs * float(ChainTurnBeats) / 1000.0f);
        uint32 const spinBeats = urand(3, 6);
        uint32 const stops = start + spinBeats;
        uint32 const spinMs = Grid.At(stops) - Elapsed();
        // Four arms from him (a line sweeps round its start): two chains through him, square
        Areas spinning;
        for (uint32 arm = 0; arm < 4; ++arm)
            spinning.push_back(GroundIndicators::ShowSweepingRectangle(me, center,
                facing + float(arm) * float(M_PI) / 2.0f, radiansPerSecond, ChainLength / 2.0f, ChainWidth, spinMs,
                MistakeDamage));
        // Frozen where they are, struck two beats later
        Wave(stops, start + 8, [this, spinning, radiansPerSecond, spinMs](uint32 ms)
        {
            Areas areas;
            for (GroundIndicators::Area const& arm : spinning)
            {
                GroundIndicators::Area const stopped = GroundIndicators::CurrentSweep(arm, radiansPerSecond, spinMs);
                areas.push_back(GroundIndicators::ShowRectangle(me, stopped.origin, stopped.origin.GetOrientation(),
                    stopped.radius, ChainWidth, ms, GroundIndicators::Theme::None, MistakeDamage));
            }
            EndCastBar();
            return areas;
        }, RuleChains);
    }

    // --- Le Pistage: marks follow three players, freeze on a snare and strike where they froze on the next ------------
    void Tracking(uint32 start)
    {
        CastBar(CAST_TRACKING);
        uint32 const freezes = start + 4;
        std::vector<std::pair<ObjectGuid, GroundIndicators::Area>> marks;
        for (Player* player : Targets(TrackedPlayers))
            marks.push_back({ player->GetGUID(), GroundIndicators::ShowCarriedCircle(me, player, TrackRadius,
                Grid.At(freezes) - Elapsed(), MistakeDamage) });
        Wave(freezes, freezes + 4, [this, marks](uint32 ms)
        {
            EndCastBar();
            Areas areas;
            for (auto const& [guid, mark] : marks)
                if (Player* player = ObjectAccessor::GetPlayer(*me, guid))
                    areas.push_back(GroundIndicators::ShowCircle(me,
                        Ground(GroundIndicators::CurrentArea(player, mark).origin), TrackRadius, ms,
                        GroundIndicators::Theme::None, MistakeDamage));
            return areas;
        }, RuleTracked);
    }

    // --- La Battue: strips swept one a beat, a gap in each -----------------------------------------------------------
    // From the north wall south (and back, a second pass, the gaps elsewhere): each strip lands on its beat, shown
    // BattueWarnBeats ahead, its gap left open
    void Battue(uint32 start, bool back)
    {
        CastBar(CAST_BATTUE);
        Talk(SAY_BATTUE);
        AtBeat(Grid, start + 5, [this]() { EndCastBar(); });
        uint32 const first = start + BattueWarnBeats;
        float const seed = frand(0.0f, 2.0f * float(M_PI));
        Position const center = GetRoom().center;
        for (uint32 pass = 0; pass < (back ? 2u : 1u); ++pass)
            for (uint32 index = 0; index < BattueStrips; ++index)
            {
                uint32 const strip = pass ? BattueStrips - 1 - index : index;
                float const x = center.GetPositionX() + (float(BattueStrips - 1) / 2.0f - float(strip)) *
                    BattueStripDepth;
                // The gap snakes across the hall; the way back takes it elsewhere, a step aside from the first
                float gap = center.GetPositionY() + 24.0f * std::sin(seed + float(strip) * 0.9f);
                if (pass)
                    gap += (strip % 2 ? 16.0f : -16.0f);
                gap = std::clamp(gap, center.GetPositionY() - BattueHalfWidth + BattueGap,
                    center.GetPositionY() + BattueHalfWidth - BattueGap);
                uint32 const lands = first + pass * (BattueStrips + BattueWarnBeats) + index;
                Wave(lands - BattueWarnBeats, lands, [this, x, gap](uint32 ms)
                {
                    return Strip(x, gap, ms);
                }, RuleBattue);
            }
    }

    // A strip's two halves round its gap
    Areas Strip(float x, float gap, uint32 ms)
    {
        Areas strip;
        float const west = GetRoom().center.GetPositionY() + BattueHalfWidth;
        float const east = GetRoom().center.GetPositionY() - BattueHalfWidth;
        float const halfGap = BattueGap / 2.0f;
        // A rectangle from start along its orientation: +y is the hall's west (orientation pi/2)
        if (gap - halfGap > east)
            strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, east, 0.0f)), float(M_PI) / 2.0f,
                gap - halfGap - east, BattueStripDepth, ms, GroundIndicators::Theme::None, MistakeDamage));
        if (west > gap + halfGap)
            strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, gap + halfGap, 0.0f)),
                float(M_PI) / 2.0f, west - gap - halfGap, BattueStripDepth, ms, GroundIndicators::Theme::None,
                MistakeDamage));
        return strip;
    }

    // --- L'Hallali: quarries away from everyone, the others together -------------------------------------------------
    void Hallali(uint32 start, uint32 quarries)
    {
        uint32 const lands = start + HallaliMarkBeats;
        std::vector<Player*> marked = Targets(quarries);
        uint32 const durationMs = Grid.At(lands) - Elapsed();
        std::vector<ObjectGuid> guids;
        for (Player* quarry : marked)
        {
            AddTimedAura(quarry, SPELL_QUARRY, durationMs);
            GroundIndicators::ShowCarriedCircle(me, quarry, HallaliRadius, durationMs, MistakeDamage, 2.0f);
            guids.push_back(quarry->GetGUID());
            Announce(quarry, "Vous êtes une proie : éloignez-vous de tous !", "You are a quarry: get away from everyone!",
                true);
        }
        AnnounceAll("L'Hallali ! Les proies à l'écart, les autres sous le Traqueur !",
            "The Mort! Quarries away, everyone else under the Hunter!");
        // The others' meeting point: on him, his tank in it; everyone not a quarry is sent (a quarry never is)
        Position const soak = Ground(me->GetPosition());
        GroundIndicators::ShowSoak(me, soak, HallaliSoakRadius, durationMs, uint32(ArenaPlayers().size()),
            GroundIndicators::Theme::Holy, true);
        AtBeat(Grid, lands - 10, [this]() { CastBar(CAST_HALLALI); });
        AtBeat(Grid, lands, [this, guids, soak]()
        {
            EndCastBar();
            StrikeHallali(guids, soak);
        });
    }

    void StrikeHallali(std::vector<ObjectGuid> const& marked, Position const& soak)
    {
        std::vector<Player*> quarries;
        for (ObjectGuid const& guid : marked)
            if (Player* quarry = ObjectAccessor::GetPlayer(*me, guid); quarry && quarry->IsAlive())
                quarries.push_back(quarry);
        std::vector<Player*> sharing;
        for (Player* player : ArenaPlayers())
        {
            if (std::find(quarries.begin(), quarries.end(), player) != quarries.end())
                continue;
            bool leapt = false;
            for (Player* quarry : quarries)
                if (player->GetExactDist2d(quarry) <= HallaliRadius)
                    leapt = true;
            if (leapt)
                Mistake(player, RuleHallaliLeap);
            else if (player->GetExactDist2d(&soak) <= HallaliSoakRadius)
                sharing.push_back(player);
            else
                Mistake(player, RuleHallaliAstray);
        }
        for (Player* quarry : quarries)
            Hit(quarry, SPELL_HALLALI, HallaliQuarryPct);
        float const share = HallaliSoakPct / float(std::max<size_t>(1, sharing.size()));
        for (Player* player : sharing)
            Hit(player, SPELL_PACK_SHARE, share);
        LOG_INFO(_logName, "Traqueur hallali instance={} at={:.1f}s quarries={} sharing={}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, quarries.size(), sharing.size());
    }

    // --- Cor de chasse: a group hit on the beat ------------------------------------------------------------------------
    void Horn(uint32 start)
    {
        CastBar(CAST_HORN);
        Talk(SAY_HORN);
        GroundIndicators::WarnGroupDamage(me, Grid.At(start + 3) - Elapsed());
        AtBeat(Grid, start + 3, [this]()
        {
            EndCastBar();
            HitEveryone(SPELL_HORN, HornPct);
        });
    }

    // --- La Traque: gone in the dark, everything red -----------------------------------------------------------------
    void StartTraque()
    {
        _phase = Phase::Traque;
        Talk(SAY_TRAQUE);
        AnnounceAll("Le Traqueur disparaît dans le noir... Guettez le rouge sous vos pieds.",
            "The Hunter vanishes into the dark... Watch for the red under your feet.");
        CastBar(CAST_TRAQUE);
        scheduler.Schedule(2000ms, [this](TaskContext)
        {
            EndCastBar();
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            // Out of sight, not out of the fight: an invisible model (hidden, the players' combat with him dropped)
            me->SetDisplayId(InvisibleDisplay);
            Position const middle = Middle();
            me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(),
                me->GetOrientation());
        });
    }

    // Pounces: a circle under two players
    void Pounce(uint32 start)
    {
        Wave(start, start + 4, [this](uint32 ms)
        {
            Areas areas;
            for (Player* player : Targets(2, true))
                areas.push_back(GroundIndicators::ShowCircle(me, Ground(player->GetPosition()), PounceRadius, ms,
                    GroundIndicators::Theme::Shadow, MistakeDamage));
            return areas;
        }, RulePounce);
    }

    // A charge through a player out of the dark; crossed: a second one through them, square to it
    void DarkCharge(uint32 start, bool crossed)
    {
        Wave(start, start + 3, [this, crossed](uint32 ms)
        {
            std::vector<Player*> target = Targets(1, true);
            if (target.empty())
                return Areas{};
            Position const through = Ground(target.front()->GetPosition());
            float const angle = frand(0.0f, 2.0f * float(M_PI));
            Areas areas = Line(through, angle, ChargeWidth, ms);
            if (crossed)
                for (GroundIndicators::Area const& area : Line(through, angle + float(M_PI) / 2.0f, ChargeWidth, ms))
                    areas.push_back(area);
            return areas;
        }, RuleDarkCharge);
    }

    // Marks under everyone
    void DarkMarks(uint32 start)
    {
        Wave(start, start + 4, [this](uint32 ms)
        {
            Areas areas;
            for (Player* player : ArenaPlayers())
                areas.push_back(GroundIndicators::ShowCircle(me, Ground(player->GetPosition()), DarkMarkRadius, ms,
                    GroundIndicators::Theme::Shadow, MistakeDamage));
            return areas;
        }, RulePounce);
    }

    // Back from the dark on the track's hardest hit, howling
    void EndTraque()
    {
        me->RestoreDisplayId();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        _phase = Phase::Rebuild;
        AnnounceAll("Le Traqueur surgit de l'ombre !", "The Hunter bursts out of the dark!");
        HitEveryone(SPELL_HOWL, HowlPct);
    }

    // --- La Curée and the end of the hunt ---------------------------------------------------------------------------
    void Curee(uint32 start)
    {
        Talk(SAY_CUREE);
        CastBar(CAST_CUREE);
        AnnounceAll("La Curée ! Sous 15 % de sa vie, ou c'est la fin.",
            "The Quarry's Due! Below 15% of his health, or it is the end.");
        uint32 const lands = start + 16;
        GroundIndicators::WarnGroupDamage(me, Grid.At(lands) - Elapsed());
        AtBeat(Grid, lands, [this]()
        {
            EndCastBar();
            float const health = me->GetHealthPct();
            LOG_INFO(_logName, "Traqueur curee instance={} at={:.1f}s health={:.1f}% alive={}", me->GetInstanceId(),
                     Elapsed() / 1000.0f, health, ArenaPlayers().size());
            if (health > CureePct)
                for (Player* player : ArenaPlayers())
                    Doom(player, SPELL_CUREE);
            else
                HitEveryone(SPELL_CUREE, CureeHitPct);
        });
    }

    void HuntOver()
    {
        EndMusic();
        LOG_INFO(_logName, "Traqueur hard enrage instance={} at={:.1f}s health={:.1f}%", me->GetInstanceId(),
                 Elapsed() / 1000.0f, me->GetHealthPct());
        for (Player* player : ArenaPlayers())
            Doom(player, SPELL_HUNT_OVER);
    }

    // --- Mistakes ----------------------------------------------------------------------------------------------------
    // An avoidable hit, and Débusqué: what they take is heavier for a while. The player is told what hit them.
    void Mistake(Player* player, Rule const& rule)
    {
        if (!player || !player->IsAlive())
            return;
        Announce(player, rule.french, rule.english, true);
        Hit(player, rule.spell, rule.percent);
        if (player->IsAlive())
            AddTimedAura(player, SPELL_DEBUSQUE, DebusqueMs);
        Broken(rule.name, player->GetName());
    }

    void Broken(char const* what, std::string const& who)
    {
        LOG_INFO(_logName, "Traqueur rule broken instance={} at={:.1f}s {}: {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, what, who);
    }

    // Whoever fell since the last look: each death in the log once
    void WatchDeaths()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && !player->IsGameMaster())
            {
                bool const dead = !player->IsAlive();
                if (dead && !_fallen.count(player->GetGUID()))
                {
                    ++_deaths[player->GetName()];
                    LOG_INFO(_logName, "Traqueur death instance={} at={:.1f}s {}", me->GetInstanceId(),
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
        scheduler.CancelAll();
        EndCastBar();
        for (Player* player : ArenaPlayers())
            if (player->HasAura(SPELL_JAWS))
            {
                player->RemoveAurasDueToSpell(SPELL_JAWS);
                player->SetControlled(false, UNIT_STATE_ROOT);
            }
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        _traps.clear();
        _timeline.clear();
        _deaths.clear();
        _fallen.clear();
        _next = 0;
        _nextCheckMs = 0;
        _musicOver = false;
        _phase = Phase::None;
        me->SetVisible(DefiConfirmed());
        me->RestoreDisplayId();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        StopClock();
    }

    struct Trap
    {
        Position at;
        uint32 armedAt;
        GroundIndicators::Area area;
        bool sprung = false;
        bool bursting = false;
    };

    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    size_t _next = 0;
    uint32 _nextCheckMs = 0;
    bool _musicOver = false;
    std::vector<Trap> _traps;
    std::map<std::string, uint32> _deaths;
    std::set<ObjectGuid> _fallen;
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .traqueur info | skip <seconds> | cast <pattern> | pull | room: for game masters trying the fight
class EscapeHunterCommandScript final : public CommandScript
{
public:
    EscapeHunterCommandScript() : CommandScript("EscapeHunterCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable traqueurTable = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "skip", HandleSkip, SEC_GAMEMASTER, Console::No },
            { "cast", HandleCast, SEC_GAMEMASTER, Console::No },
            { "pull", HandlePull, SEC_GAMEMASTER, Console::No },
            { "room", HandleRoom, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "traqueur", traqueurTable },
        };
        return commandTable;
    }

    static boss_escape_hunter* FindHunter(Player* player)
    {
        Creature* hunter = player ? player->FindNearestCreature(NPC_TRAQUEUR, 250.0f) : nullptr;
        return hunter ? CAST_AI(boss_escape_hunter, hunter->AI()) : nullptr;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_escape_hunter* hunter = FindHunter(handler->GetPlayer());
        handler->SendSysMessage(hunter ? hunter->Describe() : std::string("The Traqueur is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_escape_hunter* hunter = FindHunter(handler->GetPlayer());
        if (!hunter || !hunter->Skip(seconds))
        {
            handler->SendErrorMessage("The Traqueur is not fighting within 250 yards.");
            return false;
        }
        handler->SendSysMessage(hunter->Describe());
        return true;
    }

    // .traqueur cast <taillade|volley|snare|encircle|lanes|pack|leap|traps|burst|chains|tracking|battue|hallali|horn|
    // curee|pounce|darkcharge|darkmarks>
    static bool HandleCast(ChatHandler* handler, std::string step)
    {
        boss_escape_hunter* hunter = FindHunter(handler->GetPlayer());
        if (!hunter || !hunter->CastNow(step))
        {
            handler->SendErrorMessage("No such pattern, or the Traqueur is not fighting within 250 yards.");
            return false;
        }
        return true;
    }

    // .traqueur room: how far the hall's floor reaches from its middle, every 10 degrees clockwise from north
    static bool HandleRoom(ChatHandler* handler)
    {
        boss_escape_hunter* hunter = FindHunter(handler->GetPlayer());
        if (!hunter)
        {
            handler->SendErrorMessage("The Traqueur is not within 250 yards.");
            return false;
        }
        std::string line = "Room reach (every 10 degrees from north, clockwise):";
        for (float reach : hunter->MeasureRoom())
            line += Acore::StringFormat(" {:.0f}", reach);
        handler->SendSysMessage(line);
        return true;
    }

    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* hunter = player ? player->FindNearestCreature(NPC_TRAQUEUR, 150.0f) : nullptr;
        if (!hunter || !hunter->IsAlive())
        {
            handler->SendErrorMessage("No living Traqueur within 150 yards.");
            return false;
        }
        hunter->SetReactState(REACT_AGGRESSIVE);
        hunter->AI()->AttackStart(player);
        handler->SendSysMessage("Traqueur pulled.");
        return true;
    }
};
}

void AddEscapeHunterScripts()
{
    RegisterCreatureAI(boss_escape_hunter);
    new EscapeHunterCommandScript();
}
