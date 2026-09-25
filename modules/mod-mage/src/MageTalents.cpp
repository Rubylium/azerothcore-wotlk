#include "AllCreatureScript.h"
#include "AllSpellScript.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <list>
#include <unordered_map>
#include <vector>

// The Mage's talents on the retail-style trees (localTools/mage/talentTree.json) that spell data cannot carry. A talent
// is its rank spell's aura on the mage (learned by mod-custom-classes' TalentTree.cpp), read here with HasAura; the
// abilities and auras named below are in localTools/mage/Spells.ps1. The WotLK talents the trees reuse keep their own
// scripts in the core.
namespace
{
// Talent ranks (dummies, read here)
enum Talents : uint32
{
    TALENT_BLINK_CHARGES        = 92002,
    TALENT_ICE_BLOCK_HEAL       = 92006,
    TALENT_CAUTERIZE            = 92016,
    TALENT_INCANTERS_FLOW       = 92017,
    TALENT_BLINK_SPEED          = 92022,
    TALENT_WARDS_1              = 92023,
    TALENT_WARDS_2              = 92024,
    TALENT_SUSPENDED_TIME       = 92029,
    TALENT_ARCANE_HARMONY       = 92030,
    TALENT_PURE_CLARITY         = 92034,
    TALENT_TOUCH_AMPLIFIED      = 92035,
    TALENT_ARCANE_BOMBARDMENT   = 92036,
    TALENT_PROMPT_PRESENCE      = 92037,
    TALENT_ENLIGHTENED          = 92038,
    TALENT_NETHER_OVERLOAD      = 92039,
    TALENT_ARCANE_AVATAR        = 92040,
    TALENT_FIRE_BLAST_CHARGES   = 92041,
    TALENT_FIRESTARTER          = 92042,
    TALENT_KINDLING             = 92043,
    TALENT_SEARING_TOUCH        = 92044,
    TALENT_CHAIN_BOMB           = 92045,
    TALENT_SUN_KING             = 92046,
    TALENT_FLAME_MASTER         = 92047,
    TALENT_SPLITTING_ICE        = 92050,
    TALENT_FROZEN_VEINS         = 92051,
    TALENT_LONELY_WINTER        = 92052,
    TALENT_EMPOWERED_ELEMENTAL  = 92053,
    TALENT_BONE_CHILLING        = 92055,
    TALENT_ICICLES              = 92056,
};

// The abilities and auras of localTools/mage/Spells.ps1
enum Spells : uint32
{
    SPELL_GREATER_INVISIBILITY  = 92101,
    SPELL_ALTER_TIME            = 92102,
    SPELL_INVISIBILITY_REDUCTION = 92103,
    SPELL_BLINK_CHARGES         = 92106,
    SPELL_FIRE_BLAST_CHARGES    = 92107,
    SPELL_CAUTERIZE_BURN        = 92108,
    SPELL_CAUTERIZED            = 92109,
    SPELL_TOUCH_OF_THE_MAGI     = 92110,
    SPELL_NETHER_TEMPEST        = 92112,
    SPELL_INCANTERS_FLOW        = 92115,
    SPELL_BLINK_SPEED           = 92116,
    SPELL_ARCANE_HARMONY        = 92118,
    SPELL_COMBUSTION            = 92120,
    SPELL_PHOENIX_FLAMES        = 92121,
    SPELL_METEOR                = 92122,
    SPELL_METEOR_IMPACT         = 92124,
    SPELL_FROZEN_VEINS          = 92125,
    SPELL_BONE_CHILLING         = 92126,
    SPELL_ICICLES               = 92127,
    SPELL_SUN_KING              = 92129,
    SPELL_COMET_STORM           = 92130,
    SPELL_COMET_IMPACT          = 92131,
    SPELL_ARCANE_AVATAR         = 92133,

    // Stock
    SPELL_BLINK                 = 1953,
    SPELL_FIRE_BLAST            = 2136,
    SPELL_ICE_BLOCK             = 45438,
    SPELL_PRESENCE_OF_MIND      = 12043,
    SPELL_ARCANE_POWER          = 12042,
    SPELL_ICY_VEINS             = 12472,
    SPELL_CLEARCASTING          = 12536,
    SPELL_ARCANE_BLAST_DEBUFF   = 36032,
    SPELL_HOT_STREAK            = 48108,
    SPELL_IGNITE                = 12654,
    SPELL_LIVING_BOMB_1         = 44457,
    SPELL_LIVING_BOMB_2         = 55359,
    SPELL_LIVING_BOMB_3         = 55360,
    SPELL_ICE_LANCE             = 42914,
};

// Water Elementals: the temporary one, and the Glyph of Eternal Water's
constexpr uint32 NPC_WATER_ELEMENTAL = 510;
constexpr uint32 NPC_WATER_ELEMENTAL_ETERNAL = 37994;

// Mage family flags of the stock spells the talents watch (SpellFamilyFlags words 0 and 1)
constexpr uint32 FLAG0_FIREBALL = 0x00000001;
constexpr uint32 FLAG0_FIRE_BLAST = 0x00000002;
constexpr uint32 FLAG0_SCORCH = 0x00000010;
constexpr uint32 FLAG0_FROSTBOLT = 0x00000020;
constexpr uint32 FLAG0_ARCANE_MISSILES = 0x00000800;
constexpr uint32 FLAG0_BLINK = 0x00010000;
constexpr uint32 FLAG0_ICE_LANCE = 0x00020000;
constexpr uint32 FLAG0_MISSILE = 0x00200000;          // an Arcane Missiles missile
constexpr uint32 FLAG0_PYROBLAST = 0x00400000;
constexpr uint32 FLAG0_MANA_SHIELD = 0x00008000;
constexpr uint32 FLAG0_FIRE_WARD = 0x00000008;
constexpr uint32 FLAG0_FROST_WARD = 0x00000100;
constexpr uint32 FLAG1_ICE_BARRIER = 0x00000001;
constexpr uint32 FLAG1_ARCANE_BARRAGE = 0x00008000;

constexpr uint8 MaxCharges = 2;
constexpr uint32 FlowStepMs = 1000;
constexpr uint8 FlowMaxStacks = 5;
constexpr uint32 SuspendedTimeMs = 45000;
constexpr uint32 FlameMasterMs = 2000;
constexpr uint32 MeteorDelayMs = 3000;
constexpr uint32 CometCount = 7;
constexpr uint32 CometFirstMs = 600;
constexpr uint32 CometStepMs = 200;
constexpr float CometSpread = 5.0f;
constexpr uint8 SunKingStacks = 8;
constexpr uint32 SunKingCombustionMs = 6000;
constexpr uint8 HarmonyMaxStacks = 10;
constexpr uint8 BoneChillingMaxStacks = 10;
constexpr uint8 IciclesMaxStacks = 5;

// A spell's charges (Blink, Fire Blast): how many are left, and the recharge of the next one
struct Charges
{
    uint8 count = MaxCharges;
    int32 rechargeMs = 0;
    uint32 spellId = 0;              // the rank last cast, whose cooldown a spare charge clears
    bool clearCooldown = false;      // a charge is left: the cooldown the cast just started goes, next update
};

// Something the mage's talents land a moment later (a meteor, a comet)
struct Pending
{
    int32 dueMs = 0;
    uint32 spellId = 0;
    uint32 mapId = 0;
    Position where;
};

// A character's state for its talents. Kept on the player, so it dies with the session.
struct MageState : public DataMap::Base
{
    Charges blink;
    Charges fireBlast;
    uint32 flowTimer = 0;
    bool flowRising = true;
    uint32 suspendedTimer = 0;
    uint32 flameMasterTimer = 0;
    std::vector<Pending> pending;

    // Alter Time: where and how the mage was when it started
    bool altered = false;
    WorldLocation alteredAt;
    uint32 alteredHealth = 0;
    uint32 alteredMana = 0;

    // Bonuses a cast sets for its own hits
    float barrageBonus = 0.0f;       // Arcane Harmony and Nether Overload, on the next Arcane Barrage
    float iceLanceBonus = 0.0f;      // the Icicles thrown with the next Ice Lance
    uint32 clarityUntil = 0;         // Pure Clarity: Arcane Missiles cast under Clearcasting
    bool promptPresence = false;     // Prompt Presence: Presence of Mind has a second use left
    bool sunKingReady = false;       // Sun King's Blessing: the next Hot Streak spent comes back, with Combustion
    bool dismissElemental = false;   // Lonely Winter: a Water Elemental came, and goes on the next update
};

constexpr char const* StateKey = "MageTalentState";

// Touch of the Magi: what a marked enemy has stored, by the mage who marked it
struct TouchState : public DataMap::Base
{
    std::unordered_map<ObjectGuid, uint32> stored;
};

constexpr char const* TouchKey = "MageTouchOfTheMagi";

MageState* GetState(Player* player)
{
    return player->CustomData.GetDefault<MageState>(StateKey);
}

Player* MagePlayer(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    return player && player->getClass() == CLASS_MAGE ? player : nullptr;
}

bool IsMageSpell(SpellInfo const* spellInfo)
{
    return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_MAGE;
}

bool HasFlag0(SpellInfo const* spellInfo, uint32 flag)
{
    return IsMageSpell(spellInfo) && (spellInfo->SpellFamilyFlags[0] & flag);
}

bool HasFlag1(SpellInfo const* spellInfo, uint32 flag)
{
    return IsMageSpell(spellInfo) && (spellInfo->SpellFamilyFlags[1] & flag);
}

bool IsLivingBomb(uint32 spellId)
{
    return spellId == SPELL_LIVING_BOMB_1 || spellId == SPELL_LIVING_BOMB_2 || spellId == SPELL_LIVING_BOMB_3;
}

uint32 NowMs()
{
    return GameTime::GetGameTimeMS().count();
}

// An aura's stack count, raised by one up to max (applied first if it is not there)
void AddStack(Player* player, uint32 spellId, uint8 max)
{
    if (Aura* aura = player->GetAura(spellId))
    {
        if (aura->GetStackAmount() < max)
            aura->SetStackAmount(aura->GetStackAmount() + 1);
        aura->RefreshDuration();
        return;
    }
    player->AddAura(spellId, player);
}

uint8 Stacks(Unit* unit, uint32 spellId)
{
    Aura* aura = unit->GetAura(spellId);
    return aura ? aura->GetStackAmount() : 0;
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

// Enemies of the mage within range of center, center left out
std::list<Unit*> EnemiesNear(Player* player, Unit* center, float range)
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

// Damage under a spell's name, of an amount the talent works out: logged as that spell, absorbs applied
void DealNamed(Player* player, Unit* target, uint32 spellId, uint32 amount)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo || !amount || !target->IsAlive())
        return;

    SpellNonMeleeDamage log(player, target, spellInfo, spellInfo->GetSchoolMask());
    log.damage = amount;
    Unit::DealDamageMods(target, log.damage, &log.absorb);
    player->SendSpellNonMeleeDamageLog(&log);
    player->DealSpellDamage(&log, false);
}

// --- Charges ------------------------------------------------------------------------------------------------------

// The cooldown of a rank of a charged spell, with the mage's modifiers (Improved Fire Blast)
int32 ChargeCooldown(Player* player, uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    int32 cooldown = spellInfo ? int32(spellInfo->RecoveryTime) : 0;
    player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
    return std::max(cooldown, 1000);
}

void SpendCharge(Player* player, Charges& charges, uint32 spellId, uint32 aura)
{
    charges.spellId = spellId;
    if (charges.count == MaxCharges)
        charges.rechargeMs = ChargeCooldown(player, spellId);
    if (charges.count > 0)
        --charges.count;
    // A charge left: the cooldown the cast starts is cleared on the next update (it is set after this hook)
    charges.clearCooldown = charges.count > 0;
    ShowStacks(player, aura, charges.count);
}

void UpdateCharges(Player* player, Charges& charges, uint32 talent, uint32 aura, uint32 diff)
{
    if (!player->HasAura(talent))
    {
        charges.count = MaxCharges;
        charges.rechargeMs = 0;
        player->RemoveAurasDueToSpell(aura);
        return;
    }

    if (charges.clearCooldown)
    {
        charges.clearCooldown = false;
        player->RemoveSpellCooldown(charges.spellId, true);
    }

    if (charges.count >= MaxCharges)
    {
        ShowStacks(player, aura, MaxCharges);
        return;
    }

    charges.rechargeMs -= int32(diff);
    if (charges.rechargeMs > 0)
        return;

    // A charge back: usable again if it was the last one gone
    if (charges.count == 0 && charges.spellId)
        player->RemoveSpellCooldown(charges.spellId, true);
    ++charges.count;
    charges.rechargeMs = charges.count < MaxCharges ? ChargeCooldown(player, charges.spellId) : 0;
    ShowStacks(player, aura, charges.count);
}

// --- Spell casts --------------------------------------------------------------------------------------------------

class MageTalentSpellScript : public AllSpellScript
{
public:
    MageTalentSpellScript() : AllSpellScript("MageTalentSpellScript", { ALLSPELLHOOK_ON_CAST }) { }

    void OnSpellCast(Spell* spell, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        Player* player = MagePlayer(caster);
        if (!player || !spellInfo)
            return;
        MageState* state = GetState(player);

        switch (spellInfo->Id)
        {
            case SPELL_METEOR:
                if (WorldLocation const* dest = spell->m_targets.GetDstPos())
                    state->pending.push_back({ int32(MeteorDelayMs), SPELL_METEOR_IMPACT, player->GetMapId(), *dest });
                return;
            case SPELL_COMET_STORM:
                if (Unit* target = spell->m_targets.GetUnitTarget())
                    for (uint32 comet = 0; comet < CometCount; ++comet)
                    {
                        float const angle = rand_norm() * 2.0f * float(M_PI);
                        float const distance = rand_norm() * CometSpread;
                        Position where(target->GetPositionX() + std::cos(angle) * distance,
                            target->GetPositionY() + std::sin(angle) * distance, target->GetPositionZ());
                        state->pending.push_back({ int32(CometFirstMs + comet * CometStepMs), SPELL_COMET_IMPACT,
                            player->GetMapId(), where });
                    }
                return;
            case SPELL_PRESENCE_OF_MIND:
                state->promptPresence = player->HasAura(TALENT_PROMPT_PRESENCE);
                return;
            default:
                break;
        }

        if (!IsMageSpell(spellInfo))
            return;

        uint32 const firstRank = spellInfo->GetFirstRankSpell()->Id;
        if (firstRank == SPELL_BLINK && player->HasAura(TALENT_BLINK_CHARGES))
            SpendCharge(player, state->blink, spellInfo->Id, SPELL_BLINK_CHARGES);
        if (firstRank == SPELL_BLINK && player->HasAura(TALENT_BLINK_SPEED))
            player->CastSpell(player, SPELL_BLINK_SPEED, true);
        if (firstRank == SPELL_FIRE_BLAST && player->HasAura(TALENT_FIRE_BLAST_CHARGES))
            SpendCharge(player, state->fireBlast, spellInfo->Id, SPELL_FIRE_BLAST_CHARGES);

        // Arcane Harmony: each missile of Arcane Missiles adds to the next Arcane Barrage
        if (HasFlag0(spellInfo, FLAG0_MISSILE) && player->HasAura(TALENT_ARCANE_HARMONY))
            AddStack(player, SPELL_ARCANE_HARMONY, HarmonyMaxStacks);

        // Pure Clarity: Arcane Missiles channelled under Clearcasting
        if (HasFlag0(spellInfo, FLAG0_ARCANE_MISSILES) && !HasFlag0(spellInfo, FLAG0_MISSILE) &&
            player->HasAura(TALENT_PURE_CLARITY) && player->HasAura(SPELL_CLEARCASTING))
            state->clarityUntil = NowMs() + 6000;

        // Arcane Barrage takes its bonuses with it: the harmony stored, and a full Arcane Blast count
        if (HasFlag1(spellInfo, FLAG1_ARCANE_BARRAGE))
        {
            float bonus = 0.05f * Stacks(player, SPELL_ARCANE_HARMONY);
            player->RemoveAurasDueToSpell(SPELL_ARCANE_HARMONY);
            if (player->HasAura(TALENT_NETHER_OVERLOAD) && Stacks(player, SPELL_ARCANE_BLAST_DEBUFF) >= 4)
            {
                bonus = (1.0f + bonus) * 2.0f - 1.0f;
                player->ModifyPower(POWER_MANA, int32(player->GetMaxPower(POWER_MANA) / 10));
            }
            state->barrageBonus = bonus;
        }

        // Sun King's Blessing: Pyroblast spending a Hot Streak
        if (HasFlag0(spellInfo, FLAG0_PYROBLAST) && player->HasAura(SPELL_HOT_STREAK) &&
            player->HasAura(TALENT_SUN_KING))
        {
            AddStack(player, SPELL_SUN_KING, SunKingStacks);
            if (Stacks(player, SPELL_SUN_KING) >= SunKingStacks)
            {
                player->RemoveAurasDueToSpell(SPELL_SUN_KING);
                state->sunKingReady = true;
            }
        }

        // Frost: Bone Chilling on every Frost spell, Icicles on Frostbolt, thrown by Ice Lance
        if ((spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST) && player->HasAura(TALENT_BONE_CHILLING))
            AddStack(player, SPELL_BONE_CHILLING, BoneChillingMaxStacks);
        if (HasFlag0(spellInfo, FLAG0_FROSTBOLT) && player->HasAura(TALENT_ICICLES))
            AddStack(player, SPELL_ICICLES, IciclesMaxStacks);
        if (HasFlag0(spellInfo, FLAG0_ICE_LANCE))
        {
            state->iceLanceBonus = 0.5f * Stacks(player, SPELL_ICICLES);
            player->RemoveAurasDueToSpell(SPELL_ICICLES);
        }
    }
};

// --- Damage, crits, auras -----------------------------------------------------------------------------------------

class MageTalentUnitScript : public UnitScript
{
public:
    MageTalentUnitScript() : UnitScript("MageTalentUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_ON_AURA_APPLY,
        UNITHOOK_ON_AURA_REMOVE,
        UNITHOOK_MODIFY_SPELL_CRIT_CHANCE,
        UNITHOOK_ON_SPELL_DAMAGE_DONE
    }) { }

    // Cautérisation: a hit that would kill leaves 35% health and a burn
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        Player* player = MagePlayer(victim);
        if (!player || damage < player->GetHealth() || !player->HasAura(TALENT_CAUTERIZE) ||
            player->HasAura(SPELL_CAUTERIZED))
            return;

        uint32 const floor = player->CountPctFromMaxHealth(35);
        if (player->GetHealth() > floor)
            damage = player->GetHealth() - floor;
        else
        {
            damage = 0;
            player->SetHealth(floor);
        }
        player->AddAura(SPELL_CAUTERIZED, player);
        int32 const tick = int32(player->CountPctFromMaxHealth(5));
        player->CastCustomSpell(player, SPELL_CAUTERIZE_BURN, &tick, nullptr, nullptr, true);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage <= 0 || !attacker)
            return;

        // Empowered elemental: the Water Elemental of a mage with the talent
        if (Creature* creature = attacker->ToCreature())
        {
            uint32 const entry = creature->GetEntry();
            if (entry == NPC_WATER_ELEMENTAL || entry == NPC_WATER_ELEMENTAL_ETERNAL)
                if (Unit* owner = creature->GetOwner(); owner && owner->HasAura(TALENT_EMPOWERED_ELEMENTAL))
                    damage = int32(damage * 1.5f);
            return;
        }

        Player* player = MagePlayer(attacker);
        if (!player || !spellInfo)
            return;
        MageState* state = GetState(player);
        float factor = 1.0f;

        if (HasFlag1(spellInfo, FLAG1_ARCANE_BARRAGE))
        {
            factor *= 1.0f + state->barrageBonus;
            state->barrageBonus = 0.0f;
            if (player->HasAura(TALENT_ARCANE_BOMBARDMENT) && target->HealthBelowPct(35))
                factor *= 1.5f;
        }
        if (HasFlag0(spellInfo, FLAG0_MISSILE) && NowMs() < state->clarityUntil)
            factor *= 1.2f;
        if (HasFlag0(spellInfo, FLAG0_SCORCH) && player->HasAura(TALENT_SEARING_TOUCH) && target->HealthBelowPct(30))
            factor *= 2.5f;
        if (HasFlag0(spellInfo, FLAG0_ICE_LANCE))
        {
            factor *= 1.0f + state->iceLanceBonus;
            state->iceLanceBonus = 0.0f;
        }
        if (player->HasAura(TALENT_ENLIGHTENED) && player->GetPowerPct(POWER_MANA) > 70.0f)
            factor *= 1.06f;

        if (factor != 1.0f)
            damage = int32(damage * factor);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
    {
        Player* player = MagePlayer(attacker);
        if (!player || !spellInfo || !damage)
            return;

        // Nether Tempest: half of each tick on the enemies around the target
        if (spellInfo->Id == SPELL_NETHER_TEMPEST)
            for (Unit* enemy : EnemiesNear(player, target, 10.0f))
                DealNamed(player, enemy, SPELL_NETHER_TEMPEST, damage / 2);

        Store(player, target, damage);
    }

    void ModifySpellCritChance(Unit const* caster, Unit const* victim, SpellInfo const* spellInfo,
                               float& critChance) override
    {
        if (!caster || !victim || !IsMageSpell(spellInfo) || !caster->IsPlayer())
            return;

        // Firestarter: Fireball and Pyroblast against a target still above 90% health
        if ((HasFlag0(spellInfo, FLAG0_FIREBALL) || HasFlag0(spellInfo, FLAG0_PYROBLAST)) &&
            caster->HasAura(TALENT_FIRESTARTER) && victim->GetHealthPct() > 90.0f)
            critChance = 100.0f;
        // Searing Touch: Scorch against a target under 30% health
        if (HasFlag0(spellInfo, FLAG0_SCORCH) && caster->HasAura(TALENT_SEARING_TOUCH) && victim->GetHealthPct() < 30.0f)
            critChance = 100.0f;
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage, bool critical) override
    {
        Player* player = MagePlayer(caster);
        if (!player || !victim || !spellInfo)
            return;

        // Kindling: crits bring Combustion closer
        if (critical && player->HasAura(TALENT_KINDLING) &&
            (HasFlag0(spellInfo, FLAG0_FIREBALL | FLAG0_PYROBLAST | FLAG0_FIRE_BLAST | FLAG0_SCORCH) ||
             spellInfo->Id == SPELL_PHOENIX_FLAMES))
            player->ModifySpellCooldown(SPELL_COMBUSTION, -1000);

        // Splitting Ice: a second enemy near the target takes 80% of the Ice Lance
        if (HasFlag0(spellInfo, FLAG0_ICE_LANCE) && player->HasAura(TALENT_SPLITTING_ICE))
        {
            std::list<Unit*> enemies = EnemiesNear(player, victim, 8.0f);
            if (!enemies.empty())
                DealNamed(player, enemies.front(), spellInfo->Id, damage * 4 / 5);
        }

        // Phoenix Flames: half of it on the enemies around the target
        if (spellInfo->Id == SPELL_PHOENIX_FLAMES)
            for (Unit* enemy : EnemiesNear(player, victim, 8.0f))
                DealNamed(player, enemy, SPELL_PHOENIX_FLAMES, damage / 2);

        Store(player, victim, damage);
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        Player* player = MagePlayer(unit);
        if (!player || !aura)
            return;
        SpellInfo const* spellInfo = aura->GetSpellInfo();

        switch (aura->GetId())
        {
            case SPELL_ALTER_TIME:
            {
                MageState* state = GetState(player);
                state->altered = true;
                state->alteredAt.WorldRelocate(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
                    player->GetPositionZ(), player->GetOrientation());
                state->alteredHealth = player->GetHealth();
                state->alteredMana = player->GetPower(POWER_MANA);
                return;
            }
            case SPELL_ICY_VEINS:
                if (player->HasAura(TALENT_FROZEN_VEINS))
                    if (Aura* veins = player->AddAura(SPELL_FROZEN_VEINS, player))
                        veins->SetDuration(aura->GetDuration());
                return;
            case SPELL_ARCANE_POWER:
                if (player->HasAura(TALENT_ARCANE_AVATAR))
                    if (Aura* avatar = player->AddAura(SPELL_ARCANE_AVATAR, player))
                    {
                        avatar->SetMaxDuration(aura->GetMaxDuration());
                        avatar->SetDuration(aura->GetDuration());
                    }
                return;
            default:
                break;
        }

        // Écrans magiques: the mage's shields absorb more
        uint32 const wards = player->HasAura(TALENT_WARDS_2) ? 30 : player->HasAura(TALENT_WARDS_1) ? 15 : 0;
        if (wards && aura->GetCasterGUID() == player->GetGUID() &&
            (HasFlag0(spellInfo, FLAG0_MANA_SHIELD | FLAG0_FIRE_WARD | FLAG0_FROST_WARD) ||
             HasFlag1(spellInfo, FLAG1_ICE_BARRIER)))
            for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
                if (AuraEffect* effect = aura->GetEffect(index))
                    if (effect->GetAuraType() == SPELL_AURA_SCHOOL_ABSORB || effect->GetAuraType() == SPELL_AURA_MANA_SHIELD)
                        effect->ChangeAmount(effect->GetAmount() * int32(100 + wards) / 100);
    }

    void OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode) override
    {
        if (!unit || !aurApp)
            return;
        Aura const* aura = aurApp->GetBase();
        uint32 const spellId = aura->GetId();

        // On an enemy: Touch of the Magi going off, Living Bomb spreading
        if (spellId == SPELL_TOUCH_OF_THE_MAGI || IsLivingBomb(spellId))
        {
            Player* player = MagePlayer(aura->GetCaster());
            if (!player || mode == AURA_REMOVE_BY_DEATH)
                return;
            if (spellId == SPELL_TOUCH_OF_THE_MAGI)
                Detonate(player, unit);
            else if (mode == AURA_REMOVE_BY_EXPIRE && player->HasAura(TALENT_CHAIN_BOMB))
                SpreadBomb(player, unit, spellId);
            return;
        }

        Player* player = MagePlayer(unit);
        if (!player)
            return;
        MageState* state = GetState(player);

        switch (spellId)
        {
            case SPELL_ICE_BLOCK:
                if (player->HasAura(TALENT_ICE_BLOCK_HEAL) && player->IsAlive())
                    player->ModifyHealth(int32(player->CountPctFromMaxHealth(30)));
                break;
            case SPELL_GREATER_INVISIBILITY:
                if (player->IsAlive())
                    player->AddAura(SPELL_INVISIBILITY_REDUCTION, player);
                break;
            case SPELL_ALTER_TIME:
                if (state->altered && player->IsAlive() && mode != AURA_REMOVE_BY_DEATH &&
                    player->GetMapId() == state->alteredAt.GetMapId())
                {
                    player->NearTeleportTo(state->alteredAt.GetPositionX(), state->alteredAt.GetPositionY(),
                        state->alteredAt.GetPositionZ(), state->alteredAt.GetOrientation());
                    player->SetHealth(std::min(state->alteredHealth, player->GetMaxHealth()));
                    player->SetPower(POWER_MANA, std::min(state->alteredMana, player->GetMaxPower(POWER_MANA)));
                }
                state->altered = false;
                break;
            case SPELL_PRESENCE_OF_MIND:
                // Prompt Presence: the second instant spell
                if (state->promptPresence && mode == AURA_REMOVE_BY_DEFAULT && player->IsAlive())
                {
                    state->promptPresence = false;
                    player->AddAura(SPELL_PRESENCE_OF_MIND, player);
                }
                break;
            case SPELL_ICY_VEINS:
                player->RemoveAurasDueToSpell(SPELL_FROZEN_VEINS);
                break;
            case SPELL_ARCANE_POWER:
                player->RemoveAurasDueToSpell(SPELL_ARCANE_AVATAR);
                break;
            case SPELL_HOT_STREAK:
                // Sun King's Blessing: the Hot Streak spent comes back, and Combustion with it
                if (state->sunKingReady && mode == AURA_REMOVE_BY_DEFAULT && player->IsAlive())
                {
                    state->sunKingReady = false;
                    player->AddAura(SPELL_HOT_STREAK, player);
                    if (Aura* combustion = player->AddAura(SPELL_COMBUSTION, player))
                    {
                        combustion->SetMaxDuration(SunKingCombustionMs);
                        combustion->SetDuration(SunKingCombustionMs);
                    }
                }
                break;
            default:
                break;
        }
    }

private:
    // Touch of the Magi: a share of the damage the mage deals to a target it marked
    static void Store(Player* player, Unit* target, uint32 damage)
    {
        if (!target->HasAura(SPELL_TOUCH_OF_THE_MAGI, player->GetGUID()))
            return;
        uint32 const share = player->HasAura(TALENT_TOUCH_AMPLIFIED) ? 35 : 25;
        target->CustomData.GetDefault<TouchState>(TouchKey)->stored[player->GetGUID()] += damage * share / 100;
    }

    // The mark ends: all of it on the target, half on the enemies around
    static void Detonate(Player* player, Unit* target)
    {
        TouchState* touch = target->CustomData.Get<TouchState>(TouchKey);
        if (!touch)
            return;
        auto const itr = touch->stored.find(player->GetGUID());
        if (itr == touch->stored.end())
            return;
        uint32 const amount = itr->second;
        touch->stored.erase(itr);
        if (!amount || !target->IsAlive())
            return;

        for (Unit* enemy : EnemiesNear(player, target, 8.0f))
            DealNamed(player, enemy, SPELL_TOUCH_OF_THE_MAGI, amount / 2);
        DealNamed(player, target, SPELL_TOUCH_OF_THE_MAGI, amount);
    }

    // Chain Bomb: a Living Bomb that went off takes root in the nearest enemy without one
    static void SpreadBomb(Player* player, Unit* from, uint32 bombSpell)
    {
        Unit* next = nullptr;
        for (Unit* enemy : EnemiesNear(player, from, 10.0f))
            if (!enemy->HasAura(bombSpell, player->GetGUID()) &&
                (!next || from->GetExactDist(enemy) < from->GetExactDist(next)))
                next = enemy;
        if (next)
            player->CastSpell(next, bombSpell, true);
    }
};

// --- Every update: charges, Incanter's Flow, Suspended Time, the Flame Master, what lands later -----------------------

class MageTalentPlayerScript : public PlayerScript
{
public:
    MageTalentPlayerScript() : PlayerScript("MageTalentPlayerScript", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_REGENERATE_POWER
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (player->getClass() != CLASS_MAGE)
            return;
        MageState* state = GetState(player);

        UpdateCharges(player, state->blink, TALENT_BLINK_CHARGES, SPELL_BLINK_CHARGES, diff);
        UpdateCharges(player, state->fireBlast, TALENT_FIRE_BLAST_CHARGES, SPELL_FIRE_BLAST_CHARGES, diff);
        UpdateFlow(player, state, diff);
        UpdateSuspendedTime(player, state, diff);
        UpdateFlameMaster(player, state, diff);
        UpdatePending(player, state, diff);

        if (state->dismissElemental)
        {
            state->dismissElemental = false;
            if (Pet* pet = player->GetPet())
                if (pet->GetEntry() == NPC_WATER_ELEMENTAL || pet->GetEntry() == NPC_WATER_ELEMENTAL_ETERNAL)
                    player->RemovePet(pet, PET_SAVE_NOT_IN_SLOT);
        }
    }

    // Esprit éclairé: below 70% mana, a quarter more of it back
    void OnPlayerBeforeRegeneratePower(Player* player, Powers power, float& amount) override
    {
        if (power == POWER_MANA && player->getClass() == CLASS_MAGE && player->HasAura(TALENT_ENLIGHTENED) &&
            player->GetPowerPct(POWER_MANA) <= 70.0f)
            amount *= 1.25f;
    }

private:
    // Incanter's Flow: in combat, one stack more each second up to five, then one less down to one, and round again
    static void UpdateFlow(Player* player, MageState* state, uint32 diff)
    {
        if (!player->HasAura(TALENT_INCANTERS_FLOW) || !player->IsInCombat())
        {
            player->RemoveAurasDueToSpell(SPELL_INCANTERS_FLOW);
            state->flowTimer = 0;
            state->flowRising = true;
            return;
        }

        state->flowTimer += diff;
        if (state->flowTimer < FlowStepMs && player->HasAura(SPELL_INCANTERS_FLOW))
            return;
        state->flowTimer = 0;

        uint8 stacks = Stacks(player, SPELL_INCANTERS_FLOW);
        if (!stacks)
            stacks = 1;
        else if (state->flowRising)
        {
            if (++stacks >= FlowMaxStacks)
                state->flowRising = false;
        }
        else if (--stacks <= 1)
            state->flowRising = true;
        ShowStacks(player, SPELL_INCANTERS_FLOW, stacks);
    }

    // Suspended Time: every 45 s in combat, Presence of Mind's effect for free
    static void UpdateSuspendedTime(Player* player, MageState* state, uint32 diff)
    {
        if (!player->HasAura(TALENT_SUSPENDED_TIME) || !player->IsInCombat())
        {
            state->suspendedTimer = 0;
            return;
        }
        state->suspendedTimer += diff;
        if (state->suspendedTimer < SuspendedTimeMs)
            return;
        state->suspendedTimer = 0;
        if (!player->HasAura(SPELL_PRESENCE_OF_MIND))
            player->AddAura(SPELL_PRESENCE_OF_MIND, player);
    }

    // Master of the Flame: every 2 s, the mage's Ignites spread from each burning enemy to two more around it
    static void UpdateFlameMaster(Player* player, MageState* state, uint32 diff)
    {
        if (!player->HasAura(TALENT_FLAME_MASTER) || !player->IsInCombat())
        {
            state->flameMasterTimer = 0;
            return;
        }
        state->flameMasterTimer += diff;
        if (state->flameMasterTimer < FlameMasterMs)
            return;
        state->flameMasterTimer = 0;

        std::vector<std::pair<Unit*, int32>> burning;
        for (Unit* enemy : EnemiesNear(player, player, 40.0f))
            if (AuraEffect const* ignite = enemy->GetAuraEffect(SPELL_IGNITE, EFFECT_0, player->GetGUID()))
                burning.emplace_back(enemy, ignite->GetAmount());

        for (auto const& [source, amount] : burning)
        {
            uint32 spread = 0;
            for (Unit* enemy : EnemiesNear(player, source, 8.0f))
            {
                if (spread >= 2)
                    break;
                if (enemy->HasAura(SPELL_IGNITE, player->GetGUID()))
                    continue;
                player->CastCustomSpell(enemy, SPELL_IGNITE, &amount, nullptr, nullptr, true);
                ++spread;
            }
        }
    }

    // A meteor, a comet: cast where it was aimed once its time has come, if the mage is still on that map
    static void UpdatePending(Player* player, MageState* state, uint32 diff)
    {
        if (state->pending.empty())
            return;

        std::vector<Pending> due;
        for (auto itr = state->pending.begin(); itr != state->pending.end();)
        {
            itr->dueMs -= int32(diff);
            if (itr->dueMs > 0)
            {
                ++itr;
                continue;
            }
            due.push_back(*itr);
            itr = state->pending.erase(itr);
        }

        for (Pending const& landing : due)
            if (landing.mapId == player->GetMapId() && player->IsAlive())
                player->CastSpell(landing.where.GetPositionX(), landing.where.GetPositionY(),
                    landing.where.GetPositionZ(), landing.spellId, true);
    }
};

// Lonely Winter: the Water Elemental of a mage who chose to fight without one is sent back, on the mage's next update
// (a pet cannot be taken away while it is still being added to the world)
class MageTalentCreatureScript : public AllCreatureScript
{
public:
    MageTalentCreatureScript() : AllCreatureScript("MageTalentCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override
    {
        uint32 const entry = creature->GetEntry();
        if (entry != NPC_WATER_ELEMENTAL && entry != NPC_WATER_ELEMENTAL_ETERNAL)
            return;
        Player* owner = MagePlayer(creature->GetOwner());
        if (owner && owner->HasAura(TALENT_LONELY_WINTER))
            GetState(owner)->dismissElemental = true;
    }
};
}

void AddMageTalentScripts()
{
    new MageTalentSpellScript();
    new MageTalentUnitScript();
    new MageTalentPlayerScript();
    new MageTalentCreatureScript();
}
