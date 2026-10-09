#include "LootFit.h"

#include "Group.h"
#include "ItemTemplate.h"
#include "LFG.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"

#include <algorithm>
#include <set>

// mod-custom-classes (TalentTree.cpp): the talent tree specialization's role, 0 when the class has none
uint8 GetTalentSpecRole(Player* player);

namespace LootFit
{
namespace
{
// GetTalentSpecRole's roles
constexpr uint8 SpecRoleMelee = 1;
constexpr uint8 SpecRoleCaster = 2;
constexpr uint8 SpecRoleHealer = 3;
constexpr uint8 SpecRoleTank = 4;

// What an item gives, added up from its stats and its spells. A proc's or a use effect's count half: it is not always
// there.
struct Gives
{
    float strength = 0.0f;
    float agility = 0.0f;
    float intellect = 0.0f;
    float spirit = 0.0f;
    float stamina = 0.0f;
    float attackPower = 0.0f;       // attack power, armour penetration, expertise, weapon haste
    float spellPower = 0.0f;        // spell power, spell penetration, casting speed
    float healing = 0.0f;           // healing done, mana back
    float defence = 0.0f;           // defense, dodge, parry, block, block value, health, armour, absorbs, damage taken
    float shared = 0.0f;            // hit, crit, haste rating: every role's

    bool Physical() const { return strength > 0.0f || agility > 0.0f || attackPower > 0.0f; }
    bool Caster() const { return intellect > 0.0f || spellPower > 0.0f || healing > 0.0f; }
    bool Tanking() const { return defence > 0.0f; }
};

bool GroupTank(Player* player)
{
    Group* group = player->GetGroup();
    if (!group)
        return false;
    for (Group::MemberSlot const& slot : group->GetMemberSlots())
        if (slot.guid == player->GetGUID())
            return slot.roles & lfg::PLAYER_ROLE_TANK;
    return false;
}

// A fighter class's primary stat, as the class it is built on (mod-custom-classes); the Oathblade fights as a warrior
bool AgilityClass(Player* player)
{
    uint8 const classId = player->getClass();
    if (classId == 10)
        return false;
    switch (sObjectMgr->GetClassFormulaTemplate(classId))
    {
        case CLASS_ROGUE:
        case CLASS_HUNTER:
        case CLASS_DRUID:
        case CLASS_SHAMAN:
            return true;
        default:
            return false;
    }
}

void AddStat(Gives& gives, uint32 type, float value)
{
    switch (type)
    {
        case ITEM_MOD_STRENGTH: gives.strength += value; break;
        case ITEM_MOD_AGILITY: gives.agility += value; break;
        case ITEM_MOD_INTELLECT: gives.intellect += value; break;
        case ITEM_MOD_SPIRIT: gives.spirit += value; break;
        case ITEM_MOD_STAMINA: gives.stamina += value; break;
        case ITEM_MOD_HEALTH: gives.defence += value / 10.0f; break;
        case ITEM_MOD_MANA: gives.healing += value / 10.0f; break;
        case ITEM_MOD_ATTACK_POWER:
        case ITEM_MOD_RANGED_ATTACK_POWER: gives.attackPower += value / 2.0f; break;
        case ITEM_MOD_EXPERTISE_RATING:
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
        case ITEM_MOD_HIT_MELEE_RATING:
        case ITEM_MOD_CRIT_MELEE_RATING:
        case ITEM_MOD_HASTE_MELEE_RATING:
        case ITEM_MOD_HIT_RANGED_RATING:
        case ITEM_MOD_CRIT_RANGED_RATING:
        case ITEM_MOD_HASTE_RANGED_RATING: gives.attackPower += value; break;
        case ITEM_MOD_SPELL_POWER:
        case ITEM_MOD_SPELL_DAMAGE_DONE:
        case ITEM_MOD_SPELL_PENETRATION:
        case ITEM_MOD_HIT_SPELL_RATING:
        case ITEM_MOD_CRIT_SPELL_RATING:
        case ITEM_MOD_HASTE_SPELL_RATING: gives.spellPower += value; break;
        case ITEM_MOD_SPELL_HEALING_DONE: gives.healing += value; break;
        case ITEM_MOD_MANA_REGENERATION: gives.healing += value * 2.0f; break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
        case ITEM_MOD_DODGE_RATING:
        case ITEM_MOD_PARRY_RATING:
        case ITEM_MOD_BLOCK_RATING:
        case ITEM_MOD_BLOCK_VALUE: gives.defence += value; break;
        case ITEM_MOD_HIT_RATING:
        case ITEM_MOD_CRIT_RATING:
        case ITEM_MOD_HASTE_RATING: gives.shared += value; break;
        default: break;
    }
}

// Who sets a proc off: spells only (a caster's), weapon blows only (a fighter's), being struck (a tank's), or either
enum class Lean : uint8
{
    None,
    Caster,
    Physical,
    Tank,
};

Lean LeanOf(SpellInfo const* spell)
{
    uint32 flags = spell->ProcFlags;
    if (SpellProcEntry const* entry = sSpellMgr->GetSpellProcEntry(spell->Id))
        if (entry->ProcFlags)
            flags = entry->ProcFlags;
    constexpr uint32 physical = PROC_FLAG_DONE_MELEE_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_MELEE_DMG_CLASS |
        PROC_FLAG_DONE_RANGED_AUTO_ATTACK | PROC_FLAG_DONE_SPELL_RANGED_DMG_CLASS;
    constexpr uint32 magic = PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG |
        PROC_FLAG_DONE_PERIODIC;
    constexpr uint32 struck = PROC_FLAG_TAKEN_MELEE_AUTO_ATTACK | PROC_FLAG_TAKEN_SPELL_MELEE_DMG_CLASS |
        PROC_FLAG_TAKEN_RANGED_AUTO_ATTACK | PROC_FLAG_TAKEN_SPELL_RANGED_DMG_CLASS | PROC_FLAG_TAKEN_DAMAGE;
    if ((flags & struck) && !(flags & (physical | magic)))
        return Lean::Tank;
    if ((flags & magic) && !(flags & physical))
        return Lean::Caster;
    if ((flags & physical) && !(flags & magic))
        return Lean::Physical;
    return Lean::None;
}

// What a spell gives while it lasts (an aura's amount), following what it triggers, scaled by `share`. A shared
// rating a proc gives goes to the side that sets the proc off (a stone hastening its wearer's spells is a caster's).
void AddSpell(Gives& gives, uint32 spellId, float share, std::set<uint32>& seen, Lean lean = Lean::None)
{
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
    if (!spell || !seen.insert(spellId).second || seen.size() > 8)
        return;
    auto addShared = [&gives, lean](float amount)
    {
        if (lean == Lean::Caster)
            gives.spellPower += amount;
        else if (lean == Lean::Physical)
            gives.attackPower += amount;
        else
            gives.shared += amount;
    };
    for (SpellEffectInfo const& effect : spell->GetEffects())
    {
        float const amount = std::max(1.0f, std::abs(float(effect.CalcValue()))) * share;
        if (effect.Effect == SPELL_EFFECT_ENERGIZE && effect.MiscValue == POWER_MANA)
            gives.healing += amount / 10.0f;
        if (effect.Effect == SPELL_EFFECT_TRIGGER_SPELL && effect.TriggerSpell)
            AddSpell(gives, effect.TriggerSpell, share, seen, lean);
        if (!effect.IsAura())
            continue;
        switch (effect.ApplyAuraName)
        {
            case SPELL_AURA_PROC_TRIGGER_SPELL:
            case SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE:
            {
                if (!effect.TriggerSpell)
                    break;
                Lean const procLean = lean != Lean::None ? lean : LeanOf(spell);
                if (procLean != Lean::Tank)
                {
                    AddSpell(gives, effect.TriggerSpell, share, seen, procLean);
                    break;
                }
                // Set off by being struck: whatever it gives is a tank's
                Gives struck;
                AddSpell(struck, effect.TriggerSpell, share, seen);
                gives.defence += struck.strength + struck.agility + struck.intellect + struck.spirit +
                    struck.stamina + struck.attackPower + struck.spellPower + struck.healing + struck.defence +
                    struck.shared;
                break;
            }
            case SPELL_AURA_MOD_DAMAGE_DONE:
                // A school besides the physical one: spell power
                if (effect.MiscValue & SPELL_SCHOOL_MASK_MAGIC)
                    gives.spellPower += amount;
                break;
            case SPELL_AURA_MOD_HEALING_DONE:
            case SPELL_AURA_MOD_HEALING_PCT:
                gives.healing += amount;
                break;
            case SPELL_AURA_MOD_HEALING_DONE_PERCENT:
                gives.healing += amount * 10.0f;
                break;
            case SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK:
                gives.spellPower += amount * 10.0f;
                break;
            case SPELL_AURA_MOD_POWER_REGEN:
            case SPELL_AURA_MOD_POWER_REGEN_PERCENT:
                if (effect.MiscValue == POWER_MANA)
                    gives.healing += amount;
                break;
            case SPELL_AURA_MOD_ATTACK_POWER:
            case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                gives.attackPower += amount / 2.0f;
                break;
            case SPELL_AURA_MOD_MELEE_HASTE:
            case SPELL_AURA_MOD_MELEE_RANGED_HASTE:
                gives.attackPower += amount * 10.0f;
                break;
            case SPELL_AURA_MOD_STAT:
                switch (effect.MiscValue)
                {
                    case STAT_STRENGTH: gives.strength += amount; break;
                    case STAT_AGILITY: gives.agility += amount; break;
                    case STAT_STAMINA: gives.stamina += amount; break;
                    case STAT_INTELLECT: gives.intellect += amount; break;
                    case STAT_SPIRIT: gives.spirit += amount; break;
                    default: break;
                }
                break;
            case SPELL_AURA_MOD_RATING:
            {
                uint32 const ratings = uint32(effect.MiscValue);
                if (ratings & ((1u << CR_DEFENSE_SKILL) | (1u << CR_DODGE) | (1u << CR_PARRY) | (1u << CR_BLOCK)))
                    gives.defence += amount;
                else if (ratings & ((1u << CR_EXPERTISE) | (1u << CR_ARMOR_PENETRATION)))
                    gives.attackPower += amount;
                else
                    addShared(amount);
                break;
            }
            case SPELL_AURA_MOD_RESISTANCE:
            case SPELL_AURA_MOD_BASE_RESISTANCE:
                // Armour or resistances: a tank's
                gives.defence += amount / 10.0f;
                break;
            case SPELL_AURA_MOD_INCREASE_HEALTH:
            case SPELL_AURA_MOD_INCREASE_HEALTH_2:
                gives.defence += amount / 10.0f;
                break;
            case SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT:
                gives.defence += amount * 10.0f;
                break;
            case SPELL_AURA_SCHOOL_ABSORB:
            case SPELL_AURA_MOD_SHIELD_BLOCKVALUE:
                gives.defence += amount / 10.0f;
                break;
            case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
                if (effect.CalcValue() < 0)
                    gives.defence += amount * 10.0f;
                break;
            default:
                break;
        }
    }
}

Gives GivesOf(ItemTemplate const& item);
}

// What the item was read as giving, for .lootfit
std::string Describe(ItemTemplate const& item)
{
    Gives const g = GivesOf(item);
    std::string spells;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_SPELLS; ++index)
        if (SpellInfo const* spell = item.Spells[index].SpellId > 0 ?
            sSpellMgr->GetSpellInfo(uint32(item.Spells[index].SpellId)) : nullptr)
        {
            spells += Acore::StringFormat(" [{} procflags {:#x}:", spell->Id, spell->ProcFlags);
            for (SpellEffectInfo const& effect : spell->GetEffects())
                if (effect.Effect)
                    spells += Acore::StringFormat(" e{} a{} m{} v{} t{}", effect.Effect, uint32(effect.ApplyAuraName),
                        effect.MiscValue, effect.CalcValue(), effect.TriggerSpell);
            spells += "]";
        }
    return Acore::StringFormat("str {:.0f} agi {:.0f} int {:.0f} spi {:.0f} sta {:.0f} ap {:.0f} sp {:.0f} heal {:.0f} "
        "def {:.0f} shared {:.0f}{}", g.strength, g.agility, g.intellect, g.spirit, g.stamina, g.attackPower,
        g.spellPower, g.healing, g.defence, g.shared, spells);
}

namespace
{
Gives GivesOf(ItemTemplate const& item)
{
    Gives gives;
    for (uint32 index = 0; index < item.StatsCount && index < MAX_ITEM_PROTO_STATS; ++index)
        AddStat(gives, item.ItemStat[index].ItemStatType, float(item.ItemStat[index].ItemStatValue));
    for (uint32 index = 0; index < MAX_ITEM_PROTO_SPELLS; ++index)
    {
        _Spell const& spell = item.Spells[index];
        if (spell.SpellId <= 0)
            continue;
        // Always there while worn, or now and then (a use, a proc)
        float const share = spell.SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP ? 1.0f : 0.5f;
        std::set<uint32> seen;
        AddSpell(gives, uint32(spell.SpellId), share, seen);
    }
    return gives;
}
}

Role RoleOf(Player* player)
{
    if (uint8 const spec = GetTalentSpecRole(player))
    {
        if (spec == SpecRoleTank)
            return Role::Tank;
        if (spec == SpecRoleHealer)
            return Role::Healer;
        if (spec == SpecRoleCaster)
            return Role::Caster;
        // A fighter's tree: a bear druid's is the cat's, the tank it plays picked in the group
        if (spec == SpecRoleMelee && GroupTank(player))
            return Role::Tank;
        return AgilityClass(player) ? Role::Agility : Role::Strength;
    }
    if (player->HasTankSpec() || GroupTank(player))
        return Role::Tank;
    if (player->HasHealSpec())
        return Role::Healer;
    if (player->HasCasterSpec())
        return Role::Caster;
    switch (sObjectMgr->GetClassFormulaTemplate(player->getClass()))
    {
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return Role::Caster;
        default:
            return AgilityClass(player) ? Role::Agility : Role::Strength;
    }
}

char const* RoleName(Role role)
{
    switch (role)
    {
        case Role::Tank: return "tank";
        case Role::Strength: return "strength fighter";
        case Role::Agility: return "agility fighter";
        case Role::Caster: return "caster";
        case Role::Healer: return "healer";
    }
    return "?";
}

bool Fits(Player* player, ItemTemplate const& item)
{
    return FitsRole(RoleOf(player), AgilityClass(player), item);
}

int32 Score(Player* player, ItemTemplate const& item)
{
    return ScoreRole(RoleOf(player), AgilityClass(player), item);
}

bool RoleByName(std::string_view name, Role& role)
{
    for (Role candidate : { Role::Tank, Role::Strength, Role::Agility, Role::Caster, Role::Healer })
        if (name == RoleName(candidate) || name == std::string_view(RoleName(candidate)).substr(0, name.size()))
        {
            role = candidate;
            return true;
        }
    return false;
}

bool FitsRole(Role role, bool agilityTank, ItemTemplate const& item)
{
    Gives const gives = GivesOf(item);
    // Held in the off hand (orbs, tomes): a caster's or a healer's, whatever its stats
    if (item.InventoryType == INVTYPE_HOLDABLE && role != Role::Caster && role != Role::Healer)
        return false;
    bool const physical = gives.Physical();
    bool const caster = gives.Caster();
    switch (role)
    {
        case Role::Tank:
        {
            // Its defences, or a fighter's stats (its threat); never a caster's alone
            if (caster && !physical && !gives.Tanking())
                return false;
            // The other fighter's primary stat alone (a plate tank's agility trinket, a bear's strength one)
            bool const agility = agilityTank;
            if (!gives.Tanking() && gives.attackPower <= 0.0f && (agility ?
                gives.strength > 0.0f && gives.agility <= 0.0f : gives.agility > 0.0f && gives.strength <= 0.0f))
                return false;
            return true;
        }
        case Role::Strength:
        case Role::Agility:
        {
            if (caster && !physical)
                return false;
            // A tank's defences with nothing of a damage dealer's beside them
            if (gives.Tanking() && !physical && gives.shared <= 0.0f)
                return false;
            // The other fighter's primary stat alone: a warrior's agility trinket, a rogue's strength ring
            if (role == Role::Strength && gives.agility > 0.0f && gives.strength <= 0.0f && gives.attackPower <= 0.0f)
                return false;
            if (role == Role::Agility && gives.strength > 0.0f && gives.agility <= 0.0f && gives.attackPower <= 0.0f)
                return false;
            return true;
        }
        case Role::Caster:
        case Role::Healer:
            if (physical && !caster)
                return false;
            if (gives.Tanking() && !caster)
                return false;
            return true;
    }
    return true;
}

int32 ScoreRole(Role role, bool agilityTank, ItemTemplate const& item)
{
    Gives const g = GivesOf(item);
    float score = 0.0f;
    switch (role)
    {
        case Role::Tank:
        {
            bool const agility = agilityTank;
            // Its defences first: a damage dealer's trinket (its threat) only when nothing better is there
            score = g.stamina * 4.0f + g.defence * 6.0f + (agility ? g.agility * 3.0f + g.strength :
                g.strength * 3.0f + g.agility) + g.attackPower * 0.75f + g.shared * 0.75f -
                (g.spellPower + g.intellect + g.healing + g.spirit) * 4.0f;
            break;
        }
        case Role::Strength:
            score = g.strength * 6.0f + g.agility + g.attackPower * 3.0f + g.shared * 3.0f + g.stamina -
                (g.spellPower + g.intellect + g.healing + g.spirit) * 4.0f - g.defence * 2.0f;
            break;
        case Role::Agility:
            score = g.agility * 6.0f + g.strength * 0.5f + g.attackPower * 3.0f + g.shared * 3.0f + g.stamina -
                (g.spellPower + g.intellect + g.healing + g.spirit) * 4.0f - g.defence * 2.0f;
            break;
        case Role::Caster:
            score = g.spellPower * 4.0f + g.intellect * 5.0f + g.shared * 3.0f + g.spirit * 2.0f + g.healing +
                g.stamina - (g.strength + g.agility + g.attackPower) * 4.0f - g.defence * 2.0f;
            break;
        case Role::Healer:
            score = g.spellPower * 4.0f + g.healing * 4.0f + g.intellect * 5.0f + g.spirit * 4.0f + g.shared * 2.0f +
                g.stamina - (g.strength + g.agility + g.attackPower) * 4.0f - g.defence * 2.0f;
            break;
    }
    return int32(score);
}

uint32 PrimaryStat(Player* player)
{
    switch (RoleOf(player))
    {
        case Role::Caster:
        case Role::Healer:
            return ITEM_MOD_INTELLECT;
        case Role::Agility:
            return ITEM_MOD_AGILITY;
        case Role::Tank:
            return AgilityClass(player) ? ITEM_MOD_AGILITY : ITEM_MOD_STRENGTH;
        case Role::Strength:
        default:
            return ITEM_MOD_STRENGTH;
    }
}
}

// For the other modules (mod-playerbots, mod-legendary), built into the same library
bool LootFitsPlayer(Player* player, ItemTemplate const* item)
{
    return player && item && LootFit::Fits(player, *item);
}

uint8 LootRoleOfPlayer(Player* player)
{
    return uint8(LootFit::RoleOf(player));
}

uint32 LootPrimaryStatOf(Player* player)
{
    return LootFit::PrimaryStat(player);
}
