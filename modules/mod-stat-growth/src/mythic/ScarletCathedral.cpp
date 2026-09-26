#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "GroundIndicators.h"

#include "Containers.h"
#include "CreatureScript.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SmartAI.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include <list>
#include <vector>

// Scarlet Monastery, the Cathedral in Mythic+: damage tuning, boss reworks and trash abilities (audit:
// .agents/plans/mplus-damage-audit/).
//
// The wing's danger was its trash (Scarlet Champions' Holy Strikes, which armour does not reduce) and Mograine's pull
// of the whole chapel; its bosses were melee dummies whose only mechanics were a heal to kick, a 100-yard sleep with
// no warning and a random mind control. The bosses keep their SmartAI fight (Mograine's fall and Whitemane's
// resurrection of him, Fairbanks' gossip) and gain abilities drawn in red before they land, resolved against the area
// drawn, so bots step out of them like players. They run in the normal dungeon too, at leveling players' scale.
//
// High Inquisitor Fairbanks: Inquisitor's Brand, a circle following a player that burns whoever stands in it (take it
// away); Penance, a line at a player (step aside). His heal stays the interrupt check.
// Scarlet Commander Mograine: Crusader Strike, a cone at the tank he holds still for (step aside); Consecration, a
// circle around him that stays and burns (drag him out); Hammer Throw, a line at the player furthest from him that
// stuns (step aside). In a key he calls only the chapel's Scarlets within 30 yards, not the whole room.
// High Inquisitor Whitemane: Deep Sleep, a circle around her when she goes to raise Mograine (run out or sleep);
// Dominate Mind, a circle following a player that hurts whoever stands in it (take it away); Scarlet Judgement,
// circles under three players (move). Her Holy Smite stays the interruptible tank damage.
namespace
{
// What the normal dungeon's version of an ability deals, per percent of the mythic reference health: leveling
// players of the Cathedral have 2 500-4 000 health, so a big hit there stays about a third of it
constexpr float NormalReferenceHealth = 1500.0f;
constexpr float PlayerRange = 80.0f;

// Only named in the combat log: the damage is dealt by the scripts, on the areas they drew
enum Spells
{
    SPELL_HOLY_FIRE             = 15264,
    SPELL_PENANCE               = 47666,
    SPELL_CRUSADER_STRIKE_HOLY  = 14517,    // the holy one; Mograine's own (14518) is physical
    SPELL_CONSECRATION          = 20922,
    SPELL_HAMMER_OF_WRATH       = 24275,
    SPELL_HAMMER_OF_JUSTICE     = 5589,     // its stun, on whoever the hammer hits
    SPELL_DEEP_SLEEP            = 9256,     // its sleep, on whoever stays in the circle
    SPELL_DOMINATE_MIND         = 14515,
    SPELL_JUDGEMENT             = 23590,
    SPELL_SCARLET_RESURRECTION  = 9232,     // Whitemane's; keys the immunities that keep it from being interrupted
    SPELL_CLEAVE                = 40505,

    // Tuned stock spells (the audit's +10 numbers, with the level catch-up the core does itself taken out)
    SPELL_HOLY_STRIKE           = 17143,
    SPELL_HOLY_SMITE            = 9481,
    SPELL_HEAL                  = 12039,
    SPELL_FROSTBOLT             = 9672,
    SPELL_ARCANE_EXPLOSION      = 8439,
    SPELL_BLIZZARD              = 8364,
};

enum Creatures
{
    NPC_SCARLET_MYRMIDON        = 4295,
    NPC_SCARLET_CHAPLAIN        = 4299,
    NPC_SCARLET_WIZARD          = 4300,
    NPC_SCARLET_CENTURION       = 4301,
    NPC_SCARLET_CHAMPION        = 4302,
    NPC_SCARLET_ABBOT           = 4303,
    NPC_SCARLET_MONK            = 4540,
};

// instance_scarlet_monastery: TYPE_MOGRAINE_AND_WHITE_EVENT. Set IN_PROGRESS it sends every Scarlet of the chapel
// within 80 yards of Mograine at his target.
constexpr uint32 DATA_MOGRAINE_AND_WHITE_EVENT = 1;
// In a key Mograine calls only these, within this range: the room left alive brought 9 Champions at once
constexpr float MythicCallRange = 30.0f;

// Holy Strike in a key: the Champions' weapon strike in the holy school, which armour does not reduce, a sixth of the
// wing's tank damage at every pew
constexpr int32 HolyStrikeMythicPercent = 50;

// The part every reworked boss here shares: who is around, who stands in an area, how hard it hits, and the wind-up
// of an ability that comes out of the boss (from the warning to the moment it lands it stays where it is, facing it)
class Telegraphs
{
public:
    explicit Telegraphs(Creature* me) : _me(me) { }

    std::vector<Player*> Players(float range = PlayerRange) const
    {
        std::vector<Player*> players;
        for (auto const& ref : _me->GetMap()->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->IsAlive() && !player->IsGameMaster() && _me->IsWithinDistInMap(player, range) &&
                    _me->IsValidAttackTarget(player))
                    players.push_back(player);
        return players;
    }

    std::vector<Player*> PlayersIn(GroundIndicators::Area const& area) const
    {
        std::vector<Player*> inside;
        for (Player* player : Players())
            if (area.Contains(*player))
                inside.push_back(player);
        return inside;
    }

    // A random player other than the one it fights; that one only when nobody else is there
    Player* RandomPlayer(float range, bool allowVictim = true) const
    {
        Unit const* victim = _me->GetVictim();
        std::vector<Player*> const all = Players(range);
        std::vector<Player*> others;
        for (Player* player : all)
            if (player != victim)
                others.push_back(player);
        if (!others.empty())
            return Acore::Containers::SelectRandomContainerElement(others);
        return allowVictim && !all.empty() ? all.front() : nullptr;
    }

    // Up to count different players, at random
    std::vector<Player*> RandomPlayers(float range, uint32 count) const
    {
        std::vector<Player*> players = Players(range);
        Acore::Containers::RandomResize(players, count);
        return players;
    }

    Player* FarthestPlayer(float range) const
    {
        Player* farthest = nullptr;
        for (Player* player : Players(range))
            if (!farthest || _me->GetExactDist2d(player) > _me->GetExactDist2d(farthest))
                farthest = player;
        return farthest;
    }

    // A share of the reference health in a key; the same share of a leveling player's health in the normal dungeon.
    // Defensives, resistances, armour for a physical one, and absorbs apply.
    void Hit(Unit* target, uint32 spellId, float percent) const
    {
        if (_me->GetMap()->IsMythic())
            MythicTuning::DealReferenceDamage(_me, target, spellId, percent);
        else
            MythicTuning::DealAbilityDamage(_me, target, spellId,
                static_cast<uint32>(NormalReferenceHealth * percent / 100.0f));
    }

    std::vector<Player*> HitIn(GroundIndicators::Area const& area, uint32 spellId, float percent) const
    {
        std::vector<Player*> const inside = PlayersIn(area);
        for (Player* player : inside)
            Hit(player, spellId, percent);
        return inside;
    }

    // An aura of the spell on the target for durationMs (a stun or a sleep for those caught)
    void Afflict(Unit* target, uint32 spellId, int32 durationMs = 0) const
    {
        if (Aura* aura = _me->AddAura(spellId, target))
            if (durationMs > 0)
            {
                aura->SetMaxDuration(durationMs);
                aura->SetDuration(durationMs);
            }
    }

    void BeginWindup(float facing)
    {
        _windup = true;
        _me->StopMoving();
        _me->SetControlled(true, UNIT_STATE_ROOT);
        _me->SetTarget();
        _me->SetFacingTo(facing);
    }

    void EndWindup()
    {
        if (!_windup)
            return;

        _windup = false;
        _me->SetControlled(false, UNIT_STATE_ROOT);
        if (Unit* victim = _me->GetVictim())
            _me->SetTarget(victim->GetGUID());
    }

    bool WindingUp() const { return _windup; }

private:
    Creature* _me;
    bool _windup = false;
};

// A boss whose stock SmartAI (yells, instance data, Mograine's fall and resurrection) keeps running, with the
// telegraphed abilities of its rework on top. The creature keeps AIName SmartAI and gets the rework's ScriptName.
// Nothing new begins while the SmartAI holds it out of the fight (passive, unselectable, casting).
struct TelegraphingSmartAI : public SmartAI
{
    explicit TelegraphingSmartAI(Creature* creature) : SmartAI(creature), tele(creature) { }

    void UpdateAI(uint32 diff) override
    {
        if (!me->IsAlive() || !me->IsInCombat())
        {
            StopAbilities();
            SmartAI::UpdateAI(diff);
            return;
        }

        if (!_started)
        {
            _started = true;
            ScheduleAbilities();
        }

        // No swing while it winds an ability up
        SetAutoAttack(!tele.WindingUp());
        SmartAI::UpdateAI(diff);
        if (!me->IsAlive() || !me->IsInCombat())
            return;

        scheduler.Update(diff);
        events.Update(diff);
        OnCombatUpdate();
        if (!me->GetVictim() || me->HasUnitState(UNIT_STATE_CASTING) || me->GetReactState() == REACT_PASSIVE ||
            me->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            // One wind-up at a time
            if (tele.WindingUp())
            {
                events.ScheduleEvent(eventId, 1s);
                continue;
            }
            ExecuteAbility(eventId);
        }
    }

protected:
    virtual void ScheduleAbilities() = 0;
    virtual void ExecuteAbility(uint32 eventId) = 0;
    // Every update in combat, whatever state the SmartAI holds it in
    virtual void OnCombatUpdate() { }
    virtual void OnStop() { }

    void StopAbilities()
    {
        if (!_started)
            return;

        _started = false;
        events.Reset();
        scheduler.CancelAll();
        tele.EndWindup();
        SetAutoAttack(true);
        OnStop();
    }

    Telegraphs tele;

private:
    bool _started = false;
};

// ---------------------------------------------------------------------------------------------------------------
// High Inquisitor Fairbanks
// ---------------------------------------------------------------------------------------------------------------
enum FairbanksEvents
{
    EVENT_INQUISITORS_BRAND = 1,
    EVENT_PENANCE,
};

constexpr float BrandRadius = 6.0f;
constexpr uint32 BrandMs = 6000;
constexpr float BrandPercent = 45.0f;           // to everyone else still in it
constexpr float BrandCarrierPercent = 20.0f;
constexpr float PenanceLength = 30.0f;
constexpr float PenanceWidth = 5.0f;
constexpr uint32 PenanceWarningMs = 2500;
constexpr float PenancePercent = 55.0f;

struct boss_high_inquisitor_fairbanks_evolutions : public TelegraphingSmartAI
{
    explicit boss_high_inquisitor_fairbanks_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        events.ScheduleEvent(EVENT_PENANCE, 6s);
        events.ScheduleEvent(EVENT_INQUISITORS_BRAND, 10s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_INQUISITORS_BRAND:
                InquisitorsBrand();
                events.ScheduleEvent(EVENT_INQUISITORS_BRAND, 18s);
                break;
            case EVENT_PENANCE:
                Penance();
                events.ScheduleEvent(EVENT_PENANCE, 14s);
                break;
            default:
                break;
        }
    }

private:
    // A circle following a player other than the tank; it burns whoever else is still in it when it ends
    void InquisitorsBrand()
    {
        Player* carrier = tele.RandomPlayer(40.0f, false);
        if (!carrier)
            return;

        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, BrandRadius, BrandMs);
        ObjectGuid const carrierGuid = carrier->GetGUID();
        scheduler.Schedule(Milliseconds(BrandMs), [this, area, carrierGuid](TaskContext)
        {
            Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            GroundIndicators::Burst(me, carrier->GetPosition(), GroundIndicators::Theme::Holy);
            tele.Hit(carrier, SPELL_HOLY_FIRE, BrandCarrierPercent);
            for (Player* player : tele.PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                if (player != carrier)
                    tele.Hit(player, SPELL_HOLY_FIRE, BrandPercent);
        });
    }

    // A line at a player other than the tank; he holds still for it
    void Penance()
    {
        Player* target = tele.RandomPlayer(35.0f);
        if (!target)
            return;

        float const facing = me->GetAngle(target);
        tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing,
            PenanceLength, PenanceWidth, PenanceWarningMs, GroundIndicators::Theme::Holy);
        scheduler.Schedule(Milliseconds(PenanceWarningMs), [this, area](TaskContext)
        {
            tele.HitIn(area, SPELL_PENANCE, PenancePercent);
            tele.EndWindup();
        });
    }
};

// ---------------------------------------------------------------------------------------------------------------
// Scarlet Commander Mograine (his SmartAI keeps his fall, Whitemane's call and his resurrection; the call of the
// chapel is made here: the whole room in the normal dungeon, as before, the Scarlets within 30 yards in a key)
// ---------------------------------------------------------------------------------------------------------------
enum MograineEvents
{
    EVENT_CRUSADER_STRIKE = 1,
    EVENT_CONSECRATION,
    EVENT_HAMMER_THROW,
};

constexpr float CrusaderStrikeRadius = 8.0f;
constexpr float CrusaderStrikeArc = 60.0f;
constexpr uint32 CrusaderStrikeWarningMs = 2000;
constexpr float CrusaderStrikePercent = 58.0f;  // holy: two fifths of a tank that stays in it
constexpr float ConsecrationRadius = 10.0f;
constexpr uint32 ConsecrationWarningMs = 2500;
constexpr uint32 ConsecrationTicks = 8;         // one a second after it lands
constexpr float ConsecrationPercent = 16.0f;    // when it lands
constexpr float ConsecrationTickPercent = 4.0f;
constexpr float HammerLength = 30.0f;
constexpr float HammerWidth = 5.0f;
constexpr uint32 HammerWarningMs = 2500;
constexpr float HammerPercent = 45.0f;
constexpr int32 HammerStunMs = 3000;

struct boss_scarlet_commander_mograine_evolutions : public TelegraphingSmartAI
{
    explicit boss_scarlet_commander_mograine_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        scheduler.Schedule(1s, [this](TaskContext)
        {
            CallTheChapel();
        });
        events.ScheduleEvent(EVENT_CRUSADER_STRIKE, 6s);
        events.ScheduleEvent(EVENT_HAMMER_THROW, 9s);
        events.ScheduleEvent(EVENT_CONSECRATION, 12s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_CRUSADER_STRIKE:
                CrusaderStrike();
                events.ScheduleEvent(EVENT_CRUSADER_STRIKE, 12s);
                break;
            case EVENT_CONSECRATION:
                Consecration();
                events.ScheduleEvent(EVENT_CONSECRATION, 20s);
                break;
            case EVENT_HAMMER_THROW:
                HammerThrow();
                events.ScheduleEvent(EVENT_HAMMER_THROW, 16s);
                break;
            default:
                break;
        }
    }

private:
    // The stock call (SmartAI "In Combat - Set Instance Data 1 to 1", moved here) sends the whole chapel at his
    // target; a key only brings the Scarlets standing near him
    void CallTheChapel()
    {
        if (!me->GetMap()->IsMythic())
        {
            if (InstanceScript* instance = me->GetInstanceScript())
                instance->SetData(DATA_MOGRAINE_AND_WHITE_EVENT, IN_PROGRESS);
            return;
        }

        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        std::list<Creature*> scarlets;
        me->GetCreatureListWithEntryInGrid(scarlets, std::vector<uint32>{ NPC_SCARLET_MONK, NPC_SCARLET_ABBOT,
            NPC_SCARLET_CHAMPION, NPC_SCARLET_CENTURION, NPC_SCARLET_WIZARD, NPC_SCARLET_CHAPLAIN }, MythicCallRange);
        for (Creature* scarlet : scarlets)
            if (scarlet->IsAlive() && !scarlet->IsInCombat() && scarlet->AI())
                scarlet->AI()->AttackStart(victim);
    }

    // A cone at the tank, in the holy school; he holds still for it, so a step aside is a step out
    void CrusaderStrike()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        float const facing = me->GetAngle(victim);
        tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing,
            CrusaderStrikeRadius, CrusaderStrikeArc, CrusaderStrikeWarningMs, GroundIndicators::Theme::Holy);
        scheduler.Schedule(Milliseconds(CrusaderStrikeWarningMs), [this, area](TaskContext)
        {
            tele.HitIn(area, SPELL_CRUSADER_STRIKE_HOLY, CrusaderStrikePercent);
            tele.EndWindup();
        });
    }

    // A circle around him that lands, then stays and burns every second: the tank drags him out of it
    void Consecration()
    {
        uint32 const durationMs = ConsecrationWarningMs + ConsecrationTicks * 1000;
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), ConsecrationRadius,
            durationMs, GroundIndicators::Theme::Holy);
        scheduler.Schedule(Milliseconds(ConsecrationWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Holy);
            tele.HitIn(area, SPELL_CONSECRATION, ConsecrationPercent);
        });
        for (uint32 tick = 1; tick <= ConsecrationTicks; ++tick)
            scheduler.Schedule(Milliseconds(ConsecrationWarningMs + tick * 1000), [this, area](TaskContext)
            {
                tele.HitIn(area, SPELL_CONSECRATION, ConsecrationTickPercent);
            });
    }

    // A line at whoever stands furthest from him; the hammer stuns those it hits
    void HammerThrow()
    {
        Player* target = tele.FarthestPlayer(40.0f);
        if (!target)
            return;

        float const facing = me->GetAngle(target);
        tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing,
            HammerLength, HammerWidth, HammerWarningMs, GroundIndicators::Theme::Holy);
        scheduler.Schedule(Milliseconds(HammerWarningMs), [this, area](TaskContext)
        {
            for (Player* player : tele.HitIn(area, SPELL_HAMMER_OF_WRATH, HammerPercent))
                tele.Afflict(player, SPELL_HAMMER_OF_JUSTICE, HammerStunMs);
            tele.EndWindup();
        });
    }
};

// ---------------------------------------------------------------------------------------------------------------
// High Inquisitor Whitemane (her SmartAI's Deep Sleep and Dominate Mind are removed; they are made here)
// ---------------------------------------------------------------------------------------------------------------
enum WhitemaneEvents
{
    EVENT_SCARLET_JUDGEMENT = 1,
    EVENT_DOMINATE_MIND,
};

constexpr float DeepSleepRadius = 20.0f;
constexpr uint32 DeepSleepWarningMs = 3000;
constexpr int32 DeepSleepMs = 6000;
// Mograine is raised about 9 seconds after she goes to him; nothing interrupts her until she fights again
constexpr uint32 ResurrectionGuardMs = 15000;
constexpr float DominateRadius = 6.0f;
constexpr uint32 DominateMs = 4000;
constexpr float DominatePercent = 40.0f;        // to everyone else still in it
constexpr float DominateCarrierPercent = 15.0f;
constexpr float JudgementRadius = 6.0f;
constexpr uint32 JudgementWarningMs = 3000;
constexpr uint32 JudgementCount = 3;
constexpr float JudgementPercent = 40.0f;

struct boss_high_inquisitor_whitemane_evolutions : public TelegraphingSmartAI
{
    explicit boss_high_inquisitor_whitemane_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        _slept = false;
        _risen = false;
        events.ScheduleEvent(EVENT_SCARLET_JUDGEMENT, 8s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_SCARLET_JUDGEMENT:
                ScarletJudgement();
                events.ScheduleEvent(EVENT_SCARLET_JUDGEMENT, 16s);
                break;
            case EVENT_DOMINATE_MIND:
                DominateMind();
                events.ScheduleEvent(EVENT_DOMINATE_MIND, 25s);
                break;
            default:
                break;
        }
    }

    // Her SmartAI stops her at half health to raise Mograine (passive), then sends her back into the fight
    void OnCombatUpdate() override
    {
        if (!_slept && me->GetReactState() == REACT_PASSIVE && me->HealthBelowPct(51))
        {
            _slept = true;
            DeepSleep();
        }
        else if (_slept && !_risen && me->GetReactState() != REACT_PASSIVE)
        {
            _risen = true;
            GuardResurrection(false);
            events.ScheduleEvent(EVENT_DOMINATE_MIND, 12s);
        }
    }

    void OnStop() override
    {
        GuardResurrection(false);
    }

private:
    // A circle around her as she goes to Mograine: whoever is still in it sleeps
    void DeepSleep()
    {
        GuardResurrection(true);
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), DeepSleepRadius,
            DeepSleepWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(DeepSleepWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Shadow);
            for (Player* player : tele.PlayersIn(area))
                tele.Afflict(player, SPELL_DEEP_SLEEP, DeepSleepMs);
        });
        scheduler.Schedule(Milliseconds(ResurrectionGuardMs), [this](TaskContext)
        {
            GuardResurrection(false);
        });
    }

    // Those who ran out of the sleep could otherwise kick her Scarlet Resurrection, and Mograine would stay down,
    // unselectable, for the rest of the fight
    void GuardResurrection(bool apply)
    {
        if (_guarded == apply)
            return;

        _guarded = apply;
        me->ApplySpellImmune(SPELL_SCARLET_RESURRECTION, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, apply);
        me->ApplySpellImmune(SPELL_SCARLET_RESURRECTION, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, apply);
        me->ApplySpellImmune(SPELL_SCARLET_RESURRECTION, IMMUNITY_MECHANIC, MECHANIC_SILENCE, apply);
    }

    // A circle following a player other than the tank; it hurts whoever else is still in it when it ends
    void DominateMind()
    {
        Player* carrier = tele.RandomPlayer(40.0f, false);
        if (!carrier)
            return;

        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, DominateRadius,
            DominateMs);
        ObjectGuid const carrierGuid = carrier->GetGUID();
        scheduler.Schedule(Milliseconds(DominateMs), [this, area, carrierGuid](TaskContext)
        {
            Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            GroundIndicators::Burst(me, carrier->GetPosition(), GroundIndicators::Theme::Shadow);
            tele.Hit(carrier, SPELL_DOMINATE_MIND, DominateCarrierPercent);
            for (Player* player : tele.PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                if (player != carrier)
                    tele.Hit(player, SPELL_DOMINATE_MIND, DominatePercent);
        });
    }

    // Circles under three players, where they stand now
    void ScarletJudgement()
    {
        for (Player* player : tele.RandomPlayers(40.0f, JudgementCount))
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, player->GetPosition(),
                JudgementRadius, JudgementWarningMs, GroundIndicators::Theme::Holy);
            scheduler.Schedule(Milliseconds(JudgementWarningMs), [this, area](TaskContext)
            {
                GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Holy);
                tele.HitIn(area, SPELL_JUDGEMENT, JudgementPercent);
            });
        }
    }

    bool _slept = false;
    bool _risen = false;
    bool _guarded = false;
};

// Holy Strike (Scarlet Champions): half as hard in a key. A weapon spell: the spell multipliers never reach it.
class spell_mythic_cathedral_holy_strike : public SpellScript
{
    PrepareSpellScript(spell_mythic_cathedral_holy_strike);

    void HandleHit()
    {
        Unit* caster = GetCaster();
        Map const* map = caster ? caster->FindMap() : nullptr;
        if (map && map->IsMythic())
            SetHitDamage(GetHitDamage() * HolyStrikeMythicPercent / 100);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_mythic_cathedral_holy_strike::HandleHit);
    }
};

void RegisterTuning()
{
    // Every multiplier below is for +10 against the reference health (90k), after the engine's level fix: spells the
    // core already levels (creature-level attribute or points per level) now land at a tenth of the audit's numbers.
    MythicTuning::SetSpellMultiplier(SPELL_HOLY_SMITE, 8.0f);        // 1.2k -> 9.7k (7.5% of a tank)
    MythicTuning::SetSpellMultiplier(SPELL_HEAL, 3.0f);              // 9.7k -> the 3% cap: the heal to interrupt
    MythicTuning::SetSpellMultiplier(SPELL_FROSTBOLT, 8.0f);         // 0.8k -> 6.5k (5% of a tank)
    // Arcane Explosion's points per level stop at 43: it gets the classic catch-up, -> 8.8k (10%)
    MythicTuning::SetSpellMultiplier(SPELL_ARCANE_EXPLOSION, 2.0f);
    MythicTuning::SetSpellMultiplier(SPELL_BLIZZARD, 6.0f);          // 1k -> 5.8k a tick, 29k if stood in (32%)

    // Monks' Thrash (three swings at once) made the pew packs' spikes
    MythicTuning::SetMeleeMultiplier(NPC_SCARLET_MONK, 0.85f);
}

void RegisterTrash()
{
    // Scarlet Chaplains call holy fire under a player
    MythicTrash::Ability holyFire;
    holyFire.entry = NPC_SCARLET_CHAPLAIN;
    holyFire.shape = MythicTrash::Shape::UnderTarget;
    holyFire.size = 6.0f;
    holyFire.warnMs = 2500;
    holyFire.cooldownMs = 22000;
    holyFire.firstMs = 8000;
    holyFire.percent = 22.0f;
    holyFire.spellId = SPELL_HOLY_FIRE;
    holyFire.theme = GroundIndicators::Theme::Holy;
    holyFire.holdStill = false;
    MythicTrash::Register(holyFire);

    // Scarlet Myrmidons sweep their blade in front of them
    MythicTrash::Ability sweep;
    sweep.entry = NPC_SCARLET_MYRMIDON;
    sweep.shape = MythicTrash::Shape::ConeAtVictim;
    sweep.size = 8.0f;
    sweep.width = 90.0f;
    sweep.warnMs = 2000;
    sweep.cooldownMs = 20000;
    sweep.firstMs = 7000;
    sweep.percent = 20.0f;
    sweep.spellId = SPELL_CLEAVE;
    MythicTrash::Register(sweep);
}
}

void AddMythicScarletCathedralScripts()
{
    RegisterTuning();
    RegisterTrash();

    RegisterCreatureAI(boss_high_inquisitor_fairbanks_evolutions);
    RegisterCreatureAI(boss_scarlet_commander_mograine_evolutions);
    RegisterCreatureAI(boss_high_inquisitor_whitemane_evolutions);
    RegisterSpellScript(spell_mythic_cathedral_holy_strike);
}
