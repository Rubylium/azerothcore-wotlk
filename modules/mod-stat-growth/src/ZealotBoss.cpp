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
#include <functional>
#include <map>
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
// - Lances de lumière: a grid of lines wall to wall, then the grid shifted half a square.
// **The lights**: every few seconds a soak of light falls somewhere in the arena (the retail soak's look: it shows when
// someone stands in it). Whoever stands in it as it lands takes its light (Lumière dérobée: 3% damage and healing a
// stack - the temptation); left alone, the light flies to him and fills his power bar. At the end of the track, only if
// every light reached him does he ascend (phase 2); one taken, and the jailers come for him: he is beaten, and
// leaves 1 gold on the floor, no gear. A light left alone hits everyone for LightReturnPct of their health.
// Phase 2 (to come) is for now a bare fight: no mechanic, its own health, its music - a win as any other.
// Health paced (Defi::BossAI StartPace): 1% only at the end; above it then, the light floods the arena.
// Red shapes while the design is tried (art later); nothing stays on the floor once a pattern has struck (the room is
// read fast: its sounds mark the hits).
//
// .valerian lab: in the FX lab (map 451) he stands at its middle in rehearsal - every pattern drawn for real by
// .valerian cast, striking nobody, each strike logging how much of the arena it left safe.

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As the Gaol's other gates (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can
// be hit the whole phase (to its end, beat JudgementBeat), part of that time on him (OnHimShare: the dance)
constexpr float OnHimShare = 0.7f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, 127.4f * OnHimShare, 1.095f / 1.18f };
// Phase 2 while it is bare (no mechanic): its own health, the seconds it lasts against the profile's group
Defi::Sizing const AscendedSizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, 60.0f, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("valerian.health_scale", 1.0f);
constexpr float HoldPct = 1.0f;

// Keli'dan's hall in the Blood Furnace, its middle (ForgeMaster.cpp's Room: the same arena); the arena round it: a ring
// of WallPieces of Vorhan's wall pieces (GroundIndicators::WardenWallPieceLength each), ArenaRadius from the middle
// reach: the whole hall (a group standing back at the landing spot is drawn in by the wall); its music, the instance
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 1000.0f, 60.0f };
constexpr uint32 WallPieces = 16;
float const ArenaRadius = GroundIndicators::WardenWallPieceLength / (2.0f * std::sin(float(M_PI) / float(WallPieces)));
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
constexpr float FervorStackPct = 150.0f;    // Ferveur partagée: shared by those under it
constexpr float AscendPct = 30.0f;          // the light pouring out of him as he rises
constexpr float AscendKnockback = 7.0f;     // ... throwing everyone back (never as far as the wall)
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
constexpr uint32 FervorFlipBeats = 3;       // ... and turn to a stack this long before
constexpr float RageInnerShare = 0.4f;      // Rage contenue: the inside out to this share of the arena (a ring's own)
constexpr uint32 RageFollowBeats = 3;
constexpr float LanceWidth = 2.5f;
constexpr float LanceSpacing = 8.0f;
constexpr uint32 LanceShiftBeats = 3;
constexpr uint32 BurstCastBeats = 3;

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

// The end of the phase: the jailers walk in, then the mission ends
constexpr uint32 Jailers = 3;
constexpr uint32 JailersInMs = 2000;
constexpr uint32 JailerLineMs = 7000;
constexpr uint32 DeniedMs = 11000;      // the jailers' line said: he is beaten
constexpr uint32 AscendMs = 3000;

// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

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
    SAY_JAILER              = 0,        // the jailer's own
};

enum class Phase : uint8
{
    None,
    Zealot,         // phase 1
    Chained,        // the light stolen: he kneels, the jailers come
    Ascending,      // every light in him: he rises
    Ascended,       // phase 2 (bare for now)
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
        { 39, Event::Rage, 0 },
        { 51, Event::Fervor, 0 },
        { 63, Event::Revers },                  // on the 36.0 s hit
        { 77, Event::Lances },                  // on the 42.9 s hit
        { 87, Event::Chain },
        { 102, Event::Wrath, 1 },
        { 115, Event::Fervor, 1 },              // on the 62.7 s hit, turned to a stack
        { 130, Event::Rage, 1 },                // on the 69.6 s hit, out then in
        { 141, Event::Burst },                  // on the 73.5 s hit
        { 146, Event::Lances },
        { 157, Event::Chain },
        { 173, Event::Revers },
        { 183, Event::Wrath, 1 },
        { 195, Event::Fervor, 2 },
        { 205, Event::Rage, 2 },
        { 216, Event::Lances },
        { 227, Event::Chain },
        { 241, Event::Revers },
        { 250, Event::Judgement },              // on the 129.3 s hit
    };
}

// What a mistake costs (a share of the victim's own health; 100: death), and its name in the log
// sound: played as it strikes (client-assets/audio/valerian.json), from the area nearest each listener
struct Rule
{
    uint32 spell;
    float percent;
    char const* name;
    char const* sound = nullptr;
};

Rule const RuleRevers = { SPELL_REVERS, DodgePct, "revers", "Valerian.Revers" };
Rule const RuleWrath = { SPELL_WRATH, DodgePct, "wrath", "Valerian.Wrath" };
Rule const RuleChain = { SPELL_CHAIN, DodgePct, "chain", "Valerian.Chain" };
Rule const RuleFervor = { SPELL_FERVOR, DodgePct, "fervor" };
Rule const RuleRage = { SPELL_RAGE, DodgePct, "rage", "Valerian.Rage" };
Rule const RuleLances = { SPELL_LANCES, DodgePct, "lances", "Valerian.Lances" };

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
            SetModelHealth();
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
        SetModelHealth();
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
            return;
        if (_phase != Phase::Zealot)
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
        if (_phase == Phase::Rehearsal)
        {
            scheduler.Update(diff);
            UpdateLights(diff);
            return;
        }
        if (Fighting())
            WatchDeaths();
        // Kneeling, rising: no one to hit, his steps still go on (the jailers, the end; phase 2)
        if (_phase == Phase::Chained || _phase == Phase::Ascending)
        {
            scheduler.Update(diff);
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
        if (elapsed >= MusicEndMs && !_musicOver)
        {
            _musicOver = true;
            EndMusic();
        }
        if (_phase == Phase::Ascended && !me->HasUnitState(UNIT_STATE_CASTING))
            DoMeleeAttackIfReady();
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
    bool CastNow(std::string const& what)
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return false;
        uint32 const now = CurrentBeat(Grid) + 1;
        if (what == "lights")
        {
            StartLights(now, now + 60);
            return true;
        }
        static std::map<std::string, Step> const steps = {
            { "revers", { 0, Event::Revers } }, { "wrath", { 0, Event::Wrath, 0 } },
            { "wrath2", { 0, Event::Wrath, 1 } }, { "chain", { 0, Event::Chain } },
            { "fervor", { 0, Event::Fervor, 0 } }, { "fervor2", { 0, Event::Fervor, 1 } },
            { "rage", { 0, Event::Rage, 0 } }, { "rage2", { 0, Event::Rage, 1 } },
            { "lances", { 0, Event::Lances } }, { "burst", { 0, Event::Burst } },
            { "judgement", { 0, Event::Judgement } },
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
    Defi::Sizing const& GetSizing() const override { return _phase == Phase::Ascended ? AscendedSizing : Sizing; }
    float HealthScale() const override { return float(::HealthScale); }
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
            _phase == Phase::Ascended;
    }

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
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return;
        std::vector<Position> spots;
        for (GroundIndicators::Area const& area : areas)
            spots.push_back(BurstSpot(area));
        if (rule.sound)
            SoundNearest(rule.sound, spots);
        if (_phase == Phase::Rehearsal)
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

    // A cone from the middle to the wall, arcDegrees wide, around facing
    GroundIndicators::Area Cone(float facing, float arcDegrees, uint32 ms, Unit* aimedAt = nullptr)
    {
        Position apex = Middle();
        apex.SetOrientation(facing);
        if (aimedAt)
            return GroundIndicators::ShowAimedCone(me, apex, facing, ArenaRadius + 1.0f, arcDegrees, ms, aimedAt,
                GroundIndicators::Theme::None, MistakeDamage);
        return GroundIndicators::ShowCone(me, apex, facing, ArenaRadius + 1.0f, arcDegrees, ms,
            GroundIndicators::Theme::None, MistakeDamage);
    }

    // --- The arena ---------------------------------------------------------------------------------------------------
    // Its wall rises round the middle (Vorhan's pieces, WallPieces of them: ArenaRadius) and kills from WallRiseBeats on.
    // For the bots, everything past it is one area to keep out of, all fight.
    void RaiseArena()
    {
        Position const middle = Middle();
        uint32 const lasts = MusicEndMs + 30000;
        uint32 const shows = _phase == Phase::Rehearsal ? 0 : WallShowBeats;
        AtBeat(Grid, shows, [this, middle, lasts]()
        {
            GroundIndicators::ShowCurtainRing(me, middle, GroundIndicators::SPELL_WARDEN_WALL_FIRST,
                GroundIndicators::WardenWallLooks, WallPieces, GroundIndicators::WardenWallPieceLength, lasts);
            Sound("Valerian.WallRise", middle);
        });
        _wallFrom = Grid.At(shows + WallRiseBeats) + WallDrawInGraceMs;
        AtBeat(Grid, shows + WallRiseBeats, [this]() { DrawIn(); });
        KeepBotsIn(lasts);
    }

    void KeepBotsIn(uint32 lastsMs)
    {
        GroundIndicators::Area outside;
        outside.kind = GroundIndicators::Area::Kind::Ring;
        outside.origin = Middle();
        outside.radius = 60.0f;
        outside.inner = ArenaRadius - WallMargin - 0.5f;
        GroundIndicators::WatchArea(me, outside, lastsMs, MistakeDamage);
    }

    // Everyone past the wall set just inside it, where they stood round it
    void DrawIn()
    {
        if (_phase != Phase::Zealot)
            return;
        Position const middle = Middle();
        for (Player* player : ArenaPlayers())
            if (player->GetExactDist2d(&middle) > ArenaRadius - WallMargin)
            {
                Position const in = AtAngle(middle, middle.GetAngle(player), ArenaRadius - 3.0f);
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
            if (player->GetExactDist2d(&middle) > ArenaRadius - WallMargin)
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
        if (StacksOf(tank, SPELL_ZEAL) < SwapStacks)
            return;
        for (Player* player : ArenaPlayers())
            if (player != tank && IsTank(player) && StacksOf(player, SPELL_ZEAL) < SwapStacks)
            {
                GroundIndicators::SetBossHolder(me, player, 8000);
                return;
            }
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

    // Somewhere in the arena, away from him and from the wall, on another side than the last one
    void ShowLight(uint32 lands)
    {
        if (_phase != Phase::Zealot && _phase != Phase::Rehearsal)
            return;
        float const angle = Position::NormalizeOrientation(_lastLightAngle + frand(float(M_PI) / 2.0f,
            3.0f * float(M_PI) / 2.0f));
        _lastLightAngle = angle;
        Light light;
        light.at = OnArena(angle, frand(LightNear, ArenaRadius - LightWallGap));
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

    void ShowAbsorbed()
    {
        float const share = std::min(1.0f, float(_absorbed) / float(LightCount));
        me->SetMaxPower(POWER_MANA, 100);
        me->SetPower(POWER_MANA, uint32(share * 100.0f));
        me->SetObjectScale(1.0f + 0.25f * share);
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
        Sound("Valerian.ReversCast", me->GetPosition());
        float const facing = tank ? me->GetAngle(tank) : me->GetOrientation();
        if (tank)
            me->SetFacingToObject(tank);
        ObjectGuid const tankGuid = tank ? tank->GetGUID() : ObjectGuid::Empty;
        uint32 const lands = start + WarnBeats;
        GroundIndicators::Area const front = Cone(facing, ReversArc, MsTo(lands), tank);
        AtBeat(Grid, lands - 1, [this, front]() { Probe({ front }, "revers"); });
        AtBeat(Grid, lands, [this, front, tankGuid]()
        {
            EndCastBar();
            Sound("Valerian.Revers", BurstSpot(front));
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
        // The same cone behind him, shown as the first strikes
        float const back = Position::NormalizeOrientation(facing + float(M_PI));
        Wave(lands, lands + ReversBackBeats, [this, back](uint32 ms)
        {
            return Areas{ Cone(back, ReversArc, ms) };
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
        // His fist raised over the half: its light where it will strike
        AtBeat(Grid, start, [this, facing]() { Fist(facing); });
        AtBeat(Grid, first, [this, second]() { Fist(second); });
        Wave(start, first, [this, facing](uint32 ms) { return Areas{ Cone(facing, 180.0f, ms) }; }, RuleWrath);
        Wave(first, first + WrathFollowBeats, [this, second](uint32 ms) { return Areas{ Cone(second, 180.0f, ms) }; },
            RuleWrath);
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
        Sound("Valerian.ChainSpin", me->GetPosition());
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const turn = way * ChainTurnDegrees * float(M_PI) / 180.0f;
        float gap = frand(0.0f, 2.0f * float(M_PI));
        uint32 shown = start;
        uint32 lands = start + WarnBeats;
        for (uint32 strike = 0; strike < ChainStrikes; ++strike, gap += turn)
        {
            Wave(shown, lands, [this, gap](uint32 ms) { return Sweep(gap, ms); }, RuleChain);
            shown = lands;
            lands += ChainStepBeats;
        }
        AtBeat(Grid, shown, [this]() { EndCastBar(); });
    }

    // Everything round him but ChainGapDegrees around gap: a half and two eighths
    Areas Sweep(float gap, uint32 ms)
    {
        float const half = float(M_PI) / 2.0f;
        float const eighth = float(M_PI) / 4.0f;
        float const opposite = gap + float(M_PI);
        static_assert(ChainGapDegrees == 90.0f, "the sweep is a half and two eighths");
        return Areas{ Cone(Position::NormalizeOrientation(opposite), 180.0f, ms),
                      Cone(Position::NormalizeOrientation(opposite - half - eighth / 2.0f), 45.0f, ms),
                      Cone(Position::NormalizeOrientation(opposite + half + eighth / 2.0f), 45.0f, ms) };
    }

    // --- Ferveur: three marked to spread - turned to a stack at the last moment ------------------------------------------
    // twist 0: a spread; 1: turned to a stack FervorFlipBeats before it explodes; 2: either, at random
    void Fervor(uint32 start, uint32 twist)
    {
        CastBar(CAST_FERVOR);
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
            GroundIndicators::ShowCarriedCircle(me, carrier, FervorRadius, MsTo(stack ? flips : lands),
                MistakeDamage);
        }
        std::vector<Position> marked;
        for (Player* carrier : carriers)
            marked.push_back(carrier->GetPosition());
        SoundNearest("Valerian.FervorMark", marked);
        if (_phase == Phase::Rehearsal && carriers.empty())
            LogSafe("fervor (no carrier in the lab)", {});
        if (stack)
            AtBeat(Grid, flips, [this, marks, lands]()
            {
                // The marks turn: a soak on each, its own colour, the others to come under them
                Sound("Valerian.FervorFlip", Middle());
                for (ObjectGuid const& guid : marks)
                    if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                        GroundIndicators::ShowSoak(me, carrier->GetPosition(), FervorRadius, MsTo(lands),
                            uint32(ArenaPlayers().size() / FervorMarks), GroundIndicators::Theme::Holy);
                FollowStacks(marks, lands);
            });
        AtBeat(Grid, lands, [this, marks, stack]()
        {
            std::vector<Position> bursts;
            for (ObjectGuid const& guid : marks)
                if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                    bursts.push_back(carrier->GetPosition());
            SoundNearest("Valerian.Fervor", bursts);
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

    // Each bot to the nearest mark (the marked hold still), until it lands
    void FollowStacks(std::vector<ObjectGuid> const& marks, uint32 lands)
    {
        if (CurrentBeat(Grid) >= lands || _phase == Phase::Over)
            return;
        uint32 const ms = MsTo(lands);
        std::vector<Player*> carriers;
        for (ObjectGuid const& guid : marks)
            if (Player* carrier = ObjectAccessor::GetPlayer(*me, guid))
                carriers.push_back(carrier);
        if (carriers.empty())
            return;
        for (Player* player : ArenaPlayers())
        {
            Player* nearest = carriers.front();
            for (Player* carrier : carriers)
                if (player->GetExactDist2d(carrier) < player->GetExactDist2d(nearest))
                    nearest = carrier;
            if (player != nearest && !IsTank(player))
                GroundIndicators::SetUnitSpot(me, player, nearest->GetPosition(), FervorRadius - 2.0f, ms);
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
            return Areas{ GroundIndicators::ShowCircle(me, Middle(), inner, ms, GroundIndicators::Theme::None,
                MistakeDamage) };
        };
        auto out = [this, outer, inner](uint32 ms)
        {
            return Areas{ GroundIndicators::ShowRing(me, Middle(), outer, inner, ms, GroundIndicators::Theme::None,
                MistakeDamage) };
        };
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
        AtBeat(Grid, first, [this]() { EndCastBar(); });
        float const turn = frand(0.0f, float(M_PI) / 2.0f);
        Wave(start, first, [this, turn](uint32 ms) { return Grille(turn, LanceSpacing / 2.0f, ms); }, RuleLances);
        Wave(first, first + LanceShiftBeats, [this, turn](uint32 ms) { return Grille(turn, 0.0f, ms); }, RuleLances);
    }

    // Lines LanceSpacing apart both ways (turned by turn), the first offset from the middle: each from wall to wall
    Areas Grille(float turn, float offset, uint32 ms)
    {
        Areas lines;
        Position const middle = Middle();
        float const reach = ArenaRadius + 1.0f;
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
                lines.push_back(GroundIndicators::ShowRectangle(me, startAt, along, 2.0f * half, LanceWidth, ms,
                    GroundIndicators::Theme::None, MistakeDamage));
            }
        }
        return lines;
    }

    // --- Éclat de zèle: a group hit on the music's hit ------------------------------------------------------------------
    void Burst(uint32 start)
    {
        uint32 const lands = start + BurstCastBeats;
        CastBar(CAST_BURST);
        GroundIndicators::WarnGroupDamage(me, MsTo(lands));
        AtBeat(Grid, lands, [this]()
        {
            EndCastBar();
            Sound("Valerian.Burst", Middle());
            if (_phase == Phase::Zealot)
                HitEveryone(SPELL_BURST, BurstGroupPct);
        });
    }

    // --- The end of the phase: the check, then the lights' verdict -------------------------------------------------------
    void Judgement(uint32 start)
    {
        uint32 const strikes = std::max(start + 2, JudgementBeat);
        CastBar(CAST_JUDGEMENT);
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
                Ascend();
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
        EndMusic();
        Talk(SAY_STOLEN);
        Sound("Valerian.Chained", Middle());
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetImmuneToAll(true);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
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
            me->SetImmuneToAll(false);
            me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            std::vector<Player*> const players = ArenaPlayers();
            Unit* credit = players.empty() ? nullptr : players.front();
            Unit::Kill(credit ? credit : me, me);
        });
    }

    // Every light in him: he rises off the floor, the light pours out of him (everyone thrown back), then phase 2 - bare
    // for now: his own health, his music, his melee, no mechanic
    void Ascend()
    {
        _phase = Phase::Ascending;
        ClearLights();
        GroundIndicators::ClearAreasOf(me);
        Talk(SAY_ASCEND);
        Sound("Valerian.Ascend", Middle());
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetCanFly(true);
        me->SetDisableGravity(true);
        Position up = Middle();
        up.m_positionZ += 4.0f;
        me->GetMotionMaster()->MoveTakeoff(1, up, 2.0f);
        Position const middle = Middle();
        for (uint32 index = 0; index < 8; ++index)
            GroundIndicators::Burst(me, OnArena(float(index) * float(M_PI) / 4.0f, 6.0f), GroundIndicators::Theme::Holy);
        for (Player* player : ArenaPlayers())
        {
            Hit(player, SPELL_ASCEND, AscendPct);
            player->KnockbackFrom(middle.GetPositionX(), middle.GetPositionY(), AscendKnockback, 6.0f);
        }
        scheduler.Schedule(Milliseconds(AscendMs), [this](TaskContext) { StartAscended(); });
    }

    // Phase 2, bare: back on the floor at the middle, his health anew, his music
    void StartAscended()
    {
        EndPace();
        _phase = Phase::Ascended;
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        Position const middle = Middle();
        me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(), me->GetOrientation());
        me->SetControlled(true, UNIT_STATE_ROOT);
        SetModelHealth();
        me->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(me, GetRoom().reach);
        StartMusic("Music.ValerianAscended");
        LOG_INFO(_logName, "Valerian ascended instance={} health={}", me->GetInstanceId(), me->GetMaxHealth());
    }

    // --- Mistakes ----------------------------------------------------------------------------------------------------
    // A share of the victim's own health (100: death); a second under Ébloui is death
    void Mistake(Player* player, Rule const& rule)
    {
        if (!player || !player->IsAlive())
            return;
        if (rule.percent >= 100.0f || player->HasAura(SPELL_DAZZLED))
            Doom(player, rule.spell);
        else
        {
            HitOwnHealth(player, rule.spell, rule.percent);
            if (player->IsAlive())
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
        _spawned = 0;
        _absorbed = 0;
        _taken = 0;
        _phase = Phase::None;
        me->SetVisible(DefiConfirmed());
        me->SetImmuneToAll(false);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        me->SetObjectScale(1.0f);
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
    std::vector<Light> _lights;
    std::vector<ObjectGuid> _jailers;
    uint32 _lightTimer = 0;
    float _lastLightAngle = 0.0f;
    uint32 _spawned = 0;
    uint32 _absorbed = 0;
    uint32 _taken = 0;
    std::map<std::string, uint32> _deaths;
    std::set<ObjectGuid> _fallen;
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .valerian info | skip <seconds> | cast <pattern> | lab [off] | pull | measure
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

    // .valerian cast <revers|wrath|wrath2|chain|fervor|fervor2|rage|rage2|lances|burst|judgement|lights>
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
