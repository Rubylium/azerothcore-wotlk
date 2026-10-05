#include "AllSpellScript.h"
#include "CellImpl.h"
#include "Chat.h"
#include "DynamicObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Item.h"
#include "LiveTuning.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "UnitScript.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <list>
#include <string>

// The Faucheur (Reaper, class 15): what its spell data (localTools/reaper/Spells.ps1) cannot carry. Ascension's Reaper
// remade on our class framework: its soul resource, its abilities taught by level and the talents its trees mark as
// dummies (localTools/reaper/talentTree.json; a talent is its rank spell's aura on the Faucheur, learned by
// mod-custom-classes' TalentTree.cpp).
//
// The resource: Fragments d'âme (an aura of up to 3 stacks) fill Âmes moissonnées (up to 3); at 3 souls the Faucheur
// holds the Infusion d'âme. Reliquaire des perdus, Âmes tourmentées, Fracas d'âmes and Faux spectrale consume every
// soul, scaling with how many, and 20% stronger when they consume all 3 (the Infusion). Every change of the resource is
// sent to the class HUD (clientPatcher/interface/Interface/FrameXML/ClassHud.lua) as the addon message
// "REAPER\t<souls>:<fragments>:<infused>".
//
// Runic power is the spenders' currency (the Death Knight bar); the core drains it out of combat for any class whose
// power it is (Player::Regenerate).
namespace
{
constexpr uint8 CLASS_REAPER = 15;
constexpr uint32 SPELL_FAMILY_REAPER = 21;
constexpr char const* HudPrefix = "REAPER";

enum Spells : uint32
{
    SPELL_REAP = 98000,
    SPELL_MURDER = 98001,
    SPELL_SOULREND = 98002,
    SPELL_SPECTRE_STRIDE = 98003,
    SPELL_UNDERWALK = 98004,
    SPELL_RELIQUARY = 98005,
    SPELL_SOUL_BOLT = 98006,
    SPELL_GHOST_CLAW = 98007,
    SPELL_SOUL_SHOCK = 98009,
    SPELL_DEATHWIND = 98010,
    SPELL_SOULSLAM = 98011,
    SPELL_DEATHSTALKER = 98012,
    SPELL_SOUL_SHEAR = 98013,
    SPELL_SOULSTONE_LURE = 98014,
    SPELL_SOUL_FRAGMENT = 98016,
    SPELL_REAPED_SOUL = 98017,
    SPELL_SOUL_INFUSION = 98018,
    SPELL_SOUL_COLLECTOR = 98019,
    SPELL_SOULREND_DEBUFF = 98021,
    SPELL_DEATHSTALKER_SPEED = 98022,
    SPELL_ANXIETY = 98023,
    SPELL_SPECTRE_STRIDE_HIT = 98024,
    SPELL_REAP_OFFHAND = 98025,
    SPELL_SINISTER_LITANY = 98030,
    SPELL_LIMBO = 98031,
    SPELL_VEILWALK = 98033,
    SPELL_GHASTLY_SCREECH = 98034,
    SPELL_GHASTLY_SCREECH_BURST = 98035,
    SPELL_JAILERS_BARGAIN = 98036,
    SPELL_SCYTHE_RUSH = 98037,
    SPELL_WRAITHBLADE = 98040,
    SPELL_TORMENTED_SOULS = 98041,
    SPELL_TORMENTED_SOULS_AURA = 98042,
    SPELL_MASOCHISTIC_RAGE = 98043,
    SPELL_MASOCHISTIC_RAGE_BUFF = 98044,
    SPELL_SOUL_HARVEST = 98046,
    SPELL_GHASTLY_FORM_SHIELD = 98047,
    SPELL_GHOST = 98048,
    SPELL_FROM_THE_SHADOWS = 98049,
    SPELL_DOMINION = 98050,
    SPELL_DAMNED = 98051,
    SPELL_SOUL_HARVESTER = 98052,
    SPELL_TORMENTED_SOUL_HEAL = 98053,
    SPELL_ESSENCE_INVIGORATION = 98054,
    SPELL_REAPER_PASSIVE = 98055,
    SPELL_SOUL_HARVEST_HEAL = 98056,
    // Moisson
    SPELL_SLAUGHTER = 98100,
    SPELL_CROWS_HARVEST = 98101,
    SPELL_DOOMREND = 98102,
    SPELL_DOOMREND_SHIELD = 98103,
    SPELL_HARVEST_TIME = 98104,
    SPELL_HARVESTING_GROUNDS = 98106,
    SPELL_SHUDDER_SCYTHE = 98108,
    SPELL_SHUDDER_SCYTHE_HIT = 98109,
    SPELL_THRESH = 98110,
    SPELL_THRESH_HIT = 98111,
    SPELL_BLOOD_FRENZY = 98112,
    SPELL_BLOOD_FRENZY_BURST = 98113,
    SPELL_DARKREND = 98114,
    SPELL_EXTINCTION = 98115,
    SPELL_CRIMSON_THIRST = 98116,
    SPELL_RUIN = 98117,
    SPELL_RUINOUS_FRENZY = 98118,
    SPELL_SOULS_FOR_THE_SLAUGHTER = 98119,
    SPELL_SOULS_FOR_THE_SLAUGHTER_HIT = 98120,
    SPELL_HARVESTER_HEAL = 98121,
    SPELL_BLOOD_HARVESTER_AURA = 98123,
    SPELL_DEATHS_PRESENCE_AURA = 98125,
    SPELL_ETHEREAL_GUARD_AURA = 98126,
    SPELL_CRIMSON_THIRST_HEAL = 98127,
    // Âme
    SPELL_DIRGE = 98200,
    SPELL_DIRGE_OFFHAND = 98201,
    SPELL_DEATHCHASER = 98202,
    SPELL_ENDBRINGER = 98203,
    SPELL_GHOSTLY_WEAPON = 98204,
    SPELL_GHOSTLY_WEAPON_HIT = 98205,
    SPELL_SHADE = 98206,
    SPELL_SEPULCHRAL_RENEWAL = 98207,
    SPELL_GRAVESITE = 98208,
    SPELL_APPARITION = 98209,
    SPELL_SOULROT = 98210,
    SPELL_SOULROT_BURST = 98211,
    SPELL_ANIMA_AMBUSH = 98212,
    SPELL_THE_END_IS_NEAR = 98213,
    SPELL_PURGATORY = 98214,
    SPELL_THE_END_IS_NEAR_HIT = 98215,
    SPELL_BEYOND_THE_VEIL = 98216,
    SPELL_SPIRITUAL_REFLEXES = 98217,
    SPELL_JAILERS_CALL = 98218,
    SPELL_GHASTLY_HILT = 98219,
    SPELL_LAMENTING_HEAL = 98220,
    SPELL_ENDBRINGER_HEAL = 98221,
    SPELL_WEAKENED_SOUL = 98223,
    SPELL_SOULFORGED_WEAPONRY = 98224,
    // Domination
    SPELL_SOUL_STRIKE = 98300,
    SPELL_SPECTRAL_SCYTHE = 98301,
    SPELL_SPECTRAL_SCYTHE_HIT = 98302,
    SPELL_SPECTRAL_WARDEN = 98303,
    SPELL_SPECTRAL_WARDEN_SHIELD = 98304,
    SPELL_DREADWAKE = 98305,
    SPELL_REQUIEM = 98306,
    SPELL_BOLSTERED_FORM = 98308,
    SPELL_DECIMATE = 98309,
    SPELL_DECIMATION = 98310,
    SPELL_PAINMAIL = 98311,
    SPELL_INTIMIDATED = 98312,
    SPELL_SOUL_KNIGHT = 98313,
    SPELL_FATESEALER = 98314,
    SPELL_SOUL_SPLINTERS = 98315,
    SPELL_EATER_OF_SOULS = 98316,
    SPELL_ESSENCE_BINDER = 98317,
    SPELL_SIPHON_ANIMA = 98319,
    SPELL_SPIRIT_REALM = 98320,
    SPELL_HARD_BARGAIN = 98321,
    SPELL_DECIMATE_READY = 98322,
    SPELL_LIFE_TAP_HEAL = 98323,
    SPELL_SPECTRAL_WARDEN_HIT = 98324,
    SPELL_SOUL_SPLINTERS_HIT = 98325,
    SPELL_SIPHON_ANIMA_HEAL = 98326,
    SPELL_SOUL_STRIKE_HEAL = 98328,
    // The specializations
    SPELL_SPEC_REAPING = 98900,
    SPELL_SPEC_SOUL = 98901,
    SPELL_SPEC_DOMINATION = 98902,
};

// Talent ranks the C++ reads (localTools/reaper/talentTree.json, through .agents/plans/reaper/talentIds.txt)
enum Talents : uint32
{
    TALENT_BACKSWING = 98403,
    TALENT_BETWEEN_SHADOWS = 98407,
    TALENT_GHOST = 98410,
    TALENT_GHASTLY_FORM = 98411,
    TALENT_SOUL_HARVEST = 98412,
    TALENT_SOULFUSED_CONSTITUTION = 98413,
    TALENT_SOUL_FURNACE = 98414,
    TALENT_PAINBRINGER = 98417,
    TALENT_HARVESTERS_SCYTHE = 98418,
    TALENT_DARK_PASSAGE = 98419,
    TALENT_ESSENCE_INVIGORATION = 98421,
    TALENT_DOMINION = 98422,
    TALENT_DAMNED = 98423,
    TALENT_SOUL_HARVESTER = 98424,
    TALENT_FROM_THE_SHADOWS = 98425,
    // Moisson
    TALENT_RED_WAKE = 98427,
    TALENT_HEMORRHAGE = 98433,
    TALENT_HUNGERING_SCYTHE = 98434,
    TALENT_BLOOD_KNIGHT = 98437,
    TALENT_MY_DOMAIN = 98439,
    TALENT_CRIMSON_THIRST = 98440,
    TALENT_EXTINCTION = 98441,
    TALENT_SOULS_FOR_THE_SLAUGHTER = 98442,
    TALENT_DARKREND_SCYTHE = 98443,
    TALENT_RUIN = 98444,
    TALENT_REDSHADE = 98446,
    TALENT_CRIMSON_DEATH = 98447,
    TALENT_BLOOD_HARVESTER = 98451,
    TALENT_BLOOD_FRENZY = 98452,
    TALENT_BLOOD_FUELED = 98453,
    TALENT_THE_TIME_HAS_COME = 98454,
    TALENT_MORTALS_END = 98457,
    TALENT_UNBOUND = 98458,
    // Âme
    TALENT_SOULFORGED_WEAPONRY_1 = 98464,
    TALENT_SOULFORGED_WEAPONRY_2 = 98465,
    TALENT_SHATTERED_SOULS = 98470,
    TALENT_SOULROT = 98472,
    TALENT_LAMENTING = 98474,
    TALENT_SOUL_GENERATOR = 98475,
    TALENT_PURGATORY = 98476,
    TALENT_SPECTRE = 98477,
    TALENT_BEYOND_THE_VEIL = 98478,
    TALENT_SPIRITUAL_REFLEXES = 98480,
    TALENT_SOUL_PIERCER = 98482,
    TALENT_ANIMA_AMBUSHER = 98484,
    TALENT_JAILERS_CALL = 98486,
    TALENT_LORD_OF_DEATH = 98487,
    TALENT_GHASTLY_HILT = 98488,
    TALENT_DEATHS_PRESENCE = 98489,
    TALENT_GRAVESITE = 98490,
    TALENT_THE_END_IS_NEAR = 98491,
    TALENT_BEYOND_DEATH = 98492,
    TALENT_CHASING_DEATH = 98495,
    // Domination
    TALENT_SOUL_SLIP = 98501,
    TALENT_EMPYREAN_FORTITUDE = 98502,
    TALENT_LIFESTEALER = 98504,
    TALENT_JAILERS_WILL = 98505,
    TALENT_WELL_OF_SOULS = 98507,
    TALENT_SIPHON_ANIMA = 98509,
    TALENT_PAINMAIL = 98510,
    TALENT_ESSENCE_BINDER = 98511,
    TALENT_HARNESSED_LIFE = 98512,
    TALENT_FINAL_REQUIEM_1 = 98513,
    TALENT_FINAL_REQUIEM_2 = 98514,
    TALENT_ETHEREAL_GUARD = 98515,
    TALENT_SOULTAKER = 98516,
    TALENT_HARD_BARGAIN = 98517,
    TALENT_DECIMATION = 98518,
    TALENT_SOULSTORM = 98519,
    TALENT_SOUL_SPLINTERS = 98520,
    TALENT_EATER_OF_SOULS = 98521,
    TALENT_SOUL_KNIGHT = 98522,
    TALENT_LIFE_TAP = 98523,
    TALENT_SPIRIT_ARMY = 98524,
    TALENT_WARDEN_OF_THE_LOST = 98525,
    TALENT_FATESEALER = 98526,
    TALENT_SPIRIT_REALM = 98527,
    TALENT_INTIMIDATING_PRESENCE = 98528,
    TALENT_SPIRIT_CULLING = 98531,
    TALENT_SOULSIGHT = 98532,
};

// The abilities every Faucheur holds, by level (the others come from its talent trees and specialization)
struct AbilityUnlock
{
    uint32 spellId;
    uint8 level;
};

constexpr std::array<AbilityUnlock, 15> AbilityUnlocks = { {
    { SPELL_REAP, 1 }, { SPELL_SOUL_COLLECTOR, 1 }, { SPELL_REAPER_PASSIVE, 1 }, { SPELL_MURDER, 3 },
    { SPELL_SOULREND, 5 }, { SPELL_SPECTRE_STRIDE, 8 }, { SPELL_UNDERWALK, 10 }, { SPELL_RELIQUARY, 12 },
    { SPELL_GHOST_CLAW, 14 }, { SPELL_SOUL_SHOCK, 16 }, { SPELL_DEATHWIND, 18 }, { SPELL_SOULSLAM, 20 },
    { SPELL_DEATHSTALKER, 24 }, { SPELL_SOUL_SHEAR, 28 }, { SPELL_SOULSTONE_LURE, 30 }
} };

// The proficiencies (PlayerStart.CustomSpells is off, so playercreateinfo_spell_custom teaches none of them): cloth,
// leather, mail, plate, the axes, maces, polearms, swords, staves, daggers, dual wield and parry
constexpr std::array<uint32, 16> ProficiencySpells = { 9078, 9077, 8737, 750, 196, 197, 198, 199, 200, 201, 202, 227,
    1180, 674, 3127, 204 };
constexpr std::array<uint32, 10> WeaponSkills = { SKILL_AXES, SKILL_2H_AXES, SKILL_MACES, SKILL_2H_MACES,
    SKILL_POLEARMS, SKILL_SWORDS, SKILL_2H_SWORDS, SKILL_STAVES, SKILL_DAGGERS, SKILL_UNARMED };
constexpr std::array<uint32, 2> ArmorSkills = { SKILL_MAIL, SKILL_PLATE_MAIL };

// Balance, live with .tune (LiveTuning.h)
LiveTuning::Knob const HarvesterPct("reaper.harvester_pct", 10.0f);            // Moisson: healed from damage dealt
LiveTuning::Knob const SoulStrikeHealPct("reaper.soul_strike_heal_pct", 40.0f); // of its damage
LiveTuning::Knob const SoulStrikeMissingPct("reaper.soul_strike_missing_pct", 10.0f);
LiveTuning::Knob const DeathwindHealPct("reaper.deathwind_heal_pct", 25.0f);
LiveTuning::Knob const GhostlyWeaponPct("reaper.ghostly_weapon_pct", 25.0f);
LiveTuning::Knob const SoulslamPerSoul("reaper.soulslam_per_soul", 0.3f);
LiveTuning::Knob const InfusedFactor("reaper.infused_factor", 1.2f);           // consuming all 3 souls
LiveTuning::Knob const TormentedHealAp("reaper.tormented_heal_ap", 0.058f);
LiveTuning::Knob const TormentedHealSta("reaper.tormented_heal_sta", 0.24f);
LiveTuning::Knob const DarkrendTickAp("reaper.darkrend_tick_ap", 0.085f);
LiveTuning::Knob const SoulrotPerStackAp("reaper.soulrot_per_stack_ap", 0.06f);
LiveTuning::Knob const MasochisticRageAp("reaper.masochistic_rage_ap", 0.36f);
LiveTuning::Knob const DoomrendAbsorbStr("reaper.doomrend_absorb_str", 1.0f);
LiveTuning::Knob const WardenShieldSta("reaper.warden_shield_sta", 1.5f);
LiveTuning::Knob const SoulSplinterAp("reaper.soul_splinter_ap", 0.05f);
LiveTuning::Knob const SoulSplinterSta("reaper.soul_splinter_sta", 0.035f);
LiveTuning::KnobUInt const SoulBoltIntervalMs("reaper.soul_bolt_interval_ms", 1000);
LiveTuning::KnobUInt const ShudderIntervalMs("reaper.shudder_interval_ms", 300);

constexpr char const* StateKey = "ReaperState";

struct ReaperState : public DataMap::Base
{
    // What the HUD last heard (souls, fragments, infused), so only changes are sent
    int16 sentSouls = -1;
    int16 sentFragments = -1;
    int16 sentInfused = -1;
    // Heals worked out from damage dealt, applied once a second (one combat log line rather than one per hit)
    float pendingHarvester = 0.0f;
    float pendingSiphon = 0.0f;
    uint32 healMs = 0;
    uint32 tickMs = 0;
    // The consumers' strength while their output lands: the souls each consumed and whether it was all 3
    float soulslamFactor = 1.0f;
    uint8 soulslamRefund = 0;           // Perce-âme: the souls to give back once Fracas d'âmes hits
    bool boltsInfused = false;
    bool scythesInfused = false;
    uint8 crowsFragments = 0;           // Moisson des corbeaux: fragments its current swing gave (3 at most)
    bool dreadwakeSoul = false;         // Sillage d'effroi's soul, once per cast
    bool requiemSoul = false;           // Tempête d'âmes, once per cast
    bool soulrendSoul = false;
    bool soultakerDone = false;
    bool crimsonDeathUsed = false;      // Mort cramoisie: its second blow never procs again
    uint32 soulSlipCooldownUntil = 0;   // Glissement d'âme: once every 3 s
    uint32 redshadeCooldownUntil = 0;   // Écarlatine: once every 20 s
    uint32 chasingDeathUntil = 0;       // Mort poursuivante: its 30 s window
    ObjectGuid chasingDeathTarget;
};

Player* Reaper(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_REAPER ? player : nullptr;
}

Player const* Reaper(Unit const* unit)
{
    Player const* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_REAPER ? player : nullptr;
}

ReaperState* GetState(Player* player)
{
    return player->CustomData.GetDefault<ReaperState>(StateKey);
}

uint32 Now()
{
    return GameTime::GetGameTimeMS().count();
}

uint8 Rank(Unit const* unit, uint32 first, uint32 second)
{
    return unit->HasAura(second) ? 2 : unit->HasAura(first) ? 1 : 0;
}

bool IsReaperSpell(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELL_FAMILY_REAPER;
}

// The Faucheur's current enemy: its target if it may attack it, else what it fights
Unit* CurrentEnemy(Player* player, float range)
{
    Unit* target = player->GetSelectedUnit();
    if (!target || !player->IsValidAttackTarget(target))
        target = player->GetVictim();
    if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target) ||
        !player->IsWithinDistInMap(target, range))
        return nullptr;
    return target;
}

// The enemies within `radius` of `center` the Faucheur may strike, nearest first, `center` itself left out
std::list<Unit*> NearbyEnemies(Player* player, WorldObject* center, float radius, std::size_t limit)
{
    std::list<Unit*> enemies;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck check(center, player, radius);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center, enemies, check);
    Cell::VisitObjects(center, searcher, radius);
    enemies.remove_if([player, center](Unit* unit)
        { return unit == center || !unit->IsAlive() || !player->IsValidAttackTarget(unit); });
    enemies.sort([center](Unit const* left, Unit const* right)
        { return center->GetDistance(left) < center->GetDistance(right); });
    while (enemies.size() > limit)
        enemies.pop_back();
    return enemies;
}

void ReduceCooldown(Player* player, uint32 spellId, uint32 ms)
{
    if (!player->HasSpellCooldown(spellId))
        return;
    if (player->GetSpellCooldownDelay(spellId) <= ms)
        player->RemoveSpellCooldown(spellId, true);
    else
        player->ModifySpellCooldown(spellId, -int32(ms));
}

void Heal(Player* player, uint32 spellId, float amount)
{
    if (amount >= 1.0f && player->IsAlive())
        player->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, int32(amount), player, true);
}

// An aura of the Faucheur's on a unit, one more stack up to the spell's own maximum, its duration refreshed or not
Aura* AddStack(Player* player, Unit* target, uint32 spellId, bool refresh = true)
{
    if (Aura* aura = target->GetAura(spellId, player->GetGUID()))
    {
        int32 const duration = aura->GetDuration();
        if (aura->GetStackAmount() < aura->GetSpellInfo()->StackAmount)
            aura->ModStackAmount(1);
        if (refresh)
            aura->RefreshDuration();
        else
            aura->SetDuration(duration);
        return aura;
    }
    return player->AddAura(spellId, target);
}

// --- The resource ---------------------------------------------------------------------------------------------------
uint8 Stacks(Player const* player, uint32 spellId)
{
    Aura const* aura = player->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
}

void SetStacks(Player* player, uint32 spellId, uint8 stacks)
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
}

uint8 Souls(Player const* player)
{
    return Stacks(player, SPELL_REAPED_SOUL);
}

// The class HUD's state: souls, fragments, infused
void SyncHud(Player* player, bool force = false)
{
    if (!player->GetSession())
        return;
    ReaperState* state = GetState(player);
    int16 const souls = Souls(player);
    int16 const fragments = Stacks(player, SPELL_SOUL_FRAGMENT);
    int16 const infused = player->HasAura(SPELL_SOUL_INFUSION) ? 1 : 0;
    if (!force && souls == state->sentSouls && fragments == state->sentFragments && infused == state->sentInfused)
        return;
    state->sentSouls = souls;
    state->sentFragments = fragments;
    state->sentInfused = infused;
    std::string const payload = std::string(HudPrefix) + "\t" + std::to_string(souls) + ":" +
        std::to_string(fragments) + ":" + std::to_string(infused);
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, payload);
    player->GetSession()->SendPacket(&packet);
}

void MasochisticRageProc(Player* player)
{
    // Porte-douleur: the rage without its price, or 3 s more of it
    if (Aura* aura = player->GetAura(SPELL_MASOCHISTIC_RAGE_BUFF))
    {
        int32 const duration = aura->GetDuration() + 3000;
        if (duration > aura->GetMaxDuration())
            aura->SetMaxDuration(duration);
        aura->SetDuration(duration);
        return;
    }
    if (Aura* aura = player->AddAura(SPELL_MASOCHISTIC_RAGE_BUFF, player))
    {
        aura->SetMaxDuration(5000);
        aura->SetDuration(5000);
    }
}

void SummonSpectralScythes(Player* player, uint8 count, int32 durationMs, bool infused);

void OnInfusionGained(Player* player)
{
    if (player->HasAura(TALENT_DOMINION))
        player->CastSpell(player, SPELL_DOMINION, true);
    if (player->HasAura(TALENT_DAMNED))
        player->CastSpell(player, SPELL_DAMNED, true);
    if (player->HasAura(TALENT_PURGATORY))
        AddStack(player, player, SPELL_PURGATORY);
    // Dévoreur d'âmes: reaching 3 souls, unless it is already running
    if (player->HasAura(TALENT_EATER_OF_SOULS) && !player->HasAura(SPELL_EATER_OF_SOULS))
        player->CastSpell(player, SPELL_EATER_OF_SOULS, true);
}

void UpdateInfusion(Player* player)
{
    if (Souls(player) >= 3)
    {
        if (!player->HasAura(SPELL_SOUL_INFUSION))
        {
            player->CastSpell(player, SPELL_SOUL_INFUSION, true);
            OnInfusionGained(player);
        }
        else if (Aura* aura = player->GetAura(SPELL_SOUL_INFUSION))
            aura->RefreshDuration();
    }
    else
        player->RemoveAurasDueToSpell(SPELL_SOUL_INFUSION);
}

// A soul harvested: the talents that ride on it
void OnSoulGained(Player* player, Unit* target)
{
    if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        target = CurrentEnemy(player, 40.0f);
    if (player->HasAura(TALENT_SOUL_HARVEST) && target)
        player->CastSpell(target, SPELL_SOUL_HARVEST, true);
    if (player->HasAura(TALENT_PAINBRINGER) && roll_chance_i(15))
        MasochisticRageProc(player);
    if (player->HasAura(TALENT_FATESEALER))
        AddStack(player, player, SPELL_FATESEALER);
    if (player->HasAura(TALENT_SOUL_SPLINTERS))
        player->CastSpell(player, SPELL_SOUL_SPLINTERS, true);
    if (player->HasAura(TALENT_SPIRIT_CULLING) && roll_chance_i(10))
        SummonSpectralScythes(player, 1, 10000, false);
}

void AddSouls(Player* player, uint8 count, Unit* target)
{
    uint8 souls = Souls(player);
    for (uint8 index = 0; index < count && souls < 3; ++index)
    {
        ++souls;
        SetStacks(player, SPELL_REAPED_SOUL, souls);
        OnSoulGained(player, target);
    }
    if (Aura* aura = player->GetAura(SPELL_REAPED_SOUL))
        aura->RefreshDuration();
    UpdateInfusion(player);
    SyncHud(player);
}

void AddFragments(Player* player, uint8 count, Unit* target)
{
    uint8 fragments = Stacks(player, SPELL_SOUL_FRAGMENT) + count;
    uint8 souls = 0;
    while (fragments >= 3)
    {
        fragments -= 3;
        ++souls;
    }
    // At 3 souls the fragments keep filling (and wait), so a consumer is followed by a soul at once
    if (Souls(player) >= 3 && souls)
    {
        fragments = 2;
        souls = 0;
    }
    SetStacks(player, SPELL_SOUL_FRAGMENT, fragments);
    if (souls)
        AddSouls(player, souls, target);
    else
        SyncHud(player);
}

// Every soul held, consumed (L'heure de la moisson may keep them): how many, and whether it was all 3
uint8 ConsumeSouls(Player* player, bool& infused)
{
    uint8 const souls = Souls(player);
    infused = souls >= 3;
    if (!souls)
        return 0;
    bool const kept = player->HasAura(SPELL_HARVEST_TIME) && roll_chance_i(50);
    if (!kept)
    {
        SetStacks(player, SPELL_REAPED_SOUL, 0);
        UpdateInfusion(player);
    }
    // Tourmenteur (Domination): Âmes tourmentées comes back sooner
    if (player->HasAura(SPELL_SPEC_DOMINATION))
        ReduceCooldown(player, SPELL_TORMENTED_SOULS, 3000);
    // Revigoration d'essence: 10% of missing health
    if (player->HasAura(TALENT_ESSENCE_INVIGORATION) && roll_chance_i(25))
        Heal(player, SPELL_ESSENCE_INVIGORATION, float(player->GetMaxHealth() - player->GetHealth()) * 0.1f);
    SyncHud(player);
    return souls;
}

// --- Spectral Scythes and Wardens ----------------------------------------------------------------------------------
void SummonSpectralScythes(Player* player, uint8 count, int32 durationMs, bool infused)
{
    if (!count)
        return;
    Aura* aura = player->GetAura(SPELL_SPECTRAL_SCYTHE);
    if (aura)
        aura->SetStackAmount(std::min<uint8>(3, aura->GetStackAmount() + count));
    else
    {
        aura = player->AddAura(SPELL_SPECTRAL_SCYTHE, player);
        if (!aura)
            return;
        aura->SetStackAmount(count);
        if (durationMs > 0)
        {
            aura->SetMaxDuration(durationMs);
            aura->SetDuration(durationMs);
        }
    }
    GetState(player)->scythesInfused = infused;
    // Siphon d'anima
    if (player->HasAura(TALENT_SIPHON_ANIMA))
        player->CastSpell(player, SPELL_SIPHON_ANIMA, true);
}

// Frappe d'âme makes every scythe strike its target and five enemies near it
void ScythesFollowSoulStrike(Player* player, Unit* target)
{
    Aura* aura = player->GetAura(SPELL_SPECTRAL_SCYTHE);
    if (!aura || !target)
        return;
    uint8 const scythes = aura->GetStackAmount();
    std::list<Unit*> enemies = NearbyEnemies(player, target, 8.0f, 5);
    enemies.push_front(target);
    for (Unit* enemy : enemies)
        for (uint8 index = 0; index < scythes; ++index)
            player->CastSpell(enemy, SPELL_SPECTRAL_SCYTHE_HIT, true);
}

void SpectralWarden(Player* player)
{
    bool const army = player->HasAura(TALENT_SPIRIT_ARMY);
    if (Aura* aura = player->GetAura(SPELL_SPECTRAL_WARDEN))
    {
        aura->SetStackAmount(army ? 3 : 1);
        if (army)
        {
            int32 const duration = std::max(1000, aura->GetMaxDuration() - 8000);
            aura->SetMaxDuration(duration);
            aura->SetDuration(duration);
        }
    }
    // The shields: the Faucheur and up to 5 group members within 15 yd
    int32 const shield = int32(float(player->GetStat(STAT_STAMINA)) * WardenShieldSta * (army ? 2.0f : 1.0f));
    std::vector<Unit*> allies { player };
    if (Group* group = player->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            if (Player* member = itr->GetSource())
                if (member != player && member->IsAlive() && member->IsInMap(player) &&
                    player->IsWithinDist(member, 15.0f))
                    allies.push_back(member);
    }
    std::sort(allies.begin() + 1, allies.end(), [player](Unit const* left, Unit const* right)
        { return player->GetDistance(left) < player->GetDistance(right); });
    if (allies.size() > 6)
        allies.resize(6);
    for (Unit* ally : allies)
        player->CastCustomSpell(SPELL_SPECTRAL_WARDEN_SHIELD, SPELLVALUE_BASE_POINT0, shield, ally, true);
}

// --- Training -------------------------------------------------------------------------------------------------------
void RestoreTraining(Player* player)
{
    for (uint32 spellId : ProficiencySpells)
        if (!player->HasSpell(spellId))
            player->learnSpell(spellId, false);
    // The weapon skills at the level's maximum: a skill granted at 1 makes a boosted or bench Faucheur miss nearly
    // every swing
    uint16 const maxSkill = player->GetMaxSkillValueForLevel();
    for (uint32 skill : WeaponSkills)
        if (player->GetSkillValue(skill) < maxSkill)
            player->SetSkill(skill, player->GetSkillStep(skill), maxSkill, maxSkill);
    for (uint32 skill : ArmorSkills)
        if (!player->GetSkillValue(skill))
            player->SetSkill(skill, 0, 1, 1);
}

void LearnUnlockedAbilities(Player* player)
{
    for (AbilityUnlock const& unlock : AbilityUnlocks)
    {
        if (player->GetLevel() >= unlock.level)
        {
            if (!player->HasSpell(unlock.spellId))
                player->learnSpell(unlock.spellId, false);
        }
        else if (player->HasSpell(unlock.spellId))
            player->removeSpell(unlock.spellId, SPEC_MASK_ALL, false);
    }
}

// --- The group auras three talents keep up ------------------------------------------------------------------------
void KeepAura(Player* player, uint32 spellId, bool wanted)
{
    bool const has = player->HasAura(spellId);
    if (wanted && !has)
        player->AddAura(spellId, player);
    else if (!wanted && has)
        player->RemoveAurasDueToSpell(spellId);
}

bool InsideOwnDynObject(Player* player, uint32 spellId, Unit const* unit)
{
    DynamicObject* dynObject = player->GetDynObject(spellId);
    return dynObject && unit->IsInMap(dynObject) && unit->GetExactDist2d(dynObject) <= dynObject->GetRadius();
}

// Champ de moisson: an enemy leaving the field is pulled back to its centre
void HoldHarvestingGround(Player* player)
{
    DynamicObject* field = player->GetDynObject(SPELL_HARVESTING_GROUNDS);
    if (!field)
        return;
    float const radius = field->GetRadius();
    for (Unit* enemy : NearbyEnemies(player, field, radius + 4.0f, 20))
    {
        float const distance = enemy->GetExactDist2d(field);
        if (distance <= radius || distance > radius + 4.0f || (!enemy->IsPlayer() && !enemy->IsInCombatWith(player)))
            continue;
        enemy->GetMotionMaster()->MoveJump(field->GetPositionX(), field->GetPositionY(), field->GetPositionZ(), 18.0f,
            6.0f);
    }
}

// --- What a strike does after it lands -------------------------------------------------------------------------------
void GhostlyWeaponEcho(Player* player, Unit* victim, uint32 damage)
{
    if (!damage || !victim->IsAlive() || !player->HasAura(SPELL_GHOSTLY_WEAPON))
        return;
    int32 const frost = int32(float(damage) * GhostlyWeaponPct / 100.0f);
    if (frost > 0)
        player->CastCustomSpell(SPELL_GHOSTLY_WEAPON_HIT, SPELLVALUE_BASE_POINT0, frost, victim, true);
}

// Garde fantomatique: under Arme fantomatique, a melee ability's damage on up to two enemies beside its target
void GhastlyHiltCleave(Player* player, Unit* victim, uint32 damage)
{
    if (!damage || !player->HasAura(TALENT_GHASTLY_HILT) || !player->HasAura(SPELL_GHOSTLY_WEAPON))
        return;
    for (Unit* enemy : NearbyEnemies(player, victim, 8.0f, 2))
        player->CastCustomSpell(SPELL_GHASTLY_HILT, SPELLVALUE_BASE_POINT0, int32(damage), enemy, true);
}

void GravesiteApparition(Player* player, Unit* victim)
{
    if (!player->HasAura(TALENT_GRAVESITE) || !victim->IsAlive())
        return;
    if (InsideOwnDynObject(player, SPELL_GRAVESITE, victim))
        player->CastSpell(victim, SPELL_APPARITION, true);
}

void PainmailStack(Player* player)
{
    if (player->HasAura(TALENT_PAINMAIL))
        AddStack(player, player, SPELL_PAINMAIL, false);
}

void DarkrendBleed(Player* player, Unit* victim)
{
    if (player->HasAura(TALENT_DARKREND_SCYTHE) && victim->IsAlive())
        AddStack(player, victim, SPELL_DARKREND, false);
}

// A direct critical strike: Sillage rouge, Passage obscur, Lieu de sépulture
void OnDirectCritical(Player* player, Unit* victim, SpellInfo const* spellInfo)
{
    if (player->HasAura(TALENT_RED_WAKE))
        AddFragments(player, 1, victim);
    if (spellInfo && player->HasAura(TALENT_DARK_PASSAGE) && (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW))
        ReduceCooldown(player, SPELL_LIMBO, 2000);
    GravesiteApparition(player, victim);
}

// Physical damage dealt: Soif cramoisie, Extinction
void OnPhysicalDamage(Player* player)
{
    if (player->HasAura(TALENT_CRIMSON_THIRST))
        AddStack(player, player, SPELL_CRIMSON_THIRST);
    if (player->HasAura(TALENT_EXTINCTION) && roll_chance_i(5 + 5 * Souls(player)))
        if (Aura* aura = player->AddAura(SPELL_EXTINCTION, player))
            aura->SetCharges(player->HasAura(TALENT_MORTALS_END) ? 2 : 1);
}

// An auto attack landed (the hidden passive's proc): what rides on it
void OnAutoAttackHit(Player* player, Unit* victim, uint32 damage, bool critical, WeaponAttackType attackType)
{
    if (!victim || !victim->IsAlive())
        return;
    GhostlyWeaponEcho(player, victim, damage);
    if (damage)
        OnPhysicalDamage(player);
    // Armes forgées d'âme: two weapons, a free Meurtre now and then
    if (player->haveOffhandWeapon() && roll_chance_i(10) &&
        Rank(player, TALENT_SOULFORGED_WEAPONRY_1, TALENT_SOULFORGED_WEAPONRY_2))
        player->CastSpell(player, SPELL_SOULFORGED_WEAPONRY, true);
    // Âmes pour le massacre: 85% of the blow again, as shadow
    if (damage && player->HasAura(SPELL_SOULS_FOR_THE_SLAUGHTER))
        player->CastCustomSpell(SPELL_SOULS_FOR_THE_SLAUGHTER_HIT, SPELLVALUE_BASE_POINT0, int32(damage * 0.85f),
            victim, true);
    // Vue de l'âme: direct damage now and then harvests a fragment
    if (player->HasAura(TALENT_SOULSIGHT) && roll_chance_i(15))
        AddFragments(player, 1, victim);
    // Appel du Geôlier: enemies near death
    if (player->HasAura(TALENT_JAILERS_CALL) && victim->HealthBelowPct(20))
        player->CastSpell(victim, SPELL_JAILERS_CALL, true);
    if (critical)
        OnDirectCritical(player, victim, nullptr);
    (void)attackType;
}

// A melee or dodge avoided (the hidden passive's proc): Glissement d'âme, Chevalier d'âme, Robustesse empyréenne,
// Réflexes spirituels
void OnAvoided(Player* player, Unit* attacker, bool parry)
{
    ReaperState* state = GetState(player);
    if (player->HasAura(TALENT_SOUL_SLIP) && Now() >= state->soulSlipCooldownUntil && roll_chance_i(50))
    {
        state->soulSlipCooldownUntil = Now() + 3000;
        AddFragments(player, 1, attacker);
    }
    if (player->HasAura(TALENT_SOUL_KNIGHT))
        AddStack(player, player, SPELL_SOUL_KNIGHT);
    if (parry && player->HasAura(TALENT_EMPYREAN_FORTITUDE))
        player->ModifyPower(POWER_RUNIC_POWER, 20);
    if (!parry && attacker && player->HasAura(SPELL_SPIRITUAL_REFLEXES) && attacker->IsAlive() &&
        player->IsValidAttackTarget(attacker))
        player->CastSpell(attacker, SPELL_SOUL_HARVEST, true);
}

void GhastlyFormShield(Player* player)
{
    if (!player->HasAura(TALENT_GHASTLY_FORM))
        return;
    int32 const amount = int32(player->GetTotalAttackPowerValue(BASE_ATTACK) * 0.33f);
    player->CastCustomSpell(SPELL_GHASTLY_FORM_SHIELD, SPELLVALUE_BASE_POINT0, std::max(1, amount), player, true);
}

// Schedules `count` more blows of `spellId` at `target`, `intervalMs` apart
void ScheduleBlows(Player* player, Unit* target, uint32 spellId, uint8 count, uint32 intervalMs)
{
    if (!target)
        return;
    ObjectGuid const guid = player->GetGUID();
    ObjectGuid const targetGuid = target->GetGUID();
    for (uint8 index = 1; index <= count; ++index)
        player->m_Events.AddEventAtOffset([guid, targetGuid, spellId]()
        {
            Player* reaper = ObjectAccessor::FindPlayer(guid);
            if (!reaper || !reaper->IsAlive())
                return;
            Unit* enemy = ObjectAccessor::GetUnit(*reaper, targetGuid);
            if (enemy && enemy->IsAlive() && reaper->IsValidAttackTarget(enemy) && reaper->IsWithinMeleeRange(enemy))
                reaper->CastSpell(enemy, spellId, true);
        }, Milliseconds(intervalMs * index));
}

// Reliquaire des perdus: one Trait d'âme a second, one per soul (Au-delà de la mort: sometimes one more)
void ScheduleSoulBolts(Player* player, uint8 souls)
{
    ObjectGuid const guid = player->GetGUID();
    for (uint8 index = 0; index < souls; ++index)
        player->m_Events.AddEventAtOffset([guid]()
        {
            Player* reaper = ObjectAccessor::FindPlayer(guid);
            if (!reaper || !reaper->IsAlive())
                return;
            reaper->CastSpell(reaper, SPELL_SOUL_BOLT, true);
            if (reaper->HasAura(TALENT_BEYOND_DEATH) && roll_chance_i(8))
                reaper->CastSpell(reaper, SPELL_SOUL_BOLT, true);
        }, Milliseconds(uint32(SoulBoltIntervalMs) * (index + 1)));
    // The bolts keep the Infusion's strength until the last one has flown
    player->m_Events.AddEventAtOffset([guid]()
    {
        if (Player* reaper = ObjectAccessor::FindPlayer(guid))
            GetState(reaper)->boltsInfused = false;
    }, Milliseconds(uint32(SoulBoltIntervalMs) * (souls + 1)));
}

class ReaperSpellScript : public AllSpellScript
{
public:
    ReaperSpellScript() : AllSpellScript("ReaperSpellScript", { ALLSPELLHOOK_ON_CAST }) { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = Reaper(caster);
        if (!player || !spellInfo)
            return;
        ReaperState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();
        if (spell->IsTriggered())
            return;
        switch (spellInfo->Id)
        {
            case SPELL_REAP:
                // Two weapons: the off hand strikes too
                if (target && player->haveOffhandWeapon())
                    player->CastSpell(target, SPELL_REAP_OFFHAND, true);
                if (player->HasAura(TALENT_BACKSWING))
                    player->ModifyPower(POWER_RUNIC_POWER, 50);
                // Écarlatine: Battage for 10 s, once every 20 s; under it, Faucher strikes every enemy around
                if (player->HasAura(SPELL_THRESH))
                    player->CastSpell(player, SPELL_THRESH_HIT, true);
                else if (player->HasAura(TALENT_REDSHADE) && Now() >= state->redshadeCooldownUntil)
                {
                    state->redshadeCooldownUntil = Now() + 20000;
                    player->CastSpell(player, SPELL_THRESH, true);
                }
                // Décimation: the 6th Faucher readies Décimer
                if (player->HasAura(TALENT_DECIMATION))
                {
                    if (Stacks(player, SPELL_DECIMATION) >= 5)
                    {
                        player->RemoveAurasDueToSpell(SPELL_DECIMATION);
                        player->CastSpell(player, SPELL_DECIMATE_READY, true);
                    }
                    else
                        AddStack(player, player, SPELL_DECIMATION);
                }
                break;
            case SPELL_SPECTRE_STRIDE:
            {
                // The strike as the Faucheur lands behind its target
                ObjectGuid const guid = player->GetGUID();
                ObjectGuid const targetGuid = target ? target->GetGUID() : ObjectGuid::Empty;
                player->m_Events.AddEventAtOffset([guid, targetGuid]()
                {
                    Player* reaper = ObjectAccessor::FindPlayer(guid);
                    if (!reaper)
                        return;
                    if (Unit* enemy = ObjectAccessor::GetUnit(*reaper, targetGuid))
                        if (enemy->IsAlive() && reaper->IsValidAttackTarget(enemy))
                            reaper->CastSpell(enemy, SPELL_SPECTRE_STRIDE_HIT, true);
                }, Milliseconds(250));
                if (player->HasAura(TALENT_BETWEEN_SHADOWS))
                    AddSouls(player, 1, target);
                GhastlyFormShield(player);
                break;
            }
            case SPELL_SCYTHE_RUSH:
            case SPELL_VEILWALK:
                GhastlyFormShield(player);
                break;
            case SPELL_RELIQUARY:
            {
                bool infused = false;
                if (uint8 const souls = ConsumeSouls(player, infused))
                {
                    state->boltsInfused = infused;
                    ScheduleSoulBolts(player, souls);
                }
                break;
            }
            case SPELL_DEATHWIND:
                AddSouls(player, 1, nullptr);
                break;
            case SPELL_SOULSLAM:
            {
                bool infused = false;
                uint8 const souls = ConsumeSouls(player, infused);
                state->soulslamFactor = (1.0f + SoulslamPerSoul * float(souls)) *
                    (infused ? float(InfusedFactor) : 1.0f);
                state->soulslamRefund = player->HasAura(TALENT_SOUL_PIERCER) ? souls : 0;
                break;
            }
            case SPELL_DEATHSTALKER:
                // Anxiété on a humanoid
                if (target && target->GetCreatureType() == CREATURE_TYPE_HUMANOID)
                    player->CastSpell(target, SPELL_ANXIETY, true);
                break;
            case SPELL_SINISTER_LITANY:
                AddSouls(player, 3, CurrentEnemy(player, 40.0f));
                if (player->HasAura(TALENT_GHOST))
                    player->CastSpell(player, SPELL_GHOST, true);
                break;
            case SPELL_TORMENTED_SOULS:
            {
                bool infused = false;
                if (uint8 const souls = ConsumeSouls(player, infused))
                {
                    uint8 const stacks = souls * (player->HasAura(TALENT_SOULFUSED_CONSTITUTION) ? 2 : 1);
                    if (Aura* aura = player->AddAura(SPELL_TORMENTED_SOULS_AURA, player))
                        aura->SetStackAmount(std::min<uint8>(stacks, aura->GetSpellInfo()->StackAmount));
                    player->ModifyPower(POWER_RUNIC_POWER, 100 * souls);
                }
                break;
            }
            case SPELL_MASOCHISTIC_RAGE:
            {
                ObjectGuid const guid = player->GetGUID();
                player->m_Events.AddEventAtOffset([guid]()
                {
                    Player* reaper = ObjectAccessor::FindPlayer(guid);
                    if (!reaper || !reaper->IsAlive())
                        return;
                    uint32 const damage = uint32(reaper->GetTotalAttackPowerValue(BASE_ATTACK) * MasochisticRageAp);
                    // Never the killing blow
                    if (damage && damage < reaper->GetHealth())
                        Unit::DealDamage(reaper, reaper, damage, nullptr, SELF_DAMAGE, SPELL_SCHOOL_MASK_SHADOW,
                            nullptr, false);
                    reaper->CastSpell(reaper, SPELL_MASOCHISTIC_RAGE_BUFF, true);
                }, Milliseconds(1000));
                break;
            }
            case SPELL_CROWS_HARVEST:
                state->crowsFragments = 0;
                break;
            case SPELL_SLAUGHTER:
                state->crimsonDeathUsed = false;
                break;
            case SPELL_DOOMREND:
                player->ModifyPower(POWER_RUNIC_POWER, 150);
                break;
            case SPELL_HARVEST_TIME:
                // Frénésie sanglante on the target, 20 yd at most
                if (player->HasAura(TALENT_BLOOD_FRENZY))
                    if (Unit* enemy = CurrentEnemy(player, 20.0f))
                        player->CastSpell(enemy, SPELL_BLOOD_FRENZY, true);
                break;
            case SPELL_SHUDDER_SCYTHE:
                ScheduleBlows(player, target, SPELL_SHUDDER_SCYTHE_HIT, 4, uint32(ShudderIntervalMs));
                break;
            case SPELL_DIRGE:
                if (target && player->haveOffhandWeapon())
                    player->CastSpell(target, SPELL_DIRGE_OFFHAND, true);
                // Porteur de fin: a soul with each Complainte
                if (player->HasAura(SPELL_ENDBRINGER))
                    AddSouls(player, 1, target);
                break;
            case SPELL_DEATHCHASER:
                if (target)
                {
                    state->chasingDeathTarget = target->GetGUID();
                    state->chasingDeathUntil = Now() + 30000;
                }
                break;
            case SPELL_ENDBRINGER:
                if (player->HasAura(TALENT_GRAVESITE))
                    player->CastSpell(player, SPELL_GRAVESITE, true);
                break;
            case SPELL_SHADE:
                // Into Underwalk even in combat
                player->CombatStop(true);
                player->CastSpell(player, SPELL_UNDERWALK, true);
                break;
            case SPELL_SEPULCHRAL_RENEWAL:
                for (uint32 spellId : { uint32(SPELL_LIMBO), uint32(SPELL_SOULSLAM), uint32(SPELL_SHADE) })
                    player->RemoveSpellCooldown(spellId, true);
                break;
            case SPELL_SPECTRAL_SCYTHE:
            {
                bool infused = false;
                uint8 const souls = ConsumeSouls(player, infused);
                SummonSpectralScythes(player, souls, 0, infused);
                break;
            }
            case SPELL_SPECTRAL_WARDEN:
                SpectralWarden(player);
                break;
            case SPELL_DREADWAKE:
                state->dreadwakeSoul = false;
                state->soultakerDone = false;
                break;
            case SPELL_REQUIEM:
                state->requiemSoul = false;
                break;
            case SPELL_SOULREND:
                state->soulrendSoul = false;
                break;
            case SPELL_BOLSTERED_FORM:
                if (player->HasAura(TALENT_INTIMIDATING_PRESENCE))
                    for (Unit* enemy : NearbyEnemies(player, player, 10.0f, 20))
                        player->CastSpell(enemy, SPELL_INTIMIDATED, true);
                break;
            default:
                break;
        }
        // A Faucheur spell breaks Traqueur de mort's stalking when it hurts
        if (spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
            spellInfo->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE))
            player->RemoveAurasDueToSpell(SPELL_DEATHSTALKER_SPEED);
    }
};

class ReaperUnitScript : public UnitScript
{
public:
    ReaperUnitScript() : UnitScript("ReaperUnitScript", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_SPELL_CRIT_CHANCE,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_ON_AURA_REMOVE
    }) { }

    // The talents that change how hard an ability hits
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = Reaper(attacker);
        if (!player || !target || !spellInfo || damage <= 0 || !IsReaperSpell(spellInfo))
            return;
        damage = int32(float(damage) * DamageFactor(player, target, spellInfo, false));
        // Volonté du Geôlier: Frappe d'âme and 30% of Strength
        if (spellInfo->Id == SPELL_SOUL_STRIKE && player->HasAura(TALENT_JAILERS_WILL))
            damage += int32(player->GetStat(STAT_STRENGTH) * 0.3f);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        Player* player = Reaper(attacker);
        if (!player || !target || !spellInfo || !damage || !IsReaperSpell(spellInfo))
            return;
        damage = uint32(float(damage) * DamageFactor(player, target, spellInfo, true));
        ReaperState* state = GetState(player);
        switch (spellInfo->Id)
        {
            case SPELL_DEATHWIND:
            {
                // The mist heals the Faucheur (Dévoreur d'âmes: 40%)
                float const pct = player->HasAura(TALENT_EATER_OF_SOULS) ? 40.0f : float(DeathwindHealPct);
                state->pendingHarvester += float(damage) * pct / 100.0f;
                break;
            }
            case SPELL_DEATHCHASER:
                // Pourriture d'âme: one more stack for each tick
                if (player->HasAura(TALENT_SOULROT))
                    AddStack(player, target, SPELL_SOULROT, false);
                break;
            default:
                break;
        }
    }

    // Sans entraves: Traits d'âme always critical below 20%
    void ModifySpellCritChance(Unit const* caster, Unit const* victim, SpellInfo const* spellInfo,
                               float& critChance) override
    {
        Player const* player = Reaper(caster);
        if (!player || !victim || !spellInfo || spellInfo->Id != SPELL_SOUL_BOLT)
            return;
        if (player->HasAura(TALENT_UNBOUND) && victim->HealthBelowPct(20))
            critChance = 100.0f;
    }

    // What a hit leaves behind: souls and fragments, heals and the talents riding on each ability
    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool critical) override
    {
        Player* player = Reaper(caster);
        if (!player || !victim || !spellInfo || victim == caster || !IsReaperSpell(spellInfo))
            return;
        ReaperState* state = GetState(player);
        bool const melee = spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE;
        switch (spellInfo->Id)
        {
            case SPELL_REAP:
                AddFragments(player, 1, victim);
                PainmailStack(player);
                break;
            case SPELL_MURDER:
            {
                uint8 souls = 1;
                if (critical && player->HasAura(TALENT_SOUL_GENERATOR))
                    ++souls;
                AddSouls(player, souls, victim);
                // Soif cramoisie: spent, and at 5 it heals for half the blow
                if (Aura* thirst = player->GetAura(SPELL_CRIMSON_THIRST))
                {
                    if (thirst->GetStackAmount() >= 5)
                        Heal(player, SPELL_CRIMSON_THIRST_HEAL, float(damage) * 0.5f);
                    player->RemoveAurasDueToSpell(SPELL_CRIMSON_THIRST);
                }
                if (player->HasAura(TALENT_RUIN))
                    player->CastSpell(victim, SPELL_RUIN, true);
                if (player->HasAura(TALENT_WELL_OF_SOULS))
                    ReduceCooldown(player, SPELL_SPECTRAL_SCYTHE, 1000);
                break;
            }
            case SPELL_SOULREND:
                if (player->HasAura(SPELL_SPEC_SOUL))
                    player->CastSpell(victim, SPELL_WEAKENED_SOUL, true);
                if (!state->soulrendSoul && player->HasAura(TALENT_SOULSTORM) && roll_chance_i(40))
                {
                    state->soulrendSoul = true;
                    AddSouls(player, 1, victim);
                }
                break;
            case SPELL_SPECTRE_STRIDE_HIT:
                // Embusqué d'anima: a quarter of the strike again, over 6 s
                if (player->HasAura(TALENT_ANIMA_AMBUSHER) && damage)
                    player->CastCustomSpell(SPELL_ANIMA_AMBUSH, SPELLVALUE_BASE_POINT0,
                        std::max(1, int32(damage / 4 / 6)), victim, true);
                break;
            case SPELL_SOULSLAM:
                if (state->soulslamRefund)
                {
                    uint8 const refund = state->soulslamRefund;
                    state->soulslamRefund = 0;
                    AddSouls(player, refund, victim);
                }
                break;
            case SPELL_WRAITHBLADE:
                AddSouls(player, 3, victim);
                if (player->HasAura(TALENT_MORTALS_END))
                    if (Aura* aura = player->AddAura(SPELL_EXTINCTION, player))
                        aura->SetCharges(2);
                if (player->HasAura(TALENT_THE_END_IS_NEAR))
                    player->CastSpell(player, SPELL_THE_END_IS_NEAR, true);
                break;
            case SPELL_SOUL_HARVEST:
            {
                // Its heal: all of its damage (Fourneau d'âmes: half as much again)
                float const factor = player->HasAura(TALENT_SOUL_FURNACE) ? 1.5f : 1.0f;
                Heal(player, SPELL_SOUL_HARVEST_HEAL, float(damage) * factor);
                break;
            }
            case SPELL_SLAUGHTER:
                AddSouls(player, 1, victim);
                if (critical && player->HasAura(TALENT_SOULS_FOR_THE_SLAUGHTER))
                    player->CastSpell(player, SPELL_SOULS_FOR_THE_SLAUGHTER, true);
                // Mort cramoisie: once more, free (never from its own second blow)
                if (!state->crimsonDeathUsed && player->HasAura(TALENT_CRIMSON_DEATH) && roll_chance_i(20))
                {
                    state->crimsonDeathUsed = true;
                    ObjectGuid const guid = player->GetGUID();
                    ObjectGuid const targetGuid = victim->GetGUID();
                    player->m_Events.AddEventAtOffset([guid, targetGuid]()
                    {
                        Player* reaper = ObjectAccessor::FindPlayer(guid);
                        if (!reaper)
                            return;
                        if (Unit* enemy = ObjectAccessor::GetUnit(*reaper, targetGuid))
                            if (enemy->IsAlive() && reaper->IsValidAttackTarget(enemy))
                                reaper->CastSpell(enemy, SPELL_SLAUGHTER, true);
                    }, Milliseconds(400));
                }
                break;
            case SPELL_CROWS_HARVEST:
                if (state->crowsFragments < 3)
                {
                    ++state->crowsFragments;
                    AddFragments(player, 1, victim);
                }
                DarkrendBleed(player, victim);
                break;
            case SPELL_DOOMREND:
                if (critical && player->HasAura(TALENT_BLOOD_KNIGHT))
                    AddSouls(player, 1, victim);
                if (victim->HasAura(SPELL_RUIN, player->GetGUID()))
                    player->CastSpell(player, SPELL_RUINOUS_FRENZY, true);
                DarkrendBleed(player, victim);
                break;
            case SPELL_SHUDDER_SCYTHE:
            case SPELL_SHUDDER_SCYTHE_HIT:
                AddFragments(player, 1, victim);
                break;
            case SPELL_DIRGE:
                AddFragments(player, player->haveOffhandWeapon() ? 1 : 2, victim);
                [[fallthrough]];
            case SPELL_DIRGE_OFFHAND:
                if (spellInfo->Id == SPELL_DIRGE_OFFHAND)
                    AddFragments(player, 1, victim);
                if (critical && player->HasAura(TALENT_SOUL_GENERATOR))
                    player->ModifyPower(POWER_RUNIC_POWER, 100);
                if (player->HasAura(SPELL_ENDBRINGER))
                    Heal(player, SPELL_ENDBRINGER_HEAL, float(damage) * 0.75f);
                break;
            case SPELL_GHOSTLY_WEAPON_HIT:
                if (player->HasAura(TALENT_LAMENTING))
                    Heal(player, SPELL_LAMENTING_HEAL, float(damage) * 0.5f);
                break;
            case SPELL_SOUL_STRIKE:
            {
                AddSouls(player, 1, victim);
                // Its heal: a share of the blow and of the missing health (Voleur de vie, Faux du moissonneur)
                float missingPct = SoulStrikeMissingPct;
                if (player->HasAura(TALENT_LIFESTEALER))
                    missingPct += 3.0f;
                float heal = float(damage) * SoulStrikeHealPct / 100.0f +
                    float(player->GetMaxHealth() - player->GetHealth()) * missingPct / 100.0f;
                if (player->HasAura(TALENT_HARVESTERS_SCYTHE))
                    heal *= 1.15f;
                Heal(player, SPELL_SOUL_STRIKE_HEAL, heal);
                ScythesFollowSoulStrike(player, victim);
                if (player->HasAura(TALENT_ESSENCE_BINDER))
                    player->CastSpell(player, SPELL_ESSENCE_BINDER, true);
                if (player->HasAura(SPELL_DECIMATE_READY))
                {
                    player->RemoveAurasDueToSpell(SPELL_DECIMATE_READY);
                    player->CastSpell(player, SPELL_DECIMATE, true);
                }
                if (player->HasAura(TALENT_WELL_OF_SOULS))
                    ReduceCooldown(player, SPELL_SPECTRAL_SCYTHE, 1000);
                PainmailStack(player);
                break;
            }
            case SPELL_DECIMATE:
                AddFragments(player, 1, victim);
                player->ModifyPower(POWER_RUNIC_POWER, 50);
                break;
            case SPELL_SPECTRAL_SCYTHE_HIT:
                if (player->HasAura(TALENT_WARDEN_OF_THE_LOST))
                    ReduceCooldown(player, SPELL_SPECTRAL_WARDEN, 500);
                break;
            case SPELL_SPECTRAL_WARDEN_HIT:
                if (player->HasAura(TALENT_WARDEN_OF_THE_LOST))
                    ReduceCooldown(player, SPELL_SPECTRAL_WARDEN, 500);
                if (player->HasAura(TALENT_LIFE_TAP))
                    Heal(player, SPELL_LIFE_TAP_HEAL, float(damage) * 2.0f);
                break;
            case SPELL_DREADWAKE:
                if (!state->dreadwakeSoul)
                {
                    state->dreadwakeSoul = true;
                    AddSouls(player, 1, victim);
                    PainmailStack(player);
                }
                if (!state->soultakerDone && player->HasAura(TALENT_SOULTAKER))
                {
                    state->soultakerDone = true;
                    ReduceCooldown(player, SPELL_TORMENTED_SOULS, 1000);
                }
                break;
            case SPELL_REQUIEM:
                if (!state->requiemSoul && player->HasAura(TALENT_SOULSTORM) && roll_chance_i(40))
                {
                    state->requiemSoul = true;
                    AddSouls(player, 1, victim);
                }
                break;
            default:
                break;
        }
        // Every melee ability: Arme fantomatique's frost and Garde fantomatique's cleave, the physical talents
        if (melee && damage && spellInfo->Id != SPELL_GHASTLY_HILT)
        {
            GhostlyWeaponEcho(player, victim, damage);
            GhastlyHiltCleave(player, victim, damage);
            if (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_NORMAL)
                OnPhysicalDamage(player);
            if (player->HasAura(TALENT_JAILERS_CALL) && victim->HealthBelowPct(20))
                player->CastSpell(victim, SPELL_JAILERS_CALL, true);
            if (player->HasAura(TALENT_SOULSIGHT) && roll_chance_i(15))
                AddFragments(player, 1, victim);
        }
        if (critical)
            OnDirectCritical(player, victim, spellInfo);
    }

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!aurApp)
            return;
        Aura* aura = aurApp->GetBase();
        uint32 const spellId = aura->GetId();
        if (spellId < 98000 || spellId > 98999)
            return;
        Player* player = Reaper(aura->GetCaster());
        if (!player)
            return;
        bool const expired = mode == AURA_REMOVE_BY_EXPIRE;
        switch (spellId)
        {
            case SPELL_UNDERWALK:
                // Depuis les ombres; Au-delà du voile ends with it
                if (unit == player)
                {
                    player->RemoveAurasDueToSpell(SPELL_BEYOND_THE_VEIL);
                    if (player->HasAura(TALENT_FROM_THE_SHADOWS))
                        player->CastSpell(player, SPELL_FROM_THE_SHADOWS, true);
                }
                break;
            case SPELL_GHASTLY_SCREECH:
                // The burst as the silence ends
                if (unit != player && unit->IsAlive())
                    player->CastSpell(unit, SPELL_GHASTLY_SCREECH_BURST, true);
                break;
            case SPELL_BLOOD_FRENZY:
                if (expired && unit->IsAlive())
                    player->CastSpell(unit, SPELL_BLOOD_FRENZY_BURST, true);
                break;
            case SPELL_SOULROT:
                if (expired && unit->IsAlive())
                {
                    int32 const burst = int32(player->GetTotalAttackPowerValue(BASE_ATTACK) * SoulrotPerStackAp *
                        float(aura->GetStackAmount()));
                    player->CastCustomSpell(SPELL_SOULROT_BURST, SPELLVALUE_BASE_POINT0, std::max(1, burst), unit,
                        true);
                }
                break;
            case SPELL_DEATHCHASER:
            {
                // Mort poursuivante: a dying enemy keeps it, for 30 s from the cast
                ReaperState* state = GetState(player);
                if (expired && unit->IsAlive() && unit->HealthBelowPct(20) && player->HasAura(TALENT_CHASING_DEATH) &&
                    unit->GetGUID() == state->chasingDeathTarget && Now() < state->chasingDeathUntil)
                    player->AddAura(SPELL_DEATHCHASER, unit);
                break;
            }
            case SPELL_PAINMAIL:
                // Each stack, as it fades: 2% of maximum health and 5 s off Forme renforcée
                if (expired && unit == player)
                {
                    uint8 const stacks = aura->GetStackAmount();
                    Heal(player, SPELL_SOUL_STRIKE_HEAL, float(player->CountPctFromMaxHealth(2 * stacks)));
                    ReduceCooldown(player, SPELL_BOLSTERED_FORM, 5000 * stacks);
                }
                break;
            case SPELL_TORMENTED_SOULS_AURA:
                player->RemoveAurasDueToSpell(SPELL_HARD_BARGAIN);
                break;
            case SPELL_SPECTRAL_SCYTHE:
                GetState(player)->scythesInfused = false;
                break;
            case SPELL_REAPED_SOUL:
            case SPELL_SOUL_FRAGMENT:
            case SPELL_SOUL_INFUSION:
                // A soul or fragment faded out of combat: the HUD follows
                if (unit == player)
                    player->m_Events.AddEventAtOffset([guid = player->GetGUID()]()
                    {
                        if (Player* reaper = ObjectAccessor::FindPlayer(guid))
                        {
                            UpdateInfusion(reaper);
                            SyncHud(reaper);
                        }
                    }, Milliseconds(1));
                break;
            default:
                break;
        }
    }

    // Every unit script hears this one: the heals worked out from damage dealt, Âmes tourmentées on the Faucheur hit
    uint32 DealDamage(Unit* attacker, Unit* victim, uint32 damage, DamageEffectType type) override
    {
        if (!damage || attacker == victim)
            return damage;
        if (Player* player = Reaper(attacker))
        {
            ReaperState* state = GetState(player);
            if (player->HasAura(SPELL_SPEC_REAPING))
            {
                float pct = HarvesterPct;
                if (player->HasAura(TALENT_MY_DOMAIN))
                    pct *= InsideOwnDynObject(player, SPELL_HARVESTING_GROUNDS, player) ? 1.35f : 1.1f;
                state->pendingHarvester += float(damage) * pct / 100.0f;
            }
            if (player->HasAura(SPELL_SIPHON_ANIMA))
                state->pendingSiphon += float(damage) * 0.05f;
            player->RemoveAurasDueToSpell(SPELL_DEATHSTALKER_SPEED);
        }
        Player* player = Reaper(victim);
        if (!player || !player->IsAlive() || (type != DIRECT_DAMAGE && type != SPELL_DIRECT_DAMAGE))
            return damage;
        if (Aura* aura = player->GetAura(SPELL_TORMENTED_SOULS_AURA))
        {
            float const heal = player->GetTotalAttackPowerValue(BASE_ATTACK) * TormentedHealAp +
                float(player->GetStat(STAT_STAMINA)) * TormentedHealSta;
            Heal(player, SPELL_TORMENTED_SOUL_HEAL, heal);
            // Vie domptée: a quarter of the time, the soul stays
            if (!(player->HasAura(TALENT_HARNESSED_LIFE) && roll_chance_i(25)))
                aura->ModStackAmount(-1);
        }
        return damage;
    }

private:
    static float DamageFactor(Player* player, Unit* target, SpellInfo const* spellInfo, bool periodic)
    {
        ReaperState* state = GetState(player);
        float factor = 1.0f;
        // Hémorragie: enemies below 35%
        if (player->HasAura(TALENT_HEMORRHAGE) && target->HealthBelowPct(35))
            factor *= 1.1f;
        // Le moment est venu: slowed enemies
        if (player->HasAura(TALENT_THE_TIME_HAS_COME) && target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED))
            factor *= 1.1f;
        // Requiem final: shadow below 20%
        if (uint8 const rank = Rank(player, TALENT_FINAL_REQUIEM_1, TALENT_FINAL_REQUIEM_2))
            if ((spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_SHADOW) && target->HealthBelowPct(20))
                factor *= 1.0f + 0.1f * float(rank);
        if (periodic)
            return factor;
        switch (spellInfo->Id)
        {
            case SPELL_DOOMREND:
            case SPELL_SLAUGHTER:
                if (player->HasAura(TALENT_RUIN) && target->HasAura(SPELL_RUIN, player->GetGUID()))
                    factor *= 1.25f;
                break;
            case SPELL_WRAITHBLADE:
                if (player->HasAura(TALENT_SHATTERED_SOULS) &&
                    target->HasAura(SPELL_SOULREND_DEBUFF, player->GetGUID()))
                    factor *= 2.0f;
                break;
            case SPELL_SOUL_STRIKE:
                if (player->HasAura(TALENT_SPIRIT_CULLING) && player->HasAura(SPELL_SPECTRAL_SCYTHE))
                    factor *= 1.2f;
                break;
            case SPELL_DIRGE:
            case SPELL_DIRGE_OFFHAND:
            {
                // 130% of the weapon with a dagger rather than 70%
                Item const* weapon = player->GetWeaponForAttack(spellInfo->Id == SPELL_DIRGE ? BASE_ATTACK : OFF_ATTACK,
                    true);
                if (weapon && weapon->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER)
                    factor *= 130.0f / 70.0f;
                break;
            }
            case SPELL_SOULSLAM:
                factor *= state->soulslamFactor;
                factor *= ConsumerBonus(player);
                break;
            case SPELL_SOUL_BOLT:
                if (state->boltsInfused)
                    factor *= InfusedFactor;
                if (player->HasAura(TALENT_UNBOUND) && target->HealthBelowPct(20))
                    factor *= 1.25f;
                factor *= ConsumerBonus(player);
                break;
            case SPELL_SPECTRAL_SCYTHE_HIT:
                if (state->scythesInfused)
                    factor *= InfusedFactor;
                factor *= ConsumerBonus(player);
                break;
            case SPELL_CROWS_HARVEST:
                if (player->HasAura(TALENT_HUNGERING_SCYTHE) && InsideOwnDynObject(player, SPELL_DEATHWIND, target))
                    factor *= 1.15f;
                break;
            default:
                break;
        }
        return factor;
    }

    // Seigneur de la mort: under Arme fantomatique, what consumes souls hits harder
    static float ConsumerBonus(Player* player)
    {
        return player->HasAura(TALENT_LORD_OF_DEATH) && player->HasAura(SPELL_GHOSTLY_WEAPON) ? 1.25f : 1.0f;
    }
};

class ReaperPlayerScript : public PlayerScript
{
public:
    ReaperPlayerScript() : PlayerScript("ReaperPlayerScript", {
        PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LEVEL_CHANGED, PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!Reaper(player))
            return;
        ReaperState* state = GetState(player);
        state->healMs += diff;
        state->tickMs += diff;
        if (state->healMs >= 1000)
        {
            state->healMs = 0;
            Heal(player, SPELL_HARVESTER_HEAL, state->pendingHarvester);
            Heal(player, SPELL_SIPHON_ANIMA_HEAL, state->pendingSiphon);
            state->pendingHarvester = 0.0f;
            state->pendingSiphon = 0.0f;
        }
        if (state->tickMs < 500)
            return;
        state->tickMs = 0;
        if (!player->IsAlive())
            return;
        // The group auras of Moissonneur sanglant / Vue de l'âme, Présence de la mort, Garde éthérée
        KeepAura(player, SPELL_BLOOD_HARVESTER_AURA, player->HasAura(TALENT_BLOOD_HARVESTER) ||
            player->HasAura(TALENT_SOULSIGHT));
        KeepAura(player, SPELL_DEATHS_PRESENCE_AURA, player->HasAura(TALENT_DEATHS_PRESENCE));
        KeepAura(player, SPELL_ETHEREAL_GUARD_AURA, player->HasAura(TALENT_ETHEREAL_GUARD));
        // Réflexes spirituels below 35%
        KeepAura(player, SPELL_SPIRITUAL_REFLEXES,
            player->HasAura(TALENT_SPIRITUAL_REFLEXES) && player->HealthBelowPct(35));
        // Au-delà du voile while in Marche funèbre
        KeepAura(player, SPELL_BEYOND_THE_VEIL,
            player->HasAura(TALENT_BEYOND_THE_VEIL) && player->HasAura(SPELL_UNDERWALK));
        // Marché difficile while Âmes tourmentées lasts
        KeepAura(player, SPELL_HARD_BARGAIN, player->HasAura(TALENT_HARD_BARGAIN) &&
            player->HasAura(SPELL_TORMENTED_SOULS_AURA));
        // Royaume des esprits: inside its own Vent de mort
        if (player->HasAura(TALENT_SPIRIT_REALM) && InsideOwnDynObject(player, SPELL_DEATHWIND, player))
            player->CastSpell(player, SPELL_SPIRIT_REALM, true);
        HoldHarvestingGround(player);
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        if (!Reaper(killer))
            return;
        // Griffe fantôme comes back with every kill
        killer->RemoveSpellCooldown(SPELL_GHOST_CLAW, true);
        // Récolteur d'âmes: a kill worth experience
        if (killer->HasAura(TALENT_SOUL_HARVESTER) && killer->isHonorOrXPTarget(killed))
        {
            AddSouls(killer, 1, nullptr);
            killer->CastSpell(killer, SPELL_SOUL_HARVESTER, true);
        }
    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* killed) override
    {
        if (Reaper(killed))
            SyncHud(killed, true);
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!Reaper(player))
            return;
        RestoreTraining(player);
        LearnUnlockedAbilities(player);
        player->m_Events.AddEventAtOffset([guid = player->GetGUID()]()
        {
            if (Player* reaper = ObjectAccessor::FindPlayer(guid))
            {
                UpdateInfusion(reaper);
                SyncHud(reaper, true);
            }
        }, Milliseconds(2000));
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!Reaper(player))
            return;
        RestoreTraining(player);
        LearnUnlockedAbilities(player);
    }
};

// The hidden passive's procs: the auto attacks the Faucheur lands, the blows it parries or dodges
class spell_reaper_passive : public AuraScript
{
    PrepareAuraScript(spell_reaper_passive);

    bool CheckProc(ProcEventInfo& /*eventInfo*/)
    {
        return Reaper(GetTarget()) != nullptr;
    }

    void HandleProc(ProcEventInfo& eventInfo)
    {
        Player* player = Reaper(GetTarget());
        if (!player)
            return;
        uint32 const typeMask = eventInfo.GetTypeMask();
        uint32 const hitMask = eventInfo.GetHitMask();
        if (typeMask & PROC_FLAG_DONE_MELEE_AUTO_ATTACK)
        {
            DamageInfo* damageInfo = eventInfo.GetDamageInfo();
            uint32 const damage = damageInfo ? damageInfo->GetDamage() : 0;
            WeaponAttackType const attackType = damageInfo ? damageInfo->GetAttackType() : BASE_ATTACK;
            OnAutoAttackHit(player, eventInfo.GetActionTarget(), damage, (hitMask & PROC_HIT_CRITICAL) != 0,
                attackType);
            return;
        }
        if (hitMask & (PROC_HIT_DODGE | PROC_HIT_PARRY))
            OnAvoided(player, eventInfo.GetActor(), (hitMask & PROC_HIT_PARRY) != 0);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_reaper_passive::CheckProc);
        OnProc += AuraProcFn(spell_reaper_passive::HandleProc);
    }
};

// Pacte du Geôlier: a shield of 30% of maximum health
class spell_reaper_jailers_bargain : public AuraScript
{
    PrepareAuraScript(spell_reaper_jailers_bargain);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* owner = GetUnitOwner())
            amount = int32(owner->CountPctFromMaxHealth(30));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_reaper_jailers_bargain::CalculateAmount, EFFECT_0,
            SPELL_AURA_SCHOOL_ABSORB);
    }
};

// Lacération funeste: the healing its shield absorbs, from Strength (Nourri de sang: 30% more)
class spell_reaper_doomrend_shield : public AuraScript
{
    PrepareAuraScript(spell_reaper_doomrend_shield);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        Player* player = Reaper(GetCaster());
        if (!player)
            return;
        float value = 200.0f + float(player->GetStat(STAT_STRENGTH)) * DoomrendAbsorbStr;
        if (player->HasAura(TALENT_BLOOD_FUELED))
            value *= 1.3f;
        amount = int32(value);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_reaper_doomrend_shield::CalculateAmount, EFFECT_0,
            SPELL_AURA_SCHOOL_HEAL_ABSORB);
    }
};

// Faux déchirante's tick: a share of attack power per stack (the core multiplies by the stacks)
class spell_reaper_darkrend : public AuraScript
{
    PrepareAuraScript(spell_reaper_darkrend);

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        if (Unit* caster = GetCaster())
            amount = std::max<int32>(1, int32(caster->GetTotalAttackPowerValue(BASE_ATTACK) * DarkrendTickAp));
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_reaper_darkrend::CalculateAmount, EFFECT_0,
            SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// The Faucheur's companions as periodic auras: each tick, every scythe (warden, spectral self) strikes its enemy
class spell_reaper_companions : public AuraScript
{
    PrepareAuraScript(spell_reaper_companions);

    void Strike(AuraEffect const* /*aurEff*/)
    {
        Player* player = Reaper(GetTarget());
        if (!player)
            return;
        uint32 const id = GetId();
        if (id == SPELL_SOUL_SPLINTERS)
        {
            int32 const amount = int32(player->GetTotalAttackPowerValue(BASE_ATTACK) * SoulSplinterAp +
                float(player->GetStat(STAT_STAMINA)) * SoulSplinterSta);
            player->CastCustomSpell(SPELL_SOUL_SPLINTERS_HIT, SPELLVALUE_BASE_POINT0, std::max(1, amount), player,
                true);
            return;
        }
        Unit* enemy = CurrentEnemy(player, 30.0f);
        if (!enemy)
            return;
        uint8 const count = std::max<uint8>(1, GetStackAmount());
        uint32 const hit = id == SPELL_SPECTRAL_SCYTHE ? SPELL_SPECTRAL_SCYTHE_HIT
            : id == SPELL_SPECTRAL_WARDEN ? SPELL_SPECTRAL_WARDEN_HIT : SPELL_THE_END_IS_NEAR_HIT;
        for (uint8 index = 0; index < count; ++index)
            player->CastSpell(enemy, hit, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_reaper_companions::Strike, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};
}

void AddReaperScripts()
{
    new ReaperSpellScript();
    new ReaperUnitScript();
    new ReaperPlayerScript();
    RegisterSpellScript(spell_reaper_passive);
    RegisterSpellScript(spell_reaper_jailers_bargain);
    RegisterSpellScript(spell_reaper_doomrend_shield);
    RegisterSpellScript(spell_reaper_darkrend);
    RegisterSpellScript(spell_reaper_companions);
}
