# The simulation bench: the combat bench run on world servers of its own (simulation workers, Sim.Enable), the real
# game code on a clock that runs as fast as the world updates while a test is under way, several workers at once -
# never the realm players are on. Guide: .agents/docs/systems/sim-bench.md.
#
#   .\localTools\simBench\simBench.ps1 prepare -workers 4      # the built server copied, the databases copied (once)
#   .\localTools\simBench\simBench.ps1 start -workers 4        # the workers up (about a minute; they stay up)
#   .\localTools\simBench\simBench.ps1 sweep                   # bench.ps1 sweep's measure, a heat a worker
#   .\localTools\simBench\simBench.ps1 sweep -profiles '460:650' -specs '4 1:Assassination;8 2:Fire' -copies 3
#   .\localTools\simBench\simBench.ps1 tune                    # bench.ps1 tune's, the factors into live_tuning
#   .\localTools\simBench\simBench.ps1 cmd -worker 1 '.bench list' '.tune list balance'
#   .\localTools\simBench\simBench.ps1 check -repeats 3      # turbo against real time, the same tests (accuracy)
#   .\localTools\simBench\simBench.ps1 check 2 10 -repeats 6 # turbo at a 2 ms step against a 10 ms one
#   .\localTools\simBench\simBench.ps1 status | stop
param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet('prepare', 'start', 'status', 'stop', 'cmd', 'sweep', 'tune', 'check')][string]$action,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$arguments,
    [int]$workers = 4,
    [int]$worker = 1,                                      # cmd: the worker asked
    [int]$stepMs = 10,                                     # the game time an update moves in turbo
    [switch]$noDatabases,                                  # prepare: the binaries and configs only
    # sweep / tune, as bench.ps1's
    [string]$profiles = '',
    [string]$validate = '450:600',
    [string]$specs = '',
    [int]$copies = 2,
    [int]$repeats = 1,
    [int]$lanes = 0,                                       # lanes a heat; 0: as many heats as workers
    [string]$goal = '',
    [int]$seconds = 60,
    [string]$layouts = '',
    [int]$laneHealth = 0,
    [string]$preset = 'layout',                            # layout: packs on the aoe build, the rest single
    [int]$ilvlTolerance = 10,
    [double]$target = 1.0,
    [double]$band = 0.05,
    [double]$damping = 0.7,
    [double]$maxStep = 0.25,
    [int]$iterations = 4,
    [switch]$noApply                                       # tune: leave the found factors out of live_tuning
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SimCommon.ps1')
. (Join-Path $repositoryRoot 'localTools\combatBench\BenchSweep.ps1')

function Get-UpWorkers { @(1..32 | Where-Object { Test-SimWorker $_ }) }

function Start-SimWorkers([int]$count) {
    if (-not (Test-Path (Join-Path $simBin 'simworld.exe'))) { throw 'No simulation binaries: simBench.ps1 prepare' }
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $started = @()
    foreach ($index in 1..$count) {
        if (Test-SimWorker $index) { continue }
        Initialize-SimWorker $index $stepMs | Out-Null
        $hostArguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
            (Join-Path $PSScriptRoot 'simWorkerHost.ps1'), '-worker', $index)
        Start-Process powershell.exe -ArgumentList $hostArguments -WindowStyle Hidden | Out-Null
        $started += $index
    }
    if (-not $started.Count) { return "All $count workers are up." }
    Write-Host "Starting workers $($started -join ', ')..."
    $deadline = (Get-Date).AddMinutes(15)
    while ((Get-Date) -lt $deadline) {
        $up = @($started | Where-Object { Test-SimWorker $_ })
        if ($up.Count -eq $started.Count) { return "Workers up in $(Format-Clock $clock.Elapsed)." }
        Start-Sleep -Seconds 2
    }
    throw "Not every worker came up in 15 minutes (var\simBench\w<n>\host.log)"
}

function Stop-SimWorkers {
    foreach ($index in 1..32) {
        $directory = Get-SimWorkerDirectory $index
        if (-not (Test-Path $directory)) { continue }
        if (Test-SimWorker $index) {
            try { Send-SimRequest $index @('quit') 60 | Out-Null } catch { Write-Warning $_ }
        }
        foreach ($file in 'server.pid', 'host.pid') {
            $path = Join-Path $directory $file
            if (Test-Path $path) {
                Stop-Process -Id ([int](Get-Content $path)) -Force -ErrorAction SilentlyContinue
                Remove-Item $path
            }
        }
    }
    'Simulation workers stopped.'
}

# The work of a sweep or a tune spread over the workers: a list of parts, each run by a background job against its
# worker; their Write-Host lines shown as they come, their objects returned
function Invoke-SimParts($parts, [scriptblock]$body) {
    $jobs = foreach ($part in $parts) {
        Start-Job -ArgumentList $PSScriptRoot, $part, $body.ToString() -ScriptBlock {
            param($scriptRoot, $part, $bodyText)
            $ErrorActionPreference = 'Stop'
            . (Join-Path $scriptRoot 'SimCommon.ps1')
            . (Join-Path $repositoryRoot 'localTools\combatBench\BenchSweep.ps1')
            $script:simWorker = $part.Worker
            function Send-Request([string[]]$lines, [int]$timeoutSeconds) {
                Send-SimRequest $script:simWorker $lines $timeoutSeconds |
                    ForEach-Object { ($_ -replace '\|c[0-9a-fA-F]{8}', '') -replace '\|r', '' }
            }
            & ([scriptblock]::Create($bodyText)) $part
        }
    }
    $results = [System.Collections.Generic.List[object]]::new()
    while ($true) {
        foreach ($job in $jobs) { Receive-Job $job | ForEach-Object { $results.Add($_) } }
        if (-not @($jobs | Where-Object { $_.State -eq 'Running' -or $_.State -eq 'NotStarted' }).Count) { break }
        Start-Sleep -Milliseconds 500
    }
    foreach ($job in $jobs) {
        Receive-Job $job | ForEach-Object { $results.Add($_) }
        if ($job.State -eq 'Failed') {
            Write-Warning "A worker's part failed: $($job.ChildJobs[0].JobStateInfo.Reason)"
        }
    }
    $jobs | Remove-Job -Force
    $results
}

# Heats sized to the workers (lanes 0): each worker a heat, the Fire mage's copies in every one
function Get-SimLanes([int]$count, $specList) {
    if ($lanes -gt 0) { return [Math]::Min($lanes, 30) }
    $others = @($specList | Where-Object { $_.Key -ne $FireMageKey }).Count
    $perHeat = [Math]::Ceiling($others / [Math]::Max(1, $count))
    [Math]::Min(30, $copies * ($perHeat + 1))
}

function Invoke-SimSweep($sweepProfiles, $specList, [string]$runGoal, [int]$health, [string]$outputDirectory) {
    $up = @(Get-UpWorkers)
    if (-not $up.Count) { throw 'No simulation worker is up: simBench.ps1 start' }
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $heats = @(New-SweepHeats $specList $copies (Get-SimLanes $up.Count $specList))

    # Parts: a heat and some of its profiles, as many as there are workers (a heat's profiles together while they fit,
    # so its bots are brought once)
    $units = foreach ($index in 0..($heats.Count - 1)) {
        foreach ($prof in $sweepProfiles) { [pscustomobject]@{ Heat = $index; Profile = $prof } }
    }
    $units = @($units)
    $count = [Math]::Min($up.Count, $units.Count)
    $parts = for ($slot = 0; $slot -lt $count; ++$slot) {
        $mine = @($units | Where-Object { [array]::IndexOf($units, $_) % $count -eq $slot })
        [pscustomobject]@{
            Worker = $up[$slot]
            Pieces = @($mine | Group-Object Heat | ForEach-Object {
                [pscustomobject]@{
                    Heat = $heats[[int]$_.Name]; Context = "heat$([int]$_.Name + 1)"
                    Profiles = @($_.Group | ForEach-Object { $_.Profile })
                }
            })
            Specs = $specList; Repeats = $repeats; Goal = $runGoal; Preset = $preset; Tolerance = $ilvlTolerance
            Health = $health
        }
    }
    $runs = 0
    foreach ($prof in $sweepProfiles) { $runs += $prof.Layouts.Count * $repeats * $heats.Count }
    Write-Host ("Simulation sweep: {0} specs x {1}, {2} heat(s), {3} profile(s), {4} run(s) of {5} on {6} worker(s)" -f
        @($specList).Count, $copies, $heats.Count, @($sweepProfiles).Count, $runs, $runGoal, $parts.Count)

    $results = Invoke-SimParts $parts {
        param($part)
        $clock = [System.Diagnostics.Stopwatch]::StartNew()
        Send-Request @(".bench lanes on $($part.Health)") 30 | Out-Null
        try {
            foreach ($piece in $part.Pieces) {
                Invoke-SweepHeat @($piece.Heat) "w$($part.Worker)/$($piece.Context)" @($piece.Profiles) $part.Specs `
                    $part.Repeats $part.Goal $part.Preset $part.Tolerance $clock
            }
        } finally {
            Exit-SweepLanes
        }
    }
    $rows = @($results | Where-Object { $_.PSObject.Properties['Dps'] })
    $summary = @(Measure-Sweep $rows)
    Show-SweepSummary $summary
    $stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Export-InvariantCsv $rows (Join-Path $outputDirectory "sweep-$stamp-rows.csv")
    Export-InvariantCsv $summary (Join-Path $outputDirectory "sweep-$stamp.csv")
    "`nSimulation sweep: $runs run(s), $($rows.Count) rows, wall clock $(Format-Clock $clock.Elapsed)."
    "CSV: var\combatBench\sweep-$stamp.csv (rows: sweep-$stamp-rows.csv)"
}

function Invoke-SimTune($tuneProfiles, $validateProfiles, $specList, $settings, [string]$outputDirectory) {
    $up = @(Get-UpWorkers)
    if (-not $up.Count) { throw 'No simulation worker is up: simBench.ps1 start' }
    foreach ($prof in $tuneProfiles) {
        if ($prof.Paragon -ne 0 -and $prof.Paragon -lt 650) {
            throw "Tune profile $($prof.Label): paragon 0 (balance0.*) or 650+ (balance.*); others go in -validate"
        }
    }
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $heats = @(New-SweepHeats $specList $settings.Copies (Get-SimLanes $up.Count $specList))
    if ($heats.Count -gt $up.Count) { Write-Warning "$($heats.Count) heats on $($up.Count) workers: some run in turn" }
    $count = [Math]::Min($up.Count, $heats.Count)
    $parts = for ($slot = 0; $slot -lt $count; ++$slot) {
        [pscustomobject]@{
            Worker = $up[$slot]
            Heats = @(for ($index = $slot; $index -lt $heats.Count; $index += $count) {
                [pscustomobject]@{ Heat = $heats[$index]; Context = "heat$($index + 1)" }
            })
            TuneProfiles = @($tuneProfiles); ValidateProfiles = @($validateProfiles); Specs = $specList
            Settings = $settings
        }
    }
    Write-Host (("Simulation auto-tune: {0} specs x {1}, {2} heat(s) on {3} worker(s), target {4}% of the Fire mage " +
        "+/-{5}%, up to {6} iteration(s) a profile") -f @($specList).Count, $settings.Copies, $heats.Count,
        $parts.Count,
        (Format-Number (100 * $settings.Target) '0'), (Format-Number (100 * $settings.Band) '0'), $settings.Iterations)

    $results = Invoke-SimParts $parts {
        param($part)
        $clock = [System.Diagnostics.Stopwatch]::StartNew()
        $rows = [System.Collections.Generic.List[object]]::new()
        $changes = [System.Collections.Generic.List[object]]::new()
        $mageSamples = @{}
        foreach ($prof in @($part.TuneProfiles) + @($part.ValidateProfiles)) {
            $mageSamples[$prof.Label] = [System.Collections.Generic.List[double]]::new()
        }
        Send-Request @(".bench lanes on $($part.Settings.LaneHealth)") 30 | Out-Null
        try {
            foreach ($entry in $part.Heats) {
                Invoke-TuneHeat @($entry.Heat) "w$($part.Worker)/$($entry.Context)" @($part.TuneProfiles) `
                    @($part.ValidateProfiles) $part.Specs $part.Settings $mageSamples $rows $changes $clock
            }
        } finally {
            Exit-SweepLanes
        }
        [pscustomobject]@{ SimRows = @($rows); SimChanges = @($changes) }
    }
    $rows = @($results | Where-Object { $_.PSObject.Properties['SimRows'] } | ForEach-Object { $_.SimRows })
    $changes = @($results | Where-Object { $_.PSObject.Properties['SimChanges'] } | ForEach-Object { $_.SimChanges })
    Show-TuneResults $changes $rows

    # Every worker and the live table take the factors found (the next sweep measures them; bakeTuning.py bakes them)
    $moved = @($changes | Where-Object { $_.Knob -and $_.Knob -ne '-' -and $null -ne $_.New -and
        [Math]::Abs([double]$_.New - [double]$_.Old) -ge 0.005 })
    if ($moved.Count) {
        $commands = @($moved | ForEach-Object { ".tune set $($_.Knob) $(Format-Number ([double]$_.New) '0.00')" })
        foreach ($index in $up) { Send-SimRequest $index $commands 60 | Out-Null }
        if (-not $noApply) {
            $values = ($moved | ForEach-Object {
                "('$($_.Knob)', $(Format-Number ([double]$_.New) '0.00'))" }) -join ', '
            Invoke-SimMysql @('acore_world', '-e', "REPLACE INTO live_tuning (``Key``, ``Value``) VALUES $values;") |
                Out-Null
            "`n$($moved.Count) factor(s) written to acore_world.live_tuning (the live server reads them at its next" +
                " start; bake them: python localTools/tuning/bakeTuning.py --dry-run)"
        }
    }
    $stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Export-InvariantCsv $rows (Join-Path $outputDirectory "tune-$stamp-rows.csv")
    Export-InvariantCsv $changes (Join-Path $outputDirectory "tune-$stamp.csv")
    "`nSimulation auto-tune: wall clock $(Format-Clock $clock.Elapsed)."
    "CSV: var\combatBench\tune-$stamp.csv (rows: tune-$stamp-rows.csv)"
}

# The turbo checked against real time on one worker: the same bots, profile and layouts, each test run in real time
# and in turbo in turn (so a drift of the bots between runs falls on both), then each spec's mean DPS both ways, the
# gap and how many standard errors it is. A gap past two standard errors that repeats is a clock the game still reads
# from the real world (sim-bench.md).
function Invoke-SimValidation($checkProfiles, $specList, [string]$runGoal, [int]$checkWorker, [string[]]$modes) {
    if (-not (Test-SimWorker $checkWorker)) { throw "Worker $checkWorker is not up: simBench.ps1 start" }
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $script:simWorker = $checkWorker
    function Send-Request([string[]]$lines, [int]$timeoutSeconds) {
        Send-SimRequest $script:simWorker $lines $timeoutSeconds |
            ForEach-Object { ($_ -replace '\|c[0-9a-fA-F]{8}', '') -replace '\|r', '' }
    }
    $heat = @(New-SweepHeats $specList $copies 30)[0]
    $rows = [System.Collections.Generic.List[object]]::new()
    Send-Request @('.bench lanes on 100') 30 | Out-Null
    try {
        Send-SweepBots $heat $checkProfiles[0].Ilvl $preset
        # Geared again in place for the first profile too: a content bot can keep a former login's gear (one at
        # 270 asked 258, another at 225)
        $currentIlvl = 0
        foreach ($prof in $checkProfiles) {
            Set-SweepProfile $prof ([ref]$currentIlvl)
            foreach ($layout in $prof.Layouts) {
                for ($repeat = 1; $repeat -le $repeats; ++$repeat) {
                    foreach ($mode in $modes) {
                        Send-Request @(".bench turbo $mode") 30 | Out-Null
                        $answer = Invoke-SweepRun $layout $prof $runGoal
                        $context = if ($mode -eq 'off') { 'real' } else { "turbo $mode" }
                        ConvertFrom-SweepRows $answer $prof $specList $context $ilvlTolerance |
                            ForEach-Object { $rows.Add($_) }
                        $speed = $answer | Where-Object { $_ -match '#simspeed;\S*x=([\d.]+)' } | Select-Object -Last 1
                        $factor = if ($speed -match 'x=([\d.]+)') { $Matches[1] } else { '?' }
                        Write-Host ("{0} {1} #{2} {3}: x{4} ({5})" -f $prof.Label, $layout, $repeat, $context, $factor,
                            (Format-Clock $clock.Elapsed))
                    }
                }
            }
        }
    } finally {
        Send-Request @('.bench turbo on') 30 | Out-Null
        Exit-SweepLanes
    }

    # Each mode against the first (real time when it is 'off'), and the mean gap over every spec: a bias the noise
    # of one spec hides shows in the mean
    $reference = if ($modes[0] -eq 'off') { 'real' } else { "turbo $($modes[0])" }
    $worst = 0.0
    foreach ($mode in @($modes | Select-Object -Skip 1)) {
        $context = if ($mode -eq 'off') { 'real' } else { "turbo $mode" }
        "`n== $context against $reference`: mean DPS, gap, gap in standard errors (|z| > 2: look closer) =="
        $gaps = @()
        foreach ($group in ($rows | Where-Object { $_.Trusted } | Group-Object Profile, Layout, Spec)) {
            $base = Get-Stats @($group.Group | Where-Object { $_.Context -eq $reference } | ForEach-Object { $_.Dps })
            $other = Get-Stats @($group.Group | Where-Object { $_.Context -eq $context } | ForEach-Object { $_.Dps })
            if (-not $base.N -or -not $other.N -or $base.Mean -le 0) { continue }
            $gap = $other.Mean / $base.Mean - 1.0
            $gaps += $gap
            $standardError = [Math]::Sqrt(($base.Sd * $base.Sd) / $base.N + ($other.Sd * $other.Sd) / $other.N)
            $z = if ($standardError -gt 0) { ($other.Mean - $base.Mean) / $standardError } else { 0.0 }
            $worst = [Math]::Max($worst, [Math]::Abs($z))
            '{0,-30} {1,8} {2,8}  {3,6}%  z={4,5}  n={5}/{6}' -f $group.Name, (Format-Number $base.Mean '0'),
                (Format-Number $other.Mean '0'), (Format-Number (100 * $gap) '+0.0;-0.0'), (Format-Number $z '0.0'),
                $base.N, $other.N
        }
        if ($gaps.Count) {
            $mean = Get-Stats $gaps
            '   mean gap {0}% (+/-{1} over {2} specs and layouts)' -f (Format-Number (100 * $mean.Mean) '+0.0;-0.0'),
                (Format-Number (100 * $mean.Sd / [Math]::Sqrt($gaps.Count)) '0.0'), $gaps.Count
        }
    }
    $stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    $outputDirectory = Join-Path $repositoryRoot 'var\combatBench'
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Export-InvariantCsv $rows (Join-Path $outputDirectory "simcheck-$stamp-rows.csv")
    "`nWorst |z| $(Format-Number $worst '0.0'); wall clock $(Format-Clock $clock.Elapsed)."
    "Rows: var\combatBench\simcheck-$stamp-rows.csv"
}

switch ($action) {
    'prepare' {
        Update-SimBinaries
        if (-not $noDatabases) { Update-SimDatabases $workers }
        foreach ($index in 1..$workers) { Initialize-SimWorker $index $stepMs | Out-Null }
        "Prepared $workers worker(s) in $simRoot."
    }
    'start' { Start-SimWorkers $workers }
    'stop' { Stop-SimWorkers }
    'status' {
        $up = @(Get-UpWorkers)
        if ($up.Count) { "Workers up: $($up -join ', ')" } else { 'No simulation worker is up.' }
    }
    'cmd' {
        if (-not $arguments) { throw "cmd -worker <n> '.<command>' ['.<command>' ...]" }
        Send-SimRequest $worker $arguments (30 + 5 * $arguments.Count)
    }
    'check' {
        $checkGoal = if ($goal) { $goal } else { "$seconds" }
        $checkSpecs = if ($specs) { $specs } else {
            '8 2:Fire;1 1:Arms;2 3:Retribution;3 1:Beast Mastery;4 1:Assassination;6 3:Unholy;9 1:Affliction;' +
                '10 1:Oathblade'
        }
        $checkProfiles = if ($profiles) { $profiles } else { '258:0:10:single+pack5,460:650:defi10-25:boss+pack5' }
        $checkModes = if ($arguments) { @($arguments) } else { @('off', 'on') }
        Invoke-SimValidation @(ConvertTo-SweepProfiles $checkProfiles $layouts) @(ConvertTo-SweepSpecs $checkSpecs) `
            $checkGoal $worker $checkModes
    }
    { $_ -in 'sweep', 'tune' } {
        $runGoal = if ($goal) { $goal } else { "$seconds" }
        $health = if ($laneHealth -gt 0) { $laneHealth } elseif ($runGoal.EndsWith('%')) { 25 } else { 100 }
        $specList = @(ConvertTo-SweepSpecs $specs)
        $outputDirectory = Join-Path $repositoryRoot 'var\combatBench'
        if ($action -eq 'sweep') {
            # Single target and packs at each end of the paragon and between (the raid profiles' boss dummy stays up)
            $sweepProfiles = if ($profiles) { $profiles } else {
                '258:0:10:single+pack5+pack12,450:600:defi10-25:boss+pack5+pack12,460:650:defi10-25:boss+pack5+pack12'
            }
            Invoke-SimSweep @(ConvertTo-SweepProfiles $sweepProfiles $layouts) $specList $runGoal $health `
                $outputDirectory
        } else {
            $tuneProfiles = if ($profiles) { $profiles } else { '258:0,460:650' }
            $settings = [pscustomobject]@{
                Copies = $copies; Repeats = $repeats; Lanes = 30; LaneHealth = $health; Goal = $runGoal
                Preset = $preset; IlvlTolerance = $ilvlTolerance; Target = $target; Band = $band; Damping = $damping
                MaxStep = $maxStep; Iterations = $iterations
            }
            $checks = if ($validate) { @(ConvertTo-SweepProfiles $validate $layouts) } else { @() }
            Invoke-SimTune @(ConvertTo-SweepProfiles $tuneProfiles $layouts) $checks $specList $settings `
                $outputDirectory
        }
    }
}
