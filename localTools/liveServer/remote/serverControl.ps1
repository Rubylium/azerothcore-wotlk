# Runs ON the live VPS: start | stop | restart | status of the auth and world servers.
# They run as SYSTEM scheduled tasks (at boot, restarted after a crash), so they outlive SSH sessions.
param(
    [Parameter(Mandatory)][ValidateSet('start', 'stop', 'restart', 'status', 'register')][string]$action,
    [string]$root = 'C:\Evolutions'
)
$ErrorActionPreference = 'Stop'

$serverRoot = Join-Path $root 'server'
$servers = @(
    @{ task = 'EvolutionsAuth'; process = 'authserver' },
    @{ task = 'EvolutionsWorld'; process = 'worldserver' }
)

function Register-ServerTasks {
    foreach ($server in $servers) {
        $exe = Join-Path $serverRoot "$($server.process).exe"
        $taskAction = New-ScheduledTaskAction -Execute $exe -WorkingDirectory $serverRoot
        $trigger = New-ScheduledTaskTrigger -AtStartup
        $trigger.Delay = 'PT30S'   # MySQL first
        # A crash exits non-zero: the scheduler brings it back within a minute, up to 999 times
        $settings = New-ScheduledTaskSettingsSet -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) `
            -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -AllowStartIfOnBatteries `
            -DontStopIfGoingOnBatteries -StartWhenAvailable
        $principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
        Register-ScheduledTask -TaskName $server.task -Action $taskAction -Trigger $trigger -Settings $settings `
            -Principal $principal -Force | Out-Null
    }
    Write-Host 'Server tasks registered.'
}

$consoleSignalSource = @'
using System;
using System.Runtime.InteropServices;

public static class ConsoleSignal
{
    public delegate bool HandlerRoutine(uint controlType);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool FreeConsole();
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool AttachConsole(uint processId);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool GenerateConsoleCtrlEvent(uint e, uint g);
    [DllImport("kernel32.dll", SetLastError = true)] public static extern bool SetConsoleCtrlHandler(HandlerRoutine h, bool add);
}
'@

# CTRL_BREAK on the server's console: the world saves every online character before it exits (as stopAll.ps1)
function Stop-ServerGracefully([System.Diagnostics.Process]$process, [int]$timeoutSeconds = 60) {
    Write-Host "Stopping $($process.ProcessName) (PID $($process.Id))..."
    $signalScript = @"
Add-Type -TypeDefinition @'
$consoleSignalSource
'@
[ConsoleSignal]::SetConsoleCtrlHandler(`$null, `$true) | Out-Null
[ConsoleSignal]::FreeConsole() | Out-Null
if (-not [ConsoleSignal]::AttachConsole($($process.Id))) { exit 2 }
if (-not [ConsoleSignal]::GenerateConsoleCtrlEvent(1, 0)) { exit 3 }
Start-Sleep -Milliseconds 750
exit 0
"@
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($signalScript))
    Start-Process powershell.exe -ArgumentList '-NoProfile', '-NonInteractive', '-EncodedCommand', $encoded `
        -WindowStyle Hidden -Wait | Out-Null
    if (-not $process.WaitForExit($timeoutSeconds * 1000)) {
        Write-Host "$($process.ProcessName) ignored the stop signal for $timeoutSeconds s: killing it."
        $process.Kill()
        $process.WaitForExit()
    }
    Write-Host "$($process.ProcessName) stopped."
}

function Stop-Servers {
    # Disabled first, or the scheduler would take the clean exit for a crash and start it again
    foreach ($server in $servers) { Disable-ScheduledTask -TaskName $server.task -ErrorAction SilentlyContinue | Out-Null }
    foreach ($name in 'worldserver', 'authserver') {
        $process = Get-Process -Name $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($process) { Stop-ServerGracefully $process } else { Write-Host "$name is not running." }
    }
}

function Start-Servers {
    if ((Get-Service EvolutionsMySQL).Status -ne 'Running') { Start-Service EvolutionsMySQL }
    foreach ($server in $servers) {
        Enable-ScheduledTask -TaskName $server.task | Out-Null
        if (Get-Process -Name $server.process -ErrorAction SilentlyContinue) {
            Write-Host "$($server.process) is already running."
        } else {
            Start-ScheduledTask -TaskName $server.task
            Write-Host "Started $($server.process)."
        }
    }
}

function Show-Status {
    foreach ($server in $servers) {
        $process = Get-Process -Name $server.process -ErrorAction SilentlyContinue | Select-Object -First 1
        $state = if ($process) { "running, PID $($process.Id), $([math]::Round($process.WorkingSet64 / 1MB)) MB" } else { 'stopped' }
        Write-Host "$($server.process): $state"
    }
    foreach ($port in 3724, 8085) {
        $listening = [bool](Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction SilentlyContinue)
        Write-Host "port $port listening: $listening"
    }
}

switch ($action) {
    'register' { Register-ServerTasks }
    'start' { Start-Servers }
    'stop' { Stop-Servers }
    'restart' { Stop-Servers; Start-Servers }
    'status' { Show-Status }
}
