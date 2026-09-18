// Shared helpers for the Pestiféré (class 12): Virulence, enemy lookup, plague rescaling and levelling.

#include "Pestifere.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "World.h"

namespace Pestifere
{
bool IsPestifere(Player const* player)
{
    return player && player->getClass() == CLASS_PESTIFERE;
}

bool CarriesOwnPlague(Unit const* unit, uint32 selfSpellId)
{
    return unit && unit->HasAura(selfSpellId, unit->GetGUID());
}

uint8 GetVirulence(Unit const* unit)
{
    if (!unit)
        return 0;

    uint8 virulence = 0;
    for (Plague const& plague : Plagues)
        if (CarriesOwnPlague(unit, plague.selfSpellId))
            ++virulence;

    // The avatar counts as one plague more, but only on top of a real one
    if (virulence && unit->HasAura(SPELL_AVATAR_DE_LA_PESTE))
        ++virulence;

    return virulence;
}

std::list<Unit*> GetEnemiesInRange(Unit* caster, float radius)
{
    return GetEnemiesAround(caster, caster, radius);
}

std::list<Unit*> GetEnemiesAround(Unit* caster, WorldObject* center, float radius)
{
    std::list<Unit*> enemies;
    if (!caster || !center)
        return enemies;

    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, caster, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, enemies, check);
    Cell::VisitObjects(center, searcher, radius);
    enemies.remove_if([caster](Unit* enemy)
    {
        return !enemy->IsAlive() || !caster->IsValidAttackTarget(enemy);
    });
    return enemies;
}

void RecalculatePlagues(Unit* unit, Aura const* skip)
{
    if (!unit)
        return;

    for (Plague const& plague : Plagues)
    {
        Aura* carried = unit->GetAura(plague.selfSpellId, unit->GetGUID());
        if (!carried || carried == skip)
            continue;

        for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
            if (AuraEffect* effect = carried->GetEffect(effIndex))
                effect->RecalculateAmount(unit);
    }
}

void SetPlagueExpiring(Aura* plague, bool expiring)
{
    if (!plague)
        return;

    // The client draws a countdown whenever the maximum duration is positive, and none when it is -1, so the
    // buff itself shows the plague running down. SetDuration is the call that sends the aura update.
    int32 const duration = expiring ? PLAGUE_OUT_OF_COMBAT_DECAY : -1;
    plague->SetMaxDuration(duration);
    plague->SetDuration(duration);
}

void SetCarriedPlaguesExpiring(Player* player, bool expiring)
{
    if (!IsPestifere(player))
        return;

    for (Plague const& plague : Plagues)
        SetPlagueExpiring(player->GetAura(plague.selfSpellId, player->GetGUID()), expiring);
}

void GiveRage(Player* player, uint32 rage)
{
    if (!player || !rage)
        return;

    player->ModifyPower(POWER_RAGE, int32(rage * RAGE_UNIT));
}

float GetRageFromDamageTaken(Unit const* unit, uint32 damage)
{
    if (!unit || !damage)
        return 0.0f;

    // Unit::RewardRage, victim side
    float const level = float(unit->GetLevel());
    float rageConversion = 0.0091107836f * level * level + 3.225598133f * level + 4.2652911f;
    if (unit->GetLevel() > 70)
        rageConversion += 13.27f * (level - 70.0f);

    return float(damage) / rageConversion * 2.5f * sWorld->getRate(RATE_POWER_RAGE_INCOME);
}

void GiveRagePoints(Player* player, float rage)
{
    int32 const tenths = int32(rage * float(RAGE_UNIT));
    if (player && tenths > 0)
        player->ModifyPower(POWER_RAGE, tenths);
}

uint32 LearnUnlockedAbilities(Player* player)
{
    if (!IsPestifere(player))
        return 0;

    uint32 learned = 0;
    for (AbilityUnlock const& unlock : AbilityUnlocks)
    {
        if (player->GetLevel() < unlock.level || player->HasSpell(unlock.spellId))
            continue;

        player->learnSpell(unlock.spellId, false);
        if (player->HasSpell(unlock.spellId))
            ++learned;
    }
    return learned;
}
}
