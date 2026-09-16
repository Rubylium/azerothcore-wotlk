$ErrorActionPreference = 'Stop'

$mysqlRoot = 'C:\laragon\bin\mysql\mysql-8.0.30-winx64'
$mysqlServer = Join-Path $mysqlRoot 'bin\mysqld.exe'
$mysqlConfig = Join-Path $mysqlRoot 'my.ini'
$mysqlPort = 3307

function Test-TcpPort {
    param(
        [string]$hostName,
        [int]$port
    )

    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $connectTask = $client.ConnectAsync($hostName, $port)
        return $connectTask.Wait(500) -and $client.Connected
    }
    catch {
        return $false
    }
    finally {
        $client.Dispose()
    }
}

if (Test-TcpPort -hostName '127.0.0.1' -port $mysqlPort) {
    Write-Host "AzerothCore MySQL is already listening on port $mysqlPort."
    exit 0
}

if (-not (Test-Path -LiteralPath $mysqlServer)) {
    throw "MySQL server not found: $mysqlServer"
}

Start-Process `
    -FilePath $mysqlServer `
    -ArgumentList "--defaults-file=$mysqlConfig", "--port=$mysqlPort" `
    -WorkingDirectory $mysqlRoot `
    -WindowStyle Hidden

$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 500
} while (-not (Test-TcpPort -hostName '127.0.0.1' -port $mysqlPort) -and (Get-Date) -lt $deadline)

if (-not (Test-TcpPort -hostName '127.0.0.1' -port $mysqlPort)) {
    throw "MySQL did not become ready on port $mysqlPort."
}

Write-Host "AzerothCore MySQL is ready on 127.0.0.1:$mysqlPort."
