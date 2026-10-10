#ifndef MOD_STAT_GROWTH_DEFI_BOSS_H
#define MOD_STAT_GROWTH_DEFI_BOSS_H

#include "EvolutionsAudio.h"
#include "FightMusic.h"
#include "MythicTuning.h"

#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Group.h"
#include "LiveTuning.h"
#include "Log.h"
#include "Map.h"
#include "PathGenerator.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PowerScaling.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
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

private:
    static float PathLength(PathGenerator const& path)
    {
        Movement::PointsArray const& points = path.GetPath();
        float length = 0.0f;
        for (size_t index = 1; index < points.size(); ++index)
            length += (points[index] - points[index - 1]).length();
        return length;
    }

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

    // Whether a spot of the room is walked to from its middle in a straight enough line (not past a wall, not off
    // the floor): where an add may stand
    bool Reachable(Position const& at) const
    {
        Position const center = Ground(_room.center);
        float const distance = center.GetExactDist2d(&at);
        PathGenerator path(me);
        path.SetUseRaycast(false);
        if (!path.CalculatePath(center.GetPositionX(), center.GetPositionY(), center.GetPositionZ(),
                at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), false) ||
            path.GetPathType() != PATHFIND_NORMAL)
            return false;
        return PathLength(path) <= distance * 1.15f + 2.0f &&
            std::hypot(path.GetActualEndPosition().x - at.GetPositionX(),
                path.GetActualEndPosition().y - at.GetPositionY()) <= 1.5f &&
            std::fabs(path.GetActualEndPosition().z - at.GetPositionZ()) <= 3.0f;
    }

    // A spot distance from the room's middle the players can reach, at a random angle (from angle on, if given);
    // nearer in, a yard at a time, where the room is narrower
    Position EdgeSpot(float distance, float angle = -1.0f) const
    {
        float const first = angle < 0.0f ? frand(0.0f, 2.0f * float(M_PI)) : angle;
        for (float reach = distance; reach >= 5.0f; reach -= 3.0f)
            for (uint32 step = 0; step < 12; ++step)
            {
                Position const at = AtAngle(_room.center, first + float(step) * 2.0f * float(M_PI) / 12.0f, reach);
                if (Reachable(at))
                    return at;
            }
        return Ground(_room.center);
    }

public:
    // How far the room's floor reaches from its middle, in `directions` directions (clockwise from north): a yard at a
    // time until the floor drops or rises more than 2 yards, or a wall stands in the line of sight - for laying a fight
    // out (a game master's command)
    std::vector<float> MeasureRoom(uint32 directions = 36, float maxReach = 80.0f) const
    {
        std::vector<float> reach;
        Position const center = Ground(_room.center);
        for (uint32 index = 0; index < directions; ++index)
        {
            float const angle = Position::NormalizeOrientation(-float(index) * 2.0f * float(M_PI) / float(directions));
            float last = center.GetPositionZ();
            float distance = 1.0f;
            for (; distance <= maxReach; distance += 1.0f)
            {
                float const x = center.GetPositionX() + std::cos(angle) * distance;
                float const y = center.GetPositionY() + std::sin(angle) * distance;
                float const z = me->GetMap()->GetHeight(me->GetPhaseMask(), x, y, last + 5.0f, true, 15.0f);
                if (z <= INVALID_HEIGHT || std::fabs(z - last) > 2.0f)
                    break;
                Position const at(x, y, z);
                // Walked to, not seen: a path from the middle no longer straight is past a wall (the line of sight
                // went through the Blood Furnace's walls)
                PathGenerator path(me);
                path.SetUseRaycast(false);
                if (!path.CalculatePath(center.GetPositionX(), center.GetPositionY(), center.GetPositionZ(),
                        at.GetPositionX(), at.GetPositionY(), at.GetPositionZ(), false) ||
                    path.GetPathType() != PATHFIND_NORMAL || PathLength(path) > distance * 1.15f + 2.0f ||
                    std::hypot(path.GetActualEndPosition().x - x, path.GetActualEndPosition().y - y) > 1.5f)
                    break;
                last = at.GetPositionZ();
            }
            reach.push_back(distance - 1.0f);
        }
        return reach;
    }

protected:

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

    // --- What it deals ----------------------------------------------------------------------------------------------
    // The reference health its hits are shares of: a player of its profile (Power::ExpectedPlayerHealth)
    float Reference() const
    {
        return Power::ExpectedPlayerHealth(GetSizing().itemLevel, GetSizing().paragon);
    }

    // What makes a player take more (a fight's own debuffs): 1 by default
    virtual float TakenFactor(Unit const* /*victim*/) const { return 1.0f; }

    // A share of the reference health as spell (its name in the log and the death recap): the Défi tier's factor
    // applies on the way (MythicTuning::DealAbilityDamage), then the player's defences
    void Hit(Player* player, uint32 spellId, float percent)
    {
        if (!player || !player->IsAlive())
            return;
        float const amount = Reference() * percent / 100.0f * TakenFactor(player);
        MythicTuning::DealAbilityDamage(me, player, spellId, uint32(std::max(1.0f, amount)));
    }

    void HitEveryone(uint32 spellId, float percent)
    {
        for (Player* player : ArenaPlayers())
            Hit(player, spellId, percent);
    }

    // A rule broken where the rule is binary: death, whatever protects them, the spell named in the log
    void Doom(Player* player, uint32 spellId)
    {
        if (!player || !player->IsAlive())
            return;
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
        {
            SpellNonMeleeDamage log(me, player, info, info->GetSchoolMask());
            log.damage = player->GetHealth();
            me->SendSpellNonMeleeDamageLog(&log);
        }
        Unit::Kill(me, player, true, BASE_ATTACK, sSpellMgr->GetSpellInfo(spellId));
    }

    // A debuff of the fight's (a dummy aura) for durationMs, with stacks
    void AddTimedAura(Unit* target, uint32 spellId, uint32 durationMs, uint8 stacks = 1)
    {
        if (!target || !sSpellMgr->GetSpellInfo(spellId))
            return;
        Aura* aura = target->GetAura(spellId);
        if (!aura)
            aura = me->AddAura(spellId, target);
        if (!aura)
            return;
        aura->SetMaxDuration(int32(durationMs));
        aura->SetDuration(int32(durationMs));
        aura->SetStackAmount(std::max<uint8>(1, stacks));
    }

    uint8 StacksOf(Unit const* target, uint32 spellId) const
    {
        Aura const* aura = target ? target->GetAura(spellId) : nullptr;
        return aura ? aura->GetStackAmount() : 0;
    }

    // --- Its casts -----------------------------------------------------------------------------------------------
    // A step on its cast bar: it stops and casts for as long as the step takes, rooted (chasing its tank broke the
    // cast and the bar went while the step still came)
    void CastBar(uint32 castSpell)
    {
        me->StopMoving();
        me->SetControlled(true, UNIT_STATE_ROOT);
        _rooted = true;
        if (sSpellMgr->GetSpellInfo(castSpell))
            me->CastSpell(me, castSpell, false);
    }

    void EndCastBar()
    {
        if (_rooted)
        {
            _rooted = false;
            me->SetControlled(false, UNIT_STATE_ROOT);
        }
    }

    bool CastingBar() const { return _rooted; }

    // --- Its adds ------------------------------------------------------------------------------------------------
    // An add's health: the seconds it lasts against the profile's group - alone (its single-target damage), or as one
    // of a pack (its share of the group's area damage: Power::ExpectedDps's pack is the damage on five, all of them)
    void SizeAdd(Creature* add, float seconds, bool pack) const
    {
        Sizing const& sizing = GetSizing();
        float const dealers = Power::GroupDamageDealers(sizing.damageDealers, sizing.tanks, sizing.healers);
        float const dps = Power::ExpectedDps(sizing.itemLevel, sizing.paragon, pack) / (pack ? PackOf : 1.0f);
        uint32 const health = uint32(std::max(1.0f, dps * dealers * seconds));
        add->SetCreateHealth(health);
        add->SetMaxHealth(health);
        add->SetFullHealth();
    }

    // The skull of the room's group on target (the bots attack it first), or taken off (target nullptr)
    void SetSkull(Unit* target)
    {
        for (Player* player : ArenaPlayers())
            if (Group* group = player->GetGroup())
            {
                group->SetTargetIcon(7, ObjectGuid::Empty, target ? target->GetGUID() : ObjectGuid::Empty);
                return;
            }
    }

    // --- A beat of its music -------------------------------------------------------------------------------------
    // fn on that beat of grid (at once if it is past)
    template <typename Fn>
    void AtBeat(BeatGrid const& grid, uint32 beat, Fn fn)
    {
        uint32 const at = grid.At(beat);
        uint32 const now = Elapsed();
        scheduler.Schedule(Milliseconds(at > now ? at - now : 0), [fn](TaskContext) { fn(); });
    }

    uint32 CurrentBeat(BeatGrid const& grid) const
    {
        float const beat = grid.BeatOf(Elapsed());
        return beat > 0.0f ? uint32(std::ceil(beat - 0.05f)) : 0;
    }

    // --- What it tells the players ---------------------------------------------------------------------------------
    // A line in the middle of a player's screen (the raid boss emote frame), in their language; whisper: to them
    // alone, in its own colour (what hit them). Bots are told nothing.
    void Announce(Player* player, std::string const& french, std::string const& english, bool whisper = false) const
    {
        if (!player || !player->GetSession() || player->GetSession()->IsBot())
            return;
        bool const isFrench = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
        WorldPacket packet;
        ChatHandler::BuildChatPacket(packet, whisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_RAID_BOSS_EMOTE,
            LANG_UNIVERSAL, me, player, isFrench ? french : english);
        player->GetSession()->SendPacket(&packet);
    }

    // The same line to everyone who hears the fight
    void AnnounceAll(std::string const& french, std::string const& english) const
    {
        for (Player* player : Listeners())
            Announce(player, french, english);
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
    static constexpr uint32 GraceMs = 8000;
    static constexpr float PackOf = 5.0f;       // the targets Power::ReferencePackDps is measured on     // an instance not known as a challenge's this long: the boss goes

    Room _room;
    uint32 _pullMs = 0;
    bool _clockOn = false;
    uint32 _checkTimer = 0;
    uint32 _defiWaitMs = 0;
    uint32 _lastKillYellMs = 0;
    bool _defiConfirmed = false;
    bool _rooted = false;
    std::set<ObjectGuid> _musicListeners;
};
}

#endif
