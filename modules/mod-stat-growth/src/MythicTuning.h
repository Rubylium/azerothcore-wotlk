#ifndef MOD_STAT_GROWTH_MYTHIC_TUNING_H
#define MOD_STAT_GROWTH_MYTHIC_TUNING_H

#include "Define.h"
#include "GroundIndicators.h"

class SpellInfo;
class Unit;

// Mythic dungeon damage tuning, shared by every dungeon (modules/mod-stat-growth/src/mythic/*.cpp registers its own
// numbers at startup). The audit behind the numbers is in .agents/plans/mplus-damage-audit/.
//
// The yardstick is the reference health: what a damage dealer at the recommended paragon has at a key, 90 000 at +10,
// growing with the key like the creatures do (so a share of it stays the same danger at every key). Budgets, as a
// share of it: a telegraphed hit to dodge 45-70%, a small or frequent one 20-35%, an unavoidable group pulse 8-15%, a
// damage over time 20-40% in all; a tank buster 35-50% of a tank (about 1.45 times the reference), a boss swing 6-10%.
namespace MythicTuning
{
    // Multiplies a creature spell's damage in mythic dungeons, on top of the key's scaling. By spell id, as cast (the
    // heroic id in a heroic dungeon). Registered at startup.
    void SetSpellMultiplier(uint32 spellId, float multiplier);
    float SpellMultiplier(SpellInfo const* spellInfo);

    // Multiplies a creature's melee (weapon) damage in mythic dungeons, by entry (the heroic entry in a heroic dungeon)
    void SetMeleeMultiplier(uint32 creatureEntry, float multiplier);
    float MeleeMultiplier(uint32 creatureEntry);

    // A damage dealer's health at a key level (the key of the caster's instance; 0 for Mythique 0)
    float ReferenceHealth(int32 keyLevel);
    float ReferenceHealth(Unit const* caster);

    // Deals `amount` as `spellId` from caster to target, `amount` read like a spell's own value: the key's scaling
    // applies (and the spell's tuning multiplier), then the target's defensives, resistances and absorbs. It reads as
    // that spell in the combat log and meters. Name a spell without a weapon effect: a weapon spell is not scaled.
    void DealAbilityDamage(Unit* caster, Unit* target, uint32 spellId, uint32 amount);

    // Deals `percent` of the reference health at the caster's key, as `spellId`, whatever the spell or the dungeon's
    // era: the way to set new abilities. The target's defensives, resistances and absorbs still apply.
    void DealReferenceDamage(Unit* caster, Unit* target, uint32 spellId, float percent);
}

// Telegraphed abilities for trash (MythicTuning.cpp runs them): a creature of a listed entry, in combat in a mythic
// dungeon, now and then draws a red area, holds still while it shows, and then hits everyone still inside for a share
// of the reference health. Killing it before it lands cancels it. At most one trash ability begins per instance every
// TrashGlobalGapMs, so a big pull does not paint the room red.
namespace MythicTrash
{
    enum class Shape : uint8
    {
        AroundSelf,         // a circle of `size` around the creature
        UnderTarget,        // a circle of `size` under a random player within 30 yards, where they stood
        ConeAtVictim,       // a cone of `size` and `width` degrees from the creature towards its victim
        LineAtVictim,       // a rectangle `size` long and `width` wide from the creature towards its victim
        CarriedByTarget     // a circle of `size` following a random player: it hurts the others still next to them
    };

    struct Ability
    {
        uint32 entry = 0;                   // creature entry (register the heroic entry too when it has one)
        Shape shape = Shape::AroundSelf;
        float size = 8.0f;
        float width = 0.0f;                 // rectangle width in yards, cone arc in degrees
        uint32 warnMs = 2500;               // how long the red shows before it lands
        uint32 cooldownMs = 18000;
        uint32 firstMs = 8000;              // after entering combat (each creature gets up to a third more at random)
        float percent = 30.0f;              // of the reference health, to each player hit
        uint32 spellId = 0;                 // the spell it is named as in the log (not a weapon spell)
        GroundIndicators::Theme theme = GroundIndicators::Theme::None;
        bool holdStill = true;              // rooted while the warning shows (a cone or line would be meaningless)
    };

    void Register(Ability const& ability);
}

void AddMythicTuningScripts();

#endif
