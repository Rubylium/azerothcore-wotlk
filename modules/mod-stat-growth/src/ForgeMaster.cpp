#include "DefiBoss.h"
#include "EvolutionsAudio.h"
#include "GroundIndicators.h"

#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "GameObject.h"
#include "LFG.h"
#include "Random.h"
#include "StringFormat.h"

#include <array>
#include <functional>
#include <map>
#include <set>
#include <string>

// Vrogar, Maître-fondeur de la Geôle: the Hellfire Gaol's second gate, a Défi board boss in the Blood Furnace, in
// Keli'dan the Breaker's hall (plan: .agents/plans/escape-hunter, "The fight v3"). He casts the chains the Gaol's
// prisoners wear. 8 players - 2 tanks, 2 healers, 4 damage dealers - tuned for item level 477 and 650 paragon, as
// Gardien-chef Vorhan, the first gate.
//
// He stands at the arena's middle all fight, rooted (HoldMiddle): he leaves it only for la Trempe's walk to a prop.
//
// **A walled arena on his music.** A few seconds into the pull a ring of wall rises round the hall's middle: the
// wall kills on contact, and every pattern covers the whole arena or targets every player - nothing is outranged.
// Every step lands on a beat of his track (its drums are his hammer), two layers in turns, never on top of each
// other:
// - the WoW layer, dodges while he is tanked, one at a time in the same cycle every pull: Coup d'enclume (a cone on his
//   tank), l'Onde de choc (three rings round him shown together, struck from the middle out), Coulée (half the arena, then the other half),
//   Étincelles (a circle under everyone), Chaînes (chains through him to the wall spin, freeze, strike), le Laminoir
//   (rollers sweep the arena strip by strip, a gap in each); group hits on the music's big hits;
// - the FFXIV layer, puzzles, nothing else on the floor -
//   la Trempe (he dips his hammer in the quench trough or the crucible at the arena's edge: steam bursts in the middle,
//   or iron floods the outer ring, two bars after he is back; later both, in that order), les Soufflets (two adjacent
//   bellows of four glow, each then blasts the half on its side: the quarter opposite both), les Lingots (four ingots
//   to be held by exactly two each), le Marteau (two marked to the wall, the others share his blow under him), la Fonte
//   (the intermission: he is in his furnace, the floor in eight slices, all but two burning each bar, the safe pair
//   going round, then jumping across), la Coulée finale (the check, on the climax).
// - **Paced health** (Defi::BossAI StartPace: the Hollow Voice's first phase): the bar comes down with the music, 1%
//   only at the check; above 1% true health then, the arena floods.
// Mistakes cost a share of the victim's own health (DodgePct; a puzzle missed kills): they matter at any gear. Nothing
// is written on the screen; existing visuals while the design is tried (art later).
//
// .vrogar lab: in the FX lab (map 451) he stands at its middle in rehearsal - every pattern drawn for real by .vrogar
// cast, striking nobody, each strike logging how much of the arena it left safe.

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// As Vorhan's profile (mod-playerbots ChallengeTiers.h): 477 / 650, 2 tanks, 2 healers, 4 damage dealers. He can be hit
// from the pull to the check (4:04.0) but during la Fonte (2:04.0-2:41.2), part of that time on him (OnHimShare).
constexpr float OnHimShare = 0.66f;
Defi::Sizing const Sizing = { 477.0f, 650.0f, 4.0f, 2.0f, 2.0f, (244.0f - 37.2f) * OnHimShare, 1.095f / 1.18f };
LiveTuning::Knob const HealthScale("vrogar.health_scale", 1.0f);
constexpr float HoldPct = 1.0f;

// Keli'dan's hall, its middle; the arena round it: a ring of WallPieces of Vorhan's wall pieces
// (GroundIndicators::WardenWallPieceLength each), ArenaRadius from the middle
// reach: the whole hall (a group standing back at the landing spot is drawn in by the wall); its music, the instance
Defi::Room const Room = { Position(326.5f, -86.0f, -24.6f, 0.0f), 55.0f, 1000.0f, 60.0f };
constexpr uint32 WallPieces = 16;
float const ArenaRadius = GroundIndicators::WardenWallPieceLength / (2.0f * std::sin(float(M_PI) / float(WallPieces)));
constexpr float WallMargin = 0.6f;          // past ArenaRadius less this: in the wall
constexpr uint32 WallShowBeats = 8;         // the wall rises this long after the pull
constexpr uint32 WallRiseBeats = 8;         // and kills this long after it rose (time to step in)

// The FX lab (map 451, FxLab.cpp): his rehearsal's place
constexpr uint32 MapFxLab = 451;
Position const LabMiddle(2933.333f, 800.0f, 0.0f, 0.0f);

// --- The music ------------------------------------------------------------------------------------------------------
// prison_first_boss_music_2 (Music.EscapeHunter): 155 BPM, a line fitted to its 684 beats. A bar is 4 beats, its snare
// on the third. Sections start on bars: the build-up on bar 12, section A on 28, the peak on 76 (beat 310), the
// breakdown on 80, the rebuild on 104 (beat 416: the track's hardest hit), section B on 116, the climax on 156 (beat
// 630). Other hits: beats 55, 182, 234, 468, 517, 533, 562, 575. The track ends at MusicEndMs.
constexpr Defi::BeatGrid Grid = { 168.2f, 387.09f };
constexpr uint32 MusicEndMs = 265460;
constexpr uint32 FonteStartBeat = 320;
constexpr uint32 FonteEndBeat = 416;
constexpr uint32 CheckBeat = 630;

// --- His blows ------------------------------------------------------------------------------------------------------
// Profile shares (a damage dealer of the profile): his blows on the tank, the group hits
constexpr float MeleeFloorPct = 10.0f;
constexpr float AnvilTankPct = 50.0f;       // Coup d'enclume on his tank, and a stack of Brûlure
constexpr float BurnTakenPct = 15.0f;       // Brûlure: more taken a stack, for BurnMs; at SwapStacks the swap
constexpr uint32 BurnMs = 25000;
constexpr uint8 SwapStacks = 3;
constexpr float HammerGroupPct = 18.0f;     // Frappe de l'enclume, on the music's hits
constexpr float ReturnPct = 30.0f;          // his return from the furnace
constexpr float IngotPairPct = 30.0f;       // an ingot held by two: each
constexpr float IngotFailPct = 40.0f;       // an ingot held by fewer: everyone, for each
constexpr float HammerSharePct = 160.0f;    // le Marteau's blow under him, shared
constexpr float CheckHitPct = 40.0f;        // the check passed: everyone
// The victim's own health: a dodge missed
constexpr float DodgePct = 75.0f;
constexpr float MarkedPct = 75.0f;          // le Marteau on a marked player
// A mistake: Ébouillanté for ScaldMs - a second mistake under it kills
constexpr uint32 ScaldMs = 10000;

// The patterns' sizes (yards) and times (beats)
constexpr float AnvilArc = 90.0f;
constexpr float AnvilInner = 6.0f;          // the shockwave's rings: 0-6, 6-10, 10 to the wall
constexpr float AnvilMiddle = 10.0f;
constexpr uint32 AnvilRingBeats = 2;        // a ring strikes this long after the one inside it: time to step in
// A shape's first red: at least this long before it strikes (a follow-up of a pattern already read may come faster)
constexpr uint32 WarnBeats = 8;
constexpr uint32 FollowBeats = 6;
constexpr float SparkRadius = 4.0f;
constexpr float ChainWidth = 4.0f;
constexpr uint32 ChainTurnBeats = 8;
constexpr uint32 LaminoirStrips = 4;
constexpr float LaminoirDepth = 8.0f;
constexpr float LaminoirGap = 7.0f;
constexpr uint32 LaminoirWarnBeats = WarnBeats;
constexpr float SteamRadius = 7.0f;         // la Trempe: the trough's steam in the middle; the crucible's iron outside it
constexpr float PropDistance = 12.0f;       // the trough and the crucible from the middle
constexpr float BellowsDistance = 14.5f;
constexpr float IngotRadius = 3.0f;
constexpr float IngotDistance = 8.0f;
constexpr float MarkedRadius = 8.0f;
constexpr float HammerSoakRadius = 6.0f;
constexpr uint32 MarkBeats = 12;

// What a red area is told to the bots (GroundIndicators hitDamage): nothing known, so always left
constexpr uint32 MistakeDamage = 0;

enum Spells : uint32
{
    SPELL_ANVIL             = 94800,    // Coup d'enclume on his tank, and in the cone
    SPELL_HAMMER_GROUP      = 94801,    // Frappe de l'enclume
    SPELL_SHOCKWAVE         = 94802,    // Onde de l'enclume
    SPELL_POUR              = 94803,    // Coulée
    SPELL_SPARKS            = 94804,    // Étincelles
    SPELL_CHAINS            = 94805,    // Chaînes
    SPELL_LAMINOIR          = 94806,    // Laminoir
    SPELL_STEAM             = 94807,    // Vapeur (la Trempe, the trough)
    SPELL_FLOOD             = 94808,    // Coulée de fonte (la Trempe, the crucible)
    SPELL_BELLOWS           = 94809,    // Souffle du soufflet
    SPELL_INGOT_FAIL        = 94810,    // Lingot chu
    SPELL_INGOT_CRUSH       = 94811,    // Lingot écrasant (three or more under one)
    SPELL_INGOT_HOLD        = 94812,    // Lingot tenu
    SPELL_HAMMER_MARK       = 94813,    // Marteau (on a marked player, and near one)
    SPELL_HAMMER_SHARE      = 94814,    // Marteau partagé
    SPELL_FONTE             = 94815,    // Fonte (the intermission's floor)
    SPELL_RETURN            = 94816,    // Retour de la fournaise
    SPELL_CHECK             = 94817,    // Coulée finale
    SPELL_WALL              = 94818,    // Mur de fonte (the arena's wall)
    SPELL_BURN              = 94820,    // Brûlure (his tank's stacks)
    SPELL_SCALD             = 94821,    // Ébouillanté (a mistake)
    SPELL_MARKED            = 94822,    // Fer rouge (le Marteau's mark)
    CAST_ANVIL              = 94840,
    CAST_HAMMER_GROUP       = 94841,
    CAST_LAMINOIR           = 94842,
    CAST_HAMMER_MARK        = 94843,
    CAST_FONTE              = 94844,
    CAST_CHECK              = 94845,
    CAST_TREMPE             = 94846,
    CAST_BELLOWS            = 94847,
    CAST_INGOTS             = 94848,
    CAST_POUR               = 94849,
    CAST_CHAINS             = 94850,
    CAST_SPARKS             = 94851,
};

enum Npcs : uint32
{
    NPC_VROGAR              = 930400,
    // The props of the arena's edge: creatures wearing stock objects' models (their own displays, 60011-60013), so that
    // nobody is blocked by them - the quench trough (a cauldron), the crucible (a forge), four bellows (braziers)
    NPC_TROUGH              = 930401,
    NPC_CRUCIBLE            = 930402,
    NPC_BELLOWS             = 930403,
};

enum Says : uint8
{
    SAY_AGGRO               = 0,
    SAY_KILL                = 1,
    SAY_DEATH               = 6,
};

constexpr uint32 InvisibleDisplay = 11686;

enum class Phase : uint8
{
    None,
    Forge,          // the intro, the build-up, section A
    Fonte,          // 2:04.0, bar 80: the breakdown, he is in his furnace
    Quench,         // 2:41.2, bar 104: the rebuild and section B
    Over,
    Rehearsal,      // the FX lab
};

enum class Event : uint8
{
    Anvil,
    Shockwave,
    Pour,
    Sparks,
    Chains,
    Laminoir,       // arg 1: and back
    Trempe,         // arg: its dips (1, 2)
    Bellows,        // arg: its blasts (1, 2)
    Ingots,         // arg: its waves (1, 2)
    Hammer,
    HammerGroup,
    FonteStart,
    FonteEnd,
    Check,
};

char const* EventName(Event what)
{
    switch (what)
    {
        case Event::Anvil: return "anvil";
        case Event::Shockwave: return "shockwave";
        case Event::Pour: return "pour";
        case Event::Sparks: return "sparks";
        case Event::Chains: return "chains";
        case Event::Laminoir: return "laminoir";
        case Event::Trempe: return "trempe";
        case Event::Bellows: return "bellows";
        case Event::Ingots: return "ingots";
        case Event::Hammer: return "hammer";
        case Event::HammerGroup: return "hammer group";
        case Event::FonteStart: return "fonte start";
        case Event::FonteEnd: return "fonte end";
        case Event::Check: return "check";
    }
    return "?";
}

struct Step
{
    uint32 beat;    // where it starts
    Event what;
    uint32 arg = 0;
};

// The fight, on the music (the plan's v3 timeline). Each pattern ends before the next starts.
std::vector<Step> BuildTimeline()
{
    return {
        // Every pattern's first red shows WarnBeats (3.1 s) before it strikes; their lengths from their start: anvil 9
        // beats, shockwave 13, pour and chains 15, sparks 9, laminoir 12 (back: 19), bellows 13 (2: 23), ingots 13
        // (2: 25), hammer 17, trempe 25 (2: 39); none starts before the last one's last strike. The group hit draws
        // nothing on the floor. The wall rises on beat WallShowBeats.
        // Intro (bars 0-11): his dodges, learnt
        { 10, Event::Anvil }, { 20, Event::Shockwave }, { 34, Event::Sparks },
        // Build-up (bars 12-27): his hammer on its first beat; la Trempe, his dip on the 0:21.4 hit
        { 45, Event::HammerGroup }, { 49, Event::Trempe, 1 },
        { 75, Event::Anvil }, { 85, Event::Chains }, { 101, Event::Pour },
        // Section A (bars 28-75)
        { 117, Event::Shockwave }, { 132, Event::Bellows, 1 }, { 146, Event::Sparks }, { 156, Event::Anvil },
        { 165, Event::Sparks },
        { 174, Event::Laminoir },               // its rollers from the 1:10.6 hit (beat 182)
        { 188, Event::Anvil }, { 198, Event::Ingots, 1 }, { 212, Event::Sparks },
        { 226, Event::Laminoir, 1 },            // from the 1:30.7 hit (beat 234), and back
        { 246, Event::Shockwave }, { 260, Event::Pour }, { 276, Event::Chains },
        { 296, Event::Hammer },                 // landing on the peak (beat 310)
        // La Fonte (bars 80-103), his return on beat 416
        { FonteStartBeat, Event::FonteStart }, { FonteEndBeat, Event::FonteEnd },
        // The rebuild and section B (bars 104-155)
        { 422, Event::Anvil }, { 432, Event::Shockwave }, { 446, Event::Sparks },
        { 460, Event::Laminoir },               // from the 3:01.3 hit (beat 468)
        { 473, Event::Sparks }, { 484, Event::Trempe, 2 },
        { 514, Event::HammerGroup },            // on the 3:20.3 hit (beat 517)
        { 521, Event::Ingots, 2 },              // its first wave on the 3:26.5 hit (beat 533)
        { 554, Event::Laminoir },               // from the 3:37.7 hit (beat 562)
        { 566, Event::Sparks },
        { 575, Event::Bellows, 2 },             // on the 3:42.7 hit
        { 599, Event::Anvil },
        { 612, Event::Check },                  // landing on the climax (beat 630)
    };
}

// What a mistake costs (a share of the victim's own health; 100: death), and its name in the log
struct Rule
{
    uint32 spell;
    float percent;
    char const* name;
    char const* sound = nullptr;    // played as it strikes (client-assets/audio/vrogar.json)
};

Rule const RuleAnvil = { SPELL_ANVIL, DodgePct, "anvil cone" };
Rule const RuleShockwave = { SPELL_SHOCKWAVE, DodgePct, "shockwave", "Vrogar.Shockwave" };
Rule const RulePour = { SPELL_POUR, DodgePct, "pour", "Vrogar.Pour" };
Rule const RuleSparks = { SPELL_SPARKS, DodgePct, "sparks", "Vrogar.Sparks" };
Rule const RuleChains = { SPELL_CHAINS, DodgePct, "chains", "Vrogar.Chains" };
Rule const RuleLaminoir = { SPELL_LAMINOIR, DodgePct, "laminoir", "Vrogar.Laminoir" };
Rule const RuleSteam = { SPELL_STEAM, 100.0f, "steam", "Vrogar.Steam" };
Rule const RuleFlood = { SPELL_FLOOD, 100.0f, "flood", "Vrogar.Flood" };
Rule const RuleBellows = { SPELL_BELLOWS, 100.0f, "bellows", "Vrogar.BellowsFire" };
Rule const RuleFonte = { SPELL_FONTE, 100.0f, "fonte", "Vrogar.Fonte" };

bool IsTank(Player* player)
{
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() && member.roles)
                return member.roles & lfg::PLAYER_ROLE_TANK;
    return player->HasTankSpec();
}

using Areas = std::vector<GroundIndicators::Area>;

struct boss_forge_master : public Defi::BossAI
{
    boss_forge_master(Creature* creature) : Defi::BossAI(creature, Room, "module.vrogar")
    {
        // Held at the arena's middle all fight (HoldMiddle): his tanks come to him
        me->SetCombatMovement(false);
    }

    void Reset() override
    {
        if (_phase == Phase::Rehearsal)
            return;
        // A fight still on is a wipe: the board resets him once the players are down
        if (_phase != Phase::None && _phase != Phase::Over)
        {
            WatchDeaths();
            LOG_INFO(_logName, "Vrogar wipe instance={} at={:.1f}s true health={:.1f}% deaths={}", me->GetInstanceId(),
                     Elapsed() / 1000.0f, TrueHealthPct(), DeathsText());
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

    // Combat's own evade (no one left to hit: la Fonte holds him away) never ends the fight while a player stands in
    // the arena; the board's reset (a wipe: EVADE_REASON_OTHER) always does
    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (_phase == Phase::Rehearsal)
            return;
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
        SetModelHealth();
        StartPace(HoldPct);
        GroundIndicators::DrawInstantly(me);
        _timeline = BuildTimeline();
        _next = 0;
        StartClock();
        _phase = Phase::Forge;
        RaiseArena();
        Talk(SAY_AGGRO);
        StartMusic("Music.EscapeHunter");
        LOG_INFO(_logName, "Vrogar pulled instance={} health={} arena={:.2f} yd tier factor={}", me->GetInstanceId(),
                 me->GetMaxHealth(), ArenaRadius, GetChallengeDamageFactorOf(me));
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
        LOG_INFO(_logName, "Vrogar killed instance={} at={:.1f}s deaths={}", me->GetInstanceId(), Elapsed() / 1000.0f,
                 DeathsText());
        EndMusic();
        ResetFight();
        _phase = Phase::Over;
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_phase == Phase::Rehearsal)
        {
            damage = 0;
            return;
        }
        PaceDamage(damage, PaceMs(), CheckPaceMs());
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
            return;
        }
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
        UpdatePace(PaceMs(), CheckPaceMs());
        HoldMiddle();
        if (elapsed >= _nextCheckMs)
        {
            _nextCheckMs = elapsed + 200;
            CheckWall();
            UpdateTanks();
        }
        if (_phase != Phase::Fonte && !CastingBar() && !_walking)
            DoMeleeAttackIfReady();
    }

    // --- Testing ---------------------------------------------------------------------------------------------------
    std::string Describe() const
    {
        uint32 const elapsed = Elapsed();
        return Acore::StringFormat("Vrogar: phase {} at {:.1f}s (beat {:.1f}), shown health {:.1f}%, true {:.1f}%, next "
            "step {}/{}, held {}", uint32(_phase), elapsed / 1000.0f, Grid.BeatOf(elapsed), me->GetHealthPct(),
            TrueHealthPct(), _next, _timeline.size(), _held);
    }

    // Jumps the fight forward (the music is not: it goes on from where it was); the phase changes passed over run
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over || _phase == Phase::Rehearsal)
            return false;
        scheduler.CancelAll();
        EndCastBar();
        Release();
        AdvanceClock(seconds * IN_MILLISECONDS);
        uint32 const elapsed = Elapsed();
        while (_next < _timeline.size() && Grid.At(_timeline[_next].beat) <= elapsed)
        {
            Step const step = _timeline[_next++];
            if (step.what == Event::FonteStart || step.what == Event::FonteEnd)
                Execute(step);
        }
        return true;
    }

    // One pattern now (in a fight, or in the lab's rehearsal)
    bool CastNow(std::string const& what)
    {
        if (!me->IsInCombat() && _phase != Phase::Rehearsal)
            return false;
        static std::map<std::string, Step> const steps = {
            { "anvil", { 0, Event::Anvil } }, { "shockwave", { 0, Event::Shockwave } }, { "pour", { 0, Event::Pour } }, { "sparks", { 0, Event::Sparks } },
            { "chains", { 0, Event::Chains } }, { "laminoir", { 0, Event::Laminoir } },
            { "laminoir2", { 0, Event::Laminoir, 1 } }, { "trempe", { 0, Event::Trempe, 1 } },
            { "trempe2", { 0, Event::Trempe, 2 } }, { "bellows", { 0, Event::Bellows, 1 } },
            { "bellows2", { 0, Event::Bellows, 2 } }, { "ingots", { 0, Event::Ingots, 1 } },
            { "ingots2", { 0, Event::Ingots, 2 } }, { "hammer", { 0, Event::Hammer } },
            { "hammergroup", { 0, Event::HammerGroup } }, { "fonte", { 0, Event::FonteStart } },
            { "check", { 0, Event::Check } },
        };
        auto const found = steps.find(what);
        if (found == steps.end())
            return false;
        Step step = found->second;
        step.beat = CurrentBeat(Grid) + 1;
        if (step.what == Event::FonteStart)
        {
            // In the lab: the intermission's floor alone, a few bars of it
            FonteFloor(step.beat, step.beat + 48);
            return true;
        }
        AtBeat(Grid, step.beat, [this, step]() { Execute(step); });
        return true;
    }

    // The FX lab: he stands at its middle in rehearsal, his arena and props up, harmless
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
    Defi::Sizing const& GetSizing() const override { return Sizing; }
    float HealthScale() const override { return float(::HealthScale); }
    bool IsOwn(Creature const* /*creature*/) const override { return false; }

    float TakenFactor(Unit const* victim) const override
    {
        float factor = 1.0f;
        if (uint8 const stacks = StacksOf(victim, SPELL_BURN))
            factor *= 1.0f + BurnTakenPct / 100.0f * float(stacks);
        return factor;
    }

private:
    // --- The pace: his time to be hit (the fight but la Fonte), and the check's on it ---------------------------------
    uint32 PaceMs() const
    {
        uint32 const elapsed = Elapsed();
        uint32 const from = Grid.At(FonteStartBeat);
        uint32 const to = Grid.At(FonteEndBeat);
        if (elapsed <= from)
            return elapsed;
        return from + (elapsed >= to ? elapsed - to : 0);
    }

    uint32 CheckPaceMs() const
    {
        return Grid.At(CheckBeat) - (Grid.At(FonteEndBeat) - Grid.At(FonteStartBeat));
    }

    // --- The timeline ----------------------------------------------------------------------------------------------
    void Execute(Step const& step)
    {
        uint32 const elapsed = Elapsed();
        LOG_INFO(_logName, "Vrogar {} instance={} at={:.2f}s beat={:.1f} shown={:.1f}% true={:.1f}%",
                 EventName(step.what), me->GetInstanceId(), elapsed / 1000.0f, Grid.BeatOf(elapsed),
                 me->GetHealthPct(), TrueHealthPct());
        uint32 const at = step.beat;
        switch (step.what)
        {
            case Event::Anvil: Anvil(at); break;
            case Event::Shockwave: Shockwave(at); break;
            case Event::Pour: Pour(at); break;
            case Event::Sparks: Sparks(at); break;
            case Event::Chains: Chains(at); break;
            case Event::Laminoir: Laminoir(at, step.arg != 0); break;
            case Event::Trempe: Trempe(at, step.arg); break;
            case Event::Bellows: Bellows(at, step.arg); break;
            case Event::Ingots: Ingots(at, step.arg); break;
            case Event::Hammer: Hammer(at); break;
            case Event::HammerGroup: HammerGroup(at); break;
            case Event::FonteStart: StartFonte(at); break;
            case Event::FonteEnd: EndFonte(); break;
            case Event::Check: Check(at); break;
        }
    }

    // --- Waves: red drawn on a beat, struck on a later one ----------------------------------------------------------
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

    // Whoever stands in any of the areas: struck by rule. In rehearsal: nobody, and the share of the arena left safe
    // logged (its floor sampled a yard apart)
    void Strike(Areas const& areas, Rule const& rule)
    {
        if (rule.sound && !areas.empty())
            Sound(rule.sound, areas.front().origin);
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
        LOG_INFO(_logName, "Vrogar rehearsal {}: {} of {} square yards safe ({:.0f}%)", name, safe, total,
                 total ? 100.0f * float(safe) / float(total) : 0.0f);
    }

    // An area only the bots are told of (unseen by the players), from now to its beat
    void Watch(GroundIndicators::Area const& area, uint32 lands)
    {
        GroundIndicators::WatchArea(me, area, MsTo(lands), MistakeDamage);
    }

    Position Middle() const { return Ground(GetRoom().center); }

    // One of his sounds (client-assets/audio/vrogar.json) at where, for everyone hearing the fight
    void Sound(char const* key, Position const& where)
    {
        for (Player* player : Listeners())
            EvolutionsAudio::PlayAt(player, key, where);
    }

    Position OnArena(float angle, float distance) const { return AtAngle(GetRoom().center, angle, distance); }

    static GroundIndicators::Area MakeCircle(Position const& center, float radius)
    {
        GroundIndicators::Area area;
        area.kind = GroundIndicators::Area::Kind::Circle;
        area.origin = center;
        area.radius = radius;
        return area;
    }

    // Players at random, the tanks last
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

    // --- The arena ---------------------------------------------------------------------------------------------------
    // Its wall rises round the middle (Vorhan's pieces, WallPieces of them: ArenaRadius), the props at its edge; the
    // wall kills from WallRiseBeats on. For the bots, everything past it is one area to keep out of, all fight.
    void RaiseArena()
    {
        Position const middle = Middle();
        uint32 const lasts = MusicEndMs + 30000;
        // The wall a few seconds into the fight (at once in the lab)
        uint32 const shows = _phase == Phase::Rehearsal ? 0 : WallShowBeats;
        AtBeat(Grid, shows, [this, middle, lasts]()
        {
            GroundIndicators::ShowWardenShockwave(me, middle);
            GroundIndicators::ShowCurtainRing(me, middle, GroundIndicators::SPELL_WARDEN_WALL_FIRST,
                GroundIndicators::WardenWallLooks, WallPieces, GroundIndicators::WardenWallPieceLength, lasts);
            Sound("Vrogar.WallRise", middle);
        });
        _wallFrom = Grid.At(shows + WallRiseBeats);
        // As it closes, anyone still outside (a pull from range) is drawn in: the wall kills only from then on
        AtBeat(Grid, shows + WallRiseBeats, [this]() { DrawIn(); });
        // The props: the trough and the crucible across from each other, the bellows at the four points
        _troughAngle = float(M_PI) / 4.0f;
        _crucibleAngle = _troughAngle + float(M_PI);
        Prop(NPC_TROUGH, _troughAngle, PropDistance);
        Prop(NPC_CRUCIBLE, _crucibleAngle, PropDistance);
        for (uint32 index = 0; index < 4; ++index)
            Prop(NPC_BELLOWS, BellowsAngle(index), BellowsDistance);
        KeepBotsIn(lasts);
    }

    void Prop(uint32 entry, float angle, float distance)
    {
        Position at = Ground(OnArena(angle, distance));
        at.SetOrientation(Position::NormalizeOrientation(angle + float(M_PI)));
        if (Creature* prop = me->SummonCreature(entry, at, TEMPSUMMON_MANUAL_DESPAWN))
            _props.push_back(prop->GetGUID());
    }

    static float BellowsAngle(uint32 index) { return float(index) * float(M_PI) / 2.0f; }

    // The bots keep out of everything past the wall, the whole fight (one area: it never strikes before the others)
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
        if (_phase == Phase::Rehearsal)
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
                Doom(player, SPELL_WALL);
                LOG_INFO(_logName, "Vrogar wall instance={} at={:.1f}s {}", me->GetInstanceId(), Elapsed() / 1000.0f,
                         player->GetName());
            }
    }

    // --- The middle: he stands there all fight, rooted; his walk to la Trempe's props is the only time he leaves it,
    // and anything moving him (a knockback, a grip) has him leap straight back
    void HoldMiddle()
    {
        if (_walking || _phase == Phase::Fonte || _phase == Phase::None || _phase == Phase::Over)
            return;
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

    // --- The middle: for a puzzle he goes there and holds still until it resolves -----------------------------------
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
                me->GetMotionMaster()->MoveJump(middle, std::max(10.0f, me->GetExactDist2d(&middle) / seconds), 6.0f);
        });
        AtBeat(Grid, lands, [this]() { _landed = true; });
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

    // --- Coup d'enclume: his tank's cone ------------------------------------------------------------------------------
    void Anvil(uint32 start)
    {
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (_phase == Phase::Fonte)
            return;
        CastBar(CAST_ANVIL);
        float const facing = tank ? me->GetAngle(tank) : me->GetOrientation();
        if (tank)
            me->SetFacingToObject(tank);
        Position apex = Ground(me->GetPosition());
        apex.SetOrientation(facing);
        ObjectGuid const tankGuid = tank ? tank->GetGUID() : ObjectGuid::Empty;
        uint32 const lands = start + WarnBeats;
        GroundIndicators::Area const cone = GroundIndicators::ShowAimedCone(me, apex, facing, ArenaRadius + 1.0f,
            AnvilArc, MsTo(lands), tank, GroundIndicators::Theme::None, MistakeDamage);
        Sound("Vrogar.AnvilCast", me->GetPosition());
        AtBeat(Grid, lands, [this, cone, tankGuid]()
        {
            EndCastBar();
            Sound("Vrogar.Anvil", cone.origin);
            if (_phase == Phase::Rehearsal)
                LogSafe("anvil cone", { cone });
            else
                for (Player* player : ArenaPlayers())
                {
                    if (player->GetGUID() == tankGuid)
                    {
                        Hit(player, SPELL_ANVIL, AnvilTankPct);
                        AddTimedAura(player, SPELL_BURN, BurnMs,
                            uint8(std::min(10, StacksOf(player, SPELL_BURN) + 1)));
                    }
                    else if (cone.Contains(player->GetPosition()))
                        Mistake(player, RuleAnvil);
                }
        });
    }

    // --- Onde de choc: three rings round him shown together - the middle, the ring round it, out to the wall - struck
    // in that order, AnvilRingBeats apart, the first WarnBeats after they show
    void Shockwave(uint32 start)
    {
        if (_phase == Phase::Fonte)
            return;
        CastBar(CAST_ANVIL);
        Sound("Vrogar.AnvilCast", me->GetPosition());
        Position const center = Middle();
        uint32 const first = start + WarnBeats;
        AtBeat(Grid, first, [this]() { EndCastBar(); });
        Areas rings;
        rings.push_back(GroundIndicators::ShowCircle(me, center, AnvilInner, MsTo(first),
            GroundIndicators::Theme::Fire, MistakeDamage));
        rings.push_back(GroundIndicators::ShowRing(me, center, AnvilMiddle, AnvilInner, MsTo(first + AnvilRingBeats),
            GroundIndicators::Theme::Fire, MistakeDamage));
        rings.push_back(GroundIndicators::ShowRing(me, center, ArenaRadius + 1.0f, AnvilMiddle,
            MsTo(first + 2 * AnvilRingBeats), GroundIndicators::Theme::Fire, MistakeDamage));
        for (uint32 index = 0; index < rings.size(); ++index)
        {
            GroundIndicators::Area const ring = rings[index];
            AtBeat(Grid, first + AnvilRingBeats * index, [this, ring]() { Strike({ ring }, RuleShockwave); });
        }
    }

    // The tanks: him held at the arena's middle, the other tank at his flank; the swap at SwapStacks of Brûlure
    void UpdateTanks()
    {
        if (_phase == Phase::Fonte)
            return;
        GroundIndicators::SetTankSpot(me, Middle(), 1000, 3.0f);
        Unit* victim = me->GetVictim();
        Player* tank = victim ? victim->ToPlayer() : nullptr;
        if (!tank)
            return;
        GroundIndicators::SetOffTankSpot(me, AtAngle(me->GetPosition(), me->GetAngle(tank) + float(M_PI) / 2.0f,
            4.0f), 1000);
        if (StacksOf(tank, SPELL_BURN) < SwapStacks)
            return;
        for (Player* player : ArenaPlayers())
            if (player != tank && IsTank(player) && StacksOf(player, SPELL_BURN) < SwapStacks)
            {
                GroundIndicators::SetBossHolder(me, player, 8000);
                return;
            }
    }

    // --- Coulée: half the arena, then the other half ------------------------------------------------------------------
    void Pour(uint32 start)
    {
        CastBar(CAST_POUR);
        AtBeat(Grid, start + WarnBeats, [this]() { EndCastBar(); });
        float const facing = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 half = 0; half < 2; ++half)
        {
            float const towards = facing + float(half) * float(M_PI);
            uint32 const shown = half ? start + WarnBeats : start;
            Wave(shown, shown + (half ? FollowBeats : WarnBeats), [this, towards](uint32 ms)
            {
                Position apex = Middle();
                apex.SetOrientation(towards);
                return Areas{ GroundIndicators::ShowCone(me, apex, towards, ArenaRadius + 1.0f, 180.0f, ms,
                    GroundIndicators::Theme::Fire, MistakeDamage) };
            }, RulePour);
        }
    }

    // --- Étincelles: a circle under everyone -------------------------------------------------------------------------
    void Sparks(uint32 start)
    {
        CastBar(CAST_SPARKS);
        AtBeat(Grid, start + WarnBeats, [this]() { EndCastBar(); });
        Wave(start, start + WarnBeats, [this](uint32 ms)
        {
            Areas areas;
            std::vector<Position> spots;
            for (Player* player : ArenaPlayers())
                spots.push_back(Ground(player->GetPosition()));
            if (_phase == Phase::Rehearsal)
                for (uint32 index = 0; index < 8; ++index)
                    spots.push_back(OnArena(float(index) * float(M_PI) / 4.0f, 8.0f));
            for (Position const& spot : spots)
                areas.push_back(GroundIndicators::ShowCircle(me, spot, SparkRadius, ms,
                    GroundIndicators::Theme::Fire, MistakeDamage));
            return areas;
        }, RuleSparks);
    }

    // --- Chaînes: four arms from him to the wall turning, stopped dead on a beat: two bars later they strike -----------------
    void Chains(uint32 start)
    {
        CastBar(CAST_CHAINS);
        Sound("Vrogar.ChainsSpin", me->GetPosition());
        Position const center = Ground(me->GetPosition());
        float const facing = frand(0.0f, 2.0f * float(M_PI));
        float const way = urand(0, 1) ? 1.0f : -1.0f;
        float const radiansPerSecond = way * 2.0f * float(M_PI) / (Grid.beatMs * float(ChainTurnBeats) / 1000.0f);
        uint32 const stops = start + urand(4, 6);
        uint32 const spinMs = MsTo(stops);
        Areas spinning;
        for (uint32 arm = 0; arm < 4; ++arm)
            spinning.push_back(GroundIndicators::ShowSweepingRectangle(me, center,
                facing + float(arm) * float(M_PI) / 2.0f, radiansPerSecond, ArenaRadius + 1.0f, ChainWidth, spinMs,
                MistakeDamage));
        Wave(stops, stops + WarnBeats, [this, spinning, radiansPerSecond, spinMs](uint32 ms)
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

    // --- Le Laminoir: rollers across the arena, strip by strip a beat apart, a gap in each ---------------------------------
    void Laminoir(uint32 start, bool back)
    {
        CastBar(CAST_LAMINOIR);
        AtBeat(Grid, start + LaminoirWarnBeats, [this]() { EndCastBar(); });
        uint32 const first = start + LaminoirWarnBeats;
        float const seed = frand(0.0f, 2.0f * float(M_PI));
        Position const middle = Middle();
        float const half = ArenaRadius + 1.0f;
        for (uint32 pass = 0; pass < (back ? 2u : 1u); ++pass)
            for (uint32 index = 0; index < LaminoirStrips; ++index)
            {
                uint32 const strip = pass ? LaminoirStrips - 1 - index : index;
                float const x = middle.GetPositionX() + (float(LaminoirStrips - 1) / 2.0f - float(strip)) *
                    LaminoirDepth;
                float gap = middle.GetPositionY() + 9.0f * std::sin(seed + float(strip) * 1.3f);
                if (pass)
                    gap += (strip % 2 ? 7.0f : -7.0f);
                gap = std::clamp(gap, middle.GetPositionY() - ArenaRadius + LaminoirGap,
                    middle.GetPositionY() + ArenaRadius - LaminoirGap);
                uint32 const lands = first + pass * (LaminoirStrips + 3) + index;
                Wave(lands - LaminoirWarnBeats, lands, [this, x, gap, half, middle](uint32 ms)
                {
                    Areas strip;
                    float const west = middle.GetPositionY() + half;
                    float const east = middle.GetPositionY() - half;
                    float const halfGap = LaminoirGap / 2.0f;
                    if (gap - halfGap > east)
                        strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, east, 0.0f)),
                            float(M_PI) / 2.0f, gap - halfGap - east, LaminoirDepth, ms, GroundIndicators::Theme::None,
                            MistakeDamage));
                    if (west > gap + halfGap)
                        strip.push_back(GroundIndicators::ShowRectangle(me, Ground(Position(x, gap + halfGap, 0.0f)),
                            float(M_PI) / 2.0f, west - gap - halfGap, LaminoirDepth, ms,
                            GroundIndicators::Theme::None, MistakeDamage));
                    return strip;
                }, RuleLaminoir);
            }
    }

    // --- La Trempe: he dips his hammer in the trough or the crucible, then the strike two bars after he is back ----------
    // The trough: steam bursts in the middle (be out); the crucible: iron floods the outer ring (be in). dips: 1, or 2
    // (both props, at random which first: they strike in that order, a bar apart)
    void Trempe(uint32 start, uint32 dips)
    {
        std::vector<bool> order;    // true: the trough
        bool const troughFirst = urand(0, 1);
        order.push_back(troughFirst);
        if (dips > 1)
            order.push_back(!troughFirst);
        // His walk: to each prop (6 beats), its dip, then back to the middle (6 beats)
        _held = true;
        uint32 beat = start;
        for (bool trough : order)
        {
            float const angle = trough ? _troughAngle : _crucibleAngle;
            Position const at = OnArena(angle, PropDistance - 2.5f);
            AtBeat(Grid, beat, [this, at]() { WalkTo(at); });
            beat += 6;
            AtBeat(Grid, beat, [this, trough, angle]()
            {
                CastBar(CAST_TREMPE);
                Position const prop = OnArena(angle, PropDistance);
                // Steam from the trough, fire from the crucible: what is coming
                GroundIndicators::Burst(me, prop, trough ? GroundIndicators::Theme::Frost : GroundIndicators::Theme::Fire);
                Sound(trough ? "Vrogar.DipTrough" : "Vrogar.DipCrucible", prop);
                GroundIndicators::ShowParticles(me, MakeCircle(prop, 2.5f),
                    trough ? GroundIndicators::Theme::Frost : GroundIndicators::Theme::Fire, uint32(Grid.beatMs * 4));
            });
            beat += 3;
            AtBeat(Grid, beat, [this]() { EndCastBar(); });
            beat += 1;
        }
        Position const middle = Middle();
        AtBeat(Grid, beat, [this, middle]() { WalkTo(middle); });
        beat += 6;
        AtBeat(Grid, beat, [this]() { StopWalking(); _landed = true; });
        uint32 const first = beat + 8;
        for (uint32 index = 0; index < order.size(); ++index)
        {
            bool const trough = order[index];
            uint32 const strikes = first + 4 * index;
            GroundIndicators::Area area;
            area.kind = trough ? GroundIndicators::Area::Kind::Circle : GroundIndicators::Area::Kind::Ring;
            area.origin = middle;
            area.radius = trough ? SteamRadius : ArenaRadius * 2.0f;
            area.inner = trough ? 0.0f : SteamRadius;
            // The bots are told from his return
            AtBeat(Grid, beat, [this, area, strikes]() { Watch(area, strikes); });
            AtBeat(Grid, strikes, [this, area, trough, middle]()
            {
                // Seen as it strikes: the steam's burst or the iron's flood
                if (trough)
                    GroundIndicators::ShowCircle(me, middle, SteamRadius, uint32(Grid.beatMs),
                        GroundIndicators::Theme::Frost, MistakeDamage);
                else
                    GroundIndicators::ShowRing(me, middle, ArenaRadius + 1.0f, SteamRadius, uint32(Grid.beatMs),
                        GroundIndicators::Theme::Fire, MistakeDamage);
                Strike({ area }, trough ? RuleSteam : RuleFlood);
            });
        }
        AtBeat(Grid, first + 4 * uint32(order.size()), [this]() { Release(); });
    }

    void WalkTo(Position const& at)
    {
        _walking = true;
        EndCastBar();
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MovePoint(1, at.GetPositionX(), at.GetPositionY(), at.GetPositionZ());
    }

    void StopWalking()
    {
        _walking = false;
        if (_phase != Phase::Rehearsal)
            me->SetReactState(REACT_AGGRESSIVE);
    }

    // --- Les Soufflets: two adjacent bellows of four glow, then each blasts the half on its side --------------------------
    // In the middle from start: the pair glows two bars, its blast shown two beats before it strikes; blasts 2: a second
    // pair, turned, lit as the first strikes
    void Bellows(uint32 start, uint32 blasts)
    {
        uint32 const lands = start + 2;
        uint32 const until = lands + 10 * blasts + 2;
        TakeMiddle(lands, until);
        AtBeat(Grid, lands, [this]() { CastBar(CAST_BELLOWS); });
        AtBeat(Grid, lands + 8, [this]() { EndCastBar(); });
        uint32 first = urand(0, 3);
        for (uint32 blast = 0; blast < blasts; ++blast)
        {
            uint32 const pair = (first + blast * urand(1, 2)) % 4;
            uint32 const lit = lands + 10 * blast;
            uint32 const strikes = lit + 10;
            Areas halves;
            for (uint32 index : { pair, (pair + 1) % 4 })
            {
                float const angle = BellowsAngle(index);
                Position apex = Middle();
                apex.SetOrientation(angle);
                GroundIndicators::Area half;
                half.kind = GroundIndicators::Area::Kind::Cone;
                half.origin = apex;
                half.radius = ArenaRadius + 1.0f;
                half.arc = float(M_PI);
                halves.push_back(half);
                Position const bellows = OnArena(angle, BellowsDistance);
                AtBeat(Grid, lit, [this, bellows, half, strikes, lit]()
                {
                    // The bellows glowing: a ring of fire round it and its burst on every beat until it blasts
                    GroundIndicators::ShowParticles(me, MakeCircle(bellows, 4.0f), GroundIndicators::Theme::Fire,
                        MsTo(strikes));
                    for (uint32 beat = lit; beat < strikes; ++beat)
                        AtBeat(Grid, beat, [this, bellows]()
                        {
                            GroundIndicators::Burst(me, bellows, GroundIndicators::Theme::Fire);
                        });
                    Watch(half, strikes);
                    Sound("Vrogar.BellowsLit", bellows);
                });
                AtBeat(Grid, strikes - 4, [this, half, strikes]()
                {
                    GroundIndicators::ShowCone(me, half.origin, half.origin.GetOrientation(), half.radius, 180.0f,
                        MsTo(strikes), GroundIndicators::Theme::Fire, MistakeDamage);
                });
            }
            AtBeat(Grid, strikes, [this, halves]()
            {
                Sound("Vrogar.BellowsBlast", Middle());
                Strike(halves, RuleBellows);
            });
        }
    }

    // --- Les Lingots: four ingots, each to be held by exactly two ----------------------------------------------------------
    // In the middle from start; each wave lands 10 beats after it is shown; waves 2: a second, turned an eighth
    void Ingots(uint32 start, uint32 waves)
    {
        uint32 const lands = start + 2;
        uint32 const until = lands + 12 * waves;
        TakeMiddle(lands, until);
        AtBeat(Grid, lands, [this]() { CastBar(CAST_INGOTS); });
        AtBeat(Grid, lands + 10, [this]() { EndCastBar(); });
        float const base = float(M_PI) / 4.0f;
        for (uint32 wave = 0; wave < waves; ++wave)
        {
            uint32 const shown = lands + 12 * wave;
            uint32 const strikes = shown + 10;
            float const turn = wave ? float(M_PI) / 4.0f : 0.0f;
            AtBeat(Grid, shown, [this, base, turn, strikes]()
            {
                std::vector<Position> spots;
                for (uint32 index = 0; index < 4; ++index)
                {
                    Position const spot = OnArena(base + turn + float(index) * float(M_PI) / 2.0f, IngotDistance);
                    spots.push_back(spot);
                    GroundIndicators::ShowDecal(me, spot, 0.0f, IngotRadius, MsTo(strikes),
                        GroundIndicators::SPELL_SIGIL_RADIANT);
                    GroundIndicators::ShowSoak(me, spot, IngotRadius, MsTo(strikes), 2,
                        GroundIndicators::Theme::Holy, true, false);
                }
                Sound("Vrogar.Ingots", Middle());
                AtBeat(Grid, strikes, [this, spots]() { StrikeIngots(spots); });
            });
        }
    }

    void StrikeIngots(std::vector<Position> const& spots)
    {
        uint32 failed = 0;
        for (Position const& spot : spots)
        {
            std::vector<Player*> under;
            for (Player* player : ArenaPlayers())
                if (player->GetExactDist2d(&spot) <= IngotRadius)
                    under.push_back(player);
            GroundIndicators::Burst(me, spot, GroundIndicators::Theme::Fire);
            Sound("Vrogar.IngotsLand", spot);
            if (_phase == Phase::Rehearsal)
                continue;
            if (under.size() == 2)
                for (Player* player : under)
                    Hit(player, SPELL_INGOT_HOLD, IngotPairPct);
            else if (under.size() > 2)
                for (Player* player : under)
                    Doom(player, SPELL_INGOT_CRUSH);
            else
                ++failed;
        }
        for (uint32 index = 0; index < failed; ++index)
            HitEveryone(SPELL_INGOT_FAIL, IngotFailPct);
        LOG_INFO(_logName, "Vrogar ingots instance={} at={:.1f}s failed={}", me->GetInstanceId(), Elapsed() / 1000.0f,
                 failed);
    }

    // --- Le Marteau: two marked to the wall, the others share his blow under him -----------------------------------------
    void Hammer(uint32 start)
    {
        uint32 const lands = start + 2;
        uint32 const strikes = lands + MarkBeats;
        TakeMiddle(lands, strikes + 2);
        AtBeat(Grid, lands, [this, strikes]()
        {
            CastBar(CAST_HAMMER_MARK);
            uint32 const ms = MsTo(strikes);
            std::vector<ObjectGuid> marked;
            for (Player* player : Targets(2))
            {
                AddTimedAura(player, SPELL_MARKED, ms);
                GroundIndicators::ShowCarriedCircle(me, player, MarkedRadius, ms, MistakeDamage, 2.0f);
                marked.push_back(player->GetGUID());
            }
            Position const soak = Middle();
            GroundIndicators::ShowDecal(me, soak, 0.0f, HammerSoakRadius, ms, GroundIndicators::SPELL_SIGIL_BASTION);
            GroundIndicators::ShowSoak(me, soak, HammerSoakRadius, ms, uint32(ArenaPlayers().size()),
                GroundIndicators::Theme::Holy, true, false);
            AtBeat(Grid, strikes, [this, marked, soak]()
            {
                EndCastBar();
                Sound("Vrogar.Hammer", soak);
                StrikeHammer(marked, soak);
            });
        });
    }

    void StrikeHammer(std::vector<ObjectGuid> const& marked, Position const& soak)
    {
        if (_phase == Phase::Rehearsal)
            return;
        std::vector<Player*> carriers;
        for (ObjectGuid const& guid : marked)
            if (Player* player = ObjectAccessor::GetPlayer(*me, guid); player && player->IsAlive())
                carriers.push_back(player);
        std::vector<Player*> sharing;
        for (Player* player : ArenaPlayers())
        {
            if (std::find(carriers.begin(), carriers.end(), player) != carriers.end())
                continue;
            bool besideMark = false;
            for (Player* carrier : carriers)
                if (player->GetExactDist2d(carrier) <= MarkedRadius)
                    besideMark = true;
            if (besideMark)
                Doom(player, SPELL_HAMMER_MARK);
            else if (player->GetExactDist2d(&soak) <= HammerSoakRadius)
                sharing.push_back(player);
            else
                Doom(player, SPELL_HAMMER_SHARE);
        }
        for (Player* carrier : carriers)
            HitOwnHealth(carrier, SPELL_HAMMER_MARK, MarkedPct);
        float const share = HammerSharePct / float(std::max<size_t>(1, sharing.size()));
        for (Player* player : sharing)
            Hit(player, SPELL_HAMMER_SHARE, share);
    }

    // --- Frappe de l'enclume: a group hit on the beat ------------------------------------------------------------------
    void HammerGroup(uint32 start)
    {
        CastBar(CAST_HAMMER_GROUP);
        GroundIndicators::WarnGroupDamage(me, MsTo(start + 3));
        AtBeat(Grid, start + 3, [this]()
        {
            EndCastBar();
            Sound("Vrogar.HammerGroup", me->GetPosition());
            if (_phase != Phase::Rehearsal)
                HitEveryone(SPELL_HAMMER_GROUP, HammerGroupPct);
        });
    }

    // --- La Fonte: he is in his furnace; eight slices of the floor, all but two burning each bar ----------------------------
    void StartFonte(uint32 start)
    {
        Release();
        _phase = Phase::Fonte;
        CastBar(CAST_FONTE);
        scheduler.Schedule(2000ms, [this](TaskContext)
        {
            EndCastBar();
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            me->SetDisplayId(InvisibleDisplay);
            Position const middle = Middle();
            me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(),
                me->GetOrientation());
        });
        FonteFloor(start + 4, FonteEndBeat);
    }

    // From `from` to `to`: every other bar (on its kick) the floor burns but for two adjacent slices of eight; the safe
    // pair steps one slice clockwise each time (shown a bar ahead), then in the second half jumps across (shown a bar
    // and a half ahead). One burn on the floor at a time: two overlapping read as one red disc
    void FonteFloor(uint32 from, uint32 to)
    {
        uint32 safe = urand(0, 7);
        uint32 const half = from + (to - from) / 2;
        for (uint32 lands = from + WarnBeats; lands + 1 < to; lands += 8)
        {
            bool const jumps = lands >= half;
            uint32 const warn = lands == from + WarnBeats || jumps ? WarnBeats : FollowBeats;
            uint32 const pair = safe;
            Wave(lands - warn, lands, [this, pair](uint32 ms)
            {
                // Three quarters burning, drawn as one: a half and a quarter side by side, from the safe pair's edge
                // round (eight slices drawn apart read as a pink disc, the gap lost in it)
                Position const middle = Middle();
                float const safeEnd = float(pair + 2) * float(M_PI) / 4.0f;
                Areas burning;
                for (auto const& [centre, arc] : { std::pair{ safeEnd + float(M_PI) / 2.0f, 180.0f },
                                                   std::pair{ safeEnd + float(M_PI) * 1.25f, 90.0f } })
                {
                    Position apex = middle;
                    apex.SetOrientation(centre);
                    burning.push_back(GroundIndicators::ShowCone(me, apex, centre, ArenaRadius + 1.0f, arc, ms,
                        GroundIndicators::Theme::None, MistakeDamage));
                }
                return burning;
            }, RuleFonte);
            // Clockwise: the slices are numbered counterclockwise (the orientation's way)
            safe = jumps ? (safe + 4) % 8 : (safe + 7) % 8;
        }
    }

    // Out of the furnace on the track's hardest hit, in the middle
    void EndFonte()
    {
        Position const middle = Middle();
        me->NearTeleportTo(middle.GetPositionX(), middle.GetPositionY(), middle.GetPositionZ(), me->GetOrientation());
        me->RestoreDisplayId();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetReactState(REACT_AGGRESSIVE);
        _phase = Phase::Quench;
        GroundIndicators::Burst(me, middle, GroundIndicators::Theme::Fire);
        Sound("Vrogar.Return", middle);
        HitEveryone(SPELL_RETURN, ReturnPct);
    }

    // --- La Coulée finale: the check on the climax ---------------------------------------------------------------------
    void Check(uint32 start)
    {
        uint32 const lands = start + 2;
        uint32 const strikes = CheckBeat;
        TakeMiddle(lands, strikes);
        AtBeat(Grid, lands, [this, strikes]()
        {
            CastBar(CAST_CHECK);
            GroundIndicators::WarnGroupDamage(me, MsTo(strikes));
        });
        AtBeat(Grid, strikes, [this]()
        {
            EndCastBar();
            if (_phase == Phase::Rehearsal)
                return;
            bool const passed = AtPaceFloor();
            Sound(passed ? "Vrogar.Return" : "Vrogar.Flood", Middle());
            LOG_INFO(_logName, "Vrogar check instance={} at={:.1f}s true={:.1f}% alive={} passed={}",
                     me->GetInstanceId(), Elapsed() / 1000.0f, TrueHealthPct(), ArenaPlayers().size(), passed);
            if (!passed)
            {
                GroundIndicators::ShowCircle(me, Middle(), ArenaRadius + 1.0f, 2000, GroundIndicators::Theme::Fire,
                    MistakeDamage);
                for (Player* player : ArenaPlayers())
                    Doom(player, SPELL_CHECK);
                return;
            }
            HitEveryone(SPELL_CHECK, CheckHitPct);
            EndPace();
            std::vector<Player*> const players = ArenaPlayers();
            Unit* credit = me->GetVictim() ? me->GetVictim() : (players.empty() ? nullptr : players.front());
            Unit::Kill(credit ? credit : me, me);
        });
    }

    // --- Mistakes ----------------------------------------------------------------------------------------------------
    // A share of the victim's own health (100: death); a second under Ébouillanté is death
    void Mistake(Player* player, Rule const& rule)
    {
        if (!player || !player->IsAlive())
            return;
        if (rule.percent >= 100.0f || player->HasAura(SPELL_SCALD))
            Doom(player, rule.spell);
        else
        {
            HitOwnHealth(player, rule.spell, rule.percent);
            if (player->IsAlive())
                AddTimedAura(player, SPELL_SCALD, ScaldMs);
        }
        LOG_INFO(_logName, "Vrogar rule broken instance={} at={:.1f}s {}: {}", me->GetInstanceId(),
                 Elapsed() / 1000.0f, rule.name, player->GetName());
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
                    LOG_INFO(_logName, "Vrogar death instance={} at={:.1f}s {}", me->GetInstanceId(),
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
        _held = false;
        _landed = false;
        _walking = false;
        EndPace();
        me->SetControlled(false, UNIT_STATE_ROOT);
        for (ObjectGuid const& guid : _props)
            if (Creature* prop = me->GetMap()->GetCreature(guid))
                prop->DespawnOrUnsummon();
        _props.clear();
        _summons.DespawnAll();
        GroundIndicators::ClearAreasOf(me);
        _timeline.clear();
        _deaths.clear();
        _fallen.clear();
        _next = 0;
        _nextCheckMs = 0;
        _wallFrom = 0;
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
    uint32 _wallFrom = 0;
    bool _musicOver = false;
    bool _held = false;
    bool _landed = false;
    bool _walking = false;
    float _troughAngle = 0.0f;
    float _crucibleAngle = 0.0f;
    std::vector<ObjectGuid> _props;
    std::map<std::string, uint32> _deaths;
    std::set<ObjectGuid> _fallen;
};

// --- Commands ---------------------------------------------------------------------------------------------------------
using namespace Acore::ChatCommands;

// .vrogar info | skip <seconds> | cast <pattern> | lab [off] | pull
class ForgeMasterCommandScript final : public CommandScript
{
public:
    ForgeMasterCommandScript() : CommandScript("ForgeMasterCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable vrogarTable = {
            { "info", HandleInfo, SEC_GAMEMASTER, Console::No },
            { "skip", HandleSkip, SEC_GAMEMASTER, Console::No },
            { "cast", HandleCast, SEC_GAMEMASTER, Console::No },
            { "lab", HandleLab, SEC_GAMEMASTER, Console::No },
            { "pull", HandlePull, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable = {
            { "vrogar", vrogarTable },
        };
        return commandTable;
    }

    static boss_forge_master* FindVrogar(Player* player)
    {
        Creature* vrogar = player ? player->FindNearestCreature(NPC_VROGAR, 250.0f) : nullptr;
        return vrogar ? CAST_AI(boss_forge_master, vrogar->AI()) : nullptr;
    }

    static bool HandleInfo(ChatHandler* handler)
    {
        boss_forge_master* vrogar = FindVrogar(handler->GetPlayer());
        handler->SendSysMessage(vrogar ? vrogar->Describe() : std::string("Vrogar is not within 250 yards."));
        return true;
    }

    static bool HandleSkip(ChatHandler* handler, uint32 seconds)
    {
        boss_forge_master* vrogar = FindVrogar(handler->GetPlayer());
        if (!vrogar || !vrogar->Skip(seconds))
        {
            handler->SendErrorMessage("Vrogar is not fighting within 250 yards.");
            return false;
        }
        handler->SendSysMessage(vrogar->Describe());
        return true;
    }

    // .vrogar cast <anvil|pour|sparks|chains|laminoir|laminoir2|trempe|trempe2|bellows|bellows2|ingots|ingots2|hammer|
    // hammergroup|fonte|check>
    static bool HandleCast(ChatHandler* handler, std::string step)
    {
        boss_forge_master* vrogar = FindVrogar(handler->GetPlayer());
        if (!vrogar || !vrogar->CastNow(step))
        {
            handler->SendErrorMessage("No such pattern, or Vrogar is neither fighting nor rehearsing within 250 yards.");
            return false;
        }
        return true;
    }

    // .vrogar lab: in the FX lab, Vrogar at its middle in rehearsal (his arena and props up, harmless); .vrogar lab off:
    // gone
    static bool HandleLab(ChatHandler* handler, Optional<std::string> off)
    {
        Player* player = handler->GetPlayer();
        if (!player || player->GetMapId() != MapFxLab)
        {
            handler->SendErrorMessage("In the FX lab only (.fxlab).");
            return false;
        }
        if (Creature* existing = player->FindNearestCreature(NPC_VROGAR, 250.0f))
            existing->DespawnOrUnsummon(0ms);
        if (off && *off == "off")
            return true;
        TempSummon* vrogar = player->SummonCreature(NPC_VROGAR, LabMiddle, TEMPSUMMON_MANUAL_DESPAWN);
        if (!vrogar)
            return false;
        if (boss_forge_master* ai = CAST_AI(boss_forge_master, vrogar->AI()))
            ai->Rehearse();
        handler->SendSysMessage("Vrogar rehearses at the lab's middle: .vrogar cast <pattern>.");
        return true;
    }

    static bool HandlePull(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        Creature* vrogar = player ? player->FindNearestCreature(NPC_VROGAR, 150.0f) : nullptr;
        if (!vrogar || !vrogar->IsAlive())
        {
            handler->SendErrorMessage("No living Vrogar within 150 yards.");
            return false;
        }
        vrogar->SetReactState(REACT_AGGRESSIVE);
        vrogar->AI()->AttackStart(player);
        handler->SendSysMessage("Vrogar pulled.");
        return true;
    }
};
}

void AddForgeMasterScripts()
{
    RegisterCreatureAI(boss_forge_master);
    new ForgeMasterCommandScript();
}
