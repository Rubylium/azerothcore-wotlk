#include "Necromancer.h"

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

namespace Necromancer
{
namespace
{
constexpr std::array<AbilityUnlock, 16> AbilityUnlocks = { {
    { SPELL_SOUL_BOLT, 1 }, { SPELL_NECROMANCER_PASSIVE, 1 }, { SPELL_RAISE_SKELETON, 1 },
    { SPELL_DEATHLY_BRAND, 2 }, { SPELL_DEATH_COMMAND, 4 }, { SPELL_SOUL_TAP, 6 },
    { SPELL_RAISE_DEADEYE, 8 }, { SPELL_SOUL_DRAIN, 10 }, { SPELL_CORPSE_EXPLOSION, 14 },
    { SPELL_FRENZIED_LEGION, 18 }, { SPELL_RAISE_PLAGUE_MAGE, 22 }, { SPELL_SOUL_HARVEST, 28 },
    { SPELL_SACRIFICIAL_PACT, 34 }, { SPELL_BLACK_VOLLEY, 42 }, { SPELL_CREATE_ABOMINATION, 52 },
    { SPELL_ARMY_OF_THE_DAMNED, 60 }
} };
}

bool IsNecromancer(Player const* player) { return player && player->getClass() == CLASS_NECROMANCER; }

int32 GetTalentValue(Unit const* unit, std::initializer_list<TalentRank> ranks)
{
    if (!unit)
        return 0;
    for (auto rank = std::rbegin(ranks); rank != std::rend(ranks); ++rank)
        if (unit->HasAura(rank->spellId))
            return rank->value;
    return 0;
}

uint8 GetMaxSouls(Player const* player)
{
    return uint8(10 + GetTalentValue(player, { { 90508, 1 }, { 90509, 2 } }));
}

uint8 GetSouls(Player const* player)
{
    Aura const* aura = player ? player->GetAura(SPELL_CAPTURED_SOULS, player->GetGUID()) : nullptr;
    return aura ? aura->GetStackAmount() : 0;
}

void AddSouls(Player* player, uint8 amount)
{
    if (!IsNecromancer(player) || !amount)
        return;
    uint8 const overflowChance = uint8(GetTalentValue(player, { { 90528, 10 }, { 90529, 20 } }));
    if (overflowChance && roll_chance_i(overflowChance))
        ++amount;
    uint8 const target = std::min<uint8>(GetMaxSouls(player), GetSouls(player) + amount);
    Aura* aura = player->GetAura(SPELL_CAPTURED_SOULS, player->GetGUID());
    if (!aura)
        aura = player->AddAura(SPELL_CAPTURED_SOULS, player);
    if (aura)
        aura->SetStackAmount(target);
}

bool SpendSouls(Player* player, uint8 amount)
{
    Aura* aura = player ? player->GetAura(SPELL_CAPTURED_SOULS, player->GetGUID()) : nullptr;
    if (!aura || aura->GetStackAmount() < amount)
        return false;
    uint8 const remaining = aura->GetStackAmount() - amount;
    if (remaining)
        aura->SetStackAmount(remaining);
    else
        aura->Remove();
    RestoreMana(player, uint8(amount * (2 + GetTalentValue(player, { { 90513, 1 }, { 90514, 2 } }))));
    return true;
}

void RestoreMana(Player* player, uint8 percent)
{
    if (player && percent)
        player->ModifyPower(POWER_MANA, int32(CalculatePct(player->GetMaxPower(POWER_MANA), percent)));
}

// One more stack of rot on the target, or a fresh one. The aura always belongs to the Necromancer, never to
// the minion that applied it, so the ticks are his damage and his spell power scales them.
void ApplyNecroticRot(Player* owner, Unit* target)
{
    if (!IsNecromancer(owner) || !target || !target->IsAlive() || !owner->IsValidAttackTarget(target))
        return;

    // ModStackAmount clamps to the spell's StackAmount and refreshes the timers, so a pack under attack keeps
    // rotting for as long as the army keeps hitting it.
    if (Aura* rot = target->GetAura(SPELL_NECROTIC_ROT, owner->GetGUID()))
    {
        rot->ModStackAmount(1);
        return;
    }

    owner->AddAura(SPELL_NECROTIC_ROT, target);
}

uint32 LearnUnlockedAbilities(Player* player)
{
    if (!IsNecromancer(player))
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

float GetSpellDamageMultiplier(Player const* player, uint32 spellId, bool areaSpell)
{
    float multiplier = 1.0f + float(GetTalentValue(player,
        { { 90500, 2 }, { 90501, 4 }, { 90502, 6 }, { 90503, 8 }, { 90504, 10 } })) / 100.0f;
    if (areaSpell)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90530, 8 }, { 90531, 16 }, { 90532, 24 } })) / 100.0f;
    if (spellId == SPELL_SOUL_BOLT || spellId == SPELL_SOUL_DRAIN || spellId == SPELL_SOUL_HARVEST)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90542, 3 }, { 90543, 6 }, { 90544, 9 } })) / 100.0f;
    if (spellId == SPELL_SOUL_HARVEST)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90558, 5 }, { 90559, 10 }, { 90560, 15 } })) / 100.0f;
    return multiplier;
}

float GetMinionDamageMultiplier(Player const* player, MinionKind kind)
{
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90510, 5 }, { 90511, 10 }, { 90512, 15 } })) / 100.0f;
    multiplier *= 1.0f + float(GetTalentValue(player,
        { { 90550, 2 }, { 90551, 4 }, { 90552, 6 }, { 90553, 8 }, { 90554, 10 } })) / 100.0f;
    if (kind == MinionKind::Archer)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90525, 8 }, { 90526, 16 }, { 90527, 24 } })) / 100.0f;
    if (kind == MinionKind::Mage)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90530, 8 }, { 90531, 16 }, { 90532, 24 } })) / 100.0f;
    if (kind == MinionKind::Abomination)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90547, 10 }, { 90548, 20 }, { 90549, 30 } })) / 100.0f;
    if (player && player->HasAura(SPELL_FRENZIED_LEGION))
        multiplier *= 1.30f + float(GetTalentValue(player, { { 90539, 5 }, { 90540, 10 }, { 90541, 15 } })) / 100.0f;
    return multiplier;
}

float GetMinionHealthMultiplier(Player const* player, MinionKind kind)
{
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90534, 5 }, { 90535, 10 }, { 90536, 15 } })) / 100.0f;
    if (kind == MinionKind::Abomination)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90547, 10 }, { 90548, 20 }, { 90549, 30 } })) / 100.0f;
    return multiplier;
}

std::list<Unit*> GetEnemiesAround(Player* caster, WorldObject* center, float radius, std::size_t limit)
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
