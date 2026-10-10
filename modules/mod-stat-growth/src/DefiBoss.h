#ifndef MOD_STAT_GROWTH_DEFI_BOSS_H
#define MOD_STAT_GROWTH_DEFI_BOSS_H

#include "EvolutionsAudio.h"
#include "FightMusic.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "LiveTuning.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PowerScaling.h"
#include "ScriptedCreature.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <set>
#include <vector>

// mod-playerbots (RaidFinder.cpp), built into the same modules library
bool IsChallengeInstanceFor(Map const* map, uint32 bossEntry);
float GetChallengeDamageFactorOf(Unit* attacker);

// What every Défi board boss of ours does alike, so that its script holds its fight only (plan of the Gaol's second
// gate, .agents/plans/escape-hunter: the boss scripts had grown to thousands of lines, half of it the same in each):
// - a static spawn that exists only in a challenge's instance: hidden until the board's group is known to be there,
//   gone from any other instance of the map, the room cleared of its own occupants and their encounter;
// - its health from the power model (the profile's group over the seconds it can be hit);
// - its music, sent at the pull to everyone near and silenced at the end for those still hearing it (FightMusic);
// - the fight's clock, and who the fight takes (alive, in reach, not a game master).
namespace Defi
{
// The group a boss is sized for (mod-playerbots ChallengeTiers.h BossProfiles) and the seconds it can be hit
struct Sizing
{
    float itemLevel;
    float paragon;
    float damageDealers;
    float tanks;
    float healers;
    float uptimeSeconds;
    float groupShare;           // a smaller group's buffs against Power::RaidCurve's teams (Vorhan's 8: 1.095 / 1.18)
};

// Where a boss fights
struct Room
{
    Position center;
    float reach;                // who the fight takes
    float musicReach;           // who hears its music
    float clearRadius;          // the room's own occupants, cleared in a challenge's instance
};

// A track's beat grid, measured once on the file (a line fitted to its beats): beat k at startMs + k beatMs. A fight
// on its music places every step on a beat number.
struct BeatGrid
{
    float startMs;
    float beatMs;

    uint32 At(uint32 beat) const { return uint32(startMs + beatMs * float(beat)); }
    float BeatOf(uint32 elapsedMs) const { return (float(elapsedMs) - startMs) / beatMs; }
};

class BossAI : public ScriptedAI
{
public:
    BossAI(Creature* creature, Room const& room, char const* logName) : ScriptedAI(creature), _summons(creature),
        _logName(logName), _room(room) { }

    void InitializeAI() override
    {
        if (!_defiConfirmed)
            me->SetVisible(false);
        ScriptedAI::InitializeAI();
    }

    void JustSummoned(Creature* summon) override { _summons.Summon(summon); }
    void SummonedCreatureDespawn(Creature* summon) override { _summons.Despawn(summon); }

protected:
    // --- To give ---------------------------------------------------------------------------------------------------
    virtual Sizing const& GetSizing() const = 0;
    virtual float HealthScale() const { return 1.0f; }
    // A creature of the room the boss's own (its adds, its markers): never cleared
    virtual bool IsOwn(Creature const* /*creature*/) const { return false; }

    // --- The challenge's instance ----------------------------------------------------------------------------------
    // Out of combat, once a second: a challenge's instance confirmed (shown, sized, the room cleared) or left
    void UpdateOutOfCombat(uint32 diff)
    {
        _checkTimer += diff;
        if (_checkTimer < 1000)
            return;
        _checkTimer = 0;
        if (_defiConfirmed)
        {
            ClearRoom();
            return;
        }
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
            SetModelHealth();
            ClearRoom();
            LOG_INFO(_logName, "{} appears for a challenge instance={} health={}", me->GetName(), me->GetInstanceId(),
                     me->GetMaxHealth());
            return;
        }
        _defiWaitMs += 1000;
        if (_defiWaitMs >= GraceMs)
        {
            LOG_INFO(_logName, "{} leaves an instance that is no challenge's instance={}", me->GetName(),
                     me->GetInstanceId());
            me->DespawnOrUnsummon(0ms, Seconds(7 * DAY));
        }
    }

    bool DefiConfirmed() const { return _defiConfirmed; }

    // The health of the profile's group over its seconds (Power::RaidBossHealth), times HealthScale
    void SetModelHealth()
    {
        Sizing const& sizing = GetSizing();
        float const dealers = Power::GroupDamageDealers(sizing.damageDealers, sizing.tanks, sizing.healers);
        float const health = Power::RaidBossHealth(sizing.itemLevel, sizing.paragon, dealers, sizing.uptimeSeconds) *
            sizing.groupShare * HealthScale();
        me->SetCreateHealth(uint32(health));
        me->SetMaxHealth(uint32(health));
        me->SetFullHealth();
    }

    // The room is the boss's: its hostile occupants go (triggers and the boss's own stay); no stock encounter is
    // fought in its instance, and one left in progress is set back
    void ClearRoom()
    {
        std::list<Creature*> found;
        Acore::AllWorldObjectsInRange check(me, _room.clearRadius);
        Acore::CreatureListSearcher<Acore::AllWorldObjectsInRange> searcher(me, found, check);
        Cell::VisitObjects(me, searcher, _room.clearRadius);
        for (Creature* creature : found)
            if (creature != me && creature->IsAlive() && !creature->IsTrigger() && !IsOwn(creature) &&
                !creature->IsCharmedOwnedByPlayerOrPlayer() && creature->IsHostileToPlayers())
                creature->DespawnOrUnsummon(0ms, Seconds(DAY));

        if (InstanceScript* instance = me->GetInstanceScript())
            for (uint32 boss = 0; boss < instance->GetEncounterCount(); ++boss)
                if (instance->GetBossState(boss) == IN_PROGRESS)
                    instance->SetBossState(boss, NOT_STARTED);
    }

    // --- The fight's clock -----------------------------------------------------------------------------------------
    void StartClock() { _pullMs = getMSTime(); _clockOn = true; }
    void StopClock() { _clockOn = false; }
    uint32 Elapsed() const { return _clockOn ? getMSTimeDiff(_pullMs, getMSTime()) : 0; }
    // Moves the clock forward (a game master's skip)
    void AdvanceClock(uint32 ms) { _pullMs -= ms; }

    // --- Who the fight takes -----------------------------------------------------------------------------------------
    // Alive in the room's reach, not game masters
    std::vector<Player*> ArenaPlayers() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (player && !player->IsGameMaster() && player->IsAlive() &&
                player->GetExactDist2d(&_room.center) <= _room.reach)
                players.push_back(player);
        }
        return players;
    }

    // Everyone near the room, dead or game masters too: who hears its music
    std::vector<Player*> Listeners() const
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource(); player && player->GetExactDist2d(&_room.center) <= _room.musicReach)
                players.push_back(player);
        return players;
    }

    // The ground under a point of the room
    Position Ground(Position const& at) const
    {
        float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), at.GetPositionX(), at.GetPositionY(),
            _room.center.GetPositionZ() + 5.0f, true, 15.0f);
        return Position(at.GetPositionX(), at.GetPositionY(), z > INVALID_HEIGHT ? z : _room.center.GetPositionZ(),
            at.GetOrientation());
    }

    Position AtAngle(Position const& from, float angle, float distance) const
    {
        return Ground(Position(from.GetPositionX() + std::cos(angle) * distance,
            from.GetPositionY() + std::sin(angle) * distance, from.GetPositionZ(), angle));
    }

    Room const& GetRoom() const { return _room; }

    // --- The music -------------------------------------------------------------------------------------------------
    // Its track to everyone near (the bots too are claimed: their owner's music is not theirs to end), from its start
    void StartMusic(char const* key, uint32 fadeInMs = 0)
    {
        _musicListeners.clear();
        for (Player* player : Listeners())
        {
            if (player->GetSession() && !player->GetSession()->IsBot())
                EvolutionsAudio::PlayMusic(player, key, fadeInMs);
            _musicListeners.insert(player->GetGUID());
            FightMusic::Claim(player->GetGUID(), me->GetGUID());
        }
    }

    // Silence for whoever still hears this fight's track
    void EndMusic()
    {
        for (ObjectGuid const& guid : _musicListeners)
            if (FightMusic::Release(guid, me->GetGUID()))
                if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
                    if (player->GetSession() && !player->GetSession()->IsBot())
                        EvolutionsAudio::StopMusic(player);
        _musicListeners.clear();
    }

    // A kill's yell, at most every 10 s
    bool KillYellReady()
    {
        uint32 const now = getMSTime();
        if (_lastKillYellMs && getMSTimeDiff(_lastKillYellMs, now) < 10000)
            return false;
        _lastKillYellMs = now;
        return true;
    }

    SummonList _summons;
    char const* _logName;

private:
    static constexpr uint32 GraceMs = 8000;     // an instance not known as a challenge's this long: the boss goes

    Room _room;
    uint32 _pullMs = 0;
    bool _clockOn = false;
    uint32 _checkTimer = 0;
    uint32 _defiWaitMs = 0;
    uint32 _lastKillYellMs = 0;
    bool _defiConfirmed = false;
    std::set<ObjectGuid> _musicListeners;
};
}

#endif
