// Legendary items: per-copy rolls, applied when worn, and their powers. See Legendary.h and README.md.

#include "Legendary.h"

#include "Bag.h"
#include "CellImpl.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "GlobalScript.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "GroundLoot.h"
#include "Item.h"
#include "LiveTuning.h"
#include "Log.h"
#include "Map.h"
#include "MythicDungeon.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "PowerScaling.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "Timer.h"
#include "Tokenize.h"
#include "UnitScript.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>

namespace Legendary
{
namespace
{
// The client's tooltip feed (FrameXML Legendary.lua):
//   server -> client  C <tab> seed <tab> legendary <tab> item level <tab> power x10 <tab> window low x10 <tab>
//                         window high x10 <tab> armour <tab> type=value,type=value... [<tab> bag:slot]
//   client -> server  W <tab> bag <tab> slot    (a copy the player carries, by where it is: the server's bag and slot)
//                     Q <tab> seed              (a copy by its id)
// The seed is the copy's item guid. The client knows its own copies by where they sit: the 3.3.5 client does not put
// an item's property seed in its links (only a random suffix item's), so a link alone does not say which copy it is.
constexpr std::string_view Prefix = "LEGENDARY";

// localTools/legendary/Spells.ps1
constexpr uint32 SPELL_INQUISITOR_BRAND = 97000;
constexpr uint32 SPELL_WHITEMANE_OATH_HEAL = 97001;
constexpr uint32 SPELL_WHITEMANE_OATH_SPENT = 97002;
constexpr uint32 SPELL_MOGRAINE_GROUND = 97003;
constexpr uint32 SPELL_MOGRAINE_GROUND_DAMAGE = 97004;
constexpr uint32 SPELL_MOGRAINE_GROUND_HEAL = 97005;
constexpr int32 BrandTicks = 4;
constexpr int32 OathTicks = 4;
// Consécration de Mograine: every this long in combat, for this many pulses a second apart, this far around
constexpr uint32 GroundEveryMs = 10000;
constexpr uint32 GroundPulses = 6;
constexpr float GroundRadius = 8.0f;

// The slots' budgets: a cloak (Cloak of Burning Dusk, 284: its armour the same for every wearer) and a ring (Ring of
// Phased Regeneration, 284) share one; gloves (the Icecrown heroic ones, 277) carry more, and the armour of the
// looter's type
constexpr Budget CloakBudget = { 284.0f, 83, 83, 118, 69, { 189, 189, 189, 189 } };
constexpr Budget RingBudget = { 284.0f, 83, 83, 118, 69, { 0, 0, 0, 0 } };
constexpr Budget GlovesBudget = { 277.0f, 147, 155, 209, 86, { 231, 434, 964, 1723 } };

// The Scarlet Cathedral's Mythic+ (Scarlet Monastery, Dungeon Finder dungeon 164); localTools/patchSinisterStrike.ps1
// and the module's world SQL hold their base items
std::array<Definition, 3> const Definitions = { {
    // A cloak in the Scarlet Onslaught's red (item 24567): 5-10% at +2, 25-35% at +60
    { 1, 24567, POWER_INQUISITOR_BRAND, 5.0f, 10.0f, 25.0f, 35.0f, Mythic::GetItemLevel(2), CloakBudget, 164 },
    // A ring (item 996): 10-15% of the health at +2, 40-50% at +60
    { 2, 996, POWER_WHITEMANE_OATH, 10.0f, 15.0f, 40.0f, 50.0f, Mythic::GetItemLevel(2), RingBudget, 164 },
    // Gloves in Turalyon's red and gold (item 21428): 5-10% of the attack or spell power at +2, 25-35% at +60
    { 3, 21428, POWER_MOGRAINE_GROUND, 5.0f, 10.0f, 25.0f, 35.0f, Mythic::GetItemLevel(2), GlovesBudget, 164 },
} };

// The armour a player wears: 0 cloth, 1 leather, 2 mail, 3 plate (the heaviest they are trained in)
uint32 ArmorType(Player* player)
{
    if (player->HasSkill(SKILL_PLATE_MAIL))
        return 3;
    if (player->HasSkill(SKILL_MAIL))
        return 2;
    if (player->HasSkill(SKILL_LEATHER))
        return 1;
    return 0;
}

// A legendary drops for each player who completes a key of its source, rarely: this chance, raised by the step for
// every key of that source completed without one, never above the cap (bad luck protection, reset by a drop). Kept
// per character and per source in character_legendary_luck. Average about 1 in 33 keys at 2 / 0.5 / 3.
LiveTuning::Knob const DropBasePct("legendary.drop_base_pct", 2.0f);
LiveTuning::Knob const DropStepPct("legendary.drop_step_pct", 0.5f);
LiveTuning::Knob const DropCapPct("legendary.drop_cap_pct", 3.0f);

std::shared_mutex StoreLock;
std::unordered_map<ObjectGuid::LowType, Copy> Store;

std::string EncodeStats(Copy const& copy)
{
    std::string text;
    for (auto const& [type, value] : copy.stats)
        text += Acore::StringFormat("{}{}={}", text.empty() ? "" : ",", type, value);
    return text;
}

std::vector<std::pair<uint32, int32>> DecodeStats(std::string_view text)
{
    std::vector<std::pair<uint32, int32>> stats;
    for (std::string_view pair : Acore::Tokenize(text, ',', false))
    {
        std::vector<std::string_view> parts = Acore::Tokenize(pair, '=', false);
        if (parts.size() != 2)
            continue;
        Optional<uint32> type = Acore::StringTo<uint32>(parts[0]);
        Optional<int32> value = Acore::StringTo<int32>(parts[1]);
        if (type && value)
            stats.emplace_back(*type, *value);
    }
    return stats;
}

std::optional<Copy> FindCopy(ObjectGuid::LowType guid)
{
    std::shared_lock lock(StoreLock);
    auto const found = Store.find(guid);
    if (found == Store.end())
        return std::nullopt;
    return found->second;
}

void Send(Player* player, std::string const& body)
{
    if (!player->GetSession() || player->GetSession()->IsBot())
        return;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(Prefix) + "\t" + body);
    player->GetSession()->SendPacket(&packet);
}

void SendCopy(Player* player, ObjectGuid::LowType seed, Copy const& copy, Item const* where = nullptr)
{
    Definition const* definition = GetDefinition(copy.legendary);
    if (!definition)
        return;
    auto const [low, high] = PowerWindow(*definition, copy.itemLevel);
    std::string body = Acore::StringFormat("C\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}", seed, copy.legendary,
        copy.itemLevel, std::lround(copy.power * 10.0f), std::lround(low * 10.0f), std::lround(high * 10.0f),
        copy.armor, EncodeStats(copy));
    if (where)
        body += Acore::StringFormat("\t{}:{}", where->GetBagSlot(), where->GetSlot());
    Send(player, body);
}

// The copy's id in the item itself: links carry it, the client asks for the copy by it. Not saved with the item, so
// set again whenever a legendary is loaded (login, equipping, a mail taken).
void Mark(Item* item)
{
    uint32 const seed = item->GetGUID().GetCounter();
    if (item->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED) != seed)
        item->SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, seed);
}

template <typename Visit>
void ForEachItem(Player* player, Visit visit)
{
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < BANK_SLOT_BAG_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;
        visit(item);
        if (Bag* bag = item->ToBag())
            for (uint32 index = 0; index < bag->GetBagSize(); ++index)
                if (Item* inside = bag->GetItemByPos(uint8(index)))
                    visit(inside);
    }
}

// The primary stat a player's gear favours: the one the copy rolls
uint32 FavouredPrimary(Player* player)
{
    std::array<int64, 3> totals = {};       // strength, agility, intellect
    ForEachItem(player, [&totals](Item* item)
    {
        if (!item->IsEquipped())
            return;
        ItemTemplate const* proto = item->GetTemplate();
        auto add = [&totals](uint32 type, int32 value)
        {
            if (type == ITEM_MOD_STRENGTH)
                totals[0] += value;
            else if (type == ITEM_MOD_AGILITY)
                totals[1] += value;
            else if (type == ITEM_MOD_INTELLECT || type == ITEM_MOD_SPELL_POWER)
                totals[2] += value;
        };
        for (uint32 index = 0; index < proto->StatsCount; ++index)
            add(proto->ItemStat[index].ItemStatType, proto->ItemStat[index].ItemStatValue);
        if (std::optional<Copy> copy = GetCopy(item))
            for (auto const& [type, value] : copy->stats)
                add(type, value);
    });
    if (totals[2] > totals[0] && totals[2] > totals[1])
        return ITEM_MOD_INTELLECT;
    return totals[1] > totals[0] ? ITEM_MOD_AGILITY : ITEM_MOD_STRENGTH;
}

int32 Spread(int32 value, float spread)
{
    return std::max(1, int32(std::lround(float(value) * frand(1.0f - spread, 1.0f + spread))));
}

Copy Roll(Definition const& definition, Player* player, uint32 itemLevel, std::optional<float> powerOverride)
{
    Copy copy;
    copy.legendary = definition.id;
    copy.itemLevel = itemLevel;
    auto const [low, high] = PowerWindow(definition, itemLevel);
    copy.power = powerOverride ? *powerOverride : std::round(frand(low, high) * 10.0f) / 10.0f;

    Budget const& budget = definition.budget;
    float const level = float(itemLevel);
    float const statGrowth = ::Power::StatGrowth(budget.itemLevel, level);
    float const ratingGrowth = ::Power::StatGrowth(budget.itemLevel, level, true);
    int32 const armor = budget.armor[ArmorType(player)];
    copy.armor = armor ? Spread(int32(std::lround(float(armor) * statGrowth)), 0.0f) : 0;

    uint32 const primary = FavouredPrimary(player);
    copy.stats.emplace_back(primary, Spread(int32(std::lround(budget.primary * statGrowth)), 0.05f));
    copy.stats.emplace_back(ITEM_MOD_STAMINA, Spread(int32(std::lround(budget.stamina * statGrowth)), 0.05f));
    bool const caster = primary == ITEM_MOD_INTELLECT;
    if (caster)
        copy.stats.emplace_back(ITEM_MOD_SPELL_POWER,
            Spread(int32(std::lround(budget.spellPower * statGrowth)), 0.05f));

    // Two secondaries drawn from the ones that suit the primary
    std::vector<uint32> pool = caster ?
        std::vector<uint32>{ ITEM_MOD_CRIT_RATING, ITEM_MOD_HASTE_RATING, ITEM_MOD_HIT_RATING, ITEM_MOD_SPIRIT } :
        std::vector<uint32>{ ITEM_MOD_CRIT_RATING, ITEM_MOD_HASTE_RATING, ITEM_MOD_HIT_RATING,
                             ITEM_MOD_EXPERTISE_RATING, ITEM_MOD_ARMOR_PENETRATION_RATING };
    for (uint32 draw = 0; draw < 2 && !pool.empty(); ++draw)
    {
        uint32 const index = urand(0, uint32(pool.size() - 1));
        float const growth = pool[index] == ITEM_MOD_SPIRIT ? statGrowth : ratingGrowth;
        copy.stats.emplace_back(pool[index], Spread(int32(std::lround(budget.secondary * growth)), 0.10f));
        pool.erase(pool.begin() + index);
    }
    return copy;
}

void Save(ObjectGuid::LowType guid, ObjectGuid::LowType owner, Copy const& copy)
{
    CharacterDatabase.Execute("REPLACE INTO character_legendary (item_guid, owner_guid, legendary, item_level, power, "
        "armor, stats) VALUES ({}, {}, {}, {}, {}, {}, '{}')", guid, owner, copy.legendary, copy.itemLevel, copy.power,
        copy.armor, EncodeStats(copy));
}

// --- The powers that are worn ----------------------------------------------------------------------------------------

constexpr char const* WornKey = "LegendaryWorn";

struct Worn : public DataMap::Base
{
    // Per equipment slot: the power it gives and its strength (0: none)
    std::array<std::pair<uint32, float>, EQUIPMENT_SLOT_END> slots = {};
    // Consécration de Mograine: time to the next ground in combat, its pulses left and time to the next
    uint32 groundIn = 0;
    uint32 pulsesLeft = 0;
    uint32 pulseIn = 0;

    float Total(Power power) const
    {
        float total = 0.0f;
        for (auto const& [worn, value] : slots)
            if (worn == power)
                total += value;
        return total;
    }
};

Worn* GetWorn(Player* player)
{
    return player->CustomData.GetDefault<Worn>(WornKey);
}

class LegendaryPlayerScript : public PlayerScript
{
public:
    LegendaryPlayerScript() : PlayerScript("LegendaryPlayerScript", {
        PLAYERHOOK_ON_AFTER_APPLY_ITEM_BONUSES,
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE
    }) { }

    // A worn copy's own armour and stats, through the core's own item stat code, and its power
    void OnPlayerAfterApplyItemBonuses(Player* player, Item* item, uint8 slot, bool apply) override
    {
        std::optional<Copy> copy = GetCopy(item);
        if (!copy)
            return;
        Mark(item);
        if (copy->armor)
            player->HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(copy->armor), apply);
        for (auto const& [type, value] : copy->stats)
            player->ApplyItemStatMod(type, value, apply);

        Definition const* definition = GetDefinition(copy->legendary);
        if (definition && slot < EQUIPMENT_SLOT_END)
            GetWorn(player)->slots[slot] = apply ? std::make_pair(uint32(definition->power), copy->power) :
                std::make_pair(0u, 0.0f);
    }

    // Consécration de Mograine: every GroundEveryMs in combat (at once when a fight starts), consecrated ground on the
    // wearer for GroundPulses seconds - its aura, which carries the ground's look - and a pulse every second: what it
    // deals to each enemy and heals each ally around, the rolled share of the wearer's attack or spell power,
    // whichever is higher
    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        Worn* worn = GetWorn(player);
        float const percent = worn->Total(POWER_MOGRAINE_GROUND);
        if (percent <= 0.0f || !player->IsAlive())
        {
            worn->pulsesLeft = 0;
            return;
        }
        if (worn->pulsesLeft)
        {
            worn->pulseIn = worn->pulseIn > diff ? worn->pulseIn - diff : 0;
            if (!worn->pulseIn)
            {
                worn->pulseIn = 1000;
                --worn->pulsesLeft;
                Pulse(player, percent);
            }
        }
        if (!player->IsInCombat())
        {
            worn->groundIn = 0;
            return;
        }
        worn->groundIn = worn->groundIn > diff ? worn->groundIn - diff : 0;
        if (worn->groundIn)
            return;
        worn->groundIn = GroundEveryMs;
        worn->pulsesLeft = GroundPulses;
        worn->pulseIn = 0;
        player->CastSpell(player, SPELL_MOGRAINE_GROUND, true);
    }

    static void Pulse(Player* player, float percent)
    {
        float const power = std::max(player->GetTotalAttackPowerValue(BASE_ATTACK),
            float(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_HOLY)));
        int32 const amount = std::max(1, int32(std::lround(power * percent / 100.0f)));

        std::list<Unit*> enemies;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(player, player, GroundRadius);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(player, enemies, check);
        Cell::VisitObjects(player, searcher, GroundRadius);
        for (Unit* enemy : enemies)
            if (enemy->IsAlive() && player->IsValidAttackTarget(enemy))
                player->CastCustomSpell(enemy, SPELL_MOGRAINE_GROUND_DAMAGE, &amount, nullptr, nullptr, true);

        auto heal = [player, amount](Player* ally)
        {
            if (ally && ally->IsAlive() && ally->IsInMap(player) && ally->IsWithinDistInMap(player, GroundRadius))
                player->CastCustomSpell(ally, SPELL_MOGRAINE_GROUND_HEAL, &amount, nullptr, nullptr, true);
        };
        if (Group* group = player->GetGroup())
        {
            for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
                heal(reference->GetSource());
        }
        else
            heal(player);
    }

    // Every copy the character carries: marked, and its rolls sent for the tooltips. The base items' records first,
    // as the server has them now: the client keeps the ones it saw (its item cache) and showed a socket the base
    // item lost.
    void OnPlayerLogin(Player* player) override
    {
        if (player->GetSession() && !player->GetSession()->IsBot())
            for (Definition const& definition : Definitions)
            {
                WorldPacket query(CMSG_ITEM_QUERY_SINGLE, 4);
                query << definition.baseItem;
                player->GetSession()->HandleItemQuerySingleOpcode(query);
            }
        ForEachItem(player, [player](Item* item)
        {
            if (std::optional<Copy> copy = GetCopy(item))
            {
                Mark(item);
                SendCopy(player, item->GetGUID().GetCounter(), *copy, item);
            }
        });
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& language,
                                       std::string& message) override
    {
        if (language != LANG_ADDON || message.size() <= Prefix.size() || !message.starts_with(Prefix) ||
            message[Prefix.size()] != '\t')
            return;
        std::vector<std::string_view> fields = Acore::Tokenize(std::string_view(message).substr(Prefix.size() + 1),
            '\t', true);
        if (fields.size() == 2 && fields[0] == "Q")
        {
            if (Optional<uint32> seed = Acore::StringTo<uint32>(fields[1]))
                if (std::optional<Copy> copy = FindCopy(*seed))
                    SendCopy(player, *seed, *copy);
        }
        else if (fields.size() == 3 && fields[0] == "W")
        {
            Optional<uint32> bag = Acore::StringTo<uint32>(fields[1]);
            Optional<uint32> slot = Acore::StringTo<uint32>(fields[2]);
            Item* item = bag && slot && *bag <= 255 && *slot <= 255 ?
                player->GetItemByPos(uint8(*bag), uint8(*slot)) : nullptr;
            if (std::optional<Copy> copy = GetCopy(item))
                SendCopy(player, item->GetGUID().GetCounter(), *copy, item);
        }
    }
};

// Marque de l'Inquisiteur: the wearer's direct damage - a swing, a spell's hit, after mitigation, what the combat log
// shows - brands its target, X% of it burning as Holy over 4 sec. The core's Ignite rolls what is left of the burn into
// the new one (Unit::CastDelayedSpellWithPeriodicAmount). Periodic damage never reaches this hook: no loop between
// damage over time effects, and the burn does not feed itself.
class LegendaryUnitScript : public UnitScript
{
public:
    LegendaryUnitScript() : UnitScript("LegendaryUnitScript", true, {
        UNITHOOK_MODIFY_FINAL_DAMAGE,
        UNITHOOK_ON_DAMAGE
    }) { }

    // Serment de Whitemane: a blow that would kill the wearer leaves them at 1 health, and the oath heals them for its
    // rolled share of their health over 4 sec; then it rests for 3 min (its debuff shows how long). Whatever the blow:
    // a hit, a damage over time effect, a fall.
    void OnDamage(Unit* /*attacker*/, Unit* victim, uint32& damage) override
    {
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (!player || !player->IsAlive() || damage < player->GetHealth() ||
            player->HasAura(SPELL_WHITEMANE_OATH_SPENT))
            return;
        float const percent = GetWorn(player)->Total(POWER_WHITEMANE_OATH);
        if (percent <= 0.0f)
            return;
        damage = player->GetHealth() - 1;
        int32 const perTick = std::max(1, int32(std::lround(float(player->GetMaxHealth()) * percent / 100.0f /
            OathTicks)));
        player->CastCustomSpell(player, SPELL_WHITEMANE_OATH_HEAL, &perTick, nullptr, nullptr, true);
        player->CastSpell(player, SPELL_WHITEMANE_OATH_SPENT, true);
    }

    void ModifyFinalDamage(Unit* attacker, Unit* victim, uint32& damage, uint32& /*absorb*/,
                           SpellInfo const* spellInfo) override
    {
        Player* player = attacker ? attacker->ToPlayer() : nullptr;
        if (!player || !victim || victim == attacker || !damage || !victim->IsAlive() ||
            (spellInfo && spellInfo->Id == SPELL_INQUISITOR_BRAND))
            return;
        float const percent = GetWorn(player)->Total(POWER_INQUISITOR_BRAND);
        if (percent <= 0.0f)
            return;
        int32 const perTick = int32(std::lround(float(damage) * percent / 100.0f / BrandTicks));
        if (perTick > 0)
            victim->CastDelayedSpellWithPeriodicAmount(player, SPELL_INQUISITOR_BRAND, SPELL_AURA_PERIODIC_DAMAGE,
                perTick);
    }
};

class LegendaryWorldScript : public WorldScript
{
public:
    LegendaryWorldScript() : WorldScript("LegendaryWorldScript", { WORLDHOOK_ON_STARTUP }) { }

    // Every copy in memory before anyone logs in: a login applies a worn copy's stats as the inventory loads, before
    // any script of the player could read them. A copy whose item is gone is dropped.
    void OnStartup() override
    {
        uint32 const startTime = getMSTime();
        CharacterDatabase.DirectExecute("DELETE l FROM character_legendary l LEFT JOIN item_instance i "
            "ON i.guid = l.item_guid WHERE i.guid IS NULL");
        std::unique_lock lock(StoreLock);
        Store.clear();
        if (QueryResult result = CharacterDatabase.Query(
            "SELECT item_guid, legendary, item_level, power, armor, stats FROM character_legendary"))
        {
            do
            {
                Field* fields = result->Fetch();
                Copy copy;
                copy.legendary = fields[1].Get<uint32>();
                copy.itemLevel = fields[2].Get<uint32>();
                copy.power = fields[3].Get<float>();
                copy.armor = fields[4].Get<int32>();
                copy.stats = DecodeStats(fields[5].Get<std::string>());
                Store[fields[0].Get<uint32>()] = std::move(copy);
            } while (result->NextRow());
        }
        LOG_INFO("server.loading", ">> Loaded {} legendary copies in {} ms", Store.size(),
            GetMSTimeDiffToNow(startTime));
    }
};

// A deleted item takes its copy with it
class LegendaryGlobalScript : public GlobalScript
{
public:
    LegendaryGlobalScript() : GlobalScript("LegendaryGlobalScript", { GLOBALHOOK_ON_ITEM_DEL_FROM_DB }) { }

    void OnItemDelFromDB(CharacterDatabaseTransaction transaction, ObjectGuid::LowType itemGuid) override
    {
        transaction->Append("DELETE FROM character_legendary WHERE item_guid = {}", itemGuid);
        std::unique_lock lock(StoreLock);
        Store.erase(itemGuid);
    }
};

// The legendaries of a source, when a Mythic+ key of it is completed: once per instance, for every real player in it,
// a roll against their luck there; a drop picks one of the source's legendaries and lands with the key's loot on the
// floor (GroundLoot), at the key's item level.
class LegendaryDropScript : public GlobalScript
{
public:
    LegendaryDropScript() : GlobalScript("LegendaryDropScript", { GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE }) { }

    void OnAfterUpdateEncounterState(Map* map, EncounterCreditType /*type*/, uint32 /*creditEntry*/, Unit* /*source*/,
        Difficulty /*difficulty*/, std::list<DungeonEncounter const*> const* /*encounters*/, uint32 dungeonCompleted,
        bool /*updated*/) override
    {
        int32 const level = map->GetMythicLevel();
        if (!dungeonCompleted || level <= 0)
            return;
        std::vector<Definition const*> pool;
        for (Definition const& definition : Definitions)
            if (definition.sourceDungeon == dungeonCompleted)
                pool.push_back(&definition);
        if (pool.empty() || !_rolled.insert(map->GetInstanceId()).second)
            return;

        uint32 const itemLevel = Mythic::GetItemLevel(level);
        map->DoForAllPlayers([&pool, itemLevel, dungeonCompleted](Player* player)
        {
            if (!player->GetSession() || player->GetSession()->IsBot())
                return;
            ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
            uint32 misses = 0;
            if (QueryResult result = CharacterDatabase.Query("SELECT misses FROM character_legendary_luck "
                "WHERE guid = {} AND source = {}", guid, dungeonCompleted))
                misses = result->Fetch()[0].Get<uint32>();
            float const chance = std::min(float(DropCapPct), float(DropBasePct) + float(DropStepPct) * float(misses));
            bool const dropped = frand(0.0f, 100.0f) < chance;
            CharacterDatabase.Execute("REPLACE INTO character_legendary_luck (guid, source, misses) "
                "VALUES ({}, {}, {})", guid, dungeonCompleted, dropped ? 0 : misses + 1);
            LOG_INFO("module", "Legendary: {} completed a key of dungeon {} at {}%: {}", player->GetName(),
                dungeonCompleted, chance, dropped ? "dropped" : "nothing");
            if (!dropped)
                return;

            Definition const* definition = pool[urand(0, uint32(pool.size() - 1))];
            uint32 const id = definition->id;
            ItemTemplate const* base = sObjectMgr->GetItemTemplate(definition->baseItem);
            ObjectGuid const owner = player->GetGUID();
            bool const thrown = base && GroundLoot::Throw(player, base, [owner, id, itemLevel](Item* item)
            {
                if (Player* looter = ObjectAccessor::FindConnectedPlayer(owner))
                    MakeCopy(looter, item, id, itemLevel);
            });
            if (!thrown)
                GiveLegendary(player, id, itemLevel);
        });
    }

private:
    std::set<uint32> _rolled;
};

using namespace Acore::ChatCommands;

class LegendaryCommandScript : public CommandScript
{
public:
    LegendaryCommandScript() : CommandScript("LegendaryCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable legendaryTable =
        {
            { "add", HandleAdd, SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "legendary", legendaryTable },
        };
        return commandTable;
    }

    // .legendary add <legendary> [item level] [power %]: a rolled copy for the selected player (or yourself)
    static bool HandleAdd(ChatHandler* handler, uint32 legendary, Optional<uint32> itemLevel, Optional<float> power)
    {
        Definition const* definition = GetDefinition(legendary);
        if (!definition)
        {
            handler->PSendSysMessage("Légendaire inconnu : {}.", legendary);
            return false;
        }
        Player* target = handler->getSelectedPlayerOrSelf();
        uint32 const level = itemLevel.value_or(definition->floorItemLevel);
        Item* item = GiveLegendary(target, legendary, level, power ? std::optional<float>(*power) : std::nullopt);
        if (!item)
        {
            handler->SendSysMessage("Sacs pleins.");
            return false;
        }
        std::optional<Copy> copy = GetCopy(item);
        auto const [low, high] = PowerWindow(*definition, level);
        handler->PSendSysMessage("{} : niveau d'objet {}, pouvoir {:.1f}% (fenêtre {:.1f}-{:.1f}%), {}.",
            target->GetName(), level, copy ? copy->power : 0.0f, low, high, copy ? EncodeStats(*copy) : "");
        return true;
    }
};
}

Definition const* GetDefinition(uint32 id)
{
    for (Definition const& definition : Definitions)
        if (definition.id == id)
            return &definition;
    return nullptr;
}

Definition const* GetDefinitionByItem(uint32 baseItem)
{
    for (Definition const& definition : Definitions)
        if (definition.baseItem == baseItem)
            return &definition;
    return nullptr;
}

std::optional<Copy> GetCopy(Item const* item)
{
    if (!item || !GetDefinitionByItem(item->GetEntry()))
        return std::nullopt;
    std::optional<Copy> copy = FindCopy(item->GetGUID().GetCounter());
    // A reused guid of another legendary's base item: not this copy
    if (copy && GetDefinition(copy->legendary)->baseItem != item->GetEntry())
        return std::nullopt;
    return copy;
}

// A copy rolled for an item: kept in memory and in the database, the item marked, the client told
static void Keep(Player* player, Item* item, Copy const& copy)
{
    ObjectGuid::LowType const guid = item->GetGUID().GetCounter();
    {
        std::unique_lock lock(StoreLock);
        Store[guid] = copy;
    }
    Save(guid, player->GetGUID().GetCounter(), copy);
    Mark(item);
    SendCopy(player, guid, copy, item->IsInWorld() ? item : nullptr);
}

std::pair<float, float> PowerWindow(Definition const& definition, uint32 itemLevel)
{
    float const span = float(TopItemLevel) - float(definition.floorItemLevel);
    float const t = span > 0.0f ?
        std::clamp((float(itemLevel) - float(definition.floorItemLevel)) / span, 0.0f, 1.0f) : 1.0f;
    return { definition.bottomLow + (definition.topLow - definition.bottomLow) * t,
             definition.bottomHigh + (definition.topHigh - definition.bottomHigh) * t };
}

Item* GiveLegendary(Player* player, uint32 legendary, uint32 itemLevel, std::optional<float> powerOverride)
{
    Definition const* definition = GetDefinition(legendary);
    if (!player || !definition)
        return nullptr;
    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, definition->baseItem, 1) != EQUIP_ERR_OK)
        return nullptr;

    // Rolled before the item exists: its primary stat reads the gear worn now
    Copy copy = Roll(*definition, player, itemLevel, powerOverride);
    Item* item = player->StoreNewItem(destination, definition->baseItem, true);
    if (!item)
        return nullptr;
    Keep(player, item, copy);
    player->SendNewItem(item, 1, true, false);
    return item;
}

void MakeCopy(Player* player, Item* item, uint32 legendary, uint32 itemLevel)
{
    Definition const* definition = GetDefinition(legendary);
    if (!player || !item || !definition || item->GetEntry() != definition->baseItem)
        return;
    Keep(player, item, Roll(*definition, player, itemLevel, std::nullopt));
}
}

void AddLegendaryScripts()
{
    new Legendary::LegendaryPlayerScript();
    new Legendary::LegendaryUnitScript();
    new Legendary::LegendaryWorldScript();
    new Legendary::LegendaryGlobalScript();
    new Legendary::LegendaryDropScript();
    new Legendary::LegendaryCommandScript();
}
