#include "MythicDungeons.h"
#include "MythicTuning.h"

#include "AllCreatureScript.h"
#include "Containers.h"
#include "CreatureScript.h"
#include "GroundIndicators.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TaskScheduler.h"
#include "UnitScript.h"
#include <algorithm>
#include <list>
#include <vector>

// Drak'Tharon Keep in Mythic+: damage tuning, boss reworks and trash abilities
// (audit: .agents/plans/mplus-damage-audit/).
//
// Trollgore (boss_trollgore_evolutions): Corpse Explode draws a nature circle on the invader corpse it picks for three
// seconds, then bursts on whoever is still in it (two corpses at once in a key below half health). Consume's stacks
// only come from players in a key, give 3% each instead of 5% and stop at 30: a readable soft enrage instead of one
// that raced with how many invaders happened to stand near him.
// Novos: no Arcane Field in a key. It was laid once at the pull and stayed for good, so in phase 2 his melee stood in
// a slowing, burning red circle (and bots would not step into it at all).
// The Prophet Tharon'ja: in his flesh phase the players are skeletons without armour; his swings on them are cut to
// what an armoured tank would take.
// King Dred: Ravage, a circle following a player that hurts everyone else still next to them, for some group pressure.
namespace
{
// The stock instance script (instance_drak_tharon_keep) keeps the boss states
enum DrakTharonData
{
    DATA_TROLLGORE                  = 0,
};

enum DrakTharonCreatures
{
    NPC_DRAKKARI_INVADER_A          = 27709,
    NPC_DRAKKARI_INVADER_B          = 27753,
    NPC_DRAKKARI_INVADER_C          = 27754,
    NPC_NOVOS                       = 26631,
    NPC_THARONJA                    = 26632,
    NPC_THARONJA_HEROIC             = 31360,
    NPC_KING_DRED                   = 27483,
    NPC_KING_DRED_HEROIC            = 31349,

    // Trash with a telegraphed ability
    NPC_RISEN_DRAKKARI_WARRIOR      = 26635,
    NPC_RISEN_DRAKKARI_WARRIOR_H    = 31355,
    NPC_DARKWEB_RECLUSE             = 26625,
    NPC_DARKWEB_RECLUSE_H           = 31336,
    NPC_DRAKKARI_GUARDIAN           = 26620,
    NPC_DRAKKARI_GUARDIAN_H         = 31339,
};

enum DrakTharonSpells
{
    // Trollgore
    SPELL_SUMMON_INVADER_A          = 49456,
    SPELL_SUMMON_INVADER_B          = 49457,
    SPELL_SUMMON_INVADER_C          = 49458,
    SPELL_INFECTED_WOUND            = 49637,
    SPELL_CRUSH                     = 49639,
    SPELL_CONSUME                   = 49380,
    SPELL_CONSUME_H                 = 59803,
    SPELL_CONSUME_AURA              = 49381,
    SPELL_CONSUME_AURA_H            = 59805,
    SPELL_CORPSE_EXPLODE_DAMAGE     = 49618,
    SPELL_CORPSE_EXPLODE_DAMAGE_H   = 59809,

    // Novos
    SPELL_NOVOS_FROSTBOLT           = 49037,
    SPELL_NOVOS_FROSTBOLT_H         = 59855,
    SPELL_NOVOS_BLIZZARD            = 49034,
    SPELL_NOVOS_BLIZZARD_H          = 59854,

    // The Prophet Tharon'ja
    SPELL_GIFT_OF_THARONJA          = 52509,
    SPELL_LIGHTNING_BREATH          = 49537,
    SPELL_LIGHTNING_BREATH_H        = 59963,
    SPELL_POISON_CLOUD              = 49548,
    SPELL_POISON_CLOUD_H            = 59969,

    // Named only: the damage is dealt on the drawn areas
    SPELL_RAVAGE                    = 50518,    // King Dred
    SPELL_BRUTAL_SWIPE              = 42384,    // Risen Drakkari Warrior
    SPELL_VENOM_SPIT                = 55700,    // Darkweb Recluse
    SPELL_TRAMPLE                   = 40340,    // Drakkari Guardian
};

// Damage, before the key's scaling, for the reworked abilities outside a key: heroic, and normal at NormalPercent of
// it. In a key they deal a share of the reference health instead.
constexpr uint32 NormalPercent = 40;

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
// Trollgore
// ---------------------------------------------------------------------------------------------------------------------
enum TrollgoreTexts
{
    SAY_TROLLGORE_AGGRO             = 0,
    SAY_TROLLGORE_KILL              = 1,
    SAY_TROLLGORE_CONSUME           = 2,
    SAY_TROLLGORE_EXPLODE           = 3,
    SAY_TROLLGORE_DEATH             = 4,
};

enum TrollgoreEvents
{
    EVENT_TROLLGORE_INFECTED_WOUND  = 1,
    EVENT_TROLLGORE_CRUSH,
    EVENT_TROLLGORE_CONSUME,
    EVENT_TROLLGORE_CORPSE_EXPLODE,
    EVENT_TROLLGORE_SPAWN_INVADERS,
};

constexpr float ConsumeRadius = 50.0f;
constexpr uint32 MaxConsumeStacks = 30;
constexpr int32 ConsumeDamagePerStack = 3;      // percent damage done (stock heroic 5)
constexpr int32 ConsumeScalePerStack = 2;       // percent size (stock heroic 5)
constexpr float CorpseRadius = 5.0f;
constexpr uint32 CorpseWarningMs = 3000;
constexpr float CorpsePercent = 45.0f;
constexpr uint32 CorpseDamage = 10000;
// Crush in a key, in weapon percent (stock 150): his only tank ability, and he swings every second
constexpr int32 CrushMythicPoints = 249;

struct boss_trollgore_evolutions : public BossAI
{
    boss_trollgore_evolutions(Creature* creature) : BossAI(creature, DATA_TROLLGORE) { }

    void Reset() override
    {
        BossAI::Reset();
        _consumeStacks = 0;
        _markedCorpses.clear();
        _invaderEvents.Reset();
        _invaderEvents.ScheduleEvent(EVENT_TROLLGORE_SPAWN_INVADERS, 30s);
    }

    void JustEngagedWith(Unit* who) override
    {
        events.ScheduleEvent(EVENT_TROLLGORE_INFECTED_WOUND, 6s, 10s);
        events.ScheduleEvent(EVENT_TROLLGORE_CRUSH, 3s, 5s);
        events.ScheduleEvent(EVENT_TROLLGORE_CONSUME, 15s);
        events.ScheduleEvent(EVENT_TROLLGORE_CORPSE_EXPLODE, 35s);
        _invaderEvents.RescheduleEvent(EVENT_TROLLGORE_SPAWN_INVADERS, 20s, 30s);

        me->setActive(true);
        instance->SetBossState(DATA_TROLLGORE, IN_PROGRESS);
        if (who->IsPlayer())
        {
            Talk(SAY_TROLLGORE_AGGRO);
            me->SetInCombatWithZone();
        }
    }

    void JustDied(Unit* killer) override
    {
        Talk(SAY_TROLLGORE_DEATH);
        BossAI::JustDied(killer);
    }

    void KilledUnit(Unit* /*victim*/) override
    {
        Talk(SAY_TROLLGORE_KILL);
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    bool CheckInRoom() override
    {
        return me->GetPositionY() >= -700.0f && me->GetPositionY() <= -628.0f;
    }

    void UpdateAI(uint32 diff) override
    {
        _invaderEvents.Update(diff);
        if (_invaderEvents.ExecuteEvent() == EVENT_TROLLGORE_SPAWN_INVADERS)
        {
            me->CastSpell(me, SPELL_SUMMON_INVADER_A, true);
            me->CastSpell(me, SPELL_SUMMON_INVADER_B, true);
            me->CastSpell(me, SPELL_SUMMON_INVADER_C, true);
            _invaderEvents.ScheduleEvent(EVENT_TROLLGORE_SPAWN_INVADERS, 30s);
        }

        if (!UpdateVictim())
            return;

        if (!CheckInRoom())
        {
            EnterEvadeMode(EVADE_REASON_BOUNDARY);
            return;
        }

        scheduler.Update(diff);
        ReconcileConsume();

        events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
            case EVENT_TROLLGORE_INFECTED_WOUND:
                me->CastSpell(me->GetVictim(), SPELL_INFECTED_WOUND, false);
                events.ScheduleEvent(EVENT_TROLLGORE_INFECTED_WOUND, 25s, 35s);
                break;
            case EVENT_TROLLGORE_CRUSH:
                if (me->GetMap()->IsMythic())
                    me->CastCustomSpell(me->GetVictim(), SPELL_CRUSH, &CrushMythicPoints, nullptr, nullptr, false);
                else
                    me->CastSpell(me->GetVictim(), SPELL_CRUSH, false);
                events.ScheduleEvent(EVENT_TROLLGORE_CRUSH, 10s, 15s);
                break;
            case EVENT_TROLLGORE_CONSUME:
                Consume();
                events.ScheduleEvent(EVENT_TROLLGORE_CONSUME, 15s);
                break;
            case EVENT_TROLLGORE_CORPSE_EXPLODE:
                if (CorpseExplode())
                    events.ScheduleEvent(EVENT_TROLLGORE_CORPSE_EXPLODE, 15s, 19s);
                else
                    events.ScheduleEvent(EVENT_TROLLGORE_CORPSE_EXPLODE, 5s);
                break;
            default:
                break;
        }

        DoMeleeAttackIfReady();
    }

private:
    bool IsInvader(uint32 entry) const
    {
        return entry == NPC_DRAKKARI_INVADER_A || entry == NPC_DRAKKARI_INVADER_B || entry == NPC_DRAKKARI_INVADER_C;
    }

    // Consume: the stock spell (its pulse and its visual); in a key only the players it reaches feed him
    void Consume()
    {
        Talk(SAY_TROLLGORE_CONSUME);
        if (me->GetMap()->IsMythic())
            _consumeStacks = std::min<uint32>(_consumeStacks + uint32(PlayersNear(me, ConsumeRadius).size()),
                MaxConsumeStacks);
        me->CastSpell(me, SPELL_CONSUME, false);
    }

    // The stock spell script makes every unit Consume hits give him a stack, invaders included, at 5% each and
    // without end. In a key his stacks are held to the players' count, capped, at 3% each.
    void ReconcileConsume()
    {
        if (!me->GetMap()->IsMythic())
            return;

        for (uint32 auraId : { SPELL_CONSUME_AURA_H, SPELL_CONSUME_AURA })
        {
            Aura* aura = me->GetAura(auraId);
            if (!aura)
                continue;

            if (!_consumeStacks)
            {
                me->RemoveAura(aura);
                continue;
            }

            if (aura->GetStackAmount() != _consumeStacks)
                aura->SetStackAmount(uint8(_consumeStacks));
            int32 const stacks = int32(_consumeStacks);
            if (AuraEffect* scale = aura->GetEffect(EFFECT_0))
                if (scale->GetAmount() != ConsumeScalePerStack * stacks)
                    scale->ChangeAmount(ConsumeScalePerStack * stacks);
            if (AuraEffect* damage = aura->GetEffect(EFFECT_1))
                if (damage->GetAmount() != ConsumeDamagePerStack * stacks)
                    damage->ChangeAmount(ConsumeDamagePerStack * stacks);
        }
    }

    // Corpse Explode: a nature circle on an invader corpse, then it bursts on whoever is still in it
    bool CorpseExplode()
    {
        std::list<Creature*> dead;
        me->GetDeadCreatureListInGrid(dead, 60.0f);
        std::vector<Creature*> corpses;
        for (Creature* corpse : dead)
            if (IsInvader(corpse->GetEntry()) && !_markedCorpses.count(corpse->GetGUID()))
                corpses.push_back(corpse);
        if (corpses.empty())
            return false;

        std::size_t const count = (me->GetMap()->IsMythic() && me->HealthBelowPct(50)) ? 2 : 1;
        Acore::Containers::RandomResize(corpses, count);
        Talk(SAY_TROLLGORE_EXPLODE);

        uint32 const spellId = IsHeroic() ? SPELL_CORPSE_EXPLODE_DAMAGE_H : SPELL_CORPSE_EXPLODE_DAMAGE;
        for (Creature* corpse : corpses)
        {
            ObjectGuid const corpseGuid = corpse->GetGUID();
            _markedCorpses.insert(corpseGuid);
            GroundIndicators::Area const area = GroundIndicators::ShowCircle(me, corpse->GetPosition(), CorpseRadius,
                CorpseWarningMs, GroundIndicators::Theme::Nature);
            scheduler.Schedule(Milliseconds(CorpseWarningMs), [this, area, corpseGuid, spellId](TaskContext)
            {
                GroundIndicators::Burst(me, area.origin, GroundIndicators::Theme::Nature);
                for (Player* player : PlayersIn(me, area))
                    Hit(me, player, spellId, CorpsePercent, CorpseDamage);
                if (Creature* exploded = ObjectAccessor::GetCreature(*me, corpseGuid))
                    exploded->DespawnOrUnsummon();
                _markedCorpses.erase(corpseGuid);
            });
        }
        return true;
    }

    EventMap _invaderEvents;
    uint32 _consumeStacks = 0;
    GuidUnorderedSet _markedCorpses;
};

// ---------------------------------------------------------------------------------------------------------------------
// Novos: no Arcane Field in a key (see the top of the file)
// ---------------------------------------------------------------------------------------------------------------------
class spell_novos_arcane_field_evolutions : public SpellScript
{
    PrepareSpellScript(spell_novos_arcane_field_evolutions);

    void PreventInKey(SpellEffIndex effIndex)
    {
        Unit* caster = GetCaster();
        if (caster && caster->GetEntry() == NPC_NOVOS && caster->GetMap()->IsMythic())
            PreventHitDefaultEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_novos_arcane_field_evolutions::PreventInKey, EFFECT_ALL,
            SPELL_EFFECT_PERSISTENT_AREA_AURA);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// The Prophet Tharon'ja: his swings on the armourless skeletons of the flesh phase
// ---------------------------------------------------------------------------------------------------------------------
// Gift of Tharon'ja takes 99000 armour: a boss swing landed whole, 20-30% of a player's health each. Cut to what an
// armoured tank takes from a boss (about 6-8%), so taunting and Bone Armor carry the phase, not a wipe.
constexpr float FleshPhaseMeleeFactor = 0.3f;

class DrakTharonMythicUnitScript : public UnitScript
{
public:
    DrakTharonMythicUnitScript() : UnitScript("DrakTharonMythicUnitScript", true, { UNITHOOK_MODIFY_MELEE_DAMAGE }) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (!target || !attacker || !damage)
            return;
        Creature* creature = attacker->ToCreature();
        if (!creature || (creature->GetEntry() != NPC_THARONJA && creature->GetEntry() != NPC_THARONJA_HEROIC))
            return;
        Map* map = creature->FindMap();
        if (!map || !map->IsMythic() || !target->HasAura(SPELL_GIFT_OF_THARONJA))
            return;

        damage = std::max<uint32>(1, static_cast<uint32>(damage * FleshPhaseMeleeFactor));
    }
};

void RegisterTuning()
{
    // Trollgore: Consume pulsed at 17% of a damage dealer, unavoidable, every 15 s
    MythicTuning::SetSpellMultiplier(SPELL_CONSUME, 0.7f);
    MythicTuning::SetSpellMultiplier(SPELL_CONSUME_H, 0.7f);

    // Novos: his Frostbolt auto-attack hit the tank for 2.3 boss swings of unmitigated frost; Blizzard is drawn
    MythicTuning::SetSpellMultiplier(SPELL_NOVOS_FROSTBOLT, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_NOVOS_FROSTBOLT_H, 0.6f);
    MythicTuning::SetSpellMultiplier(SPELL_NOVOS_BLIZZARD, 1.3f);
    MythicTuning::SetSpellMultiplier(SPELL_NOVOS_BLIZZARD_H, 1.3f);

    // Tharon'ja, flesh phase: Lightning Breath is the only tank spell, Poison Cloud is drawn and was weak
    MythicTuning::SetSpellMultiplier(SPELL_LIGHTNING_BREATH, 1.5f);
    MythicTuning::SetSpellMultiplier(SPELL_LIGHTNING_BREATH_H, 1.5f);
    MythicTuning::SetSpellMultiplier(SPELL_POISON_CLOUD, 1.5f);
    MythicTuning::SetSpellMultiplier(SPELL_POISON_CLOUD_H, 1.5f);
}

void RegisterTrash()
{
    using MythicTrash::Shape;
    using GroundIndicators::Theme;

    // King Dred: Ravage, a circle following a player; the others step away from them. He only hit his tank before.
    for (uint32 entry : { NPC_KING_DRED, NPC_KING_DRED_HEROIC })
    {
        MythicTrash::Ability ravage;
        ravage.entry = entry;
        ravage.shape = Shape::CarriedByTarget;
        ravage.size = 6.0f;
        ravage.warnMs = 3000;
        ravage.cooldownMs = 22000;
        ravage.firstMs = 12000;
        ravage.percent = 35.0f;
        ravage.spellId = SPELL_RAVAGE;
        ravage.theme = Theme::None;
        ravage.holdStill = false;
        MythicTrash::Register(ravage);
    }

    // Risen Drakkari Warrior: Brutal Swipe, a cone at its target
    for (uint32 entry : { NPC_RISEN_DRAKKARI_WARRIOR, NPC_RISEN_DRAKKARI_WARRIOR_H })
    {
        MythicTrash::Ability swipe;
        swipe.entry = entry;
        swipe.shape = Shape::ConeAtVictim;
        swipe.size = 10.0f;
        swipe.width = 90.0f;
        swipe.warnMs = 2000;
        swipe.cooldownMs = 18000;
        swipe.firstMs = 7000;
        swipe.percent = 30.0f;
        swipe.spellId = SPELL_BRUTAL_SWIPE;
        swipe.theme = Theme::None;
        swipe.holdStill = true;
        MythicTrash::Register(swipe);
    }

    // Darkweb Recluse: Venom Spit, a poison pool under a player
    for (uint32 entry : { NPC_DARKWEB_RECLUSE, NPC_DARKWEB_RECLUSE_H })
    {
        MythicTrash::Ability spit;
        spit.entry = entry;
        spit.shape = Shape::UnderTarget;
        spit.size = 5.0f;
        spit.warnMs = 2500;
        spit.cooldownMs = 16000;
        spit.firstMs = 5000;
        spit.percent = 25.0f;
        spit.spellId = SPELL_VENOM_SPIT;
        spit.theme = Theme::Nature;
        spit.holdStill = false;
        MythicTrash::Register(spit);
    }

    // Drakkari Guardian: Trample, a circle around itself
    for (uint32 entry : { NPC_DRAKKARI_GUARDIAN, NPC_DRAKKARI_GUARDIAN_H })
    {
        MythicTrash::Ability trample;
        trample.entry = entry;
        trample.shape = Shape::AroundSelf;
        trample.size = 7.0f;
        trample.warnMs = 2500;
        trample.cooldownMs = 22000;
        trample.firstMs = 9000;
        trample.percent = 30.0f;
        trample.spellId = SPELL_TRAMPLE;
        trample.theme = Theme::None;
        trample.holdStill = true;
        MythicTrash::Register(trample);
    }
}
}

void AddMythicDrakTharonKeepScripts()
{
    RegisterTuning();
    RegisterTrash();
    RegisterCreatureAI(boss_trollgore_evolutions);
    RegisterSpellScript(spell_novos_arcane_field_evolutions);
    new DrakTharonMythicUnitScript();
}
