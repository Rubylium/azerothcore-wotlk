#include "Oathblade.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuras.h"
#include "Unit.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace Oathblade
{
namespace
{
constexpr std::array<AbilityUnlock, 13> AbilityUnlocks = { {
    { SPELL_SWIFT_CUT, 1 }, { SPELL_PRECISE_THRUST, 4 }, { SPELL_NOBLE_VERDICT, 6 },
    { SPELL_SWEEPING_ARC, 10 }, { SPELL_FLAWLESS_FORM_PASSIVE, 12 }, { SPELL_REVERSAL, 18 },
    { SPELL_NOBLE_ADVANCE, 24 }, { SPELL_CRESCENT_SWEEP, 30 }, { SPELL_BLADE_WARD, 36 },
    { SPELL_FLOURISH, 42 }, { SPELL_ZEAL_STRIKE, 50 }, { SPELL_RALLY, 60 },
    { SPELL_FINAL_EDICT, 70 }
} };
}

bool IsOathblade(Player const* player)
{
    return player && player->getClass() == CLASS_OATHBLADE;
}

int32 GetTalentValue(Unit const* unit, std::initializer_list<TalentRank> ranks)
{
    if (!unit)
        return 0;
    for (auto rank = std::rbegin(ranks); rank != std::rend(ranks); ++rank)
        if (unit->HasAura(rank->spellId))
            return rank->value;
    return 0;
}

uint8 GetFlow(Player const* player)
{
    Aura const* aura = player ? player->GetAura(SPELL_FLOW, player->GetGUID()) : nullptr;
    return aura ? aura->GetStackAmount() : 0;
}

void AddFlow(Player* player, uint8 amount)
{
    if (!IsOathblade(player) || !amount)
        return;
    int32 const openingChance = GetTalentValue(player, { { 90919, 10 }, { 90920, 20 } });
    if (openingChance && roll_chance_i(uint32(openingChance)))
        ++amount;
    uint8 const total = std::min<uint8>(5, GetFlow(player) + amount);
    Aura* aura = player->GetAura(SPELL_FLOW, player->GetGUID());
    if (!aura)
        aura = player->AddAura(SPELL_FLOW, player);
    if (aura)
        aura->SetStackAmount(total);
}

bool SpendFlow(Player* player, uint8 amount)
{
    Aura* aura = player ? player->GetAura(SPELL_FLOW, player->GetGUID()) : nullptr;
    if (!aura || aura->GetStackAmount() < amount)
        return false;
    uint8 const remaining = aura->GetStackAmount() - amount;
    if (remaining)
        aura->SetStackAmount(remaining);
    else
        aura->Remove();
    return true;
}

bool InFlawlessForm(Player const* player)
{
    return player && player->HasAura(SPELL_FLAWLESS_FORM);
}

void AddTempo(Player* player, uint8 amount)
{
    if (!IsOathblade(player) || !player->HasSpell(SPELL_FLAWLESS_FORM_PASSIVE) || !amount ||
        InFlawlessForm(player))
        return;

    // Flawless Mastery (spec tree capstone) brings the burst two Tempo sooner
    uint8 const threshold = player->HasAura(SPELL_TALENT_FLAWLESS_MASTERY) ? 10 : 12;
    Aura* tempo = player->GetAura(SPELL_TEMPO, player->GetGUID());
    uint8 const stacks = std::min<uint8>(threshold, uint8((tempo ? tempo->GetStackAmount() : 0) + amount));
    if (stacks >= threshold)
    {
        if (tempo)
            tempo->Remove();
        player->CastSpell(player, SPELL_FLAWLESS_FORM, true);
        if (Aura* form = player->GetAura(SPELL_FLAWLESS_FORM, player->GetGUID()))
        {
            int32 const extension = GetTalentValue(player, { { 90959, 1 }, { 90960, 2 } });
            form->SetMaxDuration(8000 + extension * IN_MILLISECONDS);
            form->SetDuration(8000 + extension * IN_MILLISECONDS);
        }
        player->ModifyPower(POWER_ENERGY, 40);
        if (player->HasAura(SPELL_TALENT_FLAWLESS_SURGE))
            AddFlow(player, 2);
        return;
    }
    if (!tempo)
        tempo = player->AddAura(SPELL_TEMPO, player);
    if (tempo)
        tempo->SetStackAmount(stacks);
}

void AdvanceTechnique(Player* player, uint32 spellId, uint8 flow)
{
    if (!IsOathblade(player))
        return;
    uint32 const ownMarker = spellId == SPELL_SWIFT_CUT ? SPELL_LAST_SWIFT_CUT : SPELL_LAST_PRECISE_THRUST;
    uint32 const otherMarker = spellId == SPELL_SWIFT_CUT ? SPELL_LAST_PRECISE_THRUST : SPELL_LAST_SWIFT_CUT;
    if (player->HasAura(otherMarker))
    {
        ++flow;
        player->RemoveAurasDueToSpell(otherMarker);
    }
    Aura* marker = player->GetAura(ownMarker, player->GetGUID());
    if (!marker)
        marker = player->AddAura(ownMarker, player);
    if (marker)
    {
        int32 const duration = 7000 + IN_MILLISECONDS * GetTalentValue(player, { { 90943, 2 }, { 90944, 4 } });
        marker->SetMaxDuration(duration);
        marker->SetDuration(duration);
    }
    AddFlow(player, flow);
    uint8 tempo = 1;
    int32 const rhythmChance = GetTalentValue(player, { { 90908, 10 }, { 90909, 20 } });
    if (rhythmChance && roll_chance_i(uint32(rhythmChance)))
        ++tempo;
    AddTempo(player, tempo);
    if (InFlawlessForm(player))
        player->ModifyPower(POWER_ENERGY, 12);
}

int32 GetStrikeDamage(Player const* player, float weaponScale, float attackPowerScale, uint32 spellId)
{
    if (!player)
        return 0;
    // A melee ability's "weapon damage" is a swing, and a swing is the weapon's own damage plus what attack
    // power adds over the time that swing takes: attack power / 14 per second of it. GetWeaponDamageRange is
    // only the first half of that, so every strike here was paying the weapon's base damage and then a small
    // flat share of attack power in place of the much larger amount a real swing carries.
    //
    // The result was a class whose abilities hit for less than its own auto-attack. With a 2.4 second sword
    // and 3000 attack power a white hit is worth about 760, of which 510 comes from attack power; Swift Cut,
    // at 0.80 weapon and 0.11 attack power, was paying 196 + 330 = 526 for a global cooldown. No amount of
    // coefficient tuning fixes that shape - the swing itself has to be a swing.
    //
    // GetAPMultiplier is the core's own normalisation (3.3 for a two-hander, 1.7 for a dagger, 2.4 for the
    // rest), so this stays in step with how every stock melee special is sized. It is not const, hence the
    // cast; nothing about the player is changed by asking.
    float const weaponBase = (player->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE) +
        player->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE)) * 0.5f;
    float const attackPower = std::max(0.0f, player->GetTotalAttackPowerValue(BASE_ATTACK));
    float const swingTime = const_cast<Player*>(player)->GetAPMultiplier(BASE_ATTACK, true);
    float const weapon = weaponBase + attackPower / 14.0f * swingTime;
    // Talent multipliers (localTools/oathblade/talentTree.json): Keen Edge on every technique, then each
    // ability's own, then the finisher and sweeping-attack families
    bool const sweeping = spellId == SPELL_SWEEPING_ARC || spellId == SPELL_CRESCENT_SWEEP ||
        spellId == SPELL_BLADE_DANCE || spellId == SPELL_GRAND_FLOURISH;
    bool const finisher = spellId == SPELL_NOBLE_VERDICT || spellId == SPELL_CRESCENT_SWEEP ||
        spellId == SPELL_FINAL_EDICT || spellId == SPELL_GRAND_FLOURISH;
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90900, 4 }, { 90901, 8 } })) / 100.0f;
    if (spellId == SPELL_SWIFT_CUT)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 91040, 10 }, { 91041, 20 } })) / 100.0f;
    if (spellId == SPELL_PRECISE_THRUST)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90911, 8 }, { 90912, 15 } })) / 100.0f;
    if (spellId == SPELL_NOBLE_VERDICT)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 91044, 8 }, { 91045, 15 } })) / 100.0f;
    if (spellId == SPELL_CRESCENT_SWEEP)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 91057, 8 }, { 91058, 15 } })) / 100.0f;
    if (sweeping)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90921, 8 }, { 90922, 15 } })) / 100.0f;
    if (finisher)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90948, 5 }, { 90949, 10 } })) / 100.0f;
    if (InFlawlessForm(player))
        multiplier *= 1.2f + float(GetTalentValue(player, { { 90956, 5 }, { 90957, 10 } })) / 100.0f;
    return std::max<int32>(1, int32((weapon * weaponScale + attackPower * attackPowerScale) * multiplier));
}

uint32 LearnUnlockedAbilities(Player* player)
{
    if (!IsOathblade(player))
        return 0;
    uint32 learned = 0;
    for (AbilityUnlock const& unlock : AbilityUnlocks)
    {
        if (player->GetLevel() < unlock.level || player->HasSpell(unlock.spellId))
            continue;
        player->learnSpell(unlock.spellId, false);
        learned += player->HasSpell(unlock.spellId) ? 1 : 0;
    }
    return learned;
}

void ForgetAbilitiesAboveLevel(Player* player, uint8 level)
{
    if (!IsOathblade(player))
        return;
    for (AbilityUnlock const& unlock : AbilityUnlocks)
        if (unlock.level > level && player->HasSpell(unlock.spellId))
            player->removeSpell(unlock.spellId, SPEC_MASK_ALL, false);
    LearnUnlockedAbilities(player);
}

std::list<Unit*> GetNearbyEnemies(Player* caster, WorldObject* center, float radius, std::size_t limit)
{
    std::list<Unit*> enemies;
    if (!caster || !center)
        return enemies;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, caster, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, enemies, check);
    Cell::VisitObjects(center, searcher, radius);
    enemies.remove_if([caster](Unit* unit) { return !unit->IsAlive() || !caster->IsValidAttackTarget(unit); });
    enemies.sort([center](Unit const* left, Unit const* right)
        { return center->GetDistance(left) < center->GetDistance(right); });
    while (enemies.size() > limit)
        enemies.pop_back();
    return enemies;
}
}
