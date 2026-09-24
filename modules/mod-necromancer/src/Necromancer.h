#ifndef MOD_NECROMANCER_H
#define MOD_NECROMANCER_H

#include "Common.h"
#include "ObjectGuid.h"

#include <initializer_list>
#include <list>

class Player;
class TempSummon;
class Unit;
class WorldObject;
struct Position;

namespace Necromancer
{
constexpr uint8 CLASS_NECROMANCER = 13;

enum Spells : uint32
{
    SPELL_SOUL_BOLT = 90400, SPELL_CAPTURED_SOULS = 90401, SPELL_RAISE_DEAD = 90402,
    SPELL_DEATHLY_BRAND = 90403, SPELL_SOUL_TAP = 90404, SPELL_DEATH_COMMAND = 90405,
    SPELL_RAISE_DEADEYE = 90406, SPELL_SOUL_DRAIN = 90407, SPELL_CORPSE_EXPLOSION = 90408,
    SPELL_FRENZIED_LEGION = 90409, SPELL_RAISE_PLAGUE_MAGE = 90410, SPELL_SOUL_HARVEST = 90411,
    SPELL_SACRIFICIAL_PACT = 90412, SPELL_BLACK_VOLLEY = 90413, SPELL_CREATE_ABOMINATION = 90414,
    SPELL_ARMY_OF_THE_DAMNED = 90415, SPELL_SPECTRAL_BOLT = 90420, SPELL_PLAGUE_VOLLEY = 90421,
    SPELL_NECROMANCER_PASSIVE = 90422, SPELL_FUNERAL_BLAST = 90423, SPELL_NECROTIC_ROT = 90424,
    SPELL_BONE_SHROUD = 90430, SPELL_BONE_SPEARHEAD = 90518, SPELL_GRAVE_TIDE = 90533,
    SPELL_MASTER_OF_THE_DEAD = 90561,
    // Single-rank talents of the talent trees (localTools/necromancer/talentTree.json). Ranked ones are read in
    // place with GetTalentValue, next to the values their tooltips quote.
    TALENT_HUNGRY_SOULS = 90568, TALENT_BLOOD_FOR_BLOOD = 90580, TALENT_LAST_BREATH = 90581,
    TALENT_FLESH_GUARD = 90588, TALENT_NECROPOLIS = 90593, TALENT_HUNGRY_SWARM = 90416, TALENT_GREEDY_DRAIN = 90596,
    TALENT_LIVING_OSSUARY = 90599, TALENT_MASTER_OF_TOMBS = 90434, TALENT_MASS_GRAVE = 90435,
    TALENT_EXALTED_SACRIFICE = 90438, TALENT_HUNGRY_HORDE = 90445, TALENT_SOUL_THREAD = 90448,
    TALENT_CONTAGION = 90449, TALENT_COMMANDER = 90450, TALENT_HAND_OF_DEATH = 90451,
    TALENT_PESTILENT_ARMY = 90452, TALENT_RELENTLESS_MARCH = 90453, TALENT_LORD_OF_THE_LEGION = 90454
};

enum Creatures : uint32
{
    NPC_SKELETON_WARRIOR = 910100, NPC_CURSED_ARCHER = 910101,
    NPC_PLAGUE_MAGE = 910102, NPC_ABOMINATION = 910103
};

// Where the class sits on the server's damage ladder. Every number the Necromancer deals - his own spells,
// his damage over time, and every swing and shot his minions take - is multiplied by this on its way out.
//
// The coefficients further down were written against a flatter curve than this server turned out to have, and
// measured against real gear they left the class at roughly a third of what a damage dealer is expected to do.
// Rather than rewrite each coefficient and lose the shape they encode - which spell is worth more than which -
// they keep their values and this single number sets the class's place. Raise it to raise the whole class
// evenly; nothing else has to move.
constexpr float DAMAGE_SCALE = 1.5f;

// Necrotic Rot: the plague the army carries. Every minion that lands a hit stacks it on what it hit, so a
// pack rots without the Necromancer spending a single global on it - which is the whole point of an army.
// The cap matches the spell's own StackAmount, so ModStackAmount clamps it for us.
constexpr uint8 NECROTIC_ROT_MAX_STACKS = 5;
// A brand planted by Morbid Explosion rather than cast by hand: shorter, and it never gets Eternal Seal.
constexpr uint32 SPREAD_BRAND_DURATION = 12000;

// Levée des morts takes this many Âmes: the squad is the one thing Âmes buy
constexpr uint8 RAISE_DEAD_SOULS = 10;
// Décomposition: an ordinary minion loses this share of its health every second (about 25 seconds alone), the
// abomination half of it. Drain d'âme, Ordre de mort and leech are what keep an army standing.
constexpr uint32 DECAY_PERCENT = 4;
constexpr uint32 ABOMINATION_DECAY_PERCENT = 2;
// Drain d'âme heals every minion by this share of its health per pulse
constexpr uint32 DRAIN_HEAL_PERCENT = 6;
// Minions leech this share of what they deal to the target the Nécromancien marked
constexpr uint32 BRAND_LEECH_PERCENT = 10;

enum class MinionKind : uint8 { Skeleton, Archer, Mage, Abomination };
struct TalentRank { uint32 spellId; int32 value; };
struct AbilityUnlock { uint32 spellId; uint8 level; };

bool IsNecromancer(Player const* player);
int32 GetTalentValue(Unit const* unit, std::initializer_list<TalentRank> ranks);
uint8 GetMaxSouls(Player const* player);
uint8 GetSouls(Player const* player);
void AddSouls(Player* player, uint8 amount);
// Souls from Trait d'âme and Marque funèbre, which Âmes débordantes can double
void AddGeneratedSouls(Player* player, uint8 amount);
bool SpendSouls(Player* player, uint8 amount);
uint8 GetRaiseDeadCost(Player const* player);
void RestoreMana(Player* player, uint8 percent);
void ApplyNecroticRot(Player* owner, Unit* target, uint8 stacks = 1);
uint32 LearnUnlockedAbilities(Player* player);
void ForgetAbilitiesAboveLevel(Player* player, uint8 level);
float GetSpellDamageMultiplier(Player const* player, uint32 spellId, bool areaSpell = false);
float GetMinionDamageMultiplier(Player const* player, MinionKind kind);
float GetMinionHealthMultiplier(Player const* player, MinionKind kind);
std::list<Unit*> GetEnemiesAround(Player* caster, WorldObject* center, float radius, std::size_t limit);

// Timed bonuses a talent hangs on the Nécromancien himself (Commandant, Seigneur de la Légion)
void StartCommander(Player* player, uint32 durationMs);
bool IsCommanding(Player const* player);
void StartLordOfTheLegion(Player* player, uint32 durationMs);
bool IsLordOfTheLegion(Player const* player);

TempSummon* SummonMinion(Player* owner, MinionKind kind, uint32 durationMs = 0);
// The squad Levée des morts raises: two skeleton warriors (three with Ossuaire vivant), an archer and a mage
void RaiseSquad(Player* owner);
uint32 CountMinions(Player* owner);
bool HasAbomination(Player* owner);
void CommandMinions(Player* owner, Unit* target, uint32 durationMs, uint32 healPercent);
void DirectMinionsAt(Player* owner, Unit* target);
void HealMinions(Player* owner, uint32 percent);
void HealMostWoundedMinion(Player* owner, uint32 percent);
// Sacrifices the weakest ordinary minion; its position is where it fell, for the blast
bool SacrificeWeakestMinion(Player* owner, Position& where);
void RefreshMinionDurations(Player* owner);
void CleanupMinions(Player* owner);
void RemoveMinion(ObjectGuid ownerGuid, ObjectGuid minionGuid);
}

#endif
