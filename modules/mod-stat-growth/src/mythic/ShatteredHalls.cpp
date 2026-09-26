#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "Containers.h"
#include "CreatureScript.h"
#include "GameObject.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <list>
#include <string>
#include <vector>

// The Shattered Halls in Mythic+: damage tuning, boss reworks and trash abilities (audit:
// .agents/plans/mplus-damage-audit/).
//
// About half of the damage budget: the trash packs press the tank hard, but the first two bosses were free and the
// danger of the last two sat in two mechanics nobody could see. Here the bosses' melee is brought to the 6-10% a boss
// swing is meant to be, their spells to their budgets, and what was hidden is drawn:
// - Grand Warlock Nethekurse: a Shadow Fissure's spot is drawn 2 seconds before it opens (it now hurts, at its heroic
//   value); his Shadow Slam is a tank buster, a cone at the tank he holds still for (others keep out of his front).
// - Blood Guard Porung's gauntlet: each Flame Arrow's landing spot is drawn 2 seconds before it lands (move).
// - Warbringer O'mrogg: the player Burning Maul fixes on is told, and marked with the splash around them (run to the
//   tank, the others keep clear); his Fear is not drawn any more (30 yards: the whole room, nothing to step out of).
// - Warchief Kargath Bladefist: Blade Dance is five hops, each landing spot drawn before he leaps there (step out of
//   each circle); it was nine invisible hops seeking the players.
namespace
{
// Deals an ability's damage: in a key, a share of the reference health; elsewhere, the heroic amount read as a spell
// value (normal mode takes 60% of it).
void Hit(Unit* caster, Unit* target, uint32 spellId, float percent, uint32 heroicAmount)
{
    if (!caster || !target || !target->IsAlive())
        return;

    if (caster->GetMap()->IsMythic())
        MythicTuning::DealReferenceDamage(caster, target, spellId, percent);
    else
        MythicTuning::DealAbilityDamage(caster, target, spellId,
            caster->GetMap()->IsHeroic() ? heroicAmount : heroicAmount * 6 / 10);
}

std::vector<Player*> PlayersNear(Unit* source, float range)
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
    for (Player* player : PlayersNear(source, 100.0f))
        if (area.Contains(player->GetPosition()))
            inside.push_back(player);
    return inside;
}

void WarnPlayer(Creature* me, Unit* target, std::string const& text)
{
    if (Player* player = target ? target->ToPlayer() : nullptr)
        me->Whisper(text, LANG_UNIVERSAL, player, true);
}

// The stock instance script (instance_shattered_halls) keeps the boss states and a few guids
enum ShatteredHallsData : uint32
{
    DATA_NETHEKURSE                 = 0,
    DATA_OMROGG                     = 1,
    DATA_KARGATH                    = 2,
    DATA_PORUNG                     = 3,

    DATA_EXECUTIONER                = 14,
    DATA_OMROGG_LEFT_HEAD           = 15,
    DATA_OMROGG_RIGHT_HEAD          = 16,
    DATA_WARCHIEF_PORTAL            = 17,
    DATA_LAST_FLAME_ARROW           = 18,
};

enum ShatteredHallsNpcs : uint32
{
    // Normal entry, then the heroic one (a spawn keeps its normal entry in heroic: both are registered)
    NPC_NETHEKURSE                  = 16807,
    NPC_NETHEKURSE_H                = 20568,
    NPC_PORUNG                      = 20923,
    NPC_PORUNG_H                    = 20993,
    NPC_KARGATH                     = 16808,
    NPC_KARGATH_H                   = 20597,

    NPC_SHADOWMOON_DARKCASTER       = 17694,
    NPC_SHADOWMOON_DARKCASTER_H     = 20577,
    NPC_SHATTERED_HAND_LEGIONNAIRE  = 16700,
    NPC_SHATTERED_HAND_LEGIONNAIRE_H = 20589,
};

// Spells tuned by id (as cast: the heroic id where the core maps one, the normal id where it does not)
enum ShatteredHallsTunedSpells : uint32
{
    SPELL_CONSUMPTION               = 30498,    // Lesser Shadow Fissure: no heroic mapping, normal values in heroic
    SPELL_DEATH_COIL_H              = 35954,
    SPELL_DEATH_COIL                = 30500,
    SPELL_THUNDERCLAP               = 30633,
    SPELL_BLAST_WAVE                = 30600,
    SPELL_BURNING_MAUL_PROC         = 30599,
    SPELL_BURNING_MAUL_PROC_H       = 36057,
    SPELL_FLAME_ARROW_EXPLOSION     = 30953,

    // Only named in the log by the trash abilities
    SPELL_SHADOW_NOVA               = 30852,
    SPELL_SHATTERING_STOMP          = 52237,
};

// ---------------------------------------------------------------------------------------------------------------------
// Grand Warlock Nethekurse
struct NethekursePeonRoleplay
{
    uint32 spellId;
    uint8 textId;
};

struct boss_grand_warlock_nethekurse_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_SKIP_INTRO          = 0,
        SAY_INTRO_2             = 1,
        SAY_PEON_ATTACKED       = 2,
        SAY_PEON_DIES           = 3,
        SAY_SHADOW_SEAR         = 4,
        SAY_SHADOW_FISSURE      = 5,
        SAY_DEATH_COIL          = 6,
        SAY_SLAY                = 7,
        SAY_DIE                 = 8,
    };

    enum Spells : uint32
    {
        SPELL_DARK_SPIN             = 30502,
        SPELL_SHADOW_SEAR           = 30735,
        SPELL_DEATH_COIL_RP         = 30741,
        SPELL_SHADOW_FISSURE_RP     = 30745,
        // Only named in the log: Shadow Slam is dealt by the script (the stock Shadow Slam is a weapon spell, whose
        // fixed amount the key would not scale)
        SPELL_SHADOW_CLEAVE         = 50581,
    };

    static constexpr uint32 NPC_PEON = 17083;
    static constexpr uint32 NPC_LESSER_SHADOW_FISSURE = 17471;
    static constexpr uint32 GO_GRAND_WARLOCK_CHAMBER_DOOR_1 = 182539;

    static constexpr uint32 SETDATA_DATA = 1;
    static constexpr uint32 SETDATA_PEON_AGGRO = 1;
    static constexpr uint32 SETDATA_PEON_DEATH = 2;
    static constexpr uint32 GROUP_RP = 0;
    static constexpr int32 ACTION_START_INTRO = 0;
    static constexpr int32 ACTION_CANCEL_INTRO = 1;

    static constexpr float FissureRadius = 3.0f;
    static constexpr uint32 FissureWarningMs = 2000;
    static constexpr uint32 FissureLifeMs = 26000;
    static constexpr float SlamRadius = 12.0f;
    static constexpr float SlamArc = 90.0f;
    static constexpr uint32 SlamWarningMs = 2500;

    boss_grand_warlock_nethekurse_evolutions(Creature* creature) : BossAI(creature, DATA_NETHEKURSE) { }

    void Reset() override
    {
        _timers.CancelAll();
        EndHold();

        ScheduleHealthCheckEvent(25, [&] {
            DoCastSelf(SPELL_DARK_SPIN);
        });

        instance->SetBossState(DATA_NETHEKURSE, NOT_STARTED);

        if (!_canAggro)
            me->SetImmuneToAll(true);
    }

    void JustReachedHome() override
    {
        me->GetMotionMaster()->Initialize();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _timers.CancelAll();
        EndHold();
        Talk(SAY_DIE);
        _JustDied();
    }

    void SetData(uint32 data, uint32 value) override
    {
        if (data != SETDATA_DATA || me->IsInCombat())
            return;

        if (value == SETDATA_PEON_AGGRO && _peonEngagedCount <= 4)
            Talk(SAY_PEON_ATTACKED);
        else if (value == SETDATA_PEON_DEATH && _peonKilledCount <= 4)
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->SetFacingTo(4.572762489318847656f);

            scheduler.Schedule(500ms, GROUP_RP, [this](TaskContext /*context*/)
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
                Talk(SAY_PEON_DIES);

                scheduler.Schedule(1s, GROUP_RP, [this](TaskContext /*context*/)
                {
                    me->GetMotionMaster()->Initialize();
                });

                if (++_peonKilledCount == 4)
                {
                    Talk(SAY_INTRO_2);
                    DoAction(ACTION_CANCEL_INTRO);
                    if (Unit* target = me->SelectNearestPlayer(80.0f))
                        AttackStart(target);
                }
            });
        }
    }

    void IntroRP()
    {
        scheduler.Schedule(500ms, GROUP_RP, [this](TaskContext context)
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->SetFacingTo(4.572762489318847656f);

            scheduler.Schedule(2500ms, GROUP_RP, [this](TaskContext /*context*/)
            {
                static std::array<NethekursePeonRoleplay, 3> const roleplay = { {
                    { SPELL_DEATH_COIL_RP,     SAY_DEATH_COIL     },
                    { SPELL_SHADOW_FISSURE_RP, SAY_SHADOW_FISSURE },
                    { SPELL_SHADOW_SEAR,       SAY_SHADOW_SEAR    },
                } };
                NethekursePeonRoleplay const& data = Acore::Containers::SelectRandomContainerElement(roleplay);
                DoCast(me, data.spellId);
                Talk(data.textId);
                me->GetMotionMaster()->Initialize();
            });

            context.Repeat(16400ms, 28500ms);
        });
    }

    void JustEngagedWith(Unit* who) override
    {
        if (who->GetEntry() == NPC_PEON)
            return;

        _JustEngagedWith();
        DoAction(ACTION_CANCEL_INTRO);

        scheduler.CancelAll();

        scheduler.Schedule(12150ms, 19850ms, [this](TaskContext context)
        {
            if (me->HealthBelowPct(90))
                DoCastRandomTarget(SPELL_DEATH_COIL, 0, 30.0f, true);
            context.Repeat();
        }).Schedule(8100ms, 17300ms, [this](TaskContext context)
        {
            ShadowFissure();
            context.Repeat(8450ms, 9450ms);
        }).Schedule(10950ms, 21850ms, [this](TaskContext context)
        {
            ShadowSlam();
            context.Repeat(14s, 20s);
        });

        if (_peonKilledCount < 4)
            Talk(SAY_SKIP_INTRO);
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_SLAY);
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_CANCEL_INTRO)
        {
            scheduler.CancelGroup(GROUP_RP);
            me->SetInCombatWithZone();
            return;
        }
        else if (action == ACTION_START_INTRO && !_introStarted)
        {
            // As the stock script: no pulling from behind the door, and the intro starts once (area trigger or door)
            me->SetImmuneToAll(false);
            _canAggro = true;
            _introStarted = true;

            std::list<Creature*> creatureList;
            GetCreatureListWithEntryInGrid(creatureList, me, NPC_PEON, 60.0f);
            for (Creature* creature : creatureList)
                if (creature)
                    creature->SetImmuneToAll(false);
            IntroRP();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _timers.Update(diff);
        scheduler.Update(diff);

        // The intro starts when the door nearest the entrance is opened, if the area trigger did not start it
        if (!_introStarted)
            if (GameObject* door = GetClosestGameObjectWithEntry(me, GO_GRAND_WARLOCK_CHAMBER_DOOR_1, 100.0f))
                if (door->GetGoState() == GO_STATE_ACTIVE)
                    DoAction(ACTION_START_INTRO);

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (!_holding && !me->HealthBelowPct(25))
            DoMeleeAttackIfReady();
    }

private:
    // Shadow Fissure: the spot under a random player is drawn 2 seconds before the fissure opens there (it opened
    // under them with 1 second before its first tick)
    void ShadowFissure()
    {
        Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 60.0f, true);
        if (!target)
            return;

        Position const spot = target->GetPosition();
        GroundIndicators::ShowCircle(me, spot, FissureRadius, FissureWarningMs, GroundIndicators::Theme::Shadow);
        _timers.Schedule(Milliseconds(FissureWarningMs), [this, spot](TaskContext)
        {
            if (me->IsAlive() && me->IsInCombat())
                me->SummonCreature(NPC_LESSER_SHADOW_FISSURE, spot, TEMPSUMMON_TIMED_DESPAWN, FissureLifeMs);
        });
    }

    // Shadow Slam: a tank buster, a cone at the tank he turns to and holds still for. The tank takes it (or steps
    // out of it); anyone else in his front takes it too.
    void ShadowSlam()
    {
        Unit* victim = me->GetVictim();
        if (!victim || me->HealthBelowPct(25))
            return;

        float const facing = me->GetAngle(victim);
        StartHold(facing);
        me->HandleEmoteCommand(EMOTE_ONESHOT_SPELL_CAST_OMNI);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing, SlamRadius,
            SlamArc, SlamWarningMs, GroundIndicators::Theme::Shadow);
        _timers.Schedule(Milliseconds(SlamWarningMs), [this, area, facing](TaskContext)
        {
            me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK2HTIGHT);
            GroundIndicators::Burst(me, Position(me->GetPositionX() + std::cos(facing) * 5.0f,
                me->GetPositionY() + std::sin(facing) * 5.0f, me->GetPositionZ()), GroundIndicators::Theme::Shadow);
            for (Player* player : PlayersIn(me, area))
                Hit(me, player, SPELL_SHADOW_CLEAVE, 50.0f, 4000);
            EndHold();
        });
    }

    void StartHold(float facing)
    {
        _holding = true;
        me->StopMoving();
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->SetTarget();
        me->SetFacingTo(facing);
    }

    void EndHold()
    {
        if (!_holding)
            return;
        _holding = false;
        me->SetControlled(false, UNIT_STATE_ROOT);
        if (Unit* victim = me->GetVictim())
            me->SetTarget(victim->GetGUID());
    }

    TaskScheduler _timers;
    uint8 _peonEngagedCount = 0;
    uint8 _peonKilledCount = 0;
    bool _canAggro = false;
    bool _introStarted = false;
    bool _holding = false;
};

// ---------------------------------------------------------------------------------------------------------------------
// Blood Guard Porung's gauntlet: the Shattered Hand Scout runs it (as the stock npc_shattered_hand_scout), but the
// archers' Flame Arrows are aimed at the start of their volley and each landing spot is drawn for 2 seconds. The
// stock arrows picked their spot when they landed, with nothing to see.
struct npc_shattered_hand_scout_evolutions : public ScriptedAI
{
    enum Texts : uint8
    {
        SAY_INVADERS_BREACHED   = 0,

        SAY_PORUNG_ARCHERS      = 0,
        SAY_PORUNG_READY        = 1,
        SAY_PORUNG_AIM          = 2,
        SAY_PORUNG_FIRE         = 3,
    };

    enum Spells : uint32
    {
        SPELL_CLEAR_ALL         = 28471,
        SPELL_SUMMON_ZEALOTS    = 30976,
    };

    static constexpr uint32 NPC_PORUNG_NORMAL = 20923;
    static constexpr uint32 NPC_BLOOD_GUARD = 17461;
    static constexpr uint32 NPC_SH_ZEALOT = 17462;
    static constexpr uint32 NPC_SH_ARCHER = 17427;
    static constexpr uint32 NPC_SH_FLAME_ARROW = 17687;
    static constexpr uint32 GO_BLAZE = 181915;

    static constexpr uint32 POINT_SCOUT_WP_END = 4;
    static constexpr uint32 SET_DATA_ARBITRARY_VALUE = 1;
    static constexpr uint32 SET_DATA_ENCOUNTER_DONE = 2;

    // The Explosion's own radius: what is drawn is what it hits
    static constexpr float ArrowRadius = 10.0f;
    static constexpr uint32 ArrowWarningMs = 2000;

    npc_shattered_hand_scout_evolutions(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    }

    void SetData(uint32 type, uint32 /*data*/) override
    {
        if (type != SET_DATA_ENCOUNTER_DONE)
            return;

        _scheduler.CancelAll();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!me->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) && me->IsWithinDist2d(who, 50.0f) &&
            who->GetPositionZ() > -3.0f && who->IsPlayer())
        {
            me->SetReactState(REACT_PASSIVE);
            DoCastSelf(SPELL_CLEAR_ALL);
            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
            Talk(SAY_INVADERS_BREACHED);
            me->GetMotionMaster()->MoveWaypoint(me->GetEntry() * 10, false);

            _firstZealots.clear();
            std::list<Creature*> creatureList;
            GetCreatureListWithEntryInGrid(creatureList, me, NPC_SH_ZEALOT, 15.0f);
            for (Creature* creature : creatureList)
                if (creature)
                    _firstZealots.insert(creature->GetGUID());
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*school*/) override
    {
        // Falls to 1 health but never dies
        if (damage >= me->GetHealth())
            damage = me->GetHealth() - 1;
    }

    void MovementInform(uint32 type, uint32 point) override
    {
        if (type != WAYPOINT_MOTION_TYPE || point != POINT_SCOUT_WP_END)
            return;

        me->SetVisible(false);

        if (Creature* porung = GetPorung())
        {
            porung->setActive(true);
            porung->AI()->DoCastAOE(SPELL_SUMMON_ZEALOTS);
            porung->AI()->Talk(SAY_PORUNG_ARCHERS);

            _scheduler.Schedule(45s, [this](TaskContext context)
            {
                if (Creature* porung = GetPorung())
                    porung->AI()->DoCastAOE(SPELL_SUMMON_ZEALOTS);

                context.Repeat();
            });
        }

        _scheduler.Schedule(1s, [this](TaskContext /*context*/)
        {
            _zealotGUIDs.clear();
            std::list<Creature*> creatureList;
            GetCreatureListWithEntryInGrid(creatureList, me, NPC_SH_ZEALOT, 100.0f);
            for (Creature* creature : creatureList)
            {
                if (creature)
                {
                    creature->AI()->SetData(SET_DATA_ARBITRARY_VALUE, SET_DATA_ARBITRARY_VALUE);
                    _zealotGUIDs.insert(creature->GetGUID());
                }
            }

            for (auto const& guid : _firstZealots)
                if (Creature* zealot = ObjectAccessor::GetCreature(*me, guid))
                    zealot->SetInCombatWithZone();

            if (Creature* porung = GetPorung())
            {
                porung->AI()->Talk(SAY_PORUNG_READY, 3600ms);
                porung->AI()->Talk(SAY_PORUNG_AIM, 4800ms);
            }

            _scheduler.Schedule(5800ms, [this](TaskContext /*context*/)
            {
                FireArrows();

                if (Creature* porung = GetPorung())
                    porung->AI()->Talk(SAY_PORUNG_FIRE, 200ms);

                _scheduler.Schedule(2s, 9750ms, [this](TaskContext context)
                {
                    if (FireArrows())
                        context.Repeat();

                    if (!me->SelectNearestPlayer(250.0f))
                    {
                        me->SetVisible(true);
                        me->DespawnOrUnsummon(5s, 5s);
                        ResetZealots(_zealotGUIDs);
                        ResetZealots(_firstZealots);
                        _scheduler.CancelAll();
                    }
                });
            });
        });
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    void ResetZealots(GuidSet const& guids)
    {
        for (auto const& guid : guids)
        {
            if (Creature* zealot = ObjectAccessor::GetCreature(*me, guid))
            {
                if (zealot->IsAlive())
                    zealot->DespawnOrUnsummon(5s, 5s);
                else
                    zealot->Respawn(true);
            }
        }
    }

    // Every living archer aims at a Flame Arrow spot near a player (not one already burning, not the last one shot
    // at, not one another archer of this volley took), and the spot is drawn until the arrow lands and explodes
    bool FireArrows()
    {
        std::list<Creature*> archers;
        GetCreatureListWithEntryInGrid(archers, me, NPC_SH_ARCHER, 100.0f);
        if (archers.empty())
            return false;

        std::list<Creature*> spots;
        GetCreatureListWithEntryInGrid(spots, me, NPC_SH_FLAME_ARROW, 200.0f);
        InstanceScript* instance = me->GetInstanceScript();
        ObjectGuid const last = instance ? instance->GetGuidData(DATA_LAST_FLAME_ARROW) : ObjectGuid::Empty;

        std::vector<Creature*> taken;
        for (Creature* archer : archers)
        {
            if (!archer || !archer->IsAlive())
                continue;

            std::vector<Creature*> candidates;
            for (Creature* spot : spots)
            {
                if (!spot || spot->GetGUID() == last || !spot->SelectNearestPlayer(15.0f) ||
                    spot->FindNearestGameObject(GO_BLAZE, 6.0f) ||
                    std::find(taken.begin(), taken.end(), spot) != taken.end())
                    continue;
                candidates.push_back(spot);
            }
            if (candidates.empty())
                continue;

            Creature* spot = Acore::Containers::SelectRandomContainerElement(candidates);
            taken.push_back(spot);
            archer->SetFacingToObject(spot);
            archer->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK_BOW);
            GroundIndicators::ShowCircle(archer, spot->GetPosition(), ArrowRadius, ArrowWarningMs,
                GroundIndicators::Theme::Fire);

            ObjectGuid const spotGuid = spot->GetGUID();
            _scheduler.Schedule(Milliseconds(ArrowWarningMs), [this, spotGuid](TaskContext)
            {
                if (Creature* arrow = ObjectAccessor::GetCreature(*me, spotGuid))
                    arrow->CastSpell(arrow, SPELL_FLAME_ARROW_EXPLOSION, true);
            });
        }

        if (!taken.empty() && instance)
            instance->SetGuidData(DATA_LAST_FLAME_ARROW, taken.back()->GetGUID());
        return true;
    }

    Creature* GetPorung()
    {
        return me->FindNearestCreature(IsHeroic() ? NPC_PORUNG_NORMAL : NPC_BLOOD_GUARD, 100.0f);
    }

    TaskScheduler _scheduler;
    GuidSet _zealotGUIDs;
    GuidSet _firstZealots;
};

// ---------------------------------------------------------------------------------------------------------------------
// Warbringer O'mrogg
struct boss_warbringer_omrogg_evolutions : public BossAI
{
    enum Spells : uint32
    {
        SPELL_FEAR                  = 30584,
        SPELL_BEATDOWN              = 30618,
        SPELL_BURNING_MAUL          = 30598,
    };

    enum HeadYells : uint8
    {
        SAY_ON_AGGRO                = 0,
        SAY_ON_AGGRO_2,
        SAY_ON_AGGRO_3,
        SAY_ON_BEATDOWN,
        SAY_ON_BEATDOWN_2,
        SAY_ON_BEATDOWN_3,
        SAY_ON_KILL,
        SAY_ON_KILL_2,
        SAY_ON_DEATH,
    };

    static constexpr uint8 EMOTE_BURNING_MAUL = 0;
    // spell_burning_maul (stock) tells him when the maul ends
    static constexpr uint32 DATA_BURNING_MAUL_END = 1;
    static constexpr uint32 GROUP_NON_BURNING_PHASE = 0;
    static constexpr uint32 GROUP_BURNING_PHASE = 1;

    // Burning Maul's splash (3 yards around whoever he hits), shown around the player he chases, for the rest of it
    static constexpr float MaulSplashRadius = 5.0f;
    static constexpr uint32 MaulMarkMs = 14000;

    boss_warbringer_omrogg_evolutions(Creature* creature) : BossAI(creature, DATA_OMROGG) { }

    void HandleHeadTalk(HeadYells yell)
    {
        switch (yell)
        {
            case SAY_ON_AGGRO:
            {
                uint8 group = urand(SAY_ON_AGGRO, SAY_ON_AGGRO_3);
                if (Creature* leftHead = instance->GetCreature(DATA_OMROGG_LEFT_HEAD))
                {
                    leftHead->AI()->Talk(group);
                    _headTalk.Schedule(3600ms, [this, group](TaskContext /*context*/)
                    {
                        if (Creature* rightHead = instance->GetCreature(DATA_OMROGG_RIGHT_HEAD))
                            rightHead->AI()->Talk(group);
                    });
                }
                break;
            }
            case SAY_ON_BEATDOWN:
            {
                if (Creature* leftHead = instance->GetCreature(DATA_OMROGG_LEFT_HEAD))
                {
                    leftHead->AI()->Talk(SAY_ON_BEATDOWN);
                    _headTalk.Schedule(3600ms, [this](TaskContext context)
                    {
                        if (Creature* rightHead = instance->GetCreature(DATA_OMROGG_RIGHT_HEAD))
                            rightHead->AI()->Talk(SAY_ON_BEATDOWN);
                        context.Schedule(3600ms, [this](TaskContext context)
                        {
                            uint8 group = urand(SAY_ON_BEATDOWN_2, SAY_ON_BEATDOWN_3);
                            if (Creature* leftHead = instance->GetCreature(DATA_OMROGG_LEFT_HEAD))
                                leftHead->AI()->Talk(group);
                            context.Schedule(3600ms, [this, group](TaskContext /*context*/)
                            {
                                if (Creature* rightHead = instance->GetCreature(DATA_OMROGG_RIGHT_HEAD))
                                    rightHead->AI()->Talk(group);
                            });
                        });
                    });
                }
                break;
            }
            case SAY_ON_KILL:
            {
                uint8 group = urand(SAY_ON_KILL, SAY_ON_KILL_2);
                if (Creature* leftHead = instance->GetCreature(DATA_OMROGG_LEFT_HEAD))
                    leftHead->AI()->Talk(group);
                _headTalk.Schedule(3600ms, [this, group](TaskContext /*context*/)
                {
                    if (Creature* rightHead = instance->GetCreature(DATA_OMROGG_RIGHT_HEAD))
                        rightHead->AI()->Talk(group);
                });
                break;
            }
            case SAY_ON_DEATH:
            {
                if (Creature* leftHead = instance->GetCreature(DATA_OMROGG_LEFT_HEAD))
                    leftHead->AI()->Talk(SAY_ON_DEATH);
                _headTalk.Schedule(3600ms, [this](TaskContext /*context*/)
                {
                    if (Creature* rightHead = instance->GetCreature(DATA_OMROGG_RIGHT_HEAD))
                        rightHead->AI()->Talk(SAY_ON_DEATH);
                });
                break;
            }
            default:
                break;
        }
    }

    void SetData(uint32 data, uint32 /*value*/) override
    {
        if (data != DATA_BURNING_MAUL_END)
            return;

        scheduler.CancelGroup(GROUP_BURNING_PHASE);
        ScheduleNonBurningPhase();
        ScheduleBurningPhase();
    }

    void ScheduleNonBurningPhase()
    {
        scheduler.Schedule(12100ms, 17300ms, GROUP_NON_BURNING_PHASE, [this](TaskContext context)
        {
            DoCastAOE(SPELL_THUNDERCLAP);
            context.Repeat(17200ms, 24200ms);
        }).Schedule(20s, 30s, GROUP_NON_BURNING_PHASE, [this](TaskContext context)
        {
            DoCastSelf(SPELL_BEATDOWN);
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            context.Schedule(200ms, GROUP_NON_BURNING_PHASE, [this](TaskContext context)
            {
                DoResetThreatList();
                if (Unit* newTarget = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, false, false))
                {
                    me->AddThreat(newTarget, 2250.f);
                    WarnPlayer(me, newTarget, "O'mrogg turns on you: run to your tank!");
                }
                HandleHeadTalk(SAY_ON_BEATDOWN);
                context.Schedule(1200ms, GROUP_NON_BURNING_PHASE, [this](TaskContext /*context*/)
                {
                    me->SetReactState(REACT_AGGRESSIVE);
                });
            });
            context.Repeat();
        });
    }

    void ScheduleBurningPhase()
    {
        scheduler.Schedule(45s, 60s, GROUP_BURNING_PHASE, [this](TaskContext context)
        {
            me->AttackStop();
            me->SetReactState(REACT_PASSIVE);
            context.CancelGroup(GROUP_NON_BURNING_PHASE);
            context.Schedule(1200ms, [this](TaskContext context)
            {
                // Triggered: no 30-yard circle drawn over the whole room for a fear nobody can step out of
                DoCastAOE(SPELL_FEAR, true);
                DoCast(SPELL_BURNING_MAUL);
                context.Schedule(200ms, [this](TaskContext context)
                {
                    Talk(EMOTE_BURNING_MAUL);
                    context.Schedule(2200ms, [this](TaskContext context)
                    {
                        DoResetThreatList();
                        if (Unit* newTarget = SelectTarget(SelectTargetMethod::Random, 0, 0.0f, false, false))
                        {
                            me->AddThreat(newTarget, 2250.f);
                            MarkMaulTarget(newTarget);
                        }
                        me->SetReactState(REACT_AGGRESSIVE);
                        context.Schedule(4850ms, 8500ms, GROUP_BURNING_PHASE, [this](TaskContext context)
                        {
                            DoCastAOE(SPELL_BLAST_WAVE);
                            context.Repeat();
                        });
                    });
                });
            });
        });
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        _headTalk.CancelAll();
        HandleHeadTalk(SAY_ON_AGGRO);

        ScheduleNonBurningPhase();
        ScheduleBurningPhase();
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && victim->IsPlayer())
            HandleHeadTalk(SAY_ON_KILL);
    }

    void JustDied(Unit* killer) override
    {
        HandleHeadTalk(SAY_ON_DEATH);
        BossAI::JustDied(killer);
    }

    void UpdateAI(uint32 diff) override
    {
        _headTalk.Update(diff);

        if (!UpdateVictim())
            return;

        scheduler.Update(diff, [this]
        {
            DoMeleeAttackIfReady();
        });
    }

private:
    // The player the Burning Maul goes after: told to run to the tank, and the maul's splash drawn around them so the
    // others keep clear (he outruns anyone: only a taunt takes him off)
    void MarkMaulTarget(Unit* target)
    {
        if (!target->IsPlayer())
            return;

        me->TextEmote(std::string("%s swings his burning maul at ") + target->GetName() + "!", nullptr, true);
        WarnPlayer(me, target, "O'mrogg's burning maul is after you: run to your tank, away from the others!");
        GroundIndicators::ShowCarriedCircle(me, target, MaulSplashRadius, MaulMarkMs);
    }

    TaskScheduler _headTalk;
};

// ---------------------------------------------------------------------------------------------------------------------
// Warchief Kargath Bladefist
struct boss_warchief_kargath_bladefist_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                   = 0,
        SAY_SLAY                    = 1,
        SAY_DEATH                   = 2,
        SAY_EVADE                   = 5,
    };

    enum Spells : uint32
    {
        SPELL_BLADE_DANCE_CHARGE    = 30751,
        // Only named in the log: the hops' damage is dealt by the script (the stock Blade Dance is a weapon spell,
        // whose fixed amount the key would not scale)
        SPELL_FAN_OF_BLADES         = 39954,
    };

    static constexpr uint32 NPC_SHATTERED_ASSASSIN = 17695;
    static constexpr uint32 NPC_BLADE_DANCE_TARGET = 20709;
    // npc_warchief_portal (stock)
    static constexpr uint32 DATA_START_FIGHT = 1;
    static constexpr uint32 DATA_RESET_FIGHT = 2;

    static constexpr uint8 DanceHops = 5;
    static constexpr float HopRadius = 8.0f;
    static constexpr uint32 HopWarningMs = 1800;

    boss_warchief_kargath_bladefist_evolutions(Creature* creature) : BossAI(creature, DATA_KARGATH) { }

    void InitializeAI() override
    {
        BossAI::InitializeAI();
        if (instance)
            if (Creature* executioner = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_EXECUTIONER)))
                executioner->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (!summon)
            return;

        summon->SetVisible(false);
        ObjectGuid const guid = summon->GetGUID();
        scheduler.Schedule(20s, [this, guid](TaskContext /*context*/)
        {
            if (Creature* dead = ObjectAccessor::GetCreature(*me, guid))
            {
                dead->Respawn(true);
                dead->SetVisible(true);
            }
        });
    }

    void Reset() override
    {
        StopDance();
        me->SetReactState(REACT_AGGRESSIVE);
        BossAI::Reset();
        if (Creature* warchiefPortal = instance->GetCreature(DATA_WARCHIEF_PORTAL))
            warchiefPortal->AI()->SetData(DATA_RESET_FIGHT, 0);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        StopDance();
        BossAI::EnterEvadeMode(why);
    }

    void JustDied(Unit* killer) override
    {
        StopDance();
        Talk(SAY_DEATH);
        BossAI::JustDied(killer);
        if (Creature* warchiefPortal = instance->GetCreature(DATA_WARCHIEF_PORTAL))
            warchiefPortal->AI()->SetData(DATA_RESET_FIGHT, 0);
        if (Creature* executioner = ObjectAccessor::GetCreature(*me, instance->GetGuidData(DATA_EXECUTIONER)))
            executioner->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);
        BossAI::JustEngagedWith(who);
        if (Creature* warchiefPortal = instance->GetCreature(DATA_WARCHIEF_PORTAL))
            warchiefPortal->AI()->SetData(DATA_START_FIGHT, 0);
        RespawnAssassins();
        scheduler.Schedule(30s, [this](TaskContext context)
        {
            StartDance();
            context.Repeat(32850ms, 41350ms);
        });
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void MovementInform(uint32 type, uint32 /*id*/) override
    {
        if (type == POINT_MOTION_TYPE && _dancing)
            Land(_hops);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!IsInRoom())
        {
            Talk(SAY_EVADE);
            EnterEvadeMode();
            return;
        }

        _timers.Update(diff);

        if (!UpdateVictim())
            return;

        scheduler.Update(diff, [this]
        {
            if (!_dancing)
                DoMeleeAttackIfReady();
        });
    }

private:
    void RespawnAssassins()
    {
        static std::array<Position, 4> const assassinsPos = { {
            { 172.68164f, -80.65692f, 2.0834563f, 5.4279f },
            { 167.8295f,  -86.55783f, 1.9949634f, 0.8118f },
            { 287.0375f,  -88.17879f, 2.0663502f, 3.2490f },
            { 292.1491f,  -82.25267f, 1.9973913f, 5.8568f },
        } };
        for (Position const& summonPos : assassinsPos)
            me->SummonCreature(NPC_SHATTERED_ASSASSIN, summonPos);
    }

    bool IsInRoom() const
    {
        static Position const kargathRespawnPos = { 231.25f, -83.6449f, 5.02341f };
        return me->GetExactDist2d(kargathRespawnPos) < 42.f;
    }

    // Blade Dance: five hops. Each landing spot is drawn before he leaps there, and whoever is still in it when he
    // lands takes the blades. He does nothing else meanwhile.
    void StartDance()
    {
        _dancing = true;
        _hops = 0;
        me->SetReactState(REACT_PASSIVE);
        me->AttackStop();
        me->StopMoving();
        me->GetMotionMaster()->Clear();
        me->GetMotionMaster()->MoveIdle();
        NextHop();
    }

    void NextHop()
    {
        if (!_dancing)
            return;

        if (_hops >= DanceHops)
        {
            EndDance();
            return;
        }

        ++_hops;
        _landed = false;
        Creature* spot = PickSpot();
        Position const dest = spot ? spot->GetPosition() : me->GetPosition();
        _hopArea = GroundIndicators::ShowCircle(me, dest, HopRadius, HopWarningMs + 500);
        me->SetFacingTo(me->GetAngle(dest.GetPositionX(), dest.GetPositionY()));

        ObjectGuid const spotGuid = spot ? spot->GetGUID() : ObjectGuid::Empty;
        uint8 const hop = _hops;
        _timers.Schedule(Milliseconds(HopWarningMs), [this, spotGuid, hop](TaskContext context)
        {
            Creature* target = spotGuid.IsEmpty() ? nullptr : ObjectAccessor::GetCreature(*me, spotGuid);
            if (!target || me->GetExactDist2d(target) < 2.0f)
            {
                Land(hop);
                return;
            }

            me->CastSpell(target, SPELL_BLADE_DANCE_CHARGE, true);
            // In case the leap never reports its landing
            context.Schedule(1500ms, [this, hop](TaskContext)
            {
                Land(hop);
            });
        });
    }

    void Land(uint8 hop)
    {
        if (!_dancing || hop != _hops || _landed)
            return;

        _landed = true;
        me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK2HTIGHT);
        for (Player* player : PlayersIn(me, _hopArea))
            Hit(me, player, SPELL_FAN_OF_BLADES, 45.0f, 6000);
        _timers.Schedule(400ms, [this](TaskContext)
        {
            NextHop();
        });
    }

    void EndDance()
    {
        _dancing = false;
        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0))
            AttackStart(target);
    }

    void StopDance()
    {
        _timers.CancelAll();
        _dancing = false;
        _hops = 0;
    }

    // A Blade Dance Target 5 to 16 yards away; two times in three one with a player near it, as the stock dance
    Creature* PickSpot()
    {
        std::list<Creature*> spots;
        me->GetCreatureListWithEntryInGrid(spots, NPC_BLADE_DANCE_TARGET, 30.0f);
        std::vector<Creature*> all;
        std::vector<Creature*> nearPlayers;
        for (Creature* spot : spots)
        {
            float const distance = me->GetDistance2d(spot);
            if (distance < 5.0f || distance > 16.0f)
                continue;
            all.push_back(spot);
            if (spot->SelectNearestPlayer(15.0f))
                nearPlayers.push_back(spot);
        }

        if (!nearPlayers.empty() && urand(0, 2))
            return Acore::Containers::SelectRandomContainerElement(nearPlayers);
        if (!all.empty())
            return Acore::Containers::SelectRandomContainerElement(all);
        return nullptr;
    }

    TaskScheduler _timers;
    GroundIndicators::Area _hopArea;
    bool _dancing = false;
    bool _landed = false;
    uint8 _hops = 0;
};

void RegisterTrash(std::initializer_list<uint32> entries, MythicTrash::Ability ability)
{
    for (uint32 entry : entries)
    {
        ability.entry = entry;
        MythicTrash::Register(ability);
    }
}

void SetMelee(std::initializer_list<uint32> entries, float multiplier)
{
    for (uint32 entry : entries)
        MythicTuning::SetMeleeMultiplier(entry, multiplier);
}
}

void AddMythicShatteredHallsScripts()
{
    // Melee, towards the 6-10% of a tank a boss swing is meant to be (bosses already get x1.6 from the engine). At
    // +10: Nethekurse 2.2% -> 5.7% (a caster, below the warriors), Porung 2.9% -> 8.0%, O'mrogg 5.0% -> 8.0% (the
    // engine's x1.6 alone), Kargath 6.5% -> 8.3% (x1.6 alone would put him at 10.4%).
    SetMelee({ NPC_NETHEKURSE, NPC_NETHEKURSE_H }, 1.6f);
    SetMelee({ NPC_PORUNG, NPC_PORUNG_H }, 1.75f);
    SetMelee({ NPC_KARGATH, NPC_KARGATH_H }, 0.8f);

    // Lesser Shadow Fissure's Consumption runs on normal-mode values in heroic (the heroic 35951 is 3.5 times it):
    // 3.4% -> 11.8% a tick, a real floor to keep off (and now drawn 2 seconds before it opens)
    MythicTuning::SetSpellMultiplier(SPELL_CONSUMPTION, 3.5f);
    // Death Coil: 7.8% -> 10%
    MythicTuning::SetSpellMultiplier(SPELL_DEATH_COIL, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_DEATH_COIL_H, 1.3f);
    // O'mrogg's Thunderclap: 4.2% -> 10.5% (a pulse on the melee); Blast Wave: 11.5% -> 25% for a drawn, avoidable
    // 10 yards; the Burning Maul's splash: a fixed-on clothie took 31% a swing, now 27% (three swings to taunt back)
    MythicTuning::SetSpellMultiplier(SPELL_THUNDERCLAP, 2.5f);
    MythicTuning::SetSpellMultiplier(SPELL_BLAST_WAVE, 2.2f);
    MythicTuning::SetSpellMultiplier(SPELL_BURNING_MAUL_PROC, 0.8f);
    MythicTuning::SetSpellMultiplier(SPELL_BURNING_MAUL_PROC_H, 0.8f);
    // Porung's gauntlet: a Flame Arrow's Explosion, 2.5% -> 20% now that its spot is drawn before it lands
    MythicTuning::SetSpellMultiplier(SPELL_FLAME_ARROW_EXPLOSION, 8.0f);

    // Trash: the packs already press the tank hard, so only two of them get an ability, both aimed away from the tank

    // Shadowmoon Darkcaster: a Shadow Nova under a player (move)
    MythicTrash::Ability nova;
    nova.shape = MythicTrash::Shape::UnderTarget;
    nova.size = 6.0f;
    nova.warnMs = 2500;
    nova.cooldownMs = 20000;
    nova.firstMs = 8000;
    nova.percent = 25.0f;
    nova.spellId = SPELL_SHADOW_NOVA;
    nova.theme = GroundIndicators::Theme::Shadow;
    nova.holdStill = true;
    RegisterTrash({ NPC_SHADOWMOON_DARKCASTER, NPC_SHADOWMOON_DARKCASTER_H }, nova);

    // Shattered Hand Legionnaire: a Shattering Stomp around it (melee step out; the tank takes it through armour)
    MythicTrash::Ability stomp;
    stomp.shape = MythicTrash::Shape::AroundSelf;
    stomp.size = 8.0f;
    stomp.warnMs = 2500;
    stomp.cooldownMs = 22000;
    stomp.firstMs = 10000;
    stomp.percent = 25.0f;
    stomp.spellId = SPELL_SHATTERING_STOMP;
    stomp.theme = GroundIndicators::Theme::None;
    stomp.holdStill = true;
    RegisterTrash({ NPC_SHATTERED_HAND_LEGIONNAIRE, NPC_SHATTERED_HAND_LEGIONNAIRE_H }, stomp);

    RegisterCreatureAI(boss_grand_warlock_nethekurse_evolutions);
    RegisterCreatureAI(npc_shattered_hand_scout_evolutions);
    RegisterCreatureAI(boss_warbringer_omrogg_evolutions);
    RegisterCreatureAI(boss_warchief_kargath_bladefist_evolutions);
}
