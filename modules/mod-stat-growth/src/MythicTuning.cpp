#include "MythicTuning.h"

#include "Creature.h"
#include "CreatureScript.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "MythicDungeonSystem.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
constexpr float ReferenceHealthAtTen = 90000.0f;
constexpr int32 ReferenceKey = 10;

std::unordered_map<uint32, float> SpellMultipliers;
std::unordered_map<uint32, float> MeleeMultipliers;

std::vector<MythicTrash::Ability> TrashAbilities;
std::unordered_map<uint32, std::vector<std::size_t>> TrashByEntry;

// A trash ability starts at most this often per instance, whatever the size of the pull
constexpr uint32 TrashGlobalGapMs = 1500;
// Players further than this from the creature are not looked at when an ability lands
constexpr float TrashReach = 60.0f;
constexpr float TrashTargetRange = 30.0f;

std::mutex InstanceGapLock;
std::unordered_map<uint32, uint32> NextTrashStart;      // instance id -> getMSTime

int32 KeyOf(Unit const* unit)
{
    Map const* map = unit ? unit->FindMap() : nullptr;
    return map ? std::max(map->GetMythicLevel(), 0) : 0;
}

struct TrashState : public DataMap::Base
{
    std::vector<uint32> readyAt;            // per registered ability of the entry, getMSTime
    bool inCombat = false;

    // The ability being shown
    bool pending = false;
    std::size_t ability = 0;
    uint32 resolveAt = 0;
    GroundIndicators::Area area;
    ObjectGuid carrier;
    bool rooted = false;
};

bool TryClaimInstanceSlot(Creature* creature, uint32 now)
{
    std::lock_guard<std::mutex> guard(InstanceGapLock);
    uint32& next = NextTrashStart[creature->GetInstanceId()];
    if (next && now < next && next - now < 60000)
        return false;
    next = now + TrashGlobalGapMs;
    return true;
}

std::vector<Player*> PlayersNear(Creature* creature, float range)
{
    std::vector<Player*> players;
    for (auto const& ref : creature->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->IsAlive() && !player->IsGameMaster() && creature->IsWithinDistInMap(player, range) &&
                creature->IsValidAttackTarget(player))
                players.push_back(player);
    return players;
}

void Release(Creature* creature, TrashState* state)
{
    if (state->rooted)
    {
        creature->SetControlled(false, UNIT_STATE_ROOT);
        state->rooted = false;
    }
    state->pending = false;
}

bool Begin(Creature* creature, TrashState* state, std::size_t index)
{
    MythicTrash::Ability const& ability = TrashAbilities[index];
    Unit* victim = creature->GetVictim();
    GroundIndicators::Area area;
    ObjectGuid carrier;

    switch (ability.shape)
    {
        case MythicTrash::Shape::AroundSelf:
            area = GroundIndicators::ShowCircle(creature, creature->GetPosition(), ability.size, ability.warnMs,
                ability.theme);
            break;
        case MythicTrash::Shape::UnderTarget:
        case MythicTrash::Shape::CarriedByTarget:
        {
            std::vector<Player*> players = PlayersNear(creature, TrashTargetRange);
            if (players.empty())
                return false;
            Player* target = Acore::Containers::SelectRandomContainerElement(players);
            if (ability.shape == MythicTrash::Shape::UnderTarget)
                area = GroundIndicators::ShowCircle(creature, target->GetPosition(), ability.size, ability.warnMs,
                    ability.theme);
            else
            {
                area = GroundIndicators::ShowCarriedCircle(creature, target, ability.size, ability.warnMs);
                carrier = target->GetGUID();
            }
            break;
        }
        case MythicTrash::Shape::ConeAtVictim:
        case MythicTrash::Shape::LineAtVictim:
        {
            if (!victim)
                return false;
            float const facing = creature->GetAngle(victim);
            creature->SetFacingTo(facing);
            if (ability.shape == MythicTrash::Shape::ConeAtVictim)
                area = GroundIndicators::ShowCone(creature, creature->GetPosition(), facing, ability.size,
                    ability.width, ability.warnMs, ability.theme);
            else
                area = GroundIndicators::ShowRectangle(creature, creature->GetPosition(), facing, ability.size,
                    ability.width, ability.warnMs, ability.theme);
            break;
        }
    }

    state->pending = true;
    state->ability = index;
    state->area = area;
    state->carrier = carrier;
    state->resolveAt = getMSTime() + ability.warnMs;
    if (ability.holdStill)
    {
        creature->SetControlled(true, UNIT_STATE_ROOT);
        state->rooted = true;
    }
    return true;
}

void Resolve(Creature* creature, TrashState* state)
{
    MythicTrash::Ability const& ability = TrashAbilities[state->ability];
    GroundIndicators::Area area = state->area;
    Unit* carrier = nullptr;
    if (!state->carrier.IsEmpty())
    {
        carrier = ObjectAccessor::GetUnit(*creature, state->carrier);
        if (!carrier || !carrier->IsAlive())
        {
            Release(creature, state);
            return;
        }
        area = GroundIndicators::CurrentArea(carrier, area);
    }
    Release(creature, state);

    GroundIndicators::Burst(creature, area.origin, ability.theme);
    for (Player* player : PlayersNear(creature, TrashReach))
        if (player != carrier && area.Contains(player->GetPosition()))
            MythicTuning::DealReferenceDamage(creature, player, ability.spellId, ability.percent);
}

class MythicTrashCreatureScript : public AllCreatureScript
{
public:
    MythicTrashCreatureScript() : AllCreatureScript("MythicTrashCreatureScript") { }

    void OnAllCreatureUpdate(Creature* creature, uint32 /*diff*/) override
    {
        if (TrashByEntry.empty())
            return;
        auto const entry = TrashByEntry.find(creature->GetEntry());
        if (entry == TrashByEntry.end())
            return;
        Map* map = creature->FindMap();
        if (!map || !map->IsMythic())
            return;

        TrashState* state = creature->CustomData.GetDefault<TrashState>("MythicTrash");
        uint32 const now = getMSTime();

        if (!creature->IsAlive() || !creature->IsInCombat())
        {
            if (state->pending)
                Release(creature, state);
            state->inCombat = false;
            return;
        }

        if (state->pending)
        {
            if (now >= state->resolveAt)
                Resolve(creature, state);
            return;
        }

        std::vector<std::size_t> const& indices = entry->second;
        if (!state->inCombat || state->readyAt.size() != indices.size())
        {
            state->inCombat = true;
            state->readyAt.assign(indices.size(), 0);
            for (std::size_t i = 0; i < indices.size(); ++i)
            {
                uint32 const first = TrashAbilities[indices[i]].firstMs;
                state->readyAt[i] = now + first + urand(0, first / 3);
            }
            return;
        }

        // Not while casting its own spells, or while it cannot act
        if (creature->HasUnitState(UNIT_STATE_CASTING) || creature->HasUnitState(UNIT_STATE_CONTROLLED) ||
            !creature->GetVictim())
            return;

        for (std::size_t i = 0; i < indices.size(); ++i)
        {
            if (now < state->readyAt[i])
                continue;
            if (!TryClaimInstanceSlot(creature, now))
                return;
            MythicTrash::Ability const& ability = TrashAbilities[indices[i]];
            state->readyAt[i] = now + ability.cooldownMs + urand(0, ability.cooldownMs / 5);
            if (Begin(creature, state, indices[i]))
                return;
        }
    }
};
}

namespace MythicTuning
{
void SetSpellMultiplier(uint32 spellId, float multiplier)
{
    SpellMultipliers[spellId] = multiplier;
}

float SpellMultiplier(SpellInfo const* spellInfo)
{
    if (!spellInfo || SpellMultipliers.empty())
        return 1.0f;
    auto const itr = SpellMultipliers.find(spellInfo->Id);
    return itr == SpellMultipliers.end() ? 1.0f : itr->second;
}

void SetMeleeMultiplier(uint32 creatureEntry, float multiplier)
{
    MeleeMultipliers[creatureEntry] = multiplier;
}

float MeleeMultiplier(uint32 creatureEntry)
{
    auto const itr = MeleeMultipliers.find(creatureEntry);
    return itr == MeleeMultipliers.end() ? 1.0f : itr->second;
}

float ReferenceHealth(int32 keyLevel)
{
    return ReferenceHealthAtTen * Mythic::GetLevelScaling(std::max(keyLevel, 0)) /
        Mythic::GetLevelScaling(ReferenceKey);
}

float ReferenceHealth(Unit const* caster)
{
    return ReferenceHealth(KeyOf(caster));
}

void DealAbilityDamage(Unit* caster, Unit* target, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!caster || !target || !spellInfo || !target->IsAlive() || !amount)
        return;

    // The target's damage-taken auras (Shield Wall, Pain Suppression, ...) apply before the key's scaling does
    uint32 const damage = target->SpellDamageBonusTaken(caster, spellInfo, amount, SPELL_DIRECT_DAMAGE);
    SpellNonMeleeDamage damageInfo(caster, target, spellInfo, spellInfo->GetSchoolMask());
    caster->CalculateSpellDamageTaken(&damageInfo, int32(damage), spellInfo);
    Unit::DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);
    caster->SendSpellNonMeleeDamageLog(&damageInfo);
    caster->DealSpellDamage(&damageInfo, true);
}

void DealReferenceDamage(Unit* caster, Unit* target, uint32 spellId, float percent)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!caster || !target || !spellInfo || percent <= 0.0f)
        return;

    // The key's scaling is applied on the way (MythicDungeonSystem's damage hook): take it out beforehand, so what
    // lands is the share asked for
    float const factor = std::max(GetMythicSpellFactor(caster, spellInfo), 0.01f);
    float const wanted = ReferenceHealth(caster) * percent / 100.0f;
    DealAbilityDamage(caster, target, spellId, static_cast<uint32>(std::max(1.0f, wanted / factor)));
}
}

namespace MythicTrash
{
void Register(Ability const& ability)
{
    TrashByEntry[ability.entry].push_back(TrashAbilities.size());
    TrashAbilities.push_back(ability);
}
}

void AddMythicTuningScripts()
{
    new MythicTrashCreatureScript();
}
