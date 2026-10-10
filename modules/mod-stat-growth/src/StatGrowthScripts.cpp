#include "mythic/MythicDungeons.h"
#include "MythicTuning.h"
#include "AutoLearnSpellsSystem.h"
#include "AdaptiveTrainingDummy.h"
#include "BotCatchUpSystem.h"
#include "BotEssenceSystem.h"
#include "DungeonProgressSystem.h"
#include "EssenceFeedback.h"
#include "EssenceTierSystem.h"
#include "EssenceTuning.h"
#include "ExperienceBoostSystem.h"
#include "FortuneBoostSystem.h"
#include "GladiatorStanceSystem.h"
#include "GroundIndicators.h"
#include "GroundLoot.h"
#include "InfiniteDungeonSystem.h"
#include "MythicAppearance.h"
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
#include "LiveTuning.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StatGrowthConfig.h"
#include "StringFormat.h"

#include <algorithm>
#include <array>
#include <limits>

// RaidFinder.cpp (mod-playerbots, same modules library): whether the player's Défi board group is in the board's wipe
bool IsChallengeWipeOnFor(Player const* player);

namespace
{
void PlayTieredFeedback(Player* player, EssenceVisual visual, EssenceTier tier, std::string_view powerName)
{
    std::string const announcement = Acore::StringFormat(
        "{} {} AWAKENED", GetEssenceTierName(tier), powerName);
    PlayEssenceFeedback(player, visual, tier, announcement);
}

// Out of combat, a maximum health raised by the module (essences and stat growth at login, the paragon board and
// a bot's mirror on entering a key or a raid) keeps the share of health the character had: a full character stays
// full. The core leaves the health where it was, so logging in or zoning in showed a missing part. The share is
// taken before the first rise and put back on the next update, once the new maximum is set. In combat nothing
// changes: a buff raising the maximum does not heal.
struct HealthShare : public DataMap::Base
{
    float share = -1.0f;
};
constexpr char const* HealthShareKey = "StatGrowthHealthShare";

void NoteMaxHealthRise(Player* player, float value)
{
    uint32 const current = player->GetMaxHealth();
    if (!player->IsInWorld() || !player->IsAlive() || player->IsInCombat() || !current ||
        value <= static_cast<float>(current))
        return;
    HealthShare* keep = player->CustomData.GetDefault<HealthShare>(HealthShareKey);
    if (keep->share < 0.0f)
        keep->share = std::min(1.0f, static_cast<float>(player->GetHealth()) / static_cast<float>(current));
}

void KeepHealthShare(Player* player)
{
    HealthShare* keep = player->CustomData.Get<HealthShare>(HealthShareKey);
    if (!keep || keep->share < 0.0f)
        return;
    float const share = keep->share;
    keep->share = -1.0f;
    if (!player->IsAlive() || player->IsInCombat())
        return;
    uint32 const wanted = static_cast<uint32>(std::lround(share * static_cast<float>(player->GetMaxHealth())));
    if (wanted > player->GetHealth())
        player->SetHealth(wanted);
}

// Grants the permanent bonus of one essence and plays its feedback. The caller removes the item.
bool ConsumeEssence(Player* player, uint32 itemEntry, bool quiet = false)
{
    EssenceFamily family;
    if (!player || !statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) ||
        !TryGetEssenceFamily(itemEntry, family))
        return false;

    uint32 const resolvedEntry = ResolveEssenceEntry(player, itemEntry);
    bool const converted = resolvedEntry != itemEntry;
    itemEntry = resolvedEntry;
    if (!TryGetEssenceFamily(itemEntry, family))
        return false;
    EssenceTier const tier = GetEssenceTier(itemEntry);
    std::string_view const tierName = GetEssenceTierName(tier);
    ChatHandler chat(player->GetSession());
    uint32 totalBonus = 0;
    if (converted && !quiet)
        chat.SendSysMessage("That essence is capped; it grants another essence family of the same tier instead.");

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
            // Only the room left under the cap (EssenceTuning::MaxExperienceBonus); an essence at the cap was already
            // turned into another family (ResolveEssenceEntry)
            uint32 const amount = EssenceTuning::GrantAmount(GetEffectiveExperienceBonus(player),
                GetTieredEssenceBonus(statGrowthConfig.GetConfigValue<uint32>(
                    StatGrowthConfigKey::ExperienceBonusPerUse), itemEntry), EssenceTuning::MaxExperienceBonus);
            if (!GrantExperienceBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent experience bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Experience, tier, "WISDOM");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Experience gained "
                "permanently increased by +{}%. Total: +{}% (at most +{}%).|r", tierName, amount,
                GetEffectiveExperienceBonus(player), EssenceTuning::MaxExperienceBonus);
            return true;
        }
        case EssenceFamily::Resource:
        {
            uint32 const amount = EssenceTuning::GrantAmount(GetResourceBonus(player), GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::ResourceBonusPerUse), itemEntry),
                EssenceTuning::MaxResourceBonus);
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
            // What the points are worth after the diminishing returns (EssenceTuning::VitalityCeiling)
            uint32 const before = GetEffectiveVitalityPoints(totalBonus - amount);
            uint32 const after = GetEffectiveVitalityPoints(totalBonus);
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Vitality permanently "
                "increased by +{} (+{} maximum health at your level). Total: {} Vitality, worth {}: +{} maximum "
                "health.|r", tierName, amount, GetVitalityHealth(player, after - before), totalBonus, after,
                GetVitalityHealth(player, after));
            return true;
        }
        case EssenceFamily::Fortune:
        {
            uint32 const amount = EssenceTuning::GrantAmount(GetFortuneBonus(player), GetTieredEssenceBonus(
                statGrowthConfig.GetConfigValue<uint32>(StatGrowthConfigKey::FortuneBonusPerUse), itemEntry),
                EssenceTuning::MaxFortuneBonus);
            if (!GrantFortuneBoost(player, amount, totalBonus))
            {
                chat.SendSysMessage("Unable to grant a permanent fortune bonus.");
                return false;
            }

            if (quiet)
                return true;

            PlayTieredFeedback(player, EssenceVisual::Fortune, tier, "FORTUNE");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Fortune permanently "
                "increased by +{}%. Permanent total: +{}% (earned gold bonus caps at +100%).|r",
                tierName, amount, totalBonus);
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

    // An Infinite Dungeon floor has its own death rules (InfiniteDungeonSystem.cpp): the ghost waits where it fell
    if (InfiniteDungeon::IsInRun(player))
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
    // An Infinite Dungeon floor pays for itself (InfiniteDungeonSystem.cpp)
    if (IsMythicLootless(killed) || InfiniteDungeon::IsFloorCreature(killed))
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
        LoadBotCatchUpConfig();
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
        PLAYERHOOK_ON_MONEY_REWARD,
        PLAYERHOOK_ON_LOOT_ITEM,
        PLAYERHOOK_ON_GROUP_ROLL_REWARD_ITEM,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_ON_UNEQUIP_ITEM,
        PLAYERHOOK_ON_SPELL_CAST,
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
        LearnGladiatorStance(player);
        ApplyEquippedPersonalLoot(player);
        BeginPersonalLootAddonHandshake(player);
        // A bot's catch-up display comes back from the database with its other auras; the bonus itself does not
        ClearBotCatchUp(player);
        SendDungeonProgress(player);
        SendGeneratedItemRecords(player);
    }

    // Entering or leaving an instance: the client swaps between the dungeon tracker and the quest tracker
    void OnPlayerMapChanged(Player* player) override
    {
        RememberDungeonArrival(player);
        SendDungeonProgress(player);
        UpdateMythicTankResolve(player);
        // A bot entering a key or a challenge takes up the board it asks for before the first pull
        RefreshBotParagon(player);
        RefreshBotEssences(player);
    }

    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        // A Défi board wipe: the board raises the whole group itself at its landing spot within seconds (RaidFinder
        // UpdateChallengeWipe). A release neither brings the player back to life at the start nor sends the ghost
        // out: it waits where it fell.
        if (IsChallengeWipeOnFor(player))
            return false;
        return !RespawnAtDungeonStart(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearQuickTravel(player);
        ClearPersonalLootPlayerState(player);
        ForgetParagonForPlayer(player);
        ClearBotCatchUp(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        KeepHealthShare(player);
        UpdateGladiatorStance(player);
        UpdatePersonalLootAddonHandshake(player, diff);
        UpdateParagonBuffs(player);
        UpdateBotParagon(player, diff);
        UpdateBotEssences(player, diff);
        UpdateBotCatchUp(player, diff);
        UpdateMythicTankResolve(player, diff);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32&, uint32& language, std::string& message) override
    {
        HandlePersonalLootAddonMessage(player, language, message);
        HandleEssenceAddonMessage(player, language, message);
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
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        AddKillLoot(killer, killed);
        TryAwardParagonPoint(killer, killed);
        OnParagonKill(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* petOwner, Creature* killed) override
    {
        AddKillLoot(petOwner, killed);
        TryAwardParagonPoint(petOwner, killed);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit*, uint8) override
    {
        ApplyExperienceBoost(player, amount);
        if (InfiniteDungeon::IsInRun(player))
            InfiniteDungeon::OnRunExperience(player, amount);
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
        NoteMaxHealthRise(player, value);
    }

    void OnPlayerMoneyReward(Player* player, uint32& amount) override
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

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        OnParagonSpellCast(player, spell);
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

// mod-custom-classes (TalentTree.cpp): the specialization as an index among the class's spec trees
int8 GetTalentSpecializationIndex(Player* player);
// ParagonSystem.cpp: the paragon points a character fights with
uint32 GetParagonBalancePoints(Player* player);

namespace
{
// Spec balance (the combat bench, Fire mage the reference, .agents/docs/systems/combat-bench.md): the last step
// after a spec's own spells and talents, every hit of the spec - its pets' and totems' too - scaled as one, so its
// abilities keep their shares. Two factors, as specs drift apart with paragon (at +10 the melee dealt twice the mage,
// at the Hollow Voice's 650 points a fraction of it): one at no paragon (balance0, measured at +10 and its gear),
// one at 650 points (balance, measured at the Hollow Voice's 460), in between by the character's points.
// Live: .tune set balance.<class>.<spec> <factor> / balance0.<class>.<spec> (spec: its tree, 1 to 4).
// Assassination and the Oathblade were measured again on 2026-10-08 (Gardien-chef Vorhan's 450 / 600, 460 / 650, +30
// and +10) on a boss dummy that stays up and on a whole kill, not on the Défi I one that dies every few seconds; the
// Barbarian on 2026-10-08 too (450 / 600 and +10), once its bots wore damage gear instead of the bear tank's. Every
// damage spec set again from the combat bench's sweep (2026-10-08, each bot alone in its lane, 12-32 rows a spec at
// 258 / 0 and 460 / 650 pooled over three auto-tune passes, overflow.crit_rate 0.3).
constexpr float SpecBalanceParagon = 650.0f;
struct SpecBalanceRow
{
    LiveTuning::Knob specs[4];
};
SpecBalanceRow const SpecBalance[] = {
    { { { "balance.none.1", 1.0f }, { "balance.none.2", 1.0f },
        { "balance.none.3", 1.0f }, { "balance.none.4", 1.0f } } },
    { { { "balance.warrior.1", 1.12f }, { "balance.warrior.2", 0.96f },
        { "balance.warrior.3", 1.0f }, { "balance.warrior.4", 1.24f } } },
    { { { "balance.paladin.1", 1.0f }, { "balance.paladin.2", 1.0f },
        { "balance.paladin.3", 0.69f }, { "balance.paladin.4", 1.0f } } },
    { { { "balance.hunter.1", 1.06f }, { "balance.hunter.2", 0.74f },
        { "balance.hunter.3", 0.75f }, { "balance.hunter.4", 1.0f } } },
    { { { "balance.rogue.1", 1.48f }, { "balance.rogue.2", 1.28f },
        { "balance.rogue.3", 0.72f }, { "balance.rogue.4", 1.0f } } },
    { { { "balance.priest.1", 1.0f }, { "balance.priest.2", 1.0f },
        { "balance.priest.3", 0.65f }, { "balance.priest.4", 1.0f } } },
    { { { "balance.dk.1", 1.0f }, { "balance.dk.2", 1.41f },
        { "balance.dk.3", 1.27f }, { "balance.dk.4", 1.0f } } },
    { { { "balance.shaman.1", 0.89f }, { "balance.shaman.2", 0.91f },
        { "balance.shaman.3", 1.0f }, { "balance.shaman.4", 1.0f } } },
    { { { "balance.mage.1", 0.83f }, { "balance.mage.2", 1.0f },
        { "balance.mage.3", 1.15f }, { "balance.mage.4", 1.0f } } },
    { { { "balance.warlock.1", 1.19f }, { "balance.warlock.2", 0.86f },
        { "balance.warlock.3", 1.0f }, { "balance.warlock.4", 1.0f } } },
    { { { "balance.oathblade.1", 0.57f }, { "balance.oathblade.2", 1.0f },
        { "balance.oathblade.3", 1.0f }, { "balance.oathblade.4", 1.0f } } },
    { { { "balance.druid.1", 1.13f }, { "balance.druid.2", 1.46f },
        { "balance.druid.3", 1.0f }, { "balance.druid.4", 1.0f } } },
    { { { "balance.pestifere.1", 1.0f }, { "balance.pestifere.2", 1.0f },
        { "balance.pestifere.3", 1.0f }, { "balance.pestifere.4", 1.0f } } },
    { { { "balance.necromancer.1", 1.0f }, { "balance.necromancer.2", 1.0f },
        { "balance.necromancer.3", 1.0f }, { "balance.necromancer.4", 1.0f } } },
    { { { "balance.barbarian.1", 1.12f }, { "balance.barbarian.2", 0.96f },
        { "balance.barbarian.3", 1.67f }, { "balance.barbarian.4", 1.0f } } },
    { { { "balance.reaper.1", 1.01f }, { "balance.reaper.2", 0.93f },
        { "balance.reaper.3", 1.0f }, { "balance.reaper.4", 1.0f } } },
};
SpecBalanceRow const SpecBalanceLow[] = {
    { { { "balance0.none.1", 1.0f }, { "balance0.none.2", 1.0f },
        { "balance0.none.3", 1.0f }, { "balance0.none.4", 1.0f } } },
    { { { "balance0.warrior.1", 0.86f }, { "balance0.warrior.2", 0.87f },
        { "balance0.warrior.3", 1.0f }, { "balance0.warrior.4", 0.95f } } },
    { { { "balance0.paladin.1", 1.0f }, { "balance0.paladin.2", 1.0f },
        { "balance0.paladin.3", 0.61f }, { "balance0.paladin.4", 1.0f } } },
    { { { "balance0.hunter.1", 0.8f }, { "balance0.hunter.2", 0.76f },
        { "balance0.hunter.3", 0.71f }, { "balance0.hunter.4", 1.0f } } },
    { { { "balance0.rogue.1", 0.97f }, { "balance0.rogue.2", 0.69f },
        { "balance0.rogue.3", 0.51f }, { "balance0.rogue.4", 1.0f } } },
    { { { "balance0.priest.1", 1.0f }, { "balance0.priest.2", 1.0f },
        { "balance0.priest.3", 0.71f }, { "balance0.priest.4", 1.0f } } },
    { { { "balance0.dk.1", 1.0f }, { "balance0.dk.2", 1.21f },
        { "balance0.dk.3", 0.92f }, { "balance0.dk.4", 1.0f } } },
    { { { "balance0.shaman.1", 1.03f }, { "balance0.shaman.2", 0.65f },
        { "balance0.shaman.3", 1.0f }, { "balance0.shaman.4", 1.0f } } },
    { { { "balance0.mage.1", 0.78f }, { "balance0.mage.2", 1.0f },
        { "balance0.mage.3", 1.03f }, { "balance0.mage.4", 1.0f } } },
    { { { "balance0.warlock.1", 0.97f }, { "balance0.warlock.2", 0.76f },
        { "balance0.warlock.3", 0.98f }, { "balance0.warlock.4", 1.0f } } },
    { { { "balance0.oathblade.1", 0.63f }, { "balance0.oathblade.2", 1.0f },
        { "balance0.oathblade.3", 1.0f }, { "balance0.oathblade.4", 1.0f } } },
    { { { "balance0.druid.1", 1.07f }, { "balance0.druid.2", 1.03f },
        { "balance0.druid.3", 1.0f }, { "balance0.druid.4", 1.0f } } },
    { { { "balance0.pestifere.1", 1.0f }, { "balance0.pestifere.2", 1.0f },
        { "balance0.pestifere.3", 1.0f }, { "balance0.pestifere.4", 1.0f } } },
    { { { "balance0.necromancer.1", 1.0f }, { "balance0.necromancer.2", 1.0f },
        { "balance0.necromancer.3", 1.0f }, { "balance0.necromancer.4", 1.0f } } },
    { { { "balance0.barbarian.1", 1.01f }, { "balance0.barbarian.2", 1.04f },
        { "balance0.barbarian.3", 1.18f }, { "balance0.barbarian.4", 1.0f } } },
    { { { "balance0.reaper.1", 0.88f }, { "balance0.reaper.2", 0.86f },
        { "balance0.reaper.3", 1.0f }, { "balance0.reaper.4", 1.0f } } },
};

float GetSpecBalance(Unit* attacker)
{
    Player* player = attacker ? attacker->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    if (!player || player->getClass() >= std::size(SpecBalance))
        return 1.0f;
    int8 const index = GetTalentSpecializationIndex(player);
    std::size_t const spec = index >= 0 && index < 4 ? std::size_t(index) : 0;
    float const high = SpecBalance[player->getClass()].specs[spec];
    float const low = SpecBalanceLow[player->getClass()].specs[spec];
    if (high == low)
        return high;
    float const share = std::min(float(GetParagonBalancePoints(player)) / SpecBalanceParagon, 1.0f);
    return low + (high - low) * share;
}

template<typename T>
void ScaleByCatchUp(Unit* attacker, Unit* target, T& damage)
{
    if (!(damage > 0))
        return;
    float multiplier = GetSpecBalance(attacker);
    if (float const catchUp = GetBotCatchUpMultiplier(attacker, target); catchUp > 1.0f)
        multiplier *= catchUp;
    if (multiplier != 1.0f)
        damage = static_cast<T>(std::min<double>(double(damage) * multiplier, std::numeric_limits<T>::max()));
}
}

// The combat bench's sweep (mod-playerbots Script/CombatBench.cpp, localTools/combatBench/bench.ps1 tune): the two
// spec balance knobs a character's damage goes through, their values and its paragon share between them,
// ";"-separated: "balance0.rogue.1;0.77;balance.rogue.1;1.45;0.923" ("-;1;-;1;0" for a class without a row)
std::string DescribeSpecBalance(Player* player)
{
    if (!player || player->getClass() >= std::size(SpecBalance))
        return "-;1;-;1;0";
    int8 const index = GetTalentSpecializationIndex(player);
    std::size_t const spec = index >= 0 && index < 4 ? std::size_t(index) : 0;
    LiveTuning::Knob const& low = SpecBalanceLow[player->getClass()].specs[spec];
    LiveTuning::Knob const& high = SpecBalance[player->getClass()].specs[spec];
    float const share = std::min(float(GetParagonBalancePoints(player)) / SpecBalanceParagon, 1.0f);
    return Acore::StringFormat("{};{:.4g};{};{:.4g};{:.3f}", low.Key(), low.Get(), high.Key(), high.Get(), share);
}

class StatGrowthUnitScript : public UnitScript
{
public:
    StatGrowthUnitScript() : UnitScript("StatGrowthUnitScript", true, {
        UNITHOOK_ON_DAMAGE,
        UNITHOOK_MODIFY_MELEE_DAMAGE,
        UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_FINAL_DAMAGE,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
        UNITHOOK_ON_SPELL_DAMAGE_DONE,
        UNITHOOK_ON_UNIT_DEATH,
        UNITHOOK_ON_UNIT_EXIT_COMBAT,
        UNITHOOK_MODIFY_HEAL_RECEIVED,
        UNITHOOK_ON_HEAL
    }) { }

    // The Pantheon's healing Blessings (ParagonSystem.cpp): Eonar's on the heal as it is cast, Freya's on what landed
    void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* spellInfo) override
    {
        OnParagonHealDone(healer, target, heal, spellInfo);
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
    {
        OnParagonHealReceived(healer, receiver, gain);
    }

    // What is about to deal the next hit, noted for the paragon board: the damage hook below is not told whether a
    // weapon or a spell dealt it
    void ModifyFinalDamage(Unit* attacker, Unit* victim, uint32& damage, uint32& absorb,
        SpellInfo const* spellInfo) override
    {
        NoteParagonDamageSource(attacker, victim, spellInfo, false);
        NoteParagonHitTaken(victim, attacker, damage + absorb,
            spellInfo ? spellInfo->GetSchoolMask() : SPELL_SCHOOL_MASK_NORMAL);
        MythicTuning::OnHitTaken(victim, attacker, spellInfo);
    }

    // A bot trailing its players deals more (BotCatchUpSystem.cpp): before mitigation, so the combat log and the
    // meters show it
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        ScaleByCatchUp(attacker, target, damage);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (spellInfo && IsPassThroughDamage(spellInfo->Id))
            return;
        ScaleByCatchUp(attacker, target, damage);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        NoteParagonDamageSource(attacker, target, spellInfo, true);
        // Before its absorb: a tick a shield soaks still counts for Rancune (a heal tick's healer is friendly)
        NoteParagonHitTaken(target, attacker, damage,
            spellInfo ? spellInfo->GetSchoolMask() : SPELL_SCHOOL_MASK_NORMAL);
        if (damage)
            MythicTuning::OnHitTaken(target, attacker, spellInfo);
        // A heal tick comes through here too, but on a friendly target, which never gets the bonus
        ScaleByCatchUp(attacker, target, damage);
    }

    void OnSpellDamageDone(Unit* caster, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
        bool critical) override
    {
        OnParagonSpellDamageDone(caster, victim, spellInfo, damage, critical);
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        ApplyPersonalLootLeech(attacker, victim, damage);
        // Both sides of the same hit: what the victim's paragon board does about being hit, and what the
        // attacker's does about landing one.
        OnParagonDamageDealt(attacker, victim, damage);
        OnParagonDamageTaken(victim, attacker, damage);
        RecordBotCatchUpDamage(attacker, victim, damage);
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        OnDungeonProgressUnitDeath(unit);
        // A heroic raid boss: glyph experience and a chance of a glyph for every real player in the raid
        if (Creature* creature = unit->ToCreature())
            OnParagonCreatureDeath(creature);
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

// frontier/Frontier.cpp: Le Front du Nord, open-world content at level 80
void AddFrontierScripts();
void AddWardenVorhanScripts();
void AddEscapeHunterScripts();
void AddFrontierQuartermasterScripts();
// DaytimeCommand.cpp: .daytime, the game master's client set to a time of day
void AddDaytimeCommand();
// FxLab.cpp: .fxlab, the plain gray room to look at spell visuals in (map 606)
void AddFxLabScripts();
// StatOverflowSystem.cpp: Robustesse on hits taken, the character sheet's overflow summary
void AddStatOverflowScripts();

void AddStatGrowthScripts()
{
    AddAdaptiveTrainingDummyScripts();
    AddBotCatchUpScripts();
    AddGladiatorStanceScripts();
    AddVictoryRushScripts();
    AddQuickTravelScripts();
    AddRidingInstructorScripts();
    AddParagonScripts();
    AddPrestigeScripts();
    AddDungeonFinderLockScripts();
    AddMythicDungeonScripts();
    AddGroundIndicatorScripts();
    AddGroundLootScripts();
    AddExperienceRateCommand();
    AddDaytimeCommand();
    AddFxLabScripts();
    AddOnyxiaReworkScripts();
    AddBronjahmReworkScripts();
    AddDevourerReworkScripts();
    AddInfiniteGodScripts();
    AddHollowVoiceScripts();
    AddWardenVorhanScripts();
    AddEscapeHunterScripts();
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
    AddMythicAppearanceScripts();
    AddSmartLootScripts();
    AddInfiniteDungeonScripts();
    AddFrontierScripts();
    AddFrontierQuartermasterScripts();
    new StatGrowthWorldScript();
    new StatGrowthGlobalScript();
    // Before StatGrowthUnitScript: Robustesse comes off a hit before the paragon board's reduction and procs see it
    AddStatOverflowScripts();
    new StatGrowthUnitScript();
    new StatGrowthPlayerScript();
    new EssenceItemScript("item_stat_growth_essence");
    new EssenceItemScript("item_experience_boost_essence");
    new EssenceItemScript("item_resource_boost_essence");
    new EssenceItemScript("item_vitality_boost_essence");
    new EssenceItemScript("item_fortune_boost_essence");
}
