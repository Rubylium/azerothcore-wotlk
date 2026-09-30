#ifndef MOD_STAT_GROWTH_ESSENCE_TUNING_H
#define MOD_STAT_GROWTH_ESSENCE_TUNING_H

#include "Define.h"
#include "EssenceTierSystem.h"

#include <algorithm>

namespace EssenceTuning
{
// Hard limits include gear and, for resource regeneration, the essences mirrored onto bots.
constexpr uint32 MaxResourceBonus = 500;
constexpr uint32 MaxFortuneBonus = 100;
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

constexpr bool IsFamilyCapped(EssenceFamily family, uint32 resource, uint32 fortune)
{
    return (family == EssenceFamily::Resource && resource >= MaxResourceBonus) ||
        (family == EssenceFamily::Fortune && fortune >= MaxFortuneBonus);
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
