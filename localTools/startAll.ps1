$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$serverRoot = Join-Path $repoRoot 'server'

& (Join-Path $PSScriptRoot 'startDatabase.ps1')

$servers = @(
    @{ name = 'authserver'; executable = Join-Path $serverRoot 'authserver.exe' },
    @{ name = 'worldserver'; executable = Join-Path $serverRoot 'worldserver.exe' }
)

foreach ($server in $servers) {
    if (Get-Process -Name $server.name -ErrorAction SilentlyContinue) {
        Write-Host "$($server.name) is already running."
        continue
    }

    if (-not (Test-Path -LiteralPath $server.executable)) {
        throw "Server executable not found: $($server.executable)"
    }

    Start-Process `
        -FilePath $server.executable `
        -WorkingDirectory $serverRoot `
        -WindowStyle Hidden

    Write-Host "Started $($server.name)."
}

Write-Host 'AzerothCore startup requested in the background.'
