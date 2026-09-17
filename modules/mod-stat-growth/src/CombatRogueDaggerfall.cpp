#include "CombatRogueDaggerfall.h"

#include "CombatRogueCommon.h"

#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellScript.h"

#include <algorithm>

using namespace CombatRogue;

namespace
{
Player* GetRogue(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return IsRogue(player) ? player : nullptr;
}

bool HasOwnedDamageOverTime(Player const* player, Unit const* target)
{
    for (auto const& [spellId, application] : target->GetAppliedAuras())
    {
        (void)spellId;
        Aura const* aura = application->GetBase();
        if (!aura || aura->GetCasterGUID() != player->GetGUID())
            continue;

        SpellInfo const* spellInfo = aura->GetSpellInfo();
        uint8 const effectMask = application->GetEffectMask();
        for (uint8 index = EFFECT_0; index < MAX_SPELL_EFFECTS; ++index)
        {
            if (!(effectMask & (1 << index)))
                continue;

            AuraType const auraType = AuraType(spellInfo->Effects[index].ApplyAuraName);
            if (auraType == SPELL_AURA_PERIODIC_DAMAGE || auraType == SPELL_AURA_PERIODIC_DAMAGE_PERCENT ||
                auraType == SPELL_AURA_PERIODIC_LEECH)
                return true;
        }
    }
    return false;
}

// 90105 Crimson Daggerfall: cooldown AoE finisher. The DBC sends a dagger missile and a randomized custom
// impact sound to every target selected by effect 0; this script owns scaling and finisher integration.
class CombatRogueCrimsonDaggerfallScript : public SpellScript
{
    PrepareSpellScript(CombatRogueCrimsonDaggerfallScript);

    uint8 _comboPoints = 0;
    uint32 _overflowEnergy = 0;

    void HandleBeforeCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        _comboPoints = std::max<uint8>(player->GetComboPoints(), 1);
        _overflowEnergy = TakeOverflowEnergy(player, GetSpellInfo()->ManaCost);
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        float const radius = GetAoeRadius(player, false);
        targets.remove_if([player, radius](WorldObject* target)
        {
            return !target || !player->IsWithinDistInMap(target, radius);
        });
    }

    void HandleHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;

        float multiplier = float(_comboPoints) * GetOverflowMultiplier(_overflowEnergy) *
            GetExecutionerMultiplier(player, target);

        // The DBC starts at 60% weapon damage per point; level 45 raises it to 70%.
        if (HasEvolution(player, EVOLUTION_DAGGERFALL_DAMAGE))
            multiplier *= 70.0f / 60.0f;

        if (HasOwnedDamageOverTime(player, target))
            multiplier *= HasEvolution(player, EVOLUTION_DAGGERFALL_DOT_BONUS) ? 1.50f : 1.30f;

        SetHitDamage(int32(GetHitDamage() * multiplier));
    }

    void HandleAfterCast()
    {
        if (Player* player = GetRogue(GetCaster()))
            OnFinisherCast(player, _comboPoints, GetExplTargetUnit());
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueCrimsonDaggerfallScript::HandleBeforeCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            CombatRogueCrimsonDaggerfallScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(CombatRogueCrimsonDaggerfallScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueCrimsonDaggerfallScript::HandleAfterCast);
    }
};
}

void AddCombatRogueDaggerfallScripts()
{
    RegisterSpellScript(CombatRogueCrimsonDaggerfallScript);
}
