#include "MythicDungeonSystem.h"

#include "EssenceTierSystem.h"
#include "PersonalLootSystem.h"
#include "SmartLootSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "LootMgr.h"
#include "Mail.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "WorldSession.h"
#include <algorithm>
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

struct MythicCreatureData : DataMap::Base
{
    uint8 originalLevel = 0;
    uint8 level = 0;
    float spellFactor = 1.0f;
    bool lootGiven = false;
};

// Classic templates carry modifiers made for the small classic base stats. These bring one to what a WotLK heroic
// template of the same role carries (trash: health 5-6, damage 13).
constexpr float ClassicHealthScale = 2.0f;
constexpr float ClassicDamageScale = 6.5f;
constexpr float WotlkDamageModifier = 13.0f;
// A boss never has less health than this modifier gives, so a classic boss is a real fight
constexpr float BossHealthModifierFloor = 20.0f;
// Creature spells have fixed values made for their original level: they follow the base health ratio between the
// original and the mythic level (roughly how much players grew in between), capped
constexpr float MaxSpellLevelFactor = 10.0f;
constexpr uint8 MinSpellScalingLevel = 10;

bool IsPlayerControlled(Creature const* creature)
{
    return creature->IsPet() || creature->IsTotem() || creature->IsGuardian() ||
        creature->GetCharmerOrOwnerGUID().IsPlayer();
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
    return creature->IsDungeonBoss() || creature->GetCreatureTemplate()->rank == CREATURE_ELITE_WORLDBOSS;
}

// Triggers, critters and unselectable helpers keep their stats: they are not fights
bool ShouldScale(CreatureTemplate const* cinfo, Creature const* creature)
{
    return IsInMythicMap(creature) && !IsPlayerControlled(creature) &&
        !cinfo->HasFlagsExtra(CREATURE_FLAG_EXTRA_TRIGGER) && cinfo->type != CREATURE_TYPE_CRITTER &&
        !(cinfo->unit_flags & UNIT_FLAG_NOT_SELECTABLE);
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
float GetSpellFactor(Unit const* caster, SpellInfo const* spellInfo)
{
    Creature const* creature = caster ? caster->ToCreature() : nullptr;
    if (!creature || IsPlayerControlled(creature) || !IsInMythicMap(creature))
        return 1.0f;

    if (MythicCreatureData const* data = GetMythicData(creature))
        return IsWeaponSpell(spellInfo) ? 1.0f : data->spellFactor;
    return Mythic::DamageMultiplier * Mythic::GetLevelScaling(GetMythicLevel(creature));
}

uint32 ScaleValue(uint32 value, float factor)
{
    return static_cast<uint32>(std::min<double>(value * static_cast<double>(factor),
        std::numeric_limits<int32>::max()));
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
    // classic template is raised towards a WotLK one
    float damageScale = 1.0f;
    if (classic && cinfo->DamageModifier > 0.0f)
        damageScale = std::min(cinfo->DamageModifier * ClassicDamageScale, WotlkDamageModifier) / cinfo->DamageModifier;

    float const baseDamage = stats->BaseDamage[EXPANSION_WRATH_OF_THE_LICH_KING] * damageScale *
        Mythic::DamageMultiplier * levelScaling;
    for (WeaponAttackType attackType : { BASE_ATTACK, OFF_ATTACK, RANGED_ATTACK })
    {
        creature->SetBaseWeaponDamage(attackType, MINDAMAGE, baseDamage);
        creature->SetBaseWeaponDamage(attackType, MAXDAMAGE, baseDamage * 1.5f);
    }

    // Spells
    data.spellFactor = Mythic::DamageMultiplier * levelScaling;
    if (data.originalLevel >= MinSpellScalingLevel)
        if (CreatureBaseStats const* original = sObjectMgr->GetCreatureBaseStats(data.originalLevel, cinfo->unit_class))
        {
            float const ratio = static_cast<float>(stats->BaseHealth[EXPANSION_WRATH_OF_THE_LICH_KING]) /
                std::max<uint32>(1, original->BaseHealth[expansion]);
            data.spellFactor *= std::clamp(ratio, 1.0f, MaxSpellLevelFactor);
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
        if (MythicCreatureData* data = creature->CustomData.Get<MythicCreatureData>(MythicDataKey))
            ScaleCreature(cinfo, creature, *data);
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
        damage = ScaleValue(damage, GetSpellFactor(attacker, spellInfo));
    }

    void ModifyHealReceived(Unit* /*target*/, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        heal = ScaleValue(heal, GetSpellFactor(healer, spellInfo));
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

// Mythic+ rewards, once the last boss of the dungeon is down: an epic of the key's item level and an essence for
// every real player in the instance (higher keys roll the essence tier more times)
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
        map->DoForAllPlayers([itemLevel, essenceRolls](Player* player)
        {
            if (player->GetSession()->IsBot())
                return;

            GiveMythicItem(player, itemLevel);
            ConsumeEssenceReward(player, RollEssenceEntry(essenceRolls));
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
