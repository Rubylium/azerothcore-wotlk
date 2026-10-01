# Runs the combat bench (Terrain d'essai, `.bench`) headless: a GM client (AzerothGhost) brings bench bots to GM
# Island, runs each layout and prints the in-game reports, then the per-spell hits of every run from the telemetry
# (hits per cast show a target cap at a glance). Needs the local auth + world servers up and Go.
# For a tuning loop, bench.ps1 keeps a session logged in instead (no login, no build, no bots brought each time).
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
. (Join-Path $PSScriptRoot 'BenchCommon.ps1')

$go = Get-BenchGo
Initialize-BenchGhost $go
Set-BenchEnvironment
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
    $output = & $go test -tags=e2e ./tools/bench -run 'TestTool_CombatBench$' -count=1 -v -timeout 30m 2>&1
} finally {
    Pop-Location
}
$output | Set-Content $log -Encoding utf8

$reports = Format-BenchReports ($output | Where-Object { "$_" -match 'bench_e2e_test\.go:\d+: (\[|RESULT)' })
$reports
if (-not ($output | Select-String -Pattern '--- PASS' -Quiet)) {
    Write-Warning "The bench run did not pass; full log: $log"
}

if (-not $noSpells) {
    Show-BenchSpells $reports
}
"`nFull log: $log"
