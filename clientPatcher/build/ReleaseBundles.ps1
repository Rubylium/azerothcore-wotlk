Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

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

function Get-CachedAddonBundle([string]$folder, [string]$bundleRoot) {
    $name = Split-Path -Leaf $folder
    $zipPath = Join-Path $bundleRoot "$name.zip"
    Invoke-CachedBuildStep "addon-$name" @($folder, $PSCommandPath) @($zipPath) -action {
        New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
        New-DeterministicZip $folder $zipPath
    }
    return $zipPath
}
