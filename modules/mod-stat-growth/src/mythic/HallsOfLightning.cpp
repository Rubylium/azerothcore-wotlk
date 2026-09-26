#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "CreatureAIImpl.h"
#include "CreatureScript.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <initializer_list>
#include <list>
#include <vector>

// Halls of Lightning in Mythic+: damage tuning, boss reworks and trash abilities
// (audit: .agents/plans/mplus-damage-audit/).
//
// The four bosses are copies of the stock scripts (src/server/scripts/Northrend/Ulduar/HallsOfLightning/) under the
// ScriptNames *_evolutions (data/sql/db-world/base/stat_growth_mythic_halls_of_lightning.sql), each keeping the stock
// fight, its instance data and its texts, with its invisible or random mechanics drawn in red before they land:
// - General Bjarngrim: Mortal Strike is a cone at the tank he holds still for (the tank's defensive), Cleave a wide
//   cone in front of him (stand behind), Whirlwind is shown on him before he spins. No pre-pull electrical charge in a
//   key (it hid +30% damage behind the pull timing).
// - Volkhan: at 25% the brittle golems are drawn in red for the whole Shattering Stomp and shatter when it lands, not
//   when it starts; a Molten Golem that breaks overheats for a moment before its Blast Wave.
// - Ionar: Ball Lightning is a circle under its target that lands where it was drawn; Static Overload a circle carried
//   by its target (take it away from the group); every spark carries its circle (keep away from them, and apart).
// - Loken: the Pulsing Shockwave's rule is said on the pull, and in a key it no longer grows past 15 yards.
// Damage in a key is a share of the reference health (MythicTuning.h); elsewhere the heroic value, halved in normal.
namespace
{
constexpr char const* HallsOfLightningScript = "instance_halls_of_lightning";

template <class AI>
AI* GetHallsOfLightningEvolutionsAI(Creature* creature)
{
    return GetInstanceAI<AI>(creature, HallsOfLightningScript);
}

// The stock instance script's boss ids and data (halls_of_lightning.h, not reachable from a module)
enum HoLData : uint32
{
    DATA_BJARNGRIM              = 0,
    DATA_IONAR                  = 1,
    DATA_LOKEN                  = 2,
    DATA_VOLKHAN                = 3,

    DATA_BJARNGRIM_ACHIEVEMENT  = 10,
    DATA_VOLKHAN_ACHIEVEMENT    = 11,
};

struct AbilityDamage
{
    uint32 heroic;          // the spell value it replaces, heroic
    float mythicPercent;    // of the reference health, in a key
};

constexpr uint32 NormalPercent = 50;

bool InMythicKey(Unit const* unit)
{
    Map const* map = unit->FindMap();
    return map && map->IsMythic();
}

// Deals an ability's damage as spellId: a share of the reference health in a key, the heroic value (halved in normal)
// elsewhere. The target's defensives, armour or resistances and absorbs apply.
void DealHit(Unit* caster, Unit* target, uint32 spellId, AbilityDamage const& damage)
{
    if (!caster || !target || !target->IsAlive())
        return;

    if (InMythicKey(caster))
        MythicTuning::DealReferenceDamage(caster, target, spellId, damage.mythicPercent);
    else
        MythicTuning::DealAbilityDamage(caster, target, spellId,
            caster->GetMap()->IsHeroic() ? damage.heroic : damage.heroic * NormalPercent / 100);
}

std::vector<Player*> PlayersAround(Unit* source, float range)
{
    std::vector<Player*> players;
    for (auto const& ref : source->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->IsAlive() && !player->IsGameMaster() && source->IsWithinDistInMap(player, range))
                players.push_back(player);
    return players;
}

std::vector<Player*> PlayersIn(Unit* source, GroundIndicators::Area const& area)
{
    std::vector<Player*> inside;
    for (Player* player : PlayersAround(source, 100.0f))
        if (area.Contains(*player))
            inside.push_back(player);
    return inside;
}

// ----------------------------------------------------------------------------------------------------------------
// General Bjarngrim
// ----------------------------------------------------------------------------------------------------------------
namespace Bjarngrim
{
enum Spells : uint32
{
    // Defensive stance
    SPELL_DEFENSIVE_STANCE              = 53790,
    SPELL_DEFENSIVE_AURA                = 41105,
    SPELL_BJARNGRIM_REFLETION           = 36096,
    SPELL_PUMMEL                        = 12555,
    SPELL_KNOCK_AWAY                    = 52029,
    SPELL_IRONFORM                      = 52022,

    // Berserker stance
    SPELL_BERSERKER_STANCE              = 53791,
    SPELL_BERSERKER_AURA                = 41107,
    SPELL_MORTAL_STRIKE                 = 16856,
    SPELL_WHIRLWIND                     = 52027,

    // Battle stance
    SPELL_BATTLE_STANCE                 = 53792,
    SPELL_BATTLE_AURA                   = 41106,
    SPELL_INTERCEPT                     = 58769,
    SPELL_CLEAVE                        = 15284,
    SPELL_SLAM                          = 52026,

    SPELL_CHARGE_UP                     = 52098,
    SPELL_TEMPORARY_ELECTRICAL_CHARGE   = 52092,

    // Named in a key, where the cone's damage is the script's: a physical school-damage "Cleave" (15284 is a weapon
    // spell). Mortal Strike keeps its own id (a weapon spell: DealReferenceDamage deals the share asked for all the
    // same, the key's scaling being 1 on both sides) for its healing debuff.
    SPELL_CLEAVE_NAMED                  = 37476,
};

enum Misc : uint32
{
    STANCE_DEFENSIVE                    = 1,
    STANCE_BERSERKER                    = 2,
    STANCE_BATTLE                       = 3,

    NPC_STORMFORGED_LIEUTENANT          = 29240,

    EQUIP_SWORD                         = 37871,
    EQUIP_SHIELD                        = 35642,
    EQUIP_MACE                          = 43623,
};

enum Events : uint32
{
    EVENT_BJARNGRIM_CHANGE_STANCE       = 1,

    EVENT_BJARNGRIM_REFLECTION          = 11,
    EVENT_BJARNGRIM_PUMMEL              = 12,
    EVENT_BJARNGRIM_KNOCK               = 13,
    EVENT_BJARNGRIM_IRONFORM            = 14,

    EVENT_BJARNGRIM_MORTAL_STRIKE       = 21,
    EVENT_BJARNGRIM_WHIRLWIND           = 22,

    EVENT_BJARNGRIM_INTERCEPT           = 31,
    EVENT_BJARNGRIM_CLEAVE              = 32,
    EVENT_BJARNGRIM_SLAM                = 33,

    EVENT_CHARGE_UP                     = 51,
};

enum Texts : uint32
{
    SAY_AGGRO                           = 0,
    SAY_DEFENSIVE_STANCE                = 1,
    SAY_BATTLE_STANCE                   = 2,
    SAY_BERSERKER_STANCE                = 3,
    SAY_SLAY                            = 4,
    SAY_DEATH                           = 5,
};

enum Waypoints : uint32
{
    POINT_FIRST_PLATFORM                = 1,
    POINT_FIRST_PLATFORM_END            = 2,
    POINT_FIRST_PLATFORM_BACK           = 3,
    POINT_SECOND_PLATFORM               = 4,
    POINT_SECOND_PLATFORM_END           = 5,
    POINT_SECOND_PLATFORM_BACK          = 6,
    POINT_START                         = 7,
};

// Mortal Strike: a narrow cone at the tank; the tank takes the tank buster, anyone else in front the cleave's share
constexpr float MortalStrikeRadius = 8.0f;
constexpr float MortalStrikeArc = 60.0f;
constexpr uint32 MortalStrikeWarningMs = 2000;
constexpr float MortalStrikeTankPercent = 150.0f;   // physical: about 38% of a tank's health after armour
constexpr float MortalStrikeOthersPercent = 40.0f;
// Cleave: a wide cone in front of him
constexpr float CleaveRadius = 10.0f;
constexpr float CleaveArc = 90.0f;
constexpr uint32 CleaveWarningMs = 2000;
constexpr AbilityDamage CleaveDamage = { 7000, 40.0f };  // physical: about 30% of a damage dealer after armour
// Whirlwind: shown on him before he spins
constexpr float WhirlwindRadius = 8.0f;
constexpr uint32 WhirlwindWarningMs = 2000;

struct boss_bjarngrim_evolutions : public npc_escortAI
{
    boss_bjarngrim_evolutions(Creature* creature) : npc_escortAI(creature), _summons(creature),
        _stance(STANCE_BATTLE)
    {
        _instance = creature->GetInstanceScript();
        InitializeWaypoints();
        me->SetWalk(true);
        Start(true, ObjectGuid::Empty, nullptr, false, true);
    }

    void InitializeWaypoints()
    {
        AddWaypoint(POINT_FIRST_PLATFORM, 1262.0f, -26.9f, 33.5f, 10000);
        AddWaypoint(POINT_FIRST_PLATFORM_END, 1262.18f, 99.3f, 33.5f, 10000);
        AddWaypoint(POINT_FIRST_PLATFORM_BACK, 1262.0f, -26.9f, 33.5f, 0);
        AddWaypoint(POINT_SECOND_PLATFORM, 1332.0f, -26.6f, 40.18f, 10000);
        AddWaypoint(POINT_SECOND_PLATFORM_END, 1395.092f, 36.6425f, 50.038f, 10000);
        AddWaypoint(POINT_SECOND_PLATFORM_BACK, 1332.0f, -26.6f, 40.18f, 0);
        AddWaypoint(POINT_START, 1262.0f, -26.9f, 33.5f, 0);
    }

    void JustRespawned() override
    {
        npc_escortAI::JustRespawned();
        me->SetWalk(true);
        Start(true, ObjectGuid::Empty, nullptr, false, true);
    }

    void Reset() override
    {
        events.Reset();
        scheduler.CancelAll();
        EndWindup();
        _summons.DespawnAll();

        for (uint8 i = 0; i < 2; ++i)
            if (Creature* dwarf = me->SummonCreature(NPC_STORMFORGED_LIEUTENANT, me->GetPositionX() + urand(4, 12),
                me->GetPositionY() + urand(4, 12), me->GetPositionZ()))
            {
                float const angle = i == 0 ? 2.5f : 3.78f;
                dwarf->GetMotionMaster()->MoveFollow(me, 3, angle);
                _summons.Summon(dwarf);
            }

        me->RemoveAllAuras();

        if (_instance)
            _instance->SetBossState(DATA_BJARNGRIM, NOT_STARTED);

        DoCastSelf(SPELL_BATTLE_STANCE, true);
        SetEquipmentSlots(false, EQUIP_SWORD, EQUIP_SHIELD, EQUIP_NO_CHANGE);
    }

    void JustEngagedWith(Unit*) override
    {
        me->SetInCombatWithZone();
        Talk(SAY_AGGRO);

        RollStance(STANCE_BATTLE);

        events.ScheduleEvent(EVENT_BJARNGRIM_CHANGE_STANCE, 20s, 0);

        // The stock script's groups (its comments name the stances wrongly): Defensive, Berserker, Battle
        events.ScheduleEvent(EVENT_BJARNGRIM_REFLECTION, 8s, STANCE_DEFENSIVE);
        events.ScheduleEvent(EVENT_BJARNGRIM_KNOCK, 16s, STANCE_DEFENSIVE);
        events.ScheduleEvent(EVENT_BJARNGRIM_IRONFORM, 12s, STANCE_DEFENSIVE);

        events.ScheduleEvent(EVENT_BJARNGRIM_INTERCEPT, 23s, STANCE_BERSERKER);
        events.ScheduleEvent(EVENT_BJARNGRIM_CLEAVE, 25s, STANCE_BERSERKER);
        events.ScheduleEvent(EVENT_BJARNGRIM_WHIRLWIND, 26s, STANCE_BERSERKER);

        events.ScheduleEvent(EVENT_BJARNGRIM_PUMMEL, 5s, STANCE_BATTLE);
        events.ScheduleEvent(EVENT_BJARNGRIM_MORTAL_STRIKE, 24s, STANCE_BATTLE);
        events.ScheduleEvent(EVENT_BJARNGRIM_SLAM, 30s, STANCE_BATTLE);

        if (_instance)
        {
            _instance->SetBossState(DATA_BJARNGRIM, IN_PROGRESS);
            _instance->SetData(DATA_BJARNGRIM_ACHIEVEMENT, me->HasAura(SPELL_TEMPORARY_ELECTRICAL_CHARGE));
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit*) override
    {
        Talk(SAY_DEATH);
        scheduler.CancelAll();
        EndWindup();

        if (_instance)
            _instance->SetBossState(DATA_BJARNGRIM, DONE);
    }

    void RemoveStanceAura(uint8 stance)
    {
        switch (stance)
        {
            case STANCE_DEFENSIVE:
                me->RemoveAura(SPELL_DEFENSIVE_STANCE);
                me->RemoveAura(SPELL_DEFENSIVE_AURA);
                break;
            case STANCE_BERSERKER:
                me->RemoveAura(SPELL_BERSERKER_STANCE);
                me->RemoveAura(SPELL_BERSERKER_AURA);
                break;
            case STANCE_BATTLE:
                me->RemoveAura(SPELL_BATTLE_STANCE);
                me->RemoveAura(SPELL_BATTLE_AURA);
                break;
            default:
                break;
        }
    }

    void RollStance(uint8 stance)
    {
        if (urand(0, 1))
            stance = stance >= STANCE_BATTLE ? uint8(STANCE_DEFENSIVE) : uint8(stance + 1);
        else
            stance = stance <= STANCE_DEFENSIVE ? uint8(STANCE_BATTLE) : uint8(stance - 1);

        switch (stance)
        {
            case STANCE_DEFENSIVE:
                Talk(SAY_DEFENSIVE_STANCE);
                DoCastSelf(SPELL_DEFENSIVE_STANCE, true);
                DoCastSelf(SPELL_DEFENSIVE_AURA, true);
                events.DelayEvents(20s, STANCE_BERSERKER);
                events.DelayEvents(20s, STANCE_BATTLE);
                SetEquipmentSlots(false, EQUIP_SWORD, EQUIP_SHIELD, EQUIP_NO_CHANGE);
                break;
            case STANCE_BERSERKER:
                Talk(SAY_BERSERKER_STANCE);
                DoCastSelf(SPELL_BERSERKER_STANCE, true);
                DoCastSelf(SPELL_BERSERKER_AURA, true);
                events.DelayEvents(20s, STANCE_DEFENSIVE);
                events.DelayEvents(20s, STANCE_BATTLE);
                SetEquipmentSlots(false, EQUIP_SWORD, EQUIP_SWORD, EQUIP_NO_CHANGE);
                break;
            case STANCE_BATTLE:
                Talk(SAY_BATTLE_STANCE);
                DoCastSelf(SPELL_BATTLE_STANCE, true);
                DoCastSelf(SPELL_BATTLE_AURA, true);
                events.DelayEvents(20s, STANCE_BERSERKER);
                events.DelayEvents(20s, STANCE_DEFENSIVE);
                SetEquipmentSlots(false, EQUIP_MACE, EQUIP_UNEQUIP, EQUIP_NO_CHANGE);
                break;
            default:
                break;
        }

        _stance = stance;
    }

    using CreatureAI::WaypointReached;
    void WaypointReached(uint32 point) override
    {
        switch (point)
        {
            case POINT_FIRST_PLATFORM:
            case POINT_SECOND_PLATFORM:
                events.CancelEvent(EVENT_CHARGE_UP);
                events.ScheduleEvent(EVENT_CHARGE_UP, 2500ms, 0);
                break;
            case POINT_FIRST_PLATFORM_END:
            case POINT_SECOND_PLATFORM_END:
                events.CancelEvent(EVENT_CHARGE_UP);
                break;
            case POINT_FIRST_PLATFORM_BACK:
            case POINT_SECOND_PLATFORM_BACK:
                me->RemoveAura(SPELL_TEMPORARY_ELECTRICAL_CHARGE);
                break;
            default:
                break;
        }
    }

    // His charge-up on the platforms: +30% damage to him and his allies when pulled during it. In a key that is a
    // hidden trap set by the pull's timing, so he does not charge there.
    void ChargeUp()
    {
        if (InMythicKey(me))
            return;

        DoCastSelf(SPELL_CHARGE_UP, true);
        DoCastSelf(SPELL_TEMPORARY_ELECTRICAL_CHARGE, true);
    }

    void UpdateEscortAI(uint32 diff) override
    {
        events.Update(diff);

        if (!me->IsInCombat())
        {
            if (uint32 eventId = events.ExecuteEvent())
                if (eventId == EVENT_CHARGE_UP)
                    ChargeUp();
            return;
        }

        if (!UpdateVictim())
        {
            Reset();
            return;
        }

        scheduler.Update(diff);

        // While he holds still for a drawn ability, the rest waits
        if (_windup || me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_BJARNGRIM_CHANGE_STANCE:
                RemoveStanceAura(_stance);
                RollStance(_stance);
                events.Repeat(20s);
                break;
            case EVENT_CHARGE_UP:
                ChargeUp();
                break;

            case EVENT_BJARNGRIM_REFLECTION:
                DoCastSelf(SPELL_BJARNGRIM_REFLETION, true);
                events.Repeat(8s, 9s);
                break;
            case EVENT_BJARNGRIM_PUMMEL:
                DoCastVictim(SPELL_PUMMEL);
                events.Repeat(10s, 11s);
                break;
            case EVENT_BJARNGRIM_KNOCK:
                DoCastAOE(SPELL_KNOCK_AWAY);
                events.Repeat(20s, 21s);
                break;
            case EVENT_BJARNGRIM_IRONFORM:
                DoCastSelf(SPELL_IRONFORM, true);
                events.Repeat(18s, 23s);
                break;

            case EVENT_BJARNGRIM_MORTAL_STRIKE:
                MortalStrike();
                // A tank buster: not every swing
                events.Repeat(14s);
                break;
            case EVENT_BJARNGRIM_WHIRLWIND:
                Whirlwind();
                events.Repeat(25s);
                break;

            case EVENT_BJARNGRIM_INTERCEPT:
                DoCastRandomTarget(SPELL_INTERCEPT, 0, 40.0f, false, true);
                events.Repeat(30s);
                break;
            case EVENT_BJARNGRIM_CLEAVE:
                Cleave();
                events.Repeat(25s);
                break;
            case EVENT_BJARNGRIM_SLAM:
                DoCastVictim(SPELL_SLAM);
                events.Repeat(10s, 12s);
                break;
            default:
                break;
        }

        if (!_windup)
            DoMeleeAttackIfReady();
    }

private:
    // From the warning to the blow he stays where he is, facing it
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

    // Mortal Strike: a narrow cone at the tank. The tank takes a tank buster (and the healing debuff); a player caught
    // in front takes the cleave's share. Outside a key it is the stock Mortal Strike on whoever stayed in the cone.
    void MortalStrike()
    {
        Unit* tank = me->GetVictim();
        if (!tank)
            return;

        float const facing = me->GetAngle(tank);
        BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, *me, facing, MortalStrikeRadius,
            MortalStrikeArc, MortalStrikeWarningMs);
        ObjectGuid const tankGuid = tank->GetGUID();
        scheduler.Schedule(Milliseconds(MortalStrikeWarningMs), [this, area, tankGuid](TaskContext)
        {
            bool const mythic = InMythicKey(me);
            for (Player* player : PlayersIn(me, area))
            {
                if (!mythic)
                {
                    me->CastSpell(player, SPELL_MORTAL_STRIKE, true);
                    continue;
                }

                bool const isTank = player->GetGUID() == tankGuid;
                MythicTuning::DealReferenceDamage(me, player, SPELL_MORTAL_STRIKE,
                    isTank ? MortalStrikeTankPercent : MortalStrikeOthersPercent);
                if (isTank && player->IsAlive())
                    me->AddAura(SPELL_MORTAL_STRIKE, player);
            }
            EndWindup();
        });
    }

    // Cleave: a wide cone in front of him. Outside a key, the stock Cleave on the tank once the cone has shown.
    void Cleave()
    {
        Unit* tank = me->GetVictim();
        if (!tank)
            return;

        float const facing = me->GetAngle(tank);
        BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, *me, facing, CleaveRadius, CleaveArc,
            CleaveWarningMs);
        scheduler.Schedule(Milliseconds(CleaveWarningMs), [this, area](TaskContext)
        {
            EndWindup();
            if (!InMythicKey(me))
            {
                if (Unit* victim = me->GetVictim())
                    if (area.Contains(*victim))
                        DoCastVictim(SPELL_CLEAVE, true);
                return;
            }

            for (Player* player : PlayersIn(me, area))
                DealHit(me, player, SPELL_CLEAVE_NAMED, CleaveDamage);
        });
    }

    // Whirlwind: his circle shows for two seconds before he spins, so the first tick is never a surprise
    void Whirlwind()
    {
        me->TextEmote("%s begins to spin his blades!", nullptr, true);
        GroundIndicators::ShowCarriedCircle(me, me, WhirlwindRadius, WhirlwindWarningMs);
        scheduler.Schedule(Milliseconds(WhirlwindWarningMs), [this](TaskContext)
        {
            DoCastSelf(SPELL_WHIRLWIND, true);
        });
    }

    InstanceScript* _instance;
    SummonList _summons;
    uint8 _stance;
    bool _windup = false;
};
}

// ----------------------------------------------------------------------------------------------------------------
// Volkhan
// ----------------------------------------------------------------------------------------------------------------
namespace Volkhan
{
enum Spells : uint32
{
    SPELL_HEAT                          = 52387,
    SPELL_SHATTERING_STOMP              = 52237,
    SPELL_TEMPER                        = 52238,
    SPELL_SUMMON_MOLTEN_GOLEM           = 52405,

    SPELL_BLAST_WAVE                    = 23113,
    SPELL_COOL_DOWN                     = 52443,
    SPELL_IMMOLATION_STRIKE             = 52433,
    SPELL_SHATTER                       = 52429,
};

enum Misc : uint32
{
    NPC_MOLTEN_GOLEM                    = 28695,
    NPC_BRITTLE_GOLEM                   = 28681,
    NPC_SLAG                            = 28585,

    POINT_ANVIL                         = 1,
};

// The stock golem's actions, and one of ours
enum Actions : int32
{
    ACTION_SHATTER                      = 1,
    ACTION_DESTROYED                    = 2,
    ACTION_SHATTER_WARNING              = 3,
};

enum Events : uint32
{
    EVENT_HEAT                          = 1,
    EVENT_CHECK_HEALTH                  = 2,
    EVENT_SHATTER                       = 3,
    EVENT_POSITION                      = 4,
    EVENT_MOVE_TO_ANVIL                 = 5,

    EVENT_IMMOLATION_STRIKE             = 12,
    EVENT_CHANGE_TARGET                 = 13,
};

enum Texts : uint32
{
    SAY_AGGRO                           = 0,
    SAY_FORGE                           = 1,
    SAY_STOMP                           = 2,
    SAY_SLAY                            = 3,
    SAY_DEATH                           = 4,
};

// Shattering Stomp's cast; the brittle golems shatter when it lands
constexpr uint32 StompCastMs = 3000;
// Shatter's 10 yards, and a margin for the bodies' size the spell counts in
constexpr float ShatterRadius = 11.0f;
constexpr AbilityDamage ShatterDamage = { 10750, 55.0f };    // physical: about 41% of a damage dealer after armour
// A Molten Golem breaking overheats this long before its Blast Wave (heroic)
constexpr float BlastWaveRadius = 10.0f;
constexpr uint32 BlastWaveWarningMs = 1500;
constexpr float BlastWavePercent = 20.0f;

struct boss_volkhan_evolutions : public BossAI
{
    boss_volkhan_evolutions(Creature* creature) : BossAI(creature, DATA_VOLKHAN) { }

    void Reset() override
    {
        _Reset();
        _x = _y = _z = 0.0f;
        _pointId = 0;
        _shatteredCount = 0;
        _stompCast = false;
        me->SetSpeed(MOVE_RUN, 1.2f, true);
        me->SetReactState(REACT_AGGRESSIVE);
        instance->SetData(DATA_VOLKHAN_ACHIEVEMENT, true);
    }

    void JustEngagedWith(Unit*) override
    {
        _JustEngagedWith();
        me->SetInCombatWithZone();
        Talk(SAY_AGGRO);
        events.ScheduleEvent(EVENT_MOVE_TO_ANVIL, 9s, 14s);
        events.ScheduleEvent(EVENT_HEAT, 18s, 38s);
        events.ScheduleEvent(EVENT_CHECK_HEALTH, 1s);
        events.ScheduleEvent(EVENT_POSITION, 4s);
    }

    void JustDied(Unit*) override
    {
        _JustDied();
        Talk(SAY_DEATH);

        std::list<Creature*> slags;
        GetCreatureListWithEntryInGrid(slags, me, NPC_SLAG, 100.0f);
        for (Creature* slag : slags)
            if (slag)
                slag->DespawnOrUnsummon();
    }

    void GetNextPos()
    {
        if (me->GetPositionY() < -180)
        {
            _x = me->GetPositionX() > 1330 ? 1355 : 1308;
            _y = -178;
            _z = 52.5f;
        }
        else if (me->GetPositionY() < -145)
        {
            _x = me->GetPositionX() > 1330 ? 1355 : 1308;
            _y = -137;
            _z = 52.5f;
        }
        else if (me->GetPositionY() < -130)
        {
            _x = me->GetPositionX() > 1330 ? 1343 : 1320;
            _y = -123;
            _z = 56.7f;
        }
        else
        {
            _pointId = POINT_ANVIL;
            _x = 1327;
            _y = -96;
            _z = 56.7f;
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_MOLTEN_GOLEM)
        {
            summon->SetFaction(me->GetFaction());
            if (Unit* target = SelectTarget(SelectTargetMethod::Random))
                summon->AI()->AttackStart(target);
        }
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_DESTROYED)
        {
            ++_shatteredCount;
            if (_shatteredCount > 4)
                instance->SetData(DATA_VOLKHAN_ACHIEVEMENT, false);
        }
    }

    bool HasActiveGolem()
    {
        for (ObjectGuid const& guid : summons)
            if (Creature* golem = ObjectAccessor::GetCreature(*me, guid))
                if (golem->GetEntry() == NPC_MOLTEN_GOLEM && golem->IsAlive())
                    return true;
        return false;
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == POINT_ANVIL)
        {
            me->SetSpeed(MOVE_RUN, 1.2f, true);
            DoCastSelf(SPELL_TEMPER);
            _pointId = 0;
            me->SetOrientation(2.19f);
            me->SendMovementFlagUpdate(false);
            me->SetControlled(true, UNIT_STATE_ROOT);
        }
        else
            me->GetMotionMaster()->MovePoint(_pointId, _x, _y, _z);
    }

    void SpellHitTarget(Unit* /*who*/, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_TEMPER)
            return;

        DoCastSelf(SPELL_SUMMON_MOLTEN_GOLEM, true);
        DoCastSelf(SPELL_SUMMON_MOLTEN_GOLEM, true);
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetReactState(REACT_AGGRESSIVE);
        if (me->GetVictim())
            me->GetMotionMaster()->MoveChase(me->GetVictim());

        events.RescheduleEvent(EVENT_HEAT, 9s, 24s);
    }

    void GoToAnvil()
    {
        me->SetSpeed(MOVE_RUN, 4.0f, true);
        me->SetReactState(REACT_PASSIVE);
        Talk(SAY_FORGE);

        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
            me->GetMotionMaster()->MovementExpired();

        GetNextPos();
        me->GetMotionMaster()->MovePoint(_pointId, _x, _y, _z);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);
        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_HEAT:
                if (HasActiveGolem())
                {
                    DoCastSelf(SPELL_HEAT);
                    events.Repeat(9s, 24s);
                }
                break;
            case EVENT_CHECK_HEALTH:
                if (!_stompCast && HealthBelowPct(25))
                {
                    // The stock script shattered every brittle golem as the stomp began, unseen. Here they are
                    // drawn for the whole cast and shatter as it lands.
                    _stompCast = true;
                    DoCastAOE(SPELL_SHATTERING_STOMP);
                    Talk(SAY_STOMP);
                    me->TextEmote("The brittle golems glow red: get away from them before the stomp lands!", nullptr,
                        true);
                    summons.DoAction(ACTION_SHATTER_WARNING);
                    events.ScheduleEvent(EVENT_SHATTER, Milliseconds(StompCastMs));
                }
                events.Repeat(1s);
                return;
            case EVENT_SHATTER:
                summons.DoAction(ACTION_SHATTER);
                break;
            case EVENT_MOVE_TO_ANVIL:
                GoToAnvil();
                events.Repeat(30s, 36s);
                return;
            case EVENT_POSITION:
                if (me->GetDistance(1331.9f, -106, 56) > 95)
                    EnterEvadeMode();
                else
                    events.Repeat(4s);
                return;
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    float _x = 0.0f;
    float _y = 0.0f;
    float _z = 0.0f;
    uint8 _pointId = 0;
    uint8 _shatteredCount = 0;
    bool _stompCast = false;
};

struct npc_molten_golem_evolutions : public ScriptedAI
{
    npc_molten_golem_evolutions(Creature* creature) : ScriptedAI(creature)
    {
        _instance = creature->GetInstanceScript();
    }

    void Reset() override
    {
        events.Reset();
        events.ScheduleEvent(EVENT_IMMOLATION_STRIKE, 3s);
        events.ScheduleEvent(EVENT_CHANGE_TARGET, 5s);
        DoCastSelf(SPELL_COOL_DOWN, true);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (me->GetEntry() == NPC_BRITTLE_GOLEM)
        {
            damage = 0;
            return;
        }

        if (damage < me->GetHealth())
            return;

        me->UpdateEntry(NPC_BRITTLE_GOLEM, 0, false);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_DISABLE_MOVE);
        me->SetHealth(me->GetMaxHealth());
        me->RemoveAllAuras();
        me->AttackStop();
        damage = 0;

        if (me->IsNonMeleeSpellCast(false))
            me->InterruptNonMeleeSpells(false);

        me->SetControlled(true, UNIT_STATE_STUNNED);

        // Heroic: it overheats, drawn, before its Blast Wave (the stock golem cast it on the spot, unseen)
        if (me->GetMap()->IsHeroic())
            Overheat();
    }

    void DoAction(int32 param) override
    {
        if (me->GetEntry() != NPC_BRITTLE_GOLEM)
            return;

        if (param == ACTION_SHATTER_WARNING)
            _shatterArea = GroundIndicators::ShowCircle(me, *me, ShatterRadius, StompCastMs,
                GroundIndicators::Theme::Fire);
        else if (param == ACTION_SHATTER)
            Shatter();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim() || me->GetEntry() == NPC_BRITTLE_GOLEM)
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_IMMOLATION_STRIKE:
                if (SelectTarget(SelectTargetMethod::MaxThreat, 0, 0.0f, true, true, -int32(SPELL_IMMOLATION_STRIKE)))
                    DoCastVictim(SPELL_IMMOLATION_STRIKE);
                events.Repeat(5s);
                break;
            case EVENT_CHANGE_TARGET:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true))
                {
                    me->GetThreatMgr().ResetAllThreat();
                    me->AddThreat(target, 30000.0f);
                    AttackStart(target);
                }
                break;
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    void Overheat()
    {
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, *me, BlastWaveRadius,
            BlastWaveWarningMs, GroundIndicators::Theme::Fire);
        scheduler.Schedule(Milliseconds(BlastWaveWarningMs), [this, area](TaskContext)
        {
            if (!InMythicKey(me))
            {
                DoCastSelf(SPELL_BLAST_WAVE, true);
                return;
            }

            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Fire);
            for (Player* player : PlayersIn(me, area))
                MythicTuning::DealReferenceDamage(me, player, SPELL_BLAST_WAVE, BlastWavePercent);
        });
    }

    void Shatter()
    {
        if (_instance)
            if (Creature* volkhan = _instance->GetCreature(DATA_VOLKHAN))
                volkhan->AI()->DoAction(ACTION_DESTROYED);

        if (InMythicKey(me))
        {
            // Resolved on the circle drawn during the stomp: what showed is what hits
            GroundIndicators::Area area = _shatterArea;
            if (area.radius <= 0.0f)
            {
                area.kind = GroundIndicators::Area::Kind::Circle;
                area.origin = me->GetPosition();
                area.radius = ShatterRadius;
            }
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Fire);
            for (Player* player : PlayersIn(me, area))
                DealHit(me, player, SPELL_SHATTER, ShatterDamage);
        }
        else
            me->CastSpell(me, SPELL_SHATTER, true);

        me->DespawnOrUnsummon(500ms);
    }

    InstanceScript* _instance;
    GroundIndicators::Area _shatterArea;
};
}

// ----------------------------------------------------------------------------------------------------------------
// Ionar
// ----------------------------------------------------------------------------------------------------------------
namespace Ionar
{
enum Spells : uint32
{
    SPELL_STATIC_OVERLOAD               = 52658,
    SPELL_DISPERSE                      = 52770,
    SPELL_SPARK_VISUAL_TRIGGER          = 52667,
    SPELL_RANDOM_LIGHTNING              = 52663,

    // Named in the log: the ball's missile, the sparks' burn (heroic ids)
    SPELL_BALL_LIGHTNING_HIT            = 59801,
    SPELL_ARCING_BURN                   = 59834,
};

enum Misc : uint32
{
    NPC_SPARK_OF_IONAR                  = 28926,
};

// The stock spark's actions (npc_spark_of_ionar keeps chasing and coming back)
enum Actions : int32
{
    ACTION_CALLBACK                     = 1,
    ACTION_SPARK_DESPAWN                = 2,
};

enum Phases : uint8
{
    PHASE_FIGHT                         = 1,
    PHASE_SPLIT                         = 2,
};

enum Texts : uint32
{
    SAY_AGGRO                           = 0,
    SAY_SPLIT                           = 1,
    SAY_SLAY                            = 2,
    SAY_DEATH                           = 3,
};

enum Events : uint32
{
    EVENT_BALL_LIGHTNING                = 1,
    EVENT_STATIC_OVERLOAD               = 2,
    EVENT_CHECK_HEALTH                  = 3,
    EVENT_CALL_SPARKS                   = 4,
    EVENT_RESTORE                       = 5,
    EVENT_SPARK_PULSE                   = 6,
};

constexpr uint32 SparkCount = 5;
constexpr uint32 SplitMs = 20000;
constexpr float SparkRadius = 5.0f;
// A spark burns whoever it reaches every second (in a key: no chain, no stacking debuff)
constexpr AbilityDamage SparkDamage = { 1075, 4.0f };
constexpr float BallLightningRadius = 6.0f;
constexpr uint32 BallLightningWarningMs = 2000;
constexpr AbilityDamage BallLightningDamage = { 5287, 30.0f };
constexpr float StaticOverloadRadius = 8.0f;
constexpr uint32 StaticOverloadMs = 10000;

struct boss_ionar_evolutions : public BossAI
{
    boss_ionar_evolutions(Creature* creature) : BossAI(creature, DATA_IONAR) { }

    void Reset() override
    {
        _Reset();
        me->SetVisible(true);
        me->SetControlled(false, UNIT_STATE_STUNNED);

        ScheduleHealthCheckEvent(50, [&] {
            DoCastSelf(SPELL_DISPERSE);
        });
    }

    void ScheduleEvents()
    {
        events.SetPhase(PHASE_FIGHT);
        events.RescheduleEvent(EVENT_BALL_LIGHTNING, 7s, 11s, 0, PHASE_FIGHT);
        events.RescheduleEvent(EVENT_STATIC_OVERLOAD, 6s, 12s, 0, PHASE_FIGHT);
    }

    void JustEngagedWith(Unit*) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);
        ScheduleEvents();
    }

    void JustDied(Unit*) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void SpellHit(Unit* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_DISPERSE)
            Split();
    }

    void Split()
    {
        Talk(SAY_SPLIT);
        me->TextEmote("%s bursts into sparks that hunt you: keep away from them, and apart!", nullptr, true);
        bool const mythic = InMythicKey(me);

        for (uint32 i = 0; i < SparkCount; ++i)
        {
            Creature* spark = me->SummonCreature(NPC_SPARK_OF_IONAR, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN,
                SplitMs);
            if (!spark)
                continue;

            // In a key the sparks' burn is the script's (below): the stock one chained to three players and
            // stacked a debuff up to 99 times
            if (!mythic)
                spark->CastSpell(spark, SPELL_SPARK_VISUAL_TRIGGER, true);
            spark->CastSpell(spark, SPELL_RANDOM_LIGHTNING, true);
            spark->SetUnitFlag(UNIT_FLAG_PACIFIED | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE);
            spark->SetHomePosition(me->GetPosition());
            GroundIndicators::ShowCarriedCircle(me, spark, SparkRadius, SplitMs);

            if (Player* target = SelectTargetFromPlayerList(100))
                spark->GetMotionMaster()->MoveFollow(target, 0.0f, 0.0f, MOTION_SLOT_CONTROLLED);
        }

        me->SetVisible(false);
        me->SetControlled(true, UNIT_STATE_STUNNED);

        events.SetPhase(PHASE_SPLIT);
        events.ScheduleEvent(EVENT_CALL_SPARKS, Milliseconds(SplitMs), 0, PHASE_SPLIT);
        if (mythic)
            events.ScheduleEvent(EVENT_SPARK_PULSE, 1s, 0, PHASE_SPLIT);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);
        scheduler.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_BALL_LIGHTNING:
                BallLightning();
                events.Repeat(8s, 18s);
                break;
            case EVENT_STATIC_OVERLOAD:
                StaticOverload();
                events.Repeat(9s, 14s);
                break;
            case EVENT_SPARK_PULSE:
                SparkPulse();
                events.Repeat(1s);
                return;
            case EVENT_CALL_SPARKS:
            {
                events.CancelEvent(EVENT_SPARK_PULSE);
                EntryCheckPredicate pred(NPC_SPARK_OF_IONAR);
                summons.DoAction(ACTION_CALLBACK, pred);
                events.ScheduleEvent(EVENT_RESTORE, 5s, 0, PHASE_SPLIT);
                return;
            }
            case EVENT_RESTORE:
            {
                EntryCheckPredicate pred(NPC_SPARK_OF_IONAR);
                summons.DoAction(ACTION_SPARK_DESPAWN, pred);

                me->SetVisible(true);
                me->SetControlled(false, UNIT_STATE_STUNNED);
                ScheduleEvents();
                return;
            }
            default:
                break;
        }

        if (events.IsInPhase(PHASE_FIGHT))
            DoMeleeAttackIfReady();
    }

private:
    // Ball Lightning: a circle under a player other than the tank; it lands where it was drawn
    void BallLightning()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 1, 100.0f, true);
        if (!target)
            return;

        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, *target, BallLightningRadius,
            BallLightningWarningMs, GroundIndicators::Theme::Nature);
        scheduler.Schedule(Milliseconds(BallLightningWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Nature);
            for (Player* player : PlayersIn(me, area))
                DealHit(me, player, SPELL_BALL_LIGHTNING_HIT, BallLightningDamage);
        });
    }

    // Static Overload: the stock aura, its 8 yards drawn around the player carrying it (take it away)
    void StaticOverload()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true);
        if (!target)
            return;

        if (DoCast(target, SPELL_STATIC_OVERLOAD) == SPELL_CAST_OK)
            GroundIndicators::ShowCarriedCircle(me, target, StaticOverloadRadius, StaticOverloadMs);
    }

    // In a key: each spark burns whoever stands in its circle
    void SparkPulse()
    {
        std::vector<Player*> players = PlayersAround(me, 100.0f);
        for (ObjectGuid const& guid : summons)
        {
            Creature* spark = ObjectAccessor::GetCreature(*me, guid);
            if (!spark || spark->GetEntry() != NPC_SPARK_OF_IONAR || !spark->IsAlive())
                continue;

            GroundIndicators::Area area;
            area.kind = GroundIndicators::Area::Kind::Circle;
            area.radius = SparkRadius;
            area = GroundIndicators::CurrentArea(spark, area);
            for (Player* player : players)
                if (area.Contains(*player))
                    DealHit(me, player, SPELL_ARCING_BURN, SparkDamage);
        }
    }
};
}

// ----------------------------------------------------------------------------------------------------------------
// Loken
// ----------------------------------------------------------------------------------------------------------------
namespace Loken
{
enum Spells : uint32
{
    SPELL_ARC_LIGHTNING                 = 52921,
    SPELL_LIGHTNING_NOVA                = 52960,
    SPELL_LIGHTNING_NOVA_VISUAL         = 56502,
    SPELL_LIGHTNING_NOVA_THUNDERS       = 52663,

    SPELL_PULSING_SHOCKWAVE             = 52961,
    SPELL_PULSING_SHOCKWAVE_AURA        = 59414,
    // Named in the log by the key's own shockwave (heroic id of the pulse)
    SPELL_PULSING_SHOCKWAVE_HIT         = 59837,

    ACHIEVEMENT_TIMELY_DEATH            = 20384,
};

enum Texts : uint32
{
    SAY_INTRO_1                         = 0,
    SAY_INTRO_2                         = 1,
    SAY_AGGRO                           = 2,
    SAY_NOVA                            = 3,
    SAY_SLAY                            = 4,
    SAY_75HEALTH                        = 5,
    SAY_50HEALTH                        = 6,
    SAY_25HEALTH                        = 7,
    SAY_DEATH                           = 8,
    EMOTE_NOVA                          = 9,
};

// The stock pulse: 150 x the distance in yards, every 2 seconds (about 0.5% of the reference health per yard in a
// key). In a key it stops growing at ShockwaveCapYards, so a misplaced player is punished, not deleted.
constexpr float ShockwavePercentPerYard = 0.5f;
constexpr float ShockwaveCapYards = 15.0f;
constexpr float ShockwaveReach = 100.0f;
// Beyond this, a burst on the player each pulse shows where the damage comes from
constexpr float ShockwaveShownYards = 10.0f;

struct boss_loken_evolutions : public BossAI
{
    boss_loken_evolutions(Creature* creature) : BossAI(creature, DATA_LOKEN) { }

    void Reset() override
    {
        _Reset();
        instance->DoStopTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEVEMENT_TIMELY_DEATH);

        me->RemoveAllAuras();

        ScheduleHealthCheckEvent(75, [&] {
            Talk(SAY_75HEALTH);
        });

        ScheduleHealthCheckEvent(50, [&] {
            Talk(SAY_50HEALTH);
        });

        ScheduleHealthCheckEvent(25, [&] {
            Talk(SAY_25HEALTH);
        });
    }

    void MoveInLineOfSight(Unit* who) override
    {
        BossAI::MoveInLineOfSight(who);

        if (_introDone || !who->IsPlayer() || !me->IsWithinDistInMap(who, 40.0f))
            return;

        Talk(SAY_INTRO_1);
        Talk(SAY_INTRO_2, 10s);
        _introDone = true;
    }

    void OnAuraRemove(AuraApplication* auraApp, AuraRemoveMode /*mode*/) override
    {
        if (auraApp->GetBase()->GetId() == SPELL_LIGHTNING_NOVA_VISUAL)
        {
            me->RemoveAura(SPELL_LIGHTNING_NOVA_THUNDERS);
            me->ClearUnitState(UNIT_STATE_CASTING);
            me->ResumeChasingVictim();
        }
    }

    void ScheduleTasks() override
    {
        bool const mythic = InMythicKey(me);
        me->m_Events.AddEventAtOffset([this, mythic] {
            DoCastAOE(SPELL_PULSING_SHOCKWAVE_AURA, true);
            me->ClearUnitState(UNIT_STATE_CASTING); // the aura above is a channeled spell, so we need this
            if (!mythic)
                DoCastSelf(SPELL_PULSING_SHOCKWAVE);
        }, 3s);

        if (mythic)
            ScheduleTimedEvent(5s, 5s, [this] {
                PulsingShockwave();
            }, 2s, 2s);

        ScheduleTimedEvent(15s, [&] {
            Talk(SAY_NOVA);
            Talk(EMOTE_NOVA);
            DoCastSelf(SPELL_LIGHTNING_NOVA_VISUAL, true);
            DoCastSelf(SPELL_LIGHTNING_NOVA_THUNDERS, true);
            DoCastAOE(SPELL_LIGHTNING_NOVA);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
        }, 15s);

        if (IsHeroic())
        {
            ScheduleTimedEvent(10s, [&] {
                DoCastRandomTarget(SPELL_ARC_LIGHTNING, 0, 100.0f, false);
            }, 12s);

            instance->DoStartTimedAchievement(ACHIEVEMENT_TIMED_TYPE_EVENT, ACHIEVEMENT_TIMELY_DEATH);
        }
    }

    void JustEngagedWith(Unit*) override
    {
        me->m_Events.KillAllEvents(false);
        _JustEngagedWith();
        Talk(SAY_AGGRO);
        me->TextEmote("%s's Pulsing Shockwave hits harder the further you stand from him: stay close!", nullptr,
            true);
    }

    void JustDied(Unit*) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

private:
    // In a key: the stock pulse, the distance capped
    void PulsingShockwave()
    {
        for (Player* player : PlayersAround(me, ShockwaveReach))
        {
            float const distance = me->GetDistance2d(player);
            float const yards = distance > 1.0f ? std::min(distance, ShockwaveCapYards) : 1.0f;
            MythicTuning::DealReferenceDamage(me, player, SPELL_PULSING_SHOCKWAVE_HIT,
                ShockwavePercentPerYard * yards);
            if (distance > ShockwaveShownYards)
                GroundIndicators::Burst(me, *player, GroundIndicators::Theme::Nature);
        }
    }

    bool _introDone = false;
};
}

enum HoLTrash : uint32
{
    NPC_HARDENED_STEEL_REAVER           = 28578,
    NPC_HARDENED_STEEL_REAVER_H         = 30967,
    NPC_STORMFORGED_TACTICIAN           = 28581,
    NPC_STORMFORGED_TACTICIAN_H         = 30977,
    NPC_UNBOUND_FIRESTORM               = 28584,
    NPC_UNBOUND_FIRESTORM_H             = 30983,
};

enum HoLTrashSpells : uint32
{
    // Only named in the log by the trash abilities (none is a weapon spell)
    SPELL_SHIELD_SLAM                   = 59142,
    SPELL_WELDING_BEAM                  = 59166,
    SPELL_LAVA_BURST                    = 59182,
};

void RegisterFor(std::initializer_list<uint32> entries, MythicTrash::Ability ability)
{
    for (uint32 entry : entries)
    {
        ability.entry = entry;
        MythicTrash::Register(ability);
    }
}
}

void AddMythicHallsOfLightningScripts()
{
    // Damage over time above the 20-40% budget: Hurl Weapon (Hardened Steel Berserker, 55%) and Poison Tipped Spear
    // (Titanium Vanguard, 54%)
    for (uint32 spell : { 52740u, 59259u, 53059u, 59178u })
        MythicTuning::SetSpellMultiplier(spell, 0.7f);
    // Volkhan's golems: Immolation Strike was a 12% damage over time
    for (uint32 spell : { 52433u, 59530u })
        MythicTuning::SetSpellMultiplier(spell, 2.0f);
    // Ionar's Static Overload: 45% over its 10 seconds on the carrier, now drawn; a little less
    for (uint32 spell : { 52659u, 59796u })
        MythicTuning::SetSpellMultiplier(spell, 0.8f);
    // Loken: Lightning Nova at the low end of a telegraphed hit (48%), Arc Lightning 15%
    for (uint32 spell : { 52960u, 59835u })
        MythicTuning::SetSpellMultiplier(spell, 1.2f);
    MythicTuning::SetSpellMultiplier(Loken::SPELL_ARC_LIGHTNING, 1.3f);

    // The ScriptName is the struct's name, without its namespace (as RegisterCreatureAIWithFactory would give it)
    new FactoryCreatureScript<Bjarngrim::boss_bjarngrim_evolutions,
        &GetHallsOfLightningEvolutionsAI<Bjarngrim::boss_bjarngrim_evolutions>>("boss_bjarngrim_evolutions");
    new FactoryCreatureScript<Volkhan::boss_volkhan_evolutions,
        &GetHallsOfLightningEvolutionsAI<Volkhan::boss_volkhan_evolutions>>("boss_volkhan_evolutions");
    new FactoryCreatureScript<Volkhan::npc_molten_golem_evolutions,
        &GetHallsOfLightningEvolutionsAI<Volkhan::npc_molten_golem_evolutions>>("npc_molten_golem_evolutions");
    new FactoryCreatureScript<Ionar::boss_ionar_evolutions,
        &GetHallsOfLightningEvolutionsAI<Ionar::boss_ionar_evolutions>>("boss_ionar_evolutions");
    new FactoryCreatureScript<Loken::boss_loken_evolutions,
        &GetHallsOfLightningEvolutionsAI<Loken::boss_loken_evolutions>>("boss_loken_evolutions");

    // Hardened Steel Reaver: a Shield Slam driven through its target and whoever stands behind (step aside)
    MythicTrash::Ability slam;
    slam.shape = MythicTrash::Shape::LineAtVictim;
    slam.size = 15.0f;
    slam.width = 4.0f;
    slam.warnMs = 2500;
    slam.cooldownMs = 18000;
    slam.firstMs = 8000;
    slam.percent = 35.0f;       // physical: about 26% of a damage dealer after armour
    slam.spellId = SPELL_SHIELD_SLAM;
    RegisterFor({ NPC_HARDENED_STEEL_REAVER, NPC_HARDENED_STEEL_REAVER_H }, slam);

    // Stormforged Tactician: a Welding Beam's arc carried by one player, burning whoever stays next to them
    MythicTrash::Ability arc;
    arc.shape = MythicTrash::Shape::CarriedByTarget;
    arc.size = 6.0f;
    arc.warnMs = 3000;
    arc.cooldownMs = 22000;
    arc.firstMs = 10000;
    arc.percent = 25.0f;
    arc.spellId = SPELL_WELDING_BEAM;
    arc.theme = GroundIndicators::Theme::Nature;
    arc.holdStill = false;
    RegisterFor({ NPC_STORMFORGED_TACTICIAN, NPC_STORMFORGED_TACTICIAN_H }, arc);

    // Unbound Firestorm: a Lava Burst under a player (move)
    MythicTrash::Ability lava;
    lava.shape = MythicTrash::Shape::UnderTarget;
    lava.size = 6.0f;
    lava.warnMs = 2500;
    lava.cooldownMs = 16000;
    lava.firstMs = 6000;
    lava.percent = 30.0f;
    lava.spellId = SPELL_LAVA_BURST;
    lava.theme = GroundIndicators::Theme::Fire;
    lava.holdStill = false;
    RegisterFor({ NPC_UNBOUND_FIRESTORM, NPC_UNBOUND_FIRESTORM_H }, lava);
}
