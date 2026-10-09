# The combat bench's sweep and auto-tune (bench.ps1 sweep / tune): every damage spec measured at once, each bot alone
# on its own dummies (`.bench lanes`, a phase a lane), at a list of profiles (item level : paragon), then summed up
# against the Fire mage; the auto-tune moves the spec balance knobs (balance0.* from the no-paragon profile, balance.*
# from the 650 one) toward the mage, live. Dot-sourced by bench.ps1 after its Send-Request, and by the simulation bench
# (localTools/simBench/simBench.ps1: a heat a simulation worker, Invoke-SweepHeat / Invoke-TuneHeat).
# Guide: .agents/docs/systems/combat-bench.md ("Fast survey: the sweep").

$Invariant = [System.Globalization.CultureInfo]::InvariantCulture
$FireMageKey = '8 2'

# Every damage spec a sweep measures by default: the `.bench bot` class id and premade spec number (numbers, so the
# rows map back without ambiguity) and a name. Healers and tanks keep their own tests (pulse, tank layouts).
$DefaultSweepSpecs = @(
    '1 1:Arms', '1 2:Fury', '1 4:Gladiator', '2 3:Retribution',
    '3 1:Beast Mastery', '3 2:Marksmanship', '3 3:Survival',
    '4 1:Assassination', '4 2:Outlaw', '4 3:Subtlety', '5 3:Shadow',
    '6 2:Frost DK', '6 3:Unholy', '7 1:Elemental', '7 2:Enhancement',
    '8 1:Arcane', '8 2:Fire', '8 3:Frost mage',
    '9 1:Affliction', '9 2:Demonology', '9 3:Destruction',
    '11 1:Balance', '11 4:Feral cat', '10 1:Oathblade',
    '14 1:Barbarian Brutality', '14 2:Barbarian Headhunter', '14 3:Barbarian Ascendance',
    '15 1:Reaper Harvest', '15 2:Reaper Soul')

function Format-Number([double]$value, [string]$format = '0.00') { $value.ToString($format, $Invariant) }
function ConvertTo-Double([string]$text) {
    $value = 0.0
    $style = [System.Globalization.NumberStyles]::Float
    if ([double]::TryParse($text, $style, $Invariant, [ref]$value)) { return $value }
    0.0
}

# --- Inputs ----------------------------------------------------------------------------------------------------------

# "ilvl:paragon[:scaling[:layouts]]": scaling by default the +10 key at no paragon, the Défi X 25 boss otherwise;
# layouts ("+"-separated) by default single target on a key, the boss dummy (that stays up) on a raid scaling
function ConvertTo-SweepProfiles([string]$text, [string]$layoutsOverride) {
    foreach ($part in ($text.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        $fields = $part.Split(':')
        if ($fields.Count -lt 2) { throw "Profile '$part': ilvl:paragon[:scaling[:layouts]]" }
        $paragon = [int]$fields[1]
        $scaling = if ($fields.Count -ge 3 -and $fields[2]) { $fields[2] }
                   elseif ($paragon -eq 0) { '10' } else { 'defi10-25' }
        $layouts = if ($layoutsOverride) { $layoutsOverride.Split(',') }
                   elseif ($fields.Count -ge 4 -and $fields[3]) { $fields[3].Split('+') }
                   elseif ($scaling -match '^(m?\+?\d+)$') { @('single') } else { @('boss') }
        [pscustomobject]@{
            Ilvl = [int]$fields[0]; Paragon = $paragon; Scaling = $scaling
            Layouts = @($layouts | ForEach-Object { $_.Trim() } | Where-Object { $_ })
            Label = "$($fields[0])/$paragon"
        }
    }
}

# ";"-separated "<class id> <spec number>:<name>", or the default list
function ConvertTo-SweepSpecs([string]$text) {
    $entries = if ($text) { $text.Split(';') } else { $DefaultSweepSpecs }
    foreach ($entry in ($entries | ForEach-Object { $_.Trim() } | Where-Object { $_ })) {
        $bot, $name = $entry.Split([char[]]':', 2)
        $words = $bot.Trim().Split([char[]]' ', [System.StringSplitOptions]::RemoveEmptyEntries)
        if ($words.Count -ne 2) { throw "Spec '$entry': '<class id> <spec number>:<name>'" }
        if (-not $name) { $name = $bot.Trim() }
        $class = [int]$words[0]
        $spec = [int]$words[1]
        [pscustomobject]@{ Class = $class; Spec = $spec; Name = $name.Trim(); Key = "$class $spec" }
    }
}

# The heats: as many lanes as the bench takes, each spec's copies in the same heat (the auto-tune settles a heat's
# specs with its bots brought once), and the Fire mage's copies in every heat (the reference, pooled over all of them)
function New-SweepHeats($specs, [int]$copies, [int]$lanes) {
    $fire = @($specs | Where-Object { $_.Key -eq $FireMageKey } | Select-Object -First 1)
    if (-not $fire) { $fire = @([pscustomobject]@{ Class = 8; Spec = 2; Name = 'Fire'; Key = $FireMageKey }) }
    $others = @($specs | Where-Object { $_.Key -ne $FireMageKey })
    $perHeat = [Math]::Floor(($lanes - $copies) / $copies)
    if ($perHeat -lt 1) { throw "-lanes $lanes leaves no room for -copies $copies beside the Fire mage" }
    $heatCount = [Math]::Max(1, [Math]::Ceiling($others.Count / $perHeat))
    $size = [Math]::Ceiling($others.Count / $heatCount)
    for ($heat = 0; $heat -lt $heatCount; ++$heat) {
        $chosen = @($fire) + @($others | Select-Object -Skip ($heat * $size) -First $size)
        $entries = foreach ($spec in $chosen) { for ($copy = 0; $copy -lt $copies; ++$copy) { $spec } }
        , @($entries)
    }
}

# --- Session steps ---------------------------------------------------------------------------------------------------

# The bots' talent build: single, aoe, auto, or layout (the default) - each layout fought on its own build, the packs
# on the area one and the single targets on the other (.bench preset switches them in place between runs)
function Get-LayoutBuild([string]$layout) {
    if ($layout -match '^(pack\d+|spread\d+|tankpack)$') { 'aoe' } else { 'single' }
}

function Send-SweepBots($heat, [int]$ilvl, [string]$preset) {
    $script:SweepPreset = $preset
    $script:SweepBuild = if ($preset -eq 'layout') { 'single' } else { $preset }
    $line = ($heat | ForEach-Object { "$($_.Class) $($_.Spec) $ilvl $($script:SweepBuild)" }) -join ';'
    $answer = Send-Request @("bots $line") (180 + 8 * $heat.Count)
    $state = $answer | Where-Object { $_ -match '#benchstate;bots=(\d+)' } | Select-Object -Last 1
    $count = if ($state -match 'bots=(\d+)') { [int]$Matches[1] } else { -1 }
    if ($count -ge 0 -and $count -lt $heat.Count) {
        Write-Warning "Only $count of $($heat.Count) bots came (content bot pool?); their specs have no row this heat."
        $answer | Where-Object { $_ -match 'could not|no content bot|too many|unknown spec' } |
            ForEach-Object { Write-Warning $_ }
    }
}

# A profile on the bots already there: their gear (in place, no new login) and their paragon board
function Set-SweepProfile($prof, [ref]$currentIlvl) {
    $lines = @()
    if ($currentIlvl.Value -ne $prof.Ilvl) {
        $lines += ".bench gear $($prof.Ilvl)", 'settle 300'
        $currentIlvl.Value = $prof.Ilvl
    }
    $lines += ".bench paragon $($prof.Paragon)"
    Send-Request $lines 360 | Out-Null
}

function Invoke-SweepRun([string]$layout, $prof, [string]$goal) {
    $timeout = if ($goal.EndsWith('%')) { 900 } else { [int]$goal.TrimEnd('s') + 240 }
    $lines = @()
    if ($script:SweepPreset -eq 'layout' -and $script:SweepBuild -ne (Get-LayoutBuild $layout)) {
        $script:SweepBuild = Get-LayoutBuild $layout
        $lines += ".bench preset $($script:SweepBuild)"
    }
    Send-Request ($lines + @("run $layout $($prof.Scaling) $goal")) $timeout
}

function Exit-SweepLanes {
    try { Send-Request @('.bench dismiss all', '.bench lanes off') 60 | Out-Null } catch { Write-Warning $_ }
}

# --- Rows ------------------------------------------------------------------------------------------------------------

# The #sweep rows of a run's answer (CombatBench.cpp BuildLaneReport), each checked: a row at the wrong item level,
# with a tank's board, without the board asked for, on another spec than asked or with no damage is not trusted
function ConvertFrom-SweepRows($answer, $prof, $specs, [string]$context, [int]$ilvlTolerance) {
    foreach ($line in $answer) {
        if ("$line" -notmatch '#sweep;(.*)$') { continue }
        $f = $Matches[1].Split(';')
        if ($f.Count -lt 23) { continue }
        $class = [int]$f[7]
        $asked = [int]$f[8]
        $key = "$class $asked"
        $spec = $specs | Where-Object { $_.Key -eq $key } | Select-Object -First 1
        $damage = ConvertTo-Double $f[19]
        $ms = ConvertTo-Double $f[20]
        $board = $f[13]
        $flags = @()
        $ilvl = ConvertTo-Double $f[12]
        if ([Math]::Abs($ilvl - [double]$f[11]) -gt $ilvlTolerance) { $flags += 'ilvl' }
        if ([int]$f[9] -ne $asked) { $flags += 'spec' }
        if ($board -match '^board tank') { $flags += 'tank board' }
        $points = if ($board -match '^board \S+ (\d+) pts') { [int]$Matches[1] } else { 0 }
        if ($board -eq 'offline') { $flags += 'offline' }
        elseif ($points -ne [int]$f[4]) { $flags += 'board' }
        if ($damage -le 0) { $flags += 'zero damage' }
        [pscustomobject]@{
            Context = $context; Profile = $prof.Label; Run = $f[0]; Layout = $f[1]; Scaling = $f[2]; Goal = $f[3]
            Paragon = [int]$f[4]; Lane = [int]$f[5]; Name = $f[6]; Key = $key
            Spec = if ($spec) { $spec.Name } else { $key }; Label = $f[10]
            AskedIlvl = [int]$f[11]; Ilvl = $ilvl; Board = $board
            LowKey = $f[14]; Low = ConvertTo-Double $f[15]; HighKey = $f[16]; High = ConvertTo-Double $f[17]
            Share = ConvertTo-Double $f[18]
            Damage = $damage; Seconds = $ms / 1000.0; Dps = if ($ms -gt 0) { $damage * 1000.0 / $ms } else { 0.0 }
            Done = $f[21] -eq '1'; Deaths = [int]$f[22]
            Trusted = $flags.Count -eq 0; Flags = $flags -join ','
        }
    }
}

function Get-Stats([double[]]$values) {
    $n = $values.Count
    if ($n -eq 0) { return [pscustomobject]@{ N = 0; Mean = 0.0; Sd = 0.0 } }
    $mean = ($values | Measure-Object -Average).Average
    $sum = 0.0
    foreach ($value in $values) { $sum += ($value - $mean) * ($value - $mean) }
    $sd = if ($n -gt 1) { [Math]::Sqrt($sum / ($n - 1)) } else { 0.0 }
    [pscustomobject]@{ N = $n; Mean = $mean; Sd = $sd }
}

# Per profile, layout and spec: mean DPS of the trusted rows, its share of the Fire mage's mean, the spread (one
# standard deviation, % of the mean), the share's standard error, and how many rows (and untrusted ones)
function Measure-Sweep($rows) {
    foreach ($group in ($rows | Group-Object Profile, Layout)) {
        $first = $group.Group[0]
        $mage = Get-Stats @($group.Group | Where-Object { $_.Key -eq $FireMageKey -and $_.Trusted } |
            ForEach-Object { $_.Dps })
        foreach ($spec in ($group.Group | Group-Object Key)) {
            $trusted = @($spec.Group | Where-Object { $_.Trusted })
            $stats = Get-Stats @($trusted | ForEach-Object { $_.Dps })
            $share = if ($mage.Mean -gt 0) { $stats.Mean / $mage.Mean } else { 0.0 }
            $relative = if ($stats.Mean -gt 0 -and $stats.N -gt 0) { $stats.Sd / $stats.Mean } else { 0.0 }
            $mageRelative = if ($mage.Mean -gt 0 -and $mage.N -gt 0) { $mage.Sd / $mage.Mean } else { 0.0 }
            $shareError = if ($stats.N -gt 0 -and $mage.N -gt 0) {
                $share * [Math]::Sqrt($relative * $relative / $stats.N + $mageRelative * $mageRelative / $mage.N)
            } else { 0.0 }
            $flags = @($spec.Group | Where-Object { -not $_.Trusted } | ForEach-Object { "$($_.Name): $($_.Flags)" })
            $last = $spec.Group[-1]
            [pscustomobject]@{
                Profile = $first.Profile; Layout = $first.Layout; Scaling = $first.Scaling; Spec = $last.Spec
                Key = $spec.Name; Dps = $stats.Mean; Share = $share; ShareError = $shareError
                Spread = $relative; N = $stats.N; Untrusted = $flags.Count; Flags = $flags -join ' | '
                LowKey = $last.LowKey; Low = $last.Low; HighKey = $last.HighKey; High = $last.High
                MageDps = $mage.Mean; MageN = $mage.N
            }
        }
    }
}

function Show-SweepSummary($summary) {
    foreach ($group in ($summary | Group-Object Profile, Layout)) {
        $first = $group.Group[0]
        "`n== {0} ({1}) - {2} - Fire mage {3} DPS over {4} rows ==" -f $first.Profile, $first.Scaling, $first.Layout,
            (Format-Number $first.MageDps '0'), $first.MageN
        $group.Group | Sort-Object Share -Descending | ForEach-Object {
            $mark = if ($_.N -eq 0) { '  NO TRUSTED ROW' }
                    elseif ($_.Untrusted) { "  ($($_.Untrusted) untrusted)" } else { '' }
            '{0,-26} {1,8} DPS  {2,5}% of the mage  +/-{3,3}% (share +/-{4,2})  n={5}{6}' -f $_.Spec,
                (Format-Number $_.Dps '0'), (Format-Number (100 * $_.Share) '0'), (Format-Number (100 * $_.Spread) '0'),
                (Format-Number (100 * $_.ShareError) '0'), $_.N, $mark
        }
        $group.Group | Where-Object { $_.Untrusted } | ForEach-Object { "   untrusted $($_.Spec): $($_.Flags)" }
    }
}

# A CSV written with invariant numbers (a French Windows would write 1,23 in a comma-separated file)
function Export-InvariantCsv($objects, [string]$path) {
    $objects = @($objects)
    if (-not $objects.Count) { return }
    $names = $objects[0].PSObject.Properties.Name
    $quote = { param($text) '"' + ("$text" -replace '"', '""') + '"' }
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add(($names | ForEach-Object { & $quote $_ }) -join ',')
    foreach ($object in $objects) {
        $lines.Add(($names | ForEach-Object {
            $value = $object.$_
            if ($value -is [double] -or $value -is [single]) { $value.ToString('0.####', $Invariant) }
            elseif ($value -is [int] -or $value -is [bool]) { "$value" }
            else { & $quote $value }
        }) -join ',')
    }
    [System.IO.File]::WriteAllLines($path, $lines, [System.Text.UTF8Encoding]::new($false))
}

function Format-Clock([TimeSpan]$span) { '{0}:{1:00}' -f [int][Math]::Floor($span.TotalMinutes), $span.Seconds }

# --- The sweep -------------------------------------------------------------------------------------------------------

# One heat (lanes on): its bots brought once, then every profile (geared in place, its board), layout and repeat.
# Returns its rows.
function Invoke-SweepHeat($heat, [string]$context, $profiles, $specs, [int]$repeats, [string]$goal, [string]$preset,
    [int]$ilvlTolerance, $clock) {
    $heatClock = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host ("[{0}] bringing {1} bots at ilvl {2}..." -f $context, $heat.Count, $profiles[0].Ilvl)
    Send-SweepBots $heat $profiles[0].Ilvl $preset
    Write-Host "[$context] bots in after $(Format-Clock $heatClock.Elapsed)"
    # Geared again in place for the first profile too: a content bot can keep a former login's gear (one at
    # 270 asked 258, another at 225)
    $currentIlvl = 0
    foreach ($prof in $profiles) {
        Set-SweepProfile $prof ([ref]$currentIlvl)
        foreach ($layout in $prof.Layouts) {
            for ($repeat = 1; $repeat -le $repeats; ++$repeat) {
                $answer = Invoke-SweepRun $layout $prof $goal
                $found = @(ConvertFrom-SweepRows $answer $prof $specs $context $ilvlTolerance)
                if (-not $found.Count) {
                    Write-Warning 'No sweep row in that run:'
                    $answer | Select-Object -Last 8 | ForEach-Object { Write-Warning $_ }
                }
                $found
                $untrusted = @($found | Where-Object { -not $_.Trusted }).Count
                Write-Host ("[{0}] {1} {2} #{3}: {4} rows, {5} untrusted ({6})" -f $context, $prof.Label, $layout,
                    $repeat, $found.Count, $untrusted, (Format-Clock $clock.Elapsed))
            }
        }
    }
}

function Invoke-BenchSweep($profiles, $specs, [int]$copies, [int]$repeats, [int]$lanes, [int]$laneHealth,
    [string]$goal, [string]$preset, [int]$ilvlTolerance, [string]$outputDirectory) {
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $heats = @(New-SweepHeats $specs $copies $lanes)
    $runs = 0
    foreach ($prof in $profiles) { $runs += $prof.Layouts.Count * $repeats * $heats.Count }
    Write-Host ("Sweep: {0} specs x {1}, {2} heat(s) of up to {3} lanes, {4} profile(s), {5} run(s) of {6}" -f
        @($specs).Count, $copies, $heats.Count, $lanes, @($profiles).Count, $runs, $goal)
    $rows = [System.Collections.Generic.List[object]]::new()
    Send-Request @(".bench lanes on $laneHealth") 30 | Out-Null
    try {
        for ($index = 0; $index -lt $heats.Count; ++$index) {
            Invoke-SweepHeat $heats[$index] "heat$($index + 1)" $profiles $specs $repeats $goal $preset $ilvlTolerance `
                $clock | ForEach-Object { $rows.Add($_) }
        }
    } finally {
        Exit-SweepLanes
    }

    $summary = @(Measure-Sweep $rows)
    Show-SweepSummary $summary
    $stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Export-InvariantCsv $rows (Join-Path $outputDirectory "sweep-$stamp-rows.csv")
    Export-InvariantCsv $summary (Join-Path $outputDirectory "sweep-$stamp.csv")
    "`nSweep: $runs run(s), $($rows.Count) rows, wall clock $(Format-Clock $clock.Elapsed)."
    "CSV: var\combatBench\sweep-$stamp.csv (rows: sweep-$stamp-rows.csv)"
}

# --- The auto-tune ---------------------------------------------------------------------------------------------------

function Get-TuneStatus($entry, [bool]$tunable) {
    if ($null -eq $entry.Share) { 'no trusted row' }
    elseif (-not $tunable -or -not $entry.Knob -or $entry.Knob -eq '-') { 'no knob' }
    elseif ($entry.Noisy) { 'in noise' }
    elseif ($entry.Settled) { 'in band' }
    elseif ($entry.Stuck -ge 2) { 'stuck' }
    elseif ($entry.Steps) { 'moved' }
    else { 'unchanged' }
}

# Each tunable spec of a heat toward target x the Fire mage at one profile: the no-paragon profile moves balance0.*,
# the 650 one balance.* (the factor goes from one to the other by the character's points, so each end is its own).
# A step: factor x (target / share) ^ damping, at most maxStep either way, measured again on the next iteration; a
# spec within the band is settled; one whose gap grew twice running (noise, or a factor that does not move it) is
# stuck and left. The mage's samples pool over every iteration and heat (its own factor never moves). Returns a row
# per spec: its knob, old -> new, its last measured share.
function Invoke-TuneProfile($prof, $heat, $specs, $mageSamples, $rows, $settings, [string]$context) {
    $knobSide = if ($prof.Paragon -eq 0) { 'Low' } elseif ($prof.Paragon -ge 650) { 'High' } else { $null }
    $state = @{}
    foreach ($spec in ($heat | Where-Object { $_.Key -ne $FireMageKey } | Sort-Object Key -Unique)) {
        $state[$spec.Key] = [pscustomobject]@{
            Spec = $spec.Name; Settled = $false; Noisy = $false; Stuck = 0; LastGap = $null; Knob = $null; Start = $null
            Value = $null; Before = $null; Share = $null; ShareError = 0.0; N = 0; Steps = 0
        }
    }
    for ($iteration = 1; $iteration -le $settings.Iterations; ++$iteration) {
        $found = [System.Collections.Generic.List[object]]::new()
        foreach ($layout in $prof.Layouts) {
            for ($repeat = 1; $repeat -le $settings.Repeats; ++$repeat) {
                $answer = Invoke-SweepRun $layout $prof $settings.Goal
                ConvertFrom-SweepRows $answer $prof $specs "$context/it$iteration" $settings.IlvlTolerance |
                    ForEach-Object { $found.Add($_); $rows.Add($_) }
            }
        }
        $found | Where-Object { $_.Key -eq $FireMageKey -and $_.Trusted } | ForEach-Object { $mageSamples.Add($_.Dps) }
        $mage = Get-Stats @($mageSamples)
        if ($mage.Mean -le 0) {
            Write-Warning 'No trusted Fire mage row yet: nothing to tune against.'
            continue
        }

        $commands = @()
        foreach ($key in @($state.Keys)) {
            $entry = $state[$key]
            $mine = @($found | Where-Object { $_.Key -eq $key })
            if (-not $mine.Count) { continue }
            $stats = Get-Stats @($mine | Where-Object { $_.Trusted } | ForEach-Object { $_.Dps })
            $last = $mine[-1]
            if ($knobSide) {
                $entry.Knob = $last."${knobSide}Key"
                if ($null -eq $entry.Start) { $entry.Start = $last.$knobSide }
                $entry.Value = $last.$knobSide
            }
            $entry.Before = $entry.Value
            if (-not $stats.N) { continue }
            $entry.Share = $stats.Mean / $mage.Mean
            $entry.N = $stats.N
            $relative = if ($stats.Mean -gt 0) { $stats.Sd / $stats.Mean } else { 0.0 }
            $entry.ShareError = $entry.Share * $relative / [Math]::Sqrt($stats.N)
            if (-not $knobSide -or -not $entry.Knob -or $entry.Knob -eq '-') { continue }

            # Within the band, or within the measure's own noise (one standard error: more repeats narrow it)
            $gap = $entry.Share / $settings.Target - 1.0
            $entry.Noisy = [Math]::Abs($gap) -gt $settings.Band -and
                [Math]::Abs($gap) -le $entry.ShareError / $settings.Target
            $entry.Settled = [Math]::Abs($gap) -le $settings.Band -or $entry.Noisy
            if ($entry.Settled) { continue }
            if ($null -ne $entry.LastGap -and [Math]::Abs($gap) -ge [Math]::Abs($entry.LastGap)) { ++$entry.Stuck }
            else { $entry.Stuck = 0 }
            $entry.LastGap = $gap
            if ($entry.Stuck -ge 2) { continue }
            $step = [Math]::Pow($settings.Target / [Math]::Max($entry.Share, 0.05), $settings.Damping)
            $step = [Math]::Min([Math]::Max($step, 1.0 / (1.0 + $settings.MaxStep)), 1.0 + $settings.MaxStep)
            $new = [Math]::Round([Math]::Min([Math]::Max($entry.Value * $step, 0.2), 4.0), 2)
            if ([Math]::Abs($new - $entry.Value) -lt 0.005) { continue }
            $commands += ".tune set $($entry.Knob) $(Format-Number $new '0.00')"
            $entry.Value = $new
            ++$entry.Steps
        }

        Write-Host ("`n[{0}] {1} iteration {2}: Fire mage {3} DPS ({4} rows, all iterations)" -f $context,
            $prof.Label, $iteration, (Format-Number $mage.Mean '0'), $mage.N)
        foreach ($entry in ($state.Values | Sort-Object Spec)) {
            $factor = if ($null -eq $entry.Before) { '' }
                      elseif ($entry.Value -ne $entry.Before) {
                          "$(Format-Number $entry.Before '0.00') -> $(Format-Number $entry.Value '0.00')"
                      } else { Format-Number $entry.Before '0.00' }
            $share = if ($null -eq $entry.Share) { '   -' } else { Format-Number (100 * $entry.Share) '0' }
            Write-Host ('   {0,-26} {1,4}% (+/-{2,2}) n={3}  {4} {5}  {6}' -f $entry.Spec, $share,
                (Format-Number (100 * $entry.ShareError) '0'), $entry.N, $entry.Knob, $factor,
                (Get-TuneStatus $entry ([bool]$knobSide)))
        }
        if (-not $commands.Count) {
            Write-Host '   every spec in band, stuck or without a knob: done'
            break
        }
        Send-Request $commands 60 | Out-Null
    }
    foreach ($entry in $state.Values) {
        [pscustomobject]@{
            Profile = $prof.Label; Spec = $entry.Spec; Knob = $entry.Knob; Old = $entry.Start; New = $entry.Value
            Share = $entry.Share; ShareError = $entry.ShareError; N = $entry.N; Steps = $entry.Steps
            Status = Get-TuneStatus $entry ([bool]$knobSide)
        }
    }
}

function Invoke-BenchTune($tuneProfiles, $validateProfiles, $specs, $settings, [string]$outputDirectory) {
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    foreach ($prof in $tuneProfiles) {
        if ($prof.Paragon -ne 0 -and $prof.Paragon -lt 650) {
            throw "Tune profile $($prof.Label): paragon 0 (balance0.*) or 650+ (balance.*); others go in -validate"
        }
    }
    $heats = @(New-SweepHeats $specs $settings.Copies $settings.Lanes)
    Write-Host (("Auto-tune: {0} specs x {1}, {2} heat(s), target {3}% of the Fire mage +/-{4}%, up to {5} " +
        "iteration(s) a profile") -f @($specs).Count, $settings.Copies, $heats.Count,
        (Format-Number (100 * $settings.Target) '0'), (Format-Number (100 * $settings.Band) '0'), $settings.Iterations)
    $rows = [System.Collections.Generic.List[object]]::new()
    $mageSamples = @{}
    foreach ($prof in @($tuneProfiles) + @($validateProfiles)) {
        $mageSamples[$prof.Label] = [System.Collections.Generic.List[double]]::new()
    }
    $changes = [System.Collections.Generic.List[object]]::new()
    Send-Request @(".bench lanes on $($settings.LaneHealth)") 30 | Out-Null
    try {
        for ($index = 0; $index -lt $heats.Count; ++$index) {
            Invoke-TuneHeat $heats[$index] "heat$($index + 1)" $tuneProfiles $validateProfiles $specs $settings `
                $mageSamples $rows $changes $clock
        }
    } finally {
        Exit-SweepLanes
    }

    Show-TuneResults $changes $rows
    $stamp = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    New-Item -ItemType Directory -Force $outputDirectory | Out-Null
    Export-InvariantCsv $rows (Join-Path $outputDirectory "tune-$stamp-rows.csv")
    Export-InvariantCsv $changes (Join-Path $outputDirectory "tune-$stamp.csv")
    "`nAuto-tune: wall clock $(Format-Clock $clock.Elapsed)."
    "CSV: var\combatBench\tune-$stamp.csv (rows: tune-$stamp-rows.csv)"
    'Overrides stay live (.tune list balance). Check, then: python localTools/tuning/bakeTuning.py --dry-run'
}

# One heat of the auto-tune (lanes on): its bots brought once, each tune profile settled in turn, then the validation
# profiles measured once. Adds to rows and changes; the Fire mage's samples pool by profile over the heats given.
function Invoke-TuneHeat($heat, [string]$context, $tuneProfiles, $validateProfiles, $specs, $settings, $mageSamples,
    $rows, $changes, $clock) {
    $first = @($tuneProfiles)[0]
    Write-Host ("`n[{0}] bringing {1} bots at ilvl {2}... ({3})" -f $context, $heat.Count, $first.Ilvl,
        (Format-Clock $clock.Elapsed))
    Send-SweepBots $heat $first.Ilvl $settings.Preset
    # Geared again in place for the first profile too: a content bot can keep a former login's gear (one at
    # 270 asked 258, another at 225)
    $currentIlvl = 0
    foreach ($prof in $tuneProfiles) {
        $profileClock = [System.Diagnostics.Stopwatch]::StartNew()
        Set-SweepProfile $prof ([ref]$currentIlvl)
        Invoke-TuneProfile $prof $heat $specs $mageSamples[$prof.Label] $rows $settings $context |
            ForEach-Object { $changes.Add($_) }
        Write-Host "[$context] $($prof.Label) tuned in $(Format-Clock $profileClock.Elapsed)"
    }
    foreach ($prof in $validateProfiles) {
        Set-SweepProfile $prof ([ref]$currentIlvl)
        foreach ($layout in $prof.Layouts) {
            for ($repeat = 1; $repeat -le $settings.Repeats; ++$repeat) {
                $answer = Invoke-SweepRun $layout $prof $settings.Goal
                ConvertFrom-SweepRows $answer $prof $specs "$context/validate" $settings.IlvlTolerance |
                    ForEach-Object { $rows.Add($_) }
            }
        }
    }
    Write-Host "[$context] done after $(Format-Clock $clock.Elapsed)"
}

# The tune's summary: every knob moved (old -> new, the last share), then the validation profiles' sweep
function Show-TuneResults($changes, $rows) {
    "`n== Spec balance factors (live overrides; bake: python localTools/tuning/bakeTuning.py --dry-run) =="
    "   moved: its last step not measured again; in noise: off the band by less than its standard error (more"
    "   -repeats); stuck: its gap grew twice running (look at its rotation and rows first)"
    $changes | Where-Object { $_.Knob -and $_.Knob -ne '-' } | Sort-Object Knob | ForEach-Object {
        '{0,-26} {1,-24} {2} -> {3}   {4,4}% of the mage (+/-{5}, n={6})  {7}' -f $_.Spec, $_.Knob,
            (Format-Number ([double]$_.Old) '0.00'), (Format-Number ([double]$_.New) '0.00'),
            (Format-Number (100 * [double]$_.Share) '0'), (Format-Number (100 * [double]$_.ShareError) '0'), $_.N,
            $_.Status
    }
    $checks = @($rows | Where-Object { $_.Context -like '*/validate' })
    if ($checks.Count) {
        "`n== Validation (no tuning) =="
        Show-SweepSummary @(Measure-Sweep $checks)
    }
}
