#include "DefiBoss.h"
#include "GroundIndicators.h"

#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "LFG.h"
#include "Random.h"
#include "StringFormat.h"
#include "TemporarySummon.h"

#include <array>
#include <map>
#include <set>
#include <string>

// Le Traqueur d'évadés, the Hellfire Gaol's second gate: a Défi board boss in the Blood Furnace, in Keli'dan the
// Breaker's hall (plan: .agents/plans/escape-hunter). 8 players - 2 tanks, 2 healers, 4 damage dealers - tuned for
// item level 477 and 650 paragon, as Gardien-chef Vorhan, the first gate.
//
// A mix of the gates before it: WoW-speed fighting between FFXIV set pieces, **all of it on his music**. The track is
// played once from the pull and every step lands on a beat of it: the script holds the beat grid measured on the track
// (155 BPM) and places each step on a beat number, never on a time of its own.
// - His rotation (WoW): Taillade on his tank (a cone that stacks Lacération: the tank swap), and in turn his traps
//   (under players; armed, they catch a player - or a hound dragged into one), an archer beater (its arrow
//   interrupted, the skull on it), a net (a player rooted until the hound holding it dies), his horn (a group hit and
//   a pack for the second tank).
// - The set pieces (FFXIV): la Battue (strips of the hall swept one a beat, a gap in each to stand in, sometimes a
//   second pass back with other gaps), l'Hallali on the music's peak (marked quarries away from everyone, the others
//   together to share the pack's blow), la Traque on the breakdown (he is gone; torchlight pools move round the hall,
//   whoever stands out of one on a strike's beat is caught), la Curée on the climax (above CureePct of his health,
//   the group dies).
// - A mistake (a strip, a trap, a quarry's leap, out of the light) marks: Débusqué, and a second one under it is
//   death. The music ends at 4:25.5: the silence, then the hard enrage.
//
// What every Défi boss does alike (a challenge's instance only, the hall cleared, its health, its music, its hits) is
// Defi::BossAI's (DefiBoss.h).

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As Vorhan's profile (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can be hit
// from the pull to the Curée (4:04.0) but during la Traque (2:04.0-2:41.2, he is away), less the seconds his pack
// takes (AddSeconds: the horns' hounds, the nets', the archers'). Checked on three 8-bot groups, 2026-10-10: at this
// health they would stand at the Curée at 3%, 15% and 30% - the middle one on its line.
constexpr float AddSeconds = 45.0f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, 244.0f - 37.2f - AddSeconds, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("traqueur.health_scale", 1.0f);

// Keli'dan's hall: open floor 55 yd and more round its middle (measured on the navigation mesh, 2026-10-10), an exit
// to the south. The fight keeps within FightRadius.
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 120.0f, 60.0f };
constexpr float FightRadius = 40.0f;

// --- The music ------------------------------------------------------------------------------------------------------
// prison_first_boss_music_2 (Music.EscapeHunter): 155 BPM, a line fitted to its 684 beats (95% within 42 ms). Its
// sections start on bars (4 beats): the build-up on bar 12, section A on 28, the peak on 76, the breakdown on 80, the
// rebuild on 104, section B on 116, the outro on 160. The strongest hits are on beats 55, 182, 234, 310 (the peak),
// 468, 517, 533, 562, 575 and 630 (the climax). The track ends at MusicEndMs.
constexpr Defi::BeatGrid Grid = { 168.2f, 387.09f };
constexpr uint32 MusicEndMs = 265460;

// --- His abilities --------------------------------------------------------------------------------------------------
// Hits are shares of the reference health (a damage dealer of the profile)
constexpr float MeleeFloorPct = 10.0f;      // his swing on a player: at least this
constexpr float HoundMeleePct = 4.0f;       // a hound's
// Taillade: a cone on his tank after its cast; the tank takes TailladeTankPct and a stack of Lacération (taken
// LacerationTakenPct more a stack, for LacerationMs), anyone else in it a mistake. At SwapStacks the other tank takes him.
constexpr float TailladeTankPct = 55.0f;
constexpr uint32 ToolBeats = 6;              // his next tool, this many beats after Taillade's start
constexpr float TailladeRadius = 12.0f;
constexpr float TailladeArc = 100.0f;
constexpr uint32 TailladeCastMs = 2000;
constexpr float LacerationTakenPct = 15.0f;
constexpr uint32 LacerationMs = 25000;
constexpr uint8 SwapStacks = 3;
// Cor de chasse: everyone, and a pack (each hound lasting PackSeconds of the group's area damage), the hounds alive
// topped up to MaxHounds at most
constexpr float HornPct = 20.0f;
constexpr float PackSeconds = 6.0f;
constexpr uint32 MaxHounds = 6;
// La Traque's hounds, on a healer each: HuntingSeconds of one target's damage; gone back to the dark at its end
constexpr float HuntingSeconds = 2.5f;
// Filet: a player rooted for NetMs, freed when the hound holding it dies (NetHoundSeconds of one target's damage)
constexpr uint32 NetMs = 9000;
constexpr float NetHoundSeconds = 3.5f;
constexpr float NetStranglePct = 90.0f;
// Pièges à mâchoires: TrapCount traps under players, armed after TrapWarnMs, for TrapArmedMs
constexpr uint32 TrapCount = 3;
constexpr uint32 TrapWarnMs = 2000;
constexpr uint32 TrapArmedMs = 25000;
constexpr float TrapRadius = 3.0f;
constexpr float TrapPct = 40.0f;
constexpr uint32 TrapRootMs = 3000;
constexpr uint32 TrapHoundRootMs = 6000;
constexpr float TrappedHoundTakenPct = 50.0f;
// An archer beater: ArcherSeconds of one target's damage, its arrow every ArcherEveryMs (a 4 s cast)
constexpr float ArcherSeconds = 4.0f;
constexpr uint32 ArcherEveryMs = 7000;
constexpr float ArrowPct = 55.0f;
// La Battue: BattueStrips strips across the hall, BattueStripDepth deep, a gap BattueGap wide in each; a strip lands a
// beat after the one before, shown BattueWarnBeats ahead
constexpr uint32 BattueStrips = 9;
constexpr float BattueStripDepth = 8.0f;
constexpr float BattueHalfWidth = 40.0f;
constexpr float BattueGap = 10.0f;
constexpr uint32 BattueWarnBeats = 8;
constexpr float BattuePct = 70.0f;
// L'Hallali: the quarries' circles (HallaliRadius), the group's share (HallaliSoakPct split among those in its circle)
constexpr float HallaliRadius = 8.0f;
constexpr float HallaliQuarryPct = 45.0f;
constexpr float HallaliMistakePct = 80.0f;
constexpr float HallaliSoakRadius = 7.0f;
constexpr float HallaliSoakPct = 160.0f;
constexpr uint32 HallaliMarkBeats = 12;
// La Traque: TorchCount pools round the hall, moved every TorchMoveBeats (the next shown TorchPreviewBeats ahead, the
// old ones lit until they go), a strike every TraqueStrikeBeats between the moves (off the move's own beats: none from
// a set's preview until TraqueStrikeBeats / 2 after it is in place) on whoever stands out of every pool
constexpr uint32 TorchCount = 3;
constexpr float TorchRadius = 7.0f;
constexpr float TorchDistance = 22.0f;
constexpr uint32 TorchMoveBeats = 32;
constexpr uint32 TorchPreviewBeats = 8;
constexpr uint32 TraqueStrikeBeats = 8;
constexpr float TraquePct = 60.0f;
constexpr uint32 InvisibleDisplay = 11686;     // the invisible trigger's
// A mistake: Débusqué for DebusqueMs, MistakeTakenPct more taken; a mistake under it is death
constexpr uint32 DebusqueMs = 15000;
constexpr float MistakeTakenPct = 100.0f;
// La Curée: above this share of his health as it lands, the group dies; under it, a group hit
constexpr float CureePct = 15.0f;
constexpr float CureeHitPct = 40.0f;
// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

enum Spells : uint32
{
    SPELL_TAILLADE          = 94800,
    SPELL_HORN              = 94801,
    SPELL_STRANGLE          = 94802,
    SPELL_TRAP              = 94803,
    SPELL_ARROW_HIT         = 94804,
    SPELL_BATTUE            = 94805,
    SPELL_HALLALI           = 94806,
    SPELL_PACK_SHARE        = 94807,
    SPELL_TRAQUE            = 94808,
    SPELL_CUREE             = 94809,
    SPELL_HUNT_OVER         = 94810,
    SPELL_LACERATION        = 94820,
    SPELL_DEBUSQUE          = 94821,
    SPELL_QUARRY            = 94822,
    SPELL_JAWS              = 94823,
    SPELL_NET               = 94824,
    CAST_TAILLADE           = 94840,
    CAST_HORN               = 94841,
    CAST_BATTUE             = 94842,
    CAST_HALLALI            = 94843,
    CAST_TRAQUE             = 94844,
    CAST_CUREE              = 94845,
    SPELL_ARROW             = 94846,
};

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

// What happens on a beat
enum class Event : uint8
{
    Horn,
    Net,
    Rotation,       // Taillade and the next of his tools
    Battue,         // two passes
    BattueOnce,     // one pass
    Hallali,        // its marks, 12 beats before the beat it lands on
    TraqueStart,
    TraqueEnd,
    FrenzyStart,
    NetAndHorn,
    Curee,          // its cast, 16 beats before the beat it lands on
    Outro,
    MusicEnd,
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
        { 48, Event::Horn },                    // 0:18.7, the build-up
        { 55, Event::Net },                     // 0:21.4, the first hit
        { 174, Event::Battue },                 // its strips from beat 182 (1:10.6), back by 200
        { 226, Event::BattueOnce },             // from beat 234 (1:30.7)
        { 310 - HallaliMarkBeats, Event::Hallali },     // lands on 310, the peak (2:00.1)
        { 320, Event::TraqueStart },            // 2:04.0, the breakdown
        { 416, Event::TraqueEnd },              // 2:41.2, the rebuild: back with his pack
        { 464, Event::FrenzyStart },            // 2:59.8, section B
        { 460, Event::BattueOnce },             // its strips from 468 (3:01.3)
        { 517, Event::NetAndHorn },             // 3:20.3
        { 533 - HallaliMarkBeats, Event::Hallali },     // lands on 533 (3:26.5)
        { 554, Event::BattueOnce },             // from 562 (3:37.7)
        { 575, Event::Net },                    // 3:42.7: then the run to the Curée, his pack spared
        { 630 - 16, Event::Curee },             // lands on 630, the climax (4:04.0)
        { 640, Event::Outro },                  // 4:07.9
    };
    // Section A: his rotation every 8 bars (32 beats) from bar 28; section B every 4 bars, from bar 120
    for (uint32 beat = 112; beat < 304; beat += 32)
        steps.push_back({ beat, Event::Rotation });
    for (uint32 beat = 480; beat < 612; beat += 16)
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
        case Event::BattueOnce: return "battue once";
        case Event::Hallali: return "hallali";
        case Event::TraqueStart: return "traque start";
        case Event::TraqueEnd: return "traque end";
        case Event::FrenzyStart: return "frenzy";
        case Event::NetAndHorn: return "net and horn";
        case Event::Curee: return "curee";
        case Event::Outro: return "outro";
        case Event::MusicEnd: return "music end";
    }
    return "?";
}

bool IsPhaseStep(Event what)
{
    return what == Event::TraqueStart || what == Event::TraqueEnd || what == Event::FrenzyStart ||
        what == Event::Outro;
}

bool IsTank(Player* player)
{
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() && member.roles)
                return member.roles & lfg::PLAYER_ROLE_TANK;
    return player->HasTankSpec();
}

struct boss_escape_hunter;
boss_escape_hunter* HunterOf(Creature* add);

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

    // A wipe is the players', never combat's: while one stands in the hall he fights on (la Traque holds him away
    // from them). The board resets him once they are down.
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (_phase != Phase::None && _phase != Phase::Over && !ArenaPlayers().empty())
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
            Execute(_timeline[_next++].what);
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            Execute(Event::MusicEnd);
        }
        if (elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 250;
            CheckTraps();
            UpdateTanks();
        }
        if (_phase != Phase::Traque && !CastingBar())
            DoMeleeAttackIfReady();
    }

    // --- The pack's callbacks ----------------------------------------------------------------------------------------
    void PackDamage(Creature* add, Unit* victim, uint32& damage)
    {
        if (!victim || !victim->IsPlayer() || add->GetEntry() != NPC_FELHOUND)
            return;
        float const floor = Reference() * HoundMeleePct / 100.0f * std::max(GetChallengeDamageFactorOf(me), 1.0f);
        damage = uint32(std::max(float(damage), floor) * TakenFactor(victim));
    }

    void PackDied(Creature* add)
    {
        if (add->GetGUID() == _netHound)
            FreeNet(false);
        if (add->GetGUID() == _skulled)
        {
            _skulled.Clear();
            SetSkull(NextSkull());
        }
    }

    // An archer's arrow landed (its cast not interrupted)
    void ArrowHit(Unit* target)
    {
        if (Player* player = target ? target->ToPlayer() : nullptr)
        {
            Hit(player, SPELL_ARROW_HIT, ArrowPct);
            LOG_INFO(_logName, "Traqueur arrow instance={} at={:.1f}s on {}", me->GetInstanceId(), Elapsed() / 1000.0f,
                     player->GetName());
        }
    }

    // A player for an archer to shoot: anyone but a tank, at random
    Player* ArrowTarget()
    {
        std::vector<Player*> targets;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player))
                targets.push_back(player);
        return targets.empty() ? nullptr : Acore::Containers::SelectRandomContainerElement(targets);
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        uint32 const elapsed = Elapsed();
        uint32 hounds = 0;
        uint32 archers = 0;
        for (ObjectGuid const& guid : _summons)
            if (Creature* add = ObjectAccessor::GetCreature(*me, guid); add && add->IsAlive())
            {
                if (add->GetEntry() == NPC_ARCHER)
                    ++archers;
                else if (add->GetEntry() == NPC_FELHOUND)
                    ++hounds;
            }
        // Who fights him: the players hitting him, those on his pack
        uint32 onHim = 0;
        uint32 onPack = 0;
        for (Player* player : ArenaPlayers())
            if (Unit* target = player->GetVictim())
                ++(target == me ? onHim : onPack);
        Creature* skulled = _skulled.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*me, _skulled);
        return Acore::StringFormat("Traqueur: phase {} at {:.1f}s (beat {:.1f}), health {}/{} ({:.1f}%), next step "
            "{}/{}, hounds {}, archers {}, on him {}, on his pack {}, his victim {}, skull {}", uint32(_phase),
            elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealth(), me->GetMaxHealth(), me->GetHealthPct(), _next,
            _timeline.size(), hounds, archers, onHim, onPack, me->GetVictim() ? me->GetVictim()->GetName() : "none",
            skulled ? Acore::StringFormat("{} {:.0f} yd out at {:.0f}%{}", skulled->GetName(),
                skulled->GetExactDist2d(&GetRoom().center), skulled->GetHealthPct(),
                skulled->IsEvadingAttacks() ? " EVADING" : "") : std::string("none"));
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
            if (Event const what = _timeline[_next++].what; IsPhaseStep(what))
                Execute(what);
        return true;
    }

    // One step now, the fight going on
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat())
            return false;
        static std::map<std::string, Event> const steps = {
            { "horn", Event::Horn }, { "net", Event::Net }, { "rotation", Event::Rotation },
            { "battue", Event::Battue }, { "battueonce", Event::BattueOnce }, { "hallali", Event::Hallali },
            { "curee", Event::Curee },
        };
        auto const found = steps.find(what);
        if (found == steps.end())
            return false;
        Execute(found->second);
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
    void Execute(Event what)
    {
        uint32 const elapsed = Elapsed();
        LOG_INFO(_logName, "Traqueur {} instance={} at={:.2f}s beat={:.1f} health={:.1f}%", EventName(what),
                 me->GetInstanceId(), elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealthPct());
        switch (what)
        {
            case Event::Horn: Horn(2); break;
            case Event::Net: Net(); break;
            case Event::Rotation: Rotation(); break;
            case Event::Battue: Battue(true); break;
            case Event::BattueOnce: Battue(false); break;
            case Event::Hallali: Hallali(_phase == Phase::Frenzy ? 2 : 4); break;
            case Event::TraqueStart: StartTraque(); break;
            case Event::TraqueEnd: EndTraque(); break;
            case Event::FrenzyStart: _phase = Phase::Frenzy; break;
            case Event::NetAndHorn: Net(); Horn(3); break;
            case Event::Curee: Curee(); break;
            case Event::Outro: _phase = Phase::Outro; break;
            case Event::MusicEnd: HuntOver(); break;
        }
    }

    // Taillade, and the next of his tools: in section A his traps, an archer, a net, his horn in turn; in section B
    // (Taillade twice as often) traps and an archer every other turn (the nets and horns come on the music's hits)
    void Rotation()
    {
        if (_phase == Phase::Intro)
            _phase = Phase::Hunt;
        // A set piece running (a Battue's strips, an Hallali's marks): nothing of his rotation - a net or a trap in it is
        // death, and the Hallali's pack gathers under him, in Taillade's way
        if (CurrentBeat(Grid) + ToolBeats < _setPieceUntil)
            return;
        Taillade();
        // His tool ToolBeats after Taillade has landed, never with it (a trap under a player in the cone)
        uint32 const turn = _rotation++;
        bool const frenzy = _phase == Phase::Frenzy || _phase == Phase::Outro;
        AtBeat(Grid, CurrentBeat(Grid) + ToolBeats, [this, turn, frenzy]()
        {
            if (_phase == Phase::Traque || CurrentBeat(Grid) < _setPieceUntil)
                return;
            if (frenzy)
            {
                // Taillade every 4 bars, a tool every 8: traps, then an archer
                if (turn % 4 == 0)
                    Traps();
                else if (turn % 4 == 2)
                    Archer();
                return;
            }
            switch (turn % 4)
            {
                case 0: Traps(); break;
                case 1: Archer(); break;
                case 2: Net(); break;
                case 3: Horn(2); break;
            }
        });
    }

    // --- Taillade: his tank's cone, the tank swap --------------------------------------------------------------------
    void Taillade()
    {
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (!tank)
            return;
        CastBar(CAST_TAILLADE);
        me->SetFacingToObject(tank);
        float const orientation = me->GetAngle(tank);
        Position const apex = Ground(me->GetPosition());
        GroundIndicators::ShowAimedCone(me, apex, orientation, TailladeRadius, TailladeArc, TailladeCastMs, tank,
            GroundIndicators::Theme::None, MistakeDamage);
        ObjectGuid const tankGuid = tank->GetGUID();
        scheduler.Schedule(Milliseconds(TailladeCastMs), [this, apex, orientation, tankGuid](TaskContext)
        {
            EndCastBar();
            GroundIndicators::Area cone;
            cone.kind = GroundIndicators::Area::Kind::Cone;
            cone.origin = apex;
            cone.origin.SetOrientation(orientation);
            cone.radius = TailladeRadius;
            cone.arc = TailladeArc * float(M_PI) / 180.0f;
            for (Player* player : ArenaPlayers())
            {
                if (player->GetGUID() == tankGuid)
                {
                    Hit(player, SPELL_TAILLADE, TailladeTankPct);
                    AddTimedAura(player, SPELL_LACERATION, LacerationMs,
                        uint8(std::min(10, StacksOf(player, SPELL_LACERATION) + 1)));
                }
                else if (cone.Contains(player->GetPosition()))
                    Mistake(player, SPELL_TAILLADE, TailladeTankPct, "taillade");
            }
        });
    }

    // The tanks: him held at the hall's middle, and the swap - the tank with SwapStacks of Lacération leaves him to
    // the other (bots: GroundIndicators::SetBossHolder)
    void UpdateTanks()
    {
        if (_phase == Phase::Traque)
            return;
        GroundIndicators::SetTankSpot(me, Ground(GetRoom().center), 1000, 8.0f);
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        // The other tank: at his back, out of Taillade's cone, ready to take him
        if (tank)
            GroundIndicators::SetOffTankSpot(me, AtAngle(me->GetPosition(), me->GetAngle(tank) + float(M_PI), 4.0f),
                1000);
        if (!tank || StacksOf(tank, SPELL_LACERATION) < SwapStacks)
            return;
        for (Player* player : ArenaPlayers())
            if (player != tank && IsTank(player) && StacksOf(player, SPELL_LACERATION) < SwapStacks)
            {
                GroundIndicators::SetBossHolder(me, player, 8000);
                return;
            }
    }

    // --- Cor de chasse: a group hit and a pack ---------------------------------------------------------------------
    void Horn(uint32 hounds)
    {
        CastBar(CAST_HORN);
        Talk(SAY_HORN);
        scheduler.Schedule(1000ms, [this, hounds](TaskContext)
        {
            EndCastBar();
            HitEveryone(SPELL_HORN, HornPct);
            Pack(hounds);
        });
    }

    // Hounds from the hall's edge, on the tank he is not hitting (the second tank picks them up)
    void Pack(uint32 hounds)
    {
        Player* offTank = nullptr;
        for (Player* player : ArenaPlayers())
            if (IsTank(player) && player != me->GetVictim())
                offTank = player;
        float const base = frand(0.0f, 2.0f * float(M_PI));
        uint32 alive = 0;
        for (ObjectGuid const& guid : _summons)
            if (Creature* add = ObjectAccessor::GetCreature(*me, guid);
                add && add->IsAlive() && add->GetEntry() == NPC_FELHOUND)
                ++alive;
        hounds = std::min(hounds, MaxHounds > alive ? MaxHounds - alive : 0u);
        for (uint32 index = 0; index < hounds; ++index)
        {
            Position const spawn = EdgeSpot(FightRadius, base + float(index) * 0.5f);
            Creature* hound = me->SummonCreature(NPC_FELHOUND, spawn, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            if (!hound)
                continue;
            SizeAdd(hound, PackSeconds, true);
            hound->SetInCombatWithZone();
            if (Player* target = offTank ? offTank : ArrowTarget())
            {
                hound->GetThreatMgr().AddThreat(target, 1000000.0f);
                hound->AI()->AttackStart(target);
            }
        }
    }

    // --- Filet: a player held until the hound holding the net dies ---------------------------------------------------
    void Net()
    {
        if (!_netTarget.IsEmpty())
            return;
        Player* target = ArrowTarget();
        if (!target)
            return;
        Position const spawn = AtAngle(target->GetPosition(), target->GetOrientation(), 3.0f);
        Creature* hound = me->SummonCreature(NPC_FELHOUND, spawn, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
        if (!hound)
            return;
        SizeAdd(hound, NetHoundSeconds, false);
        hound->SetInCombatWithZone();
        hound->GetThreatMgr().AddThreat(target, 1000000.0f);
        hound->AI()->AttackStart(target);
        _netTarget = target->GetGUID();
        _netHound = hound->GetGUID();
        target->SetControlled(true, UNIT_STATE_ROOT);
        AddTimedAura(target, SPELL_NET, NetMs);
        Skull(hound);
        LOG_INFO(_logName, "Traqueur net instance={} at={:.1f}s on {}", me->GetInstanceId(), Elapsed() / 1000.0f,
                 target->GetName());
        ObjectGuid const houndGuid = hound->GetGUID();
        scheduler.Schedule(Milliseconds(NetMs), [this, houndGuid](TaskContext)
        {
            if (_netHound == houndGuid)
                FreeNet(true);
        });
    }

    void FreeNet(bool strangled)
    {
        Player* target = _netTarget.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*me, _netTarget);
        if (target)
        {
            target->SetControlled(false, UNIT_STATE_ROOT);
            target->RemoveAurasDueToSpell(SPELL_NET);
            if (strangled && target->IsAlive())
            {
                Hit(target, SPELL_STRANGLE, NetStranglePct);
                Broken("net", target->GetName());
            }
        }
        _netTarget.Clear();
        _netHound.Clear();
    }

    // --- Pièges à mâchoires: under players, then armed ---------------------------------------------------------------
    void Traps()
    {
        std::vector<Player*> players;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player))
                players.push_back(player);
        Acore::Containers::RandomResize(players, TrapCount);
        uint32 const now = Elapsed();
        for (Player* player : players)
        {
            Position const at = Ground(player->GetPosition());
            GroundIndicators::ShowCircle(me, at, TrapRadius, TrapWarnMs, GroundIndicators::Theme::None, MistakeDamage);
            _traps.push_back({ at, now + TrapWarnMs, now + TrapWarnMs + TrapArmedMs });
            scheduler.Schedule(Milliseconds(TrapWarnMs), [this, at](TaskContext)
            {
                GroundIndicators::ShowCircle(me, at, TrapRadius, TrapArmedMs, GroundIndicators::Theme::Fire,
                    MistakeDamage);
            });
        }
    }

    // An armed trap catches whoever steps in: a player (hit, held), or a hound (held, and wounded more)
    void CheckTraps()
    {
        if (_traps.empty())
            return;
        uint32 const now = Elapsed();
        std::erase_if(_traps, [now](Trap const& trap) { return now >= trap.until; });
        for (Trap& trap : _traps)
        {
            if (now < trap.armedAt || trap.sprung)
                continue;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(&trap.at) <= TrapRadius)
                {
                    trap.sprung = true;
                    Mistake(player, SPELL_TRAP, TrapPct, "trap");
                    player->SetControlled(true, UNIT_STATE_ROOT);
                    AddTimedAura(player, SPELL_JAWS, TrapRootMs);
                    ObjectGuid const guid = player->GetGUID();
                    scheduler.Schedule(Milliseconds(TrapRootMs), [this, guid](TaskContext)
                    {
                        if (Player* caught = ObjectAccessor::GetPlayer(*me, guid))
                            if (guid != _netTarget)
                                caught->SetControlled(false, UNIT_STATE_ROOT);
                    });
                    break;
                }
            if (trap.sprung)
                continue;
            for (ObjectGuid const& guid : _summons)
                if (Creature* hound = ObjectAccessor::GetCreature(*me, guid);
                    hound && hound->IsAlive() && hound->GetEntry() == NPC_FELHOUND &&
                    hound->GetExactDist2d(&trap.at) <= TrapRadius)
                {
                    trap.sprung = true;
                    hound->SetControlled(true, UNIT_STATE_ROOT);
                    AddTimedAura(hound, SPELL_JAWS, TrapHoundRootMs);
                    scheduler.Schedule(Milliseconds(TrapHoundRootMs), [this, guid](TaskContext)
                    {
                        if (Creature* caught = ObjectAccessor::GetCreature(*me, guid))
                            caught->SetControlled(false, UNIT_STATE_ROOT);
                    });
                    LOG_INFO(_logName, "Traqueur trap instance={} at={:.1f}s caught a hound", me->GetInstanceId(),
                             Elapsed() / 1000.0f);
                    break;
                }
        }
    }

    // --- An archer beater ----------------------------------------------------------------------------------------
    void Archer()
    {
        Position const spawn = EdgeSpot(FightRadius - 8.0f);
        Creature* archer = me->SummonCreature(NPC_ARCHER, spawn, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
        if (!archer)
            return;
        SizeAdd(archer, ArcherSeconds, false);
        archer->SetInCombatWithZone();
        archer->SetControlled(true, UNIT_STATE_ROOT);
        Skull(archer);
        LOG_INFO(_logName, "Traqueur archer instance={} at={:.1f}s spot=({:.1f}, {:.1f}, {:.1f}) {:.0f} yd out",
                 me->GetInstanceId(), Elapsed() / 1000.0f, spawn.GetPositionX(), spawn.GetPositionY(),
                 spawn.GetPositionZ(), spawn.GetExactDist2d(&GetRoom().center));
    }

    // The skull on an add the group is to kill first (the net's hound before an archer)
    void Skull(Creature* add)
    {
        Creature* current = _skulled.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*me, _skulled);
        if (current && current->IsAlive() && current->GetGUID() == _netHound)
            return;
        _skulled = add->GetGUID();
        SetSkull(add);
    }

    Creature* NextSkull()
    {
        for (ObjectGuid const& guid : _summons)
            if (Creature* add = ObjectAccessor::GetCreature(*me, guid);
                add && add->IsAlive() && (add->GetGUID() == _netHound || add->GetEntry() == NPC_ARCHER))
            {
                _skulled = add->GetGUID();
                return add;
            }
        return nullptr;
    }

    // --- La Battue: strips swept one a beat, a gap in each ------------------------------------------------------------
    // From the north wall south (and back, a second pass, the gaps elsewhere): each strip lands on its beat, shown
    // BattueWarnBeats ahead, its gap left open
    void Battue(bool back)
    {
        CastBar(CAST_BATTUE);
        Talk(SAY_BATTUE);
        scheduler.Schedule(2000ms, [this](TaskContext) { EndCastBar(); });
        uint32 const first = CurrentBeat(Grid) + BattueWarnBeats;
        _setPieceUntil = std::max(_setPieceUntil, first + (back ? 2 : 1) * (BattueStrips + BattueWarnBeats));
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
                AtBeat(Grid, lands - BattueWarnBeats, [this, x, gap, lands]()
                {
                    ShowStrip(lands, x, gap, Grid.At(lands) - Grid.At(lands - BattueWarnBeats));
                });
                AtBeat(Grid, lands, [this, lands]() { StrikeStrip(lands); });
            }
    }

    // A strip's two halves round its gap, warned for durationMs; it strikes what was drawn
    void ShowStrip(uint32 lands, float x, float gap, uint32 durationMs)
    {
        std::vector<GroundIndicators::Area>& strip = _strips[lands];
        float const west = GetRoom().center.GetPositionY() + BattueHalfWidth;
        float const east = GetRoom().center.GetPositionY() - BattueHalfWidth;
        float const halfGap = BattueGap / 2.0f;
        // A rectangle from start along its orientation: +y is the hall's west (orientation pi/2)
        if (gap - halfGap > east)
            strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, east, 0.0f)), float(M_PI) / 2.0f,
                gap - halfGap - east, BattueStripDepth, durationMs, GroundIndicators::Theme::None, MistakeDamage));
        if (west > gap + halfGap)
            strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, gap + halfGap, 0.0f)), float(M_PI) / 2.0f,
                west - gap - halfGap, BattueStripDepth, durationMs, GroundIndicators::Theme::None, MistakeDamage));
    }

    void StrikeStrip(uint32 lands)
    {
        auto const found = _strips.find(lands);
        if (found == _strips.end())
            return;
        for (Player* player : ArenaPlayers())
            for (GroundIndicators::Area const& half : found->second)
                if (half.Contains(player->GetPosition()))
                {
                    Mistake(player, SPELL_BATTUE, BattuePct, "battue");
                    break;
                }
        _strips.erase(found);
    }

    // --- L'Hallali: quarries away from everyone, the others together --------------------------------------------------
    void Hallali(uint32 quarries)
    {
        uint32 const lands = CurrentBeat(Grid) + HallaliMarkBeats;
        _setPieceUntil = std::max(_setPieceUntil, lands + 1);
        std::vector<Player*> candidates;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player))
                candidates.push_back(player);
        Acore::Containers::RandomResize(candidates, quarries);
        uint32 const durationMs = Grid.At(lands) - Elapsed();
        std::vector<ObjectGuid> marked;
        for (Player* quarry : candidates)
        {
            AddTimedAura(quarry, SPELL_QUARRY, durationMs);
            GroundIndicators::ShowCarriedCircle(me, quarry, HallaliRadius, durationMs, MistakeDamage, 2.0f);
            marked.push_back(quarry->GetGUID());
        }
        // The others' meeting point: on him, his tank in it
        Position const soak = Ground(me->GetPosition());
        // Everyone not a quarry is wanted in it (a bot that keeps a quarry's circle is never sent)
        uint32 const wanted = uint32(ArenaPlayers().size());
        GroundIndicators::ShowSoak(me, soak, HallaliSoakRadius, durationMs, wanted, GroundIndicators::Theme::Holy,
            true);
        AtBeat(Grid, lands - 10, [this]() { CastBar(CAST_HALLALI); });
        AtBeat(Grid, lands, [this, marked, soak]()
        {
            EndCastBar();
            StrikeHallali(marked, soak);
        });
    }

    void StrikeHallali(std::vector<ObjectGuid> const& marked, Position const& soak)
    {
        std::vector<Player*> quarries;
        for (ObjectGuid const& guid : marked)
            if (Player* quarry = ObjectAccessor::GetPlayer(*me, guid); quarry && quarry->IsAlive())
                quarries.push_back(quarry);
        std::vector<Player*> sharing;
        std::vector<Player*> astray;
        for (Player* player : ArenaPlayers())
        {
            if (std::find(quarries.begin(), quarries.end(), player) != quarries.end())
                continue;
            bool leapt = false;
            for (Player* quarry : quarries)
                if (player->GetExactDist2d(quarry) <= HallaliRadius)
                    leapt = true;
            if (leapt)
                Mistake(player, SPELL_HALLALI, HallaliMistakePct, "hallali leap");
            else if (player->GetExactDist2d(&soak) <= HallaliSoakRadius)
                sharing.push_back(player);
            else
                astray.push_back(player);
        }
        for (Player* quarry : quarries)
            Hit(quarry, SPELL_HALLALI, HallaliQuarryPct);
        float const share = HallaliSoakPct / float(std::max<size_t>(1, sharing.size()));
        for (Player* player : sharing)
            Hit(player, SPELL_PACK_SHARE, share);
        for (Player* player : astray)
            Mistake(player, SPELL_PACK_SHARE, HallaliMistakePct, "hallali astray");
        LOG_INFO(_logName, "Traqueur hallali instance={} at={:.1f}s quarries={} sharing={} astray={}",
                 me->GetInstanceId(), Elapsed() / 1000.0f, quarries.size(), sharing.size(), astray.size());
    }

    // --- La Traque: gone in the dark, torchlight pools to stand in ----------------------------------------------------
    void StartTraque()
    {
        _phase = Phase::Traque;
        Talk(SAY_TRAQUE);
        CastBar(CAST_TRAQUE);
        uint32 const start = CurrentBeat(Grid);
        scheduler.Schedule(2000ms, [this](TaskContext)
        {
            EndCastBar();
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            // Out of sight, not out of the fight: an invisible model (hidden, the players' combat with him dropped)
            me->SetDisplayId(InvisibleDisplay);
            me->NearTeleportTo(GetRoom().center.GetPositionX(), GetRoom().center.GetPositionY(),
                Ground(GetRoom().center).GetPositionZ(), me->GetOrientation());
        });
        // The pools: a set every TorchMoveBeats, each shown TorchPreviewBeats before its own (the first at once)
        uint32 const end = 416;
        for (uint32 set = start; set < end; set += TorchMoveBeats)
        {
            float const base = frand(0.0f, 2.0f * float(M_PI));
            std::vector<Position> pools;
            for (uint32 index = 0; index < TorchCount; ++index)
                pools.push_back(EdgeSpot(TorchDistance, base + float(index) * 2.0f * float(M_PI) / TorchCount));
            uint32 const shownAt = set > start + TorchPreviewBeats ? set - TorchPreviewBeats : start;
            uint32 const until = std::min(set + TorchMoveBeats, end);
            AtBeat(Grid, shownAt, [this, pools, shownAt, until]()
            {
                // The bots leave the old pools for the new at once (the old stay lit until they go)
                for (Position const& old : _soaked)
                    GroundIndicators::EndSoak(me, old);
                _soaked = pools;
                uint32 const durationMs = Grid.At(until) - Grid.At(shownAt);
                for (Position const& pool : pools)
                    GroundIndicators::ShowSoak(me, pool, TorchRadius, durationMs, 3, GroundIndicators::Theme::Holy,
                        true);
                _torches.push_back({ pools, Grid.At(shownAt), Grid.At(until) });
            });
        }
        // The strikes: half a strike's beats off the grid of moves, none while the pools change places
        for (uint32 offset = TraqueStrikeBeats + TraqueStrikeBeats / 2; start + offset < end;
             offset += TraqueStrikeBeats)
        {
            uint32 const sinceMove = offset % TorchMoveBeats;
            if (sinceMove >= TorchMoveBeats - TorchPreviewBeats)
                continue;
            AtBeat(Grid, start + offset, [this]() { StrikeTraque(); });
        }
        // Hounds hunting in the dark, on the healers
        for (uint32 beat : { start + 24, start + 56 })
            AtBeat(Grid, beat, [this]() { HuntingHounds(); });
    }

    void StrikeTraque()
    {
        uint32 const now = Elapsed();
        std::erase_if(_torches, [now](Torches const& torches) { return now >= torches.until; });
        for (Player* player : ArenaPlayers())
        {
            bool lit = false;
            for (Torches const& torches : _torches)
                for (Position const& pool : torches.pools)
                    if (player->GetExactDist2d(&pool) <= TorchRadius)
                        lit = true;
            if (!lit)
                Mistake(player, SPELL_TRAQUE, TraquePct, "traque");
        }
    }

    void HuntingHounds()
    {
        std::vector<Player*> healers;
        for (Player* player : ArenaPlayers())
            if (!IsTank(player))
                healers.push_back(player);
        Acore::Containers::RandomResize(healers, 2);
        for (Player* target : healers)
        {
            Position const spawn = EdgeSpot(FightRadius);
            Creature* hound = me->SummonCreature(NPC_FELHOUND, spawn, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            if (!hound)
                continue;
            SizeAdd(hound, HuntingSeconds, false);
            hound->SetInCombatWithZone();
            hound->GetThreatMgr().AddThreat(target, 1000000.0f);
            hound->AI()->AttackStart(target);
            _hunting.push_back(hound->GetGUID());
        }
    }

    // Back from the dark with his pack: the rebuild
    void EndTraque()
    {
        for (ObjectGuid const& guid : _hunting)
            if (Creature* hound = ObjectAccessor::GetCreature(*me, guid); hound && hound->IsAlive())
                hound->DespawnOrUnsummon(0ms);
        _hunting.clear();
        _torches.clear();
        _soaked.clear();
        me->RestoreDisplayId();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        _phase = Phase::Rebuild;
        Horn(4);
        Archer();
    }

    // --- La Curée and the end of the hunt ---------------------------------------------------------------------------
    void Curee()
    {
        Talk(SAY_CUREE);
        CastBar(CAST_CUREE);
        GroundIndicators::WarnGroupDamage(me, Grid.At(630) - Elapsed());
        AtBeat(Grid, 630, [this]()
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
    // An avoidable hit: Débusqué, and a second under it is death
    void Mistake(Player* player, uint32 spellId, float percent, char const* what)
    {
        if (!player || !player->IsAlive())
            return;
        if (player->HasAura(SPELL_DEBUSQUE))
        {
            Doom(player, spellId);
            Broken(what, Acore::StringFormat("{} (second mistake)", player->GetName()));
            return;
        }
        Hit(player, spellId, percent);
        if (player->IsAlive())
            AddTimedAura(player, SPELL_DEBUSQUE, DebusqueMs);
        Broken(what, player->GetName());
    }

    void Broken(char const* what, std::string const& who)
    {
        LOG_INFO(_logName, "Traqueur rule broken instance={} at={:.1f}s {}: {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, what, who);
    }

    // Whoever fell since the last look (to the pack, an arrow, his blows): each death in the log once
    void WatchDeaths()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && !player->IsGameMaster())
            {
                bool const dead = !player->IsAlive();
                if (dead && !_fallen.count(player->GetGUID()))
                    RecordDeath(player);
                if (dead)
                    _fallen.insert(player->GetGUID());
                else
                    _fallen.erase(player->GetGUID());
            }
    }

    void RecordDeath(Player* player)
    {
        ++_deaths[player->GetName()];
        LOG_INFO(_logName, "Traqueur death instance={} at={:.1f}s {}", me->GetInstanceId(), Elapsed() / 1000.0f,
                 player->GetName());
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
        FreeNet(false);
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        SetSkull(nullptr);
        _skulled.Clear();
        _traps.clear();
        _torches.clear();
        _soaked.clear();
        _hunting.clear();
        _strips.clear();
        _timeline.clear();
        _deaths.clear();
        _fallen.clear();
        _setPieceUntil = 0;
        _next = 0;
        _rotation = 0;
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
        uint32 until;
        bool sprung = false;
    };

    struct Torches
    {
        std::vector<Position> pools;
        uint32 from;
        uint32 until;
    };

    Phase _phase = Phase::None;
    std::vector<Step> _timeline;
    size_t _next = 0;
    uint32 _rotation = 0;
    uint32 _nextCheckMs = 0;
    bool _musicOver = false;
    ObjectGuid _netTarget;
    ObjectGuid _netHound;
    ObjectGuid _skulled;
    std::vector<Trap> _traps;
    std::vector<Torches> _torches;
    std::vector<Position> _soaked;
    std::vector<ObjectGuid> _hunting;
    std::map<uint32, std::vector<GroundIndicators::Area>> _strips;
    std::map<std::string, uint32> _deaths;
    std::set<ObjectGuid> _fallen;
    uint32 _setPieceUntil = 0;
};

boss_escape_hunter* HunterOf(Creature* add)
{
    TempSummon* summon = add ? add->ToTempSummon() : nullptr;
    Creature* hunter = summon ? summon->GetSummonerCreatureBase() : nullptr;
    return hunter ? dynamic_cast<boss_escape_hunter*>(hunter->AI()) : nullptr;
}

// His pack: the felhounds (melee, their bites floored as his own blows are) and the archer beaters (rooted at the
// hall's edge, an arrow every ArcherEveryMs at anyone but a tank: its 4 s cast to interrupt)
struct npc_escape_hunter_pack : public ScriptedAI
{
    npc_escape_hunter_pack(Creature* creature) : ScriptedAI(creature)
    {
        // An archer shoots from where it stands: no chase (a rooted chase that cannot reach evades, immune)
        if (me->GetEntry() == NPC_ARCHER)
            me->SetCombatMovement(false);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->DespawnOrUnsummon(0ms);
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type != DIRECT_DAMAGE)
            return;
        if (boss_escape_hunter* hunter = HunterOf(me))
            hunter->PackDamage(me, victim, damage);
    }

    // A hound held in a trap takes more
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (me->HasAura(SPELL_JAWS))
            damage = uint32(float(damage) * (1.0f + TrappedHoundTakenPct / 100.0f));
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (boss_escape_hunter* hunter = HunterOf(me))
            hunter->PackDied(me);
    }

    void SpellHitTarget(Unit* target, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_ARROW)
            if (boss_escape_hunter* hunter = HunterOf(me))
                hunter->ArrowHit(target);
    }

    void UpdateAI(uint32 diff) override
    {
        if (me->GetEntry() == NPC_ARCHER)
        {
            if (!me->IsInCombat() || me->HasUnitState(UNIT_STATE_CASTING))
                return;
            _arrowTimer = _arrowTimer > diff ? _arrowTimer - diff : 0;
            if (_arrowTimer)
                return;
            _arrowTimer = ArcherEveryMs;
            if (boss_escape_hunter* hunter = HunterOf(me))
                if (Player* target = hunter->ArrowTarget())
                    me->CastSpell(target, SPELL_ARROW, false);
            return;
        }
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }

private:
    uint32 _arrowTimer = 2000;
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .traqueur info | skip <seconds> | cast <step> | pull | room: for game masters trying the fight
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

    // .traqueur cast <horn|net|rotation|battue|battueonce|hallali|curee>
    static bool HandleCast(ChatHandler* handler, std::string step)
    {
        boss_escape_hunter* hunter = FindHunter(handler->GetPlayer());
        if (!hunter || !hunter->CastNow(step))
        {
            handler->SendErrorMessage("No such step, or the Traqueur is not fighting within 250 yards.");
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
    RegisterCreatureAI(npc_escape_hunter_pack);
    new EscapeHunterCommandScript();
}
