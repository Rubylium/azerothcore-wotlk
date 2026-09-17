param(
    [string]$clientPath = 'C:\Users\alexi\Documents\GitHub\CleanWOTLK'
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath($clientPath).TrimEnd('\')
$wowPath = Join-Path $clientRoot 'Wow.exe'
$pendingMpqPath = Join-Path $clientRoot '_pending\patch-Z.MPQ'
$installedMpqPath = Join-Path $clientRoot 'Data\patch-Z.MPQ'
$logPath = Join-Path $clientRoot '_pending\install.log'

if (-not (Test-Path -LiteralPath $pendingMpqPath)) {
    throw "Pending client patch not found: $pendingMpqPath"
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

Copy-Item -LiteralPath $pendingMpqPath -Destination $installedMpqPath -Force
$installedHash = (Get-FileHash -LiteralPath $installedMpqPath -Algorithm SHA256).Hash
Remove-Item -LiteralPath $pendingMpqPath -Force

@(
    "InstalledAtUtc=$((Get-Date).ToUniversalTime().ToString('o'))"
    "Path=$installedMpqPath"
    "SHA256=$installedHash"
) | Set-Content -LiteralPath $logPath -Encoding UTF8
