#ifndef MOD_OATHBLADE_H
#define MOD_OATHBLADE_H

#include "Common.h"

#include <initializer_list>
#include <list>

class Player;
class Unit;
class WorldObject;

namespace Oathblade
{
constexpr uint8 CLASS_OATHBLADE = 10;

enum Spells : uint32
{
    SPELL_SWIFT_CUT = 90800,
    SPELL_FLOW = 90801,
    SPELL_PRECISE_THRUST = 90802,
    SPELL_NOBLE_VERDICT = 90803,
    SPELL_SWEEPING_ARC = 90804,
    SPELL_FLAWLESS_FORM_PASSIVE = 90805,
    SPELL_REVERSAL = 90806,
    SPELL_NOBLE_ADVANCE = 90807,
    SPELL_CRESCENT_SWEEP = 90808,
    SPELL_BLADE_WARD = 90809,
    SPELL_FLOURISH = 90810,
    SPELL_ZEAL_STRIKE = 90811,
    SPELL_RALLY = 90812,
    SPELL_FINAL_EDICT = 90813,
    SPELL_TEMPO = 90825,
    SPELL_FLAWLESS_FORM = 90826,
    SPELL_LAST_SWIFT_CUT = 90827,
    SPELL_LAST_PRECISE_THRUST = 90828,
    SPELL_SECOND_STRIKE = 90829,
    // Sweep() deals every area ability's damage by casting a spell at each enemy, and the combat log names
    // the spell that was cast. With one carrier for all of them, four abilities' damage arrived under
    // "Second Strike" - on a three-target dummy test that was 56% of the meter, filed under a proc. One
    // carrier per ability, named after it, so the log says what actually hit.
    SPELL_SWEEPING_ARC_HIT = 90831,
    SPELL_CRESCENT_SWEEP_HIT = 90832,
    SPELL_BLADE_DANCE_HIT = 90833,
    SPELL_GRAND_FLOURISH_HIT = 90834,
    SPELL_SERRATED_CUTS = 90830,
    SPELL_BLADE_DANCE = 90942,
    SPELL_GRAND_FLOURISH = 90953,
    SPELL_PERFECT_EXECUTION = 90961,
    // Single-rank talents of the talent trees (localTools/oathblade/talentTree.json). Ranked ones are read in
    // place with GetTalentValue, next to the values their tooltip quotes.
    SPELL_TALENT_RALLYING_OATH = 91009,
    SPELL_TALENT_OATHBOUND_RESOLVE = 91012,
    SPELL_TALENT_RIPOSTE_WARD = 91015,
    SPELL_TALENT_ENDURING_WARD = 91016,
    SPELL_TALENT_SECOND_BREATH = 91023,
    SPELL_TALENT_RELENTLESS_OATH = 91028,
    SPELL_TALENT_WARDENS_OATH = 91068,
    SPELL_TALENT_SINGULAR_FORM = 91050,
    SPELL_TALENT_TEMPEST_FORM = 91051,
    SPELL_TALENT_RIPOSTE_CHAIN = 91054,
    SPELL_TALENT_RENDING_VERDICT = 91059,
    SPELL_TALENT_FLAWLESS_SURGE = 91062,
    SPELL_TALENT_CRESCENT_MOMENTUM = 91063,
    SPELL_TALENT_EDICT_MASTERY = 91064,
    SPELL_TALENT_BLADE_TEMPEST = 91065,
    SPELL_TALENT_RELENTLESS_STORM = 91066,
    SPELL_TALENT_FLAWLESS_MASTERY = 91067
};

struct TalentRank
{
    uint32 spellId;
    int32 value;
};

struct AbilityUnlock
{
    uint32 spellId;
    uint8 level;
};

bool IsOathblade(Player const* player);
int32 GetTalentValue(Unit const* unit, std::initializer_list<TalentRank> ranks);
uint8 GetFlow(Player const* player);
void AddFlow(Player* player, uint8 amount);
bool SpendFlow(Player* player, uint8 amount);
void AddTempo(Player* player, uint8 amount);
void AdvanceTechnique(Player* player, uint32 spellId, uint8 flow);
bool InFlawlessForm(Player const* player);
int32 GetStrikeDamage(Player const* player, float weaponScale, float attackPowerScale, uint32 spellId);
uint32 LearnUnlockedAbilities(Player* player);
void ForgetAbilitiesAboveLevel(Player* player, uint8 level);
std::list<Unit*> GetNearbyEnemies(Player* caster, WorldObject* center, float radius, std::size_t limit);
}

#endif
