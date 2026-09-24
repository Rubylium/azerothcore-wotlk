param(
    # Default: bump the patch number of the latest release
    [string]$version,
    # Address players connect to. The launcher writes it into their realmlist.wtf and Config.wtf, and pings it
    # for the realm card's status. It used to default to empty, which left every release without an address:
    # the realm card could only ever say "Adresse inconnue". Pass '' to leave players' clients as they are.
    [string]$realmlist = '87.91.69.235',
    # Publish the newest package in dist instead of building a new one
    [switch]$skipBuild,
    # Publish this package from dist (e.g. CustomWotLKClientPatch-1.0.24.zip): a launcher-only release keeps the
    # client files the players already have. Implies -skipBuild.
    [string]$packageName = '',
    [string]$repository = 'Rubylium/Evolutions',
    # Publish an existing package even though sources have changed since it was built. Only for deliberately
    # re-releasing an older client build; it is never the right answer to a build that just failed.
    [switch]$allowStale
)

# Publishes a release that the Evolutions launcher installs from.
#
# The release carries a manifest.json listing every client file with its size and SHA-256, the launcher itself
# (Evolutions.exe, in every release so the "latest" download link always works) and the Wow.exe patch spec.
# Files are stored under a name derived from their content: a file unchanged since an earlier release is not
# uploaded again, its manifest entry points at the release that already holds it. Old releases are therefore
# never deleted.

$ErrorActionPreference = 'Stop'
# gh colorizes JSON when it still sees a console. Windows PowerShell then cannot parse the release list.
$env:NO_COLOR = '1'
$env:GH_FORCE_TTY = '0'
$patcherRoot = $PSScriptRoot
$gh = 'C:\Program Files\GitHub CLI\gh.exe'
$work = Join-Path $patcherRoot '.release'

$removedAddons = @('Atlas', 'Atlas_Battlegrounds', 'Atlas_DungeonLocs', 'Atlas_OutdoorRaids', 'Atlas_Transportation')

Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

# A zip whose bytes depend only on the folder's content: entries sorted, every date fixed. Unchanged content gives
# the very same file, so it is neither uploaded nor downloaded again.
function New-DeterministicZip([string]$folder, [string]$zipPath) {
    $fixedDate = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
    $entries = Get-ChildItem -LiteralPath $folder -File -Recurse | ForEach-Object {
        [pscustomobject]@{ Name = $_.FullName.Substring($folder.Length + 1).Replace('\', '/'); Path = $_.FullName }
    } | Sort-Object Name -CaseSensitive
    $zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in $entries) {
            $entry = $zip.CreateEntry($item.Name, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $fixedDate
            $output = $entry.Open()
            try {
                $source = [IO.File]::OpenRead($item.Path)
                try { $source.CopyTo($output) } finally { $source.Dispose() }
            }
            finally {
                $output.Dispose()
            }
        }
    }
    finally {
        $zip.Dispose()
    }
}

# Refuses to publish a package that no longer matches the sources it was built from.
#
# -skipBuild takes the newest zip in dist, and a build that FAILS leaves no new zip - so without this the
# newest one is the previous build and it goes out as a new release carrying the old client, silently. The
# same check catches the other way of getting there: editing a file and forgetting to build at all.
function Assert-PackageIsCurrent([IO.FileInfo]$package) {
    $roots = @('interface', 'addons', 'assets', 'vendor') |
        ForEach-Object { Join-Path $patcherRoot $_ } |
        Where-Object { Test-Path -LiteralPath $_ }
    if (-not $roots) { return }

    $newer = Get-ChildItem -LiteralPath $roots -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object {
            $_.LastWriteTime -gt $package.LastWriteTime -and
            $_.FullName -notmatch '\\(node_modules|__pycache__|\.git)\\'
        } |
        Sort-Object LastWriteTime -Descending

    if (-not $newer) { return }

    $listed = $newer | Select-Object -First 5 | ForEach-Object {
        "    $($_.FullName.Substring($patcherRoot.Length + 1))  ($($_.LastWriteTime.ToString('HH:mm:ss')))"
    }
    $extra = if ($newer.Count -gt 5) { "    ... and $($newer.Count - 5) more" } else { $null }

    $message = @(
        "$($package.Name) was built at $($package.LastWriteTime.ToString('HH:mm:ss')), before these changed:",
        $listed,
        $extra,
        '',
        'Publishing it would ship the previous client. If a build just failed - the interface step fails while',
        'WoW holds the patch files - close the client and run Build-FriendPatch.ps1 again.',
        'Pass -allowStale only to deliberately re-release an older build.'
    ) | Where-Object { $null -ne $_ }

    throw ($message -join [Environment]::NewLine)
}

function Get-Sha256([string]$path) {
    (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Invoke-Gh {
    & $gh @args
    if ($LASTEXITCODE -ne 0) {
        throw "gh $($args -join ' ') failed (exit $LASTEXITCODE)."
    }
}

if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}
$payloadRoot = Join-Path $work 'package'
$uploadRoot = Join-Path $work 'upload'
New-Item -ItemType Directory -Path $payloadRoot, $uploadRoot -Force | Out-Null

# 1. The client package
if ($packageName) {
    $package = Get-Item -LiteralPath (Join-Path (Join-Path $patcherRoot 'dist') $packageName)
}
else {
    if (-not $skipBuild) {
        & (Join-Path $patcherRoot 'Build-FriendPatch.ps1')
    }
    $package = Get-ChildItem -LiteralPath (Join-Path $patcherRoot 'dist') -Filter 'CustomWotLKClientPatch-*.zip' |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $package) {
        throw 'No package in dist: run without -skipBuild.'
    }
}
if ($packageName -or $skipBuild) {
    if ($allowStale) {
        Write-Warning "Publishing $($package.Name) without checking it against the sources (-allowStale)."
    }
    else {
        Assert-PackageIsCurrent $package
    }
}

Write-Host "Package: $($package.Name)"
Expand-Archive -LiteralPath $package.FullName -DestinationPath $payloadRoot

# 2. The launcher
Write-Host 'Building the launcher...'
& dotnet build (Join-Path $patcherRoot 'launcher\Evolutions.csproj') -c Release -nologo -v q | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Launcher build failed (exit $LASTEXITCODE)."
}
$launcherPath = Join-Path $patcherRoot 'launcher\bin\Release\net48\Evolutions.exe'

# 3. What the latest release already holds
$previous = $null
$previousTag = $null
# stderr has to stay off the pipeline: under Windows PowerShell, 2>$null on gh feeds ConvertFrom-Json a
# non-JSON token and the publish stops before a release exists.
$releaseJson = & $gh release list --repo $repository --limit 1 --json tagName,isLatest 2>&1 |
    Where-Object { $_ -is [string] }
$releases = $null
if ($LASTEXITCODE -eq 0 -and $releaseJson) {
    $releases = @(($releaseJson -join "`n") | ConvertFrom-Json)
}
if ($releases) {
    $previousTag = $releases[0].tagName
    Invoke-Gh release download $previousTag --repo $repository --pattern manifest.json --dir $work --clobber
    $previous = Get-Content -LiteralPath (Join-Path $work 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
}

if ([string]::IsNullOrWhiteSpace($version)) {
    if ($previous) {
        $current = [version]$previous.Version
        $version = "$($current.Major).$($current.Minor).$($current.Build + 1)"
    }
    else {
        $version = '1.0.0'
    }
}
$tag = "v$version"
$downloadBase = "https://github.com/$repository/releases/download/$tag"

# Content already published, by hash: reused instead of uploaded again
$published = @{}
if ($previous) {
    foreach ($file in @($previous.Files) + @($previous.Bundles) + @($previous.WowExePatch)) {
        if ($file -and $file.Sha256) {
            $published[$file.Sha256] = $file.Url
        }
    }
}

$uploads = [Collections.Generic.List[string]]::new()
function Publish-File([string]$path) {
    $sha = Get-Sha256 $path
    $size = (Get-Item -LiteralPath $path).Length
    if ($published.ContainsKey($sha)) {
        $url = $published[$sha]
    }
    else {
        $asset = $sha.Substring(0, 16) + '-' + (Split-Path -Leaf $path)
        Copy-Item -LiteralPath $path -Destination (Join-Path $uploadRoot $asset)
        $uploads.Add((Join-Path $uploadRoot $asset))
        $url = "$downloadBase/$asset"
        $published[$sha] = $url
    }
    [ordered]@{ Url = $url; Sha256 = $sha; Size = $size }
}

# 4. The manifest
# Each addon ships as one zip (a release holds at most 1000 files, and addons carry thousands); the rest file by file
$payloadFiles = Join-Path $payloadRoot 'payload'
$addonRoot = Join-Path $payloadFiles 'Interface\AddOns'
$bundleRoot = Join-Path $work 'bundles'
New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null

$bundles = foreach ($folder in (Get-ChildItem -LiteralPath $addonRoot -Directory | Sort-Object Name)) {
    $zipPath = Join-Path $bundleRoot ($folder.Name + '.zip')
    New-DeterministicZip $folder.FullName $zipPath
    $entry = Publish-File $zipPath
    $entry.Folder = 'Interface/AddOns/' + $folder.Name
    $entry
}
$addons = @($bundles | ForEach-Object { Split-Path -Leaf $_.Folder })

$files = foreach ($item in (Get-ChildItem -LiteralPath $payloadFiles -File -Recurse | Sort-Object FullName)) {
    if ($item.FullName.StartsWith($addonRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        continue
    }
    $entry = Publish-File $item.FullName
    $entry.Path = $item.FullName.Substring($payloadFiles.Length + 1).Replace('\', '/')
    $entry
}

# Announcements for the launcher's home page, newest first
$newsPath = Join-Path $patcherRoot 'news.json'
$news = @()
if (Test-Path -LiteralPath $newsPath) {
    # Windows PowerShell hands a JSON array over as one object: going through the pipeline unrolls it
    $news = @(Get-Content -LiteralPath $newsPath -Raw -Encoding UTF8 | ConvertFrom-Json | ForEach-Object { $_ })
}

$launcherSha = Get-Sha256 $launcherPath
Copy-Item -LiteralPath $launcherPath -Destination (Join-Path $uploadRoot 'Evolutions.exe')
$uploads.Add((Join-Path $uploadRoot 'Evolutions.exe'))

$manifest = [ordered]@{
    Version = $version
    Launcher = [ordered]@{ Url = "$downloadBase/Evolutions.exe"; Sha256 = $launcherSha; Size = (Get-Item $launcherPath).Length }
    WowExePatch = Publish-File (Join-Path $payloadRoot 'WowExePatch.json')
    Realmlist = $realmlist
    Addons = $addons
    RemovedAddons = $removedAddons
    Files = @($files)
    Bundles = @($bundles)
    News = $news
}

# Nothing to publish when neither the files, the launcher, the news nor the settings moved
if ($previous) {
    $same = ($previous.Launcher.Sha256 -eq $launcherSha) -and ($previous.Realmlist -eq $realmlist) -and
        ($previous.WowExePatch.Sha256 -eq $manifest.WowExePatch.Sha256) -and
        (($previous.Files | ForEach-Object { "$($_.Path)=$($_.Sha256)" }) -join '|') -eq
        (($manifest.Files | ForEach-Object { "$($_.Path)=$($_.Sha256)" }) -join '|') -and
        (($previous.Bundles | ForEach-Object { "$($_.Folder)=$($_.Sha256)" }) -join '|') -eq
        (($manifest.Bundles | ForEach-Object { "$($_.Folder)=$($_.Sha256)" }) -join '|') -and
        (ConvertTo-Json @($previous.News) -Depth 4 -Compress) -eq (ConvertTo-Json @($news) -Depth 4 -Compress)
    if ($same) {
        Write-Host "Nothing changed since ${previousTag}: no release published."
        return
    }
}

$manifestPath = Join-Path $uploadRoot 'manifest.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
$uploads.Add($manifestPath)

# 5. The release
$newBytes = ($uploads | ForEach-Object { (Get-Item $_).Length } | Measure-Object -Sum).Sum
Write-Host ("Publishing {0}: {1} files, {2} addons, {3} uploaded ({4:N1} MB)" -f $tag, $manifest.Files.Count,
    $manifest.Bundles.Count, $uploads.Count, ($newBytes / 1MB))
$releaseNotesPath = Join-Path $work 'release-notes.txt'
[IO.File]::WriteAllText($releaseNotesPath, '', [Text.UTF8Encoding]::new($false))
Invoke-Gh release create $tag @uploads --repo $repository --title $tag --notes-file $releaseNotesPath --latest

Write-Host "Published: https://github.com/$repository/releases/tag/$tag"
Write-Host "Launcher:  https://github.com/$repository/releases/latest/download/Evolutions.exe"
