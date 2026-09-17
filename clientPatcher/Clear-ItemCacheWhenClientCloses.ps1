param(
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK'
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath($clientPath).TrimEnd('\')
$wowPath = Join-Path $clientRoot 'Wow.exe'
$cacheRoot = Join-Path $clientRoot 'Cache\WDB'
$logPath = Join-Path $clientRoot 'Cache\item-cache-reset.log'

do {
    $clientProcesses = @(Get-Process -Name 'Wow' -ErrorAction SilentlyContinue | Where-Object {
        try {
            [System.IO.Path]::GetFullPath($_.Path) -eq $wowPath
        }
        catch {
            $true
        }
    })

    if ($clientProcesses.Count -gt 0) {
        Start-Sleep -Milliseconds 500
    }
} while ($clientProcesses.Count -gt 0)

$cacheFiles = @(Get-ChildItem -LiteralPath $cacheRoot -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
    $_.Name -ieq 'itemcache.wdb' -or $_.Name -ieq 'itemtextcache.wdb'
})

foreach ($cacheFile in $cacheFiles) {
    Remove-Item -LiteralPath $cacheFile.FullName -Force
}

@(
    "ClearedAtUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    "RemovedFiles=$($cacheFiles.Count)"
) | Set-Content -LiteralPath $logPath -Encoding UTF8
