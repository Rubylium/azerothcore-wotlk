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
    # --- What hits ---
    (New-Hit 94800 'Taillade' 845 $physical `
        'Le Traqueur taille devant lui. Hors du cône si vous ne le tenez pas.'),
    (New-Hit 94801 'Cor de chasse' 23154 $physical `
        'Le cor du Traqueur lâche sa meute et frappe tous les évadés.'),
    (New-Hit 94802 'Étranglement' 6533 $physical `
        'Le filet s''est resserré : le gangrechien qui le tenait n''est pas mort à temps.'),
    (New-Hit 94803 'Piège à mâchoires' 1499 $physical `
        'Vous avez marché dans un piège armé. Contournez-les.'),
    (New-Hit 94804 'Flèche empoisonnée' 3034 $nature `
        'Un rabatteur archer a tiré. Interrompez-le, ou tuez-le.'),
    (New-Hit 94805 'Battue' 5246 $physical `
        'Les rabatteurs ont passé votre rangée. Tenez-vous dans sa trouée.'),
    (New-Hit 94806 'Hallali' 1130 $physical `
        'La meute a bondi sur la proie : éloignez-vous des proies.'),
    (New-Hit 94807 'Curée de meute' 1130 $physical `
        'La meute s''abat sur le groupe : ensemble, le coup se partage.'),
    (New-Hit 94808 'Traque' 58984 $shadow `
        'Le Traqueur vous a trouvé hors de la lumière des torches.'),
    (New-Hit 94809 'Curée' 34026 $physical `
        'La proie n''était pas aux abois : le Traqueur avait encore trop de forces.'),
    (New-Hit 94810 'Fin de la traque' 26662 $shadow `
        'La traque est finie, et vous aussi.'),

    # --- The debuffs ---
    (New-Debuff 94820 'Lacération' 845 10 `
        'Les tailles du Traqueur.' 'Dégâts physiques subis augmentés. Laissez-le à l''autre tank.'),
    (New-Debuff 94821 'Débusqué' 1130 1 `
        'Débusqué par une erreur.' 'Dégâts subis augmentés : une seconde erreur est mortelle.'),
    (New-Debuff 94822 'Proie' 1130 1 `
        'Le Traqueur vous a désigné.' 'La meute bondira sur vous : éloignez-vous du groupe.'),
    (New-Debuff 94823 'Mâchoires' 1499 1 `
        'Pris dans un piège.' 'Immobilisé.'),
    (New-Debuff 94824 'Filet' 6533 1 `
        'Pris dans un filet.' 'Immobilisé. Tuez le gangrechien qui tient le filet.'),

    # --- His cast bars ---
    (New-Cast 94840 'Taillade' 845 5 'Le Traqueur taille devant lui.'),
    (New-Cast 94841 'Cor de chasse' 23154 4 'Le Traqueur sonne la meute.'),
    (New-Cast 94842 'Battue' 5246 5 'Les rabatteurs entrent.'),
    (New-Cast 94843 'Hallali' 1130 15 'La meute va bondir sur ses proies.'),
    (New-Cast 94844 'Traque' 58984 5 'Les torches s''éteignent.'),
    (New-Cast 94845 'Curée' 34026 171 'La curée : la proie doit être aux abois.'),

    # --- The archer's arrow: a real cast at a player (4 s, interruptible as Smite, any range); it lands through the
    # script (the arrow's hit, 94804) ---
    @{ Id = 94846; Clone = 585; Name = 'Flèche empoisonnée'; FallbackIconSpell = 3034; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un rabatteur archer vise un évadé. Interrompez-le.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 })
       Fields = @{ 28 = 15; 46 = 13; 131 = 0; 213 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
)

return $spells
