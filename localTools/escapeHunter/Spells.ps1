# Le Traqueur d'évadés's spell data (modules/mod-stat-growth/src/EscapeHunter.cpp, the Hellfire Gaol's second gate;
# plan: .agents/plans/escape-hunter). Ids 94800-94859; his looks (shapes.json) 94860-94869.
#
# As Gardien-chef Vorhan's (localTools/wardenVorhan/Spells.ps1): every hit a spell of its own whose description says
# what went wrong (the combat log and the death recap: nothing is written on the screen in the fight), the debuffs
# dummy auras timed and stacked by the script, his casts a dummy on himself as long as the step takes to come, so the
# players read it coming on his cast bar. Icons: ICON_Traqueur_* once painted (localTools/escapeHunter/buildIcons.py,
# the brief: .agents/plans/escape-hunter/escape-hunter.ASSETS.md), a stock one until then.

$physical = 1
$nature = 8
$shadow = 32
$debuff = 0x04000000

# A hit, never cast: its name and text in the combat log and the death recap
function New-Hit($id, $name, $icon, $fallback, $school, $description) {
    @{ Id = $id; Clone = 64596; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Fields = @{ 225 = $school } }
}

# A debuff the script puts on and times (40: its duration row, the script sets the real one)
function New-Debuff($id, $name, $icon, $fallback, $maxStacks, $description, $auraDescription) {
    @{ Id = $id; Clone = 2983; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = $maxStacks; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Fields = @{ 4 = $debuff; 40 = 21 } }
}

# A step announced on his cast bar: a dummy on himself, cast for as long as the step takes to come (28: SpellCastTimes,
# 4 = 1 s, 16 = 1.5 s, 5 = 2 s, 15 = 4 s, 171 = 6 s)
function New-Cast($id, $name, $icon, $fallback, $castTime, $description) {
    @{ Id = $id; Clone = 585; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = @{ 28 = $castTime; 131 = 0; 213 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
}

$spells = @(
    # --- What hits: one spell a mechanic, its text saying what went wrong ---
    (New-Hit 94800 'Taillade' 'ICON_Traqueur_Taillade' 845 $physical `
        'Le Traqueur taille devant lui. Tenez-vous sur ses flancs si vous ne le tenez pas.'),
    (New-Hit 94801 'Cor de chasse' 'ICON_Traqueur_Cor' 23154 $physical `
        'Le cor du Traqueur frappe tous les évadés.'),
    (New-Hit 94802 'Lumière des lanternes' 'ICON_Traqueur_Lanternes' 2006 $physical `
        'Les lanternes des rabatteurs vous ont montré au Traqueur. Seul le côté d''une lanterne reste dans l''ombre.'),
    (New-Hit 94803 'Piège de la proie' 'ICON_Traqueur_PiegeProie' 1499 $physical `
        'Un piège laissé par la proie s''est refermé sur vous. Que la proie l''emmène le long du mur.'),
    (New-Hit 94805 'Battue' 'ICON_Traqueur_Battue' 5246 $physical `
        'Les rabatteurs ont passé votre rangée. Tenez-vous dans sa trouée.'),
    (New-Hit 94806 'Hallali' 'ICON_Traqueur_Hallali' 1130 $physical `
        'La meute a bondi sur la proie : éloignez-vous des proies.'),
    (New-Hit 94807 'Curée de meute' 'ICON_Traqueur_CureeMeute' 1130 $physical `
        'La meute s''abat sur le groupe : ensemble sous le Traqueur, le coup se partage.'),
    (New-Hit 94808 'Charge dans le noir' 'ICON_Traqueur_Charge' 58984 $shadow `
        'Le Traqueur a chargé depuis le bruissement, à travers le milieu de la salle.'),
    (New-Hit 94809 'Curée' 'ICON_Traqueur_Curee' 34026 $physical `
        'La proie n''était pas aux abois : le Traqueur avait encore trop de forces.'),
    (New-Hit 94811 'Revers' 'ICON_Traqueur_Revers' 845 $physical `
        'Le Traqueur frappe derrière lui après sa taille. Tenez-vous sur ses flancs.'),
    (New-Hit 94812 'Collet' 'ICON_Traqueur_Collet' 1499 $physical `
        'Le collet s''est refermé. Retenez l''ordre de ses éclats.'),
    (New-Hit 94818 'Hurlement du Traqueur' 'ICON_Traqueur_Hurlement' 26662 $shadow `
        'Le Traqueur revient de l''ombre en hurlant.'),
    (New-Hit 94825 'Moulinet' 'ICON_Traqueur_Moulinet' 6533 $physical `
        'Ses chaînes se sont arrêtées sur vous. Regardez où elles se figent.'),
    (New-Hit 94826 'Pistage' 'ICON_Traqueur_Pistage' 1130 $physical `
        'Votre piste a été prise. Quittez la marque quand elle se fige.'),

    # --- The debuffs ---
    (New-Debuff 94820 'Lacération' 'ICON_Traqueur_Laceration' 845 10 `
        'Les tailles du Traqueur.' 'Dégâts physiques subis augmentés. Laissez-le à l''autre tank.'),
    (New-Debuff 94821 'Débusqué' 'ICON_Traqueur_Debusque' 1130 1 `
        'Débusqué par une erreur.' 'Dégâts subis augmentés.'),
    (New-Debuff 94822 'Proie de l''hallali' 'ICON_Traqueur_Hallali' 1130 1 `
        'La meute va bondir sur vous.' 'La meute bondira sur vous.'),
    (New-Debuff 94824 'Proie' 'ICON_Traqueur_Proie' 1499 1 `
        'Vos pas laissent des pièges derrière vous.' 'Vos pas laissent des pièges derrière vous.'),

    # --- His cast bars (28: SpellCastTimes - 4 = 1 s, 16 = 1.5 s, 5 = 2 s, 15 = 4 s, 171 = 6 s) ---
    (New-Cast 94840 'Taillade' 'ICON_Traqueur_Taillade' 845 16 'Le Traqueur taille devant lui, puis derrière.'),
    (New-Cast 94841 'Cor de chasse' 'ICON_Traqueur_Cor' 23154 4 'Le Traqueur sonne du cor.'),
    (New-Cast 94842 'Battue' 'ICON_Traqueur_Battue' 5246 5 'Les rabatteurs entrent.'),
    (New-Cast 94843 'Hallali' 'ICON_Traqueur_Hallali' 1130 15 'La meute va bondir sur ses proies.'),
    (New-Cast 94844 'Traque' 'ICON_Traqueur_Traque' 58984 5 'Le Traqueur disparaît dans le noir.'),
    (New-Cast 94845 'Curée' 'ICON_Traqueur_Curee' 34026 171 'La curée : la proie doit être aux abois.'),
    (New-Cast 94847 'Collet' 'ICON_Traqueur_Collet' 1499 15 'Le Traqueur tend son collet.'),
    (New-Cast 94848 'Lanternes des rabatteurs' 'ICON_Traqueur_Lanternes' 2006 15 'Les rabatteurs lèvent leurs lanternes.'),
    (New-Cast 94849 'Proie' 'ICON_Traqueur_Proie' 1499 16 'Le Traqueur désigne ses proies.'),
    (New-Cast 94852 'Moulinet' 'ICON_Traqueur_Moulinet' 6533 5 'Le Traqueur fait tournoyer ses chaînes.'),
    (New-Cast 94853 'Pistage' 'ICON_Traqueur_Pistage' 1130 16 'Le Traqueur prend vos pistes.')
)

return $spells
