/*
 * Combat telemetry for balance analysis. Hits are aggregated in memory and
 * persisted only when a Mythic+ run or raid boss kill finishes.
 */

#ifndef COMBAT_TELEMETRY_H
#define COMBAT_TELEMETRY_H

#include "Define.h"
#include "SharedDefines.h"

#include <string_view>

class Group;
class Map;
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
}

#endif
