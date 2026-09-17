$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# Converts every icon PNG in modules/mod-stat-growth/client-assets/source into a 64x64 TGA in client-assets/compiled.
# Existing icons keep their historical names; any other file becomes CombatRogue_<PascalCaseName>.tga
# (keen-openings.png -> CombatRogue_KeenOpenings.tga), which is the name localTools/patchSinisterStrike.ps1 expects.

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'modules\mod-stat-growth\client-assets\source'
$compiledRoot = Join-Path $repoRoot 'modules\mod-stat-growth\client-assets\compiled'
$clientAssetRoot = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK\Interface\AddOns\PersonalLoot\Textures\Rogue'

$legacySpellIcons = @{
    'quick-cut.png' = 'RogueMomentum_QuickCut'
    'shadow-lunge.png' = 'RogueMomentum_ShadowLunge'
    'riposte.png' = 'RogueMomentum_Riposte'
    'sanguine-veil.png' = 'RogueMomentum_SanguineVeil'
    'opportunity.png' = 'RogueMomentum_Opening'
    'battle-tempo.png' = 'RogueMomentum_BattleTempo'
    'killing-momentum.png' = 'RogueMomentum_KillingMomentum'
    'crimson-sweep.png' = 'RogueMomentum_CrimsonSweep'
    'gladiator-stance.png' = 'Ability_Warrior_GladiatorStance'
}

# Textures still shipped with the PersonalLoot addon
$addonTextures = @{
    'quick-cut.png' = 'QuickCut.tga'; 'shadow-lunge.png' = 'ShadowLunge.tga'; 'riposte.png' = 'Riposte.tga'
    'opportunity.png' = 'Opportunity.tga'; 'battle-tempo.png' = 'BattleTempo.tga'; 'killing-momentum.png' = 'KillingMomentum.tga'
}

function Get-CompiledName([string]$fileName) {
    if ($legacySpellIcons.ContainsKey($fileName)) { return $legacySpellIcons[$fileName] }
    $words = [IO.Path]::GetFileNameWithoutExtension($fileName) -split '[-_ ]+' | Where-Object { $_ }
    $pascal = ($words | ForEach-Object { $_.Substring(0, 1).ToUpperInvariant() + $_.Substring(1) }) -join ''
    return "CombatRogue_$pascal"
}

function Convert-ToTga([string]$sourcePath, [string]$destinationPath) {
    $source = [Drawing.Image]::FromFile($sourcePath)
    try {
        $bitmap = [Drawing.Bitmap]::new(64, 64, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($source, 0, 0, 64, 64)
            } finally { $graphics.Dispose() }
            $rect = [Drawing.Rectangle]::new(0, 0, 64, 64)
            $data = $bitmap.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $pixels = [byte[]]::new([Math]::Abs($data.Stride) * 64)
                [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
            } finally { $bitmap.UnlockBits($data) }
            $header = [byte[]]::new(18)
            $header[2] = 2; $header[12] = 64; $header[14] = 64; $header[16] = 32; $header[17] = 40
            $stream = [IO.File]::Open($destinationPath, [IO.FileMode]::Create)
            try { $stream.Write($header, 0, $header.Length); $stream.Write($pixels, 0, $pixels.Length) } finally { $stream.Dispose() }
        } finally { $bitmap.Dispose() }
    } finally { $source.Dispose() }
}

New-Item -ItemType Directory -Path $compiledRoot -Force | Out-Null
New-Item -ItemType Directory -Path $clientAssetRoot -Force | Out-Null

$compiled = 0
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Filter '*.png' -File | Sort-Object Name) {
    Convert-ToTga $file.FullName (Join-Path $compiledRoot "$(Get-CompiledName $file.Name).tga")
    ++$compiled
    if ($addonTextures.ContainsKey($file.Name)) {
        Convert-ToTga $file.FullName (Join-Path $clientAssetRoot $addonTextures[$file.Name])
    }
}

Write-Host "Built $compiled spell icons in $compiledRoot"
