# A combat bench session for tuning loops: a GM client logged in once at the bench, taking runs and commands until
# stopped - each run costs only its own seconds (no driver build, no login, no bots brought again). With live tuning
# (.tune, src/server/game/Tuning/LiveTuning.h) a change needs no build or restart either: change, run, read, again.
# Guide: .agents/docs/systems/combat-bench.md ("Tuning loop").
#
#   .\localTools\combatBench\bench.ps1 start                                   # log the session in (once)
#   .\localTools\combatBench\bench.ps1 bots 'mage fire aoe;warrior arms aoe'    # the bench bots (replaces them)
#   .\localTools\combatBench\bench.ps1 bots 'shaman resto;mage fire' -pulse 12  # healer test
#   .\localTools\combatBench\bench.ps1 run -layouts single,pack5 -seconds 30    # reports + per-spell tables
#   .\localTools\combatBench\bench.ps1 run -tune '.tune spell 47486 1.1'        # commands first, then the run
#   .\localTools\combatBench\bench.ps1 cmd '.tune set warrior.mortal_strike_bonus 1.3' '.tune list warrior'
#   .\localTools\combatBench\bench.ps1 status | stop
#
# Every damage spec at once (each bot alone on its own dummies, up to 30 a heat), at a list of profiles
# (ilvl:paragon[:scaling[:layouts]]), summed up against the Fire mage, CSV in var\combatBench (BenchSweep.ps1):
#   .\localTools\combatBench\bench.ps1 sweep -profiles '258:0,450:600,460:650' -copies 2 -repeats 1
#   .\localTools\combatBench\bench.ps1 sweep -specs '4 1:Assassination;8 2:Fire' -profiles '460:650' -goal 1%
# The spec balance moved toward the mage, live (balance0.* at no paragon, balance.* at 650), then a check profile:
#   .\localTools\combatBench\bench.ps1 tune -profiles '258:0,460:650' -validate '450:600'
param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet('start', 'bots', 'run', 'cmd', 'status', 'stop', 'sweep', 'tune')][string]$action,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$arguments,
    [string]$layouts = 'single,pack5',
    [string]$key = '10',
    [int]$seconds = 30,
    [int]$pulse = 0,
    [string[]]$tune = @(),                                 # commands sent before the run (.tune ...)
    [switch]$noSpells,
    # sweep / tune
    [string]$profiles = '',                                # sweep: '258:0,450:600,460:650'; tune: '258:0,460:650'
    [string]$validate = '450:600',                         # tune: profiles measured once the factors are set
    [string]$specs = '',                                   # "<class id> <spec>:<name>;...", default every DPS spec
    [int]$copies = 2,                                      # bots a spec (pools the bot-to-bot spread)
    [int]$repeats = 1,                                     # runs a layout and profile (a tune iteration)
    [int]$lanes = 30,                                      # lanes a heat (the bench's phases: 30 at most)
    [string]$goal = '',                                    # '1%': a whole kill a lane instead of -seconds
    [int]$laneHealth = 0,                                  # lanes' dummies' health %, default 100 (25 for a kill)
    [string]$preset = 'single',                            # the bots' talent build: single, aoe or auto
    [int]$ilvlTolerance = 10,                              # a row further from the asked item level is untrusted
    [double]$target = 1.0,                                 # tune: the share of the Fire mage aimed at
    [double]$band = 0.05,                                  # tune: settled within target +/- band
    [double]$damping = 0.7,                                # tune: a step moves (target / share) ^ damping
    [double]$maxStep = 0.25,                               # tune: at most +/-25% a step
    [int]$iterations = 4                                   # tune: measured and moved at most this many times
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BenchCommon.ps1')
. (Join-Path $PSScriptRoot 'BenchSweep.ps1')

$sessionDirectory = Join-Path $repositoryRoot 'var\combatBench\session'
$readyFile = Join-Path $sessionDirectory 'ready'
$pidFile = Join-Path $sessionDirectory 'pid'
$sessionLog = Join-Path $sessionDirectory 'session.log'

function Test-Session {
    if (-not (Test-Path $readyFile) -or -not (Test-Path $pidFile)) { return $false }
    $process = Get-Process -Id ([int](Get-Content $pidFile)) -ErrorAction SilentlyContinue
    return [bool]$process
}

# Sends one request (lines) and prints its answer once the session is done with it
function Send-Request([string[]]$lines, [int]$timeoutSeconds) {
    if (-not (Test-Session)) { throw 'No bench session: .\localTools\combatBench\bench.ps1 start' }
    $name = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $temporary = Join-Path $sessionDirectory "tmp-$name.txt"
    [System.IO.File]::WriteAllText($temporary, ($lines -join "`n"), [System.Text.UTF8Encoding]::new($false))
    Move-Item $temporary (Join-Path $sessionDirectory "req-$name.txt")

    $answer = Join-Path $sessionDirectory "res-$name.txt"
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while (-not (Test-Path $answer)) {
        if ((Get-Date) -gt $deadline) { throw "The session did not answer in $timeoutSeconds s (log: $sessionLog)" }
        if (-not (Test-Session)) { throw "The session ended (log: $sessionLog)" }
        Start-Sleep -Milliseconds 300
    }
    $reports = Format-BenchReports (Get-Content $answer -Encoding utf8)
    Remove-Item $answer
    $reports
}

# Logs the session in (once): the driver built into a binary and started, its process the session's
function Start-Session {
    if (Test-Session) { return "The bench session is already up." }
    New-Item -ItemType Directory -Force $sessionDirectory | Out-Null
    Get-ChildItem $sessionDirectory -Filter '*.txt' | Remove-Item
    Remove-Item $readyFile -ErrorAction SilentlyContinue

    $go = Get-BenchGo
    Initialize-BenchGhost $go
    Set-BenchEnvironment
    $env:BENCH_SESSION_DIR = $sessionDirectory
    # The driver built once into a binary, run directly: its process is the session's (stop can end it)
    $binary = Join-Path $sessionDirectory 'bench.test.exe'
    Push-Location $e2eDirectory
    try { & $go test -c -tags=e2e -o $binary ./tools/bench } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw 'The bench driver did not build.' }
    $process = Start-Process -FilePath $binary -WorkingDirectory (Join-Path $e2eDirectory 'tools\bench') `
        -ArgumentList '-test.run', 'TestTool_CombatBenchSession$', '-test.v', '-test.timeout', '0' `
        -RedirectStandardOutput $sessionLog -RedirectStandardError "$sessionLog.err" -WindowStyle Hidden -PassThru
    Set-Content $pidFile $process.Id
    $deadline = (Get-Date).AddSeconds(90)
    while (-not (Test-Path $readyFile)) {
        if ($process.HasExited -or (Get-Date) -gt $deadline) {
            throw "The bench session did not come up (log: $sessionLog)"
        }
        Start-Sleep -Milliseconds 500
    }
    "Bench session up ($(Get-Content $readyFile)). Next: bench.ps1 bots '<class spec ...;...>'"
}

switch ($action) {
    'start' { Start-Session }
    'bots' {
        if (-not $arguments) { throw "bots '<.bench bot arguments;...>'" }
        $line = "bots $($arguments -join ' ')"
        if ($pulse -gt 0) { $line += " pulse=$pulse" }
        Send-Request @($line) 180
    }
    'run' {
        $layoutCount = ($layouts.Split(',') | Where-Object { $_.Trim() }).Count
        $reports = Send-Request (@($tune) + @("run $layouts $key $seconds")) (120 + $layoutCount * ($seconds + 60))
        $reports
        if (-not $noSpells) { Show-BenchSpells $reports }
    }
    'cmd' {
        if (-not $arguments) { throw "cmd '.<command>' ['.<command>' ...]" }
        Send-Request $arguments (30 + 5 * $arguments.Count)
    }
    { $_ -in 'sweep', 'tune' } {
        if (-not (Test-Session)) { Start-Session | Write-Host }
        $runGoal = if ($goal) { $goal } elseif ($PSBoundParameters.ContainsKey('seconds')) { "$seconds" } else { '60' }
        $health = if ($laneHealth -gt 0) { $laneHealth } elseif ($runGoal.EndsWith('%')) { 25 } else { 100 }
        $layoutsOverride = if ($PSBoundParameters.ContainsKey('layouts')) { $layouts } else { '' }
        $specList = @(ConvertTo-SweepSpecs $specs)
        $outputDirectory = Join-Path $repositoryRoot 'var\combatBench'
        if ($action -eq 'sweep') {
            $sweepProfiles = if ($profiles) { $profiles } else { '258:0,450:600,460:650' }
            Invoke-BenchSweep @(ConvertTo-SweepProfiles $sweepProfiles $layoutsOverride) $specList $copies $repeats `
                ([Math]::Min($lanes, 30)) $health $runGoal $preset $ilvlTolerance $outputDirectory
        } else {
            $tuneProfiles = if ($profiles) { $profiles } else { '258:0,460:650' }
            $settings = [pscustomobject]@{
                Copies = $copies; Repeats = $repeats; Lanes = [Math]::Min($lanes, 30); LaneHealth = $health
                Goal = $runGoal; Preset = $preset; IlvlTolerance = $ilvlTolerance; Target = $target; Band = $band
                Damping = $damping; MaxStep = $maxStep; Iterations = $iterations
            }
            $checks = if ($validate) { @(ConvertTo-SweepProfiles $validate $layoutsOverride) } else { @() }
            Invoke-BenchTune @(ConvertTo-SweepProfiles $tuneProfiles $layoutsOverride) $checks $specList $settings `
                $outputDirectory
        }
    }
    'status' {
        if (Test-Session) { "Bench session up ($(Get-Content $readyFile)), pid $(Get-Content $pidFile)." }
        else { "No bench session." }
    }
    'stop' {
        if (Test-Session) {
            try { Send-Request @('quit') 30 | Out-Null } catch { }
            Stop-Process -Id ([int](Get-Content $pidFile)) -ErrorAction SilentlyContinue
        }
        Remove-Item $readyFile, $pidFile -ErrorAction SilentlyContinue
        "Bench session stopped."
    }
}
