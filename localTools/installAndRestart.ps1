param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$configuration = 'RelWithDebInfo'
)

# The second half of buildAndRestart.ps1, for when the build has just been run on its own (deployWithProgress.ps1):
# stop the servers, install what was built, start them again and wait for the world to accept logins.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'buildSettings.ps1')

Write-Host 'Stopping the servers...'
& (Join-Path $PSScriptRoot 'stopAll.ps1')

try {
    Write-Host 'Installing the new build...'
    & $cmakePath --install $buildRoot --config $configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Server installation failed with exit code $LASTEXITCODE."
    }
}
finally {
    Write-Host 'Starting the servers...'
    & (Join-Path $PSScriptRoot 'startAll.ps1')
}

Write-Host 'Waiting for the world to open (port 8085)...'
$deadline = (Get-Date).AddMinutes(10)
while ((Get-Date) -lt $deadline) {
    if (Get-NetTCPConnection -LocalPort 8085 -State Listen -ErrorAction SilentlyContinue) {
        Write-Host 'World server online.'
        exit 0
    }
    Start-Sleep -Seconds 3
}
throw 'The world server did not open port 8085 within 10 minutes.'
