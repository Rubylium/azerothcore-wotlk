#include "MythicDungeonSystem.h"

#include "EssenceTierSystem.h"
#include "PersonalLootSystem.h"
#include "SmartLootSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "MythicTuning.h"
#include "ObjectMgr.h"
#include "ParagonSystem.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <limits>
#include <set>

// Mythic dungeons (see MythicDungeon.h in the core): every creature of a mythic instance is brought to level 80
// (bosses 82) with WotLK stats a step above a heroic, whatever the dungeon was made for, and the loot changes:
// - Mythique 0: trash gives nothing, each boss gives every real player in the instance one epic of item level 213.
// - Mythic+ (key level 2 and up): creatures grow with the key level (Mythic::GetLevelScaling) and nothing drops
//   until the dungeon is completed; then every real player in it gets one epic of the key's item level and one
//   essence. The key, timer and score are mod-playerbots' (Script/MythicPlus.cpp).
namespace
{
constexpr char const* MythicDataKey = "MythicDungeon";
constexpr char const* HealBudgetKey = "MythicHealBudget";

// Healing one creature gives another. Their heals scale like their spells, and a classic healer's catch up with the
// levels far more than the health they land on: in a big pull, several priests keeping each other topped off made
// the pack unkillable (the Scarlet Cathedral: Chaplains, Abbots, Adepts, Protectors healing whoever misses 400 HP).
// Whatever the number of healers, a trash creature gets back at most this much of its health from them.
constexpr uint32 CreatureHealCapPct = 3;            // a single heal or tick
constexpr uint32 CreatureHealBudgetPct = 10;        // everything, per window
constexpr uint32 CreatureHealWindowMs = 10 * IN_MILLISECONDS;

// What a trash creature has been healed by other creatures in the current window
struct HealBudget : DataMap::Base
{
    uint32 windowStart = 0;
    uint32 received = 0;
};

struct MythicCreatureData : DataMap::Base
{
    uint8 originalLevel = 0;
    uint8 level = 0;
    float spellFactor = 1.0f;
    float levelFactor = 1.0f;       // the part of spellFactor that brings old spell values up to the mythic level
    bool lootGiven = false;
};

// Boss melee: WotLK heroic bosses carry the same damage modifier as their trash, so a boss swung no harder than the
// pack before it (5% of a tank's health a swing at +10, against the 6-10% a boss is meant to hit for)
constexpr float BossMeleeScale = 1.6f;

// Bosses the core does not flag as dungeon bosses (encounters credited by spell, or bosses of a pair), and the ghosts
// of Skarvald and Dalronn, which cannot be targeted but still fight and must grow with the key like the living ones
constexpr std::array<uint32, 6> ExtraMythicBosses = {
    3976,               // Scarlet Commander Mograine (Scarlet Monastery)
    26632, 31360,       // The Prophet Tharon'ja, normal and heroic (Drak'Tharon Keep)
    24200, 31679,       // Skarvald the Constructor (Utgarde Keep)
    24201               // Dalronn the Controller
};
constexpr std::array<uint32, 4> UnselectableCombatants = {
    27389, 31657,       // Dalronn's ghost
    27390, 31680        // Skarvald's ghost
};

template <std::size_t N>
bool IsListed(std::array<uint32, N> const& list, uint32 entry)
{
    return std::find(list.begin(), list.end(), entry) != list.end();
}

// Classic templates carry modifiers made for the small classic base stats. These bring one to what a WotLK heroic
// template of the same role carries (trash: health 5-6, damage 13).
constexpr float ClassicHealthScale = 2.0f;
constexpr float ClassicDamageScale = 6.5f;
// What a WotLK heroic template of the role carries at most: 13 for an elite, about 1 for normal-rank trash (3 left
// for older normal mobs made to hit harder). Older templates above it, tuned for other base stats, are brought
// down to it; bosses keep their own.
constexpr float WotlkEliteDamageModifier = 13.0f;
constexpr float WotlkNormalDamageModifier = 3.0f;
// A boss never has less health than this modifier gives, so a classic boss is a real fight
constexpr float BossHealthModifierFloor = 20.0f;
// Creature spells have fixed values made for their original level: they follow the base health ratio between the
// original and the mythic level (roughly how much players grew in between), capped
// A TBC heroic's spells are already tuned for its heroic mode: they catch up with the levels much less than
// classic ones, whose values are made for a level-40 fight
constexpr float MaxSpellLevelFactor = 10.0f;
constexpr float MaxTbcSpellLevelFactor = 1.25f;
constexpr uint8 MinSpellScalingLevel = 10;

// How long anything killed in a mythic instance stays dead: longer than any run, so a cleared room stays cleared
constexpr uint32 MythicRespawnDelay = 2 * HOUR;

// A player's pet, totem or guardian. A creature's own guardians (VanCleef's allies, the engineers' golems) fight for the
// dungeon and grow with it; they used to be skipped as if a player owned them and stayed at their classic level.
bool IsPlayerControlled(Creature const* creature)
{
    return creature->GetCharmerOrOwnerGUID().IsPlayer();
}

bool IsInMythicMap(WorldObject const* object)
{
    Map const* map = object ? object->FindMap() : nullptr;
    return map && map->IsMythic();
}

int32 GetMythicLevel(WorldObject const* object)
{
    Map const* map = object ? object->FindMap() : nullptr;
    return map ? map->GetMythicLevel() : -1;
}

bool IsMythicBoss(Creature const* creature)
{
    return creature->IsDungeonBoss() || creature->GetCreatureTemplate()->rank == CREATURE_ELITE_WORLDBOSS ||
        IsListed(ExtraMythicBosses, creature->GetEntry());
}

// Triggers, critters and unselectable helpers keep their stats: they are not fights
bool ShouldScale(CreatureTemplate const* cinfo, Creature const* creature)
{
    return IsInMythicMap(creature) && !IsPlayerControlled(creature) &&
        !cinfo->HasFlagsExtra(CREATURE_FLAG_EXTRA_TRIGGER) && cinfo->type != CREATURE_TYPE_CRITTER &&
        (!(cinfo->unit_flags & UNIT_FLAG_NOT_SELECTABLE) || IsListed(UnselectableCombatants, cinfo->Entry));
}

MythicCreatureData const* GetMythicData(Unit const* unit)
{
    Creature const* creature = unit ? unit->ToCreature() : nullptr;
    return creature ? creature->CustomData.Get<MythicCreatureData>(MythicDataKey) : nullptr;
}

bool IsWeaponSpell(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
        spellInfo->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL) ||
        spellInfo->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE) ||
        spellInfo->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG));
}

// Factor applied to the spell damage and healing of a creature of a mythic instance. A scaled creature uses its
// own; one left unscaled (a trigger casting a boss mechanic) takes the plain mythic multiplier. Weapon-based spells
// of scaled creatures already follow their scaled weapon damage.
// A spell whose values the core already raises with the caster's level (the "scales with creature level" attribute,
// or points per level): the core does the level catch-up, and multiplying it by our level factor as well counted it
// twice - the same classic value landed up to five times apart from one spell to the next.
bool IsSelfLevelling(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;
    if (spellInfo->HasAttribute(SPELL_ATTR0_SCALES_WITH_CREATURE_LEVEL))
        return true;
    for (SpellEffectInfo const& effect : spellInfo->Effects)
        if (effect.RealPointsPerLevel > 0.0f)
            return true;
    return false;
}

// periodic: a damage-over-time tick. Those always scale: a weapon spell's hit follows the scaled weapon, but the
// poison or bleed it leaves has fixed values (the Deadmines harpoon's poison ticked for 45).
float GetSpellFactor(Unit const* caster, SpellInfo const* spellInfo, bool periodic = false)
{
    Creature const* creature = caster ? caster->ToCreature() : nullptr;
    if (!creature || IsPlayerControlled(creature) || !IsInMythicMap(creature))
        return 1.0f;

    float factor = Mythic::DamageMultiplier * Mythic::GetLevelScaling(GetMythicLevel(creature));
    if (MythicCreatureData const* data = GetMythicData(creature))
    {
        if (!periodic && IsWeaponSpell(spellInfo))
            return 1.0f;
        factor = data->spellFactor;
        if (IsSelfLevelling(spellInfo) && data->levelFactor > 0.0f)
            factor /= data->levelFactor;
    }
    return factor * MythicTuning::SpellMultiplier(spellInfo);
}

uint32 ScaleValue(uint32 value, float factor)
{
    return static_cast<uint32>(std::min<double>(value * static_cast<double>(factor),
        std::numeric_limits<int32>::max()));
}

// A heal of a share of the target's maximum health
bool IsPercentHeal(SpellInfo const* spellInfo)
{
    return spellInfo && (spellInfo->HasEffect(SPELL_EFFECT_HEAL_PCT) || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH));
}

// A heal-over-time tick: the core hands it to the periodic damage hook as well as the heal hook
bool IsPeriodicHeal(SpellInfo const* spellInfo)
{
    return spellInfo &&
        (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL) || spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)) &&
        !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE) && !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT) &&
        !spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH);
}

// Healing a mythic creature gives another, or itself: capped per heal, and by a budget per window
// (CreatureHealCapPct). Bosses too: Whitemane's Heal and the lieutenants' Renew Steel on Bjarngrim gave back a tenth
// of a boss's health a cast, and 90% a Renew Steel once scaled.
uint32 LimitCreatureHeal(Unit* target, Unit const* healer, uint32 heal)
{
    Creature* patient = target ? target->ToCreature() : nullptr;
    Creature const* source = healer ? healer->ToCreature() : nullptr;
    if (!patient || !source || !heal || IsPlayerControlled(patient) || IsPlayerControlled(source) ||
        !IsInMythicMap(patient))
        return heal;

    uint32 const maxHealth = patient->GetMaxHealth();
    heal = std::min(heal, std::max<uint32>(1, CalculatePct(maxHealth, CreatureHealCapPct)));

    HealBudget* budget = patient->CustomData.GetDefault<HealBudget>(HealBudgetKey);
    uint32 const now = getMSTime();
    if (getMSTimeDiff(budget->windowStart, now) >= CreatureHealWindowMs)
    {
        budget->windowStart = now;
        budget->received = 0;
    }
    uint32 const allowance = CalculatePct(maxHealth, CreatureHealBudgetPct);
    heal = budget->received >= allowance ? 0 : std::min(heal, allowance - budget->received);
    budget->received += heal;
    return heal;
}

void ScaleCreature(CreatureTemplate const* cinfo, Creature* creature, MythicCreatureData& data)
{
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(data.level, cinfo->unit_class);
    uint32 const expansion = std::min<uint32>(cinfo->expansion, EXPANSION_WRATH_OF_THE_LICH_KING);
    bool const classic = expansion == EXPANSION_CLASSIC;

    // Health: the level-80 WotLK base, the template's own modifier and the world rate of its rank (read back from
    // what the core just set), then the mythic multiplier
    float const coreBaseHealth = static_cast<float>(std::max<uint32>(1, stats->GenerateHealth(cinfo)));
    float const rankRate = creature->GetCreateHealth() / coreBaseHealth;
    float healthModifier = cinfo->ModHealth * (classic ? ClassicHealthScale : 1.0f);
    if (IsMythicBoss(creature))
        healthModifier = std::max(healthModifier, BossHealthModifierFloor);

    float const levelScaling = Mythic::GetLevelScaling(GetMythicLevel(creature));
    uint32 const health = std::max<uint32>(1, ScaleValue(stats->BaseHealth[EXPANSION_WRATH_OF_THE_LICH_KING],
        healthModifier * rankRate * Mythic::HealthMultiplier * levelScaling));
    creature->SetCreateHealth(health);
    creature->SetMaxHealth(health);
    creature->SetHealth(health);
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, static_cast<float>(health));
    creature->ResetPlayerDamageReq();

    // Weapon damage: the WotLK base; the template's DamageModifier is applied on top by the stat system, so a
    // classic template is raised towards a WotLK one, and none but a boss hits harder than a WotLK one of its rank
    float damageScale = 1.0f;
    if (cinfo->DamageModifier > 0.0f)
    {
        float const modifier = cinfo->DamageModifier * (classic ? ClassicDamageScale : 1.0f);
        float const reference = cinfo->rank == CREATURE_ELITE_NORMAL ? WotlkNormalDamageModifier
                                                                     : WotlkEliteDamageModifier;
        // A boss keeps its own modifier; a classic one is only raised as far as a WotLK elite
        float limit = reference;
        if (IsMythicBoss(creature))
            limit = classic ? WotlkEliteDamageModifier : cinfo->DamageModifier;
        damageScale = std::min(modifier, limit) / cinfo->DamageModifier;
    }

    float const baseDamage = stats->BaseDamage[EXPANSION_WRATH_OF_THE_LICH_KING] * damageScale *
        Mythic::DamageMultiplier * levelScaling * (IsMythicBoss(creature) ? BossMeleeScale : 1.0f) *
        MythicTuning::MeleeMultiplier(creature->GetEntry());
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
    {
        creature->SetBaseWeaponDamage(attackType, MINDAMAGE, baseDamage);
        creature->SetBaseWeaponDamage(attackType, MAXDAMAGE, baseDamage * 1.5f);
    }

    // Spells
    data.spellFactor = Mythic::DamageMultiplier * levelScaling;
    data.levelFactor = 1.0f;
    if (data.originalLevel >= MinSpellScalingLevel)
        if (CreatureBaseStats const* original = sObjectMgr->GetCreatureBaseStats(data.originalLevel, cinfo->unit_class))
        {
            float const ratio = static_cast<float>(stats->BaseHealth[EXPANSION_WRATH_OF_THE_LICH_KING]) /
                std::max<uint32>(1, original->BaseHealth[expansion]);
            data.levelFactor = std::clamp(ratio, 1.0f,
                expansion == EXPANSION_THE_BURNING_CRUSADE ? MaxTbcSpellLevelFactor : MaxSpellLevelFactor);
            data.spellFactor *= data.levelFactor;
        }

    creature->UpdateAllStats();
}

void MailMythicItem(Player* player, ItemTemplate const* itemTemplate)
{
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    MailDraft draft("Mythic loot", "Your bags were full: here is the item a mythic boss gave you.");
    Item* item = Item::CreateItem(itemTemplate->ItemId, 1, player);
    if (item)
    {
        item->SaveToDB(transaction);
        draft.AddItem(item);
    }

    draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()),
        MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
    CharacterDatabase.CommitTransaction(transaction);

    if (item)
        TryRollPersonalLoot(player, item);
    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cffa335eeYour bags are full: {} was sent to your mailbox.|r", itemTemplate->Name1);
}

// One epic of the run's item level, fitted to the player's class, straight into their bags. Above the game's best
// items it is the generated variant of that item level (MythicItemGeneration.cpp) of a best item.
void GiveMythicItem(Player* player, uint32 itemLevel)
{
    ItemTemplate const* itemTemplate = SelectMythicLootItem(player, std::min(itemLevel, Mythic::MaxItemLevel));
    if (itemTemplate && itemLevel > Mythic::MaxItemLevel)
        if (ItemTemplate const* generated = sObjectMgr->GetItemTemplate(
                Mythic::GetGeneratedItemEntry(itemTemplate->ItemId, Mythic::GetGeneratedVariant(itemLevel))))
            itemTemplate = generated;
    if (!itemTemplate)
    {
        ChatHandler(player->GetSession()).SendSysMessage("No mythic item fits your class this time.");
        return;
    }

    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, itemTemplate->ItemId, 1) != EQUIP_ERR_OK)
    {
        MailMythicItem(player, itemTemplate);
        return;
    }

    if (Item* item = player->StoreNewItem(destination, itemTemplate->ItemId, true,
            Item::GenerateItemRandomPropertyId(itemTemplate->ItemId)))
    {
        player->SendNewItem(item, 1, true, false, true);
        TryRollPersonalLoot(player, item);
    }
}

// The dungeon's own drops: its equipment never drops (the boss gives mythic items instead), trash keeps only
// quest items, Mythique 0 bosses keep the rest (emblems, recipes; essences are added afterwards). In Mythic+
// every creature is trash: the loot comes at the end.
bool IsStrippedMythicLoot(LootItem const& lootItem, bool boss)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(lootItem.itemid);
    if (!itemTemplate || itemTemplate->Class == ITEM_CLASS_WEAPON || itemTemplate->Class == ITEM_CLASS_ARMOR)
        return true;
    return !boss && itemTemplate->Class != ITEM_CLASS_QUEST && itemTemplate->StartQuest == 0;
}

class MythicDungeonCreatureScript : public AllCreatureScript
{
public:
    MythicDungeonCreatureScript() : AllCreatureScript("MythicDungeonCreatureScript") { }

    void OnBeforeCreatureSelectLevel(CreatureTemplate const* cinfo, Creature* creature, uint8& level) override
    {
        if (!ShouldScale(cinfo, creature))
        {
            creature->CustomData.Erase(MythicDataKey);
            return;
        }

        MythicCreatureData* data = creature->CustomData.GetDefault<MythicCreatureData>(MythicDataKey);
        data->originalLevel = level;
        data->level = std::max(level, IsMythicBoss(creature) ? Mythic::BossLevel : Mythic::CreatureLevel);
        level = data->level;
    }

    void OnCreatureSelectLevel(CreatureTemplate const* cinfo, Creature* creature) override
    {
        MythicCreatureData* data = creature->CustomData.Get<MythicCreatureData>(MythicDataKey);
        if (!data)
            return;

        ScaleCreature(cinfo, creature, *data);

        // Nothing a mythic group kills comes back while the run lasts. Some rooms repopulate fast enough to be a
        // treadmill rather than a fight -- the Halls of Lightning forge puts its slags back every 20 seconds --
        // and a key is timed, so clearing the way has to stay cleared. The instance is new for the next run, so
        // this never outlives it.
        if (creature->GetRespawnDelay() < MythicRespawnDelay)
            creature->SetRespawnDelay(MythicRespawnDelay);
    }
};

class MythicDungeonUnitScript : public UnitScript
{
public:
    MythicDungeonUnitScript() : UnitScript("MythicDungeonUnitScript", true, {
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_UNIT_DEATH
    }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (damage > 0)
            damage = static_cast<int32>(ScaleValue(static_cast<uint32>(damage), GetSpellFactor(attacker, spellInfo)));
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        // A heal tick comes through here and through ModifyHealReceived: scaled in both, a creature's Renew
        // healed for the square of its factor. The heal hook scales it.
        if (IsPeriodicHeal(spellInfo))
            return;
        damage = ScaleValue(damage, GetSpellFactor(attacker, spellInfo, true));
    }

    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        // A heal of a share of the target's health is already the size of that health, which the key has grown
        if (!IsPercentHeal(spellInfo))
            heal = ScaleValue(heal, GetSpellFactor(healer, spellInfo));
        heal = LimitCreatureHeal(target, healer, heal);
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        Creature* creature = unit->ToCreature();
        if (!creature || IsPlayerControlled(creature) || GetMythicLevel(creature) != 0 || !IsMythicBoss(creature))
            return;

        MythicCreatureData* data = creature->CustomData.GetDefault<MythicCreatureData>(MythicDataKey);
        if (data->lootGiven)
            return;
        data->lootGiven = true;

        uint32 const itemLevel = Mythic::BaseItemLevel;
        creature->GetMap()->DoForAllPlayers([itemLevel](Player* player)
        {
            if (!player->GetSession()->IsBot())
                GiveMythicItem(player, itemLevel);
        });
    }
};

// Mythic+ rewards, once the last boss of the dungeon is down: an epic of the key's item level and the dungeon's
// essences for every real player in the instance. Nothing drops during a key, so this pays for the whole run --
// a clear is worth what the same dungeon would have dropped on heroic, more as the key rises, and higher keys
// roll each essence's tier more times on top of that.
class MythicDungeonGlobalScript : public GlobalScript
{
public:
    MythicDungeonGlobalScript() : GlobalScript("MythicDungeonGlobalScript", {
        GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE
    }) { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType /*type*/, uint32 /*creditEntry*/, Unit* /*source*/,
        Difficulty /*difficulty*/, std::list<DungeonEncounter const*> const* /*encounters*/, uint32 dungeonCompleted,
        bool /*updated*/) override
    {
        int32 const level = map->GetMythicLevel();
        if (!dungeonCompleted || level <= 0 || !rewardedInstances.insert(map->GetInstanceId()).second)
            return;

        uint32 const itemLevel = Mythic::GetItemLevel(level);
        uint32 const essenceRolls = 1 + static_cast<uint32>(level) / EssenceRollLevels;
        uint32 const essences = GetMythicEssenceReward(static_cast<uint32>(level));
        map->DoForAllPlayers([itemLevel, essenceRolls, essences](Player* player)
        {
            if (player->GetSession()->IsBot())
                return;

            GiveMythicItem(player, itemLevel);
            GrantEssenceRewards(player, essences, essenceRolls);
            // Always, for finishing it: a key is worth a known amount rather than a roll of the dice.
            AwardParagonPoints(player, 1, "Mythique+");
        });
    }

private:
    // One more essence tier roll every that many key levels
    static constexpr uint32 EssenceRollLevels = 5;
    std::set<uint32> rewardedInstances;
};

class MythicDungeonMiscScript : public MiscScript
{
public:
    MythicDungeonMiscScript() : MiscScript("MythicDungeonMiscScript", {
        MISCHOOK_ON_AFTER_LOOT_TEMPLATE_PROCESS
    }) { }

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& store,
        Player* lootOwner, bool /*personal*/, bool /*noEmptyError*/, uint16 /*lootMode*/) override
    {
        if (&store != &LootTemplates_Creature || !lootOwner || !IsInMythicMap(lootOwner))
            return;

        Creature* creature = lootOwner->GetMap()->GetCreature(loot->sourceWorldObjectGUID);
        if (!creature || IsPlayerControlled(creature))
            return;

        bool const boss = IsMythicBoss(creature) && GetMythicLevel(creature) == 0;
        std::erase_if(loot->items, [boss](LootItem const& lootItem) { return IsStrippedMythicLoot(lootItem, boss); });

        // Same count Loot::AddItem keeps for what is left; the conditional and free-for-all ones are counted
        // when the loot is filled for each player, after this hook
        loot->unlootedCount = 0;
        for (uint8 index = 0; index < loot->items.size(); ++index)
        {
            LootItem& lootItem = loot->items[index];
            lootItem.itemIndex = index;
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(lootItem.itemid);
            if (lootItem.conditions.empty() && !itemTemplate->HasFlag(ITEM_FLAG_MULTI_DROP))
                ++loot->unlootedCount;
        }
    }
};
}

bool IsMythicCreature(Creature const* creature)
{
    return creature && !IsPlayerControlled(creature) && IsInMythicMap(creature);
}

// -----------------------------------------------------------------------------------------------------------
// Inébranlable - the Mythic+ tank's footing
// -----------------------------------------------------------------------------------------------------------
//
// A stun or a fear on the tank in a timed run is not a test of anyone's skill; it is the pull scattering for
// reasons nobody could have played around, on a clock. The tank of a mythic group is immune to losing control
// of itself for as long as it is in there.
//
// The immunities are applied from here rather than written into the spell. The aura that carries a whole
// mask of mechanics - the one the PvP trinket uses - resolves its mask from a hardcoded list of spell ids in
// SpellInfo.cpp, so a custom spell cannot join it without editing the core. 90400 is therefore only the buff
// the player sees, and the mechanics below are the substance.
//
// Movement is deliberately left alone: a root or a snare does not take control away, it just holds you still,
// and a tank that cannot be slowed at all is a different decision from the one that was asked for. A knockback does
// take it away, and throws the pack's position with it, so it is refused too.
constexpr uint32 SPELL_MYTHIC_TANK_RESOLVE = 90600;

constexpr std::array<Mechanics, 13> MythicTankImmunities = {
    MECHANIC_CHARM, MECHANIC_DISORIENTED, MECHANIC_FEAR, MECHANIC_SLEEP, MECHANIC_STUN,
    MECHANIC_FREEZE, MECHANIC_KNOCKOUT, MECHANIC_POLYMORPH, MECHANIC_BANISH, MECHANIC_SHACKLE,
    MECHANIC_TURN, MECHANIC_HORROR, MECHANIC_SAPPED
};
// Many creature spells stun or fear through the aura alone, with no mechanic set on the spell: the mechanic list
// never saw them, and they went through. The aura types themselves are refused as well.
constexpr std::array<AuraType, 6> MythicTankAuraImmunities = {
    SPELL_AURA_MOD_STUN, SPELL_AURA_MOD_FEAR, SPELL_AURA_MOD_CONFUSE, SPELL_AURA_MOD_CHARM, SPELL_AURA_MOD_POSSESS,
    SPELL_AURA_AOE_CHARM
};
constexpr std::array<SpellEffects, 2> MythicTankEffectImmunities = {
    SPELL_EFFECT_KNOCK_BACK, SPELL_EFFECT_KNOCK_BACK_DEST
};

// Threat in a dungeon or a raid. A board-boosted damage dealer outdamages a tank several times over, and the tank's
// stance was built for WotLK numbers: the tank's presence doubles its threat, and everyone else in a group with a
// tank makes less of it. The paragon board's tank nodes add to the presence.
constexpr uint32 SPELL_TANK_PRESENCE = 90664;
constexpr uint32 SPELL_GROUP_DISCRETION = 90665;
constexpr int32 TankThreatPct = 100;
constexpr int32 DiscretionThreatPct = -40;

constexpr uint32 RoleUpdateMs = 2000;
constexpr uint8 ClassPestifere = 12;
constexpr uint32 SPELL_PESTIFERE_CARAPACE_NECROSEE = 90211;
constexpr uint32 SPELL_DEFENSIVE_STANCE = 71;
constexpr uint32 SPELL_RIGHTEOUS_FURY = 25780;
constexpr uint32 SPELL_FROST_PRESENCE = 48263;

// Per character: when the roles are next looked at, and whether the immunities are on (they are not an aura, so
// dying does not take them off, and the aura being gone says nothing about them)
struct MythicRoleState : public DataMap::Base
{
    uint32 nextUpdate = 0;
    bool resolve = false;
};

// Tanking right now: a tank stance, form or presence, or the Pestiféré's Carapace nécrosée
bool IsInTankStance(Player* player)
{
    switch (player->getClass())
    {
        case CLASS_WARRIOR: return player->HasAura(SPELL_DEFENSIVE_STANCE);
        case CLASS_PALADIN: return player->HasAura(SPELL_RIGHTEOUS_FURY);
        case CLASS_DEATH_KNIGHT: return player->HasAura(SPELL_FROST_PRESENCE);
        case CLASS_DRUID:
            return player->GetShapeshiftForm() == FORM_BEAR || player->GetShapeshiftForm() == FORM_DIREBEAR;
        case ClassPestifere: return player->HasAura(SPELL_PESTIFERE_CARAPACE_NECROSEE, player->GetGUID());
        default: return false;
    }
}

bool IsGroupTank(Player* player)
{
    if (uint8 const roles = sLFGMgr->GetRoles(player->GetGUID()))
        if (roles & lfg::PLAYER_ROLE_TANK)
            return true;

    // A group put together by hand never ran a role check through the finder, but the group still carries a
    // role and a main-tank flag per member, and either one is a clear enough statement of who is tanking.
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID() &&
                ((member.roles & lfg::PLAYER_ROLE_TANK) || (member.flags & MEMBER_FLAG_MAINTANK)))
                return true;

    // Nobody set a role at all, which is most groups made by hand: whoever is in a tank stance is the tank
    return IsInTankStance(player);
}

bool GroupHasTank(Player* player)
{
    Group* group = player->GetGroup();
    if (!group)
        return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        if (Player* member = ref->GetSource(); member && member != player && member->IsInMap(player) &&
            IsGroupTank(member))
            return true;
    return false;
}

void SetMythicTankResolve(Player* player, bool apply)
{
    uint64 mask = 0;
    for (Mechanics mechanic : MythicTankImmunities)
    {
        player->ApplySpellImmune(SPELL_MYTHIC_TANK_RESOLVE, IMMUNITY_MECHANIC, mechanic, apply);
        mask |= 1ULL << mechanic;
    }
    for (AuraType aura : MythicTankAuraImmunities)
        player->ApplySpellImmune(SPELL_MYTHIC_TANK_RESOLVE, IMMUNITY_STATE, aura, apply);
    for (SpellEffects effect : MythicTankEffectImmunities)
        player->ApplySpellImmune(SPELL_MYTHIC_TANK_RESOLVE, IMMUNITY_EFFECT, effect, apply);

    if (apply)
    {
        // Whatever already had hold of it lets go the moment the buff lands, or the tank would stand there
        // immune and still stunned until the old aura ran out.
        player->RemoveAurasWithMechanic(mask);
        for (AuraType aura : MythicTankAuraImmunities)
            player->RemoveAurasByType(aura);
    }
    else
        player->RemoveAurasDueToSpell(SPELL_MYTHIC_TANK_RESOLVE);
}

// A threat aura at `amount` percent, or none at 0
void SetThreatAura(Player* player, uint32 spellId, int32 amount)
{
    Aura* aura = player->GetAura(spellId);
    if (!amount)
    {
        if (aura)
            player->RemoveAurasDueToSpell(spellId);
        return;
    }
    if (!aura)
        aura = player->AddAura(spellId, player);
    if (AuraEffect* effect = aura ? aura->GetEffect(EFFECT_0) : nullptr; effect && effect->GetAmount() != amount)
        effect->ChangeAmount(amount);
}

void UpdateMythicRoles(Player* player, bool now)
{
    MythicRoleState* state = player->CustomData.GetDefault<MythicRoleState>("MythicRoles");
    uint32 const msNow = getMSTime();
    if (!now && msNow < state->nextUpdate)
        return;
    state->nextUpdate = msNow + RoleUpdateMs;

    Map* map = player->FindMap();
    bool const instance = map && map->IsDungeon();
    bool const tank = instance && IsGroupTank(player);

    bool const resolve = tank && map->IsMythic();
    if (resolve != state->resolve)
    {
        SetMythicTankResolve(player, resolve);
        state->resolve = resolve;
    }
    // The buff is only what the player sees: dying takes it off, the immunities stay, and it comes back here
    if (resolve && player->IsAlive() && !player->HasAura(SPELL_MYTHIC_TANK_RESOLVE))
        player->CastSpell(player, SPELL_MYTHIC_TANK_RESOLVE, true);

    bool const alive = player->IsAlive();
    SetThreatAura(player, SPELL_TANK_PRESENCE,
        tank && alive ? TankThreatPct + static_cast<int32>(GetParagonThreatPct(player)) : 0);
    SetThreatAura(player, SPELL_GROUP_DISCRETION,
        instance && alive && !tank && GroupHasTank(player) ? DiscretionThreatPct : 0);
}

void UpdateMythicTankResolve(Player* player)
{
    if (!player || !player->GetSession())
        return;
    UpdateMythicRoles(player, true);
}

void UpdateMythicTankResolve(Player* player, uint32 /*diff*/)
{
    if (!player || !player->GetSession() || !player->IsInWorld())
        return;
    UpdateMythicRoles(player, false);
}

float GetMythicSpellFactor(Unit const* caster, SpellInfo const* spellInfo)
{
    return GetSpellFactor(caster, spellInfo);
}

bool IsMythicLootless(Creature const* creature)
{
    return IsMythicCreature(creature) && (!IsMythicBoss(creature) || GetMythicLevel(creature) > 0);
}

void AddMythicDungeonScripts()
{
    new MythicDungeonCreatureScript();
    new MythicDungeonUnitScript();
    new MythicDungeonMiscScript();
    new MythicDungeonGlobalScript();
}
