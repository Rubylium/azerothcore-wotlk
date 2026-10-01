#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "LiveTuning.h"
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
#include <array>
#include <limits>
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
constexpr uint32 FLAG0_AMBUSH = 0x00000200;
constexpr uint32 FLAG0_VANISH = 0x00000800;
constexpr uint32 FLAG0_EVISCERATE = 0x00020000;
constexpr uint32 FLAG0_RUPTURE = 0x00100000;
constexpr uint32 FLAG0_BLIND = 0x01000000;
constexpr uint32 FLAG0_HEMORRHAGE = 0x02000000;
constexpr uint32 FLAG0_FEINT = 0x08000000;
constexpr uint32 FLAG1_ENVENOM = 0x00000008;
constexpr uint32 FLAG1_SHADOWSTEP = 0x00000200;
constexpr uint32 FLAG1_CLOAK_OF_SHADOWS = 0x00010000;

LiveTuning::KnobUInt const MarkedForDeathMs("rogue.marked_for_death_ms", 60000);
LiveTuning::KnobInt const AlacrityMaxStacks("rogue.alacrity_max_stacks", 5);
constexpr uint32 PoisonRefreshMs = 5 * MINUTE * IN_MILLISECONDS;
constexpr uint32 PoisonDurationMs = HOUR * IN_MILLISECONDS;
LiveTuning::KnobInt const ShurikenMaxPoints("rogue.shuriken_max_points", 5);

// The specs' area kits (Assassinat: bleeds and poisons everywhere; Finesse: Shuriken Storm, Black Powder, Secret
// Technique). Amounts are shares of the rogue's attack power per combo point spent, so they grow with the character.
LiveTuning::Knob const AreaRadius("rogue.area_radius", 10.0f);
// the slash on every enemy
LiveTuning::Knob const CrimsonTempestHitPerPoint("rogue.crimson_tempest_hit_per_point", 0.06f);
// the whole bleed, over 2 s per point
LiveTuning::Knob const CrimsonTempestBleedPerPoint("rogue.crimson_tempest_bleed_per_point", 0.3f);
// Assassinat lives on its bleeds: Rupture and Garrote deal this much more (WotLK's values were made for a tenth of
// the attack power a character reaches here)
LiveTuning::KnobInt const AssassinationBleedBonusPct("rogue.assassination_bleed_bonus_pct", 150);
// Assassinat's energy (retail's Venomous Wounds, built in): a bleed ticking on a poisoned enemy gives this much, at
// most once per BleedEnergyGapMs whatever the number of dots up
LiveTuning::KnobUInt const AssassinationBleedEnergy("rogue.assassination_bleed_energy", 3);
LiveTuning::KnobUInt const BleedEnergyGapMs("rogue.bleed_energy_gap_ms", 500);
LiveTuning::KnobUInt const CrimsonTempestTickMs("rogue.crimson_tempest_tick_ms", 2000);
// Finesse's pack finisher. Alone, a little under Eviscerate; on a pack, it hits harder the more enemies it reaches (up
// to three), harder still in Shadow Dance, and gives energy back for every enemy beyond the first.
// On a pack the loop is a Shuriken Storm (five combo points) then a Black Powder: the builder carries a real share
// of the damage (ShurikenStormPerEnemy), so the pack's damage is not all in the finisher
LiveTuning::Knob const BlackPowderPerPoint("rogue.black_powder_per_point", 0.075f);
LiveTuning::KnobUInt const BlackPowderFlatPerPoint("rogue.black_powder_flat_per_point", 200);
LiveTuning::KnobInt const BlackPowderPerExtraEnemyPct("rogue.black_powder_per_extra_enemy_pct", 25);
LiveTuning::KnobUInt const BlackPowderMaxExtraEnemies("rogue.black_powder_max_extra_enemies", 2);
LiveTuning::KnobInt const BlackPowderDancePct("rogue.black_powder_dance_pct", 25);
LiveTuning::KnobUInt const BlackPowderEnergyPerExtraEnemy("rogue.black_powder_energy_per_extra_enemy", 6);
LiveTuning::KnobUInt const BlackPowderMaxEnergy("rogue.black_powder_max_energy", 18);
// Shuriken Storm's hit on every enemy, a share of the attack power in place of the weapon damage it was cloned with
// (Fan of Knives' without a dagger bonus: a few hundred, next to the finisher's thousands). About two thirds of a Black
// Powder's hit on each enemy of a pack (the bench: physical, armour takes a share); on one or two enemies it keeps
// ShurikenStormFewEnemiesPct of it, under Backstab, not a single-target builder
LiveTuning::Knob const ShurikenStormPerEnemy("rogue.shuriken_storm_per_enemy", 1.8f);
LiveTuning::KnobUInt const ShurikenStormFullEnemies("rogue.shuriken_storm_full_enemies", 3);
LiveTuning::KnobInt const ShurikenStormFewEnemiesPct("rogue.shuriken_storm_few_enemies_pct", 40);
LiveTuning::Knob const SecretTechniquePerPoint("rogue.secret_technique_per_point", 0.10f);  // each of its three strikes
LiveTuning::KnobUInt const SecretTechniqueFlatPerPoint("rogue.secret_technique_flat_per_point", 150);
LiveTuning::KnobUInt const SecretTechniqueStrikes("rogue.secret_technique_strikes", 3);
// Finesse's own damage (its passive, Danseur des ombres): Eviscerate and the dagger builders
LiveTuning::KnobInt const SubtletyEviscerateBonusPct("rogue.subtlety_eviscerate_bonus_pct", 120);
LiveTuning::KnobInt const SubtletyBuilderBonusPct("rogue.subtlety_builder_bonus_pct", 50);
// Danse de la mort lengthens a Shadow Dance up to this long in all: with the builders' combo points on a critical
// strike, an uncapped Dance never ended
LiveTuning::KnobInt const DeathDanceMaxMs("rogue.death_dance_max_ms", 16000);
// Envenom carries the target's bleeds to this many enemies around it that do not have them
LiveTuning::KnobUInt const SpreadTargets("rogue.spread_targets", 4);
// Virulence: 2% damage per affliction of the rogue on its enemies (the aura's own amount), up to this many
LiveTuning::KnobInt const VirulenceMaxStacks("rogue.virulence_max_stacks", 15);
LiveTuning::Knob const VirulenceRange("rogue.virulence_range", 40.0f);
constexpr uint32 VirulenceUpdateMs = 1000;

// A character's state for its talents. Kept on the player, so it dies with the session.
struct RogueState : public DataMap::Base
{
    ObjectGuid markedTarget;         // Marqué pour la mort: its target, until markedUntil
    uint32 markedUntil = 0;
    ObjectGuid shurikenTarget;       // Tempête de shurikens: where its combo points go, how many it gave, and
    uint8 shurikenPoints = 0;        // how many enemies it hit
    uint8 shurikenHits = 0;
    uint32 poisonTimer = 5000;       // Poisons tenaces: the next refresh of the weapons' poisons
    uint32 virulenceTimer = 0;       // Virulence: the next count of the afflictions
    uint32 bleedEnergyAt = 0;        // Assassinat: when a bleed tick may give energy again
    uint32 autoPoisonTimer = 0;      // Assassinat: the next look at the weapons' poisons
    // A finisher's combo points, read when it begins: an instant spell has spent them by the time its cast hook runs
    uint32 finisherSpell = 0;
    uint8 finisherPoints = 0;
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

bool IsSubtlety(Player* player)
{
    return player->HasAura(SPELL_SUBTLETY_PASSIVE);
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

void SpreadBleeds(Player* player, Unit* target, WorldObject* center, uint32 limit);

// Tempête cramoisie: a slash and a bleed on every enemy around the rogue, the bleed longer with every point
void CrimsonTempest(Player* player, Unit* target, uint8 comboPoints)
{
    // The target's Rupture and Garrote go to everything around first, then the slash and its own bleed
    if (target)
        SpreadBleeds(player, target, player, std::numeric_limits<uint32>::max());

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

// Poudre noire: shadow damage to every enemy around the rogue, worked out once for the cast: more for every enemy
// beyond the first (up to BlackPowderMaxExtraEnemies) and in Shadow Dance, and energy back for the enemies beyond
// the first
void BlackPowder(Player* player, uint8 comboPoints)
{
    std::list<Unit*> const enemies = EnemiesAround(player, player, AreaRadius);
    if (enemies.empty())
        return;

    uint32 const extra = uint32(enemies.size() - 1);
    float multiplier = 1.0f + std::min(extra, BlackPowderMaxExtraEnemies.Get()) * BlackPowderPerExtraEnemyPct / 100.0f;
    if (player->HasAura(SPELL_SHADOW_DANCE))
        multiplier *= 1.0f + BlackPowderDancePct / 100.0f;
    uint32 const damage = uint32(PerPoint(player, BlackPowderPerPoint, BlackPowderFlatPerPoint, comboPoints) *
        multiplier);

    bool const terrors = player->HasAura(TALENT_NIGHT_TERRORS);
    for (Unit* enemy : enemies)
    {
        DealAbility(player, enemy, SPELL_BLACK_POWDER, damage);
        if (terrors && enemy->IsAlive())
            player->AddAura(SPELL_NIGHT_TERRORS, enemy);
    }

    if (uint32 const energy = std::min(extra * BlackPowderEnergyPerExtraEnemy, BlackPowderMaxEnergy.Get()))
        Energize(player, SPELL_BLACK_POWDER, energy);
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

// Assassinat: Envenom and Crimson Tempest carry the target's bleeds to the enemies around (around the target for
// Envenom, around the rogue for Crimson Tempest) that do not have them yet, with the time and damage they have left
void SpreadBleeds(Player* player, Unit* target, WorldObject* center, uint32 limit)
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
        for (Unit* enemy : EnemiesAround(player, center, AreaRadius))
        {
            if (spread >= limit)
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

    // Danse de la mort: the Dance's full length grows with it, up to DeathDanceMaxMs
    Aura* dance = player->GetAura(SPELL_SHADOW_DANCE);
    if (dance && player->HasAura(TALENT_DEATH_DANCE))
    {
        int32 const added = std::min(int32(1000 * comboPoints), DeathDanceMaxMs - dance->GetMaxDuration());
        if (added > 0)
        {
            dance->SetMaxDuration(dance->GetMaxDuration() + added);
            dance->SetDuration(dance->GetDuration() + added);
        }
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
        SpreadBleeds(player, target, target, SpreadTargets);
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
    if (HasFlag0(spellInfo, FLAG0_RUPTURE | FLAG0_GARROTE) && player->HasAura(SPELL_ASSASSINATION_PASSIVE))
        percent += AssassinationBleedBonusPct;
    // Backstab from behind (it works from any side)
    if (HasFlag0(spellInfo, FLAG0_BACKSTAB) && !target->HasInArc(float(M_PI), player))
        percent += 20;

    // Finesse's own, on top of the rest: Eviscerate, and the dagger builders
    int32 specPercent = 0;
    if (spellInfo && IsSubtlety(player))
    {
        if (HasFlag0(spellInfo, FLAG0_EVISCERATE))
            specPercent = SubtletyEviscerateBonusPct;
        else if (HasFlag0(spellInfo, FLAG0_BACKSTAB | FLAG0_AMBUSH | FLAG0_HEMORRHAGE))
            specPercent = SubtletyBuilderBonusPct;
    }
    return (1.0f + percent / 100.0f) * (1.0f + specPercent / 100.0f);
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

// Assassinat keeps its weapons poisoned (as on retail, where poisons are simply on): a weapon with no temporary
// enchantment gets Deadly Poison (main hand) or Instant Poison (off hand), the best rank the level allows. A poison
// the rogue put on (Wound Poison, ...) is never replaced. Envenom needs Deadly Poison on its target, and scales with it.
struct PoisonRank
{
    uint8 level;
    uint32 enchant;     // SpellItemEnchantment id, as the poison's own ENCHANT_ITEM_TEMPORARY effect gives it
};
constexpr std::array<PoisonRank, 9> DeadlyPoisonRanks = { {
    { 30, 7 }, { 38, 8 }, { 46, 626 }, { 54, 627 }, { 60, 2630 }, { 62, 2642 }, { 70, 2643 }, { 76, 3770 },
    { 80, 3771 } } };
constexpr std::array<PoisonRank, 9> InstantPoisonRanks = { {
    { 20, 323 }, { 28, 324 }, { 36, 325 }, { 44, 623 }, { 52, 624 }, { 60, 625 }, { 68, 2641 }, { 73, 3768 },
    { 79, 3769 } } };
constexpr uint32 AutoPoisonCheckMs = 3000;

template <std::size_t N>
uint32 BestPoison(std::array<PoisonRank, N> const& ranks, uint8 level)
{
    uint32 enchant = 0;
    for (PoisonRank const& rank : ranks)
        if (level >= rank.level)
            enchant = rank.enchant;
    return enchant;
}

void EnsureAssassinationPoisons(Player* player)
{
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK })
    {
        Item* item = player->GetWeaponForAttack(attackType);
        if (!item || item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
            continue;
        uint32 const enchant = attackType == BASE_ATTACK ? BestPoison(DeadlyPoisonRanks, player->GetLevel()) :
            BestPoison(InstantPoisonRanks, player->GetLevel());
        if (!enchant || !sSpellItemEnchantmentStore.LookupEntry(enchant))
            continue;
        item->SetEnchantment(TEMP_ENCHANTMENT_SLOT, enchant, PoisonDurationMs, 0);
        player->ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, true);
    }
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

        // The finisher's combo points: still there for a spell with a travel time, spent already for an instant one
        uint8 points = player->GetComboPoints();
        if (state->finisherSpell == spellInfo->Id)
        {
            points = std::max(points, state->finisherPoints);
            state->finisherSpell = 0;
        }

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
                if (points)
                    CrimsonTempest(player, target, points);
                break;
            case SPELL_BLACK_POWDER:
                if (points)
                    BlackPowder(player, points);
                break;
            case SPELL_SECRET_TECHNIQUE:
                if (points)
                    SecretTechnique(player, points);
                break;
            case SPELL_FAN_OF_KNIVES:
                if (!IsAssassination(player))
                    break;
                [[fallthrough]];
            case SPELL_SHURIKEN_STORM:
            {
                // Its combo points go on the rogue's target when it has one, else on the first enemy hit (Fan of
                // Knives too, in Assassination: in WotLK it gave none, and a pack never reached a finisher)
                Unit* selected = player->GetSelectedUnit();
                state->shurikenTarget = selected && player->IsValidAttackTarget(selected) ? selected->GetGUID() :
                    ObjectGuid::Empty;
                state->shurikenPoints = 0;
                state->shurikenHits = 0;
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
        if (spellInfo->NeedsComboPoints() && points)
            OnFinisher(player, spellInfo, target, points);
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
        Player* player = RoguePlayer(attacker);
        if (!player || !target || damage <= 0)
            return;

        // Shuriken Storm: its hit from the attack power, with the rogue's and the target's bonuses (as DealAbility)
        if (spellInfo && spellInfo->Id == SPELL_SHURIKEN_STORM)
        {
            uint32 amount = uint32(player->GetTotalAttackPowerValue(BASE_ATTACK) * ShurikenStormPerEnemy);
            if (EnemiesAround(player, player, AreaRadius).size() < ShurikenStormFullEnemies)
                amount = amount * ShurikenStormFewEnemiesPct / 100;
            uint32 const done = player->SpellDamageBonusDone(target, spellInfo, amount, SPELL_DIRECT_DAMAGE, EFFECT_0);
            damage = int32(target->SpellDamageBonusTaken(player, spellInfo, done, SPELL_DIRECT_DAMAGE));
        }
        damage = int32(damage * DamageBonus(player, target, spellInfo, false));
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        // Called for periodic heals too: only an enemy's damage counts
        Player* player = RoguePlayer(attacker);
        if (!player || !target || !damage || !player->IsValidAttackTarget(target))
            return;

        damage = uint32(damage * DamageBonus(player, target, spellInfo, false));

        if (IsBleed(spellInfo) && IsPoisonedBy(target, player))
        {
            if (player->HasAura(TALENT_VENOMOUS_WOUNDS))
                Energize(player, spellInfo->Id, 5);
            RogueState* state = GetState(player);
            if (IsAssassination(player) && NowMs() >= state->bleedEnergyAt)
            {
                state->bleedEnergyAt = NowMs() + BleedEnergyGapMs;
                Energize(player, spellInfo->Id, AssassinationBleedEnergy);
            }
        }
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

    // Tempête de shurikens: a combo point for the first enemy hit and two for every other one (one more for the first
    // under Lames de l'ombre), all on one target, up to 5; Terreurs nocturnes slows each. Fan of Knives in
    // Assassination: a combo point per enemy hit. Finesse: a builder's critical strike adds a combo point.
    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 /*damage*/,
                           bool critical) override
    {
        Player* player = RoguePlayer(caster);
        if (!player || !victim || !spellInfo)
            return;

        bool const fan = spellInfo->Id == SPELL_FAN_OF_KNIVES && IsAssassination(player);
        if (fan && victim->IsAlive())
            PoisonFromWeapons(player, victim);
        if (spellInfo->Id != SPELL_SHURIKEN_STORM && !fan)
        {
            if (critical && victim->IsAlive() && IsComboBuilder(spellInfo) && IsSubtlety(player))
                player->AddComboPoints(victim, 1);
            return;
        }

        if (!fan && player->HasAura(TALENT_NIGHT_TERRORS))
            player->AddAura(SPELL_NIGHT_TERRORS, victim);

        RogueState* state = GetState(player);
        uint8 gain = 1;
        if (!fan)
        {
            if (state->shurikenHits)
                gain = 2;
            else if (player->HasAura(SPELL_SHADOW_BLADES))
                gain = 2;
        }
        state->shurikenHits = std::min<uint8>(state->shurikenHits + 1, std::numeric_limits<uint8>::max());
        if (state->shurikenPoints >= ShurikenMaxPoints)
            return;
        gain = std::min<uint8>(gain, ShurikenMaxPoints - state->shurikenPoints);
        if (state->shurikenTarget.IsEmpty())
            state->shurikenTarget = victim->GetGUID();
        if (Unit* comboTarget = ObjectAccessor::GetUnit(*player, state->shurikenTarget))
        {
            player->AddComboPoints(comboTarget, int8(gain));
            state->shurikenPoints += gain;
        }
    }
};

// --- Every update: Focalisation des ombres, the weapons' poisons -------------------------------------------------

class RogueTalentPlayerScript : public PlayerScript
{
public:
    RogueTalentPlayerScript() : PlayerScript("RogueTalentPlayerScript", { PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_SPELL_CAST }) { }

    // A finisher's combo points, read before the spell runs: an instant one has spent them by the time the cast hook
    // (RogueTalentSpellScript) sees it. The core's prepare hook comes after an instant cast too, so this is the one.
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        SpellInfo const* spellInfo = spell ? spell->GetSpellInfo() : nullptr;
        if (player->getClass() != CLASS_ROGUE || !spellInfo || !spellInfo->NeedsComboPoints())
            return;
        RogueState* state = GetState(player);
        state->finisherSpell = spellInfo->Id;
        state->finisherPoints = player->GetComboPoints();
    }

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
        if (state->autoPoisonTimer > diff)
            state->autoPoisonTimer -= diff;
        else
        {
            state->autoPoisonTimer = AutoPoisonCheckMs;
            if (IsAssassination(player) && player->IsAlive())
                EnsureAssassinationPoisons(player);
        }

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
