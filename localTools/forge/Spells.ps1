# The Forge's spell data (modules/mod-forge): the embers forged gear gives off, and the master smith's hammer.
# Ids 92400-92409. Their visual kits (FORGE_*) are in patchSinisterStrike.ps1's $customVisualKits.
#
# Worn forged armour smoulders: the ranks forged on the armour and jewellery the character wears are counted (the
# weapons glow on their own, their visible enchantment), and the more there are, the more the forge shows: embers at
# the feet, then the hands, a molten heart, the head aflame, and at last the Avatar of the Forge, winged in gold.
# mod-forge keeps the one aura the count reaches.

$spells = @(
    @{ Id = 92400; Clone = 2983; Name = 'Braises de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé fume encore de la forge.'
       AuraDescription = 'Au moins 24 rangs de forge sur vos armures et bijoux : votre équipement fume encore de la forge.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers1' } },
    @{ Id = 92401; Clone = 2983; Name = 'Braises de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé rougeoie encore de la forge.'
       AuraDescription = 'Au moins 48 rangs de forge sur vos armures et bijoux : votre équipement rougeoie encore de la forge.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers2' } },
    @{ Id = 92402; Clone = 2983; Name = 'Cœur de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Votre équipement forgé brûle du feu de la forge.'
       AuraDescription = 'Au moins 72 rangs de forge sur vos armures et bijoux : un cœur de métal en fusion bat dans votre poitrine.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers3' } },
    @{ Id = 92404; Clone = 2983; Name = 'Fournaise'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vous portez la fournaise sur vous.'
       AuraDescription = 'Au moins 88 rangs de forge sur vos armures et bijoux : la fournaise vous couronne de flammes.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers4' } },
    @{ Id = 92405; Clone = 2983; Name = 'Avatar de la forge'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Le feu de la forge a fait de vous autre chose qu''un mortel.'
       AuraDescription = 'Au moins 104 rangs de forge sur vos armures et bijoux : le feu de la forge a fait de vous autre chose qu''un mortel.'
       Fields = @{ 40 = 21 }
       Visual = @{ Clone = 10141; Impact = 0; State = 'FORGE_Embers5' } },
    # The master smith's hammer on the anvil: a one-hand strike, molten sparks at the hammer, the clang of metal.
    # Cast by the smith on himself, three times while he forges a piece.
    @{ Id = 92403; Clone = 2983; Name = 'Coup de marteau'; IconPath = 'Interface\Icons\Trade_BlackSmithing'; FallbackIconSpell = 2018; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le maître forgeron frappe l''enclume.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = @{ 40 = 0; 205 = 0; 206 = 0 }
       Visual = @{ Clone = 406; Precast = 0; Cast = 'FORGE_Strike'; Impact = 0 } }
)

return $spells
