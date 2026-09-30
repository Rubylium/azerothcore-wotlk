#ifndef MOD_STAT_GROWTH_ESSENCE_TUNING_H
#define MOD_STAT_GROWTH_ESSENCE_TUNING_H

#include "Define.h"
#include "EssenceTierSystem.h"

#include <algorithm>
#include <cmath>

namespace EssenceTuning
{
// Hard limits include gear and, for resource regeneration, the essences mirrored onto bots.
constexpr uint32 MaxResourceBonus = 500;
constexpr uint32 MaxFortuneBonus = 100;
// Experience feeds the paragon bar at the level cap (AddParagonExperience): past this, paragon would come too easily.
// Essences and the gear's experience affix together.
constexpr uint32 MaxExperienceBonus = 200;

// Diminishing returns on the uncapped families (.agents/docs/systems/power-scaling.md): a character's saved points
// count as ceiling * (1 - e^(-points / ceiling)) - the first few hundred almost in full, then less and less, never
// past the ceiling. The saved points are never rewritten: only what they give is curved. The power model's expected
// player (PowerScaling.h, EssenceGrowthCeiling / EssenceVitalityCeiling) uses the same ceilings: change them together.
// - Growth: per stat (strength, agility, stamina, intellect, spirit, attack power, spell power)
// - Vitality: points of maximum health (40 health each at level 80)
constexpr float GrowthCeiling = 600.0f;
constexpr float VitalityCeiling = 1250.0f;

inline uint32 Diminished(uint32 points, float ceiling)
{
    return static_cast<uint32>(std::lround(ceiling * (1.0f - std::exp(-static_cast<float>(points) / ceiling))));
}
constexpr uint32 MaxGearFortune = 20;
constexpr uint32 MaxItemFortune = 2;
constexpr float FortuneItemScale = 0.0025f;
constexpr float MaxAffixChanceBonus = 10.0f;
constexpr float MaxAffixValueBonus = 10.0f;

constexpr uint32 CombinedBonus(uint32 permanent, uint32 equipment, uint32 borrowed, uint32 cap)
{
    return static_cast<uint32>(std::min<uint64>(static_cast<uint64>(permanent) + equipment + borrowed, cap));
}

// Effective bonuses determine remaining room. Saved points above a cap are retained, never rewritten downward.
constexpr uint32 GrantAmount(uint32 effectiveBonus, uint32 requested, uint32 cap)
{
    return effectiveBonus >= cap ? 0 : std::min(requested, cap - effectiveBonus);
}

constexpr bool IsFamilyCapped(EssenceFamily family, uint32 resource, uint32 fortune, uint32 experience)
{
    return (family == EssenceFamily::Resource && resource >= MaxResourceBonus) ||
        (family == EssenceFamily::Fortune && fortune >= MaxFortuneBonus) ||
        (family == EssenceFamily::Experience && experience >= MaxExperienceBonus);
}

constexpr uint32 ItemFortune(uint32 value)
{
    return std::min(value, MaxItemFortune);
}

inline float AffixChanceBonus(uint32 fortune, float perPoint)
{
    return std::clamp(static_cast<float>(fortune) * perPoint, 0.0f, MaxAffixChanceBonus);
}

inline float AffixValueBonus(uint32 fortune, float perPoint, float configuredMax)
{
    return std::clamp(std::min(static_cast<float>(fortune) * perPoint, configuredMax), 0.0f, MaxAffixValueBonus);
}
}

#endif
