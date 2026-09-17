$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot 'build'
$serverRoot = Join-Path $repoRoot 'server'

$cmakePath = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$msbuildPath = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'

$logicalProcessors = [Environment]::ProcessorCount
$projectWorkers = [Math]::Min(2, $logicalProcessors)
$compilerWorkers = [Math]::Max(1, [Math]::Floor($logicalProcessors / $projectWorkers))

foreach ($requiredPath in @($cmakePath, $msbuildPath, $buildRoot)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required build path not found: $requiredPath"
    }
}
