$ErrorActionPreference = 'Stop'
$patcherRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $patcherRoot 'build/BuildCache.ps1')
. (Join-Path $patcherRoot 'build/ClientPayload.ps1')
. (Join-Path $patcherRoot 'build/ReleaseBundles.ps1')
. (Join-Path $patcherRoot 'build/ClientInputs.ps1')
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('client-cache-test-' + [guid]::NewGuid())
$buildCacheRoot = Join-Path $testRoot 'cache'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
function Assert-True($condition, [string]$message) {
    if (-not $condition) { throw $message }
}
function Assert-Throws([scriptblock]$action, [string]$message) {
    $threw = $false
    try { & $action | Out-Null } catch { $threw = $true }
    Assert-True $threw $message
}
try {
    $inputPath = Join-Path $testRoot 'input.txt'
    $outputPath = Join-Path $testRoot 'output.txt'
    [IO.File]::WriteAllText($inputPath, 'aaa')
    $script:runs = 0
    $action = { $script:runs++; Copy-Item -LiteralPath $inputPath -Destination $outputPath -Force }
    Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) $action
    Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) $action
    Assert-True ($script:runs -eq 1) 'Unchanged stage ran twice.'
    $timestamp = (Get-Item $inputPath).LastWriteTimeUtc
    [IO.File]::WriteAllText($inputPath, 'bbb')
    (Get-Item $inputPath).LastWriteTimeUtc = $timestamp
    Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) $action
    Assert-True ($script:runs -eq 2) 'Same size/time source change was missed.'
    [IO.File]::WriteAllText($outputPath, 'bad')
    Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) $action
    Assert-True ($script:runs -eq 3) 'Output corruption was missed.'
    Remove-Item -LiteralPath $outputPath
    Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) $action
    Assert-True ($script:runs -eq 4) 'Missing output was missed.'
    Assert-Throws { Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) { throw 'failed' } -force } 'Failure was swallowed.'
    Assert-True (-not (Test-Path (Join-Path $buildCacheRoot 'fixture.json'))) 'Failed stage retained its cache record.'
    Assert-Throws {
        Invoke-CachedBuildStep 'fixture' @($inputPath) @($outputPath) {
            [IO.File]::WriteAllText($inputPath, 'edited while building')
        }
    } 'Concurrent source edit was silently marked cached.'
    Assert-True (-not (Test-Path (Join-Path $buildCacheRoot 'fixture.json'))) 'Concurrent edit retained a cache record.'
    [IO.File]::WriteAllText($inputPath, 'bbb')

    $addonPath = Join-Path $testRoot 'addon'
    New-Item -ItemType Directory -Path $addonPath | Out-Null
    $addonFile = Join-Path $addonPath 'main.lua'
    [IO.File]::WriteAllText($addonFile, 'original')
    $sources = @{}
    Add-PayloadSource $sources $addonPath 'Interface\AddOns\Example'
    Add-PayloadSource $sources $inputPath 'Interface\AddOns\Example\main.lua'
    $payloadPath = Join-Path $testRoot 'payload'
    Sync-ClientPayload $sources $payloadPath
    $stagedFile = Join-Path $payloadPath 'Interface/AddOns/Example/main.lua'
    Assert-True ((Get-Content $stagedFile -Raw) -eq 'bbb') 'Repo file did not override client addon file.'
    $stagedTimestamp = (Get-Item $stagedFile).LastWriteTimeUtc
    Sync-ClientPayload $sources $payloadPath
    Assert-True ((Get-Item $stagedFile).LastWriteTimeUtc -eq $stagedTimestamp) 'Unchanged payload was copied again.'
    Sync-ClientPayload @{} $payloadPath
    Assert-True (-not (Test-Path $stagedFile)) 'Removed source remained in staged payload.'

    $bundleRoot = Join-Path $testRoot 'bundles'
    $zipPath = Get-CachedAddonBundle $addonPath $bundleRoot
    $referenceZip = Join-Path $testRoot 'reference.zip'
    New-DeterministicZip $addonPath $referenceZip
    Assert-True ((Get-FileHash $zipPath).Hash -eq (Get-FileHash $referenceZip).Hash) 'Bundle format changed.'
    $zipTime = (Get-Item $zipPath).LastWriteTimeUtc
    $null = Get-CachedAddonBundle $addonPath $bundleRoot
    Assert-True ((Get-Item $zipPath).LastWriteTimeUtc -eq $zipTime) 'Unchanged addon was recompressed.'
    $extraPath = Join-Path $addonPath 'extra.lua'
    [IO.File]::WriteAllText($extraPath, 'extra')
    $oldHash = (Get-FileHash $zipPath).Hash
    $null = Get-CachedAddonBundle $addonPath $bundleRoot
    Assert-True ((Get-FileHash $zipPath).Hash -ne $oldHash) 'Added addon file was missed.'
    Remove-Item -LiteralPath $extraPath
    $null = Get-CachedAddonBundle $addonPath $bundleRoot
    Assert-True ((Get-FileHash $zipPath).Hash -eq $oldHash) 'Deleted addon file remained in bundle.'
    [IO.File]::WriteAllText($zipPath, 'corrupt')
    $null = Get-CachedAddonBundle $addonPath $bundleRoot
    Assert-True ((Get-FileHash $zipPath).Hash -eq $oldHash) 'Corrupt cached bundle was reused.'

    # Exercise the publish guard with isolated source/payload roots.
    function Get-ClientReleaseFingerprint { Get-BuildFingerprint @($inputPath) }
    $currentPath = Join-Path $testRoot 'current'
    New-Item -ItemType Directory -Path $currentPath | Out-Null
    Copy-Item $inputPath (Join-Path $currentPath 'file.txt')
    $readyPath = Join-Path $testRoot 'ready.json'
    Write-BuildJson $readyPath @{ sources = (Get-ClientReleaseFingerprint)
        payload = (Get-BuildFingerprint @($currentPath)); clientPath = ''; awesomeWotlkPath = '' }
    $null = Assert-ClientBuildReady $testRoot
    [IO.File]::WriteAllText($inputPath, 'ccc')
    Assert-Throws { Assert-ClientBuildReady $testRoot } 'Stale sources passed publish validation.'
    [IO.File]::WriteAllText($inputPath, 'bbb')
    Remove-Item -LiteralPath (Join-Path $currentPath 'file.txt')
    Assert-Throws { Assert-ClientBuildReady $testRoot } 'Damaged payload passed publish validation.'
    Remove-Item -LiteralPath $readyPath
    Assert-Throws { Assert-ClientBuildReady $testRoot } 'Failed/incomplete build passed publish validation.'

    $lock = Open-BuildLock $testRoot
    try { Assert-Throws { Open-BuildLock $testRoot } 'Concurrent pipeline acquired the lock.' }
    finally { $lock.Dispose() }
    Assert-Throws { Remove-BuildDirectory $testRoot $testRoot } 'Cleanup accepted its allowed root itself.'
    Write-Host 'All client cache, staging, bundle and publish-guard tests passed.'
}
finally { Remove-BuildDirectory $testRoot ([IO.Path]::GetTempPath()) }
