param(
    [string]$awesomeWotlkPath = 'C:\Users\alexi\Documents\GitHub\awesome_wotlk',
    [string]$vcvarsPath = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat'
)

# The client extension DLL (awesome_wotlk): a 32-bit Ninja build in the Visual Studio x86 environment. The DLL lands in
# build\Release\AwesomeWotlkLib.dll, which clientPatcher/Build-FriendPatch.ps1 ships.
$ErrorActionPreference = 'Stop'

foreach ($requiredPath in @($awesomeWotlkPath, $vcvarsPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required path not found: $requiredPath"
    }
}

Write-Host 'Building AwesomeWotlkLib (x86)...'
Push-Location $awesomeWotlkPath
try {
    & cmd.exe /c "`"$vcvarsPath`" >nul && cmake --build out\build\win32 --target AwesomeWotlkLib"
    if ($LASTEXITCODE -ne 0) {
        throw "AwesomeWotlkLib build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
Write-Host "Built $(Join-Path $awesomeWotlkPath 'build\Release\AwesomeWotlkLib.dll')"
