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
// Breaker's hall (plan: .agents/plans/escape-hunter, "The fight, on the music (v2)"). 8 players - 2 tanks, 2 healers,
// 4 damage dealers - tuned for item level 477 and 650 paragon, as Gardien-chef Vorhan, the first gate.
//
// **A flow on his music, two layers in turns, never on top of each other.** The track is played once from the pull and
// every step lands on a beat of it (the beat grid measured on the track, 155 BPM).
// - The WoW layer, dodging, while he is tanked: one pattern at a time in the same cycle every pull - Taillade and its
//   Revers (stand at his flanks; its stacks swap the tanks), le Moulinet (chains spin round him, freeze, strike a bar
//   later), le Pistage (marks follow three players, freeze, strike a bar later), la Battue (strips a beat apart, a gap
//   in each) - and group hits on the music's big hits.
// - The FFXIV layer, placement puzzles: for each he leaps to the middle of the hall and holds still until it resolves,
//   nothing else on the floor. A wipe the first time, easy once the safe spot is learned:
//   - le Collet: two flashes (three later) show an order - the snare's ring or its circle; two bars later they spring
//     in that order, unseen: out-in or in-out, remembered;
//   - les Lanternes des rabatteurs: lanterns stand at the hall's edge; two bars later each throws its light over the
//     hall, and he strikes whatever it shows: only beside a lantern is dark (later: two volleys, turned);
//   - l'Hallali: quarries marked; the pack leaps on them, the others share a blow under him;
//   - la Proie: two marked players leave a trail of snapping traps behind them, a beat apart: led along the wall;
//   - la Traque (the breakdown, an intermission): he is gone in the dark; a rustle at the hall's edge, and two bars
//     later he charges from there straight through the middle. He comes back howling on the track's hardest hit.
// - **The whole track**: his health stops at HoldPct until la Curée on the climax (4:04.0): held there, he dies on its
//   beat; above it, the group dies.
// Nothing is written on the screen: the cast bar, the marks and the music tell it. Markers (over players, at the
// hall's edge, on the floor) are shapes.json looks of their own (localTools/escapeHunter/indicatorArt.py), painted
// later; placeholders until then. A mistake marks (Débusqué: more damage taken for a while).
//
// What every Défi boss does alike (a challenge's instance only, the hall cleared, its health, its music, its hits) is
// Defi::BossAI's (DefiBoss.h).

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As Vorhan's profile (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can be hit
// from the pull to the Curée (4:04.0) but during la Traque (2:04.0-2:41.2, he is away), and only part of that time on
// him (OnHimShare: the patterns keep the group on its feet). Held at HoldPct, a group past it waits for the Curée: sized
// for an 8-bot group to reach it near 3:52 (one reached it at 3:05 at 0.54, 2026-10-10).
constexpr float OnHimShare = 0.66f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, (244.0f - 37.2f) * OnHimShare, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("traqueur.health_scale", 1.0f);
constexpr float HoldPct = 1.0f;

// Keli'dan's hall: open floor 55 yd and more round its middle (measured on the navigation mesh, 2026-10-10), an exit
// to the south. The patterns keep within HallRadius of its middle.
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 120.0f, 60.0f };
constexpr float HallRadius = 40.0f;

// --- The music ------------------------------------------------------------------------------------------------------
// prison_first_boss_music_2 (Music.EscapeHunter): 155 BPM, a line fitted to its 684 beats (95% within 42 ms). A bar is
// 4 beats, its snare on the third (beat 4b + 2). Sections start on bars: the build-up on bar 12, section A on 28, the
// peak on 76 (beat 310), the breakdown on 80, the rebuild on 104 (beat 416: the track's hardest hit), section B on 116,
// the climax on 156 (beat 630). Other hits: beats 55, 182, 234, 468, 517, 533, 562, 575. The track ends at
// MusicEndMs.
constexpr Defi::BeatGrid Grid = { 168.2f, 387.09f };
constexpr uint32 MusicEndMs = 265460;

// --- His blows ------------------------------------------------------------------------------------------------------
// Shares of the reference health (a damage dealer of the profile)
constexpr float MeleeFloorPct = 10.0f;      // his swing on a player: at least this
// Taillade: his tank takes TailladeTankPct and a stack of Lacération (LacerationTakenPct more taken a stack, for
// LacerationMs); at SwapStacks the other tank takes him. Its Revers lands behind him two beats later.
constexpr float TailladeTankPct = 50.0f;
constexpr float TailladeRadius = 12.0f;
constexpr float TailladeArc = 120.0f;
constexpr float LacerationTakenPct = 15.0f;
constexpr uint32 LacerationMs = 25000;
constexpr uint8 SwapStacks = 3;
constexpr float HornPct = 18.0f;
constexpr float HowlPct = 30.0f;            // Hurlement: his return from the dark
// What a mistake takes
constexpr float DodgePct = 65.0f;           // a WoW pattern stood in
constexpr float PuzzlePct = 90.0f;          // a puzzle's spot missed
constexpr float BattuePct = 70.0f;
// The patterns' sizes and times (beats)
constexpr float ChainLength = 40.0f;        // le Moulinet: four arms from him, a full turn in ChainTurnBeats
constexpr float ChainWidth = 6.0f;
constexpr uint32 ChainTurnBeats = 8;
constexpr float TrackRadius = 6.0f;         // le Pistage
constexpr uint32 TrackedPlayers = 3;
constexpr float SnareRadius = 16.0f;        // le Collet: its circle; its ring from there to the hall's edge
constexpr float LanternDistance = 34.0f;    // les Lanternes: where they stand, their light's reach and spread
constexpr float LanternReach = 45.0f;
constexpr float LanternArc = 120.0f;
constexpr float LanternWideArc = 150.0f;
constexpr float TrapRadius = 5.0f;          // la Proie: the traps its trail leaves
constexpr uint32 TrailBeats = 8;
constexpr float ChargeWidth = 10.0f;        // la Traque: his charges through the middle
constexpr float RustleDistance = 36.0f;
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
constexpr float HallaliSoakRadius = 7.0f;
constexpr float HallaliSoakPct = 160.0f;
constexpr uint32 HallaliMarkBeats = 12;
// A mistake: Débusqué for DebusqueMs, MistakeTakenPct more taken
constexpr uint32 DebusqueMs = 10000;
constexpr float MistakeTakenPct = 50.0f;
constexpr float CureeHitPct = 40.0f;
// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

enum Spells : uint32
{
    SPELL_TAILLADE          = 94800,
    SPELL_HORN              = 94801,
    SPELL_LANTERNS          = 94802,
    SPELL_PREY_TRAP         = 94803,
    SPELL_BATTUE            = 94805,
    SPELL_HALLALI           = 94806,
    SPELL_PACK_SHARE        = 94807,
    SPELL_CHARGE            = 94808,
    SPELL_CUREE             = 94809,
    SPELL_REVERS            = 94811,
    SPELL_SNARE             = 94812,
    SPELL_HOWL              = 94818,
    SPELL_CHAINS            = 94825,
    SPELL_TRACKED           = 94826,
    SPELL_LACERATION        = 94820,
    SPELL_DEBUSQUE          = 94821,
    SPELL_QUARRY            = 94822,
    SPELL_PREY              = 94824,
    CAST_TAILLADE           = 94840,
    CAST_HORN               = 94841,
    CAST_BATTUE             = 94842,
    CAST_HALLALI            = 94843,
    CAST_TRAQUE             = 94844,
    CAST_CUREE              = 94845,
    CAST_SNARE              = 94847,
    CAST_LANTERNS           = 94848,
    CAST_PREY               = 94849,
    CAST_CHAINS             = 94852,
    CAST_TRACKING           = 94853,
};

// His looks (localTools/groundIndicators/shapes.json, pictures in localTools/escapeHunter/indicatorArt.py)
enum Looks : uint32
{
    LOOK_QUARRY_MARK        = 94860,    // over a quarry of the Hallali
    LOOK_PREY_MARK          = 94861,    // over a player leaving la Proie's trail
    LOOK_TRACK_MARK         = 94862,    // over a tracked player (le Pistage)
    LOOK_LANTERN            = 94863,    // a beater's lantern at the hall's edge
    LOOK_SNARE_RING         = 94864,    // le Collet's ring, flashed
    LOOK_SNARE_CIRCLE       = 94865,    // le Collet's circle, flashed
    LOOK_RUSTLE             = 94866,    // la Traque: where he lurks at the edge
    LOOK_PACK_SIGIL         = 94867,    // l'Hallali: the group's place under him
    LOOK_HUNT_SIGIL         = 94868,    // under him in the middle while a puzzle runs
};

enum Npcs : uint32
{
    NPC_TRAQUEUR            = 930400,
};

enum Says : uint8
{
    SAY_AGGRO               = 0,
    SAY_KILL                = 1,
    SAY_DEATH               = 6,
};

// An invisible model: his, during la Traque (hidden, the players' combat with him would drop)
constexpr uint32 InvisibleDisplay = 11686;

enum class Phase : uint8
{
    None,
    Hunt,           // 0:00: the intro, the build-up, section A
    Traque,         // 2:04.0, bar 80: the breakdown, the hunt in the dark
    Pursuit,        // 2:41.2, bar 104: the rebuild and section B
    Over,
};

enum class Event : uint8
{
    Taillade,
    Chains,         // le Moulinet
    Tracking,       // le Pistage
    Battue,         // arg 1: and back
    Snare,          // le Collet: arg its springs
    Lanterns,       // arg its volleys
    Hallali,        // arg its quarries; arg2 1: they trail la Proie
    Prey,           // la Proie
    Horn,
    TraqueStart,
    Rustle,         // la Traque: arg its charges at once (1, 2 crossing)
    TraqueEnd,
    Curee,
};

char const* EventName(Event what)
{
    switch (what)
    {
        case Event::Taillade: return "taillade";
        case Event::Chains: return "chains";
        case Event::Tracking: return "tracking";
        case Event::Battue: return "battue";
        case Event::Snare: return "snare";
        case Event::Lanterns: return "lanterns";
        case Event::Hallali: return "hallali";
        case Event::Prey: return "prey";
        case Event::Horn: return "horn";
        case Event::TraqueStart: return "traque start";
        case Event::Rustle: return "rustle";
        case Event::TraqueEnd: return "traque end";
        case Event::Curee: return "curee";
    }
    return "?";
}

bool IsPhaseStep(Event what)
{
    return what == Event::TraqueStart || what == Event::TraqueEnd;
}

struct Step
{
    uint32 beat;    // where it starts: a WoW pattern's first red, a puzzle's leap landing in the middle
    Event what;
    uint32 arg = 0;
    uint32 arg2 = 0;
};

// The fight, on the music (the plan's timeline, v2). Each pattern ends before the next starts.
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps = {
        // Intro (bars 0-11): his blows, one tracking
        { 6, Event::Taillade }, { 22, Event::Taillade }, { 28, Event::Tracking }, { 38, Event::Taillade },
        // Build-up (bars 12-27): his horn on its first beat; le Collet, his leap landing on the 0:21.4 hit
        { 45, Event::Horn }, { 55, Event::Snare, 2 }, { 86, Event::Taillade }, { 96, Event::Chains },
        // Section A (bars 28-75)
        { 110, Event::Taillade }, { 118, Event::Tracking }, { 126, Event::Taillade },
        { 132, Event::Lanterns, 1 },
        { 146, Event::Taillade }, { 156, Event::Chains },
        { 174, Event::Battue },                         // its strips from the 1:10.6 hit (beat 182)
        { 190, Event::Taillade }, { 198, Event::Tracking }, { 210, Event::Taillade },
        { 226, Event::Battue, 1 },                      // from the 1:30.7 hit (beat 234), and back
        { 264, Event::Prey },
        { 282, Event::Taillade },
        { 296, Event::Hallali, 4 },                     // landing on the peak (beat 310)
        // La Traque (bars 80-103): a rustle a bar, then two crossing every other bar; his return on beat 416
        { 320, Event::TraqueStart },
        // The rebuild (bars 104-115)
        { 416, Event::TraqueEnd },
        { 422, Event::Taillade }, { 430, Event::Chains }, { 446, Event::Taillade },
        // Section B (bars 116-155)
        { 460, Event::Battue },                         // from the 3:01.3 hit (beat 468)
        { 478, Event::Taillade },
        { 486, Event::Snare, 3 },
        { 514, Event::Horn },                           // on the 3:20.3 hit (beat 517)
        { 519, Event::Hallali, 2, 1 },                  // landing on the 3:26.5 hit (beat 533), the quarries trailing
        { 538, Event::Taillade }, { 542, Event::Chains },
        { 554, Event::Battue },                         // from the 3:37.7 hit (beat 562)
        { 575, Event::Lanterns, 2 },                    // his leap on the 3:42.7 hit
        { 594, Event::Taillade }, { 600, Event::Tracking },
        { 612, Event::Curee },                          // landing on the climax (beat 630)
    };
    for (uint32 bar = 81; bar <= 89; ++bar)
        steps.push_back({ 4 * bar, Event::Rustle, 1 });
    for (uint32 bar = 92; bar <= 100; bar += 2)
        steps.push_back({ 4 * bar, Event::Rustle, 2 });
    std::stable_sort(steps.begin(), steps.end(), [](Step const& a, Step const& b) { return a.beat < b.beat; });
    return steps;
}

// What a mistake costs, and its name in the log
struct Rule
{
    uint32 spell;
    float percent;
    char const* name;
};

Rule const RuleCone = { SPELL_TAILLADE, DodgePct, "taillade" };
Rule const RuleRevers = { SPELL_REVERS, DodgePct, "revers" };
Rule const RuleChains = { SPELL_CHAINS, DodgePct, "chains" };
Rule const RuleTracked = { SPELL_TRACKED, DodgePct, "tracking" };
Rule const RuleBattue = { SPELL_BATTUE, BattuePct, "battue" };
Rule const RuleSnare = { SPELL_SNARE, PuzzlePct, "snare" };
Rule const RuleLanterns = { SPELL_LANTERNS, PuzzlePct, "lanterns" };
Rule const RulePreyTrap = { SPELL_PREY_TRAP, DodgePct, "prey trap" };
Rule const RuleCharge = { SPELL_CHARGE, PuzzlePct, "charge" };
Rule const RuleHallaliLeap = { SPELL_HALLALI, PuzzlePct, "hallali leap" };
Rule const RuleHallaliAstray = { SPELL_PACK_SHARE, PuzzlePct, "hallali astray" };

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
        _phase = Phase::Hunt;
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

    // His health stops at HoldPct until the Curée lets him fall
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_mayDie || _phase == Phase::None)
            return;
        uint32 const hold = HoldHealth();
        if (me->GetHealth() <= hold)
            damage = 0;
        else if (me->GetHealth() - damage < hold)
            damage = me->GetHealth() - hold;
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
            Execute(step);
        }
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            EndMusic();
        }
        // Held in the middle for a puzzle: rooted there once landed (a cast bar's end lets go of its own root only)
        if (_held && _landed && !me->HasUnitState(UNIT_STATE_ROOT))
            me->SetControlled(true, UNIT_STATE_ROOT);
        if (elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 200;
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
            "{}/{}, on him {}, his victim {}, held {}", uint32(_phase), elapsed / 1000.0f, Grid.BeatOf(elapsed),
            me->GetHealth(), me->GetMaxHealth(), me->GetHealthPct(), _next, _timeline.size(), onHim,
            me->GetVictim() ? me->GetVictim()->GetName() : "none", _held);
    }

    // Jumps the fight forward (the music is not: it goes on from where it was); the phase changes passed over run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        EndCastBar();
        Release();
        AdvanceClock(seconds * IN_MILLISECONDS);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed)
        {
            Step const step = _timeline[_next++];
            if (IsPhaseStep(step.what))
                Execute(step);
        }
        return true;
    }

    // One pattern now, the fight going on (a puzzle's leap lands two beats from now)
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat())
            return false;
        static std::map<std::string, Step> const steps = {
            { "taillade", { 0, Event::Taillade } }, { "chains", { 0, Event::Chains } },
            { "tracking", { 0, Event::Tracking } }, { "battue", { 0, Event::Battue, 1 } },
            { "snare", { 2, Event::Snare, 2 } }, { "snare3", { 2, Event::Snare, 3 } },
            { "lanterns", { 2, Event::Lanterns, 1 } }, { "lanterns2", { 2, Event::Lanterns, 2 } },
            { "hallali", { 2, Event::Hallali, 4 } }, { "hallaliprey", { 2, Event::Hallali, 2, 1 } },
            { "prey", { 2, Event::Prey } }, { "horn", { 0, Event::Horn } }, { "rustle", { 0, Event::Rustle, 1 } },
            { "rustle2", { 0, Event::Rustle, 2 } }, { "curee", { 2, Event::Curee } },
        };
        auto const found = steps.find(what);
        if (found == steps.end())
            return false;
        Step step = found->second;
        step.beat += CurrentBeat(Grid);
        Execute(step);
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
    uint32 HoldHealth() const
    {
        return std::max<uint32>(1, uint32(float(me->GetMaxHealth()) * HoldPct / 100.0f));
    }

    // --- The timeline ----------------------------------------------------------------------------------------------
    // step.beat: a WoW pattern's first red, a puzzle's leap landing
    void Execute(Step const& step)
    {
        uint32 const elapsed = Elapsed();
        LOG_INFO(_logName, "Traqueur {} instance={} at={:.2f}s beat={:.1f} health={:.1f}%", EventName(step.what),
                 me->GetInstanceId(), elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealthPct());
        uint32 const at = step.beat;
        switch (step.what)
        {
            case Event::Taillade: Taillade(at); break;
            case Event::Chains: Chains(at); break;
            case Event::Tracking: Tracking(at); break;
            case Event::Battue: Battue(at, step.arg != 0); break;
            case Event::Snare: Snare(at, step.arg); break;
            case Event::Lanterns: Lanterns(at, step.arg); break;
            case Event::Hallali: Hallali(at, step.arg, step.arg2 != 0); break;
            case Event::Prey: Prey(at); break;
            case Event::Horn: Horn(at); break;
            case Event::TraqueStart: StartTraque(); break;
            case Event::Rustle: Rustle(at, step.arg); break;
            case Event::TraqueEnd: EndTraque(); break;
            case Event::Curee: Curee(at); break;
        }
    }

    // --- A wave: red drawn on a beat, struck on a later one ----------------------------------------------------------
    // draw(durationMs) draws the wave's areas (the time left to its beat) and returns them as drawn; whoever stands in
    // any of them on the beat it lands is struck by rule (a mistake)
    void Wave(uint32 shown, uint32 lands, std::function<Areas(uint32)> draw, Rule const& rule)
    {
        AtBeat(Grid, shown, [this, lands, draw, &rule]()
        {
            Areas const areas = draw(MsTo(lands));
            AtBeat(Grid, lands, [this, areas, &rule]() { Strike(areas, rule); });
        });
    }

    uint32 MsTo(uint32 beat) const
    {
        uint32 const at = Grid.At(beat);
        uint32 const now = Elapsed();
        return at > now ? at - now : 0;
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

    // An area only the bots are told of (a puzzle's spring, unseen by the players), from now to its beat
    void Watch(GroundIndicators::Area const& area, uint32 lands)
    {
        GroundIndicators::WatchArea(me, area, MsTo(lands), MistakeDamage);
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

    // --- The middle: his leap there for a puzzle, held until it resolves -----------------------------------------------
    // The leap takes the two beats before lands; his hunt's sigil under him while he holds
    void TakeMiddle(uint32 lands, uint32 until)
    {
        _held = true;
        _landed = false;
        AtBeat(Grid, lands >= 2 ? lands - 2 : 0, [this]()
        {
            EndCastBar();
            me->SetControlled(false, UNIT_STATE_ROOT);
            Position const middle = Middle();
            float const seconds = Grid.beatMs * 2.0f / 1000.0f;
            if (me->GetExactDist2d(&middle) > 1.0f)
                me->GetMotionMaster()->MoveJump(middle, std::max(10.0f, me->GetExactDist2d(&middle) / seconds), 8.0f);
        });
        AtBeat(Grid, lands, [this, until]()
        {
            _landed = true;
            GroundIndicators::ShowDecal(me, Middle(), 0.0f, 6.0f, MsTo(until), LOOK_HUNT_SIGIL);
        });
        AtBeat(Grid, until, [this]() { Release(); });
    }

    void Release()
    {
        if (!_held)
            return;
        _held = false;
        _landed = false;
        if (!CastingBar())
            me->SetControlled(false, UNIT_STATE_ROOT);
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
        GroundIndicators::Area const cone = GroundIndicators::ShowAimedCone(me, apex, facing, TailladeRadius,
            TailladeArc, MsTo(lands), tank, GroundIndicators::Theme::None, MistakeDamage);
        // The Revers drawn at once behind him: his flanks are the only place clear of both
        GroundIndicators::Area const back = GroundIndicators::ShowCone(me, apex, facing + float(M_PI), TailladeRadius,
            TailladeArc, MsTo(lands + 2), GroundIndicators::Theme::None, MistakeDamage);
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

    // The tanks: him held at the hall's middle, the other tank at his flank; the swap - the tank with SwapStacks of
    // Lacération leaves him to the other (bots: GroundIndicators::SetBossHolder)
    void UpdateTanks()
    {
        if (_phase == Phase::Traque)
            return;
        GroundIndicators::SetTankSpot(me, Middle(), 1000, 6.0f);
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

    // --- Le Moulinet: four arms from him turning, stopped dead on a beat: a bar later they strike where they stopped -----
    void Chains(uint32 start)
    {
        CastBar(CAST_CHAINS);
        Position const center = Ground(me->GetPosition());
        float const facing = frand(0.0f, 2.0f * float(M_PI));
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const radiansPerSecond = way * 2.0f * float(M_PI) / (Grid.beatMs * float(ChainTurnBeats) / 1000.0f);
        uint32 const stops = start + urand(4, 6);
        uint32 const spinMs = MsTo(stops);
        Areas spinning;
        for (uint32 arm = 0; arm < 4; ++arm)
            spinning.push_back(GroundIndicators::ShowSweepingRectangle(me, center,
                facing + float(arm) * float(M_PI) / 2.0f, radiansPerSecond, ChainLength / 2.0f, ChainWidth, spinMs,
                MistakeDamage));
        Wave(stops, stops + 4, [this, spinning, radiansPerSecond, spinMs](uint32 ms)
        {
            EndCastBar();
            Areas areas;
            for (GroundIndicators::Area const& arm : spinning)
            {
                GroundIndicators::Area const stopped = GroundIndicators::CurrentSweep(arm, radiansPerSecond, spinMs);
                areas.push_back(GroundIndicators::ShowRectangle(me, stopped.origin, stopped.origin.GetOrientation(),
                    stopped.radius, ChainWidth, ms, GroundIndicators::Theme::None, MistakeDamage));
            }
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
        {
            GroundIndicators::ShowCarriedLook(player, LOOK_TRACK_MARK, MsTo(freezes + 4));
            marks.push_back({ player->GetGUID(), GroundIndicators::ShowCarriedCircle(me, player, TrackRadius,
                MsTo(freezes), MistakeDamage) });
        }
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
                Wave(lands - BattueWarnBeats, lands, [this, x, gap](uint32 ms) { return Strip(x, gap, ms); },
                    RuleBattue);
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

    // --- Le Collet: the order flashed, then sprung unseen ------------------------------------------------------------
    // In the middle from beat `lands`: each spring flashed in turn (its painted look, two beats each), then two bars of
    // nothing, then they spring in that order a bar apart - the ring (from SnareRadius to the hall's edge: be in) or
    // the circle (be out). Nothing red ahead: what the flashes said is remembered; each spring is seen as it snaps.
    void Snare(uint32 lands, uint32 springs)
    {
        uint32 const flashes = lands + 3;
        uint32 const first = flashes + 2 * springs + 8;
        uint32 const until = first + 4 * (springs - 1) + 2;
        TakeMiddle(lands, until);
        AtBeat(Grid, lands, [this]() { CastBar(CAST_SNARE); });
        AtBeat(Grid, first - 4, [this]() { EndCastBar(); });
        Position const center = Middle();
        for (uint32 spring = 0; spring < springs; ++spring)
        {
            bool const ring = urand(0, 1);
            uint32 const flash = flashes + 2 * spring;
            uint32 const strikes = first + 4 * spring;
            AtBeat(Grid, flash, [this, center, ring]()
            {
                GroundIndicators::ShowDecal(me, center, 0.0f, ring ? HallRadius : SnareRadius,
                    uint32(Grid.beatMs * 2.0f), ring ? LOOK_SNARE_RING : LOOK_SNARE_CIRCLE);
            });
            GroundIndicators::Area area;
            area.kind = ring ? GroundIndicators::Area::Kind::Ring : GroundIndicators::Area::Kind::Circle;
            area.origin = center;
            area.radius = ring ? HallRadius : SnareRadius;
            area.inner = ring ? SnareRadius : 0.0f;
            // The bots are told of each spring a bar ahead
            AtBeat(Grid, strikes - 4, [this, area, strikes]() { Watch(area, strikes); });
            AtBeat(Grid, strikes, [this, area, ring, center]()
            {
                // The snare snapping, seen as it strikes
                if (ring)
                    GroundIndicators::ShowRing(me, center, HallRadius, SnareRadius, uint32(Grid.beatMs),
                        GroundIndicators::Theme::None, MistakeDamage);
                else
                    GroundIndicators::ShowCircle(me, center, SnareRadius, uint32(Grid.beatMs),
                        GroundIndicators::Theme::None, MistakeDamage);
                Strike({ area }, RuleSnare);
            });
        }
    }

    // --- Les Lanternes des rabatteurs: lanterns at the edge, then their light over the hall -----------------------------
    // In the middle from beat `lands`: lanterns stand at the hall's edge, facing the middle; two bars later each throws
    // its light, a cone over the hall, and he strikes whatever it shows - only beside a lantern is dark. A second
    // volley: more lanterns, turned, their light wider.
    void Lanterns(uint32 lands, uint32 volleys)
    {
        uint32 const until = lands + 2 + 8 * volleys + 2;
        TakeMiddle(lands, until);
        // His cast from the leap to the first light
        AtBeat(Grid, lands, [this]() { CastBar(CAST_LANTERNS); });
        AtBeat(Grid, lands + 10, [this]() { EndCastBar(); });
        float const base = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 volley = 0; volley < volleys; ++volley)
        {
            uint32 const lit = lands + 2 + 8 * volley;
            uint32 const strikes = lit + 8;
            uint32 const count = volley ? 4 : 3;
            float const arc = volley ? LanternWideArc : LanternArc;
            float const turn = volley ? float(M_PI) / 4.0f : 0.0f;
            AtBeat(Grid, lit, [this, strikes, count, arc, base, turn]()
            {
                Position const middle = Middle();
                Areas lights;
                for (uint32 index = 0; index < count; ++index)
                {
                    float const angle = base + turn + float(index) * 2.0f * float(M_PI) / float(count);
                    Position spot = AtAngle(middle, angle, LanternDistance);
                    GroundIndicators::ShowBillboard(me, spot, LOOK_LANTERN, MsTo(strikes + 2));
                    spot.SetOrientation(angle + float(M_PI));
                    GroundIndicators::Area light;
                    light.kind = GroundIndicators::Area::Kind::Cone;
                    light.origin = spot;
                    light.radius = LanternReach;
                    light.arc = arc * float(M_PI) / 180.0f;
                    Watch(light, strikes);
                    lights.push_back(light);
                }
                // Their light thrown two beats before it strikes: seen, too late to read it then
                AtBeat(Grid, strikes - 2, [this, lights, arc, strikes]()
                {
                    for (GroundIndicators::Area const& light : lights)
                        GroundIndicators::ShowCone(me, light.origin, light.origin.GetOrientation(), LanternReach, arc,
                            MsTo(strikes), GroundIndicators::Theme::Fire, MistakeDamage);
                });
                AtBeat(Grid, strikes, [this, lights]() { Strike(lights, RuleLanterns); });
            });
        }
    }

    // --- L'Hallali: quarries away from everyone, the others together under him ------------------------------------------
    // In the middle from beat `lands`; the quarries marked two beats later, struck HallaliMarkBeats after. trail: the
    // quarries leave la Proie's traps behind them meanwhile.
    void Hallali(uint32 lands, uint32 quarries, bool trail)
    {
        uint32 const marked = lands + 2;
        uint32 const strikes = marked + HallaliMarkBeats;
        TakeMiddle(lands, strikes + 2);
        AtBeat(Grid, marked, [this, quarries, trail, marked, strikes]()
        {
            std::vector<Player*> chosen = Targets(quarries);
            uint32 const durationMs = MsTo(strikes);
            std::vector<ObjectGuid> guids;
            for (Player* quarry : chosen)
            {
                AddTimedAura(quarry, SPELL_QUARRY, durationMs);
                GroundIndicators::ShowCarriedLook(quarry, LOOK_QUARRY_MARK, durationMs);
                GroundIndicators::ShowCarriedCircle(me, quarry, HallaliRadius, durationMs, MistakeDamage, 2.0f);
                guids.push_back(quarry->GetGUID());
                if (trail)
                    Trail(quarry->GetGUID(), marked + 4, strikes - 1);
            }
            // The others' place: under him, his tank in it; everyone not a quarry is sent (a quarry never is)
            Position const soak = Middle();
            GroundIndicators::ShowDecal(me, soak, 0.0f, HallaliSoakRadius, durationMs, LOOK_PACK_SIGIL);
            GroundIndicators::ShowSoak(me, soak, HallaliSoakRadius, durationMs, uint32(ArenaPlayers().size()),
                GroundIndicators::Theme::Holy, true);
            AtBeat(Grid, strikes - 10, [this]() { CastBar(CAST_HALLALI); });
            AtBeat(Grid, strikes, [this, guids, soak]()
            {
                EndCastBar();
                StrikeHallali(guids, soak);
            });
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

    // --- La Proie: two marked players leave a trail of snapping traps, a beat apart -------------------------------------
    // In the middle from beat `lands`; marked two beats later, their trail for TrailBeats after a bar
    void Prey(uint32 lands)
    {
        uint32 const marked = lands + 2;
        uint32 const from = marked + 4;
        TakeMiddle(lands, from + TrailBeats + 4);
        AtBeat(Grid, marked, [this, from]()
        {
            CastBar(CAST_PREY);
            for (Player* prey : Targets(2))
            {
                AddTimedAura(prey, SPELL_PREY, MsTo(from + TrailBeats));
                GroundIndicators::ShowCarriedLook(prey, LOOK_PREY_MARK, MsTo(from + TrailBeats));
                Trail(prey->GetGUID(), from, from + TrailBeats - 1);
            }
        });
        AtBeat(Grid, from, [this]() { EndCastBar(); });
    }

    // A trap where the player stands, every beat from `from` to `to`, each snapping three beats after it is laid
    void Trail(ObjectGuid guid, uint32 from, uint32 to)
    {
        for (uint32 beat = from; beat <= to; ++beat)
            Wave(beat, beat + 3, [this, guid](uint32 ms)
            {
                Player* prey = ObjectAccessor::GetPlayer(*me, guid);
                if (!prey || !prey->IsAlive())
                    return Areas{};
                return Areas{ GroundIndicators::ShowCircle(me, Ground(prey->GetPosition()), TrapRadius, ms,
                    GroundIndicators::Theme::None, MistakeDamage) };
            }, RulePreyTrap);
    }

    // --- Cor de chasse: a group hit on the beat ------------------------------------------------------------------------
    void Horn(uint32 start)
    {
        CastBar(CAST_HORN);
        GroundIndicators::WarnGroupDamage(me, MsTo(start + 3));
        AtBeat(Grid, start + 3, [this]()
        {
            EndCastBar();
            HitEveryone(SPELL_HORN, HornPct);
        });
    }

    // --- La Traque: gone in the dark ---------------------------------------------------------------------------------
    void StartTraque()
    {
        Release();
        _phase = Phase::Traque;
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

    // A rustle at the hall's edge (two, square to each other: crossing); two bars later he charges from there straight
    // through the middle to the far wall. His line is seen only as he charges.
    void Rustle(uint32 start, uint32 charges)
    {
        uint32 const strikes = start + 8;
        float const base = frand(0.0f, 2.0f * float(M_PI));
        Position const middle = Middle();
        Areas lines;
        for (uint32 index = 0; index < charges; ++index)
        {
            float const angle = base + float(index) * float(M_PI) / 2.0f;
            GroundIndicators::ShowBillboard(me, AtAngle(middle, angle, RustleDistance), LOOK_RUSTLE, MsTo(strikes));
            // His line: from the rustle through the middle, as two halves from the middle out
            for (float facing : { angle, angle + float(M_PI) })
            {
                GroundIndicators::Area line;
                line.kind = GroundIndicators::Area::Kind::Rectangle;
                line.origin = middle;
                line.origin.SetOrientation(facing);
                line.radius = RustleDistance + 4.0f;
                line.width = ChargeWidth;
                Watch(line, strikes);
                lines.push_back(line);
            }
        }
        AtBeat(Grid, strikes - 2, [this, lines, strikes]()
        {
            for (GroundIndicators::Area const& line : lines)
                GroundIndicators::ShowRectangle(me, line.origin, line.origin.GetOrientation(), line.radius, line.width,
                    MsTo(strikes), GroundIndicators::Theme::Shadow, MistakeDamage);
        });
        AtBeat(Grid, strikes, [this, lines]() { Strike(lines, RuleCharge); });
    }

    // Back from the dark on the track's hardest hit, in the middle, howling
    void EndTraque()
    {
        Position const middle = Middle();
        me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(), me->GetOrientation());
        me->RestoreDisplayId();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        _phase = Phase::Pursuit;
        HitEveryone(SPELL_HOWL, HowlPct);
    }

    // --- La Curée: the check on the climax ---------------------------------------------------------------------------
    // In the middle from beat `lands`, its cast from two beats later to the climax: held at HoldPct, he falls on its
    // beat (the group hit all the same); above it, the group dies
    void Curee(uint32 lands)
    {
        uint32 const cast = lands + 2;
        uint32 const strikes = cast + 16;
        TakeMiddle(lands, strikes);
        AtBeat(Grid, cast, [this, strikes]()
        {
            CastBar(CAST_CUREE);
            GroundIndicators::WarnGroupDamage(me, MsTo(strikes));
        });
        AtBeat(Grid, strikes, [this]()
        {
            EndCastBar();
            bool const held = me->GetHealth() <= HoldHealth();
            LOG_INFO(_logName, "Traqueur curee instance={} at={:.1f}s health={:.1f}% alive={} held={}",
                     me->GetInstanceId(), Elapsed() / 1000.0f, me->GetHealthPct(), ArenaPlayers().size(), held);
            if (!held)
            {
                for (Player* player : ArenaPlayers())
                    Doom(player, SPELL_CUREE);
                return;
            }
            HitEveryone(SPELL_CUREE, CureeHitPct);
            _mayDie = true;
            std::vector<Player*> const players = ArenaPlayers();
            Unit* credit = me->GetVictim() ? me->GetVictim() : (players.empty() ? nullptr : players.front());
            Unit::Kill(credit ? credit : me, me);
        });
    }

    // --- Mistakes ----------------------------------------------------------------------------------------------------
    // An avoidable hit, and Débusqué: what they take is heavier for a while
    void Mistake(Player* player, Rule const& rule)
    {
        if (!player || !player->IsAlive())
            return;
        Hit(player, rule.spell, rule.percent);
        if (player->IsAlive())
            AddTimedAura(player, SPELL_DEBUSQUE, DebusqueMs);
        LOG_INFO(_logName, "Traqueur rule broken instance={} at={:.1f}s {}: {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, rule.name, player->GetName());
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
        _held = false;
        _landed = false;
        _mayDie = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
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

    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    size_t _next = 0;
    uint32 _nextCheckMs = 0;
    bool _musicOver = false;
    bool _held = false;
    bool _landed = false;
    bool _mayDie = false;
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

    // .traqueur cast <taillade|chains|tracking|battue|snare|snare3|lanterns|lanterns2|hallali|hallaliprey|prey|horn|
    // rustle|rustle2|curee>
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
