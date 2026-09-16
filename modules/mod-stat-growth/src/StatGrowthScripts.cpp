#include "AutoLearnSpellsSystem.h"
#include "EssenceFeedback.h"
#include "EssenceTierSystem.h"
#include "ExperienceBoostSystem.h"
#include "FortuneBoostSystem.h"
#include "GladiatorStanceSystem.h"
#include "PersonalLootSystem.h"
#include "QuickTravelSystem.h"
#include "ResourceBoostSystem.h"
#include "SmartLootSystem.h"
#include "RogueMomentumSystem.h"
#include "StatGrowthSystem.h"
#include "VictoryRushSystem.h"
#include "VitalityBoostSystem.h"

#include "Chat.h"
#include "Creature.h"
#include "Item.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "StatGrowthConfig.h"
#include "StringFormat.h"

namespace
{
void PlayTieredFeedback(Player* player, EssenceVisual visual, EssenceTier tier, std::string_view powerName)
{
    std::string const announcement = Acore::StringFormat(
        "{} {} AWAKENED", GetEssenceTierName(tier), powerName);
    PlayEssenceFeedback(player, visual, tier, announcement);
}

// Grants the permanent bonus of one essence and plays its feedback. The caller removes the item.
bool ConsumeEssence(Player* player, uint32 itemEntry)
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

            PlayTieredFeedback(player, EssenceVisual::Vitality, tier, "VITALITY");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Maximum health "
                "permanently increased by +{}%. Total: +{}%.|r", tierName, amount, totalBonus);
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

            PlayTieredFeedback(player, EssenceVisual::Fortune, tier, "FORTUNE");
            chat.PSendSysMessage("|cffa335eeThe {} essence binds to your soul.|r |cff00ff00Gold gains and loot "
                "quality permanently increased by +{}%. Total: +{}%.|r", tierName, amount, totalBonus);
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
}

class StatGrowthWorldScript : public WorldScript
{
public:
    StatGrowthWorldScript() : WorldScript("StatGrowthWorldScript", {
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE
    }) { }

    void OnBeforeConfigLoad(bool reload) override
    {
        statGrowthConfig.Initialize(reload);
    }

    void OnLoadCustomDatabaseTable() override
    {
        LoadPersonalLootRolls();
    }
};

class StatGrowthPlayerScript : public PlayerScript
{
public:
    StatGrowthPlayerScript() : PlayerScript("StatGrowthPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
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
        PLAYERHOOK_ON_UNEQUIP_ITEM
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled))
            ApplyStoredStatGrowth(player);

        LearnAvailableClassSpells(player);
        LearnRogueMomentumAbilities(player);
        LearnGladiatorStance(player);
        ApplyEquippedPersonalLoot(player);
        BeginPersonalLootAddonHandshake(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearRogueMomentum(player);
        ClearQuickTravel(player);
        ClearPersonalLootPlayerState(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        UpdateRogueMomentum(player, diff);
        UpdateGladiatorStance(player);
        UpdatePersonalLootAddonHandshake(player, diff);
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32&, uint32& language, std::string& message) override
    {
        HandlePersonalLootAddonMessage(player, language, message);
        HandleQuickTravelAddonMessage(player, language, message);
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (player->GetLevel() > oldLevel)
        {
            LearnAvailableClassSpells(player);
            LearnRogueMomentumAbilities(player);
            LearnGladiatorStance(player);
        }
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        ImproveBaseEquipmentLoot(killer, killed);
        ApplyFortuneLootBoost(killer, killed);
        TryAddStatGrowthLoot(killer, killed);
        TryAddExperienceBoostLoot(killer, killed);
        TryAddResourceBoostLoot(killer, killed);
        TryAddVitalityBoostLoot(killer, killed);
        TryAddFortuneBoostLoot(killer, killed);
        OnRogueMomentumKill(killer);
    }

    void OnPlayerCreatureKilledByPet(Player* petOwner, Creature* killed) override
    {
        ImproveBaseEquipmentLoot(petOwner, killed);
        ApplyFortuneLootBoost(petOwner, killed);
        TryAddStatGrowthLoot(petOwner, killed);
        TryAddExperienceBoostLoot(petOwner, killed);
        TryAddResourceBoostLoot(petOwner, killed);
        TryAddVitalityBoostLoot(petOwner, killed);
        TryAddFortuneBoostLoot(petOwner, killed);
        OnRogueMomentumKill(petOwner);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit*, uint8) override
    {
        ApplyExperienceBoost(player, amount);
    }

    void OnPlayerBeforeRegeneratePower(Player* player, Powers power, float& amount) override
    {
        ApplyResourceRegenerationBoost(player, power, amount);
        ApplyRogueMomentumRegeneration(player, power, amount);
    }

    void OnPlayerBeforeModifyPower(Player* player, Powers power, int32& amount) override
    {
        ApplyResourceGenerationBoost(player, power, amount);
    }

    void OnPlayerAfterUpdateMaxHealth(Player* player, float& value) override
    {
        ApplyVitalityBoost(player, value);
    }

    void OnPlayerMoneyChanged(Player* player, int32& amount) override
    {
        ApplyFortuneGoldBoost(player, amount);
    }

    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid) override
    {
        if (!AutoConsumeLootedEssences(player, item, count))
            TryRollPersonalLoot(player, item);
    }

    void OnPlayerGroupRollRewardItem(Player* player, Item* item, uint32 count, RollVote, Roll*) override
    {
        if (!AutoConsumeLootedEssences(player, item, count))
            TryRollPersonalLoot(player, item);
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
    StatGrowthUnitScript() : UnitScript("StatGrowthUnitScript", true, { UNITHOOK_ON_DAMAGE }) { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        ApplyPersonalLootLeech(attacker, victim, damage);
    }
};

class StatGrowthGlobalScript : public GlobalScript
{
public:
    StatGrowthGlobalScript() : GlobalScript("StatGrowthGlobalScript", {
        GLOBALHOOK_ON_ITEM_DEL_FROM_DB
    }) { }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        DeletePersonalLootRoll(transaction, itemGuid);
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
    AddRogueMomentumScripts();
    AddGladiatorStanceScripts();
    AddVictoryRushScripts();
    AddQuickTravelScripts();
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
