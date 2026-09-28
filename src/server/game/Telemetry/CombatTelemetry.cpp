#include "CombatTelemetry.h"

#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "SpellInfo.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace CombatTelemetry
{
namespace
{
enum class RunType : uint8
{
    MythicPlus = 1,
    RaidBoss = 2,
    Bench = 3
};

enum class RunResult : uint8
{
    Completed = 1,
    Depleted = 2,
    Abandoned = 3,
    RaidKill = 4,
    BenchCompleted = 5,     // a combat bench test that ran its course
    BenchStopped = 6        // stopped before
};

struct AbilityTotals
{
    uint64 damage = 0;
    uint64 bossDamage = 0;
    uint64 trashDamage = 0;
    uint32 hits = 0;
    uint32 petHits = 0;
    uint8 damageType = 0;
};

struct TargetTotals
{
    uint64 damage = 0;
    uint32 hits = 0;
    bool boss = false;
};

struct Participant
{
    uint32 guid = 0;
    std::string name;
    bool bot = false;
    uint8 classId = 0;
    uint8 raceId = 0;
    uint8 level = 0;
    uint8 roleMask = 0;
    uint32 specId = 0;
    float itemLevel = 0.0f;
    uint32 maxHealth = 0;
    uint32 strength = 0;
    uint32 agility = 0;
    uint32 stamina = 0;
    uint32 intellect = 0;
    uint32 spirit = 0;
    uint32 attackPower = 0;
    int32 spellPower = 0;
    float meleeCrit = 0.0f;
    float spellCrit = 0.0f;
    float meleeHaste = 0.0f;
    float spellHaste = 0.0f;
    uint64 damage = 0;
    uint64 bossDamage = 0;
    uint64 trashDamage = 0;
    uint64 petDamage = 0;
    uint32 hits = 0;
    uint32 deaths = 0;
    uint64 firstDamageMs = 0;
    uint64 lastDamageMs = 0;
    std::unordered_map<uint32, AbilityTotals> abilities;
    std::unordered_map<uint32, TargetTotals> targets;
};

struct RouteEvent
{
    uint64 offsetMs = 0;
    std::string eventType;
    uint32 actorGuid = 0;
    uint32 targetEntry = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::string details;
};

struct Run
{
    RunType type = RunType::MythicPlus;
    uint64 id = 0;
    uint32 mapId = 0;
    uint32 instanceId = 0;
    uint32 dungeonId = 0;
    uint32 difficulty = 0;
    uint32 keyLevel = 0;
    uint32 timeLimitSeconds = 0;
    uint32 bossEntry = 0;
    std::string bossName;
    uint64 startedEpochMs = 0;
    uint64 startedGameMs = 0;
    uint32 elapsedSeconds = 0;
    uint32 timerDeaths = 0;
    RunResult result = RunResult::Abandoned;
    std::unordered_map<uint32, Participant> participants;
    std::vector<RouteEvent> routeEvents;
};

// A combat bench test, beside its Run (the tables every run fills): what only the combat log tells
struct BenchTotals
{
    uint32 casts = 0;
    uint32 hits = 0;
    uint32 crits = 0;
    uint64 amount = 0;
    uint64 overheal = 0;
};

struct BenchTargetTotals
{
    uint32 entry = 0;
    uint64 damage = 0;
    uint32 hits = 0;
};

struct BenchDetail
{
    std::string label;
    uint64 healing = 0;
    uint64 overhealing = 0;
    uint64 damageTaken = 0;
    std::unordered_map<uint64, BenchTotals> spells;     // BenchKey
    std::unordered_map<uint64, uint32> casts;           // BenchKey(Damage, pet, spell)
    std::unordered_map<ObjectGuid, BenchTargetTotals> targets;
};

struct BenchRun
{
    Run run;
    ObjectGuid owner;
    std::unordered_set<ObjectGuid> dummies;
    std::unordered_set<ObjectGuid> bossDummies;
    std::unordered_map<uint32, BenchDetail> details;    // by participant guid counter
    uint64 firstEventMs = 0;
};

std::mutex telemetryMutex;
std::unordered_map<uint64, Run> mythicRuns;
std::unordered_map<uint64, Run> raidEncounters;
std::atomic<uint32> runSequence = 0;

// Combat bench tests by owner, and who and what belongs to which. The log hooks look at activeBenchRuns first.
std::unordered_map<ObjectGuid, BenchRun> benchRuns;
std::unordered_map<uint32, ObjectGuid> benchParticipants;
std::unordered_map<ObjectGuid, ObjectGuid> benchDummies;
std::atomic<uint32> activeBenchRuns = 0;

uint64 GetEpochMilliseconds()
{
    return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

uint64 GetMapKey(uint32 mapId, uint32 instanceId)
{
    return (static_cast<uint64>(mapId) << 32) | instanceId;
}

uint64 CreateRunId()
{
    return (GetEpochMilliseconds() << 12) | (runSequence.fetch_add(1, std::memory_order_relaxed) & 0xFFF);
}

bool IsBoss(Creature const* creature)
{
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}

uint8 GetRoleMask(Player const* player)
{
    Group const* group = player ? player->GetGroup() : nullptr;
    if (!group)
        return 0;

    for (Group::MemberSlot const& slot : group->GetMemberSlots())
        if (slot.guid == player->GetGUID())
            return slot.roles;
    return 0;
}

Participant MakeParticipant(Player* player)
{
    Participant result;
    result.guid = player->GetGUID().GetCounter();
    result.name = player->GetName();
    result.bot = player->GetSession() && player->GetSession()->IsBot();
    result.classId = player->getClass();
    result.raceId = player->getRace();
    result.level = player->GetLevel();
    result.roleMask = GetRoleMask(player);
    result.specId = player->GetSpec();
    result.itemLevel = player->GetAverageItemLevel();
    result.maxHealth = player->GetMaxHealth();
    result.strength = static_cast<uint32>(player->GetStat(STAT_STRENGTH));
    result.agility = static_cast<uint32>(player->GetStat(STAT_AGILITY));
    result.stamina = static_cast<uint32>(player->GetStat(STAT_STAMINA));
    result.intellect = static_cast<uint32>(player->GetStat(STAT_INTELLECT));
    result.spirit = static_cast<uint32>(player->GetStat(STAT_SPIRIT));
    result.attackPower = static_cast<uint32>(std::max(0.0f, player->GetTotalAttackPowerValue(BASE_ATTACK)));
    result.spellPower = std::max(0, player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ALL));
    result.meleeCrit = player->GetRatingBonusValue(CR_CRIT_MELEE);
    result.spellCrit = player->GetRatingBonusValue(CR_CRIT_SPELL);
    result.meleeHaste = player->GetRatingBonusValue(CR_HASTE_MELEE);
    result.spellHaste = player->GetRatingBonusValue(CR_HASTE_SPELL);
    return result;
}

Participant& EnsureParticipant(Run& run, Player* player)
{
    auto [itr, inserted] = run.participants.try_emplace(player->GetGUID().GetCounter());
    if (inserted)
        itr->second = MakeParticipant(player);
    return itr->second;
}

void AddMapParticipants(Run& run, Map* map)
{
    if (!map)
        return;
    map->DoForAllPlayers([&run](Player* player) { EnsureParticipant(run, player); });
}

std::string Escape(std::string value)
{
    CharacterDatabase.EscapeString(value);
    return value;
}

void AppendRun(CharacterDatabaseTransaction& transaction, Run& run)
{
    uint64 const endedEpochMs = GetEpochMilliseconds();
    if (!run.elapsedSeconds)
        run.elapsedSeconds = static_cast<uint32>((GameTime::GetGameTimeMS().count() - run.startedGameMs) / 1000);

    uint32 humanCount = 0;
    uint32 botCount = 0;
    uint64 totalDamage = 0;
    for (auto const& [guid, participant] : run.participants)
    {
        (void)guid;
        participant.bot ? ++botCount : ++humanCount;
        totalDamage += participant.damage;
    }

    transaction->Append(
        "INSERT INTO mod_combat_run (run_id, run_type, result, map_id, instance_id, dungeon_id, difficulty, "
        "key_level, time_limit_seconds, boss_entry, boss_name, started_at_ms, ended_at_ms, duration_seconds, "
        "timer_deaths, human_count, bot_count, total_damage) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
        "'{}', {}, {}, {}, {}, {}, {}, {})",
        run.id, static_cast<uint32>(run.type), static_cast<uint32>(run.result), run.mapId, run.instanceId,
        run.dungeonId, run.difficulty, run.keyLevel, run.timeLimitSeconds, run.bossEntry, Escape(run.bossName),
        run.startedEpochMs, endedEpochMs, run.elapsedSeconds, run.timerDeaths, humanCount, botCount, totalDamage);

    for (auto const& [guid, participant] : run.participants)
    {
        uint64 const activeMs = participant.lastDamageMs >= participant.firstDamageMs && participant.firstDamageMs
            ? participant.lastDamageMs - participant.firstDamageMs : 0;
        transaction->Append(
            "INSERT INTO mod_combat_participant (run_id, guid, name, is_bot, class_id, race_id, level, role_mask, "
            "spec_id, item_level, max_health, strength, agility, stamina, intellect, spirit, attack_power, "
            "spell_power, melee_crit, spell_crit, melee_haste, spell_haste, damage, boss_damage, trash_damage, "
            "pet_damage, hits, deaths, active_ms) VALUES ({}, {}, '{}', {}, {}, {}, {}, {}, {}, {:.2f}, {}, {}, "
            "{}, {}, {}, {}, {}, {}, {:.3f}, {:.3f}, {:.3f}, {:.3f}, {}, {}, {}, {}, {}, {}, {})",
            run.id, guid, Escape(participant.name), participant.bot ? 1 : 0, participant.classId,
            participant.raceId, participant.level, participant.roleMask, participant.specId, participant.itemLevel,
            participant.maxHealth, participant.strength, participant.agility, participant.stamina,
            participant.intellect, participant.spirit, participant.attackPower, participant.spellPower,
            participant.meleeCrit, participant.spellCrit, participant.meleeHaste, participant.spellHaste,
            participant.damage, participant.bossDamage, participant.trashDamage, participant.petDamage,
            participant.hits, participant.deaths, activeMs);

        for (auto const& [spellId, ability] : participant.abilities)
            transaction->Append(
                "INSERT INTO mod_combat_ability (run_id, guid, spell_id, damage_type, damage, boss_damage, "
                "trash_damage, hits, pet_hits) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {})",
                run.id, guid, spellId, ability.damageType, ability.damage, ability.bossDamage,
                ability.trashDamage, ability.hits, ability.petHits);

        for (auto const& [entry, target] : participant.targets)
            transaction->Append(
                "INSERT INTO mod_combat_target (run_id, guid, creature_entry, is_boss, damage, hits) "
                "VALUES ({}, {}, {}, {}, {}, {})",
                run.id, guid, entry, target.boss ? 1 : 0, target.damage, target.hits);
    }

    uint32 sequence = 0;
    for (RouteEvent const& event : run.routeEvents)
        transaction->Append(
            "INSERT INTO mod_combat_route_event (run_id, sequence, offset_ms, event_type, actor_guid, "
            "target_entry, position_x, position_y, position_z, details) VALUES ({}, {}, {}, '{}', {}, {}, "
            "{:.3f}, {:.3f}, {:.3f}, '{}')",
            run.id, sequence++, event.offsetMs, Escape(event.eventType), event.actorGuid, event.targetEntry,
            event.x, event.y, event.z, Escape(event.details));

    LOG_INFO("combat.telemetry", "Queued combat telemetry run={} type={} result={} map={} instance={} key={} "
        "humans={} bots={} damage={} duration={}s", run.id, static_cast<uint32>(run.type),
        static_cast<uint32>(run.result), run.mapId, run.instanceId, run.keyLevel, humanCount, botCount,
        totalDamage, run.elapsedSeconds);
}

void PersistRun(Run&& run)
{
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    AppendRun(transaction, run);
    CharacterDatabase.AsyncCommitTransaction(transaction);
}

Run* FindActiveRun(uint64 key)
{
    if (auto itr = mythicRuns.find(key); itr != mythicRuns.end())
        return &itr->second;
    if (auto itr = raidEncounters.find(key); itr != raidEncounters.end())
        return &itr->second;
    return nullptr;
}

uint64 BenchKey(BenchKind kind, bool pet, uint32 spellId)
{
    return (static_cast<uint64>(kind) << 40) | (static_cast<uint64>(pet ? 1 : 0) << 32) | spellId;
}

// The bench test a participant belongs to (the lock held)
BenchRun* FindBenchRun(Player const* player)
{
    auto const participant = benchParticipants.find(player->GetGUID().GetCounter());
    if (participant == benchParticipants.end())
        return nullptr;
    auto const bench = benchRuns.find(participant->second);
    return bench != benchRuns.end() ? &bench->second : nullptr;
}

BenchDetail& JoinBench(BenchRun& bench, Player* player, std::string_view label = {})
{
    EnsureParticipant(bench.run, player);
    BenchDetail& detail = bench.details[player->GetGUID().GetCounter()];
    if (!label.empty())
        detail.label = label;
    benchParticipants[player->GetGUID().GetCounter()] = bench.owner;
    return detail;
}

// Forgets a bench test and everything pointing to it, handing it to out when given (the lock held)
bool TakeBench(ObjectGuid owner, BenchRun* out = nullptr)
{
    auto const itr = benchRuns.find(owner);
    if (itr == benchRuns.end())
        return false;

    for (auto const& [guid, detail] : itr->second.details)
    {
        (void)detail;
        if (auto const participant = benchParticipants.find(guid);
            participant != benchParticipants.end() && participant->second == owner)
            benchParticipants.erase(participant);
    }
    for (ObjectGuid const& dummy : itr->second.dummies)
        if (auto const known = benchDummies.find(dummy); known != benchDummies.end() && known->second == owner)
            benchDummies.erase(known);
    if (out)
        *out = std::move(itr->second);
    benchRuns.erase(itr);
    activeBenchRuns.fetch_sub(1, std::memory_order_relaxed);
    return true;
}

void StartBenchClock(BenchRun& bench)
{
    if (!bench.firstEventMs)
        bench.firstEventMs = GameTime::GetGameTimeMS().count();
}

void AddBenchSpell(BenchDetail& detail, BenchKind kind, bool pet, uint32 spellId, uint64 amount, uint64 overheal,
    bool crit)
{
    BenchTotals& totals = detail.spells[BenchKey(kind, pet, spellId)];
    ++totals.hits;
    totals.crits += crit ? 1 : 0;
    totals.amount += amount;
    totals.overheal += overheal;
}

uint32 GetBenchDurationMs(BenchRun const& bench, bool completed)
{
    if (!bench.firstEventMs)
        return 0;
    uint64 duration = GameTime::GetGameTimeMS().count() - bench.firstEventMs;
    // A timed test ends on the world update after its time: not a few milliseconds later on paper
    if (completed && bench.run.timeLimitSeconds)
        duration = std::min<uint64>(duration, static_cast<uint64>(bench.run.timeLimitSeconds) * IN_MILLISECONDS);
    return static_cast<uint32>(std::max<uint64>(duration, 1));
}

// The casts go with the spell's damage, else its healing, else on their own (a buff, a cooldown)
void MergeBenchCasts(BenchDetail& detail)
{
    for (auto const& [key, casts] : detail.casts)
    {
        uint64 const healingKey = (key & ((uint64(1) << 40) - 1)) | (static_cast<uint64>(BenchKind::Healing) << 40);
        if (auto const damage = detail.spells.find(key); damage != detail.spells.end())
            damage->second.casts += casts;
        else if (auto const healing = detail.spells.find(healingKey); healing != detail.spells.end())
            healing->second.casts += casts;
        else
            detail.spells[key].casts += casts;
    }
    detail.casts.clear();
}

BenchResult BuildBenchResult(BenchRun const& bench, uint32 durationMs)
{
    BenchResult result;
    result.runId = bench.run.id;
    result.durationMs = durationMs;
    for (auto const& [guid, participant] : bench.run.participants)
    {
        BenchParticipant entry;
        entry.guid = ObjectGuid::Create<HighGuid::Player>(guid);
        entry.name = participant.name;
        entry.bot = participant.bot;
        entry.classId = participant.classId;
        entry.itemLevel = participant.itemLevel;
        entry.damage = participant.damage;
        entry.petDamage = participant.petDamage;
        entry.deaths = participant.deaths;
        if (auto const detail = bench.details.find(guid); detail != bench.details.end())
        {
            entry.label = detail->second.label;
            entry.healing = detail->second.healing;
            entry.overhealing = detail->second.overhealing;
            entry.damageTaken = detail->second.damageTaken;
            for (auto const& [key, totals] : detail->second.spells)
            {
                BenchSpell spell;
                spell.spellId = static_cast<uint32>(key & 0xFFFFFFFF);
                spell.pet = ((key >> 32) & 1) != 0;
                spell.kind = static_cast<BenchKind>(key >> 40);
                spell.casts = totals.casts;
                spell.hits = totals.hits;
                spell.crits = totals.crits;
                spell.amount = totals.amount;
                spell.overheal = totals.overheal;
                entry.spells.push_back(spell);
            }
            for (auto const& [target, totals] : detail->second.targets)
                entry.targets.push_back({ target, totals.entry, totals.damage, totals.hits });
        }
        std::sort(entry.spells.begin(), entry.spells.end(), [](BenchSpell const& left, BenchSpell const& right)
        {
            return left.amount != right.amount ? left.amount > right.amount : left.casts > right.casts;
        });
        result.participants.push_back(std::move(entry));
    }
    std::sort(result.participants.begin(), result.participants.end(),
        [](BenchParticipant const& left, BenchParticipant const& right)
    {
        return left.damage != right.damage ? left.damage > right.damage : left.healing > right.healing;
    });
    return result;
}

void AppendBench(CharacterDatabaseTransaction& transaction, BenchRun const& bench)
{
    for (auto const& [guid, detail] : bench.details)
    {
        transaction->Append(
            "INSERT INTO mod_combat_bench_participant (run_id, guid, label, healing, overhealing, damage_taken) "
            "VALUES ({}, {}, '{}', {}, {}, {})",
            bench.run.id, guid, Escape(detail.label), detail.healing, detail.overhealing, detail.damageTaken);
        for (auto const& [key, totals] : detail.spells)
            transaction->Append(
                "INSERT INTO mod_combat_bench_spell (run_id, guid, kind, spell_id, from_pet, casts, hits, crits, "
                "amount, overheal) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {})",
                bench.run.id, guid, static_cast<uint32>(key >> 40), static_cast<uint32>(key & 0xFFFFFFFF),
                static_cast<uint32>((key >> 32) & 1), totals.casts, totals.hits, totals.crits, totals.amount,
                totals.overheal);
    }
}
}

void InitializeDatabase()
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_run ("
        "run_id BIGINT UNSIGNED NOT NULL, run_type TINYINT UNSIGNED NOT NULL, result TINYINT UNSIGNED NOT NULL, "
        "map_id SMALLINT UNSIGNED NOT NULL, instance_id INT UNSIGNED NOT NULL, dungeon_id INT UNSIGNED NOT NULL "
        "DEFAULT 0, difficulty TINYINT UNSIGNED NOT NULL DEFAULT 0, key_level SMALLINT UNSIGNED NOT NULL DEFAULT 0, "
        "time_limit_seconds INT UNSIGNED NOT NULL DEFAULT 0, boss_entry INT UNSIGNED NOT NULL DEFAULT 0, "
        "boss_name VARCHAR(120) NOT NULL DEFAULT '', started_at_ms BIGINT UNSIGNED NOT NULL, ended_at_ms BIGINT "
        "UNSIGNED NOT NULL, duration_seconds INT UNSIGNED NOT NULL, timer_deaths INT UNSIGNED NOT NULL DEFAULT 0, "
        "human_count SMALLINT UNSIGNED NOT NULL DEFAULT 0, bot_count SMALLINT UNSIGNED NOT NULL DEFAULT 0, "
        "total_damage BIGINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (run_id), KEY idx_type_time "
        "(run_type, ended_at_ms), KEY idx_mythic (dungeon_id, key_level), KEY idx_raid (map_id, boss_entry)) "
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_participant ("
        "run_id BIGINT UNSIGNED NOT NULL, guid INT UNSIGNED NOT NULL, name VARCHAR(32) NOT NULL, is_bot TINYINT "
        "UNSIGNED NOT NULL, class_id TINYINT UNSIGNED NOT NULL, race_id TINYINT UNSIGNED NOT NULL, level TINYINT "
        "UNSIGNED NOT NULL, role_mask TINYINT UNSIGNED NOT NULL DEFAULT 0, spec_id INT UNSIGNED NOT NULL DEFAULT 0, "
        "item_level DECIMAL(7,2) NOT NULL DEFAULT 0, max_health INT UNSIGNED NOT NULL DEFAULT 0, strength INT UNSIGNED "
        "NOT NULL DEFAULT 0, agility INT UNSIGNED NOT NULL DEFAULT 0, stamina INT UNSIGNED NOT NULL DEFAULT 0, "
        "intellect INT UNSIGNED NOT NULL DEFAULT 0, spirit INT UNSIGNED NOT NULL DEFAULT 0, attack_power INT UNSIGNED "
        "NOT NULL DEFAULT 0, spell_power INT NOT NULL DEFAULT 0, melee_crit FLOAT NOT NULL DEFAULT 0, spell_crit "
        "FLOAT NOT NULL DEFAULT 0, melee_haste FLOAT NOT NULL DEFAULT 0, spell_haste FLOAT NOT NULL DEFAULT 0, "
        "damage BIGINT UNSIGNED NOT NULL DEFAULT 0, boss_damage BIGINT UNSIGNED NOT NULL DEFAULT 0, trash_damage "
        "BIGINT UNSIGNED NOT NULL DEFAULT 0, pet_damage BIGINT UNSIGNED NOT NULL DEFAULT 0, hits INT UNSIGNED NOT "
        "NULL DEFAULT 0, deaths INT UNSIGNED NOT NULL DEFAULT 0, active_ms BIGINT UNSIGNED NOT NULL DEFAULT 0, "
        "PRIMARY KEY (run_id, guid), KEY idx_class_spec (class_id, spec_id), KEY idx_bot (is_bot)) "
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_ability ("
        "run_id BIGINT UNSIGNED NOT NULL, guid INT UNSIGNED NOT NULL, spell_id INT UNSIGNED NOT NULL, damage_type "
        "TINYINT UNSIGNED NOT NULL DEFAULT 0, damage BIGINT UNSIGNED NOT NULL DEFAULT 0, boss_damage BIGINT UNSIGNED "
        "NOT NULL DEFAULT 0, trash_damage BIGINT UNSIGNED NOT NULL DEFAULT 0, hits INT UNSIGNED NOT NULL DEFAULT 0, "
        "pet_hits INT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (run_id, guid, spell_id), KEY idx_spell (spell_id)) "
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_target ("
        "run_id BIGINT UNSIGNED NOT NULL, guid INT UNSIGNED NOT NULL, creature_entry INT UNSIGNED NOT NULL, is_boss "
        "TINYINT UNSIGNED NOT NULL DEFAULT 0, damage BIGINT UNSIGNED NOT NULL DEFAULT 0, hits INT UNSIGNED NOT NULL "
        "DEFAULT 0, PRIMARY KEY (run_id, guid, creature_entry), KEY idx_target (creature_entry)) ENGINE=InnoDB "
        "DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_route_event ("
        "run_id BIGINT UNSIGNED NOT NULL, sequence INT UNSIGNED NOT NULL, offset_ms BIGINT UNSIGNED NOT NULL, "
        "event_type VARCHAR(32) NOT NULL, actor_guid INT UNSIGNED NOT NULL DEFAULT 0, target_entry INT UNSIGNED "
        "NOT NULL DEFAULT 0, position_x FLOAT NOT NULL DEFAULT 0, position_y FLOAT NOT NULL DEFAULT 0, "
        "position_z FLOAT NOT NULL DEFAULT 0, details VARCHAR(255) NOT NULL DEFAULT '', PRIMARY KEY (run_id, "
        "sequence), KEY idx_event_type (event_type)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_bench_participant ("
        "run_id BIGINT UNSIGNED NOT NULL, guid INT UNSIGNED NOT NULL, label VARCHAR(64) NOT NULL DEFAULT '', "
        "healing BIGINT UNSIGNED NOT NULL DEFAULT 0, overhealing BIGINT UNSIGNED NOT NULL DEFAULT 0, damage_taken "
        "BIGINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (run_id, guid)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 "
        "COLLATE=utf8mb4_unicode_ci");
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_combat_bench_spell ("
        "run_id BIGINT UNSIGNED NOT NULL, guid INT UNSIGNED NOT NULL, kind TINYINT UNSIGNED NOT NULL, spell_id INT "
        "UNSIGNED NOT NULL, from_pet TINYINT UNSIGNED NOT NULL DEFAULT 0, casts INT UNSIGNED NOT NULL DEFAULT 0, "
        "hits INT UNSIGNED NOT NULL DEFAULT 0, crits INT UNSIGNED NOT NULL DEFAULT 0, amount BIGINT UNSIGNED NOT "
        "NULL DEFAULT 0, overheal BIGINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY (run_id, guid, kind, spell_id, "
        "from_pet), KEY idx_spell (spell_id)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    LOG_INFO("server.loading", ">> Combat telemetry database ready");
}

void StartMythicRun(Map* map, Group* group, uint32 dungeonId, uint32 keyLevel, uint32 timeLimitSeconds)
{
    if (!map)
        return;

    Run run;
    run.type = RunType::MythicPlus;
    run.id = CreateRunId();
    run.mapId = map->GetId();
    run.instanceId = map->GetInstanceId();
    run.dungeonId = dungeonId;
    run.difficulty = static_cast<uint32>(map->GetDifficulty());
    run.keyLevel = keyLevel;
    run.timeLimitSeconds = timeLimitSeconds;
    run.startedEpochMs = GetEpochMilliseconds();
    run.startedGameMs = GameTime::GetGameTimeMS().count();
    if (group)
        for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
            if (Player* player = reference->GetSource())
                EnsureParticipant(run, player);
    AddMapParticipants(run, map);

    std::lock_guard lock(telemetryMutex);
    mythicRuns[GetMapKey(run.mapId, run.instanceId)] = std::move(run);
}

void FinishMythicRun(Map* map, uint32 elapsedSeconds, bool timed, uint32 timerDeaths)
{
    if (!map)
        return;

    Run finished;
    {
        std::lock_guard lock(telemetryMutex);
        auto itr = mythicRuns.find(GetMapKey(map->GetId(), map->GetInstanceId()));
        if (itr == mythicRuns.end())
            return;
        AddMapParticipants(itr->second, map);
        itr->second.elapsedSeconds = elapsedSeconds;
        itr->second.timerDeaths = timerDeaths;
        itr->second.result = timed ? RunResult::Completed : RunResult::Depleted;
        finished = std::move(itr->second);
        mythicRuns.erase(itr);
    }
    PersistRun(std::move(finished));
}

void AbandonMythicRun(uint32 mapId, uint32 instanceId)
{
    Run abandoned;
    {
        std::lock_guard lock(telemetryMutex);
        auto itr = mythicRuns.find(GetMapKey(mapId, instanceId));
        if (itr == mythicRuns.end())
            return;
        itr->second.result = RunResult::Abandoned;
        abandoned = std::move(itr->second);
        mythicRuns.erase(itr);
    }
    PersistRun(std::move(abandoned));
}

void RecordDamage(Unit* attacker, Unit* victim, uint32 damage, SpellInfo const* spellInfo,
    DamageEffectType damageType)
{
    if (!attacker || !victim || !damage || !victim->IsCreature())
        return;

    Player* player = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    Creature* creature = victim->ToCreature();
    Map* map = victim->GetMap();
    if (!player || !creature || !map || creature->IsControlledByPlayer())
        return;
    if (map->GetMythicLevel() <= 0 && !map->IsRaid())
        return;

    uint64 const key = GetMapKey(map->GetId(), map->GetInstanceId());
    std::lock_guard lock(telemetryMutex);
    Run* run = FindActiveRun(key);
    if (!run && map->IsRaid() && IsBoss(creature))
    {
        Run raid;
        raid.type = RunType::RaidBoss;
        raid.id = CreateRunId();
        raid.mapId = map->GetId();
        raid.instanceId = map->GetInstanceId();
        raid.difficulty = static_cast<uint32>(map->GetDifficulty());
        raid.bossEntry = creature->GetEntry();
        raid.bossName = creature->GetName();
        raid.startedEpochMs = GetEpochMilliseconds();
        raid.startedGameMs = GameTime::GetGameTimeMS().count();
        AddMapParticipants(raid, map);
        run = &raidEncounters.emplace(key, std::move(raid)).first->second;
    }
    if (!run)
        return;

    Participant& participant = EnsureParticipant(*run, player);
    bool const boss = IsBoss(creature);
    bool const petDamage = attacker != player;
    uint64 const now = GameTime::GetGameTimeMS().count();
    if (!participant.firstDamageMs)
        participant.firstDamageMs = now;
    participant.lastDamageMs = now;
    participant.damage += damage;
    participant.bossDamage += boss ? damage : 0;
    participant.trashDamage += boss ? 0 : damage;
    participant.petDamage += petDamage ? damage : 0;
    ++participant.hits;

    uint32 const spellId = spellInfo ? spellInfo->Id : 0;
    AbilityTotals& ability = participant.abilities[spellId];
    ability.damage += damage;
    ability.bossDamage += boss ? damage : 0;
    ability.trashDamage += boss ? 0 : damage;
    ++ability.hits;
    ability.petHits += petDamage ? 1 : 0;
    ability.damageType = static_cast<uint8>(damageType);

    TargetTotals& target = participant.targets[creature->GetEntry()];
    target.damage += damage;
    ++target.hits;
    target.boss = target.boss || boss;
}

void RecordPlayerDeath(Unit* unit)
{
    Player* player = unit ? unit->ToPlayer() : nullptr;
    Map* map = player ? player->GetMap() : nullptr;
    if (!player || !map)
        return;

    std::lock_guard lock(telemetryMutex);
    if (Run* run = FindActiveRun(GetMapKey(map->GetId(), map->GetInstanceId())))
        ++EnsureParticipant(*run, player).deaths;
    if (activeBenchRuns.load(std::memory_order_relaxed))
        if (BenchRun* bench = FindBenchRun(player))
            ++EnsureParticipant(bench->run, player).deaths;
}

void RecordMythicRouteEvent(Map* map, std::string_view eventType, Unit const* actor, Unit const* target,
    std::string_view details)
{
    if (!map || map->GetMythicLevel() <= 0 || eventType.empty())
        return;

    std::lock_guard lock(telemetryMutex);
    auto itr = mythicRuns.find(GetMapKey(map->GetId(), map->GetInstanceId()));
    if (itr == mythicRuns.end())
        return;

    RouteEvent event;
    event.offsetMs = GameTime::GetGameTimeMS().count() - itr->second.startedGameMs;
    event.eventType = eventType;
    event.actorGuid = actor && actor->GetCharmerOrOwnerPlayerOrPlayerItself()
        ? actor->GetCharmerOrOwnerPlayerOrPlayerItself()->GetGUID().GetCounter() : 0;
    event.targetEntry = target && target->IsCreature() ? target->ToCreature()->GetEntry() : 0;
    Unit const* position = actor ? actor : target;
    if (position)
    {
        event.x = position->GetPositionX();
        event.y = position->GetPositionY();
        event.z = position->GetPositionZ();
    }
    event.details = details;
    itr->second.routeEvents.push_back(std::move(event));
}

void FinishRaidEncounter(Map* map, Unit* source, bool updated)
{
    Creature* boss = source ? source->ToCreature() : nullptr;
    if (!map || !map->IsRaid() || !updated || !IsBoss(boss))
        return;

    Run killed;
    {
        std::lock_guard lock(telemetryMutex);
        auto itr = raidEncounters.find(GetMapKey(map->GetId(), map->GetInstanceId()));
        if (itr == raidEncounters.end())
            return;
        AddMapParticipants(itr->second, map);
        itr->second.result = RunResult::RaidKill;
        itr->second.bossEntry = boss->GetEntry();
        itr->second.bossName = boss->GetName();
        killed = std::move(itr->second);
        raidEncounters.erase(itr);
    }
    PersistRun(std::move(killed));
}

void AbortRaidEncounter(Unit* unit)
{
    Creature* boss = unit ? unit->ToCreature() : nullptr;
    Map* map = boss ? boss->GetMap() : nullptr;
    if (!map || !map->IsRaid() || !boss->IsAlive() || !IsBoss(boss))
        return;

    std::lock_guard lock(telemetryMutex);
    auto itr = raidEncounters.find(GetMapKey(map->GetId(), map->GetInstanceId()));
    if (itr != raidEncounters.end() && itr->second.bossEntry == boss->GetEntry())
        raidEncounters.erase(itr);
}

void StartBench(BenchSetup const& setup)
{
    if (!setup.map || !setup.owner)
        return;

    std::lock_guard lock(telemetryMutex);
    TakeBench(setup.owner);

    BenchRun& bench = benchRuns[setup.owner];
    activeBenchRuns.fetch_add(1, std::memory_order_relaxed);
    bench.owner = setup.owner;
    Run& run = bench.run;
    run.type = RunType::Bench;
    run.id = CreateRunId();
    run.mapId = setup.map->GetId();
    run.instanceId = setup.map->GetInstanceId();
    run.dungeonId = setup.layout;
    run.difficulty = setup.scaling;
    run.keyLevel = setup.level;
    run.timeLimitSeconds = setup.limitSeconds;
    run.bossName = setup.label;
    run.startedEpochMs = GetEpochMilliseconds();
    run.startedGameMs = GameTime::GetGameTimeMS().count();
    for (ObjectGuid const& dummy : setup.dummies)
    {
        bench.dummies.insert(dummy);
        benchDummies[dummy] = setup.owner;
    }
    for (ObjectGuid const& boss : setup.bossDummies)
        bench.bossDummies.insert(boss);
    // Someone taking part in another owner's test leaves it for this one
    for (auto const& [player, label] : setup.participants)
        if (player)
            JoinBench(bench, player, label);
}

void AddBenchParticipant(ObjectGuid owner, Player* player, std::string_view label)
{
    if (!player)
        return;

    std::lock_guard lock(telemetryMutex);
    if (auto const itr = benchRuns.find(owner); itr != benchRuns.end())
        JoinBench(itr->second, player, label);
}

bool IsBenchRunning(ObjectGuid owner)
{
    std::lock_guard lock(telemetryMutex);
    return benchRuns.contains(owner);
}

uint32 GetBenchElapsedMs(ObjectGuid owner)
{
    std::lock_guard lock(telemetryMutex);
    auto const itr = benchRuns.find(owner);
    if (itr == benchRuns.end() || !itr->second.firstEventMs)
        return 0;
    return static_cast<uint32>(std::max<uint64>(GameTime::GetGameTimeMS().count() - itr->second.firstEventMs, 1));
}

bool FinishBench(ObjectGuid owner, bool completed, bool discard, BenchResult& result)
{
    BenchRun finished;
    {
        std::lock_guard lock(telemetryMutex);
        if (!TakeBench(owner, &finished))
            return false;
    }
    if (!finished.firstEventMs)
        return false;

    uint32 const durationMs = GetBenchDurationMs(finished, completed);
    for (auto& [guid, detail] : finished.details)
    {
        (void)guid;
        MergeBenchCasts(detail);
    }
    result = BuildBenchResult(finished, durationMs);
    if (discard)
        return true;

    finished.run.elapsedSeconds = std::max<uint32>(1, (durationMs + 500) / IN_MILLISECONDS);
    finished.run.result = completed ? RunResult::BenchCompleted : RunResult::BenchStopped;
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    AppendRun(transaction, finished.run);
    AppendBench(transaction, finished);
    CharacterDatabase.AsyncCommitTransaction(transaction);
    return true;
}

void RecordLogDamage(Unit* attacker, Unit* victim, SpellInfo const* spellInfo, uint32 damage, uint32 overkill,
    bool crit, DamageEffectType damageType)
{
    if (!activeBenchRuns.load(std::memory_order_relaxed) || !attacker || !victim || !damage)
        return;

    // Health actually taken: the overkill of a killing blow is not damage anyone could use
    uint32 const amount = damage > overkill ? damage - overkill : 0;
    uint32 const spellId = spellInfo ? spellInfo->Id : 0;
    Player* dealer = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
    Player* taker = victim->ToPlayer();

    std::lock_guard lock(telemetryMutex);
    if (dealer && victim->IsCreature())
    {
        auto const dummy = benchDummies.find(victim->GetGUID());
        auto const itr = dummy != benchDummies.end() ? benchRuns.find(dummy->second) : benchRuns.end();
        if (itr != benchRuns.end())
        {
            BenchRun& bench = itr->second;
            // Whoever hits a dummy of a test takes part in it
            BenchDetail& detail = FindBenchRun(dealer) == &bench ? bench.details[dealer->GetGUID().GetCounter()] :
                JoinBench(bench, dealer);
            Participant& participant = EnsureParticipant(bench.run, dealer);
            StartBenchClock(bench);

            bool const pet = attacker != dealer;
            bool const boss = bench.bossDummies.contains(victim->GetGUID());
            uint64 const now = GameTime::GetGameTimeMS().count();
            if (!participant.firstDamageMs)
                participant.firstDamageMs = now;
            participant.lastDamageMs = now;
            participant.damage += amount;
            participant.bossDamage += boss ? amount : 0;
            participant.trashDamage += boss ? 0 : amount;
            participant.petDamage += pet ? amount : 0;
            ++participant.hits;

            AbilityTotals& ability = participant.abilities[spellId];
            ability.damage += amount;
            ability.bossDamage += boss ? amount : 0;
            ability.trashDamage += boss ? 0 : amount;
            ++ability.hits;
            ability.petHits += pet ? 1 : 0;
            ability.damageType = static_cast<uint8>(damageType);

            TargetTotals& target = participant.targets[victim->GetEntry()];
            target.damage += amount;
            ++target.hits;
            target.boss = target.boss || boss;

            AddBenchSpell(detail, BenchKind::Damage, pet, spellId, amount, 0, crit);
            BenchTargetTotals& dummyTotals = detail.targets[victim->GetGUID()];
            dummyTotals.entry = victim->GetEntry();
            dummyTotals.damage += amount;
            ++dummyTotals.hits;
        }
    }

    if (taker)
        if (BenchRun* bench = FindBenchRun(taker))
        {
            StartBenchClock(*bench);
            BenchDetail& detail = bench->details[taker->GetGUID().GetCounter()];
            detail.damageTaken += amount;
            AddBenchSpell(detail, BenchKind::DamageTaken, false, spellId, amount, 0, crit);
        }
}

void RecordLogHeal(Unit* healer, Unit* target, SpellInfo const* spellInfo, uint32 heal, uint32 overheal, bool crit)
{
    if (!activeBenchRuns.load(std::memory_order_relaxed) || !healer || !target || !heal)
        return;

    Player* dealer = healer->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!dealer)
        return;

    std::lock_guard lock(telemetryMutex);
    BenchRun* bench = FindBenchRun(dealer);
    if (!bench)
        return;

    overheal = std::min(overheal, heal);
    BenchDetail& detail = bench->details[dealer->GetGUID().GetCounter()];
    detail.healing += heal - overheal;
    detail.overhealing += overheal;
    AddBenchSpell(detail, BenchKind::Healing, healer != dealer, spellInfo ? spellInfo->Id : 0, heal - overheal,
        overheal, crit);
}

void RecordSpellCast(Unit* caster, SpellInfo const* spellInfo)
{
    if (!activeBenchRuns.load(std::memory_order_relaxed) || !caster || !spellInfo)
        return;

    Player* dealer = caster->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!dealer)
        return;

    std::lock_guard lock(telemetryMutex);
    if (BenchRun* bench = FindBenchRun(dealer))
        ++bench->details[dealer->GetGUID().GetCounter()].casts[BenchKey(BenchKind::Damage, caster != dealer,
            spellInfo->Id)];
}
}
