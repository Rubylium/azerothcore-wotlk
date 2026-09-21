$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent $PSScriptRoot
$assetRoot = Join-Path $repoRoot 'modules\mod-necromancer\client-assets'
$iconSourceRoot = Join-Path $assetRoot 'source\icons'
$compiledIconRoot = Join-Path $assetRoot 'compiled\icons'
$spellPatchPath = Join-Path $repoRoot 'localTools\necromancer\Spells.ps1'

$requiredIcons = [regex]::Matches(
    [IO.File]::ReadAllText($spellPatchPath),
    "Icon\s*=\s*'(Necromancer(?:Talent)?_[^']+)'"
) | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique

$missingSources = @($requiredIcons | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $iconSourceRoot "${_}.png"))
})
if ($missingSources.Count) {
    throw "Missing Necromancer icon sources: $($missingSources -join ', ')"
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
    $header[17] = 40

    $stream = [IO.File]::Open($destinationPath, [IO.FileMode]::Create)
    try {
        $stream.Write($header, 0, $header.Length)
        $stream.Write($pixels, 0, $pixels.Length)
    }
    finally {
        $stream.Dispose()
    }
}

function Resize-Icon([Drawing.Image]$source) {
    $bitmap = [Drawing.Bitmap]::new(64, 64, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($source, [Drawing.Rectangle]::new(0, 0, 64, 64))
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

New-Item -ItemType Directory -Path $compiledIconRoot -Force | Out-Null
foreach ($iconName in $requiredIcons) {
    $source = [Drawing.Image]::FromFile((Join-Path $iconSourceRoot "${iconName}.png"))
    try {
        $bitmap = Resize-Icon $source
        try {
            Write-Tga $bitmap (Join-Path $compiledIconRoot "${iconName}.tga")
        }
        finally {
            $bitmap.Dispose()
        }
    }
    finally {
        $source.Dispose()
    }
}

$missingCompiled = @($requiredIcons | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $compiledIconRoot "${_}.tga"))
})
if ($missingCompiled.Count) {
    throw "Missing compiled Necromancer icons: $($missingCompiled -join ', ')"
}

Write-Host "Built $($requiredIcons.Count) Necromancer icons."
