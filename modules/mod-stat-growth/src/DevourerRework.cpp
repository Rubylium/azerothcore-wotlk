#include "GroundIndicators.h"
#include "MythicTuning.h"

#include "Containers.h"
#include "CreatureScript.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "QuestDef.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

// The Devourer of Souls (the Forge of Souls), reworked from nothing of his own. The stock fight asks for Mirrored
// Soul to be seen and stopped, the Well of Souls to be walked out of before it grows, the unleashed souls to be
// outrun and Wailing Souls' sweeping beam to be kept behind: bots managed none of it. Here every ability is drawn in
// red before it lands and resolved against the area drawn, so bots step out of it like players (mod-playerbots'
// "avoid indicators"). His three faces mark the three phases.
//
// Desire (100-70%): Soul Strike, a long line at the tank he holds still for (step aside); Wail of Souls, circles near
// a few players (move); Soul Scream, a small circle on everyone that bursts on whoever stands in another's (spread).
// Sorrow (70-35%): Siphon Soul, cones sweeping round him one after the other (keep behind the sweep); Soul Strike and
// Wail of Souls go on.
// Anger (35-0%): Harvest Soul, the half of the room he faces, then the other half (cross over); Soul Blast, three
// waves of circles raining over the room; Soul Scream and Soul Strike come faster.
namespace
{
// The stock instance script (instance_forge_of_souls) keeps the boss state: boss 1
constexpr uint32 DATA_DEVOURER = 1;
constexpr uint32 NPC_WORLD_TRIGGER = 22515;
// The quest Tempering the Blade needs the Crucible of Souls at hand when he is fought
constexpr uint32 NPC_CRUCIBLE_OF_SOULS = 37094;
constexpr uint32 QUEST_TEMPERING_THE_BLADE_A = 24476;
constexpr uint32 QUEST_TEMPERING_THE_BLADE_H = 24560;
Position const CruciblePosition = { 5672.29f, 2520.69f, 713.44f, 0.96f };

// His faces
constexpr uint32 MODEL_ANGER = 30148;
constexpr uint32 MODEL_SORROW = 30149;
constexpr uint32 MODEL_DESIRE = 30150;

// Only named in the combat log: the damage is dealt by the script, on the areas it drew
enum Spells
{
    SPELL_SOUL_STRIKE       = 69088,
    SPELL_WAIL_OF_SOULS     = 28459,
    SPELL_SOUL_SCREAM       = 41545,
    SPELL_SIPHON_SOUL       = 43501,
    SPELL_HARVEST_SOUL      = 68980,
    SPELL_SOUL_BLAST        = 41245,
};

// Stock spell visual kits of those spells
enum Kits
{
    KIT_STRIKE_CAST         = 539,
    KIT_STRIKE_HIT          = 330,
    KIT_WAIL_CAST           = 4009,
    KIT_WAIL_HIT            = 2350,
    KIT_SCREAM_CAST         = 7991,
    KIT_SCREAM_HIT          = 2729,
    KIT_BLAST_CAST          = 218,
};

enum Events
{
    EVENT_SOUL_STRIKE = 1,
    EVENT_WAIL_OF_SOULS,
    EVENT_SOUL_SCREAM,
    EVENT_SIPHON_SOUL,
    EVENT_HARVEST_SOUL,
    EVENT_SOUL_BLAST,
};

enum Phases
{
    PHASE_NONE,
    PHASE_DESIRE,
    PHASE_SORROW,
    PHASE_ANGER
};

enum Texts
{
    SAY_AGGRO               = 0,
    SAY_SLAY_ANGER          = 1,
    SAY_SLAY_SORROW         = 2,
    SAY_SLAY_DESIRE         = 3,
    SAY_DEATH               = 4,
    SAY_ANGER               = 7,    // "SUFFERING! ANGUISH! CHAOS! RISE AND FEED!"
    SAY_SORROW              = 9,    // "Stare into the abyss and see your end."
};

// Sizes and warnings. Every area is drawn for its warning time, then resolved.
constexpr float StrikeLength = 32.0f;
constexpr float StrikeWidth = 7.0f;
constexpr uint32 StrikeWarningMs = 2500;
constexpr float WailRadius = 7.0f;
constexpr uint32 WailWarningMs = 3000;
constexpr uint32 WailCount = 3;
constexpr float ScreamRadius = 5.0f;
constexpr uint32 ScreamMs = 5000;
constexpr float SiphonRadius = 40.0f;
constexpr float SiphonArc = 60.0f;
constexpr uint32 SiphonWarningMs = 2500;
constexpr uint32 SiphonStepMs = 1200;
constexpr uint32 SiphonSteps = 6;
constexpr float HarvestRadius = 45.0f;
constexpr float HarvestArc = 180.0f;
constexpr uint32 HarvestWarningMs = 3500;
constexpr float BlastRadius = 6.0f;
constexpr uint32 BlastWarningMs = 2500;
constexpr uint32 BlastWaves = 3;
constexpr uint32 BlastsPerWave = 5;
constexpr uint32 BlastWaveMs = 1500;
constexpr float BlastSpread = 28.0f;

// Damage. In a mythic key, a share of the reference health (MythicTuning.h: what a damage dealer has at that key),
// the same danger at every key; the budgets are in .agents/plans/mplus-damage-audit/forge-of-souls.ANALYSIS.md.
// Elsewhere, the heroic amount (normal at NormalPercent of it): heroic players of the Forge have 20-25k health.
struct AbilityDamage
{
    uint32 heroic;
    float mythicPercent;
};

constexpr uint32 NormalPercent = 60;
constexpr AbilityDamage StrikeDamage = { 10000, 55.0f };
constexpr AbilityDamage WailDamage = { 8000, 44.0f };
constexpr AbilityDamage ScreamDamage = { 7000, 45.0f };         // to everyone else in the circle
constexpr AbilityDamage ScreamCarrierDamage = { 1500, 10.0f };  // to the one carrying it
constexpr AbilityDamage SiphonDamage = { 9000, 50.0f };
constexpr AbilityDamage HarvestDamage = { 12000, 64.0f };
constexpr AbilityDamage BlastDamage = { 7000, 31.0f };

struct boss_devourer_of_souls_evolutions : public BossAI
{
    boss_devourer_of_souls_evolutions(Creature* creature) : BossAI(creature, DATA_DEVOURER) { }

    void Reset() override
    {
        _phase = PHASE_NONE;
        scheduler.CancelAll();
        EndWindup();
        me->SetDisplayId(me->GetNativeDisplayId());
        BossAI::Reset();
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        scheduler.CancelAll();
        EndWindup();
        _phase = PHASE_NONE;
        BossAI::EnterEvadeMode(why);
    }

    // The achievement Three Faced asked for Mirrored Soul, which he no longer casts
    uint32 GetData(uint32 /*id*/) const override
    {
        return 0;
    }

    void JustEngagedWith(Unit* who) override
    {
        Talk(SAY_AGGRO);
        BossAI::JustEngagedWith(who);
        SetPhase(PHASE_DESIRE);

        // The quest Tempering the Blade: the Crucible of Souls stands by for whoever has it
        if (me->FindNearestCreature(NPC_CRUCIBLE_OF_SOULS, 100.0f))
            return;
        bool wanted = false;
        me->GetMap()->DoForAllPlayers([&wanted](Player* player)
        {
            uint32 const quest = player->GetTeamId() == TEAM_ALLIANCE ? QUEST_TEMPERING_THE_BLADE_A
                                                                      : QUEST_TEMPERING_THE_BLADE_H;
            if (player->GetQuestStatus(quest) == QUEST_STATUS_INCOMPLETE)
                wanted = true;
        });
        if (wanted)
            me->SummonCreature(NPC_CRUCIBLE_OF_SOULS, CruciblePosition);
    }

    void KilledUnit(Unit* victim) override
    {
        if (!victim->IsPlayer())
            return;
        Talk(_phase == PHASE_ANGER ? SAY_SLAY_ANGER : _phase == PHASE_SORROW ? SAY_SLAY_SORROW : SAY_SLAY_DESIRE);
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (_phase == PHASE_DESIRE && me->HealthBelowPctDamaged(70, damage))
            SetPhase(PHASE_SORROW);
        else if (_phase == PHASE_SORROW && me->HealthBelowPctDamaged(35, damage))
            SetPhase(PHASE_ANGER);
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
        if (!UpdateVictim())
            return;

        events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            // One held ability at a time: while he stands still for one, the next waits
            bool const held = eventId == EVENT_SOUL_STRIKE || eventId == EVENT_SIPHON_SOUL ||
                              eventId == EVENT_HARVEST_SOUL;
            if (_windup && held)
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
            case PHASE_DESIRE:
                me->SetDisplayId(MODEL_DESIRE);
                events.ScheduleEvent(EVENT_SOUL_STRIKE, 10s);
                events.ScheduleEvent(EVENT_WAIL_OF_SOULS, 14s);
                events.ScheduleEvent(EVENT_SOUL_SCREAM, 22s);
                break;
            case PHASE_SORROW:
                Talk(SAY_SORROW);
                me->SetDisplayId(MODEL_SORROW);
                events.ScheduleEvent(EVENT_SIPHON_SOUL, 5s);
                events.ScheduleEvent(EVENT_SOUL_STRIKE, 16s);
                events.ScheduleEvent(EVENT_WAIL_OF_SOULS, 20s);
                break;
            case PHASE_ANGER:
                Talk(SAY_ANGER);
                me->SetDisplayId(MODEL_ANGER);
                events.ScheduleEvent(EVENT_HARVEST_SOUL, 5s);
                events.ScheduleEvent(EVENT_SOUL_BLAST, 14s);
                events.ScheduleEvent(EVENT_SOUL_SCREAM, 20s);
                events.ScheduleEvent(EVENT_SOUL_STRIKE, 11s);
                break;
            default:
                break;
        }
    }

    void ExecuteEvent(uint32 eventId)
    {
        bool const anger = _phase == PHASE_ANGER;
        switch (eventId)
        {
            case EVENT_SOUL_STRIKE:
                SoulStrike();
                events.ScheduleEvent(EVENT_SOUL_STRIKE, anger ? 10s : 13s);
                break;
            case EVENT_WAIL_OF_SOULS:
                WailOfSouls();
                events.ScheduleEvent(EVENT_WAIL_OF_SOULS, 15s);
                break;
            case EVENT_SOUL_SCREAM:
                SoulScream();
                events.ScheduleEvent(EVENT_SOUL_SCREAM, anger ? 20s : 25s);
                break;
            case EVENT_SIPHON_SOUL:
                SiphonSoul();
                events.ScheduleEvent(EVENT_SIPHON_SOUL, 26s);
                break;
            case EVENT_HARVEST_SOUL:
                HarvestSoul();
                events.ScheduleEvent(EVENT_HARVEST_SOUL, 24s);
                break;
            case EVENT_SOUL_BLAST:
                SoulBlast();
                events.ScheduleEvent(EVENT_SOUL_BLAST, 18s);
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

    // Deals damage as the named spell: defensives, resistances and absorbs apply, and it reads as that spell
    void Burn(Unit* target, uint32 spellId, AbilityDamage const& damage)
    {
        if (me->GetMap()->IsMythic())
            MythicTuning::DealReferenceDamage(me, target, spellId, damage.mythicPercent);
        else
            MythicTuning::DealAbilityDamage(me, target, spellId, Amount(damage.heroic));
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

    void BurnIn(std::vector<GroundIndicators::Area> const& areas, uint32 spellId, AbilityDamage const& damage)
    {
        std::vector<Player*> burned;
        for (GroundIndicators::Area const& area : areas)
            for (Player* player : PlayersIn(area))
                if (std::find(burned.begin(), burned.end(), player) == burned.end())
                    burned.push_back(player);
        for (Player* player : burned)
            Burn(player, spellId, damage);
    }

    Position Ground(float x, float y) const
    {
        float z = me->GetMap()->GetHeight(me->GetPhaseMask(), x, y, me->GetPositionZ() + 10.0f, true, 50.0f);
        if (z <= INVALID_HEIGHT)
            z = me->GetPositionZ();
        return Position(x, y, z);
    }

    // Bursts along an area's middle line (a rectangle's, or a cone's axis and edges)
    void BurstAlong(GroundIndicators::Area const& area, float from, float to, float step)
    {
        float const facing = area.origin.GetOrientation();
        std::vector<float> rays = { facing };
        if (area.kind == GroundIndicators::Area::Kind::Cone)
            rays = { facing - area.arc / 3.0f, facing, facing + area.arc / 3.0f };
        for (float ray : rays)
            for (float along = from; along <= to; along += step)
                GroundIndicators::Burst(me, Ground(area.origin.GetPositionX() + std::cos(ray) * along,
                    area.origin.GetPositionY() + std::sin(ray) * along), GroundIndicators::Theme::Shadow);
    }

    // Soul Strike: a long line at the tank; he holds still for it, so a step aside is a step out
    void SoulStrike()
    {
        Unit* victim = me->GetVictim();
        float const facing = victim ? me->GetAngle(victim) : me->GetOrientation();
        BeginWindup(facing);
        me->SendPlaySpellVisual(KIT_STRIKE_CAST);
        GroundIndicators::Area const line = GroundIndicators::ShowRectangle(me, *me, facing, StrikeLength,
            StrikeWidth, StrikeWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(StrikeWarningMs), [this, line, facing](TaskContext)
        {
            me->SetFacingTo(facing);
            me->SendPlaySpellVisual(KIT_STRIKE_CAST);
            BurstAlong(line, 4.0f, StrikeLength, 7.0f);
            for (Player* player : PlayersIn(line))
            {
                player->SendPlaySpellVisual(KIT_STRIKE_HIT);
                Burn(player, SPELL_SOUL_STRIKE, StrikeDamage);
            }
            EndWindup();
        });
    }

    // Wail of Souls: circles near a few players, a little off where they stand
    void WailOfSouls()
    {
        me->SendPlaySpellVisual(KIT_WAIL_CAST);
        std::vector<Player*> players = Players();
        Acore::Containers::RandomShuffle(players);
        if (players.size() > WailCount)
            players.resize(WailCount);

        for (Player* player : players)
        {
            float const angle = rand_norm() * 2.0f * float(M_PI);
            float const distance = rand_norm() * 4.0f;
            Position const center = Ground(player->GetPositionX() + std::cos(angle) * distance,
                player->GetPositionY() + std::sin(angle) * distance);
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, center, WailRadius, WailWarningMs,
                GroundIndicators::Theme::Shadow);
            scheduler.Schedule(Milliseconds(WailWarningMs), [this, area](TaskContext)
            {
                GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Shadow);
                for (Player* hit : PlayersIn(area))
                {
                    hit->SendPlaySpellVisual(KIT_WAIL_HIT);
                    Burn(hit, SPELL_WAIL_OF_SOULS, WailDamage);
                }
            });
        }
    }

    // Soul Scream: a small circle on everyone, bursting on whoever stands in another's
    void SoulScream()
    {
        me->SendPlaySpellVisual(KIT_SCREAM_CAST);
        for (Player* carrier : Players())
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, ScreamRadius,
                ScreamMs);
            ObjectGuid const carrierGuid = carrier->GetGUID();
            scheduler.Schedule(Milliseconds(ScreamMs), [this, area, carrierGuid](TaskContext)
            {
                Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
                if (!carrier || !carrier->IsAlive())
                    return;

                carrier->SendPlaySpellVisual(KIT_SCREAM_HIT);
                GroundIndicators::Burst(me, *carrier, GroundIndicators::Theme::Shadow);
                Burn(carrier, SPELL_SOUL_SCREAM, ScreamCarrierDamage);
                for (Player* player : PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                    if (player != carrier)
                        Burn(player, SPELL_SOUL_SCREAM, ScreamDamage);
            });
        }
    }

    // Siphon Soul: cones sweeping round him, each drawn a step after the last, the sweep turning one way
    void SiphonSoul()
    {
        Unit* victim = me->GetVictim();
        float const start = (victim ? me->GetAngle(victim) : me->GetOrientation()) + float(M_PI) / 3.0f;
        float const turn = urand(0, 1) ? 1.0f : -1.0f;
        BeginWindup(me->GetOrientation());
        for (uint32 step = 0; step < SiphonSteps; ++step)
        {
            float const facing = Position::NormalizeOrientation(start + turn * step * float(M_PI) / 3.0f);
            scheduler.Schedule(Milliseconds(step * SiphonStepMs), [this, facing, step](TaskContext)
            {
                me->SetFacingTo(facing);
                GroundIndicators::Area const cone = GroundIndicators::ShowCone(me, *me, facing, SiphonRadius,
                    SiphonArc, SiphonWarningMs, GroundIndicators::Theme::Shadow);
                scheduler.Schedule(Milliseconds(SiphonWarningMs), [this, cone, step](TaskContext)
                {
                    BurstAlong(cone, 8.0f, SiphonRadius, 9.0f);
                    BurnIn({ cone }, SPELL_SIPHON_SOUL, SiphonDamage);
                    if (step + 1 == SiphonSteps)
                        EndWindup();
                });
            });
        }
    }

    // Harvest Soul: the half of the room he faces, then the other half
    void HarvestSoul()
    {
        Unit* victim = me->GetVictim();
        float const facing = victim ? me->GetAngle(victim) : me->GetOrientation();
        BeginWindup(facing);
        HarvestHalf(facing, true);
    }

    void HarvestHalf(float facing, bool first)
    {
        me->SetFacingTo(facing);
        me->SendPlaySpellVisual(KIT_WAIL_CAST);
        GroundIndicators::Area const half = GroundIndicators::ShowCone(me, *me, facing, HarvestRadius, HarvestArc,
            HarvestWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(HarvestWarningMs), [this, half, facing, first](TaskContext)
        {
            BurstAlong(half, 8.0f, HarvestRadius, 10.0f);
            BurnIn({ half }, SPELL_HARVEST_SOUL, HarvestDamage);
            if (first)
                HarvestHalf(Position::NormalizeOrientation(facing + float(M_PI)), false);
            else
                EndWindup();
        });
    }

    // Soul Blast: waves of circles raining over the room around him, one of each wave under a player
    void SoulBlast()
    {
        me->SendPlaySpellVisual(KIT_BLAST_CAST);
        for (uint32 wave = 0; wave < BlastWaves; ++wave)
        {
            scheduler.Schedule(Milliseconds(wave * BlastWaveMs), [this](TaskContext)
            {
                std::vector<Position> spots;
                std::vector<Player*> players = Players();
                if (!players.empty())
                    spots.push_back(players[urand(0, uint32(players.size() - 1))]->GetPosition());
                while (spots.size() < BlastsPerWave)
                {
                    float const angle = rand_norm() * 2.0f * float(M_PI);
                    float const distance = 6.0f + rand_norm() * (BlastSpread - 6.0f);
                    spots.push_back(Ground(me->GetPositionX() + std::cos(angle) * distance,
                        me->GetPositionY() + std::sin(angle) * distance));
                }

                for (Position const& spot : spots)
                {
                    GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, spot, BlastRadius,
                        BlastWarningMs, GroundIndicators::Theme::Shadow);
                    scheduler.Schedule(Milliseconds(BlastWarningMs), [this, area](TaskContext)
                    {
                        GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Shadow);
                        BurnIn({ area }, SPELL_SOUL_BLAST, BlastDamage);
                    });
                }
            });
        }
    }

    Phases _phase = PHASE_NONE;
    bool _windup = false;
};
}

void AddDevourerReworkScripts()
{
    RegisterCreatureAI(boss_devourer_of_souls_evolutions);
}
