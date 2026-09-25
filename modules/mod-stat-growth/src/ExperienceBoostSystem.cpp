#include "ExperienceBoostSystem.h"

#include "CharacterDatabase.h"
#include "Chat.h"
#include "Creature.h"
#include "EssenceTierSystem.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "PersonalLootSystem.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include <algorithm>
#include <limits>

namespace
{
constexpr char ExperienceSettingsSource[] = "mod_stat_growth_xp";
constexpr uint32 ExperienceBonusSetting = 0;

// A character's own experience rate, in percent (.xp): 0 is unset, which is 100. A source of its own - the bonus above
// is saved by rewriting its whole source.
constexpr char ExperienceRateSource[] = "mod_stat_growth_xp_rate";
constexpr uint32 ExperienceRateSetting = 0;
constexpr uint32 MaxExperienceRate = 10000;

uint32 GetExperienceRate(Player* player)
{
    uint32 const rate = player->GetPlayerSetting(ExperienceRateSource, ExperienceRateSetting).value;
    return rate ? rate : 100;
}

void SetExperienceRate(Player* player, uint32 rate)
{
    uint32 const stored = rate == 100 ? 0 : rate;
    player->UpdatePlayerSetting(ExperienceRateSource, ExperienceRateSetting, stored);
    PlayerSettingVector settings(1);
    settings[ExperienceRateSetting].value = stored;
    CharacterDatabase.Execute(PlayerSettingsStore::PrepareReplaceStatement(player->GetGUID().GetCounter(),
        ExperienceRateSource, settings));
}

// Leveling pace: from 70 the XP a level needs doubles (717,000 at 69, 1,523,800 at 70) while a kill gives only about
// 1.6 times more, so WotLK levels took twice as long as TBC ones. From 70 to 79, XP is scaled so a level takes as
// long as an average TBC level (65): how much more the level needs, over how much more a same-level kill gives.
constexpr uint8 WotlkPaceFirstLevel = 70;
constexpr uint8 WotlkPaceLastLevel = 79;
constexpr uint8 TbcReferenceLevel = 65;

// Unit::BaseGain for a same-level mob: level * 5 plus the content's base (TBC 235, WotLK 580)
float SameLevelKillExperience(uint8 level)
{
    return float(level) * 5.0f + (level < WotlkPaceFirstLevel ? 235.0f : 580.0f);
}

float GetLevelingPaceMultiplier(uint8 level)
{
    if (level < WotlkPaceFirstLevel || level > WotlkPaceLastLevel)
        return 1.0f;

    uint32 const referenceNeed = sObjectMgr->GetXPForLevel(TbcReferenceLevel);
    uint32 const need = sObjectMgr->GetXPForLevel(level);
    if (!referenceNeed || !need)
        return 1.0f;

    float const needRatio = float(need) / float(referenceNeed);
    float const killRatio = SameLevelKillExperience(level) / SameLevelKillExperience(TbcReferenceLevel);
    return std::max(1.0f, needRatio / killRatio);
}

bool IsEligibleCreature(Creature const* killed)
{
    return killed && !killed->IsPet() && !killed->IsTotem() &&
        killed->GetCreatureTemplate()->type != CREATURE_TYPE_CRITTER;
}
}

void ApplyExperienceBoost(Player* player, uint32& amount)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || amount == 0)
        return;

    uint64 const bonusPercent = static_cast<uint64>(player->GetPlayerSetting(
        ExperienceSettingsSource, ExperienceBonusSetting).value) +
        GetEquippedPersonalLootBonus(player, PersonalLootAffix::ExperienceGain);
    // Every source (kills, quests, dungeons, exploration) goes through here
    uint64 const pacedAmount = static_cast<uint64>(float(amount) * GetLevelingPaceMultiplier(player->GetLevel()));
    uint64 const boostedAmount = pacedAmount + pacedAmount * bonusPercent / 100;
    // The character's own rate (.xp) last, on everything else
    uint64 const ratedAmount = boostedAmount * GetExperienceRate(player) / 100;
    amount = static_cast<uint32>(std::min<uint64>(ratedAmount, std::numeric_limits<uint32>::max()));
}

bool GrantExperienceBoost(Player* player, uint32 amount, uint32& totalBonus)
{
    uint32 const currentBonus = player->GetPlayerSetting(
        ExperienceSettingsSource, ExperienceBonusSetting).value;
    if (amount > std::numeric_limits<uint32>::max() - currentBonus)
        return false;

    totalBonus = currentBonus + amount;
    PlayerSettingVector settings(1);
    settings[ExperienceBonusSetting].value = totalBonus;
    player->UpdatePlayerSetting(ExperienceSettingsSource, ExperienceBonusSetting, totalBonus);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), ExperienceSettingsSource, settings);
    CharacterDatabase.Execute(statement);
    return true;
}

void TryAddExperienceBoostLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player ||
        !IsEligibleCreature(killed))
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::ExperienceDropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Experience);
}

namespace
{
using namespace Acore::ChatCommands;

// .xp <percent>: the game master's own experience rate - .xp 200 doubles every experience gain, .xp 100 is the normal
// rate again, .xp alone says the current one. Kept on the character across logins.
class ExperienceRateCommandScript : public CommandScript
{
public:
    ExperienceRateCommandScript() : CommandScript("ExperienceRateCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "xp", HandleExperienceRate, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    static bool HandleExperienceRate(ChatHandler* handler, Optional<uint32> percent)
    {
        Player* player = handler->GetPlayer();
        bool const french = player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
        if (!percent)
        {
            handler->PSendSysMessage(french ? "Taux d'expérience : {}%." : "Experience rate: {}%.",
                GetExperienceRate(player));
            return true;
        }

        if (*percent < 1 || *percent > MaxExperienceRate)
        {
            handler->PSendSysMessage(french ? "Le taux doit aller de 1 à {}%." : "The rate must be between 1 and {}%.",
                MaxExperienceRate);
            return false;
        }

        SetExperienceRate(player, *percent);
        handler->PSendSysMessage(french ? "Taux d'expérience réglé à {}%." : "Experience rate set to {}%.", *percent);
        return true;
    }
};
}

void AddExperienceRateCommand()
{
    new ExperienceRateCommandScript();
}
