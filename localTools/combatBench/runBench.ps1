# Runs the combat bench (Terrain d'essai, `.bench`) headless: a GM client (AzerothGhost) brings bench bots to GM
# Island, runs each layout and prints the in-game reports, then the per-spell hits of every run from the telemetry
# (hits per cast show a target cap at a glance). Needs the local auth + world servers up and Go.
# Guide: .agents/docs/systems/combat-bench.md
#
#   .\localTools\combatBench\runBench.ps1 -bots 'mage fire aoe;rogue sub aoe'
#   .\localTools\combatBench\runBench.ps1 -bots 'hunter bm aoe;hunter mm aoe' -layouts 'pack5,pack12' -key 15
#   .\localTools\combatBench\runBench.ps1 -bots 'shaman resto;priest holy' -layouts 'tankpack' -pulse 12
param(
    [Parameter(Mandatory = $true)][string]$bots,          # ";"-separated `.bench bot` arguments
    [string]$layouts = 'single,pack5,pack8,pack12',
    [string]$key = '10',
    [int]$seconds = 45,
    [int]$pulse = 0,                                       # a group damage pulse for healer tests (% of reference health)
    [switch]$noSpells                                      # skip the per-spell tables
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$e2eDirectory = Join-Path $repositoryRoot 'e2e'
$githubRoot = Split-Path -Parent $repositoryRoot

# --- Go --------------------------------------------------------------------------------------------------------------
$go = (Get-Command go.exe -ErrorAction SilentlyContinue).Source
if (-not $go) {
    $go = Get-ChildItem (Join-Path $env:USERPROFILE 'sdk') -Filter go.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $go) {
    throw 'go.exe was not found (PATH or %USERPROFILE%\sdk\go*\bin). Install Go 1.26 (winget install GoLang.Go).'
}

# --- AzerothGhost: the published v1.0.8 with this auth server's logon fix, as a sibling checkout ----------------------
$ghost = Join-Path $githubRoot 'AzerothGhost'
if (-not (Test-Path (Join-Path $ghost 'e2e\e2eharness'))) {
    $cached = Join-Path $env:USERPROFILE 'go\pkg\mod\github.com\azerothcore\!azeroth!ghost@v1.0.8'
    if (-not (Test-Path $cached)) {
        Push-Location $e2eDirectory
        try { & $go mod download github.com/azerothcore/AzerothGhost | Out-Null } finally { Pop-Location }
    }
    Copy-Item $cached $ghost -Recurse
    Get-ChildItem $ghost -Recurse -File | ForEach-Object { $_.IsReadOnly = $false }
}
# The logon challenge's OS field: v1.0.8 sends it misordered and this auth server refuses the session
$auth = Join-Path $ghost 'client\auth.go'
$authSource = Get-Content $auth -Raw
$badOs = "os := [4]byte{0, 'n', 'i', 'W'}"
if ($authSource.Contains($badOs)) {
    Set-Content $auth ($authSource.Replace($badOs, "os := [4]byte{'n', 'i', 'W', 0}")) -NoNewline -Encoding utf8
}
$goWork = Join-Path $e2eDirectory 'go.work'
if (-not (Test-Path $goWork) -or -not (Select-String -Path $goWork -Pattern 'AzerothGhost =>' -Quiet)) {
    $ghostPath = $ghost.Replace('\', '/')
    $workSource = "go 1.26.0`n`nuse .`n`nreplace github.com/azerothcore/AzerothGhost => $ghostPath`n"
    Set-Content $goWork $workSource -Encoding ascii
}

# --- Databases, from the world server's config ---------------------------------------------------------------------
$worldServerConfig = Join-Path $repositoryRoot 'server\configs\worldserver.conf'
function Get-Database([string]$setting) {
    $line = Select-String -Path $worldServerConfig -Pattern "^$setting\s*=\s*`"([^`"]+)`"" | Select-Object -First 1
    if (-not $line) { throw "$setting was not found in $worldServerConfig" }
    $parts = $line.Matches[0].Groups[1].Value.Split(';')
    [pscustomobject]@{ Host = $parts[0]; Port = $parts[1]; User = $parts[2]; Password = $parts[3]; Name = $parts[4] }
}
function Get-Dsn($database) {
    $credentials = if ($database.Password) { "$($database.User):$($database.Password)" } else { $database.User }
    "$credentials@tcp($($database.Host):$($database.Port))/$($database.Name)"
}
$authDatabase = Get-Database 'LoginDatabaseInfo'
$characterDatabase = Get-Database 'CharacterDatabaseInfo'
$worldDatabase = Get-Database 'WorldDatabaseInfo'

$env:E2E_AUTH_ADDR = '127.0.0.1:3724'
$env:E2E_AUTH_DSN = Get-Dsn $authDatabase
$env:E2E_CHAR_DSN = Get-Dsn $characterDatabase
$env:E2E_WORLD_DSN = Get-Dsn $worldDatabase
$env:BENCH_BOTS = $bots
$env:BENCH_LAYOUTS = $layouts
$env:BENCH_KEY = $key
$env:BENCH_SECS = "$seconds"
$env:BENCH_PULSE = if ($pulse -gt 0) { "$pulse" } else { '' }

# --- Run ------------------------------------------------------------------------------------------------------------
$outputDirectory = Join-Path $repositoryRoot 'var\combatBench'
New-Item -ItemType Directory -Force $outputDirectory | Out-Null
$log = Join-Path $outputDirectory ((Get-Date -Format 'yyyy-MM-dd_HH-mm-ss') + '.log')

Push-Location $e2eDirectory
try {
    $output = & $go test -tags=e2e ./tools/bench -run TestTool_CombatBench -count=1 -v -timeout 30m 2>&1
} finally {
    Pop-Location
}
$output | Set-Content $log -Encoding utf8

# The reports, without the chat colours
$reports = $output | ForEach-Object { "$_" } |
    Where-Object { $_ -match 'bench_e2e_test\.go:\d+: (\[|RESULT)' } |
    ForEach-Object {
        ($_ -replace '^.*bench_e2e_test\.go:\d+: ', '') -replace '\|c[0-9a-fA-F]{8}', '' -replace '\|r', ''
    } |
    Where-Object { $_ -notmatch '^\[setup\] (You are|Map:|X:|grid|ZoneX|GroundZ)' }
$reports
if (-not ($output | Select-String -Pattern '--- PASS' -Quiet)) {
    Write-Warning "The bench run did not pass; full log: $log"
}

# --- Per-spell hits of each run -------------------------------------------------------------------------------------
if (-not $noSpells) {
    $mysql = Get-ChildItem 'C:\laragon\bin\mysql' -Filter mysql.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $mysql) { $mysql = (Get-Command mysql.exe -ErrorAction SilentlyContinue).Source }
    $runs = $reports | Where-Object { $_ -match '^RESULT (\S+) (\d+)$' } | ForEach-Object {
        [pscustomobject]@{ Layout = $Matches[1]; Run = $Matches[2] }
    }
    if ($mysql -and $runs) {
        $arguments = @("--user=$($characterDatabase.User)", "--host=$($characterDatabase.Host)",
            "--port=$($characterDatabase.Port)", '--protocol=TCP', '--table')
        $env:MYSQL_PWD = $characterDatabase.Password
        $ErrorActionPreference = 'Continue'
        foreach ($run in $runs) {
            "`n== $($run.Layout) (run $($run.Run)): damage per spell, hits per cast =="
            $query = @"
SELECT c.name, IF(s.from_pet, 'pet', '') AS pet, s.spell_id, s.casts, s.hits,
       ROUND(s.hits / NULLIF(s.casts, 0), 1) AS hits_per_cast, s.crits,
       ROUND(s.amount / NULLIF(s.hits, 0)) AS per_hit, s.amount,
       ROUND(100 * s.amount / t.total, 1) AS share
FROM $($characterDatabase.Name).mod_combat_bench_spell s
JOIN $($characterDatabase.Name).characters c ON c.guid = s.guid
JOIN (SELECT guid, SUM(amount) AS total FROM $($characterDatabase.Name).mod_combat_bench_spell
      WHERE run_id = $($run.Run) AND kind = 1 GROUP BY guid) t ON t.guid = s.guid
WHERE s.run_id = $($run.Run) AND s.kind = 1 AND s.amount > 0
ORDER BY c.name, s.amount DESC;
"@
            $query | & $mysql @arguments 2>&1 | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] }
        }
    }
}
"`nFull log: $log"
