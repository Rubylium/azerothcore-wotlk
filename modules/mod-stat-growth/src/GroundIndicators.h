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
        float inner = 0.0f;         // a ring's inner radius, as its model draws it; a cone's clear middle (its
                                    // first yards from the apex are not in it: the burnt floor round a boss)

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
    // A stock spell visual kit (SpellVisualKit) played on the ground at where, scale times its size: once, or every
    // intervalMs while it lasts (durationMs). The impact of a hit no theme fits (a weapon's dust and cracks).
    void PlayKit(Unit* owner, Position const& where, uint32 kit, float scale = 1.0f, uint32 intervalMs = 0,
                 uint32 durationMs = 3000);

    // owner's marks shown at their size at once (a boss's fight that asks it), cones too, not grown into place from a
    // yard as everyone's are by default (the client eases a scale it is told of after a mark shows)
    void DrawInstantly(Unit* owner, bool instantly = true);
    // Each draws the area for durationMs and returns it as drawn. owner is the unit the indicator belongs to (it
    // is summoned by it). With a theme, particles of it rise over the area while it is drawn.
    // hitDamage: what one hit does to a player in it (before their defences), if known - a bot stays in an area it
    // would survive well (FindEscape); 0, unknown, is always left.
    Area ShowCircle(Unit* owner, Position const& center, float radius, uint32 durationMs, Theme theme = Theme::None,
                    uint32 hitDamage = 0);
    Area ShowRectangle(Unit* owner, Position const& start, float orientation, float length, float width,
                       uint32 durationMs, Theme theme = Theme::None, uint32 hitDamage = 0);
    // clearMiddle: its first yards from the apex are not in it (the bots may cross there: something else of the
    // fight's guards it, at its own time - an execution's DOOM strikes the boss's feet, warned on its own)
    Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                  uint32 durationMs, Theme theme = Theme::None, uint32 hitDamage = 0, float clearMiddle = 0.0f);
    // A cone aimed at one unit (a tank buster that must land on its tank): that unit does not step out of it, and a
    // hit it takes from it does not count as standing in the red. Everyone else is to leave it.
    Area ShowAimedCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                       uint32 durationMs, Unit* aimedAt, Theme theme = Theme::None, uint32 hitDamage = 0,
                       float clearMiddle = 0.0f);
    // A line from start that turns radiansPerSecond (negative: the other way) while it is drawn: a laser swept round.
    // No particles. Where it points after some time: CurrentSweep.
    // look: a painted line (shapes.json kind texture) drawn in place of the red, at width as given
    // curtain: its light standing up from the floor (shapes.json kind curtain, built to the line's length), turning
    // with it on a carrier of its own
    Area ShowSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond,
                               float length, float width, uint32 durationMs, uint32 hitDamage = 0, uint32 look = 0,
                               uint32 curtain = 0);
    Area CurrentSweep(Area const& area, float radiansPerSecond, uint32 elapsedMs);
    // The same sweep with a painted line built to its size (shapes.json `scale`: never scaled, it would grow in on the
    // client), its carrier at start: a blade swept round the room
    Area ShowSweepingLine(Unit* owner, Position const& start, float orientation, float radiansPerSecond, float length,
                          float width, uint32 durationMs, uint32 look, uint32 hitDamage = 0);
    // The same line for the bots only, nothing drawn: a sweep shown by its own visual (a beam)
    void WatchSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond,
                                float length, float width, uint32 durationMs, uint32 hitDamage = 0);
    // A cone from apex aimed at one unit, as ShowAimedCone, that turns to face it wherever it goes until it lands
    // (no particles: they would stay behind). Read where it points back with CurrentCone when it resolves.
    // away: the cone points straight away from the unit instead (the far end of a cone through it and out behind)
    Area ShowTrackingCone(Unit* owner, Position const& apex, float radius, float arcDegrees, uint32 durationMs,
                          Unit* aimedAt, uint32 hitDamage = 0, bool away = false);
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
    // The same circle for the bots only, nothing drawn: a mark its carrier has to know (a debuff, the fight's rule),
    // never shown on the ground
    Area WatchCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs, uint32 hitDamage = 0,
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
    // A painted line that swings round its start: one model drawn from area's origin (look, built to its length),
    // turned `turn` radians (positive: counterclockwise) in swingMs from startMs after it shows, eased, then held until
    // durationMs. The area where it stops is registered (the bots leave it) and returned; placed: its carrier, to turn
    // its look to its blow where it lands.
    Area ShowSwingingLine(Unit* owner, Area const& area, uint32 look, float turn, uint32 startMs, uint32 swingMs,
                          uint32 durationMs, uint32 hitDamage = 0, uint32 lingerMs = 0, ObjectGuid* placed = nullptr);
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

    // An upright picture turned to the camera (shapes.json kind billboard: a boss's eye over it) on a stalker at the
    // floor under position, for durationMs; its height over the floor is the model's own (elevation). A flipbook
    // starts on its first frame as it appears. follow: a unit it moves along with (the boss). placed: its stalker.
    void ShowBillboard(Unit* owner, Position const& position, uint32 look, uint32 durationMs, Unit* follow = nullptr,
                       ObjectGuid* placed = nullptr);
    // A look worn by carrier as an aura (a carried billboard: a mark over a player's head), for durationMs (0: until
    // taken off - it still goes when they die). False if it could not be put on.
    bool ShowCarriedLook(Unit* carrier, uint32 look, uint32 durationMs);
    void ClearCarriedLook(Unit* carrier, uint32 look);
    // A ring of upright curtain pieces round center (shapes.json kind curtain, one plane, built to pieceLength: never
    // scaled), a regular polygon of `pieces` sides: their corners on the circle of radius
    // pieceLength / (2 sin(pi / pieces)), their middles at pieceLength / (2 tan(pi / pieces)) from center. Piece i
    // wears look firstLook + i % looks: looks showing a painting's band in that many shares (`segment`), so the band
    // runs on round the ring, counterclockwise. Enough pieces read as a circle (ten long ones read as a polygon).
    void ShowCurtainRing(Unit* owner, Position const& center, uint32 firstLook, uint32 looks, uint32 pieces,
                         float pieceLength, uint32 durationMs);

    // Gardien-chef Vorhan's painted marks (shapes.json VW_*, their pictures localTools/wardenVorhan/indicatorArt.py).
    // None is an area: what they mark is the fight's to resolve.
    constexpr uint32 SPELL_WARDEN_GAZE = 94200;             // his eye opening over 6 s (64 frames), then its burst
    constexpr uint32 SPELL_WARDEN_NUMBER_FIRST = 94201;     // the numbers 1-8 over a player's head
    constexpr uint32 SPELL_WARDEN_CELL_IRON = 94209;
    constexpr uint32 SPELL_WARDEN_CELL_RUNES = 94210;
    constexpr uint32 SPELL_WARDEN_CELL_FLARE = 94211;
    constexpr uint32 SPELL_WARDEN_CELL_NUMBER_FIRST = 94212;
    constexpr uint32 SPELL_WARDEN_ROLL_CALL = 94220;
    constexpr uint32 SPELL_WARDEN_ROLL_CALL_PAIR_FIRST = 94221;  // "1-2", "3-4", "5-6", "7-8"
    constexpr uint32 SPELL_WARDEN_WALL_SHOCKWAVE = 94226;
    constexpr uint32 SPELL_WARDEN_WALL_FIRST = 94227;          // its three pieces, each a third of the band
    constexpr uint32 SPELL_WARDEN_STRIKE = 94230;              // the isolation's blow: a cone of fire from his fist
    constexpr uint32 SPELL_WARDEN_PUSH_TRAIL = 94231;          // the furrows the thrown tank leaves
    constexpr uint32 SPELL_WARDEN_SEAL_BURST = 94232;          // the seal going off round the tank
    constexpr uint32 SPELL_WARDEN_CELL_BARS = 94233;           // a side of a cell's cage of red-hot bars
    constexpr uint32 SPELL_WARDEN_CURFEW_RING = 94234;         // the curfew's dial round the room
    constexpr uint32 SPELL_WARDEN_CURFEW_MARK = 94235;         // the curfew's hourglass over each head
    constexpr uint32 SPELL_WARDEN_CELL_BARS_LOW = 94236;       // the cage's side, knee-high, before the doors
    constexpr uint32 SPELL_WARDEN_ROLL_CALL_BLOW = 94247;      // the roll call's blow at each player (harmless)
    constexpr uint32 SPELL_WARDEN_BURNT_FLOOR = 94248;         // an execution's burnt floor (from 5 yards out)
    constexpr uint32 SPELL_WARDEN_PUNISHMENT = 94249;          // Châtiment exemplaire: the tanks' cone
    // The gaze's model: one eye, 8 x 4 yards, its middle this high over the floor (on the warden's chest and head,
    // drawn over his body), its burst on from WardenGazeBurstMs
    constexpr float WardenGazeElevation = 4.5f;
    constexpr uint32 WardenGazeBurstMs = 6000;
    // The electrified wall: 30 pieces of 6.27 yards, 4.7 high, their corners on the 30-yard circle, their middles
    // 29.84 yards from the middle; the band (4 wide for 1 high, 18.85 yards) runs on round it, ten times. Each rises
    // from the floor and fades in over half a second as it appears, and fades out as it goes.
    constexpr uint32 WardenWallPieces = 30;
    constexpr uint32 WardenWallLooks = 3;
    constexpr float WardenWallPieceLength = 6.2717f;
    // The wave that raises it: from the middle out to the wall's 30 yards in WardenShockwaveArriveMs (fast, settling),
    // fading out after; shown WardenShockwaveMs. The wall is best started WardenWallRiseDelayMs after the wave, so it
    // rises as the wave reaches it.
    constexpr float WardenShockwaveRadius = 30.0f;
    constexpr uint32 WardenShockwaveArriveMs = 600;
    constexpr uint32 WardenShockwaveMs = 1400;
    constexpr uint32 WardenWallRiseDelayMs = 400;
    // The isolation's blow (shapes.json VW_Strike: a 14-yard cone, flashing on, held near a second, fading), from
    // `from` towards orientation; the furrows of the tank's throw (VW_PushTrail), 25 yards from where it stood along
    // orientation, burning out over 4.5 s; the seal's burst round center (VW_SealBurst), a blast out to 40 yards in a
    // fifth of a second, its lethal ring at 16, gone within a second. Each model is built to its size (a scale set as
    // it appears grows in slowly on the client). Only shown: none is an area.
    constexpr uint32 WardenStrikeMs = 1800;
    constexpr uint32 WardenPushTrailMs = 4600;
    constexpr uint32 WardenSealBurstMs = 1000;
    void ShowWardenStrike(Unit* owner, Position const& from, float orientation);
    void ShowWardenPushTrail(Unit* owner, Position const& from, float orientation);
    void ShowWardenSealBurst(Unit* owner, Position const& center);
    // The roll call answered (VW_RollCallBlow: a 56-yard cone of fire, harmless): from `from` towards orientation,
    // growing out of its apex to its full size in WardenRollCallBlowGrowMs, held, fading out by WardenRollCallBlowMs
    constexpr uint32 WardenRollCallBlowGrowMs = 1000;
    constexpr uint32 WardenRollCallBlowMs = 2600;
    void ShowWardenRollCallBlow(Unit* owner, Position const& from, float orientation);
    // One of his marks built to its size (the burnt floor: 40 yards; the punishment: 14), its apex at `from`, turned
    // to orientation, for durationMs (it fades out at its end). Drawn only: the fight registers what hits.
    void ShowWardenMark(Unit* owner, Position const& from, float orientation, uint32 look, uint32 durationMs);
    // A cell's cage round center: sixteen sides of red-hot bars on the cell's 3-yard circle, rising out of the floor as
    // they come, for durationMs - knee-high while the players find their cells (VW_CellBarsLow), full height once the
    // doors slam on them (tall, VW_CellBars)
    void ShowWardenCellBars(Unit* owner, Position const& center, uint32 durationMs, bool tall);
    // The curfew's dial round the whole room at center (VW_CurfewRing, 31 yards: its edge, breathing faster and faster
    // until the bell), for durationMs
    void ShowWardenCurfew(Unit* owner, Position const& center, uint32 durationMs);
    // The eye over owner at position (its stalker's place; the model stands WardenGazeElevation over the floor there),
    // for durationMs: the cast's 6 s and a moment of its burst. follow: the boss, if it moves meanwhile.
    void ShowWardenGaze(Unit* owner, Position const& position, uint32 durationMs, Unit* follow = nullptr);
    // number (1-8) over carrier's head, for durationMs (0: until cleared); any other of the eight is taken off first
    bool ShowCarriedNumber(Unit* carrier, uint32 number, uint32 durationMs);
    void ClearCarriedNumber(Unit* carrier);
    // A cell seal at center, radius yards: its iron, its fel runes turning slowly and breathing, its number (1-8)
    // breathing in the middle, the number's top pointing orientation (from the room's middle to the cell: it reads
    // from the middle)
    void ShowCell(Unit* owner, Position const& center, uint32 number, float orientation, uint32 durationMs,
                  float radius = 3.0f);
    // The cell's runes flaring as the doors slam (a flash, then fading out over durationMs), over its seal
    void FlareCell(Unit* owner, Position const& center, float radius = 3.0f, uint32 durationMs = 1800);
    // A roll call mark at center, radius yards, reading pair (0: "1-2", 1: "3-4", 2: "5-6", 3: "7-8"), its top
    // pointing orientation. Its rim and numbers breathe faster and faster over the 6 s cast from when it is put on.
    void ShowRollCallMark(Unit* owner, Position const& center, uint32 pair, float orientation, uint32 durationMs,
                          float radius = 2.0f);
    // The electrified wall round center (ShowCurtainRing of its pieces), flowing and flickering, for durationMs from
    // when it starts: delayMs after this call (WardenWallRiseDelayMs after the wave: it rises as the wave arrives)
    void ShowWardenWall(Unit* owner, Position const& center, uint32 durationMs, uint32 delayMs = 0);
    // The wave that raises the wall: a ring of fel fire and dark ash on the floor, growing from center to the wall
    // (WardenShockwaveArriveMs), then fading. Dim on purpose: no flash.
    void ShowWardenShockwave(Unit* owner, Position const& center, uint32 durationMs = WardenShockwaveMs);
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
    // particles: the golden ones over it (they last durationMs); a fight that draws the soak itself and ends it
    // early (EndSoak) leaves them out
    void ShowSoak(Unit* owner, Position const& center, float radius, uint32 durationMs, uint32 wanted,
                  Theme theme = Theme::Holy, bool tanks = false, bool particles = true);
    // The soak of owner's at center taken off before its time (its bots stop going there)
    void EndSoak(Unit* owner, Position const& center);
    // A spot for one bot of its own (a light to pick up, the altar to bring it to): it goes there until within
    // radius. Set again as it changes; EndUnitSpot when it has nothing more to do there.
    void SetUnitSpot(Unit* owner, Unit* unit, Position const& spot, float radius, uint32 durationMs);
    void EndUnitSpot(Unit* owner, Unit* unit);
    // hold: a spot the tank must stand on until it lands (a Bastion tower, a hammer to take): it goes to within
    // OffTankHoldSlack of it and holds there (HoldsOffTankSpot), instead of following its target about
    // tank: the one tank it is for, whoever the owner is hitting; otherwise any tank the owner is not hitting. A fight
    // that names its off-tank keeps the two tanks apart through a taunt (the spot no longer jumps to whichever tank
    // lost the boss a moment).
    void SetOffTankSpot(Unit* owner, Position const& spot, uint32 durationMs, bool hold = false, Unit* tank = nullptr);
    // A look's fading twin (its alpha falling to nothing in FadingTwinMs from when it is put on), 0 if it has none: a
    // fight's own looks turn to it before they go, as the indicators do on their own
    constexpr uint32 FadingTwinMs = 300;
    uint32 FadingTwinOf(uint32 look);

    // The tank spot: where the tank owner is hitting should hold it (the middle of a room whose mechanics are laid
    // around it), slack yards of it. A bot tank further away walks back to it, the boss following; nothing while it
    // dodges (FindEscape first). Set again while it applies (a boss held still for an intermission has none).
    void SetTankSpot(Unit* owner, Position const& spot, uint32 durationMs, float slack);
    // The tank swap: which tank should hold owner now (a fight whose boss stacks a debuff on its tank says so, set
    // again while it applies). The bot tank named takes the boss with its taunt (mod-playerbots "fight taunt"); the
    // other tanks leave it to that one, their taunt on losing aggro held back (BossToTaunt, LeavesToOtherTank).
    void SetBossHolder(Unit* owner, Unit* tank, uint32 durationMs);
    // The boss unit is to taunt now: a holder names it and the boss is not on it; nullptr otherwise
    Unit* BossToTaunt(Unit* unit);
    // Whether target is a boss whose holder is another tank than unit: unit is not to taunt it back
    bool LeavesToOtherTank(Unit* unit, Unit* target);
    // Whether owner places its tanks itself (an off-tank spot on show): their own facing of it (mod-playerbots "tank
    // face") would only drag its aimed cones across the group
    bool PlacesTanks(Unit* owner);
    // Whether unit is a tank standing on a spot it must hold (SetOffTankSpot hold)
    bool HoldsOffTankSpot(Unit* unit);
    // Whether unit should go somewhere for one of those, and where. Always after FindEscape: the red comes first.
    bool FindGoal(Unit* unit, Position& spot, bool tank);
    // Whether unit stands in the soak it was given (bots are shared out between the soaks shown, one each): it holds
    // there until it lands rather than walking back to its fight
    bool HoldsSoak(Unit* unit);
    // Whether the fight wants unit somewhere: a soak it was given, a spot of its own, an off-tank spot it holds, a
    // circle it carries away. A bot then casts nothing that moves it (a warrior's charge took it off its tower and
    // back to the boss).
    bool HasFightPlace(Unit* unit);
    // Whether unit stands on the spot of its own it was given (SetUnitSpot): it holds there (a melee bot no longer
    // chases its target back out of it; a ranged one keeps casting from it)
    bool HoldsUnitSpot(Unit* unit);
    // A curfew: unit is not to move at all for durationMs (bots: mod-playerbots AvoidGroundIndicatorAction holds it,
    // its chase stopped), whatever else it does. HoldsStill: whether one applies now.
    // A beam from one unit to another that follows both (a chain between two players): spell, a beam or a channel,
    // cast by an invisible stalker following `from` on another following `to` - never by the units themselves (a
    // channelling bot stops in its tracks). EndTether takes it down before its time.
    ObjectGuid ShowTether(Unit* owner, Unit* from, Unit* to, uint32 spell, uint32 durationMs);
    void EndTether(Unit* owner, ObjectGuid tether);
    void SetHoldStill(Unit* owner, Unit* unit, uint32 durationMs);
    bool HoldsStill(Unit* unit);
    // A gaze about to open: everyone near owner turns their back on where owner stands now, for durationMs (bots stop
    // attacking and casting meanwhile, a swing or a cast would turn them back). LooksAway: whether unit should, and
    // from where.
    void SetLookAway(Unit* owner, uint32 durationMs);
    bool LooksAway(Unit* unit, Position& from);
    // The bots given owner's soak at center (a fight's log of a soak that failed)
    std::vector<ObjectGuid> SoakAssignees(Unit* owner, Position const& center);

    // A hit on the whole group coming in inMs (a shared tower, a raid-wide blast): the bots' healers and others with
    // a group defensive put it up before it lands (mod-playerbots "group damage soon")
    void WarnGroupDamage(Unit* owner, uint32 inMs);
    // Whether such a hit lands on unit's group within withinMs
    bool GroupDamageSoon(Unit* unit, uint32 withinMs);

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
    // Whether spot is in an area unit would leave (FindEscape's): a bot's ordinary moves (chasing its target, getting
    // in range, its formation) are not to end there - only an escape crosses the red
    bool KeepsOutOf(Unit* unit, Position const& spot);
    // Whether a straight move of unit to spot ends in such an area or crosses one on the way (a blink, a leap: no
    // walking round it then)
    bool CrossesAreas(Unit* unit, Position const& spot);
    // A walk to goal that would cross an area unit is to keep out of: the waypoint to go round by first (false: the
    // straight walk is clean, or no way round was found)
    bool Detour(Unit* unit, Position const& goal, Position& waypoint);
    // Whether a straight walk of unit to goal crosses an area striking before it is across (at its run speed), sooner
    // than where it stands strikes: an ordinary move waits for it to strike instead (rings struck in turn: back to its
    // boss through the next one)
    bool StruckOnTheWay(Unit* unit, Position const& goal);
    // Whether the fight gave unit a spot of its own (SetUnitSpot) it is not on yet, clear of what will strike there
    // and on the way: it goes there before anything else - the fight placed it (a spread's spot, its soak's side).
    // The circles players carry are left out (the fight placed them too); in its escapes as well.
    bool FindOwnSpot(Unit* unit, Position& spot);
    // Whether spot, a way out unit chose a moment ago, still is one as FindEscape takes it (out of what strikes
    // first, in a later ring at most): a bot keeps it rather than turning to an equal one on the other side
    bool StillAWayOut(Unit* unit, Position const& spot);

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
