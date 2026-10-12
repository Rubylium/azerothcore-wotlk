# Valérian, le Zélote's spell data (modules/mod-stat-growth/src/ZealotBoss.cpp, the Hellfire Gaol's third gate; plan:
# .agents/plans/gaol-third). Ids 94630-94679.
#
# As the Gaol's other gates (localTools/escapeHunter/Spells.ps1): every hit a spell of its own whose description says
# what went wrong (the combat log and the death recap: nothing is written on the screen in the fight), the debuffs
# dummy auras timed and stacked by the script, his casts a dummy on himself as long as the step takes to come, so the
# players read it coming on his cast bar. Stock icons (a boss's own icons are never painted). One exception: the light
# a player steals from him is a real buff (3% damage and healing done a stack).

$physical = 1
$holy = 2
$debuff = 0x04000000

# His looks imported from the Ascension client (localTools/valerian/ascensionVisuals.json)
$valerianImported = Get-Content -LiteralPath (Join-Path $repoRoot `
    'modules\mod-stat-growth\client-assets\importedValerian\visuals.json') -Raw -Encoding UTF8 | ConvertFrom-Json
function ValerianLook([string]$key) {
    $id = $valerianImported.ids.$key
    if (-not $id) { throw "No imported Valerian look '$key' (localTools\valerian\ascensionVisuals.json)." }
    return [uint32]$id
}

# La Litanie's spears in flight: a dummy at a point (87: TARGET_DEST_DEST, anywhere: range 13), its missile flying at
# 45 yd/s (field 47, a float: ZealotBoss.cpp's LitanySpearSpeed) - only a look, the script strikes
$spearSpeed = [BitConverter]::ToUInt32([BitConverter]::GetBytes([single]45), 0)
function New-Missile($id, $name, $look, $description) {
    @{ Id = $id; Clone = 585; Name = $name; FallbackIconSpell = 48782; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 87; BasePoints = 0 })
       Fields = @{ 28 = 1; 46 = 13; 47 = $spearSpeed; 131 = (ValerianLook $look); 213 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
}

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
    (New-Hit 94630 'Revers sacré' 53595 $holy `
        "Le Zélote frappe devant lui, puis derrière. Ne restez pas dans le cône, ni dans celui qui suit."),
    (New-Hit 94631 'Colère du juste' 48806 $holy `
        "La moitié de l'arène que son poing désigne brûle. Le poing qu'il relève dit où frappe la suivante."),
    (New-Hit 94632 'Chaîne brisée' 25203 $physical `
        "Sa chaîne balaie tout autour de lui, sauf une brèche. Tenez-vous dans la brèche, puis suivez-la."),
    (New-Hit 94633 'Ferveur' 48819 $holy `
        "Une marque explose sur son porteur et ceux qui l'entourent. Écartez-vous des marques."),
    (New-Hit 94634 'Ferveur partagée' 48819 $holy `
        "La marque s'est retournée : partagée par ceux qui l'entourent. Seul, elle tue."),
    (New-Hit 94635 'Rage contenue' 31884 $holy `
        "Sa rage éclate près de lui puis au loin, ou l'inverse. Son éclat avant le coup dit l'ordre."),
    (New-Hit 94636 'Lances de lumière' 48782 $holy `
        "Des lances de mur à mur, en quadrillage, qui se décalent d'une demi-case. Changez de case."),
    (New-Hit 94637 'Mur de lumière' 642 $holy `
        "Le mur de l'arène brûle quiconque le touche."),
    (New-Hit 94638 'Éclat de zèle' 31884 $holy `
        "Le Zélote libère sa ferveur sur tout le groupe."),
    (New-Hit 94639 'Lumière aveuglante' 642 $holy `
        "La Lumière a submergé l'arène : le Zélote n'avait pas assez faibli."),
    (New-Hit 94640 "Jugement de l'Ascensionné" 20271 $holy `
        "Devenu dieu, il abat toute sa lumière sur l'arène : chacun n'en garde qu'un souffle."),
    (New-Hit 94643 'Verset' 48782 $holy `
        "La lance de la Litanie frappe le verset qui portait son numéro."),
    (New-Hit 94644 'Profanation' 642 $holy `
        "Une lance de la Litanie a frappé un autre verset que le sien : la lumière a tout emporté."),
    (New-Missile 94645 'Lance de la Litanie' 'VerseSpear' "La lance de la Litanie, en vol."),
    (New-Missile 94646 'Foudre de la Litanie' 'VerseLightning' "La foudre dorée qui suit la lance."),
    (New-Hit 94642 'Éclats de lumière' 48782 $holy `
        "Pendant qu'il s'élève, des éclats de lumière tombent dans l'arène. Sortez de leur cercle."),
    (New-Hit 94641 'Lumière rendue' 48782 $holy `
        "Une lumière laissée au Zélote : il la reprend, et tout le groupe en est frappé."),

    # --- The debuffs ---
    @{ Id = 94650; Clone = 2983; Name = 'Lumière dérobée'; FallbackIconSpell = 48782; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 99; Spellbook = $false
       Description = "Une lumière prise au Zélote."
       AuraDescription = 'Dégâts et soins augmentés de 3% par charge.'
       Effects = @(
           @{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 3; Misc = 127 },
           @{ Index = 1; Effect = 6; Aura = 136; TargetA = 1; Value = 3 })
       Fields = @{ 40 = 21 } },
    (New-Debuff 94651 'Ébloui' 20066 1 `
        "Une erreur : la prochaine sera mortelle." "Une seconde erreur est mortelle."),
    (New-Debuff 94652 'Zèle brûlant' 53595 10 `
        "Les revers du Zélote." "Dégâts subis augmentés. Laissez-le à l'autre tank."),
    (New-Debuff 94653 'Marque de ferveur' 48819 1 `
        "Sa ferveur va exploser sur vous." "Sa ferveur va exploser sur vous : écoutez-la."),
    # His guard down after a ward (La Litanie): a real buff, 50% damage done, timed by the script (15 s)
    @{ Id = 94654; Clone = 2983; Name = 'Garde brisée'; FallbackIconSpell = 31884; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 1; Spellbook = $false
       Description = "La litanie achevée, la garde du Zélote tombe."
       AuraDescription = 'Dégâts infligés augmentés de 50%.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 79; TargetA = 1; Value = 50; Misc = 127 })
       Fields = @{ 40 = 21 } },
    # His ward while he recites (Divine Shield's look and immunity, nothing a dispel takes): the script puts it on and off
    @{ Id = 94669; Clone = 642; Name = 'Litanie'; FallbackIconSpell = 642; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Le Zélote récite sa litanie : rien ne l'atteint."
       AuraDescription = "Rien ne l'atteint tant qu'il récite."
       Effects = @(@{ Index = 0; Effect = 6; Aura = 39; TargetA = 1; Misc = 127 })
       Fields = @{ 2 = 0; 3 = 0; 40 = 21 } },

    # --- His cast bars ---
    (New-Cast 94660 'Revers sacré' 53595 16 "Le Zélote arme son revers."),
    (New-Cast 94661 'Colère du juste' 48806 16 "Le Zélote lève le poing."),
    (New-Cast 94662 'Chaîne brisée' 25203 5 "Le Zélote fait tourner sa chaîne."),
    (New-Cast 94663 'Ferveur' 48819 5 "Le Zélote marque ses fidèles."),
    (New-Cast 94664 'Rage contenue' 31884 16 "La rage du Zélote monte."),
    (New-Cast 94665 'Lances de lumière' 48782 16 "Des lances de lumière se forment."),
    (New-Cast 94666 'Éclat de zèle' 31884 4 "La ferveur du Zélote déborde."),
    (New-Cast 94667 'Jugement' 20271 171 "La Lumière va rendre son verdict."),
    (New-Cast 94668 'Litanie' 48782 171 "Le Zélote récite sa litanie : chacun son verset, chacun sa place.")
)

return $spells
