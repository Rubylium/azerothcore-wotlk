#include "GladiatorStanceSystem.h"

#include "Chat.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "WorldSession.h"

// Gladiator Stance (spell 90021, see localTools/patchSinisterStrike.ps1) doubles damage done and taken through
// native percent auras. The Defensive Stance requirement and its removal on stance change come from the spell data;
// the shield requirement lives here because a spell item requirement would exclude weapon attacks from the bonus.
namespace
{
constexpr uint32 GladiatorStance = 90021;
constexpr uint8 GladiatorStanceLevel = 10;

bool IsWarrior(Player const* player)
{
    return player && player->getClass() == CLASS_WARRIOR;
}

bool HasShieldEquipped(Player const* player)
{
    Item const* shield = player->GetShield();
    return shield && !shield->IsBroken();
}

class WarriorGladiatorStanceScript : public SpellScript
{
    PrepareSpellScript(WarriorGladiatorStanceScript);

    SpellCastResult CheckCast()
    {
        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return SPELL_FAILED_DONT_REPORT;

        // Casting it again leaves the stance
        if (player->HasAura(GladiatorStance))
        {
            player->RemoveAura(GladiatorStance);
            return SPELL_FAILED_DONT_REPORT;
        }

        if (!HasShieldEquipped(player))
        {
            ChatHandler(player->GetSession()).SendNotification("Gladiator Stance requires a shield.");
            return SPELL_FAILED_DONT_REPORT;
        }

        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(WarriorGladiatorStanceScript::CheckCast);
    }
};
}

void LearnGladiatorStance(Player* player)
{
    if (!IsWarrior(player) || player->GetLevel() < GladiatorStanceLevel || player->HasSpell(GladiatorStance))
        return;

    // The spell only exists once the patched Spell.dbc is installed
    if (!sSpellMgr->GetSpellInfo(GladiatorStance))
        return;

    player->learnSpell(GladiatorStance, false);
    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cffc79c6eGladiator Stance learned: double damage dealt and taken in Defensive Stance with a shield.|r");
}

void OnGladiatorLevelChanged(Player* player, uint8 oldLevel)
{
    if (!IsWarrior(player) || player->GetLevel() >= oldLevel)
        return;
    if (player->GetLevel() >= GladiatorStanceLevel || !player->HasSpell(GladiatorStance))
        return;

    player->RemoveAura(GladiatorStance);
    player->removeSpell(GladiatorStance, SPEC_MASK_ALL, false);
}

void UpdateGladiatorStance(Player* player)
{
    if (!IsWarrior(player) || !player->HasAura(GladiatorStance))
        return;

    if (!HasShieldEquipped(player))
        player->RemoveAura(GladiatorStance);
}

void AddGladiatorStanceScripts()
{
    RegisterSpellScript(WarriorGladiatorStanceScript);
}
