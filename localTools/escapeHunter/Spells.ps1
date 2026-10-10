# Vrogar, Maître-fondeur de la Geôle's spell data (modules/mod-stat-growth/src/ForgeMaster.cpp, the Hellfire Gaol's
# second gate; plan: .agents/plans/escape-hunter, "The fight v3"). Ids 94800-94859.
#
# As Gardien-chef Vorhan's (localTools/wardenVorhan/Spells.ps1): every hit a spell of its own whose description says
# what went wrong (the combat log and the death recap: nothing is written on the screen in the fight), the debuffs
# dummy auras timed and stacked by the script, his casts a dummy on himself as long as the step takes to come, so the
# players read it coming on his cast bar. Stock icons (a boss's own icons are never painted).

$physical = 1
$fire = 4
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
    (New-Hit 94800 "Coup d'enclume" 25203 $physical `
        "Le Maître-fondeur abat son marteau devant lui. Hors du cône si vous ne le tenez pas."),
    (New-Hit 94801 "Frappe de l'enclume" 16244 $physical `
        "Le marteau du Maître-fondeur frappe l'enclume, et toute l'arène."),
    (New-Hit 94802 "Onde de l'enclume" 25203 $fire `
        "L'onde de son coup court du centre au mur. Entrez dans l'anneau qui vient de frapper."),
    (New-Hit 94803 'Coulée' 11366 $fire `
        "La fonte a couvert votre moitié de l'arène. Traversez avant qu'elle coule."),
    (New-Hit 94804 'Étincelles' 2120 $fire `
        "Une étincelle a jailli sous vous. Sortez du cercle."),
    (New-Hit 94805 'Chaînes' 6533 $physical `
        "Ses chaînes se sont arrêtées sur vous. Regardez où elles se figent."),
    (New-Hit 94806 'Laminoir' 5246 $physical `
        "Les rouleaux ont passé votre rangée. Tenez-vous dans sa trouée."),
    (New-Hit 94807 'Vapeur' 10 $fire `
        "Il a trempé son marteau dans le bac : la vapeur jaillit au centre."),
    (New-Hit 94808 'Coulée de fonte' 11366 $fire `
        "Il a trempé son marteau dans le creuset : la fonte inonde le bord de l'arène."),
    (New-Hit 94809 'Souffle du soufflet' 2120 $fire `
        "Le soufflet a soufflé sa moitié de l'arène. Le quart opposé aux deux soufflets est épargné."),
    (New-Hit 94810 'Lingot chu' 25203 $fire `
        "Un lingot est tombé sans être tenu par deux."),
    (New-Hit 94811 'Lingot écrasant' 25203 $fire `
        "Trois sous le même lingot : il écrase tout le monde."),
    (New-Hit 94812 'Lingot tenu' 25203 $fire `
        "Le poids d'un lingot, porté à deux."),
    (New-Hit 94813 'Marteau' 25203 $physical `
        "Le marteau tombe sur le fer rouge, et sur qui se tient près de lui."),
    (New-Hit 94814 'Marteau partagé' 25203 $physical `
        "Son marteau sur le groupe : ensemble sous lui, le coup se partage. Seul, il tue."),
    (New-Hit 94815 'Fonte' 11366 $fire `
        "Le sol de l'arène a fondu sous vous. Suivez les deux parts épargnées."),
    (New-Hit 94816 'Retour de la fournaise' 2120 $fire `
        "Le Maître-fondeur sort de sa fournaise."),
    (New-Hit 94817 'Coulée finale' 11366 $fire `
        "Le moule n'était pas prêt : la fonte a tout recouvert."),
    (New-Hit 94818 'Mur de fonte' 11366 $fire `
        "Le mur de l'arène est de fonte : on ne le traverse pas."),
    (New-Hit 94819 'Gerbe de fonte' 11366 $fire `
        "La fonte a jailli de quelqu'un près de vous. Écartez-vous de ceux qui la portent."),
    (New-Hit 94824 'Coulée partagée' 11366 $fire `
        "Une coulée sur un soigneur, partagée par ceux qui l'entourent. Seul, elle tue."),

    # --- The debuffs ---
    (New-Debuff 94820 'Brûlure' 25203 10 `
        "Les coups d'enclume du Maître-fondeur." "Dégâts subis augmentés. Laissez-le à l'autre tank."),
    (New-Debuff 94821 'Ébouillanté' 11366 1 `
        "Une erreur : la prochaine sera mortelle." "Une seconde erreur est mortelle."),
    (New-Debuff 94822 'Fer rouge' 25203 1 `
        "Son marteau va tomber sur vous." "Son marteau va tomber sur vous : éloignez-vous de tous."),
    (New-Debuff 94823 'Métal en fusion' 11366 1 `
        "Une coulée partagée vous a touché." "Une autre coulée partagée vous frappe deux fois plus fort."),

    # --- His cast bars ---
    (New-Cast 94840 "Coup d'enclume" 25203 152 "Le Maître-fondeur lève son marteau."),
    (New-Cast 94841 "Frappe de l'enclume" 16244 4 "Le marteau va frapper l'enclume."),
    (New-Cast 94842 'Laminoir' 5246 152 "Les rouleaux se mettent en marche."),
    (New-Cast 94843 'Marteau' 25203 15 "Le Maître-fondeur marque le fer rouge."),
    (New-Cast 94844 'Fonte' 11366 5 "Le Maître-fondeur rentre dans sa fournaise."),
    (New-Cast 94845 'Coulée finale' 11366 171 "Le moule va se remplir."),
    (New-Cast 94846 'Trempe' 10 4 "Le Maître-fondeur trempe son marteau."),
    (New-Cast 94847 'Soufflets' 2120 15 "Les soufflets s'emballent."),
    (New-Cast 94848 'Lingots' 25203 15 "Les lingots vont tomber."),
    (New-Cast 94849 'Coulée' 11366 152 "La fonte va couler."),
    (New-Cast 94850 'Chaînes' 6533 5 "Le Maître-fondeur fait tournoyer ses chaînes."),
    (New-Cast 94851 'Étincelles' 2120 152 "Les étincelles vont jaillir.")
)

return $spells
