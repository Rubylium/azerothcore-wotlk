#include "AdaptiveTrainingDummy.h"

#include "CombatManager.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <unordered_map>

namespace
{
struct npc_adaptive_training_dummy : NullCreatureAI
{
    explicit npc_adaptive_training_dummy(Creature* creature) : NullCreatureAI(creature) { }

    void Reset() override
    {
        _scaleTimer = 0;
        _combatTimers.clear();
        ScaleToNearestPlayer();
    }

    void JustEnteredCombat(Unit* who) override
    {
        ScaleToAttacker(who);
        if (who)
            _combatTimers[who->GetGUID()] = 5s;
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType damageType, SpellSchoolMask) override
    {
        ScaleToAttacker(attacker);
        damage = 0;

        if (!attacker || damageType == DOT)
            return;

        _combatTimers[attacker->GetGUID()] = 5s;
        if (Unit* owner = attacker->GetCharmerOrOwner())
            if (me->GetCombatManager().IsInCombatWith(owner))
                _combatTimers[owner->GetGUID()] = 5s;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_scaleTimer <= diff)
        {
            ScaleToNearestPlayer();
            _scaleTimer = 500;
        }
        else
            _scaleTimer -= diff;

        for (auto iterator = _combatTimers.begin(); iterator != _combatTimers.end();)
        {
            iterator->second -= Milliseconds(diff);
            if (iterator->second > 0s)
            {
                ++iterator;
                continue;
            }

            auto const& references = me->GetCombatManager().GetPvECombatRefs();
            auto reference = references.find(iterator->first);
            if (reference != references.end())
                reference->second->EndCombat();
            iterator = _combatTimers.erase(iterator);
        }
    }

private:
    void ScaleToPlayer(Player const* player)
    {
        if (player && me->GetLevel() != player->GetLevel())
            me->SetLevel(player->GetLevel());
    }

    void ScaleToAttacker(Unit* attacker)
    {
        if (attacker)
            ScaleToPlayer(attacker->GetCharmerOrOwnerPlayerOrPlayerItself());
    }

    void ScaleToNearestPlayer()
    {
        ScaleToPlayer(me->SelectNearestPlayer(50.0f));
    }

    uint32 _scaleTimer = 0;
    std::unordered_map<ObjectGuid, Milliseconds> _combatTimers;
};
}

void AddAdaptiveTrainingDummyScripts()
{
    RegisterCreatureAI(npc_adaptive_training_dummy);
}
