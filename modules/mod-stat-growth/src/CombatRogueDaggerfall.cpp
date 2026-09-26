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
    return IsCombatRogue(player) ? player : nullptr;
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
// Its cooldown resets when it kills an enemy (and when Quick Cut spends an Opening, see CombatRogueScripts.cpp).
class CombatRogueCrimsonDaggerfallScript : public SpellScript
{
    PrepareSpellScript(CombatRogueCrimsonDaggerfallScript);

    uint8 _comboPoints = 0;
    uint32 _overflowEnergy = 0;
    // Rained for free by another finisher (see OnFinisherCast): full strength, no energy, no finisher effects
    bool _free = false;

    void HandleBeforeCast()
    {
        Player* player = GetRogue(GetCaster());
        if (!player)
            return;

        _free = GetSpell()->IsTriggered();
        if (_free)
        {
            _comboPoints = 5;
            return;
        }

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
        if (Player* player = GetRogue(GetCaster()); player && !_free)
            OnFinisherCast(player, _comboPoints, GetExplTargetUnit(), true);
    }

    // The daggers land after the cast: a kill among them readies it again
    void HandleAfterHit()
    {
        Player* player = GetRogue(GetCaster());
        Unit* target = GetHitUnit();
        if (player && target && !target->IsAlive() && player->HasSpellCooldown(SPELL_CRIMSON_DAGGERFALL))
            player->RemoveSpellCooldown(SPELL_CRIMSON_DAGGERFALL, true);
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(CombatRogueCrimsonDaggerfallScript::HandleBeforeCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(
            CombatRogueCrimsonDaggerfallScript::FilterTargets, EFFECT_0, TARGET_UNIT_DEST_AREA_ENEMY);
        OnHit += SpellHitFn(CombatRogueCrimsonDaggerfallScript::HandleHit);
        AfterCast += SpellCastFn(CombatRogueCrimsonDaggerfallScript::HandleAfterCast);
        AfterHit += SpellHitFn(CombatRogueCrimsonDaggerfallScript::HandleAfterHit);
    }
};
}

void AddCombatRogueDaggerfallScripts()
{
    RegisterSpellScript(CombatRogueCrimsonDaggerfallScript);
}
