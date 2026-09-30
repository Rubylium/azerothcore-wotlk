#include "MythicTuning.h"

#include "CellImpl.h"
#include "Chat.h"
#include "CommandScript.h"
#include "CreatureScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PowerScaling.h"
#include "ScriptedCreature.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <list>
#include <set>
#include <string>
#include <vector>

// The Hollow Voice, the pinnacle of the Défi board: a 10-player fight of the board's own in Sunwell Plateau's M'uru
// chamber, for item level 460 and 650 paragon (plan: .agents/plans/hollow-voice). Like L'Infini (InfiniteGod.cpp) it
// runs on a fixed timeline set by its two tracks, played as one (patch-Z Sound\Music\Evolutions\HollowVoice.mp3,
// SoundEntries 30112; localTools/hollowVoice/buildMusic.py joins them):
//
// 0:00 Archbishop Aldric Dawnmantle (his track, up to 2:02.0 in its fade). His health stops at 1%. At 1:59.7, as his
//      track fades: at 1% he falls; above it, Last Rites - a holy judgement kills the group, and the demon is never
//      seen.
// 2:02.0 Vel'thazar's track (5:30) follows; on its first hit (2:03.5) Vel'thazar, the
//      Hollow Voice, tears out of the Archbishop, who stays hidden. Killing the demon frees and kills the Archbishop:
//      his kill is the board's win (RaidFinder follows the board's boss, 930100).
// 7:19.1 / 7:19.7 / 7:20.1 (the track's BAM BAM BAM, 5:17.1-5:18.1): the hard enrage, three blasts of three times
//      everyone's health, then a pulse every second that kills whatever protects them.
//
// This is the fight's frame: phases, music, the 1% hold, the reveal, the wipes and the enrage. The abilities come on it
// by increments. Only in a challenge's instance: the static spawn hides at once and goes from any other Sunwell
// (IsChallengeInstanceFor, mod-playerbots RaidFinder.cpp), and clears M'uru's chamber of its own occupants.

// mod-playerbots (RaidFinder.cpp), built into the same modules library
bool IsChallengeInstanceFor(Map const* map, uint32 bossEntry);
float GetChallengeDamageFactorOf(Unit* attacker);

namespace
{
// --- Tuning ---------------------------------------------------------------------------------------------------------
// The profile it is made for (mod-playerbots ChallengeTiers.h BossProfiles): hits are shares of the health of a
// damage dealer at it (Power::ExpectedPlayerHealth, about 221 000), the Défi tier's damage factor on top
constexpr float ProfileItemLevel = 460.0f;
constexpr float ProfileParagon = 650.0f;
constexpr float AldricMeleeFloorPct = 10.0f;    // his melee on a player: at least this, whatever their armour
constexpr float VelthazarMeleeFloorPct = 14.0f;
constexpr float HoldHealthPct = 1.0f;           // the Archbishop's health stops here
constexpr float EnrageBlastHealthPct = 300.0f;  // each of the three blasts, of the target's maximum health
constexpr uint32 EnragePulseMs = 1000;          // then a pulse that kills, whatever protects them
constexpr uint32 FinishPulses = 3;              // the bots left alone: pulses, the last one kills them
constexpr uint32 FinishPulseMs = 1000;
constexpr Milliseconds WipeLinger = 8s;         // a wipe: it stands this long, the music on, before it goes
constexpr Seconds WipeRespawnDelay = 5s;
constexpr uint32 DefiGraceMs = 8000;            // players are waited for this long before a non-challenge's goes

// M'uru's chamber: its middle (where M'uru floats), players this close are in the fight
constexpr float ArenaReach = 60.0f;
constexpr float MusicReach = 120.0f;
constexpr float ChamberClearRadius = 60.0f;     // the chamber's own occupants this close go (M'uru, its guards)

// --- Timeline (ms from the pull), on the tracks' beats (measured on their loudness) ---------------------------------
constexpr uint32 AtJudgement = 119700;          // the Archbishop's track fades: he falls at 1%, or Last Rites
// Vel'thazar's track follows in the fade of his (1:59.7-2:03.4), in the one file. Sent as a second music, it never
// played through: the first reaching its own end stopped it (sent at 2:03.45: not heard; at 2:02.0: cut a few seconds
// in, 2026-09-30).
constexpr uint32 AtSecondTrack = 122000;
constexpr uint32 TrackEndMarginMs = 500;       // a wipe before it ends the track this long before it
constexpr uint32 AtReveal = AtSecondTrack + 1500;               // its first hit
constexpr std::array<uint32, 3> AtEnrageBlasts = { AtSecondTrack + 317090, AtSecondTrack + 317650,
                                                   AtSecondTrack + 318100 };

constexpr uint32 NPC_ALDRIC = 930100;
constexpr uint32 NPC_VELTHAZAR = 930101;

// SoundEntries (localTools/patchSinisterStrike.ps1): the fight's track, the two alone (.hollow music, and a clock
// skipped past the switch), L'Infini's silence to end them, the abilities' sounds
constexpr uint32 MUSIC_FIGHT = 30112;
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

// The combat log's names, stock spells until the fight has its own
constexpr uint32 SPELL_LAST_RITES = 48817;      // Holy Wrath
constexpr uint32 SPELL_SILENCE = 47809;         // Shadow Bolt

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
    Aldric,                                     // 0:00-1:59.7
    Fallen,                                     // at 1%, his track fading: the demon is about to show
    Hollow,                                     // Vel'thazar
    Over,                                       // a kill or a wipe
};

float Reference()
{
    return Power::ExpectedPlayerHealth(ProfileItemLevel, ProfileParagon);
}

bool IsRealPlayer(Player const* player)
{
    return player->GetSession() && !player->GetSession()->IsBot();
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

struct boss_hollow_voice_velthazar : public ScriptedAI
{
    boss_hollow_voice_velthazar(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override { }

    // The Archbishop ends the fight: a wipe is his to see (boss_hollow_voice_aldric Wipe)
    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && !_enraged && KillYellReady(_lastKillYellMs))
            Talk(SAY_VELTHAZAR_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_VELTHAZAR_DEATH);
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type == DIRECT_DAMAGE && victim && victim->IsPlayer())
            damage = uint32(std::max(float(damage), Reference() * VelthazarMeleeFloorPct / 100.0f *
                                     std::max(GetChallengeDamageFactorOf(me), 1.0f)));
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!UpdateVictim())
            return;
        DoMeleeAttackIfReady();
    }

    bool _enraged = false;

private:
    uint32 _lastKillYellMs = 0;
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
        Talk(SAY_ALDRIC_AGGRO);
        _fightListeners.clear();
        for (Player* player : Listeners())
        {
            SendMusic(player, MUSIC_FIGHT);
            _fightListeners.insert(player->GetGUID());
        }
        LOG_INFO("module.hollowvoice", "The Hollow Voice pulled instance={} health={} tier factor={}",
                 me->GetInstanceId(), me->GetMaxHealth(), GetChallengeDamageFactorOf(me));
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer() && _phase == Phase::Aldric && KillYellReady(_lastKillYellMs))
            Talk(SAY_ALDRIC_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        EndTrack(MUSIC_SILENCE);
        LOG_INFO("module.hollowvoice", "The Hollow Voice killed instance={} elapsed={}ms", me->GetInstanceId(),
                 Elapsed());
        // The demon's corpse stays with his
        scheduler.CancelAll();
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

    // His health stops at 1%; fallen or hidden, nothing reaches him
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (_phase == Phase::Fallen || _phase == Phase::Hollow)
        {
            damage = 0;
            return;
        }
        if (_phase != Phase::Aldric)
            return;
        uint32 const floor = HoldHealth();
        uint32 const health = uint32(me->GetHealth());
        if (health <= floor)
            damage = 0;
        else if (damage >= health - floor)
            damage = health - floor;
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType type, SpellSchoolMask /*mask*/) override
    {
        if (type == DIRECT_DAMAGE && victim && victim->IsPlayer())
            damage = uint32(std::max(float(damage), Reference() * AldricMeleeFloorPct / 100.0f *
                                     std::max(GetChallengeDamageFactorOf(me), 1.0f)));
    }

    void UpdateAI(uint32 diff) override
    {
        // Fallen, then hidden: he runs the clock with no one to fight
        if (_phase == Phase::Fallen || _phase == Phase::Hollow)
        {
            scheduler.Update(diff);
            UpdateChamber(diff);
            UpdateClock();
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
        UpdateClock();
        UpdateStanding();
        if (_phase == Phase::Aldric)
            DoMeleeAttackIfReady();
    }

    // --- The game master's commands --------------------------------------------------------------------------------
    std::string Describe() const
    {
        return Acore::StringFormat("The Hollow Voice: phase {} at {:.1f}s, Aldric {}/{} ({:.1f}%), Vel'thazar {}, "
                                   "tier damage x{:.2f}, reference {:.0f}", uint32(_phase), Elapsed() / 1000.0f,
                                   me->GetHealth(), me->GetMaxHealth(), me->GetHealthPct(),
                                   Velthazar() ? Acore::StringFormat("{}/{}", Velthazar()->GetHealth(),
                                                                     Velthazar()->GetMaxHealth()) : std::string("-"),
                                   GetChallengeDamageFactorOf(me), Reference());
    }

    // Jumps the fight forward (testing): the steps passed over run at once
    bool Skip(uint32 seconds)
    {
        if (_phase == Phase::None || _phase == Phase::Over)
            return false;
        _pullMs -= seconds * IN_MILLISECONDS;
        _skipped = true;
        UpdateClock();
        return true;
    }

    // The Archbishop at 1%, the clock just before he falls (testing the reveal)
    bool SkipToReveal()
    {
        if (_phase != Phase::Aldric)
            return false;
        me->SetHealth(HoldHealth());
        uint32 const elapsed = Elapsed();
        if (elapsed + 1000 < AtJudgement)
            _pullMs -= AtJudgement - 1000 - elapsed;
        _skipped = true;
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
                summon && summon->GetEntry() == NPC_VELTHAZAR)
                return summon;
        return nullptr;
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
                !creature->IsCharmedOwnedByPlayerOrPlayer() && creature->IsHostileToPlayers())
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
        _phase = Phase::None;
        _secondTrack = false;
        _skipped = false;
        _blasts = 0;
        _nextPulseMs = 0;
        _finishing = false;
        _nextStandingCheckMs = 0;
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_STAND);
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

    // --- Music: sent as stock encounters send theirs (SMSG_PLAY_MUSIC); only a music ends a music -------------------
    void EndTrack(uint32 soundId)
    {
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

    void PlayTrack(uint32 soundId)
    {
        for (Player* player : Listeners())
        {
            SendMusic(player, soundId);
            _fightListeners.insert(player->GetGUID());
        }
    }

    // --- The clock -------------------------------------------------------------------------------------------------
    void UpdateClock()
    {
        uint32 const elapsed = Elapsed();
        if (_phase == Phase::Aldric && elapsed >= AtJudgement)
            Judge();
        // The fight's track goes on to Vel'thazar's by itself; a clock skipped forward (.hollow skip, reveal) left it
        // behind, and his is sent on its own
        if (_phase == Phase::Fallen && _skipped && !_secondTrack && elapsed >= AtSecondTrack)
        {
            _secondTrack = true;
            PlayTrack(MUSIC_VELTHAZAR);
        }
        if (_phase == Phase::Fallen && elapsed >= AtReveal)
            Reveal();
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

    // 1:59.7, his track fading: at 1% he falls, the demon about to show; above it the group was too slow
    void Judge()
    {
        if (me->GetHealth() > HoldHealth())
        {
            LastRites();
            return;
        }
        _phase = Phase::Fallen;
        Talk(SAY_ALDRIC_FALLS);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        me->GetMotionMaster()->Clear();
        me->StopMoving();
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Aldric falls instance={}", me->GetInstanceId());
    }

    // Too slow: a holy judgement on everyone in the chamber, and the demon is never seen
    void LastRites()
    {
        _phase = Phase::Over;
        Talk(SAY_ALDRIC_LAST_RITES);
        me->PlayDirectSound(SOUND_LAST_RITES);
        for (Player* player : ArenaPlayers())
        {
            MythicTuning::DealAbilityDamage(me, player, SPELL_LAST_RITES, player->GetMaxHealth());
            if (player->IsAlive())
                Unit::Kill(me, player);
        }
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Last Rites (too slow) instance={} health={:.1f}%",
                 me->GetInstanceId(), me->GetHealthPct());
        Wipe();
    }

    // 2:03.5, the second track's first hit: Vel'thazar tears out of the Archbishop, who is no longer seen
    void Reveal()
    {
        _phase = Phase::Hollow;
        me->PlayDirectSound(SOUND_REVEAL);
        Position const at = me->GetPosition();
        Creature* demon = me->SummonCreature(NPC_VELTHAZAR, at, TEMPSUMMON_MANUAL_DESPAWN);
        me->SetVisible(false);
        if (!demon)
        {
            LOG_ERROR("module.hollowvoice", "The Hollow Voice: Vel'thazar could not be summoned instance={}",
                      me->GetInstanceId());
            return;
        }
        demon->AI()->Talk(SAY_VELTHAZAR_REVEAL);
        demon->SetReactState(REACT_AGGRESSIVE);
        DoZoneInCombat(demon, ArenaReach);
        LOG_INFO("module.hollowvoice", "The Hollow Voice: Vel'thazar revealed instance={} health={}",
                 me->GetInstanceId(), demon->GetMaxHealth());
    }

    // 7:19.1 / 7:19.7 / 7:20.1: three blasts of three times everyone's health (an immunity still holds)...
    void EnrageBlast(uint32 index)
    {
        Creature* demon = Velthazar();
        Unit* source = demon ? static_cast<Unit*>(demon) : me;
        if (demon && index == 0)
        {
            if (auto* ai = dynamic_cast<boss_hollow_voice_velthazar*>(demon->AI()))
                ai->_enraged = true;
            demon->AI()->Talk(SAY_VELTHAZAR_ENRAGE);
        }
        source->PlayDirectSound(SOUND_HARD_ENRAGE);
        for (Player* player : ArenaPlayers())
            MythicTuning::DealAbilityDamage(source, player, SPELL_SILENCE,
                                            uint32(float(player->GetMaxHealth()) * EnrageBlastHealthPct / 100.0f));
    }

    // ... then a pulse every second that kills, whatever protects them
    void EnragePulse()
    {
        Creature* demon = Velthazar();
        Unit* source = demon ? static_cast<Unit*>(demon) : me;
        for (Player* player : ArenaPlayers())
            Unit::Kill(source, player);
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
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        if (Creature* demon = Velthazar())
        {
            demon->AttackStop();
            demon->SetReactState(REACT_PASSIVE);
        }
        uint32 const elapsed = Elapsed();
        LOG_INFO("module.hollowvoice", "The Hollow Voice wipe instance={} elapsed={}ms", me->GetInstanceId(), elapsed);
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
            me->CombatStop(true);
            me->SetVisible(true);
            me->DespawnOnEvade(WipeRespawnDelay);
        }, WipeLinger);
    }

    SummonList _summons;
    Phase _phase = Phase::None;
    uint32 _pullMs = 0;
    bool _defiConfirmed = false;
    bool _lingering = false;
    bool _secondTrack = false;
    bool _skipped = false;
    bool _finishing = false;
    uint32 _defiWaitMs = 0;
    uint32 _checkTimer = 0;
    uint32 _nextStandingCheckMs = 0;
    uint32 _blasts = 0;
    uint32 _nextPulseMs = 0;
    uint32 _lastKillYellMs = 0;
    std::set<ObjectGuid> _fightListeners;
};

boss_hollow_voice_aldric* FindAldric(Player* player)
{
    if (!player || !player->IsInWorld())
        return nullptr;
    Creature* aldric = player->FindNearestCreature(NPC_ALDRIC, 250.0f, true);
    return aldric ? dynamic_cast<boss_hollow_voice_aldric*>(aldric->AI()) : nullptr;
}

using namespace Acore::ChatCommands;

// .hollow info | skip <seconds> | reveal | pull | music <aldric|velthazar|stop>: for game masters trying the fight.
// The fight itself starts from the board: .defi start 930100 (hidden from the boards until it is revealed).
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
            { "music",  HandleMusic,  SEC_GAMEMASTER, Console::No },
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

    // .hollow reveal: the Archbishop at 1% a second before he falls
    static bool HandleReveal(ChatHandler* handler)
    {
        boss_hollow_voice_aldric* aldric = FindAldric(handler->GetPlayer());
        if (!aldric || !aldric->SkipToReveal())
            return Fail(handler, "The Archbishop is not fighting within 250 yards.");
        handler->SendSysMessage(aldric->Describe());
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
    new HollowVoiceCommandScript();
}
