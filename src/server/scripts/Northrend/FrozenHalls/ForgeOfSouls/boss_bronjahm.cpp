/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "Map.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "forge_of_souls.h"

#include <algorithm>

enum Yells
{
    SAY_AGGRO           = 0,
    SAY_SLAY            = 1,
    SAY_DEATH           = 2,
    SAY_SOUL_STORM      = 3,
    SAY_CORRUPT_SOUL    = 4,
};

enum Spells
{
    SPELL_SOULSTORM_CHANNEL_OOC     = 69008,

    SPELL_SHADOW_BOLT               = 70043,
    SPELL_FEAR                      = 68950,
    SPELL_MAGICS_BANE               = 68793,
    SPELL_CORRUPT_SOUL              = 68839,
    SPELL_CONSUME_SOUL              = 68861,
    //SPELL_CONSUME_SOUL_HEAL       = 68858,

    SPELL_TELEPORT                  = 68988,
    SPELL_TELEPORT_VISUAL           = 52096,

    SPELL_SOULSTORM_VISUAL          = 68870,
    SPELL_SOULSTORM_VISUAL2         = 68904,
    SPELL_SOULSTORM                 = 68872,
};

enum BronjahmCreatures
{
    NPC_CORRUPTED_SOUL_FRAGMENT     = 36535,
};

// Corrupt Soul summons a fragment that has to be killed before it reaches Bronjahm and heals him. On its own
// twenty second cycle that is a fine rhythm - but the fragment carries the instance's health scaling, and in a
// mythic run it can still be alive when the next cast comes due. The mechanic then stacks on itself and the
// group never gets to touch the boss.
//
// So the cast waits for the floor to be clear, and once a fragment falls there is a guaranteed stretch of
// fight before another is summoned.
constexpr uint32 CorruptSoulBreather = 12000;

// What a fragment costs the group when it reaches Bronjahm. A share of maximum health rather than a flat
// number, so it stays a real threat through every key level without ever being the whole health bar: the
// punishment for letting one through should be a healer under pressure, not a wipe.
constexpr uint8 ConsumeSoulHealthPct = 25;

enum Events
{
    EVENT_SPELL_SHADOW_BOLT = 1,
    EVENT_SPELL_FEAR,
    EVENT_SPELL_MAGICS_BANE,
    EVENT_SPELL_CORRUPT_SOUL,
    EVENT_START_SOULSTORM,
};

struct boss_bronjahm : public BossAI
{
    boss_bronjahm(Creature* creature) : BossAI(creature, DATA_BRONJAHM) { }

    void JustReachedHome() override
    {
        BossAI::JustReachedHome();
        DoCastSelf(SPELL_SOULSTORM_CHANNEL_OOC, true);
    }

    void Reset() override
    {
        BossAI::Reset();
        _corruptSoulBreather = 0;
        me->RemoveUnitFlag(UNIT_FLAG_DISABLE_MOVE);
        DoCastSelf(SPELL_SOULSTORM_CHANNEL_OOC, true);
    }

    void SummonedCreatureDies(Creature* summon, Unit* killer) override
    {
        BossAI::SummonedCreatureDies(summon, killer);
        if (summon->GetEntry() == NPC_CORRUPTED_SOUL_FRAGMENT)
            _corruptSoulBreather = CorruptSoulBreather;
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        me->RemoveAurasDueToSpell(SPELL_SOULSTORM_CHANNEL_OOC);

        events.RescheduleEvent(EVENT_SPELL_SHADOW_BOLT, 2s);
        events.RescheduleEvent(EVENT_SPELL_MAGICS_BANE, 5s, 10s);
        events.RescheduleEvent(EVENT_SPELL_CORRUPT_SOUL, 14s, 20s);
    }

    void DamageTaken(Unit*, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        if (!me->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE) && me->HealthBelowPctDamaged(35, damage))
        {
            me->SetUnitFlag(UNIT_FLAG_DISABLE_MOVE);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            DoCastSelf(SPELL_TELEPORT);
            events.CancelEvent(EVENT_SPELL_CORRUPT_SOUL);
            events.DelayEvents(6s);
            events.RescheduleEvent(EVENT_SPELL_FEAR, 8s, 14s);
        }
    }

    void SpellHitTarget(Unit* /*target*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_TELEPORT)
        {
            DoCastSelf(SPELL_TELEPORT_VISUAL, true);
            events.RescheduleEvent(EVENT_START_SOULSTORM, 1ms);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);
        _corruptSoulBreather -= std::min(_corruptSoulBreather, diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        if (me->HasUnitFlag(UNIT_FLAG_DISABLE_MOVE))
            if (me->isAttackReady())
                me->SetFacingToObject(me->GetVictim());

        switch (events.ExecuteEvent())
        {
            case EVENT_SPELL_SHADOW_BOLT:
                if (!me->IsWithinMeleeRange(me->GetVictim()))
                    DoCastVictim(SPELL_SHADOW_BOLT);
                events.Repeat(2s);
                break;
            case EVENT_SPELL_FEAR:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 10.0f, true))
                    me->CastCustomSpell(SPELL_FEAR, SPELLVALUE_MAX_TARGETS, 1, target);
                events.Repeat(8s, 12s);
                break;
            case EVENT_SPELL_MAGICS_BANE:
                DoCastVictim(SPELL_MAGICS_BANE);
                events.Repeat(10s, 15s);
                break;
            case EVENT_SPELL_CORRUPT_SOUL:
                // A fragment still on the floor, or one that only just fell: look again shortly rather than
                // summoning a second one on top of it.
                if (summons.HasEntry(NPC_CORRUPTED_SOUL_FRAGMENT) || _corruptSoulBreather)
                {
                    events.Repeat(2s);
                    break;
                }

                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 100.0f, true))
                {
                    Talk(SAY_CORRUPT_SOUL);
                    DoCast(target, SPELL_CORRUPT_SOUL);
                }
                events.Repeat(20s, 25s);
                break;
            case EVENT_START_SOULSTORM:
                Talk(SAY_SOUL_STORM);
                DoCastSelf(SPELL_SOULSTORM);
                DoCastSelf(SPELL_TELEPORT_VISUAL, true);
                DoCastSelf(SPELL_SOULSTORM_VISUAL, true);
                break;
        }

        DoMeleeAttackIfReady();
    }

    void JustDied(Unit* killer) override
    {
        BossAI::JustDied(killer);
        Talk(SAY_DEATH);
    }

    void KilledUnit(Unit* who) override
    {
        if (who->IsPlayer())
            Talk(SAY_SLAY);
    }

    void JustSummoned(Creature* summon) override
    {
        BossAI::JustSummoned(summon);
        summon->SetReactState(REACT_PASSIVE);
    }

    void EnterEvadeMode(EvadeReason why) override
    {
        me->RemoveUnitFlag(UNIT_FLAG_DISABLE_MOVE);
        BossAI::EnterEvadeMode(why);
    }

private:
    // Counts down the guaranteed stretch of fight after a soul fragment dies, before another may be summoned.
    uint32 _corruptSoulBreather = 0;
};

struct npc_fos_corrupted_soul_fragment : public NullCreatureAI
{
    npc_fos_corrupted_soul_fragment(Creature* creature) : NullCreatureAI(creature)
    {
        Instance = me->GetInstanceScript();
    }

    uint32 Timer = 0;
    InstanceScript* Instance = nullptr;

    void Reset() override
    {
        Timer = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!Instance)
            return;

        Creature* bronjahm = Instance->GetCreature(DATA_BRONJAHM);
        if (!bronjahm)
            return;

        if (me->GetExactDist2d(bronjahm) <= 2.0f)
        {
            me->GetMotionMaster()->MoveIdle();
            me->CastSpell(bronjahm, SPELL_CONSUME_SOUL, true);
            ConsumeSoul(bronjahm);
            me->DespawnOrUnsummon(1ms);
            return;
        }

        if (Timer <= diff)
        {
            if (!me->HasUnitState(UNIT_STATE_ROOT | UNIT_STATE_STUNNED))
                me->GetMotionMaster()->MovePoint(0, *bronjahm);
            Timer = 1000;
        }
        else
            Timer -= diff;
    }

private:
    // A fragment that gets home takes it out on the group rather than feeding Bronjahm.
    //
    // Retail has the soul heal him for a flat 120,000 (spell 68858), which this core never casts - 68861 is a
    // script effect nothing implements, so until now a fragment reaching him did nothing whatsoever and the
    // mechanic could be ignored outright. Either way a flat heal is the wrong shape for a scaled instance: it
    // is trivial at a high key and enormous at a low one, and healing a boss punishes the group by making the
    // fight longer, which is the least interesting way to be punished.
    void ConsumeSoul(Creature* bronjahm)
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_CONSUME_SOUL);
        if (!spellInfo)
            return;

        Map::PlayerList const& players = me->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster())
                continue;

            uint32 const damage = CalculatePct(player->GetMaxHealth(), ConsumeSoulHealthPct);
            if (!damage)
                continue;

            // Dealt as the boss's own spell so it reads as Consume Soul in the log and in a damage meter,
            // rather than as health quietly disappearing.
            SpellNonMeleeDamage damageInfo(bronjahm, player, spellInfo, spellInfo->SchoolMask);
            damageInfo.damage = damage;
            bronjahm->SendSpellNonMeleeDamageLog(&damageInfo);
            bronjahm->DealSpellDamage(&damageInfo, false);
        }
    }
};

class spell_bronjahm_magic_bane : public SpellScript
{
    PrepareSpellScript(spell_bronjahm_magic_bane);

    void RecalculateDamage()
    {
        if (GetHitUnit()->getPowerType() != POWER_MANA)
            return;

        if (Unit* caster = GetCaster())
        {
            int32 const maxDamage = caster->GetMap()->GetSpawnMode() == 1 ? 15000 : 10000;
            int32 newDamage = GetHitDamage();
            newDamage += GetHitUnit()->GetMaxPower(POWER_MANA) / 2;
            newDamage = std::min<int32>(maxDamage, newDamage);

            SetHitDamage(newDamage);
        }
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_bronjahm_magic_bane::RecalculateDamage);
    }
};

class spell_bronjahm_soulstorm_channel_ooc_aura : public AuraScript
{
    PrepareAuraScript(spell_bronjahm_soulstorm_channel_ooc_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_SOULSTORM_VISUAL2, SPELL_SOULSTORM_VISUAL2+1, SPELL_SOULSTORM_VISUAL2+2, SPELL_SOULSTORM_VISUAL2+3 });
    }

    void HandlePeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        GetTarget()->CastSpell(GetTarget(), SPELL_SOULSTORM_VISUAL2 + (aurEff->GetTickNumber() % 4), true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_bronjahm_soulstorm_channel_ooc_aura::HandlePeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_bronjahm_soulstorm_visual_aura : public AuraScript
{
    PrepareAuraScript(spell_bronjahm_soulstorm_visual_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ 68886, 68896, 68897, 68898 });
    }

    void HandlePeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        uint32 spellId = 0;
        switch (aurEff->GetTickNumber() % 4)
        {
            case 0:
                spellId = 68886;
                break;
            case 1:
                spellId = 68896;
                break;
            case 2:
                spellId = 68897;
                break;
            case 3:
                spellId = 68898;
                break;
        }
        GetTarget()->CastSpell(GetTarget(), spellId, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_bronjahm_soulstorm_visual_aura::HandlePeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_bronjahm_soulstorm_targeting : public SpellScript
{
    PrepareSpellScript(spell_bronjahm_soulstorm_targeting);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Acore::AllWorldObjectsInExactRange(GetCaster(), 10.0f, false));
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_bronjahm_soulstorm_targeting::FilterTargets, EFFECT_ALL, TARGET_UNIT_DEST_AREA_ENEMY);
    }
};

void AddSC_boss_bronjahm()
{
    RegisterForgeOfSoulsCreatureAI(boss_bronjahm);
    RegisterForgeOfSoulsCreatureAI(npc_fos_corrupted_soul_fragment);
    RegisterSpellScript(spell_bronjahm_magic_bane);
    RegisterSpellScript(spell_bronjahm_soulstorm_channel_ooc_aura);
    RegisterSpellScript(spell_bronjahm_soulstorm_visual_aura);
    RegisterSpellScript(spell_bronjahm_soulstorm_targeting);
}
