#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"

#include <algorithm>
#include <list>
#include <vector>

// The Rogue's talents on the retail-style trees (localTools/rogue/talentTree.json) that spell data cannot carry, and
// two changes every rogue gets: Backstab works from any side (20% more damage from behind), and the poisons put on
// the weapons never wear off. A talent is its rank spell's aura on the rogue (learned by mod-custom-classes'
// TalentTree.cpp), read here with HasAura; the abilities and auras named below are in localTools/rogue/Spells.ps1.
// The Combat tree is the Crimson Duelist rework, scripted by mod-stat-growth; the WotLK talents the trees reuse keep
// their own scripts in the core.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_IMPROVED_SPRINT      = 92200,
    TALENT_FEINT                = 92203,
    TALENT_EVASION              = 92204,
    TALENT_ALACRITY_1           = 92205,
    TALENT_ALACRITY_2           = 92206,
    TALENT_LEECHING_POISON      = 92207,
    TALENT_SHADOW_MOMENTUM      = 92213,
    TALENT_MASTER_ASSASSIN      = 92219,
    TALENT_RESTLESS_BLADES      = 92220,
    TALENT_ZOLDYCK_1            = 92235,
    TALENT_ZOLDYCK_2            = 92236,
    TALENT_VENOMOUS_WOUNDS      = 92237,
    TALENT_COLD_BLOODED         = 92238,
    TALENT_TWIN_BLADES          = 92241,
    TALENT_VENOM_MASTER         = 92242,
    TALENT_DARK_SHADOW          = 92250,
    TALENT_WEAPONMASTER_1       = 92251,
    TALENT_WEAPONMASTER_2       = 92252,
    TALENT_SHOT_IN_THE_DARK     = 92253,
    TALENT_DEEPER_DAGGERS_1     = 92254,
    TALENT_DEEPER_DAGGERS_2     = 92255,
    TALENT_NIGHT_TERRORS        = 92256,
    TALENT_SHADOW_FOCUS         = 92260,
    TALENT_DEATH_DANCE          = 92261,
    TALENT_INVISIBLE_KILLER     = 92262,
};

// The abilities and auras of localTools/rogue/Spells.ps1
enum Spells : uint32
{
    SPELL_FEINT_REDUCTION       = 92300,
    SPELL_EVASION_REDUCTION     = 92301,
    SPELL_ALACRITY              = 92302,
    SPELL_SHADOW_MOMENTUM       = 92303,
    SPELL_MASTER_ASSASSIN       = 92304,
    SPELL_LEECHING_POISON       = 92305,
    SPELL_MARKED_FOR_DEATH      = 92311,
    SPELL_VENDETTA              = 92312,
    SPELL_EXSANGUINATE          = 92313,
    SPELL_SHURIKEN_STORM        = 92321,
    SPELL_SHADOW_BLADES         = 92322,
    SPELL_SHOT_IN_THE_DARK      = 92323,
    SPELL_DEEPER_DAGGERS        = 92324,
    SPELL_NIGHT_TERRORS         = 92325,
    SPELL_SHADOW_FOCUS          = 92326,
    SPELL_ASSASSINATION_PASSIVE = 92192,
    SPELL_SUBTLETY_PASSIVE      = 92193,
    SPELL_CRIMSON_TEMPEST       = 92330,
    SPELL_CRIMSON_TEMPEST_BLEED = 92331,
    SPELL_VIRULENCE             = 92332,
    SPELL_BLACK_POWDER          = 92340,
    SPELL_SECRET_TECHNIQUE      = 92341,

    // Stock
    SPELL_COLD_BLOOD            = 14177,
    SPELL_MUTILATE              = 1329,
    SPELL_SHADOW_DANCE          = 51713,
    SPELL_FAN_OF_KNIVES         = 51723,
};

// Rogue family flags of the stock spells the talents watch (SpellFamilyFlags words 0 and 1)
constexpr uint32 FLAG0_BACKSTAB = 0x00000004;
constexpr uint32 FLAG0_EVASION = 0x00000020;
constexpr uint32 FLAG0_SPRINT = 0x00000040;
constexpr uint32 FLAG0_GARROTE = 0x00000100;
constexpr uint32 FLAG0_VANISH = 0x00000800;
constexpr uint32 FLAG0_RUPTURE = 0x00100000;
constexpr uint32 FLAG0_BLIND = 0x01000000;
constexpr uint32 FLAG0_HEMORRHAGE = 0x02000000;
constexpr uint32 FLAG0_FEINT = 0x08000000;
constexpr uint32 FLAG1_ENVENOM = 0x00000008;
constexpr uint32 FLAG1_SHADOWSTEP = 0x00000200;
constexpr uint32 FLAG1_CLOAK_OF_SHADOWS = 0x00010000;

constexpr uint32 MarkedForDeathMs = 60000;
constexpr uint8 AlacrityMaxStacks = 5;
constexpr uint32 PoisonRefreshMs = 5 * MINUTE * IN_MILLISECONDS;
constexpr uint32 PoisonDurationMs = HOUR * IN_MILLISECONDS;
constexpr uint8 ShurikenMaxPoints = 5;

// The specs' area kits (Assassinat: bleeds and poisons everywhere; Finesse: Shuriken Storm, Black Powder, Secret
// Technique). Amounts are shares of the rogue's attack power per combo point spent, so they grow with the character.
constexpr float AreaRadius = 10.0f;
constexpr float CrimsonTempestHitPerPoint = 0.03f;      // the slash on every enemy
constexpr float CrimsonTempestBleedPerPoint = 0.06f;    // the whole bleed, over 2 s per point
constexpr uint32 CrimsonTempestTickMs = 2000;
constexpr float BlackPowderPerPoint = 0.05f;
constexpr uint32 BlackPowderFlatPerPoint = 80;
constexpr float SecretTechniquePerPoint = 0.04f;        // each of its three strikes
constexpr uint32 SecretTechniqueFlatPerPoint = 60;
constexpr uint32 SecretTechniqueStrikes = 3;
// Envenom carries the target's bleeds to this many enemies around it that do not have them
constexpr uint32 SpreadTargets = 4;
// Virulence: 2% damage per affliction of the rogue on its enemies (the aura's own amount), up to this many
constexpr uint8 VirulenceMaxStacks = 15;
constexpr float VirulenceRange = 40.0f;
constexpr uint32 VirulenceUpdateMs = 1000;

// A character's state for its talents. Kept on the player, so it dies with the session.
struct RogueState : public DataMap::Base
{
    ObjectGuid markedTarget;         // Marqué pour la mort: its target, until markedUntil
    uint32 markedUntil = 0;
    ObjectGuid shurikenTarget;       // Tempête de shurikens: where its combo points go, and how many it gave
    uint8 shurikenPoints = 0;
    uint32 poisonTimer = 5000;       // Poisons tenaces: the next refresh of the weapons' poisons
    uint32 virulenceTimer = 0;       // Virulence: the next count of the afflictions
};

constexpr char const* StateKey = "RogueTalentState";

RogueState* GetState(Player* player)
{
    return player->CustomData.GetDefault<RogueState>(StateKey);
}

Player* RoguePlayer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_ROGUE ? player : nullptr;
}

bool IsRogueSpell(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE;
}

bool HasFlag0(SpellInfo const* spellInfo, uint32 flag)
{
    return IsRogueSpell(spellInfo) && (spellInfo->SpellFamilyFlags[0] & flag);
}

bool HasFlag1(SpellInfo const* spellInfo, uint32 flag)
{
    return IsRogueSpell(spellInfo) && (spellInfo->SpellFamilyFlags[1] & flag);
}

uint32 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

// A two-rank talent's rank
uint8 Rank(Player* player, uint32 first, uint32 second)
{
    return player->HasAura(second) ? 2 : player->HasAura(first) ? 1 : 0;
}

void AddStack(Player* player, uint32 spellId, uint8 max)
{
    if (Aura* aura = player->GetAura(spellId))
    {
        if (aura->GetStackAmount() < max)
            aura->SetStackAmount(aura->GetStackAmount() + 1);
        aura->RefreshDuration();
        return;
    }
    player->AddAura(spellId, player);
}

void Energize(Player* player, uint32 spellId, uint32 amount)
{
    player->EnergizeBySpell(player, spellId, amount, POWER_ENERGY);
}

// Damage under a spell's name, of an amount the talent works out: logged as that spell, absorbs applied
void DealNamed(Player* player, Unit* target, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !amount || !target->IsAlive())
        return;

    SpellNonMeleeDamage log(player, target, spellInfo, spellInfo->GetSchoolMask());
    log.damage = amount;
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    player->SendSpellNonMeleeDamageLog(&log);
    player->DealSpellDamage(&log, false);
}

bool IsBleed(SpellInfo const* spellInfo)
{
    return HasFlag0(spellInfo, FLAG0_RUPTURE | FLAG0_GARROTE) ||
        (spellInfo && spellInfo->Id == SPELL_CRIMSON_TEMPEST_BLEED);
}

bool IsAssassination(Player* player)
{
    return player->HasAura(SPELL_ASSASSINATION_PASSIVE);
}

std::list<Unit*> EnemiesAround(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> found;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, found, check);
    Cell::VisitObjects(center, searcher, range);
    std::list<Unit*> enemies;
    for (Unit* unit : found)
        if (unit->IsAlive() && player->IsValidAttackTarget(unit) && !unit->IsTotem())
            enemies.push_back(unit);
    return enemies;
}

// Damage an ability of the kit works out, dealt as that ability: the rogue's damage bonuses (Symbols of Death,
// Virulence, ...) and the target's, a critical strike on the rogue's melee chance, armour for physical ones
void DealAbility(Player* player, Unit* target, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !amount || !target->IsAlive())
        return;

    uint32 damage = player->SpellDamageBonusDone(target, spellInfo, amount, SPELL_DIRECT_DAMAGE, EFFECT_0);
    damage = target->SpellDamageBonusTaken(player, spellInfo, damage, SPELL_DIRECT_DAMAGE);
    bool const crit = roll_chance_f(player->GetUnitCriticalChance(BASE_ATTACK, target));
    SpellNonMeleeDamage log(player, target, spellInfo, spellInfo->GetSchoolMask());
    player->CalculateSpellDamageTaken(&log, int32(damage), spellInfo, BASE_ATTACK, crit);
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    player->SendSpellNonMeleeDamageLog(&log);
    player->DealSpellDamage(&log, true);
}

uint32 PerPoint(Player* player, float apShare, uint32 flat, uint8 comboPoints)
{
    float const ap = player->GetTotalAttackPowerValue(BASE_ATTACK);
    return uint32((ap * apShare + float(flat)) * comboPoints);
}

// Puts an aura of the rogue on a unit with the amount and time left it is given (a bleed spread or set by the kit)
void PlaceBleed(Player* player, Unit* target, uint32 spellId, int32 amount, int32 duration)
{
    Aura* aura = player->AddAura(spellId, target);
    if (!aura)
        return;
    aura->SetMaxDuration(std::max(duration, 1));
    aura->SetDuration(std::max(duration, 1));
    for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
        if (AuraEffect* effect = aura->GetEffect(index))
            if (effect->GetAuraType() == SPELL_AURA_PERIODIC_DAMAGE)
                effect->ChangeAmount(amount);
}

// Tempête cramoisie: a slash and a bleed on every enemy around the rogue, the bleed longer with every point
void CrimsonTempest(Player* player, uint8 comboPoints)
{
    int32 const durationMs = int32(2000 * (1 + comboPoints));
    uint32 const ticks = std::max<uint32>(1, uint32(durationMs) / CrimsonTempestTickMs);
    uint32 const hit = PerPoint(player, CrimsonTempestHitPerPoint, 0, comboPoints);
    int32 const tick = int32(PerPoint(player, CrimsonTempestBleedPerPoint, 0, comboPoints) / ticks);
    for (Unit* enemy : EnemiesAround(player, player, AreaRadius))
    {
        DealAbility(player, enemy, SPELL_CRIMSON_TEMPEST, hit);
        PlaceBleed(player, enemy, SPELL_CRIMSON_TEMPEST_BLEED, std::max(tick, 1), durationMs);
    }
}

// Poudre noire: shadow damage to every enemy around the rogue
void BlackPowder(Player* player, uint8 comboPoints)
{
    uint32 const damage = PerPoint(player, BlackPowderPerPoint, BlackPowderFlatPerPoint, comboPoints);
    bool const terrors = player->HasAura(TALENT_NIGHT_TERRORS);
    for (Unit* enemy : EnemiesAround(player, player, AreaRadius))
    {
        DealAbility(player, enemy, SPELL_BLACK_POWDER, damage);
        if (terrors && enemy->IsAlive())
            player->AddAura(SPELL_NIGHT_TERRORS, enemy);
    }
}

// Technique secrète: the rogue and two shadows of it strike every enemy around, one after the other
void SecretTechnique(Player* player, uint8 comboPoints)
{
    uint32 const damage = PerPoint(player, SecretTechniquePerPoint, SecretTechniqueFlatPerPoint, comboPoints);
    for (uint32 strike = 0; strike < SecretTechniqueStrikes; ++strike)
        player->m_Events.AddEventAtOffset([player, damage]()
        {
            if (!player->IsAlive() || !player->IsInWorld())
                return;
            for (Unit* enemy : EnemiesAround(player, player, AreaRadius))
                DealAbility(player, enemy, SPELL_SECRET_TECHNIQUE, damage);
        }, Milliseconds(300 * strike));
}

// Assassinat: Envenom carries the target's bleeds to the enemies around it that do not have them yet, with the time
// and damage they have left there
void SpreadBleeds(Player* player, Unit* target)
{
    struct Bleed
    {
        uint32 spellId;
        int32 amount;
        int32 duration;
    };
    std::vector<Bleed> bleeds;
    for (AuraEffect const* effect : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
        if (effect->GetCasterGUID() == player->GetGUID() && IsBleed(effect->GetSpellInfo()))
            bleeds.push_back({ effect->GetId(), effect->GetAmount(), effect->GetBase()->GetDuration() });
    if (bleeds.empty())
        return;

    for (Bleed const& bleed : bleeds)
    {
        uint32 spread = 0;
        for (Unit* enemy : EnemiesAround(player, target, AreaRadius))
        {
            if (spread >= SpreadTargets)
                break;
            if (enemy == target || enemy->HasAura(bleed.spellId, player->GetGUID()))
                continue;
            PlaceBleed(player, enemy, bleed.spellId, bleed.amount, bleed.duration);
            ++spread;
        }
    }
}

// Assassinat: Fan of Knives puts the rogue's weapon poisons on every enemy it hits
void PoisonFromWeapons(Player* player, Unit* victim)
{
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK })
    {
        Item* item = player->GetWeaponForAttack(attackType);
        SpellItemEnchantmentEntry const* enchant = item ?
            sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT)) : nullptr;
        if (!enchant)
            continue;
        for (uint8 index = 0; index < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++index)
            if (enchant->type[index] == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                if (SpellInfo const* poison = sSpellMgr->GetSpellInfo(enchant->spellid[index]))
                    if (poison->Dispel == DISPEL_POISON)
                        player->CastSpell(victim, poison->Id, TRIGGERED_FULL_MASK, item);
    }
}

// Virulence: a stack for every bleed and poison of the rogue on the enemies around it
void UpdateVirulence(Player* player)
{
    uint32 count = 0;
    if (player->IsInCombat())
        for (Unit* enemy : EnemiesAround(player, player, VirulenceRange))
            for (auto const& [id, application] : enemy->GetAppliedAuras())
            {
                Aura const* aura = application->GetBase();
                if (aura->GetCasterGUID() != player->GetGUID())
                    continue;
                SpellInfo const* spellInfo = aura->GetSpellInfo();
                if (IsBleed(spellInfo) || spellInfo->Dispel == DISPEL_POISON)
                    ++count;
            }

    uint8 const stacks = uint8(std::min<uint32>(count, VirulenceMaxStacks));
    Aura* aura = player->GetAura(SPELL_VIRULENCE);
    if (!stacks)
    {
        if (aura)
            player->RemoveAurasDueToSpell(SPELL_VIRULENCE);
        return;
    }
    if (!aura)
        aura = player->AddAura(SPELL_VIRULENCE, player);
    if (aura && aura->GetStackAmount() != stacks)
        aura->SetStackAmount(stacks);
}

// A weapon strike that awards combo points (not Premeditation or Marqué pour la mort). Hemorrhage's combo point is
// the core's, not an effect of its own.
bool IsComboBuilder(SpellInfo const* spellInfo)
{
    return spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE &&
        (spellInfo->HasEffect(SPELL_EFFECT_ADD_COMBO_POINTS) || HasFlag0(spellInfo, FLAG0_HEMORRHAGE));
}

bool IsPoisonedBy(Unit* target, Player* player)
{
    for (auto const& [id, application] : target->GetAppliedAuras())
    {
        Aura const* aura = application->GetBase();
        if (aura->GetCasterGUID() == player->GetGUID() && aura->GetSpellInfo()->Dispel == DISPEL_POISON)
            return true;
    }
    return false;
}

// Exsanguiner: the caster's Rupture and Garrote on the target deal what they had left, at once
void Exsanguinate(Player* player, Unit* target)
{
    uint32 total = 0;
    std::vector<uint32> spent;
    for (AuraEffect const* effect : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
    {
        if (effect->GetCasterGUID() != player->GetGUID() || !IsBleed(effect->GetSpellInfo()))
            continue;
        int32 const ticks = effect->GetTotalTicks() - int32(effect->GetTickNumber());
        if (ticks > 0)
            total += uint32(std::max(effect->GetAmount(), 0)) * uint32(ticks);
        spent.push_back(effect->GetId());
    }
    for (uint32 spellId : spent)
        target->RemoveAurasDueToSpell(spellId, player->GetGUID());
    DealNamed(player, target, SPELL_EXSANGUINATE, total);
}

// Lames sans repos: the utility cooldowns tick faster with every finisher
void ReduceUtilityCooldowns(Player* player, uint8 comboPoints)
{
    std::vector<uint32> ready;
    for (auto const& [spellId, cooldown] : player->GetSpellCooldownMap())
    {
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (HasFlag0(spellInfo, FLAG0_VANISH | FLAG0_SPRINT | FLAG0_EVASION | FLAG0_BLIND) ||
            HasFlag1(spellInfo, FLAG1_CLOAK_OF_SHADOWS | FLAG1_SHADOWSTEP))
            ready.push_back(spellId);
    }
    for (uint32 spellId : ready)
        player->ModifySpellCooldown(spellId, -int32(1000 * comboPoints));
}

void OnFinisher(Player* player, SpellInfo const* spellInfo, Unit* target, uint8 comboPoints)
{
    if (uint8 const alacrity = Rank(player, TALENT_ALACRITY_1, TALENT_ALACRITY_2))
        if (roll_chance_i(5 * alacrity * comboPoints))
            AddStack(player, SPELL_ALACRITY, AlacrityMaxStacks);

    if (player->HasAura(TALENT_RESTLESS_BLADES))
        ReduceUtilityCooldowns(player, comboPoints);

    if (uint8 const daggers = Rank(player, TALENT_DEEPER_DAGGERS_1, TALENT_DEEPER_DAGGERS_2))
    {
        player->RemoveAurasDueToSpell(SPELL_DEEPER_DAGGERS);
        if (Aura* aura = player->AddAura(SPELL_DEEPER_DAGGERS, player))
            aura->SetStackAmount(daggers);
    }

    Aura* dance = player->GetAura(SPELL_SHADOW_DANCE);
    if (dance && player->HasAura(TALENT_DEATH_DANCE))
    {
        int32 const duration = dance->GetDuration() + int32(1000 * comboPoints);
        dance->SetMaxDuration(std::max(dance->GetMaxDuration(), duration));
        dance->SetDuration(duration);
    }
    else if (!dance && comboPoints >= 5 && player->HasAura(TALENT_INVISIBLE_KILLER) && roll_chance_i(20))
        if (Aura* aura = player->AddAura(SPELL_SHADOW_DANCE, player))
        {
            aura->SetMaxDuration(3000);
            aura->SetDuration(3000);
        }

    // Maître du venin: Envenom brings the bleeds back to their full length
    if (target && HasFlag1(spellInfo, FLAG1_ENVENOM) && player->HasAura(TALENT_VENOM_MASTER))
        for (AuraEffect const* effect : target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE))
            if (effect->GetCasterGUID() == player->GetGUID() && IsBleed(effect->GetSpellInfo()))
                effect->GetBase()->RefreshDuration();

    // Assassinat: Envenom carries the bleeds to the enemies around
    if (target && HasFlag1(spellInfo, FLAG1_ENVENOM) && IsAssassination(player))
        SpreadBleeds(player, target);
}

// Maître des armes: the strike again, a moment later (a triggered cast: no cost, and it does not roll again)
void StrikeAgain(Player* player, Unit* target, uint32 spellId)
{
    ObjectGuid const targetGuid = target->GetGUID();
    player->m_Events.AddEventAtOffset([player, targetGuid, spellId]()
    {
        Unit* again = ObjectAccessor::GetUnit(*player, targetGuid);
        if (player->IsAlive() && again && again->IsAlive())
            player->CastSpell(again, spellId, TRIGGERED_FULL_MASK);
    }, 200ms);
}

// Every bonus to a rogue's damage on a target the spell data cannot express, as a multiplier
float DamageBonus(Player* player, Unit* target, SpellInfo const* spellInfo, bool autoAttack)
{
    int32 percent = 0;
    if (target->HasAura(SPELL_VENDETTA, player->GetGUID()))
        percent += 20;
    if (target->HealthBelowPct(35))
        percent += 15 * Rank(player, TALENT_ZOLDYCK_1, TALENT_ZOLDYCK_2);
    if (player->HasAura(TALENT_DARK_SHADOW) && player->HasAura(SPELL_SHADOW_DANCE))
        percent += 30;
    if (autoAttack && player->HasAura(SPELL_SHADOW_BLADES))
        percent += 50;
    // Backstab from behind (it works from any side)
    if (HasFlag0(spellInfo, FLAG0_BACKSTAB) && !target->HasInArc(float(M_PI), player))
        percent += 20;
    return 1.0f + percent / 100.0f;
}

bool IsPoisonEnchant(uint32 enchantId)
{
    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
    if (!enchant)
        return false;
    for (uint8 index = 0; index < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++index)
        if (enchant->type[index] == ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(enchant->spellid[index]))
                if (spellInfo->Dispel == DISPEL_POISON)
                    return true;
    return false;
}

// Poisons tenaces: a poison on a weapon is put back to a full hour every few minutes, so it never wears off
void RefreshPoisons(Player* player)
{
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK })
    {
        Item* item = player->GetWeaponForAttack(attackType);
        if (item && IsPoisonEnchant(item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT)))
            player->AddEnchantmentDuration(item, TEMP_ENCHANTMENT_SLOT, PoisonDurationMs);
    }
}

// --- Spell casts --------------------------------------------------------------------------------------------------

class RogueTalentSpellScript : public AllSpellScript
{
public:
    RogueTalentSpellScript() : AllSpellScript("RogueTalentSpellScript", { ALLSPELLHOOK_ON_CAST }) { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = RoguePlayer(caster);
        if (!player || !spellInfo)
            return;
        RogueState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();

        switch (spellInfo->Id)
        {
            case SPELL_MARKED_FOR_DEATH:
                state->markedTarget = target ? target->GetGUID() : ObjectGuid::Empty;
                state->markedUntil = NowMs() + MarkedForDeathMs;
                return;
            case SPELL_EXSANGUINATE:
                if (target)
                    Exsanguinate(player, target);
                return;
            case SPELL_CRIMSON_TEMPEST:
                if (uint8 const comboPoints = player->GetComboPoints())
                    CrimsonTempest(player, comboPoints);
                break;
            case SPELL_BLACK_POWDER:
                if (uint8 const comboPoints = player->GetComboPoints())
                    BlackPowder(player, comboPoints);
                break;
            case SPELL_SECRET_TECHNIQUE:
                if (uint8 const comboPoints = player->GetComboPoints())
                    SecretTechnique(player, comboPoints);
                break;
            case SPELL_SHURIKEN_STORM:
            {
                // Its combo points go on the rogue's target when it has one, else on the first enemy hit
                Unit* selected = player->GetSelectedUnit();
                state->shurikenTarget = selected && player->IsValidAttackTarget(selected) ? selected->GetGUID() :
                    ObjectGuid::Empty;
                state->shurikenPoints = 0;
                return;
            }
            default:
                break;
        }

        if (!IsRogueSpell(spellInfo))
            return;

        uint32 const firstRank = spellInfo->GetFirstRankSpell()->Id;
        if (HasFlag0(spellInfo, FLAG0_SPRINT) && player->HasAura(TALENT_IMPROVED_SPRINT))
            player->RemoveMovementImpairingAuras(true);
        if (HasFlag0(spellInfo, FLAG0_FEINT) && player->HasAura(TALENT_FEINT))
            player->AddAura(SPELL_FEINT_REDUCTION, player);
        if (HasFlag0(spellInfo, FLAG0_EVASION) && player->HasAura(TALENT_EVASION))
            player->AddAura(SPELL_EVASION_REDUCTION, player);
        if (HasFlag0(spellInfo, FLAG0_VANISH) && player->HasAura(TALENT_SHADOW_MOMENTUM))
        {
            Energize(player, SPELL_SHADOW_MOMENTUM, 30);
            player->AddAura(SPELL_SHADOW_MOMENTUM, player);
        }
        if (firstRank == SPELL_COLD_BLOOD && player->HasAura(TALENT_COLD_BLOODED))
            Energize(player, SPELL_COLD_BLOOD, 25);

        if (spell->IsTriggered())
            return;

        // Lames jumelles: a Mutilate now and then costs nothing and adds a combo point
        if (firstRank == SPELL_MUTILATE && target && player->HasAura(TALENT_TWIN_BLADES) && roll_chance_i(25))
        {
            player->ModifyPower(POWER_ENERGY, spell->GetPowerCost());
            player->AddComboPoints(target, 1);
        }

        if (IsComboBuilder(spellInfo) && target)
        {
            if (player->HasAura(SPELL_SHADOW_BLADES))
                player->AddComboPoints(target, 1);
            if (uint8 const weaponmaster = Rank(player, TALENT_WEAPONMASTER_1, TALENT_WEAPONMASTER_2))
                if (roll_chance_i(5 * weaponmaster))
                    StrikeAgain(player, target, spellInfo->Id);
        }

        // Finishers, while their combo points are still there
        if (spellInfo->NeedsComboPoints())
            if (uint8 const comboPoints = player->GetComboPoints())
                OnFinisher(player, spellInfo, target, comboPoints);
    }
};

// --- Damage and auras ---------------------------------------------------------------------------------------------

class RogueTalentUnitScript : public UnitScript
{
public:
    RogueTalentUnitScript() : UnitScript("RogueTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_ON_AURA_APPLY,
        UNITHOOK_ON_AURA_REMOVE,
        UNITHOOK_ON_UNIT_DEATH,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Poison sangsue: 5% of the damage dealt comes back as health
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        Player* player = RoguePlayer(attacker);
        if (!player || !victim || victim == player || !damage || !player->IsAlive() || player->IsFullHealth() ||
            !player->HasAura(TALENT_LEECHING_POISON))
            return;

        uint32 const healing = std::max<uint32>(1, std::min(damage, victim->GetHealth()) / 20);
        HealInfo healInfo(player, player, healing, sSpellMgr->GetSpellInfo(SPELL_LEECHING_POISON),
            SPELL_SCHOOL_MASK_NATURE);
        player->HealBySpell(healInfo);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (Player* player = RoguePlayer(attacker); player && target && damage)
            damage = uint32(damage * DamageBonus(player, target, nullptr, true));
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (Player* player = RoguePlayer(attacker); player && target && damage > 0)
            damage = int32(damage * DamageBonus(player, target, spellInfo, false));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        // Called for periodic heals too: only an enemy's damage counts
        Player* player = RoguePlayer(attacker);
        if (!player || !target || !damage || !player->IsValidAttackTarget(target))
            return;

        damage = uint32(damage * DamageBonus(player, target, spellInfo, false));

        if (IsBleed(spellInfo) && player->HasAura(TALENT_VENOMOUS_WOUNDS) && IsPoisonedBy(target, player))
            Energize(player, spellInfo->Id, 5);
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        Player* player = RoguePlayer(unit);
        if (!player || !aura)
            return;

        // Coup dans le noir: stealth or Shadow Dance make the next Cheap Shot free
        SpellInfo const* spellInfo = aura->GetSpellInfo();
        if ((aura->GetId() == SPELL_SHADOW_DANCE || spellInfo->HasAura(SPELL_AURA_MOD_STEALTH)) &&
            player->HasAura(TALENT_SHOT_IN_THE_DARK))
            player->AddAura(SPELL_SHOT_IN_THE_DARK, player);
    }

    // Maître assassin: leaving stealth
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        Player* player = RoguePlayer(unit);
        if (!player || !aurApp || !player->IsInWorld() || !player->IsAlive() || mode == AURA_REMOVE_BY_DEATH)
            return;

        if (aurApp->GetBase()->GetSpellInfo()->HasAura(SPELL_AURA_MOD_STEALTH) &&
            player->HasAura(TALENT_MASTER_ASSASSIN))
            player->AddAura(SPELL_MASTER_ASSASSIN, player);
    }

    // Marqué pour la mort: its target killed within the minute
    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        Unit* owner = killer ? killer->GetCharmerOrOwnerOrSelf() : nullptr;
        Player* player = RoguePlayer(owner);
        if (!player || !unit)
            return;

        RogueState* state = GetState(player);
        if (state->markedTarget != unit->GetGUID() || NowMs() > state->markedUntil)
            return;
        state->markedTarget.Clear();
        player->RemoveSpellCooldown(SPELL_MARKED_FOR_DEATH, true);
    }

    // Tempête de shurikens: a combo point per enemy hit, on one target; Terreurs nocturnes slows each
    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 /*damage*/,
                           bool /*critical*/) override
    {
        Player* player = RoguePlayer(caster);
        if (!player || !victim || !spellInfo)
            return;

        if (spellInfo->Id == SPELL_FAN_OF_KNIVES && victim->IsAlive() && IsAssassination(player))
        {
            PoisonFromWeapons(player, victim);
            return;
        }
        if (spellInfo->Id != SPELL_SHURIKEN_STORM)
            return;

        if (player->HasAura(TALENT_NIGHT_TERRORS))
            player->AddAura(SPELL_NIGHT_TERRORS, victim);

        RogueState* state = GetState(player);
        if (state->shurikenPoints >= ShurikenMaxPoints)
            return;
        if (state->shurikenTarget.IsEmpty())
            state->shurikenTarget = victim->GetGUID();
        if (Unit* comboTarget = ObjectAccessor::GetUnit(*player, state->shurikenTarget))
        {
            player->AddComboPoints(comboTarget, 1);
            ++state->shurikenPoints;
        }
    }
};

// --- Every update: Focalisation des ombres, the weapons' poisons -------------------------------------------------

class RogueTalentPlayerScript : public PlayerScript
{
public:
    RogueTalentPlayerScript() : PlayerScript("RogueTalentPlayerScript", { PLAYERHOOK_ON_UPDATE }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_ROGUE)
            return;

        // Focalisation des ombres: held while stealthed or dancing
        bool const focused = player->HasAura(TALENT_SHADOW_FOCUS) &&
            (player->HasStealthAura() || player->HasAura(SPELL_SHADOW_DANCE));
        if (focused != player->HasAura(SPELL_SHADOW_FOCUS))
        {
            if (focused)
                player->AddAura(SPELL_SHADOW_FOCUS, player);
            else
                player->RemoveAurasDueToSpell(SPELL_SHADOW_FOCUS);
        }

        RogueState* state = GetState(player);
        if (state->virulenceTimer > diff)
            state->virulenceTimer -= diff;
        else
        {
            state->virulenceTimer = VirulenceUpdateMs;
            if (IsAssassination(player))
                UpdateVirulence(player);
            else if (player->HasAura(SPELL_VIRULENCE))
                player->RemoveAurasDueToSpell(SPELL_VIRULENCE);
        }

        if (state->poisonTimer > diff)
        {
            state->poisonTimer -= diff;
            return;
        }
        state->poisonTimer = PoisonRefreshMs;
        RefreshPoisons(player);
    }
};
}

void AddRogueTalentScripts()
{
    new RogueTalentSpellScript();
    new RogueTalentUnitScript();
    new RogueTalentPlayerScript();
}
