param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$configuration = 'RelWithDebInfo',

    [switch]$configure
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'buildSettings.ps1')

if ($configure) {
    & $cmakePath `
        -S $repoRoot `
        -B $buildRoot `
        "-DCMAKE_INSTALL_PREFIX=$serverRoot"

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }
}

$previousCompilerWorkers = $env:CL_MPCount

try {
    $env:CL_MPCount = $compilerWorkers.ToString()
    Write-Host "Building $configuration with $projectWorkers MSBuild workers x $compilerWorkers compiler workers..."

    & $msbuildPath `
        (Join-Path $buildRoot 'AzerothCore.sln') `
        '/target:authserver;worldserver' `
        "/property:Configuration=$configuration" `
        '/property:Platform=x64' `
        "/maxCpuCount:$projectWorkers" `
        '/nologo' `
        '/verbosity:minimal'

    if ($LASTEXITCODE -ne 0) {
        throw "Server build failed with exit code $LASTEXITCODE."
    }
}
finally {
    $env:CL_MPCount = $previousCompilerWorkers
}
