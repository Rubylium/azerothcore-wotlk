# Later sources override earlier folders (the repo's Details sheets and DragonUI fixes).
function Add-PayloadSource([hashtable]$sources, [string]$sourcePath, [string]$relativePath) {
    $source = Get-Item -LiteralPath $sourcePath
    foreach ($file in @(Get-BuildFiles @($sourcePath))) {
        if ($file.FullName -match '\\(\.kilo|\.vscode|\.idea)\\' -or
            $file.Extension -in '.bak', '.tmp', '.log') { continue }
        $relative = if ($source.PSIsContainer) {
            Join-Path $relativePath $file.FullName.Substring($source.FullName.Length + 1)
        } else { $relativePath }
        $sources[$relative] = $file.FullName
    }
}

function Sync-ClientPayload([hashtable]$sources, [string]$payloadPath) {
    New-Item -ItemType Directory -Path $payloadPath -Force | Out-Null
    foreach ($relative in $sources.Keys) {
        Copy-BuildFile $sources[$relative] (Join-Path $payloadPath $relative)
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $payloadPath -File -Recurse -Force)) {
        $relative = $file.FullName.Substring($payloadPath.Length + 1)
        if (-not $sources.ContainsKey($relative)) { Remove-Item -LiteralPath $file.FullName -Force }
    }
    Get-ChildItem -LiteralPath $payloadPath -Directory -Recurse -Force |
        Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
            if (-not (Get-ChildItem -LiteralPath $_.FullName -Force)) {
                Remove-Item -LiteralPath $_.FullName -Force
            }
        }
}
