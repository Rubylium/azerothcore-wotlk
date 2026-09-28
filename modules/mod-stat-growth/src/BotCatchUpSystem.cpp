#include "BotCatchUpSystem.h"

#include "BotEssenceSystem.h"
#include "Chat.h"
#include "DataMap.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Map.h"
#include "MythicDungeonSystem.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "StatGrowthConfig.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace
{
constexpr char TrackerKey[] = "StatGrowthBotCatchUp";

// Chaotic Charge: a stock dummy aura with no effect, stacking to 200 and lasting until removed. Only a Black Temple
// creature script reads it (on its own creature), so it is free to name the bonus on a bot: a stack per percent. The
// bonus itself has no limit (BotCatchUp.MaxMultiplier 0): past +200% the display stays at 200, .botcatchup shows it.
constexpr uint32 SPELL_CATCH_UP_DISPLAY = 41033;
constexpr uint32 MaxDisplayStacks = 200;

constexpr uint32 SampleMs = 1000;           // damage summed per second
constexpr float WindowMs = 30000.0f;        // and smoothed over about this much combat
constexpr uint32 AdjustMs = 5000;           // a bot's multiplier moves this often
constexpr float StepFactor = 1.10f;         // by at most 10% each time
constexpr uint32 MinCombatSamples = 8;      // seconds of combat before a character's damage is trusted
constexpr float DecayHalfLifeMs = 300000.0f; // out of combat, the bonus halves every 5 minutes
constexpr float MinRawDps = 1.0f;

struct CatchUpTracker : public DataMap::Base
{
    uint32 instanceId = 0;
    double bucket = 0.0;        // damage since the last sample, without the bonus
    uint32 sampleTimer = 0;
    uint32 adjustTimer = 0;
    float dps = 0.0f;           // smoothed damage per second of combat, without the bonus
    uint32 combatSamples = 0;
    float multiplier = 1.0f;
    float target = 1.0f;        // last target worked out (.botcatchup)
    float groupDps = 0.0f;      // the real damage dealers' average at the last adjustment (.botcatchup)
};

std::bitset<32> ExcludedClasses;

bool IsEnabled()
{
    return statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::Enabled) &&
        statGrowthConfig.GetConfigValue<bool>(StatGrowthConfigKey::BotCatchUpEnabled);
}

bool IsBot(Player const* player)
{
    return player && player->GetSession() && player->GetSession()->IsBot();
}

bool HasGroupRole(Player* player, uint8 role)
{
    if (sLFGMgr->GetRoles(player->GetGUID()) & role)
        return true;
    if (Group* group = player->GetGroup())
        for (Group::MemberSlot const& member : group->GetMemberSlots())
            if (member.guid == player->GetGUID())
                return (member.roles & role) != 0;
    return false;
}

// Tanks and healers are neither measured against nor helped: their damage is not their job
bool IsDamageDealer(Player* player)
{
    return !IsGroupTank(player) && !player->HasTankSpec() && !player->HasHealSpec() &&
        !HasGroupRole(player, lfg::PLAYER_ROLE_HEALER);
}

// A dungeon or a raid (Map::IsDungeon is both): the instance id, else 0
uint32 CatchUpInstance(Player* player)
{
    Map* map = player->FindMap();
    return map && map->IsDungeon() ? map->GetInstanceId() : 0;
}

void SyncDisplay(Player* bot, float multiplier)
{
    uint32 const stacks = std::min<uint32>(MaxDisplayStacks,
        static_cast<uint32>(std::lround(std::max(0.0f, multiplier - 1.0f) * 100.0f)));
    Aura* aura = bot->GetAura(SPELL_CATCH_UP_DISPLAY);
    if (!stacks)
    {
        if (aura)
            bot->RemoveAurasDueToSpell(SPELL_CATCH_UP_DISPLAY);
        return;
    }

    // Death takes it off; the next adjustment puts it back
    if (!bot->IsAlive())
        return;
    if (!aura)
        aura = bot->AddAura(SPELL_CATCH_UP_DISPLAY, bot);
    if (aura && aura->GetStackAmount() != stacks)
        aura->SetStackAmount(static_cast<uint8>(stacks));
}

void SetMultiplier(Player* bot, CatchUpTracker* tracker, float multiplier)
{
    // Close enough to nothing is nothing, so the fade ends
    if (multiplier < 1.005f)
        multiplier = 1.0f;
    tracker->multiplier = multiplier;
    SyncDisplay(bot, multiplier);
}

// The real damage dealers of the bot's group on its map, by their smoothed damage. Only that map is read: another
// map updates on another thread.
uint32 ReadPlayersDps(Player* bot, float& average)
{
    average = 0.0f;
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    double total = 0.0;
    uint32 counted = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->GetSession() || member->GetSession()->IsBot() || !member->IsInMap(bot) ||
            !IsDamageDealer(member))
            continue;

        CatchUpTracker const* tracker = member->CustomData.Get<CatchUpTracker>(TrackerKey);
        if (!tracker || tracker->combatSamples < MinCombatSamples || tracker->dps <= 0.0f)
            continue;
        total += tracker->dps;
        ++counted;
    }

    if (counted)
        average = static_cast<float>(total / counted);
    return counted;
}

bool HasRealPlayerOnMap(Player* bot)
{
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member && member->GetSession() &&
                !member->GetSession()->IsBot() && member->IsInMap(bot))
                return true;
    return false;
}

bool IsEligibleBot(Player* bot)
{
    uint8 const classId = bot->getClass();
    return !(classId < ExcludedClasses.size() && ExcludedClasses.test(classId)) && IsDamageDealer(bot) &&
        HasRealPlayerOnMap(bot);
}

void Adjust(Player* bot, CatchUpTracker* tracker, uint32 elapsed)
{
    if (!IsEligibleBot(bot))
    {
        tracker->target = 1.0f;
        SetMultiplier(bot, tracker, 1.0f);
        return;
    }

    if (!bot->IsInCombat())
    {
        float const excess = tracker->multiplier - 1.0f;
        SetMultiplier(bot, tracker, 1.0f + excess * std::exp2(-static_cast<float>(elapsed) / DecayHalfLifeMs));
        return;
    }

    float average = 0.0f;
    if (!ReadPlayersDps(bot, average) || tracker->combatSamples < MinCombatSamples)
    {
        SyncDisplay(bot, tracker->multiplier);
        return;
    }

    // MaxMultiplier 0: no limit. A bot dealing next to nothing then keeps climbing a step at a time.
    float const maxMultiplier = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::BotCatchUpMaxMultiplier);
    float const limit = maxMultiplier > 0.0f ? maxMultiplier : std::numeric_limits<float>::max();
    float const wanted = statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::BotCatchUpTargetShare) * average;
    float const target = std::clamp(tracker->dps > MinRawDps ? wanted / tracker->dps :
        std::min(limit, tracker->multiplier * StepFactor), 1.0f, limit);
    tracker->groupDps = average;
    tracker->target = target;

    float const current = tracker->multiplier;
    float const next = target > current ? std::min(target, current * StepFactor)
                                        : std::max(target, current / StepFactor);
    SetMultiplier(bot, tracker, next);
}

std::string FormatDps(float dps)
{
    return dps >= 1000.0f ? Acore::StringFormat("{:.1f}k", dps / 1000.0f) : Acore::StringFormat("{:.0f}", dps);
}

using namespace Acore::ChatCommands;

// .botcatchup: the selected player's group (else your own) on your map, with each member's smoothed damage, and each
// bot's multiplier, its target and its essence mirror
class BotCatchUpCommandScript : public CommandScript
{
public:
    BotCatchUpCommandScript() : CommandScript("BotCatchUpCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "botcatchup", HandleBotCatchUp, SEC_GAMEMASTER, Console::No },
        };
        return commandTable;
    }

    static bool HandleBotCatchUp(ChatHandler* handler)
    {
        Player* anchor = handler->getSelectedPlayerOrSelf();
        Group* group = anchor ? anchor->GetGroup() : nullptr;
        if (!group)
        {
            handler->SendSysMessage("No group: select a member of the group to look at.");
            return true;
        }

        float const maxMultiplier =
            statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::BotCatchUpMaxMultiplier);
        handler->PSendSysMessage("Bot catch-up: {} (target {:.0f}% of the players' average, {}).",
            IsEnabled() ? "on" : "off",
            statGrowthConfig.GetConfigValue<float>(StatGrowthConfigKey::BotCatchUpTargetShare) * 100.0f,
            maxMultiplier > 0.0f ? Acore::StringFormat("max x{:.2f}", maxMultiplier) : std::string("no limit"));

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->GetSession() || !member->IsInMap(anchor))
                continue;

            CatchUpTracker const* tracker = member->CustomData.Get<CatchUpTracker>(TrackerKey);
            float const dps = tracker ? tracker->dps : 0.0f;
            uint32 const samples = tracker ? tracker->combatSamples : 0;
            std::string_view const role = !IsDamageDealer(member) ? "tank/heal" : "dps";

            if (!IsBot(member))
            {
                handler->PSendSysMessage("  {} (player, class {}, {}): {} DPS over {} s of combat", member->GetName(),
                    member->getClass(), role, FormatDps(dps), samples);
                continue;
            }

            uint32 statPoints = 0;
            uint32 vitality = 0;
            uint32 resource = 0;
            uint32 players = 0;
            GetBotEssenceSummary(member, statPoints, vitality, resource, players);
            uint8 const classId = member->getClass();
            bool const excluded = classId < ExcludedClasses.size() && ExcludedClasses.test(classId);
            handler->PSendSysMessage("  {} (bot, class {}, {}{}): x{:.2f} (target x{:.2f}, players {} DPS), raw {} "
                "DPS over {} s | essences of {} player(s): +{} stat points, {} Vitality, +{}% resource",
                member->GetName(), classId, role, excluded ? ", excluded" : "", tracker ? tracker->multiplier : 1.0f,
                tracker ? tracker->target : 1.0f, FormatDps(tracker ? tracker->groupDps : 0.0f), FormatDps(dps),
                samples, players, statPoints, vitality, resource);
        }
        return true;
    }
};
}

void LoadBotCatchUpConfig()
{
    ExcludedClasses.reset();
    std::string_view list = statGrowthConfig.GetConfigValue(StatGrowthConfigKey::BotCatchUpExcludedClasses);
    while (!list.empty())
    {
        size_t const comma = list.find(',');
        std::string const token(list.substr(0, comma));
        list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);

        char* end = nullptr;
        unsigned long const classId = std::strtoul(token.c_str(), &end, 10);
        if (end != token.c_str() && classId < ExcludedClasses.size())
            ExcludedClasses.set(classId);
    }
}

void RecordBotCatchUpDamage(Unit* attacker, Unit* victim, uint32 damage)
{
    if (!damage || !attacker || !victim || attacker == victim || victim->IsCharmedOwnedByPlayerOrPlayer() ||
        !attacker->IsCharmedOwnedByPlayerOrPlayer() || !IsEnabled())
        return;

    Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!owner || !owner->GetGroup() || !CatchUpInstance(owner))
        return;

    // What the hit really took, before a bot's bonus: the bonus must not feed its own measure
    CatchUpTracker* tracker = owner->CustomData.GetDefault<CatchUpTracker>(TrackerKey);
    tracker->bucket += static_cast<double>(std::min<uint32>(damage, victim->GetHealth())) / tracker->multiplier;
}

float GetBotCatchUpDps(Player const* player)
{
    CatchUpTracker const* tracker = player ? player->CustomData.Get<CatchUpTracker>(TrackerKey) : nullptr;
    return tracker && tracker->combatSamples >= MinCombatSamples ? tracker->dps : 0.0f;
}

float GetBotCatchUpMultiplier(Unit* attacker, Unit* victim)
{
    if (!attacker || !victim || victim->IsCharmedOwnedByPlayerOrPlayer() ||
        !attacker->IsCharmedOwnedByPlayerOrPlayer())
        return 1.0f;

    Player* owner = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!IsBot(owner))
        return 1.0f;

    CatchUpTracker const* tracker = owner->CustomData.Get<CatchUpTracker>(TrackerKey);
    return tracker ? tracker->multiplier : 1.0f;
}

void UpdateBotCatchUp(Player* player, uint32 diff)
{
    CatchUpTracker* tracker = player->CustomData.Get<CatchUpTracker>(TrackerKey);
    if (!tracker)
        return;

    // Out of the instance the measure started in, or switched off: gone, bonus and all
    uint32 const instanceId = CatchUpInstance(player);
    if (!IsEnabled() || !instanceId || (tracker->instanceId && tracker->instanceId != instanceId))
    {
        ClearBotCatchUp(player);
        return;
    }
    tracker->instanceId = instanceId;

    tracker->sampleTimer += diff;
    if (tracker->sampleTimer >= SampleMs)
    {
        if (player->IsInCombat())
        {
            float const sample = static_cast<float>(tracker->bucket * 1000.0 / tracker->sampleTimer);
            // A plain mean over the first seconds, so the start is neither averaged with nothing nor one burst
            ++tracker->combatSamples;
            float const alpha = std::max(1.0f - std::exp(-static_cast<float>(tracker->sampleTimer) / WindowMs),
                1.0f / static_cast<float>(tracker->combatSamples));
            tracker->dps += alpha * (sample - tracker->dps);
        }
        tracker->bucket = 0.0;
        tracker->sampleTimer = 0;
    }

    if (!IsBot(player))
        return;

    tracker->adjustTimer += diff;
    if (tracker->adjustTimer < AdjustMs)
        return;
    uint32 const elapsed = tracker->adjustTimer;
    tracker->adjustTimer = 0;
    Adjust(player, tracker, elapsed);
}

void ClearBotCatchUp(Player* player)
{
    if (!player)
        return;
    player->CustomData.Erase(TrackerKey);
    if (IsBot(player))
        player->RemoveAurasDueToSpell(SPELL_CATCH_UP_DISPLAY);
}

void AddBotCatchUpScripts()
{
    new BotCatchUpCommandScript();
}
