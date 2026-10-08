# Gardien-chef Vorhan's spell data (modules/mod-stat-growth/src/WardenVorhan.cpp, the Défi board's prison warden;
# plan: .agents/plans/warden-vorhan). Ids 94400-94451; 94200-94249 are its painted ground marks
# (localTools/groundIndicators/shapes.json).
#
# The fight explains itself: every rule shows on the players as a debuff, and every hit is a spell of its own whose
# description says what went wrong - the combat log and Details' death recap read as the warden's rules.
# - What hits (94400-94414): never cast; the script deals its damage on the area it drew
#   (MythicTuning::DealAbilityDamage) or kills with it, naming the spell. Copies of Cosmic Smash, school set (field
#   225: 1 physical, 4 fire, 8 nature, 32 shadow).
# - The debuffs (94420-94437): dummy auras, stacked and timed by the script, shown as debuffs (field 4).
# - The cast bars (94440-94449): what the warden casts, a dummy on himself with the cast time the rule takes (field 28,
#   SpellCastTimes: 4 = 1 s, 16 = 1.5 s, 5 = 2 s, 15 = 4 s, 171 = 6 s, 170 = 8 s), so the players read the rule coming
#   on his cast bar. The script resolves the rule itself at the end, whatever the bar.
# - The shackles' chain (94450): a beam between two chained players (Shackle, 62646: Ulduar's chain).
# Icons: the painted ones (localTools/wardenVorhan/art, compiled into client-assets/compiled) once they exist, a stock
# one until then.

$fire = 4
$nature = 8
$shadow = 32
$physical = 1
$debuff = 0x04000000

# A hit, never cast: its name and text in the combat log and the death recap
function New-Hit($id, $name, $icon, $fallback, $school, $description) {
    @{ Id = $id; Clone = 64596; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Fields = @{ 225 = $school } }
}

# A debuff the script puts on and times (40: its duration row, the script sets the real one)
function New-Debuff($id, $name, $icon, $fallback, $duration, $maxStacks, $description, $auraDescription) {
    @{ Id = $id; Clone = 2983; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = $maxStacks; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Fields = @{ 4 = $debuff; 40 = $duration } }
}

# A rule announced on the warden's cast bar: a dummy on himself, cast for as long as the rule takes to come
function New-Cast($id, $name, $icon, $fallback, $castTime, $description) {
    @{ Id = $id; Clone = 585; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = @{ 28 = $castTime; 131 = 0; 213 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
}

$spells = @(
    # --- What hits ---
    (New-Hit 94400 'Sentence' 'ICON_Sentence' 589 $shadow `
        'Le gardien-chef prononce sa sentence sur tous les détenus.'),
    (New-Hit 94401 "Mise à l'isolement" 'ICON_MiseIsolement' 33813 $fire `
        "Un coup qui projette sa cible loin du gardien-chef et la place à l'isolement."),
    (New-Hit 94402 "Sceau d'isolement" 'ICON_SceauIsolement' 30616 $fire `
        "Le sceau d'un détenu à l'isolement a explosé près de vous. Laissez-le seul."),
    (New-Hit 94403 'Évasion' 'ICON_Evasion' 31458 $shadow `
        'Une cellule est restée vide quand les portes se sont fermées. Chaque cellule attend son détenu.'),
    (New-Hit 94404 'Cellule surpeuplée' 'ICON_CelluleSurpeuplee' 33813 $physical `
        'Deux détenus dans la même cellule. Une cellule, un détenu : la vôtre porte votre numéro.'),
    (New-Hit 94405 'Hors cellule' 'ICON_HorsCellule' 33813 $shadow `
        "Vous n'étiez dans aucune cellule quand les portes se sont fermées."),
    (New-Hit 94406 'Menottes' 'ICON_Menottes' 38505 $shadow `
        'La chaîne s''est resserrée. Éloignez-vous de votre codétenu pour la briser.'),
    (New-Hit 94407 'Regard du geôlier' 'ICON_Regard' 30616 $fire `
        "Vous faisiez face à l'œil du gardien-chef quand il s'est ouvert. Tournez-lui le dos."),
    (New-Hit 94408 "Absent à l'appel" 'ICON_Appel' 589 $shadow `
        'Vous étiez seul sur votre marque, ou sur aucune. Chaque marque attend exactement ses deux matricules.'),
    (New-Hit 94409 'Rassemblement interdit' 'ICON_Appel' 589 $shadow `
        'Plus de deux détenus sur une même marque.'),
    (New-Hit 94410 'Mauvais matricule' 'ICON_MauvaisMatricule' 589 $shadow `
        "Vous vous teniez sur la marque d'un autre matricule. Lisez les numéros au sol."),
    (New-Hit 94411 'Barrière électrifiée' 'ICON_Barriere' 421 $nature `
        'Projeté contre la barrière. Avant la charge, rapprochez-vous du gardien-chef.'),
    (New-Hit 94412 'Violation du couvre-feu' 'ICON_ViolationCouvreFeu' 589 $shadow `
        'Vous bougiez quand le couvre-feu a sonné.'),
    (New-Hit 94413 'Perpétuité' 'ICON_Perpetuite' 589 $shadow `
        "La peine s'alourdit à chaque instant."),
    (New-Hit 94414 'Peine capitale' 'ICON_PeineCapitale' 589 $shadow `
        'Le temps est écoulé.'),

    # --- The debuffs ---
    (New-Debuff 94420 'Marque du geôlier' 'ICON_MarqueGeolier' 30616 1 1 `
        'Le gardien-chef vous a pris en faute.' `
        'Dégâts subis augmentés de 50 %. Une nouvelle faute vous sera fatale.'),
    (New-Debuff 94421 'Isolement' 'ICON_Isolement' 30616 1 1 `
        "Vous êtes à l'isolement." `
        "Le sceau explose sur quiconque se tient à moins de 10 m de vous à la fin de ce temps : tenez-vous à l'écart."),
    (New-Debuff 94422 'Menottes' 'ICON_Menottes' 38505 1 1 `
        'Enchaîné à un autre détenu.' `
        'La chaîne se resserre à chaque seconde : éloignez-vous à plus de 20 m de votre codétenu pour la briser.'),
    (New-Debuff 94423 'Couvre-feu' 'ICON_CouvreFeu' 589 1 1 `
        'Le couvre-feu va sonner.' `
        'Le couvre-feu sonne à la fin de ce temps : quiconque bouge encore à ce moment-là sera puni.'),
    (New-Debuff 94424 'Évasion' 'ICON_Evasion' 31458 1 8 `
        "Un détenu s'est échappé." `
        'Une cellule est restée vide. Dégâts subis augmentés de 25 % par charge.'),
    # On the warden: his eye opening, the time it has left
    @{ Id = 94425; Clone = 2983; Name = 'Œil entrouvert'; Icon = 'ICON_Regard'; FallbackIconSpell = 30616; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = "L'œil du gardien-chef s'ouvre."
       AuraDescription = 'Ne lui faites pas face quand il sera grand ouvert.'
       Fields = @{ 40 = 1 } },
    # Curfew broken: stunned 3 sec (Stun's own row, 56)
    @{ Id = 94426; Clone = 56; Name = 'Couvre-feu violé'; Icon = 'ICON_ViolationCouvreFeu'; FallbackIconSpell = 33912; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Vous bougiez quand le couvre-feu a sonné.'
       AuraDescription = 'Étourdi.' },
    # The inmate numbers, for the whole fight
    (New-Debuff 94430 'Matricule 1' 'ICON_Matricule1' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 1 : votre cellule (au nord), votre marque à l''Appel nominal (avec le 2) ; enchaîné au 5.'),
    (New-Debuff 94431 'Matricule 2' 'ICON_Matricule2' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 2 : votre cellule (au nord-est), votre marque à l''Appel nominal (avec le 1) ; enchaîné au 6.'),
    (New-Debuff 94432 'Matricule 3' 'ICON_Matricule3' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 3 : votre cellule (à l''est), votre marque à l''Appel nominal (avec le 4) ; enchaîné au 7.'),
    (New-Debuff 94433 'Matricule 4' 'ICON_Matricule4' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 4 : votre cellule (au sud-est), votre marque à l''Appel nominal (avec le 3) ; enchaîné au 8.'),
    (New-Debuff 94434 'Matricule 5' 'ICON_Matricule5' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 5 : votre cellule (au sud), votre marque à l''Appel nominal (avec le 6) ; enchaîné au 1.'),
    (New-Debuff 94435 'Matricule 6' 'ICON_Matricule6' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 6 : votre cellule (au sud-ouest), votre marque à l''Appel nominal (avec le 5) ; enchaîné au 2.'),
    (New-Debuff 94436 'Matricule 7' 'ICON_Matricule7' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 7 : votre cellule (à l''ouest), votre marque à l''Appel nominal (avec le 8) ; enchaîné au 3.'),
    (New-Debuff 94437 'Matricule 8' 'ICON_Matricule8' 38505 21 1 `
        'Votre numéro de détenu.' `
        'Matricule 8 : votre cellule (au nord-ouest), votre marque à l''Appel nominal (avec le 7) ; enchaîné au 4.'),

    # --- The cast bars ---
    (New-Cast 94440 'Sentence' 'ICON_Sentence' 589 5 'Le gardien-chef prononce sa sentence.'),
    (New-Cast 94441 "Mise à l'isolement" 'ICON_MiseIsolement' 33813 16 "Le gardien-chef s'apprête à frapper son gardien."),
    (New-Cast 94442 'Cellules' 'ICON_HorsCellule' 33813 170 'Les cellules vont se refermer : chacun dans la sienne.'),
    (New-Cast 94443 'Menottes' 'ICON_Menottes' 38505 4 'Le gardien-chef enchaîne les détenus deux à deux.'),
    (New-Cast 94444 'Regard du geôlier' 'ICON_Regard' 30616 171 "L'œil du gardien-chef s'ouvre : tournez-lui le dos."),
    (New-Cast 94445 'Appel nominal' 'ICON_Appel' 589 171 'Chaque paire de matricules à sa marque.'),
    (New-Cast 94446 'Charge du geôlier' 'ICON_Barriere' 421 15 'Les barrières s''allument : rapprochez-vous du gardien-chef.'),
    (New-Cast 94447 'Couvre-feu' 'ICON_CouvreFeu' 589 4 'Le couvre-feu va sonner.'),
    (New-Cast 94448 'Mutinerie' 'ICON_Evasion' 31458 5 'Les portes des cellules cèdent.'),
    (New-Cast 94449 'Perpétuité' 'ICON_Perpetuite' 589 5 'La peine ne finira plus.'),

    # --- The shackles' chain: a beam from one chained player to the other (cast by one on the other: any target,
    # any range, a dummy aura) ---
    @{ Id = 94450; Clone = 62646; Name = 'Menottes'; Icon = 'ICON_Menottes'; FallbackIconSpell = 38505; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La chaîne qui lie deux détenus.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 25; Aura = 4; BasePoints = 0 })
       Fields = @{ 40 = 21; 46 = 13; 28 = 1; 213 = 0 } },
    # --- A cell's door: the bars of a cage round its stalker as the doors slam (Encaged Emberseer's cage, 15282) ---
    @{ Id = 94451; Clone = 15282; Name = 'Cellule'; Icon = 'ICON_HorsCellule'; FallbackIconSpell = 38505; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une cellule de la Geôle.'
       Fields = @{ 40 = 21 } }
)

return $spells
