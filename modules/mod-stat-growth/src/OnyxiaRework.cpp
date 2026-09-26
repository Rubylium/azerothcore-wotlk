#include "GroundIndicators.h"
#include "MythicTuning.h"

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

// Onyxia, reworked around the red ground indicators (GroundIndicators.cpp). The stock fight spends its middle third
// with her flying out of reach, breathing fire bots never learned to read. Here she never leaves the ground: every
// ability is drawn in red before it lands, and resolved against the very area drawn, so what the raid sees is what
// hits it - and bots step out of it like players (mod-playerbots' "avoid indicators").
//
// Phase 1, the Broodmother (100-65%): Flame Breath, a cone where she faces; Tail Sweep, a cone behind her that
// throws back whoever stands there; Searing Brand, a circle following a few players that bursts on whoever is still
// next to them (take it away from the others); whelps from the side caves.
// Phase 2, Wings of Flame (65-35%): Deep Breath, four long lines from her in a cross, then four more between them
// (stand between the lines of the first, then move); Fire Rain, a circle under every player (keep moving); the lair
// guard joins the fight.
// Phase 3, the Molten Lair (35-0%): the lair erupts one half at a time, the other half right after (cross over as
// soon as the first half has burst); Bellowing Roar hurts the whole raid; the Broodmother's abilities come faster.
namespace
{
// The stock instance script (src/server/scripts/Kalimdor/OnyxiasLair) keeps the boss state: boss 0
constexpr uint32 DATA_ONYXIA = 0;
constexpr uint32 NPC_ONYXIAN_WHELP = 11262;
constexpr uint32 NPC_ONYXIA_TRIGGER = 12758;
constexpr uint32 NPC_ONYXIAN_LAIR_GUARD = 36561;

enum Spells
{
    SPELL_CLEAVE            = 68868,
    SPELL_SUMMON_WHELP      = 17646,
    SPELL_SUMMON_LAIR_GUARD = 68968,

    // Only named in the combat log: the damage is dealt by the script, on the areas it drew
    SPELL_FLAME_BREATH      = 18435,
    SPELL_TAIL_SWEEP        = 68867,
    SPELL_SEARING_BRAND     = 68958,    // Blast Nova
    SPELL_DEEP_BREATH       = 17086,
    SPELL_FIRE_RAIN         = 18392,    // Fireball
    SPELL_ERUPTION          = 17731,
    SPELL_BELLOWING_ROAR    = 18431,
};

// Stock spell visual kits, played where each ability lands
enum Kits
{
    KIT_FLAME_BREATH        = 13375,    // Flame Breath's breath
    KIT_DEEP_BREATH         = 13364,    // Deep Breath's breath
    KIT_TAIL_SWEEP          = 3509,
    KIT_FIRE_EXPLOSION      = 608,      // Fireball's impact
    KIT_FIRE_RING           = 984,      // Blast Wave's ring
    KIT_ERUPTION            = 4409,     // the lair's lava eruption
    KIT_ROAR                = 4310,
    KIT_ROAR_HIT            = 498,
};

enum Events
{
    EVENT_CLEAVE = 1,
    EVENT_FLAME_BREATH,
    EVENT_TAIL_SWEEP,
    EVENT_SEARING_BRAND,
    EVENT_WHELPS,
    EVENT_DEEP_BREATH,
    EVENT_FIRE_RAIN,
    EVENT_ERUPTION,
    EVENT_ROAR,
};

enum Phases
{
    PHASE_NONE,
    PHASE_BROODMOTHER,
    PHASE_WINGS,
    PHASE_MOLTEN
};

enum Yells
{
    SAY_AGGRO               = 0,
    SAY_KILL                = 1,
    SAY_PHASE_3             = 3,
    EMOTE_BREATH            = 4,
    SAY_EVADE               = 5
};

// The lair: the middle of its floor and how far it reaches
Position const LairCenter = { -21.0f, -215.0f, -86.0f, 0.0f };
constexpr float LairHalfLength = 50.0f;
constexpr float LairHalfWidth = 25.0f;
Position const LairGuardSpawn = { -101.654f, -214.491f, -80.70f, 0.0f };
Position const WhelpCaves[] = { { -33.18f, -258.80f, -89.0f, 0.0f }, { -32.535f, -170.190f, -89.0f, 0.0f } };

// Sizes and warnings. Every area is drawn for its warning time, then resolved.
constexpr float FlameBreathRadius = 40.0f;
constexpr float FlameBreathArc = 60.0f;
constexpr uint32 FlameBreathWarningMs = 2500;
constexpr float TailSweepRadius = 20.0f;
constexpr float TailSweepArc = 120.0f;
constexpr uint32 TailSweepWarningMs = 2000;
constexpr float SearingBrandRadius = 8.0f;
constexpr uint32 SearingBrandMs = 6000;
constexpr float DeepBreathLength = 60.0f;
constexpr float DeepBreathWidth = 8.0f;
constexpr uint32 DeepBreathWarningMs = 3500;
constexpr float FireRainRadius = 6.0f;
constexpr uint32 FireRainWarningMs = 3000;
constexpr uint32 EruptionWarningMs = 3000;
constexpr uint32 RoarWarningMs = 3000;

// Damage by size (10 / 25 players). Players of this raid have 20-30k health.
struct Damage
{
    uint32 raid10;
    uint32 raid25;
};
constexpr Damage FlameBreathDamage = { 20000, 25000 };
constexpr Damage TailSweepDamage = { 9000, 11000 };
constexpr Damage SearingBrandDamage = { 18000, 22000 };          // to everyone else in the circle
constexpr Damage SearingBrandCarrierDamage = { 6000, 7500 };     // to the one carrying it
constexpr Damage DeepBreathDamage = { 28000, 34000 };
constexpr Damage FireRainDamage = { 12000, 15000 };
constexpr Damage EruptionDamage = { 25000, 30000 };
constexpr Damage RoarDamage = { 7000, 9000 };

struct boss_onyxia_evolutions : public BossAI
{
    boss_onyxia_evolutions(Creature* creature) : BossAI(creature, DATA_ONYXIA) { }

    void Reset() override
    {
        _phase = PHASE_NONE;
        scheduler.CancelAll();
        EndWindup();
        me->SetReactState(REACT_AGGRESSIVE);
        me->SetCanFly(false);
        me->SetDisableGravity(false);
        BossAI::Reset();
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelAll();
        EndWindup();
        BossAI::EnterEvadeMode(why);
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);
        BossAI::JustEngagedWith(who);
        SetPhase(PHASE_BROODMOTHER);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KILL);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (_phase == PHASE_BROODMOTHER && me->HealthBelowPctDamaged(65, damage))
            SetPhase(PHASE_WINGS);
        else if (_phase == PHASE_WINGS && me->HealthBelowPctDamaged(35, damage))
            SetPhase(PHASE_MOLTEN);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() != NPC_ONYXIAN_WHELP && summon->GetEntry() != NPC_ONYXIAN_LAIR_GUARD)
            return;

        if (Unit* target = summon->SelectNearestTarget(300.0f))
        {
            summon->AI()->AttackStart(target);
            DoZoneInCombat(summon);
        }
    }

    bool CheckInRoom() override
    {
        if (me->GetDistance2d(me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY()) > 95.0f)
        {
            Talk(SAY_EVADE);
            EnterEvadeMode();
            return false;
        }
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim() || !CheckInRoom())
            return;

        scheduler.Update(diff);
        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            // One breath at a time: while she holds still for one, the next warned ability waits
            if (_windup && eventId != EVENT_CLEAVE)
            {
                events.ScheduleEvent(eventId, 1s);
                continue;
            }
            ExecuteEvent(eventId);
        }

        DoMeleeAttackIfReady();
    }

private:
    void SetPhase(Phases phase)
    {
        _phase = phase;
        events.Reset();
        switch (phase)
        {
            case PHASE_BROODMOTHER:
                events.ScheduleEvent(EVENT_CLEAVE, 5s);
                events.ScheduleEvent(EVENT_FLAME_BREATH, 12s);
                events.ScheduleEvent(EVENT_TAIL_SWEEP, 16s);
                events.ScheduleEvent(EVENT_SEARING_BRAND, 20s);
                events.ScheduleEvent(EVENT_WHELPS, 30s);
                break;
            case PHASE_WINGS:
                Talk(EMOTE_BREATH);
                me->CastSpell(LairGuardSpawn.GetPositionX(), LairGuardSpawn.GetPositionY(),
                    LairGuardSpawn.GetPositionZ(), SPELL_SUMMON_LAIR_GUARD, true);
                events.ScheduleEvent(EVENT_CLEAVE, 5s);
                events.ScheduleEvent(EVENT_DEEP_BREATH, 6s);
                events.ScheduleEvent(EVENT_FIRE_RAIN, 14s);
                events.ScheduleEvent(EVENT_FLAME_BREATH, 20s);
                events.ScheduleEvent(EVENT_SEARING_BRAND, 26s);
                events.ScheduleEvent(EVENT_WHELPS, 35s);
                break;
            case PHASE_MOLTEN:
                Talk(SAY_PHASE_3);
                events.ScheduleEvent(EVENT_CLEAVE, 5s);
                events.ScheduleEvent(EVENT_ERUPTION, 5s);
                events.ScheduleEvent(EVENT_FLAME_BREATH, 12s);
                events.ScheduleEvent(EVENT_TAIL_SWEEP, 9s);
                events.ScheduleEvent(EVENT_SEARING_BRAND, 16s);
                events.ScheduleEvent(EVENT_ROAR, 24s);
                events.ScheduleEvent(EVENT_WHELPS, 30s);
                break;
            default:
                break;
        }
    }

    void ExecuteEvent(uint32 eventId)
    {
        bool const molten = _phase == PHASE_MOLTEN;
        switch (eventId)
        {
            case EVENT_CLEAVE:
                DoCastVictim(SPELL_CLEAVE);
                events.ScheduleEvent(EVENT_CLEAVE, 5s, 8s);
                break;
            case EVENT_FLAME_BREATH:
                FlameBreath();
                events.ScheduleEvent(EVENT_FLAME_BREATH, molten ? 12s : (_phase == PHASE_WINGS ? 25s : 16s));
                break;
            case EVENT_TAIL_SWEEP:
                TailSweep();
                events.ScheduleEvent(EVENT_TAIL_SWEEP, molten ? 10s : 14s);
                break;
            case EVENT_SEARING_BRAND:
                SearingBrand();
                events.ScheduleEvent(EVENT_SEARING_BRAND, molten ? 18s : 24s);
                break;
            case EVENT_WHELPS:
                SummonWhelps();
                events.ScheduleEvent(EVENT_WHELPS, molten ? 30s : 40s);
                break;
            case EVENT_DEEP_BREATH:
                DeepBreath();
                events.ScheduleEvent(EVENT_DEEP_BREATH, 24s);
                break;
            case EVENT_FIRE_RAIN:
                FireRain();
                events.ScheduleEvent(EVENT_FIRE_RAIN, 16s);
                break;
            case EVENT_ERUPTION:
                Eruption();
                events.ScheduleEvent(EVENT_ERUPTION, 20s);
                break;
            case EVENT_ROAR:
                Roar();
                events.ScheduleEvent(EVENT_ROAR, 30s);
                break;
            default:
                break;
        }
    }

    // A breath comes out of her where the red is drawn, so from its warning to the moment it lands she stays where
    // she is, facing it: rooted, and no longer turning to her target (the client turns a creature towards its
    // target). Without it she followed a tank stepping aside and breathed from somewhere else than the drawn area.
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

    uint32 Amount(Damage const& damage) const
    {
        return me->GetMap()->Is25ManRaid() ? damage.raid25 : damage.raid10;
    }

    // Deals damage as the named spell: resistances and absorbs apply, and it reads as that spell in the log
    void Burn(Unit* target, uint32 spellId, uint32 amount)
    {
        MythicTuning::DealAbilityDamage(me, target, spellId, amount);
    }

    std::vector<Player*> PlayersIn(GroundIndicators::Area const& area)
    {
        std::vector<Player*> players;
        for (auto const& ref : me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && area.Contains(*player))
                    players.push_back(player);
        return players;
    }

    // A stock visual kit where something lands on the ground, on a short-lived invisible trigger
    void PlayOnGround(Position const& where, uint32 kit)
    {
        if (Creature* trigger = me->SummonCreature(NPC_ONYXIA_TRIGGER, where, TEMPSUMMON_TIMED_DESPAWN, 3000))
            trigger->SendPlaySpellVisual(kit);
    }

    Position Ground(float x, float y) const
    {
        float z = me->GetMap()->GetHeight(me->GetPhaseMask(), x, y, LairCenter.GetPositionZ() + 10.0f, true, 50.0f);
        if (z <= INVALID_HEIGHT)
            z = LairCenter.GetPositionZ();
        return Position(x, y, z);
    }

    void FlameBreath()
    {
        Unit* victim = me->GetVictim();
        float const facing = victim ? me->GetAngle(victim) : me->GetOrientation();
        BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, *me, facing, FlameBreathRadius,
            FlameBreathArc, FlameBreathWarningMs, GroundIndicators::Theme::Fire);
        scheduler.Schedule(Milliseconds(FlameBreathWarningMs), [this, area, facing](TaskContext)
        {
            me->SetFacingTo(facing);
            me->SendPlaySpellVisual(KIT_FLAME_BREATH);
            for (Player* player : PlayersIn(area))
                Burn(player, SPELL_FLAME_BREATH, Amount(FlameBreathDamage));
            EndWindup();
        });
    }

    void TailSweep()
    {
        float const facing = me->GetOrientation();
        float const behind = Position::NormalizeOrientation(facing + float(M_PI));
        BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, *me, behind, TailSweepRadius,
            TailSweepArc, TailSweepWarningMs, GroundIndicators::Theme::Fire);
        scheduler.Schedule(Milliseconds(TailSweepWarningMs), [this, area](TaskContext)
        {
            me->SendPlaySpellVisual(KIT_TAIL_SWEEP);
            for (Player* player : PlayersIn(area))
            {
                Burn(player, SPELL_TAIL_SWEEP, Amount(TailSweepDamage));
                player->KnockbackFrom(me->GetPositionX(), me->GetPositionY(), 20.0f, 8.0f);
            }
            EndWindup();
        });
    }

    void SearingBrand()
    {
        Unit* tank = me->GetVictim();
        std::list<Unit*> carriers;
        SelectTargetList(carriers, me->GetMap()->Is25ManRaid() ? 3 : 2, SelectTargetMethod::Random, 0,
            [tank](Unit* unit) { return unit->IsPlayer() && unit != tank; });

        for (Unit* carrier : carriers)
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, SearingBrandRadius,
                SearingBrandMs);
            ObjectGuid const carrierGuid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(SearingBrandMs), [this, area, carrierGuid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
                if (!carrier || !carrier->IsAlive())
                    return;

                carrier->SendPlaySpellVisual(KIT_FIRE_RING);
                Burn(carrier, SPELL_SEARING_BRAND, Amount(SearingBrandCarrierDamage));
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                        Burn(player, SPELL_SEARING_BRAND, Amount(SearingBrandDamage));
            });
        }
    }

    void SummonWhelps()
    {
        uint32 const perCave = me->GetMap()->Is25ManRaid() ? 2 : 1;
        for (Position const& cave : WhelpCaves)
        {
            for (uint32 index = 0; index < perCave; ++index)
            {
                float const angle = rand_norm() * 2.0f * float(M_PI);
                float const distance = rand_norm() * 4.0f;
                me->CastSpell(cave.GetPositionX() + std::cos(angle) * distance,
                    cave.GetPositionY() + std::sin(angle) * distance, cave.GetPositionZ(), SPELL_SUMMON_WHELP, true);
            }
        }
    }

    // Four lines in a cross from where she stands, then the four between them
    void DeepBreath()
    {
        Talk(EMOTE_BREATH);
        Position const origin = me->GetPosition();
        float const base = rand_norm() * float(M_PI) / 2.0f;
        // She holds still for both waves: every line starts from her
        BeginWindup(base);
        BreathWave(origin, base, 0);
    }

    void BreathWave(Position const& origin, float base, uint32 wave)
    {
        std::vector<GroundIndicators::Area> lines;
        for (uint32 arm = 0; arm < 4; ++arm)
        {
            float const facing = Position::NormalizeOrientation(base + arm * float(M_PI) / 2.0f);
            lines.push_back(GroundIndicators::ShowRectangle(me, origin, facing, DeepBreathLength, DeepBreathWidth,
                DeepBreathWarningMs, GroundIndicators::Theme::Fire));
        }

        scheduler.Schedule(Milliseconds(DeepBreathWarningMs), [this, origin, base, wave, lines](TaskContext)
        {
            me->SetFacingTo(base);
            me->SendPlaySpellVisual(KIT_DEEP_BREATH);
            for (GroundIndicators::Area const& line : lines)
            {
                for (float along = 10.0f; along <= DeepBreathLength; along += 12.5f)
                {
                    float const facing = line.origin.GetOrientation();
                    PlayOnGround(Ground(origin.GetPositionX() + std::cos(facing) * along,
                        origin.GetPositionY() + std::sin(facing) * along), KIT_FIRE_EXPLOSION);
                }
            }

            std::vector<Player*> burned;
            for (GroundIndicators::Area const& line : lines)
                for (Player* player : PlayersIn(line))
                    if (std::find(burned.begin(), burned.end(), player) == burned.end())
                        burned.push_back(player);
            for (Player* player : burned)
                Burn(player, SPELL_DEEP_BREATH, Amount(DeepBreathDamage));

            if (wave == 0)
            {
                me->SetFacingTo(base + float(M_PI) / 4.0f);
                BreathWave(origin, base + float(M_PI) / 4.0f, 1);
            }
            else
                EndWindup();
        });
    }

    // A circle under every player, where they stand now
    void FireRain()
    {
        for (auto const& ref : me->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster())
                continue;

            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, *player, FireRainRadius,
                FireRainWarningMs, GroundIndicators::Theme::Fire);
            scheduler.Schedule(Milliseconds(FireRainWarningMs), [this, area](TaskContext)
            {
                PlayOnGround(area.origin, KIT_FIRE_EXPLOSION);
                for (Player* hit : PlayersIn(area))
                    Burn(hit, SPELL_FIRE_RAIN, Amount(FireRainDamage));
            });
        }
    }

    // One half of the lair, then the other: the line between them is a random one through the middle
    void Eruption()
    {
        float const axis = rand_norm() * float(M_PI);
        EruptHalf(axis, 1.0f, true);
    }

    void EruptHalf(float axis, float side, bool first)
    {
        float const normal = axis + float(M_PI) / 2.0f;
        float const startX = LairCenter.GetPositionX() - std::cos(axis) * LairHalfLength +
            std::cos(normal) * side * LairHalfWidth;
        float const startY = LairCenter.GetPositionY() - std::sin(axis) * LairHalfLength +
            std::sin(normal) * side * LairHalfWidth;
        Position const start = Ground(startX, startY);
        GroundIndicators::Area const half = GroundIndicators::ShowRectangle(me, start, axis, 2.0f * LairHalfLength,
            2.0f * LairHalfWidth, EruptionWarningMs, GroundIndicators::Theme::Fire);

        scheduler.Schedule(Milliseconds(EruptionWarningMs), [this, axis, side, first, half, normal](TaskContext)
        {
            for (float along = 10.0f; along < 2.0f * LairHalfLength; along += 20.0f)
            {
                for (float across = 8.0f; across < 2.0f * LairHalfWidth; across += 16.0f)
                {
                    float const x = half.origin.GetPositionX() + std::cos(axis) * along -
                        std::cos(normal) * side * (across - LairHalfWidth);
                    float const y = half.origin.GetPositionY() + std::sin(axis) * along -
                        std::sin(normal) * side * (across - LairHalfWidth);
                    PlayOnGround(Ground(x, y), KIT_ERUPTION);
                }
            }
            for (Player* player : PlayersIn(half))
                Burn(player, SPELL_ERUPTION, Amount(EruptionDamage));

            if (first)
                EruptHalf(axis, -side, false);
        });
    }

    void Roar()
    {
        me->SendPlaySpellVisual(KIT_ROAR);
        scheduler.Schedule(Milliseconds(RoarWarningMs), [this](TaskContext)
        {
            for (auto const& ref : me->GetMap()->GetPlayers())
            {
                Player* player = ref.GetSource();
                if (!player || !player->IsAlive() || player->IsGameMaster())
                    continue;
                player->SendPlaySpellVisual(KIT_ROAR_HIT);
                Burn(player, SPELL_BELLOWING_ROAR, Amount(RoarDamage));
            }
        });
    }

    Phases _phase = PHASE_NONE;
    bool _windup = false;
};
}

void AddOnyxiaReworkScripts()
{
    RegisterCreatureAI(boss_onyxia_evolutions);
}
