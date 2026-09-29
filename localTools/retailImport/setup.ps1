$ErrorActionPreference = 'Stop'

# Prepares localTools\retailImport (see README.md): everything it downloads goes to .deps (gitignored).
# - MultiConverter (github.com/MaxtorCoder/MultiConverter, originally by Adspartan), pinned: the tool compiles its
#   M2/skin converters from this checkout. The project has no license, so its source is not copied into the repo.
# - the community listfile (github.com/wowdev/wow-listfile): FileDataID <-> file names
# Then builds the tool. TACTSharp and DBCD (wowdev, MIT) come from NuGet; the DB2 definitions (WoWDBDefs) are
# fetched on first use into .deps\dbd.

$root = $PSScriptRoot
$deps = Join-Path $root '.deps'
$multiConverter = Join-Path $deps 'MultiConverter'
$multiConverterCommit = '22edb669faef7bebec2b5026cd48f551f88fc0df'
$listfile = Join-Path $deps 'community-listfile.csv'

New-Item -ItemType Directory -Force -Path $deps | Out-Null
if (-not (Test-Path -LiteralPath $multiConverter)) {
    git clone https://github.com/MaxtorCoder/MultiConverter $multiConverter
    if ($LASTEXITCODE -ne 0) { throw 'Could not clone MultiConverter.' }
}
git -C $multiConverter checkout --quiet $multiConverterCommit
if ($LASTEXITCODE -ne 0) { throw "Could not check out MultiConverter $multiConverterCommit." }

$listfileStale = -not (Test-Path -LiteralPath $listfile) -or
    (Get-Item -LiteralPath $listfile).LastWriteTime -lt (Get-Date).AddDays(-30)
if ($listfileStale) {
    Write-Host 'Downloading the community listfile...'
    Invoke-WebRequest -UseBasicParsing -OutFile $listfile `
        -Uri 'https://github.com/wowdev/wow-listfile/releases/latest/download/community-listfile.csv'
}

dotnet build (Join-Path $root 'RetailImport\RetailImport.csproj') -c Release
if ($LASTEXITCODE -ne 0) { throw 'RetailImport failed to build.' }
Write-Host "Ready: $(Join-Path $root 'RetailImport\bin\Release\net10.0\RetailImport.exe')"
