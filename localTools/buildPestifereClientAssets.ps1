$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent $PSScriptRoot
$assetRoot = Join-Path $repoRoot 'modules\mod-pestifere\client-assets'
$iconSourceRoot = Join-Path $assetRoot 'source\icons'
$backgroundSourceRoot = Join-Path $assetRoot 'source\backgrounds'
$compiledIconRoot = Join-Path $assetRoot 'compiled\icons'
$compiledTalentRoot = Join-Path $assetRoot 'compiled\talentframe'

$talentBackgrounds = @('PestifereCharnier', 'PestifereSangsue')
$spellPatchPath = Join-Path $repoRoot 'localTools\patchSinisterStrike.ps1'
$requiredHealerIcons = [regex]::Matches(
    [IO.File]::ReadAllText($spellPatchPath),
    "Icon = '(PestifereHealer_[^']+)'"
) | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$missingHealerSources = @($requiredHealerIcons | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $iconSourceRoot "${_}.png"))
})
if ($missingHealerSources.Count) {
    throw "Missing Pestiféré healer icon sources: $($missingHealerSources -join ', ')"
}

function Write-Tga([Drawing.Bitmap]$bitmap, [string]$destinationPath) {
    $width = $bitmap.Width
    $height = $bitmap.Height
    $rect = [Drawing.Rectangle]::new(0, 0, $width, $height)
    $data = $bitmap.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $rowSize = $width * 4
        $pixels = [byte[]]::new($rowSize * $height)
        for ($row = 0; $row -lt $height; ++$row) {
            $source = [IntPtr]::Add($data.Scan0, $row * $data.Stride)
            [Runtime.InteropServices.Marshal]::Copy($source, $pixels, $row * $rowSize, $rowSize)
        }
    }
    finally {
        $bitmap.UnlockBits($data)
    }

    $header = [byte[]]::new(18)
    $header[2] = 2
    $header[12] = $width -band 0xff
    $header[13] = ($width -shr 8) -band 0xff
    $header[14] = $height -band 0xff
    $header[15] = ($height -shr 8) -band 0xff
    $header[16] = 32
    $header[17] = 40 # top-left origin, 8 alpha bits

    $stream = [IO.File]::Open($destinationPath, [IO.FileMode]::Create)
    try {
        $stream.Write($header, 0, $header.Length)
        $stream.Write($pixels, 0, $pixels.Length)
    }
    finally {
        $stream.Dispose()
    }
}

function Resize-Image([Drawing.Image]$source, [int]$width, [int]$height,
    [Drawing.Rectangle]$sourceRect) {
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source, [Drawing.Rectangle]::new(0, 0, $width, $height), $sourceRect,
            [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

New-Item -ItemType Directory -Path $compiledIconRoot,$compiledTalentRoot -Force | Out-Null

$iconCount = 0
foreach ($file in Get-ChildItem -LiteralPath $iconSourceRoot -Filter '*.png' -File | Sort-Object Name) {
    $source = [Drawing.Image]::FromFile($file.FullName)
    try {
        $rect = [Drawing.Rectangle]::new(0, 0, $source.Width, $source.Height)
        $bitmap = Resize-Image $source 64 64 $rect
        try {
            Write-Tga $bitmap (Join-Path $compiledIconRoot ($file.BaseName + '.tga'))
        }
        finally {
            $bitmap.Dispose()
        }
    }
    finally {
        $source.Dispose()
    }
    ++$iconCount
}

# WotLK talent backgrounds are 320x384 images split into 256/64 columns and 256/128 rows.
foreach ($backgroundName in $talentBackgrounds) {
    $backgroundSource = Join-Path $backgroundSourceRoot "${backgroundName}.png"
    if (-not (Test-Path -LiteralPath $backgroundSource)) {
        throw "Missing Pestiféré talent background: $backgroundSource"
    }

    $background = [Drawing.Image]::FromFile($backgroundSource)
    try {
        $targetAspect = 320.0 / 384.0
        $cropWidth = [Math]::Min($background.Width, [int][Math]::Round($background.Height * $targetAspect))
        $cropHeight = [Math]::Min($background.Height, [int][Math]::Round($background.Width / $targetAspect))
        $cropX = [int](($background.Width - $cropWidth) / 2)
        $cropY = [int](($background.Height - $cropHeight) / 2)
        $rect = [Drawing.Rectangle]::new($cropX, $cropY, $cropWidth, $cropHeight)
        $canvas = Resize-Image $background 320 384 $rect
        try {
            $quadrants = @(
                @{ Suffix = 'TopLeft'; X = 0; Y = 0; Width = 256; Height = 256 },
                @{ Suffix = 'TopRight'; X = 256; Y = 0; Width = 64; Height = 256 },
                @{ Suffix = 'BottomLeft'; X = 0; Y = 256; Width = 256; Height = 128 },
                @{ Suffix = 'BottomRight'; X = 256; Y = 256; Width = 64; Height = 128 }
            )
            foreach ($quadrant in $quadrants) {
                $sourceRect = [Drawing.Rectangle]::new($quadrant.X, $quadrant.Y, $quadrant.Width, $quadrant.Height)
                $tile = Resize-Image $canvas $quadrant.Width $quadrant.Height $sourceRect
                try {
                    $tileName = "${backgroundName}-$($quadrant.Suffix).tga"
                    Write-Tga $tile (Join-Path $compiledTalentRoot $tileName)
                }
                finally {
                    $tile.Dispose()
                }
            }
        }
        finally {
            $canvas.Dispose()
        }
    }
    finally {
        $background.Dispose()
    }
}

$missingHealerCompiled = @($requiredHealerIcons | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $compiledIconRoot "${_}.tga"))
})
if ($missingHealerCompiled.Count) {
    throw "Missing compiled Pestiféré healer icons: $($missingHealerCompiled -join ', ')"
}

$backgroundTileCount = $talentBackgrounds.Count * 4
Write-Host "Built $iconCount Pestifere icons and $backgroundTileCount talent-background tiles."
