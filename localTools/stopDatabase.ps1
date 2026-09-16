$ErrorActionPreference = 'Stop'

$mysqlAdmin = 'C:\laragon\bin\mysql\mysql-8.0.30-winx64\bin\mysqladmin.exe'

if (-not (Test-Path -LiteralPath $mysqlAdmin)) {
    throw "mysqladmin not found: $mysqlAdmin"
}

& $mysqlAdmin --protocol=tcp --host=127.0.0.1 --port=3307 --user=root shutdown

if ($LASTEXITCODE -ne 0) {
    throw "mysqladmin exited with code $LASTEXITCODE."
}

Write-Host 'AzerothCore MySQL stopped.'
