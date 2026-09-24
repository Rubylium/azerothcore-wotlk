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
#include <utility>

namespace CombatTelemetry
{
namespace
{
enum class RunType : uint8
{
    MythicPlus = 1,
    RaidBoss = 2
};

enum class RunResult : uint8
{
    Completed = 1,
    Depleted = 2,
    Abandoned = 3,
    RaidKill = 4
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

std::mutex telemetryMutex;
std::unordered_map<uint64, Run> mythicRuns;
std::unordered_map<uint64, Run> raidEncounters;
std::atomic<uint32> runSequence = 0;

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

void PersistRun(Run&& run)
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

    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
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

    CharacterDatabase.AsyncCommitTransaction(transaction);
    LOG_INFO("combat.telemetry", "Queued combat telemetry run={} type={} result={} map={} instance={} key={} "
        "humans={} bots={} damage={} duration={}s", run.id, static_cast<uint32>(run.type),
        static_cast<uint32>(run.result), run.mapId, run.instanceId, run.keyLevel, humanCount, botCount,
        totalDamage, run.elapsedSeconds);
}

Run* FindActiveRun(uint64 key)
{
    if (auto itr = mythicRuns.find(key); itr != mythicRuns.end())
        return &itr->second;
    if (auto itr = raidEncounters.find(key); itr != raidEncounters.end())
        return &itr->second;
    return nullptr;
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
}
