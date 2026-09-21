#include "Necromancer.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace Necromancer;

namespace
{
std::mutex minionMutex;
std::unordered_map<ObjectGuid, std::deque<ObjectGuid>> ownerMinions;

uint32 GetEntry(MinionKind kind)
{
    switch (kind)
    {
        case MinionKind::Skeleton: return NPC_SKELETON_WARRIOR;
        case MinionKind::Archer: return NPC_CURSED_ARCHER;
        case MinionKind::Mage: return NPC_PLAGUE_MAGE;
        case MinionKind::Abomination: return NPC_ABOMINATION;
    }
    return NPC_SKELETON_WARRIOR;
}

bool IsOrdinary(Creature const* creature)
{
    return creature && creature->GetEntry() != NPC_ABOMINATION;
}

MinionKind GetKind(Creature const* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_CURSED_ARCHER: return MinionKind::Archer;
        case NPC_PLAGUE_MAGE: return MinionKind::Mage;
        case NPC_ABOMINATION: return MinionKind::Abomination;
        default: return MinionKind::Skeleton;
    }
}

std::vector<Creature*> ResolveMinions(Player* owner)
{
    std::vector<ObjectGuid> guids;
    {
        std::lock_guard<std::mutex> lock(minionMutex);
        auto itr = ownerMinions.find(owner->GetGUID());
        if (itr != ownerMinions.end())
            guids.assign(itr->second.begin(), itr->second.end());
    }

    std::vector<Creature*> minions;
    std::deque<ObjectGuid> alive;
    for (ObjectGuid guid : guids)
        if (Creature* creature = ObjectAccessor::GetCreature(*owner, guid))
        {
            minions.push_back(creature);
            alive.push_back(guid);
        }

    // A minion is only struck off the list when it dies; one that ran out of time, was sacrificed or was
    // evicted by the army cap stayed on it and counted against the cap forever. The list is what is left.
    if (alive.size() != guids.size())
    {
        std::lock_guard<std::mutex> lock(minionMutex);
        auto itr = ownerMinions.find(owner->GetGUID());
        if (itr != ownerMinions.end())
            itr->second = std::move(alive);
    }
    return minions;
}

uint32 GetArmyCap(Player const* owner)
{
    return 8u + uint32(GetTalentValue(owner, { { 90537, 1 }, { 90538, 2 } }));
}

// What makes a raised creature the Nécromancien's, and able to fight at all.
//
// Its creature template is faction 35, friendly to everyone: on that faction IsValidAttackTarget refuses every
// enemy in both directions (Unit.cpp, the creature-versus-creature rule), so the minions were summoned, followed
// their master and never struck anything. The owner's faction is what makes an enemy an enemy.
//
// UNIT_FLAG_PLAYER_CONTROLLED is the other half: without it the same rule treats the minion as a wild creature,
// which cannot touch anything flagged immune to NPCs -- a training dummy among them -- and the pack would answer
// to it as a monster rather than as the player's. Aggressive so it defends itself when its master is jumped.
void BindToOwner(Creature* minion, Player* owner)
{
    if (!minion || !owner)
        return;

    minion->SetFaction(owner->GetFaction());
    minion->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    minion->SetReactState(REACT_AGGRESSIVE);
}

class NecromancerMinionAI : public ScriptedAI
{
public:
    NecromancerMinionAI(Creature* creature, MinionKind kind) : ScriptedAI(creature), kind(kind) { }

    void InitializeAI() override
    {
        ScriptedAI::InitializeAI();
        if (Player* owner = GetOwner())
        {
            BindToOwner(me, owner);
            Scale(owner);
            me->GetMotionMaster()->MoveFollow(owner, FollowDistance(), frand(0.0f, float(M_PI * 2.0)),
                MOTION_SLOT_ACTIVE);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* owner = GetOwner())
            RemoveMinion(owner->GetGUID(), me->GetGUID());
    }

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* owner = summoner ? summoner->ToPlayer() : nullptr)
        {
            // The summoner is only certain to be resolvable here, after the minion has joined the map
            BindToOwner(me, owner);
            Scale(owner);
        }
    }

    // A minion that is being beaten on fights back: ScriptedAI does nothing with this by itself
    void AttackedBy(Unit* attacker) override
    {
        if (!me->GetVictim() && attacker && me->IsValidAttackTarget(attacker))
            AttackStart(attacker);
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type == 1)
            commandTimer = value;
        else if (type == 2)
            masterTimer = value;
    }

    void AttackStart(Unit* target) override
    {
        if (!target)
            return;
        if (kind == MinionKind::Archer || kind == MinionKind::Mage)
            ScriptedAI::AttackStartCaster(target, kind == MinionKind::Mage ? 22.0f : 26.0f);
        else
            ScriptedAI::AttackStart(target);
    }

    void UpdateAI(uint32 diff) override
    {
        Player* owner = GetOwner();
        if (!owner || !owner->IsAlive())
        {
            if (TempSummon* summon = me->ToTempSummon())
                summon->UnSummon();
            return;
        }

        commandTimer = commandTimer > diff ? commandTimer - diff : 0;
        masterTimer = masterTimer > diff ? masterTimer - diff : 0;
        scaleTimer = scaleTimer > diff ? scaleTimer - diff : 0;
        attackTimer = attackTimer > diff ? attackTimer - diff : 0;

        if (me->GetDistance(owner) > 45.0f)
        {
            Position position = owner->GetNearPosition(2.0f, frand(0.0f, float(M_PI * 2.0)));
            me->NearTeleportTo(position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(),
                owner->GetOrientation());
            me->CombatStop(true);
        }

        if (!scaleTimer)
        {
            Scale(owner);
            scaleTimer = 2000;
        }

        Unit* target = me->GetVictim();
        if (target && (!target->IsAlive() || !me->IsValidAttackTarget(target)))
        {
            me->AttackStop();
            me->CombatStop(true);
            me->GetMotionMaster()->Clear();
            target = nullptr;
        }

        // What the master is fighting, what it has selected, and failing both, whoever is hitting the master:
        // a Nécromancien casts rather than swings, so it often has no victim of its own, and a minion that
        // waited for one stood by while its master was beaten
        Unit* const candidates[] = { owner->GetVictim(), owner->GetSelectedUnit(), owner->getAttackerForHelper() };
        Unit* preferred = nullptr;
        for (Unit* candidate : candidates)
            if (candidate && candidate->IsAlive() && me->IsValidAttackTarget(candidate))
            {
                preferred = candidate;
                break;
            }

        if (preferred && preferred != target)
        {
            me->AttackStop();
            me->GetMotionMaster()->Clear();
            AttackStart(preferred);
            target = preferred;
        }

        if (!target)
        {
            if (!me->HasUnitState(UNIT_STATE_FOLLOW))
                me->GetMotionMaster()->MoveFollow(owner, FollowDistance(), frand(0.0f, float(M_PI * 2.0)),
                    MOTION_SLOT_ACTIVE);
            return;
        }

        if (kind == MinionKind::Archer || kind == MinionKind::Mage)
        {
            if (!attackTimer && me->IsWithinLOSInMap(target))
            {
                CastRanged(owner, target);
                attackTimer = kind == MinionKind::Mage ? 2500 : 1900;
            }
            return;
        }

        bool const swinging = me->isAttackReady() && me->IsWithinMeleeRange(target);
        if (swinging && owner->HasAura(SPELL_FRENZIED_LEGION) && roll_chance_i(20))
            RestoreMana(owner, 1);
        DoMeleeAttackIfReady();
        if (swinging)
            ApplyNecroticRot(owner, target);
    }

private:
    Player* GetOwner() const
    {
        TempSummon* summon = me->ToTempSummon();
        return summon && summon->GetSummonerUnit() ? summon->GetSummonerUnit()->ToPlayer() : nullptr;
    }

    // The ones that shoot keep their distance; the rest crowd their master. Every follow uses this, so a
    // shooter no longer ends up at melee range after its first target dies.
    float FollowDistance() const
    {
        return kind == MinionKind::Archer || kind == MinionKind::Mage ? 6.0f : 2.0f;
    }

    float ActiveMultiplier(Player const* owner) const
    {
        float multiplier = GetMinionDamageMultiplier(owner, kind);
        if (commandTimer)
            multiplier *= 1.25f + float(GetTalentValue(owner, { { 90519, 5 }, { 90520, 10 }, { 90521, 15 } })) / 100.0f;
        if (masterTimer)
            multiplier *= 1.50f;
        return multiplier;
    }

    void Scale(Player* owner)
    {
        me->SetLevel(owner->GetLevel());
        float const spellPower = float(std::max<int32>(0, owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)));
        float health = (180.0f + float(owner->GetLevel()) * 42.0f + float(owner->GetMaxHealth()) * 0.22f)
            * GetMinionHealthMultiplier(owner, kind);
        if (kind == MinionKind::Abomination)
            health *= 2.4f;
        me->SetMaxHealth(uint32(health));
        if (me->GetHealth() > me->GetMaxHealth() || me->GetHealth() == 0)
            me->SetHealth(me->GetMaxHealth());
        me->SetArmor(int32(40 + owner->GetLevel() * 35));

        float baseDamage = (4.0f + float(owner->GetLevel()) * 1.45f + spellPower * 0.10f)
            * ActiveMultiplier(owner) * DAMAGE_SCALE;
        if (kind == MinionKind::Abomination)
            baseDamage *= 1.8f;
        me->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, baseDamage * 0.85f);
        me->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, baseDamage * 1.15f);
        me->UpdateDamagePhysical(BASE_ATTACK);
        me->SetAttackTime(BASE_ATTACK, owner->HasAura(SPELL_FRENZIED_LEGION) ? 1400 : 2000);
    }

    void CastRanged(Player* owner, Unit* target)
    {
        float const spellPower = float(std::max<int32>(0, owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)));
        int32 damage = int32((6.0f + owner->GetLevel() * 2.0f + spellPower * 0.16f)
            * ActiveMultiplier(owner) * DAMAGE_SCALE);
        if (target->HasAura(SPELL_DEATHLY_BRAND, owner->GetGUID()))
            damage = int32(float(damage) * 1.15f);

        if (kind == MinionKind::Mage)
        {
            // The plague mage is where the rot spreads fastest: its salvo rots every enemy it splashes.
            for (Unit* enemy : GetEnemiesAround(owner, target, 8.0f, 4))
            {
                me->CastCustomSpell(SPELL_PLAGUE_VOLLEY, SPELLVALUE_BASE_POINT0, damage, enemy,
                    TRIGGERED_FULL_MASK, nullptr, nullptr, owner->GetGUID());
                ApplyNecroticRot(owner, enemy);
            }
        }
        else
        {
            me->CastCustomSpell(SPELL_SPECTRAL_BOLT, SPELLVALUE_BASE_POINT0, damage, target,
                TRIGGERED_FULL_MASK, nullptr, nullptr, owner->GetGUID());
            ApplyNecroticRot(owner, target);
        }

        if ((owner->HasAura(SPELL_FRENZIED_LEGION) || masterTimer) && roll_chance_i(20))
            RestoreMana(owner, 1);
    }

    MinionKind kind;
    uint32 commandTimer = 0;
    uint32 masterTimer = 0;
    uint32 scaleTimer = 0;
    uint32 attackTimer = 500;
};

template<MinionKind Kind>
class NecromancerMinionScript : public CreatureScript
{
public:
    explicit NecromancerMinionScript(char const* name) : CreatureScript(name) { }
    CreatureAI* GetAI(Creature* creature) const override { return new NecromancerMinionAI(creature, Kind); }
};
}

namespace Necromancer
{
TempSummon* SummonMinion(Player* owner, MinionKind kind, uint32 durationMs)
{
    if (!IsNecromancer(owner))
        return nullptr;

    std::vector<Creature*> minions = ResolveMinions(owner);
    if (kind == MinionKind::Abomination)
    {
        for (Creature* minion : minions)
            if (minion->GetEntry() == NPC_ABOMINATION)
                if (TempSummon* summon = minion->ToTempSummon())
                    summon->UnSummon();
    }
    else
    {
        uint32 ordinary = std::count_if(minions.begin(), minions.end(), IsOrdinary);
        if (ordinary >= GetArmyCap(owner))
            for (Creature* minion : minions)
                if (IsOrdinary(minion))
                {
                    if (TempSummon* summon = minion->ToTempSummon())
                        summon->UnSummon();
                    break;
                }
    }

    uint32 extension = uint32(GetTalentValue(owner, { { 90505, 2 }, { 90506, 4 }, { 90507, 6 } }) +
        GetTalentValue(owner, { { 90522, 2 }, { 90523, 4 }, { 90524, 6 } })) * IN_MILLISECONDS;
    if (!durationMs)
        durationMs = (kind == MinionKind::Abomination ? 35 : 30) * IN_MILLISECONDS;
    Position position = owner->GetNearPosition(frand(1.5f, 3.0f), frand(0.0f, float(M_PI * 2.0)));
    TempSummon* summon = owner->SummonCreature(GetEntry(kind), position, TEMPSUMMON_TIMED_OR_DEAD_DESPAWN,
        durationMs + extension);
    if (!summon)
        return nullptr;

    summon->SetOwnerGUID(owner->GetGUID());
    summon->SetCreatorGUID(owner->GetGUID());
    BindToOwner(summon, owner);
    {
        std::lock_guard<std::mutex> lock(minionMutex);
        ownerMinions[owner->GetGUID()].push_back(summon->GetGUID());
    }
    if (Unit* target = owner->GetSelectedUnit())
        if (owner->IsValidAttackTarget(target))
            summon->AI()->AttackStart(target);
    return summon;
}

uint32 CountMinions(Player* owner) { return owner ? uint32(ResolveMinions(owner).size()) : 0; }

void CommandMinions(Player* owner, Unit* target, uint32 durationMs)
{
    for (Creature* minion : ResolveMinions(owner))
    {
        minion->SetHealth(std::min<uint32>(minion->GetMaxHealth(),
            minion->GetHealth() + CalculatePct(minion->GetMaxHealth(), 30)));
        minion->AI()->SetData(1, durationMs);
        if (target && minion->IsValidAttackTarget(target))
            minion->AI()->AttackStart(target);
    }
}

void DirectMinionsAt(Player* owner, Unit* target)
{
    if (!owner || !target || !target->IsAlive())
        return;

    for (Creature* minion : ResolveMinions(owner))
        if (minion->IsValidAttackTarget(target) && minion->GetVictim() != target)
        {
            minion->AttackStop();
            minion->GetMotionMaster()->Clear();
            minion->AI()->AttackStart(target);
        }
}

bool SacrificeOldestMinion(Player* owner)
{
    for (Creature* minion : ResolveMinions(owner))
        if (IsOrdinary(minion))
        {
            if (TempSummon* summon = minion->ToTempSummon())
                summon->UnSummon();
            return true;
        }
    return false;
}

void ExtendMinionDurations(Player* owner, uint32 durationMs)
{
    for (Creature* minion : ResolveMinions(owner))
        if (TempSummon* summon = minion->ToTempSummon())
            summon->SetTimer(summon->GetTimer() + durationMs);
}

void RefreshMinionDurations(Player* owner)
{
    for (Creature* minion : ResolveMinions(owner))
        if (TempSummon* summon = minion->ToTempSummon())
        {
            summon->SetTimer(std::max<uint32>(summon->GetTimer(), 30000));
            summon->AI()->SetData(2, 20000);
        }
}

void CleanupMinions(Player* owner)
{
    if (!owner)
        return;
    for (Creature* minion : ResolveMinions(owner))
        if (TempSummon* summon = minion->ToTempSummon())
            summon->UnSummon();
    std::lock_guard<std::mutex> lock(minionMutex);
    ownerMinions.erase(owner->GetGUID());
}

void RemoveMinion(ObjectGuid ownerGuid, ObjectGuid minionGuid)
{
    std::lock_guard<std::mutex> lock(minionMutex);
    auto itr = ownerMinions.find(ownerGuid);
    if (itr == ownerMinions.end())
        return;
    auto& guids = itr->second;
    guids.erase(std::remove(guids.begin(), guids.end(), minionGuid), guids.end());
    if (guids.empty())
        ownerMinions.erase(itr);
}
}

void AddNecromancerMinionScripts()
{
    new NecromancerMinionScript<MinionKind::Skeleton>("NecromancerSkeletonAI");
    new NecromancerMinionScript<MinionKind::Archer>("NecromancerArcherAI");
    new NecromancerMinionScript<MinionKind::Mage>("NecromancerMageAI");
    new NecromancerMinionScript<MinionKind::Abomination>("NecromancerAbominationAI");
}
