# Le Traqueur d'évadés's spell data (modules/mod-stat-growth/src/EscapeHunter.cpp, the Hellfire Gaol's second gate;
# plan: .agents/plans/escape-hunter). Ids 94800-94859.
#
# As Gardien-chef Vorhan's (localTools/wardenVorhan/Spells.ps1): every hit a spell of its own whose description says
# what went wrong, the debuffs dummy auras timed and stacked by the script, his casts a dummy on himself with the time
# the step takes, so the players read it coming on his cast bar. Icons: stock ones until painted.

$physical = 1
$nature = 8
$shadow = 32
$debuff = 0x04000000

# A hit, never cast: its name and text in the combat log and the death recap
function New-Hit($id, $name, $fallback, $school, $description) {
    @{ Id = $id; Clone = 64596; Name = $name; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Fields = @{ 225 = $school } }
}

# A debuff the script puts on and times (40: its duration row, the script sets the real one)
function New-Debuff($id, $name, $fallback, $maxStacks, $description, $auraDescription) {
    @{ Id = $id; Clone = 2983; Name = $name; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = $maxStacks; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Fields = @{ 4 = $debuff; 40 = 21 } }
}

# A step announced on his cast bar: a dummy on himself, cast for as long as the step takes to come (28: SpellCastTimes,
# 4 = 1 s, 16 = 1.5 s, 5 = 2 s, 15 = 4 s, 171 = 6 s)
function New-Cast($id, $name, $fallback, $castTime, $description) {
    @{ Id = $id; Clone = 585; Name = $name; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = @{ 28 = $castTime; 131 = 0; 213 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
}

$spells = @(
    # --- What hits: one spell a mechanic, its text saying what went wrong ---
    (New-Hit 94800 'Taillade' 845 $physical `
        'Le Traqueur taille devant lui. Tenez-vous sur ses flancs si vous ne le tenez pas.'),
    (New-Hit 94801 'Cor de chasse' 23154 $physical `
        'Le cor du Traqueur frappe tous les évadés.'),
    (New-Hit 94802 'Rabattage' 5246 $physical `
        'Les rabatteurs ont balayé votre couloir. Changez de couloir à chaque vague.'),
    (New-Hit 94803 'Piège à mâchoires' 1499 $physical `
        'Vous avez marché dans un piège armé. Contournez-les.'),
    (New-Hit 94804 'Volée de flèches' 3034 $physical `
        'Une volée des rabatteurs. Sortez des cercles.'),
    (New-Hit 94805 'Battue' 5246 $physical `
        'Les rabatteurs ont passé votre rangée. Tenez-vous dans sa trouée.'),
    (New-Hit 94806 'Hallali' 1130 $physical `
        'La meute a bondi sur la proie : éloignez-vous des proies.'),
    (New-Hit 94807 'Curée de meute' 1130 $physical `
        'La meute s''abat sur le groupe : ensemble, le coup se partage.'),
    (New-Hit 94808 'Bond dans l''ombre' 58984 $shadow `
        'Le Traqueur a bondi sur vous depuis l''ombre. Sortez du cercle.'),
    (New-Hit 94809 'Curée' 34026 $physical `
        'La proie n''était pas aux abois : le Traqueur avait encore trop de forces.'),
    (New-Hit 94810 'Fin de la traque' 26662 $shadow `
        'La traque est finie, et vous aussi.'),
    (New-Hit 94811 'Revers' 845 $physical `
        'Le Traqueur frappe derrière lui après sa taille. Tenez-vous sur ses flancs.'),
    (New-Hit 94812 'Collet' 1499 $physical `
        'Le collet s''est refermé. Dedans, dehors : suivez son rythme.'),
    (New-Hit 94813 'Encerclement' 3034 $physical `
        'Les rabatteurs tirent de trois côtés. Suivez le quart épargné.'),
    (New-Hit 94814 'Charge de la meute' 1130 $physical `
        'La meute a chargé à travers vous. Écartez-vous de sa ligne.'),
    (New-Hit 94815 'Bond du Traqueur' 6533 $physical `
        'Le Traqueur a bondi sur vous. Quittez le cercle avant qu''il retombe.'),
    (New-Hit 94816 'Onde de choc' 6533 $physical `
        'L''onde de son bond. Rejoignez le Traqueur là où il est retombé.'),
    (New-Hit 94817 'Déclenchement' 1499 $physical `
        'Les pièges ont sauté l''un après l''autre. Éloignez-vous d''eux.'),
    (New-Hit 94818 'Hurlement du Traqueur' 26662 $shadow `
        'Le Traqueur revient de l''ombre en hurlant.'),
    (New-Hit 94819 'Griffes dans le noir' 58984 $shadow `
        'Le Traqueur a chargé à travers le noir. Écartez-vous de sa ligne.'),
    (New-Hit 94825 'Moulinet' 6533 $physical `
        'Ses chaînes se sont arrêtées sur vous. Regardez où elles s''arrêtent.'),
    (New-Hit 94826 'Pistage' 1130 $physical `
        'Votre piste a été prise. Quittez la marque quand elle se fige.'),

    # --- The debuffs ---
    (New-Debuff 94820 'Lacération' 845 10 `
        'Les tailles du Traqueur.' 'Dégâts physiques subis augmentés. Laissez-le à l''autre tank.'),
    (New-Debuff 94821 'Débusqué' 1130 1 `
        'Débusqué par une erreur.' 'Dégâts subis augmentés.'),
    (New-Debuff 94822 'Proie' 1130 1 `
        'Le Traqueur vous a désigné.' 'La meute bondira sur vous : éloignez-vous du groupe.'),
    (New-Debuff 94823 'Mâchoires' 1499 1 `
        'Pris dans un piège.' 'Immobilisé.'),

    # --- His cast bars ---
    (New-Cast 94840 'Taillade' 845 16 'Le Traqueur taille devant lui, puis derrière.'),
    (New-Cast 94841 'Cor de chasse' 23154 4 'Le Traqueur sonne la meute.'),
    (New-Cast 94842 'Battue' 5246 5 'Les rabatteurs entrent.'),
    (New-Cast 94843 'Hallali' 1130 15 'La meute va bondir sur ses proies.'),
    (New-Cast 94844 'Traque' 58984 5 'Le Traqueur disparaît dans le noir.'),
    (New-Cast 94845 'Curée' 34026 171 'La curée : la proie doit être aux abois.'),
    (New-Cast 94846 'Bond du Traqueur' 6533 5 'Le Traqueur va bondir sur sa proie.'),
    (New-Cast 94847 'Collet' 1499 16 'Le collet se referme.'),
    (New-Cast 94848 'Encerclement' 3034 16 'Les rabatteurs encerclent la salle.'),
    (New-Cast 94849 'Rabattage' 5246 16 'Les rabatteurs balaient les couloirs.'),
    (New-Cast 94850 'Charge de la meute' 1130 16 'La meute charge.'),
    (New-Cast 94851 'Pièges à mâchoires' 1499 16 'Le Traqueur sème ses pièges.'),
    (New-Cast 94852 'Moulinet' 6533 16 'Le Traqueur fait tournoyer ses chaînes.'),
    (New-Cast 94853 'Pistage' 1130 16 'Le Traqueur prend vos pistes.')
)

return $spells
