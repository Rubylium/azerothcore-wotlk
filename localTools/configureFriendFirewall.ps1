$ErrorActionPreference = 'Stop'

$rules = @(
    @{
        name = 'RubyEbon AzerothCore Auth TCP 3724'
        description = 'Allows remote WotLK clients to authenticate with the local RubyEbon AzerothCore server.'
        port = 3724
        program = 'C:\Users\alexi\Documents\GitHub\TestWoW\server\authserver.exe'
    },
    @{
        name = 'RubyEbon AzerothCore World TCP 8085'
        description = 'Allows remote WotLK clients to connect to the local RubyEbon AzerothCore world server.'
        port = 8085
        program = 'C:\Users\alexi\Documents\GitHub\TestWoW\server\worldserver.exe'
    }
)

foreach ($definition in $rules) {
    Get-NetFirewallRule -DisplayName $definition.name -ErrorAction SilentlyContinue | Remove-NetFirewallRule
    New-NetFirewallRule `
        -DisplayName $definition.name `
        -Description $definition.description `
        -Direction Inbound `
        -Action Allow `
        -Enabled True `
        -Profile Private `
        -Protocol TCP `
        -LocalPort $definition.port `
        -Program $definition.program | Out-Null
}
