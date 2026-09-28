param(
    [int]$days = 30,
    [string]$outputDirectory = '',
    [string]$worldServerConfig = ''
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $worldServerConfig) {
    $worldServerConfig = Join-Path $repositoryRoot 'server\configs\worldserver.conf'
}
if (-not $outputDirectory) {
    $outputDirectory = Join-Path $repositoryRoot 'var\combatTelemetry'
}

$databaseLine = Select-String -Path $worldServerConfig -Pattern '^CharacterDatabaseInfo\s*=\s*"([^"]+)"' |
    Select-Object -First 1
if (-not $databaseLine) {
    throw "CharacterDatabaseInfo was not found in $worldServerConfig"
}

$databaseParts = $databaseLine.Matches[0].Groups[1].Value.Split(';')
if ($databaseParts.Count -lt 5) {
    throw 'CharacterDatabaseInfo has an unsupported format.'
}
$databaseHost, $databasePort, $databaseUser, $databasePassword, $databaseName = $databaseParts[0..4]

$mysqlCandidates = @(
    Get-ChildItem 'C:\laragon\bin\mysql' -Filter mysql.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -ExpandProperty FullName
)
$mysql = $mysqlCandidates | Select-Object -First 1
if (-not $mysql) {
    $mysql = (Get-Command mysql.exe -ErrorAction SilentlyContinue).Source
}
if (-not $mysql) {
    throw 'mysql.exe was not found.'
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$minimumTimestamp = [DateTimeOffset]::UtcNow.AddDays(-[Math]::Max(1, $days)).ToUnixTimeMilliseconds()
$env:MYSQL_PWD = $databasePassword

function exportQuery([string]$name, [string]$query) {
    $target = Join-Path $outputDirectory "$name.tsv"
    $output = & $mysql --host=$databaseHost --port=$databasePort --user=$databaseUser --database=$databaseName `
        --batch --raw --default-character-set=utf8mb4 --execute=$query
    if ($LASTEXITCODE -ne 0) {
        throw "mysql.exe failed while exporting $name"
    }
    Set-Content -Path $target -Value $output -Encoding utf8
    Write-Host $target
}

try {
    exportQuery 'runs' @"
SELECT run_id, run_type, result, FROM_UNIXTIME(started_at_ms / 1000) AS started_at,
       map_id, instance_id, dungeon_id, difficulty, key_level, time_limit_seconds,
       boss_entry, boss_name, duration_seconds, timer_deaths, human_count, bot_count, total_damage
FROM mod_combat_run
WHERE ended_at_ms >= $minimumTimestamp
ORDER BY ended_at_ms DESC;
"@

    exportQuery 'participants' @"
SELECT p.run_id, r.run_type, r.result, r.map_id, r.dungeon_id, r.key_level, r.boss_entry,
       p.guid, p.name, p.is_bot, p.class_id, p.race_id, p.level, p.role_mask, p.spec_id,
       p.item_level, p.max_health, p.strength, p.agility, p.stamina, p.intellect, p.spirit,
       p.attack_power, p.spell_power, p.melee_crit, p.spell_crit, p.melee_haste, p.spell_haste,
       p.damage, p.boss_damage, p.trash_damage, p.pet_damage, p.hits, p.deaths, p.active_ms,
       ROUND(p.damage / GREATEST(r.duration_seconds, 1), 2) AS run_dps,
       ROUND(p.damage / GREATEST(p.active_ms / 1000, 1), 2) AS active_dps
FROM mod_combat_participant p
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp
ORDER BY r.ended_at_ms DESC, p.damage DESC;
"@

    exportQuery 'classSpecSummary' @"
SELECT r.run_type, p.class_id, p.spec_id, p.is_bot, COUNT(*) AS samples,
       ROUND(AVG(p.item_level), 2) AS avg_item_level,
       ROUND(AVG(p.damage / GREATEST(r.duration_seconds, 1)), 2) AS avg_run_dps,
       ROUND(AVG(p.boss_damage / GREATEST(r.duration_seconds, 1)), 2) AS avg_boss_dps,
       ROUND(AVG(p.trash_damage / GREATEST(r.duration_seconds, 1)), 2) AS avg_trash_dps,
       ROUND(AVG(p.deaths), 2) AS avg_deaths
FROM mod_combat_participant p
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp AND p.damage > 0
GROUP BY r.run_type, p.class_id, p.spec_id, p.is_bot
ORDER BY r.run_type, avg_run_dps DESC;
"@

    exportQuery 'abilities' @"
SELECT a.run_id, p.name, p.is_bot, p.class_id, p.spec_id, a.spell_id, a.damage_type,
       a.damage, a.boss_damage, a.trash_damage, a.hits, a.pet_hits,
       ROUND(a.damage * 100 / GREATEST(p.damage, 1), 2) AS damage_share_percent
FROM mod_combat_ability a
JOIN mod_combat_participant p USING (run_id, guid)
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp
ORDER BY a.run_id DESC, p.damage DESC, a.damage DESC;
"@

    exportQuery 'targets' @"
SELECT t.run_id, p.name, p.is_bot, p.class_id, p.spec_id, t.creature_entry, t.is_boss,
       t.damage, t.hits
FROM mod_combat_target t
JOIN mod_combat_participant p USING (run_id, guid)
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp
ORDER BY t.run_id DESC, p.damage DESC, t.damage DESC;
"@

    exportQuery 'routeEvents' @"
SELECT e.run_id, r.dungeon_id, r.key_level, e.sequence, e.offset_ms, e.event_type,
       e.actor_guid, e.target_entry, e.position_x, e.position_y, e.position_z, e.details
FROM mod_combat_route_event e
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp
ORDER BY e.run_id DESC, e.sequence;
"@

    # Combat bench tests (run_type 3, mod-playerbots Script/CombatBench.cpp): one row per participant, with what the
    # bench set a bot up as, its healing and damage taken; the run's boss_name is "<layout> <scaling> <duration>"
    exportQuery 'benchParticipants' @"
SELECT r.run_id, FROM_UNIXTIME(r.started_at_ms / 1000) AS started_at, r.result, r.boss_name AS test,
       r.dungeon_id AS layout, r.difficulty AS scaling, r.key_level AS level, r.duration_seconds,
       p.guid, p.name, p.is_bot, p.class_id, b.label, p.item_level, p.damage, p.pet_damage,
       ROUND(p.damage / GREATEST(r.duration_seconds, 1), 2) AS dps,
       b.healing, b.overhealing, ROUND(b.healing / GREATEST(r.duration_seconds, 1), 2) AS hps,
       b.damage_taken, ROUND(b.damage_taken / GREATEST(r.duration_seconds, 1), 2) AS dtps, p.deaths
FROM mod_combat_run r
JOIN mod_combat_participant p USING (run_id)
LEFT JOIN mod_combat_bench_participant b ON b.run_id = p.run_id AND b.guid = p.guid
WHERE r.run_type = 3 AND r.ended_at_ms >= $minimumTimestamp
ORDER BY r.ended_at_ms DESC, p.damage DESC;
"@

    # Their spells: kind 1 damage done, 2 healing done, 3 damage taken; spell 0 is melee
    exportQuery 'benchSpells' @"
SELECT s.run_id, r.boss_name AS test, p.name, p.class_id, b.label, s.kind, s.spell_id, s.from_pet, s.casts, s.hits,
       s.crits, ROUND(s.crits * 100 / GREATEST(s.hits, 1), 2) AS crit_percent, s.amount, s.overheal,
       ROUND(s.amount / GREATEST(r.duration_seconds, 1), 2) AS per_second
FROM mod_combat_bench_spell s
JOIN mod_combat_run r USING (run_id)
JOIN mod_combat_participant p ON p.run_id = s.run_id AND p.guid = s.guid
LEFT JOIN mod_combat_bench_participant b ON b.run_id = s.run_id AND b.guid = s.guid
WHERE r.run_type = 3 AND r.ended_at_ms >= $minimumTimestamp
ORDER BY s.run_id DESC, p.name, s.kind, s.amount DESC;
"@

    # Tank route pulls (MythicTankLead): the bot tank's, and the player tanks' it watches
    exportQuery 'pulls' @"
SELECT e.run_id, r.dungeon_id, r.key_level, r.human_count, r.result, e.sequence, e.offset_ms, e.event_type,
       e.position_x, e.position_y, e.position_z, e.details
FROM mod_combat_route_event e
JOIN mod_combat_run r USING (run_id)
WHERE r.ended_at_ms >= $minimumTimestamp
  AND e.event_type IN ('route_loaded', 'route_pull_started', 'route_pull_drag', 'route_pull_hold', 'route_pull_done',
                       'route_pull_failed', 'route_pack_skipped', 'route_wait', 'real_pull_started', 'real_pull_done',
                       'boss_checkpoint', 'group_wipe', 'tank_stuck')
ORDER BY e.run_id DESC, e.sequence;
"@
}
finally {
    Remove-Item Env:MYSQL_PWD -ErrorAction SilentlyContinue
}
