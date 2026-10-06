# Content-based cache shared by generation, payload staging and release bundling.
$buildCacheVersion = '1'

function Get-BuildFiles([string[]]$paths) {
    $files = @{}
    foreach ($path in $paths) {
        foreach ($item in @(Get-Item -Path $path -ErrorAction SilentlyContinue)) {
            $children = if ($item.PSIsContainer) {
                $directories = [Collections.Generic.Stack[string]]::new()
                $directories.Push($item.FullName)
                while ($directories.Count) {
                    foreach ($child in Get-ChildItem -LiteralPath $directories.Pop() -Force) {
                        if ($child.PSIsContainer) {
                            if ($child.Name -notin 'node_modules', '.git', '__pycache__', '.deps', '.kilo', '.vscode', '.idea') {
                                $directories.Push($child.FullName)
                            }
                        } else { $child }
                    }
                }
            } else { @($item) }
            foreach ($file in $children) {
                if ($file.FullName -notmatch '\\(node_modules|\.git|__pycache__|\.deps)\\') {
                    $files[$file.FullName] = $file
                }
            }
        }
    }
    @($files.Values | Sort-Object FullName)
}

# A file's SHA-256, remembered with its size and write time (as git's index does): a build hashes the same thousands of
# files several times over - fingerprints, payload copies, the manifest, the release check - and most never change.
# The table lives in the build cache (file-hashes.json), loaded on first use and saved by Save-FileHashCache.
$script:fileHashCache = $null
$script:fileHashCacheDirty = $false

function Get-FileHashCachePath {
    $root = if ($buildCacheRoot) { $buildCacheRoot } else { Join-Path $env:TEMP 'evolutions-build-cache' }
    Join-Path $root 'file-hashes.json'
}

function Get-CachedFileHash([string]$path) {
    if ($null -eq $script:fileHashCache) {
        $script:fileHashCache = @{}
        $saved = Read-BuildJson (Get-FileHashCachePath)
        if ($saved) {
            foreach ($entry in $saved.PSObject.Properties) { $script:fileHashCache[$entry.Name] = $entry.Value }
        }
    }
    $file = Get-Item -LiteralPath $path -Force
    $known = $script:fileHashCache[$file.FullName]
    if ($known -and $known.size -eq $file.Length -and $known.ticks -eq $file.LastWriteTimeUtc.Ticks) {
        return $known.hash
    }
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $script:fileHashCache[$file.FullName] = [pscustomobject]@{ size = $file.Length
        ticks = $file.LastWriteTimeUtc.Ticks; hash = $hash }
    $script:fileHashCacheDirty = $true
    return $hash
}

function Save-FileHashCache {
    if (-not $script:fileHashCacheDirty) { return }
    # Only files that still exist: the table never grows past what the builds read
    $kept = [ordered]@{}
    foreach ($name in @($script:fileHashCache.Keys | Sort-Object)) {
        if (Test-Path -LiteralPath $name) { $kept[$name] = $script:fileHashCache[$name] }
    }
    Write-BuildJson (Get-FileHashCachePath) $kept
    $script:fileHashCacheDirty = $false
}

function Get-TextHash([string]$text) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($text)))).Replace('-', '')
    } finally { $sha.Dispose() }
}

function Get-BuildFingerprint([string[]]$paths, [string]$salt = '') {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("cache=$buildCacheVersion;$salt")
    foreach ($path in @($paths | Sort-Object -Unique)) {
        $lines.Add("root=$path;exists=$(Test-Path -Path $path)")
    }
    foreach ($file in @(Get-BuildFiles $paths)) {
        $lines.Add("$($file.FullName)=$(Get-CachedFileHash $file.FullName)")
    }
    Get-TextHash ($lines -join "`n")
}

function Write-BuildJson([string]$path, $value) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    $temporary = "$path.tmp"
    [IO.File]::WriteAllText($temporary, ($value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporary -Destination $path -Force
}

function Read-BuildJson([string]$path) {
    if (Test-Path -LiteralPath $path) {
        try { return Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json } catch { }
    }
    return $null
}

function Invoke-CachedBuildStep {
    param([string]$name, [string[]]$inputs, [string[]]$outputs, [scriptblock]$action,
        [string]$salt = '', [switch]$force, [string[]]$mutableInputs = @())
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $statePath = Join-Path $buildCacheRoot "$name.json"
    $inputHash = Get-BuildFingerprint $inputs $salt
    $state = Read-BuildJson $statePath
    if (-not $force -and $state -and $state.inputs -eq $inputHash -and
        $state.outputs -eq (Get-BuildFingerprint $outputs)) {
        Write-Host ("Cached {0} ({1:N1}s)" -f $name, $timer.Elapsed.TotalSeconds)
        return
    }
    # Never retain a valid record for a failed or partially completed generation.
    if (Test-Path -LiteralPath $statePath) { Remove-Item -LiteralPath $statePath -Force }
    # Only explicitly declared caches/backups may be populated by the generator itself.
    # Never bless a source edit made while the action was still reading its inputs.
    $stableInputs = @($inputs | Where-Object { $_ -notin $mutableInputs })
    $stableHash = Get-BuildFingerprint $stableInputs $salt
    Write-Host "Building $name..."
    & $action | Out-Host
    if ($stableHash -ne (Get-BuildFingerprint $stableInputs $salt)) {
        throw "$name sources changed during generation. Run the build again."
    }
    foreach ($output in $outputs) {
        if (-not (Test-Path -Path $output)) { throw "$name did not produce $output" }
    }
    Write-BuildJson $statePath @{ inputs = (Get-BuildFingerprint $inputs $salt)
        outputs = (Get-BuildFingerprint $outputs) }
    Write-Host ("Built {0} ({1:N1}s)" -f $name, $timer.Elapsed.TotalSeconds)
}

function Invoke-BuildTool([string]$program, [string[]]$arguments) {
    & $program @arguments | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$program failed (exit $LASTEXITCODE)." }
}

function Copy-BuildFile([string]$source, [string]$destination) {
    $sourceHash = Get-CachedFileHash $source
    if ((Test-Path -LiteralPath $destination) -and (Get-CachedFileHash $destination) -eq $sourceHash) { return }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

function Remove-BuildDirectory([string]$path, [string]$allowedRoot) {
    $resolved = [IO.Path]::GetFullPath($path).TrimEnd('\')
    $root = [IO.Path]::GetFullPath($allowedRoot).TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside $allowedRoot`: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

function Open-BuildLock([string]$root) {
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    try { return [IO.File]::Open((Join-Path $root 'pipeline.lock'), 'OpenOrCreate', 'ReadWrite', 'None') }
    catch { throw 'Another client build or publication is running. Wait for it to finish.' }
}
