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
#include <atomic>
#include <cmath>
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
constexpr uint32 EscapeDirections = 16;
// A carried circle is taken this far past its own radius from every other player
constexpr float CarrierClearance = 2.0f;
// Spreading out: each unit looks for its spot along its own directions and at its own distances (from its guid, so
// it keeps to them from one check to the next), and a spot someone already stands on costs more. Without it a whole
// raid in the same red area ran to the very same spot.
constexpr float EscapeDistanceSpread = 2.5f;
constexpr float CrowdRadius = 3.0f;
constexpr float CrowdCost = 4.0f;

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
    float carriedBase = 0.0f;   // a carried circle's radius at its carrier's scale 1: it grows with the carrier
    float sweepSpeed = 0.0f;    // a sweeping rectangle (ShowSweepingRectangle): radians a second, from sweepFromMs
    uint64 sweepFromMs = 0;
    float sweepFrom = 0.0f;     // ... its facing then
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
    uint32 hitDamage = 0, ObjectGuid aimedAt = ObjectGuid::Empty, ObjectGuid turnsTo = ObjectGuid::Empty)
{
    ActiveArea active;
    active.hitDamage = hitDamage;
    active.aimedAt = aimedAt;
    active.turnsTo = turnsTo;
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
            entry.area.origin.SetOrientation(entry.area.origin.GetAngle(turnsTo->GetPositionX(),
                turnsTo->GetPositionY()));
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
    TrackingConeAI(Creature* creature, ObjectGuid turnsTo) : NullCreatureAI(creature), _turnsTo(turnsTo) { }

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
        float const facing = me->GetAngle(turnsTo);
        float turn = std::fabs(facing - me->GetOrientation());
        turn = std::min(turn, 2.0f * float(M_PI) - turn);
        if (turn > MinTurn)
            me->SetFacingTo(facing);
    }

private:
    static constexpr uint32 TurnEveryMs = 100;
    static constexpr float MinTurn = 0.02f;

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

// The stalker that shows spellId at a size of scale yards; it goes away by itself after durationMs
Creature* Place(Unit* owner, Position const& position, float orientation, uint32 spellId, float scale,
                uint32 durationMs)
{
    if (!owner || !owner->IsInWorld() || durationMs == 0)
        return nullptr;

    Position placed(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(), orientation);
    TempSummon* stalker = owner->SummonCreature(NPC_GROUND_INDICATOR, placed, TEMPSUMMON_TIMED_DESPAWN, durationMs);
    if (!stalker)
        return nullptr;

    stalker->SetObjectScale(scale);
    stalker->AddAura(spellId, stalker);
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
    if (!owner || !owner->IsAlive() || owner->GetVictim() != tank || owner->IsDungeonBoss() || owner->isWorldBoss())
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
        OffTank
    };

    Kind kind = Kind::Soak;
    uint32 mapId = 0;
    uint32 instanceId = 0;
    ObjectGuid owner;
    Position center;
    float radius = 0.0f;
    uint32 wanted = 0;
    uint64 endMs = 0;
};

std::mutex GoalLock;
std::vector<Goal> Goals;

// A soak is looked at by the players this close to it; a player this far inside its edge stands in it
constexpr float SoakReach = 50.0f;
constexpr float SoakInsideMargin = 1.0f;
// The off-tank goes back to its spot once this far from it
constexpr float OffTankSlack = 3.0f;

void AddGoal(Goal goal, bool replaceOwnersOfKind)
{
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(GoalLock);
    Goals.erase(std::remove_if(Goals.begin(), Goals.end(), [&goal, now, replaceOwnersOfKind](Goal const& entry)
        {
            return entry.endMs <= now ||
                (replaceOwnersOfKind && entry.owner == goal.owner && entry.kind == goal.kind);
        }), Goals.end());
    Goals.push_back(goal);
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
            if (distance > radius + margin)
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

Area ShowSweepingRectangle(Unit* owner, Position const& start, float orientation, float radiansPerSecond, float length,
                           float width, uint32 durationMs, uint32 hitDamage)
{
    width = std::max(width, 0.5f);
    ShapeSpell const& shape = NearestShape(RectangleSpells.data(), RectangleSpells.data() + RectangleSpells.size(),
        length / width);
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = length / shape.size;
    if (Creature* stalker = Place(owner, start, orientation, shape.spell, length, durationMs))
    {
        stalker->AIM_Initialize(new SweepAI(stalker, orientation, radiansPerSecond));
        uint64 const id = Register(owner, nullptr, area, durationMs, hitDamage);
        std::lock_guard<std::mutex> guard(RegistryLock);
        for (ActiveArea& entry : Registry)
            if (entry.id == id)
            {
                entry.sweepSpeed = radiansPerSecond;
                entry.sweepFromMs = NowMs();
                entry.sweepFrom = orientation;
            }
    }
    return area;
}

Area CurrentSweep(Area const& area, float radiansPerSecond, uint32 elapsedMs)
{
    Area current = area;
    current.origin.SetOrientation(Position::NormalizeOrientation(area.origin.GetOrientation() +
        radiansPerSecond * float(elapsedMs) / 1000.0f));
    return current;
}

Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
              uint32 durationMs, Theme theme, uint32 hitDamage)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    Area area = MakeArea(Area::Kind::Cone, apex, orientation, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    if (Creature* stalker = Place(owner, apex, orientation, shape.spell, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowAimedCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
                   uint32 durationMs, Unit* aimedAt, Theme theme, uint32 hitDamage)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    Area area = MakeArea(Area::Kind::Cone, apex, orientation, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    if (Creature* stalker = Place(owner, apex, orientation, shape.spell, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs, hitDamage, aimedAt ? aimedAt->GetGUID() : ObjectGuid::Empty);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowTrackingCone(Unit* owner, Position const& apex, float radius, float arcDegrees, uint32 durationMs,
                      Unit* aimedAt, uint32 hitDamage)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    float const facing = apex.GetAngle(aimedAt->GetPositionX(), aimedAt->GetPositionY());
    Area area = MakeArea(Area::Kind::Cone, apex, facing, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    if (Creature* stalker = Place(owner, apex, facing, shape.spell, radius, durationMs))
    {
        stalker->AIM_Initialize(new TrackingConeAI(stalker, aimedAt->GetGUID()));
        Register(owner, nullptr, area, durationMs, hitDamage, aimedAt->GetGUID(), aimedAt->GetGUID());
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

    ObjectGuid const guid = owner->GetGUID();
    uint64 const now = NowMs();
    std::lock_guard<std::mutex> guard(RegistryLock);
    for (ActiveArea& entry : Registry)
        if (entry.owner == guid)
            entry.endMs = std::min(entry.endMs, now);
}

Area ShowCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs, uint32 hitDamage)
{
    float drawnRadius = radius;
    uint32 const aura = Carry(carrier, radius, durationMs, drawnRadius);
    Area area = MakeArea(Area::Kind::Circle, *carrier, 0.0f, drawnRadius);
    if (aura)
        Register(owner, carrier, area, durationMs, hitDamage);
    return area;
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

void WatchArea(Unit* owner, Area const& area, uint32 durationMs, uint32 hitDamage)
{
    if (owner && owner->IsInWorld() && durationMs)
        Register(owner, nullptr, area, durationMs, hitDamage);
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

bool FindEscape(Unit* unit, Position& escape, bool tank)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    std::vector<ActiveArea> areas = AreasAround(unit);
    if (tank)
        areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
            { return HeldByTank(unit, entry); }), areas.end());
    // A cone aimed at it lands on it wherever it goes: it holds its ground (a tank keeps it pointed away)
    areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
        { return entry.aimedAt == unit->GetGUID(); }), areas.end());
    // What would not come close to killing it is not worth giving up the fight for: stood in (SurvivableHit)
    areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
        { return SurvivableHit(unit, entry); }), areas.end());
    if (areas.empty())
        return false;

    // A circle this unit carries: it is the others who must not be in it. A tank does not run from its group
    // with one; they step away from it.
    float carried = 0.0f;
    if (!tank)
        for (ActiveArea const& entry : areas)
            if (entry.carrier == unit->GetGUID())
                carried = std::max(carried, entry.area.radius);

    Position const here = unit->GetPosition();
    bool const inside = InAnyArea(areas, here, unit->GetGUID(), InsideMargin);
    bool const crowding = carried > 0.0f && OtherPlayerNear(unit, here, carried + InsideMargin);
    if (!inside && !crowding)
        return false;

    Unit* victim = unit->GetVictim();
    float const angleOffset = UnitSpread(unit, 1) * 2.0f * float(M_PI) / EscapeDirections;
    float const distanceOffset = UnitSpread(unit, 2) * EscapeDistanceSpread;
    bool found = false;
    float foundRing = 0.0f;
    Candidate best;
    for (float ring = EscapeStep; ring <= EscapeReach; ring += EscapeStep)
    {
        float const distance = ring + distanceOffset;
        for (uint32 direction = 0; direction < EscapeDirections; ++direction)
        {
            float const angle = angleOffset + 2.0f * float(M_PI) * direction / EscapeDirections;
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

            // The shortest way out, and not too far from what it is fighting
            float cost = here.GetExactDist2d(&spot);
            if (victim)
                cost += std::max(0.0f, spot.GetExactDist2d(victim) - here.GetExactDist2d(victim)) * 0.5f;
            cost += CrowdCost * PlayersNear(unit, spot, CrowdRadius);
            if (!found || cost < best.cost)
            {
                if (!found)
                    foundRing = ring;
                best.spot = spot;
                best.cost = cost;
                found = true;
            }
        }

        // The nearest ring with a way out, and the one after it (an empty spot a step further beats a crowded one),
        // are enough: any further only costs more
        if (found && ring >= foundRing + EscapeStep)
            break;
    }

    if (found)
        escape = best.spot;
    return found;
}

void ShowSoak(Unit* owner, Position const& center, float radius, uint32 durationMs, uint32 wanted)
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
    AddGoal(goal, false);
    ShowParticles(owner, MakeArea(Area::Kind::Circle, center, 0.0f, radius), Theme::Holy, durationMs);
}

void SetOffTankSpot(Unit* owner, Position const& spot, uint32 durationMs)
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
    AddGoal(goal, true);
}

bool FindGoal(Unit* unit, Position& spot, bool tank)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    for (Goal const& goal : GoalsAround(unit))
    {
        Unit* owner = ObjectAccessor::GetUnit(*unit, goal.owner);
        if (!owner || !owner->IsAlive())
            continue;

        if (goal.kind == Goal::Kind::OffTank)
        {
            // Only a tank the owner is not hitting: the one it is stays where it holds it
            if (!tank || !owner->IsInCombat() || owner->GetVictim() == unit ||
                unit->GetExactDist2d(&goal.center) <= OffTankSlack)
                continue;
            spot = goal.center;
            return true;
        }

        // A soak: the players nearest to it go, until `wanted` stand in it. The owner's target (its tank) and
        // tanks stay on the boss; a player already in it stays there.
        if (tank || owner->GetVictim() == unit)
            continue;
        float const inside = std::max(goal.radius - SoakInsideMargin, 0.5f);
        if (unit->GetExactDist2d(&goal.center) <= inside)
            continue;

        uint32 standing = 0;
        std::vector<std::pair<float, ObjectGuid>> candidates;
        for (auto const& ref : unit->GetMap()->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player || !player->IsAlive() || player->IsGameMaster() || player == owner->GetVictim())
                continue;
            float const distance = player->GetExactDist2d(&goal.center);
            if (distance <= inside)
                ++standing;
            else if (distance <= SoakReach)
                candidates.emplace_back(distance, player->GetGUID());
        }
        if (standing >= goal.wanted)
            continue;

        std::sort(candidates.begin(), candidates.end(), [](auto const& left, auto const& right)
            { return left.first < right.first; });
        uint32 const missing = goal.wanted - standing;
        bool chosen = false;
        for (std::size_t index = 0; index < candidates.size() && index < missing; ++index)
            if (candidates[index].second == unit->GetGUID())
                chosen = true;
        if (!chosen)
            continue;

        // Each to a spot of its own inside it, not all onto its very middle
        float const angle = UnitSpread(unit, 3) * 2.0f * float(M_PI);
        float const distance = inside * 0.5f * UnitSpread(unit, 4);
        spot.Relocate(goal.center.GetPositionX() + std::cos(angle) * distance,
            goal.center.GetPositionY() + std::sin(angle) * distance, goal.center.GetPositionZ());
        return true;
    }
    return false;
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
