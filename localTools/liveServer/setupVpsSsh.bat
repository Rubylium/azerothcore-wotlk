<# : batch part
@echo off
rem Opens SSH on the live server's Windows VPS so the dev machine can drive it.
rem Run it once on the VPS (double click, accept the admin prompt).
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-Expression ([IO.File]::ReadAllText('%~f0'))"
echo.
pause
exit /b
#>

# PowerShell part
$ErrorActionPreference = 'Stop'

# The dev machine's key (C:\Users\alexi\.ssh\id_ed25519.pub): the only way in
$devKey = 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIERPRFiI13wiPUlGP1lMtHhn15DM7yp2L97CjvgZ4GN7 dev-pc'

function Step($text) { Write-Host "`n== $text" -ForegroundColor Yellow }

try {
    Step 'OpenSSH Server'
    $cap = Get-WindowsCapability -Online -Name 'OpenSSH.Server*' | Select-Object -First 1
    if ($cap.State -ne 'Installed') {
        Write-Host 'Installing (can take a few minutes)...'
        Add-WindowsCapability -Online -Name $cap.Name | Out-Null
    }
    Set-Service -Name sshd -StartupType Automatic
    Start-Service sshd   # the first start writes C:\ProgramData\ssh\sshd_config
    Write-Host 'Installed and running.'

    Step 'Firewall (TCP 22)'
    $rule = Get-NetFirewallRule -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -in 'OpenSSH-Server-In-TCP', 'sshd' }
    if ($rule) {
        # Windows makes its rule Private only; a VPS's internet link is Public
        $rule | Set-NetFirewallRule -Enabled True -Profile Any
    } else {
        New-NetFirewallRule -Name sshd -DisplayName 'OpenSSH Server (sshd)' -Enabled True -Direction Inbound `
            -Protocol TCP -Action Allow -LocalPort 22 | Out-Null
    }
    Write-Host 'Port 22 open.'

    Step 'Dev machine key'
    # Administrators read their keys from here, never from ~\.ssh
    $keyFile = 'C:\ProgramData\ssh\administrators_authorized_keys'
    $keys = @()
    if (Test-Path $keyFile) { $keys = @(Get-Content $keyFile | Where-Object { $_.Trim() }) }
    $keyBody = ($devKey -split ' ')[1]
    if (-not ($keys | Where-Object { $_ -like "*$keyBody*" })) { $keys += $devKey }
    [IO.File]::WriteAllLines($keyFile, [string[]]$keys)
    # sshd ignores the file unless only Administrators and SYSTEM can touch it (SIDs: any Windows language)
    icacls $keyFile /inheritance:r /grant '*S-1-5-32-544:F' /grant '*S-1-5-18:F' | Out-Null
    Write-Host 'Key added.'

    Step 'Default shell: PowerShell'
    New-Item -Path 'HKLM:\SOFTWARE\OpenSSH' -Force | Out-Null
    New-ItemProperty -Path 'HKLM:\SOFTWARE\OpenSSH' -Name DefaultShell -PropertyType String -Force `
        -Value "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" | Out-Null
    Write-Host 'Done.'

    Step 'Key login only'
    $config = 'C:\ProgramData\ssh\sshd_config'
    $lines = Get-Content $config | Where-Object { $_ -notmatch '^\s*#?\s*(PasswordAuthentication|PubkeyAuthentication)\s' }
    # Settings before any Match block apply to everyone
    $lines = @('PubkeyAuthentication yes', 'PasswordAuthentication no') + $lines
    [IO.File]::WriteAllLines($config, [string[]]$lines)
    Restart-Service sshd
    Write-Host 'Passwords refused over SSH (RDP is unchanged).'

    $ip = try { (Invoke-RestMethod -Uri 'https://api.ipify.org' -TimeoutSec 10).Trim() } catch { '<vps-ip>' }
    Write-Host "`nAll set. From the dev machine:" -ForegroundColor Green
    Write-Host "    ssh $env:USERNAME@$ip" -ForegroundColor Green
    Write-Host "`nIf it times out, allow TCP 22 in the OVHcloud Network Firewall too (Control Panel > IP)."
} catch {
    Write-Host "`nFAILED: $($_.Exception.Message)" -ForegroundColor Red
}
