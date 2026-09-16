$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'modules\mod-stat-growth\client-assets\source'
$clientAssetRoot = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK\Interface\AddOns\PersonalLoot\Textures\Rogue'
$compiledRoot = Join-Path $repoRoot 'modules\mod-stat-growth\client-assets\compiled'
$assets = @{
    'quick-cut.png' = 'QuickCut.tga'; 'shadow-lunge.png' = 'ShadowLunge.tga'; 'riposte.png' = 'Riposte.tga'
    'opportunity.png' = 'Opportunity.tga'; 'battle-tempo.png' = 'BattleTempo.tga'; 'killing-momentum.png' = 'KillingMomentum.tga'
}
$spellAssets = @{
    'quick-cut.png' = 'RogueMomentum_QuickCut.tga'; 'shadow-lunge.png' = 'RogueMomentum_ShadowLunge.tga'
    'riposte.png' = 'RogueMomentum_Riposte.tga'; 'sanguine-veil.png' = 'RogueMomentum_SanguineVeil.tga'
    'opportunity.png' = 'RogueMomentum_Opening.tga'; 'battle-tempo.png' = 'RogueMomentum_BattleTempo.tga'
    'killing-momentum.png' = 'RogueMomentum_KillingMomentum.tga'
    'crimson-sweep.png' = 'RogueMomentum_CrimsonSweep.tga'
    'gladiator-stance.png' = 'Ability_Warrior_GladiatorStance.tga'
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

New-Item -ItemType Directory -Path $clientAssetRoot -Force | Out-Null
New-Item -ItemType Directory -Path $compiledRoot -Force | Out-Null
foreach ($asset in $assets.GetEnumerator()) {
    $sourcePath = Join-Path $sourceRoot $asset.Key
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing generated Rogue asset: $sourcePath" }
    Convert-ToTga $sourcePath (Join-Path $clientAssetRoot $asset.Value)
}
foreach ($asset in $spellAssets.GetEnumerator()) {
    $sourcePath = Join-Path $sourceRoot $asset.Key
    if (-not (Test-Path -LiteralPath $sourcePath)) { throw "Missing generated Rogue spell asset: $sourcePath" }
    Convert-ToTga $sourcePath (Join-Path $compiledRoot $asset.Value)
}
Write-Host "Installed $($assets.Count) Rogue Momentum icons in $clientAssetRoot"
Write-Host "Built $($spellAssets.Count) native spell icons in $compiledRoot"
