# The Forge's spell data (modules/mod-forge): the embers forged gear gives off, and the master smith's hammer.
# Ids 92400-92409. Their visual kits (FORGE_*) are in patchSinisterStrike.ps1's $customVisualKits.
#
# Worn forged gear smoulders: the ranks forged on everything the character wears are counted, and the more there
# are, the more of the character the forge's embers reach - the feet, then the hands too, then the whole body.
# mod-forge keeps the one aura the count reaches. The weapons glow on their own (their visible enchantment).

$spells = @(
    @{ Id = 92400; Clone = 2983; Name = 'Braises de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé fume encore de la forge.'
       AuraDescription = 'Au moins 24 rangs de forge portés : votre équipement fume encore de la forge.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers1' } },
    @{ Id = 92401; Clone = 2983; Name = 'Braises de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé rougeoie encore de la forge.'
       AuraDescription = 'Au moins 64 rangs de forge portés : votre équipement rougeoie encore de la forge.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers2' } },
    @{ Id = 92402; Clone = 2983; Name = 'Braises de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé brûle du feu de la forge.'
       AuraDescription = 'Au moins 112 rangs de forge portés : votre équipement brûle du feu de la forge.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers3' } },
    # The master smith's hammer on the anvil: a one-hand strike, molten sparks at the hammer, the clang of metal.
    # Cast by the smith on himself, three times while he forges a piece.
    @{ Id = 92403; Clone = 2983; Name = 'Coup de marteau'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le maître forgeron frappe l''enclume.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 40 = 0; 205 = 0; 206 = 0 }
       Visual = @{ Clone = 406; Precast = 0; Cast = 'FORGE_Strike'; Impact = 0 } }
)

return $spells
