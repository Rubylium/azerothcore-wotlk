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
    float carriedBase = 0.0f;   // a carried circle's radius at its carrier's scale 1: it grows with the carrier
    GroundIndicators::Area area;
    uint64 endMs = 0;
};

// Maps update on several threads: every access to the registry holds the lock
std::mutex RegistryLock;
std::vector<ActiveArea> Registry;
uint64 NextAreaId = 0;

uint64 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

uint64 Register(Unit* owner, Unit* carrier, GroundIndicators::Area const& area, uint32 durationMs)
{
    ActiveArea active;
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
        [now](ActiveArea const& entry) { return entry.endMs <= now; }), Registry.end());
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

void Unregister(uint64 areaId)
{
    std::lock_guard<std::mutex> guard(RegistryLock);
    Registry.erase(std::remove_if(Registry.begin(), Registry.end(),
        [areaId](ActiveArea const& entry) { return entry.id == areaId; }), Registry.end());
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
    {
        if (entry.carrier.IsEmpty())
            continue;
        if (Unit* carrier = ObjectAccessor::GetUnit(*unit, entry.carrier))
        {
            entry.area.origin.Relocate(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ());
            if (entry.carriedBase > 0.0f)
                entry.area.radius = entry.carriedBase * carrier->GetObjectScale();
        }
        else
            entry.endMs = 0;
    }
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
// Emitters stand about this far apart
constexpr float EmitterSpacing = 6.0f;

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

Position OnGround(Unit* owner, float x, float y, float z)
{
    float ground = owner->GetMap()->GetHeight(owner->GetPhaseMask(), x, y, z + 6.0f, true, 30.0f);
    if (ground <= INVALID_HEIGHT)
        ground = z;
    return Position(x, y, ground);
}

void SpawnEmitter(Unit* owner, Position const& where, uint32 kit, uint32 intervalMs, uint32 durationMs)
{
    TempSummon* emitter = owner->SummonCreature(NPC_GROUND_INDICATOR, where, TEMPSUMMON_TIMED_DESPAWN, durationMs);
    if (!emitter)
        return;

    // Pulses spread over the interval, so an area shimmers rather than blinking all at once
    uint32 const first = 150 + (intervalMs ? urand(0, intervalMs) : 0);
    emitter->AIM_Initialize(new ParticleEmitterAI(emitter, kit, intervalMs, first));
}

// The spots an area's emitters stand on: spread over its shape about EmitterSpacing apart
std::vector<Position> EmitterSpots(Unit* owner, GroundIndicators::Area const& area)
{
    using Kind = GroundIndicators::Area::Kind;
    std::vector<Position> spots;
    Position const& origin = area.origin;
    float const z = origin.GetPositionZ();
    auto add = [&](float x, float y)
    {
        if (spots.size() < MaxEmitters)
            spots.push_back(OnGround(owner, x, y, z));
    };

    switch (area.kind)
    {
        case Kind::Circle:
        {
            add(origin.GetPositionX(), origin.GetPositionY());
            for (float share : { 0.55f, 0.9f })
            {
                float const ring = area.radius * share;
                if (ring < EmitterSpacing * 0.6f)
                    continue;
                uint32 const count = std::max<uint32>(4, uint32(2.0f * float(M_PI) * ring / EmitterSpacing));
                float const turn = share * 1.3f;
                for (uint32 index = 0; index < count; ++index)
                {
                    float const angle = turn + 2.0f * float(M_PI) * index / count;
                    add(origin.GetPositionX() + std::cos(angle) * ring, origin.GetPositionY() + std::sin(angle) * ring);
                }
            }
            break;
        }
        case Kind::Rectangle:
        {
            float const facing = origin.GetOrientation();
            std::vector<float> across = { 0.0f };
            if (area.width > EmitterSpacing * 1.2f)
                across = { -area.width / 4.0f, area.width / 4.0f };
            for (float along = EmitterSpacing / 2.0f; along < area.radius; along += EmitterSpacing)
                for (float side : across)
                    add(origin.GetPositionX() + std::cos(facing) * along - std::sin(facing) * side,
                        origin.GetPositionY() + std::sin(facing) * along + std::cos(facing) * side);
            break;
        }
        case Kind::Cone:
        {
            float const facing = origin.GetOrientation();
            for (float along = EmitterSpacing * 0.7f; along <= area.radius; along += EmitterSpacing)
            {
                float const span = area.arc * 0.8f;
                uint32 const count = std::max<uint32>(1, uint32(span * along / EmitterSpacing));
                for (uint32 index = 0; index < count; ++index)
                {
                    float const angle = count == 1 ? facing :
                        facing - span / 2.0f + span * float(index) / float(count - 1);
                    add(origin.GetPositionX() + std::cos(angle) * along, origin.GetPositionY() + std::sin(angle) * along);
                }
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

// Shows the area of a cast that starts now and lasts durationMs. A circle around the target follows it: the spell
// lands where the target is when it goes off.
void ShowSpellArea(Spell const* spell, Unit* caster, uint32 durationMs)
{
    SpellArea area;
    if (!FindArea(spell, caster, area))
        return;

    SpellInfo const* spellInfo = spell->GetSpellInfo();
    Unit* target = spell->m_targets.GetUnitTarget();

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
                durationMs) });
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
                MakeArea(GroundIndicators::Area::Kind::Circle, center, 0.0f, drawnRadius), durationMs) });
        return;
    }

    if (Creature* stalker = Place(caster, center, 0.0f, SPELL_INDICATOR_CIRCLE, area.radius, durationMs))
    {
        GroundIndicators::Area const circle = MakeArea(GroundIndicators::Area::Kind::Circle, center, 0.0f,
            area.radius);
        Remember(caster, spellInfo->Id, { stalker->GetGUID(), 0, Register(caster, nullptr, circle, durationMs) });
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

    for (Position const& spot : EmitterSpots(owner, area))
        SpawnEmitter(owner, spot, kit, ParticlePulseMs, durationMs);
}

void Burst(Unit* owner, Position const& where, Theme theme)
{
    uint32 const kit = KitsOf(theme).burst;
    if (!kit || !owner || !owner->IsInWorld())
        return;

    SpawnEmitter(owner, OnGround(owner, where.GetPositionX(), where.GetPositionY(), where.GetPositionZ()), kit, 0,
                 3000);
}

Area ShowCircle(Unit* owner, Position const& center, float radius, uint32 durationMs, Theme theme)
{
    Area area = MakeArea(Area::Kind::Circle, center, 0.0f, radius);
    if (Creature* stalker = Place(owner, center, 0.0f, SPELL_INDICATOR_CIRCLE, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowRectangle(Unit* owner, Position const& start, float orientation, float length, float width,
                   uint32 durationMs, Theme theme)
{
    width = std::max(width, 0.5f);
    ShapeSpell const& shape = NearestShape(RectangleSpells.data(), RectangleSpells.data() + RectangleSpells.size(),
        length / width);
    Area area = MakeArea(Area::Kind::Rectangle, start, orientation, length);
    area.width = length / shape.size;
    if (Creature* stalker = Place(owner, start, orientation, shape.spell, length, durationMs))
    {
        Register(owner, nullptr, area, durationMs);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowCone(Unit* owner, Position const& apex, float orientation, float radius, float arcDegrees,
              uint32 durationMs, Theme theme)
{
    ShapeSpell const& shape = NearestShape(ConeSpells.data(), ConeSpells.data() + ConeSpells.size(), arcDegrees);
    Area area = MakeArea(Area::Kind::Cone, apex, orientation, radius);
    area.arc = shape.size * float(M_PI) / 180.0f;
    if (Creature* stalker = Place(owner, apex, orientation, shape.spell, radius, durationMs))
    {
        Register(owner, nullptr, area, durationMs);
        ShowParticles(owner, area, theme, durationMs);
    }
    return area;
}

Area ShowCarriedCircle(Unit* owner, Unit* carrier, float radius, uint32 durationMs)
{
    float drawnRadius = radius;
    uint32 const aura = Carry(carrier, radius, durationMs, drawnRadius);
    Area area = MakeArea(Area::Kind::Circle, *carrier, 0.0f, drawnRadius);
    if (aura)
        Register(owner, carrier, area, durationMs);
    return area;
}

Area CurrentArea(Unit* carrier, Area const& area)
{
    Area current = area;
    if (carrier)
        current.origin.Relocate(carrier->GetPositionX(), carrier->GetPositionY(), carrier->GetPositionZ());
    return current;
}

bool FindEscape(Unit* unit, Position& escape, bool tank)
{
    if (!unit || !unit->IsInWorld() || !unit->IsAlive())
        return false;

    std::vector<ActiveArea> areas = AreasAround(unit);
    if (tank)
        areas.erase(std::remove_if(areas.begin(), areas.end(), [unit](ActiveArea const& entry)
            { return HeldByTank(unit, entry); }), areas.end());
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
            MakeArea(GroundIndicators::Area::Kind::Circle, *creature, 0.0f, drawnRadius), durationMs) });
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
        circle.areaId = Register(caster, nullptr, area, uint32(duration));
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
