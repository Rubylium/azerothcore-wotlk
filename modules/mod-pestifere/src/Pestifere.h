#ifndef MOD_PESTIFERE_H
#define MOD_PESTIFERE_H

#include "Common.h"
#include "Define.h"
#include "Unit.h"

#include <array>
#include <list>
#include <vector>

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
    // Healing the body cannot use, kept as a growth that takes the next hits instead of being wasted
    SPELL_EXCROISSANCE              = 90230,

    // Active spells taught by a talent: each is that talent's rank spell
    SPELL_CHARNIER_AMBULANT         = 90256,
    SPELL_PURGE_CATHARTIQUE         = 90265,
    SPELL_SEPULCRE                  = 90268,
    SPELL_VOMISSURE                 = 90284,
    SPELL_AVATAR_DE_LA_PESTE        = 90287,

    // Talent actives of the retail-style trees (localTools/pestifere/talentTree.json)
    SPELL_JET_DE_SANG               = 90296,    // the healer's reach: a spit whose damage heals the most injured
    SPELL_POUSSEE_DE_SANG           = 90297,    // the healer's burst
    SPELL_MIASME_SUFFOCANT          = 90298,    // pure data: enemies around deal less damage
    SPELL_CAL_PUTRIDE               = 90299,    // Carapace réactive's armor

    // The healer tree "Sangsue" (pestifere-healer.DESIGN.md). The actives are their talent's rank spell.
    SPELL_TRANSFUSION_HEAL          = 90301,    // names Transfusion's healing in the combat log
    SPELL_SANGSUE                   = 90302,
    SPELL_SAIGNEE                   = 90303,
    SPELL_ABSORPTION_MORBIDE        = 90304,
    SPELL_DON_DE_SANG               = 90305,
    SPELL_SYMBIOTE                  = 90306,
    SPELL_SYMBIOTE_AURA             = 90307,    // on the bearer
    SPELL_PESTILENCE_SALVATRICE     = 90308,
    SPELL_COAGULATION               = 90309,    // on an ally Transfusion healed
    SPELL_RESERVE_DE_SANG           = 90310,    // shown while hits are banked
    SPELL_CONTAGION_BENIGNE_HEAL    = 90311,    // names Contagion bénigne's healing in the combat log
    SPELL_SPORES                    = 90312,    // Transfusion's heal-over-time proc
    SPELL_PUSTULE                   = 90313,    // names the Pustule éclatante proc's healing
    SPELL_ESSAIM                    = 90314,    // names the Essaim proc's healing
    SPELL_BRUME_PESTILENTIELLE      = 90315,
    SPELL_BRUME_HOT                 = 90316,    // Brume pestilentielle on each group member
    SPELL_CAILLOT                   = 90317,    // the absorb Symbiote builds on its bearer
    SPELL_HEMOSTASE                 = 90318     // the healer's burst buff: its next melee hits heal three times as much
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
// % chance a melee hit taken hardens the skin (Cal putride)
constexpr Talent<1> TALENT_CARAPACE_REACTIVE = { { { 91214, 20 } } };
// Frappe putride sows a third stack of rot
constexpr Talent<1> TALENT_CONTAGION_ETERNELLE = { { { 91246, 1 } } };
// % of all damage dealt that heals the Pestiféré
constexpr Talent<1> TALENT_SYMBIOSE_PARASITAIRE = { { { 91247, 10 } } };
// % more healing from Jet de sang
constexpr Talent<2> TALENT_VEINES_OUVERTES = { { { 91260, 15 }, { 91261, 30 } } };
// Jet de sang also heals the second most injured ally, for this % of it
constexpr Talent<1> TALENT_SANG_PROJETE = { { { 91264, 50 } } };
// % of Jet de sang's cooldown taken off
constexpr Talent<1> TALENT_CRACHAT_URGENCE = { { { 91265, 50 } } };

// The "Sangsue" healer tree (pestifere-healer.DESIGN.md section 3). Talents missing here are data only: Humeurs
// noires, Veines gonflées, Anticorps, Circulation, Force vitale (auras), Sangsue vorace and Saignée profonde
// (spell modifiers), and the ones that teach a spell.
// The spine: melee hits trade their damage for healing
constexpr Talent<1> TALENT_TRANSFUSION = { { { 90300, 1 } } };
// % more Transfusion healing
constexpr Talent<3> TALENT_TRANSFUSION_VIGOUREUSE = { { { 90326, 5 }, { 90327, 10 }, { 90328, 15 } } };
// % less damage taken by an ally Transfusion healed
constexpr Talent<3> TALENT_COAGULATION = { { { 90329, 2 }, { 90330, 4 }, { 90331, 6 } } };
// Learned: Carapace suintante also shields the most injured ally
constexpr Talent<1> TALENT_CARAPACE_PARTAGEE = { { { 90334, 1 } } };
// % more healing on an ally under 35% health
constexpr Talent<2> TALENT_TRIAGE = { { { 90335, 10 }, { 90336, 20 } } };
// % of each hit banked while nobody needs healing
constexpr Talent<3> TALENT_RESERVE_DE_SANG = { { { 90343, 10 }, { 90344, 20 }, { 90345, 30 } } };
// % chance for a leech whose enemy dies to jump to the nearest enemy
constexpr Talent<2> TALENT_SANGSUE_PROLIFERE = { { { 90346, 50 }, { 90347, 100 } } };
// % more Détonation healing, which goes to injured allies instead of the caster
constexpr Talent<2> TALENT_DETONATION_SALVATRICE = { { { 90348, 50 }, { 90349, 100 } } };
// % less health Don de sang costs
constexpr Talent<2> TALENT_DONNEUR_UNIVERSEL = { { { 90353, 50 }, { 90354, 100 } } };
// % of maximum health Contagion heals each group member in its radius
constexpr Talent<3> TALENT_CONTAGION_BENIGNE = { { { 90355, 2 }, { 90356, 4 }, { 90357, 6 } } };
// % of the Transfusion healing given to others that heals the caster too
constexpr Talent<2> TALENT_SANG_PARTAGE = { { { 90358, 10 }, { 90359, 20 } } };
// % more Sangsue healing
constexpr Talent<2> TALENT_SANGSUE_GEANTE = { { { 90363, 15 }, { 90364, 30 } } };
// % of Transfusion healing the Symbiote copies (25% without the talent)
constexpr Talent<2> TALENT_SYMBIOSE_PARFAITE = { { { 90365, 35 }, { 90366, 50 } } };
// % less damage taken by the Symbiote's bearer
constexpr Talent<3> TALENT_SANG_DE_L_HOTE = { { { 90367, 3 }, { 90368, 6 }, { 90369, 9 } } };
// % more Transfusion healing from Frappe putride
constexpr Talent<3> TALENT_HEMOPHAGIE = { { { 90370, 20 }, { 90371, 40 }, { 90372, 60 } } };
// % of each Transfusion heal that also heals the next injured ally
constexpr Talent<1> TALENT_COEUR_BATTANT = { { { 90375, 30 } } };
// Extra % chance per hit to proc Spores
constexpr Talent<3> TALENT_SPORES_FERTILES = { { { 90376, 5 }, { 90377, 10 }, { 90378, 15 } } };
// Extra % chance per hit to proc Pustule éclatante (and +25% splash per point)
constexpr Talent<2> TALENT_PUSTULES_MULTIPLES = { { { 90379, 4 }, { 90380, 8 } } };
// Extra Essaim bounces
constexpr Talent<2> TALENT_ESSAIM_VORACE = { { { 90381, 1 }, { 90382, 2 } } };

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

// Dual Wield: the class wields two one-handers like a Death Knight. It is also a starting spell, but characters created
// before that never got it, and without it the server refuses anything in the off hand.
constexpr uint32 SPELL_DUAL_WIELD = 674;

constexpr std::array<AbilityUnlock, 14> AbilityUnlocks = { {
    { SPELL_DUAL_WIELD, 1 },
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

// Drops abilities the new level has not reached, then teaches whatever that level still allows
void ForgetAbilitiesAboveLevel(Player* player, uint8 level);

// --- The healer (PestifereHealer.cpp): every heal picks its own target ---

// How far the healer reaches an ally, in yards
constexpr float HEAL_RANGE = 40.0f;

// The healer and its group members within `range`, alive and in line of sight (pets excluded)
std::vector<Unit*> GetGroupMembersInRange(Player* healer, float range);

// The injured ones among them, most injured (lowest health percentage) first, at most `maxCount`.
// `exclude` is left out (Don de sang never picks its caster).
std::vector<Unit*> GetInjuredAllies(Player* healer, float range, std::size_t maxCount, Unit const* exclude = nullptr);

// Heals one ally: Triage and the target's healing-taken modifiers apply, the heal is logged under `spellId`
// and threatens the enemies fighting that ally. Returns the health actually restored.
uint32 HealAlly(Player* healer, Unit* target, uint32 amount, uint32 spellId);

// Spreads `amount` over `allies` in their order, each taking what it is missing; the last one takes the rest.
// Returns the health actually restored.
uint32 HealByNeed(Player* healer, std::vector<Unit*> const& allies, uint32 amount, uint32 spellId);
}

#endif
