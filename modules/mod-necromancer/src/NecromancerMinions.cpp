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

// A raised minion climbs out of the ground (the Death Knight ghoul's rise) before it does anything: it holds
// still for this long so the emerge animation plays out instead of being cut by its first step
constexpr uint32 RISE_MS = 1400;
// SpellVisualKit of Raise Dead's ghoul (spell 52150, visual 9311): the earth bursting open under it
constexpr uint32 SPELL_VISUAL_KIT_SUMMON_GHOULS = 9491;
// Nothing lives past this, decay or not; Décomposition is meant to end them long before
constexpr uint32 MINION_MAX_LIFETIME_MS = 60 * IN_MILLISECONDS;

enum MinionData : uint32
{
    DATA_COMMAND = 1,       // Ordre de mort: the damage bonus runs this long
    DATA_MASTER = 2         // Maître des morts: empowered and not decaying for this long
};

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
            if (creature->IsAlive())
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
    // Ossuaire vivant: the squad brings a third skeleton, and the army has room for it
    return 8u + (owner->HasAura(TALENT_LIVING_OSSUARY) ? 1u : 0u);
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

void Heal(Creature* minion, uint32 amount)
{
    if (minion && minion->IsAlive() && amount)
        minion->SetHealth(std::min<uint32>(minion->GetMaxHealth(), minion->GetHealth() + amount));
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
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        // It crumbles, and is gone a moment later rather than lying there for the corpse's full decay
        me->DespawnOrUnsummon(3s);
        Player* owner = GetOwner();
        if (!owner)
            return;
        RemoveMinion(owner->GetGUID(), me->GetGUID());

        // Dernier souffle
        if (owner->HasAura(TALENT_LAST_BREATH))
            AddSouls(owner, 1);

        // Fosse commune: what falls bursts, and the burst is the Nécromancien's
        if (owner->HasAura(TALENT_MASS_GRAVE))
        {
            float const spellPower =
                float(std::max<int32>(0, owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW)));
            int32 const damage = std::max<int32>(1, int32((float(owner->GetLevel()) * 4.0f + spellPower * 0.25f) *
                GetSpellDamageMultiplier(owner, SPELL_FUNERAL_BLAST, true) * DAMAGE_SCALE));
            for (Unit* enemy : GetEnemiesAround(owner, me, 8.0f, 6))
                owner->CastCustomSpell(SPELL_FUNERAL_BLAST, SPELLVALUE_BASE_POINT0, damage, enemy, TRIGGERED_FULL_MASK);
        }
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
        if (!riseTimer && !me->GetVictim() && attacker && me->IsValidAttackTarget(attacker))
            AttackStart(attacker);
    }

    // Melee leech: the brand and Crocs avides turn a share of every swing into the minion's own health. Ranged
    // minions deal their damage through a spell credited to their master, so theirs is healed in CastRanged.
    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/,
        SpellSchoolMask /*schoolMask*/) override
    {
        if (Player* owner = GetOwner())
            Heal(me, CalculatePct(damage, LeechPercent(owner, victim)));
    }

    void SetData(uint32 type, uint32 value) override
    {
        if (type == DATA_COMMAND)
            commandTimer = value;
        else if (type == DATA_MASTER)
            masterTimer = value;
    }

    void AttackStart(Unit* target) override
    {
        if (!target || riseTimer)
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

        // Still climbing out of the ground
        if (riseTimer)
        {
            riseTimer = riseTimer > diff ? riseTimer - diff : 0;
            if (!riseTimer)
                me->GetMotionMaster()->MoveFollow(owner, FollowDistance(), frand(0.0f, float(M_PI * 2.0)),
                    MOTION_SLOT_ACTIVE);
            return;
        }

        commandTimer = commandTimer > diff ? commandTimer - diff : 0;
        masterTimer = masterTimer > diff ? masterTimer - diff : 0;
        scaleTimer = scaleTimer > diff ? scaleTimer - diff : 0;
        attackTimer = attackTimer > diff ? attackTimer - diff : 0;

        if (Decay(owner, diff))
            return;

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
            // The brand bonus and Marque dévorante are part of the weapon: rescale for the new target
            Scale(owner);
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
        {
            ApplyNecroticRot(owner, target);
            Feed(owner);
        }
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

    // Décomposition: the army is always dying, and only the Nécromancien keeps it standing. Returns whether the
    // minion fell to it.
    bool Decay(Player* owner, uint32 diff)
    {
        decayTimer += diff;
        if (decayTimer < IN_MILLISECONDS)
            return false;
        decayTimer -= IN_MILLISECONDS;

        // Maître des morts suspends it, and so does Légion frénétique with Horde affamée
        if (masterTimer || (owner->HasAura(SPELL_FRENZIED_LEGION) && owner->HasAura(TALENT_HUNGRY_HORDE)))
            return false;

        float percent = float(kind == MinionKind::Abomination ? ABOMINATION_DECAY_PERCENT : DECAY_PERCENT);
        // Décomposition ralentie
        percent *= 1.0f - float(GetTalentValue(owner, { { 90522, 15 }, { 90523, 30 } })) / 100.0f;
        uint32 const loss = std::max<uint32>(1, uint32(float(me->GetMaxHealth()) * percent / 100.0f));
        if (me->GetHealth() <= loss)
        {
            me->KillSelf(false);
            return true;
        }
        me->SetHealth(me->GetHealth() - loss);
        return false;
    }

    uint32 LeechPercent(Player const* owner, Unit const* victim) const
    {
        uint32 percent = uint32(GetTalentValue(owner, { { 90594, 10 }, { 90595, 20 } }));
        if (victim && victim->HasAura(SPELL_DEATHLY_BRAND, owner->GetGUID()))
            percent += BRAND_LEECH_PERCENT;
        return percent;
    }

    // What the brand is worth against the current target: 15%, and Marque dévorante on top
    float BrandMultiplier(Player const* owner, Unit const* victim) const
    {
        if (!victim || !victim->HasAura(SPELL_DEATHLY_BRAND, owner->GetGUID()))
            return 1.0f;
        return 1.15f + float(GetTalentValue(owner, { { 90597, 10 }, { 90598, 20 } })) / 100.0f;
    }

    float ActiveMultiplier(Player const* owner) const
    {
        float multiplier = GetMinionDamageMultiplier(owner, kind);
        // Présence du maître: only within 30 yards of him
        if (me->GetDistance(owner) <= 30.0f)
            multiplier *= 1.0f + float(GetTalentValue(owner, { { 90550, 4 }, { 90551, 8 } })) / 100.0f;
        // Ordre de mort, and Commandement impitoyable on top of it
        if (commandTimer)
            multiplier *= 1.25f + float(GetTalentValue(owner, { { 90519, 10 }, { 90520, 20 } })) / 100.0f;
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
        // Keeps the same share of its health when the maximum moves, so rescaling never heals or wounds it
        float const share = me->GetMaxHealth() ? float(me->GetHealth()) / float(me->GetMaxHealth()) : 1.0f;
        me->SetMaxHealth(uint32(health));
        me->SetHealth(std::max<uint32>(1, uint32(health * std::min(1.0f, share))));
        me->SetArmor(int32(40 + owner->GetLevel() * 35));

        float baseDamage = (4.0f + float(owner->GetLevel()) * 1.45f + spellPower * 0.10f)
            * ActiveMultiplier(owner) * BrandMultiplier(owner, me->GetVictim()) * DAMAGE_SCALE;
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
        int32 const damage = int32((6.0f + owner->GetLevel() * 2.0f + spellPower * 0.16f)
            * ActiveMultiplier(owner) * DAMAGE_SCALE);

        if (kind == MinionKind::Mage)
        {
            // The plague mage is where the rot spreads fastest: its salvo rots every enemy it splashes.
            // Armée pestilentielle widens it.
            std::size_t const targets = owner->HasAura(TALENT_PESTILENT_ARMY) ? 6 : 4;
            for (Unit* enemy : GetEnemiesAround(owner, target, 8.0f, targets))
            {
                int32 const dealt = int32(float(damage) * BrandMultiplier(owner, enemy));
                me->CastCustomSpell(SPELL_PLAGUE_VOLLEY, SPELLVALUE_BASE_POINT0, dealt, enemy,
                    TRIGGERED_FULL_MASK, nullptr, nullptr, owner->GetGUID());
                ApplyNecroticRot(owner, enemy);
                Heal(me, CalculatePct(uint32(dealt), LeechPercent(owner, enemy)));
            }
        }
        else
        {
            int32 const dealt = int32(float(damage) * BrandMultiplier(owner, target));
            me->CastCustomSpell(SPELL_SPECTRAL_BOLT, SPELLVALUE_BASE_POINT0, dealt, target,
                TRIGGERED_FULL_MASK, nullptr, nullptr, owner->GetGUID());
            ApplyNecroticRot(owner, target);
            Heal(me, CalculatePct(uint32(dealt), LeechPercent(owner, target)));
        }

        if ((owner->HasAura(SPELL_FRENZIED_LEGION) || masterTimer) && roll_chance_i(20))
            RestoreMana(owner, 1);
        Feed(owner);
    }

    // Nuée affamée: now and then an attack tears a soul loose for the master
    static void Feed(Player* owner)
    {
        if (owner->HasAura(TALENT_HUNGRY_SWARM) && roll_chance_i(5))
            AddSouls(owner, 1);
    }

    MinionKind kind;
    uint32 riseTimer = RISE_MS;
    uint32 decayTimer = 0;
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

    // In the arc before the Nécromancien, where he sees them climb out: behind him the burst fills the camera
    Position position = owner->GetNearPosition(frand(2.5f, 5.0f), frand(-1.2f, 1.2f));
    // Timed, not timed-or-dead: that one restarts its clock whenever the minion is in combat, so it never ran out
    TempSummon* summon = owner->SummonCreature(GetEntry(kind), position, TEMPSUMMON_TIMED_DESPAWN,
        durationMs ? durationMs : MINION_MAX_LIFETIME_MS);
    if (!summon)
        return nullptr;

    summon->SetOwnerGUID(owner->GetGUID());
    summon->SetCreatorGUID(owner->GetGUID());
    summon->SetFacingToObject(owner);
    BindToOwner(summon, owner);
    // Out of the ground, like the Death Knight's ghouls: the earth bursts and the minion climbs out of it
    summon->SendPlaySpellVisual(SPELL_VISUAL_KIT_SUMMON_GHOULS);
    summon->HandleEmoteCommand(EMOTE_ONESHOT_EMERGE);
    {
        std::lock_guard<std::mutex> lock(minionMutex);
        ownerMinions[owner->GetGUID()].push_back(summon->GetGUID());
    }
    return summon;
}

void RaiseSquad(Player* owner)
{
    uint8 const skeletons = owner->HasAura(TALENT_LIVING_OSSUARY) ? 3 : 2;
    for (uint8 i = 0; i < skeletons; ++i)
        SummonMinion(owner, MinionKind::Skeleton);
    SummonMinion(owner, MinionKind::Archer);
    SummonMinion(owner, MinionKind::Mage);

    // Seigneur de la Légion
    if (owner->HasAura(TALENT_LORD_OF_THE_LEGION))
        StartLordOfTheLegion(owner, 10 * IN_MILLISECONDS);
}

uint32 CountMinions(Player* owner) { return owner ? uint32(ResolveMinions(owner).size()) : 0; }

bool HasAbomination(Player* owner)
{
    for (Creature* minion : ResolveMinions(owner))
        if (minion->GetEntry() == NPC_ABOMINATION)
            return true;
    return false;
}

void CommandMinions(Player* owner, Unit* target, uint32 durationMs, uint32 healPercent)
{
    for (Creature* minion : ResolveMinions(owner))
    {
        Heal(minion, CalculatePct(minion->GetMaxHealth(), healPercent));
        minion->AI()->SetData(DATA_COMMAND, durationMs);
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

void HealMinions(Player* owner, uint32 percent)
{
    for (Creature* minion : ResolveMinions(owner))
        Heal(minion, CalculatePct(minion->GetMaxHealth(), percent));
}

void HealMostWoundedMinion(Player* owner, uint32 percent)
{
    Creature* weakest = nullptr;
    for (Creature* minion : ResolveMinions(owner))
        if (!weakest || minion->GetHealthPct() < weakest->GetHealthPct())
            weakest = minion;
    if (weakest)
        Heal(weakest, CalculatePct(weakest->GetMaxHealth(), percent));
}

bool SacrificeWeakestMinion(Player* owner, Position& where)
{
    Creature* weakest = nullptr;
    for (Creature* minion : ResolveMinions(owner))
        if (IsOrdinary(minion) && (!weakest || minion->GetHealthPct() < weakest->GetHealthPct()))
            weakest = minion;
    if (!weakest)
        return false;
    where = weakest->GetPosition();
    if (TempSummon* summon = weakest->ToTempSummon())
        summon->UnSummon();
    return true;
}

// Maître des morts: the whole army back to full, empowered and not decaying for 20 seconds
void RefreshMinionDurations(Player* owner)
{
    for (Creature* minion : ResolveMinions(owner))
    {
        minion->SetHealth(minion->GetMaxHealth());
        minion->AI()->SetData(DATA_MASTER, 20000);
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
