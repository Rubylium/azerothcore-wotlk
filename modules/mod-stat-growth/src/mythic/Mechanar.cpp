#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "Containers.h"
#include "CreatureAIImpl.h"
#include "CreatureScript.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptedCreature.h"
#include "SpellAuras.h"
#include "TaskScheduler.h"
#include "TemporarySummon.h"
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

// The Mechanar in Mythic+: damage tuning, boss reworks and trash abilities (audit: .agents/plans/mplus-damage-audit/).
//
// The dungeon was the easiest of the pool by far (about a third of the damage budget: a +29 was timed with 44% of the
// timer left). Its bosses swung for 2-4% of a tank's health and their mechanics hit for 6-9%. Here their melee is
// brought to the 6-10% a boss swing is meant to be, their spells to their budgets, and each boss is rebuilt on the
// ground indicators where the stock fight hid what was coming:
// - Gatewatcher Gyro-Kill: Saw Blade marks its target with a 10-yard circle for 3 seconds, and the saw bursts on
//   everyone still next to them (take it away from the group).
// - Gatewatcher Iron-Hand: Jackhammer's 15 yards are drawn from the start of its cast, not after it (get out).
// - Mechano-Lord Capacitus: a Nether Charge only shows its (now 10-yard) circle for the 3 seconds before it goes off,
//   instead of a 15-yard one for its whole life that covered the room; Polarity Shift has a real cast bar, its two
//   halves of the room are shown in the colours of the charges and each player is told theirs.
// - Nethermancer Sepethrea: Dragon's Breath is a fire cone she holds still for (leave her front); Arcane Blast marks
//   the tank 2 seconds before it lands (a tank buster: others stand clear); the Raging Flames draw Inferno's 10 yards
//   2 seconds before it starts, and the player a flame chases is told.
// - Pathaleon the Calculator: Arcane Explosion is a 12-yard circle around him he holds still for (melee step out)
//   instead of an unavoidable 30-yard one; Domination marks its victim 3 seconds before it takes them.
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

// A random player other than the tank, or the tank when there is no one else
Unit* RandomNonTank(ScriptedAI* ai, float range)
{
    if (Unit* target = ai->SelectTarget(SelectTargetMethod::Random, 0, range, true, false))
        return target;
    return ai->SelectTarget(SelectTargetMethod::Random, 0, range, true);
}

void WarnPlayer(Creature* me, Unit* target, std::string const& text)
{
    if (Player* player = target ? target->ToPlayer() : nullptr)
        me->Whisper(text, LANG_UNIVERSAL, player, true);
}

// Stops, turns to facing and stays there (no turning back to its target) while something drawn in front of it lands
void HoldStill(Creature* me, float facing)
{
    me->StopMoving();
    me->SetControlled(true, UNIT_STATE_ROOT);
    me->SetTarget();
    me->SetFacingTo(facing);
}

void Release(Creature* me)
{
    me->SetControlled(false, UNIT_STATE_ROOT);
    if (Unit* victim = me->GetVictim())
        me->SetTarget(victim->GetGUID());
}

// The stock instance script (instance_mechanar) keeps the boss states
enum MechanarData : uint32
{
    DATA_GATEWATCHER_GYROKILL       = 0,
    DATA_GATEWATCHER_IRON_HAND      = 1,
    DATA_MECHANOLORD_CAPACITUS      = 2,
    DATA_NETHERMANCER_SEPRETHREA    = 3,
    DATA_PATHALEON_THE_CALCULATOR   = 4,

    // persistent data index
    DATA_BRIDGE_MOB_DEATH_COUNT     = 0,
};

enum MechanarNpcs : uint32
{
    // Normal entry, then the heroic one (a spawn keeps its normal entry in heroic: both are registered)
    NPC_GYROKILL                    = 19218,
    NPC_GYROKILL_H                  = 21525,
    NPC_IRON_HAND                   = 19710,
    NPC_IRON_HAND_H                 = 21526,
    NPC_CAPACITUS                   = 19219,
    NPC_CAPACITUS_H                 = 21533,
    NPC_SEPETHREA                   = 19221,
    NPC_SEPETHREA_H                 = 21536,
    NPC_PATHALEON                   = 19220,
    NPC_PATHALEON_H                 = 21537,

    NPC_SUNSEEKER_ASTROMAGE         = 19168,
    NPC_SUNSEEKER_ASTROMAGE_H       = 21539,
    NPC_SUNSEEKER_ENGINEER          = 20988,
    NPC_SUNSEEKER_ENGINEER_H        = 21540,
    NPC_MECHANAR_DRILLER            = 19712,
    NPC_MECHANAR_DRILLER_H          = 21528,
    NPC_MECHANAR_WRECKER            = 19713,
    NPC_MECHANAR_WRECKER_H          = 21532,
};

// Spells tuned by id (as cast: the heroic id where the core maps one, the normal id where it does not)
enum MechanarTunedSpells : uint32
{
    SPELL_FROST_ATTACK_PROC         = 45195,    // Sepethrea's melee proc: no heroic mapping, normal values in heroic
    SPELL_JACKHAMMER_EFFECT         = 35330,
    SPELL_JACKHAMMER_EFFECT_H       = 39195,
    SPELL_INFERNO_DAMAGE            = 35283,    // Raging Flames' Inferno tick
    SPELL_NETHER_EXPLOSION          = 35058,    // Nether Wraith
    SPELL_POSITIVE_CHARGE_PULSE     = 39090,
    SPELL_NEGATIVE_CHARGE_PULSE     = 39093,

    // Only named in the log by the trash abilities
    SPELL_SOLARBURN                 = 38930,
    SPELL_DEATH_RAY                 = 39196,
    SPELL_GLOB_OF_MACHINE_FLUID     = 38923,
};

// ---------------------------------------------------------------------------------------------------------------------
// Gatewatcher Gyro-Kill
struct boss_gatewatcher_gyrokill_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                   = 0,
        SAY_SLAY                    = 1,
        SAY_SAW_BLADE               = 2,
        SAY_DEATH                   = 3,
    };

    enum Spells : uint32
    {
        SPELL_STREAM_OF_MACHINE_FLUID   = 35311,
        SPELL_SAW_BLADE                 = 35318,
        SPELL_SAW_BLADE_H               = 39192,
        SPELL_SHADOW_POWER              = 35322,
    };

    static constexpr float SawRadius = 10.0f;
    static constexpr uint32 SawWarningMs = 3000;

    boss_gatewatcher_gyrokill_evolutions(Creature* creature) : BossAI(creature, DATA_GATEWATCHER_GYROKILL) { }

    void Reset() override
    {
        _timers.CancelAll();
        BossAI::Reset();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _timers.CancelAll();
        _JustDied();
        Talk(SAY_DEATH);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler.Schedule(10s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_STREAM_OF_MACHINE_FLUID);
            context.Repeat(12s, 14s);
        }).Schedule(20s, [this](TaskContext context)
        {
            SawBlade();
            context.Repeat(25s);
        }).Schedule(30s, [this](TaskContext context)
        {
            me->CastSpell(me, SPELL_SHADOW_POWER, false);
            context.Repeat(25s);
        });

        Talk(SAY_AGGRO);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void UpdateAI(uint32 diff) override
    {
        _timers.Update(diff);
        BossAI::UpdateAI(diff);
    }

private:
    // Saw Blade: a circle follows a player for 3 seconds, then the saw bursts on them and on everyone still inside
    // it (the stock chain jumped 10 yards between players, with nothing to show it). Everyone hit bleeds.
    void SawBlade()
    {
        Unit* target = RandomNonTank(this, 50.0f);
        if (!target)
            return;

        Talk(SAY_SAW_BLADE);
        WarnPlayer(me, target, "Gyro-Kill's saw blade is coming for you: take it away from the others!");
        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, target, SawRadius, SawWarningMs);
        ObjectGuid const targetGuid = target->GetGUID();
        _timers.Schedule(Milliseconds(SawWarningMs), [this, area, targetGuid](TaskContext)
        {
            Unit* carrier = ObjectAccessor::GetUnit(*me, targetGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            uint32 const spell = me->GetMap()->IsHeroic() ? uint32(SPELL_SAW_BLADE_H) : uint32(SPELL_SAW_BLADE);
            GroundIndicators::Burst(me, carrier->GetPosition(), GroundIndicators::Theme::None);
            Hit(me, carrier, spell, 19.0f, 3700);
            me->AddAura(spell, carrier);
            for (Player* player : PlayersIn(me, GroundIndicators::CurrentArea(carrier, area)))
            {
                if (player == carrier)
                    continue;
                Hit(me, player, spell, 38.0f, 5500);
                me->AddAura(spell, player);
            }
        });
    }

    TaskScheduler _timers;
};

// ---------------------------------------------------------------------------------------------------------------------
// Gatewatcher Iron-Hand
struct boss_gatewatcher_iron_hand_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                   = 0,
        SAY_HAMMER                  = 1,
        SAY_SLAY                    = 2,
        SAY_DEATH                   = 3,
        EMOTE_HAMMER                = 4,
    };

    enum Spells : uint32
    {
        SPELL_SHADOW_POWER              = 35322,
        SPELL_JACKHAMMER                = 35327,
        SPELL_STREAM_OF_MACHINE_FLUID   = 35311,
    };

    // Jackhammer's 1.5-second cast: the channel draws its own circle once it starts, this one covers the cast
    static constexpr float JackhammerRadius = 15.0f;
    static constexpr uint32 JackhammerCastMs = 1700;

    boss_gatewatcher_iron_hand_evolutions(Creature* creature) : BossAI(creature, DATA_GATEWATCHER_IRON_HAND) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler.Schedule(15s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_STREAM_OF_MACHINE_FLUID);
            context.Repeat(20s);
        }).Schedule(35s, [this](TaskContext context)
        {
            Talk(EMOTE_HAMMER);
            Talk(SAY_HAMMER);
            me->StopMoving();
            GroundIndicators::ShowCircle(me, me->GetPosition(), JackhammerRadius, JackhammerCastMs);
            DoCastSelf(SPELL_JACKHAMMER);
            context.Repeat(40s);
        }).Schedule(25s, [this](TaskContext context)
        {
            DoCastSelf(SPELL_SHADOW_POWER);
            context.Repeat(25s);
        });

        Talk(SAY_AGGRO);
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Mechano-Lord Capacitus
enum CapacitusSpells : uint32
{
    SPELL_HEADCRACK                 = 35161,
    SPELL_REFLECTIVE_MAGIC_SHIELD   = 35158,
    SPELL_REFLECTIVE_DAMAGE_SHIELD  = 35159,
    SPELL_POLARITY_SHIFT            = 39096,
    SPELL_BERSERK                   = 26662,

    SPELL_SUMMON_NETHER_CHARGE_NE   = 35153,
    SPELL_SUMMON_NETHER_CHARGE_NW   = 35904,
    SPELL_SUMMON_NETHER_CHARGE_SE   = 35905,
    SPELL_SUMMON_NETHER_CHARGE_SW   = 35906,

    SPELL_POSITIVE_POLARITY         = 39088,
    SPELL_POSITIVE_CHARGE_STACK     = 39089,
    SPELL_NEGATIVE_POLARITY         = 39091,
    SPELL_NEGATIVE_CHARGE_STACK     = 39092,

    SPELL_NETHER_CHARGE_PASSIVE     = 35150,
    SPELL_NETHER_CHARGE_PULSE       = 35151,
    SPELL_NETHER_DETONATION         = 35152,
};

struct boss_mechano_lord_capacitus_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                       = 0,
        SAY_REFLECTIVE_MAGIC_SHIELD     = 1,
        SAY_REFLECTIVE_DAMAGE_SHIELD    = 2,
        SAY_KILL                        = 3,
        SAY_DEATH                       = 4,
    };

    static constexpr uint32 PolarityCastMs = 3000;
    // The halves stay shown until just before the next shift
    static constexpr uint32 PolarityShownMs = 26000;
    static constexpr float HalfRadius = 35.0f;

    boss_mechano_lord_capacitus_evolutions(Creature* creature) : BossAI(creature, DATA_MECHANOLORD_CAPACITUS) { }

    void Reset() override
    {
        _timers.CancelAll();
        BossAI::Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();
        Talk(SAY_AGGRO);

        scheduler.Schedule(6s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_HEADCRACK);
            context.Repeat(20s);
        }).Schedule(10s, [this](TaskContext context)
        {
            // Every 4-5 seconds (2.4-3.6 before): with a charge living 16.5 seconds, three or four are about
            uint32 const spellId = RAND(SPELL_SUMMON_NETHER_CHARGE_NE, SPELL_SUMMON_NETHER_CHARGE_NW,
                SPELL_SUMMON_NETHER_CHARGE_SE, SPELL_SUMMON_NETHER_CHARGE_SW);
            DoCastAOE(spellId);
            context.Repeat(4s, 5s);
        }).Schedule(3min, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_BERSERK, true);
        });

        if (IsHeroic())
        {
            scheduler.Schedule(15s, [this](TaskContext context)
            {
                PolarityShift();
                context.Repeat(30s);
            });
        }
        else
        {
            scheduler.Schedule(15s, [this](TaskContext context)
            {
                if (IsEvenNumber(context.GetRepeatCounter()))
                {
                    Talk(SAY_REFLECTIVE_DAMAGE_SHIELD);
                    DoCastSelf(SPELL_REFLECTIVE_DAMAGE_SHIELD);
                }
                else
                {
                    Talk(SAY_REFLECTIVE_MAGIC_SHIELD);
                    DoCastSelf(SPELL_REFLECTIVE_MAGIC_SHIELD);
                }

                context.Repeat(20s);
            });
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_KILL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _timers.CancelAll();
        _JustDied();
        Talk(SAY_DEATH);
    }

    // The charges go their own way (npc_nether_charge_evolutions): not into combat
    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void UpdateAI(uint32 diff) override
    {
        _timers.Update(diff);
        BossAI::UpdateAI(diff);
    }

private:
    // Polarity Shift, cast for real (3 seconds, it was instant and silent). Its charges and their pulses are the stock
    // ones: every 5 seconds a charged player hurts those of the other charge within 10 yards. While it casts, the room
    // is split in two halves shown in the charges' colours (particles, not red: nothing lands there), and when it
    // lands each player is told their charge and their half. Standing with your own charge is all it asks.
    void PolarityShift()
    {
        // The charges are dealt anew: the old ones go, so no one carries both
        for (Player* player : PlayersNear(me, 100.0f))
        {
            player->RemoveAurasDueToSpell(SPELL_POSITIVE_POLARITY);
            player->RemoveAurasDueToSpell(SPELL_NEGATIVE_POLARITY);
            player->RemoveAurasDueToSpell(SPELL_POSITIVE_CHARGE_STACK);
            player->RemoveAurasDueToSpell(SPELL_NEGATIVE_CHARGE_STACK);
        }

        float const axis = frand(0.0f, 2.0f * float(M_PI));
        GroundIndicators::Area positive;
        positive.kind = GroundIndicators::Area::Kind::Cone;
        positive.origin = me->GetPosition();
        positive.origin.SetOrientation(axis);
        positive.radius = HalfRadius;
        positive.arc = float(M_PI);
        GroundIndicators::Area negative = positive;
        negative.origin.SetOrientation(Position::NormalizeOrientation(axis + float(M_PI)));
        GroundIndicators::ShowParticles(me, positive, GroundIndicators::Theme::Holy, PolarityCastMs + PolarityShownMs);
        GroundIndicators::ShowParticles(me, negative, GroundIndicators::Theme::Shadow,
            PolarityCastMs + PolarityShownMs);

        DoCastSelf(SPELL_POLARITY_SHIFT);

        _timers.Schedule(Milliseconds(PolarityCastMs + 300), [this](TaskContext)
        {
            for (Player* player : PlayersNear(me, 100.0f))
            {
                if (player->HasAura(SPELL_POSITIVE_POLARITY))
                    WarnPlayer(me, player, "Positive charge: stand in the golden half with the other positives.");
                else if (player->HasAura(SPELL_NEGATIVE_POLARITY))
                    WarnPlayer(me, player, "Negative charge: stand in the purple half with the other negatives.");
            }
        });
    }

    TaskScheduler _timers;
};

// Nether Charge: wanders for 8.5 seconds, stops, pulses, and goes off 16.5 seconds after it appeared. Its circle (10
// yards, 15 before) is only shown for the last 3 seconds: the stock charge carried its 15 yards from its first second,
// and the five or six alive at once covered the whole room in red.
struct npc_nether_charge_evolutions : public ScriptedAI
{
    static constexpr float WanderRadius = 20.0f;
    static constexpr float DetonationRadius = 10.0f;
    static constexpr uint32 WarningMs = 3000;

    npc_nether_charge_evolutions(Creature* creature) : ScriptedAI(creature) { }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->AddAura(SPELL_NETHER_CHARGE_PASSIVE, me);
        me->GetMotionMaster()->MoveRandom(WanderRadius);

        scheduler.Schedule(8500ms, [this](TaskContext)
        {
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->StopMoving();
        }).Schedule(10500ms, [this](TaskContext context)
        {
            DoCastSelf(SPELL_NETHER_CHARGE_PULSE, true);
            if (context.GetRepeatCounter() < 2)
                context.Repeat(2s);
        }).Schedule(Milliseconds(16500 - WarningMs), [this](TaskContext)
        {
            _area = GroundIndicators::ShowCircle(me, me->GetPosition(), DetonationRadius, WarningMs,
                GroundIndicators::Theme::Arcane);
        }).Schedule(16500ms, [this](TaskContext)
        {
            GroundIndicators::Burst(me, _area.origin, GroundIndicators::Theme::Arcane);
            for (Player* player : PlayersIn(me, _area))
                Hit(me, player, SPELL_NETHER_DETONATION, 28.0f, 2750);
            me->DespawnOrUnsummon(1s);
        });
    }

    void AttackStart(Unit* /*who*/) override { }
    void MoveInLineOfSight(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    GroundIndicators::Area _area;
};

// ---------------------------------------------------------------------------------------------------------------------
// Nethermancer Sepethrea
enum SepethreaSpells : uint32
{
    SPELL_FROST_ATTACK              = 45196,
    SPELL_SUMMON_RAGING_FLAMES      = 35275,
    SPELL_QUELL_RAGING_FLAMES       = 35277,
    SPELL_ARCANE_BLAST              = 35314,
    SPELL_DRAGONS_BREATH            = 35250,
    SPELL_DRAGONS_BREATH_H          = 37289,

    SPELL_RAGING_FLAMES_AREA_AURA   = 35281,
    SPELL_INVIS_STEALTH_DETECTION   = 18950,
    SPELL_INFERNO                   = 35268,
};

struct boss_nethermancer_sepethrea_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                   = 0,
        SAY_SUMMON                  = 1,
        SAY_DRAGONS_BREATH          = 2,
        SAY_SLAY                    = 3,
        SAY_DEATH                   = 4,
    };

    static constexpr float BreathRadius = 12.0f;
    static constexpr float BreathArc = 60.0f;
    static constexpr uint32 BreathWarningMs = 2000;
    static constexpr float BlastRadius = 5.0f;
    static constexpr uint32 BlastWarningMs = 2000;

    boss_nethermancer_sepethrea_evolutions(Creature* creature) : BossAI(creature, DATA_NETHERMANCER_SEPRETHREA) { }

    void Reset() override
    {
        _timers.CancelAll();
        EndHold();
        BossAI::Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        scheduler.Schedule(6s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_FROST_ATTACK);
            context.Repeat(8s);
        }).Schedule(15s, 25s, [this](TaskContext context)
        {
            ArcaneBlast();
            context.Repeat();
        }).Schedule(20s, 30s, [this](TaskContext context)
        {
            DragonsBreath();
            context.Repeat(25s, 35s);
        });

        Talk(SAY_AGGRO);
        DoCastSelf(SPELL_SUMMON_RAGING_FLAMES, true);
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        _timers.CancelAll();
        EndHold();
        // As the stock script: the flames are quelled (and kill themselves on evade in their own script)
        DoCastSelf(SPELL_QUELL_RAGING_FLAMES, true);
        ScriptedAI::EnterEvadeMode(why);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (Unit* victim = me->GetVictim())
        {
            summon->AI()->AttackStart(victim);
            summon->AddThreat(victim, 1000.0f);
            summon->SetInCombatWithZone();
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _timers.CancelAll();
        EndHold();
        _JustDied();
        Talk(SAY_DEATH);
        DoCastSelf(SPELL_QUELL_RAGING_FLAMES, true);
    }

    void UpdateAI(uint32 diff) override
    {
        _timers.Update(diff);
        if (!UpdateVictim())
            return;

        events.Update(diff);
        scheduler.Update(diff);
        if (me->IsActionPreventedByCasting())
            return;

        if (!_holding)
            DoMeleeAttackIfReady();
    }

private:
    // Arcane Blast: a tank buster, marked on the tank 2 seconds before (the stock one landed with no warning, for 3%
    // of a tank). It knocks the tank back and halves its threat, as it did; anyone next to the tank is hit too.
    void ArcaneBlast()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        me->TextEmote(std::string("%s gathers arcane power against ") + victim->GetName() + "!", nullptr, true);
        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, victim, BlastRadius,
            BlastWarningMs);
        ObjectGuid const victimGuid = victim->GetGUID();
        _timers.Schedule(Milliseconds(BlastWarningMs), [this, area, victimGuid](TaskContext)
        {
            Unit* tank = ObjectAccessor::GetUnit(*me, victimGuid);
            if (!tank || !tank->IsAlive() || !me->IsInCombat())
                return;

            GroundIndicators::Burst(me, tank->GetPosition(), GroundIndicators::Theme::Arcane);
            for (Player* player : PlayersIn(me, GroundIndicators::CurrentArea(tank, area)))
                if (player != tank)
                    Hit(me, player, SPELL_ARCANE_BLAST, 25.0f, 1000);
            Hit(me, tank, SPELL_ARCANE_BLAST, 50.0f, 1000);
            tank->KnockbackFrom(me->GetPositionX(), me->GetPositionY(), 7.5f, 3.0f);
            DoModifyThreatByPercent(tank, -50);
        });
    }

    // Dragon's Breath: a cone of fire she turns to the tank and holds still for; everyone in it burns and is
    // disoriented. It was an instant 24-degree cone for 2-3%, on normal-mode values in heroic.
    void DragonsBreath()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        if (roll_chance_i(50))
            Talk(SAY_DRAGONS_BREATH);

        float const facing = me->GetAngle(victim);
        StartHold(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing, BreathRadius,
            BreathArc, BreathWarningMs, GroundIndicators::Theme::Fire);
        _timers.Schedule(Milliseconds(BreathWarningMs), [this, area](TaskContext)
        {
            uint32 const spell = me->GetMap()->IsHeroic() ? uint32(SPELL_DRAGONS_BREATH_H)
                                                          : uint32(SPELL_DRAGONS_BREATH);
            for (Player* player : PlayersIn(me, area))
            {
                Hit(me, player, spell, 26.0f, 2500);
                if (player->IsAlive())
                    me->AddAura(spell, player);
            }
            EndHold();
        });
    }

    void StartHold(float facing)
    {
        _holding = true;
        HoldStill(me, facing);
    }

    void EndHold()
    {
        if (!_holding)
            return;
        _holding = false;
        Release(me);
    }

    TaskScheduler _timers;
    bool _holding = false;
};

// Raging Flames: as the stock flames (chase a random player, leave burning ground, Inferno now and then), but the
// player chased is told, and Inferno's 10 yards are drawn 2 seconds before it starts (it was invisible: its aura
// names no spell for the indicators to read).
struct npc_raging_flames_evolutions : public ScriptedAI
{
    static constexpr float InfernoRadius = 10.0f;
    static constexpr uint32 InfernoWarningMs = 2000;
    static constexpr uint32 InfernoMs = 8000;

    npc_raging_flames_evolutions(Creature* creature) : ScriptedAI(creature) { }

    void InitializeAI() override
    {
        me->SetCorpseDelay(20);
    }

    void FixateRandomTarget()
    {
        me->GetThreatMgr().ClearAllThreat();

        if (TempSummon* summon = me->ToTempSummon())
            if (Creature* summoner = summon->GetSummonerCreatureBase())
                if (summoner->IsAIEnabled)
                {
                    if (Unit* target = summoner->AI()->SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true, false))
                    {
                        me->AddThreat(target, 1000000.0f);
                        WarnPlayer(me, target, "A Raging Flame is chasing you: keep moving, and away from the others!");
                    }
                    else
                        me->KillSelf();
                }
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        DoZoneInCombat();
        DoCastSelf(SPELL_RAGING_FLAMES_AREA_AURA);
        DoCastSelf(SPELL_INVIS_STEALTH_DETECTION);

        FixateRandomTarget();

        scheduler.Schedule(15s, 25s, [this](TaskContext task)
        {
            me->StopMoving();
            me->SetControlled(true, UNIT_STATE_ROOT);
            GroundIndicators::ShowCircle(me, me->GetPosition(), InfernoRadius, InfernoWarningMs + InfernoMs,
                GroundIndicators::Theme::Fire);
            task.Schedule(Milliseconds(InfernoWarningMs), [this](TaskContext)
            {
                me->SetControlled(false, UNIT_STATE_ROOT);
                DoCastSelf(SPELL_INFERNO);
                FixateRandomTarget();
            });
            task.Repeat(20s, 30s);
        });
    }

    void Reset() override
    {
        scheduler.CancelAll();
        me->SetControlled(false, UNIT_STATE_ROOT);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        FixateRandomTarget();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);

        DoMeleeAttackIfReady();
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// Pathaleon the Calculator
struct boss_pathaleon_the_calculator_evolutions : public BossAI
{
    enum Texts : uint8
    {
        SAY_AGGRO                   = 0,
        SAY_DOMINATION              = 1,
        SAY_SUMMON                  = 2,
        SAY_ENRAGE                  = 3,
        SAY_SLAY                    = 4,
        SAY_DEATH                   = 5,
        SAY_APPEAR                  = 6,
    };

    enum Spells : uint32
    {
        SPELL_ARCANE_EXPLOSION          = 15453,
        SPELL_DISGRUNTLED_ANGER         = 35289,
        SPELL_ARCANE_TORRENT            = 36022,
        SPELL_MANA_TAP                  = 36021,
        SPELL_DOMINATION                = 35280,
        SPELL_FRENZY                    = 36992,
        SPELL_SUICIDE                   = 35301,
        SPELL_ETHEREAL_TELEPORT         = 34427,
        SPELL_GREATER_INVISIBILITY      = 34426,
        SPELL_SUMMON_NETHER_WRAITH_1    = 35285,
    };

    // SmartAI of the bridge packs tells him each death
    static constexpr int32 ACTION_BRIDGE_MOB_DEATH = 1;
    static constexpr int8 EQUIPMENT_NORMAL = 1;
    static constexpr int8 EQUIPMENT_FRENZY = 2;

    static constexpr float ExplosionRadius = 12.0f;
    static constexpr uint32 ExplosionWarningMs = 2500;
    static constexpr float DominationRadius = 6.0f;
    static constexpr uint32 DominationWarningMs = 3000;
    static constexpr int32 DominationMs = 6000;

    boss_pathaleon_the_calculator_evolutions(Creature* creature) : BossAI(creature, DATA_PATHALEON_THE_CALCULATOR) { }

    void Reset() override
    {
        _timers.CancelAll();
        EndHold();
        _Reset();
        _isEnraged = false;
        me->LoadEquipment(EQUIPMENT_NORMAL);

        if (instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT) < 4)
            DoCastSelf(SPELL_GREATER_INVISIBILITY);
    }

    bool CanAIAttack(Unit const* /*target*/) const override
    {
        return instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT) >= 4;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _JustEngagedWith();

        ScheduleHealthCheckEvent(20, [&]()
        {
            DoCastSelf(SPELL_SUICIDE, true);
            DoCastSelf(SPELL_FRENZY, true);
            Talk(SAY_ENRAGE);
            _isEnraged = true;
            me->LoadEquipment(EQUIPMENT_FRENZY);
        });

        scheduler.Schedule(10s, 16s, [this](TaskContext context)
        {
            if (!_isEnraged)
            {
                for (uint8 i = 0; i < DUNGEON_MODE(3, 4); ++i)
                    me->CastSpell(me, SPELL_SUMMON_NETHER_WRAITH_1 + i, true);

                Talk(SAY_SUMMON);
            }
            context.Repeat(45s, 50s);
        }).Schedule(12s, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0,
                PowerUsersSelector(me, POWER_MANA, 40.0f, false)))
                DoCast(target, SPELL_MANA_TAP);
            context.Repeat(18s);
        }).Schedule(16s, [this](TaskContext context)
        {
            me->RemoveAurasDueToSpell(SPELL_MANA_TAP);
            me->ModifyPower(POWER_MANA, 5000);
            DoCastSelf(SPELL_ARCANE_TORRENT);
            context.Repeat(15s);
        }).Schedule(10s, 16s, [this](TaskContext context)
        {
            Domination();
            context.Repeat(27s, 40s);
        }).Schedule(25s, [this](TaskContext context)
        {
            DoCast(SPELL_DISGRUNTLED_ANGER);
            context.Repeat(40s, 90s);
        });

        if (IsHeroic())
        {
            scheduler.Schedule(8s, [this](TaskContext context)
            {
                ArcaneExplosion();
                context.Repeat(12s);
            });
        }

        Talk(SAY_AGGRO);
    }

    void DoAction(int32 actionId) override
    {
        if (actionId != ACTION_BRIDGE_MOB_DEATH)
            return;

        uint8 mobCount = instance->GetPersistentData(DATA_BRIDGE_MOB_DEATH_COUNT);
        instance->StorePersistentData(DATA_BRIDGE_MOB_DEATH_COUNT, ++mobCount);

        if (mobCount >= 4)
        {
            DoCastSelf(SPELL_ETHEREAL_TELEPORT);
            Talk(SAY_APPEAR);

            scheduler.Schedule(2s, [this](TaskContext)
            {
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY1H);
            }).Schedule(25s, [this](TaskContext)
            {
                DoZoneInCombat();
            });
        }
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustDied(Unit* /*killer*/) override
    {
        _timers.CancelAll();
        EndHold();
        _JustDied();
        Talk(SAY_DEATH);
    }

    void UpdateAI(uint32 diff) override
    {
        _timers.Update(diff);
        if (!UpdateVictim())
            return;

        events.Update(diff);
        scheduler.Update(diff);
        if (me->IsActionPreventedByCasting())
            return;

        if (!_holding)
            DoMeleeAttackIfReady();
    }

private:
    // Arcane Explosion: 12 yards around him, drawn 2.5 seconds before, and he holds still for it (melee step out and
    // come back). The stock one covered 30 yards: the whole melee group and more, with nowhere to go.
    void ArcaneExplosion()
    {
        StartHold(me->GetOrientation());
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), ExplosionRadius,
            ExplosionWarningMs, GroundIndicators::Theme::Arcane);
        _timers.Schedule(Milliseconds(ExplosionWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Arcane);
            for (Player* player : PlayersIn(me, area))
                Hit(me, player, SPELL_ARCANE_EXPLOSION, 45.0f, 3000);
            EndHold();
        });
    }

    // Domination: its victim is marked 3 seconds before (told, and a circle on them: keep away from them), then
    // taken for 6 seconds (10 before, with no warning)
    void Domination()
    {
        Unit* target = RandomNonTank(this, 50.0f);
        if (!target)
            return;

        Talk(SAY_DOMINATION);
        me->TextEmote(std::string("%s turns his gaze on ") + target->GetName() + "!", nullptr, true);
        WarnPlayer(me, target, "Pathaleon is about to dominate you: move away from your allies!");
        GroundIndicators::ShowCarriedCircle(me, target, DominationRadius, DominationWarningMs);
        ObjectGuid const targetGuid = target->GetGUID();
        _timers.Schedule(Milliseconds(DominationWarningMs), [this, targetGuid](TaskContext)
        {
            Unit* victim = ObjectAccessor::GetUnit(*me, targetGuid);
            if (!victim || !victim->IsAlive() || !me->IsAlive() || !me->IsInCombat())
                return;

            me->CastSpell(victim, SPELL_DOMINATION, true);
            if (Aura* aura = victim->GetAura(SPELL_DOMINATION, me->GetGUID()))
            {
                aura->SetMaxDuration(DominationMs);
                aura->SetDuration(DominationMs);
            }
        });
    }

    void StartHold(float facing)
    {
        _holding = true;
        HoldStill(me, facing);
    }

    void EndHold()
    {
        if (!_holding)
            return;
        _holding = false;
        Release(me);
    }

    TaskScheduler _timers;
    bool _isEnraged = false;
    bool _holding = false;
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

void AddMythicMechanarScripts()
{
    // Melee, towards the 6-10% of a tank a boss swing is meant to be (bosses already get x1.6 from the engine).
    // Gyro-Kill and Iron-Hand are not dungeon bosses for the engine (no encounter credit): no x1.6, boss-level melee
    // from here. At +10: Gyro-Kill 2.0% -> 6.1%, Iron-Hand 2.9% -> 7.9%, Capacitus 2.1% -> 7.9%, Pathaleon 4.3% ->
    // 8.2% (Frenzy 14% below 20%). Sepethrea's swing comes with her Frost Attack proc, raised below.
    SetMelee({ NPC_GYROKILL, NPC_GYROKILL_H }, 3.0f);
    SetMelee({ NPC_IRON_HAND, NPC_IRON_HAND_H }, 2.75f);
    SetMelee({ NPC_CAPACITUS, NPC_CAPACITUS_H }, 2.35f);
    SetMelee({ NPC_PATHALEON, NPC_PATHALEON_H }, 1.2f);

    // Sepethrea's Frost Attack proc runs on normal-mode values in heroic (the heroic 39087 is 2.8 times it): 2.9k ->
    // 7.3k a swing, her swing with it 8.4% of a tank
    MythicTuning::SetSpellMultiplier(SPELL_FROST_ATTACK_PROC, 2.5f);
    // Jackhammer: 12.5% -> 16% of a damage dealer per tick, now drawn from its cast
    MythicTuning::SetSpellMultiplier(SPELL_JACKHAMMER_EFFECT, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_JACKHAMMER_EFFECT_H, 1.3f);
    // Inferno: 8.4% -> 11% per tick, now drawn 2 seconds before its first
    MythicTuning::SetSpellMultiplier(SPELL_INFERNO_DAMAGE, 1.3f);
    // Nether Wraith's Nether Explosion: 6.3% for a drawn, avoidable 10 yards -> 22%
    MythicTuning::SetSpellMultiplier(SPELL_NETHER_EXPLOSION, 3.5f);
    // Polarity Shift's charges: 8.4% -> 12.6% for each neighbour of the other charge, every 5 seconds
    MythicTuning::SetSpellMultiplier(SPELL_POSITIVE_CHARGE_PULSE, 1.5f);
    MythicTuning::SetSpellMultiplier(SPELL_NEGATIVE_CHARGE_PULSE, 1.5f);

    // Trash: a few packs get one ability to step out of

    // Sunseeker Astromage: a Solarburn under a player (move)
    MythicTrash::Ability solarburn;
    solarburn.shape = MythicTrash::Shape::UnderTarget;
    solarburn.size = 6.0f;
    solarburn.warnMs = 2500;
    solarburn.cooldownMs = 20000;
    solarburn.firstMs = 9000;
    solarburn.percent = 25.0f;
    solarburn.spellId = SPELL_SOLARBURN;
    solarburn.theme = GroundIndicators::Theme::Fire;
    solarburn.holdStill = true;
    RegisterTrash({ NPC_SUNSEEKER_ASTROMAGE, NPC_SUNSEEKER_ASTROMAGE_H }, solarburn);

    // Sunseeker Engineer: a Death Ray locks on a player and arcs to whoever stands next to them (take it away)
    MythicTrash::Ability deathRay;
    deathRay.shape = MythicTrash::Shape::CarriedByTarget;
    deathRay.size = 8.0f;
    deathRay.warnMs = 3000;
    deathRay.cooldownMs = 22000;
    deathRay.firstMs = 10000;
    deathRay.percent = 25.0f;
    deathRay.spellId = SPELL_DEATH_RAY;
    deathRay.theme = GroundIndicators::Theme::Nature;
    deathRay.holdStill = false;
    RegisterTrash({ NPC_SUNSEEKER_ENGINEER, NPC_SUNSEEKER_ENGINEER_H }, deathRay);

    // Mechanar Driller and Wrecker: a spray of machine fluid in front of them, held still for (step aside)
    MythicTrash::Ability glob;
    glob.shape = MythicTrash::Shape::ConeAtVictim;
    glob.size = 10.0f;
    glob.width = 60.0f;
    glob.warnMs = 2500;
    glob.cooldownMs = 18000;
    glob.firstMs = 7000;
    glob.percent = 30.0f;
    glob.spellId = SPELL_GLOB_OF_MACHINE_FLUID;
    glob.theme = GroundIndicators::Theme::Nature;
    glob.holdStill = true;
    RegisterTrash({ NPC_MECHANAR_DRILLER, NPC_MECHANAR_DRILLER_H, NPC_MECHANAR_WRECKER, NPC_MECHANAR_WRECKER_H }, glob);

    RegisterCreatureAI(boss_gatewatcher_gyrokill_evolutions);
    RegisterCreatureAI(boss_gatewatcher_iron_hand_evolutions);
    RegisterCreatureAI(boss_mechano_lord_capacitus_evolutions);
    RegisterCreatureAI(npc_nether_charge_evolutions);
    RegisterCreatureAI(boss_nethermancer_sepethrea_evolutions);
    RegisterCreatureAI(npc_raging_flames_evolutions);
    RegisterCreatureAI(boss_pathaleon_the_calculator_evolutions);
}
