# One simulation worker's host (started hidden by simBench.ps1 start): runs the worker's world server (var\simBench\bin
# simworld.exe, Sim.Enable) with its console on pipes, and answers the request files of its folder in the bench
# session's language (bench.ps1, e2e/tools/bench) - the bench driven from the console instead of a GM client:
#
#   .<command>                        a console command and what the bench says back
#   bots <a;b;...> [pulse=<percent>]  the bench bots replaced by these (`.bench bot` arguments), once they settled
#   run <layouts> [key] [seconds|N%]  the layouts in turn, their reports and RESULT <layout> <run id> lines
#   settle [seconds]                  until no bench bot is logging in or being geared, then `.bench list`
#   wait <seconds>
#   quit                              ends the worker
param(
    [Parameter(Mandatory = $true)][int]$worker
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'SimCommon.ps1')

$directory = Get-SimWorkerDirectory $worker
$hostLog = Join-Path $directory 'host.log'
$readyFile = Join-Path $directory 'ready'
Remove-Item $readyFile -ErrorAction SilentlyContinue
Get-ChildItem $directory -Filter '*.txt' -ErrorAction SilentlyContinue | Remove-Item
Set-Content (Join-Path $directory 'host.pid') $PID
$logWriter = [System.IO.StreamWriter]::new($hostLog, $false, [System.Text.UTF8Encoding]::new($false))
$logWriter.AutoFlush = $true

function Write-HostLog([string]$text) { $logWriter.WriteLine("$(Get-Date -Format 'HH:mm:ss.fff') $text") }

# --- The world server and its console --------------------------------------------------------------------------------

$start = [System.Diagnostics.ProcessStartInfo]::new((Join-Path $simBin 'simworld.exe'))
$start.WorkingDirectory = $directory
$start.UseShellExecute = $false
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.CreateNoWindow = $true
$start.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
$server = [System.Diagnostics.Process]::Start($start)
Set-Content (Join-Path $directory 'server.pid') $server.Id
Write-HostLog "world server started, pid $($server.Id)"
$stdin = $server.StandardInput
$stdin.AutoFlush = $true

# Every line the server printed, in order; the bench's own after "[bench] "
$script:lines = [System.Collections.Generic.List[string]]::new()
$script:pendingRead = $server.StandardOutput.ReadLineAsync()
$script:pendingError = $server.StandardError.ReadLineAsync()

# Takes whatever the server printed since the last call (never blocks: its pipe must be drained, or it stalls)
function Read-ServerOutput {
    while ($script:pendingRead -and $script:pendingRead.IsCompleted) {
        $line = $script:pendingRead.Result
        if ($null -eq $line) { $script:pendingRead = $null; break }
        # The console's prompt lands at the start of whatever is printed next
        $line = $line -replace '^(AC>\s*)+', ''
        $script:lines.Add($line)
        Write-HostLog "< $line"
        $script:pendingRead = $server.StandardOutput.ReadLineAsync()
    }
    while ($script:pendingError -and $script:pendingError.IsCompleted) {
        $line = $script:pendingError.Result
        if ($null -eq $line) { $script:pendingError = $null; break }
        Write-HostLog "<! $line"
        $script:pendingError = $server.StandardError.ReadLineAsync()
    }
}

function Send-ServerLine([string]$line) {
    Write-HostLog "> $line"
    $stdin.WriteLine($line)
}

# Waits for a printed line matching the pattern after index `from`; returns its index (-1 on timeout)
function Wait-ServerLine([int]$from, [string]$pattern, [double]$timeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    $index = $from
    while ($true) {
        Read-ServerOutput
        for (; $index -lt $script:lines.Count; ++$index) {
            if ($script:lines[$index] -match $pattern) { return $index }
        }
        if ($server.HasExited) { throw "The world server exited (code $($server.ExitCode))" }
        if ((Get-Date) -gt $deadline) { return -1 }
        Start-Sleep -Milliseconds 20
    }
}

$script:pingCount = 0

# A console command and what the bench said back: every line printed until its ping comes back (the console answers
# in order, on the world's update)
function Invoke-ServerCommand([string]$command, [double]$timeoutSeconds = 30) {
    $from = $script:lines.Count
    $token = "p$((++$script:pingCount))"
    Send-ServerLine $command
    Send-ServerLine ".bench ping $token"
    $at = Wait-ServerLine $from ([regex]::Escape("#pong;$token")) $timeoutSeconds
    $until = if ($at -ge 0) { $at } else { $script:lines.Count }
    $replies = for ($index = $from; $index -lt $until; ++$index) { $script:lines[$index] }
    # The config's missing-key notes are no answer
    $replies |
        Where-Object { $_.Trim() -and $_ -notmatch '^> Config: Missing property' } |
        ForEach-Object { $_ -replace '^\[bench\] ', '' }
}

function Get-BenchState {
    $replies = @(Invoke-ServerCommand '.bench list')
    $line = $replies | Where-Object { $_ -match '#benchstate;(\S+)' } | Select-Object -Last 1
    $state = @{}
    if ($line -match '#benchstate;(\S+)') {
        foreach ($field in $Matches[1].Split(';')) {
            $key, $value = $field.Split('=', 2)
            $state[$key] = $value
        }
    }
    [pscustomobject]@{ State = $state; Lines = $replies }
}

# Until no bench bot is logging in or being geared
function Wait-BenchSettled([double]$timeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $state = Get-BenchState
        if ($state.State['pending'] -eq '0' -and $state.State['gearing'] -eq '0') { return $state }
        Start-Sleep -Milliseconds 250
    }
    Write-HostLog "the bench bots did not settle in $timeoutSeconds s"
    Get-BenchState
}

# --- Requests --------------------------------------------------------------------------------------------------------

function Invoke-Request([string]$content) {
    $out = [System.Collections.Generic.List[string]]::new()
    $quit = $false
    foreach ($raw in ($content.TrimStart([char]0xFEFF) -replace "`r", '').Split("`n")) {
        $line = $raw.Trim()
        if (-not $line) { continue }
        $fields = $line.Split(' ', [System.StringSplitOptions]::RemoveEmptyEntries)
        if ($line.StartsWith('.')) {
            Invoke-ServerCommand $line | ForEach-Object { $out.Add("[$($fields[0])] $_") }
        }
        elseif ($fields[0] -eq 'settle') {
            $timeout = if ($fields.Count -ge 2) { [double]$fields[1] } else { 180 }
            (Wait-BenchSettled $timeout).Lines | ForEach-Object { $out.Add("[settle] $_") }
        }
        elseif ($fields[0] -eq 'bots') {
            Invoke-ServerCommand '.bench dismiss all' | Out-Null
            $arguments = $line.Substring(4).Trim()
            $pulse = ''
            $at = $arguments.LastIndexOf(' pulse=')
            if ($at -ge 0) {
                $pulse = $arguments.Substring($at + 7).Trim()
                $arguments = $arguments.Substring(0, $at)
            }
            $count = 0
            foreach ($bot in $arguments.Split(';')) {
                if (-not $bot.Trim()) { continue }
                Invoke-ServerCommand ".bench bot $($bot.Trim())" | ForEach-Object { $out.Add("[setup] $_") }
                ++$count
            }
            (Wait-BenchSettled (90 + 5 * $count)).Lines | ForEach-Object { $out.Add("[setup] $_") }
            if ($pulse) { Invoke-ServerCommand ".bench pulse $pulse" | ForEach-Object { $out.Add("[setup] $_") } }
        }
        elseif ($fields[0] -eq 'run' -and $fields.Count -ge 2) {
            $key = if ($fields.Count -ge 3) { $fields[2] } else { '10' }
            $seconds = if ($fields.Count -ge 4) { $fields[3] } else { '45' }
            # Real seconds: a test in turbo takes a fraction of its game time, a stalled one is given up
            $timeout = if ($seconds.EndsWith('%')) { 900 } else { [double]$seconds.TrimEnd('s') + 240 }
            foreach ($layout in $fields[1].Split(',')) {
                if (-not $layout.Trim()) { continue }
                $from = $script:lines.Count
                $clock = [System.Diagnostics.Stopwatch]::StartNew()
                Send-ServerLine ".bench run $layout $key $seconds"
                $at = Wait-ServerLine $from 'run (\d{6,})' $timeout
                $until = if ($at -ge 0) { $at + 1 } else { $script:lines.Count }
                for ($index = $from; $index -lt $until; ++$index) {
                    $text = $script:lines[$index]
                    if ($text -match '^\[bench\] ') { $out.Add("[$layout] $($text.Substring(8))") }
                }
                if ($at -ge 0) {
                    $script:lines[$at] -match 'run (\d{6,})' | Out-Null
                    $out.Add("RESULT $layout $($Matches[1])")
                } else {
                    $out.Add("RESULT $layout none")
                }
                Write-HostLog ("run {0} {1} {2}: {3:0.0} s real" -f $layout, $key, $seconds,
                    $clock.Elapsed.TotalSeconds)
                Invoke-ServerCommand '.bench reset' | Out-Null
            }
        }
        elseif ($fields[0] -eq 'wait' -and $fields.Count -eq 2) {
            $until = (Get-Date).AddSeconds([double]$fields[1])
            while ((Get-Date) -lt $until) { Read-ServerOutput; Start-Sleep -Milliseconds 50 }
        }
        elseif ($fields[0] -eq 'quit') { $quit = $true }
        else { $out.Add("[session] unknown line: $line") }
    }
    [pscustomobject]@{ Lines = $out; Quit = $quit }
}

# --- Main ------------------------------------------------------------------------------------------------------------

try {
    # The world loads, then the owner logs in and walks to the bench
    $at = Wait-ServerLine 0 '#simowner;(ready|missing|failed)' 900
    if ($at -lt 0) { throw 'The simulation owner did not reach the bench in 15 minutes' }
    if ($script:lines[$at] -notmatch '#simowner;ready') { throw "The simulation owner: $($script:lines[$at])" }
    Set-Content $readyFile $script:lines[$at]
    Write-HostLog 'ready'

    while (-not $server.HasExited) {
        Read-ServerOutput
        $request = Get-ChildItem $directory -Filter 'req-*.txt' | Sort-Object Name | Select-Object -First 1
        if (-not $request) {
            # Keep a long-idle host's line list short
            if ($script:lines.Count -gt 200000) { $script:lines.Clear() }
            Start-Sleep -Milliseconds 50
            continue
        }
        $content = [System.IO.File]::ReadAllText($request.FullName)
        Remove-Item $request.FullName
        Write-HostLog "request $($request.Name)"
        $result = Invoke-Request $content
        $answer = Join-Path $directory ($request.Name -replace '^req-', 'res-')
        [System.IO.File]::WriteAllLines("$answer.tmp", [string[]]$result.Lines, [System.Text.UTF8Encoding]::new($false))
        Move-Item "$answer.tmp" $answer -Force
        if ($result.Quit) { break }
    }
}
catch {
    Write-HostLog "error: $_"
}
finally {
    Remove-Item $readyFile -ErrorAction SilentlyContinue
    if (-not $server.HasExited) {
        try {
            Send-ServerLine '.bench clear'
            Send-ServerLine 'server shutdown 1'
            if (-not $server.WaitForExit(30000)) { $server.Kill() }
        } catch { try { $server.Kill() } catch { } }
    }
    Write-HostLog 'stopped'
    $logWriter.Dispose()
}
