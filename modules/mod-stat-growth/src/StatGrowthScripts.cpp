#include "mythic/MythicDungeons.h"
#include "MythicTuning.h"
#include "AutoLearnSpellsSystem.h"
#include "AdaptiveTrainingDummy.h"
#include "CombatRogue.h"
#include "DungeonProgressSystem.h"
#include "EssenceFeedback.h"
#include "EssenceTierSystem.h"
#include "ExperienceBoostSystem.h"
#include "FortuneBoostSystem.h"
#include "GladiatorStanceSystem.h"
#include "GroundIndicators.h"
#include "MythicDungeonSystem.h"
#include "MythicItemGeneration.h"
#include "PersonalLootSystem.h"
#include "QuickTravelSystem.h"
#include "ParagonSystem.h"
#include "PrestigeSystem.h"
#include "RidingInstructor.h"
#include "ResourceBoostSystem.h"
#include "SmartLootSystem.h"
#include "TalentResetSystem.h"
#include "StatGrowthSystem.h"
#include "VictoryRushSystem.h"
#include "VitalityBoostSystem.h"

#include "Chat.h"
#include "CombatTelemetry.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "LFGMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StatGrowthConfig.h"
#include "StringFormat.h"

#include <array>

namespace
{
void PlayTieredFeedback(Player* player, EssenceVisual visual, EssenceTier tier, std::string_view powerName)
{
    std::string const announcement = Acore::StringFormat(
        "{} {} AWAKENED", GetEssenceTierName(tier), powerName);
    PlayEssenceFeedback(player, visual, tier, announcement);
}

// Grants the permanent bonus of one essence and plays its feedback. The caller removes the item.
bool ConsumeEssence(Player* player, uint32 itemEntry, bool quiet = false)
{
    EssenceFamily family;
    if (!player || !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) ||
        !TryGetEssenceFamily(itemEntry, family))
        return false;

    EssenceTier const tier = GetEssenceTier(itemEntry);
    std::string_view const tierName = GetEssenceTierName(tier);
    ChatHandler chat(player->GetSession());
    uint32 totalBonus = 0;

    switch (family)
    {
        case EssenceFamily::Growth:
        {
            uint32 const amount = GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::BonusPerUse), itemEntry);
            std::string_view statName;
            if (!GrantRandomStatGrowth(player, amount, statName))
            {
                chat.SendSysMessage("Unable to grant a permanent stat bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Growth, tier, "POWER");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00{} permanently "
                "increased by +{}.|r", tierName, statName, amount);
            return true;
        }
        case EssenceFamily::Experience:
        {
            uint32 const amount = GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ExperienceBonusPerUse), itemEntry);
            if (!GrantExperienceBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent experience bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Experience, tier, "WISDOM");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Experience gained "
                "permanently increased by +{}%. Total: +{}%.|r", tierName, amount, totalBonus);
            return true;
        }
        case EssenceFamily::Resource:
        {
            uint32 const amount = GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ResourceBonusPerUse), itemEntry);
            if (!GrantResourceBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent resource bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Resource, tier, "FLOW");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Primary-resource "
                "regeneration permanently increased by +{}%. Total: +{}%.|r", tierName, amount, totalBonus);
            return true;
        }
        case EssenceFamily::Vitality:
        {
            uint32 const amount = GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::VitalityBonusPerUse), itemEntry);
            if (!GrantVitalityBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent vitality bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Vitality, tier, "VITALITY");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Vitality permanently "
                "increased by +{} ({} maximum health at your level). Total: {} Vitality, +{} maximum health.|r",
                tierName, amount, GetVitalityHealth(player, amount), totalBonus, GetVitalityHealth(player, totalBonus));
            return true;
        }
        case EssenceFamily::Fortune:
        {
            uint32 const amount = GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::FortuneBonusPerUse), itemEntry);
            if (!GrantFortuneBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent fortune bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Fortune, tier, "FORTUNE");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Gold gains and gear bonuses "
                "(chance and strength) permanently increased by +{}%. Total: +{}%.|r", tierName, amount, totalBonus);
            return true;
        }
    }

    return false;
}

// Essences are applied as soon as they are looted so they never take bag space
bool AutoConsumeLootedEssences(Player* player, Item* item, uint32 count)
{
    if (!player || !item || !IsEssenceItem(item->GetEntry()))
        return false;

    uint32 const itemEntry = item->GetEntry();
    uint32 consumed = 0;
    while (consumed < count && ConsumeEssence(player, itemEntry))
        ++consumed;

    if (consumed > 0)
        player->DestroyItemCount(itemEntry, consumed, true);
    return true;
}

// Where the player arrived in the instance it is in: the true start of the run. A map can hold several dungeons
// (Scarlet Monastery's four wings are one map), and Mythic+ runs are not Dungeon Finder dungeons, so neither the
// group's Dungeon Finder entry nor the map's entrance portal can be trusted to name the right wing.
struct DungeonArrival : public DataMap::Base
{
    uint32 instanceId = 0;
    WorldLocation location;
};

void RememberDungeonArrival(Player* player)
{
    Map const* map = player ? player->GetMap() : nullptr;
    if (!map || !map->IsDungeon() || map->IsBattlegroundOrArena())
        return;

    DungeonArrival* arrival = player->CustomData.GetDefault<DungeonArrival>("DungeonArrival");
    // A player already in this instance keeps its first arrival (a reconnect or a second teleport inside the
    // same run is not a new start)
    if (arrival->instanceId == map->GetInstanceId())
        return;

    arrival->instanceId = map->GetInstanceId();
    arrival->location.WorldRelocate(player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
        player->GetPositionZ(), player->GetOrientation());
}

// Releasing the spirit inside a dungeon or raid brings the player back to life at the start of the instance
// instead of sending the ghost to the graveyard outside. Bots keep the default: the dungeon bot manager revives
// them next to their real player once the fight is over.
bool RespawnAtDungeonStart(Player* player)
{
    if (!player || player->IsAlive() || player->GetSession()->IsBot())
        return false;

    Map const* map = player->GetMap();
    if (!map || !map->IsDungeon() || map->IsBattlegroundOrArena())
        return false;

    uint32 const mapId = player->GetMapId();
    WorldLocation start;
    bool found = false;

    // Where the player came in, when it is still this instance
    if (DungeonArrival const* arrival = player->CustomData.Get<DungeonArrival>("DungeonArrival");
        arrival && arrival->instanceId == map->GetInstanceId() && arrival->location.GetMapId() == mapId)
    {
        start = arrival->location;
        found = true;
    }

    // Else Dungeon Finder runs start where the Dungeon Finder teleports the group in
    if (Group* group = player->GetGroup(); !found && group && group->isLFGGroup())
        if (lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(sLFGMgr->GetDungeon(group->GetGUID())))
            if (dungeon->map == mapId && (dungeon->x != 0.0f || dungeon->y != 0.0f || dungeon->z != 0.0f))
            {
                start.WorldRelocate(mapId, dungeon->x, dungeon->y, dungeon->z, dungeon->o);
                found = true;
            }

    // Else the landing point of the instance entrance portal
    if (!found)
        if (AreaTriggerTeleport const* entrance = sObjectMgr->GetMapEntranceTrigger(mapId))
        {
            start.WorldRelocate(mapId, entrance->target_X, entrance->target_Y, entrance->target_Z,
                entrance->target_Orientation);
            found = true;
        }

    if (!found)
        return false;

    player->ResurrectPlayer(1.0f);
    player->SpawnCorpseBones();
    player->TeleportTo(start);
    return true;
}

// Mythic trash gives nothing and mythic bosses hand out their own equipment (MythicDungeonSystem.cpp): only the
// essences of a Mythique 0 boss are added there (Mythic+ gives its essences at the end of the dungeon)
void AddKillLoot(Player* player, Creature* killed)
{
    if (IsMythicLootless(killed))
        return;

    if (!IsMythicCreature(killed))
        ImproveBaseEquipmentLoot(player, killed);
    TryAddStatGrowthLoot(player, killed);
    TryAddExperienceBoostLoot(player, killed);
    TryAddResourceBoostLoot(player, killed);
    TryAddVitalityBoostLoot(player, killed);
    TryAddFortuneBoostLoot(player, killed);
}
}

// A whole dungeon's essences at once, for the end of a Mythic+ run. Each is rolled and applied on its own, so
// the families and tiers land exactly as they would have one by one; only the reporting is pooled, into a single
// line and a single effect keyed to the best tier of the batch.
uint32 GrantEssenceRewards(Player* player, uint32 count, uint32 tierRolls)
{
    if (!player || !count)
        return 0;

    std::array<uint32, 3> granted = {};
    for (uint32 index = 0; index < count; ++index)
    {
        uint32 const itemEntry = RollEssenceEntry(tierRolls);
        if (ConsumeEssence(player, itemEntry, true))
            ++granted[static_cast<uint8>(GetEssenceTier(itemEntry))];
    }

    uint32 const total = granted[0] + granted[1] + granted[2];
    if (!total)
        return 0;

    EssenceTier const best = granted[2] ? EssenceTier::Ascendant
        : (granted[1] ? EssenceTier::Greater : EssenceTier::Faint);
    PlayTieredFeedback(player, EssenceVisual::Growth, best, "POWER");
    ChatHandler(player->GetSession()).PSendSysMessage(
        "|cffa335eeThe dungeon's essences bind to your soul.|r |cff00ff00{} absorbed: {} faint, {} greater, "
        "{} ascendant.|r", total, granted[0], granted[1], granted[2]);
    return total;
}

class StatGrowthWorldScript : public WorldScript
{
public:
    StatGrowthWorldScript() : WorldScript("StatGrowthWorldScript", {
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnBeforeConfigLoad(bool reload) override
    {
        statGrowthConfig.Initialize(reload);
    }

    void OnLoadCustomDatabaseTable() override
    {
        LoadPersonalLootRolls();
        LoadParagonBoard();
    }

    // Every restart gives players a clean Dungeon Finder slate: saved deserter and random dungeon cooldown
    // auras are removed before anyone can log in
    void OnStartup() override
    {
        CombatTelemetry::InitializeDatabase();
        CharacterDatabase.DirectExecute("DELETE FROM character_aura WHERE spell IN ({}, {})",
            uint32(lfg::LFG_SPELL_DUNGEON_DESERTER), uint32(lfg::LFG_SPELL_DUNGEON_COOLDOWN));
        LOG_INFO("server.loading", ">> Cleared saved Dungeon Finder deserter and cooldown auras");
    }
};

class StatGrowthPlayerScript : public PlayerScript
{
public:
    StatGrowthPlayerScript() : PlayerScript("StatGrowthPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_MAP_CHANGED,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_CREATURE_KILLED_BY_PET,
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_BEFORE_REGENERATE_POWER,
        PLAYERHOOK_ON_BEFORE_MODIFY_POWER,
        PLAYERHOOK_ON_AFTER_UPDATE_MAX_HEALTH,
        PLAYERHOOK_ON_MONEY_CHANGED,
        PLAYERHOOK_ON_LOOT_ITEM,
        PLAYERHOOK_ON_GROUP_ROLL_REWARD_ITEM,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_ON_UNEQUIP_ITEM,
        PLAYERHOOK_CAN_REPOP_AT_GRAVEYARD
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled))
            ApplyStoredStatGrowth(player);

        // Read before it is applied, and before anything else touches the character's stats
        LoadParagonForPlayer(player);
        ApplyStoredParagon(player);

        LearnAvailableClassSpells(player);
        OnCombatRogueLogin(player);
        LearnGladiatorStance(player);
        ApplyEquippedPersonalLoot(player);
        BeginPersonalLootAddonHandshake(player);
        SendDungeonProgress(player);
    }

    // Entering or leaving an instance: the client swaps between the dungeon tracker and the quest tracker
    void OnPlayerMapChanged(Player* player) override
    {
        RememberDungeonArrival(player);
        SendDungeonProgress(player);
        UpdateMythicTankResolve(player);
    }

    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        return !RespawnAtDungeonStart(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearQuickTravel(player);
        ClearPersonalLootPlayerState(player);
        ForgetParagonForPlayer(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        UpdateGladiatorStance(player);
        UpdatePersonalLootAddonHandshake(player, diff);
        UpdateParagonBuffs(player);
        UpdateCombatRogue(player, diff);
        UpdateMythicTankResolve(player, diff);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32&, uint32& language, std::string& message) override
    {
        HandlePersonalLootAddonMessage(player, language, message);
        HandleQuickTravelAddonMessage(player, language, message);
        HandleInstanceTravelAddonMessage(player, language, message);
        HandleTalentResetAddonMessage(player, language, message);
        HandleParagonAddonMessage(player, language, message);
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (player->GetLevel() > oldLevel)
        {
            LearnAvailableClassSpells(player);
            LearnGladiatorStance(player);
        }
        else if (player->GetLevel() < oldLevel)
            OnGladiatorLevelChanged(player, oldLevel);

        OnCombatRogueLevelChanged(player, oldLevel);
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        AddKillLoot(killer, killed);
        TryAwardParagonPoint(killer, killed);
        OnParagonKill(killer, killed);
        OnCombatRogueKill(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* petOwner, Creature* killed) override
    {
        AddKillLoot(petOwner, killed);
        TryAwardParagonPoint(petOwner, killed);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit*, uint8) override
    {
        ApplyExperienceBoost(player, amount);
        // At the level cap the core throws this away; the paragon bar takes it instead
        AddParagonExperience(player, amount);
    }

    void OnPlayerBeforeRegeneratePower(Player* player, Powers power, float& amount) override
    {
        ApplyResourceRegenerationBoost(player, power, amount);
    }

    void OnPlayerBeforeModifyPower(Player* player, Powers power, int32& amount) override
    {
        ApplyResourceGenerationBoost(player, power, amount);
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        ApplyVitalityBoost(player, value);
        ApplyParagonHealth(player, value);
    }

    void OnPlayerMoneyChanged(Player* player, int32& amount) override
    {
        ApplyFortuneGoldBoost(player, amount);
    }

    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootGuid) override
    {
        if (!AutoConsumeLootedEssences(player, item, count))
            TryRollPersonalLoot(player, item, lootGuid);
    }

    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, RollVote, Roll* roll) override
    {
        if (!AutoConsumeLootedEssences(player, item, count))
            TryRollPersonalLoot(player, item, roll ? roll->itemGUID : ObjectGuid::Empty, roll ? roll->itemSlot : -1);
    }

    void OnPlayerEquip(Player* player, Item* item, uint8, uint8, bool) override
    {
        ApplyPersonalLootItem(player, item);
    }

    void OnPlayerUnequip(Player* player, Item* item) override
    {
        RemovePersonalLootItem(player, item);
    }

private:
    static void LearnAvailableClassSpells(Player* player)
    {
        if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::AutoLearnClassSpells))
            return;

        uint32 const learnedCount = AutoLearnClassSpells(player);
        if (learnedCount > 0)
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00You automatically learned {} new class {} for free.|r",
                learnedCount, learnedCount == 1 ? "ability" : "abilities");
    }
};

class StatGrowthUnitScript : public UnitScript
{
public:
    StatGrowthUnitScript() : UnitScript("StatGrowthUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_ON_UNIT_DEATH,
        UNITHOOK_ON_UNIT_EXIT_COMBAT
    }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        ApplyPersonalLootLeech(attacker, victim, damage);
        // Both sides of the same hit: what the victim's paragon board does about being hit, and what the
        // attacker's does about landing one.
        OnParagonDamageDealt(attacker, victim, damage);
        OnParagonDamageTaken(victim, attacker, damage);
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        OnDungeonProgressUnitDeath(unit);
    }

    void OnUnitExitCombat(Unit* unit) override
    {
        CombatTelemetry::AbortRaidEncounter(unit);
    }
};

class StatGrowthGlobalScript : public GlobalScript
{
public:
    StatGrowthGlobalScript() : GlobalScript("StatGrowthGlobalScript", {
        GLOBALHOOK_ON_ITEM_DEL_FROM_DB,
        GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE
    }) { }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        DeletePersonalLootRoll(transaction, itemGuid);
    }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType, uint32, Unit* source, Difficulty,
        std::list<DungeonEncounter const*> const*, uint32, bool updated) override
    {
        CombatTelemetry::FinishRaidEncounter(map, source, updated);
    }
};

class EssenceItemScript : public ItemScript
{
public:
    explicit EssenceItemScript(char const* name) : ItemScript(name) { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const&) override
    {
        if (ConsumeEssence(player, item->GetEntry()))
            player->DestroyItemCount(item->GetEntry(), 1, true);
        return true;
    }
};

void AddStatGrowthScripts()
{
    AddAdaptiveTrainingDummyScripts();
    AddCombatRogueScripts();
    AddGladiatorStanceScripts();
    AddVictoryRushScripts();
    AddQuickTravelScripts();
    AddRidingInstructorScripts();
    AddParagonScripts();
    AddPrestigeScripts();
    AddDungeonFinderLockScripts();
    AddMythicDungeonScripts();
    AddGroundIndicatorScripts();
    AddExperienceRateCommand();
    AddOnyxiaReworkScripts();
    AddBronjahmReworkScripts();
    AddDevourerReworkScripts();
    AddMythicTuningScripts();
    AddMythicForgeOfSoulsScripts();
    AddMythicHallsOfLightningScripts();
    AddMythicUtgardeKeepScripts();
    AddMythicDrakTharonKeepScripts();
    AddMythicMechanarScripts();
    AddMythicShatteredHallsScripts();
    AddMythicDeadminesScripts();
    AddMythicScarletCathedralScripts();
    AddMythicItemGenerationScripts();
    new StatGrowthWorldScript();
    new StatGrowthGlobalScript();
    new StatGrowthUnitScript();
    new StatGrowthPlayerScript();
    new EssenceItemScript("item_stat_growth_essence");
    new EssenceItemScript("item_experience_boost_essence");
    new EssenceItemScript("item_resource_boost_essence");
    new EssenceItemScript("item_vitality_boost_essence");
    new EssenceItemScript("item_fortune_boost_essence");
}
