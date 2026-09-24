#include "Necromancer.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "Random.h"
#include "SpellAuras.h"
#include "Timer.h"
#include "Unit.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace Necromancer
{
namespace
{
// Levée des morts replaces the three raises of the first version: the archer and the plague mage now come in
// its squad, and their own spells are taken back from anyone who still knows them
constexpr std::array<AbilityUnlock, 14> AbilityUnlocks = { {
    { SPELL_SOUL_BOLT, 1 }, { SPELL_NECROMANCER_PASSIVE, 1 }, { SPELL_RAISE_DEAD, 1 },
    { SPELL_DEATHLY_BRAND, 2 }, { SPELL_DEATH_COMMAND, 4 }, { SPELL_SOUL_TAP, 6 },
    { SPELL_SOUL_DRAIN, 8 }, { SPELL_CORPSE_EXPLOSION, 14 }, { SPELL_FRENZIED_LEGION, 18 },
    { SPELL_BLACK_VOLLEY, 22 }, { SPELL_SOUL_HARVEST, 28 }, { SPELL_SACRIFICIAL_PACT, 34 },
    { SPELL_CREATE_ABOMINATION, 52 }, { SPELL_ARMY_OF_THE_DAMNED, 60 }
} };
constexpr std::array<uint32, 2> RetiredSpells = { { SPELL_RAISE_DEADEYE, SPELL_RAISE_PLAGUE_MAGE } };

// Talent bonuses that run on a timer, kept on the player so they die with the session
struct NecromancerState : public DataMap::Base
{
    uint32 commanderUntil = 0;
    uint32 lordUntil = 0;
};

constexpr char const* StateKey = "NecromancerState";
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
    return uint8(RAISE_DEAD_SOULS + GetTalentValue(player, { { 90508, 1 }, { 90509, 2 } }));
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
    uint8 const target = std::min<uint8>(GetMaxSouls(player), GetSouls(player) + amount);
    Aura* aura = player->GetAura(SPELL_CAPTURED_SOULS, player->GetGUID());
    if (!aura)
        aura = player->AddAura(SPELL_CAPTURED_SOULS, player);
    if (aura)
        aura->SetStackAmount(target);
}

void AddGeneratedSouls(Player* player, uint8 amount)
{
    // Âmes débordantes
    int32 const overflowChance = GetTalentValue(player, { { 90528, 10 }, { 90529, 20 } });
    if (overflowChance && roll_chance_i(overflowChance))
        ++amount;
    AddSouls(player, amount);
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
    // Économie funèbre: every Âme spent gives mana back
    int32 const perSoul = GetTalentValue(player, { { 90513, 1 }, { 90514, 2 } });
    if (perSoul)
        RestoreMana(player, uint8(std::min<int32>(100, amount * perSoul)));
    return true;
}

uint8 GetRaiseDeadCost(Player const* player)
{
    // Maître des âmes
    return uint8(RAISE_DEAD_SOULS - GetTalentValue(player, { { 90589, 1 }, { 90590, 2 } }));
}

void RestoreMana(Player* player, uint8 percent)
{
    if (player && percent)
        player->ModifyPower(POWER_MANA, int32(CalculatePct(player->GetMaxPower(POWER_MANA), percent)));
}

// More stacks of rot on the target, or a fresh one. The aura always belongs to the Necromancer, never to the
// minion that applied it, so the ticks are his damage and his spell power scales them.
void ApplyNecroticRot(Player* owner, Unit* target, uint8 stacks)
{
    if (!IsNecromancer(owner) || !target || !target->IsAlive() || !owner->IsValidAttackTarget(target) || !stacks)
        return;

    // ModStackAmount clamps to the spell's StackAmount and refreshes the timers, so a pack under attack keeps
    // rotting for as long as the army keeps hitting it.
    Aura* rot = target->GetAura(SPELL_NECROTIC_ROT, owner->GetGUID());
    if (!rot)
    {
        rot = owner->AddAura(SPELL_NECROTIC_ROT, target);
        if (!rot)
            return;
        --stacks;
    }
    if (stacks)
        rot->ModStackAmount(stacks);
}

uint32 LearnUnlockedAbilities(Player* player)
{
    if (!IsNecromancer(player))
        return 0;
    for (uint32 spellId : RetiredSpells)
        if (player->HasSpell(spellId))
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
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
    if (!IsNecromancer(player))
        return;

    for (AbilityUnlock const& unlock : AbilityUnlocks)
        if (unlock.level > level && player->HasSpell(unlock.spellId))
            player->removeSpell(unlock.spellId, SPEC_MASK_ALL, false);
    LearnUnlockedAbilities(player);
}

float GetSpellDamageMultiplier(Player const* player, uint32 spellId, bool areaSpell)
{
    // Savoir interdit
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90500, 4 }, { 90501, 8 } })) / 100.0f;
    // Doctrine de la peste
    if (areaSpell)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90530, 8 }, { 90531, 16 } })) / 100.0f;
    // Feu des âmes
    if (spellId == SPELL_SOUL_BOLT || spellId == SPELL_SOUL_DRAIN || spellId == SPELL_SOUL_HARVEST)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90542, 5 }, { 90543, 10 } })) / 100.0f;
    // Moisson vorace
    if (spellId == SPELL_SOUL_HARVEST)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90558, 10 }, { 90559, 20 } })) / 100.0f;
    // Explosion en chaîne
    if (spellId == SPELL_CORPSE_EXPLOSION)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90436, 10 }, { 90437, 20 } })) / 100.0f;
    // Averse d'os
    if (spellId == SPELL_BLACK_VOLLEY)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90446, 10 }, { 90447, 20 } })) / 100.0f;
    // Pourriture virulente
    if (spellId == SPELL_NECROTIC_ROT)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90432, 15 }, { 90433, 30 } })) / 100.0f;
    // Commandant: Ordre de mort lends its fury to the Nécromancien's own spells
    if (IsCommanding(player))
        multiplier *= 1.15f;
    return multiplier;
}

float GetMinionDamageMultiplier(Player const* player, MinionKind kind)
{
    // Artisan des os
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90510, 5 }, { 90511, 10 } })) / 100.0f;
    // Tireurs de la crypte
    if (kind == MinionKind::Archer)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90525, 10 }, { 90526, 20 } })) / 100.0f;
    // Doctrine de la peste
    if (kind == MinionKind::Mage)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90530, 8 }, { 90531, 16 } })) / 100.0f;
    // Chair d'abomination
    if (kind == MinionKind::Abomination)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90547, 15 }, { 90548, 30 } })) / 100.0f;
    // Légion frénétique, and Frénésie parfaite on top of it
    if (player && player->HasAura(SPELL_FRENZIED_LEGION))
        multiplier *= 1.30f + float(GetTalentValue(player, { { 90539, 10 }, { 90540, 20 } })) / 100.0f;
    // Seigneur de la Légion
    if (IsLordOfTheLegion(player))
        multiplier *= 1.20f;
    return multiplier;
}

float GetMinionHealthMultiplier(Player const* player, MinionKind kind)
{
    // Cohorte immortelle
    float multiplier = 1.0f + float(GetTalentValue(player, { { 90534, 10 }, { 90535, 20 } })) / 100.0f;
    if (kind == MinionKind::Abomination)
        multiplier *= 1.0f + float(GetTalentValue(player, { { 90547, 15 }, { 90548, 30 } })) / 100.0f;
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

void StartCommander(Player* player, uint32 durationMs)
{
    if (player)
        player->CustomData.GetDefault<NecromancerState>(StateKey)->commanderUntil = getMSTime() + durationMs;
}

bool IsCommanding(Player const* player)
{
    NecromancerState const* state = player ? player->CustomData.Get<NecromancerState>(StateKey) : nullptr;
    return state && getMSTime() < state->commanderUntil;
}

void StartLordOfTheLegion(Player* player, uint32 durationMs)
{
    if (player)
        player->CustomData.GetDefault<NecromancerState>(StateKey)->lordUntil = getMSTime() + durationMs;
}

bool IsLordOfTheLegion(Player const* player)
{
    NecromancerState const* state = player ? player->CustomData.Get<NecromancerState>(StateKey) : nullptr;
    return state && getMSTime() < state->lordUntil;
}
}
