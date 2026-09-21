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

namespace Necromancer
{
constexpr uint8 CLASS_NECROMANCER = 13;

enum Spells : uint32
{
    SPELL_SOUL_BOLT = 90400, SPELL_CAPTURED_SOULS = 90401, SPELL_RAISE_SKELETON = 90402,
    SPELL_DEATHLY_BRAND = 90403, SPELL_SOUL_TAP = 90404, SPELL_DEATH_COMMAND = 90405,
    SPELL_RAISE_DEADEYE = 90406, SPELL_SOUL_DRAIN = 90407, SPELL_CORPSE_EXPLOSION = 90408,
    SPELL_FRENZIED_LEGION = 90409, SPELL_RAISE_PLAGUE_MAGE = 90410, SPELL_SOUL_HARVEST = 90411,
    SPELL_SACRIFICIAL_PACT = 90412, SPELL_BLACK_VOLLEY = 90413, SPELL_CREATE_ABOMINATION = 90414,
    SPELL_ARMY_OF_THE_DAMNED = 90415, SPELL_SPECTRAL_BOLT = 90420, SPELL_PLAGUE_VOLLEY = 90421,
    SPELL_NECROMANCER_PASSIVE = 90422, SPELL_FUNERAL_BLAST = 90423, SPELL_NECROTIC_ROT = 90424,
    SPELL_BONE_SPEARHEAD = 90518, SPELL_GRAVE_TIDE = 90533, SPELL_MASTER_OF_THE_DEAD = 90561
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

enum class MinionKind : uint8 { Skeleton, Archer, Mage, Abomination };
struct TalentRank { uint32 spellId; int32 value; };
struct AbilityUnlock { uint32 spellId; uint8 level; };

bool IsNecromancer(Player const* player);
int32 GetTalentValue(Unit const* unit, std::initializer_list<TalentRank> ranks);
uint8 GetMaxSouls(Player const* player);
uint8 GetSouls(Player const* player);
void AddSouls(Player* player, uint8 amount);
bool SpendSouls(Player* player, uint8 amount);
void RestoreMana(Player* player, uint8 percent);
void ApplyNecroticRot(Player* owner, Unit* target);
uint32 LearnUnlockedAbilities(Player* player);
float GetSpellDamageMultiplier(Player const* player, uint32 spellId, bool areaSpell = false);
float GetMinionDamageMultiplier(Player const* player, MinionKind kind);
float GetMinionHealthMultiplier(Player const* player, MinionKind kind);
std::list<Unit*> GetEnemiesAround(Player* caster, WorldObject* center, float radius, std::size_t limit);

TempSummon* SummonMinion(Player* owner, MinionKind kind, uint32 durationMs = 0);
uint32 CountMinions(Player* owner);
void CommandMinions(Player* owner, Unit* target, uint32 durationMs);
void DirectMinionsAt(Player* owner, Unit* target);
bool SacrificeOldestMinion(Player* owner);
void ExtendMinionDurations(Player* owner, uint32 durationMs);
void RefreshMinionDurations(Player* owner);
void CleanupMinions(Player* owner);
void RemoveMinion(ObjectGuid ownerGuid, ObjectGuid minionGuid);
}

#endif
