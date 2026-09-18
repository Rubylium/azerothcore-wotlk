#ifndef MOD_PESTIFERE_H
#define MOD_PESTIFERE_H

#include "Common.h"
#include "Define.h"
#include "Unit.h"

#include <array>
#include <list>

class Aura;
class Player;
class WorldObject;

namespace Pestifere
{
// ChrClasses id of the Pestiféré. The class itself is declared in the world table `custom_class`
// (see modules/mod-custom-classes); this module only implements its kit.
constexpr uint8 CLASS_PESTIFERE = 12;

enum Spells : uint32
{
    SPELL_FRAPPE_PUTRIDE            = 90200,
    SPELL_CONTAGION                 = 90201,
    SPELL_DETONATION                = 90202,
    SPELL_ODEUR_DE_CHAROGNE         = 90203,
    SPELL_CRACHAT_BILIEUX           = 90204,
    SPELL_POURRITURE                = 90205,
    SPELL_DETONATION_CHAIN          = 90206,
    SPELL_SEPULCRE_STORED           = 90207,
    SPELL_SEPULCRE_STORED_ENEMY     = 90208,
    SPELL_CARAPACE_THREAT           = 90209,
    SPELL_INOCULATION_CARAPACE      = 90210,
    SPELL_CARAPACE_NECROSEE         = 90211,
    SPELL_INOCULATION_CHAIR         = 90212,
    SPELL_CHAIR_PUTRIDE             = 90213,
    SPELL_INOCULATION_PESTE         = 90214,
    SPELL_PESTE_VIRULENTE           = 90215,
    SPELL_FIEVRE                    = 90216,
    SPELL_FRAPPE_PUTRIDE_ECHO       = 90217,
    SPELL_CARAPACE_NECROSEE_ENEMY   = 90220,
    SPELL_CHAIR_PUTRIDE_ENEMY       = 90221,
    SPELL_PESTE_VIRULENTE_ENEMY     = 90222,

    // The kit from level 24 to 70
    SPELL_FLAQUE_DE_BILE            = 90223,
    SPELL_MORSURE_FETIDE            = 90224,
    SPELL_RIPOSTE_PURULENTE         = 90225,
    SPELL_CARAPACE_SUINTANTE        = 90226,
    SPELL_BOND_PUTRIDE              = 90227,
    SPELL_PUANTEUR_INSOUTENABLE     = 90228,
    SPELL_PANDEMIE                  = 90229,

    // Active spells taught by a talent: each is that talent's rank spell
    SPELL_CHARNIER_AMBULANT         = 90256,
    SPELL_PURGE_CATHARTIQUE         = 90265,
    SPELL_SEPULCRE                  = 90268,
    SPELL_VOMISSURE                 = 90284,
    SPELL_AVATAR_DE_LA_PESTE        = 90287,

    // Rigor mortis's cooldown, a debuff while it cannot save the carrier again
    SPELL_RIGOR_MORTIS_COOLDOWN     = 90286
};

// A learned talent rank is a passive aura on the player (its rank spell). Each rank names the value it grants.
struct TalentRank
{
    uint32 spellId;
    int32 value;
};

template<std::size_t Ranks>
using Talent = std::array<TalentRank, Ranks>;

// The "Charnier" tree (pestifere.DESIGN.md section 6). Talents missing here are data only: Peau coriace (armor),
// Inoculation rapide, Miasme, Crocs infectés and Pandémie prolongée (spell modifiers), Mains putrides (a proc, see
// its aura script), and the ones that teach a spell (Chair putride, Peste virulente, Charnier ambulant, Purge
// cathartique, Vomissure, Sépulcre, Avatar de la peste).
// % rage from damage taken
constexpr Talent<3> TALENT_RAGE_FIELLEUSE = { { { 90234, 10 }, { 90235, 20 }, { 90236, 30 } } };
// % two-hand Frappe putride damage
constexpr Talent<3> TALENT_FOSSOYEUR = { { { 90246, 8 }, { 90247, 16 }, { 90248, 24 } } };
// % Chair putride healing per plague
constexpr Talent<2> TALENT_SYMBIOSE_MORBIDE = { { { 90249, 15 }, { 90250, 30 } } };
// % of normal rage from own plagues
constexpr Talent<3> TALENT_METABOLISME_NECROTIQUE = { { { 90251, 33 }, { 90252, 66 }, { 90253, 100 } } };
// Extra enemies detonated
constexpr Talent<2> TALENT_DETONATION_EN_CHAINE = { { { 90254, 1 }, { 90255, 2 } } };
// % damage reduction per plague
constexpr Talent<3> TALENT_CROUTE_NECROSEE = { { { 90257, 2 }, { 90258, 4 }, { 90259, 6 } } };
// % extra threat from plague damage
constexpr Talent<3> TALENT_MENACE_CONTAGIEUSE = { { { 90260, 30 }, { 90261, 60 }, { 90262, 90 } } };
// % less Peste virulente self-damage
constexpr Talent<2> TALENT_PORTEUR_ENDURCI = { { { 90263, 15 }, { 90264, 30 } } };
// % less damage taken at Virulence 3
constexpr Talent<2> TALENT_RESILIENCE_DU_PORTEUR = { { { 90266, 3 }, { 90267, 6 } } };
// % chance for Frappe putride to reset Contagion's cooldown
constexpr Talent<2> TALENT_CONTAGION_GALOPANTE = { { { 90269, 15 }, { 90270, 30 } } };
// % chance per Pourriture tick to grant Fièvre
constexpr Talent<3> TALENT_FIEVRE = { { { 90271, 3 }, { 90272, 6 }, { 90273, 9 } } };
// % of a dying enemy's Pourriture stacks passed to the nearest enemy
constexpr Talent<2> TALENT_CHAROGNARD = { { { 90274, 50 }, { 90275, 100 } } };
// Extra enemies Riposte purulente rots (and +10% damage per point)
constexpr Talent<2> TALENT_RIPOSTE_FETIDE = { { { 90276, 1 }, { 90277, 2 } } };
// % less damage dealt by enemies in Flaque de bile
constexpr Talent<3> TALENT_BILE_CORROSIVE = { { { 90278, 2 }, { 90279, 4 }, { 90280, 6 } } };
// % parry per plague carried
constexpr Talent<3> TALENT_HOTE_PARFAIT = { { { 90281, 1 }, { 90282, 2 }, { 90283, 3 } } };
// % more absorbed by Carapace suintante
constexpr Talent<3> TALENT_PUS_EPAIS = { { { 90293, 10 }, { 90294, 20 }, { 90295, 30 } } };
// Learned: a killing blow leaves the carrier at 1 health instead, once every 3 min
constexpr Talent<1> TALENT_RIGOR_MORTIS = { { { 90285, 1 } } };

// The value of the highest rank the unit has learned, 0 without the talent
template<std::size_t Ranks>
int32 GetTalentValue(Unit const* unit, Talent<Ranks> const& talent)
{
    if (!unit)
        return 0;

    for (auto rank = talent.rbegin(); rank != talent.rend(); ++rank)
        if (unit->HasAura(rank->spellId))
            return rank->value;

    return 0;
}

// A plague exists twice: the self aura the Pestiféré carries, and the version Contagion hands to enemies
// and Détonation consumes. The two never share an id, so a plague on an enemy is never a gift to it.
struct Plague
{
    uint32 selfSpellId;
    uint32 enemySpellId;
};

constexpr std::array<Plague, 3> Plagues = { {
    { SPELL_CARAPACE_NECROSEE, SPELL_CARAPACE_NECROSEE_ENEMY },
    { SPELL_CHAIR_PUTRIDE, SPELL_CHAIR_PUTRIDE_ENEMY },
    { SPELL_PESTE_VIRULENTE, SPELL_PESTE_VIRULENTE_ENEMY }
} };

// Pourriture caps at 6 stacks; a target detonated at the cap is "ripe" and refunds rage
constexpr uint8 POURRITURE_MAX_STACKS = 6;

// Rage is stored in tenths of a point
constexpr uint32 RAGE_UNIT = 10;

// The class has no trainer: its kit is granted by level
struct AbilityUnlock
{
    uint32 spellId;
    uint8 level;
};

constexpr std::array<AbilityUnlock, 13> AbilityUnlocks = { {
    { SPELL_FRAPPE_PUTRIDE, 1 },
    { SPELL_INOCULATION_CARAPACE, 1 },
    { SPELL_CONTAGION, 6 },
    { SPELL_DETONATION, 10 },
    { SPELL_ODEUR_DE_CHAROGNE, 14 },
    { SPELL_CRACHAT_BILIEUX, 20 },
    { SPELL_FLAQUE_DE_BILE, 24 },
    { SPELL_MORSURE_FETIDE, 30 },
    { SPELL_RIPOSTE_PURULENTE, 36 },
    { SPELL_CARAPACE_SUINTANTE, 44 },
    { SPELL_BOND_PUTRIDE, 50 },
    { SPELL_PUANTEUR_INSOUTENABLE, 60 },
    { SPELL_PANDEMIE, 70 }
} };

bool IsPestifere(Player const* player);

// True when the unit inoculated itself with that plague (it is both the owner and the origin of the aura)
bool CarriesOwnPlague(Unit const* unit, uint32 selfSpellId);

// Virulence: how many of the three plagues the unit carries, 0-3, and one more as an Avatar de la peste
uint8 GetVirulence(Unit const* unit);

std::list<Unit*> GetEnemiesInRange(Unit* caster, float radius);

// Living enemies of `caster` within `radius` of `center` (another unit, for a blast that travels)
std::list<Unit*> GetEnemiesAround(Unit* caster, WorldObject* center, float radius);

// Re-runs the amount calculation of every plague the unit carries, so the whole set rescales as soon as
// one is added or removed. `skip` leaves out an aura that is on its way out.
void RecalculatePlagues(Unit* unit, Aura const* skip = nullptr);

// The self plagues have no duration in the spell data: in combat they last as long as they are carried.
// Out of combat they run down this long, visibly on the buff, so a chain-puller can still arrive sick at the
// next pack; entering combat stops the countdown again.
constexpr int32 PLAGUE_OUT_OF_COMBAT_DECAY = 20 * IN_MILLISECONDS;

// Starts (expiring) or stops the out-of-combat countdown on one carried plague
void SetPlagueExpiring(Aura* plague, bool expiring);

// Starts or stops the countdown on every plague the player carries
void SetCarriedPlaguesExpiring(Player* player, bool expiring);

void GiveRage(Player* player, uint32 rage);

// The rage `damage` would give the unit if an enemy dealt it (Unit::RewardRage's damage-taken formula), in rage
// points; talents that turn other damage into rage pay a share of it
float GetRageFromDamageTaken(Unit const* unit, uint32 damage);

// Gives a (possibly fractional) number of rage points
void GiveRagePoints(Player* player, float rage);

// Teaches every ability the player's level unlocks; returns how many were newly learned
uint32 LearnUnlockedAbilities(Player* player);
}

#endif
