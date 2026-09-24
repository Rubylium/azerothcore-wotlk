#include "Oathblade.h"

#include "Item.h"
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

using namespace Oathblade;

namespace
{
Player* GetOathblade(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return IsOathblade(player) ? player : nullptr;
}

void RestoreEquipmentTraining(Player* player)
{
    if (!IsOathblade(player))
        return;

    // PlayerStart.CustomSpells is normally disabled. Unlike the authored combat kit, the
    // proficiency spells in playercreateinfo_spell_custom are therefore not learned.
    for (uint32 spellId : { 201u, 674u, 8737u, 750u })
        if (!player->HasSpell(spellId))
            player->learnSpell(spellId, false);

    if (!player->GetSkillValue(SKILL_SWORDS))
        player->SetSkill(SKILL_SWORDS, 0, 1, player->GetMaxSkillValueForLevel());
    if (!player->GetSkillValue(SKILL_MAIL))
        player->SetSkill(SKILL_MAIL, 0, 1, 1);
    if (!player->GetSkillValue(SKILL_PLATE_MAIL))
        player->SetSkill(SKILL_PLATE_MAIL, 0, 1, 1);

    uint32 const armorMask = (1u << ITEM_SUBCLASS_ARMOR_MAIL) | (1u << ITEM_SUBCLASS_ARMOR_PLATE);
    if ((player->GetArmorProficiency() & armorMask) != armorMask)
    {
        player->AddArmorProficiency(armorMask);
        player->SendProficiency(ITEM_CLASS_ARMOR, player->GetArmorProficiency());
    }

    if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND))
        return;

    // The creation path put the starter sword in a bag when proficiency was missing.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || item->GetTemplate()->Class != ITEM_CLASS_WEAPON ||
            item->GetTemplate()->SubClass != ITEM_SUBCLASS_WEAPON_SWORD ||
            (item->GetTemplate()->InventoryType != INVTYPE_WEAPON &&
                item->GetTemplate()->InventoryType != INVTYPE_WEAPONMAINHAND))
            continue;

        uint16 destination = 0;
        if (player->CanEquipItem(NULL_SLOT, destination, item, false) == EQUIP_ERR_OK &&
            uint8(destination) == EQUIPMENT_SLOT_MAINHAND)
            player->SwapItem(item->GetPos(), destination);
        break;
    }
}

// The global cooldown while Flawless Form is up, against the 900 ms the spell data carries
constexpr uint32 FLAWLESS_FORM_GCD = 650;

bool IsFinisher(uint32 spellId)
{
    return spellId == SPELL_NOBLE_VERDICT || spellId == SPELL_CRESCENT_SWEEP ||
        spellId == SPELL_FINAL_EDICT || spellId == SPELL_GRAND_FLOURISH;
}

void DealSecondaryStrike(Player* player, Unit* target, int32 damage)
{
    if (player && target && damage > 0)
        player->CastCustomSpell(SPELL_SECOND_STRIKE, SPELLVALUE_BASE_POINT0, damage, target, TRIGGERED_FULL_MASK);
}

// The carrier an area ability's damage arrives on, so a meter can tell them apart
uint32 GetSweepCarrier(uint32 spellId)
{
    switch (spellId)
    {
        case SPELL_SWEEPING_ARC:
            return SPELL_SWEEPING_ARC_HIT;
        case SPELL_CRESCENT_SWEEP:
            return SPELL_CRESCENT_SWEEP_HIT;
        case SPELL_BLADE_DANCE:
            return SPELL_BLADE_DANCE_HIT;
        case SPELL_GRAND_FLOURISH:
            return SPELL_GRAND_FLOURISH_HIT;
        default:
            return SPELL_SECOND_STRIKE;
    }
}

// Serrated Cuts on a target, for its full nine seconds (the aura script sizes each tick)
void ApplyBleed(Player* player, Unit* target)
{
    if (Aura* bleed = player->AddAura(SPELL_SERRATED_CUTS, target))
    {
        bleed->SetMaxDuration(9000);
        bleed->SetDuration(9000);
    }
}

// Blade Ward raised by a talent rather than cast: no cooldown spent, no cast of its own
void GrantBladeWard(Player* player, int32 duration)
{
    if (Aura* ward = player->AddAura(SPELL_BLADE_WARD, player))
    {
        ward->SetMaxDuration(duration);
        ward->SetDuration(duration);
    }
}

void RewardFinisher(Player* player, uint8 flowSpent)
{
    if (!player || !flowSpent)
        return;
    player->ModifyPower(POWER_ENERGY, 4 * flowSpent);
    AddTempo(player, uint8(2 + GetTalentValue(player, { { 90933, 1 }, { 90934, 2 } })));
    int32 const heal = GetTalentValue(player, { { 90931, 1 }, { 90932, 2 } });
    if (heal)
        player->ModifyHealth(int32(CalculatePct(player->GetMaxHealth(), heal * flowSpent)));
    // Relentless Oath: a full finisher brings the big strikes back sooner
    if (flowSpent >= 5 && player->HasAura(SPELL_TALENT_RELENTLESS_OATH))
        for (uint32 spellId : { SPELL_ZEAL_STRIKE, SPELL_BLADE_DANCE, SPELL_FINAL_EDICT })
            player->ModifySpellCooldown(spellId, -2000);
}

class OathbladeAbilitySpellScript : public SpellScript
{
    PrepareSpellScript(OathbladeAbilitySpellScript);

    SpellCastResult CheckCast()
    {
        Player* player = GetOathblade(GetCaster());
        if (!player)
            return SPELL_FAILED_DONT_REPORT;
        uint32 const id = GetSpellInfo()->Id;
        if (IsFinisher(id) && GetFlow(player) < 2)
            return SPELL_FAILED_NO_COMBO_POINTS;
        // Riposte Ward: Blade Ward holds the opening Precise Thrust would otherwise have to make
        if (id == SPELL_REVERSAL && !player->HasAura(SPELL_LAST_PRECISE_THRUST) &&
            !(player->HasAura(SPELL_TALENT_RIPOSTE_WARD) && player->HasAura(SPELL_BLADE_WARD)))
            return SPELL_FAILED_CANT_DO_THAT_RIGHT_NOW;
        return SPELL_CAST_OK;
    }

    void HandleHit()
    {
        Player* player = GetOathblade(GetCaster());
        Unit* target = GetHitUnit();
        if (!player || !target || !player->IsValidAttackTarget(target))
            return;

        uint32 const id = GetSpellInfo()->Id;
        int32 damage = 0;
        switch (id)
        {
            case SPELL_SWIFT_CUT:
            {
                damage = GetStrikeDamage(player, 0.80f, 0.11f, id);
                player->ModifyPower(POWER_ENERGY, 22);
                bool const alternated = player->HasAura(SPELL_LAST_PRECISE_THRUST);
                AdvanceTechnique(player, id, 1);
                if (alternated)
                    player->ModifyPower(POWER_ENERGY,
                        GetTalentValue(player, { { 90914, 3 }, { 90915, 6 } }));
                break;
            }
            case SPELL_PRECISE_THRUST:
            {
                damage = GetStrikeDamage(player, 1.20f, 0.18f, id);
                bool const alternated = player->HasAura(SPELL_LAST_SWIFT_CUT);
                AdvanceTechnique(player, id, 2);
                if (alternated)
                    player->ModifyPower(POWER_ENERGY,
                        GetTalentValue(player, { { 90914, 3 }, { 90915, 6 } }));
                break;
            }
            case SPELL_NOBLE_VERDICT:
            case SPELL_FINAL_EDICT:
            {
                uint8 const flow = std::min<uint8>(5, GetFlow(player));
                float const base = id == SPELL_FINAL_EDICT ? 1.15f : 0.70f;
                float const each = id == SPELL_FINAL_EDICT ? 0.90f : 0.55f;
                damage = GetStrikeDamage(player, base + each * flow, 0.15f + 0.07f * flow, id);
                if (id == SPELL_FINAL_EDICT && player->HasAura(SPELL_PERFECT_EXECUTION))
                    damage = int32(float(damage) * 1.25f);
                // Executioner's Oath
                if (target->HealthBelowPct(35))
                    damage = int32(float(damage) *
                        (1.0f + float(GetTalentValue(player, { { 91055, 10 }, { 91056, 20 } })) / 100.0f));
                // Rending Verdict: a bleeding target takes more, and keeps bleeding
                if (player->HasAura(SPELL_TALENT_RENDING_VERDICT))
                    if (Aura* bleed = target->GetAura(SPELL_SERRATED_CUTS, player->GetGUID()))
                    {
                        damage = int32(float(damage) * 1.10f);
                        bleed->RefreshDuration();
                    }
                if (id == SPELL_NOBLE_VERDICT && player->HasAura(SPELL_TALENT_RIPOSTE_CHAIN))
                    player->RemoveSpellCooldown(SPELL_REVERSAL, true);
                if (SpendFlow(player, flow))
                    RewardFinisher(player, flow);
                break;
            }
            case SPELL_ZEAL_STRIKE:
                damage = GetStrikeDamage(player, 1.85f, 0.24f, id);
                AddFlow(player, 2);
                AddTempo(player, 2);
                break;
            case SPELL_NOBLE_ADVANCE:
                damage = GetStrikeDamage(player, 1.05f, 0.12f, id);
                AddFlow(player, 1);
                AddTempo(player, 1);
                break;
            case SPELL_REVERSAL:
                damage = GetStrikeDamage(player, 1.35f, 0.15f, id);
                player->RemoveAurasDueToSpell(SPELL_LAST_PRECISE_THRUST);
                AddFlow(player, uint8(1 + player->HasAura(90924)));
                AddTempo(player, 1);
                break;
            default:
                return;
        }
        // Duelist's Focus: every ability here is single-target, and pays more with nobody else close
        if (int32 const focus = GetTalentValue(player, { { 91048, 4 }, { 91049, 8 } }))
            if (GetNearbyEnemies(player, player, 8.0f, 2).size() <= 1)
                damage = int32(float(damage) * (1.0f + float(focus) / 100.0f));
        SetHitDamage(damage);
        if ((id == SPELL_SWIFT_CUT || id == SPELL_PRECISE_THRUST) &&
            GetTalentValue(player, { { 90928, 1 }, { 90929, 2 } }))
            ApplyBleed(player, target);
        // Flawless Form's echo is a third of the strike, half with Singular Form; Twin Blades' is always a third
        bool const echo = InFlawlessForm(player);
        if (echo || (id == SPELL_PRECISE_THRUST && roll_chance_i(uint32(GetTalentValue(player,
                { { 90916, 15 }, { 90917, 30 } })))))
            DealSecondaryStrike(player, target, std::max<int32>(1,
                damage / (echo && player->HasAura(SPELL_TALENT_SINGULAR_FORM) ? 2 : 3)));
    }

    // Flawless Form is the burst window, and it should be felt in the hands: while it is up the global
    // cooldown is cut again, on top of the 900 ms the spell data already asks for.
    //
    // It is done here rather than with a SPELLMOD_GLOBAL_COOLDOWN aura because a spell whose
    // StartRecoveryTime is under MIN_GCD skips the whole block that applies spell mods - the same property
    // that lets the base sit below a second in the first place. AddGlobalCooldown overwrites the entry for
    // the category, so this replaces the one the cast just started rather than adding to it.
    static void QuickenGlobalCooldown(Player* player, SpellInfo const* spellInfo)
    {
        if (!spellInfo || !spellInfo->StartRecoveryCategory || !InFlawlessForm(player))
            return;

        player->GetGlobalCooldownMgr().AddGlobalCooldown(spellInfo, FLAWLESS_FORM_GCD);
    }

    void AfterCastHandler()
    {
        Player* player = GetOathblade(GetCaster());
        if (!player)
            return;
        QuickenGlobalCooldown(player, GetSpellInfo());
        uint32 const id = GetSpellInfo()->Id;
        switch (id)
        {
            case SPELL_SWEEPING_ARC:
                Sweep(player, id);
                ShortenCooldown(player, id, GetTalentValue(player, { { 91042, 1500 }, { 91043, 3000 } }));
                break;
            case SPELL_BLADE_DANCE:
                Sweep(player, id);
                if (player->HasAura(SPELL_TALENT_RELENTLESS_STORM))
                    ShortenCooldown(player, id, 4000);
                break;
            case SPELL_CRESCENT_SWEEP:
            case SPELL_GRAND_FLOURISH:
                Sweep(player, id);
                break;
            case SPELL_FLOURISH:
                player->ModifyPower(POWER_ENERGY, 45);
                AddFlow(player, 2);
                AddTempo(player, 2);
                ShortenCooldown(player, id, GetTalentValue(player, { { 91017, 4000 }, { 91018, 8000 } }));
                break;
            case SPELL_RALLY:
                player->ModifyHealth(int32(CalculatePct(player->GetMaxHealth(),
                    18 + (player->HasAura(SPELL_TALENT_RALLYING_OATH) ? 10 : 0))));
                if (player->HasAura(SPELL_TALENT_SECOND_BREATH))
                    ShortenCooldown(player, id, 15000);
                break;
            case SPELL_BLADE_WARD:
                if (Aura* ward = player->GetAura(SPELL_BLADE_WARD, player->GetGUID()))
                {
                    int32 const duration = player->HasAura(SPELL_TALENT_ENDURING_WARD) ? 9000 : 6000;
                    ward->SetMaxDuration(duration);
                    ward->SetDuration(duration);
                }
                // Swift Recovery
                if (int32 const heal = GetTalentValue(player, { { 90954, 5 }, { 90955, 10 } }))
                    player->ModifyHealth(int32(CalculatePct(player->GetMaxHealth(), heal)));
                break;
            case SPELL_ZEAL_STRIKE:
                ShortenCooldown(player, id, GetTalentValue(player, { { 91060, 2000 }, { 91061, 4000 } }));
                break;
            case SPELL_NOBLE_ADVANCE:
                ShortenCooldown(player, id, GetTalentValue(player, { { 91005, 3000 }, { 91006, 6000 } }));
                break;
            case SPELL_FINAL_EDICT:
                if (player->HasAura(SPELL_TALENT_EDICT_MASTERY))
                    ShortenCooldown(player, id, 5000);
                break;
            default:
                break;
        }
    }

    // The cooldown the cast just started, taken down by a talent. Runs after the cast, once it exists.
    static void ShortenCooldown(Player* player, uint32 spellId, int32 milliseconds)
    {
        if (milliseconds > 0)
            player->ModifySpellCooldown(spellId, -milliseconds);
    }

    static void Sweep(Player* player, uint32 spellId)
    {
        std::size_t const limit = std::size_t(6 + GetTalentValue(player, { { 90936, 1 }, { 90937, 2 } }));
        // Wide Arcs
        float const radius = 8.0f + float(GetTalentValue(player, { { 91052, 2 }, { 91053, 4 } }));
        std::list<Unit*> enemies = GetNearbyEnemies(player, player, radius, limit);
        if (enemies.empty())
            return;
        uint8 flow = 0;
        if (IsFinisher(spellId))
        {
            flow = std::min<uint8>(5, GetFlow(player));
            if (!SpendFlow(player, flow))
                return;
        }
        float const weaponScale = spellId == SPELL_SWEEPING_ARC ? 0.70f :
            spellId == SPELL_BLADE_DANCE ? 0.85f :
            spellId == SPELL_CRESCENT_SWEEP ? 0.55f + 0.42f * flow : 0.95f + 0.65f * flow;
        int32 const damage = GetStrikeDamage(player, weaponScale, 0.11f + 0.04f * flow, spellId);
        uint32 const carrier = GetSweepCarrier(spellId);
        // Tempest Form: during Flawless Form the sweep lands twice, the second time for a third
        bool const tempest = InFlawlessForm(player) && player->HasAura(SPELL_TALENT_TEMPEST_FORM);
        // Blade Tempest: the area finishers leave every enemy they hit bleeding
        bool const spreadsBleed = IsFinisher(spellId) && player->HasAura(SPELL_TALENT_BLADE_TEMPEST);
        for (Unit* enemy : enemies)
        {
            if (damage > 0)
                player->CastCustomSpell(carrier, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
            if (tempest && damage > 0 && enemy->IsAlive())
                player->CastCustomSpell(carrier, SPELLVALUE_BASE_POINT0, std::max<int32>(1, damage / 3), enemy,
                    TRIGGERED_FULL_MASK);
            if (spreadsBleed && enemy->IsAlive())
                ApplyBleed(player, enemy);
        }
        if (flow)
        {
            RewardFinisher(player, flow);
            if (enemies.size() >= 3 && player->HasAura(SPELL_TALENT_CRESCENT_MOMENTUM))
                AddFlow(player, 1);
        }
        else
        {
            AddFlow(player, uint8(enemies.size() >= 3 ? 2 : 1));
            // Storm Rhythm
            int32 const stormChance = GetTalentValue(player, { { 91046, 25 }, { 91047, 50 } });
            AddTempo(player, uint8(1 + (stormChance && roll_chance_i(uint32(stormChance)) ? 1 : 0)));
        }
        if (spellId == SPELL_BLADE_DANCE)
            player->ModifyPower(POWER_ENERGY, 20);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(OathbladeAbilitySpellScript::CheckCast);
        OnHit += SpellHitFn(OathbladeAbilitySpellScript::HandleHit);
        AfterCast += SpellCastFn(OathbladeAbilitySpellScript::AfterCastHandler);
    }
};

class OathbladeBleedAuraScript : public AuraScript
{
    PrepareAuraScript(OathbladeBleedAuraScript);

    void CalculateDamage(AuraEffect const* /*effect*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Player* player = GetOathblade(GetCaster());
        if (!player)
            return;
        // Serrated Cuts' rank, or its first when Blade Tempest spread the bleed without it
        int32 const rank = std::max<int32>(GetTalentValue(player, { { 90928, 1 }, { 90929, 2 } }),
            player->HasAura(SPELL_TALENT_BLADE_TEMPEST) ? 1 : 0);
        amount = std::max<int32>(1, int32(player->GetTotalAttackPowerValue(BASE_ATTACK) * 0.02f * rank));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(OathbladeBleedAuraScript::CalculateDamage, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE);
    }
};

class OathbladeUnitScript : public UnitScript
{
public:
    OathbladeUnitScript() : UnitScript("OathbladeUnitScript") { }

    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType /*type*/) override
    {
        // Warden's Oath: behind Blade Ward, every strike mends the Oathblade
        if (Player* striker = attacker ? attacker->ToPlayer() : nullptr)
            if (damage && victim != attacker && IsOathblade(striker) && striker->HasAura(SPELL_BLADE_WARD) &&
                striker->HasAura(SPELL_TALENT_WARDENS_OATH))
                striker->ModifyHealth(int32(CalculatePct(damage, 15)));

        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!IsOathblade(player) || !damage)
            return damage;

        // Oathbound Resolve: a hit that takes the Oathblade under 35% raises Blade Ward first, once a minute, so
        // the ward already softens that hit
        if (!player->HasAura(SPELL_BLADE_WARD) && player->HasAura(SPELL_TALENT_OATHBOUND_RESOLVE) &&
            !player->HasSpellCooldown(SPELL_TALENT_OATHBOUND_RESOLVE) && damage < player->GetHealth() &&
            player->GetHealth() - damage < CalculatePct(player->GetMaxHealth(), 35))
        {
            player->AddSpellCooldown(SPELL_TALENT_OATHBOUND_RESOLVE, 0, 60000);
            GrantBladeWard(player, 4000);
        }

        int32 reduction = 0;
        if (player->HasAura(SPELL_BLADE_WARD))
            reduction += 25 + GetTalentValue(player, { { 90939, 4 }, { 90940, 8 } });
        reduction += GetTalentValue(player, { { 90951, 2 }, { 90952, 4 } });
        if (reduction)
            damage = uint32(CalculatePct(damage, 100 - std::min<int32>(75, reduction)));

        return damage;
    }
};

class OathbladePlayerScript : public PlayerScript
{
public:
    OathbladePlayerScript() : PlayerScript("OathbladePlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEVEL_CHANGED
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        RestoreEquipmentTraining(player);
        LearnUnlockedAbilities(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (!IsOathblade(player))
            return;
        RestoreEquipmentTraining(player);
        if (player->GetLevel() < oldLevel)
            ForgetAbilitiesAboveLevel(player, player->GetLevel());
        else
            LearnUnlockedAbilities(player);
    }
};
}

void AddOathbladeScripts()
{
    new OathbladeUnitScript();
    new OathbladePlayerScript();
    RegisterSpellScript(OathbladeAbilitySpellScript);
    RegisterSpellScript(OathbladeBleedAuraScript);
}
