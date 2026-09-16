$ErrorActionPreference = 'Stop'

$consoleSignalSource = @'
using System;
using System.Runtime.InteropServices;

public static class ConsoleSignal
{
    public delegate bool HandlerRoutine(uint controlType);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeConsole();

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool AttachConsole(uint processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GenerateConsoleCtrlEvent(uint controlEvent, uint processGroupId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool SetConsoleCtrlHandler(HandlerRoutine handler, bool add);
}
'@

Add-Type -TypeDefinition $consoleSignalSource

function Stop-ServerGracefully {
    param(
        [Parameter(Mandatory)]
        [System.Diagnostics.Process]$process,

        [int]$timeoutSeconds = 45
    )

    Write-Host "Gracefully stopping $($process.ProcessName) (PID $($process.Id))..."

    # Run the console-signal call in a child PowerShell process so this script
    # remains attached to startServer.cmd's console.
    $signalScript = @"
Add-Type -TypeDefinition @'
$consoleSignalSource
'@
[ConsoleSignal]::SetConsoleCtrlHandler(`$null, `$true) | Out-Null
[ConsoleSignal]::FreeConsole() | Out-Null
if (-not [ConsoleSignal]::AttachConsole($($process.Id))) { exit 2 }
if (-not [ConsoleSignal]::GenerateConsoleCtrlEvent(0, 0)) { exit 3 }
Start-Sleep -Milliseconds 750
[ConsoleSignal]::FreeConsole() | Out-Null
exit 0
"@

    $encodedScript = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($signalScript))
    $signalProcess = Start-Process `
        -FilePath 'powershell.exe' `
        -ArgumentList '-NoProfile', '-NonInteractive', '-EncodedCommand', $encodedScript `
        -WindowStyle Hidden `
        -Wait `
        -PassThru

    if ($signalProcess.ExitCode -ne 0) {
        throw "Could not send the graceful stop signal to $($process.ProcessName) (exit $($signalProcess.ExitCode))."
    }

    if (-not $process.WaitForExit($timeoutSeconds * 1000)) {
        throw "$($process.ProcessName) did not stop within $timeoutSeconds seconds. It was not force-killed."
    }

    Write-Host "$($process.ProcessName) stopped cleanly."
}

$worldServer = Get-Process -Name 'worldserver' -ErrorAction SilentlyContinue
if ($worldServer) {
    Stop-ServerGracefully -process $worldServer
}
else {
    Write-Host 'worldserver is not running.'
}

$authServer = Get-Process -Name 'authserver' -ErrorAction SilentlyContinue
if ($authServer) {
    Stop-ServerGracefully -process $authServer
}
else {
    Write-Host 'authserver is not running.'
}

Write-Host 'AzerothCore stopped. Online characters were saved during worldserver shutdown.'
