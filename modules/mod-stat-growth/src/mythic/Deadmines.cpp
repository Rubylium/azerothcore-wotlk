#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "GroundIndicators.h"

#include "Containers.h"
#include "CreatureScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SmartAI.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include <algorithm>
#include <list>
#include <vector>

// The Deadmines in Mythic+: damage tuning, boss reworks and trash abilities (audit: .agents/plans/mplus-damage-audit/).
//
// The classic bosses were melee dummies: the few abilities they had were stuns the tank is immune to, a random fear
// or a harpoon nobody saw coming. Each boss below keeps its fight (SmartAI yells, doors, VanCleef's allies, Mr.
// Smite's weapon changes) and gains abilities drawn in red before they land, resolved against the area drawn, so
// bots step out of them like players. They run in the normal dungeon too, at leveling players' scale.
//
// Rhahk'Zor: Ground Slam, a cone at the tank he holds still for (step aside); Boulder Toss, a circle under the player
// furthest from him (move).
// Gilnid: Molten Splash, circles under two players (move); his Molten Metal stays the interruptible tank DoT.
// Mr. Smite: at 67% and 34% Smite Stomp, a circle around him (run out or be stunned), then his walk to the chest;
// with two swords Blade Whirl, a circle around him while he spins in place (melee out); with the mace Smite Slam, a
// line at the tank (step aside).
// Captain Greenskin: Harpoon, a line at a player that poisons (step aside); Cleave, a cone in front of him.
// Edwin VanCleef: VanCleef's Mark, a circle following a player that hurts whoever stands in it (take it away); Fan of
// Knives, a circle around him (melee out); Shadowstep Strike, a line at the player furthest from him (step aside).
namespace
{
// What the normal dungeon's version of an ability deals, per percent of the mythic reference health: leveling
// players of the Deadmines have 1 000-1 800 health, so a big hit there stays about a third of it
constexpr float NormalReferenceHealth = 700.0f;
constexpr float PlayerRange = 80.0f;

// Only named in the combat log: the damage is dealt by the scripts, on the areas they drew
enum Spells
{
    SPELL_RHAHKZOR_SLAM         = 6304,
    SPELL_BOULDER               = 9483,
    SPELL_MOLTEN_BLAST          = 15040,
    SPELL_SMITE_STOMP           = 6432,     // also its stun, on whoever stays in the circle
    SPELL_SMITE_SLAM            = 6435,
    SPELL_WHIRLWIND             = 26686,
    SPELL_POISONED_HARPOON      = 5208,     // also its poison, on whoever the harpoon hits
    SPELL_CLEAVE                = 40505,
    SPELL_MARK_OF_DEATH         = 37125,
    SPELL_FAN_OF_KNIVES         = 61739,
    SPELL_SHADOWSTEP            = 41176,
    SPELL_THROW_DYNAMITE        = 7978,
    SPELL_FROST_NOVA            = 11831,

    // Tuned stock spells (the audit's +10 numbers, with the level catch-up the core does itself taken out)
    SPELL_FLAMESTRIKE           = 11829,
    SPELL_FIREBALL              = 9053,
    SPELL_SHOOT                 = 6660,
    SPELL_FIRE_BLAST            = 2138,
    SPELL_ACID_SPLASH           = 6306,
    SPELL_MOLTEN_METAL          = 5213,
    SPELL_PIERCE_ARMOR          = 6016,
    SPELL_PIERCE_ARMOR_JOHNSON  = 12097,
};

enum Creatures
{
    NPC_SNEEDS_SHREDDER         = 642,
    NPC_RHAHKZOR                = 644,
    NPC_GOBLIN_WOODCARVER       = 641,
    NPC_GOBLIN_CRAFTSMAN        = 1731,
    NPC_DEFIAS_SQUALLSHAPER     = 1732,
};

// Pierce Armor in a key: the miners re-apply it all through the mine, and at -50% (-75% for Miner Johnson) the tank
// took half again (twice) as much from every swing, with nothing to see
constexpr int32 PierceArmorMythicPercent = 30;

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

    // An aura of the spell on the target for durationMs (a stun for those caught, a poison for those hit)
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

// A boss whose stock SmartAI (yells, doors, instance data, adds) keeps running, with the telegraphed abilities of
// its rework on top. The creature keeps AIName SmartAI and gets the rework's ScriptName.
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

    void StopAbilities()
    {
        if (!_started)
            return;

        _started = false;
        events.Reset();
        scheduler.CancelAll();
        tele.EndWindup();
        SetAutoAttack(true);
    }

    Telegraphs tele;

private:
    bool _started = false;
};

// ---------------------------------------------------------------------------------------------------------------
// Rhahk'Zor
// ---------------------------------------------------------------------------------------------------------------
enum RhahkZorEvents
{
    EVENT_GROUND_SLAM = 1,
    EVENT_BOULDER_TOSS,
};

constexpr float GroundSlamRadius = 12.0f;
constexpr float GroundSlamArc = 90.0f;
constexpr uint32 GroundSlamWarningMs = 2500;
constexpr float GroundSlamPercent = 80.0f;      // physical: about a fifth of a tank that stays in it
constexpr float BoulderRadius = 6.0f;
constexpr uint32 BoulderWarningMs = 3000;
constexpr float BoulderPercent = 45.0f;

struct boss_rhahkzor_evolutions : public TelegraphingSmartAI
{
    explicit boss_rhahkzor_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        events.ScheduleEvent(EVENT_GROUND_SLAM, 8s);
        events.ScheduleEvent(EVENT_BOULDER_TOSS, 12s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_GROUND_SLAM:
                GroundSlam();
                events.ScheduleEvent(EVENT_GROUND_SLAM, 15s);
                break;
            case EVENT_BOULDER_TOSS:
                BoulderToss();
                events.ScheduleEvent(EVENT_BOULDER_TOSS, 18s);
                break;
            default:
                break;
        }
    }

private:
    // A cone at the tank; he holds still for it, so a step aside is a step out
    void GroundSlam()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        float const facing = me->GetAngle(victim);
        tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing,
            GroundSlamRadius, GroundSlamArc, GroundSlamWarningMs);
        scheduler.Schedule(Milliseconds(GroundSlamWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::None);
            tele.HitIn(area, SPELL_RHAHKZOR_SLAM, GroundSlamPercent);
            tele.EndWindup();
        });
    }

    // A circle under whoever stands furthest from him
    void BoulderToss()
    {
        Player* target = tele.FarthestPlayer(40.0f);
        if (!target)
            return;

        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, target->GetPosition(), BoulderRadius,
            BoulderWarningMs);
        scheduler.Schedule(Milliseconds(BoulderWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::None);
            tele.HitIn(area, SPELL_BOULDER, BoulderPercent);
        });
    }
};

// ---------------------------------------------------------------------------------------------------------------
// Gilnid
// ---------------------------------------------------------------------------------------------------------------
enum GilnidEvents
{
    EVENT_MOLTEN_SPLASH = 1,
};

constexpr float MoltenSplashRadius = 5.0f;
constexpr uint32 MoltenSplashWarningMs = 3000;
constexpr uint32 MoltenSplashCount = 2;
constexpr float MoltenSplashPercent = 45.0f;

struct boss_gilnid_evolutions : public TelegraphingSmartAI
{
    explicit boss_gilnid_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        events.ScheduleEvent(EVENT_MOLTEN_SPLASH, 8s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        if (eventId != EVENT_MOLTEN_SPLASH)
            return;

        MoltenSplash();
        events.ScheduleEvent(EVENT_MOLTEN_SPLASH, 16s);
    }

private:
    // Circles under two players, where they stand now
    void MoltenSplash()
    {
        for (Player* player : tele.RandomPlayers(40.0f, MoltenSplashCount))
        {
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, player->GetPosition(),
                MoltenSplashRadius, MoltenSplashWarningMs, GroundIndicators::Theme::Fire);
            scheduler.Schedule(Milliseconds(MoltenSplashWarningMs), [this, area](TaskContext)
            {
                GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Fire);
                tele.HitIn(area, SPELL_MOLTEN_BLAST, MoltenSplashPercent);
            });
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------
// Mr. Smite (replaces boss_mr_smite; keeps its weapon changes at his chest)
// ---------------------------------------------------------------------------------------------------------------
enum SmiteMisc
{
    EQUIP_SWORD             = 1,
    EQUIP_TWO_SWORDS        = 2,
    EQUIP_MACE              = 3,

    SAY_SWAP1               = 2,
    SAY_SWAP2               = 3,
};

enum SmiteEvents
{
    EVENT_BLADE_WHIRL = 1,
    EVENT_SMITE_SLAM,
    EVENT_SWAP_WEAPON,
    EVENT_RESTORE_COMBAT,
};

enum SmiteStages
{
    STAGE_SWORD,
    STAGE_TO_TWO_SWORDS,    // stomping, then walking to his chest
    STAGE_TWO_SWORDS,
    STAGE_TO_MACE,
    STAGE_MACE,
};

Position const SmiteChest = { 1.859f, -780.72f, 9.831f, 0.0f };
constexpr float SmiteChestFacing = 5.558f;

constexpr float StompRadius = 12.0f;
constexpr uint32 StompWarningMs = 3000;
constexpr int32 StompStunMs = 4000;
constexpr float StompPercent = 50.0f;
constexpr float WhirlRadius = 8.0f;
constexpr uint32 WhirlGraceMs = 2000;      // the circle shows this long before the blades hurt
constexpr uint32 WhirlTicks = 5;
constexpr float WhirlTickPercent = 9.0f;
constexpr float SlamLength = 25.0f;
constexpr float SlamWidth = 6.0f;
constexpr uint32 SlamWarningMs = 3000;
constexpr float SlamPercent = 90.0f;       // physical: about a fifth of a tank that stays in it

struct boss_mr_smite_evolutions : public ScriptedAI
{
    explicit boss_mr_smite_evolutions(Creature* creature) : ScriptedAI(creature), _tele(creature) { }

    void Reset() override
    {
        events.Reset();
        scheduler.CancelAll();
        _tele.EndWindup();
        _stage = STAGE_SWORD;
        me->LoadEquipment(EQUIP_SWORD);
        me->SetCanDualWield(false);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        me->SetReactState(REACT_AGGRESSIVE);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        events.Update(diff);

        if (_stage == STAGE_SWORD && me->HealthBelowPct(67))
            Stomp(STAGE_TO_TWO_SWORDS);
        else if (_stage == STAGE_TWO_SWORDS && me->HealthBelowPct(34))
            Stomp(STAGE_TO_MACE);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            if (_tele.WindingUp() && (eventId == EVENT_BLADE_WHIRL || eventId == EVENT_SMITE_SLAM))
            {
                events.ScheduleEvent(eventId, 1s);
                continue;
            }
            ExecuteEvent(eventId);
        }

        if (!_tele.WindingUp() && !me->HasUnitFlag(UNIT_FLAG_PACIFIED))
            DoMeleeAttackIfReady();
    }

    void MovementInform(uint32 type, uint32 point) override
    {
        if (type != POINT_MOTION_TYPE || (point != EQUIP_TWO_SWORDS && point != EQUIP_MACE))
            return;

        me->SetTarget();
        me->SetFacingTo(SmiteChestFacing);
        me->SendMeleeAttackStop(me->GetVictim());
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
        events.ScheduleEvent(EVENT_SWAP_WEAPON, 1500ms);
        events.ScheduleEvent(EVENT_RESTORE_COMBAT, 3s);
    }

private:
    void ExecuteEvent(uint32 eventId)
    {
        switch (eventId)
        {
            case EVENT_BLADE_WHIRL:
                BladeWhirl();
                events.ScheduleEvent(EVENT_BLADE_WHIRL, 20s);
                break;
            case EVENT_SMITE_SLAM:
                SmiteSlam();
                events.ScheduleEvent(EVENT_SMITE_SLAM, 12s);
                break;
            case EVENT_SWAP_WEAPON:
                if (_stage == STAGE_TO_TWO_SWORDS)
                {
                    me->LoadEquipment(EQUIP_TWO_SWORDS);
                    me->SetCanDualWield(true);
                }
                else
                {
                    me->LoadEquipment(EQUIP_MACE);
                    me->SetCanDualWield(false);
                }
                break;
            case EVENT_RESTORE_COMBAT:
                RestoreCombat();
                break;
            default:
                break;
        }
    }

    // At 67% and 34%: a circle around him, and a stun for whoever is still in it; then he walks to his chest
    void Stomp(SmiteStages next)
    {
        _stage = next;
        events.Reset();
        scheduler.CancelAll();
        _tele.EndWindup();
        Talk(next == STAGE_TO_TWO_SWORDS ? SAY_SWAP1 : SAY_SWAP2);

        _tele.BeginWindup(me->GetOrientation());
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), StompRadius,
            StompWarningMs);
        scheduler.Schedule(Milliseconds(StompWarningMs), [this, area, next](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::None);
            for (Player* player : _tele.HitIn(area, SPELL_SMITE_STOMP, StompPercent))
                _tele.Afflict(player, SPELL_SMITE_STOMP, StompStunMs);
            _tele.EndWindup();

            me->SetUnitFlag(UNIT_FLAG_PACIFIED);
            me->SetReactState(REACT_PASSIVE);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MovePoint(next == STAGE_TO_TWO_SWORDS ? EQUIP_TWO_SWORDS : EQUIP_MACE, SmiteChest);
        });
    }

    void RestoreCombat()
    {
        me->SetReactState(REACT_AGGRESSIVE);
        me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        me->SetStandState(UNIT_STAND_STATE_STAND);
        if (Unit* victim = me->GetVictim())
        {
            me->GetMotionMaster()->MoveChase(victim);
            me->SetTarget(victim->GetGUID());
        }

        if (_stage == STAGE_TO_TWO_SWORDS)
        {
            _stage = STAGE_TWO_SWORDS;
            events.ScheduleEvent(EVENT_BLADE_WHIRL, 6s);
        }
        else if (_stage == STAGE_TO_MACE)
        {
            _stage = STAGE_MACE;
            events.ScheduleEvent(EVENT_SMITE_SLAM, 5s);
        }
    }

    // Two swords: he spins where he stands; the circle shows, then the blades hurt whoever is in it every second
    void BladeWhirl()
    {
        _tele.BeginWindup(me->GetOrientation());
        uint32 const durationMs = WhirlGraceMs + (WhirlTicks - 1) * 1000;
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), WhirlRadius,
            durationMs);
        for (uint32 tick = 0; tick < WhirlTicks; ++tick)
        {
            bool const last = tick + 1 == WhirlTicks;
            scheduler.Schedule(Milliseconds(WhirlGraceMs + tick * 1000), [this, area, last](TaskContext)
            {
                _tele.HitIn(area, SPELL_WHIRLWIND, WhirlTickPercent);
                if (last)
                    _tele.EndWindup();
            });
        }
    }

    // The mace: a line at the tank; he holds still for it, so a step aside is a step out
    void SmiteSlam()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        float const facing = me->GetAngle(victim);
        _tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing,
            SlamLength, SlamWidth, SlamWarningMs);
        scheduler.Schedule(Milliseconds(SlamWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::None);
            _tele.HitIn(area, SPELL_SMITE_SLAM, SlamPercent);
            _tele.EndWindup();
        });
    }

    Telegraphs _tele;
    SmiteStages _stage = STAGE_SWORD;
};

// ---------------------------------------------------------------------------------------------------------------
// Captain Greenskin (replaces his SmartAI: its Cleave and Poisoned Harpoon become the two below)
// ---------------------------------------------------------------------------------------------------------------
enum GreenskinEvents
{
    EVENT_HARPOON = 1,
    EVENT_CLEAVE,
};

constexpr float HarpoonLength = 30.0f;
constexpr float HarpoonWidth = 4.0f;
constexpr uint32 HarpoonWarningMs = 2500;
constexpr float HarpoonPercent = 40.0f;         // and the harpoon's poison, about a fifth more over a minute
constexpr float CleaveRadius = 8.0f;
constexpr float CleaveArc = 120.0f;
constexpr uint32 CleaveWarningMs = 1500;
constexpr float CleavePercent = 60.0f;          // physical: the tank in front takes about a seventh of its health

struct boss_captain_greenskin_evolutions : public ScriptedAI
{
    explicit boss_captain_greenskin_evolutions(Creature* creature) : ScriptedAI(creature), _tele(creature) { }

    void Reset() override
    {
        events.Reset();
        scheduler.CancelAll();
        _tele.EndWindup();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(EVENT_CLEAVE, 5s);
        events.ScheduleEvent(EVENT_HARPOON, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        scheduler.Update(diff);
        events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            if (_tele.WindingUp())
            {
                events.ScheduleEvent(eventId, 1s);
                continue;
            }

            switch (eventId)
            {
                case EVENT_HARPOON:
                    Harpoon();
                    events.ScheduleEvent(EVENT_HARPOON, 15s);
                    break;
                case EVENT_CLEAVE:
                    Cleave();
                    events.ScheduleEvent(EVENT_CLEAVE, 10s);
                    break;
                default:
                    break;
            }
        }

        if (!_tele.WindingUp())
            DoMeleeAttackIfReady();
    }

private:
    // A line at a player other than the tank; whoever it hits is poisoned
    void Harpoon()
    {
        Player* target = _tele.RandomPlayer(30.0f);
        if (!target)
            return;

        float const facing = me->GetAngle(target);
        _tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing,
            HarpoonLength, HarpoonWidth, HarpoonWarningMs, GroundIndicators::Theme::Nature);
        scheduler.Schedule(Milliseconds(HarpoonWarningMs), [this, area](TaskContext)
        {
            for (Player* player : _tele.HitIn(area, SPELL_POISONED_HARPOON, HarpoonPercent))
                _tele.Afflict(player, SPELL_POISONED_HARPOON);
            _tele.EndWindup();
        });
    }

    // A cone in front of him, at the tank
    void Cleave()
    {
        Unit* victim = me->GetVictim();
        if (!victim)
            return;

        float const facing = me->GetAngle(victim);
        _tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowCone(me, me->GetPosition(), facing, CleaveRadius,
            CleaveArc, CleaveWarningMs);
        scheduler.Schedule(Milliseconds(CleaveWarningMs), [this, area](TaskContext)
        {
            _tele.HitIn(area, SPELL_CLEAVE, CleavePercent);
            _tele.EndWindup();
        });
    }

    Telegraphs _tele;
};

// ---------------------------------------------------------------------------------------------------------------
// Edwin VanCleef
// ---------------------------------------------------------------------------------------------------------------
enum VanCleefEvents
{
    EVENT_VANCLEEFS_MARK = 1,
    EVENT_FAN_OF_KNIVES,
    EVENT_SHADOWSTEP_STRIKE,
};

constexpr float MarkRadius = 8.0f;
constexpr uint32 MarkMs = 6000;
constexpr float MarkPercent = 55.0f;            // to everyone else still in it
constexpr float MarkCarrierPercent = 15.0f;
constexpr float FanRadius = 10.0f;
constexpr uint32 FanWarningMs = 2500;
constexpr float FanPercent = 40.0f;
constexpr float ShadowstepLength = 20.0f;
constexpr float ShadowstepWidth = 4.0f;
constexpr uint32 ShadowstepWarningMs = 2000;
constexpr float ShadowstepPercent = 50.0f;

struct boss_edwin_vancleef_evolutions : public TelegraphingSmartAI
{
    explicit boss_edwin_vancleef_evolutions(Creature* creature) : TelegraphingSmartAI(creature) { }

protected:
    void ScheduleAbilities() override
    {
        events.ScheduleEvent(EVENT_FAN_OF_KNIVES, 6s);
        events.ScheduleEvent(EVENT_VANCLEEFS_MARK, 10s);
        events.ScheduleEvent(EVENT_SHADOWSTEP_STRIKE, 14s);
    }

    void ExecuteAbility(uint32 eventId) override
    {
        switch (eventId)
        {
            case EVENT_VANCLEEFS_MARK:
                VanCleefsMark();
                events.ScheduleEvent(EVENT_VANCLEEFS_MARK, 20s);
                break;
            case EVENT_FAN_OF_KNIVES:
                FanOfKnives();
                events.ScheduleEvent(EVENT_FAN_OF_KNIVES, 15s);
                break;
            case EVENT_SHADOWSTEP_STRIKE:
                ShadowstepStrike();
                events.ScheduleEvent(EVENT_SHADOWSTEP_STRIKE, 18s);
                break;
            default:
                break;
        }
    }

private:
    // A circle following a player other than the tank; it hurts whoever else is still in it when it ends
    void VanCleefsMark()
    {
        Player* carrier = tele.RandomPlayer(40.0f, false);
        if (!carrier)
            return;

        GroundIndicators::Area const area = GroundIndicators::ShowCarriedCircle(me, carrier, MarkRadius, MarkMs);
        ObjectGuid const carrierGuid = carrier->GetGUID();
        scheduler.Schedule(Milliseconds(MarkMs), [this, area, carrierGuid](TaskContext)
        {
            Player* carrier = ObjectAccessor::GetPlayer(*me, carrierGuid);
            if (!carrier || !carrier->IsAlive())
                return;

            tele.Hit(carrier, SPELL_MARK_OF_DEATH, MarkCarrierPercent);
            for (Player* player : tele.PlayersIn(GroundIndicators::CurrentArea(carrier, area)))
                if (player != carrier)
                    tele.Hit(player, SPELL_MARK_OF_DEATH, MarkPercent);
        });
    }

    // A circle around him: the melee step out
    void FanOfKnives()
    {
        tele.BeginWindup(me->GetOrientation());
        GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, me->GetPosition(), FanRadius,
            FanWarningMs);
        scheduler.Schedule(Milliseconds(FanWarningMs), [this, area](TaskContext)
        {
            GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::None);
            tele.HitIn(area, SPELL_FAN_OF_KNIVES, FanPercent);
            tele.EndWindup();
        });
    }

    // A line at whoever stands furthest from him
    void ShadowstepStrike()
    {
        Player* target = tele.FarthestPlayer(40.0f);
        if (!target)
            return;

        float const facing = me->GetAngle(target);
        tele.BeginWindup(facing);
        GroundIndicators::Area const area = GroundIndicators::ShowRectangle(me, me->GetPosition(), facing,
            ShadowstepLength, ShadowstepWidth, ShadowstepWarningMs, GroundIndicators::Theme::Shadow);
        scheduler.Schedule(Milliseconds(ShadowstepWarningMs), [this, area](TaskContext)
        {
            tele.HitIn(area, SPELL_SHADOWSTEP, ShadowstepPercent);
            tele.EndWindup();
        });
    }
};

// Pierce Armor (the miners, Miner Johnson): a lighter armour cut in a key
class spell_mythic_deadmines_pierce_armor : public AuraScript
{
    PrepareAuraScript(spell_mythic_deadmines_pierce_armor);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Unit* owner = GetUnitOwner();
        Map const* map = owner ? owner->FindMap() : nullptr;
        if (map && map->IsMythic())
            amount = amount * PierceArmorMythicPercent / 100;
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_mythic_deadmines_pierce_armor::CalculateAmount, EFFECT_0,
            SPELL_AURA_MOD_RESISTANCE_PCT);
    }
};

void RegisterTuning()
{
    // Every multiplier below is for +10 against the reference health (90k), after the engine's level fix: spells the
    // core already levels (creature-level attribute or points per level) now land at a tenth of the audit's numbers.
    MythicTuning::SetSpellMultiplier(SPELL_FLAMESTRIKE, 18.0f);     // 1.2k -> 22k (24%), its pool 4.2k a tick
    MythicTuning::SetSpellMultiplier(SPELL_FIREBALL, 7.0f);         // 1.1k -> 7.7k (6% of a tank)
    MythicTuning::SetSpellMultiplier(SPELL_SHOOT, 7.0f);            // 0.6k -> 4.3k
    // Fire Blast's points per level stop at 27: it gets the classic catch-up, -> 3k
    MythicTuning::SetSpellMultiplier(SPELL_FIRE_BLAST, 0.8f);
    MythicTuning::SetSpellMultiplier(SPELL_ACID_SPLASH, 8.0f);      // 2.7k -> 22k over 30 s (24%)
    MythicTuning::SetSpellMultiplier(SPELL_MOLTEN_METAL, 15.0f);    // 1.9k -> 28k over 15 s on the tank (21%)
    MythicTuning::SetSpellMultiplier(SPELL_POISONED_HARPOON, 2.2f); // the poison: 8.2k -> 18k over a minute (20%)

    // Rhahk'Zor's 3.5 s swing with the boss scaling was 13% of a tank a swing
    MythicTuning::SetMeleeMultiplier(NPC_RHAHKZOR, 0.8f);
    // Sneed's Shredder is no encounter creature (Sneed is): its melee gets what the bosses' does
    MythicTuning::SetMeleeMultiplier(NPC_SNEEDS_SHREDDER, 1.6f);
}

void RegisterTrash()
{
    // Goblin Craftsmen throw dynamite under a player
    MythicTrash::Ability dynamite;
    dynamite.entry = NPC_GOBLIN_CRAFTSMAN;
    dynamite.shape = MythicTrash::Shape::UnderTarget;
    dynamite.size = 6.0f;
    dynamite.warnMs = 2500;
    dynamite.cooldownMs = 20000;
    dynamite.firstMs = 7000;
    dynamite.percent = 25.0f;
    dynamite.spellId = SPELL_THROW_DYNAMITE;
    dynamite.theme = GroundIndicators::Theme::Fire;
    dynamite.holdStill = false;
    MythicTrash::Register(dynamite);

    // Goblin Woodcarvers swing their axe through what stands in front of them
    MythicTrash::Ability cleave;
    cleave.entry = NPC_GOBLIN_WOODCARVER;
    cleave.shape = MythicTrash::Shape::ConeAtVictim;
    cleave.size = 10.0f;
    cleave.width = 100.0f;
    cleave.warnMs = 2000;
    cleave.cooldownMs = 18000;
    cleave.firstMs = 6000;
    cleave.percent = 25.0f;
    cleave.spellId = SPELL_CLEAVE;
    MythicTrash::Register(cleave);

    // Defias Squallshapers freeze the ground around them
    MythicTrash::Ability nova;
    nova.entry = NPC_DEFIAS_SQUALLSHAPER;
    nova.shape = MythicTrash::Shape::AroundSelf;
    nova.size = 8.0f;
    nova.warnMs = 2500;
    nova.cooldownMs = 22000;
    nova.firstMs = 9000;
    nova.percent = 22.0f;
    nova.spellId = SPELL_FROST_NOVA;
    nova.theme = GroundIndicators::Theme::Frost;
    MythicTrash::Register(nova);
}
}

void AddMythicDeadminesScripts()
{
    RegisterTuning();
    RegisterTrash();

    RegisterCreatureAI(boss_rhahkzor_evolutions);
    RegisterCreatureAI(boss_gilnid_evolutions);
    RegisterCreatureAI(boss_mr_smite_evolutions);
    RegisterCreatureAI(boss_captain_greenskin_evolutions);
    RegisterCreatureAI(boss_edwin_vancleef_evolutions);
    RegisterSpellScript(spell_mythic_deadmines_pierce_armor);
}
