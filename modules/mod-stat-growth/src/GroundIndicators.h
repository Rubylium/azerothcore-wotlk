#ifndef MOD_STAT_GROWTH_GROUND_INDICATORS_H
#define MOD_STAT_GROWTH_GROUND_INDICATORS_H

#include "Define.h"
#include "Position.h"

class Unit;
class WorldObject;
class ObjectGuid;

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
            Cone,       // from origin, around its orientation
            Ring,       // around origin, between inner and radius: the middle is safe
            Cross       // four arms from origin (forward, back, both sides), radius long and width wide
        };

        Kind kind = Kind::Circle;
        Position origin;
        float radius = 0.0f;        // a circle's, a cone's or a ring's (outer) radius, a rectangle's length
        float width = 0.0f;         // a rectangle's width, as its model draws it
        float arc = 0.0f;           // a cone's full arc, in radians, as its model draws it
        float inner = 0.0f;         // a ring's inner radius, as its model draws it

        // Whether a point stands in the area, on the ground (height is ignored), grown by margin yards
        [[nodiscard]] bool Contains(Position const& point, float margin = 0.0f) const;
    };

    // Particles over an area while its warning runs, in the colours of what is coming: the red says where, the
    // particles say what. Stock spell visuals played at the feet of invisible emitters spread over the area.
    enum class Theme : uint8
    {
        None,
        Shadow,
        Fire,
        Frost,
        Nature,
        Arcane,
        Holy
    };
    // The theme of a spell school mask (SpellSchoolMask); None for a plain physical one
    Theme ThemeOf(uint32 schoolMask);
    // Particles over area for durationMs. The Show functions do it themselves when given a theme.
    void ShowParticles(Unit* owner, Area const& area, Theme theme, uint32 durationMs);
    // The theme's burst on the ground at where: for the moment something lands
    void Burst(Unit* owner, Position const& where, Theme theme);

    // Each draws the area for durationMs and returns it as drawn. owner is the unit the indicator belongs to (it
    // is summoned by it). With a theme, particles of it rise over the area while it is drawn.
    // hitDamage: what one hit does to a player in it (before their defences), if known - a bot stays in an area it
    // would survive well (FindEscape); 0, unknown, is always left.
    Area ShowCircle(Unit* owner, Position const& center, float radius, uint32 durationMs, Theme theme = Theme::None,
                    uint32 hitDamage = 0);
    Area ShowRectangle(Unit* owner, Position const& start, float orientation, float length, float width,
                       uint32 durationMs, Theme theme = Theme::None, uint32 hitDamage = 0);
    Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                  uint32 durationMs, Theme theme = Theme::None, uint32 hitDamage = 0);
    // A cone aimed at one unit (a tank buster that must land on its tank): that unit does not step out of it, and a
    // hit it takes from it does not count as standing in the red. Everyone else is to leave it.
    Area ShowAimedCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                       uint32 durationMs, Unit* aimedAt, Theme theme = Theme::None, uint32 hitDamage = 0);
    // A line from start that turns radiansPerSecond (negative: the other way) while it is drawn: a laser swept round.
    // No particles. Where it points after some time: CurrentSweep.
    // look: a painted line (shapes.json kind texture) drawn in place of the red, at width as given
    // curtain: its light standing up from the floor (shapes.json kind curtain, built to the line's length), turning
    // with it on a carrier of its own
    Area ShowSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond,
                               float length, float width, uint32 durationMs, uint32 hitDamage = 0, uint32 look = 0,
                               uint32 curtain = 0);
    Area CurrentSweep(Area const& area, float radiansPerSecond, uint32 elapsedMs);
    // The same line for the bots only, nothing drawn: a sweep shown by its own visual (a beam)
    void WatchSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond,
                                float length, float width, uint32 durationMs, uint32 hitDamage = 0);
    // A cone from apex aimed at one unit, as ShowAimedCone, that turns to face it wherever it goes until it lands
    // (no particles: they would stay behind). Read where it points back with CurrentCone when it resolves.
    Area ShowTrackingCone(Unit* owner, Position const& apex, float radius, float arcDegrees, uint32 durationMs,
                          Unit* aimedAt, uint32 hitDamage = 0);
    // Where a tracking cone points now
    Area CurrentCone(Unit* aimedAt, Area const& area);
    // Everything between innerRadius and outerRadius around center: only the middle is safe. The rings come in a
    // few proportions (inner / outer 0.2, 0.4, 0.6, 0.8): the nearest one to what is asked is drawn, and returned.
    Area ShowRing(Unit* owner, Position const& center, float outerRadius, float innerRadius, uint32 durationMs,
                  Theme theme = Theme::None, uint32 hitDamage = 0);
    // A circle that follows carrier wherever it goes: whoever carries it should take it away from the others.
    // Read its position back with CurrentArea when it resolves.
    // keepAway: past its drawn radius, how far its carrier keeps from the others (a hit that falls off with
    // distance past the red); the carrier then holds there until it lands (KeepsAway)
    Area ShowCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs, uint32 hitDamage = 0,
                           float keepAway = 0.0f);
    // Whether unit carries a circle of someone else's: it keeps away from the group and does not come back to fight
    // in melee until the circle lands (bots: mod-playerbots AvoidGroundIndicatorAction)
    bool KeepsAway(Unit* unit);
    // Four arms of a star around carrier (CarriedStarArm long, CarriedStarWidth wide): it follows them where they go,
    // its arms pointing a direction of their own (not turning with the carrier). Read it back with CurrentArea when it
    // resolves.
    constexpr float CarriedStarArm = 10.0f;
    constexpr float CarriedStarWidth = 3.0f;
    Area ShowCarriedStar(Unit* owner, Unit* carrier, uint32 durationMs, uint32 hitDamage = 0);
    // An area the bots keep out of, drawn by nothing here: its own visual says where it is (a black hole's pool)
    // tanksTake: a hit a tank must take (a hammer marked for it): the tanks stay in it, only the others leave
    void WatchArea(Unit* owner, Area const& area, uint32 durationMs, uint32 hitDamage = 0, bool tanksTake = false);
    // Where a carried area is now (it moves with its carrier; a star turns with them too)
    Area CurrentArea(Unit* carrier, Area const& area);
    // An area drawn as a painted ability in place of the red (shapes.json kind texture: a line, a cone, a ring or a
    // circle painted to the area's own proportions), and registered as any red one: the bots leave it, a hit in it is
    // one in the red. area: its kind, origin (and facing) and size, as the painting was made for (a line's width is its
    // length over the painting's ratio, a ring's inner radius its proportion of the outer).
    // placed: the stalker carrying it, to change its look where it lands (a new stalker fades in as the client shows
    // any new unit: a flash made of one appeared half-way and went); lingerMs: how long it stays past the warning
    Area ShowPainted(Unit* owner, Area const& area, uint32 look, uint32 durationMs, Theme theme = Theme::None,
                     uint32 hitDamage = 0, ObjectGuid* placed = nullptr, uint32 lingerMs = 0);
    // A painted line in pieces (shapes.json `segment`): `count` models from firstSpell, each `ratio` times as long as
    // the line is wide, chained along it, each on a carrier of its own at its middle. One long model's carrier stood
    // at the line's end, often past a wall: the client hides a unit it cannot see from the camera, and the whole line
    // went with it, depending on the view.
    struct PaintedLine
    {
        uint32 firstSpell = 0;
        uint32 count = 4;
        float ratio = 2.0f;
        bool builtToSize = false;   // its models are built at the piece's length (shapes.json scale): never scaled
        uint32 curtain = 0;         // the first of its pieces' curtains (shapes.json kind curtain: its light standing
                                    // up from the floor, 3D), `count` of them; 0 for none
    };
    // A rectangle area drawn as a painted line (ShowPainted otherwise). clipCenter / clipRadius: only its part within
    // that circle is drawn (a chamber: pieces past its walls would be hidden anyway); the area itself is registered
    // whole. lingerMs: how long the pieces stay past the warning (RepaintLine).
    Area ShowPaintedLine(Unit* owner, Area const& area, PaintedLine const& look, uint32 durationMs,
                         Theme theme = Theme::None, uint32 hitDamage = 0, uint32 lingerMs = 0,
                         Position const* clipCenter = nullptr, float clipRadius = 0.0f);
    // The pieces of a line shown with look `from` turned to look `to` (a warning's to its hit, where it lands)
    void RepaintLine(Unit* owner, Area const& area, PaintedLine const& from, PaintedLine const& to);
    // A picture painted on the ground at center, radius yards, turned to orientation, for durationMs: a boss's sigil
    // (localTools/groundIndicators/shapes.json, kind image: spells 90734-90737). Nothing to leave: no area, no bot
    // reads it - what it marks is told to them by the area or the soak shown with it.
    constexpr uint32 SPELL_SIGIL_RADIANT = 90734;
    constexpr uint32 SPELL_SIGIL_BASTION = 90735;
    constexpr uint32 SPELL_SIGIL_VOID = 90736;
    constexpr uint32 SPELL_SIGIL_AEGIS = 90737;
    void ShowDecal(Unit* owner, Position const& center, float orientation, float radius, uint32 durationMs,
                   uint32 spellId);
    // Ends every area owner has on show, for the bots too (a fight reset while a long one was still drawn). Its
    // stalkers are its summons: despawning them is the owner's business.
    void ClearAreasOf(Unit* owner);

    // Where a fight wants the players to stand, for bots (FindGoal). Nothing red: a soak is shown by golden
    // particles over it, the off-tank's spot not at all.
    // - A soak: a circle `wanted` players should stand in together (a shared hit). The players nearest to it go,
    //   tanks aside, until that many stand in it.
    // - The off-tank's spot: where a tank that is not owner's current target should stand (a boss aiming a cleave at
    //   each tank: the two apart, their cones away from the group). Set again as the boss moves.
    // tanks: the tanks are sent too (a soak while the boss holds still); otherwise they stay on it
    void ShowSoak(Unit* owner, Position const& center, float radius, uint32 durationMs, uint32 wanted,
                  Theme theme = Theme::Holy, bool tanks = false);
    // hold: a spot the tank must stand on until it lands (a Bastion tower, a hammer to take): it goes to within
    // OffTankHoldSlack of it and holds there (HoldsOffTankSpot), instead of following its target about
    void SetOffTankSpot(Unit* owner, Position const& spot, uint32 durationMs, bool hold = false);
    // Whether unit is a tank standing on a spot it must hold (SetOffTankSpot hold)
    bool HoldsOffTankSpot(Unit* unit);
    // Whether unit should go somewhere for one of those, and where. Always after FindEscape: the red comes first.
    bool FindGoal(Unit* unit, Position& spot, bool tank);
    // Whether unit stands in the soak it was given (bots are shared out between the soaks shown, one each): it holds
    // there until it lands rather than walking back to its fight
    bool HoldsSoak(Unit* unit);

    // Whether unit stands in an area it should leave: in one of the red areas around it, or carrying one next to
    // another player. If so, escape is the nearest spot where it would not. An area whose hit is known and would
    // leave the unit above 30% of its health is not one to leave (bots keep fighting in it), unless Imprudence has
    // piled up on it.
    //
    // A tank holds its ground against a trash creature's circle around itself while that creature is attacking the
    // tank: the creature follows it out, so stepping away only drags the pack and brings the next circle along, and
    // a hall of casters doing it in turn chased a tank to its death. The melee around it step out instead. A tank
    // carrying a circle does not run from the group either; the group leaves it. Bosses' circles, and every area
    // laid elsewhere, a tank still dodges.
    bool FindEscape(Unit* unit, Position& escape, bool tank = false);

    // Whether victim stands in a red area of attacker's (or of its summoner's), on show or ended a moment ago: a hit
    // it took from it was one to dodge. A circle it carries itself, and a trash circle around a creature fighting it
    // as its tank, do not count.
    bool StoodInAreaOf(Unit* victim, Unit* attacker);
}

void AddGroundIndicatorScripts();
// OnyxiaRework.cpp: Onyxia's fight rebuilt on the indicators
void AddOnyxiaReworkScripts();
void AddBronjahmReworkScripts();
void AddDevourerReworkScripts();
// InfiniteGod.cpp: L'Infini, the Défi board's god fight in the Celestial Planetarium
void AddInfiniteGodScripts();
// HollowVoice.cpp: The Hollow Voice, the board's pinnacle in Sunwell's M'uru chamber
void AddHollowVoiceScripts();

#endif
