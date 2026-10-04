# The Warrior's spell data for its retail-style talent trees (localTools/warrior/talentTree.json): the abilities its
# nodes and specializations teach, the auras its scripted talents show, and the rank spells of its new talents.
# Behaviour lives in modules/mod-warrior; this file owns client/server spell data only. The stock Warrior spells it
# changes in place (no stance requirements, Overpower and Revenge as retail plays them, Slam instant...) are in
# localTools/warrior/StockSpells.ps1.
# Ids 95000-95299: 95000-95099 talent ranks (generated from the tree), 95100-95279 abilities and auras, 95280-95289
# the specializations' passives.
#
# Clone field notes: 1 category, 12 stances, 16 target flags (0x40 a ground target), 21 target aura state, 28 casting
# time index (1 instant), 29 cooldown, 30 category cooldown, 40 duration index (35 4 s, 32 6 s, 31 8 s, 1 10 s, 29
# 12 s, 8 15 s, 18 20 s, 21 never), 41 power type (1 rage), 42 cost (rage in tenths: 350 = 35 rage), 46 range index (1
# self, 2 melee, 4 30 yd, 94 8-40 yd, 95 8-25 yd), 68-70 equipped item (4 / 64 a shield), 72-73 effects 1-2, 83-85
# effect mechanic (7 root, 12 stun, 15 bleed), 86-88 target A, 89-91 target B (15 enemies around the source), 92-94
# radius index (13 10 yd, 14 8 yd, 32 12 yd, 10 30 yd), 98-100 periodic interval, 116-118 triggered spell, 122-130 the
# effects' class masks, 131 visual, 204 cost percentage, 205-206 global cooldown category and time (0 0: off the global
# cooldown), 208 family (4 Warrior), 209-211 family flags, 212 maximum targets (0 every one), 213 damage class (2
# melee), 225 school (1 physical, 4 fire).
#
# Targets: 1 the caster, 6 the enemy target, 16 the enemies around the chosen spot, 22 around the caster (with target B
# 15), 56 the raid around the caster, 87 the chosen spot. Weapon strikes keep Mortal Strike's layout (121 normalized
# weapon damage plus the flat bonus, 31 the weapon percentage applied to both); off-hand strikes Whirlwind's off-hand
# hit (44949, which strikes with the off-hand weapon).
#
# The Warrior keeps rage. Damage the module works out (Meat Cleaver's relays, the Ravager's blades) goes through spells
# with explicit zero coefficients in modules/mod-warrior's SQL; the area spells carry their attack power coefficients
# there.

$classMask = 1
$arms = 26
$fury = 256
$protection = 257
# The Gladiateur's own spellbook tab (localTools/customClasses/classes.json, the Warrior's specSkills)
$gladiator = 910

# The new abilities' own family flags, word 2 (free among the Warrior's spells; the core reads none of them). Words 0
# and 1 stay clear on every clone: the core reads Devastate, Shield Slam, Victory Rush and the shouts from them.
$flagLeap = 0x1
$flagRally = 0x2
$flagStormBolt = 0x4
$flagAvatar = 0x8
$flagRoar = 0x10
$flagSpear = 0x20
$flagColossus = 0x40
$flagSkullsplitter = 0x80
$flagRavager = 0x100
$flagDieByTheSword = 0x200
$flagRagingBlow = 0x400
$flagRampage = 0x800
$flagOnslaught = 0x1000
$flagOdyn = 0x2000
$flagIgnorePain = 0x4000
$flagShieldCharge = 0x8000
$flagDisrupt = 0x10000
$flagShieldThrow = 0x20000      # Gladiateur: Lancer de bouclier (Ricochet's modifiers)
$flagArenaStorm = 0x40000
$flagDuel = 0x80000
$flagCoupDeGrace = 0x100000
$flagWound = 0x200000           # Plaie du gladiateur's bleed (Arène sanglante's modifier)
$flagWoundBurst = 0x400000

# The stock spells the modifiers name: word 0 Overpower 0x4, Revenge 0x400, Thunder Clap 0x80, Slam 0x200000, Cleave
# 0x400000, Mortal Strike 0x2000000, Execute 0x20000000; word 1 Whirlwind 0x4, Shield Slam 0x200, Bloodthirst 0x400
$maskOverpower = 0x4
$maskRevenge = 0x400
$maskThunderClap = 0x80
$maskSlam = 0x200000
$maskCleave = 0x400000
$maskMortalStrike = 0x2000000
$maskExecute = 0x20000000
$maskWhirlwind = 0x4
$maskShieldSlam = 0x200
$maskBloodthirst = 0x400

# Mortal Strike's layout for a weapon strike; Whirlwind's off-hand hit for an off-hand one; Execute's damage (20647)
# for damage the module or a coefficient sets; Whirlwind for weapon damage around the Warrior; Thunder Clap for damage
# around the Warrior; Sprint's for an instant self buff
$strike = 12294
$offhandStrike = 44949
$computed = 20647
$whirl = 1680
$clap = 6343
$selfBuff = 2983

# The Gladiateur's looks imported from the Ascension client (localTools/warrior/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py): field 131 names an imported SpellVisual by its key; LookKit gives one of
# its kits (by SpellVisual field: 2 cast, 3 impact, 4 state) for a stock visual's slot, a look mixed from both.
$imported = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-warrior\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function Look([string]$key) {
    $id = $imported.ids.$key
    if (-not $id) { throw "No imported Warrior look '$key' (localTools\warrior\ascensionVisuals.json)." }
    return [uint32]$id
}
function LookKit([string]$key, [int]$field) {
    $id = Look $key
    $visual = @($imported.tables.SpellVisual | Where-Object { [uint32]$_.id -eq $id })[0]
    $kit = [uint32]$visual.fields[$field]
    if (-not $kit -or $kit -eq [uint32]::MaxValue) { throw "The imported Warrior look '$key' has no kit in field $field." }
    return $kit
}

# A weapon strike's effects: flat bonus, then the weapon percentage
function Strike($flat, $percent, $target = 6) {
    return @(
        @{ Index = 0; Effect = 121; TargetA = $target; Value = $flat },
        @{ Index = 1; Effect = 31; TargetA = $target; Value = $percent })
}

# Every clone of a Warrior spell: its own category and flags, no stance
function Own($flag, $extra = @{}) {
    $fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 208 = 4; 209 = 0; 210 = 0; 211 = $flag }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$spells = @(
    # --- Class tree -------------------------------------------------------------------------------------------------
    # Bond héroïque (Heroic Leap): the 3.3.5 data's own Heroic Leap (6544), its jump aimed at the chosen spot (87); its
    # stun and second effect gone. mod-warrior strikes the landing (95101) as the Warrior comes down.
    @{ Id = 95100; Clone = 6544; Name = 'Bond héroïque'; IconPath = 'Interface\Icons\Ability_HeroicLeap'; FallbackIconSpell = 60970; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask
       Description = "Bondit jusqu'à l'endroit visé, entre 8 et 40 m, et frappe les ennemis à 8 m de l'atterrissage."
       Fields = (Own $flagLeap @{ 40 = 0; 46 = 94; 72 = 0; 73 = 0; 86 = 87; 87 = 0; 88 = 0; 96 = 0; 97 = 0; 117 = 0; 118 = 0 }) },
    @{ Id = 95101; Clone = 52174; Name = 'Bond héroïque'; IconPath = 'Interface\Icons\Ability_HeroicLeap'; FallbackIconSpell = 60970; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de l'arme."; Effects = (Strike 1 100 16)
       Fields = (Own $flagLeap @{ 40 = 0; 47 = 0; 92 = 14; 93 = 14; 212 = 0 }) },
    # Cri de ralliement (Rallying Cry): Commanding Shout's raid area, 15% maximum health for 10 s
    @{ Id = 95102; Clone = 47440; Name = 'Cri de ralliement'; IconPath = 'Interface\Icons\Ability_Warrior_RallyingCry'; FallbackIconSpell = 469; Cost = 0; Cooldown = 180000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = 'Galvanise votre groupe : les membres à 30 m gagnent 15% de points de vie maximum pendant 10 s.'
       AuraDescription = 'Points de vie maximum augmentés de 15%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseHealthPercent; TargetA = 56; Value = 15 })
       Fields = (Own $flagRally @{ 40 = 1; 92 = 10; 205 = 0; 206 = 0 }) },
    # Éclair de tempête (Storm Bolt): Heroic Throw's missile, physical damage and a 4 s stun
    @{ Id = 95103; Clone = 57755; Name = 'Éclair de tempête'; IconPath = 'Interface\Icons\INV_Hammer_02'; FallbackIconSpell = 57755; Cost = 0; Cooldown = 30000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = "Lance une arme sur l'ennemi à 30 m : dégâts physiques et étourdissement pendant 4 s."
       AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 6; Value = 300 },
           @{ Index = 1; Effect = 6; Aura = 12; TargetA = 6 })
       Fields = (Own $flagStormBolt @{ 40 = 35; 84 = 12 }) },
    # Avatar: 20% more damage and a larger build for 20 s, off the global cooldown
    @{ Id = 95104; Clone = $selfBuff; Name = 'Avatar'; IconPath = 'Interface\Icons\Ability_Racial_Avatar'; FallbackIconSpell = 29759; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous prenez la forme d'un colosse pendant 20 s : vos dégâts augmentent de 20%."
       AuraDescription = 'Dégâts augmentés de 20%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 20; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = $A_ModScale; TargetA = 1; Value = 20 })
       Fields = (Own $flagAvatar @{ 40 = 18; 41 = 1; 131 = 4599; 205 = 0; 206 = 0; 225 = 1 }) },
    # Rugissement de tonnerre (Thunderous Roar): physical damage within 12 yd, then a bleed every 2 s for 8 s
    @{ Id = 95105; Clone = $clap; Name = 'Rugissement de tonnerre'; IconPath = 'Interface\Icons\Ability_Warrior_WarCry'; FallbackIconSpell = 1160; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = 'Un rugissement assourdissant : dégâts physiques aux ennemis à 12 m, qui saignent ensuite pendant 8 s.'
       AuraDescription = 'Saigne toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 100 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 22; Value = 50 })
       Fields = (Own $flagRoar @{ 40 = 31; 84 = 15; 89 = 15; 90 = 15; 92 = 32; 93 = 32; 99 = 2000; 131 = 210; 213 = 2; 225 = 1 }) },
    # Lance du champion (Champion's Spear): Shadowfury's ground target, physical damage and a 4 s root within 8 yd,
    # 10 rage (mod-warrior)
    @{ Id = 95106; Clone = 47847; Name = 'Lance du champion'; IconPath = 'Interface\Icons\INV_Spear_06'; FallbackIconSpell = 57755; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = "Projette une lance à l'endroit visé, à 30 m : dégâts physiques et immobilisation de 4 s aux ennemis à 8 m, et 10 points de rage."
       AuraDescription = 'Immobilisé.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 16; Value = 200 },
           @{ Index = 1; Effect = 6; Aura = 26; TargetA = 16 })
       Fields = (Own $flagSpear @{ 31 = 0; 40 = 35; 41 = 1; 84 = 7; 92 = 14; 93 = 14; 131 = 13222; 204 = 0; 205 = 133; 206 = 1500; 213 = 2; 225 = 1 }) },

    # --- Armes ------------------------------------------------------------------------------------------------------
    # Frappe du colosse (Colossus Smash): 175% weapon damage; mod-warrior marks the target (95115)
    @{ Id = 95110; Clone = $strike; Name = 'Frappe du colosse'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask
       Description = "Écrase la cible (175% des dégâts de l'arme) : elle subit 20% de dégâts en plus de votre part pendant 10 s."
       Effects = (Strike 1 175)
       Fields = (Own $flagColossus @{ 131 = 12295 }) },
    # Briseguerre (Warbreaker): Whirlwind's area, 150% weapon damage within 8 yd; mod-warrior marks every enemy hit
    @{ Id = 95111; Clone = $whirl; Name = 'Briseguerre'; IconPath = 'Interface\Icons\Ability_Warrior_Sunder'; FallbackIconSpell = 7386; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask
       Description = "Frappe le sol : 150% des dégâts de l'arme aux ennemis à 8 m, qui subissent 20% de dégâts en plus de votre part pendant 10 s."
       Effects = (Strike 1 150 22)
       Fields = (Own $flagColossus @{ 89 = 15; 90 = 15; 92 = 14; 93 = 14; 131 = 145; 212 = 0 }) },
    # Fend-crâne (Skullsplitter): 200% weapon damage and 20 rage (mod-warrior)
    @{ Id = 95112; Clone = $strike; Name = 'Fend-crâne'; IconPath = 'Interface\Icons\INV_Misc_Bone_HumanSkull_01'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 21000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask
       Description = "Fend le crâne de la cible (200% des dégâts de l'arme) et vous rend 20 points de rage."
       Effects = (Strike 1 200)
       Fields = (Own $flagSkullsplitter @{ 131 = 1165 }) },
    # Ravageur (Ravager, Arms): a dummy on the chosen spot; mod-warrior whirls the blades there (95124) every second
    # for 7 s, 5 rage a blow
    @{ Id = 95113; Clone = 47847; Name = 'Ravageur'; IconPath = 'Interface\Icons\INV_Axe_68'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask
       Description = "Lance un ravageur tournoyant à l'endroit visé : chaque seconde pendant 7 s, il frappe les ennemis à 8 m et vous rend 5 points de rage."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagRavager @{ 31 = 0; 40 = 0; 41 = 1; 131 = 13222; 204 = 0; 205 = 133; 206 = 1500; 213 = 2; 225 = 1 }) },
    # Par le fil de l'épée (Die by the Sword): 100% parry and 30% less damage for 8 s, off the global cooldown
    @{ Id = 95114; Clone = $selfBuff; Name = "Par le fil de l'épée"; IconPath = 'Interface\Icons\Ability_Parry'; FallbackIconSpell = 20230; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $arms; ClassMask = $classMask; NoEquipment = $true
       Description = 'Pendant 8 s, vos chances de parer augmentent de 100% et vous subissez 30% de dégâts en moins.'
       AuraDescription = 'Parade augmentée de 100%, dégâts subis réduits de 30%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModParryPercent; TargetA = 1; Value = 100 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -30; Misc = 127 })
       Fields = (Own $flagDieByTheSword @{ 40 = 31; 41 = 1; 131 = 7395; 205 = 0; 206 = 0; 225 = 1 }) },
    # The mark of Frappe du colosse and Briseguerre (mod-warrior reads it): 20% more damage from the Warrior, 10 s
    @{ Id = 95115; Clone = 2983; Name = 'Frappe du colosse'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Exposé.'; AuraDescription = 'Subit 20% de dégâts en plus du guerrier.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 1 } },
    # Prouesse martiale (Martial Prowess): 15% more damage on the next Mortal Strike a stack, 2 stacks
    @{ Id = 95116; Clone = 2983; Name = 'Prouesse martiale'; IconPath = 'Interface\Icons\Ability_MeleeDamage'; FallbackIconSpell = 7384; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Frappe mortelle renforcée.'; AuraDescription = 'Votre prochaine Frappe mortelle inflige 15% de dégâts en plus par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 15; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 40 = 8; 122 = $maskMortalStrike; 123 = 0; 124 = 0; 208 = 4 } },
    # Précision de l'exécuteur (Executioner's Precision): on the target, 25% more from the next Mortal Strike a stack
    @{ Id = 95117; Clone = 2983; Name = "Précision de l'exécuteur"; IconPath = 'Interface\Icons\INV_Sword_62'; FallbackIconSpell = 5308; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Vulnérable.'; AuraDescription = 'Subit 25% de dégâts en plus de la prochaine Frappe mortelle du guerrier, par charge.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 6 }); Fields = @{ 40 = 9 } },
    # Pour la mise à mort (In for the Kill): melee haste for 10 s, its amount set by mod-warrior (10% or 25%)
    @{ Id = 95118; Clone = 2983; Name = 'Pour la mise à mort'; IconPath = 'Interface\Icons\Ability_Warrior_PunishingBlow'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Hâte augmentée.'; AuraDescription = 'Hâte en mêlée augmentée.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 10 }); Fields = @{ 40 = 1 } },
    # Broyeur impitoyable (Merciless Bonegrinder): Whirlwind (word 1 0x4, Bladestorm's whirls too) and Cleave 50%
    @{ Id = 95119; Clone = 2983; Name = 'Broyeur impitoyable'; IconPath = 'Interface\Icons\Ability_Warrior_Innerrage'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Tourbillon et Enchaînement renforcés.'; AuraDescription = 'Tourbillon et Enchaînement infligent 50% de dégâts en plus.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 50; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 40 = 8; 122 = $maskCleave; 123 = $maskWhirlwind; 124 = 0; 208 = 4 } },
    # The Ravager's blades, both specializations' (amount from the attack power coefficient in the SQL)
    @{ Id = 95124; Clone = $computed; Name = 'Ravageur'; IconPath = 'Interface\Icons\INV_Axe_68'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 50 })
       Fields = (Own $flagRavager @{ 21 = 0; 46 = 13; 131 = 11756 }) },
    # Fulgurance's charges (Arms), shown as stacks
    @{ Id = 95148; Clone = 2983; Name = 'Charges de Fulgurance'; IconPath = 'Interface\Icons\Ability_MeleeDamage'; FallbackIconSpell = 7384; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 2; Spellbook = $false
       Description = 'Charges de Fulgurance.'; AuraDescription = 'Charges de Fulgurance disponibles.'; Fields = @{ 40 = 21 } },

    # --- Fureur -----------------------------------------------------------------------------------------------------
    # Saccage (Rampage): 80 rage, the first of its 4 blows (60% weapon damage); mod-warrior strikes the 3 others
    # (95126 main hand, 95127 off hand) and Enrages
    @{ Id = 95120; Clone = $strike; Name = 'Saccage'; IconPath = 'Interface\Icons\Ability_Warrior_Rampage'; FallbackIconSpell = 23881; Cost = 800; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = "Frappe la cible 4 fois avec vos deux armes (60% des dégâts de l'arme chacune) et vous rend Enragé."
       Effects = (Strike 1 60)
       Fields = (Own $flagRampage @{ 131 = 372 }) },
    # Coup déchaîné (Raging Blow): 130% weapon damage with each weapon (95125 the off hand), 12 rage; 2 charges on 8 s
    # (mod-warrior)
    @{ Id = 95121; Clone = $strike; Name = 'Coup déchaîné'; IconPath = 'Interface\Icons\Ability_Warrior_Innerrage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 8000; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = "Frappe la cible avec vos deux armes (130% des dégâts de chacune) et vous rend 12 points de rage. 2 charges."
       Effects = (Strike 1 130)
       Fields = (Own $flagRagingBlow @{ 131 = 372 }) },
    # Assaut (Onslaught): 200% weapon damage and 15 rage
    @{ Id = 95122; Clone = $strike; Name = 'Assaut'; IconPath = 'Interface\Icons\Ability_Warrior_UnrelentingAssault'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 18000; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = "Un assaut brutal sur la cible (200% des dégâts de l'arme) qui vous rend 15 points de rage."
       Effects = (Strike 1 200)
       Fields = (Own $flagOnslaught @{ 131 = 39 }) },
    # Fureur d'Odyn (Odyn's Fury): fire damage within 12 yd and a 4 s burn; mod-warrior Enrages
    @{ Id = 95123; Clone = $clap; Name = "Fureur d'Odyn"; IconPath = 'Interface\Icons\Spell_Fire_SealOfFire'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $fury; ClassMask = $classMask
       Description = "Déchaîne la fureur d'Odyn : dégâts de Feu aux ennemis à 12 m, qui brûlent ensuite pendant 4 s, et vous rend Enragé."
       AuraDescription = 'Brûle toutes les 2 s.'
       Effects = @(
           @{ Index = 0; Effect = 2; TargetA = 22; Value = 100 },
           @{ Index = 1; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 22; Value = 50 })
       Fields = (Own $flagOdyn @{ 40 = 35; 89 = 15; 90 = 15; 92 = 32; 93 = 32; 99 = 2000; 131 = 5600; 213 = 2; 225 = 4 }) },
    @{ Id = 95125; Clone = $offhandStrike; Name = 'Coup déchaîné'; IconPath = 'Interface\Icons\Ability_Warrior_Innerrage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de l'arme en main gauche."; Effects = (Strike 1 130)
       Fields = (Own $flagRagingBlow @{ 89 = 0; 92 = 0; 131 = 372; 212 = 0 }) },
    @{ Id = 95126; Clone = $strike; Name = 'Saccage'; IconPath = 'Interface\Icons\Ability_Warrior_Rampage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de l'arme."; Effects = (Strike 1 60)
       Fields = (Own $flagRampage @{ 131 = 372; 205 = 0; 206 = 0 }) },
    @{ Id = 95127; Clone = $offhandStrike; Name = 'Saccage'; IconPath = 'Interface\Icons\Ability_Warrior_Rampage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de l'arme en main gauche."; Effects = (Strike 1 60)
       Fields = (Own $flagRampage @{ 89 = 0; 92 = 0; 131 = 372; 212 = 0 }) },
    # Enragé (Enrage): 25% melee haste for 4 s
    @{ Id = 95128; Clone = 2983; Name = 'Enragé'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 12292; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Enragé.'; AuraDescription = "Vitesse d'attaque augmentée de 25%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 25 })
       Fields = @{ 40 = 35 } },
    # Fendoir à viande (Meat Cleaver): the next single-target attacks also strike nearby enemies (mod-warrior, 95130)
    @{ Id = 95129; Clone = 2983; Name = 'Fendoir à viande'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 845; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 4; Spellbook = $false
       Description = 'Fendoir à viande.'; AuraDescription = "Votre prochaine technique à cible unique frappe aussi jusqu'à 4 ennemis proches pour 50% de ses dégâts."
       Fields = @{ 40 = 18 } },
    # The relay carries an amount already final: no damage class (no second roll, no second critical strike) and no
    # caster modifiers (attributes ex 3 0x20000000)
    @{ Id = 95130; Clone = $computed; Name = 'Fendoir à viande'; IconPath = 'Interface\Icons\Ability_Warrior_Cleave'; FallbackIconSpell = 845; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Own 0 @{ 7 = 0x60000200; 21 = 0; 46 = 13; 131 = 219; 213 = 0 }) },
    # Abandon téméraire (Reckless Abandon): Bloodthirst (word 1 0x400) and Raging Blow 50% while Recklessness lasts
    @{ Id = 95131; Clone = 2983; Name = 'Abandon téméraire'; IconPath = 'Interface\Icons\Ability_CriticalStrike'; FallbackIconSpell = 1719; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Sanguinaire et Coup déchaîné renforcés.'; AuraDescription = 'Sanguinaire et Coup déchaîné infligent 50% de dégâts en plus.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 50; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 40 = 29; 122 = 0; 123 = $maskBloodthirst; 124 = $flagRagingBlow; 208 = 4 } },
    # Lames dansantes (Dancing Blades): 30% melee haste for 10 s
    @{ Id = 95132; Clone = 2983; Name = 'Lames dansantes'; IconPath = 'Interface\Icons\Ability_Warrior_Bladestorm'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Vitesse d'attaque augmentée."; AuraDescription = "Vitesse d'attaque augmentée de 30%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 30 }); Fields = @{ 40 = 1 } },
    # Rage protectrice: less damage taken while Enraged, its amount set by mod-warrior (5% or 10%)
    @{ Id = 95133; Clone = 2983; Name = 'Rage protectrice'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 12292; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts subis réduits.'; AuraDescription = 'Dégâts subis réduits.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -5; Misc = 127 }); Fields = @{ 40 = 35 } },
    # Coup déchaîné's charges, shown as stacks
    @{ Id = 95134; Clone = 2983; Name = 'Charges de Coup déchaîné'; IconPath = 'Interface\Icons\Ability_Warrior_Innerrage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Coup déchaîné.'; AuraDescription = 'Charges de Coup déchaîné disponibles.'; Fields = @{ 40 = 21 } },

    # --- Protection -------------------------------------------------------------------------------------------------
    # Charge de bouclier (Shield Charge): Intercept's charge with a shield, its stun gone, up to 25 yd (no minimum); mod-warrior strikes the
    # target and the enemies within 8 yd of it (95145) and gives 20 rage
    @{ Id = 95140; Clone = 20252; Name = 'Charge de bouclier'; IconPath = 'Interface\Icons\INV_Shield_04'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 45000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = "Charge un ennemi à 25 m au plus, bouclier en avant : 150% des dégâts de l'arme à la cible et aux ennemis à 8 m, et 20 points de rage."
       Fields = (Own $flagShieldCharge @{ 46 = 34; 68 = 4; 69 = 64; 70 = 0; 72 = 0; 87 = 0; 117 = 0 }) },
    # Ravageur (Ravager, Protection): as the Arms one
    @{ Id = 95141; Clone = 47847; Name = 'Ravageur'; IconPath = 'Interface\Icons\INV_Axe_68'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = "Lance un ravageur tournoyant à l'endroit visé : chaque seconde pendant 7 s, il frappe les ennemis à 8 m et vous rend 5 points de rage."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87 })
       Fields = (Own $flagRavager @{ 31 = 0; 40 = 0; 41 = 1; 131 = 13222; 204 = 0; 205 = 133; 206 = 1500; 213 = 2; 225 = 1 }) },
    # Cri perturbateur (Disrupting Shout): Pummel's interrupt on every enemy within 10 yd
    @{ Id = 95142; Clone = 6552; Name = 'Cri perturbateur'; IconPath = 'Interface\Icons\Ability_Warrior_Challange'; FallbackIconSpell = 1161; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask
       Description = "Un cri qui interrompt les sorts des ennemis à 10 m et les empêche d'en lancer pendant 4 s."
       Effects = @(@{ Index = 0; Effect = 68; TargetA = 22 })
       Fields = (Own $flagDisrupt @{ 46 = 1; 83 = 26; 89 = 15; 92 = 13; 131 = 209 }) },
    # Ignorer la douleur (Ignore Pain): 35 rage, off the global cooldown; mod-warrior puts up (or tops up) the absorb
    # (95144), capped at 30% of maximum health
    @{ Id = 95143; Clone = $selfBuff; Name = 'Ignorer la douleur'; IconPath = 'Interface\Icons\Ability_Warrior_ShieldGuard'; FallbackIconSpell = 2565; Cost = 350; Cooldown = 1000; Level = 1; Spellbook = $true; SkillLine = $protection; ClassMask = $classMask; NoEquipment = $true
       Description = "Vous ignorez la douleur : pendant 12 s, la moitié de chaque coup reçu est absorbée, jusqu'à un total qui dépend de votre puissance d'attaque et qui se cumule jusqu'à 30% de vos points de vie maximum. Ne déclenche pas le temps de recharge global."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1 })
       Fields = (Own $flagIgnorePain @{ 40 = 0; 41 = 1; 131 = 0; 205 = 0; 206 = 0; 225 = 1 }) },
    @{ Id = 95144; Clone = 2983; Name = 'Ignorer la douleur'; IconPath = 'Interface\Icons\Ability_Warrior_ShieldGuard'; FallbackIconSpell = 2565; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Absorbe des dégâts.'; AuraDescription = 'Absorbe la moitié de chaque coup reçu.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_SchoolAbsorb; TargetA = 1; Value = 1; Misc = 127 }); Fields = @{ 40 = 29 } },
    @{ Id = 95145; Clone = $strike; Name = 'Charge de bouclier'; IconPath = 'Interface\Icons\INV_Shield_04'; FallbackIconSpell = 20252; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Dégâts de l'arme."; Effects = (Strike 1 150)
       Fields = (Own $flagShieldCharge @{ 46 = 13; 68 = -1; 69 = 0; 70 = 0; 131 = 42; 205 = 0; 206 = 0 }) },
    # Revanche ! (a free Revenge: word 0 0x400) after a dodge, a parry or a block
    @{ Id = 95146; Clone = 2983; Name = 'Revanche !'; IconPath = 'Interface\Icons\Ability_Warrior_Revenge'; FallbackIconSpell = 6572; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vengeance gratuite.'; AuraDescription = 'Votre prochaine Vengeance ne coûte pas de rage.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 40 = 1; 122 = $maskRevenge; 123 = 0; 124 = 0; 208 = 4 } },
    # Maîtrise du blocage's charges, shown as stacks
    @{ Id = 95147; Clone = 2983; Name = 'Charges de Maîtrise du blocage'; IconPath = 'Interface\Icons\Ability_Defend'; FallbackIconSpell = 2565; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Charges de Maîtrise du blocage.'; AuraDescription = 'Charges de Maîtrise du blocage disponibles.'; Fields = @{ 40 = 21 } },

    # --- Gladiateur --------------------------------------------------------------------------------------------------
    # A damage dealer with a one-handed weapon and a shield (mod-warrior): Shield Slam opens Revenge (Ouverture),
    # Revenge leaves Plaie du gladiateur, the bleed's ticks can bring Shield Slam back, and Execute spends all rage and
    # the bleed. Ids 95150-95179; its talent ranks 95200-95259.
    # Duel: Intervene's charge to an ally, its interception gone; mod-warrior puts Duel (95155) on both and the share
    # (95163) on the ally
    @{ Id = 95150; Clone = 3411; Name = 'Duel'; Icon = 'Gladiator_Duel'; FallbackIconSpell = 3411; Cost = 0; Cooldown = 120000; Level = 1; Spellbook = $true; SkillLine = $gladiator; ClassMask = $classMask
       Description = "Vous chargez un allié à 25 m et descendez tous deux dans l'arène pendant 15 s : votre puissance d'attaque et la sienne augmentent de 15%, ainsi que vos dégâts et soins des sorts, et vous subissez 30% des dégâts qu'il reçoit tant que vous êtes au-dessus de 35% de vos points de vie."
       Fields = (Own $flagDuel @{ 46 = 34; 72 = 0; 81 = 0; 87 = 0; 96 = 0; 205 = 0; 206 = 0 })
       # Intervene's charge; on arrival the arena horn and its shout (stock kit 11611, Scourge_Horn), the crowd's
       # cheer on the ally (stock kit 12798, CrowdCheerAlliance)
       Visual = @{ Clone = 9107; CasterImpact = 11611; Impact = 12798 } },
    # Lancer de bouclier: Avenger's Shield's bounce (4 enemies, more with Ricochet), physical, from the Warrior's
    # attack power (warrior_spells.sql); mod-warrior leaves Sunder Armor and the bleed on each enemy it hits
    @{ Id = 95151; Clone = 48827; Name = 'Lancer de bouclier'; Icon = 'Gladiator_ShieldThrow'; FallbackIconSpell = 48827; Cost = 0; Cooldown = 15000; Level = 1; Spellbook = $true; SkillLine = $gladiator; ClassMask = $classMask
       Description = "Lance votre bouclier sur l'ennemi à 30 m : il rebondit sur jusqu'à 3 autres ennemis proches et laisse Fracasser armure et Plaie du gladiateur sur chacun."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Own $flagShieldThrow @{ 41 = 1; 72 = 0; 84 = 0; 96 = 0; 104 = 4; 105 = 0; 204 = 0; 213 = 2; 225 = 1 })
       # The stock Throw Shield's missile (the thrower's own shield, spun as a boomerang), cast with Shield Toss's
       # shield bash and clang, striking with its sparks and metal shield clang
       Visual = @{ Clone = 14714; Precast = 0; Cast = (LookKit 'ShieldThrow' 2); Impact = (LookKit 'ShieldThrow' 3)
                   State = 0 } },
    # Tempête de l'arène: Bladestorm's spin, its whirls (95157) refresh the bleed of what they hit (mod-warrior)
    @{ Id = 95152; Clone = 46924; Name = "Tempête de l'arène"; Icon = 'Gladiator_ArenaStorm'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $gladiator; ClassMask = $classMask
       Description = "Vous tournoyez bouclier en avant pendant 6 s : chaque seconde, vous frappez les ennemis proches et prolongez leur Plaie du gladiateur."
       Fields = (Own $flagArenaStorm @{ 116 = 95157; 131 = (Look 'ArenaStorm') }) },
    # Coup de grâce: the next Execute within 10 s costs nothing and strikes as with 100 rage (mod-warrior)
    @{ Id = 95153; Clone = $selfBuff; Name = 'Coup de grâce'; Icon = 'Gladiator_CoupDeGrace'; FallbackIconSpell = 5308; Cost = 0; Cooldown = 90000; Level = 1; Spellbook = $true; SkillLine = $gladiator; ClassMask = $classMask; NoEquipment = $true
       Description = "Votre prochaine Exécution dans les 10 s ne coûte pas de rage et frappe comme si elle en dépensait 100."
       AuraDescription = 'Votre prochaine Exécution frappe comme avec 100 points de rage, sans coût.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = (Own $flagCoupDeGrace @{ 40 = 1; 41 = 1; 122 = $maskExecute; 123 = 0; 124 = 0; 131 = (Look 'CoupDeGrace'); 205 = 0; 206 = 0; 225 = 1 }) },
    # Clameur de la foule: haste for 10 s after Execute, its amount set by mod-warrior
    @{ Id = 95154; Clone = 2983; Name = 'Clameur de la foule'; Icon = 'Gladiator_Crowd'; FallbackIconSpell = 1719; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Hâte augmentée.'; AuraDescription = 'Hâte en mêlée augmentée.'
       # The crowd's cheer as it goes up (stock Bested... visual 13758: CrowdCheerAlliance)
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 3 })
       Fields = @{ 40 = 1; 131 = 13758 } },
    # Duel, on the Warrior and the ally: 15% attack power, 15% spell damage and healing, 15 s (longer with
    # Amphithéâtre, mod-warrior)
    @{ Id = 95155; Clone = 2983; Name = 'Duel'; Icon = 'Gladiator_Duel'; FallbackIconSpell = 3411; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "Dans l'arène."; AuraDescription = "Puissance d'attaque, dégâts et soins des sorts augmentés de 15%."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 166; TargetA = 1; Value = 15 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 15; Misc = 126 },
           @{ Index = 2; Effect = 6; Aura = 136; TargetA = 1; Value = 15 })
       Fields = @{ 40 = 8; 131 = (Look 'DuelState') } },
    # Tempête de l'arène's whirl: Bladestorm's, 200% weapon damage within 8 yd (combat bench, 2026-10-04), its off-hand
    # blow gone
    @{ Id = 95157; Clone = 50622; Name = "Tempête de l'arène"; Icon = 'Gladiator_ArenaStorm'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "200% des dégâts de l'arme aux ennemis proches."; Effects = (Strike 1 200 22)
       Fields = (Own $flagArenaStorm @{ 89 = 15; 90 = 15; 92 = 14; 93 = 14; 117 = 0
                                        131 = (Look 'ArenaStormWhirl') }) },
    # Plaie du gladiateur: Rend's bleed, a tick every second for 6 s (longer with Arène sanglante). mod-warrior rolls
    # it (each Revenge adds to what is left, as Ignite) and sets each tick already final: no caster bonus (attributes
    # ex 3 0x20000000), no damage class
    @{ Id = 95158; Clone = 772; Name = 'Plaie du gladiateur'; Icon = 'Gladiator_Wound'; FallbackIconSpell = 772; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Saigne.'; AuraDescription = 'Saigne chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; Value = 1 })
       Fields = (Own $flagWound @{ 7 = 0x20000400; 40 = 32; 41 = 1; 42 = 0; 46 = 13; 98 = 1000; 205 = 0; 206 = 0; 213 = 0 })
       # Rend's, its swing gone (Revenge swings already): Blood Strike's wet blood hit, then its bleeding chest
       Visual = @{ Clone = 372; Cast = 0; Impact = (LookKit 'Wound' 3); State = (LookKit 'Wound' 4) } },
    # What is left of the bleed at once, when Execute consumes it (an amount already final, as Meat Cleaver's relay)
    @{ Id = 95159; Clone = $computed; Name = 'Plaie du gladiateur'; Icon = 'Gladiator_Wound'; FallbackIconSpell = 772; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Dégâts physiques.'; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 })
       Fields = (Own $flagWoundBurst @{ 7 = 0x60000200; 21 = 0; 46 = 13; 131 = (Look 'WoundBurst'); 213 = 0 }) },
    # Ouverture: Shield Slam opens Revenge, free (word 0 0x400), for 8 s
    @{ Id = 95160; Clone = 2983; Name = 'Ouverture'; IconPath = 'Interface\Icons\Ability_Warrior_Revenge'; FallbackIconSpell = 6572; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vengeance utilisable.'; AuraDescription = 'Vengeance est utilisable, sans coût.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -100; Misc = $SPELLMOD_COST })
       Fields = @{ 40 = 31; 122 = $maskRevenge; 123 = 0; 124 = 0; 208 = 4 }
       # Sword and Board's, with a sword's flash and a steel ring as it comes (only when cast: mod-warrior adds the aura
       # without a cast), then blood rage on the hands while it lasts
       Visual = @{ Clone = 345; Impact = (LookKit 'OpeningRing' 3); State = (LookKit 'Opening' 4) } },
    # Garde brisée: Devastate makes the next Revenge (word 0 0x400) stronger, its amount set by mod-warrior
    @{ Id = 95161; Clone = 2983; Name = 'Garde brisée'; Icon = 'Gladiator_ShatteredGuard'; FallbackIconSpell = 20243; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vengeance renforcée.'; AuraDescription = 'Votre prochaine Vengeance inflige plus de dégâts.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 20; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 40 = 8; 122 = $maskRevenge; 123 = 0; 124 = 0; 131 = (Look 'BrokenGuard'); 208 = 4 } },
    # Bouclier du gladiateur: the shield's defense, dodge, parry and block ratings as melee and ranged critical strike
    # rating (combat ratings 8 and 9), its amount set by mod-warrior from the shield worn
    @{ Id = 95162; Clone = 2983; Name = 'Bouclier du gladiateur'; Icon = 'Gladiator_Spec'; FallbackIconSpell = 71; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = 'Score de coup critique.'; AuraDescription = 'Score de coup critique augmenté.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 189; TargetA = 1; Value = 1; Misc = 768 }); Fields = @{ 40 = 21 } },
    # Duel's share, on the ally, the Warrior its caster: the Warrior takes 30% of what the ally takes (mod-warrior
    # sets it to nothing while the Warrior is low)
    @{ Id = 95163; Clone = 2983; Name = 'Duel'; Icon = 'Gladiator_Duel'; FallbackIconSpell = 3411; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Dégâts partagés.'; AuraDescription = 'Le gladiateur subit une partie des dégâts que vous recevez.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 81; TargetA = 1; Value = 30; Misc = 127 }); Fields = @{ 40 = 8 } },

    # --- Spec passives ----------------------------------------------------------------------------------------------
    # Each specialization learns its own (specSpells in talentTree.json); mod-warrior also reads them to know which one
    # is on.
    # Armes: Mortal Strike, Overpower, Execute and Slam (word 0) deal 5% more, Whirlwind (word 1 0x4, Bladestorm's whirls
    # too) and Cleave 30% more; Fulgurance gets 2 charges (mod-warrior)
    @{ Id = 95280; Clone = 2983; Name = 'Armes'; IconPath = 'Interface\Icons\Ability_Warrior_SavageBlow'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Frappe mortelle, Fulgurance, Exécution et Heurtoir infligent 5% de dégâts en plus, Tourbillon, Tempête de lames et Enchaînement 30% de plus. Fulgurance n'a plus besoin d'une esquive et dispose de 2 charges."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 5; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 30; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 122 = ($maskMortalStrike -bor $maskOverpower -bor $maskExecute -bor $maskSlam); 123 = 0; 124 = 0
                   125 = $maskCleave; 126 = $maskWhirlwind; 127 = 0; 208 = 4 } },
    # Fureur: Whirlwind (word 1) deals 30% more, the off-hand 10% more; 60% less rage from auto attacks
    # (aura 213: the builders feed Rampage, not the swings); Enrage and Raging Blow's charges (mod-warrior)
    @{ Id = 95281; Clone = 2983; Name = 'Fureur'; IconPath = 'Interface\Icons\Ability_Warrior_Innerrage'; FallbackIconSpell = 23881; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Sanguinaire et Coup déchaîné vous rendent de la rage, que Saccage dépense. Un coup critique de Sanguinaire et Saccage vous rendent Enragé : vitesse d'attaque +25% pendant 4 s. Tourbillon inflige 30% de dégâts en plus, et votre main gauche 10% de plus ; vos attaques automatiques génèrent 60% de rage en moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 30; Misc = $SPELLMOD_DAMAGE },
           @{ Index = 1; Effect = 6; Aura = $A_ModOffhandDamagePct; TargetA = 1; Value = 10 },
           @{ Index = 2; Effect = 6; Aura = 213; TargetA = 1; Value = -60 })
       Fields = @{ 122 = 0; 123 = $maskWhirlwind; 124 = 0; 208 = 4 } },
    # Protection: 10% stamina and armor; Thunder Clap, Revenge (word 0) and Shield Slam (word 1) deal 30% more
    @{ Id = 95282; Clone = 2983; Name = 'Protection'; IconPath = 'Interface\Icons\Ability_Warrior_DefensiveStance'; FallbackIconSpell = 71; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Votre Endurance et votre armure augmentent de 10%. Heurt de bouclier et Coup de tonnerre vous rendent de la rage, Ignorer la douleur et Maîtrise du blocage la dépensent ; une esquive, une parade ou un blocage rend Vengeance gratuite. Coup de tonnerre, Vengeance et Heurt de bouclier infligent 30% de dégâts en plus."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModTotalStatPercentage; TargetA = 1; Value = 10; Misc = 2 },
           @{ Index = 1; Effect = 6; Aura = 101; TargetA = 1; Value = 10; Misc = 1 },
           @{ Index = 2; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = 30; Misc = $SPELLMOD_DAMAGE })
       Fields = @{ 128 = ($maskThunderClap -bor $maskRevenge); 129 = $maskShieldSlam; 130 = 0; 208 = 4 } },
    # Gladiateur: half the threat; Shield Slam (word 1) recharges in 12 s rather than 6; the rest is mod-warrior's
    @{ Id = 95283; Clone = 2983; Name = 'Gladiateur'; Icon = 'Gladiator_Spec'; FallbackIconSpell = 23922; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = "Heurt de bouclier se recharge en 12 s, vous rend 20 points de rage et rend Vengeance utilisable, sans coût. Vengeance laisse Plaie du gladiateur, un saignement qui s'additionne à chaque Vengeance et dont chaque dégât peut réinitialiser Heurt de bouclier. Dévaster renforce votre prochaine Vengeance. Exécution est utilisable sur une cible qui saigne de votre Plaie : elle dépense toute votre rage et consume la plaie. Les scores de défense, d'esquive, de parade et de blocage de votre bouclier deviennent du score de coup critique, et vous générez 50% de menace en moins."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModThreat; TargetA = 1; Value = -50; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = $A_AddFlatModifier; TargetA = 1; Value = 6000; Misc = $SPELLMOD_COOLDOWN })
       Fields = @{ 125 = 0; 126 = $maskShieldSlam; 127 = 0; 208 = 4 } }
)

# The rank spells of the new talents, one hidden passive per rank (modifiers, or dummies mod-warrior reads)
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\warrior\talentTree.json') -Family 4

return $spells
