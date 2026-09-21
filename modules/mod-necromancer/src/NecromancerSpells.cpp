#include "Necromancer.h"

#include "Creature.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "Unit.h"
#include "UnitScript.h"

#include <algorithm>

using namespace Necromancer;

namespace
{
Player* GetNecromancer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return IsNecromancer(player) ? player : nullptr;
}

int32 SpellDamage(Player* player, uint32 spellId, float levelScale, float spellPowerScale, bool area = false)
{
    float const spellPower = float(std::max<int32>(0, player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)));
    return std::max<int32>(1, int32((float(player->GetLevel()) * levelScale + spellPower * spellPowerScale) *
        GetSpellDamageMultiplier(player, spellId, area) * DAMAGE_SCALE));
}

uint8 SoulCost(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_RAISE_SKELETON: return 2;
        case SPELL_RAISE_DEADEYE: return 3;
        case SPELL_RAISE_PLAGUE_MAGE: return 4;
        case SPELL_CREATE_ABOMINATION: return 8;
        default: return 0;
    }
}

class NecromancerAbilitySpellScript : public SpellScript
{
    PrepareSpellScript(NecromancerAbilitySpellScript);

    SpellCastResult CheckCast()
    {
        Player* player = GetNecromancer(GetCaster());
        if (!player)
            return SPELL_FAILED_DONT_REPORT;
        uint8 const cost = SoulCost(GetSpellInfo()->Id);
        if (cost && GetSouls(player) < cost)
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        if (GetSpellInfo()->Id == SPELL_SACRIFICIAL_PACT && !CountMinions(player))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        if (GetSpellInfo()->Id == SPELL_CORPSE_EXPLOSION)
        {
            Unit* target = GetExplTargetUnit();
            if (!target || !target->HasAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
                return SPELL_FAILED_TARGET_AURASTATE;
        }
        return SPELL_CAST_OK;
    }

    void HandleHit()
    {
        Player* player = GetNecromancer(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        if (GetSpellInfo()->Id == SPELL_SOUL_BOLT || GetSpellInfo()->Id == SPELL_DEATHLY_BRAND)
            DirectMinionsAt(player, target);
        switch (GetSpellInfo()->Id)
        {
            case SPELL_SOUL_BOLT:
                SetHitDamage(SpellDamage(player, SPELL_SOUL_BOLT, 7.5f, 0.55f));
                AddSouls(player, 1);
                break;
            case SPELL_DEATHLY_BRAND:
                AddSouls(player, 1);
                if (Aura* brand = target->GetAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
                {
                    int32 const extension = GetTalentValue(player, { { 90545, 3 }, { 90546, 6 } }) * IN_MILLISECONDS;
                    brand->SetMaxDuration(18000 + extension);
                    brand->SetDuration(18000 + extension);
                }
                break;
            case SPELL_BONE_SPEARHEAD:
                SetHitDamage(SpellDamage(player, SPELL_BONE_SPEARHEAD, 8.5f, 0.60f, true));
                AddSouls(player, 1);
                break;
            default:
                break;
        }
    }

    void AfterCastHandler()
    {
        Player* player = GetNecromancer(GetCaster());
        if (!player)
            return;
        uint32 const id = GetSpellInfo()->Id;
        uint8 const cost = SoulCost(id);
        if (cost && !SpendSouls(player, cost))
            return;

        switch (id)
        {
            case SPELL_RAISE_SKELETON: SummonMinion(player, MinionKind::Skeleton); break;
            case SPELL_RAISE_DEADEYE: SummonMinion(player, MinionKind::Archer); break;
            case SPELL_RAISE_PLAGUE_MAGE: SummonMinion(player, MinionKind::Mage); break;
            case SPELL_CREATE_ABOMINATION: SummonMinion(player, MinionKind::Abomination); break;
            case SPELL_SOUL_TAP:
                RestoreMana(player, 20);
                AddSouls(player, 2);
                break;
            case SPELL_DEATH_COMMAND:
                CommandMinions(player, GetExplTargetUnit(), 6000);
                break;
            case SPELL_CORPSE_EXPLOSION:
                ExplodeBrand(player, GetExplTargetUnit());
                break;
            case SPELL_SOUL_HARVEST:
                Harvest(player);
                break;
            case SPELL_SACRIFICIAL_PACT:
                if (SacrificeOldestMinion(player))
                {
                    player->ModifyHealth(int32(CalculatePct(player->GetMaxHealth(), 20)));
                    RestoreMana(player, 25);
                    AddSouls(player, 2);
                }
                break;
            case SPELL_BLACK_VOLLEY:
                BlackVolley(player, GetExplTargetUnit());
                break;
            case SPELL_ARMY_OF_THE_DAMNED:
                for (uint8 i = 0; i < 3; ++i) SummonMinion(player, MinionKind::Skeleton);
                for (uint8 i = 0; i < 2; ++i) SummonMinion(player, MinionKind::Archer);
                ExtendMinionDurations(player, 15000);
                break;
            case SPELL_GRAVE_TIDE:
                SummonMinion(player, MinionKind::Skeleton);
                SummonMinion(player, MinionKind::Skeleton);
                CommandMinions(player, player->GetSelectedUnit(), 6000);
                break;
            case SPELL_MASTER_OF_THE_DEAD:
                RefreshMinionDurations(player);
                break;
            default:
                break;
        }
    }

    static void ExplodeBrand(Player* player, Unit* center)
    {
        if (!center)
            return;
        int32 damage = SpellDamage(player, SPELL_CORPSE_EXPLOSION, 7.0f, 0.45f, true);
        damage = int32(float(damage) * (1.0f + std::min<uint32>(10, CountMinions(player)) * 0.04f));
        for (Unit* enemy : GetEnemiesAround(player, center, 10.0f, 8))
        {
            player->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
            // The blast sows what it just consumed: everything caught in it starts carrying the brand. The
            // target it went off on does not - it pays the mark again, which is what keeps the button honest.
            if (enemy == center)
                continue;
            if (Aura* spread = player->AddAura(SPELL_DEATHLY_BRAND, enemy))
            {
                spread->SetMaxDuration(SPREAD_BRAND_DURATION);
                spread->SetDuration(SPREAD_BRAND_DURATION);
            }
        }
        center->RemoveAura(SPELL_DEATHLY_BRAND, player->GetGUID());
    }

    static void Harvest(Player* player)
    {
        std::size_t const limit = 5 + std::size_t(GetTalentValue(player, { { 90558, 1 }, { 90559, 2 }, { 90560, 3 } }));
        int32 const baseDamage = SpellDamage(player, SPELL_SOUL_HARVEST, 6.0f, 0.35f, true);
        uint8 souls = 0;
        for (Unit* enemy : GetEnemiesAround(player, player, 12.0f, limit))
        {
            int32 damage = baseDamage;
            if (enemy->HasAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
                damage = int32(float(damage) * 1.35f);
            player->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
            ++souls;
        }
        AddSouls(player, souls);
    }

    static void BlackVolley(Player* player, Unit* center)
    {
        if (!center)
            return;
        int32 const damage = SpellDamage(player, SPELL_BLACK_VOLLEY, 8.0f, 0.50f, true);
        for (Unit* enemy : GetEnemiesAround(player, center, 10.0f, 8))
        {
            player->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
            // Black Volley is the one spell of his own that seeds the rot, so pressing the pack button starts
            // the pack ticking instead of waiting on the army to walk over and do it.
            ApplyNecroticRot(player, enemy);
            if (roll_chance_i(20))
                AddSouls(player, 1);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(NecromancerAbilitySpellScript::CheckCast);
        OnHit += SpellHitFn(NecromancerAbilitySpellScript::HandleHit);
        AfterCast += SpellCastFn(NecromancerAbilitySpellScript::AfterCastHandler);
    }
};

class NecromancerPeriodicAuraScript : public AuraScript
{
    PrepareAuraScript(NecromancerPeriodicAuraScript);

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Player* player = GetNecromancer(GetCaster());
        Unit* target = GetTarget();
        if (!player || !target)
            return;
        PreventDefaultAction();
        uint32 const spellId = GetSpellInfo()->Id;
        int32 amount;
        if (spellId == SPELL_DEATHLY_BRAND)
            amount = SpellDamage(player, spellId, 1.8f, 0.13f)
                * (100 + GetTalentValue(player, { { 90515, 10 }, { 90516, 20 }, { 90517, 30 } })) / 100;
        else if (spellId == SPELL_NECROTIC_ROT)
            // Per stack, and counted as area damage so Doctrine de la peste pays for the pack it is rotting.
            amount = SpellDamage(player, spellId, 0.45f, 0.033f, true) * int32(GetStackAmount());
        else
            amount = SpellDamage(player, spellId, 3.0f, 0.22f);

        // PreventDefaultAction above stops the core from handling this tick, and sending it to the combat log
        // is part of what it would have done. Dealing the damage by hand and saying nothing means the tick
        // lands on the target but no client ever hears about it: the damage is real, and every meter reads the
        // class as having no damage over time whatsoever. Announce it the way the core would have.
        SpellInfo const* spellInfo = GetSpellInfo();
        uint32 damage = uint32(std::max<int32>(1, amount));
        DamageInfo damageInfo(player, target, damage, spellInfo, spellInfo->GetSchoolMask(), DOT);
        Unit::CalcAbsorbResist(damageInfo);
        uint32 absorb = damageInfo.GetAbsorb();
        damage = damageInfo.GetDamage();
        Unit::DealDamageMods(target, damage, &absorb);

        uint32 const overkill = damage > target->GetHealth() ? damage - target->GetHealth() : 0;
        SpellPeriodicAuraLogInfo log(aurEff, damage, overkill, absorb, damageInfo.GetResist(), 0.0f, false);
        target->SendPeriodicAuraLog(&log);

        Unit::DealDamage(player, target, damage, nullptr, DOT, spellInfo->GetSchoolMask(), spellInfo, true);
        if (spellId == SPELL_SOUL_DRAIN)
            RestoreMana(player, 3);
    }

    void HandleRemove(AuraEffect const* /*effect*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetSpellInfo()->Id != SPELL_SOUL_DRAIN || GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;
        if (Player* player = GetNecromancer(GetCaster()))
            AddSouls(player, 2);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(NecromancerPeriodicAuraScript::HandlePeriodic, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE);
        AfterEffectRemove += AuraEffectRemoveFn(NecromancerPeriodicAuraScript::HandleRemove, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE, AURA_EFFECT_HANDLE_REAL);
    }
};

class NecromancerUnitScript : public UnitScript
{
public:
    NecromancerUnitScript() : UnitScript("NecromancerUnitScript") { }

    uint32 DealDamage(Unit* /*attacker*/, Unit* victim, uint32 damage, DamageEffectType /*type*/) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!IsNecromancer(player) || CountMinions(player) < 3)
            return damage;
        int32 reduction = GetTalentValue(player, { { 90555, 3 }, { 90556, 6 }, { 90557, 9 } });
        return reduction ? uint32(CalculatePct(damage, 100 - reduction)) : damage;
    }
};

class NecromancerPlayerScript : public PlayerScript
{
public:
    NecromancerPlayerScript() : PlayerScript("NecromancerPlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEVEL_CHANGED, PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET, PLAYERHOOK_ON_PLAYER_JUST_DIED,
        PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_MAP_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        LearnUnlockedAbilities(player);
        ApplyClassPassive(player);
    }
    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (player->GetLevel() > oldLevel)
            LearnUnlockedAbilities(player);
    }
    void OnPlayerCreatureKill(Player* player, Creature* killed) override { RewardMarkedKill(player, killed); }
    void OnPlayerCreatureKilledByPet(Player* player, Creature* killed) override { RewardMarkedKill(player, killed); }
    void OnPlayerJustDied(Player* player) override { if (IsNecromancer(player)) CleanupMinions(player); }
    void OnPlayerLogout(Player* player) override { if (IsNecromancer(player)) CleanupMinions(player); }
    void OnPlayerMapChanged(Player* player) override { if (IsNecromancer(player)) CleanupMinions(player); }

private:
    // learnSpell casts a passive only on the login where it is first learned, and every Necromancer already
    // knows this one from before it carried anything. Casting it here covers them without a reset.
    static void ApplyClassPassive(Player* player)
    {
        if (!IsNecromancer(player) || player->HasAura(SPELL_NECROMANCER_PASSIVE))
            return;
        player->CastSpell(player, SPELL_NECROMANCER_PASSIVE, true);
    }

    static void RewardMarkedKill(Player* player, Creature* killed)
    {
        if (IsNecromancer(player) && killed && killed->HasAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
            AddSouls(player, 1);
    }
};
}

void AddNecromancerMinionScripts();

void AddNecromancerScripts()
{
    AddNecromancerMinionScripts();
    new NecromancerUnitScript();
    new NecromancerPlayerScript();
    RegisterSpellScript(NecromancerAbilitySpellScript);
    RegisterSpellScript(NecromancerPeriodicAuraScript);
}
