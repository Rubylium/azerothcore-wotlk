#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "Containers.h"
#include "CreatureScript.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "StringFormat.h"
#include "TaskScheduler.h"
#include <cmath>
#include <list>
#include <vector>

// Utgarde Keep in Mythic+: damage tuning, boss reworks and trash abilities (audit: .agents/plans/mplus-damage-audit/).
//
// Prince Keleseth (boss_keleseth_evolutions): Frost Tomb marks a player with a circle first, then entombs everyone
// still inside it (spread from the marked player); Shadow Nova, a circle around him that melee step out of. His tombs
// take longer to break in a key.
// Skarvald the Constructor (boss_skarvald_evolutions, his ghost too): no more threat wipe onto a random player. He
// marks a player who is not his tank with a circle that follows them, then charges the spot where they stood and
// hits everyone in it (the marked player runs from the group, then steps out as the circle stops following).
// Ingvar the Plunderer (boss_ingvar_evolutions), reworked whole: his own section below.
namespace
{
// The stock instance script (instance_utgarde_keep) keeps these
enum UtgardeKeepData
{
    DATA_KELESETH                   = 0,
    DATA_DALRONN_AND_SKARVALD       = 1,
    DATA_DALRONN                    = 5,
    DATA_UNLOCK_SKARVALD_LOOT       = 200,
};

enum UtgardeKeepCreatures
{
    NPC_SKARVALD                    = 24200,
    NPC_FROST_TOMB                  = 23965,
    NPC_FROST_TOMB_HEROIC           = 31672,
    NPC_VRYKUL_SKELETON             = 23970,
    NPC_VRYKUL_SKELETON_HEROIC      = 31635,
    NPC_INGVAR_THROW_DUMMY          = 23997,
    NPC_INGVAR_THROW_DUMMY_HEROIC   = 31835,

    // Trash with a telegraphed ability
    NPC_DRAGONFLAYER_IRONHELM       = 23961,
    NPC_DRAGONFLAYER_IRONHELM_H     = 30747,
    NPC_DRAGONFLAYER_RUNECASTER     = 23960,
    NPC_DRAGONFLAYER_RUNECASTER_H   = 31663,
    NPC_DRAGONFLAYER_BONECRUSHER    = 24069,
    NPC_DRAGONFLAYER_BONECRUSHER_H  = 31658,
};

enum UtgardeKeepSpells
{
    // Prince Keleseth
    SPELL_KELESETH_SHADOW_BOLT      = 43667,
    SPELL_KELESETH_SHADOW_BOLT_H    = 59389,
    SPELL_FROST_TOMB                = 42672,
    SPELL_FROST_TOMB_AURA           = 48400,
    SPELL_SHADOW_NOVA               = 30852,    // named only: the damage is dealt on the drawn circle

    // Skarvald and Dalronn
    SPELL_STONE_STRIKE              = 48583,
    SPELL_SKARVALD_ENRAGE           = 48193,
    SPELL_SUMMON_SKARVALD_GHOST     = 48613,
    SPELL_SKARVALD_CHARGE_IMPACT    = 58991,    // "Charge", named only
    SPELL_DALRONN_DEBILITATE        = 43650,

    // Trash
    SPELL_PROTO_DRAKE_REND_H        = 59691,
    SPELL_TICKING_BOMB_EXPLOSION_H  = 59687,
    SPELL_SHOCKWAVE                 = 55636,    // named only (Ironhelm)
    SPELL_RUNE_DETONATION           = 55031,    // named only (Runecaster)
    SPELL_GROUND_SLAM               = 52058,    // named only (Bonecrusher)
};

// Damage, before the key's scaling, for the reworked abilities outside a key: heroic, and normal at NormalPercent of
// it (heroic players here have 20-25k health). In a key they deal a share of the reference health instead.
constexpr uint32 NormalPercent = 60;

void Hit(Creature* me, Unit* target, uint32 spellId, float mythicPercent, uint32 heroicAmount)
{
    if (me->GetMap()->IsMythic())
        MythicTuning::DealReferenceDamage(me, target, spellId, mythicPercent);
    else
        MythicTuning::DealAbilityDamage(me, target, spellId,
            me->GetMap()->IsHeroic() ? heroicAmount : heroicAmount * NormalPercent / 100);
}

std::vector<Player*> PlayersNear(Creature* me, float range)
{
    std::vector<Player*> players;
    for (auto const& ref : me->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->IsAlive() && !player->IsGameMaster() && me->IsWithinDistInMap(player, range))
                players.push_back(player);
    return players;
}

std::vector<Player*> PlayersIn(Creature* me, GroundIndicators::Area const& area)
{
    std::vector<Player*> inside;
    for (Player* player : PlayersNear(me, 100.0f))
        if (area.Contains(player->GetPosition()))
            inside.push_back(player);
    return inside;
}

// ---------------------------------------------------------------------------------------------------------------------
// Prince Keleseth
// ---------------------------------------------------------------------------------------------------------------------
enum KelesethTexts
{
    SAY_KELESETH_START_COMBAT       = 1,
    SAY_KELESETH_SUMMON_SKELETONS   = 2,
    SAY_KELESETH_FROST_TOMB         = 3,
    SAY_KELESETH_FROST_TOMB_EMOTE   = 4,
    SAY_KELESETH_DEATH              = 5,
    SAY_KELESETH_KILL               = 6,
};

enum KelesethEvents
{
    EVENT_KELESETH_SHADOW_BOLT      = 1,
    EVENT_KELESETH_FROST_TOMB,
    EVENT_KELESETH_SHADOW_NOVA,
};

constexpr float FrostTombRadius = 5.0f;
constexpr uint32 FrostTombMarkMs = 3000;
// A tomb holds this many times longer in a key: at +10 the stock tomb broke to a single global, so its damage over
// time never mattered
constexpr float FrostTombHealthFactor = 2.5f;
constexpr float ShadowNovaRadius = 8.0f;
constexpr uint32 ShadowNovaWarningMs = 2500;
constexpr float ShadowNovaPercent = 40.0f;
constexpr uint32 ShadowNovaDamage = 7000;

struct boss_keleseth_evolutions : public BossAI
{
    boss_keleseth_evolutions(Creature* creature) : BossAI(creature, DATA_KELESETH) { }

    void Reset() override
    {
        EndWindup();
        _boostedTombs.clear();
        BossAI::Reset();
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelAll();
        EndWindup();
        BossAI::EnterEvadeMode(why);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KELESETH_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_KELESETH_DEATH);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_KELESETH_START_COMBAT);

        events.ScheduleEvent(EVENT_KELESETH_SHADOW_BOLT, 1s);
        events.ScheduleEvent(EVENT_KELESETH_FROST_TOMB, 20s);
        events.ScheduleEvent(EVENT_KELESETH_SHADOW_NOVA, 14s);

        me->m_Events.AddEventAtOffset([this]()
        {
            Talk(SAY_KELESETH_SUMMON_SKELETONS);
            for (uint8 i = 0; i < 5; ++i)
            {
                float const dist = rand_norm() * 4 + 3.0f;
                float const angle = rand_norm() * 2 * M_PI;
                me->SummonCreature(NPC_VRYKUL_SKELETON, 156.2f + std::cos(angle) * dist,
                    259.1f + std::sin(angle) * dist, 42.9f, 0, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
            }
        }, 4s);
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        UnitAI::AttackStartCaster(who, 12.0f);
    }

    void ExecuteEvent(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_KELESETH_SHADOW_BOLT:
                DoCastVictim(SPELL_KELESETH_SHADOW_BOLT);
                events.Repeat(4s, 5s);
                break;
            case EVENT_KELESETH_FROST_TOMB:
                if (MarkFrostTomb())
                    events.Repeat(18s, 22s);
                else
                    events.Repeat(3s);
                break;
            case EVENT_KELESETH_SHADOW_NOVA:
                if (_windup)
                {
                    events.Repeat(1s);
                    break;
                }
                ShadowNova();
                events.Repeat(20s, 24s);
                break;
            default:
                break;
        }
    }

private:
    // Frost Tomb: a circle follows the chosen player; when it ends, everyone still inside is entombed
    bool MarkFrostTomb()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, false, -SPELL_FROST_TOMB_AURA);
        if (!target)
            target = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, true, true, -SPELL_FROST_TOMB_AURA);
        if (!target)
            return false;

        Talk(SAY_KELESETH_FROST_TOMB_EMOTE, target);
        Talk(SAY_KELESETH_FROST_TOMB);
        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, target, FrostTombRadius,
            FrostTombMarkMs);
        ObjectGuid const carrierGuid = target->GetGUID();
        scheduler.Schedule(Milliseconds(FrostTombMarkMs), [this, area, carrierGuid](TaskContext)
        {
            Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            GroundIndicators::Area const landed = GroundIndicators::CurrentArea(carrier, area);
            GroundIndicators::Burst(me, landed.origin, GroundIndicators::Theme::Frost);
            for (Player* player : PlayersIn(me, landed))
                if (!player->HasAura(SPELL_FROST_TOMB_AURA))
                    me->CastSpell(player, SPELL_FROST_TOMB, true);

            // The tombs appear on the first tick of the stun (1 s)
            if (me->GetMap()->IsMythic())
                scheduler.Schedule(1500ms, [this](TaskContext) { StrengthenTombs(); });
        });
        return true;
    }

    void StrengthenTombs()
    {
        std::list<Creature*> tombs;
        me->GetCreatureListWithEntryInGrid(tombs, { NPC_FROST_TOMB, NPC_FROST_TOMB_HEROIC }, 150.0f);
        for (Creature* tomb : tombs)
        {
            if (!tomb->IsAlive() || !_boostedTombs.insert(tomb->GetGUID()).second)
                continue;

            uint32 const health = static_cast<uint32>(tomb->GetMaxHealth() * FrostTombHealthFactor);
            tomb->SetCreateHealth(health);
            tomb->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, static_cast<float>(health));
            tomb->SetMaxHealth(health);
            tomb->SetHealth(health);
        }
    }

    // Shadow Nova: a circle around him; he holds still until it bursts
    void ShadowNova()
    {
        _windup = true;
        me->StopMoving();
        me->SetControlled(true, UNIT_STATE_ROOT);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), ShadowNovaRadius,
            ShadowNovaWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(ShadowNovaWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Shadow);
            for (Player* player : PlayersIn(me, area))
                Hit(me, player, SPELL_SHADOW_NOVA, ShadowNovaPercent, ShadowNovaDamage);
            EndWindup();
        });
    }

    void EndWindup()
    {
        if (!_windup)
            return;

        _windup = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
    }

    bool _windup = false;
    GuidUnorderedSet _boostedTombs;
};

// ---------------------------------------------------------------------------------------------------------------------
// Skarvald the Constructor (and his ghost). Dalronn keeps his stock script: the pair calls work unchanged.
// ---------------------------------------------------------------------------------------------------------------------
enum SkarvaldTexts
{
    YELL_SKARVALD_AGGRO             = 0,
    YELL_SKARVALD_DAL_DIED          = 1,
    YELL_SKARVALD_SKA_DIEDFIRST     = 2,
    YELL_SKARVALD_KILL              = 3,
    YELL_SKARVALD_DAL_DIEDFIRST     = 4,
};

enum SkarvaldEvents
{
    EVENT_SKARVALD_PURSUIT          = 1,
    EVENT_SKARVALD_STONE_STRIKE,
    EVENT_SKARVALD_ENRAGE,
    EVENT_SKARVALD_MATE_DIED,
};

enum SkarvaldMisc
{
    ACTION_SKARVALD_MATE_DIED       = 1,
    POINT_SKARVALD_CHARGE           = 1,
};

constexpr float PursuitRadius = 6.0f;
constexpr uint32 PursuitMarkMs = 3000;        // the circle follows the marked player
constexpr uint32 PursuitLockMs = 1000;        // then stays where they stood before he charges it
constexpr uint32 PursuitSpotMs = 2500;        // how long the spot is drawn: the lock and his run
constexpr float PursuitSpeed = 30.0f;
constexpr float PursuitPercent = 55.0f;       // physical: armour takes its share
constexpr uint32 PursuitDamage = 9000;
// Stone Strike in a key, in weapon percent (stock 100): with no more threat wipes it only ever lands on the tank
constexpr int32 StoneStrikeMythicPoints = 149;

struct boss_skarvald_evolutions : public ScriptedAI
{
    boss_skarvald_evolutions(Creature* creature) : ScriptedAI(creature), _instance(creature->GetInstanceScript()) { }

    void Reset() override
    {
        me->SetLootMode(0);
        events.Reset();
        scheduler.CancelAll();
        _marking = false;
        _charging = false;
        if (me->GetEntry() == NPC_SKARVALD)
        {
            if (_instance)
                _instance->SetData(DATA_DALRONN_AND_SKARVALD, NOT_STARTED);
        }
        else if (Unit* target = me->SelectNearestTarget(50.0f))
        {
            // The ghost joins the fight at once
            me->AddThreat(target, 0.0f);
            AttackStart(target);
        }
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_SKARVALD_MATE_DIED)
            events.RescheduleEvent(EVENT_SKARVALD_MATE_DIED, 3500ms);
    }

    void JustEngagedWith(Unit* who) override
    {
        events.Reset();
        events.RescheduleEvent(EVENT_SKARVALD_PURSUIT, 8s);
        events.RescheduleEvent(EVENT_SKARVALD_STONE_STRIKE, 10s);
        if (me->GetEntry() == NPC_SKARVALD)
        {
            Talk(YELL_SKARVALD_AGGRO);
            if (IsHeroic())
                events.ScheduleEvent(EVENT_SKARVALD_ENRAGE, 1s);
        }

        if (!_instance)
            return;

        _instance->SetData(DATA_DALRONN_AND_SKARVALD, IN_PROGRESS);
        if (Creature* dalronn = _instance->instance->GetCreature(_instance->GetGuidData(DATA_DALRONN)))
            if (!dalronn->IsInCombat() && who)
            {
                dalronn->AddThreat(who, 0.0f);
                dalronn->AI()->AttackStart(who);
            }
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        if (me->GetEntry() == NPC_SKARVALD)
            Talk(YELL_SKARVALD_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        scheduler.CancelAll();
        if (me->GetEntry() != NPC_SKARVALD)
            return;

        if (_instance)
            if (Creature* dalronn = _instance->instance->GetCreature(_instance->GetGuidData(DATA_DALRONN)))
            {
                if (dalronn->isDead())
                {
                    Talk(YELL_SKARVALD_SKA_DIEDFIRST);
                    _instance->SetData(DATA_DALRONN_AND_SKARVALD, DONE);
                    _instance->SetData(DATA_UNLOCK_SKARVALD_LOOT, 0);
                    return;
                }

                Talk(YELL_SKARVALD_DAL_DIED);
                dalronn->AI()->DoAction(ACTION_SKARVALD_MATE_DIED);
            }
        me->CastSpell((Unit*)nullptr, SPELL_SUMMON_SKARVALD_GHOST, true);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_SKARVALD_CHARGE && _charging)
            Land();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        events.Update(diff);

        if (_charging || me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_SKARVALD_MATE_DIED:
                Talk(YELL_SKARVALD_DAL_DIEDFIRST);
                break;
            case EVENT_SKARVALD_PURSUIT:
                if (!_marking && MarkPursuit())
                    events.Repeat(14s, 18s);
                else
                    events.Repeat(4s);
                break;
            case EVENT_SKARVALD_STONE_STRIKE:
                if (Unit* victim = me->GetVictim(); victim && me->IsWithinMeleeRange(victim))
                {
                    if (me->GetMap()->IsMythic())
                        me->CastCustomSpell(victim, SPELL_STONE_STRIKE, &StoneStrikeMythicPoints, nullptr, nullptr,
                            false);
                    else
                        me->CastSpell(victim, SPELL_STONE_STRIKE, false);
                    events.Repeat(5s, 10s);
                }
                else
                    events.Repeat(3s);
                break;
            case EVENT_SKARVALD_ENRAGE:
                if (me->GetHealthPct() <= 60)
                {
                    me->CastSpell(me, SPELL_SKARVALD_ENRAGE, true);
                    break;
                }
                events.Repeat(1s);
                break;
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    // The mark: a circle follows a player who is not his tank, then stays where they stood, and he charges it
    bool MarkPursuit()
    {
        Unit* tank = me->GetVictim();
        float const range = IsHeroic() ? 100.0f : 30.0f;
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, [this, tank, range](Unit* unit)
        {
            return unit->IsPlayer() && unit != tank && unit->IsAlive() && me->IsWithinDistInMap(unit, range);
        });
        if (!target)
            return false;

        _marking = true;
        me->TextEmote(Acore::StringFormat("{} fixes his eyes on {}!", me->GetName(), target->GetName()), target,
            true);
        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, target, PursuitRadius,
            PursuitMarkMs);
        ObjectGuid const carrierGuid = target->GetGUID();
        scheduler.Schedule(Milliseconds(PursuitMarkMs), [this, area, carrierGuid](TaskContext)
        {
            _marking = false;
            Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            GroundIndicators::Area const spot = GroundIndicators::CurrentArea(carrier, area);
            _chargeArea = GroundIndicators::ShowCircle(me, spot.origin, spot.radius, PursuitSpotMs);
            scheduler.Schedule(Milliseconds(PursuitLockMs), [this](TaskContext)
            {
                _charging = true;
                Position const& to = _chargeArea.origin;
                me->GetMotionMaster()->MoveCharge(to.GetPositionX(), to.GetPositionY(), to.GetPositionZ(),
                    PursuitSpeed, POINT_SKARVALD_CHARGE, nullptr, true);
                // Whatever stops him on the way, the spot still bursts
                scheduler.Schedule(3s, [this](TaskContext)
                {
                    if (_charging)
                        Land();
                });
            });
        });
        return true;
    }

    void Land()
    {
        _charging = false;
        for (Player* player : PlayersIn(me, _chargeArea))
            Hit(me, player, SPELL_SKARVALD_CHARGE_IMPACT, PursuitPercent, PursuitDamage);
    }

    InstanceScript* _instance;
    bool _marking = false;
    bool _charging = false;
    GroundIndicators::Area _chargeArea;
};

// ---------------------------------------------------------------------------------------------------------------------
// Ingvar the Plunderer, reworked whole (boss_ingvar_evolutions): every hit is a share of the reference health in a key
// (heroic amounts outside one), drawn before it lands, and read by the bots. His stock script's Smashes, roars and
// thrown axe were stock spells only scaled by multipliers, and grew apart from the rest of the key.
//
// Phase one, the Plunderer:
// - Fracas: a cone follows a player who is not his tank for 2 s, stops, and lands 1 s later.
// - Haches lancées: two players carry a circle for 4 s, then an axe lands on it (spread).
// - Rugissement titubant: a 3 s cast to interrupt, or the whole group is struck.
// - Charge du berserker (from 50%): a line to the farthest player, then he charges down it.
// - Cleave on his tank.
// He falls at 0 health and Annhylde the Caller raises him (the stock resurrection, about 20 s). Meanwhile three
// Vrykul souls walk to his body from the edges of the hall: each one that reaches it adds 10% to his damage for the
// rest of the fight (Âmes dévorées).
// Phase two, undead:
// - Fracas ténébreux: a 4 s cast, then everything in the half circle in front of him is struck (the tank too).
// - Hache de l'ombre: his axe thrown on a player's spot burns there for 8 s, then flies back to him: a line drawn
//   1.5 s before, and what is on it is struck.
// - Rugissement d'effroi: the whole group, and a stack of Effroi (5% more damage taken for 20 s).
// - Frappe du malheur: a hit on his tank, and half the healing it receives for 6 s.
// - Tourbillon des âmes (from 50%): a circle round him to leave, then the ring around it to step back into.
// ---------------------------------------------------------------------------------------------------------------------
enum IngvarTexts
{
    YELL_INGVAR_AGGRO_1             = 0,
    YELL_INGVAR_KILL_1              = 1,
    YELL_INGVAR_DEAD_1              = 2,
    YELL_INGVAR_AGGRO_2             = 3,
    YELL_INGVAR_KILL_2              = 4,
    YELL_INGVAR_DEAD_2              = 5,
    EMOTE_INGVAR_ROAR               = 6,
    YELL_ANNHYLDE_RAISE             = 1,
};

enum IngvarEvents
{
    EVENT_INGVAR_SMASH              = 1,
    EVENT_INGVAR_AXES,
    EVENT_INGVAR_ROAR,
    EVENT_INGVAR_CHARGE,
    EVENT_INGVAR_CLEAVE,
    EVENT_INGVAR_DARK_SMASH,
    EVENT_INGVAR_SHADOW_AXE,
    EVENT_INGVAR_DREADFUL_ROAR,
    EVENT_INGVAR_WOE_STRIKE,
    EVENT_INGVAR_WHIRL,
    // The resurrection (the stock sequence)
    EVENT_INGVAR_FALL_YELL,
    EVENT_INGVAR_SUMMON_VALKYR,
    EVENT_INGVAR_VALKYR_DESCENT,
    EVENT_INGVAR_VALKYR_YELL,
    EVENT_INGVAR_VALKYR_BEAM,
    EVENT_INGVAR_RESURRECTION_BALL,
    EVENT_INGVAR_RESURRECTION_HEAL,
    EVENT_INGVAR_MORPH,
    EVENT_INGVAR_PHASE_TWO,
};

enum IngvarMisc
{
    DATA_INGVAR                     = 2,
    NPC_INGVAR_UNDEAD               = 23980,
    NPC_ANNHYLDE                    = 24068,
    NPC_VRYKUL_SPIRIT               = 930301,
    DISPLAY_INGVAR_HUMAN            = 21953,
    POINT_INGVAR_CHARGE             = 1,

    SPELL_INGVAR_SUMMON_VALKYR      = 42912,
    SPELL_INGVAR_RESURRECTION_BEAM  = 42857,
    SPELL_INGVAR_RESURRECTION_BALL  = 42862,
    SPELL_INGVAR_RESURRECTION_HEAL  = 42704,
    SPELL_INGVAR_TRANSFORM          = 42796,
    SPELL_INGVAR_CLEAVE             = 42724,

    // localTools/mythicDungeons/Spells.ps1: the cast bars (his own spells' animations), the hits and the auras
    SPELL_INGVAR_SMASH_CAST         = 94700,
    SPELL_INGVAR_SMASH              = 94701,
    SPELL_INGVAR_THROWN_AXE         = 94702,
    SPELL_INGVAR_ROAR_CAST          = 94703,
    SPELL_INGVAR_ROAR               = 94704,
    SPELL_INGVAR_CHARGE_CAST        = 94705,
    SPELL_INGVAR_CHARGE             = 94706,
    SPELL_INGVAR_DARK_SMASH_CAST    = 94707,
    SPELL_INGVAR_DARK_SMASH         = 94708,
    SPELL_INGVAR_SHADOW_AXE         = 94709,
    SPELL_INGVAR_DREADFUL_ROAR_CAST = 94710,
    SPELL_INGVAR_DREADFUL_ROAR      = 94711,
    SPELL_INGVAR_DREAD              = 94712,
    SPELL_INGVAR_WOE_STRIKE         = 94713,
    SPELL_INGVAR_WOE                = 94714,
    SPELL_INGVAR_WHIRL              = 94715,
    SPELL_INGVAR_DEVOURED           = 94716,

    // What lands, beside the red: stock visual kits played on the floor (GroundIndicators::PlayKit)
    KIT_INGVAR_GROUND_SLAM          = 10546,    // Ground Slam's cracks and dust: an axe's blow
    KIT_INGVAR_SHOCKWAVE            = 9855,     // Shockwave's impact: the charge's wake
    // (His shadow blows land as the shadow theme's burst, Shadowfury's: Dark Smash's own impact kit shows nothing on
    // the floor, Woe Strike's is a green swirl)
    // His undead phase's painted ground (localTools/hollowVoice textures: void and souls)
    LOOK_INGVAR_SOULS_CORE          = 90786,    // HV_EchoCore: a circle
    LOOK_INGVAR_SOULS_RING          = 90784,    // HV_EchoRing: a ring, inner 0.4 of the outer
    LOOK_INGVAR_AXE_SCORCH          = 90798,    // HV_VoidScorch: where his axe burns
};

// What a heroic hit does outside a key, for each percent of the reference health it takes in one
constexpr uint32 IngvarHeroicPerPercent = 220;

// Phase one
constexpr float SmashRadius = 22.0f;
constexpr float SmashArc = 90.0f;
constexpr uint32 SmashTrackMs = 2000;       // the cone follows its player
constexpr uint32 SmashLockMs = 1000;        // then stays where it pointed: the cast bar's 3 s in all
constexpr float SmashPercent = 60.0f;
constexpr uint32 ThrownAxes = 2;
constexpr float ThrownAxeRadius = 6.0f;
constexpr uint32 ThrownAxeMarkMs = 4000;
constexpr float ThrownAxePercent = 35.0f;
constexpr float RoarPercent = 25.0f;        // unless interrupted
constexpr float ChargeBelowPct = 50.0f;
constexpr float ChargeWidth = 5.0f;
constexpr uint32 ChargeWarnMs = 2500;       // the cast bar's
constexpr float ChargeSpeed = 35.0f;
constexpr float ChargePercent = 55.0f;
// The resurrection: the souls walk to his body from SpiritDistance yards (about 16 s, before he rises at 20 s)
constexpr uint32 Spirits = 3;
constexpr float SpiritDistance = 24.0f;
constexpr float SpiritWalkRate = 0.55f;
constexpr float SpiritReach = 3.0f;
// Phase two
constexpr float DarkSmashRadius = 25.0f;
constexpr float DarkSmashArc = 180.0f;
constexpr uint32 DarkSmashMs = 4000;
constexpr float DarkSmashPercent = 70.0f;
constexpr float ShadowAxeRadius = 8.0f;     // the thrown axe's own burning aura's (Shadow Axe, 42751)
constexpr uint32 ShadowAxeGroundMs = 8000;
constexpr uint32 ShadowAxeReturnWarnMs = 1500;
constexpr float ShadowAxeReturnWidth = 4.0f;
constexpr float ShadowAxeReturnPercent = 50.0f;
constexpr float DreadfulRoarPercent = 10.0f;
constexpr uint8 DreadMaxStacks = 10;
constexpr float WoeStrikePercent = 55.0f;   // about 38% of a tank
constexpr float WhirlBelowPct = 50.0f;
constexpr float WhirlInner = 12.0f;
constexpr float WhirlOuter = 30.0f;         // WhirlInner is 0.4 of it: the painted ring's proportion
constexpr uint32 WhirlCircleMs = 3000;
constexpr uint32 WhirlRingMs = 2500;
constexpr float WhirlPercent = 45.0f;

struct boss_ingvar_evolutions : public ScriptedAI
{
    boss_ingvar_evolutions(Creature* creature) : ScriptedAI(creature), _instance(creature->GetInstanceScript()),
        _summons(creature) { }

    void Reset() override
    {
        events.Reset();
        scheduler.CancelAll();
        _summons.DespawnAll();
        _spirits.clear();
        _valkyr.Clear();
        _axe.Clear();
        _busy = _charging = _fallen = false;
        _devoured = 0;
        me->SetDisplayId(DISPLAY_INGVAR_HUMAN);
        me->LoadEquipment(1);
        FeignDeath(false);
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        Hold(false);
        if (_instance)
            _instance->SetData(DATA_INGVAR, NOT_STARTED);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        Hold(false);
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.Reset();
        events.ScheduleEvent(EVENT_INGVAR_CLEAVE, 4s);
        events.ScheduleEvent(EVENT_INGVAR_SMASH, 8s);
        events.ScheduleEvent(EVENT_INGVAR_AXES, 14s);
        events.ScheduleEvent(EVENT_INGVAR_ROAR, 20s);
        events.ScheduleEvent(EVENT_INGVAR_CHARGE, 5s);
        Talk(YELL_INGVAR_AGGRO_1);
        me->LowerPlayerDamageReq(me->GetMaxHealth());
        if (_instance)
            _instance->SetData(DATA_INGVAR, IN_PROGRESS);
    }

    void JustSummoned(Creature* summon) override
    {
        _summons.Summon(summon);
        if (summon->GetEntry() == NPC_ANNHYLDE)
        {
            _valkyr = summon->GetGUID();
            summon->SetCanFly(true);
            summon->SetDisableGravity(true);
            summon->SetPosition(summon->GetPositionX(), summon->GetPositionY(), summon->GetPositionZ() + 35.0f,
                summon->GetOrientation());
            summon->SetFacingTo(summon->GetOrientation());
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(me->GetDisplayId() == DISPLAY_INGVAR_HUMAN ? YELL_INGVAR_KILL_2 : YELL_INGVAR_KILL_1);
    }

    void JustDied(Unit* /*killer*/) override
    {
        events.Reset();
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        _summons.DespawnAll();
        Talk(YELL_INGVAR_DEAD_2);
        if (_instance)
        {
            // The undead entry, for the achievements
            _instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE, NPC_INGVAR_UNDEAD, 1);
            _instance->SetData(DATA_INGVAR, DONE);
        }
    }

    // His first death is not one: he lies there until Annhylde raises him (residual damage meanwhile kills nothing)
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
    {
        if (me->HasUnitFlag2(UNIT_FLAG2_FEIGN_DEATH))
        {
            if (damage >= me->GetHealth())
                damage = me->GetHealth() - 1;
            return;
        }
        if (_fallen || damage < me->GetHealth())
            return;

        damage = me->GetHealth() - 1;
        Fall();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == POINT_INGVAR_CHARGE && _charging)
            LandCharge();
    }

    void OnSpellCast(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_INGVAR_ROAR_CAST)
        {
            for (Player* player : PlayersNear(me, 100.0f))
                IngvarHit(player, SPELL_INGVAR_ROAR, RoarPercent);
        }
        else if (spell->Id == SPELL_INGVAR_DREADFUL_ROAR_CAST)
        {
            for (Player* player : PlayersNear(me, 100.0f))
            {
                IngvarHit(player, SPELL_INGVAR_DREADFUL_ROAR, DreadfulRoarPercent);
                if (Aura* dread = player->GetAura(SPELL_INGVAR_DREAD))
                {
                    if (dread->GetStackAmount() < DreadMaxStacks)
                        dread->ModStackAmount(1);
                    dread->RefreshDuration();
                }
                else
                    me->AddAura(SPELL_INGVAR_DREAD, player);
            }
        }
    }

    // The roar interrupted: nothing lands
    void OnSpellFailed(SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_INGVAR_ROAR_CAST)
            me->TextEmote("Le rugissement d'Ingvar est étouffé !", nullptr, true);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        events.Update(diff);

        if (_fallen)
        {
            UpdateResurrection();
            return;
        }

        if (_busy || me->HasUnitState(UNIT_STATE_CASTING))
        {
            if (!_charging && !me->HasUnitState(UNIT_STATE_CASTING))
                DoMeleeAttackIfReady();
            return;
        }

        switch (events.ExecuteEvent())
        {
            case EVENT_INGVAR_CLEAVE:
                DoCastVictim(SPELL_INGVAR_CLEAVE);
                events.Repeat(6s, 9s);
                break;
            case EVENT_INGVAR_SMASH:
                Smash();
                events.Repeat(13s, 16s);
                break;
            case EVENT_INGVAR_AXES:
                ThrowAxes();
                events.Repeat(18s, 22s);
                break;
            case EVENT_INGVAR_ROAR:
                Talk(EMOTE_INGVAR_ROAR);
                me->CastSpell(me, SPELL_INGVAR_ROAR_CAST, false);
                events.Repeat(22s, 26s);
                break;
            case EVENT_INGVAR_CHARGE:
                if (me->GetHealthPct() > ChargeBelowPct || !Charge())
                {
                    events.Repeat(2s);
                    break;
                }
                events.Repeat(20s, 24s);
                break;
            case EVENT_INGVAR_DARK_SMASH:
                DarkSmash();
                events.Repeat(15s, 18s);
                break;
            case EVENT_INGVAR_SHADOW_AXE:
                if (!ThrowShadowAxe())
                {
                    events.Repeat(3s);
                    break;
                }
                events.Repeat(26s, 30s);
                break;
            case EVENT_INGVAR_DREADFUL_ROAR:
                Talk(EMOTE_INGVAR_ROAR);
                me->CastSpell(me, SPELL_INGVAR_DREADFUL_ROAR_CAST, false);
                events.Repeat(18s, 22s);
                break;
            case EVENT_INGVAR_WOE_STRIKE:
                if (Unit* tank = me->GetVictim(); tank && me->IsWithinMeleeRange(tank))
                {
                    WoeStrike(tank);
                    events.Repeat(10s, 13s);
                }
                else
                    events.Repeat(2s);
                break;
            case EVENT_INGVAR_WHIRL:
                if (me->GetHealthPct() > WhirlBelowPct)
                {
                    events.Repeat(2s);
                    break;
                }
                Whirl();
                events.Repeat(28s, 32s);
                break;
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    // A hit of his, the souls he has devoured on top
    void IngvarHit(Unit* target, uint32 spellId, float percent)
    {
        float const share = percent * (1.0f + 0.1f * static_cast<float>(_devoured));
        Hit(me, target, spellId, share, static_cast<uint32>(share * IngvarHeroicPerPercent));
    }

    void HitAll(GroundIndicators::Area const& area, uint32 spellId, float percent)
    {
        for (Player* player : PlayersIn(me, area))
            IngvarHit(player, spellId, percent);
    }

    // He stands his ground and keeps facing where he was set to while a blow is wound up
    void Hold(bool apply)
    {
        if (apply)
            me->StopMoving();
        me->SetControlled(apply, UNIT_STATE_ROOT);
        me->DisableRotate(apply);
        me->SendMovementFlagUpdate();
    }

    // A random player who is not his tank (his tank when no one else is near)
    Unit* PickOther(float range)
    {
        Unit* tank = me->GetVictim();
        std::vector<Player*> players = PlayersNear(me, range);
        std::vector<Player*> others;
        for (Player* player : players)
            if (player != tank)
                others.push_back(player);
        if (!others.empty())
            return others[urand(0, others.size() - 1)];
        return tank;
    }

    // Points down the middle of a cone or a line, for its impacts: at 30%, 60% and 90% of its length
    static std::vector<Position> AlongMiddle(Position const& origin, float orientation, float length)
    {
        std::vector<Position> points;
        for (float share : { 0.3f, 0.6f, 0.9f })
            points.emplace_back(origin.GetPositionX() + std::cos(orientation) * length * share,
                origin.GetPositionY() + std::sin(orientation) * length * share, origin.GetPositionZ());
        return points;
    }

    // --- Phase one ---------------------------------------------------------------------------------------------------

    // Fracas: the cone follows its player, stops, and the axe comes down on it
    void Smash()
    {
        Unit* target = PickOther(40.0f);
        if (!target)
            return;
        _busy = true;
        me->SetFacingToObject(target);
        Hold(true);
        me->CastSpell(me, SPELL_INGVAR_SMASH_CAST, false);
        GroundIndicators::Area const tracking = GroundIndicators::ShowTrackingCone(me, me->GetPosition(), SmashRadius,
            SmashArc, SmashTrackMs, target);
        ObjectGuid const targetGuid = target->GetGUID();
        scheduler.Schedule(Milliseconds(SmashTrackMs), [this, tracking, targetGuid](TaskContext)
        {
            Unit* aimed = ObjectAccessor::GetUnit(*me, targetGuid);
            GroundIndicators::Area const pointed = aimed ? GroundIndicators::CurrentCone(aimed, tracking) : tracking;
            float const orientation = pointed.origin.GetOrientation();
            me->SetFacingTo(orientation);
            GroundIndicators::Area const cone = GroundIndicators::ShowCone(me, pointed.origin, orientation,
                SmashRadius, SmashArc, SmashLockMs, GroundIndicators::Theme::None,
                static_cast<uint32>(SmashPercent * IngvarHeroicPerPercent));
            scheduler.Schedule(Milliseconds(SmashLockMs), [this, cone](TaskContext)
            {
                for (Position const& point : AlongMiddle(cone.origin, cone.origin.GetOrientation(), SmashRadius))
                    GroundIndicators::PlayKit(me, point, KIT_INGVAR_GROUND_SLAM, 1.6f);
                HitAll(cone, SPELL_INGVAR_SMASH, SmashPercent);
                Hold(false);
                _busy = false;
            });
        });
    }

    // Haches lancées: a circle on two players (not his tank when others are there), an axe lands on each
    void ThrowAxes()
    {
        Unit* tank = me->GetVictim();
        std::vector<Player*> candidates;
        for (Player* player : PlayersNear(me, 60.0f))
            if (player != tank)
                candidates.push_back(player);
        if (candidates.empty())
            return;
        Acore::Containers::RandomResize(candidates, ThrownAxes);
        for (Player* target : candidates)
        {
            me->TextEmote("Ingvar vous lance une hache ! Éloignez-vous des autres !", target, true);
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, target, ThrownAxeRadius,
                ThrownAxeMarkMs, static_cast<uint32>(ThrownAxePercent * IngvarHeroicPerPercent));
            ObjectGuid const carrierGuid = target->GetGUID();
            scheduler.Schedule(Milliseconds(ThrownAxeMarkMs), [this, area, carrierGuid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
                if (!carrier)
                    return;
                GroundIndicators::Area const landed = GroundIndicators::CurrentArea(carrier, area);
                GroundIndicators::PlayKit(me, landed.origin, KIT_INGVAR_GROUND_SLAM, 1.4f);
                HitAll(landed, SPELL_INGVAR_THROWN_AXE, ThrownAxePercent);
            });
        }
    }

    // Charge du berserker: a line to the farthest player, then he runs down it and knocks over whoever is on it
    bool Charge()
    {
        Unit* tank = me->GetVictim();
        Player* target = nullptr;
        float farthest = 0.0f;
        for (Player* player : PlayersNear(me, 40.0f))
        {
            float const distance = me->GetDistance2d(player);
            if (player != tank && distance > farthest)
            {
                target = player;
                farthest = distance;
            }
        }
        if (!target || farthest < 8.0f)
            return false;

        _busy = true;
        me->SetFacingToObject(target);
        Hold(true);
        me->CastSpell(me, SPELL_INGVAR_CHARGE_CAST, false);
        me->TextEmote(Acore::StringFormat("Ingvar fixe {} et s'apprête à charger !", target->GetName()), nullptr,
            true);
        float const orientation = me->GetAngle(target);
        float const length = farthest + 2.0f;
        _chargeLine = GroundIndicators::ShowRectangle(me, me->GetPosition(), orientation, length, ChargeWidth,
            ChargeWarnMs + 1000, GroundIndicators::Theme::None,
            static_cast<uint32>(ChargePercent * IngvarHeroicPerPercent));
        Position const end(me->GetPositionX() + std::cos(orientation) * farthest,
            me->GetPositionY() + std::sin(orientation) * farthest, me->GetPositionZ());
        scheduler.Schedule(Milliseconds(ChargeWarnMs), [this, end](TaskContext)
        {
            Hold(false);
            _charging = true;
            me->GetMotionMaster()->MoveCharge(end.GetPositionX(), end.GetPositionY(), end.GetPositionZ(), ChargeSpeed,
                POINT_INGVAR_CHARGE, nullptr, true);
            // Whatever stops him on the way, the line still lands
            scheduler.Schedule(2s, [this](TaskContext)
            {
                if (_charging)
                    LandCharge();
            });
        });
        return true;
    }

    void LandCharge()
    {
        _charging = false;
        float const orientation = _chargeLine.origin.GetOrientation();
        for (Position const& point : AlongMiddle(_chargeLine.origin, orientation, _chargeLine.radius))
            GroundIndicators::PlayKit(me, point, KIT_INGVAR_SHOCKWAVE);
        HitAll(_chargeLine, SPELL_INGVAR_CHARGE, ChargePercent);
        _busy = false;
        if (Unit* victim = me->GetVictim())
            me->GetMotionMaster()->MoveChase(victim);
    }

    // --- The resurrection --------------------------------------------------------------------------------------------

    void FeignDeath(bool apply)
    {
        if (apply)
        {
            me->SetStandState(UNIT_STAND_STATE_DEAD);
            me->SetUnitFlag(UNIT_FLAG_PREVENT_EMOTES_FROM_CHAT_TEXT);
            me->SetUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
            me->SetDynamicFlag(UNIT_DYNFLAG_DEAD);
        }
        else
        {
            me->SetStandState(UNIT_STAND_STATE_STAND);
            me->RemoveUnitFlag(UNIT_FLAG_PREVENT_EMOTES_FROM_CHAT_TEXT);
            me->RemoveUnitFlag2(UNIT_FLAG2_FEIGN_DEATH);
            me->RemoveDynamicFlag(UNIT_DYNFLAG_DEAD);
        }
    }

    // His first fall: everything he had going stops, and Annhylde comes for him
    void Fall()
    {
        _fallen = true;
        _busy = _charging = false;
        scheduler.CancelAll();
        GroundIndicators::ClearAreasOf(me);
        me->InterruptNonMeleeSpells(true);
        me->RemoveAllAuras();
        me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        Hold(false);
        me->GetMotionMaster()->MovementExpired();
        me->GetMotionMaster()->MoveIdle();
        me->StopMoving();
        FeignDeath(true);
        events.Reset();
        events.ScheduleEvent(EVENT_INGVAR_FALL_YELL, 0ms);
        events.ScheduleEvent(EVENT_INGVAR_SUMMON_VALKYR, 1s);
        SummonSpirits();
    }

    // Three Vrykul souls from the edges of the hall, walking to his body: killed on the way, or they feed him
    void SummonSpirits()
    {
        me->TextEmote("Des âmes vrykules accourent vers le corps d'Ingvar ! Abattez-les avant qu'elles ne "
            "l'atteignent !", nullptr, true);
        float const first = frand(0.0f, 2.0f * float(M_PI));
        for (uint32 index = 0; index < Spirits; ++index)
        {
            float const angle = first + 2.0f * float(M_PI) * static_cast<float>(index) / static_cast<float>(Spirits);
            Position const from = me->GetFirstCollisionPosition(SpiritDistance, angle - me->GetOrientation());
            Creature* spirit = me->SummonCreature(NPC_VRYKUL_SPIRIT, from, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 5000);
            if (!spirit)
                continue;
            spirit->SetReactState(REACT_PASSIVE);
            spirit->SetWalk(true);
            spirit->SetSpeedRate(MOVE_WALK, SpiritWalkRate);
            spirit->SetInCombatWithZone();
            spirit->GetMotionMaster()->MovePoint(0, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
            GroundIndicators::Burst(me, from, GroundIndicators::Theme::Shadow);
            _spirits.push_back(spirit->GetGUID());
        }
        scheduler.Schedule(500ms, [this](TaskContext context)
        {
            CheckSpirits();
            if (_fallen && !_spirits.empty())
                context.Repeat();
        });
    }

    // A soul at his body joins him; one killed on the way is gone
    void CheckSpirits()
    {
        for (auto it = _spirits.begin(); it != _spirits.end();)
        {
            Creature* spirit = ObjectAccessor::GetCreature(*me, *it);
            if (!spirit || !spirit->IsAlive())
            {
                it = _spirits.erase(it);
                continue;
            }
            if (spirit->GetDistance2d(me) <= SpiritReach)
            {
                Devour(spirit);
                it = _spirits.erase(it);
                continue;
            }
            ++it;
        }
    }

    void Devour(Creature* spirit)
    {
        ++_devoured;
        GroundIndicators::Burst(me, me->GetPosition(), GroundIndicators::Theme::Shadow);
        spirit->DespawnOrUnsummon();
        me->TextEmote(Acore::StringFormat("Une âme vrykule rejoint Ingvar ! ({} / {})", _devoured, Spirits), nullptr,
            true);
    }

    // The buff his souls give him, once he stands again (his fall took every aura off)
    void ShowDevoured()
    {
        if (!_devoured)
            return;
        me->AddAura(SPELL_INGVAR_DEVOURED, me);
        if (Aura* devoured = me->GetAura(SPELL_INGVAR_DEVOURED))
            devoured->SetStackAmount(static_cast<uint8>(_devoured));
    }

    void UpdateResurrection()
    {
        switch (events.ExecuteEvent())
        {
            case EVENT_INGVAR_FALL_YELL:
                Talk(YELL_INGVAR_DEAD_1);
                break;
            case EVENT_INGVAR_SUMMON_VALKYR:
                me->CastSpell(me, SPELL_INGVAR_SUMMON_VALKYR, true);
                events.ScheduleEvent(EVENT_INGVAR_VALKYR_DESCENT, 1ms);
                events.ScheduleEvent(EVENT_INGVAR_VALKYR_YELL, 3s);
                events.ScheduleEvent(EVENT_INGVAR_VALKYR_BEAM, 7s);
                break;
            case EVENT_INGVAR_VALKYR_DESCENT:
                if (Creature* valkyr = ObjectAccessor::GetCreature(*me, _valkyr))
                    valkyr->GetMotionMaster()->MovePoint(1, valkyr->GetPositionX(), valkyr->GetPositionY(),
                        valkyr->GetPositionZ() - 15.0f);
                break;
            case EVENT_INGVAR_VALKYR_YELL:
                if (Creature* valkyr = ObjectAccessor::GetCreature(*me, _valkyr))
                    valkyr->AI()->Talk(YELL_ANNHYLDE_RAISE);
                break;
            case EVENT_INGVAR_VALKYR_BEAM:
                me->RemoveAura(SPELL_INGVAR_SUMMON_VALKYR);
                if (Creature* valkyr = ObjectAccessor::GetCreature(*me, _valkyr))
                    valkyr->CastSpell(me, SPELL_INGVAR_RESURRECTION_BEAM, false);
                events.ScheduleEvent(EVENT_INGVAR_RESURRECTION_BALL, 4s);
                break;
            case EVENT_INGVAR_RESURRECTION_BALL:
                me->CastSpell(me, SPELL_INGVAR_RESURRECTION_BALL, true);
                events.ScheduleEvent(EVENT_INGVAR_RESURRECTION_HEAL, 4s);
                break;
            case EVENT_INGVAR_RESURRECTION_HEAL:
                me->RemoveAura(SPELL_INGVAR_RESURRECTION_BALL);
                me->CastSpell(me, SPELL_INGVAR_RESURRECTION_HEAL, true);
                FeignDeath(false);
                events.ScheduleEvent(EVENT_INGVAR_MORPH, 3s);
                break;
            case EVENT_INGVAR_MORPH:
                me->CastSpell(me, SPELL_INGVAR_TRANSFORM, true);
                events.ScheduleEvent(EVENT_INGVAR_PHASE_TWO, 1s);
                break;
            case EVENT_INGVAR_PHASE_TWO:
                StartPhaseTwo();
                break;
            default:
                break;
        }
    }

    void StartPhaseTwo()
    {
        // A soul still walking when he rises reaches him as he stands
        for (ObjectGuid const& guid : _spirits)
            if (Creature* spirit = ObjectAccessor::GetCreature(*me, guid); spirit && spirit->IsAlive())
                Devour(spirit);
        _spirits.clear();
        if (Creature* valkyr = ObjectAccessor::GetCreature(*me, _valkyr))
            valkyr->DespawnOrUnsummon();
        _fallen = false;
        ShowDevoured();
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
        if (Unit* victim = me->GetVictim())
        {
            AttackStart(victim);
            me->GetMotionMaster()->MoveChase(victim);
        }
        Talk(YELL_INGVAR_AGGRO_2);
        events.Reset();
        events.ScheduleEvent(EVENT_INGVAR_DREADFUL_ROAR, 2s);
        events.ScheduleEvent(EVENT_INGVAR_WOE_STRIKE, 6s);
        events.ScheduleEvent(EVENT_INGVAR_DARK_SMASH, 9s);
        events.ScheduleEvent(EVENT_INGVAR_SHADOW_AXE, 15s);
        events.ScheduleEvent(EVENT_INGVAR_WHIRL, 5s);
    }

    // --- Phase two ---------------------------------------------------------------------------------------------------

    // Fracas ténébreux: everything in the half circle in front of him, his tank too
    void DarkSmash()
    {
        _busy = true;
        if (Unit* victim = me->GetVictim())
            me->SetFacingToObject(victim);
        Hold(true);
        me->CastSpell(me, SPELL_INGVAR_DARK_SMASH_CAST, false);
        GroundIndicators::Area const cone = GroundIndicators::ShowCone(me, me->GetPosition(), me->GetOrientation(),
            DarkSmashRadius, DarkSmashArc, DarkSmashMs, GroundIndicators::Theme::Shadow,
            static_cast<uint32>(DarkSmashPercent * IngvarHeroicPerPercent));
        scheduler.Schedule(Milliseconds(DarkSmashMs), [this, cone](TaskContext)
        {
            float const orientation = cone.origin.GetOrientation();
            for (float turn : { -1.0f, 0.0f, 1.0f })
                for (Position const& point : AlongMiddle(cone.origin, orientation + turn, DarkSmashRadius))
                    if (point.GetExactDist2d(cone.origin) > DarkSmashRadius * 0.5f)
                        GroundIndicators::Burst(me, point, GroundIndicators::Theme::Shadow);
            HitAll(cone, SPELL_INGVAR_DARK_SMASH, DarkSmashPercent);
            Hold(false);
            _busy = false;
        });
    }

    // Hache de l'ombre: thrown at a player's spot, it burns there, then flies back to him down a line drawn first
    bool ThrowShadowAxe()
    {
        Unit* target = PickOther(40.0f);
        if (!target)
            return false;
        Position const spot = target->GetPosition();
        Creature* axe = me->SummonCreature(NPC_INGVAR_THROW_DUMMY, me->GetPosition(), TEMPSUMMON_TIMED_DESPAWN,
            ShadowAxeGroundMs + ShadowAxeReturnWarnMs + 3000);
        if (!axe)
            return false;
        _axe = axe->GetGUID();
        axe->GetMotionMaster()->MovePoint(0, spot);
        SetEquipmentSlots(false, EQUIP_UNEQUIP, EQUIP_NO_CHANGE, EQUIP_NO_CHANGE);
        GroundIndicators::Area burn;
        burn.kind = GroundIndicators::Area::Kind::Circle;
        burn.origin = spot;
        burn.radius = ShadowAxeRadius;
        GroundIndicators::ShowPainted(me, burn, LOOK_INGVAR_AXE_SCORCH, ShadowAxeGroundMs);
        scheduler.Schedule(Milliseconds(ShadowAxeGroundMs), [this, spot](TaskContext)
        {
            float const orientation = spot.GetAngle(me);
            float const length = spot.GetExactDist2d(me);
            GroundIndicators::Area const path = GroundIndicators::ShowRectangle(me, spot, orientation, length,
                ShadowAxeReturnWidth, ShadowAxeReturnWarnMs, GroundIndicators::Theme::Shadow,
                static_cast<uint32>(ShadowAxeReturnPercent * IngvarHeroicPerPercent));
            scheduler.Schedule(Milliseconds(ShadowAxeReturnWarnMs), [this, path](TaskContext)
            {
                if (Creature* axe = ObjectAccessor::GetCreature(*me, _axe))
                    axe->GetMotionMaster()->MoveCharge(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
                HitAll(path, SPELL_INGVAR_SHADOW_AXE, ShadowAxeReturnPercent);
                scheduler.Schedule(1500ms, [this](TaskContext)
                {
                    if (Creature* axe = ObjectAccessor::GetCreature(*me, _axe))
                        axe->DespawnOrUnsummon();
                    _axe.Clear();
                    SetEquipmentSlots(true);
                });
            });
        });
        return true;
    }

    // Frappe du malheur: his tank, and the healing it takes halved for a while
    void WoeStrike(Unit* tank)
    {
        GroundIndicators::Burst(me, tank->GetPosition(), GroundIndicators::Theme::Shadow);
        IngvarHit(tank, SPELL_INGVAR_WOE_STRIKE, WoeStrikePercent);
        me->AddAura(SPELL_INGVAR_WOE, tank);
    }

    // Tourbillon des âmes: the souls burst round him (leave), then in the ring beyond (come back in)
    void Whirl()
    {
        _busy = true;
        Hold(true);
        me->TextEmote("Les âmes tourbillonnent autour d'Ingvar ! Éloignez-vous, puis revenez vers lui !", nullptr,
            true);
        GroundIndicators::Area core;
        core.kind = GroundIndicators::Area::Kind::Circle;
        core.origin = me->GetPosition();
        core.radius = WhirlInner;
        core = GroundIndicators::ShowPainted(me, core, LOOK_INGVAR_SOULS_CORE, WhirlCircleMs,
            GroundIndicators::Theme::None, static_cast<uint32>(WhirlPercent * IngvarHeroicPerPercent));
        scheduler.Schedule(Milliseconds(WhirlCircleMs), [this, core](TaskContext)
        {
            GroundIndicators::Burst(me, core.origin, GroundIndicators::Theme::Shadow);
            HitAll(core, SPELL_INGVAR_WHIRL, WhirlPercent);
            GroundIndicators::Area ring;
            ring.kind = GroundIndicators::Area::Kind::Ring;
            ring.origin = core.origin;
            ring.radius = WhirlOuter;
            ring.inner = WhirlInner;
            ring = GroundIndicators::ShowPainted(me, ring, LOOK_INGVAR_SOULS_RING, WhirlRingMs,
                GroundIndicators::Theme::None, static_cast<uint32>(WhirlPercent * IngvarHeroicPerPercent));
            scheduler.Schedule(Milliseconds(WhirlRingMs), [this, ring](TaskContext)
            {
                float const middle = (ring.inner + ring.radius) / 2.0f;
                for (uint32 index = 0; index < 6; ++index)
                {
                    float const angle = 2.0f * float(M_PI) * static_cast<float>(index) / 6.0f;
                    GroundIndicators::Burst(me, Position(ring.origin.GetPositionX() + std::cos(angle) * middle,
                        ring.origin.GetPositionY() + std::sin(angle) * middle, ring.origin.GetPositionZ()),
                        GroundIndicators::Theme::Shadow);
                }
                HitAll(ring, SPELL_INGVAR_WHIRL, WhirlPercent);
                Hold(false);
                _busy = false;
            });
        });
    }

    InstanceScript* _instance;
    SummonList _summons;
    GuidVector _spirits;
    ObjectGuid _valkyr;
    ObjectGuid _axe;
    GroundIndicators::Area _chargeLine;
    bool _busy = false;
    bool _charging = false;
    bool _fallen = false;
    uint32 _devoured = 0;
};

void RegisterTuning()
{
    // Prince Keleseth: nothing threatened a +10 group
    MythicTuning::SetSpellMultiplier(SPELL_KELESETH_SHADOW_BOLT, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_KELESETH_SHADOW_BOLT_H, 1.3f);
    MythicTuning::SetMeleeMultiplier(NPC_VRYKUL_SKELETON, 3.0f);
    MythicTuning::SetMeleeMultiplier(NPC_VRYKUL_SKELETON_HEROIC, 3.0f);
    // The Frost Tomb is summoned but is the fight's check, not an add: it keeps an elite's health (and
    // FrostTombHealthFactor on top)
    MythicTuning::SetCreatureRole(NPC_FROST_TOMB, MythicTuning::CreatureRole::Elite);
    MythicTuning::SetCreatureRole(NPC_FROST_TOMB_HEROIC, MythicTuning::CreatureRole::Elite);

    // Dalronn: Debilitate was a weak damage over time
    MythicTuning::SetSpellMultiplier(SPELL_DALRONN_DEBILITATE, 2.0f);

    // Ingvar: his reworked hits are shares of the reference health (boss_ingvar_evolutions); the Vrykul souls of his
    // resurrection are a boss's adds, of a minion's health
    MythicTuning::SetCreatureRole(NPC_VRYKUL_SPIRIT, MythicTuning::CreatureRole::Minion);

    // Trash: the proto-drake's Rend was 41% of a tank; the Ticking Time Bomb (a classic template, so its explosion
    // took the classic level catch-up on top) 63% of a damage dealer
    MythicTuning::SetSpellMultiplier(SPELL_PROTO_DRAKE_REND_H, 0.7f);
    MythicTuning::SetSpellMultiplier(SPELL_TICKING_BOMB_EXPLOSION_H, 0.45f);
}

void RegisterTrash()
{
    using MythicTrash::Shape;
    using GroundIndicators::Theme;

    // Dragonflayer Ironhelm: Shockwave, a line at its target
    for (uint32 entry : { NPC_DRAGONFLAYER_IRONHELM, NPC_DRAGONFLAYER_IRONHELM_H })
    {
        MythicTrash::Ability shockwave;
        shockwave.entry = entry;
        shockwave.shape = Shape::LineAtVictim;
        shockwave.size = 16.0f;
        shockwave.width = 5.0f;
        shockwave.warnMs = 2500;
        shockwave.cooldownMs = 20000;
        shockwave.firstMs = 7000;
        shockwave.percent = 30.0f;
        shockwave.spellId = SPELL_SHOCKWAVE;
        shockwave.theme = Theme::None;
        shockwave.holdStill = true;
        MythicTrash::Register(shockwave);
    }

    // Dragonflayer Runecaster: Rune Detonation, a fire rune under a player
    for (uint32 entry : { NPC_DRAGONFLAYER_RUNECASTER, NPC_DRAGONFLAYER_RUNECASTER_H })
    {
        MythicTrash::Ability rune;
        rune.entry = entry;
        rune.shape = Shape::UnderTarget;
        rune.size = 5.0f;
        rune.warnMs = 2500;
        rune.cooldownMs = 18000;
        rune.firstMs = 6000;
        rune.percent = 25.0f;
        rune.spellId = SPELL_RUNE_DETONATION;
        rune.theme = Theme::Fire;
        rune.holdStill = false;
        MythicTrash::Register(rune);
    }

    // Dragonflayer Bonecrusher: Ground Slam, a circle around itself
    for (uint32 entry : { NPC_DRAGONFLAYER_BONECRUSHER, NPC_DRAGONFLAYER_BONECRUSHER_H })
    {
        MythicTrash::Ability slam;
        slam.entry = entry;
        slam.shape = Shape::AroundSelf;
        slam.size = 8.0f;
        slam.warnMs = 2500;
        slam.cooldownMs = 22000;
        slam.firstMs = 9000;
        slam.percent = 30.0f;
        slam.spellId = SPELL_GROUND_SLAM;
        slam.theme = Theme::None;
        slam.holdStill = true;
        MythicTrash::Register(slam);
    }
}
}

void AddMythicUtgardeKeepScripts()
{
    RegisterTuning();
    RegisterTrash();
    RegisterCreatureAI(boss_keleseth_evolutions);
    RegisterCreatureAI(boss_skarvald_evolutions);
    RegisterCreatureAI(boss_ingvar_evolutions);
}
