#include "StatGrowthSystem.h"

#include "CharacterDatabase.h"
#include "Creature.h"
#include "EssenceTierSystem.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "Random.h"
#include "SharedDefines.h"
#include "StatGrowthConfig.h"
#include <array>
#include <limits>
#include <span>

namespace
{
constexpr char SettingsSource[] = "mod_stat_growth";

constexpr std::array warriorStats = {
    PermanentStat::Strength, PermanentStat::Stamina, PermanentStat::AttackPower
};
constexpr std::array paladinStats = {
    PermanentStat::Strength, PermanentStat::Stamina, PermanentStat::Intellect,
    PermanentStat::AttackPower, PermanentStat::SpellPower
};
constexpr std::array hunterStats = {
    PermanentStat::Agility, PermanentStat::Stamina, PermanentStat::AttackPower
};
constexpr std::array rogueStats = {
    PermanentStat::Agility, PermanentStat::Stamina, PermanentStat::AttackPower
};
constexpr std::array priestStats = {
    PermanentStat::Stamina, PermanentStat::Intellect, PermanentStat::Spirit, PermanentStat::SpellPower
};
constexpr std::array deathKnightStats = {
    PermanentStat::Strength, PermanentStat::Stamina, PermanentStat::AttackPower
};
constexpr std::array shamanStats = {
    PermanentStat::Strength, PermanentStat::Agility, PermanentStat::Stamina,
    PermanentStat::Intellect, PermanentStat::AttackPower, PermanentStat::SpellPower
};
constexpr std::array mageStats = {
    PermanentStat::Stamina, PermanentStat::Intellect, PermanentStat::Spirit, PermanentStat::SpellPower
};
constexpr std::array warlockStats = {
    PermanentStat::Stamina, PermanentStat::Intellect, PermanentStat::Spirit, PermanentStat::SpellPower
};
constexpr std::array druidStats = {
    PermanentStat::Strength, PermanentStat::Agility, PermanentStat::Stamina, PermanentStat::Intellect,
    PermanentStat::Spirit, PermanentStat::AttackPower, PermanentStat::SpellPower
};
constexpr std::array<PermanentStat, 0> noStats = {};

std::span<PermanentStat const> GetClassStats(uint8 classId)
{
    // A custom class grows the stats of the class it is built on (see mod-custom-classes): the Pestiféré is a
    // strength plate tank on Death Knight rules. Without this it matches no case and essences grant nothing.
    switch (sObjectMgr->GetClassFormulaTemplate(classId))
    {
        case CLASS_WARRIOR:
            return warriorStats;
        case CLASS_PALADIN:
            return paladinStats;
        case CLASS_HUNTER:
            return hunterStats;
        case CLASS_ROGUE:
            return rogueStats;
        case CLASS_PRIEST:
            return priestStats;
        case CLASS_DEATH_KNIGHT:
            return deathKnightStats;
        case CLASS_SHAMAN:
            return shamanStats;
        case CLASS_MAGE:
            return mageStats;
        case CLASS_WARLOCK:
            return warlockStats;
        case CLASS_DRUID:
            return druidStats;
        default:
            return noStats;
    }
}

std::string_view GetStatName(PermanentStat stat)
{
    switch (stat)
    {
        case PermanentStat::Strength:
            return "Strength";
        case PermanentStat::Agility:
            return "Agility";
        case PermanentStat::Stamina:
            return "Stamina";
        case PermanentStat::Intellect:
            return "Intellect";
        case PermanentStat::Spirit:
            return "Spirit";
        case PermanentStat::AttackPower:
            return "Attack Power";
        case PermanentStat::SpellPower:
            return "Spell Power";
        default:
            return "Unknown";
    }
}

void ApplyStatGrowth(Player* player, PermanentStat stat, uint32 amount)
{
    switch (stat)
    {
        case PermanentStat::Strength:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, amount, true);
            player->UpdateStatBuffMod(STAT_STRENGTH);
            break;
        case PermanentStat::Agility:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, amount, true);
            player->UpdateStatBuffMod(STAT_AGILITY);
            break;
        case PermanentStat::Stamina:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, amount, true);
            player->UpdateStatBuffMod(STAT_STAMINA);
            break;
        case PermanentStat::Intellect:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, amount, true);
            player->UpdateStatBuffMod(STAT_INTELLECT);
            break;
        case PermanentStat::Spirit:
            player->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, amount, true);
            player->UpdateStatBuffMod(STAT_SPIRIT);
            break;
        case PermanentStat::AttackPower:
            player->HandleStatFlatModifier(player->getClass() == CLASS_HUNTER ? UNIT_MOD_ATTACK_POWER_RANGED
                                                                              : UNIT_MOD_ATTACK_POWER,
                TOTAL_VALUE, amount, true);
            break;
        case PermanentStat::SpellPower:
            player->ApplySpellPowerBonus(amount, true);
            break;
        default:
            break;
    }
}

bool SaveStatGrowth(Player* player, PermanentStat stat, uint32 amount)
{
    uint32 const statIndex = static_cast<uint32>(stat);
    uint32 const currentValue = player->GetPlayerSetting(SettingsSource, statIndex).value;
    if (amount > std::numeric_limits<uint32>::max() - currentValue)
        return false;

    PlayerSettingVector settings(static_cast<uint32>(PermanentStat::Count));
    for (uint32 index = 0; index < settings.size(); ++index)
        settings[index] = player->GetPlayerSetting(SettingsSource, index);

    uint32 const newValue = currentValue + amount;
    settings[statIndex].value = newValue;
    player->UpdatePlayerSetting(SettingsSource, statIndex, newValue);

    CharacterDatabasePreparedStatement* statement = PlayerSettingsStore::PrepareReplaceStatement(
        player->GetGUID().GetCounter(), SettingsSource, settings);
    CharacterDatabase.Execute(statement);
    return true;
}
}

void ApplyStoredStatGrowth(Player* player)
{
    for (uint32 index = 0; index < static_cast<uint32>(PermanentStat::Count); ++index)
    {
        uint32 const amount = player->GetPlayerSetting(SettingsSource, index).value;
        if (amount > 0)
            ApplyStatGrowth(player, static_cast<PermanentStat>(index), amount);
    }
}

bool GrantRandomStatGrowth(Player* player, uint32 amount, std::string_view& statName)
{
    std::span<PermanentStat const> const classStats = GetClassStats(player->getClass());
    if (classStats.empty())
        return false;

    PermanentStat const selectedStat = classStats[urand(0, classStats.size() - 1)];
    if (!SaveStatGrowth(player, selectedStat, amount))
        return false;

    ApplyStatGrowth(player, selectedStat, amount);
    statName = GetStatName(selectedStat);
    return true;
}

void TryAddStatGrowthLoot(Player* player, Creature* killed)
{
    if (!statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) || !player || !killed)
        return;

    if (killed->IsPet() || killed->IsTotem() || killed->GetCreatureTemplate()->type == CREATURE_TYPE_CRITTER)
        return;

    float const dropChance = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::DropChance);
    if (!roll_chance_f(dropChance))
        return;

    AddTieredEssenceLoot(killed, EssenceFamily::Growth);
}
