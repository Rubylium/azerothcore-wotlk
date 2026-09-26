#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "AllCreatureScript.h"
#include "CreatureScript.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
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
// Ingvar the Plunderer keeps his stock script: his Smashes are cut down to what a telegraphed cone may hit for, and his
// thrown Shadow Axe now carries its red circle (the stock damaging aura is applied before the axe is in the world, so
// it was never drawn).
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

    // Ingvar the Plunderer
    SPELL_SMASH                     = 42669,
    SPELL_SMASH_H                   = 59706,
    SPELL_DARK_SMASH                = 42723,
    SPELL_DARK_SMASH_H              = 59709,
    SPELL_DREADFUL_ROAR             = 42729,
    SPELL_DREADFUL_ROAR_H           = 59734,
    SPELL_STAGGERING_ROAR           = 42708,
    SPELL_STAGGERING_ROAR_H         = 59708,

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
// Ingvar's Shadow Axe: the thrown axe carries its red circle (5 yards, its periodic Shadow Axe)
// ---------------------------------------------------------------------------------------------------------------------
constexpr float ShadowAxeRadius = 5.0f;
constexpr uint32 ShadowAxeMs = 12000;       // thrown, 10 s on the ground, 1.5 s back to him

class UtgardeKeepMythicCreatureScript : public AllCreatureScript
{
public:
    UtgardeKeepMythicCreatureScript() : AllCreatureScript("UtgardeKeepMythicCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        uint32 const entry = creature->GetEntry();
        if (entry != NPC_INGVAR_THROW_DUMMY && entry != NPC_INGVAR_THROW_DUMMY_HEROIC)
            return;
        Map* map = creature->FindMap();
        if (!map || !map->IsMythic())
            return;

        GroundIndicators::ShowCarriedCircle(creature, creature, ShadowAxeRadius, ShadowAxeMs);
    }
};

void RegisterTuning()
{
    // Prince Keleseth: nothing threatened a +10 group
    MythicTuning::SetSpellMultiplier(SPELL_KELESETH_SHADOW_BOLT, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_KELESETH_SHADOW_BOLT_H, 1.3f);
    MythicTuning::SetMeleeMultiplier(NPC_VRYKUL_SKELETON, 3.0f);
    MythicTuning::SetMeleeMultiplier(NPC_VRYKUL_SKELETON_HEROIC, 3.0f);

    // Dalronn: Debilitate was a weak damage over time
    MythicTuning::SetSpellMultiplier(SPELL_DALRONN_DEBILITATE, 2.0f);

    // Ingvar: both Smashes killed any non-tank in the cone, Dark Smash nearly the tank; the roars pulsed too hard
    MythicTuning::SetSpellMultiplier(SPELL_SMASH, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_SMASH_H, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_DARK_SMASH, 0.5f);
    MythicTuning::SetSpellMultiplier(SPELL_DARK_SMASH_H, 0.5f);
    MythicTuning::SetSpellMultiplier(SPELL_DREADFUL_ROAR, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_DREADFUL_ROAR_H, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_STAGGERING_ROAR, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_STAGGERING_ROAR_H, 1.3f);

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
    new UtgardeKeepMythicCreatureScript();
}
