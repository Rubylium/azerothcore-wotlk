# What runBench.ps1 (one run) and bench.ps1 (a session) share: Go and the AzerothGhost checkout, the databases, the
# reports out of the driver's output and the per-spell tables from the telemetry. Dot-sourced.

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$e2eDirectory = Join-Path $repositoryRoot 'e2e'
$githubRoot = Split-Path -Parent $repositoryRoot

# --- Go --------------------------------------------------------------------------------------------------------------
function Get-BenchGo {
    $go = (Get-Command go.exe -ErrorAction SilentlyContinue).Source
    if (-not $go) {
        $go = Get-ChildItem (Join-Path $env:USERPROFILE 'sdk') -Filter go.exe -Recurse -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $go) {
        throw 'go.exe was not found (PATH or %USERPROFILE%\sdk\go*\bin). Install Go 1.26 (winget install GoLang.Go).'
    }
    $go
}

# --- AzerothGhost: the published v1.0.8 with this auth server's logon fix, as a sibling checkout ----------------------
function Initialize-BenchGhost([string]$go) {
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
}

# --- Databases, from the world server's config ---------------------------------------------------------------------
$worldServerConfig = Join-Path $repositoryRoot 'server\configs\worldserver.conf'
function Get-BenchDatabase([string]$setting) {
    $line = Select-String -Path $worldServerConfig -Pattern "^$setting\s*=\s*`"([^`"]+)`"" | Select-Object -First 1
    if (-not $line) { throw "$setting was not found in $worldServerConfig" }
    $parts = $line.Matches[0].Groups[1].Value.Split(';')
    [pscustomobject]@{ Host = $parts[0]; Port = $parts[1]; User = $parts[2]; Password = $parts[3]; Name = $parts[4] }
}
function Get-BenchDsn($database) {
    $credentials = if ($database.Password) { "$($database.User):$($database.Password)" } else { $database.User }
    "$credentials@tcp($($database.Host):$($database.Port))/$($database.Name)"
}
$characterDatabase = Get-BenchDatabase 'CharacterDatabaseInfo'

# The driver's environment: the servers and databases it logs into
function Set-BenchEnvironment {
    $env:E2E_AUTH_ADDR = '127.0.0.1:3724'
    $env:E2E_AUTH_DSN = Get-BenchDsn (Get-BenchDatabase 'LoginDatabaseInfo')
    $env:E2E_CHAR_DSN = Get-BenchDsn $characterDatabase
    $env:E2E_WORLD_DSN = Get-BenchDsn (Get-BenchDatabase 'WorldDatabaseInfo')
}

# --- Reports ---------------------------------------------------------------------------------------------------------
# The driver's report lines, without the test prefix and the chat colours
function Format-BenchReports($lines) {
    $lines | ForEach-Object { "$_" } |
        ForEach-Object { ($_ -replace '^.*bench_e2e_test\.go:\d+: ', '') -replace '\|c[0-9a-fA-F]{8}', '' -replace '\|r', '' } |
        Where-Object { $_ -notmatch '^\[setup\] (You are|Map:|X:|grid|ZoneX|GroundZ)' }
}

# Per-spell hits of each RESULT line's run (hits per cast show a target cap at a glance)
function Show-BenchSpells($reports) {
    $mysql = Get-ChildItem 'C:\laragon\bin\mysql' -Filter mysql.exe -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $mysql) { $mysql = (Get-Command mysql.exe -ErrorAction SilentlyContinue).Source }
    $runs = $reports | Where-Object { $_ -match '^RESULT (\S+) (\d+)$' } | ForEach-Object {
        [pscustomobject]@{ Layout = $Matches[1]; Run = $Matches[2] }
    }
    if (-not $mysql -or -not $runs) { return }
    $arguments = @("--user=$($characterDatabase.User)", "--host=$($characterDatabase.Host)",
        "--port=$($characterDatabase.Port)", '--protocol=TCP', '--table')
    $env:MYSQL_PWD = $characterDatabase.Password
    $previous = $ErrorActionPreference
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
    $ErrorActionPreference = $previous
}
