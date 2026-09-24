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

// The cooldown the cast just started, taken down by a talent. Runs after the cast, once it exists.
void ShortenCooldown(Player* player, uint32 spellId, int32 milliseconds)
{
    if (milliseconds > 0)
        player->ModifySpellCooldown(spellId, -milliseconds);
}

class NecromancerAbilitySpellScript : public SpellScript
{
    PrepareSpellScript(NecromancerAbilitySpellScript);

    SpellCastResult CheckCast()
    {
        Player* player = GetNecromancer(GetCaster());
        if (!player)
            return SPELL_FAILED_DONT_REPORT;
        switch (GetSpellInfo()->Id)
        {
            case SPELL_RAISE_DEAD:
                if (GetSouls(player) < GetRaiseDeadCost(player))
                    return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
                break;
            case SPELL_SACRIFICIAL_PACT:
                if (!CountMinions(player) || (CountMinions(player) == 1 && HasAbomination(player)))
                    return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
                break;
            case SPELL_CORPSE_EXPLOSION:
            {
                Unit* target = GetExplTargetUnit();
                if (!target || !target->HasAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
                    return SPELL_FAILED_TARGET_AURASTATE;
                break;
            }
            default:
                break;
        }
        return SPELL_CAST_OK;
    }

    void HandleHit()
    {
        Player* player = GetNecromancer(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target)
            return;
        switch (GetSpellInfo()->Id)
        {
            case SPELL_SOUL_BOLT:
            {
                DirectMinionsAt(player, target);
                float multiplier = 1.0f;
                // Exécution funèbre
                if (target->HealthBelowPct(35))
                    multiplier += float(GetTalentValue(player, { { 90443, 15 }, { 90444, 30 } })) / 100.0f;
                // Main de la mort
                if (player->HasAura(TALENT_HAND_OF_DEATH))
                    multiplier += 0.03f * float(CountMinions(player));
                SetHitDamage(int32(float(SpellDamage(player, SPELL_SOUL_BOLT, 8.0f, 0.60f)) * multiplier));
                AddGeneratedSouls(player, 1);
                // Fil d'âme
                if (player->HasAura(TALENT_SOUL_THREAD))
                    HealMostWoundedMinion(player, 10);
                break;
            }
            case SPELL_DEATHLY_BRAND:
                DirectMinionsAt(player, target);
                AddGeneratedSouls(player, 1);
                if (Aura* brand = target->GetAura(SPELL_DEATHLY_BRAND, player->GetGUID()))
                {
                    // Sceau éternel
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
        switch (id)
        {
            case SPELL_RAISE_DEAD:
                if (SpendSouls(player, GetRaiseDeadCost(player)))
                {
                    // Nécropole: the army already out is mended, and Ordre de mort is ready to throw them all in
                    if (player->HasAura(TALENT_NECROPOLIS))
                    {
                        HealMinions(player, 25);
                        player->RemoveSpellCooldown(SPELL_DEATH_COMMAND, true);
                    }
                    RaiseSquad(player);
                }
                break;
            case SPELL_CREATE_ABOMINATION:
                SummonMinion(player, MinionKind::Abomination);
                break;
            case SPELL_SOUL_TAP:
                // Esprit tourmenté
                RestoreMana(player, uint8(20 + GetTalentValue(player, { { 90584, 10 }, { 90585, 20 } })));
                AddSouls(player, 2);
                // Ponction vorace
                if (player->HasAura(TALENT_HUNGRY_SOULS))
                    ShortenCooldown(player, id, 10 * IN_MILLISECONDS);
                break;
            case SPELL_DEATH_COMMAND:
            {
                bool const masterOfTombs = player->HasAura(TALENT_MASTER_OF_TOMBS);
                CommandMinions(player, GetExplTargetUnit(), 6000, masterOfTombs ? 50 : 30);
                if (masterOfTombs)
                    ShortenCooldown(player, id, 4 * IN_MILLISECONDS);
                // Commandant
                if (player->HasAura(TALENT_COMMANDER))
                    StartCommander(player, 6000);
                break;
            }
            case SPELL_CORPSE_EXPLOSION:
                ExplodeBrand(player, GetExplTargetUnit());
                break;
            case SPELL_SOUL_HARVEST:
                Harvest(player);
                // Faux des âmes
                ShortenCooldown(player, id, GetTalentValue(player, { { 90441, 4 }, { 90442, 8 } }) * IN_MILLISECONDS);
                break;
            case SPELL_SACRIFICIAL_PACT:
                Sacrifice(player);
                // Pacte des ombres
                ShortenCooldown(player, id, GetTalentValue(player, { { 90573, 10 }, { 90574, 20 } }) * IN_MILLISECONDS);
                break;
            case SPELL_BLACK_VOLLEY:
                BlackVolley(player, GetExplTargetUnit());
                break;
            case SPELL_ARMY_OF_THE_DAMNED:
                RaiseSquad(player);
                HealMinions(player, 100);
                // Marche implacable
                if (player->HasAura(TALENT_RELENTLESS_MARCH))
                    ShortenCooldown(player, id, 60 * IN_MILLISECONDS);
                break;
            case SPELL_GRAVE_TIDE:
                SummonMinion(player, MinionKind::Skeleton);
                SummonMinion(player, MinionKind::Skeleton);
                CommandMinions(player, player->GetSelectedUnit(), 6000,
                    player->HasAura(TALENT_MASTER_OF_TOMBS) ? 50 : 30);
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
        bool const contagion = player->HasAura(TALENT_CONTAGION);
        for (Unit* enemy : GetEnemiesAround(player, center, 10.0f, 8))
        {
            player->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
            // Contagion
            if (contagion && enemy->IsAlive())
                ApplyNecroticRot(player, enemy, NECROTIC_ROT_MAX_STACKS);
            // The blast sows what it just consumed: everything caught in it starts carrying the brand. The
            // target it went off on does not - it pays the mark again, which is what keeps the button honest.
            if (enemy == center || !enemy->IsAlive())
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
        // Moisson vorace: one more target a rank
        std::size_t const limit = 5 + std::size_t(GetTalentValue(player, { { 90558, 1 }, { 90559, 2 } }));
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
        AddSouls(player, std::min<uint8>(5, souls));
    }

    // The weakest minion bursts where it stands; the Nécromancien drinks what it held
    static void Sacrifice(Player* player)
    {
        Position where;
        if (!SacrificeWeakestMinion(player, where))
            return;

        int32 damage = SpellDamage(player, SPELL_SACRIFICIAL_PACT, 7.0f, 0.45f, true);
        // Sacrifice exalté
        if (player->HasAura(TALENT_EXALTED_SACRIFICE))
            damage = int32(float(damage) * 1.5f);
        // Enemies around the fallen minion: the search runs from the Nécromancien, wide enough to hold any spot
        // his army can stand on, and keeps what is within 8 yards of the burst
        for (Unit* enemy : GetEnemiesAround(player, player, 50.0f, 40))
            if (enemy->GetExactDist(&where) <= 8.0f)
                player->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy,
                    TRIGGERED_FULL_MASK);

        player->ModifyHealth(int32(CalculatePct(player->GetMaxHealth(), 20)));
        RestoreMana(player, 25);
        AddSouls(player, 2);
        // Sang pour sang
        if (player->HasAura(TALENT_BLOOD_FOR_BLOOD))
            HealMinions(player, 30);
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
            // Marque profonde
            amount = SpellDamage(player, spellId, 1.8f, 0.13f)
                * (100 + GetTalentValue(player, { { 90515, 15 }, { 90516, 30 } })) / 100;
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

        if (spellId == SPELL_DEATHLY_BRAND)
        {
            // Nécrose
            int32 const chance = GetTalentValue(player, { { 90439, 10 }, { 90440, 20 } });
            if (chance && roll_chance_i(chance))
                AddSouls(player, 1);
        }
        else if (spellId == SPELL_SOUL_DRAIN)
            Transfuse(player, damage);
    }

    // Drain d'âme is what keeps the army standing: every pulse pours the stolen life into the minions
    void Transfuse(Player* player, uint32 damage)
    {
        // Transfusion
        uint32 const heal = DRAIN_HEAL_PERCENT * (100 + GetTalentValue(player, { { 90564, 25 }, { 90565, 50 } }));
        HealMinions(player, std::max<uint32>(1, heal / 100));
        RestoreMana(player, 3);
        // Siphon vital
        if (int32 const siphon = GetTalentValue(player, { { 90571, 15 }, { 90572, 30 } }))
        {
            HealInfo healInfo(player, player, CalculatePct(damage, siphon), GetSpellInfo(),
                GetSpellInfo()->GetSchoolMask());
            player->HealBySpell(healInfo);
        }
    }

    void HandleRemove(AuraEffect const* /*effect*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetSpellInfo()->Id != SPELL_SOUL_DRAIN || GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;
        // A drain carried to its end: one Âme, and Drain gourmand a second
        if (Player* player = GetNecromancer(GetCaster()))
            AddSouls(player, player->HasAura(TALENT_GREEDY_DRAIN) ? 2 : 1);
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
        if (!IsNecromancer(player) || !damage)
            return damage;

        uint32 const minions = CountMinions(player);
        int32 reduction = 0;
        // Volonté nécrotique
        if (minions >= 3)
            reduction += GetTalentValue(player, { { 90555, 3 }, { 90556, 6 } });
        // Linceul d'os
        if (player->HasAura(SPELL_BONE_SHROUD))
            reduction += 30;
        // Garde de chair
        if (player->HasAura(TALENT_FLESH_GUARD) && HasAbomination(player))
            reduction += 10;
        if (reduction)
            damage = uint32(CalculatePct(damage, 100 - std::min<int32>(75, reduction)));
        return damage;
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
        else if (player->GetLevel() < oldLevel)
            ForgetAbilitiesAboveLevel(player, player->GetLevel());
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
