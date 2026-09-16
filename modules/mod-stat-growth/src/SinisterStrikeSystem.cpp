#include "SinisterStrikeSystem.h"

#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

namespace
{
constexpr uint32 SinisterStrikeEnergyGain = 45;

class SinisterStrikeEnergyScript : public SpellScript
{
    PrepareSpellScript(SinisterStrikeEnergyScript);

    void HandleAfterCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player || player->getClass() != CLASS_ROGUE)
            return;

        player->EnergizeBySpell(player, GetSpellInfo()->Id, SinisterStrikeEnergyGain, POWER_ENERGY);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(SinisterStrikeEnergyScript::HandleAfterCast);
    }
};
}

void AddSinisterStrikeScripts()
{
    RegisterSpellScript(SinisterStrikeEnergyScript);
}
