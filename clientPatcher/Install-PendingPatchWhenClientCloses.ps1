param(
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK'
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath($clientPath).TrimEnd('\')
$wowPath = Join-Path $clientRoot 'Wow.exe'
# Our base patches (patch-X, -Y and -Z.MPQ) the build could not copy while the client held them
$pendingRoot = Join-Path $clientRoot '_pending'
$logPath = Join-Path $pendingRoot 'install.log'
$pending = @(Get-ChildItem -LiteralPath $pendingRoot -File -Filter 'patch-*.MPQ' -ErrorAction SilentlyContinue)

if (-not $pending) {
    throw "No pending client patch in $pendingRoot"
}

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

$log = @("InstalledAtUtc=$((Get-Date).ToUniversalTime().ToString('o'))")
foreach ($file in $pending) {
    $installedMpqPath = Join-Path $clientRoot "Data\$($file.Name)"
    Copy-Item -LiteralPath $file.FullName -Destination $installedMpqPath -Force
    $log += "$installedMpqPath SHA256=$((Get-FileHash -LiteralPath $installedMpqPath -Algorithm SHA256).Hash)"
    Remove-Item -LiteralPath $file.FullName -Force
}
$log | Set-Content -LiteralPath $logPath -Encoding UTF8
