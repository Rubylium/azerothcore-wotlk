/*
 * Combat telemetry for balance analysis. Hits are aggregated in memory and
 * persisted only when a Mythic+ run, a raid boss kill or a combat bench test finishes.
 */

#ifndef COMBAT_TELEMETRY_H
#define COMBAT_TELEMETRY_H

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <string>
#include <string_view>
#include <vector>

class Group;
class Map;
class Player;
class SpellInfo;
class Unit;

namespace CombatTelemetry
{
    AC_GAME_API void InitializeDatabase();

    AC_GAME_API void StartMythicRun(Map* map, Group* group, uint32 dungeonId, uint32 keyLevel,
        uint32 timeLimitSeconds);
    AC_GAME_API void FinishMythicRun(Map* map, uint32 elapsedSeconds, bool timed, uint32 timerDeaths);
    AC_GAME_API void AbandonMythicRun(uint32 mapId, uint32 instanceId);

    AC_GAME_API void RecordDamage(Unit* attacker, Unit* victim, uint32 damage, SpellInfo const* spellInfo,
        DamageEffectType damageType);
    AC_GAME_API void RecordPlayerDeath(Unit* player);
    AC_GAME_API void RecordMythicRouteEvent(Map* map, std::string_view eventType, Unit const* actor = nullptr,
        Unit const* target = nullptr, std::string_view details = {});

    AC_GAME_API void FinishRaidEncounter(Map* map, Unit* source, bool updated);
    AC_GAME_API void AbortRaidEncounter(Unit* boss);

    // Combat bench (the Terrain d'essai, mod-playerbots Script/CombatBench.cpp): a test on target dummies, owned by
    // the player who set it up. Unlike the runs above it reads the combat log itself - every damage, heal and cast
    // of its participants, their pets and guardians counted as their owner's - so it knows crits, overhealing, casts
    // and the damage they take. Its clock starts on the first damage a participant deals or takes. Nothing is
    // recorded while no test runs. Persisted as run_type 3 (mod_combat_run and the tables beside it, plus
    // mod_combat_bench_participant and mod_combat_bench_spell).
    enum class BenchKind : uint8
    {
        Damage      = 1,    // done to the test's dummies
        Healing     = 2,    // done to anyone (effective healing; overhealing apart)
        DamageTaken = 3     // from anything
    };

    struct BenchSpell
    {
        uint32 spellId = 0;     // 0: melee swings
        BenchKind kind = BenchKind::Damage;
        bool pet = false;       // by the participant's pet, guardian or totem
        uint32 casts = 0;
        uint32 hits = 0;
        uint32 crits = 0;
        uint64 amount = 0;
        uint64 overheal = 0;
    };

    struct BenchTarget
    {
        ObjectGuid guid;
        uint32 entry = 0;
        uint64 damage = 0;
        uint32 hits = 0;
    };

    struct BenchParticipant
    {
        ObjectGuid guid;
        std::string name;
        std::string label;      // what the bench set it up as (a bot's spec and talent preset)
        bool bot = false;
        uint8 classId = 0;
        float itemLevel = 0.0f;
        uint64 damage = 0;
        uint64 petDamage = 0;
        uint64 healing = 0;
        uint64 overhealing = 0;
        uint64 damageTaken = 0;
        uint32 deaths = 0;
        std::vector<BenchSpell> spells;     // largest amount first
        std::vector<BenchTarget> targets;
    };

    struct BenchResult
    {
        uint64 runId = 0;
        uint32 durationMs = 0;
        std::vector<BenchParticipant> participants;     // most damage first
    };

    struct BenchSetup
    {
        ObjectGuid owner;
        Map* map = nullptr;
        uint32 layout = 0;              // mod_combat_run.dungeon_id
        uint8 scaling = 0;              // mod_combat_run.difficulty
        uint32 level = 0;               // mod_combat_run.key_level: the key, or the Défi tier
        uint32 limitSeconds = 0;        // mod_combat_run.time_limit_seconds, 0 for a test run to a health share
        std::string label;              // mod_combat_run.boss_name
        std::vector<ObjectGuid> dummies;
        std::vector<ObjectGuid> bossDummies;
        std::vector<std::pair<Player*, std::string>> participants;     // and their labels
    };

    // A new test, replacing the owner's previous one (dropped unsaved)
    AC_GAME_API void StartBench(BenchSetup const& setup);
    AC_GAME_API void AddBenchParticipant(ObjectGuid owner, Player* player, std::string_view label);
    AC_GAME_API bool IsBenchRunning(ObjectGuid owner);
    // Since the test's first damage, 0 before it
    AC_GAME_API uint32 GetBenchElapsedMs(ObjectGuid owner);
    // Ends the owner's test. Returns whether anything happened in it; then result holds it and, unless discard, it
    // is saved (completed: it ran its course; else it was stopped early).
    AC_GAME_API bool FinishBench(ObjectGuid owner, bool completed, bool discard, BenchResult& result);

    // The combat log, as the core sends it (Unit.cpp, Spell.cpp): cheap no-ops while no bench test runs
    AC_GAME_API void RecordLogDamage(Unit* attacker, Unit* victim, SpellInfo const* spellInfo, uint32 damage,
        uint32 overkill, bool crit, DamageEffectType damageType);
    AC_GAME_API void RecordLogHeal(Unit* healer, Unit* target, SpellInfo const* spellInfo, uint32 heal,
        uint32 overheal, bool crit);
    AC_GAME_API void RecordSpellCast(Unit* caster, SpellInfo const* spellInfo);
}

#endif
