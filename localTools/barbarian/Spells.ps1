# The Barbarian's spell data (class 14, modules/mod-barbarian): its abilities, the auras they put up, the damage
# carriers its scripts cast, and the rank spells of its talent trees (localTools/barbarian/talentTree.json).
# Behaviour lives in modules/mod-barbarian; this file owns client/server spell data only.
# Ids: 97100-97299 abilities and auras, 97300-97499 talent ranks (generated from the trees).
#
# Every look is one imported from the Ascension client (localTools/barbarian/ascensionVisuals.json, through
# localTools/ascensionImport/importVisuals.py): field 131 names the imported SpellVisual by its key.
#
# Field notes as in localTools/warrior/Spells.ps1: 2 dispel type (9 Enrage: the core then sets AURA_STATE_ENRAGE, which
# field 20 = 17 asks for), 3 mechanic, 9 AttributesEx5 (0x8 usable while stunned), 20 caster aura state, 21 target aura
# state (13 below 35% health), 28 cast time index, 29 cooldown, 40 duration index (65 1.5 s, 39 2 s, 35 4 s, 32 6 s,
# 31 8 s, 1 10 s, 29 12 s, 8 15 s, 18 20 s, 9 30 s, 21 never), 41 power type (3 energy), 46 range index (1 self, 2
# melee, 95 8-25 yd), 68-70 equipped item, 83-85 effect mechanic, 86-88 target A, 89-91 target B, 92-94 radius index
# (14 8 yd, 18 15 yd, 10 30 yd), 98-100 periodic interval, 104-106 chain targets, 110-112 misc, 116-118 triggered
# spell, 122-130 the effects' class masks, 131 visual, 205-206 global cooldown category and time (0 0: off it), 208
# family (20), 209-211 family flags, 212 maximum targets, 213 damage class (2 melee), 225 school (1 physical, 16 frost).

$classMask = 8192
$skillLine = 909
$family = 20
$gcd = 1000

# Word 0 of the family flags: one bit per ability, for the talents' modifiers (talentTree.json says the same). Word 2
# bit 1 is every Barbarian ability.
$flagStrike = 0x1; $flagWhirl = 0x2; $flagUnbridled = 0x4; $flagSnap = 0x8; $flagAdvance = 0x10; $flagWarCry = 0x20
$flagVigor = 0x40; $flagEndless = 0x80; $flagRush = 0x100; $flagSmash = 0x200; $flagRampage = 0x400
$flagSwing = 0x800; $flagCrush = 0x1000; $flagDecapitate = 0x2000; $flagOnslaught = 0x4000; $flagOutrage = 0x8000
$flagStorm = 0x10000; $flagHull = 0x20000; $flagImpale = 0x40000; $flagCarnage = 0x80000; $flagMight = 0x100000
$flagRamhorn = 0x200000; $flagBloodthirsty = 0x400000; $flagSkull = 0x800000

$imported = Get-Content -LiteralPath (Join-Path $repoRoot 'modules\mod-barbarian\client-assets\imported\visuals.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json
function Look([string]$key) {
    $id = $imported.ids.$key
    if (-not $id) { throw "No imported Barbarian look '$key' (localTools\barbarian\ascensionVisuals.json)." }
    return [uint32]$id
}

# Mortal Strike's layout for a weapon strike, Cleave's for one that chains, Whirlwind's for weapon damage around the
# Barbarian, Sprint's for an instant self buff, Pummel's interrupt, Intercept's rush (Charge is refused in combat),
# Commanding Shout's group aura
$strike = 12294
$cleave = 845
$whirl = 1680
$selfBuff = 2983
$pummel = 6552
$charge = 20252
$shout = 47440

# A weapon strike's effects: flat bonus, then the weapon percentage
function Strike($flat, $percent, $target = 6) {
    return @(
        @{ Index = 0; Effect = 121; TargetA = $target; Value = $flat },
        @{ Index = 1; Effect = 31; TargetA = $target; Value = $percent })
}

# Every Barbarian spell: its own family and flags, energy, no stance, no category; ability flags in word 0 and the
# Barbarian bit in word 2
function Own($flag, $extra = @{}) {
    $fields = @{ 1 = 0; 12 = 0; 13 = 0; 30 = 0; 41 = 3; 205 = 133; 206 = $gcd; 208 = $family; 209 = $flag; 210 = 0
        211 = 1; 213 = 2; 225 = 1 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# An enrage: dispel type Enrage, off the global cooldown, and 20% more energy regeneration while it lasts
$enrageRegen = @{ Effect = 6; Aura = $A_ModPowerRegenPercent; TargetA = 1; Value = 20; Misc = 3 }
function Enrage($flag, $duration, $extra = @{}) {
    $fields = Own $flag @{ 2 = 9; 28 = 1; 40 = $duration; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0
        206 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$spells = @(
    # --- Class baseline (mod-barbarian teaches these by level) ------------------------------------------------------
    @{ Id = 97100; Clone = $strike; Name = 'Frappe barbare'; IconPath = 'Interface\Icons\Warrior_Wild_Strike'; FallbackIconSpell = 12294; Cost = 35; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappe l'ennemi : 120% des dégâts de l'arme, et 2 charges de Carnage."
       Effects = (Strike 30 120); Fields = (Own $flagStrike @{ 131 = (Look 'BarbaricStrike') }) },
    @{ Id = 97102; Clone = $whirl; Name = 'Tourbillon barbare'; IconPath = 'Interface\Icons\Ability_Garrosh_Whirling_Corruption'; FallbackIconSpell = 1680; Cost = 30; Cooldown = 0; Level = 3; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Tourbillonne avec fureur : 80% des dégâts de l'arme à 8 ennemis au plus à 8 m, et 1 charge de Carnage à chacun."
       Effects = (Strike 10 80 22)
       Fields = (Own $flagWhirl @{ 46 = 1; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 116 = 0; 117 = 0; 212 = 8; 131 = (Look 'BarbaricWhirl') }) },
    @{ Id = 97103; Clone = $selfBuff; Name = 'Rage débridée'; IconPath = 'Interface\Icons\_D3wrathoftheberserker'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 30000; Level = 5; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Vos dégâts physiques augmentent de 10% pendant 10 s."
       AuraDescription = 'Enragé. Dégâts physiques augmentés de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 1 }, ($enrageRegen + @{ Index = 1 }))
       Fields = (Enrage $flagUnbridled 1 @{ 131 = (Look 'Enrage') }) },
    @{ Id = 97104; Clone = $pummel; Name = 'Prise du poignet'; IconPath = 'Interface\Icons\_D3tempestrush'; FallbackIconSpell = 6552; Cost = 10; Cooldown = 15000; Level = 7; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Tord le poignet de l'ennemi : interrompt son incantation et l'empêche de lancer un sort de cette école pendant 4 s."
       Fields = (Own $flagSnap @{ 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 131 = (Look 'WristSnap') }) },
    @{ Id = 97105; Clone = $selfBuff; Name = 'Ruée empalante'; IconPath = 'Interface\Icons\_D3furiouscharge'; FallbackIconSpell = 2983; Cost = 15; Cooldown = 20000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Votre vitesse de déplacement augmente de 40% pendant 4 s, et votre prochaine attaque empale la cible : 3 charges de Carnage."
       AuraDescription = 'Vitesse de déplacement augmentée de 40%. La prochaine attaque empale la cible.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 40 })
       Fields = (Own $flagRush @{ 40 = 35; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 131 = (Look 'ImpalingRush') }) },
    @{ Id = 97106; Clone = $charge; Name = 'Avancée tourbillonnante'; IconPath = 'Interface\Icons\_D3whirlwind'; FallbackIconSpell = 100; Cost = 0; Cooldown = 20000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fonce en tourbillonnant vers un ennemi entre 8 et 25 m et frappe les ennemis à 8 m de l'arrivée : 60% des dégâts de l'arme."
       Effects = @(@{ Index = 0; Effect = 96; TargetA = 6 })
       Fields = (Own $flagAdvance @{ 12 = 0; 46 = 95; 131 = (Look 'WhirlingAdvance') }) },
    @{ Id = 97107; Clone = $shout; Name = 'Cri de guerre'; IconPath = 'Interface\Icons\_D3threateningshout'; FallbackIconSpell = 6673; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Un cri de guerre à glacer le sang : la puissance d'attaque des membres du groupe à 30 m augmente de 10% pendant 30 s."
       AuraDescription = "Puissance d'attaque augmentée de 10%."
       Effects = @(
           @{ Index = 0; Effect = 35; Aura = 166; TargetA = 1; Value = 10 },
           @{ Index = 1; Effect = 35; Aura = 167; TargetA = 1; Value = 10 })
       Fields = (Own $flagWarCry @{ 40 = 9; 92 = 10; 93 = 10; 131 = (Look 'WarCry') }) },
    @{ Id = 97108; Clone = $selfBuff; Name = 'Vigueur de bataille'; IconPath = 'Interface\Icons\Ability_Warrior_StrengthOfArms'; FallbackIconSpell = 12975; Cost = 0; Cooldown = 180000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Vos points de vie maximum augmentent de 20% pendant 15 s, puis ce surplus se perd."
       AuraDescription = 'Enragé. Points de vie maximum augmentés de 20%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModIncreaseHealthPercent; TargetA = 1; Value = 20 }, ($enrageRegen + @{ Index = 1 }))
       Fields = (Enrage $flagVigor 8 @{ 131 = (Look 'BattleVigor') }) },
    @{ Id = 97109; Clone = $selfBuff; Name = 'Fureur sans fin'; IconPath = 'Interface\Icons\Ability_Warrior_IntensifyRage'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Vous rend 3% de vos points de vie maximum par seconde et augmente de 30% votre vitesse de déplacement pendant 6 s."
       AuraDescription = 'Enragé. Rend 3% des points de vie maximum par seconde. Vitesse de déplacement augmentée de 30%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 20; TargetA = 1; Value = 3 },
           @{ Index = 1; Effect = 6; Aura = $A_ModIncreaseSpeed; TargetA = 1; Value = 30 },
           ($enrageRegen + @{ Index = 2 }))
       Fields = (Enrage $flagEndless 32 @{ 98 = 1000; 131 = (Look 'Enrage') }) },
    @{ Id = 97110; Clone = $selfBuff; Name = 'Puissance ancestrale'; IconPath = 'Interface\Icons\Achievement_AlliedRace_DarkIronDwarf'; FallbackIconSpell = 12975; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Pendant 12 s, vos techniques coûtent 25% d'énergie en moins et vous subissez 15% de dégâts en moins."
       AuraDescription = "Enragé. Coût en énergie réduit de 25%. Dégâts subis réduits de 15%."
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_AddPctModifier; TargetA = 1; Value = -25; Misc = 14 },
           @{ Index = 1; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = -15; Misc = 127 },
           ($enrageRegen + @{ Index = 2 }))
       Fields = (Enrage $flagMight 29 @{ 122 = 0; 123 = 0; 124 = 1; 131 = (Look 'AncestralMight') }) },
    @{ Id = 97111; Clone = $shout; Name = 'Rage de Corne-de-bélier'; IconPath = 'Interface\Icons\INV_Holiday_Beerfest_Maghar'; FallbackIconSpell = 12975; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Vous et les membres du groupe à 15 m récupérez 2% de vos points de vie maximum par seconde pendant 8 s."
       AuraDescription = 'Enragé. Rend 2% des points de vie maximum par seconde.'
       Effects = @(@{ Index = 0; Effect = 35; Aura = 20; TargetA = 1; Value = 2 })
       Fields = (Own $flagRamhorn @{ 2 = 9; 40 = 31; 92 = 18; 98 = 1000; 205 = 0; 206 = 0; 131 = (Look 'RamhornRage') }) },
    @{ Id = 97112; Clone = $selfBuff; Name = 'Crâne épais'; IconPath = 'Interface\Icons\Spell_Nature_StoneSkinTotem'; FallbackIconSpell = 59752; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = 'Vous libère des étourdissements et vous y rend insensible pendant 6 s. Utilisable étourdi.'
       AuraDescription = 'Insensible aux étourdissements.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 77; TargetA = 1; Misc = 12 },
           @{ Index = 1; Effect = 108; TargetA = 1; Misc = 12 })
       Fields = (Own $flagSkull @{ 9 = 0x8; 40 = 32; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 131 = (Look 'ThickSkull') }) },
    # Avancée tourbillonnante's blow on arrival (mod-barbarian casts it as the Barbarian lands)
    @{ Id = 97113; Clone = $whirl; Name = 'Avancée tourbillonnante'; IconPath = 'Interface\Icons\_D3whirlwind'; FallbackIconSpell = 1680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "60% des dégâts de l'arme."; Effects = (Strike 0 60 22)
       Fields = (Own $flagAdvance @{ 46 = 1; 89 = 15; 90 = 15; 92 = 14; 93 = 14; 116 = 0; 117 = 0; 205 = 0; 206 = 0; 212 = 0; 131 = 0 }) },
    # Carnage: a bleed of up to 10 stacks; mod-barbarian sizes each tick from the Barbarian's attack power
    @{ Id = 97114; Clone = 1943; Name = 'Carnage'; IconPath = 'Interface\Icons\Ability_Rogue_Rupture'; FallbackIconSpell = 1943; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; BleedAura = $true
       Description = 'Saigne toutes les 3 s.'; AuraDescription = 'Saigne toutes les 3 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = (Own $flagCarnage @{ 3 = 15; 40 = 29; 49 = 10; 83 = 15; 98 = 3000; 205 = 0; 206 = 0; 131 = (Look 'Carnage') }) },
    # Rage sanguinaire: Brutalité's enrage, from its auto attacks (mod-barbarian)
    @{ Id = 97115; Clone = $selfBuff; Name = 'Rage sanguinaire'; IconPath = 'Interface\Icons\Ability_Warrior_BloodFrenzy'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Effet d'enragement. Dégâts physiques augmentés de 10%."
       AuraDescription = 'Enragé. Dégâts physiques augmentés de 10%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModDamagePercentDone; TargetA = 1; Value = 10; Misc = 1 }, ($enrageRegen + @{ Index = 1 }))
       Fields = (Enrage $flagBloodthirsty 32 @{ 131 = (Look 'Enrage') }) },

    # Né dans le sang (talent): the heal over 6 s, each tick's amount set by mod-barbarian
    @{ Id = 97116; Clone = $selfBuff; Name = 'Né dans le sang'; IconPath = 'Interface\Icons\Ability_Warrior_BloodFrenzy'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Rend des points de vie toutes les secondes.'; AuraDescription = 'Rend des points de vie toutes les secondes.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 8; TargetA = 1; BasePoints = 0 })
       Fields = (Own $flagUnbridled @{ 40 = 32; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 98 = 1000; 205 = 0; 206 = 0; 209 = 0; 211 = 0; 131 = 0 }) },
    # Maîtrise de la colère (talent): Rage débridée frees the Barbarian from fear and keeps it free for 6 s
    @{ Id = 97117; Clone = $selfBuff; Name = 'Maîtrise de la colère'; IconPath = 'Interface\Icons\Spell_Shadow_UnholyFrenzy'; FallbackIconSpell = 18499; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Insensible à la peur.'; AuraDescription = 'Insensible à la peur.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 77; TargetA = 1; Misc = 5 },
           @{ Index = 1; Effect = 108; TargetA = 1; Misc = 5 })
       Fields = (Own $flagUnbridled @{ 40 = 32; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 209 = 0; 211 = 0; 131 = 0 }) },

    # --- Brutalité ------------------------------------------------------------------------------------------------
    @{ Id = 97120; Clone = $strike; Name = 'Fracas'; IconPath = 'Interface\Icons\INV_Mace_69'; FallbackIconSpell = 12294; Cost = 15; Cooldown = 6000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Nécessite d'être enragé. Fracasse l'ennemi : 210% des dégâts de l'arme."
       Effects = (Strike 40 210); Fields = (Own $flagSmash @{ 20 = 17; 131 = (Look 'Smash') }) },
    @{ Id = 97121; Clone = $strike; Name = 'Déchaînement'; IconPath = 'Interface\Icons\Ability_Warrior_Rampage'; FallbackIconSpell = 29801; Cost = 20; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Nécessite d'être enragé. Frappe sauvagement l'ennemi : 150% des dégâts de l'arme, et 3 charges de Carnage."
       Effects = (Strike 20 150); Fields = (Own $flagRampage @{ 20 = 17; 131 = (Look 'Rampage') }) },
    @{ Id = 97122; Clone = $cleave; Name = 'Taille brutale'; IconPath = 'Interface\Icons\INV_Axe_2H_OrcWarrior_C_01'; FallbackIconSpell = 845; Cost = 30; Cooldown = 8000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappe l'ennemi et jusqu'à 2 ennemis proches : 140% des dégâts de l'arme."
       Effects = (Strike 20 140); Fields = (Own $flagSwing @{ 28 = 1; 104 = 3; 105 = 3; 131 = (Look 'BrutalSwing') }) },
    @{ Id = 97123; Clone = $strike; Name = 'Écrasement'; IconPath = 'Interface\Icons\Ability_Warrior_Trauma'; FallbackIconSpell = 46968; Cost = 40; Cooldown = 20000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Écrase les ennemis dans un cône de 8 m : 250% des dégâts de l'arme, et les étourdit 1,5 s."
       AuraDescription = 'Étourdi.'
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 24; Value = 30 },
           @{ Index = 1; Effect = 31; TargetA = 24; Value = 250 },
           @{ Index = 2; Effect = 6; Aura = 12; TargetA = 24 })
       Fields = (Own $flagCrush @{ 40 = 65; 46 = 1; 85 = 12; 92 = 14; 93 = 14; 94 = 14; 212 = 0; 131 = (Look 'Crush') }) },
    @{ Id = 97124; Clone = $strike; Name = 'Décapitation'; IconPath = 'Interface\Icons\Spell_DeathKnight_Butcher2'; FallbackIconSpell = 5308; Cost = 25; Cooldown = 0; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Tente de décapiter un ennemi sous 35% de ses points de vie : 200% des dégâts de l'arme, et chaque point d'énergie en plus, jusqu'à 50, ajoute 2% de dégâts."
       Effects = (Strike 40 200); Fields = (Own $flagDecapitate @{ 21 = 13; 131 = (Look 'Decapitate') }) },
    @{ Id = 97125; Clone = $selfBuff; Name = 'Assaut'; IconPath = 'Interface\Icons\_D3warcry'; FallbackIconSpell = 2687; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Effet d'enragement. Vous gagnez 25 points d'énergie, puis 5 par seconde pendant 10 s."
       AuraDescription = "Enragé. Rend 5 points d'énergie par seconde."
       Effects = @(
           @{ Index = 0; Effect = 30; TargetA = 1; Value = 25; Misc = 3 },
           @{ Index = 1; Effect = 6; Aura = 24; TargetA = 1; Value = 5; Misc = 3 },
           ($enrageRegen + @{ Index = 2 }))
       Fields = (Enrage $flagOnslaught 1 @{ 99 = 1000; 131 = (Look 'Enrage') }) },
    @{ Id = 97126; Clone = $selfBuff; Name = 'Outrage'; IconPath = 'Interface\Icons\Ability_Warrior_FocusedRage'; FallbackIconSpell = 1719; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Nécessite d'être enragé. Pendant 20 s, votre hâte augmente de 20% et vos chances de coup critique de 15%, mais vous subissez 10% de dégâts en plus."
       AuraDescription = 'Hâte augmentée de 20%, chances de coup critique de 15%. Dégâts subis augmentés de 10%.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = $A_ModMeleeHaste; TargetA = 1; Value = 20 },
           @{ Index = 1; Effect = 6; Aura = $A_ModCritPct; TargetA = 1; Value = 15 },
           @{ Index = 2; Effect = 6; Aura = $A_ModDamagePercentTaken; TargetA = 1; Value = 10; Misc = 127 })
       Fields = (Own $flagOutrage @{ 20 = 17; 40 = 18; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 205 = 0; 206 = 0; 131 = (Look 'Outrage') }) },
    @{ Id = 97127; Clone = $selfBuff; Name = "Tempête d'acier"; IconPath = 'Interface\Icons\Achievement_Arena_5v5_5'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 30000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Frappe la cible toutes les 0,5 s pendant 6 s : 50% des dégâts de l'arme à chaque coup."
       AuraDescription = 'Frappe la cible toutes les 0,5 s.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 226; TargetA = 1 })
       Fields = (Own $flagStorm @{ 40 = 32; 46 = 1; 68 = [uint32]::MaxValue; 69 = 0; 70 = 0; 98 = 500; 131 = (Look 'StormOfSteel') }) },
    @{ Id = 97128; Clone = $strike; Name = 'Briseur de coque'; IconPath = 'Interface\Icons\INV_Archaeology_Ogres_Warmaul_Chieftain'; FallbackIconSpell = 12294; Cost = 30; Cooldown = 15000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fracasse l'ennemi : 180% des dégâts de l'arme, et il subit 10% de dégâts physiques en plus de votre part pendant 12 s."
       AuraDescription = 'Dégâts physiques subis du barbare augmentés de 10%.'
       Effects = @(
           @{ Index = 0; Effect = 121; TargetA = 6; Value = 30 },
           @{ Index = 1; Effect = 31; TargetA = 6; Value = 180 },
           @{ Index = 2; Effect = 6; Aura = 271; TargetA = 6; Value = 10; Misc = 1 })
       Fields = (Own $flagHull @{ 40 = 29; 131 = (Look 'Hullbreaker') }) },
    @{ Id = 97129; Clone = $strike; Name = 'Empaler'; IconPath = 'Interface\Icons\INV_Axe_2H_WarfrontsHorde_C_01'; FallbackIconSpell = 12294; Cost = 35; Cooldown = 14000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Empale l'ennemi : 160% des dégâts de l'arme. Sous 35% de ses points de vie, 100% de plus et 5 charges de Carnage."
       Effects = (Strike 30 160); Fields = (Own $flagImpale @{ 131 = (Look 'Impale') }) },
    # Tempête d'acier's blows (mod-barbarian casts one at the target every 0.5 s)
    @{ Id = 97130; Clone = $strike; Name = "Tempête d'acier"; IconPath = 'Interface\Icons\Achievement_Arena_5v5_5'; FallbackIconSpell = 46924; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "50% des dégâts de l'arme."; Effects = (Strike 0 50)
       Fields = (Own $flagStorm @{ 205 = 0; 206 = 0; 131 = 0 }) },
    # Brutalité: hidden, what tells the scripts the specialization is on
    @{ Id = 97290; Clone = $selfBuff; Name = 'Brutalité'; IconPath = 'Interface\Icons\Ability_Warrior_Rampage'; FallbackIconSpell = 12294; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
       Description = 'Vos attaques automatiques ont 15% de chances de vous enrager (Rage sanguinaire).'
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 }); Fields = @{ 208 = $family } }
)

# Talent ranks come from the talent trees (talentTree.json), which the server and the talent window read as well
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\barbarian\talentTree.json') -Family $family

$spells
