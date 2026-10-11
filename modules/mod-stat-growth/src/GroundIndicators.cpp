#include "GroundIndicators.h"

#include "PassiveAI.h"
#include "Random.h"

#include "MythicDungeonSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "DataMap.h"
#include "DynamicObject.h"
#include "GameTime.h"
#include "Map.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include <algorithm>
#include <array>
#include <set>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>

// The red areas under enemy abilities.
//
// An indicator is an invisible stalker (900104, a copy of the stock Invisible Stalker) wearing one hidden aura whose
// visual is a flat red shape at its feet (spells 90700-90728, localTools/groundIndicators). The client does not draw
// that shape: it projects it on the ground and the floors under it, so it follows slopes and stairs instead of
// clipping through them. Every shape is one yard; the stalker's scale is the area's size in yards. A rectangle or a
// cone turns with the stalker.
//
// In a mythic dungeon, and in the raids of IndicatorRaids, they are read from the game itself, whatever the boss:
// - a hostile creature starting to cast (or channel) a harmful area ability: the area its biggest effect covers, for
//   as long as the cast lasts, taken away as soon as it lands or is interrupted (instant abilities show nothing:
//   there is nothing left to dodge);
// - a harmful spell area left on the ground (a dynamic object: Death and Decay, Defile, a pool), for as long as it
//   lasts, growing with it;
// - a hostile creature wearing an aura that keeps hurting everyone around it (a boss's spinning blades, a hazard's
//   burning or ooze): a circle carried on it, for as long as the aura lasts.
// Boss scripts place their own (the Show functions).
//
// Every area shown is also kept in a registry, per instance, until it ends: bots read it to step out of the red
// (FindEscape, mod-playerbots' "avoid indicators" strategy).
namespace
{
constexpr uint32 NPC_GROUND_INDICATOR = 900104;
constexpr uint32 SPELL_INDICATOR_CIRCLE = 90700;

struct ShapeSpell
{
    float size;         // a rectangle's length / width, a cone's arc in degrees
    uint32 spell;
};

// localTools/groundIndicators/shapes.json
constexpr std::array<ShapeSpell, 10> RectangleSpells = { {
    { 1.0f, 90701 }, { 1.5f, 90702 }, { 2.0f, 90703 }, { 3.0f, 90704 }, { 4.0f, 90705 },
    { 5.0f, 90715 }, { 6.0f, 90706 }, { 8.0f, 90707 }, { 10.0f, 90716 }, { 12.0f, 90708 },
} };
constexpr std::array<ShapeSpell, 6> ConeSpells = { {
    { 30.0f, 90709 }, { 45.0f, 90710 }, { 60.0f, 90711 }, { 90.0f, 90712 }, { 120.0f, 90713 }, { 180.0f, 90714 },
} };
// A circle that follows someone is an aura on them, drawn at their feet by the client: it moves exactly with them.
// (A stalker told to follow lagged behind, took a place relative to their facing and re-pathed all the time: the
// circle slid around its carrier.) An aura cannot be scaled, so these come at their own sizes, in yards.
constexpr std::array<ShapeSpell, 12> CarriedSpells = { {
    { 3.0f, 90717 }, { 4.0f, 90718 }, { 5.0f, 90719 }, { 6.0f, 90720 }, { 8.0f, 90721 }, { 10.0f, 90722 },
    { 12.0f, 90723 }, { 15.0f, 90724 }, { 20.0f, 90725 }, { 25.0f, 90726 }, { 30.0f, 90727 }, { 40.0f, 90728 },
} };
// A carried star (shapes.json Star10): four arms of GroundIndicators::CarriedStarArm, drawn at its own size
constexpr uint32 SPELL_INDICATOR_CARRIED_STAR = 90733;
// A ring is one yard across to its outer edge; its hole is this share of it
constexpr std::array<ShapeSpell, 4> RingSpells = { {
    { 0.2f, 90729 }, { 0.4f, 90730 }, { 0.6f, 90731 }, { 0.8f, 90732 },
} };

// Smaller is a melee swing, bigger is the whole room: neither is something to step out of
constexpr float MinRadius = 2.0f;
constexpr float MaxRadius = 45.0f;
// A channel is shown for its whole length, up to this
constexpr uint32 MaxChannelMs = 15 * IN_MILLISECONDS;
// How long past the end of its cast an indicator can stay if the cast is pushed back: it is taken away when the
// spell lands in any case, this only bounds a cast that never reports its end
constexpr uint32 CastGraceMs = 5 * IN_MILLISECONDS;

// Escaping: how far past an area's edge counts as out of it, and where to look for a spot
constexpr float EscapeMargin = 1.5f;
constexpr float InsideMargin = 0.5f;
constexpr float EscapeStep = 3.0f;
constexpr float EscapeReach = 36.0f;
// No spot clear of every area: those striking within this of the first are left, the later ones not yet
constexpr uint64 SoonestWindowMs = 500;
// A way out through an area striking before the unit is across (its run speed, this much to spare) is no way out
constexpr uint64 CrossSpareMs = 300;
// Out of the first to strike into one striking later: yards of walk a second of time it gives (up to the cap) is
// worth - the ring striking last, not the next one round (from there no time to step back in)
constexpr float LaterStrikeYardsPerSecond = 10.0f;
constexpr float LaterStrikeCapSeconds = 3.0f;
// A way out passing this close to what the unit fights (through it): this much more walk
constexpr float AcrossVictimRadius = 2.0f;
constexpr float AcrossVictimCost = 6.0f;
// ... but never into one striking this long after the first, or never (a wall's outside, a pool): a place to keep out of
constexpr uint64 StagedHorizonMs = 4000;
constexpr uint32 EscapeDirections = 16;
// No way out at EscapeDirections: once more, finer - a lane between areas (Supernova's, 30 degrees) is narrower than
// the gap between two coarse directions once the margins are taken off, and the bot stayed in and died
constexpr uint32 EscapeDirectionsFine = 48;
// A carried circle is taken this far past its own radius from every other player
constexpr float CarrierClearance = 2.0f;
// A sweeping line (ShowSweepingRectangle, WatchSweepingRectangle) read this far ahead, in that many steps
constexpr uint32 SweepLookAheadMs = 1200;
constexpr uint32 SweepLookAheadSteps = 3;
// Spreading out: each unit looks for its spot along its own directions and at its own distances (from its guid, so
// it keeps to them from one check to the next), and a spot someone already stands on costs more. Without it a whole
// raid in the same red area ran to the very same spot.
constexpr float EscapeDistanceSpread = 2.5f;
constexpr float CrowdRadius = 3.0f;
constexpr float CrowdCost = 4.0f;
// A way out or to a goal is walked straight: the walk is checked every PathSampleStep yards for the areas it crosses
// (other than the ones it starts in: those it is leaving). An escape crossing one costs PathCrossCost more than one
// that does not (bots with fire on both sides of them walked through one, took its strike, then the other's: dead);
// a goal behind one is reached by a detour (RouteAround), its waypoints sought over RouteDirections a ring.
constexpr float PathSampleStep = 1.0f;
constexpr float PathCrossCost = 60.0f;
constexpr uint32 RouteDirections = 24;
// An escape out of the soak a bot was given costs this much more than one within it (FindEscape)
constexpr float OutOfSoakCost = 30.0f;

constexpr char const* IndicatorDataKey = "GroundIndicators";

// What an indicator is drawn with: a stalker, or an aura on the one carrying it
struct Drawn
{
    ObjectGuid object;
    uint32 carriedAura = 0;     // the carried circle's spell, 0 for a stalker
    uint64 areaId = 0;          // in the registry
};

// The indicators a creature's casts put down, so each goes when its cast ends
struct CasterIndicators : DataMap::Base
{
    std::vector<std::pair<uint32, Drawn>> placed;
};

// An area on show, for as long as it lasts
struct ActiveArea
{
    uint64 id = 0;
    uint32 mapId = 0;
    uint32 instanceId = 0;
    ObjectGuid owner;           // the creature whose ability it is
    ObjectGuid carrier;
    ObjectGuid aimedAt;         // the one unit it is meant to land on (ShowAimedCone)
    ObjectGuid turnsTo;         // a cone turning to face this unit as it moves (ShowTrackingCone)
    float turnOffset = 0.0f;    // ... or to face away from it (pi)
    float carriedBase = 0.0f;   // a carried circle's radius at its carrier's scale 1: it grows with the carrier
    float sweepSpeed = 0.0f;    // a sweeping rectangle (ShowSweepingRectangle): radians a second, from sweepFromMs
    uint64 sweepFromMs = 0;
    float sweepFrom = 0.0f;     // ... its facing then
    float keepAway = 0.0f;      // a carried circle: how far its carrier keeps from the others (its hit's reach)
    bool tanksTake = false;     // a hit a tank must take (WatchArea): the tanks do not leave it, the others do
    GroundIndicators::Area area;
    uint64 endMs = 0;
    uint32 hitDamage = 0;       // one hit's expected damage to a player in it, before their defences; 0 unknown
};

// An area that ended stays known this long, hidden from the bots: the ability it announced lands as it goes (a cast
// indicator is taken away when the spell goes off, its damage comes a moment later), and a hit then still reads as
// taken standing in it (StoodInAreaOf)
constexpr uint64 EndedAreaGraceMs = 1500;

// Maps update on several threads: every access to the registry holds the lock
std::mutex RegistryLock;
std::vector<ActiveArea> Registry;
uint64 NextAreaId = 0;

uint64 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

uint64 Register(Unit* owner, Unit* carrier, GroundIndicators::Area const& area, uint32 durationMs,
    uint32 hitDamage = 0, ObjectGuid aimedAt = ObjectGuid::Empty, ObjectGuid turnsTo = ObjectGuid::Empty,
    float turnOffset = 0.0f)
{
    ActiveArea active;
    active.hitDamage = hitDamage;
    active.aimedAt = aimedAt;
    active.turnsTo = turnsTo;
    active.turnOffset = turnOffset;
    if (carrier && carrier->GetObjectScale() > 0.0f)
        active.carriedBase = area.radius / carrier->GetObjectScale();
    active.mapId = owner->GetMapId();
    active.instanceId = owner->GetInstanceId();
    active.carrier = carrier ? carrier->GetGUID() : ObjectGuid::Empty;
    active.owner = owner->GetGUID();
    active.area = area;
    active.endMs = NowMs() + durationMs;

    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(RegistryLock);
    Registry.erase(std::remove_if(Registry.begin(), Registry.end(),
        [now](ActiveArea const& entry) { return entry.endMs + EndedAreaGraceMs <= now; }), Registry.end());
    active.id = ++NextAreaId;
    Registry.push_back(active);
    return active.id;
}

// An area that changes size as it lasts (a growing pool)
void Resize(uint64 areaId, float radius)
{
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.id == areaId)
            entry.area.radius = radius;
}

// Ended now: it leaves the bots' view, and is dropped once EndedAreaGraceMs has passed
void Unregister(uint64 areaId)
{
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.id == areaId)
            entry.endMs = std::min(entry.endMs, now);
}

// Where an area is now: a carried one on its carrier, a tracking cone facing the unit it turns to, a sweeping one at
// its angle of the moment. False if what it moves with is gone.
bool Follow(ActiveArea& entry, WorldObject const& reference)
{
    if (entry.sweepSpeed != 0.0f)
        entry.area.origin.SetOrientation(Position::NormalizeOrientation(entry.sweepFrom +
            entry.sweepSpeed * float(NowMs() - entry.sweepFromMs) / 1000.0f));
    if (!entry.turnsTo.IsEmpty())
        if (Unit* turnsTo = ObjectAccessor::GetUnit(reference, entry.turnsTo))
            entry.area.origin.SetOrientation(Position::NormalizeOrientation(entry.area.origin.GetAngle(
                turnsTo->GetPositionX(), turnsTo->GetPositionY()) + entry.turnOffset));
    if (entry.carrier.IsEmpty())
        return true;
    Unit* carrier = ObjectAccessor::GetUnit(reference, entry.carrier);
    if (!carrier)
        return false;
    entry.area.origin.Relocate(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ());
    // A carried star keeps its own facing (ShowCarriedStar); a carried circle grows with its carrier
    if (entry.area.kind != GroundIndicators::Area::Kind::Cross && entry.carriedBase > 0.0f)
        entry.area.radius = entry.carriedBase * carrier->GetObjectScale();
    return true;
}

// The areas on show in unit's instance, carried ones where their carrier is now
std::vector<ActiveArea> AreasAround(Unit* unit)
{
    std::vector<ActiveArea> areas;
    uint64 const now = NowMs();
    {
        std::lock_guard<std::mutex> guard(RegistryLock);
        for (ActiveArea const& entry : Registry)
            if (entry.endMs > now && entry.mapId == unit->GetMapId() && entry.instanceId == unit->GetInstanceId())
                areas.push_back(entry);
    }

    for (ActiveArea& entry : areas)
        if (!Follow(entry, *unit))
            entry.endMs = 0;
    areas.erase(std::remove_if(areas.begin(), areas.end(),
        [](ActiveArea const& entry) { return entry.endMs == 0; }), areas.end());
    return areas;
}

ShapeSpell const& NearestShape(ShapeSpell const* begin, ShapeSpell const* end, float size)
{
    ShapeSpell const* best = begin;
    for (ShapeSpell const* shape = begin; shape != end; ++shape)
        if (std::fabs(std::log(shape->size / size)) < std::fabs(std::log(best->size / size)))
            best = shape;
    return *best;
}

// Particles: stock spell visual kits that play only a model at a unit's feet (SpellVisualKit's base effect and
// nothing else), found in the client's DBCs. A warning kit is replayed on every emitter while the area is drawn; a
// burst kit once, where something lands.
struct ThemeKits
{
    uint32 warning;
    uint32 burst;
};

ThemeKits KitsOf(GroundIndicators::Theme theme)
{
    using Theme = GroundIndicators::Theme;
    switch (theme)
    {
        case Theme::Shadow: return { 3129, 6776 };      // Shadow_Precast_Med_Base, ShadowFury_Impact_Base
        case Theme::Fire:   return { 845, 54 };         // HellFire_Impact_Base, FlameStrike_ImpactDD_Med_Base
        case Theme::Frost:  return { 9764, 160 };       // Ritual_Frost_Precast_Base, Frost_Nova_Area
        case Theme::Nature: return { 11761, 8059 };     // AcidCloudBreath_GroundSmoke, PoisonElemental_Impact_Base
        case Theme::Arcane: return { 6887, 988 };       // Ritual_Arcane_Precast_Base, ArcaneExplosion_Base
        case Theme::Holy:   return { 7005, 9263 };      // Holy_Precast_High_Base, Consecration_Impact_Base
        default:            return { 0, 0 };
    }
}

// How often a warning kit is replayed on an emitter, and how many emitters an area may take
constexpr uint32 ParticlePulseMs = 1100;
constexpr std::size_t MaxEmitters = 28;

// An invisible emitter: it plays its kit every intervalMs (once, when intervalMs is 0), a moment after it is created
// so the client has it before the first one
struct ParticleEmitterAI : public NullCreatureAI
{
    ParticleEmitterAI(Creature* creature, uint32 kit, uint32 intervalMs, uint32 firstMs)
        : NullCreatureAI(creature), _kit(kit), _interval(intervalMs), _timer(firstMs) { }

    void UpdateAI(uint32 diff) override
    {
        if (!_kit)
            return;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }

        me->SendPlaySpellVisual(_kit);
        if (_interval)
            _timer = _interval;
        else
            _kit = 0;
    }

private:
    uint32 _kit;
    uint32 _interval;
    uint32 _timer;
};

// A cone's stalker turning to face one unit for as long as it is drawn
struct TrackingConeAI : public NullCreatureAI
{
    TrackingConeAI(Creature* creature, ObjectGuid turnsTo, float offset = 0.0f)
        : NullCreatureAI(creature), _turnsTo(turnsTo), _offset(offset) { }

    void UpdateAI(uint32 diff) override
    {
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = TurnEveryMs;

        Unit* turnsTo = ObjectAccessor::GetUnit(*me, _turnsTo);
        if (!turnsTo)
            return;
        float const facing = Position::NormalizeOrientation(me->GetAngle(turnsTo) + _offset);
        float turn = std::fabs(facing - me->GetOrientation());
        turn = std::min(turn, 2.0f * float(M_PI) - turn);
        if (turn > MinTurn)
            me->SetFacingTo(facing);
    }

private:
    static constexpr uint32 TurnEveryMs = 100;
    static constexpr float MinTurn = 0.02f;
    float _offset;                              // pi: it points away from the unit

    ObjectGuid _turnsTo;
    uint32 _timer = TurnEveryMs;
};

// A sweeping rectangle's stalker turning at its pace, from its facing when it starts
struct SweepAI : public NullCreatureAI
{
    SweepAI(Creature* creature, float from, float speed) : NullCreatureAI(creature), _from(from), _speed(speed) { }

    void UpdateAI(uint32 diff) override
    {
        _elapsed += diff;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = TurnEveryMs;
        me->SetFacingTo(Position::NormalizeOrientation(_from + _speed * float(_elapsed) / 1000.0f));
    }

private:
    static constexpr uint32 TurnEveryMs = 50;

    float _from;
    float _speed;
    uint32 _elapsed = 0;
    uint32 _timer = 0;
};

// A swinging line's stalker: from its facing, turned `turn` radians in swingMs (eased in and out) once startMs have
// passed, then held
struct SwingAI : public NullCreatureAI
{
    SwingAI(Creature* creature, float from, float turn, uint32 startMs, uint32 swingMs)
        : NullCreatureAI(creature), _from(from), _turn(turn), _startMs(startMs),
          _swingMs(std::max<uint32>(swingMs, 1)) { }

    void UpdateAI(uint32 diff) override
    {
        _elapsed += diff;
        if (_done || _elapsed < _startMs)
            return;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = TurnEveryMs;
        float const t = std::min(1.0f, float(_elapsed - _startMs) / float(_swingMs));
        float const eased = t * t * (3.0f - 2.0f * t);
        me->SetFacingTo(Position::NormalizeOrientation(_from + _turn * eased));
        _done = t >= 1.0f;
    }

private:
    static constexpr uint32 TurnEveryMs = 50;

    float _from;
    float _turn;
    uint32 _startMs;
    uint32 _swingMs;
    uint32 _elapsed = 0;
    uint32 _timer = 0;
    bool _done = false;
};

// A star's stalker at its carrier's feet: stepped onto them every FollowEveryMs, in a straight line and just fast
// enough to be there by the next step, its facing fixed. (An aura on the carrier turned with the carrier's facing; a
// stock follow lagged and slid around them.)
struct FollowCarrierAI : public NullCreatureAI
{
    FollowCarrierAI(Creature* creature, ObjectGuid carrier) : NullCreatureAI(creature), _carrier(carrier) { }

    void UpdateAI(uint32 diff) override
    {
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = FollowEveryMs;

        Unit* carrier = ObjectAccessor::GetUnit(*me, _carrier);
        if (!carrier)
            return;
        float const distance = me->GetExactDist(carrier);
        if (distance < MinStep)
            return;
        Movement::MoveSplineInit init(me);
        init.MoveTo(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ(), false);
        init.SetFacing(me->GetOrientation());
        init.SetOrientationFixed(true);
        init.SetVelocity(std::max(distance * 1000.0f / float(FollowEveryMs), 1.0f));
        init.Launch();
    }

private:
    static constexpr uint32 FollowEveryMs = 100;
    static constexpr float MinStep = 0.15f;

    ObjectGuid _carrier;
    uint32 _timer = FollowEveryMs;
};

Position OnGround(Unit* owner, float x, float y, float z)
{
    float ground = owner->GetMap()->GetHeight(owner->GetPhaseMask(), x, y, z + 6.0f, true, 30.0f);
    if (ground <= INVALID_HEIGHT)
        ground = z;
    return Position(x, y, ground);
}

void SpawnEmitter(Unit* owner, Position const& where, uint32 kit, uint32 intervalMs, uint32 durationMs,
                  float scale = 1.0f)
{
    TempSummon* emitter = owner->SummonCreature(NPC_GROUND_INDICATOR, where, TEMPSUMMON_TIMED_DESPAWN, durationMs);
    if (!emitter)
        return;

    // The kit's models are drawn at the emitter's scale: one big effect rather than many small ones
    emitter->SetObjectScale(scale);

    // Pulses spread over the interval, so an area shimmers rather than blinking all at once
    uint32 const first = 150 + (intervalMs ? urand(0, intervalMs) : 0);
    emitter->AIM_Initialize(new ParticleEmitterAI(emitter, kit, intervalMs, first));
}

// An emitter's place and its scale. A theme's effect covers about ParticleRadius yards at scale 1: every shape is
// covered by as few effects as it takes, each scaled to the part it covers - one for a circle, a row along a line, a
// few down a cone - rather than a carpet of small copies.
struct EmitterSpot
{
    Position where;
    float scale;
};

constexpr float ParticleRadius = 3.0f;
constexpr float MinParticleScale = 0.6f;
constexpr float MaxParticleScale = 6.0f;

std::vector<EmitterSpot> EmitterSpots(Unit* owner, GroundIndicators::Area const& area)
{
    using Kind = GroundIndicators::Area::Kind;
    std::vector<EmitterSpot> spots;
    Position const& origin = area.origin;
    float const z = origin.GetPositionZ();
    // One effect covering covered yards around (x, y)
    auto add = [&](float x, float y, float covered)
    {
        if (spots.size() < MaxEmitters)
            spots.push_back({ OnGround(owner, x, y, z),
                              std::clamp(covered / ParticleRadius, MinParticleScale, MaxParticleScale) });
    };

    switch (area.kind)
    {
        case Kind::Circle:
            add(origin.GetPositionX(), origin.GetPositionY(), area.radius);
            break;
        case Kind::Rectangle:
        {
            // A row along it, each effect as wide as the line, about as many as it is long in widths
            float const facing = origin.GetOrientation();
            float const half = std::max(area.width / 2.0f, 1.0f);
            uint32 const count = std::clamp<uint32>(uint32(area.radius / (half * 2.0f) + 0.5f), 1, MaxEmitters);
            float const step = area.radius / float(count);
            for (uint32 index = 0; index < count; ++index)
            {
                float const along = step * (float(index) + 0.5f);
                add(origin.GetPositionX() + std::cos(facing) * along, origin.GetPositionY() + std::sin(facing) * along,
                    std::max(half, step / 2.0f));
            }
            break;
        }
        case Kind::Ring:
        {
            // Round the band, each effect as thick as it
            float const band = std::max(area.radius - area.inner, 1.0f);
            float const middle = area.inner + band / 2.0f;
            uint32 const count = std::clamp<uint32>(uint32(2.0f * float(M_PI) * middle / band), 4, MaxEmitters);
            for (uint32 step = 0; step < count; ++step)
            {
                float const angle = 2.0f * float(M_PI) * step / count;
                add(origin.GetPositionX() + std::cos(angle) * middle, origin.GetPositionY() + std::sin(angle) * middle,
                    std::max(band / 2.0f, float(M_PI) * middle / float(count)));
            }
            break;
        }
        case Kind::Cone:
        {
            // Down its middle, each effect as wide as the cone where it stands
            float const facing = origin.GetOrientation();
            float const halfArc = std::min(area.arc / 2.0f, float(M_PI) / 2.0f);
            float along = area.radius * 0.25f;
            while (along < area.radius && spots.size() < MaxEmitters)
            {
                float const width = std::max(along * std::sin(halfArc), 1.5f);
                add(origin.GetPositionX() + std::cos(facing) * along, origin.GetPositionY() + std::sin(facing) * along,
                    width);
                along += std::max(width * 1.6f, 2.0f);
            }
            break;
        }
        case Kind::Cross:
        {
            // The middle and each arm's far half
            float const facing = origin.GetOrientation();
            add(origin.GetPositionX(), origin.GetPositionY(), area.width);
            for (uint32 arm = 0; arm < 4; ++arm)
            {
                float const angle = facing + arm * float(M_PI) / 2.0f;
                add(origin.GetPositionX() + std::cos(angle) * area.radius * 0.6f,
                    origin.GetPositionY() + std::sin(angle) * area.radius * 0.6f, area.width);
            }
            break;
        }
    }
    return spots;
}

// A look's fading twin (its alpha falling to nothing in FadeMs from when it is put on: buildGroundIndicators.py
// fade_spell, patchSinisterStrike.ps1 keep the same rule), 0 for none
constexpr uint32 FadeMs = GroundIndicators::FadingTwinMs;

uint32 FadeLookOf(uint32 look)
{
    if (look >= 90600 && look < 90900)
        return look + 6000;
    if (look >= 94000 && look < 94200)
        return look + 200;
    if (look >= 94200 && look < 94250)
        return look + 300;
    // Le Traqueur d'évadés's looks (EscapeHunter.cpp Looks)
    if (look >= 94860 && look < 94880)
        return look + 100;
    return 0;
}

// The owners whose marks show at their size at once (DrawInstantly): their stalkers are scaled before the client sees
// them. The others' grow into place from a yard - the client eases a scale it is told of after a unit shows.
std::mutex InstantLock;
// Owners drawing instantly, by guid and instance (a creature's guid is its map's: two instances' bosses share it)
std::set<std::pair<ObjectGuid, uint32>> InstantOwners;

bool DrawsInstantly(Unit* owner)
{
    std::lock_guard<std::mutex> guard(InstantLock);
    return InstantOwners.contains({ owner->GetGUID(), owner->GetInstanceId() });
}

// A stalker at its size before the client is told of it (Map::SummonCreature's steps, its scale set before AddToMap):
// it appears as it is, with no growth
TempSummon* SummonScaled(Unit* owner, Position const& placed, float scale, uint32 durationMs)
{
    Map* map = owner->GetMap();
    TempSummon* stalker = new TempSummon(nullptr, owner->GetGUID());
    if (!stalker->Create(map->GenerateLowGuid<HighGuid::Unit>(), map, owner->GetPhaseMask(), NPC_GROUND_INDICATOR, 0,
            placed.GetPositionX(), placed.GetPositionY(), placed.GetPositionZ(), placed.GetOrientation()))
    {
        delete stalker;
        return nullptr;
    }
    stalker->SetHomePosition(placed);
    stalker->InitStats(durationMs);
    stalker->SetTempSummonType(TEMPSUMMON_TIMED_DESPAWN);
    if (scale != 1.0f)
        stalker->SetObjectScale(scale);
    if (!map->AddToMap(stalker->ToCreature()))
    {
        delete stalker;
        return nullptr;
    }
    stalker->InitSummon();
    return stalker;
}

// The stalker that shows spellId at a size of scale yards; it goes away by itself after durationMs, its look turned
// to its fading twin FadeMs before (gone at once, a painting popped off the floor). A look changed meanwhile (a warning
// to its hit) fades the same way.
Creature* Place(Unit* owner, Position const& position, float orientation, uint32 spellId, float scale,
                uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return nullptr;

    Position placed(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(), orientation);
    bool const instant = DrawsInstantly(owner);
    TempSummon* stalker = instant ? SummonScaled(owner, placed, scale, durationMs) :
        owner->SummonCreature(NPC_GROUND_INDICATOR, placed, TEMPSUMMON_TIMED_DESPAWN, durationMs);
    if (!stalker)
        return nullptr;

    // A model built to its size (a line's piece) is never scaled: a unit's scale set after it shows grows into
    // place on the client, and the pieces of a line grew from their middles, the line cut up until they had
    if (!instant && scale != 1.0f)
        stalker->SetObjectScale(scale);
    stalker->AddAura(spellId, stalker);
    if (durationMs > FadeMs * 2)
        stalker->m_Events.AddEventAtOffset([stalker]()
        {
            uint32 look = 0;
            for (auto const& [auraId, application] : stalker->GetAppliedAuras())
                if (FadeLookOf(auraId))
                {
                    look = auraId;
                    break;
                }
            if (!look)
                return;
            stalker->RemoveAurasDueToSpell(look);
            stalker->AddAura(FadeLookOf(look), stalker);
        }, Milliseconds(durationMs - FadeMs));
    return stalker;
}

GroundIndicators::Area MakeArea(GroundIndicators::Area::Kind kind, Position const& origin, float orientation,
                                float radius)
{
    GroundIndicators::Area area;
    area.kind = kind;
    area.origin.Relocate(origin.GetPositionX(), origin.GetPositionY(), origin.GetPositionZ(), orientation);
    area.radius = radius;
    return area;
}

// The carried circle at the size nearest radius, on carrier for durationMs. Returns its spell, 0 if it could not.
uint32 Carry(Unit* carrier, float radius, uint32 durationMs, float& drawnRadius)
{
    // The client draws a unit's aura models at the unit's scale: a big boss carries a bigger circle than its model's
    float const scale = carrier && carrier->GetObjectScale() > 0.0f ? carrier->GetObjectScale() : 1.0f;
    ShapeSpell const& shape = NearestShape(CarriedSpells.data(), CarriedSpells.data() + CarriedSpells.size(),
        radius / scale);
    drawnRadius = shape.size * scale;
    if (!carrier || !carrier->IsInWorld() || !carrier->IsAlive() || durationMs == 0)
        return 0;

    Aura* aura = carrier->AddAura(shape.spell, carrier);
    if (!aura)
        return 0;

    aura->SetMaxDuration(int32(durationMs));
    aura->SetDuration(int32(durationMs));
    return shape.spell;
}

void Remember(Unit* caster, uint32 spellId, Drawn const& drawn)
{
    if (drawn.object.IsEmpty())
        return;

    caster->CustomData.GetDefault<CasterIndicators>(IndicatorDataKey)->placed.emplace_back(spellId, drawn);
}

void Forget(Unit* caster, uint32 spellId)
{
    CasterIndicators* indicators = caster->CustomData.Get<CasterIndicators>(IndicatorDataKey);
    if (!indicators)
        return;

    auto& placed = indicators->placed;
    for (auto itr = placed.begin(); itr != placed.end();)
    {
        if (itr->first != spellId)
        {
            ++itr;
            continue;
        }

        Drawn const& drawn = itr->second;
        if (drawn.carriedAura)
        {
            if (Unit* carrier = ObjectAccessor::GetUnit(*caster, drawn.object))
                carrier->RemoveAurasDueToSpell(drawn.carriedAura);
        }
        else if (Creature* stalker = caster->GetMap()->GetCreature(drawn.object))
            stalker->DespawnOrUnsummon();
        Unregister(drawn.areaId);
        itr = placed.erase(itr);
    }
}

// What a spell's harmful area covers: the effect reaching furthest, its shape and where it is centred
struct SpellArea
{
    bool cone = false;
    float radius = 0.0f;
    float arcDegrees = 0.0f;
    SpellImplicitTargetInfo const* reference = nullptr;     // the target type the area is placed from
    SpellImplicitTargetInfo const* area = nullptr;
};

bool IsHarmfulArea(SpellInfo const* spellInfo, uint8 effIndex, SpellImplicitTargetInfo const& target)
{
    SpellTargetSelectionCategories const category = target.GetSelectionCategory();
    if (category == TARGET_SELECT_CATEGORY_CONE || category == TARGET_SELECT_CATEGORY_AREA)
        return target.GetCheckType() == TARGET_CHECK_ENEMY;

    // A pool or a cloud left on the ground: its target is only where it goes
    return spellInfo->Effects[effIndex].Effect == SPELL_EFFECT_PERSISTENT_AREA_AURA &&
        !spellInfo->IsPositiveEffect(effIndex) && target.GetObjectType() == TARGET_OBJECT_TYPE_DEST;
}

bool FindArea(Spell const* spell, Unit* caster, SpellArea& area)
{
    SpellInfo const* spellInfo = spell->GetSpellInfo();
    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[effIndex];
        if (!effect.IsEffect())
            continue;

        for (SpellImplicitTargetInfo const* target : { &effect.TargetB, &effect.TargetA })
        {
            if (!target->GetTarget() || !IsHarmfulArea(spellInfo, effIndex, *target))
                continue;

            float const radius = effect.CalcRadius(caster, const_cast<Spell*>(spell));
            if (radius < MinRadius || radius > MaxRadius || radius <= area.radius)
                continue;

            area.radius = radius;
            area.area = target;
            area.reference = &effect.TargetA;
            area.cone = target->GetSelectionCategory() == TARGET_SELECT_CATEGORY_CONE;
            if (area.cone)
            {
                area.arcDegrees = 60.0f;
                if (SpellCone const* cone = sSpellMgr->GetSpellCone(spellInfo->Id))
                    area.arcDegrees = cone->cone_degrees;
                else if (target->GetTarget() == TARGET_UNIT_CONE_ENEMY_24)
                    area.arcDegrees = 24.0f;
                else if (target->GetTarget() == TARGET_UNIT_CONE_ENEMY_54)
                    area.arcDegrees = 54.0f;
                else if (target->GetTarget() == TARGET_UNIT_CONE_ENEMY_104)
                    area.arcDegrees = 104.0f;
            }
            break;
        }
    }
    return area.area != nullptr;
}

// One hit's damage of a spell to a player, before their defences, as the key scales it: its direct damage, a
// weapon strike's, one tick of what it leaves (a pool's, an aura's pulse). 0 when it cannot be told (a share of the
// target's health, a scripted effect): an area of unknown damage is always to be left.
uint32 EstimateHit(Unit* caster, SpellInfo const* spellInfo, uint8 depth = 0)
{
    if (!caster || !spellInfo || depth > 2)
        return 0;

    float const factor = std::max(GetMythicSpellFactor(caster, spellInfo), 0.01f);
    float spellDamage = 0.0f;
    float weaponDamage = 0.0f;
    float const weapon = (caster->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE) +
        caster->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE)) / 2.0f;
    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[effIndex];
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
                spellDamage += float(std::max(effect.CalcValue(caster), 0));
                break;
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                weaponDamage += weapon + float(std::max(effect.CalcValue(caster), 0));
                break;
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                weaponDamage += weapon * float(std::max(effect.CalcValue(caster), 0)) / 100.0f;
                break;
            case SPELL_EFFECT_TRIGGER_SPELL:
                // Already scaled by its own spell's factor
                spellDamage += float(EstimateHit(caster, sSpellMgr->GetSpellInfo(effect.TriggerSpell), depth + 1)) /
                    factor;
                break;
            case SPELL_EFFECT_APPLY_AURA:
            case SPELL_EFFECT_PERSISTENT_AREA_AURA:
            case SPELL_EFFECT_APPLY_AREA_AURA_ENEMY:
                if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE ||
                    effect.ApplyAuraName == SPELL_AURA_PERIODIC_LEECH)
                    spellDamage += float(std::max(effect.CalcValue(caster), 0));
                else if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL ||
                    effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE)
                    spellDamage += float(EstimateHit(caster, sSpellMgr->GetSpellInfo(effect.TriggerSpell),
                        depth + 1)) / factor;
                else if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                    return 0;
                break;
            default:
                break;
        }
    }

    // The key's scaling for spells; a creature's weapon is scaled with the creature itself
    float const total = spellDamage * factor + weaponDamage;
    return total >= 1.0f ? uint32(total) : 0;
}

// Shows the area of a cast that starts now and lasts durationMs. A circle around the target follows it: the spell
// lands where the target is when it goes off.
void ShowSpellArea(Spell const* spell, Unit* caster, uint32 durationMs)
{
    SpellArea area;
    if (!FindArea(spell, caster, area))
        return;

    SpellInfo const* spellInfo = spell->GetSpellInfo();
    Unit* target = spell->m_targets.GetUnitTarget();
    uint32 const hit = EstimateHit(caster, spellInfo);

    if (area.cone)
    {
        float const facing = target && target != caster ? caster->GetAngle(target) : caster->GetOrientation();
        ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(),
            area.arcDegrees);
        Creature* stalker = Place(caster, *caster, facing, shape.spell, area.radius, durationMs);
        GroundIndicators::Area cone = MakeArea(GroundIndicators::Area::Kind::Cone, *caster, facing, area.radius);
        cone.arc = shape.size * float(M_PI) / 180.0f;
        if (stalker)
        {
            Remember(caster, spellInfo->Id, { stalker->GetGUID(), 0, Register(caster, nullptr, cone,
                durationMs, hit) });
            GroundIndicators::ShowParticles(caster, cone, GroundIndicators::ThemeOf(spellInfo->GetSchoolMask()),
                durationMs);
        }
        return;
    }

    // Where the area is centred: on the caster, on its target, or on a spot it picked
    SpellTargetReferenceTypes reference = area.area->GetReferenceType();
    if (reference == TARGET_REFERENCE_TYPE_SRC || reference == TARGET_REFERENCE_TYPE_DEST)
        reference = area.reference->GetReferenceType();

    Unit* followed = nullptr;
    Position center = caster->GetPosition();
    if (reference == TARGET_REFERENCE_TYPE_TARGET && target)
    {
        followed = target;
        center = target->GetPosition();
    }
    else if (reference == TARGET_REFERENCE_TYPE_DEST && spell->m_targets.HasDst())
        center = spell->m_targets.GetDstPos()->GetPosition();

    if (followed)
    {
        float drawnRadius = area.radius;
        if (uint32 const aura = Carry(followed, area.radius, durationMs, drawnRadius))
            Remember(caster, spellInfo->Id, { followed->GetGUID(), aura, Register(caster, followed,
                MakeArea(GroundIndicators::Area::Kind::Circle, center, 0.0f, drawnRadius), durationMs, hit) });
        return;
    }

    if (Creature* stalker = Place(caster, center, 0.0f, SPELL_INDICATOR_CIRCLE, area.radius, durationMs))
    {
        GroundIndicators::Area const circle = MakeArea(GroundIndicators::Area::Kind::Circle, center, 0.0f,
            area.radius);
        Remember(caster, spellInfo->Id, { stalker->GetGUID(), 0, Register(caster, nullptr, circle, durationMs,
            hit) });
        GroundIndicators::ShowParticles(caster, circle, GroundIndicators::ThemeOf(spellInfo->GetSchoolMask()),
            durationMs);
    }
}

// Raids whose creatures' abilities are read too, as in a mythic dungeon (a raid's scripted mechanics, not cast
// from a spell, still need drawing by hand): Icecrown Citadel
constexpr std::array<uint32, 1> IndicatorRaids = { 631 };

bool InIndicatorMap(WorldObject const* object)
{
    Map const* map = object ? object->FindMap() : nullptr;
    return map && (map->IsMythic() || std::ranges::find(IndicatorRaids, map->GetId()) != IndicatorRaids.end());
}

bool IsPlayerControlled(Creature const* creature)
{
    return creature->IsPet() || creature->IsTotem() || creature->IsGuardian() ||
        creature->GetCharmerOrOwnerGUID().IsPlayer();
}

// An enemy of the players whose abilities are shown: a creature of their foes, in a map that shows them
bool ShouldShow(Unit* caster)
{
    Creature* creature = caster ? caster->ToCreature() : nullptr;
    return creature && creature->IsAlive() && creature->IsHostileToPlayers() && !IsPlayerControlled(creature) &&
        InIndicatorMap(creature);
}

// How far around its caster a spell hurts its enemies: its biggest harmful area effect centred on the caster, 0 if
// it has none (a spell aimed at a spot or a target elsewhere is no area around it)
float HarmfulRadiusAround(SpellInfo const* spellInfo, Unit* caster)
{
    float radius = 0.0f;
    if (!spellInfo || spellInfo->IsPositive())
        return radius;

    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[effIndex];
        if (!effect.IsEffect())
            continue;

        for (SpellImplicitTargetInfo const* target : { &effect.TargetA, &effect.TargetB })
        {
            if (target->GetSelectionCategory() != TARGET_SELECT_CATEGORY_AREA ||
                target->GetCheckType() != TARGET_CHECK_ENEMY)
                continue;

            SpellTargetReferenceTypes const reference = target->GetReferenceType();
            bool const aroundCaster = reference == TARGET_REFERENCE_TYPE_CASTER ||
                ((reference == TARGET_REFERENCE_TYPE_SRC || reference == TARGET_REFERENCE_TYPE_DEST) &&
                 effect.TargetA.GetReferenceType() == TARGET_REFERENCE_TYPE_CASTER);
            if (aroundCaster)
                radius = std::max(radius, effect.CalcRadius(caster));
        }

        // An aura laid on every enemy near its owner
        if (effect.Effect == SPELL_EFFECT_APPLY_AREA_AURA_ENEMY)
            radius = std::max(radius, effect.CalcRadius(caster));
    }
    return radius;
}

// In a mythic dungeon an instant ability that hurts everyone in an area gets this long a cast, so it can be drawn
// before it lands (the retail Mythic+ way): without it, about half of the pool's dangerous abilities - Whirlwind, War
// Stomp, Frost Nova, the breaths - came with no warning at all
constexpr int32 MythicWindUpMs = 1000;

// Whether a spell does something worth stepping out of: damage, a knockback, a loss of control, damage over time.
// A shout that only lowers attack power is not.
bool IsDangerous(SpellInfo const* spellInfo)
{
    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[effIndex];
        switch (effect.Effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            case SPELL_EFFECT_HEALTH_LEECH:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
            case SPELL_EFFECT_KNOCK_BACK:
            case SPELL_EFFECT_KNOCK_BACK_DEST:
                return true;
            default:
                break;
        }

        switch (effect.ApplyAuraName)
        {
            case SPELL_AURA_PERIODIC_DAMAGE:
            case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
            case SPELL_AURA_MOD_CONFUSE:
            case SPELL_AURA_MOD_FEAR:
            case SPELL_AURA_MOD_STUN:
            case SPELL_AURA_MOD_ROOT:
            case SPELL_AURA_MOD_SILENCE:
                return true;
            default:
                break;
        }
    }
    return false;
}

class GroundIndicatorSpellScript : public AllSpellScript
{
public:
    GroundIndicatorSpellScript() : AllSpellScript("GroundIndicatorSpellScript",
        { ALLSPELLHOOK_ON_PREPARE, ALLSPELLHOOK_ON_CAST, ALLSPELLHOOK_ON_CAST_CANCEL, ALLSPELLHOOK_ON_CAST_TIME }) { }

    void OnSpellCastTime(Spell* spell, Unit* caster, SpellInfo const* spellInfo, int32& castTime) override
    {
        if (castTime > 0 || spellInfo->IsChanneled() || spell->IsTriggered() || !ShouldShow(caster) ||
            !caster->GetMap()->IsMythic() || !caster->IsInCombat() || !IsDangerous(spellInfo))
            return;

        SpellArea area;
        if (FindArea(spell, caster, area))
            castTime = MythicWindUpMs;
    }

    void OnSpellPrepare(Spell* spell, Unit* caster, SpellInfo const* /*spellInfo*/) override
    {
        if (!ShouldShow(caster) || spell->GetCastTime() <= 0)
            return;

        ShowSpellArea(spell, caster, spell->GetCastTime() + CastGraceMs);
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!caster || !caster->IsCreature())
            return;

        // The cast went off: its area is where the damage is now, not where it will be
        Forget(caster, spellInfo->Id);

        if (spellInfo->IsChanneled() && ShouldShow(caster))
        {
            int32 const duration = spellInfo->GetDuration();
            if (duration > 0)
                ShowSpellArea(spell, caster, std::min<uint32>(duration, MaxChannelMs));
        }
    }

    void OnSpellCastCancel(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*bySelf*/) override
    {
        if (caster && caster->IsCreature())
            Forget(caster, spellInfo->Id);
    }
};

// A candidate spot for FindEscape, and how good it is (lower is better)
struct Candidate
{
    Position spot;
    float cost = 0.0f;
};

// Whether a tank stands its ground in this area: a circle around a trash creature that is attacking it. See
// FindEscape in GroundIndicators.h.
bool HeldByTank(Unit* tank, ActiveArea const& entry)
{
    // Laid at its feet, or carried by the creature itself (a damaging aura around it)
    if (entry.area.kind != GroundIndicators::Area::Kind::Circle ||
        (!entry.carrier.IsEmpty() && entry.carrier != entry.owner))
        return false;

    Creature* owner = ObjectAccessor::GetCreature(*tank, entry.owner);
    if (!owner || !owner->IsAlive() || owner->GetVictim() != tank || owner->IsDungeonBoss() || owner->isWorldBoss() ||
        owner->GetCreatureTemplate()->rank == CREATURE_ELITE_WORLDBOSS)
        return false;

    // Around the creature itself, not a spot it aimed at elsewhere
    return entry.area.Contains(owner->GetPosition(), 0.0f);
}

// An area a unit may stand in: its hit is known, and leaves the unit above SurvivableMarginPct of its maximum health.
// Not once Imprudence has piled up (MythicTuning::OnHitTaken): every hit taken there adds to it. An area it carries
// near others is never one of these: the others are the ones to spare.
constexpr float SurvivableMarginPct = 30.0f;
constexpr uint32 SPELL_MYTHIC_IMPRUDENCE = 90672;
constexpr uint8 ImprudenceDodgeStacks = 3;

bool SurvivableHit(Unit* unit, ActiveArea const& entry)
{
    if (!entry.hitDamage || entry.carrier == unit->GetGUID())
        return false;
    if (Aura const* imprudence = unit->GetAura(SPELL_MYTHIC_IMPRUDENCE))
        if (imprudence->GetStackAmount() >= ImprudenceDodgeStacks)
            return false;
    float const margin = unit->GetMaxHealth() * SurvivableMarginPct / 100.0f;
    return float(entry.hitDamage) + margin < float(unit->GetHealth());
}

bool InAnyArea(std::vector<ActiveArea> const& areas, Position const& point, ObjectGuid ignoredCarrier, float margin)
{
    for (ActiveArea const& entry : areas)
        if (entry.carrier != ignoredCarrier || entry.carrier.IsEmpty())
            if (entry.area.Contains(point, margin))
                return true;
    return false;
}

// Whether a straight walk from `from` to `to` crosses an area: one it does not stand in at `from` (those it is
// walking out of), a circle it carries itself aside
bool PathCrosses(std::vector<ActiveArea> const& areas, Position const& from, Position const& to, ObjectGuid unit)
{
    float const length = from.GetExactDist2d(&to);
    uint32 const steps = std::max<uint32>(1, uint32(length / PathSampleStep));
    for (ActiveArea const& entry : areas)
    {
        if ((!entry.carrier.IsEmpty() && entry.carrier == unit) || entry.area.Contains(from, InsideMargin))
            continue;
        for (uint32 step = 1; step <= steps; ++step)
        {
            float const t = float(step) / float(steps);
            Position const at(from.GetPositionX() + (to.GetPositionX() - from.GetPositionX()) * t,
                from.GetPositionY() + (to.GetPositionY() - from.GetPositionY()) * t, from.GetPositionZ());
            if (entry.area.Contains(at, 0.0f))
                return true;
        }
    }
    return false;
}

// A number of its own for each unit, the same every time: 0 to 1
float UnitSpread(Unit* unit, uint32 salt)
{
    uint64 value = unit->GetGUID().GetRawValue() * 0x9E3779B97F4A7C15ULL + salt * 0xBF58476D1CE4E5B9ULL;
    value ^= value >> 31;
    value *= 0x94D049BB133111EBULL;
    value ^= value >> 29;
    return float(value % 10000) / 10000.0f;
}

// How many other players stand within reach yards of point
uint32 PlayersNear(Unit* unit, Position const& point, float reach)
{
    uint32 count = 0;
    for (auto const& ref : unit->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (player && player != unit && player->IsAlive() && player->GetExactDist2d(&point) <= reach)
            ++count;
    }
    return count;
}

// Whether another player stands within reach yards of point
bool OtherPlayerNear(Unit* unit, Position const& point, float reach)
{
    for (auto const& ref : unit->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (player && player != unit && player->IsAlive() && player->GetExactDist2d(&point) <= reach)
            return true;
    }
    return false;
}

// Where a fight wants players to stand (ShowSoak, SetOffTankSpot), per instance, until it ends. Kept apart from the
// red areas: nothing here is to be left.
struct Goal
{
    enum class Kind : uint8
    {
        Soak,
        OffTank,
        Tank,                                   // the spot of the tank the owner is hitting (SetTankSpot)
        Holder,                                 // the tank to hold the owner (SetBossHolder)
        Unit,                                   // a spot for one unit (SetUnitSpot), named in `tank`
        GroupHit,                               // a hit on the whole group landing at endMs (WarnGroupDamage)
        Still,                                  // one unit not to move until endMs (SetHoldStill), named in `tank`
        LookAway                                // everyone's back to the owner, standing at center (SetLookAway)
    };

    Kind kind = Kind::Soak;
    uint32 mapId = 0;
    uint32 instanceId = 0;
    ObjectGuid owner;
    Position center;
    float radius = 0.0f;
    uint32 wanted = 0;
    uint64 endMs = 0;
    bool tanks = false;                         // a soak the tanks go to as well (nothing to hold meanwhile)
    bool hold = false;                          // an off-tank spot to stand on until it lands
    ObjectGuid tank;                            // an off-tank spot for this tank only (SetOffTankSpot)
    std::vector<ObjectGuid> assigned;           // a soak's bots (AssignSoaks): each bot goes to one soak only
};

std::mutex GoalLock;
std::vector<Goal> Goals;

// A unit on its own spot holds there this far from its middle past its radius (HoldsUnitSpot): it stopped as it came
// in, and the fight's own pull (a melee's chase) took it back out, in and out every tick
constexpr float UnitSpotHoldSlack = 0.75f;
// A gaze turns the backs of everyone this close to the one gazing (SetLookAway)
constexpr float LookAwayReach = 100.0f;
// A soak is looked at by the players this close to it; a player this far inside its edge stands in it
constexpr float SoakReach = 50.0f;
constexpr float SoakInsideMargin = 1.0f;
// The off-tank goes back to its spot once this far from it; to one it must hold, this close
constexpr float OffTankSlack = 5.0f;
constexpr float OffTankHoldSlack = 1.5f;

void AddGoal(Goal goal, bool replaceOwnersOfKind)
{
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [&goal, now, replaceOwnersOfKind](Goal const& entry)
        {
            return entry.endMs <= now ||
                (replaceOwnersOfKind && entry.owner == goal.owner && entry.kind == goal.kind &&
                 entry.mapId == goal.mapId && entry.instanceId == goal.instanceId);
        }), Goals.end());
    Goals.push_back(goal);
}

// The bots of an instance shared out between the soaks shown there, under GoalLock. Each bot goes to one soak, the
// nearest free one first, and keeps it until it lands: deciding alone, a bot nearest to two towers counted for both
// and walked to one, and the other was left short. Real players standing in a soak count for it; they are never sent.
// A soak without `tanks` takes neither the tanks nor whoever its owner is hitting.
void AssignSoaks(Unit* unit)
{
    uint64 const now = NowMs();
    std::vector<Goal*> soaks;
    for (Goal& goal : Goals)
        if (goal.kind == Goal::Kind::Soak && goal.endMs > now && goal.mapId == unit->GetMapId() &&
            goal.instanceId == unit->GetInstanceId())
            soaks.push_back(&goal);
    if (soaks.empty())
        return;

    std::vector<Player*> bots;
    std::vector<Player*> players;
    for (auto const& ref : unit->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsAlive() || player->IsGameMaster() || !player->IsInWorld())
            continue;
        (player->GetSession() && player->GetSession()->IsBot() ? bots : players).push_back(player);
    }
    auto isBot = [&bots](ObjectGuid guid)
    {
        return std::ranges::any_of(bots, [guid](Player* bot) { return bot->GetGUID() == guid; });
    };

    std::set<ObjectGuid> taken;
    for (Goal* soak : soaks)
    {
        std::erase_if(soak->assigned, [&isBot, &taken](ObjectGuid guid)
            { return !isBot(guid) || taken.contains(guid); });
        taken.insert(soak->assigned.begin(), soak->assigned.end());
    }

    for (Goal* soak : soaks)
    {
        Unit* owner = ObjectAccessor::GetUnit(*unit, soak->owner);
        Unit* victim = owner ? owner->GetVictim() : nullptr;
        float const inside = std::max(soak->radius - SoakInsideMargin, 0.5f);
        uint32 helping = 0;
        for (Player* player : players)
            if (player->GetExactDist2d(&soak->center) <= inside)
                ++helping;
        std::vector<std::pair<float, Player*>> candidates;
        for (Player* bot : bots)
        {
            if (taken.contains(bot->GetGUID()) || bot->GetExactDist2d(&soak->center) > SoakReach)
                continue;
            if (!soak->tanks && (bot == victim || IsGroupTank(bot)))
                continue;
            // A bot carrying a circle away from the others (a quarry, a bomb) never comes to share a hit
            if (GroundIndicators::KeepsAway(bot))
                continue;
            candidates.emplace_back(bot->GetExactDist2d(&soak->center), bot);
        }
        std::sort(candidates.begin(), candidates.end(), [](auto const& left, auto const& right)
            { return left.first < right.first; });
        for (auto const& [distance, bot] : candidates)
        {
            if (soak->assigned.size() + helping >= soak->wanted)
                break;
            soak->assigned.push_back(bot->GetGUID());
            taken.insert(bot->GetGUID());
        }
    }
}

// The soak unit is sent to, if any (a copy, taken under GoalLock)
bool AssignedSoak(Unit* unit, Goal& found)
{
    if (!unit->IsPlayer() || !unit->FindMap())
        return false;
    std::lock_guard<std::mutex> guard(GoalLock);
    AssignSoaks(unit);
    uint64 const now = NowMs();
    for (Goal const& goal : Goals)
        if (goal.kind == Goal::Kind::Soak && goal.endMs > now && goal.instanceId == unit->GetInstanceId() &&
            goal.mapId == unit->GetMapId() && std::ranges::find(goal.assigned, unit->GetGUID()) != goal.assigned.end())
        {
            found = goal;
            return true;
        }
    return false;
}

std::vector<Goal> GoalsAround(Unit* unit)
{
    std::vector<Goal> found;
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(GoalLock);
    for (Goal const& entry : Goals)
        if (entry.endMs > now && entry.mapId == unit->GetMapId() && entry.instanceId == unit->GetInstanceId())
            found.push_back(entry);
    return found;
}
}

namespace GroundIndicators
{
bool Area::Contains(Position const& point, float margin) const
{
    float const dx = point.GetPositionX() - origin.GetPositionX();
    float const dy = point.GetPositionY() - origin.GetPositionY();
    float const distance = std::sqrt(dx * dx + dy * dy);
    switch (kind)
    {
        case Kind::Circle:
            return distance <= radius + margin;
        case Kind::Rectangle:
        {
            float const facing = origin.GetOrientation();
            float const forward = dx * std::cos(facing) + dy * std::sin(facing);
            float const across = -dx * std::sin(facing) + dy * std::cos(facing);
            return forward >= -margin && forward <= radius + margin && std::fabs(across) <= width / 2.0f + margin;
        }
        case Kind::Cone:
        {
            if (distance > radius + margin || (inner > 0.0f && distance < inner - margin))
                return false;
            if (distance <= margin)
                return true;
            float angle = std::atan2(dy, dx) - origin.GetOrientation();
            angle = std::remainder(angle, 2.0f * float(M_PI));
            return std::fabs(angle) <= arc / 2.0f + std::asin(std::min(1.0f, margin / distance));
        }
        case Kind::Ring:
            return distance <= radius + margin && distance >= inner - margin;
        case Kind::Cross:
        {
            float const facing = origin.GetOrientation();
            float const forward = std::fabs(dx * std::cos(facing) + dy * std::sin(facing));
            float const across = std::fabs(-dx * std::sin(facing) + dy * std::cos(facing));
            float const half = width / 2.0f + margin;
            return (forward <= radius + margin && across <= half) || (across <= radius + margin && forward <= half);
        }
    }
    return false;
}

Theme ThemeOf(uint32 schoolMask)
{
    if (schoolMask & SPELL_SCHOOL_MASK_SHADOW)
        return Theme::Shadow;
    if (schoolMask & SPELL_SCHOOL_MASK_FIRE)
        return Theme::Fire;
    if (schoolMask & SPELL_SCHOOL_MASK_FROST)
        return Theme::Frost;
    if (schoolMask & SPELL_SCHOOL_MASK_NATURE)
        return Theme::Nature;
    if (schoolMask & SPELL_SCHOOL_MASK_ARCANE)
        return Theme::Arcane;
    if (schoolMask & SPELL_SCHOOL_MASK_HOLY)
        return Theme::Holy;
    return Theme::None;
}

void ShowParticles(Unit* owner, Area const& area, Theme theme, uint32 durationMs)
{
    uint32 const kit = KitsOf(theme).warning;
    if (!kit || !owner || !owner->IsInWorld() || durationMs == 0)
        return;

    for (EmitterSpot const& spot : EmitterSpots(owner, area))
        SpawnEmitter(owner, spot.where, kit, ParticlePulseMs, durationMs, spot.scale);
}

void Burst(Unit* owner, Position const& where, Theme theme)
{
    uint32 const kit = KitsOf(theme).burst;
    if (!kit || !owner || !owner->IsInWorld())
        return;

    SpawnEmitter(owner, OnGround(owner, where.GetPositionX(), where.GetPositionY(), where.GetPositionZ()), kit, 0,
                 3000);
}

void PlayKit(Unit* owner, Position const& where, uint32 kit, float scale, uint32 intervalMs, uint32 durationMs)
{
    if (!kit || !owner || !owner->IsInWorld())
        return;

    SpawnEmitter(owner, OnGround(owner, where.GetPositionX(), where.GetPositionY(), where.GetPositionZ()), kit,
                 intervalMs, durationMs, scale);
}

Area ShowCircle(Unit* owner, Position const& center, float radius, uint32 durationMs, Theme theme,
                uint32 hitDamage)
{
    Area area = MakeArea(Area::Kind::Circle, center, 0.0f, radius);
    if (Creature* stalker = Place(owner, center, 0.0f, SPELL_INDICATOR_CIRCLE, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowRectangle(Unit* owner, Position const& start, float orientation, float length, float width,
                   uint32 durationMs, Theme theme, uint32 hitDamage)
{
    width = std::max(width, 0.5f);
    ShapeSpell const& shape = NearestShape(RectangleSpells.data(), RectangleSpells.data() + RectangleSpells.size(),
        length / width);
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = length / shape.size;
    if (Creature* stalker = Place(owner, start, orientation, shape.spell, length, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

// A sweeping line in the registry: the bots read it at its angle of the moment
void RegisterSweep(Unit* owner, Area const& area, float radiansPerSecond, uint32 durationMs, uint32 hitDamage)
{
    uint64 const id = Register(owner, nullptr, area, durationMs, hitDamage);
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.id == id)
        {
            entry.sweepSpeed = radiansPerSecond;
            entry.sweepFromMs = NowMs();
            entry.sweepFrom = area.origin.GetOrientation();
        }
}

Area ShowSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond, float length,
                           float width, uint32 durationMs, uint32 hitDamage, uint32 look, uint32 curtain)
{
    width = std::max(width, 0.5f);
    ShapeSpell const& shape = NearestShape(RectangleSpells.data(), RectangleSpells.data() + RectangleSpells.size(),
        length / width);
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = look ? width : length / shape.size;
    if (Creature* stalker = Place(owner, start, orientation, look ? look : shape.spell, length, durationMs))
    {
        stalker->AIM_Initialize(new SweepAI(stalker, orientation, radiansPerSecond));
        RegisterSweep(owner, area, radiansPerSecond, durationMs, hitDamage);
        // Its curtain at its own size (a scaled carrier would scale its height too), turning the same way
        if (curtain)
            if (Creature* light = Place(owner, OnGround(owner, start.GetPositionX(), start.GetPositionY(),
                start.GetPositionZ()), orientation, curtain, 1.0f, durationMs))
                light->AIM_Initialize(new SweepAI(light, orientation, radiansPerSecond));
    }
    return area;
}

Area ShowSweepingLine(Unit* owner, Position const& start, float orientation, float radiansPerSecond, float length,
                      float width, uint32 durationMs, uint32 look, uint32 hitDamage)
{
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = std::max(width, 0.5f);
    if (Creature* stalker = Place(owner, start, orientation, look, 1.0f, durationMs))
    {
        stalker->AIM_Initialize(new SweepAI(stalker, orientation, radiansPerSecond));
        RegisterSweep(owner, area, radiansPerSecond, durationMs, hitDamage);
    }
    return area;
}

void WatchSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond,
                            float length, float width, uint32 durationMs, uint32 hitDamage)
{
    if (!owner || !owner->IsInWorld() || !durationMs)
        return;
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = std::max(width, 0.5f);
    RegisterSweep(owner, area, radiansPerSecond, durationMs, hitDamage);
}

Area ShowPainted(Unit* owner, Area const& area, uint32 look, uint32 durationMs, Theme theme, uint32 hitDamage,
                 ObjectGuid* placed, uint32 lingerMs)
{
    if (Creature* stalker = Place(owner, area.origin, area.origin.GetOrientation(), look, area.radius,
        durationMs + lingerMs))
    {
        if (placed)
            *placed = stalker->GetGUID();
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowPaintedLine(Unit* owner, Area const& area, PaintedLine const& look, uint32 durationMs, Theme theme,
                     uint32 hitDamage, uint32 lingerMs, Position const* clipCenter, float clipRadius)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0 || look.count == 0)
        return area;

    float const facing = area.origin.GetOrientation();
    float const dx = std::cos(facing);
    float const dy = std::sin(facing);
    // The part of the line drawn: [from, to] along it, within the clip circle
    float from = 0.0f;
    float to = area.radius;
    bool clippedFrom = false;
    bool clippedTo = false;
    if (clipCenter)
    {
        float const ox = area.origin.GetPositionX() - clipCenter->GetPositionX();
        float const oy = area.origin.GetPositionY() - clipCenter->GetPositionY();
        float const b = ox * dx + oy * dy;
        float const disc = b * b - (ox * ox + oy * oy - clipRadius * clipRadius);
        if (disc > 0.0f)
        {
            float const root = std::sqrt(disc);
            clippedFrom = -b - root > from;
            clippedTo = -b + root < to;
            from = std::max(from, -b - root);
            to = std::min(to, -b + root);
        }
        else
            to = from;
    }

    if (to > from)
    {
        // Whole pieces: they run past a clipped end (into a wall), never past one of the line's own
        float const piece = area.width * look.ratio;
        uint32 const pieces = uint32(std::ceil((to - from) / piece - 0.01f));
        float const over = float(pieces) * piece - (to - from);
        float start = from - over / 2.0f;
        if (clippedFrom != clippedTo)
            start = clippedTo ? from : to - float(pieces) * piece;
        for (uint32 index = 0; index < pieces; ++index)
        {
            float const along = start + (float(index) + 0.5f) * piece;
            Position const at(area.origin.GetPositionX() + dx * along, area.origin.GetPositionY() + dy * along,
                              area.origin.GetPositionZ());
            Place(owner, at, facing, look.firstSpell + index % look.count, look.builtToSize ? 1.0f : piece,
                  durationMs + lingerMs);
            // Its light standing up from the floor, on a carrier of its own (never scaled: built to the piece)
            if (look.curtain)
                Place(owner, OnGround(owner, at.GetPositionX(), at.GetPositionY(), at.GetPositionZ()), facing,
                      look.curtain + index % look.count, 1.0f, durationMs + lingerMs);
        }
    }
    Register(owner, nullptr, area, durationMs, hitDamage);
    ShowParticles(owner, area, theme, durationMs);
    return area;
}

Area ShowSwingingLine(Unit* owner, Area const& area, uint32 look, float turn, uint32 startMs, uint32 swingMs,
                      uint32 durationMs, uint32 hitDamage, uint32 lingerMs, ObjectGuid* placed)
{
    float const from = area.origin.GetOrientation();
    Area stop = area;
    stop.origin.SetOrientation(Position::NormalizeOrientation(from + turn));
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return stop;
    if (Creature* stalker = Place(owner, area.origin, from, look, 1.0f, durationMs + lingerMs))
    {
        stalker->AIM_Initialize(new SwingAI(stalker, from, turn, startMs, swingMs));
        if (placed)
            *placed = stalker->GetGUID();
    }
    Register(owner, nullptr, stop, durationMs, hitDamage);
    return stop;
}

void RepaintLine(Unit* owner, Area const& area, PaintedLine const& from, PaintedLine const& to)
{
    if (!owner || !owner->IsInWorld())
        return;
    float const facing = area.origin.GetOrientation();
    std::list<Creature*> stalkers;
    owner->GetCreatureListWithEntryInGrid(stalkers, NPC_GROUND_INDICATOR, area.radius + 60.0f);
    for (Creature* stalker : stalkers)
    {
        // On this line: on its axis, facing its way (two lines of a cross share their middle)
        float const along = (stalker->GetPositionX() - area.origin.GetPositionX()) * std::cos(facing) +
                            (stalker->GetPositionY() - area.origin.GetPositionY()) * std::sin(facing);
        float const aside = (stalker->GetPositionY() - area.origin.GetPositionY()) * std::cos(facing) -
                            (stalker->GetPositionX() - area.origin.GetPositionX()) * std::sin(facing);
        if (std::fabs(aside) > 0.3f || along < -area.width * from.ratio || along > area.radius + area.width * from.ratio ||
            std::fabs(Position::NormalizeOrientation(stalker->GetOrientation() - facing + float(M_PI)) - float(M_PI)) > 0.02f)
            continue;
        for (uint32 index = 0; index < from.count; ++index)
        {
            if (stalker->HasAura(from.firstSpell + index))
            {
                stalker->RemoveAurasDueToSpell(from.firstSpell + index);
                stalker->AddAura(to.firstSpell + index, stalker);
                break;
            }
            if (from.curtain && to.curtain && stalker->HasAura(from.curtain + index))
            {
                stalker->RemoveAurasDueToSpell(from.curtain + index);
                stalker->AddAura(to.curtain + index, stalker);
                break;
            }
        }
    }
}

Area CurrentSweep(Area const& area, float radiansPerSecond, uint32 elapsedMs)
{
    Area current = area;
    current.origin.SetOrientation(Position::NormalizeOrientation(area.origin.GetOrientation() +
        radiansPerSecond * float(elapsedMs) / 1000.0f));
    return current;
}

Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
              uint32 durationMs, Theme theme, uint32 hitDamage, float clearMiddle)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    Area area = MakeArea(Area::Kind::Cone, apex, orientation, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    area.inner = clearMiddle;
    if (Creature* stalker = Place(owner, apex, orientation, shape.spell, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowAimedCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                   uint32 durationMs, Unit* aimedAt, Theme theme, uint32 hitDamage, float clearMiddle)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    Area area = MakeArea(Area::Kind::Cone, apex, orientation, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    area.inner = clearMiddle;
    if (Creature* stalker = Place(owner, apex, orientation, shape.spell, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage, aimedAt ? aimedAt->GetGUID() : ObjectGuid::Empty);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowTrackingCone(Unit* owner, Position const& apex, float radius, float arcDegrees, uint32 durationMs,
                      Unit* aimedAt, uint32 hitDamage, bool away)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    float const offset = away ? float(M_PI) : 0.0f;
    float const facing = Position::NormalizeOrientation(apex.GetAngle(aimedAt->GetPositionX(),
        aimedAt->GetPositionY()) + offset);
    Area area = MakeArea(Area::Kind::Cone, apex, facing, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    if (Creature* stalker = Place(owner, apex, facing, shape.spell, radius, durationMs))
    {
        stalker->AIM_Initialize(new TrackingConeAI(stalker, aimedAt->GetGUID(), offset));
        Register(owner, nullptr, area, durationMs, hitDamage, aimedAt->GetGUID(), aimedAt->GetGUID(), offset);
    }
    return area;
}

Area CurrentCone(Unit* aimedAt, Area const& area)
{
    Area current = area;
    if (aimedAt)
        current.origin.SetOrientation(current.origin.GetAngle(aimedAt->GetPositionX(), aimedAt->GetPositionY()));
    return current;
}

Area ShowRing(Unit* owner, Position const& center, float outerRadius, float innerRadius, uint32 durationMs,
              Theme theme, uint32 hitDamage)
{
    outerRadius = std::max(outerRadius, 1.0f);
    ShapeSpell const& shape = NearestShape(RingSpells.data(), RingSpells.data() + RingSpells.size(),
        std::clamp(innerRadius / outerRadius, 0.05f, 0.95f));
    Area area = MakeArea(Area::Kind::Ring, center, 0.0f, outerRadius);
    area.inner = outerRadius * shape.size;
    if (Creature* stalker = Place(owner, center, 0.0f, shape.spell, outerRadius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

void ClearAreasOf(Unit* owner)
{
    if (!owner)
        return;

    // Its own instance's only: a creature's guid is its map's, the same boss in two instances (a static spawn) has
    // the same one - a wipe in one ended the other's areas (its arena's wall: the bots walked out through it)
    ObjectGuid const guid = owner->GetGUID();
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.owner == guid && entry.mapId == owner->GetMapId() && entry.instanceId == owner->GetInstanceId())
            entry.endMs = std::min(entry.endMs, now);
}

Area ShowCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs, uint32 hitDamage, float keepAway)
{
    float drawnRadius = radius;
    uint32 const aura = Carry(carrier, radius, durationMs, drawnRadius);
    Area area = MakeArea(Area::Kind::Circle, *carrier, 0.0f, drawnRadius);
    if (aura)
    {
        uint64 const id = Register(owner, carrier, area, durationMs, hitDamage);
        if (keepAway > drawnRadius)
        {
            std::lock_guard<std::mutex> guard(RegistryLock);
            for (ActiveArea& entry : Registry)
                if (entry.id == id)
                    entry.keepAway = keepAway;
        }
    }
    return area;
}

Area WatchCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs, uint32 hitDamage, float keepAway)
{
    Area area = MakeArea(Area::Kind::Circle, *carrier, 0.0f, radius);
    if (!owner || !carrier || !carrier->IsInWorld() || !carrier->IsAlive() || durationMs == 0)
        return area;
    uint64 const id = Register(owner, carrier, area, durationMs, hitDamage);
    if (keepAway > radius)
    {
        std::lock_guard<std::mutex> guard(RegistryLock);
        for (ActiveArea& entry : Registry)
            if (entry.id == id)
                entry.keepAway = keepAway;
    }
    return area;
}

bool KeepsAway(Unit* unit)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    ObjectGuid const guid = unit->GetGUID();
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea const& entry : Registry)
        if (entry.carrier == guid && entry.owner != guid && entry.endMs > now &&
            entry.area.kind == Area::Kind::Circle)
            return true;
    return false;
}

Area ShowCarriedStar(Unit* owner, Unit* carrier, uint32 durationMs, uint32 hitDamage)
{
    // Its rays point the way they are drawn, whichever way the carrier turns: a direction of its own, drawn at random
    float const facing = frand(0.0f, float(M_PI) / 2.0f);
    Area area = MakeArea(Area::Kind::Cross, *carrier, facing, CarriedStarArm);
    area.width = CarriedStarWidth;
    if (!carrier->IsInWorld() || !carrier->IsAlive() || durationMs == 0)
        return area;
    // The model is drawn at its own size (Star10): the stalker at scale 1
    if (Creature* stalker = Place(owner, *carrier, facing, SPELL_INDICATOR_CARRIED_STAR, 1.0f, durationMs))
    {
        stalker->AIM_Initialize(new FollowCarrierAI(stalker, carrier->GetGUID()));
        // Its carrier's own: a star is its carrier's to keep away from the others, not to step out of
        Register(owner, carrier, area, durationMs, hitDamage);
    }
    return area;
}

void WatchArea(Unit* owner, Area const& area, uint32 durationMs, uint32 hitDamage, bool tanksTake)
{
    if (!owner || !owner->IsInWorld() || !durationMs)
        return;
    uint64 const id = Register(owner, nullptr, area, durationMs, hitDamage);
    if (!tanksTake)
        return;
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.id == id)
            entry.tanksTake = true;
}

Area CurrentArea(Unit* carrier, Area const& area)
{
    Area current = area;
    if (carrier)
    {
        current.origin.Relocate(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ());
    }
    return current;
}

bool StoodInAreaOf(Unit* victim, Unit* attacker)
{
    if (!victim || !attacker || !victim->IsInWorld())
        return false;

    // The creature an area belongs to, or the one that summoned what hit (a boss's trigger or add)
    ObjectGuid const attackerGuid = attacker->GetGUID();
    ObjectGuid const ownerGuid = attacker->GetCharmerOrOwnerGUID();
    uint64 const now = NowMs();
    std::vector<ActiveArea> areas;
    {
        std::lock_guard<std::mutex> guard(RegistryLock);
        for (ActiveArea const& entry : Registry)
            if (entry.endMs + EndedAreaGraceMs > now && entry.mapId == victim->GetMapId() &&
                entry.instanceId == victim->GetInstanceId() &&
                (entry.owner == attackerGuid || (!ownerGuid.IsEmpty() && entry.owner == ownerGuid)))
                areas.push_back(entry);
    }

    Position const here = victim->GetPosition();
    for (ActiveArea& entry : areas)
    {
        // A circle carried by the victim itself is theirs to take away, not to dodge; a cone aimed at them is theirs
        // to take
        if (entry.carrier == victim->GetGUID() || entry.aimedAt == victim->GetGUID())
            continue;
        Follow(entry, *victim);
        // A trash creature's circle around itself hits the tank it is fighting: a tank holds its ground there
        if (HeldByTank(victim, entry))
            continue;
        if (entry.area.Contains(here, 0.0f))
            return true;
    }
    return false;
}

// The areas unit would leave (FindEscape): those around it, less what a tank holds and what it would survive
std::vector<ActiveArea> AreasToLeave(Unit* unit, bool tank)
{
    std::vector<ActiveArea> areas = AreasAround(unit);
    if (tank)
        areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
            { return entry.tanksTake || HeldByTank(unit, entry); }), areas.end());
    // A tank does not run from a cone aimed at a tank (a tank buster: each has its own, and where the tanks stand is
    // the off-tank's spot's business, FindGoal): running from the other tank's took it through the group
    if (tank)
        areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
            {
                if (entry.aimedAt.IsEmpty() || entry.area.kind != Area::Kind::Cone)
                    return false;
                Player* aimedAt = ObjectAccessor::GetPlayer(*unit, entry.aimedAt);
                return aimedAt && IsGroupTank(aimedAt);
            }), areas.end());
    // A sweeping line is where it will be too: its next SweepLookAheadMs, in steps, so a bot steps out ahead of it
    // rather than just beside it, where it lands a moment later
    for (std::size_t index = 0, count = areas.size(); index < count; ++index)
        if (areas[index].sweepSpeed != 0.0f)
            for (uint32 step = 1; step <= SweepLookAheadSteps; ++step)
            {
                ActiveArea ahead = areas[index];
                float const seconds = float(SweepLookAheadMs * step / SweepLookAheadSteps) / 1000.0f;
                ahead.area.origin.SetOrientation(Position::NormalizeOrientation(
                    ahead.area.origin.GetOrientation() + ahead.sweepSpeed * seconds));
                areas.push_back(ahead);
            }
    // A cone aimed at it lands on it wherever it goes: it holds its ground (a tank keeps it pointed away)
    areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
        { return entry.aimedAt == unit->GetGUID(); }), areas.end());
    // What would not come close to killing it is not worth giving up the fight for: stood in (SurvivableHit)
    areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
        { return SurvivableHit(unit, entry); }), areas.end());
    return areas;
}

bool KeepsOutOf(Unit* unit, Position const& spot)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Player* player = unit->ToPlayer();
    std::vector<ActiveArea> const areas = AreasToLeave(unit, player && IsGroupTank(player));
    return !areas.empty() && InAnyArea(areas, spot, unit->GetGUID(), InsideMargin);
}

bool CrossesAreas(Unit* unit, Position const& spot)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Player* player = unit->ToPlayer();
    std::vector<ActiveArea> const areas = AreasToLeave(unit, player && IsGroupTank(player));
    return !areas.empty() && (InAnyArea(areas, spot, unit->GetGUID(), InsideMargin) ||
        PathCrosses(areas, unit->GetPosition(), spot, unit->GetGUID()));
}

// The nearest spot out of areas for unit (FindEscape's search). all: every area it keeps out of (areas, or more): no
// way through one of them striking before the unit is across and before where it stands strikes. staged: the spot
// may be in a later one of all - the later the better
bool EscapeAmong(Unit* unit, std::vector<ActiveArea> const& areas, std::vector<ActiveArea> const& all, Position& escape,
                 bool tank, bool staged)
{
    // A circle this unit carries: it is the others who must not be in it. A tank does not run from its group
    // with one; they step away from it.
    float carried = 0.0f;
    if (!tank)
        for (ActiveArea const& entry : areas)
            if (entry.carrier == unit->GetGUID())
                carried = std::max(carried, std::max(entry.area.radius, entry.keepAway));

    Position const here = unit->GetPosition();
    bool const inside = InAnyArea(areas, here, unit->GetGUID(), InsideMargin);
    bool const crowding = carried > 0.0f && OtherPlayerNear(unit, here, carried + InsideMargin);
    if (!inside && !crowding)
        return false;

    // When where it stands strikes (the first of the areas it is in): crossing anything striking before that is
    // worse than waiting
    uint64 const now = NowMs();
    uint64 hereStrikes = std::numeric_limits<uint64>::max();
    for (ActiveArea const& entry : areas)
        if (entry.carrier != unit->GetGUID() && entry.area.Contains(here, InsideMargin))
            hereStrikes = std::min(hereStrikes, entry.endMs);
    uint64 soonest = std::numeric_limits<uint64>::max();
    for (ActiveArea const& entry : areas)
        soonest = std::min(soonest, entry.endMs);
    float const speed = std::max(unit->GetSpeed(MOVE_RUN), 1.0f);

    Unit* victim = unit->GetVictim();
    // A bot given a soak dodges within it (a hammer rolling through a Sentence): stepping out of it, the soak fell
    // short. Out only when nothing in it is safe.
    Goal soak;
    bool const soaking = AssignedSoak(unit, soak);
    float const soakInside = std::max(soak.radius - SoakInsideMargin, 0.5f);
    float const distanceOffset = UnitSpread(unit, 2) * EscapeDistanceSpread;
    bool found = false;
    bool foundClean = false;            // a spot whose walk crosses no other area
    float foundRing = 0.0f;
    Candidate best;
    for (uint32 directions : { EscapeDirections, EscapeDirectionsFine })
    {
        if (foundClean)
            break;
        float const angleOffset = UnitSpread(unit, 1) * 2.0f * float(M_PI) / directions;
        // The fine pass in half steps too: a gap a few yards wide (a roller's) falls between two coarse rings
        float const step = directions == EscapeDirectionsFine ? EscapeStep / 2.0f : EscapeStep;
        for (float ring = step; ring <= EscapeReach; ring += step)
        {
            float const distance = ring + distanceOffset;
            for (uint32 direction = 0; direction < directions; ++direction)
            {
                float const angle = angleOffset + 2.0f * float(M_PI) * direction / directions;
                float x = here.GetPositionX() + distance * std::cos(angle);
                float y = here.GetPositionY() + distance * std::sin(angle);
                float z = here.GetPositionZ();
                if (!unit->GetMap()->CheckCollisionAndGetValidCoords(unit, here.GetPositionX(), here.GetPositionY(),
                    here.GetPositionZ(), x, y, z))
                    continue;

                Position const spot(x, y, z);
                if (InAnyArea(areas, spot, unit->GetGUID(), EscapeMargin))
                    continue;
                if (carried > 0.0f && OtherPlayerNear(unit, spot, carried + CarrierClearance))
                    continue;

                // Never through an area striking on the way, before where it stands does: it waits for it instead
                uint64 const arrives = now + uint64(here.GetExactDist2d(&spot) / speed * 1000.0f) + CrossSpareMs;
                bool struckOnTheWay = false;
                for (ActiveArea const& entry : all)
                    if (entry.endMs < arrives && entry.endMs < hereStrikes &&
                        PathCrosses({ entry }, here, spot, unit->GetGUID()))
                        struckOnTheWay = true;
                if (struckOnTheWay)
                    continue;

                // The shortest way out, and not too far from what it is fighting; never through another area when a
                // way round it exists (fire on both sides: the middle, not through the fire)
                bool const crosses = PathCrosses(areas, here, spot, unit->GetGUID());
                float cost = here.GetExactDist2d(&spot);
                if (staged)
                {
                    // Standing in one striking later: the later, the longer before it has to move again
                    uint64 covered = std::numeric_limits<uint64>::max();
                    bool keptOut = false;
                    for (ActiveArea const& entry : all)
                        if (entry.area.Contains(spot, EscapeMargin))
                        {
                            covered = std::min(covered, entry.endMs);
                            keptOut = keptOut || entry.endMs > soonest + StagedHorizonMs;
                        }
                    if (keptOut)
                        continue;
                    float const seconds = covered == std::numeric_limits<uint64>::max() ? LaterStrikeCapSeconds :
                        std::min(LaterStrikeCapSeconds, float(covered > soonest ? covered - soonest : 0) / 1000.0f);
                    cost -= seconds * LaterStrikeYardsPerSecond;
                }
                if (crosses)
                    cost += PathCrossCost;
                if (victim)
                {
                    cost += std::max(0.0f, spot.GetExactDist2d(victim) - here.GetExactDist2d(victim)) * 0.5f;
                    // Not across what it is fighting: from on top of a boss, the way out on its own side, not the
                    // one as good past the boss (the best of two flipped with every step, the bot crossed and
                    // recrossed the middle until it struck)
                    float const dx = spot.GetPositionX() - here.GetPositionX();
                    float const dy = spot.GetPositionY() - here.GetPositionY();
                    float const length = std::max(0.01f, dx * dx + dy * dy);
                    float const t = std::clamp(((victim->GetPositionX() - here.GetPositionX()) * dx +
                        (victim->GetPositionY() - here.GetPositionY()) * dy) / length, 0.0f, 1.0f);
                    Position const nearest(here.GetPositionX() + dx * t, here.GetPositionY() + dy * t,
                        here.GetPositionZ());
                    if (t > 0.0f && nearest.GetExactDist2d(victim) < AcrossVictimRadius)
                        cost += AcrossVictimCost;
                }
                cost += CrowdCost * PlayersNear(unit, spot, CrowdRadius);
                if (soaking && spot.GetExactDist2d(&soak.center) > soakInside)
                    cost += OutOfSoakCost;
                if (!found || cost < best.cost)
                {
                    best.spot = spot;
                    best.cost = cost;
                    found = true;
                }
                if (!crosses && !foundClean)
                {
                    foundClean = true;
                    foundRing = ring;
                }
            }

            // The nearest ring with a clean way out, and the one after it (an empty spot a step further beats a
            // crowded one), are enough: any further only costs more. A way through an area is kept only when no
            // ring within reach has a clean one.
            if (!staged && foundClean && ring >= foundRing + EscapeStep &&
                (!soaking || best.spot.GetExactDist2d(&soak.center) <= soakInside || ring > 2.0f * soak.radius))
                break;
        }
    }

    if (found)
        escape = best.spot;
    return found;
}

// The spot of its own the fight gave unit (SetUnitSpot), if it has one
bool OwnSpot(Unit* unit, Goal& own)
{
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::Unit && goal.tank == unit->GetGUID())
        {
            own = goal;
            return true;
        }
    return false;
}

bool FindEscape(Unit* unit, Position& escape, bool tank)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    std::vector<ActiveArea> areas = AreasToLeave(unit, tank);
    // A bot the fight placed (a spot of its own) leaves the circles players carry to the fight's placing: a spread's,
    // where each has its spot - stepping away from them pushed it off its own, into another's
    Goal own;
    if (OwnSpot(unit, own))
        areas.erase(std::remove_if(areas.begin(), areas.end(),
            [](ActiveArea const& entry) { return !entry.carrier.IsEmpty(); }), areas.end());
    if (areas.empty())
        return false;
    if (EscapeAmong(unit, areas, areas, escape, tank, false))
        return true;

    // Nowhere clear of them all (rings round a boss covering the whole floor, struck in turn): out of those striking
    // first, the rest left for when they come
    uint64 soonest = areas.front().endMs;
    for (ActiveArea const& entry : areas)
        soonest = std::min(soonest, entry.endMs);
    std::vector<ActiveArea> first;
    for (ActiveArea const& entry : areas)
        if (entry.endMs <= soonest + SoonestWindowMs)
            first.push_back(entry);
    return first.size() < areas.size() && EscapeAmong(unit, first, areas, escape, tank, true);
}

void ShowSoak(Unit* owner, Position const& center, float radius, uint32 durationMs, uint32 wanted, Theme theme,
              bool tanks, bool particles)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;

    Goal goal;
    goal.kind = Goal::Kind::Soak;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = center;
    goal.radius = radius;
    goal.wanted = wanted;
    goal.endMs = NowMs() + durationMs;
    goal.tanks = tanks;
    AddGoal(goal, false);
    if (particles)
        ShowParticles(owner, MakeArea(Area::Kind::Circle, center, 0.0f, radius), theme, durationMs);
}

void SetUnitSpot(Unit* owner, Unit* unit, Position const& spot, float radius, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || !unit || durationMs == 0)
        return;

    Goal goal;
    goal.kind = Goal::Kind::Unit;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = spot;
    goal.radius = radius;
    goal.tank = unit->GetGUID();
    goal.endMs = NowMs() + durationMs;
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [&goal](Goal const& entry)
        {
            return entry.kind == Goal::Kind::Unit && entry.owner == goal.owner && entry.tank == goal.tank &&
                entry.instanceId == goal.instanceId;
        }), Goals.end());
    Goals.push_back(goal);
}

void EndUnitSpot(Unit* owner, Unit* unit)
{
    if (!owner || !unit)
        return;
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [owner, unit](Goal const& goal)
        {
            return goal.kind == Goal::Kind::Unit && goal.owner == owner->GetGUID() && goal.tank == unit->GetGUID();
        }), Goals.end());
}

bool HasFightPlace(Unit* unit)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Goal soak;
    if (AssignedSoak(unit, soak) || HoldsOffTankSpot(unit) || KeepsAway(unit))
        return true;
    for (Goal const& goal : GoalsAround(unit))
        if ((goal.kind == Goal::Kind::Unit || goal.kind == Goal::Kind::Still) && goal.tank == unit->GetGUID())
            return true;
    return false;
}

ObjectGuid ShowTether(Unit* owner, Unit* from, Unit* to, uint32 spell, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || !from || !to || durationMs == 0)
        return ObjectGuid::Empty;
    TempSummon* source = owner->SummonCreature(NPC_GROUND_INDICATOR, from->GetPosition(), TEMPSUMMON_TIMED_DESPAWN,
                                               durationMs);
    TempSummon* end = owner->SummonCreature(NPC_GROUND_INDICATOR, to->GetPosition(), TEMPSUMMON_TIMED_DESPAWN,
                                            durationMs);
    if (!source || !end)
        return ObjectGuid::Empty;
    source->AIM_Initialize(new FollowCarrierAI(source, from->GetGUID()));
    end->AIM_Initialize(new FollowCarrierAI(end, to->GetGUID()));
    // A spell cannot take a unit that cannot be selected as its target, and the stalkers are made so
    end->RemoveUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    ObjectGuid const endGuid = end->GetGUID();
    ObjectGuid const sourceGuid = source->GetGUID();
    // A moment after both exist for the clients, or the beam has nothing to reach
    source->m_Events.AddEventAtOffset([source, endGuid, spell]()
    {
        if (Creature* target = source->GetMap()->GetCreature(endGuid))
            source->CastSpell(target, spell, true);
    }, 300ms);
    return sourceGuid;
}

void EndTether(Unit* owner, ObjectGuid tether)
{
    if (!owner || !owner->IsInWorld() || tether.IsEmpty())
        return;
    if (Creature* source = owner->GetMap()->GetCreature(tether))
    {
        source->InterruptNonMeleeSpells(false);
        source->RemoveAllAuras();
        source->DespawnOrUnsummon(100ms);
    }
}

void SetHoldStill(Unit* owner, Unit* unit, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || !unit || durationMs == 0)
        return;
    Goal goal;
    goal.kind = Goal::Kind::Still;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = unit->GetPosition();
    goal.tank = unit->GetGUID();
    goal.endMs = NowMs() + durationMs;
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [&goal](Goal const& entry)
        {
            return entry.kind == Goal::Kind::Still && entry.owner == goal.owner && entry.tank == goal.tank &&
                entry.instanceId == goal.instanceId;
        }), Goals.end());
    Goals.push_back(goal);
}

bool HoldsUnitSpot(Unit* unit)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::Unit && goal.tank == unit->GetGUID() &&
            unit->GetExactDist2d(&goal.center) <= goal.radius + UnitSpotHoldSlack)
            return true;
    return false;
}

bool HoldsStill(Unit* unit)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::Still && goal.tank == unit->GetGUID())
            return true;
    return false;
}

void SetLookAway(Unit* owner, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;
    Goal goal;
    goal.kind = Goal::Kind::LookAway;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = owner->GetPosition();
    goal.radius = LookAwayReach;
    goal.endMs = NowMs() + durationMs;
    AddGoal(goal, true);
}

bool LooksAway(Unit* unit, Position& from)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::LookAway && unit->GetExactDist2d(&goal.center) <= goal.radius)
        {
            from = goal.center;
            return true;
        }
    return false;
}

std::vector<ObjectGuid> SoakAssignees(Unit* owner, Position const& center)
{
    if (!owner)
        return {};
    std::lock_guard<std::mutex> guard(GoalLock);
    for (Goal const& goal : Goals)
        if (goal.kind == Goal::Kind::Soak && goal.owner == owner->GetGUID() &&
            goal.instanceId == owner->GetInstanceId() && goal.center.GetExactDist2d(&center) < 0.5f)
            return goal.assigned;
    return {};
}

void WarnGroupDamage(Unit* owner, uint32 inMs)
{
    if (!owner || !owner->IsInWorld())
        return;
    Goal goal;
    goal.kind = Goal::Kind::GroupHit;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = owner->GetPosition();
    goal.endMs = NowMs() + inMs;
    AddGoal(goal, false);
}

bool GroupDamageSoon(Unit* unit, uint32 withinMs)
{
    if (!unit || !unit->IsInWorld())
        return false;
    uint64 const now = NowMs();
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::GroupHit && goal.endMs <= now + withinMs)
            return true;
    return false;
}

void EndSoak(Unit* owner, Position const& center)
{
    if (!owner)
        return;
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [owner, &center](Goal const& goal)
        {
            return goal.kind == Goal::Kind::Soak && goal.owner == owner->GetGUID() &&
                goal.instanceId == owner->GetInstanceId() && goal.center.GetExactDist2d(&center) < 0.5f;
        }), Goals.end());
}

void ShowDecal(Unit* owner, Position const& center, float orientation, float radius, uint32 durationMs, uint32 spellId)
{
    Place(owner, center, orientation, spellId, radius, durationMs);
}

void ShowBillboard(Unit* owner, Position const& position, uint32 look, uint32 durationMs, Unit* follow,
                   ObjectGuid* placed)
{
    if (!owner || !owner->IsInWorld())
        return;
    // On the floor: the model carries its own height (a stalker in the air would fall)
    Position const at = follow ? Position(follow->GetPositionX(), follow->GetPositionY(), follow->GetPositionZ()) :
        OnGround(owner, position.GetPositionX(), position.GetPositionY(), position.GetPositionZ());
    // Built to its size: never scaled
    Creature* stalker = Place(owner, at, 0.0f, look, 1.0f, durationMs);
    if (!stalker)
        return;
    if (follow)
        stalker->AIM_Initialize(new FollowCarrierAI(stalker, follow->GetGUID()));
    if (placed)
        *placed = stalker->GetGUID();
}

bool ShowCarriedLook(Unit* carrier, uint32 look, uint32 durationMs)
{
    if (!carrier || !carrier->IsInWorld() || !carrier->IsAlive())
        return false;
    Aura* aura = carrier->AddAura(look, carrier);
    if (!aura)
        return false;
    // The look's spell never ends by itself: a time given is set on it
    if (durationMs)
    {
        aura->SetMaxDuration(int32(durationMs));
        aura->SetDuration(int32(durationMs));
    }
    return true;
}

void ClearCarriedLook(Unit* carrier, uint32 look)
{
    if (carrier)
        carrier->RemoveAurasDueToSpell(look);
}

void ShowCurtainRing(Unit* owner, Position const& center, uint32 firstLook, uint32 looks, uint32 pieces,
                     float pieceLength, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || pieces < 3 || looks == 0 || durationMs == 0)
        return;
    // Straight pieces meeting at their ends: a regular polygon, each piece on its own carrier at its middle, turned
    // along its side counterclockwise (the model runs along its carrier's facing, its band's u with it: the next
    // piece's share follows on)
    float const middle = pieceLength / (2.0f * std::tan(float(M_PI) / float(pieces)));
    for (uint32 index = 0; index < pieces; ++index)
    {
        float const angle = 2.0f * float(M_PI) * float(index) / float(pieces);
        Position const at = OnGround(owner, center.GetPositionX() + middle * std::cos(angle),
                                     center.GetPositionY() + middle * std::sin(angle), center.GetPositionZ());
        Place(owner, at, Position::NormalizeOrientation(angle + float(M_PI) / 2.0f), firstLook + index % looks, 1.0f,
              durationMs);
    }
}

void ShowWardenGaze(Unit* owner, Position const& position, uint32 durationMs, Unit* follow)
{
    ShowBillboard(owner, position, SPELL_WARDEN_GAZE, durationMs, follow);
}

bool ShowCarriedNumber(Unit* carrier, uint32 number, uint32 durationMs)
{
    if (number < 1 || number > 8)
        return false;
    ClearCarriedNumber(carrier);
    return ShowCarriedLook(carrier, SPELL_WARDEN_NUMBER_FIRST + number - 1, durationMs);
}

void ClearCarriedNumber(Unit* carrier)
{
    for (uint32 number = 0; number < 8; ++number)
        ClearCarriedLook(carrier, SPELL_WARDEN_NUMBER_FIRST + number);
}

// How big a cell's number is, and a roll call mark's pair, for the seal's or the plaque's radius: inside its flat
// empty middle
constexpr float CellNumberShare = 0.55f;
constexpr float RollCallPairShare = 0.62f;

void ShowCell(Unit* owner, Position const& center, uint32 number, float orientation, uint32 durationMs, float radius)
{
    if (!owner || !owner->IsInWorld() || number < 1 || number > 8 || durationMs == 0)
        return;
    Position const at = OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ());
    Place(owner, at, orientation, SPELL_WARDEN_CELL_IRON, radius, durationMs);
    Place(owner, at, orientation, SPELL_WARDEN_CELL_RUNES, radius, durationMs);
    Place(owner, at, orientation, SPELL_WARDEN_CELL_NUMBER_FIRST + number - 1, radius * CellNumberShare, durationMs);
}

void FlareCell(Unit* owner, Position const& center, float radius, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;
    Place(owner, OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ()), 0.0f,
          SPELL_WARDEN_CELL_FLARE, radius, durationMs);
}

void ShowRollCallMark(Unit* owner, Position const& center, uint32 pair, float orientation, uint32 durationMs,
                      float radius)
{
    if (!owner || !owner->IsInWorld() || pair > 3 || durationMs == 0)
        return;
    Position const at = OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ());
    Place(owner, at, orientation, SPELL_WARDEN_ROLL_CALL, radius, durationMs);
    Place(owner, at, orientation, SPELL_WARDEN_ROLL_CALL_PAIR_FIRST + pair, radius * RollCallPairShare, durationMs);
}

void ShowWardenWall(Unit* owner, Position const& center, uint32 durationMs, uint32 delayMs)
{
    if (!owner || !owner->IsInWorld())
        return;
    if (delayMs)
    {
        // The owner's own event: it goes with the owner
        Position const at = center;
        owner->m_Events.AddEventAtOffset([owner, at, durationMs]()
        {
            ShowWardenWall(owner, at, durationMs);
        }, Milliseconds(delayMs));
        return;
    }
    ShowCurtainRing(owner, center, SPELL_WARDEN_WALL_FIRST, WardenWallLooks, WardenWallPieces, WardenWallPieceLength,
                    durationMs);
}

void ShowWardenShockwave(Unit* owner, Position const& center, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;
    // The ring is drawn at its full size (the model grows it from its middle on its own animation)
    Place(owner, OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ()), 0.0f,
          SPELL_WARDEN_WALL_SHOCKWAVE, WardenShockwaveRadius, durationMs);
}

void ShowWardenStrike(Unit* owner, Position const& from, float orientation)
{
    if (!owner || !owner->IsInWorld())
        return;
    // Built to its size (never scaled: a scale set as it appears grows in slowly on the client)
    Place(owner, OnGround(owner, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ()), orientation,
          SPELL_WARDEN_STRIKE, 1.0f, WardenStrikeMs);
}

void ShowWardenPushTrail(Unit* owner, Position const& from, float orientation)
{
    if (!owner || !owner->IsInWorld())
        return;
    Place(owner, OnGround(owner, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ()), orientation,
          SPELL_WARDEN_PUSH_TRAIL, 1.0f, WardenPushTrailMs);
}

void ShowWardenSealBurst(Unit* owner, Position const& center)
{
    if (!owner || !owner->IsInWorld())
        return;
    // At its full size: the model grows it from its middle
    Place(owner, OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ()), 0.0f,
          SPELL_WARDEN_SEAL_BURST, 1.0f, WardenSealBurstMs);
}

void ShowWardenRollCallBlow(Unit* owner, Position const& from, float orientation)
{
    if (!owner || !owner->IsInWorld())
        return;
    // Built to its size, its apex at its carrier (shapes.json `centred`): it grows out of it
    Place(owner, OnGround(owner, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ()), orientation,
          SPELL_WARDEN_ROLL_CALL_BLOW, 1.0f, WardenRollCallBlowMs);
}

void ShowWardenMark(Unit* owner, Position const& from, float orientation, uint32 look, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld())
        return;
    Place(owner, OnGround(owner, from.GetPositionX(), from.GetPositionY(), from.GetPositionZ()), orientation, look,
          1.0f, durationMs);
}

void ShowWardenCellBars(Unit* owner, Position const& center, uint32 durationMs, bool tall)
{
    // Sixteen sides 1.17 yards long: their corners on the cell's 3-yard circle
    ShowCurtainRing(owner, center, tall ? SPELL_WARDEN_CELL_BARS : SPELL_WARDEN_CELL_BARS_LOW, 1, 16, 1.1705f,
                    durationMs);
}

void ShowWardenCurfew(Unit* owner, Position const& center, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;
    // Built to its size
    Place(owner, OnGround(owner, center.GetPositionX(), center.GetPositionY(), center.GetPositionZ()), 0.0f,
          SPELL_WARDEN_CURFEW_RING, 1.0f, durationMs);
}

void SetOffTankSpot(Unit* owner, Position const& spot, uint32 durationMs, bool hold, Unit* tank)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;

    Goal goal;
    goal.kind = Goal::Kind::OffTank;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = spot;
    goal.endMs = NowMs() + durationMs;
    goal.hold = hold;
    if (tank)
        goal.tank = tank->GetGUID();
    AddGoal(goal, true);
}

void SetBossHolder(Unit* owner, Unit* tank, uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || !tank || durationMs == 0)
        return;

    Goal goal;
    goal.kind = Goal::Kind::Holder;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.tank = tank->GetGUID();
    goal.endMs = NowMs() + durationMs;
    AddGoal(goal, true);
}

Unit* BossToTaunt(Unit* unit)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return nullptr;
    for (Goal const& goal : GoalsAround(unit))
    {
        if (goal.kind != Goal::Kind::Holder || goal.tank != unit->GetGUID())
            continue;
        Unit* owner = ObjectAccessor::GetUnit(*unit, goal.owner);
        if (owner && owner->IsAlive() && owner->IsInCombat() && owner->GetVictim() != unit)
            return owner;
    }
    return nullptr;
}

bool LeavesToOtherTank(Unit* unit, Unit* target)
{
    if (!unit || !target || !unit->IsInWorld())
        return false;
    for (Goal const& goal : GoalsAround(unit))
        if (goal.kind == Goal::Kind::Holder && goal.owner == target->GetGUID() && goal.tank != unit->GetGUID())
            return true;
    return false;
}

void DrawInstantly(Unit* owner, bool instantly)
{
    if (!owner)
        return;
    std::lock_guard<std::mutex> guard(InstantLock);
    if (instantly)
        InstantOwners.insert({ owner->GetGUID(), owner->GetInstanceId() });
    else
        InstantOwners.erase({ owner->GetGUID(), owner->GetInstanceId() });
}

uint32 FadingTwinOf(uint32 look)
{
    return FadeLookOf(look);
}

void SetTankSpot(Unit* owner, Position const& spot, uint32 durationMs, float slack)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return;

    Goal goal;
    goal.kind = Goal::Kind::Tank;
    goal.mapId = owner->GetMapId();
    goal.instanceId = owner->GetInstanceId();
    goal.owner = owner->GetGUID();
    goal.center = spot;
    goal.radius = slack;
    goal.endMs = NowMs() + durationMs;
    AddGoal(goal, true);
}

bool PlacesTanks(Unit* owner)
{
    if (!owner || !owner->IsInWorld())
        return false;
    for (Goal const& goal : GoalsAround(owner))
        if (goal.kind == Goal::Kind::OffTank && goal.owner == owner->GetGUID())
            return true;
    return false;
}

// Where the fight wants a unit (its spot, its side of the boss, its soak), as the crow flies
bool FindGoalSpot(Unit* unit, Position& spot, bool tank);

// The way to a spot the fight wants a unit at: straight there when the walk crosses no area, else by a waypoint
// clear of every area and reached and left by clean walks (fire on both sides of the seat: round through the middle),
// the shortest such detour; straight there when none is found
Position RouteAround(Unit* unit, bool tank, Position const& goal)
{
    std::vector<ActiveArea> const areas = AreasToLeave(unit, tank);
    Position const here = unit->GetPosition();
    if (areas.empty() || !PathCrosses(areas, here, goal, unit->GetGUID()))
        return goal;

    bool found = false;
    float foundRing = 0.0f;
    Candidate best;
    for (float ring = EscapeStep; ring <= EscapeReach; ring += EscapeStep)
    {
        for (uint32 direction = 0; direction < RouteDirections; ++direction)
        {
            float const angle = 2.0f * float(M_PI) * direction / RouteDirections;
            float x = here.GetPositionX() + ring * std::cos(angle);
            float y = here.GetPositionY() + ring * std::sin(angle);
            float z = here.GetPositionZ();
            if (!unit->GetMap()->CheckCollisionAndGetValidCoords(unit, here.GetPositionX(), here.GetPositionY(),
                here.GetPositionZ(), x, y, z))
                continue;
            Position const waypoint(x, y, z);
            if (InAnyArea(areas, waypoint, unit->GetGUID(), EscapeMargin) ||
                PathCrosses(areas, here, waypoint, unit->GetGUID()) ||
                PathCrosses(areas, waypoint, goal, unit->GetGUID()))
                continue;
            float const cost = here.GetExactDist2d(&waypoint) + waypoint.GetExactDist2d(&goal);
            if (!found || cost < best.cost)
            {
                if (!found)
                    foundRing = ring;
                best.spot = waypoint;
                best.cost = cost;
                found = true;
            }
        }
        // A detour further out than the first ring that has one only gets longer
        if (found && ring >= foundRing + 2.0f * EscapeStep)
            break;
    }
    return found ? best.spot : goal;
}

bool StillAWayOut(Unit* unit, Position const& spot)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Player* player = unit->ToPlayer();
    std::vector<ActiveArea> areas = AreasToLeave(unit, player && IsGroupTank(player));
    Goal own;
    if (OwnSpot(unit, own))
        areas.erase(std::remove_if(areas.begin(), areas.end(),
            [](ActiveArea const& entry) { return !entry.carrier.IsEmpty(); }), areas.end());
    if (areas.empty())
        return false;
    // As FindEscape takes it: out of those striking first; in a later one only if it strikes before long (not a
    // wall's outside); not through one striking before it is there
    uint64 soonest = std::numeric_limits<uint64>::max();
    for (ActiveArea const& entry : areas)
        soonest = std::min(soonest, entry.endMs);
    for (ActiveArea const& entry : areas)
        if (entry.area.Contains(spot, InsideMargin) &&
            (entry.endMs <= soonest + SoonestWindowMs || entry.endMs > soonest + StagedHorizonMs))
            return false;
    return !StruckOnTheWay(unit, spot);
}

bool FindOwnSpot(Unit* unit, Position& spot)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Goal own;
    if (!OwnSpot(unit, own) || unit->GetExactDist2d(&own.center) <= own.radius)
        return false;
    // Not to a spot about to burn, nor through red striking before it is there (it waits); the circles players carry
    // are the fight's to place
    Player* player = unit->ToPlayer();
    std::vector<ActiveArea> areas = AreasToLeave(unit, player && IsGroupTank(player));
    areas.erase(std::remove_if(areas.begin(), areas.end(),
        [](ActiveArea const& entry) { return !entry.carrier.IsEmpty(); }), areas.end());
    if (InAnyArea(areas, own.center, unit->GetGUID(), InsideMargin) || StruckOnTheWay(unit, own.center))
        return false;
    spot = own.center;
    return true;
}

bool StruckOnTheWay(Unit* unit, Position const& goal)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Player* player = unit->ToPlayer();
    std::vector<ActiveArea> const areas = AreasToLeave(unit, player && IsGroupTank(player));
    if (areas.empty())
        return false;
    Position const here = unit->GetPosition();
    uint64 hereStrikes = std::numeric_limits<uint64>::max();
    for (ActiveArea const& entry : areas)
        if (entry.carrier != unit->GetGUID() && entry.area.Contains(here, InsideMargin))
            hereStrikes = std::min(hereStrikes, entry.endMs);
    float const speed = std::max(unit->GetSpeed(MOVE_RUN), 1.0f);
    uint64 const arrives = NowMs() + uint64(here.GetExactDist2d(&goal) / speed * 1000.0f) + CrossSpareMs;
    for (ActiveArea const& entry : areas)
        if (entry.endMs < arrives && entry.endMs < hereStrikes && PathCrosses({ entry }, here, goal, unit->GetGUID()))
            return true;
    return false;
}

bool Detour(Unit* unit, Position const& goal, Position& waypoint)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;
    Player* player = unit->ToPlayer();
    waypoint = RouteAround(unit, player && IsGroupTank(player), goal);
    return waypoint.GetExactDist2d(&goal) > 0.5f;
}

bool FindGoal(Unit* unit, Position& spot, bool tank)
{
    if (!FindGoalSpot(unit, spot, tank))
        return false;
    spot = RouteAround(unit, tank, spot);
    return true;
}

bool FindGoalSpot(Unit* unit, Position& spot, bool tank)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    for (Goal const& goal : GoalsAround(unit))
    {
        Unit* owner = ObjectAccessor::GetUnit(*unit, goal.owner);
        if (!owner || !owner->IsAlive())
            continue;

        // A spot of its own comes first: a fight that names one for a bot needs it there whatever its role
        if (goal.kind == Goal::Kind::Unit)
        {
            if (goal.tank != unit->GetGUID() || unit->GetExactDist2d(&goal.center) <= goal.radius)
                continue;
            spot = goal.center;
            return true;
        }

        if (goal.kind == Goal::Kind::OffTank)
        {
            // The tank it names, or a tank the owner is not hitting: the one it is stays where it holds it. Not while
            // the spot is red: it stepped out of it, and would walk straight back in
            bool const forUnit = goal.tank.IsEmpty() ? owner->GetVictim() != unit : goal.tank == unit->GetGUID();
            if (!tank || !owner->IsInCombat() || !forUnit ||
                unit->GetExactDist2d(&goal.center) <= (goal.hold ? OffTankHoldSlack : OffTankSlack) ||
                KeepsOutOf(unit, goal.center))
                continue;
            spot = goal.center;
            return true;
        }

        // The tank the owner is hitting, back to where it should hold it
        if (goal.kind == Goal::Kind::Tank)
        {
            if (!tank || !owner->IsInCombat() || owner->GetVictim() != unit ||
                unit->GetExactDist2d(&goal.center) <= goal.radius || KeepsOutOf(unit, goal.center))
                continue;
            spot = goal.center;
            return true;
        }
    }

    // A soak: the one this bot is given (AssignSoaks); inside it already, nothing to walk to (HoldsSoak keeps it)
    Goal soak;
    if (!AssignedSoak(unit, soak))
        return false;
    float const inside = std::max(soak.radius - SoakInsideMargin, 0.5f);
    if (unit->GetExactDist2d(&soak.center) <= inside)
        return false;
    // Each to a spot of its own inside it, not all onto its very middle
    float const angle = UnitSpread(unit, 3) * 2.0f * float(M_PI);
    float const distance = inside * 0.5f * UnitSpread(unit, 4);
    spot.Relocate(soak.center.GetPositionX() + std::cos(angle) * distance,
        soak.center.GetPositionY() + std::sin(angle) * distance, soak.center.GetPositionZ());
    return true;
}

bool HoldsOffTankSpot(Unit* unit)
{
    if (!unit || !unit->IsAlive() || !unit->IsInWorld())
        return false;
    for (Goal const& goal : GoalsAround(unit))
    {
        if (goal.kind != Goal::Kind::OffTank || !goal.hold ||
            unit->GetExactDist2d(&goal.center) > OffTankHoldSlack + 1.0f)
            continue;
        Unit* owner = ObjectAccessor::GetUnit(*unit, goal.owner);
        if (owner && owner->IsAlive() &&
            (goal.tank.IsEmpty() ? owner->GetVictim() != unit : goal.tank == unit->GetGUID()))
            return true;
    }
    return false;
}

bool HoldsSoak(Unit* unit)
{
    Goal soak;
    if (!unit || !unit->IsAlive() || !AssignedSoak(unit, soak))
        return false;
    return unit->GetExactDist2d(&soak.center) <= std::max(soak.radius - SoakInsideMargin, 0.5f);
}
}

namespace
{
// The circles creatures carry for their damaging auras, so each goes when its aura ends
constexpr char const* HazardDataKey = "GroundIndicatorHazards";
// An aura without an end is shown this long at a time, and shown again when it is refreshed
constexpr uint32 EndlessHazardMs = HOUR * IN_MILLISECONDS;

struct HazardCircle
{
    uint32 sourceAura = 0;
    uint32 carriedAura = 0;
    uint64 areaId = 0;
};

struct CreatureHazards : DataMap::Base
{
    std::vector<HazardCircle> circles;
};

bool IsCarriedSpell(uint32 spellId)
{
    return std::ranges::any_of(CarriedSpells, [spellId](ShapeSpell const& shape) { return shape.spell == spellId; });
}

// How far around its owner an aura keeps hurting: through the spell it triggers every tick, or as an aura laid on
// every enemy near it
// Hazards whose reach their script sets rather than their spell (a spell radius of 0, or a huge one the script cuts
// down), any difficulty's version: how far they reach, in yards, or per yard of their owner's scale for those that
// grow with it.
struct ScriptedHazard
{
    uint32 aura;
    float radius;
    bool perScale;
};
constexpr std::array<ScriptedHazard, 3> ScriptedHazards = { {
    { 69076, 9.0f, false },     // Bone Storm (Lord Marrowgar): full damage within 9 yards (spell_marrowgar_bone_storm)
    { 72743, 10.0f, true },     // Defile (the Lich King), growing (spell_the_lich_king_defile)
    { 70343, 2.5f, true },      // Slime Puddle (Professor Putricide), growing (spell_putricide_slime_puddle)
} };

float HazardRadius(Aura const* aura, Unit* owner)
{
    for (ScriptedHazard const& hazard : ScriptedHazards)
        if (aura->GetId() == hazard.aura || aura->GetId() == sSpellMgr->GetSpellIdForDifficulty(hazard.aura, owner))
            return hazard.perScale ? hazard.radius * owner->GetObjectScale() : hazard.radius;

    SpellInfo const* spellInfo = aura->GetSpellInfo();
    float radius = 0.0f;
    for (uint8 effIndex = 0; effIndex < MAX_SPELL_EFFECTS; ++effIndex)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[effIndex];
        if (effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL ||
            effect.ApplyAuraName == SPELL_AURA_PERIODIC_TRIGGER_SPELL_WITH_VALUE)
            radius = std::max(radius, HarmfulRadiusAround(sSpellMgr->GetSpellInfo(effect.TriggerSpell), owner));
        else if (effect.Effect == SPELL_EFFECT_APPLY_AREA_AURA_ENEMY && !spellInfo->IsPositiveEffect(effIndex))
            radius = std::max(radius, effect.CalcRadius(owner));
    }
    return radius;
}

// Auras a creature is given before it is in the world (its template's auras, Ingvar's Shadow Axe) cannot be drawn
// then: they are drawn on its first update in the world. The count lets every other creature's update skip the check.
constexpr char const* PendingHazardKey = "GroundIndicatorPendingHazards";
struct PendingHazards : DataMap::Base
{
    std::vector<uint32> auras;
};
std::atomic<uint32> PendingHazardCreatures{ 0 };

void ShowHazard(Creature* creature, Aura* aura);

class GroundIndicatorUnitScript : public UnitScript
{
public:
    GroundIndicatorUnitScript() : UnitScript("GroundIndicatorUnitScript", true,
        { UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_AURA_REMOVE }) { }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !aura || IsCarriedSpell(aura->GetId()) || !creature->IsAlive() ||
            IsPlayerControlled(creature))
            return;

        if (!creature->IsInWorld())
        {
            PendingHazards* pending = creature->CustomData.GetDefault<PendingHazards>(PendingHazardKey);
            if (pending->auras.empty())
                ++PendingHazardCreatures;
            pending->auras.push_back(aura->GetId());
            return;
        }
        ShowHazard(creature, aura);
    }

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override;
};

void ShowHazard(Creature* creature, Aura* aura)
{
    {
        if (!InIndicatorMap(creature))
            return;

        // A hazard of the players' foes: its own, or put on it by one of them (a boss arming a trigger). Never an aura
        // the players put there: a shadow priest's Mind Sear sits on the mob it is channelled on and pulses around it,
        // which read as a danger to the players and followed the mob about while the tank ran from it.
        Unit* caster = aura->GetCaster();
        if (!caster || !ShouldShow(caster))
            return;

        CreatureHazards* hazards = creature->CustomData.GetDefault<CreatureHazards>(HazardDataKey);
        if (std::ranges::any_of(hazards->circles, [aura](HazardCircle const& circle)
            { return circle.sourceAura == aura->GetId(); }))
            return;

        float const radius = HazardRadius(aura, creature);
        if (radius < MinRadius || radius > MaxRadius)
            return;

        uint32 const durationMs = aura->GetDuration() > 0 ? uint32(aura->GetDuration()) : EndlessHazardMs;
        float drawnRadius = radius;
        uint32 const carried = Carry(creature, radius, durationMs, drawnRadius);
        if (!carried)
            return;

        hazards->circles.push_back({ aura->GetId(), carried, Register(creature, creature,
            MakeArea(GroundIndicators::Area::Kind::Circle, *creature, 0.0f, drawnRadius), durationMs,
            EstimateHit(caster, aura->GetSpellInfo())) });
    }
}

void GroundIndicatorUnitScript::OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode /*mode*/)
{
    {
        if (!unit || !aurApp || !unit->IsCreature())
            return;

        CreatureHazards* hazards = unit->CustomData.Get<CreatureHazards>(HazardDataKey);
        if (!hazards)
            return;

        uint32 const sourceAura = aurApp->GetBase()->GetId();
        auto const circle = std::ranges::find_if(hazards->circles, [sourceAura](HazardCircle const& entry)
            { return entry.sourceAura == sourceAura; });
        if (circle == hazards->circles.end())
            return;

        HazardCircle const removed = *circle;
        hazards->circles.erase(circle);
        Unregister(removed.areaId);
        unit->RemoveAurasDueToSpell(removed.carriedAura);
    }
}

class GroundIndicatorCreatureScript : public AllCreatureScript
{
public:
    GroundIndicatorCreatureScript() : AllCreatureScript("GroundIndicatorCreatureScript") { }

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        if (!PendingHazardCreatures.load(std::memory_order_relaxed) || !creature->IsInWorld())
            return;
        PendingHazards* pending = creature->CustomData.Get<PendingHazards>(PendingHazardKey);
        if (!pending || pending->auras.empty())
            return;

        std::vector<uint32> const auras = std::move(pending->auras);
        pending->auras.clear();
        --PendingHazardCreatures;
        if (!creature->IsAlive())
            return;
        for (uint32 auraId : auras)
            if (Aura* aura = creature->GetAura(auraId))
                ShowHazard(creature, aura);
    }
};

// A spell area left on the ground: shown once, the first time it updates, and resized as it grows
constexpr char const* PoolDataKey = "GroundIndicatorPool";

struct PoolCircle : DataMap::Base
{
    bool shown = false;
    ObjectGuid stalker;
    uint64 areaId = 0;
    float radius = 0.0f;
};

class GroundIndicatorDynamicObjectScript : public DynamicObjectScript
{
public:
    GroundIndicatorDynamicObjectScript() : DynamicObjectScript("GroundIndicatorDynamicObjectScript") { }

    void OnUpdate(DynamicObject* pool, uint32 /*diff*/) override
    {
        PoolCircle* circle = pool->CustomData.Get<PoolCircle>(PoolDataKey);
        if (!circle)
        {
            circle = pool->CustomData.GetDefault<PoolCircle>(PoolDataKey);
            Show(pool, *circle);
            return;
        }

        // Growing (Defile) or shrinking: the circle follows its size
        if (!circle->shown || std::fabs(pool->GetRadius() - circle->radius) < 0.25f)
            return;

        circle->radius = pool->GetRadius();
        if (Creature* stalker = pool->GetMap()->GetCreature(circle->stalker))
            stalker->SetObjectScale(circle->radius);
        Resize(circle->areaId, circle->radius);
    }

private:
    static void Show(DynamicObject* pool, PoolCircle& circle)
    {
        if (pool->GetByteValue(DYNAMICOBJECT_BYTES, 0) != DYNAMIC_OBJECT_AREA_SPELL || !ShouldShow(pool->GetCaster()))
            return;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(pool->GetSpellId());
        float const radius = pool->GetRadius();
        int32 const duration = pool->GetDuration();
        if (!spellInfo || spellInfo->IsPositive() || radius < MinRadius || radius > MaxRadius || duration <= 0)
            return;

        Unit* caster = pool->GetCaster();
        Creature* stalker = Place(caster, *pool, 0.0f, SPELL_INDICATOR_CIRCLE, radius, uint32(duration));
        if (!stalker)
            return;

        circle.shown = true;
        circle.stalker = stalker->GetGUID();
        circle.radius = radius;
        GroundIndicators::Area const area = MakeArea(GroundIndicators::Area::Kind::Circle, *pool, 0.0f, radius);
        circle.areaId = Register(caster, nullptr, area, uint32(duration), EstimateHit(caster, spellInfo));
        GroundIndicators::ShowParticles(caster, area, GroundIndicators::ThemeOf(spellInfo->GetSchoolMask()),
            uint32(duration));
    }
};
}

namespace
{
using namespace Acore::ChatCommands;

// .indicator circle <radius> | rect <length> <width> | cone <radius> <arc>: puts one at the game master's feet,
// facing where they face, for 10 seconds. To see an indicator's size and how it lies on a floor without waiting
// for a creature to cast.
class GroundIndicatorCommandScript : public CommandScript
{
public:
    GroundIndicatorCommandScript() : CommandScript("GroundIndicatorCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable indicatorTable =
        {
            { "circle", HandleCircle, SEC_GAMEMASTER, Console::No },
            { "rect",   HandleRectangle, SEC_GAMEMASTER, Console::No },
            { "cone",   HandleCone, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "indicator", indicatorTable },
        };
        return commandTable;
    }

    static bool HandleCircle(ChatHandler* handler, float radius)
    {
        Player* player = handler->GetPlayer();
        GroundIndicators::ShowCircle(player, *player, radius, CommandDurationMs);
        return true;
    }

    static bool HandleRectangle(ChatHandler* handler, float length, float width)
    {
        Player* player = handler->GetPlayer();
        GroundIndicators::ShowRectangle(player, *player, player->GetOrientation(), length, width, CommandDurationMs);
        return true;
    }

    static bool HandleCone(ChatHandler* handler, float radius, float arcDegrees)
    {
        Player* player = handler->GetPlayer();
        GroundIndicators::ShowCone(player, *player, player->GetOrientation(), radius, arcDegrees, CommandDurationMs);
        return true;
    }

private:
    static constexpr uint32 CommandDurationMs = 10 * IN_MILLISECONDS;
};
}

void AddGroundIndicatorScripts()
{
    new GroundIndicatorSpellScript();
    new GroundIndicatorUnitScript();
    new GroundIndicatorCreatureScript();
    new GroundIndicatorDynamicObjectScript();
    new GroundIndicatorCommandScript();
}
