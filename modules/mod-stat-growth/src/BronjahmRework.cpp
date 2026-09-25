#include "GroundIndicators.h"

#include "CreatureScript.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

// Bronjahm (the Forge of Souls), reworked from nothing of his own. The stock fight hangs on soul fragments that have
// to be killed before they reach him and a Soulstorm to stand inside: bots do neither, and in a mythic key the
// fragments outlived their timer and stacked. Here every ability is drawn in red before it lands and resolved against
// the area drawn, so bots step out of it like players (mod-playerbots' "avoid indicators").
//
// Phase 1, the Forge (100-60%): Shadow Cleave, a cone at the tank he holds still for (step aside); Soul Blast, a
// circle under every player (move); Soul Sever, a circle following one player that bursts on whoever is still next to
// them (take it away); Soul Siphon, a small pulse on the whole group.
// Intermission, the Harvest (60%): he goes back to the middle of the room and cannot be hurt, and four waves of beams
// sweep out of him, each turned from the one before (stand between the lines, then move).
// Phase 2, the Storm (60-0%): Shadow Nova, two opposite quarters of the room erupting, then the other two (cross over
// as soon as the first two have burst); the Forge's abilities come faster, and Soul Sever takes two players below 25%.
namespace
{
// The stock instance script (instance_forge_of_souls) keeps the boss state: boss 0
constexpr uint32 DATA_BRONJAHM = 0;
// An invisible trigger, to play a visual where something lands on the ground
constexpr uint32 NPC_WORLD_TRIGGER = 22515;

// Only named in the combat log: the damage is dealt by the script, on the areas it drew
enum Spells
{
    SPELL_SHADOW_CLEAVE     = 29832,
    SPELL_SOUL_BLAST        = 50992,
    SPELL_SOUL_SEVER        = 45917,
    SPELL_SOUL_FLAY         = 45442,
    SPELL_SHADOW_NOVA       = 30852,
    SPELL_SOUL_SIPHON       = 7290,
};

// Stock spell visual kits of those spells, played where each ability lands
enum Kits
{
    KIT_CLEAVE_CAST         = 6784,
    KIT_CLEAVE_HIT          = 6642,
    KIT_BLAST_PRECAST       = 6818,
    KIT_SHADOW_BURST        = 7775,     // Soul Blast's impact
    KIT_SEVER_BURST         = 6706,
    KIT_FLAY_HIT            = 8540,
    KIT_NOVA_PRECAST        = 6777,
    KIT_NOVA_CAST           = 6803,
    KIT_SIPHON_CAST         = 817,
    KIT_SIPHON_HIT          = 330,
};

enum Events
{
    EVENT_CLEAVE = 1,
    EVENT_SOUL_BLAST,
    EVENT_SOUL_SEVER,
    EVENT_SOUL_SIPHON,
    EVENT_SHADOW_NOVA,
};

enum Phases
{
    PHASE_NONE,
    PHASE_FORGE,
    PHASE_HARVEST,
    PHASE_STORM
};

enum Yells
{
    SAY_AGGRO               = 0,
    SAY_SLAY                = 1,
    SAY_DEATH               = 2,
    SAY_HARVEST             = 3,    // "The vortex of the harvested calls to you!"
    SAY_SEVER               = 4,    // "I will sever your soul from your body!"
};

// Sizes and warnings. Every area is drawn for its warning time, then resolved.
constexpr float CleaveRadius = 14.0f;
constexpr float CleaveArc = 90.0f;
constexpr uint32 CleaveWarningMs = 2500;
constexpr float BlastRadius = 6.0f;
constexpr uint32 BlastWarningMs = 3000;
constexpr float SeverRadius = 8.0f;
constexpr uint32 SeverMs = 6000;
constexpr float BeamLength = 45.0f;
constexpr float BeamWidth = 6.0f;
constexpr uint32 BeamWarningMs = 3000;
constexpr uint32 BeamWaves = 4;
constexpr float NovaRadius = 45.0f;
constexpr float NovaArc = 90.0f;
constexpr uint32 NovaWarningMs = 3000;

// Damage, before the mythic scaling of a key (MythicDungeonSystem.cpp scales it like any creature spell): heroic, and
// normal at NormalPercent of it. Heroic players of the Forge have 20-25k health.
constexpr uint32 NormalPercent = 60;
constexpr uint32 CleaveDamage = 9000;
constexpr uint32 BlastDamage = 7000;
constexpr uint32 SeverDamage = 9000;            // to everyone else in the circle
constexpr uint32 SeverCarrierDamage = 3000;     // to the one carrying it
constexpr uint32 FlayDamage = 10000;
constexpr uint32 NovaDamage = 11000;
constexpr uint32 SiphonDamage = 2000;

struct boss_bronjahm_evolutions : public BossAI
{
    boss_bronjahm_evolutions(Creature* creature) : BossAI(creature, DATA_BRONJAHM) { }

    void Reset() override
    {
        _phase = PHASE_NONE;
        scheduler.CancelAll();
        EndWindup();
        me->SetReactState(REACT_AGGRESSIVE);
        BossAI::Reset();
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelAll();
        EndWindup();
        _phase = PHASE_NONE;
        me->SetReactState(REACT_AGGRESSIVE);
        BossAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);
        BossAI::JustEngagedWith(who);
        SetPhase(PHASE_FORGE);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        // The Harvest: nothing touches him while the beams run
        if (_phase == PHASE_HARVEST)
        {
            damage = 0;
            return;
        }

        if (_phase == PHASE_FORGE && me->HealthBelowPctDamaged(60, damage))
            StartHarvest();
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        if (!UpdateVictim())
            return;

        events.Update(diff);
        if (_phase == PHASE_HARVEST || me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            // One held ability at a time: while he stands still for one, the next waits
            if (_windup && (eventId == EVENT_CLEAVE || eventId == EVENT_SHADOW_NOVA))
            {
                events.ScheduleEvent(eventId, 1s);
                continue;
            }
            ExecuteEvent(eventId);
        }

        if (!_windup)
            DoMeleeAttackIfReady();
    }

private:
    void SetPhase(Phases phase)
    {
        _phase = phase;
        events.Reset();
        switch (phase)
        {
            case PHASE_FORGE:
                events.ScheduleEvent(EVENT_CLEAVE, 8s);
                events.ScheduleEvent(EVENT_SOUL_BLAST, 12s);
                events.ScheduleEvent(EVENT_SOUL_SEVER, 20s);
                events.ScheduleEvent(EVENT_SOUL_SIPHON, 15s);
                break;
            case PHASE_STORM:
                events.ScheduleEvent(EVENT_SHADOW_NOVA, 6s);
                events.ScheduleEvent(EVENT_CLEAVE, 12s);
                events.ScheduleEvent(EVENT_SOUL_BLAST, 16s);
                events.ScheduleEvent(EVENT_SOUL_SEVER, 22s);
                events.ScheduleEvent(EVENT_SOUL_SIPHON, 10s);
                break;
            default:
                break;
        }
    }

    void ExecuteEvent(uint32 eventId)
    {
        bool const storm = _phase == PHASE_STORM;
        switch (eventId)
        {
            case EVENT_CLEAVE:
                ShadowCleave();
                events.ScheduleEvent(EVENT_CLEAVE, storm ? 10s : 13s);
                break;
            case EVENT_SOUL_BLAST:
                SoulBlast();
                events.ScheduleEvent(EVENT_SOUL_BLAST, storm ? 13s : 16s);
                break;
            case EVENT_SOUL_SEVER:
                SoulSever();
                events.ScheduleEvent(EVENT_SOUL_SEVER, storm ? 18s : 22s);
                break;
            case EVENT_SOUL_SIPHON:
                SoulSiphon();
                events.ScheduleEvent(EVENT_SOUL_SIPHON, storm ? 14s : 20s);
                break;
            case EVENT_SHADOW_NOVA:
                ShadowNova();
                events.ScheduleEvent(EVENT_SHADOW_NOVA, 24s);
                break;
            default:
                break;
        }
    }

    // An ability that comes out of him where the red is drawn: from its warning to the moment it lands he stays
    // where he is, facing it, rooted and no longer turning to his target
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

    uint32 Amount(uint32 heroic) const
    {
        return me->GetMap()->IsHeroic() ? heroic : heroic * NormalPercent / 100;
    }

    // Deals damage as the named spell: resistances, absorbs and the key's scaling apply, and it reads as that spell
    void Burn(Unit* target, uint32 spellId, uint32 amount)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !target->IsAlive())
            return;

        SpellNonMeleeDamage damageInfo(me, target, spellInfo, spellInfo->GetSchoolMask());
        me->CalculateSpellDamageTaken(&damageInfo, int32(amount), spellInfo);
        Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
        me->SendSpellNonMeleeDamageLog(&damageInfo);
        me->DealSpellDamage(&damageInfo, true);
    }

    std::vector<Player*> Players()
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && me->IsWithinDistInMap(player, 100.0f))
                    players.push_back(player);
        return players;
    }

    std::vector<Player*> PlayersIn(GroundIndicators::Area const& area)
    {
        std::vector<Player*> inside;
        for (Player* player : Players())
            if (area.Contains(*player))
                inside.push_back(player);
        return inside;
    }

    void PlayOnGround(Position const& where, uint32 kit)
    {
        if (Creature* trigger = me->SummonCreature(NPC_WORLD_TRIGGER, where, TEMPSUMMON_TIMED_DESPAWN, 3000))
            trigger->SendPlaySpellVisual(kit);
    }

    Position Ground(float x, float y) const
    {
        float const top = me->GetPositionZ() + 10.0f;
        float z = me->GetMap()->GetHeight(me->GetPhaseMask(), x, y, top, true, 50.0f);
        if (z <= INVALID_HEIGHT)
            z = me->GetPositionZ();
        return Position(x, y, z);
    }

    // Shadow Cleave: a cone at the tank; he holds still for it, so a step aside is a step out
    void ShadowCleave()
    {
        Unit* victim = me->GetVictim();
        float const facing = victim ? me->GetAngle(victim) : me->GetOrientation();
        BeginWindup(facing);
        me->SendPlaySpellVisual(KIT_CLEAVE_CAST);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, *me, facing, CleaveRadius, CleaveArc,
            CleaveWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(CleaveWarningMs), [this, area, facing](TaskContext)
        {
            me->SetFacingTo(facing);
            me->SendPlaySpellVisual(KIT_CLEAVE_CAST);
            for (Player* player : PlayersIn(area))
            {
                player->SendPlaySpellVisual(KIT_CLEAVE_HIT);
                Burn(player, SPELL_SHADOW_CLEAVE, Amount(CleaveDamage));
            }
            EndWindup();
        });
    }

    // Soul Blast: a circle under every player, where they stand now
    void SoulBlast()
    {
        me->SendPlaySpellVisual(KIT_BLAST_PRECAST);
        for (Player* player : Players())
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, *player, BlastRadius,
                BlastWarningMs, GroundIndicators::Theme::Shadow);
            scheduler.Schedule(Milliseconds(BlastWarningMs), [this, area](TaskContext)
            {
                PlayOnGround(area.origin, KIT_SHADOW_BURST);
                for (Player* hit : PlayersIn(area))
                    Burn(hit, SPELL_SOUL_BLAST, Amount(BlastDamage));
            });
        }
    }

    // Soul Sever: a circle following a player that is not the tank; it bursts on whoever is still next to them
    void SoulSever()
    {
        Talk(SAY_SEVER);
        Unit* tank = me->GetVictim();
        uint32 const count = (_phase == PHASE_STORM && me->HealthBelowPct(25)) ? 2 : 1;
        std::list<Unit*> carriers;
        SelectTargetList(carriers, count, SelectTargetMethod::Random, 0,
            [tank](Unit* unit) { return unit->IsPlayer() && unit != tank; });

        for (Unit* carrier : carriers)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, SeverRadius, SeverMs);
            ObjectGuid const carrierGuid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(SeverMs), [this, area, carrierGuid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
                if (!carrier || !carrier->IsAlive())
                    return;

                carrier->SendPlaySpellVisual(KIT_SEVER_BURST);
                Burn(carrier, SPELL_SOUL_SEVER, Amount(SeverCarrierDamage));
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                        Burn(player, SPELL_SOUL_SEVER, Amount(SeverDamage));
            });
        }
    }

    // Soul Siphon: the whole group, a little, often
    void SoulSiphon()
    {
        me->SendPlaySpellVisual(KIT_SIPHON_CAST);
        for (Player* player : Players())
        {
            player->SendPlaySpellVisual(KIT_SIPHON_HIT);
            Burn(player, SPELL_SOUL_SIPHON, Amount(SiphonDamage));
        }
    }

    // The Harvest: back to the middle of the room, untouchable, and the beams
    void StartHarvest()
    {
        _phase = PHASE_HARVEST;
        events.Reset();
        scheduler.CancelAll();
        EndWindup();
        Talk(SAY_HARVEST);

        Position const& home = me->GetHomePosition();
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);
        me->GetMotionMaster()->Clear();
        me->NearTeleportTo(home.GetPositionX(), home.GetPositionY(), home.GetPositionZ(), home.GetOrientation());
        me->SendPlaySpellVisual(KIT_NOVA_PRECAST);

        float const base = rand_norm() * float(M_PI) / 2.0f;
        scheduler.Schedule(2s, [this, base](TaskContext)
        {
            BeamWave(base, 0);
        });
    }

    // Four beams in a cross from him; each wave turned a quarter of the gap from the last
    void BeamWave(float base, uint32 wave)
    {
        me->SetControlled(true, UNIT_STATE_ROOT);
        me->SetFacingTo(base);
        me->SendPlaySpellVisual(KIT_NOVA_PRECAST);
        Position const origin = me->GetPosition();
        std::vector<GroundIndicators::Area> beams;
        for (uint32 arm = 0; arm < 4; ++arm)
        {
            float const facing = Position::NormalizeOrientation(base + arm * float(M_PI) / 2.0f);
            beams.push_back(GroundIndicators::ShowRectangle(me, origin, facing, BeamLength, BeamWidth, BeamWarningMs,
                GroundIndicators::Theme::Shadow));
        }

        scheduler.Schedule(Milliseconds(BeamWarningMs), [this, origin, base, wave, beams](TaskContext)
        {
            me->SendPlaySpellVisual(KIT_NOVA_CAST);
            for (GroundIndicators::Area const& beam : beams)
            {
                float const facing = beam.origin.GetOrientation();
                for (float along = 8.0f; along <= BeamLength; along += 9.0f)
                    PlayOnGround(Ground(origin.GetPositionX() + std::cos(facing) * along,
                        origin.GetPositionY() + std::sin(facing) * along), KIT_FLAY_HIT);
            }

            std::vector<Player*> burned;
            for (GroundIndicators::Area const& beam : beams)
                for (Player* player : PlayersIn(beam))
                    if (std::find(burned.begin(), burned.end(), player) == burned.end())
                        burned.push_back(player);
            for (Player* player : burned)
                Burn(player, SPELL_SOUL_FLAY, Amount(FlayDamage));

            if (wave + 1 < BeamWaves)
                BeamWave(base + float(M_PI) / 8.0f, wave + 1);
            else
                scheduler.Schedule(1s, [this](TaskContext) { EndHarvest(); });
        });
    }

    void EndHarvest()
    {
        me->SetControlled(false, UNIT_STATE_ROOT);
        me->SetReactState(REACT_AGGRESSIVE);
        if (Unit* target = SelectTarget(SelectTargetMethod::MaxThreat, 0))
            AttackStart(target);
        SetPhase(PHASE_STORM);
    }

    // Shadow Nova: two opposite quarters of the room, then the other two
    void ShadowNova()
    {
        float const base = rand_norm() * 2.0f * float(M_PI);
        BeginWindup(me->GetOrientation());
        me->SendPlaySpellVisual(KIT_NOVA_PRECAST);
        NovaPair(base, true);
    }

    void NovaPair(float base, bool first)
    {
        std::vector<GroundIndicators::Area> quarters;
        for (float turn : { 0.0f, float(M_PI) })
            quarters.push_back(GroundIndicators::ShowCone(me, *me, Position::NormalizeOrientation(base + turn),
                NovaRadius, NovaArc, NovaWarningMs, GroundIndicators::Theme::Shadow));

        scheduler.Schedule(Milliseconds(NovaWarningMs), [this, base, first, quarters](TaskContext)
        {
            me->SendPlaySpellVisual(KIT_NOVA_CAST);
            for (GroundIndicators::Area const& quarter : quarters)
                for (float along = 10.0f; along <= NovaRadius; along += 12.0f)
                    for (float spread : { -0.5f, 0.0f, 0.5f })
                    {
                        float const facing = quarter.origin.GetOrientation() + spread;
                        PlayOnGround(Ground(me->GetPositionX() + std::cos(facing) * along,
                            me->GetPositionY() + std::sin(facing) * along), KIT_SHADOW_BURST);
                    }

            std::vector<Player*> burned;
            for (GroundIndicators::Area const& quarter : quarters)
                for (Player* player : PlayersIn(quarter))
                    if (std::find(burned.begin(), burned.end(), player) == burned.end())
                        burned.push_back(player);
            for (Player* player : burned)
                Burn(player, SPELL_SHADOW_NOVA, Amount(NovaDamage));

            if (first)
                NovaPair(base + float(M_PI) / 2.0f, false);
            else
                EndWindup();
        });
    }

    Phases _phase = PHASE_NONE;
    bool _windup = false;
};
}

void AddBronjahmReworkScripts()
{
    RegisterCreatureAI(boss_bronjahm_evolutions);
}
