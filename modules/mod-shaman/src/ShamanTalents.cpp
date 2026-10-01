#include "AllSpellScript.h"
#include "CellImpl.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Optional.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <list>
#include <vector>

// The Shaman's talents and abilities on the retail-style trees (localTools/shaman/talentTree.json) that spell data
// cannot carry. A talent is its rank spell's aura on the Shaman (learned by mod-custom-classes' TalentTree.cpp), read
// here with HasAura; a specialization is its passive (Élémentaire, Amélioration, Restauration). The abilities and auras
// named below are in localTools/shaman/Spells.ps1, the stock spells changed in place in StockSpells.ps1; the WotLK
// talents the trees reuse keep their own scripts in the core (spell_shaman.cpp).
//
// The Shaman keeps mana and its totems. Elemental fights with Maelstrom (Maelström), an aura of up to 100 stacks:
// Lightning Bolt, Chain Lightning, Lava Burst, Icefury and the empowered Frost Shocks fill it, Earth Shock, Earthquake
// and Elemental Blast spend it; Flame Shock's ticks bring Lava Surge, Lava Burst feeds Master of the Elements, and
// Stormkeeper and Icefury empower the next casts. Enhancement's melee hits charge Maelstrom Weapon (Arme du Maelström,
// up to 5 or 10 stacks) that makes Lightning Bolt, Chain Lightning and the heals faster and stronger; Crash Lightning
// carries Stormstrike, Lava Lash and Ice Strike over the pack, and Lava Lash spreads Flame Shock. Restoration has
// Riptide on charges, Healing Rain, Cloudburst, the Spirit Link and Healing Tide totems and Ascendance. Lava Burst and
// Riptide have charges kept here, shown as stacks.
//
// A spell whose hit lands later than its cast (Lightning Bolt, Elemental Blast and Icefury travel) gets the share of
// the procs it spent as it was cast; the others read them on the hit, which comes before the cast's end (OnSpellCast)
// spends them.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_ELEMENTAL_ORBIT          = 95303,    // Orbite élémentaire
    TALENT_MASTER_OF_ELEMENTS       = 95317,    // Maître des éléments
    TALENT_ECHO_OF_ELEMENTS         = 95318,    // Écho des éléments
    TALENT_ECHOES_OF_SUNDERING      = 95319,    // Échos de la Grande fracture
    TALENT_SURGE_1                  = 95321,    // Déferlante amplifiée
    TALENT_SURGE_2                  = 95322,
    TALENT_SWELLING_MAELSTROM       = 95325,    // Maelström gonflé
    TALENT_ETERNAL_STORM            = 95328,    // Tempête éternelle
    TALENT_ELEMENTAL_ASSAULT        = 95330,    // Assaut élémentaire
    TALENT_HOT_HAND_1               = 95331,    // Main brûlante
    TALENT_HOT_HAND_2               = 95332,
    TALENT_RAGING_MAELSTROM         = 95333,    // Maelström déchaîné
    TALENT_MOLTEN_ASSAULT           = 95334,    // Assaut en fusion
    TALENT_STORMFLURRY_1            = 95335,    // Rafale de tempête
    TALENT_STORMFLURRY_2            = 95336,
    TALENT_FROST_WITCH              = 95340,    // Héritage de la sorcière du givre
    TALENT_PRIMAL_MAELSTROM         = 95345,    // Maelström primordial
    TALENT_UNRULY_WINDS             = 95346,    // Vents indomptés
    TALENT_DELUGE_1                 = 95349,    // Déluge
    TALENT_DELUGE_2                 = 95350,
    TALENT_ECHO_OF_TIDE             = 95351,    // Écho de la marée
};

// The abilities and auras of localTools/shaman/Spells.ps1
enum Spells : uint32
{
    SPELL_MAELSTROM                 = 95400,
    SPELL_LAVA_SURGE                = 95401,
    SPELL_MAELSTROM_WEAPON          = 95402,
    SPELL_MASTER_OF_ELEMENTS        = 95403,
    SPELL_STORMKEEPER_BUFF          = 95404,
    SPELL_ICEFURY_BUFF              = 95405,
    SPELL_ECHOES_OF_SUNDERING       = 95406,
    SPELL_ELEMENTAL_BLAST_HASTE     = 95407,
    SPELL_ELEMENTAL_BLAST_CRIT      = 95408,
    SPELL_LAVA_BURST_CHARGES        = 95409,
    SPELL_RIPTIDE_CHARGES           = 95410,
    SPELL_HOT_HAND                  = 95411,
    SPELL_CRASH_LIGHTNING_BUFF      = 95412,
    SPELL_ICE_STRIKE_BUFF           = 95413,
    SPELL_FROST_WITCH               = 95414,
    SPELL_UNLEASH_LIFE_BUFF         = 95415,
    SPELL_CAPACITOR_TOTEM           = 95423,
    SPELL_ANCESTRAL_GUIDANCE        = 95426,
    SPELL_ANCESTRAL_GUIDANCE_HEAL   = 95427,
    SPELL_ORBIT_EARTH_SHIELD        = 95428,
    SPELL_CAPACITOR_STUN            = 95429,
    SPELL_EARTHQUAKE                = 95430,
    SPELL_ELEMENTAL_BLAST           = 95431,
    SPELL_STORMKEEPER               = 95432,
    SPELL_ICEFURY                   = 95433,
    SPELL_ASCENDANCE_ELEMENTAL      = 95434,
    SPELL_EARTHQUAKE_TICK           = 95435,
    SPELL_CRASH_LIGHTNING           = 95441,
    SPELL_ICE_STRIKE                = 95442,
    SPELL_SUNDERING                 = 95443,
    SPELL_DOOM_WINDS                = 95444,
    SPELL_CRASH_LIGHTNING_RELAY     = 95445,
    SPELL_ASCENDANCE_ENHANCEMENT    = 95454,
    SPELL_ASCENDANCE_WINDS          = 95455,
    SPELL_HEALING_RAIN              = 95470,
    SPELL_UNLEASH_LIFE              = 95471,
    SPELL_CLOUDBURST                = 95472,
    SPELL_SPIRIT_LINK               = 95473,
    SPELL_HEALING_TIDE              = 95474,
    SPELL_ASCENDANCE_RESTORATION    = 95475,
    SPELL_WELLSPRING                = 95476,
    SPELL_EARTHEN_WALL              = 95477,
    SPELL_EARTHEN_WALL_ABSORB       = 95478,
    SPELL_ASCENDANCE_HEAL           = 95479,
    SPELL_HEALING_RAIN_HEAL         = 95480,
    SPELL_CLOUDBURST_HEAL           = 95481,
    SPELL_SPIRIT_LINK_AURA          = 95482,
    SPELL_HEALING_TIDE_HEAL         = 95483,
    SPELL_WELLSPRING_HEAL           = 95484,

    // The specializations' passives
    SPELL_SPEC_ELEMENTAL            = 95580,
    SPELL_SPEC_ENHANCEMENT          = 95581,
    SPELL_SPEC_RESTORATION          = 95582,

    // Stock (first ranks)
    SPELL_HEALING_WAVE_R1           = 331,
    SPELL_LIGHTNING_BOLT_R1         = 403,
    SPELL_CHAIN_LIGHTNING_R1        = 421,
    SPELL_DUAL_WIELD                = 674,
    SPELL_EARTH_SHIELD_R1           = 974,
    SPELL_CHAIN_HEAL_R1             = 1064,
    SPELL_LESSER_HEALING_WAVE_R1    = 8004,
    SPELL_EARTH_SHOCK_R1            = 8042,
    SPELL_FLAME_SHOCK_R1            = 8050,
    SPELL_FROST_SHOCK_R1            = 8056,
    SPELL_STORMSTRIKE               = 17364,
    SPELL_STORMSTRIKE_MAIN_HAND     = 32175,
    SPELL_STORMSTRIKE_OFF_HAND      = 32176,
    SPELL_OVERLOAD_BOLT_R1          = 45284,
    SPELL_OVERLOAD_CHAIN_R1         = 45297,
    SPELL_LAVA_BURST_R1             = 51505,
    SPELL_FERAL_SPIRIT              = 51533,
    SPELL_LAVA_LASH                 = 60103,
    SPELL_RIPTIDE_R1                = 61295,
};

// Flame Shock (word 0) and Riptide (word 2), for the auras a target carries
constexpr uint32 FlagFlameShock = 0x10000000;
constexpr uint32 FlagRiptide = 0x10;
// The Flametongue Weapon's own aura on the Shaman (spell_sha_lava_lash reads it the same way)
constexpr uint32 FlagFlametongue = 0x200000;

// --- Tuning (README.md) ----------------------------------------------------------------------------------------------
// Elemental: Maelstrom
constexpr int32 MaelstromMax = 100;
constexpr int32 MaelstromSwollen = 150;            // Maelström gonflé
constexpr int32 MaelstromLightningBolt = 8;
constexpr int32 MaelstromChainTarget = 4;          // each enemy Chain Lightning hits
constexpr int32 MaelstromLavaBurst = 10;
constexpr int32 MaelstromIcefury = 25;
constexpr int32 MaelstromFrostShock = 8;           // an Icefury-empowered Frost Shock
constexpr int32 MaelstromOverload = 3;
constexpr int32 EarthShockCost = 60;
constexpr int32 EarthquakeCost = 60;
constexpr int32 ElementalBlastCost = 90;
constexpr uint32 MaelstromFadeMs = 15000;
// Elemental: the procs
constexpr int32 LavaSurgeChance = 15;               // a Flame Shock tick, +5% a rank of Déferlante amplifiée
constexpr float MasterOfElementsFactor = 1.2f;
constexpr float StormkeeperFactor = 2.5f;
constexpr uint8 StormkeeperStacks = 2;
constexpr uint8 EternalStormStacks = 3;
constexpr uint8 IcefuryStacks = 4;
constexpr float IcefuryFactor = 2.0f;
constexpr float EchoesOfSunderingFactor = 2.0f;
// Earthquake: a shake a second for 6 s within 8 yd of its spot
constexpr uint32 EarthquakeMs = 6000;
constexpr uint32 EarthquakePeriodMs = 1000;
constexpr float EarthquakeRadius = 8.0f;
constexpr float AscendanceLavaRange = 40.0f;
// Enhancement: Maelstrom Weapon
constexpr uint8 MaelstromWeaponMax = 5;
constexpr uint8 MaelstromWeaponRaging = 10;        // Maelström déchaîné
constexpr uint8 MaelstromWeaponSpent = 5;          // a cast spends at most this many
constexpr int32 MaelstromWeaponCastPct = 20;       // cast time taken off a stack
constexpr float MaelstromWeaponPerStack = 0.12f;   // damage or healing a stack spent adds
constexpr int32 MaelstromWeaponChance = 20;        // an auto attack
constexpr int32 UnrulyWindsChance = 35;
constexpr uint32 FeralSpiritMs = 45000;
constexpr uint32 FeralSpiritPeriodMs = 3000;
// Enhancement: the rest
constexpr int32 HotHandChance = 5;                 // a rank
constexpr float CrashLightningShare = 0.8f;
constexpr uint8 CrashLightningTargets = 6;
constexpr float CrashLightningRange = 8.0f;
constexpr uint32 CrashCountMs = 300;
constexpr float IceStrikeFactor = 2.0f;
constexpr int32 StormflurryChance = 15;            // a rank
constexpr uint8 MoltenAssaultTargets = 4;
constexpr float MoltenAssaultRange = 10.0f;
constexpr float AscendanceWindsRange = 8.0f;
// Restoration
constexpr float UnleashLifeFactor = 1.35f;
constexpr float DelugePerRank = 0.1f;
constexpr uint32 HealingRainMs = 10000;
constexpr uint32 HealingRainPeriodMs = 2000;
constexpr float HealingRainRadius = 10.0f;
constexpr uint8 HealingRainTargets = 6;
constexpr uint32 CloudburstMs = 15000;
constexpr float CloudburstShare = 0.3f;
constexpr uint8 CloudburstTargets = 6;
constexpr uint32 SpiritLinkMs = 6000;
constexpr uint32 SpiritLinkPeriodMs = 1000;
constexpr float SpiritLinkRadius = 12.0f;
constexpr uint32 HealingTideMs = 10000;
constexpr uint32 HealingTidePeriodMs = 2000;
constexpr float HealingTideRadius = 40.0f;
constexpr uint8 WellspringTargets = 6;
constexpr float WellspringRadius = 30.0f;
constexpr float EarthenWallRadius = 20.0f;
constexpr float EarthenWallSpellPower = 2.0f;      // the absorb, a share of spell power
constexpr float AscendanceHealShare = 0.5f;
constexpr uint8 AscendanceHealTargets = 5;
// Class tree
constexpr uint32 CapacitorDelayMs = 2000;
constexpr float AncestralGuidanceShare = 0.25f;
constexpr uint8 AncestralGuidanceTargets = 3;
constexpr float HealRange = 40.0f;
// Packs (the combat bench, Fire mage as the reference): each area hit whole up to AreaFullTargets enemies and
// sqrt(AreaFullTargets / enemies) of it past them. Eight rather than the casters' five: Earthquake and the melee areas
// are 8 yd across, so a big pack is already only partly in reach
constexpr uint8 AreaFullTargets = 8;
constexpr uint8 AreaMaxTargets = 20;
constexpr uint32 AreaCountMs = 300;
// A delayed hit keeps its share for this long after its cast at most
constexpr uint32 PendingBoostMs = 4000;
constexpr uint32 PoolFlushMs = 500;
constexpr uint32 HoldCheckMs = 1000;

enum ChargeKind : uint8
{
    CHARGE_LAVA_BURST,
    CHARGE_RIPTIDE,
    MAX_CHARGE_KINDS
};

struct ChargeDefinition
{
    uint32 spellId;
    uint32 displayAura;
};

constexpr std::array<ChargeDefinition, MAX_CHARGE_KINDS> ChargeDefinitions = {{
    { SPELL_LAVA_BURST_R1, SPELL_LAVA_BURST_CHARGES },
    { SPELL_RIPTIDE_R1, SPELL_RIPTIDE_CHARGES },
}};

struct Charges
{
    bool initialized = false;
    uint8 count = 0;
    int32 rechargeMs = 0;
    uint32 spellId = 0;             // the rank last cast
    bool clearCooldown = false;     // the cooldown its cast just started is taken back on the next update
};

// The share a travelling spell spent as it was cast, for its hit
struct PendingBoost
{
    uint32 spell = 0;
    uint32 untilMs = 0;
    float factor = 1.0f;
};

// An effect held on a spot of the ground (Earthquake, Healing Rain, Spirit Link)
struct GroundEffect
{
    uint32 untilMs = 0;
    uint32 nextMs = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float factor = 1.0f;

    bool Active() const { return untilMs != 0; }
    Position Spot() const { return Position(x, y, z); }
    void Start(Position const& spot, uint32 now, uint32 durationMs, uint32 firstTickMs)
    {
        x = spot.GetPositionX();
        y = spot.GetPositionY();
        z = spot.GetPositionZ();
        untilMs = now + durationMs;
        nextMs = now + firstTickMs;
        factor = 1.0f;
    }
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct ShamanState : public DataMap::Base
{
    std::array<Charges, MAX_CHARGE_KINDS> charges;
    std::deque<PendingBoost> pending;
    GroundEffect earthquake;
    GroundEffect healingRain;
    GroundEffect spiritLink;
    GroundEffect healingTide;
    GroundEffect capacitor;
    uint32 cloudburstUntilMs = 0;
    float cloudburstPool = 0.0f;
    float guidancePool = 0.0f;
    float ascendancePool = 0.0f;
    uint32 poolTimer = 0;
    uint32 wolvesUntilMs = 0;
    uint32 wolvesNextMs = 0;
    uint32 outOfCombatMs = 0;
    uint32 crashStamp = 0;
    uint8 crashHits = 0;
    ObjectGuid stormflurryTarget;
    bool clearLavaBurst = false;
    uint32 holdTimer = 0;
    // The enemies an area spell reaches, counted a few times a second
    uint32 areaSpell = 0;
    uint32 areaStamp = 0;
    uint8 areaCount = 1;
};

constexpr char const* StateKey = "ShamanTalentState";

ShamanState* GetState(Player* player)
{
    return player->CustomData.GetDefault<ShamanState>(StateKey);
}

Player* Shaman(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_SHAMAN ? player : nullptr;
}

uint32 Now()
{
    return uint32(GameTime::GetGameTimeMS().count());
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsElemental(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_ELEMENTAL);
}

bool IsEnhancement(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_ENHANCEMENT);
}

bool IsRestoration(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_RESTORATION);
}

// A two-rank talent's rank: 2, 1 or 0
uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

uint8 Stacks(Unit const* unit, uint32 spellId)
{
    Aura* aura = unit->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
}

// An aura of stacks on the Shaman at a count (none removes it); `effectAmount` keeps effect 0 at that amount whatever
// the stacks (the core multiplies it by them)
void ShowStacks(Player* player, uint32 spellId, uint8 stacks, Optional<int32> effectAmount = std::nullopt)
{
    if (!stacks)
    {
        player->RemoveAurasDueToSpell(spellId);
        return;
    }
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (!aura)
        return;
    aura->SetStackAmount(stacks);
    aura->RefreshDuration();
    if (effectAmount)
        if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
            effect->ChangeAmount(*effectAmount);
}

int32 Amount(float value)
{
    return int32(std::clamp(value, 1.0f, float(std::numeric_limits<int32>::max() / 2)));
}

float SpellPower(Player* player)
{
    return float(player->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
}

// Each hit's share on a pack of `enemies`: whole up to AreaFullTargets, then sqrt(AreaFullTargets / enemies)
float AreaFalloff(std::size_t enemies)
{
    return enemies <= AreaFullTargets ? 1.0f : std::sqrt(float(AreaFullTargets) / float(enemies));
}

// Enemies of the player within range of center, center left out
std::list<Unit*> EnemiesNear(Player* player, WorldObject* center, float range)
{
    std::list<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, targets, check);
    Cell::VisitObjects(center, searcher, range);
    targets.remove_if([player, center](Unit* unit)
    {
        return unit == center || !unit->IsAlive() || !player->IsValidAttackTarget(unit) ||
            unit->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
    });
    return targets;
}

// Enemies within `radius` of a spot (the player's search reaches it)
std::vector<Unit*> EnemiesAtSpot(Player* player, Position const& spot, float radius)
{
    std::vector<Unit*> enemies;
    float const reach = player->GetExactDist(&spot) + radius;
    for (Unit* enemy : EnemiesNear(player, player, reach))
    {
        if (enemy->GetExactDist(&spot) > radius + enemy->GetCombatReach())
            continue;
        enemies.push_back(enemy);
        if (enemies.size() >= AreaMaxTargets)
            break;
    }
    return enemies;
}

// The Shaman and its group's players, alive, within range of a spot
std::vector<Player*> GroupPlayersAt(Player* player, Position const& spot, float range)
{
    std::vector<Player*> players;
    Group* group = player->GetGroup();
    if (!group)
    {
        if (player->IsAlive() && player->GetExactDist(&spot) <= range)
            players.push_back(player);
        return players;
    }
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->IsInMap(player) && member->GetExactDist(&spot) <= range)
            players.push_back(member);
    }
    return players;
}

// The injured among them, most injured first, `count` at most
std::vector<Player*> InjuredAt(Player* player, Position const& spot, float range, std::size_t count)
{
    std::vector<Player*> injured = GroupPlayersAt(player, spot, range);
    std::erase_if(injured, [](Player* member) { return member->IsFullHealth(); });
    std::sort(injured.begin(), injured.end(), [](Player const* left, Player const* right)
    {
        return left->GetHealthPct() < right->GetHealthPct();
    });
    if (injured.size() > count)
        injured.resize(count);
    return injured;
}

void Strike(Player* player, Unit* target, uint32 spellId, float amount)
{
    if (!target || amount < 1.0f || !target->IsAlive())
        return;
    int32 const damage = Amount(amount);
    player->CastCustomSpell(target, spellId, &damage, nullptr, nullptr, true);
}

void Heal(Player* player, Unit* target, uint32 spellId, float amount)
{
    if (!target || amount < 1.0f || !target->IsAlive())
        return;
    int32 const heal = Amount(amount);
    player->CastCustomSpell(target, spellId, &heal, nullptr, nullptr, true);
}

// A pool of healing shared out evenly among the most injured near the Shaman
void SharePool(Player* player, float& pool, uint32 spellId, uint8 count)
{
    if (pool < 1.0f)
    {
        pool = 0.0f;
        return;
    }
    std::vector<Player*> const injured = InjuredAt(player, *player, HealRange, count);
    if (!injured.empty())
    {
        float const each = pool / float(injured.size());
        for (Player* member : injured)
            Heal(player, member, spellId, each);
    }
    pool = 0.0f;
}

// Every rank of a spell the player knows, its cooldown taken back
void ClearChainCooldown(Player* player, uint32 spellId)
{
    for (uint32 rank = sSpellMgr->GetFirstSpellInChain(spellId); rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpellCooldown(rank))
            player->RemoveSpellCooldown(rank, true);
}

// The highest rank of a chain the player knows, 0 without one
uint32 KnownRank(Player* player, uint32 firstRank)
{
    uint32 known = 0;
    for (uint32 rank = firstRank; rank; rank = sSpellMgr->GetNextSpellInChain(rank))
        if (player->HasSpell(rank))
            known = rank;
    return known;
}

bool HasFlameShock(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_DAMAGE, SPELLFAMILY_SHAMAN, FlagFlameShock, 0, 0,
        player->GetGUID());
}

bool HasRiptide(Player* player, Unit* target)
{
    return target->GetAuraEffect(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_SHAMAN, 0, 0, FlagRiptide, player->GetGUID());
}

bool IsBolt(uint32 firstRank)
{
    return firstRank == SPELL_LIGHTNING_BOLT_R1 || firstRank == SPELL_CHAIN_LIGHTNING_R1;
}

bool IsWaveHeal(uint32 firstRank)
{
    return firstRank == SPELL_HEALING_WAVE_R1 || firstRank == SPELL_LESSER_HEALING_WAVE_R1 ||
        firstRank == SPELL_CHAIN_HEAL_R1;
}

// --- Maelstrom (Elemental) -------------------------------------------------------------------------------------------

int32 Maelstrom(Player const* player)
{
    return Stacks(player, SPELL_MAELSTROM);
}

int32 MaelstromCap(Player const* player)
{
    return player->HasAura(TALENT_SWELLING_MAELSTROM) ? MaelstromSwollen : MaelstromMax;
}

void AddMaelstrom(Player* player, int32 amount)
{
    if (!amount || !IsElemental(player))
        return;
    int32 const value = std::clamp(Maelstrom(player) + amount, 0, MaelstromCap(player));
    ShowStacks(player, SPELL_MAELSTROM, uint8(value));
}

// What a spender costs, 0 for anything else (Earth Shock only for Elemental)
int32 MaelstromCost(Player const* player, uint32 id, uint32 firstRank)
{
    if (!IsElemental(player))
        return 0;
    if (firstRank == SPELL_EARTH_SHOCK_R1)
        return EarthShockCost;
    if (id == SPELL_EARTHQUAKE)
        return EarthquakeCost;
    if (id == SPELL_ELEMENTAL_BLAST)
        return ElementalBlastCost;
    return 0;
}

// --- Maelstrom Weapon (Enhancement) ----------------------------------------------------------------------------------

uint8 WeaponStacks(Player const* player)
{
    return Stacks(player, SPELL_MAELSTROM_WEAPON);
}

void SetWeaponStacks(Player* player, uint8 stacks)
{
    uint8 const cap = player->HasAura(TALENT_RAGING_MAELSTROM) ? MaelstromWeaponRaging : MaelstromWeaponMax;
    stacks = std::min(stacks, cap);
    // The cast time a cast takes off: 20% a stack, of the 5 a cast spends at most
    ShowStacks(player, SPELL_MAELSTROM_WEAPON, stacks,
        -MaelstromWeaponCastPct * int32(std::min(stacks, MaelstromWeaponSpent)));
}

void AddWeaponStacks(Player* player, uint8 stacks)
{
    if (stacks && IsEnhancement(player))
        SetWeaponStacks(player, uint8(std::min<uint32>(WeaponStacks(player) + stacks, 255)));
}

// --- Charges ---------------------------------------------------------------------------------------------------------

uint8 MaxCharges(Player* player, ChargeKind kind)
{
    switch (kind)
    {
        case CHARGE_LAVA_BURST:
            return IsElemental(player) && player->HasAura(TALENT_ECHO_OF_ELEMENTS) ? 2 : 1;
        case CHARGE_RIPTIDE:
            return IsRestoration(player) && player->HasAura(TALENT_ECHO_OF_TIDE) ? 2 : 1;
        default:
            return 1;
    }
}

// A rank's cooldown with the Shaman's modifiers
int32 Recharge(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(std::max(spellInfo->RecoveryTime, spellInfo->CategoryRecoveryTime)) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

void SpendCharge(Player* player, ShamanState* state, ChargeKind kind, uint32 spellId)
{
    uint8 const max = MaxCharges(player, kind);
    Charges& charges = state->charges[kind];
    charges.spellId = spellId;
    if (max <= 1)
        return;
    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = max;
    }
    if (charges.count >= max)
        charges.rechargeMs = Recharge(player, spellId);
    if (charges.count > 0)
        --charges.count;
    charges.clearCooldown = charges.count > 0;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

// One charge back at once (Lava Surge), or the cooldown cleared without charges
void RefundCharge(Player* player, ShamanState* state, ChargeKind kind)
{
    uint8 const max = MaxCharges(player, kind);
    Charges& charges = state->charges[kind];
    if (max <= 1 || !charges.initialized)
    {
        ClearChainCooldown(player, ChargeDefinitions[kind].spellId);
        return;
    }
    if (charges.count >= max)
        return;
    if (!charges.count)
        ClearChainCooldown(player, ChargeDefinitions[kind].spellId);
    ++charges.count;
    if (charges.count >= max)
        charges.rechargeMs = 0;
    ShowStacks(player, ChargeDefinitions[kind].displayAura, charges.count);
}

void UpdateCharges(Player* player, ShamanState* state, ChargeKind kind, uint32 diff)
{
    Charges& charges = state->charges[kind];
    uint8 const max = MaxCharges(player, kind);
    uint32 const display = ChargeDefinitions[kind].displayAura;
    if (max <= 1)
    {
        if (charges.initialized || player->HasAura(display))
        {
            charges = Charges();
            player->RemoveAurasDueToSpell(display);
        }
        return;
    }

    if (!charges.initialized)
    {
        charges.initialized = true;
        charges.count = max;
    }

    uint32 const spellId = charges.spellId ? charges.spellId : ChargeDefinitions[kind].spellId;
    if (charges.clearCooldown)
    {
        charges.clearCooldown = false;
        ClearChainCooldown(player, spellId);
    }

    if (charges.count >= max)
    {
        charges.count = max;
        if (Stacks(player, display) != max)
            ShowStacks(player, display, max);
        return;
    }

    charges.rechargeMs -= int32(diff);
    if (charges.rechargeMs > 0)
        return;

    // A charge back: usable again if it was the last one gone
    if (!charges.count)
        ClearChainCooldown(player, spellId);
    ++charges.count;
    charges.rechargeMs = charges.count < max ? Recharge(player, spellId) + charges.rechargeMs : 0;
    ShowStacks(player, display, charges.count);
}

// --- The procs a cast spends ----------------------------------------------------------------------------------------

// The share a spell's hit gains from the procs its cast spends, read off the Shaman's auras as they are now
float SpentBoost(Player* player, uint32 id, uint32 firstRank)
{
    float factor = 1.0f;
    if (IsBolt(firstRank) || IsWaveHeal(firstRank))
        if (uint8 const stacks = std::min(WeaponStacks(player), MaelstromWeaponSpent))
            factor *= 1.0f + MaelstromWeaponPerStack * float(stacks);
    if (IsBolt(firstRank) && player->HasAura(SPELL_STORMKEEPER_BUFF))
        factor *= StormkeeperFactor;
    if ((IsBolt(firstRank) || firstRank == SPELL_EARTH_SHOCK_R1 || firstRank == SPELL_FROST_SHOCK_R1 ||
         id == SPELL_ELEMENTAL_BLAST || id == SPELL_ICEFURY || id == SPELL_EARTHQUAKE) &&
        player->HasAura(SPELL_MASTER_OF_ELEMENTS))
        factor *= MasterOfElementsFactor;
    if (firstRank == SPELL_FROST_SHOCK_R1)
    {
        if (player->HasAura(SPELL_ICEFURY_BUFF))
            factor *= IcefuryFactor;
        if (player->HasAura(SPELL_ICE_STRIKE_BUFF))
            factor *= IceStrikeFactor;
    }
    if (IsWaveHeal(firstRank) && player->HasAura(SPELL_UNLEASH_LIFE_BUFF))
        factor *= UnleashLifeFactor;
    return factor;
}

bool SpendsProcs(uint32 id, uint32 firstRank)
{
    return IsBolt(firstRank) || IsWaveHeal(firstRank) || firstRank == SPELL_EARTH_SHOCK_R1 ||
        firstRank == SPELL_FROST_SHOCK_R1 || id == SPELL_ELEMENTAL_BLAST ||
        id == SPELL_ICEFURY || id == SPELL_EARTHQUAKE;
}

// A travelling spell's share, taken by its hit (the oldest cast first)
float TakePending(ShamanState* state, uint32 spell)
{
    uint32 const now = Now();
    while (!state->pending.empty() && now > state->pending.front().untilMs)
        state->pending.pop_front();
    for (auto itr = state->pending.begin(); itr != state->pending.end(); ++itr)
        if (itr->spell == spell)
        {
            float const factor = itr->factor;
            state->pending.erase(itr);
            return factor;
        }
    return 1.0f;
}

// The cast spends what boosted it: Maelstrom Weapon (5 stacks at most), a Stormkeeper stack, Master of the Elements,
// an Icefury stack, Ice Strike's and Unleash Life's boons (Riptide's ticks never take Unleash Life's)
void SpendProcs(Player* player, uint32 firstRank)
{
    if (IsBolt(firstRank) || IsWaveHeal(firstRank))
        if (uint8 const stacks = WeaponStacks(player))
        {
            uint8 const spent = std::min(stacks, MaelstromWeaponSpent);
            SetWeaponStacks(player, stacks - spent);
            // Héritage de la sorcière du givre
            if (spent >= MaelstromWeaponSpent && player->HasAura(TALENT_FROST_WITCH))
            {
                ClearChainCooldown(player, SPELL_STORMSTRIKE);
                player->AddAura(SPELL_FROST_WITCH, player);
            }
        }
    if (IsBolt(firstRank))
        if (uint8 const stacks = Stacks(player, SPELL_STORMKEEPER_BUFF))
            ShowStacks(player, SPELL_STORMKEEPER_BUFF, stacks - 1, -100);
    if (firstRank != SPELL_LAVA_BURST_R1)
        player->RemoveAurasDueToSpell(SPELL_MASTER_OF_ELEMENTS);
    if (firstRank == SPELL_FROST_SHOCK_R1)
    {
        if (uint8 const stacks = Stacks(player, SPELL_ICEFURY_BUFF))
        {
            ShowStacks(player, SPELL_ICEFURY_BUFF, stacks - 1);
            AddMaelstrom(player, MaelstromFrostShock);
        }
        player->RemoveAurasDueToSpell(SPELL_ICE_STRIKE_BUFF);
    }
    if (IsWaveHeal(firstRank))
        player->RemoveAurasDueToSpell(SPELL_UNLEASH_LIFE_BUFF);
}

// --- Spell casts -----------------------------------------------------------------------------------------------------

class ShamanTalentSpellScript : public AllSpellScript
{
public:
    ShamanTalentSpellScript() : AllSpellScript("ShamanTalentSpellScript", {
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST,
        ALLSPELLHOOK_ON_CAST
    }) { }

    // A spell kept on charges cannot be cast with none left; a Maelstrom spender needs its Maelstrom
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        if (result != SPELL_CAST_OK || spell->IsTriggered())
            return;
        Player* player = Shaman(spell->GetCaster());
        if (!player)
            return;
        SpellInfo const* spellInfo = spell->GetSpellInfo();
        uint32 const firstRank = FirstRank(spellInfo);
        ShamanState* state = GetState(player);
        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
        {
            if (ChargeDefinitions[kind].spellId != firstRank || MaxCharges(player, ChargeKind(kind)) <= 1)
                continue;
            Charges const& charges = state->charges[kind];
            if (charges.initialized && !charges.count)
            {
                result = SPELL_FAILED_NOT_READY;
                return;
            }
        }

        if (int32 const cost = MaelstromCost(player, spellInfo->Id, firstRank))
            if (Maelstrom(player) < cost)
                result = SPELL_FAILED_CASTER_AURASTATE;
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Shaman(caster);
        if (!player || !spellInfo || spell->IsTriggered())
            return;
        ShamanState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);
        uint32 const now = Now();

        // A travelling hit keeps the share its cast spent; the procs go now
        if (SpendsProcs(id, firstRank))
        {
            float const factor = SpentBoost(player, id, firstRank);
            if (id == SPELL_EARTHQUAKE)
                state->earthquake.factor = factor;
            else if (spellInfo->Speed > 0.0f && factor != 1.0f)
            {
                state->pending.push_back({ firstRank, now + PendingBoostMs, factor });
                if (state->pending.size() > 8)
                    state->pending.pop_front();
            }
            SpendProcs(player, firstRank);
        }

        // Maelstrom spent (Earth Shock costs no mana then)
        if (int32 const cost = MaelstromCost(player, id, firstRank))
        {
            AddMaelstrom(player, -cost);
            if (firstRank == SPELL_EARTH_SHOCK_R1 && spell->GetPowerCost() > 0)
                player->ModifyPower(POWER_MANA, spell->GetPowerCost());
        }

        switch (id)
        {
            // Class tree
            case SPELL_CAPACITOR_TOTEM:
                state->capacitor.Start(*player, now, CapacitorDelayMs, CapacitorDelayMs);
                return;
            // Elemental
            case SPELL_EARTHQUAKE:
            {
                Position const spot = spell->m_targets.HasDst() ? Position(*spell->m_targets.GetDstPos()) :
                    Position(target ? *target : *player);
                float const factor = state->earthquake.factor;
                state->earthquake.Start(spot, now, EarthquakeMs, EarthquakePeriodMs / 2);
                state->earthquake.factor = factor;
                if (player->HasAura(SPELL_ECHOES_OF_SUNDERING))
                {
                    state->earthquake.factor *= EchoesOfSunderingFactor;
                    player->RemoveAurasDueToSpell(SPELL_ECHOES_OF_SUNDERING);
                }
                return;
            }
            case SPELL_ELEMENTAL_BLAST:
                player->AddAura(urand(0, 1) ? SPELL_ELEMENTAL_BLAST_HASTE : SPELL_ELEMENTAL_BLAST_CRIT, player);
                if (player->HasAura(TALENT_ECHOES_OF_SUNDERING))
                    player->AddAura(SPELL_ECHOES_OF_SUNDERING, player);
                return;
            case SPELL_STORMKEEPER:
                ShowStacks(player, SPELL_STORMKEEPER_BUFF,
                    player->HasAura(TALENT_ETERNAL_STORM) ? EternalStormStacks : StormkeeperStacks, -100);
                return;
            case SPELL_ICEFURY:
                ShowStacks(player, SPELL_ICEFURY_BUFF, IcefuryStacks);
                AddMaelstrom(player, MaelstromIcefury);
                return;
            case SPELL_ASCENDANCE_ELEMENTAL:
                AscendanceLava(player);
                RefundCharge(player, state, CHARGE_LAVA_BURST);
                return;
            // Enhancement
            case SPELL_ICE_STRIKE:
                player->AddAura(SPELL_ICE_STRIKE_BUFF, player);
                ElementalAssault(player);
                return;
            case SPELL_ASCENDANCE_ENHANCEMENT:
                AscendanceWinds(player, target ? target : player->GetVictim());
                ClearChainCooldown(player, SPELL_STORMSTRIKE);
                return;
            case SPELL_STORMSTRIKE:
            case SPELL_LAVA_LASH:
                ElementalAssault(player);
                return;
            case SPELL_FERAL_SPIRIT:
                state->wolvesUntilMs = now + FeralSpiritMs;
                state->wolvesNextMs = now + FeralSpiritPeriodMs;
                return;
            // Restoration
            case SPELL_HEALING_RAIN:
            {
                Position const spot = spell->m_targets.HasDst() ? Position(*spell->m_targets.GetDstPos()) :
                    Position(target ? *target : *player);
                state->healingRain.Start(spot, now, HealingRainMs, 0);
                return;
            }
            case SPELL_UNLEASH_LIFE:
                player->AddAura(SPELL_UNLEASH_LIFE_BUFF, player);
                return;
            case SPELL_CLOUDBURST:
                state->cloudburstUntilMs = now + CloudburstMs;
                state->cloudburstPool = 0.0f;
                return;
            case SPELL_SPIRIT_LINK:
                state->spiritLink.Start(*player, now, SpiritLinkMs, 0);
                return;
            case SPELL_HEALING_TIDE:
                state->healingTide.Start(*player, now, HealingTideMs, 0);
                return;
            case SPELL_WELLSPRING:
                for (Player* member : InjuredAt(player, *player, WellspringRadius, WellspringTargets))
                    player->CastSpell(member, SPELL_WELLSPRING_HEAL, true);
                return;
            case SPELL_EARTHEN_WALL:
            {
                int32 const absorb = Amount(SpellPower(player) * EarthenWallSpellPower);
                for (Player* member : GroupPlayersAt(player, *player, EarthenWallRadius))
                    player->CastCustomSpell(member, SPELL_EARTHEN_WALL_ABSORB, &absorb, nullptr, nullptr, true);
                return;
            }
            default:
                break;
        }

        switch (firstRank)
        {
            case SPELL_LIGHTNING_BOLT_R1:
                AddMaelstrom(player, MaelstromLightningBolt);
                break;
            case SPELL_LAVA_BURST_R1:
                SpendCharge(player, state, CHARGE_LAVA_BURST, id);
                player->RemoveAurasDueToSpell(SPELL_LAVA_SURGE);
                AddMaelstrom(player, MaelstromLavaBurst);
                if (player->HasAura(TALENT_MASTER_OF_ELEMENTS))
                    player->AddAura(SPELL_MASTER_OF_ELEMENTS, player);
                // Ascension: no cooldown while it lasts
                if (player->HasAura(SPELL_ASCENDANCE_ELEMENTAL))
                {
                    RefundCharge(player, state, CHARGE_LAVA_BURST);
                    state->clearLavaBurst = true;
                }
                break;
            case SPELL_EARTH_SHOCK_R1:
                if (IsElemental(player) && player->HasAura(TALENT_ECHOES_OF_SUNDERING))
                    player->AddAura(SPELL_ECHOES_OF_SUNDERING, player);
                break;
            case SPELL_RIPTIDE_R1:
                SpendCharge(player, state, CHARGE_RIPTIDE, id);
                break;
            case SPELL_EARTH_SHIELD_R1:
                // Orbite élémentaire: a shield on an ally, one on the Shaman too
                if (target && target != player && player->HasAura(TALENT_ELEMENTAL_ORBIT))
                    player->AddAura(SPELL_ORBIT_EARTH_SHIELD, player);
                break;
            default:
                break;
        }
    }

private:
    // Assaut élémentaire: a strike charges Maelstrom Weapon (twice with Maelström primordial)
    static void ElementalAssault(Player* player)
    {
        if (player->HasAura(TALENT_ELEMENTAL_ASSAULT))
            AddWeaponStacks(player, player->HasAura(TALENT_PRIMAL_MAELSTROM) ? 2 : 1);
    }

    // Ascension (Elemental): a Lava Burst at every enemy with the Shaman's Flame Shock
    static void AscendanceLava(Player* player)
    {
        uint32 const lavaBurst = KnownRank(player, SPELL_LAVA_BURST_R1);
        if (!lavaBurst)
            return;
        uint8 cast = 0;
        for (Unit* enemy : EnemiesNear(player, player, AscendanceLavaRange))
        {
            if (cast >= AreaMaxTargets)
                break;
            if (!HasFlameShock(player, enemy))
                continue;
            player->CastSpell(enemy, lavaBurst, true);
            ++cast;
        }
    }

    // Ascension (Enhancement): the winds strike the target and the enemies near it
    static void AscendanceWinds(Player* player, Unit* target)
    {
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
            return;
        player->CastSpell(target, SPELL_ASCENDANCE_WINDS, true);
        uint8 hit = 1;
        for (Unit* enemy : EnemiesNear(player, target, AscendanceWindsRange))
        {
            if (hit++ >= AreaMaxTargets)
                break;
            player->CastSpell(enemy, SPELL_ASCENDANCE_WINDS, true);
        }
    }
};

// --- Damage and healing ----------------------------------------------------------------------------------------------

// The area spells whose hits fall off past five enemies
bool IsAreaSpell(uint32 id)
{
    switch (id)
    {
        case SPELL_CRASH_LIGHTNING:
        case SPELL_SUNDERING:
        case SPELL_EARTHQUAKE_TICK:
        case SPELL_ASCENDANCE_WINDS:
            return true;
        default:
            return false;
    }
}

// The heals the module hands out from a pool: never counted into one again
bool IsRelayHeal(uint32 id)
{
    switch (id)
    {
        case SPELL_ANCESTRAL_GUIDANCE_HEAL:
        case SPELL_ASCENDANCE_HEAL:
        case SPELL_CLOUDBURST_HEAL:
            return true;
        default:
            return false;
    }
}

class ShamanTalentUnitScript : public UnitScript
{
public:
    ShamanTalentUnitScript() : UnitScript("ShamanTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Guidance ancestrale counts every damage the Shaman deals
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || !damage || attacker == victim)
            return;
        Player* player = Shaman(attacker);
        if (!player || victim->IsFriendlyTo(player) || !player->HasAura(SPELL_ANCESTRAL_GUIDANCE))
            return;
        GetState(player)->guidancePool += float(damage) * AncestralGuidanceShare;
    }

    // Maelstrom Weapon from the swings; Main brûlante
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        Player* player = Shaman(attacker);
        if (!player || !target || !damage || !IsEnhancement(player))
            return;
        int32 const chance = player->HasAura(SPELL_DOOM_WINDS) ? 100 :
            player->HasAura(TALENT_UNRULY_WINDS) ? UnrulyWindsChance : MaelstromWeaponChance;
        if (roll_chance_i(chance))
            AddWeaponStacks(player, 1);
        if (uint8 const rank = Rank(player, TALENT_HOT_HAND_1, TALENT_HOT_HAND_2))
            if (!player->HasAura(SPELL_HOT_HAND) && roll_chance_i(HotHandChance * rank) &&
                player->GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_SHAMAN, FlagFlametongue, 0, 0))
            {
                player->AddAura(SPELL_HOT_HAND, player);
                ClearChainCooldown(player, SPELL_LAVA_LASH);
            }
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || damage <= 0)
            return;
        Player* player = Shaman(attacker);
        if (!player)
            return;
        ShamanState* state = GetState(player);
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = 1.0f;
        if (SpendsProcs(id, firstRank))
            factor *= spellInfo->Speed > 0.0f ? TakePending(state, firstRank) : SpentBoost(player, id, firstRank);
        if (id == SPELL_EARTHQUAKE_TICK)
            factor *= state->earthquake.factor;
        if (IsAreaSpell(id))
            factor *= AreaFalloff(AreaCount(player, spellInfo, target));
        if (factor != 1.0f)
            damage = int32(float(damage) * factor);
    }

    // Lava Surge from Flame Shock's ticks
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !damage || spellInfo->SpellFamilyName != SPELLFAMILY_SHAMAN ||
            !(spellInfo->SpellFamilyFlags[0] & FlagFlameShock))
            return;
        Player* player = Shaman(attacker);
        if (!player || !IsElemental(player))
            return;
        int32 const chance = LavaSurgeChance + 5 * Rank(player, TALENT_SURGE_1, TALENT_SURGE_2);
        if (!roll_chance_i(chance))
            return;
        player->AddAura(SPELL_LAVA_SURGE, player);
        RefundCharge(player, GetState(player), CHARGE_LAVA_BURST);
    }

    // The Shaman's heals: the procs they spend, Déluge; Cloudburst, Ancestral Guidance and Ascension gather a share
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (!target || !spellInfo || !heal)
            return;
        Player* player = Shaman(healer);
        if (!player)
            return;
        uint32 const id = spellInfo->Id;
        if (IsRelayHeal(id))
            return;
        uint32 const firstRank = FirstRank(spellInfo);
        float factor = 1.0f;
        if (SpendsProcs(id, firstRank))
            factor *= SpentBoost(player, id, firstRank);
        if (IsWaveHeal(firstRank))
            if (uint8 const rank = Rank(player, TALENT_DELUGE_1, TALENT_DELUGE_2))
                if (HasRiptide(player, target))
                    factor *= 1.0f + DelugePerRank * float(rank);
        if (factor != 1.0f)
            heal = uint32(float(heal) * factor);

        float const effective = float(std::min<uint32>(heal, target->GetMaxHealth() - target->GetHealth()));
        if (effective <= 0.0f)
            return;
        ShamanState* state = GetState(player);
        if (state->cloudburstUntilMs)
            state->cloudburstPool += effective * CloudburstShare;
        if (player->HasAura(SPELL_ANCESTRAL_GUIDANCE))
            state->guidancePool += effective * AncestralGuidanceShare;
        if (player->HasAura(SPELL_ASCENDANCE_RESTORATION))
            state->ascendancePool += effective * AscendanceHealShare;
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = Shaman(caster);
        if (!player || !victim || !spellInfo || !damage)
            return;
        ShamanState* state = GetState(player);
        uint32 const id = spellInfo->Id;
        uint32 const firstRank = FirstRank(spellInfo);

        switch (firstRank)
        {
            case SPELL_CHAIN_LIGHTNING_R1:
                AddMaelstrom(player, MaelstromChainTarget);
                return;
            case SPELL_OVERLOAD_BOLT_R1:
            case SPELL_OVERLOAD_CHAIN_R1:
                AddMaelstrom(player, MaelstromOverload);
                return;
            default:
                break;
        }

        switch (id)
        {
            // Foudre écrasante: two enemies or more, its boon
            case SPELL_CRASH_LIGHTNING:
            {
                uint32 const now = Now();
                if (now - state->crashStamp > CrashCountMs)
                {
                    state->crashStamp = now;
                    state->crashHits = 0;
                }
                if (++state->crashHits >= 2)
                    player->AddAura(SPELL_CRASH_LIGHTNING_BUFF, player);
                return;
            }
            case SPELL_STORMSTRIKE_MAIN_HAND:
                CrashRelay(player, victim, damage);
                if (uint8 const rank = Rank(player, TALENT_STORMFLURRY_1, TALENT_STORMFLURRY_2))
                    if (roll_chance_i(StormflurryChance * rank))
                        state->stormflurryTarget = victim->GetGUID();
                return;
            case SPELL_STORMSTRIKE_OFF_HAND:
            case SPELL_ICE_STRIKE:
                CrashRelay(player, victim, damage);
                return;
            case SPELL_LAVA_LASH:
                CrashRelay(player, victim, damage);
                MoltenAssault(player, victim);
                return;
            default:
                break;
        }
    }

private:
    // Foudre écrasante's boon: the strike also hits the enemies near its target for a share of its damage
    static void CrashRelay(Player* player, Unit* victim, uint32 damage)
    {
        if (!player->HasAura(SPELL_CRASH_LIGHTNING_BUFF))
            return;
        uint8 hit = 0;
        for (Unit* enemy : EnemiesNear(player, victim, CrashLightningRange))
        {
            if (hit++ >= CrashLightningTargets)
                break;
            Strike(player, enemy, SPELL_CRASH_LIGHTNING_RELAY, float(damage) * CrashLightningShare);
        }
    }

    // Assaut en fusion: Lava Lash carries the target's Flame Shock to the enemies near it
    static void MoltenAssault(Player* player, Unit* victim)
    {
        if (!player->HasAura(TALENT_MOLTEN_ASSAULT) || !HasFlameShock(player, victim))
            return;
        uint32 const flameShock = KnownRank(player, SPELL_FLAME_SHOCK_R1);
        if (!flameShock)
            return;
        uint8 spread = 0;
        for (Unit* enemy : EnemiesNear(player, victim, MoltenAssaultRange))
        {
            if (spread >= MoltenAssaultTargets)
                break;
            if (HasFlameShock(player, enemy))
                continue;
            player->CastSpell(enemy, flameShock, true);
            ++spread;
        }
    }

    // The enemies around the target an area spell reaches (the target included), counted a few times a second
    static uint8 AreaCount(Player* player, SpellInfo const* spellInfo, Unit* target)
    {
        ShamanState* state = GetState(player);
        uint32 const now = Now();
        if (state->areaSpell == spellInfo->Id && now - state->areaStamp <= AreaCountMs)
            return state->areaCount;
        float radius = spellInfo->Effects[EFFECT_0].CalcRadius(player);
        if (radius <= 0.0f)
            radius = EarthquakeRadius;
        state->areaSpell = spellInfo->Id;
        state->areaStamp = now;
        state->areaCount = uint8(std::min<std::size_t>(EnemiesNear(player, target, radius).size() + 1,
            std::numeric_limits<uint8>::max()));
        return state->areaCount;
    }
};

// --- Every update: charges, the effects held on the ground, the pools, Maelstrom out of combat -----------------------

class ShamanTalentPlayerScript : public PlayerScript
{
public:
    ShamanTalentPlayerScript() : PlayerScript("ShamanTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_SHAMAN || !player->IsInWorld())
            return;
        ShamanState* state = GetState(player);

        for (uint8 kind = 0; kind < MAX_CHARGE_KINDS; ++kind)
            UpdateCharges(player, state, ChargeKind(kind), diff);

        if (state->clearLavaBurst)
        {
            state->clearLavaBurst = false;
            ClearChainCooldown(player, SPELL_LAVA_BURST_R1);
        }

        uint32 const now = Now();
        UpdateEarthquake(player, state, now);
        UpdateHealingRain(player, state, now);
        UpdateSpiritLink(player, state, now);
        UpdateHealingTide(player, state, now);
        UpdateCapacitor(player, state, now);
        UpdateStormflurry(player, state);

        state->poolTimer += diff;
        if (state->poolTimer >= PoolFlushMs)
        {
            state->poolTimer = 0;
            UpdatePools(player, state, now);
        }

        state->holdTimer += diff;
        if (state->holdTimer < HoldCheckMs)
            return;
        uint32 const elapsed = state->holdTimer;
        state->holdTimer = 0;
        UpdateWolves(player, state, now);
        UpdateMaelstrom(player, state, elapsed);
        UpdateHolds(player);
    }

private:
    // Earthquake: every second, the enemies within 8 yd of its spot
    static void UpdateEarthquake(Player* player, ShamanState* state, uint32 now)
    {
        GroundEffect& quake = state->earthquake;
        if (!quake.Active() || now < quake.nextMs)
            return;
        if (now > quake.untilMs + EarthquakePeriodMs / 2 || !player->IsAlive())
        {
            quake.untilMs = 0;
            quake.factor = 1.0f;
            return;
        }
        quake.nextMs += EarthquakePeriodMs;
        for (Unit* enemy : EnemiesAtSpot(player, quake.Spot(), EarthquakeRadius))
            player->CastSpell(enemy, SPELL_EARTHQUAKE_TICK, true);
    }

    // Healing Rain: every 2 s, up to 6 allies within 10 yd of its spot, the most injured first
    static void UpdateHealingRain(Player* player, ShamanState* state, uint32 now)
    {
        GroundEffect& rain = state->healingRain;
        if (!rain.Active() || now < rain.nextMs)
            return;
        if (now > rain.untilMs || !player->IsAlive())
        {
            rain.untilMs = 0;
            return;
        }
        rain.nextMs += HealingRainPeriodMs;
        for (Player* member : InjuredAt(player, rain.Spot(), HealingRainRadius, HealingRainTargets))
            player->CastSpell(member, SPELL_HEALING_RAIN_HEAL, true);
    }

    // Spirit Link Totem: every second, the group within 12 yd of it shares its health evenly and takes less damage
    static void UpdateSpiritLink(Player* player, ShamanState* state, uint32 now)
    {
        GroundEffect& link = state->spiritLink;
        if (!link.Active() || now < link.nextMs)
            return;
        if (now > link.untilMs || !player->IsAlive())
        {
            link.untilMs = 0;
            return;
        }
        link.nextMs += SpiritLinkPeriodMs;
        std::vector<Player*> const members = GroupPlayersAt(player, link.Spot(), SpiritLinkRadius);
        if (members.empty())
            return;
        float share = 0.0f;
        for (Player* member : members)
            share += member->GetHealthPct();
        share /= float(members.size());
        for (Player* member : members)
        {
            member->SetHealth(std::max<uint32>(1, uint32(float(member->GetMaxHealth()) * share / 100.0f)));
            player->AddAura(SPELL_SPIRIT_LINK_AURA, member);
        }
    }

    // Healing Tide Totem: every 2 s, the group within 40 yd of it
    static void UpdateHealingTide(Player* player, ShamanState* state, uint32 now)
    {
        GroundEffect& tide = state->healingTide;
        if (!tide.Active() || now < tide.nextMs)
            return;
        if (now > tide.untilMs || !player->IsAlive())
        {
            tide.untilMs = 0;
            return;
        }
        tide.nextMs += HealingTidePeriodMs;
        for (Player* member : GroupPlayersAt(player, tide.Spot(), HealingTideRadius))
            if (!member->IsFullHealth())
                player->CastSpell(member, SPELL_HEALING_TIDE_HEAL, true);
    }

    // Capacitor Totem: charged, it stuns the enemies around its spot
    static void UpdateCapacitor(Player* player, ShamanState* state, uint32 now)
    {
        GroundEffect& capacitor = state->capacitor;
        if (!capacitor.Active() || now < capacitor.untilMs)
            return;
        capacitor.untilMs = 0;
        if (player->IsAlive())
            player->CastSpell(capacitor.x, capacitor.y, capacitor.z, SPELL_CAPACITOR_STUN, true);
    }

    // Rafale de tempête: Stormstrike again, outside the hit that brought it
    static void UpdateStormflurry(Player* player, ShamanState* state)
    {
        if (state->stormflurryTarget.IsEmpty())
            return;
        Unit* target = ObjectAccessor::GetUnit(*player, state->stormflurryTarget);
        state->stormflurryTarget.Clear();
        if (target && target->IsAlive() && player->IsAlive() && player->IsWithinMeleeRange(target))
            player->CastSpell(target, SPELL_STORMSTRIKE, true);
    }

    // Twice a second: Ancestral Guidance's and Ascension's shares to the injured; Cloudburst's once it is over
    static void UpdatePools(Player* player, ShamanState* state, uint32 now)
    {
        SharePool(player, state->guidancePool, SPELL_ANCESTRAL_GUIDANCE_HEAL, AncestralGuidanceTargets);
        SharePool(player, state->ascendancePool, SPELL_ASCENDANCE_HEAL, AscendanceHealTargets);
        if (state->cloudburstUntilMs && now >= state->cloudburstUntilMs)
        {
            state->cloudburstUntilMs = 0;
            SharePool(player, state->cloudburstPool, SPELL_CLOUDBURST_HEAL, CloudburstTargets);
        }
    }

    // Esprit farouche: a Maelstrom Weapon stack every 3 s while the wolves fight
    static void UpdateWolves(Player* player, ShamanState* state, uint32 now)
    {
        if (!state->wolvesUntilMs)
            return;
        if (now > state->wolvesUntilMs || !player->IsAlive())
        {
            state->wolvesUntilMs = 0;
            return;
        }
        if (now < state->wolvesNextMs)
            return;
        state->wolvesNextMs += FeralSpiritPeriodMs;
        if (player->IsInCombat())
            AddWeaponStacks(player, 1);
    }

    // Maelstrom fades 15 s after combat, and is Elemental's only
    static void UpdateMaelstrom(Player* player, ShamanState* state, uint32 elapsed)
    {
        if (!player->HasAura(SPELL_MAELSTROM))
        {
            state->outOfCombatMs = 0;
            return;
        }
        if (!IsElemental(player))
        {
            player->RemoveAurasDueToSpell(SPELL_MAELSTROM);
            return;
        }
        if (player->IsInCombat())
        {
            state->outOfCombatMs = 0;
            return;
        }
        state->outOfCombatMs += elapsed;
        if (state->outOfCombatMs < MaelstromFadeMs)
            return;
        state->outOfCombatMs = 0;
        player->RemoveAurasDueToSpell(SPELL_MAELSTROM);
    }

    // Dual wielding goes with Enhancement's Dual Wield (the core takes it back on a talent reset only); Maelstrom
    // Weapon with Enhancement
    static void UpdateHolds(Player* player)
    {
        if (player->CanDualWield() && !player->HasSpell(SPELL_DUAL_WIELD))
            player->SetCanDualWield(false);
        if (!IsEnhancement(player) && player->HasAura(SPELL_MAELSTROM_WEAPON))
            player->RemoveAurasDueToSpell(SPELL_MAELSTROM_WEAPON);
    }
};
}

void AddShamanTalentScripts()
{
    new ShamanTalentSpellScript();
    new ShamanTalentUnitScript();
    new ShamanTalentPlayerScript();
}
