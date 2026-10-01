# The Druid's spell data for its retail-style talent trees (localTools/druid/talentTree.json): the abilities its nodes
# and specializations teach, the auras its scripted talents show, the hits the module hands out, and the rank spells
# of its new talents. Behaviour lives in modules/mod-druid; this file owns client/server spell data only. The stock
# Druid spells it changes in place (Shred from any side, Tiger's Fury in percent, Force of Nature and Tranquility on
# retail cooldowns...) are in localTools/druid/StockSpells.ps1.
# Ids 95900-96199: 95900-95999 talent ranks (generated from the tree), 96000-96179 abilities, auras and hits,
# 96180-96189 the specializations' passives.
#
# Clone field notes: 1 category, 4 attributes (0x10000 not while shapeshifted: cat and bear; moonkin and the tree act
# as the normal form), 5 attributes ex (0x100000 a finishing move), 6 attributes ex 2 (0x80000 allowed out of the
# stances named), 9 attributes ex 5, 12 stances (0x1 cat, 0x2 tree, 0x10 bear, 0x80 dire bear, 0x40000000 moonkin),
# 14 stances not, 28 casting time index (1 instant, 16 1.5 s, 5 2 s), 29 cooldown, 30 category cooldown, 40 duration
# index (39 2 s, 32 6 s, 31 8 s, 1 10 s, 29 12 s, 8 15 s, 18 20 s, 106 24 s, 9 30 s, 22 45 s, 21 never), 41 power type
# (0 mana, 1 rage, 3 energy), 42 cost (rage in tenths), 46 range index (1 self, 2 melee, 4 30 yd, 5 40 yd, 6 100 yd),
# 47 speed (0: the hit lands with the cast), 49 stacks, 83-85 effect mechanic (7 root, 11 snare, 15 bleed), 86-88
# target A, 89-91 target B (15 enemies around the source, 16 enemies around the spot), 92-94 radius index (14 8 yd, 13
# 10 yd, 18 15 yd), 98-100 periodic interval, 122-130 the effects' class masks, 131 visual, 204 cost as a share of base
# mana, 205-206 global cooldown category and time (0 0: off the global cooldown), 208 family (7 Druid), 209-211 family
# flags, 212 maximum targets (0 every one), 213 damage class (1 magic, 2 melee), 225 school (1 physical, 8 nature,
# 64 arcane, 72 astral: arcane and nature).
#
# Targets: 1 the caster, 6 the enemy target, 20 the caster's party around it, 21 the friendly target, 22 around the
# caster (with target B 15), 53 the target's spot (with target B 16, the enemies around it), 87 the chosen spot.
#
# The Druid keeps mana, its forms, energy and combo points (cat) and rage (bear). Balance's Astral Power (Puissance
# astrale, 96000) is an aura of up to 100 stacks that mod-druid fills and spends; Starsurge and Starfall cost it, not
# mana. Damage and heals the module works out (the relays, Primal Wrath, the bleeds it sizes) go through spells with
# explicit zero coefficients in modules/mod-druid's SQL; the other new spells carry their coefficients there.

$classMask = 1024
$balance = 574
$feral = 134
$restoration = 573

# The new abilities' own family flags, word 2 (bits 0x400000-0x80000000 are free among the Druid's spells; the core
# reads none of them). Only the spells a talent or the module's modifiers name carry one.
$flagStarsurge = 0x400000
$flagStarfall = 0x800000
$flagSunfire = 0x1000000
$flagThrash = 0x2000000
$flagPrimalWrath = 0x4000000
$flagFeralFrenzy = 0x8000000
$flagEfflorescence = 0x10000000
$flagStellarFlare = 0x20000000
$flagFuryOfElune = 0x40000000
$flagGermination = 0x80000000

# The stock spells the modifiers name: word 0 Wrath 0x1, Moonfire 0x2, Starfire 0x4, Rejuvenation 0x10, Regrowth 0x40;
# word 1 Mangle (Bear) 0x40, Swiftmend 0x2, Wild Growth 0x4000000; Berserk's own masks for the cat's techniques
$maskWrath = 0x1
$maskStarfire = 0x4
$maskRejuvenation = 0x10
$maskRegrowth = 0x40
$maskMangleBear = 0x40
$berserkMasks = @(0x839000, 0x30000480, 0x40420)

# Moonfire's layout for an instant spell on the enemy (caster and moonkin forms); Wrath's for a cast one; Shadowfury's
# for a spell aimed at a spot on the ground; Swipe (Bear)'s for a strike around the Druid; Shred's for a cat strike;
# Ferocious Bite's for a finishing move; Rip's for a bleed; Rejuvenation's for a heal over time; Innervate's for a spell
# on an ally; Holy Shock's heal for one the module hands out (instant, triggered, 100 yd); Death Coil's for damage the
# module or a coefficient sets (its speed and visual cleared); Sprint's for an instant self buff
$moonfire = 48463
$wrath = 48461
$ground = 47847
$swipeBear = 48562
$shred = 48572
$ferociousBite = 48577
$rip = 49800
$rejuvenation = 48441
$innervate = 29166
$healHit = 25914
$computed = 47632
$selfBuff = 2983

# Every clone made a Druid spell: its own category and flags. Its forms come from its clone unless it says otherwise.
function Own($flag, $extra = @{}) {
    $fields = @{ 1 = 0; 30 = 0; 208 = 7; 209 = 0; 210 = 0; 211 = $flag }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A self buff usable in every form (Sprint's attributes cleared: it is not allowed shapeshifted), off the global
# cooldown, mana
function SelfBuff($flag, $extra = @{}) {
    $fields = Own $flag @{ 4 = 0; 5 = 0; 12 = 0; 14 = 0; 41 = 0; 205 = 0; 206 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A hit the module hands out: 100 yd, landing with its cast
function Hit($flag, $school, $visual, $extra = @{}) {
    $fields = Own $flag @{ 46 = 6; 47 = 0; 131 = $visual; 213 = 1; 225 = $school }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A relay carries an amount already final: no damage class (no second roll, no second critical strike) and no caster
# modifiers (attributes ex 3 0x20000000)
function Relay($school) {
    return (Own 0 @{ 7 = 0x60000200; 21 = 0; 46 = 13; 47 = 0; 131 = 0; 213 = 0; 225 = $school })
}

# A bleed the module sizes per tick (Rip's layout, not a finishing move any more, no cost)
function Bleed($flag, $duration) {
    return (Own $flag @{ 5 = 0x200; 40 = $duration; 41 = 3; 46 = 6; 83 = 15; 98 = 2000; 213 = 2; 225 = 1 })
}

$spells = @(
    # --- Resources, procs and their auras (mod-druid fills, reads and spends them) ---------------------------------
    # Puissance astrale (Astral Power): Balance's resource, 100 stacks; fades 15 s after combat
    @{ Id = 96000; Clone = 2983; Name = 'Puissance astrale'; IconPath = 'Interface\Icons\Spell_Arcane_StarFire'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; CantCancel = $true; MaxStacks = 100; Spellbook = $false
       Description = 'Puissance astrale.'; AuraDescription = 'Points de puissance astrale : Éruption stellaire en consomme 40, Météores 50.'; Fields = @{ 1 = 0; 4 = 0x80000000; 40 = 21; 208 = 7; 209 = 0 } },
    # Éclipse solaire: Wrath (word 0 0x1) 20% quicker to cast, nature damage 15% more
    @{ Id = 96001; Clone = 2983; Name = 'Éclipse solaire'; IconPath = 'Interface\Icons\Ability_Druid_EclipseOrange'; FallbackIconSpell = 48461; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Éclipse solaire.'; AuraDescription = 'Colère est 20% plus rapide à incanter, et vos dégâts de Nature augmentent de 15%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -20; Misc = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 8 })
       Fields = @{ 1 = 0; 4 = 0; 40 = 8; 122 = $maskWrath; 123 = 0; 124 = 0; 208 = 7; 209 = 0 } },
    # Éclipse lunaire: Starfire (word 0 0x4) 20% quicker to cast, arcane damage 15% more; mod-druid widens its splash
    @{ Id = 96002; Clone = 2983; Name = 'Éclipse lunaire'; IconPath = 'Interface\Icons\Ability_Druid_Eclipse'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Éclipse lunaire.'; AuraDescription = 'Feu stellaire est 20% plus rapide à incanter et frappe plus fort les ennemis proches de sa cible, et vos dégâts des Arcanes augmentent de 15%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -20; Misc = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 64 })
       Fields = @{ 1 = 0; 4 = 0; 40 = 8; 122 = $maskStarfire; 123 = 0; 124 = 0; 208 = 7; 209 = 0 } },
    # Seigneur des étoiles (Starlord): 3% spell haste a stack, 3 stacks (the core multiplies the amount by them)
    @{ Id = 96003; Clone = 2983; Name = 'Seigneur des étoiles'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane02'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Hâte augmentée.'; AuraDescription = 'Hâte des sorts augmentée de 3% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 65; TargetA = 1; Value = 3 }); Fields = @{ 1 = 0; 4 = 0; 40 = 8; 208 = 7; 209 = 0 } },
    # Tisse-étoiles (Starweaver): the next Starfall, or the next Starsurge, free (mod-druid)
    @{ Id = 96004; Clone = 2983; Name = 'Tisse-étoiles : Météores'; IconPath = 'Interface\Icons\Ability_Druid_Starfall'; FallbackIconSpell = 53201; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Météores gratuits.'; AuraDescription = 'Vos prochains Météores ne coûtent aucune puissance astrale.'; Fields = @{ 1 = 0; 4 = 0; 40 = 9; 208 = 7; 209 = 0 } },
    @{ Id = 96005; Clone = 2983; Name = 'Tisse-étoiles : Éruption'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane03'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Éruption stellaire gratuite.'; AuraDescription = 'Votre prochaine Éruption stellaire ne coûte aucune puissance astrale.'; Fields = @{ 1 = 0; 4 = 0; 40 = 9; 208 = 7; 209 = 0 } },
    # Griffes sanglantes (Bloodtalons): 2 stacks, each spent by a Rip, a Ferocious Bite or a Primal Wrath (mod-druid)
    @{ Id = 96006; Clone = 2983; Name = 'Griffes sanglantes'; IconPath = 'Interface\Icons\Spell_Shadow_VampiricAura'; FallbackIconSpell = 49800; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Griffes sanglantes.'; AuraDescription = 'Vos prochains Déchirure, Morsure féroce ou Courroux primordial infligent 30% de dégâts en plus.'; Fields = @{ 1 = 0; 4 = 0; 40 = 9; 208 = 7; 209 = 0 } },
    # Rapidité du prédateur (Predatory Swiftness): the next Regrowth (word 0 0x40) instant and free
    @{ Id = 96007; Clone = 2983; Name = 'Rapidité du prédateur'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Cat'; FallbackIconSpell = 48443; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Rapidité du prédateur.'; AuraDescription = 'Votre prochain Rétablissement est instantané et ne coûte pas de mana.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 1 = 0; 4 = 0; 40 = 9; 122 = $maskRegrowth; 123 = 0; 124 = 0; 125 = $maskRegrowth; 126 = 0; 127 = 0; 208 = 7; 209 = 0 } },
    # Encorner (Gore): the next Mangle (Bear) gives 4 more rage (mod-druid)
    @{ Id = 96008; Clone = 2983; Name = 'Encorner'; IconPath = 'Interface\Icons\Ability_Druid_Lacerate'; FallbackIconSpell = 48564; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Encorner.'; AuraDescription = 'Votre prochaine Mutilation (ours) vous rend 4 points de rage de plus.'; Fields = @{ 1 = 0; 4 = 0; 40 = 1; 208 = 7; 209 = 0 } },
    # Âme de la forêt (Restoration): the next Regrowth 150% stronger (mod-druid)
    @{ Id = 96009; Clone = 2983; Name = 'Âme de la forêt'; IconPath = 'Interface\Icons\Ability_Druid_ManaTree'; FallbackIconSpell = 48443; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Âme de la forêt.'; AuraDescription = 'Votre prochain Rétablissement soigne 150% de plus.'; Fields = @{ 1 = 0; 4 = 0; 40 = 8; 208 = 7; 209 = 0 } },
    # Vortex d'Ursol's snare, put back every second while the vortex lasts
    @{ Id = 96012; Clone = 49236; Name = "Vortex d'Ursol"; IconPath = 'Interface\Icons\Spell_Nature_Cyclone'; FallbackIconSpell = 33786; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Ralenti.'; AuraDescription = 'Vitesse de déplacement réduite de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 33; TargetA = 6; Value = -50 })
       Fields = (Own 0 @{ 40 = 39; 46 = 6; 83 = 11; 131 = 0; 204 = 0; 205 = 0; 206 = 0; 225 = 8 }) },

    # --- Class tree -------------------------------------------------------------------------------------------------
    # Renouveau (Renewal): 30% of maximum health at once, every form, off the global cooldown
    @{ Id = 96020; Clone = $selfBuff; Name = 'Renouveau'; IconPath = 'Interface\Icons\Spell_Nature_NatureBlessing'; FallbackIconSpell = 48441; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous soignez instantanément 30% de vos points de vie maximum. Utilisable sous toutes les formes.'
       Effects = @(@{ Index = 0; Effect = 136; TargetA = 1; Value = 30 })
       Fields = (SelfBuff 0 @{ 40 = 0; 131 = 58; 225 = 8 }) },
    # Charge sauvage (Wild Charge): mod-druid casts Feral Charge in bear form, its cat leap in cat form
    @{ Id = 96021; Clone = 16979; Name = 'Charge sauvage'; IconPath = 'Interface\Icons\Ability_Hunter_Pet_Bear'; FallbackIconSpell = 16979; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = "Fond sur l'ennemi selon votre forme : en forme d'ours, vous chargez et l'immobilisez 4 s ; en forme de félin, vous bondissez derrière lui et l'hébétez 3 s. Entre 8 et 25 m."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own 0 @{ 12 = 0x91; 41 = 1; 131 = 0; 205 = 0; 206 = 0 }) },
    # Vortex d'Ursol (Ursol's Vortex): aimed at a spot; mod-druid slows the enemies within 8 yd every second for 10 s
    @{ Id = 96022; Clone = $ground; Name = "Vortex d'Ursol"; IconPath = 'Interface\Icons\Spell_Nature_Cyclone'; FallbackIconSpell = 33786; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Fait naître un vortex à l'endroit visé pendant 10 s : les ennemis à 8 m sont ralentis de 50%."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own 0 @{ 28 = 1; 40 = 0; 46 = 4; 131 = 9489; 204 = 6; 205 = 133; 206 = 1500; 213 = 1; 225 = 8 }) },
    # Enchevêtrement de masse (Mass Entanglement): roots on the target and the enemies within 10 yd of it, 10 s
    @{ Id = 96023; Clone = 53308; Name = 'Enchevêtrement de masse'; IconPath = 'Interface\Icons\Spell_Nature_StrangleVines'; FallbackIconSpell = 53308; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Des racines immobilisent la cible et les ennemis à 10 m d'elle pendant 10 s."
       AuraDescription = 'Immobilisé.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 26; TargetA = 53 })
       Fields = (Own 0 @{ 28 = 1; 32 = 0; 34 = 0; 35 = 0; 40 = 1; 83 = 7; 89 = 16; 92 = 13; 204 = 6 }) },
    # Rugissement grégaire (Stampeding Roar): the Druid's party within 15 yd 60% faster for 8 s, every form
    @{ Id = 96024; Clone = $selfBuff; Name = 'Rugissement grégaire'; IconPath = 'Interface\Icons\Ability_Druid_ChallangingRoar'; FallbackIconSpell = 5209; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous et les membres de votre groupe à 15 m courez 60% plus vite pendant 8 s. Utilisable sous toutes les formes.'
       AuraDescription = 'Vitesse de déplacement augmentée de 60%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 20; Value = 60 })
       Fields = (SelfBuff 0 @{ 40 = 31; 92 = 18; 131 = 3949; 225 = 1 }) },
    # Rossée puissante (Mighty Bash): Bash's 4 s stun in every form, no cost
    @{ Id = 96025; Clone = 8983; Name = 'Rossée puissante'; IconPath = 'Interface\Icons\Ability_Druid_Bash'; FallbackIconSpell = 8983; Cost = 0; Cooldown = 50000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = 'Assomme la cible pendant 4 s. Utilisable sous toutes les formes.'
       AuraDescription = 'Assommé.'
       Fields = (Own 0 @{ 12 = 0; 40 = 35; 41 = 0; 205 = 133; 206 = 1500 }) },
    # Cœur sauvage (Heart of the Wild): 20% more damage and healing for 45 s, every form
    @{ Id = 96026; Clone = $selfBuff; Name = 'Cœur sauvage'; IconPath = 'Interface\Icons\Spell_Holy_BlessingOfAgility'; FallbackIconSpell = 17003; Cost = 0; Cooldown = 300000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pendant 45 s, vos dégâts et vos soins augmentent de 20%. Utilisable sous toutes les formes.'
       AuraDescription = 'Dégâts et soins augmentés de 20%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 20; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 136; TargetA = 1; Value = 20 })
       Fields = (SelfBuff 0 @{ 40 = 22; 131 = 4040; 225 = 8 }) },

    # --- Équilibre ----------------------------------------------------------------------------------------------------
    # Éruption stellaire (Starsurge): 40 Astral Power (mod-druid), astral damage, instant
    @{ Id = 96040; Clone = $moonfire; Name = 'Éruption stellaire'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane03'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Consomme 40 points de puissance astrale : une éruption d'énergie céleste inflige de lourds dégâts astraux à la cible."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1800 })
       Fields = (Own $flagStarsurge @{ 40 = 0; 131 = 1264; 204 = 0; 225 = 72 }) },
    # Éclat solaire (Sunfire): nature damage and a nature effect over 12 s; with Éclat solaire amélioré mod-druid spreads
    # it over the enemies within 8 yd of the target
    @{ Id = 96041; Clone = $moonfire; Name = 'Éclat solaire'; IconPath = 'Interface\Icons\Spell_Fire_SunKey'; FallbackIconSpell = 48463; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Brûle l'ennemi d'un éclat de soleil : des dégâts de Nature, puis d'autres toutes les 2 s pendant 12 s. Vous rend 2 points de puissance astrale."
       AuraDescription = 'Dégâts de Nature toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 230 },
           @{ Index = 1; Effect = 2; TargetA = 6; Value = 450 })
       Fields = (Own $flagSunfire @{ 40 = 29; 98 = 2000; 131 = 1263; 204 = 12; 225 = 8 }) },
    # Embrasement stellaire (Stellar Flare): 1.5 s, astral damage and an effect over 24 s
    @{ Id = 96042; Clone = $wrath; Name = 'Embrasement stellaire'; IconPath = 'Interface\Icons\Spell_Arcane_Arcane04'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Embrase la cible d'un feu stellaire : des dégâts astraux, puis d'autres toutes les 2 s pendant 24 s. Vous rend 8 points de puissance astrale."
       AuraDescription = 'Dégâts astraux toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 300 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 160 })
       Fields = (Own $flagStellarFlare @{ 28 = 16; 40 = 106; 47 = 0; 99 = 2000; 131 = 1264; 204 = 10; 225 = 72 }) },
    # Alignement céleste (Celestial Alignment): 20 s, 10% spell haste; mod-druid puts up both Eclipses
    @{ Id = 96043; Clone = $selfBuff; Name = 'Alignement céleste'; IconPath = 'Interface\Icons\Spell_Nature_StarFall'; FallbackIconSpell = 53201; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous alignez les astres pendant 20 s : vous entrez dans les deux Éclipses et votre hâte augmente de 10%.'
       AuraDescription = 'Les deux Éclipses ; hâte des sorts augmentée de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 65; TargetA = 1; Value = 10 })
       Fields = (SelfBuff 0 @{ 4 = 0x10000; 40 = 18; 131 = 11571; 225 = 64 }) },
    # Incarnation : Élu d'Élune (Incarnation: Chosen of Elune): 30 s, 10% haste and 10% spell critical strike; mod-druid
    # puts up both Eclipses
    @{ Id = 96044; Clone = $selfBuff; Name = "Incarnation : Élu d'Élune"; IconPath = 'Interface\Icons\Ability_Druid_Eclipse'; FallbackIconSpell = 53201; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous devenez l'Élu d'Élune pendant 30 s : vous entrez dans les deux Éclipses, votre hâte augmente de 10% et vos chances de coup critique avec les sorts de 10%."
       AuraDescription = 'Les deux Éclipses ; hâte et coups critiques des sorts augmentés de 10%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 65; TargetA = 1; Value = 10 },
           @{ Index = 1; Effect = 6; Aura = 57; TargetA = 1; Value = 10 })
       Fields = (SelfBuff 0 @{ 4 = 0x10000; 40 = 9; 131 = 11571; 225 = 64 }) },
    # Furie d'Élune (Fury of Elune): on the target; mod-druid strikes the enemies within 8 yd of it every second for 8 s
    # (96051) and gives 5 Astral Power each time
    @{ Id = 96045; Clone = $moonfire; Name = "Furie d'Élune"; IconPath = 'Interface\Icons\Spell_Holy_ElunesGrace'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask
       Description = "Appelle un rayon de lumière d'Élune sur la cible pendant 8 s : chaque seconde, des dégâts astraux frappent les ennemis à 8 m d'elle et vous rendent 5 points de puissance astrale."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagFuryOfElune @{ 40 = 0; 131 = 11571; 204 = 0; 225 = 72 }) },
    # Communion astrale (Astral Communion): 60 Astral Power at once (mod-druid), off the global cooldown
    @{ Id = 96046; Clone = $selfBuff; Name = 'Communion astrale'; IconPath = 'Interface\Icons\Spell_Nature_AstralRecalGroup'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 60000; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask; NoEquipment = $true
       Description = 'Vous communiez avec les astres : vous gagnez aussitôt 60 points de puissance astrale.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (SelfBuff 0 @{ 4 = 0x10000; 40 = 0; 131 = 4040; 225 = 64 }) },
    # The hits mod-druid hands out: Starfire's splash (a relay of its damage), a shooting star, Starfall's and Fury of
    # Elune's strikes, Orbit Breaker's full moon
    @{ Id = 96047; Clone = $computed; Name = 'Feu stellaire (éclaboussure)'; IconPath = 'Interface\Icons\Spell_Arcane_StarFire'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts des Arcanes.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Relay 64) },
    @{ Id = 96048; Clone = $computed; Name = 'Étoile filante'; IconPath = 'Interface\Icons\Spell_Arcane_StarFire'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts astraux.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 500 })
       Fields = (Hit 0 72 11040) },
    @{ Id = 96050; Clone = $computed; Name = 'Météores'; IconPath = 'Interface\Icons\Ability_Druid_Starfall'; FallbackIconSpell = 53201; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts des Arcanes.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 250 })
       Fields = (Hit $flagStarfall 64 11040) },
    @{ Id = 96051; Clone = $computed; Name = "Furie d'Élune"; IconPath = 'Interface\Icons\Spell_Holy_ElunesGrace'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts astraux.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 300 })
       Fields = (Hit $flagFuryOfElune 72 0) },
    @{ Id = 96052; Clone = $computed; Name = 'Pleine lune'; IconPath = 'Interface\Icons\Spell_Nature_StarFall'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts des Arcanes.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 2500 })
       Fields = (Hit 0 64 1264) },
    # Météores (Starfall): 50 Astral Power (mod-druid); for 8 s, every second, the enemies within 30 yd of the Druid
    # are struck (96050)
    @{ Id = 96049; Clone = $selfBuff; Name = 'Météores'; IconPath = 'Interface\Icons\Ability_Druid_Starfall'; FallbackIconSpell = 53201; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $balance; ClassMask = $classMask; NoEquipment = $true
       Description = "Consomme 50 points de puissance astrale : pendant 8 s, des étoiles s'abattent chaque seconde sur les ennemis à 30 m de vous."
       AuraDescription = 'Des étoiles frappent les ennemis à 30 m.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (SelfBuff $flagStarfall @{ 4 = 0x10000; 40 = 31; 131 = 11571; 205 = 133; 206 = 1500; 225 = 64 }) },

    # --- Combat farouche ---------------------------------------------------------------------------------------------
    # Rosser (félin) (Thrash, cat): physical damage and a bleed within 8 yd, 40 energy; mod-druid gives a combo point
    @{ Id = 96060; Clone = $swipeBear; Name = 'Rosser (félin)'; IconPath = 'Interface\Icons\Ability_Druid_Swipe'; FallbackIconSpell = 62078; Cost = 40; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = 'Rosse les ennemis à 8 m : des dégâts physiques, puis un saignement toutes les 3 s pendant 15 s. Vous rapporte 1 point de combo.'
       AuraDescription = 'Saigne toutes les 3 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 150 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 22; Value = 80 })
       Fields = (Own $flagThrash @{ 12 = 0x1; 40 = 8; 41 = 3; 84 = 15; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 99 = 3000; 206 = 1000; 212 = 0; 213 = 2; 225 = 1 }) },
    # Rosser (ours) (Thrash, bear): the same, 6 s, its bleed stacks 3 times, 5 rage
    @{ Id = 96061; Clone = $swipeBear; Name = 'Rosser (ours)'; IconPath = 'Interface\Icons\Ability_Druid_Swipe'; FallbackIconSpell = 48562; Cost = 0; Cooldown = 6000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = 'Rosse les ennemis à 8 m : des dégâts physiques, puis un saignement toutes les 3 s pendant 15 s, cumulable 3 fois. Vous rend 5 points de rage.'
       AuraDescription = 'Saigne toutes les 3 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 150 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 22; Value = 80 },
           @{ Index = 2; Effect = 30; TargetA = 1; Value = 50; Misc = 1 })
       Fields = (Own $flagThrash @{ 12 = 0x90; 40 = 8; 41 = 1; 49 = 3; 84 = 15; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 99 = 3000; 206 = 1500; 212 = 0; 213 = 2; 225 = 1 }) },
    # Fourrure de fer (Ironfur): 40 rage, 25% armor a stack for 8 s, 3 stacks, off the global cooldown
    @{ Id = 96062; Clone = $selfBuff; Name = 'Fourrure de fer'; IconPath = 'Interface\Icons\Ability_Druid_SkinTeeth'; FallbackIconSpell = 22812; Cost = 400; Cooldown = 1000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask; NoEquipment = $true
       Description = 'Consomme 40 points de rage : votre armure augmente de 25% pendant 8 s, cumulable 3 fois. Hors du temps de recharge global.'
       AuraDescription = 'Armure augmentée de 25% par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 101; TargetA = 1; Value = 25; Misc = 1 })
       Fields = (SelfBuff 0 @{ 12 = 0x90; 40 = 31; 41 = 1; 49 = 3; 131 = 6662; 225 = 1 }) },
    # Courroux primordial (Primal Wrath): a finishing move, 20 energy; mod-druid strikes the enemies within 8 yd (96071)
    # and bleeds them (96072) by the combo points spent
    @{ Id = 96066; Clone = $ferociousBite; Name = 'Courroux primordial'; IconPath = 'Interface\Icons\Ability_Druid_Disembowel'; FallbackIconSpell = 49800; Cost = 20; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = "Coup de grâce : frappe les ennemis à 8 m, qui subissent des dégâts physiques et une Déchirure primordiale, plus longue et plus forte selon vos points de combo."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Fields = (Own $flagPrimalWrath @{ 131 = 3941 }) },
    @{ Id = 96071; Clone = $computed; Name = 'Courroux primordial'; IconPath = 'Interface\Icons\Ability_Druid_Disembowel'; FallbackIconSpell = 49800; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Hit $flagPrimalWrath 1 0 @{ 213 = 2 }) },
    @{ Id = 96072; Clone = $rip; Name = 'Déchirure primordiale'; IconPath = 'Interface\Icons\Ability_GhoulFrenzy'; FallbackIconSpell = 49800; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Saigne toutes les 2 s.'; AuraDescription = 'Saigne toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 1 })
       Fields = (Bleed $flagPrimalWrath 29) },
    # Frénésie farouche (Feral Frenzy): 350% weapon damage, 5 combo points, 25 energy; mod-druid adds its bleed (96073)
    @{ Id = 96067; Clone = $shred; Name = 'Frénésie farouche'; IconPath = 'Interface\Icons\Ability_Druid_Rake'; FallbackIconSpell = 48574; Cost = 25; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = 'Vous lacérez la cible d''une frénésie de coups : de lourds dégâts physiques, un saignement pendant 6 s, et 5 points de combo.'
       Effects = @(
           @{ Index = 0; Effect = 31; TargetA = 6; Value = 350 },
           @{ Index = 1; Effect = 80; TargetA = 6; Value = 5 })
       Fields = (Own $flagFeralFrenzy @{ 131 = 750 }) },
    @{ Id = 96073; Clone = $rip; Name = 'Frénésie farouche'; IconPath = 'Interface\Icons\Ability_Druid_Rake'; FallbackIconSpell = 48574; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Saigne toutes les 2 s.'; AuraDescription = 'Saigne toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 1 })
       Fields = (Bleed $flagFeralFrenzy 32) },
    # Incarnation : Avatar d'Ashamane: 20 s, the cat's techniques (Berserk's masks, and the new ones) 25% cheaper,
    # physical damage 15% more
    @{ Id = 96069; Clone = 50334; Name = "Incarnation : Avatar d'Ashamane"; IconPath = 'Interface\Icons\Ability_Druid_CatForm'; FallbackIconSpell = 768; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask
       Description = "Vous incarnez Ashamane pendant 20 s : vos techniques de félin coûtent 25% d'énergie en moins et vos dégâts physiques augmentent de 15%. Forme de félin."
       AuraDescription = "Techniques de félin 25% moins chères ; dégâts physiques augmentés de 15%."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -25; Misc = $SPELLMOD_COST },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 1 },
           @{ Index = 2; Effect = 6; Aura = 77; TargetA = 1; Misc = 5 })
       Fields = (Own 0 @{ 12 = 0x1; 40 = 18; 122 = $berserkMasks[0]; 123 = $berserkMasks[1]
                         124 = ($berserkMasks[2] -bor $flagThrash -bor $flagPrimalWrath -bor $flagFeralFrenzy); 131 = 8634 }) },
    # Incarnation : Gardien d'Ursoc: 30 s, maximum health 30% more, damage 10% more, Mangle (Bear) and Thrash without a
    # cooldown
    @{ Id = 96070; Clone = $selfBuff; Name = "Incarnation : Gardien d'Ursoc"; IconPath = 'Interface\Icons\Ability_Racial_BearForm'; FallbackIconSpell = 9634; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $feral; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous incarnez Ursoc pendant 30 s : vos points de vie maximum augmentent de 30%, vos dégâts de 10%, et Mutilation et Rosser n'ont plus de temps de recharge. Forme d'ours."
       AuraDescription = "Points de vie maximum augmentés de 30%, dégâts de 10% ; Mutilation et Rosser sans temps de recharge."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModIncreaseHealthPercent; TargetA = 1; Value = 30 },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = -6000; Misc = $SPELLMOD_COOLDOWN },
           @{ Index = 2; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 127 })
       Fields = (SelfBuff 0 @{ 12 = 0x90; 40 = 9; 41 = 1; 125 = 0; 126 = $maskMangleBear; 127 = $flagThrash; 131 = 2758; 225 = 1 }) },

    # --- Restauration -------------------------------------------------------------------------------------------------
    # Efflorescence: aimed at a spot; mod-druid heals the 3 most injured allies within 10 yd every 2 s for 30 s (96086).
    # Its aura on the Druid shows how long the bloom lasts (and tells the bots it is down)
    @{ Id = 96080; Clone = $ground; Name = 'Efflorescence'; IconPath = 'Interface\Icons\INV_Misc_Herb_Talandrasrose'; FallbackIconSpell = 48438; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = "Fait fleurir le sol à l'endroit visé pendant 30 s : toutes les 2 s, les 3 alliés les plus blessés à 10 m sont soignés. Une seule Efflorescence à la fois."
       AuraDescription = 'Votre Efflorescence fleurit.'
       Effects = @(
           @{ Index = 0; Effect = 3; TargetA = 87 },
           @{ Index = 1; Effect = 6; Aura = $A_Dummy; TargetA = 1 })
       Fields = (Own 0 @{ 14 = 0x40000000; 28 = 1; 40 = 9; 46 = 5; 131 = 11568; 204 = 12; 205 = 133; 206 = 1500; 213 = 1; 225 = 8 }) },
    @{ Id = 96086; Clone = $healHit; Name = 'Efflorescence'; IconPath = 'Interface\Icons\INV_Misc_Herb_Talandrasrose'; FallbackIconSpell = 48438; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne la cible.'; Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 350 })
       Fields = (Own $flagEfflorescence @{ 46 = 6; 131 = 0; 205 = 0; 206 = 0; 225 = 8 }) },
    # Protection cénarienne (Cenarion Ward): 30 s on an ally; the first damage it takes, mod-druid puts the heal over
    # time (96087) on it
    @{ Id = 96081; Clone = $innervate; Name = 'Protection cénarienne'; IconPath = 'Interface\Icons\Ability_Druid_NaturalPerfection'; FallbackIconSpell = 48441; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = "Protège un allié pendant 30 s : dès qu'il subit des dégâts, il est soigné toutes les 2 s pendant 8 s."
       AuraDescription = 'Les prochains dégâts subis déclenchent des soins sur la durée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 21 })
       Fields = (Own 0 @{ 12 = 0x2; 14 = 0x40000000; 40 = 9; 131 = 3884; 204 = 9; 225 = 8 }) },
    @{ Id = 96087; Clone = $rejuvenation; Name = 'Protection cénarienne'; IconPath = 'Interface\Icons\Ability_Druid_NaturalPerfection'; FallbackIconSpell = 48441; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne toutes les 2 s.'; AuraDescription = 'Soigné toutes les 2 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 8; TargetA = 21; Value = 700 })
       Fields = (Own 0 @{ 40 = 31; 46 = 6; 98 = 2000; 204 = 0; 205 = 0; 206 = 0; 225 = 8 }) },
    # Écorce de fer (Ironbark): an ally takes 20% less damage for 12 s, off the global cooldown
    @{ Id = 96082; Clone = $innervate; Name = 'Écorce de fer'; IconPath = 'Interface\Icons\Spell_Nature_StoneClawTotem'; FallbackIconSpell = 22812; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask
       Description = "La peau d'un allié se fait écorce de fer : les dégâts qu'il subit sont réduits de 20% pendant 12 s."
       AuraDescription = 'Dégâts subis réduits de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 21; Value = -20; Misc = 127 })
       Fields = (Own 0 @{ 12 = 0x2; 14 = 0x40000000; 40 = 29; 131 = 6662; 204 = 0; 205 = 0; 206 = 0; 225 = 8 }) },
    # Épanouissement (Flourish): mod-druid lengthens the Druid's heals over time on the allies within 60 yd by 8 s
    @{ Id = 96083; Clone = $selfBuff; Name = 'Épanouissement'; IconPath = 'Interface\Icons\Ability_Druid_Flourish'; FallbackIconSpell = 48438; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = 'Prolonge de 8 s vos soins sur la durée sur les alliés à 60 m.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (SelfBuff 0 @{ 4 = 0x10000; 40 = 0; 131 = 11568; 205 = 133; 206 = 1500; 225 = 8 }) },
    # Germination: a second Rejuvenation (its family flag kept: the Rejuvenation talents reach it), put by mod-druid
    @{ Id = 96084; Clone = $rejuvenation; Name = 'Germination'; IconPath = 'Interface\Icons\Spell_Nature_Rejuvenation'; FallbackIconSpell = 48441; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Soigne toutes les 3 s.'; AuraDescription = 'Soigné toutes les 3 s.'
       Fields = (Own $flagGermination @{ 46 = 6; 204 = 0; 205 = 0; 206 = 0; 209 = $maskRejuvenation }) },
    # Incarnation : Arbre de vie: 30 s, healing 15% more, Rejuvenation 30% cheaper, Regrowth instant
    @{ Id = 96085; Clone = $selfBuff; Name = 'Incarnation : Arbre de vie'; IconPath = 'Interface\Icons\Ability_Druid_TreeofLife'; FallbackIconSpell = 33891; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $restoration; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous incarnez l'Arbre de vie pendant 30 s : vos soins augmentent de 15%, Récupération coûte 30% de mana en moins et Rétablissement est instantané."
       AuraDescription = 'Soins augmentés de 15% ; Récupération moins chère, Rétablissement instantané.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 136; TargetA = 1; Value = 15 },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -30; Misc = $SPELLMOD_COST },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = 10 })
       Fields = (SelfBuff 0 @{ 4 = 0x10000; 40 = 9; 125 = $maskRejuvenation; 126 = 0; 127 = $flagGermination
                              128 = $maskRegrowth; 129 = 0; 130 = 0; 131 = 8598; 225 = 8 }) },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-druid also reads them to know which one is
    # on. The modifiers are the combat bench's tuning (README.md).
    # Équilibre: Wrath and Starfire (word 0) 45% less, Starsurge (word 2) 35% less (Astral Power, Eclipse, the splash and
    # the procs carry the specialization; tuned on the combat bench)
    @{ Id = 96180; Clone = 2983; Name = 'Équilibre'; IconPath = 'Interface\Icons\Spell_Nature_StarFall'; FallbackIconSpell = 48465; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Colère et Feu stellaire vous font entrer dans les Éclipses et vous rendent de la puissance astrale, comme Éclat lunaire, Éclat solaire et Embrasement stellaire ; Éruption stellaire et Météores la consomment. Deux Colères mènent à l'Éclipse lunaire, deux Feux stellaires à l'Éclipse solaire."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -45; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -35; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = ($maskWrath -bor $maskStarfire); 123 = 0; 124 = 0; 125 = 0; 126 = 0; 127 = $flagStarsurge; 208 = 7 } },
    # Combat farouche: the cat's and the bear's (mod-druid: the bear takes 50% less; tuned on the combat bench)
    @{ Id = 96181; Clone = 2983; Name = 'Combat farouche'; IconPath = 'Interface\Icons\Ability_Racial_BearForm'; FallbackIconSpell = 768; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "En forme de félin, Griffure, Déchirure et Rosser font saigner vos proies, que Morsure féroce et Courroux primordial achèvent ; en forme d'ours, vous subissez 50% de dégâts en moins, et Fourrure de fer, Mutilation et Rosser vous font tenir la ligne."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 0; Misc = 1 })
       Fields = @{ 208 = 7 } },
    # Restauration: healing done 10% more (aura 136)
    @{ Id = 96182; Clone = 2983; Name = 'Restauration'; IconPath = 'Interface\Icons\Spell_Nature_HealingTouch'; FallbackIconSpell = 48441; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Vos soins augmentent de 10%. Récupération, Fleur de vie et Croissance sauvage entretiennent le groupe, Efflorescence fleurit sous ses pieds."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 136; TargetA = 1; Value = 10 })
       Fields = @{ 208 = 7 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-druid reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\druid\talentTree.json') -Family 7

return $spells
