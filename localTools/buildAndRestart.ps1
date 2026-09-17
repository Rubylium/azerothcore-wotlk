param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$configuration = 'RelWithDebInfo',

    [switch]$configure
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'buildSettings.ps1')

& (Join-Path $PSScriptRoot 'buildServer.ps1') `
    -configuration $configuration `
    -configure:$configure

# Keep the live server available while compilation runs. Stop it only after a successful build.
& (Join-Path $PSScriptRoot 'stopAll.ps1')

try {
    & $cmakePath --install $buildRoot --config $configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Server installation failed with exit code $LASTEXITCODE."
    }
}
finally {
    & (Join-Path $PSScriptRoot 'startAll.ps1')
}
