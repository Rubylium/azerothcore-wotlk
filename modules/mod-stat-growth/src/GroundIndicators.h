#ifndef MOD_STAT_GROWTH_GROUND_INDICATORS_H
#define MOD_STAT_GROWTH_GROUND_INDICATORS_H

#include "Define.h"
#include "Position.h"

class Unit;
class WorldObject;

// Red ground indicators: where an enemy ability is about to land, filled in low-alpha red on the ground, following
// its slopes and floors. Every enemy area ability cast or channelled in a mythic dungeon shows one on its own (see
// GroundIndicators.cpp); a boss script places its own with the Show functions, and resolves its mechanic against
// the same Area, so what the players see is exactly what hits them. Bots read the areas to step out of them.
namespace GroundIndicators
{
    struct Area
    {
        enum class Kind : uint8
        {
            Circle,
            Rectangle,  // from origin, forward along its orientation
            Cone        // from origin, around its orientation
        };

        Kind kind = Kind::Circle;
        Position origin;
        float radius = 0.0f;        // a circle's or a cone's radius, a rectangle's length
        float width = 0.0f;         // a rectangle's width, as its model draws it
        float arc = 0.0f;           // a cone's full arc, in radians, as its model draws it

        // Whether a point stands in the area, on the ground (height is ignored), grown by margin yards
        [[nodiscard]] bool Contains(Position const& point, float margin = 0.0f) const;
    };

    // Each draws the area for durationMs and returns it as drawn. owner is the unit the indicator belongs to (it
    // is summoned by it).
    Area ShowCircle(Unit* owner, Position const& center, float radius, uint32 durationMs);
    Area ShowRectangle(Unit* owner, Position const& start, float orientation, float length, float width,
                       uint32 durationMs);
    Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                  uint32 durationMs);
    // A circle that follows carrier wherever it goes: whoever carries it should take it away from the others.
    // Read its position back with CurrentArea when it resolves.
    Area ShowCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs);
    // Where a carried circle is now (it moves with its carrier)
    Area CurrentArea(Unit* carrier, Area const& area);

    // Whether unit stands in an area it should leave: in one of the red areas around it, or carrying one next to
    // another player. If so, escape is the nearest spot where it would not.
    //
    // A tank holds its ground against a trash creature's circle around itself while that creature is attacking the
    // tank: the creature follows it out, so stepping away only drags the pack and brings the next circle along, and
    // a hall of casters doing it in turn chased a tank to its death. The melee around it step out instead. A tank
    // carrying a circle does not run from the group either; the group leaves it. Bosses' circles, and every area
    // laid elsewhere, a tank still dodges.
    bool FindEscape(Unit* unit, Position& escape, bool tank = false);
}

void AddGroundIndicatorScripts();
// OnyxiaRework.cpp: Onyxia's fight rebuilt on the indicators
void AddOnyxiaReworkScripts();
void AddBronjahmReworkScripts();

#endif
