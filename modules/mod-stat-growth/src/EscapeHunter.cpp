#include "DefiBoss.h"

#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "StringFormat.h"

#include <string>

// Le Traqueur d'évadés, the Hellfire Gaol's second gate: a Défi board boss in the Blood Furnace, where Keli'dan the
// Breaker channels (plan: .agents/plans/escape-hunter). 8 players - 2 tanks, 2 healers, 4 damage dealers - tuned for
// item level 477 and 650 paragon, as Gardien-chef Vorhan, the first gate.
//
// A mix of the gates before it: WoW-speed fighting (his pack, nets, traps, horn, archers, a tank swap) between FFXIV
// set pieces (la Battue, l'Hallali, la Traque, la Curée), **all of it on his music**. The track is played once from
// the pull and every step lands on a beat of it: the script holds the beat grid measured on the track (155 BPM) and
// places each step on a beat number, never on a time of its own. The music ends at 4:25.5: the silence, then the hard
// enrage.
//
// What every Défi boss does alike (a challenge's instance only, the room cleared, its health, its music) is
// Defi::BossAI's (DefiBoss.h).

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As Vorhan's profile (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can be hit
// from the pull to the Curée (4:04.0) but during la Traque (2:04.0-2:41.2, he is away).
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, 244.0f - 37.2f, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("traqueur.health_scale", 1.0f);

// Keli'dan's room: the Traqueur where he channels, the room's middle; his channelers stood 24 yd out. Its walls are to
// be measured in game before any mechanic is placed (the plan).
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 45.0f, 120.0f, 60.0f };

// --- The music ------------------------------------------------------------------------------------------------------
// prison_first_boss_music_2 (Music.EscapeHunter): 155 BPM, a line fitted to its 684 beats (95% within 42 ms). Its
// sections start on bars (4 beats): the build-up on bar 12, section A on 28, the peak on 76, the breakdown on 80 and its
// second half on 92, the rebuild on 104, section B on 116, the outro on 160. The strongest hits are on beats 55, 182,
// 234, 310 (the peak), 363, 468, 517, 533, 562, 575, 630 (the climax) and 657. The track ends at MusicEndMs.
constexpr Defi::BeatGrid Grid = { 168.2f, 387.09f };
constexpr uint32 MusicEndMs = 265460;

enum Npcs : uint32
{
    NPC_TRAQUEUR            = 930400,
    NPC_FELHOUND            = 930401,
    NPC_ARCHER              = 930402,
    NPC_BEATER              = 930403,
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

enum class Phase : uint8
{
    None,
    Intro,          // 0:00, bar 0: him alone
    Hunt,           // 0:43.5, bar 28: section A, his rotation
    Traque,         // 2:04.0, bar 80: the breakdown, the hunt in the dark
    Rebuild,        // 2:41.2, bar 104: back with his pack
    Frenzy,         // 2:59.8, bar 116: section B, everything faster
    Outro,          // 4:07.9, bar 160: the kill window
    Over,
};

// What happens on a beat (each written in the steps to come; for now a log and his yells)
enum class Event : uint8
{
    Horn,           // Cor de chasse: a pack and a group hit
    Net,            // Filet: a net on a player
    Rotation,       // his rotation's turn (Taillade, the traps, an archer)
    Battue,         // la Battue: the beaters' lines, one a beat
    Hallali,        // l'Hallali: the peak's mechanic
    TraqueStart,    // la Traque: the dark
    TraqueEnd,
    Rebuild,        // back with his pack
    FrenzyStart,    // section B
    Combined,       // a hit of section B: two mechanics at once
    Curee,          // la Curée: the damage check
    Outro,
    MusicEnd,       // silence, the hard enrage
};

struct Step
{
    uint32 beat;
    Event what;
};

// The fight, on the music: every step on a beat of the grid
std::vector<Step> BuildTimeline()
{
    std::vector<Step> steps = {
        { 48, Event::Horn },            // 0:18.7, the build-up
        { 55, Event::Net },             // 0:21.4, the first hit
        { 182, Event::Battue },         // 1:10.6
        { 234, Event::Battue },         // 1:30.7
        { 310, Event::Hallali },        // 2:00.1, the peak
        { 320, Event::TraqueStart },    // 2:04.0, the breakdown
        { 416, Event::TraqueEnd },      // 2:41.2
        { 416, Event::Rebuild },
        { 464, Event::FrenzyStart },    // 2:59.8, section B
        { 468, Event::Combined },       // 3:01.3
        { 517, Event::Combined },       // 3:20.3
        { 533, Event::Combined },       // 3:26.5
        { 562, Event::Combined },       // 3:37.7
        { 575, Event::Combined },       // 3:42.7
        { 630, Event::Curee },          // 4:04.0, the climax
        { 640, Event::Outro },          // 4:07.9
    };
    // Section A: his rotation every 8 bars (32 beats), from bar 28 to the peak; section B every 4 bars
    for (uint32 beat = 112; beat < 304; beat += 32)
        steps.push_back({ beat, Event::Rotation });
    for (uint32 beat = 464; beat < 624; beat += 16)
        steps.push_back({ beat, Event::Rotation });
    std::stable_sort(steps.begin(), steps.end(), [](Step const& a, Step const& b) { return a.beat < b.beat; });
    return steps;
}

char const* EventName(Event what)
{
    switch (what)
    {
        case Event::Horn: return "horn";
        case Event::Net: return "net";
        case Event::Rotation: return "rotation";
        case Event::Battue: return "battue";
        case Event::Hallali: return "hallali";
        case Event::TraqueStart: return "traque start";
        case Event::TraqueEnd: return "traque end";
        case Event::Rebuild: return "rebuild";
        case Event::FrenzyStart: return "frenzy";
        case Event::Combined: return "combined";
        case Event::Curee: return "curee";
        case Event::Outro: return "outro";
        case Event::MusicEnd: return "music end";
    }
    return "?";
}

bool IsPhaseStep(Event what)
{
    return what == Event::TraqueStart || what == Event::TraqueEnd || what == Event::Rebuild ||
        what == Event::FrenzyStart || what == Event::Outro;
}

struct boss_escape_hunter : public Defi::BossAI
{
    boss_escape_hunter(Creature* creature) : Defi::BossAI(creature, Room, "module.traqueur") { }

    void Reset() override
    {
        // A fight still on is a wipe: the board resets him once the players are down
        if (_phase != Phase::None && _phase != Phase::Over)
        {
            LOG_INFO(_logName, "Traqueur wipe instance={} at={:.1f}s health={:.1f}%", me->GetInstanceId(),
                     Elapsed() / 1000.0f, me->GetHealthPct());
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
        LOG_INFO(_logName, "Traqueur killed instance={} at={:.1f}s", me->GetInstanceId(), Elapsed() / 1000.0f);
        EndMusic();
        ResetFight();
        _phase = Phase::Over;
    }

    void UpdateAI(uint32 diff) override
    {
        bool const fighting = _phase != Phase::None && _phase != Phase::Over;
        if (!UpdateVictim())
        {
            if (!fighting)
                UpdateOutOfCombat(diff);
            return;
        }

        scheduler.Update(diff);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed && _phase != Phase::Over)
            Execute(_timeline[_next++].what);
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            Execute(Event::MusicEnd);
        }
        DoMeleeAttackIfReady();
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        uint32 const elapsed = Elapsed();
        return Acore::StringFormat("Traqueur: phase {} at {:.1f}s (beat {:.1f}), health {}/{} ({:.1f}%), next step "
            "{}/{}", uint32(_phase), elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealth(), me->GetMaxHealth(),
            me->GetHealthPct(), _next, _timeline.size());
    }

    // Jumps the fight forward (the music is not: it goes on from where it was); the phase changes passed over run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        scheduler.CancelAll();
        AdvanceClock(seconds * IN_MILLISECONDS);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed)
            if (Event const what = _timeline[_next++].what; IsPhaseStep(what))
                Execute(what);
        return true;
    }

protected:
    Defi::Sizing const& GetSizing() const override { return Sizing; }
    float HealthScale() const override { return float(::HealthScale); }
    bool IsOwn(Creature const* creature) const override
    {
        uint32 const entry = creature->GetEntry();
        return entry == NPC_FELHOUND || entry == NPC_ARCHER || entry == NPC_BEATER;
    }

private:
    void Execute(Event what)
    {
        uint32 const elapsed = Elapsed();
        LOG_INFO(_logName, "Traqueur {} instance={} at={:.2f}s beat={:.1f}", EventName(what), me->GetInstanceId(),
                 elapsed / 1000.0f, Grid.BeatOf(elapsed));
        switch (what)
        {
            case Event::Horn: Talk(SAY_HORN); break;
            case Event::Battue: Talk(SAY_BATTUE); break;
            case Event::TraqueStart: _phase = Phase::Traque; Talk(SAY_TRAQUE); break;
            case Event::Rebuild: _phase = Phase::Rebuild; break;
            case Event::FrenzyStart: _phase = Phase::Frenzy; break;
            case Event::Curee: Talk(SAY_CUREE); break;
            case Event::Outro: _phase = Phase::Outro; break;
            case Event::MusicEnd: EndMusic(); break;
            case Event::Rotation:
                if (_phase == Phase::Intro)
                    _phase = Phase::Hunt;
                break;
            default:
                break;
        }
    }

    void ResetFight()
    {
        scheduler.CancelAll();
        _summons.DespawnAll();
        _timeline.clear();
        _next = 0;
        _musicOver = false;
        _phase = Phase::None;
        StopClock();
    }

    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    size_t _next = 0;
    bool _musicOver = false;
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .traqueur info | skip <seconds> | pull: for game masters trying the fight
class EscapeHunterCommandScript final : public CommandScript
{
public:
    EscapeHunterCommandScript() : CommandScript("EscapeHunterCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable traqueurTable = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "skip", HandleSkip, SEC_GAMEMASTER, Console::No },
            { "pull", HandlePull, SEC_GAMEMASTER, Console::No },
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
