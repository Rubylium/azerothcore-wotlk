# Content-based cache shared by generation, payload staging and release bundling.
$buildCacheVersion = '1'

# Folders never part of a build, wherever they are
$script:skippedBuildFolders = @('node_modules', '.git', '__pycache__', '.deps', '.kilo', '.vscode', '.idea')

# The walks and the hash table in C#: a fingerprint of the release (13 000 files) took 4.5 s a time through
# PowerShell functions, and a build takes two. Compiled once (%LOCALAPPDATA%\Evolutions\build), then loaded from there.
# - Files: every file under the roots (files or folders), the skipped folders aside, sorted ordinally.
# - Hash: a file's SHA-256, remembered with its size and write time (file-hashes.tsv: path, size, write ticks, hash,
#   when it was hashed): a build hashes the same thousands of files several times over and most never change. As
#   git does with its index, a hash taken less than two seconds after the file's last write is not trusted again: a
#   file rewritten within that moment at the same size keeps its write time, and would read as unchanged.
$script:buildHashesSource = @'
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

public static class EvolutionsBuildHashes
{
    class Entry { public long Size; public long Ticks; public string Hash; public long HashedAt; }

    static Dictionary<string, Entry> cache;
    static bool dirty;
    static readonly long RacyTicks = TimeSpan.FromSeconds(2).Ticks;

    public static void Load(string path)
    {
        if (cache != null) return;
        cache = new Dictionary<string, Entry>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(path)) return;
        foreach (string line in File.ReadAllLines(path))
        {
            string[] fields = line.Split('\t');
            if (fields.Length != 5) continue;
            cache[fields[0]] = new Entry { Size = long.Parse(fields[1]), Ticks = long.Parse(fields[2]),
                Hash = fields[3], HashedAt = long.Parse(fields[4]) };
        }
    }

    public static string Hash(FileInfo file)
    {
        file.Refresh();
        if (!file.Exists) throw new FileNotFoundException("Cannot hash a missing file: " + file.FullName);
        long written = file.LastWriteTimeUtc.Ticks;
        Entry known;
        if (cache.TryGetValue(file.FullName, out known) && known.Size == file.Length && known.Ticks == written &&
            known.HashedAt - written >= RacyTicks)
            return known.Hash;
        string hash;
        using (SHA256 sha = SHA256.Create())
        using (FileStream stream = File.OpenRead(file.FullName))
            hash = BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "");
        cache[file.FullName] = new Entry { Size = file.Length, Ticks = written, Hash = hash,
            HashedAt = DateTime.UtcNow.Ticks };
        dirty = true;
        return hash;
    }

    public static void Save(string path)
    {
        if (!dirty || cache == null) return;
        List<string> lines = new List<string>();
        foreach (KeyValuePair<string, Entry> entry in cache)
            if (File.Exists(entry.Key))
                lines.Add(entry.Key + "\t" + entry.Value.Size + "\t" + entry.Value.Ticks + "\t" + entry.Value.Hash +
                    "\t" + entry.Value.HashedAt);
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        File.WriteAllLines(path + ".tmp", lines.ToArray());
        if (File.Exists(path)) File.Delete(path);
        File.Move(path + ".tmp", path);
        dirty = false;
    }

    public static FileInfo[] Files(string[] roots, string[] skipped)
    {
        HashSet<string> skip = new HashSet<string>(skipped, StringComparer.OrdinalIgnoreCase);
        Dictionary<string, FileInfo> files = new Dictionary<string, FileInfo>(StringComparer.OrdinalIgnoreCase);
        foreach (string root in roots)
        {
            if (File.Exists(root)) { FileInfo file = new FileInfo(root); files[file.FullName] = file; continue; }
            if (!Directory.Exists(root)) continue;
            Stack<DirectoryInfo> directories = new Stack<DirectoryInfo>();
            directories.Push(new DirectoryInfo(root));
            while (directories.Count > 0)
            {
                DirectoryInfo directory = directories.Pop();
                foreach (FileInfo file in directory.EnumerateFiles()) files[file.FullName] = file;
                foreach (DirectoryInfo child in directory.EnumerateDirectories())
                    if (!skip.Contains(child.Name)) directories.Push(child);
            }
        }
        List<FileInfo> sorted = new List<FileInfo>(files.Values);
        sorted.Sort((left, right) => string.CompareOrdinal(left.FullName, right.FullName));
        return sorted.ToArray();
    }

    // The fingerprint's file lines, "\n<path>=<hash>" each
    public static string FileLines(FileInfo[] files)
    {
        StringBuilder text = new StringBuilder();
        foreach (FileInfo file in files) text.Append('\n').Append(file.FullName).Append('=').Append(Hash(file));
        return text.ToString();
    }
}
'@

function Initialize-BuildHashes {
    if (-not ('EvolutionsBuildHashes' -as [type])) {
        # Outside any build cache: a loaded assembly cannot be deleted with its folder
        $assemblyRoot = Join-Path $env:LOCALAPPDATA 'Evolutions\build'
        $version = (Get-TextHash $script:buildHashesSource).Substring(0, 12)
        $assembly = Join-Path $assemblyRoot "EvolutionsBuildHashes-$version.dll"
        if (-not (Test-Path -LiteralPath $assembly)) {
            New-Item -ItemType Directory -Force -Path $assemblyRoot | Out-Null
            Add-Type -TypeDefinition $script:buildHashesSource -OutputAssembly $assembly -OutputType Library
        }
        Add-Type -Path $assembly
    }
    [EvolutionsBuildHashes]::Load((Get-FileHashCachePath))
}

function Get-FileHashCachePath {
    $root = if ($buildCacheRoot) { $buildCacheRoot } else { Join-Path $env:TEMP 'evolutions-build-cache' }
    Join-Path $root 'file-hashes.tsv'
}

# Every file under the paths (a path may be a file, a folder or a wildcard), sorted
function Get-BuildFiles([string[]]$paths) {
    Initialize-BuildHashes
    $roots = foreach ($path in $paths) {
        if ($path -match '[\*\?\[]') {
            @(Get-Item -Path $path -Force -ErrorAction SilentlyContinue) | ForEach-Object { $_.FullName }
        } else { $path }
    }
    [EvolutionsBuildHashes]::Files([string[]]@($roots), [string[]]$script:skippedBuildFolders)
}

# path: a file's path, or its FileInfo
function Get-CachedFileHash($path) {
    Initialize-BuildHashes
    $file = if ($path -is [IO.FileInfo]) { $path } else { [IO.FileInfo]::new([string]$path) }
    [EvolutionsBuildHashes]::Hash($file)
}

function Save-FileHashCache {
    if ('EvolutionsBuildHashes' -as [type]) { [EvolutionsBuildHashes]::Save((Get-FileHashCachePath)) }
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
    $files = @(Get-BuildFiles $paths)
    Get-TextHash (($lines -join "`n") + [EvolutionsBuildHashes]::FileLines($files))
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
        Add-BuildTiming $name $timer.Elapsed.TotalSeconds 'cached'
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
    Add-BuildTiming $name $timer.Elapsed.TotalSeconds 'step'
}

# How long each step and tool of the build took, written to .build/timings.json by Save-BuildTimings: where a slow
# build spends its time, without guessing
$script:buildTimings = [Collections.Generic.List[object]]::new()

function Add-BuildTiming([string]$name, [double]$seconds, [string]$kind = 'tool') {
    $script:buildTimings.Add([pscustomobject]@{ name = $name; kind = $kind; seconds = [math]::Round($seconds, 1) })
}

function Invoke-Timed([string]$name, [scriptblock]$action) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    try { & $action | Out-Host }
    finally { Add-BuildTiming $name $timer.Elapsed.TotalSeconds }
}

function Save-BuildTimings([string]$path, [double]$totalSeconds) {
    Write-BuildJson $path @{ finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
        totalSeconds = [math]::Round($totalSeconds, 1); entries = @($script:buildTimings) }
    $slowest = @($script:buildTimings | Where-Object { $_.kind -ne 'cached' } | Sort-Object seconds -Descending |
        Select-Object -First 6 | ForEach-Object { "$($_.name) $($_.seconds)s" })
    if ($slowest) { Write-Host ("Slowest: " + ($slowest -join ', ')) }
}

function Invoke-BuildTool([string]$program, [string[]]$arguments, [string]$label = '') {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & $program @arguments | Out-Host
    Add-BuildTiming $(if ($label) { $label } else { $program }) $timer.Elapsed.TotalSeconds
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
