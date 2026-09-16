#include "VictoryRushSystem.h"

#include "ScriptMgr.h"
#include "SpellScript.h"
#include "Unit.h"

// Victory Rush (34428) works like retail: usable in any stance without a killing blow (spell data changes in
// localTools/patchSinisterStrike.ps1) and heals the warrior for a percentage of maximum health on hit.
namespace
{
constexpr int32 VictoryRushHealPercent = 20;

class WarriorVictoryRushHealScript : public SpellScript
{
    PrepareSpellScript(WarriorVictoryRushHealScript);

    void HandleHit()
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsAlive() || !GetHitUnit())
            return;

        HealInfo healInfo(caster, caster, caster->CountPctFromMaxHealth(VictoryRushHealPercent), GetSpellInfo(),
            GetSpellInfo()->GetSchoolMask());
        caster->HealBySpell(healInfo);
    }

    void Register() override
    {
        OnHit += SpellHitFn(WarriorVictoryRushHealScript::HandleHit);
    }
};
}

void AddVictoryRushScripts()
{
    RegisterSpellScript(WarriorVictoryRushHealScript);
}
