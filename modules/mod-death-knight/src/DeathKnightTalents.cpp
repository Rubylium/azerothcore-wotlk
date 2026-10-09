#include "AllSpellScript.h"
#include "CellImpl.h"
#include "DynamicObject.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "LiveTuning.h"
#include "ObjectAccessor.h"
#include "Pet.h"
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
#include "WorldPacket.h"

#include <algorithm>
#include <array>
#include <deque>
#include <list>
#include <utility>

// The Death Knight's talents and abilities on the retail-style trees (localTools/deathknight/talentTree.json) that
// spell data cannot carry. A talent is its rank spell's aura on the Death Knight (learned by mod-custom-classes'
// TalentTree.cpp), read here with HasAura; a specialization is its passive (Sentinelle sanguine, Cœur de l'hiver,
// Maître de la peste). The abilities and auras named below are in localTools/deathknight/Spells.ps1; the WotLK talents
// the trees reuse keep their own scripts in the core (spell_dk.cpp).
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_BLOOD_DRAW               = 92506,
    TALENT_ICY_TALONS               = 92512,
    TALENT_CRIMSON_SCOURGE          = 92516,
    TALENT_BOILING_BLOOD            = 92517,
    TALENT_OSSUARY                  = 92518,
    TALENT_SANGUINE_STRIKE_1        = 92521,
    TALENT_SANGUINE_STRIKE_2        = 92522,
    TALENT_REINFORCED_MARROW        = 92523,
    TALENT_SANGUINE_GROUND          = 92525,
    TALENT_MURDEROUS_EFFICIENCY     = 92527,
    TALENT_FROZEN_WASTES_1          = 92528,
    TALENT_FROZEN_WASTES_2          = 92529,
    TALENT_FROZEN_CHAMPION          = 92530,
    TALENT_OBLITERATION             = 92532,
    TALENT_IMPROVED_FESTERING       = 92533,
    TALENT_UNLEASHED_SCOURGE        = 92534,
    TALENT_BURSTING_SORES           = 92537,
    TALENT_ARMY_OF_THE_DAMNED       = 92538,
    TALENT_UNHOLY_TRANSFORMATION    = 92539,
};

// The abilities and auras of localTools/deathknight/Spells.ps1
enum Spells : uint32
{
    SPELL_SOUL_REAPER               = 92601,
    SPELL_SOUL_REAPER_DAMAGE        = 92602,
    SPELL_OUTBREAK                  = 92603,
    SPELL_BLOOD_DRAW                = 92604,
    SPELL_ICY_TALONS                = 92605,
    SPELL_DEFILED_GROUND            = 92607,
    SPELL_MARROWREND                = 92610,
    SPELL_BLOOD_BOIL_CHARGES        = 92611,
    SPELL_DEATHS_CARESS             = 92612,
    SPELL_CONSUMPTION               = 92613,
    SPELL_BONESTORM_TICK            = 92615,
    SPELL_TOMBSTONE                 = 92616,
    SPELL_TOMBSTONE_ABSORB          = 92617,
    SPELL_CRIMSON_SCOURGE           = 92619,
    SPELL_BONE_SHIELD               = 92630,
    SPELL_PILLAR_OF_FROST           = 92640,
    SPELL_FROSTSCYTHE               = 92641,
    SPELL_REMORSELESS_WINTER_DAMAGE = 92643,
    SPELL_FROSTWYRMS_FURY           = 92646,
    SPELL_GLACIAL_ADVANCE           = 92647,
    SPELL_BREATH_OF_SINDRAGOSA      = 92644,
    SPELL_BREATH_TICK               = 92645,
    SPELL_FESTERING_STRIKE          = 92660,
    SPELL_FESTERING_WOUND           = 92661,
    SPELL_WOUND_BURST               = 92662,
    SPELL_APOCALYPSE                = 92663,
    SPELL_DARK_TRANSFORMATION       = 92664,
    SPELL_EPIDEMIC                  = 92665,
    SPELL_EPIDEMIC_DAMAGE           = 92666,
    SPELL_UNHOLY_ASSAULT            = 92667,

    // The specializations' passives
    SPELL_SPEC_BLOOD                = 92700,
    SPELL_SPEC_FROST                = 92701,
    SPELL_SPEC_UNHOLY               = 92702,

    // Stock
    SPELL_BLOOD_PLAGUE              = 55078,
    SPELL_FROST_FEVER               = 55095,
    SPELL_DEATH_STRIKE_HEAL         = 45470,
    SPELL_KILLING_MACHINE           = 51124,
    SPELL_FREEZING_FOG              = 59052,
    SPELL_ARMY_OF_THE_DEAD          = 42650,
    SPELL_BLOOD_BOIL_R1             = 48721,
    SPELL_DEATH_STRIKE_R1           = 49998,
    SPELL_SCOURGE_STRIKE_R1         = 55090,
    SPELL_OBLITERATE_R1             = 49020,
};

// Death and Decay's ranks: the one the Death Knight stands in is its dynamic object
constexpr std::array<uint32, 4> DeathAndDecayRanks = { 43265, 49936, 49937, 49938 };

// Death Knight family flags of the stock spells the talents watch (SpellFamilyFlags words 0 and 1), and the new
// spells' own (word 2, Spells.ps1)
constexpr uint32 FLAG0_DEATH_STRIKE = 0x00000010;
constexpr uint32 FLAG0_DEATH_COIL = 0x00002000;
constexpr uint32 FLAG0_HEART_STRIKE = 0x01000000;
constexpr uint32 FLAG1_HOWLING_BLAST = 0x00000002;
constexpr uint32 FLAG1_FROST_STRIKE = 0x00000004;
constexpr uint32 FLAG1_OBLITERATE = 0x00020000;
constexpr uint32 FLAG2_FROSTSCYTHE = 0x00008000;

LiveTuning::KnobInt const BloodBoilCharges("deathknight.blood_boil_charges", 2);
LiveTuning::KnobInt const BloodBoilRechargeMs("deathknight.blood_boil_recharge_ms", 7500);
LiveTuning::KnobInt const BoneShieldMax("deathknight.bone_shield_max", 10);
LiveTuning::KnobUInt const BoneShieldBreakMs("deathknight.bone_shield_break_ms", 2000);
LiveTuning::KnobUInt const DamageWindowMs("deathknight.damage_window_ms", 5000);
constexpr uint32 DefiledCheckMs = 250;
LiveTuning::KnobUInt const BreathTickMs("deathknight.breath_tick_ms", 1000);
// 15 runic power, in the tenths the power is kept in
LiveTuning::KnobInt const BreathCost("deathknight.breath_cost", 150);
LiveTuning::KnobInt const WoundsMax("deathknight.wounds_max", 6);
LiveTuning::KnobInt const WoundRunicPower("deathknight.wound_runic_power", 30);
LiveTuning::KnobUInt const RimeWindowMs("deathknight.rime_window_ms", 1000);
// Pack damage (the combat bench, Fire mage as the reference). Frost's area kit is cloned from Cone of Cold and Blood
// Boil and scales with the spell power a Death Knight does not have: its hits are raised; Death and Decay a little
// less for Frost, more for Unholy, which also has Epidemic (on a pack only) and Wandering Plague
// Retuned 2026-09-29 on the bench's packs of 3 to 12: Frost 59-84% of Fire's, Unholy 210% on three enemies (its
// minions) but 63% on twelve. Big pulls (twenty) are what the area kit is weighed on.
// Lowered 2026-10-09 (simulation bench sweep, every pack on the area build): with Breath of Sindragosa now fed by the
// bot and the spec balance on top, Frost dealt 226-252% of the mage on five enemies and 143-240% on twelve while its
// single target sat at 95-101%. The cones (Frostscythe, Glacial Advance, Fury, Breath) reach the same three enemies
// on five or twelve and set the five's level: cut the most. Remorseless Winter and Death and Decay reach the whole
// pack and carry the twelve: cut less.
// Raised again 2026-10-09 once Breath of Sindragosa's and Remorseless Winter's pulses stopped setting off the paragon
// board's strikes (ParagonSystem.h IsParagonTick): Frost's packs were tuned with those, about the median spec now.
// Frostscythe, Glacial Advance, Fury, Breath
LiveTuning::Knob const FrostAreaFactor("deathknight.frost_area_factor", 1.2f);
LiveTuning::Knob const FrostWinterFactor("deathknight.frost_winter_factor", 2.0f);
LiveTuning::Knob const FrostDeathAndDecayFactor("deathknight.frost_death_and_decay_factor", 1.2f);
// Obliterate, Frost Strike, Howling Blast
LiveTuning::Knob const FrostStrikeFactor("deathknight.frost_strike_factor", 1.4f);
LiveTuning::Knob const UnholyDeathAndDecayFactor("deathknight.unholy_death_and_decay_factor", 2.6f);
LiveTuning::Knob const UnholyWanderingPlagueFactor("deathknight.unholy_wandering_plague_factor", 2.5f);
LiveTuning::Knob const EpidemicPackFactor("deathknight.epidemic_pack_factor", 4.0f);
// Unholy (the combat bench, 2026-09-28: 56-79% of Fire's everywhere): its strikes and Death Coil hit harder, Apocalypse
// much harder, and its minions (the ghoul, Dark Transformation's cleave, the gargoyle, the Army of the Dead), stock
// WotLK creatures that did about a hundred a swing at this gear, carry a real share
// Scourge Strike, Festering Strike, the wounds' bursts, Death Coil
LiveTuning::Knob const UnholyStrikeFactor("deathknight.unholy_strike_factor", 1.3f);
LiveTuning::Knob const ApocalypseFactor("deathknight.apocalypse_factor", 3.0f);
// on one enemy too; EpidemicPackFactor on top on a pack
LiveTuning::Knob const EpidemicFactor("deathknight.epidemic_factor", 1.5f);
// the ghoul and the Army of the Dead
LiveTuning::Knob const UnholyMinionFactor("deathknight.unholy_minion_factor", 3.0f);
// a third of a boss's damage at x4: a cooldown, not the spec
LiveTuning::Knob const GargoyleFactor("deathknight.gargoyle_factor", 1.5f);
constexpr uint32 NPC_EBON_GARGOYLE = 27829;
constexpr uint32 SPELL_SCOURGE_STRIKE_SHADOW = 70890;
constexpr uint32 SPELL_DEATH_COIL_DAMAGE = 47632;
LiveTuning::KnobUInt const EpidemicPackEnemies("deathknight.epidemic_pack_enemies", 3);
constexpr uint32 SPELL_DEATH_AND_DECAY_DAMAGE = 52212;
constexpr uint32 SPELL_WANDERING_PLAGUE = 50526;

// A character's state for its talents. Kept on the player, so it dies with the session.
struct DeathKnightState : public DataMap::Base
{
    // Blood Boil's charges (Blood): how many are left, the recharge of the next one, and the rank last cast
    uint8 boilCharges = BloodBoilCharges;
    bool epidemicPack = false;          // the Epidemic being dealt reaches EpidemicPackEnemies or more
    int32 boilRechargeMs = 0;
    uint32 boilSpellId = 0;
    bool boilCooling = false;

    // Blood's Death Strike heals from the damage taken over the last 5 s
    std::deque<std::pair<uint32, uint32>> damageTaken;
    uint32 boneShieldBreakAt = 0;

    uint32 defiledTimer = 0;
    uint32 breathTimer = BreathTickMs;
    uint32 rimeUntil = 0;            // Rage of the Frozen Champion: a Howling Blast freed by Rime is on its way
};

constexpr char const* StateKey = "DeathKnightTalentState";

DeathKnightState* GetState(Player* player)
{
    return player->CustomData.GetDefault<DeathKnightState>(StateKey);
}

Player* DeathKnight(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_DEATH_KNIGHT ? player : nullptr;
}

bool IsDeathKnightSpell(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_DEATHKNIGHT;
}

bool HasFlag(SpellInfo const* spellInfo, uint8 word, uint32 flag)
{
    return IsDeathKnightSpell(spellInfo) && (spellInfo->SpellFamilyFlags[word] & flag);
}

uint32 FirstRank(SpellInfo const* spellInfo)
{
    return spellInfo ? spellInfo->GetFirstRankSpell()->Id : 0;
}

bool IsBlood(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_BLOOD);
}

bool IsFrost(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_FROST);
}

bool IsUnholy(Unit const* unit)
{
    return unit->HasAura(SPELL_SPEC_UNHOLY);
}

uint32 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

uint8 Stacks(Unit const* unit, uint32 spellId, ObjectGuid caster = ObjectGuid::Empty)
{
    Aura* aura = unit->GetAura(spellId, caster);
    return aura ? aura->GetStackAmount() : 0;
}

// An aura's stack count on the player, raised by `count` up to max (applied first if it is not there)
void AddStacks(Player* player, uint32 spellId, uint8 count, uint8 max)
{
    Aura* aura = player->GetAura(spellId);
    if (!aura)
    {
        aura = player->AddAura(spellId, player);
        if (!aura || !--count)
            return;
    }
    aura->SetStackAmount(std::min<uint8>(aura->GetStackAmount() + count, max));
    aura->RefreshDuration();
}

void ShowStacks(Player* player, uint32 spellId, uint8 stacks)
{
    if (!stacks)
    {
        player->RemoveAurasDueToSpell(spellId);
        return;
    }
    Aura* aura = player->GetAura(spellId);
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (aura)
        aura->SetStackAmount(stacks);
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
        return unit == center || !unit->IsAlive() || !player->IsValidAttackTarget(unit);
    });
    return targets;
}

// Damage under a spell's name, of an amount the talent works out: logged as that spell, absorbs applied. The dealer
// may be the Death Knight or its ghoul.
void DealNamed(Unit* dealer, Unit* target, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !amount || !target->IsAlive())
        return;

    SpellNonMeleeDamage log(dealer, target, spellInfo, spellInfo->GetSchoolMask());
    log.damage = amount;
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    dealer->SendSpellNonMeleeDamageLog(&log);
    dealer->DealSpellDamage(&log, false);
}

// A heal under a spell's name (Soif de sang, Consommation, Tempête d'os)
void HealNamed(Player* player, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !amount || !player->IsAlive())
        return;
    HealInfo healInfo(player, player, amount, spellInfo, spellInfo->GetSchoolMask());
    player->HealBySpell(healInfo);
}

// Whether the spell hit the unit it was aimed at (a weapon strike can miss, be dodged or parried)
bool HitTarget(Spell* spell, Unit* target)
{
    if (!target)
        return false;
    for (TargetInfo const& info : *spell->GetUniqueTargetInfo())
        if (info.targetGUID == target->GetGUID())
            return info.missCondition == SPELL_MISS_NONE || info.missCondition == SPELL_MISS_ABSORB ||
                info.missCondition == SPELL_MISS_BLOCK;
    return false;
}

// The Death Knight stands in its own Death and Decay
bool InOwnDeathAndDecay(Player* player)
{
    for (uint32 spellId : DeathAndDecayRanks)
        if (DynamicObject* dynamic = player->GetDynObject(spellId))
            if (dynamic->GetMapId() == player->GetMapId() && player->GetExactDist2d(dynamic) <= dynamic->GetRadius())
                return true;
    return false;
}

void ResetDeathAndDecay(Player* player)
{
    for (uint32 spellId : DeathAndDecayRanks)
        if (player->HasSpellCooldown(spellId))
            player->RemoveSpellCooldown(spellId, true);
}

// Both diseases on the enemy, cast by the Death Knight as Pestilence spreads them
void Infect(Player* player, Unit* target)
{
    if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
        return;
    player->CastSpell(target, SPELL_FROST_FEVER, true);
    player->CastSpell(target, SPELL_BLOOD_PLAGUE, true);
}

// A rune on cooldown back at once, the one with the longest wait left (Apocalypse)
void RefundRune(Player* player)
{
    uint8 best = MAX_RUNES;
    uint32 longest = 0;
    for (uint8 index = 0; index < MAX_RUNES; ++index)
        if (player->GetRuneCooldown(index) > longest)
        {
            longest = player->GetRuneCooldown(index);
            best = index;
        }
    if (best == MAX_RUNES)
        return;
    player->SetRuneCooldown(best, 0);
    player->SetGracePeriod(best, player->IsInCombat());
    player->AddRunePower(best);
}

// --- Festering Wounds (Unholy) -------------------------------------------------------------------------------------

void AddWounds(Player* player, Unit* target, uint8 count)
{
    if (!target || !count || !target->IsAlive() || !player->IsValidAttackTarget(target))
        return;
    Aura* aura = target->GetAura(SPELL_FESTERING_WOUND, player->GetGUID());
    if (!aura)
    {
        aura = player->AddAura(SPELL_FESTERING_WOUND, target);
        if (!aura || !--count)
            return;
    }
    aura->SetStackAmount(std::min<uint8>(aura->GetStackAmount() + count, WoundsMax));
    aura->RefreshDuration();
}

// Bursts up to `count` of the Death Knight's wounds on the target: Shadow damage and 3 runic power each. The stacks go
// first, then the bursts land, so a burst that kills the target never reaches a removed aura. Returns how many burst.
uint8 BurstWounds(Player* player, Unit* target, uint8 count)
{
    if (!target || !target->IsAlive())
        return 0;
    Aura* aura = target->GetAura(SPELL_FESTERING_WOUND, player->GetGUID());
    if (!aura)
        return 0;
    uint8 const stacks = aura->GetStackAmount();
    uint8 const burst = std::min(count, stacks);
    if (burst >= stacks)
        aura->Remove();
    else
        aura->SetStackAmount(stacks - burst);

    for (uint8 index = 0; index < burst; ++index)
    {
        player->ModifyPower(POWER_RUNIC_POWER, WoundRunicPower);
        if (target->IsAlive())
            player->CastSpell(target, SPELL_WOUND_BURST, true);
    }
    return burst;
}

// --- Blood Boil's charges (Blood) ----------------------------------------------------------------------------------

uint8 MaxBoilCharges(Player* player)
{
    return player->HasAura(TALENT_BOILING_BLOOD) ? BloodBoilCharges + 1 : BloodBoilCharges;
}

// The cooldown the client greys the button with, and the server holds to: a Blood Boil without a charge left waits
// for the next one. Blood Boil has no cooldown of its own (runes were its limit), so spell modifiers cannot give it
// one.
void HoldCooldown(Player* player, uint32 spellId, uint32 cooldownMs)
{
    player->AddSpellCooldown(spellId, 0, cooldownMs);
    WorldPacket data;
    player->BuildCooldownPacket(data, 0, spellId, cooldownMs);
    player->SendDirectMessage(&data);
}

void SpendBoilCharge(Player* player, DeathKnightState* state, uint32 spellId)
{
    uint8 const max = MaxBoilCharges(player);
    state->boilSpellId = spellId;
    if (state->boilCharges >= max)
        state->boilRechargeMs = BloodBoilRechargeMs;
    if (state->boilCharges > 0)
        --state->boilCharges;
    if (!state->boilCharges)
    {
        state->boilCooling = true;
        HoldCooldown(player, spellId, uint32(std::max(state->boilRechargeMs, 1)));
    }
    ShowStacks(player, SPELL_BLOOD_BOIL_CHARGES, state->boilCharges);
}

void UpdateBoilCharges(Player* player, DeathKnightState* state, uint32 diff)
{
    uint8 const max = MaxBoilCharges(player);
    if (!IsBlood(player))
    {
        if (state->boilCooling && state->boilSpellId)
            player->RemoveSpellCooldown(state->boilSpellId, true);
        state->boilCharges = max;
        state->boilRechargeMs = 0;
        state->boilCooling = false;
        player->RemoveAurasDueToSpell(SPELL_BLOOD_BOIL_CHARGES);
        return;
    }

    if (state->boilCharges >= max)
    {
        state->boilCharges = max;
        ShowStacks(player, SPELL_BLOOD_BOIL_CHARGES, max);
        return;
    }

    state->boilRechargeMs -= int32(diff);
    if (state->boilRechargeMs > 0)
        return;

    ++state->boilCharges;
    if (state->boilCooling && state->boilSpellId)
        player->RemoveSpellCooldown(state->boilSpellId, true);
    state->boilCooling = false;
    state->boilRechargeMs = state->boilCharges < max ? BloodBoilRechargeMs : 0;
    ShowStacks(player, SPELL_BLOOD_BOIL_CHARGES, state->boilCharges);
}

// --- Bone Shield (Blood) -------------------------------------------------------------------------------------------

void AddBoneShield(Player* player, uint8 count)
{
    if (IsBlood(player))
        AddStacks(player, SPELL_BONE_SHIELD, count, BoneShieldMax);
}

// Death Strike, Blood: a quarter of the damage taken over the last 5 s (a third or more with Frappe de mort sanguine),
// and never less than 7% of the maximum health
void DeathStrikeHeal(Player* player, DeathKnightState* state)
{
    uint32 const now = NowMs();
    uint64 recent = 0;
    for (auto const& [when, amount] : state->damageTaken)
        if (now - when <= DamageWindowMs)
            recent += amount;
    uint32 const share = 25 + (player->HasAura(TALENT_SANGUINE_STRIKE_2) ? 10 :
        player->HasAura(TALENT_SANGUINE_STRIKE_1) ? 5 : 0);
    int32 const heal = int32(std::max<uint64>(recent * share / 100, player->CountPctFromMaxHealth(7)));
    player->CastCustomSpell(player, SPELL_DEATH_STRIKE_HEAL, &heal, nullptr, nullptr, true);
}

// An Unholy Death Knight's minion (its ghoul, gargoyle, Army of the Dead): the factor its damage is dealt with
float MinionFactor(Unit* attacker)
{
    Creature* minion = attacker ? attacker->ToCreature() : nullptr;
    if (!minion)
        return 1.0f;
    Player* owner = DeathKnight(minion->GetCharmerOrOwnerPlayerOrPlayerItself());
    if (!owner || !IsUnholy(owner))
        return 1.0f;
    return minion->GetEntry() == NPC_EBON_GARGOYLE ? GargoyleFactor : UnholyMinionFactor;
}

// --- Spell casts ---------------------------------------------------------------------------------------------------

class DeathKnightTalentSpellScript : public AllSpellScript
{
public:
    DeathKnightTalentSpellScript() : AllSpellScript("DeathKnightTalentSpellScript", {
        ALLSPELLHOOK_ON_CAST,
        ALLSPELLHOOK_ON_SPELL_CHECK_CAST
    }) { }

    // Rage of the Frozen Champion: a Howling Blast cast while Rime's Freezing Fog is up is the one it frees. The fog is
    // spent while the blast hits, so the cast is marked here, before (OnSpellPrepare comes after an instant cast)
    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& result) override
    {
        Player* player = DeathKnight(spell->GetCaster());
        if (result == SPELL_CAST_OK && player && HasFlag(spell->GetSpellInfo(), 1, FLAG1_HOWLING_BLAST) &&
            player->HasAura(SPELL_FREEZING_FOG) && player->HasAura(TALENT_FROZEN_CHAMPION))
            GetState(player)->rimeUntil = NowMs() + RimeWindowMs;
    }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = DeathKnight(caster);
        if (!player || !spellInfo)
            return;
        DeathKnightState* state = GetState(player);
        Unit* target = spell->m_targets.GetUnitTarget();

        switch (spellInfo->Id)
        {
            case SPELL_OUTBREAK:
                if (target)
                {
                    std::list<Unit*> const around = EnemiesNear(player, target, 10.0f);
                    Infect(player, target);
                    for (Unit* enemy : around)
                        Infect(player, enemy);
                }
                return;
            case SPELL_MARROWREND:
                if (HitTarget(spell, target))
                    AddBoneShield(player, player->HasAura(TALENT_REINFORCED_MARROW) ? 5 : 3);
                return;
            case SPELL_DEATHS_CARESS:
                if (target && target->IsAlive())
                    player->CastSpell(target, SPELL_BLOOD_PLAGUE, true);
                AddBoneShield(player, 2);
                return;
            case SPELL_TOMBSTONE:
            {
                uint8 const bones = std::min<uint8>(Stacks(player, SPELL_BONE_SHIELD), 5);
                if (!bones)
                    return;
                ShowStacks(player, SPELL_BONE_SHIELD, Stacks(player, SPELL_BONE_SHIELD) - bones);
                int32 const absorb = int32(player->CountPctFromMaxHealth(6 * bones));
                player->CastCustomSpell(player, SPELL_TOMBSTONE_ABSORB, &absorb, nullptr, nullptr, true);
                player->ModifyPower(POWER_RUNIC_POWER, 60 * bones);
                return;
            }
            case SPELL_FESTERING_STRIKE:
                if (HitTarget(spell, target))
                    AddWounds(player, target, player->HasAura(TALENT_IMPROVED_FESTERING) ? 3 : 2);
                return;
            case SPELL_APOCALYPSE:
                if (HitTarget(spell, target))
                    for (uint8 burst = BurstWounds(player, target, 4); burst; --burst)
                        RefundRune(player);
                return;
            case SPELL_UNHOLY_ASSAULT:
                AddWounds(player, player->GetVictim() ? player->GetVictim() : player->GetSelectedUnit(), 4);
                return;
            case SPELL_EPIDEMIC:
            {
                std::vector<Unit*> diseased;
                for (Unit* enemy : EnemiesNear(player, player, 40.0f))
                    if (enemy->HasAura(SPELL_BLOOD_PLAGUE, player->GetGUID()))
                        diseased.push_back(enemy);
                state->epidemicPack = diseased.size() >= EpidemicPackEnemies;
                for (Unit* enemy : diseased)
                    player->CastSpell(enemy, SPELL_EPIDEMIC_DAMAGE, true);
                state->epidemicPack = false;
                ArmyOfTheDamned(player);
                return;
            }
            default:
                break;
        }

        if (!IsDeathKnightSpell(spellInfo) || spell->IsTriggered())
            return;
        uint32 const firstRank = FirstRank(spellInfo);

        // Serres glaciales: every technique that spends runic power
        if (spellInfo->PowerType == POWER_RUNIC_POWER && spell->GetPowerCost() > 0 &&
            player->HasAura(TALENT_ICY_TALONS))
            AddStacks(player, SPELL_ICY_TALONS, 1, 3);

        if (IsBlood(player))
        {
            if (firstRank == SPELL_BLOOD_BOIL_R1)
                SpendBoilCharge(player, state, spellInfo->Id);
            if (firstRank == SPELL_DEATH_STRIKE_R1 && HitTarget(spell, target))
                DeathStrikeHeal(player, state);
        }

        // Scourge Strike bursts a wound (the Shadow half it triggers is left out above)
        if (firstRank == SPELL_SCOURGE_STRIKE_R1 && HitTarget(spell, target))
            BurstWounds(player, target, 1);

        // Murderous Efficiency: an Obliterate made critical by Killing Machine spends it
        if (firstRank == SPELL_OBLITERATE_R1 && player->HasAura(TALENT_MURDEROUS_EFFICIENCY))
            player->RemoveAurasDueToSpell(SPELL_KILLING_MACHINE);

        // Obliteration: under Pillar of Frost, Obliterate and Frost Strike bring Killing Machine
        if ((firstRank == SPELL_OBLITERATE_R1 || HasFlag(spellInfo, 1, FLAG1_FROST_STRIKE)) &&
            player->HasAura(TALENT_OBLITERATION) && player->HasAura(SPELL_PILLAR_OF_FROST))
            player->CastSpell(player, SPELL_KILLING_MACHINE, true);

        // Rage of the Frozen Champion: runic power for the freed Howling Blast
        if (HasFlag(spellInfo, 1, FLAG1_HOWLING_BLAST) && NowMs() < state->rimeUntil)
            player->ModifyPower(POWER_RUNIC_POWER, 80);

        if (HasFlag(spellInfo, 0, FLAG0_DEATH_COIL))
            ArmyOfTheDamned(player);
    }

private:
    // Army of the Damned: Death Coil and Epidemic bring Apocalypse and Army of the Dead closer
    static void ArmyOfTheDamned(Player* player)
    {
        if (!player->HasAura(TALENT_ARMY_OF_THE_DAMNED))
            return;
        player->ModifySpellCooldown(SPELL_APOCALYPSE, -1000);
        player->ModifySpellCooldown(SPELL_ARMY_OF_THE_DEAD, -5000);
    }
};

// --- Damage, crits, auras ------------------------------------------------------------------------------------------

class DeathKnightTalentUnitScript : public UnitScript
{
public:
    DeathKnightTalentUnitScript() : UnitScript("DeathKnightTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_SPELL_CRIT_CHANCE,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_ON_AURA_REMOVE
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!damage)
            return;

        if (Player* player = DeathKnight(victim))
        {
            DeathKnightState* state = GetState(player);
            if (IsBlood(player) && attacker != victim)
            {
                uint32 const now = NowMs();
                state->damageTaken.emplace_back(now, damage);
                while (!state->damageTaken.empty() && now - state->damageTaken.front().first > DamageWindowMs)
                    state->damageTaken.pop_front();
            }

            // Soif de sang: under 30% health, a fifth of it back at once, every 2 min at most
            if (damage < player->GetHealth() && player->HealthBelowPctDamaged(30, damage) &&
                player->HasAura(TALENT_BLOOD_DRAW) && !player->HasAura(SPELL_BLOOD_DRAW))
            {
                player->AddAura(SPELL_BLOOD_DRAW, player);
                HealNamed(player, SPELL_BLOOD_DRAW, player->CountPctFromMaxHealth(20));
            }
            return;
        }

        // Transformation impie: the abomination's blows also reach two enemies near its victim
        if (Creature* ghoul = attacker ? attacker->ToCreature() : nullptr)
        {
            Player* owner = DeathKnight(ghoul->GetOwner());
            if (!owner || s_splashing || !ghoul->HasAura(SPELL_DARK_TRANSFORMATION) ||
                !owner->HasAura(TALENT_UNHOLY_TRANSFORMATION) || !victim)
                return;
            s_splashing = true;
            uint8 hit = 0;
            for (Unit* enemy : EnemiesNear(owner, victim, 5.0f))
            {
                if (hit++ >= 2)
                    break;
                DealNamed(ghoul, enemy, SPELL_DARK_TRANSFORMATION, damage / 2);
            }
            s_splashing = false;
        }
    }

    // Auto attacks taken: they break Bone Shield, one charge every 2 s at most; Sol sanguin softens them in Death and
    // Decay
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        if (damage && attacker != target)
            if (float const minion = MinionFactor(attacker); minion != 1.0f)
                damage = uint32(damage * minion);

        Player* player = DeathKnight(target);
        if (!player || attacker == target || !damage)
            return;

        if (player->HasAura(TALENT_SANGUINE_GROUND) && player->HasAura(SPELL_DEFILED_GROUND))
            damage = damage * 9 / 10;

        uint8 const bones = Stacks(player, SPELL_BONE_SHIELD);
        DeathKnightState* state = GetState(player);
        if (!bones || NowMs() < state->boneShieldBreakAt)
            return;
        state->boneShieldBreakAt = NowMs() + BoneShieldBreakMs;
        ShowStacks(player, SPELL_BONE_SHIELD, bones - 1);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage <= 0)
            return;

        // Sol sanguin, taken
        if (Player* victim = DeathKnight(target))
            if (victim != attacker && victim->HasAura(TALENT_SANGUINE_GROUND) && victim->HasAura(SPELL_DEFILED_GROUND))
                damage = damage * 9 / 10;

        if (float const minion = MinionFactor(attacker); minion != 1.0f)
        {
            damage = int32(damage * minion);
            return;
        }

        Player* player = DeathKnight(attacker);
        if (!player || !spellInfo || !IsDeathKnightSpell(spellInfo))
            return;
        float factor = 1.0f;

        // Unholy's strikes (UnholyStrikeFactor ...)
        if (IsUnholy(player))
        {
            uint32 const id = spellInfo->Id;
            if (FirstRank(spellInfo) == SPELL_SCOURGE_STRIKE_R1 || id == SPELL_SCOURGE_STRIKE_SHADOW ||
                id == SPELL_FESTERING_STRIKE || id == SPELL_WOUND_BURST || id == SPELL_DEATH_COIL_DAMAGE)
                factor *= UnholyStrikeFactor;
            else if (id == SPELL_APOCALYPSE)
                factor *= ApocalypseFactor;
        }
        else if (IsFrost(player) && (HasFlag(spellInfo, 1, FLAG1_OBLITERATE) ||
            HasFlag(spellInfo, 1, FLAG1_FROST_STRIKE) || HasFlag(spellInfo, 1, FLAG1_HOWLING_BLAST)))
            factor *= FrostStrikeFactor;

        // Ossuaire: Death Strike and Heart Strike behind five bones or more
        if ((HasFlag(spellInfo, 0, FLAG0_DEATH_STRIKE) || HasFlag(spellInfo, 0, FLAG0_HEART_STRIKE)) &&
            player->HasAura(TALENT_OSSUARY) && Stacks(player, SPELL_BONE_SHIELD) >= 5)
            factor *= 1.15f;

        // Sol sanguin, dealt
        if (player->HasAura(TALENT_SANGUINE_GROUND) && player->HasAura(SPELL_DEFILED_GROUND))
            factor *= 1.1f;

        // Puissance des Terres gelées: Obliterate and Frostscythe with a two-hander
        if (HasFlag(spellInfo, 1, FLAG1_OBLITERATE) || HasFlag(spellInfo, 2, FLAG2_FROSTSCYTHE))
        {
            Item const* weapon = player->GetWeaponForAttack(BASE_ATTACK, true);
            if (weapon && weapon->GetTemplate()->InventoryType == INVTYPE_2HWEAPON)
            {
                if (player->HasAura(TALENT_FROZEN_WASTES_2))
                    factor *= 1.2f;
                else if (player->HasAura(TALENT_FROZEN_WASTES_1))
                    factor *= 1.1f;
            }
        }

        // Rage du champion gelé: the Howling Blast Rime freed deals double
        if (HasFlag(spellInfo, 1, FLAG1_HOWLING_BLAST) && NowMs() < GetState(player)->rimeUntil)
            factor *= 2.0f;

        // Pack damage (FrostAreaFactor ...)
        switch (spellInfo->Id)
        {
            case SPELL_REMORSELESS_WINTER_DAMAGE:
                factor *= FrostWinterFactor;
                break;
            case SPELL_FROSTSCYTHE:
            case SPELL_GLACIAL_ADVANCE:
            case SPELL_FROSTWYRMS_FURY:
            case SPELL_BREATH_TICK:
                factor *= FrostAreaFactor;
                break;
            case SPELL_DEATH_AND_DECAY_DAMAGE:
                if (IsFrost(player))
                    factor *= FrostDeathAndDecayFactor;
                else if (IsUnholy(player))
                    factor *= UnholyDeathAndDecayFactor;
                break;
            case SPELL_WANDERING_PLAGUE:
                if (IsUnholy(player))
                    factor *= UnholyWanderingPlagueFactor;
                break;
            case SPELL_EPIDEMIC_DAMAGE:
                factor *= EpidemicFactor;
                if (GetState(player)->epidemicPack)
                    factor *= EpidemicPackFactor;
                break;
            default:
                break;
        }

        if (factor != 1.0f)
            damage = int32(damage * factor);
    }

    // Fléau cramoisi: a Blood Plague tick may bring Death and Decay back
    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        Player* player = DeathKnight(attacker);
        if (!player || !spellInfo || !damage || spellInfo->Id != SPELL_BLOOD_PLAGUE ||
            !player->HasAura(TALENT_CRIMSON_SCOURGE) || !roll_chance_i(10))
            return;
        ResetDeathAndDecay(player);
        player->AddAura(SPELL_CRIMSON_SCOURGE, player);
    }

    // Efficacité meurtrière: Killing Machine makes Obliterate critical too
    void ModifySpellCritChance(Unit const* caster, Unit const* /*victim*/, SpellInfo const* spellInfo,
                               float& critChance) override
    {
        if (caster && caster->IsPlayer() && HasFlag(spellInfo, 1, FLAG1_OBLITERATE) &&
            caster->HasAura(SPELL_KILLING_MACHINE) && caster->HasAura(TALENT_MURDEROUS_EFFICIENCY))
            critChance = 100.0f;
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
                           bool /*critical*/) override
    {
        Player* player = DeathKnight(caster);
        if (!player || !victim || !spellInfo)
            return;
        uint32 const firstRank = FirstRank(spellInfo);

        // Blood Boil spreads Blood Plague to everything it hits (Blood)
        if (firstRank == SPELL_BLOOD_BOIL_R1 && IsBlood(player) && victim->IsAlive())
            player->CastSpell(victim, SPELL_BLOOD_PLAGUE, true);

        switch (spellInfo->Id)
        {
            case SPELL_CONSUMPTION:
                HealNamed(player, SPELL_CONSUMPTION, damage / 4);
                return;
            case SPELL_BONESTORM_TICK:
                HealNamed(player, SPELL_BONESTORM_TICK, player->CountPctFromMaxHealth(1));
                return;
            case SPELL_WOUND_BURST:
                // Plaies éclatantes: half of the burst on the enemies around
                if (player->HasAura(TALENT_BURSTING_SORES))
                    for (Unit* enemy : EnemiesNear(player, victim, 8.0f))
                        DealNamed(player, enemy, SPELL_WOUND_BURST, damage / 2);
                return;
            default:
                break;
        }

        // Scourge Strike in the Death Knight's own Death and Decay (Unholy): 4 more enemies (8 with Fléau
        // déchaîné), each taking the strike and losing a wound. Its triggered Shadow half is another spell.
        if (firstRank == SPELL_SCOURGE_STRIKE_R1 && IsUnholy(player) && player->HasAura(SPELL_DEFILED_GROUND))
        {
            uint8 const extra = player->HasAura(TALENT_UNLEASHED_SCOURGE) ? 8 : 4;
            uint8 struck = 0;
            for (Unit* enemy : EnemiesNear(player, victim, 8.0f))
            {
                if (struck++ >= extra)
                    break;
                DealNamed(player, enemy, spellInfo->Id, damage);
                BurstWounds(player, enemy, 1);
            }
        }
    }

    // Faucheuse d'âmes: the scythe comes down as the mark ends, on an enemy under 35%
    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!unit || !aurApp || aurApp->GetBase()->GetId() != SPELL_SOUL_REAPER || mode != AURA_REMOVE_BY_EXPIRE)
            return;
        Player* player = DeathKnight(aurApp->GetBase()->GetCaster());
        if (player && unit->IsAlive() && unit->HealthBelowPct(35) && player->IsInMap(unit))
            player->CastSpell(unit, SPELL_SOUL_REAPER_DAMAGE, true);
    }

private:
    static bool s_splashing;
};

bool DeathKnightTalentUnitScript::s_splashing = false;

// --- Every update: Blood Boil's charges, standing in Death and Decay, Breath of Sindragosa --------------------------

class DeathKnightTalentPlayerScript : public PlayerScript
{
public:
    DeathKnightTalentPlayerScript() : PlayerScript("DeathKnightTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_DEATH_KNIGHT)
            return;
        DeathKnightState* state = GetState(player);

        UpdateBoilCharges(player, state, diff);
        UpdateDefiledGround(player, state, diff);
        UpdateBreath(player, state, diff);
    }

private:
    // Sol profané: held while the Death Knight stands in its own Death and Decay (Heart Strike's extra targets are its
    // spell modifier; Scourge Strike's cleave and Sol sanguin read it)
    static void UpdateDefiledGround(Player* player, DeathKnightState* state, uint32 diff)
    {
        state->defiledTimer += diff;
        if (state->defiledTimer < DefiledCheckMs)
            return;
        state->defiledTimer = 0;

        bool const inside = player->IsAlive() && InOwnDeathAndDecay(player);
        if (inside && !player->HasAura(SPELL_DEFILED_GROUND))
            player->AddAura(SPELL_DEFILED_GROUND, player);
        else if (!inside)
            player->RemoveAurasDueToSpell(SPELL_DEFILED_GROUND);
    }

    // Souffle de Sindragosa: a breath every second while 15 runic power are left, then the aura goes
    static void UpdateBreath(Player* player, DeathKnightState* state, uint32 diff)
    {
        if (!player->HasAura(SPELL_BREATH_OF_SINDRAGOSA))
        {
            state->breathTimer = BreathTickMs;
            return;
        }
        state->breathTimer += diff;
        if (state->breathTimer < BreathTickMs)
            return;
        state->breathTimer = 0;

        if (!player->IsAlive() || int32(player->GetPower(POWER_RUNIC_POWER)) < BreathCost)
        {
            player->RemoveAurasDueToSpell(SPELL_BREATH_OF_SINDRAGOSA);
            return;
        }
        player->ModifyPower(POWER_RUNIC_POWER, -BreathCost);
        player->CastSpell(player, SPELL_BREATH_TICK, true);
    }
};
}

void AddDeathKnightTalentScripts()
{
    new DeathKnightTalentSpellScript();
    new DeathKnightTalentUnitScript();
    new DeathKnightTalentPlayerScript();
}
