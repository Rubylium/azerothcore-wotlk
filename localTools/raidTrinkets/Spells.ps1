# The raid trinkets' spell data (modules/mod-stat-growth/src/RaidTrinkets.cpp): eight trinkets of the Hollow Voice
# (items 17836-17843, item level 477) and eight of Gardien-chef Vorhan (17844-17848, 17851-17853, item level 485), one
# passive and one active for each role - a fighter, a caster, a healer, a tank. Ids 94600-94623. Items and procs:
# data/sql/updates/pending_db_world (item_template, spell_proc).
#
# What a trinket gives is sized as the raid's generated gear grows (power-scaling.md): a stock trinket's at item level
# 284 (the Ruby Sanctum's and Icecrown's heroic ones), its power and stats times the item level ratio (477/284 = 1.68,
# 485/284 = 1.71), its ratings times that ratio's square root (1.30, 1.31). What lasts a share of health or of damage
# needs no sizing.
# - An equip spell (a passive): a stock trinket's own proc row, hidden, its flags and its proc here; its chance and its
#   cooldown are spell_proc's. A buff it triggers: a stock trinket's buff, its amounts here.
# - A use spell (an active): a stock trinket's use, its amounts here; its 2 min are the item's.
# Every spell wears its trinket's icon. Duration indexes (SpellDuration): 31 = 8 s, 1 = 10 s, 29 = 12 s, 8 = 15 s,
# 18 = 20 s.

# Proc flags: dealt by a weapon (melee, ranged, their abilities), by a harmful or a helpful spell, taken from a blow
$weaponBlows = 0x154
$harmfulSpells = 0x10000
$helpfulSpells = 0x4000
$blowsTaken = 0x28

# Combat ratings (aura 189's mask): crit, haste (melee, ranged and spells), dodge
$critRating = 0x700
$hasteRating = 0xE0000
$dodgeRating = 0x4

function New-AttackPower($value) {
    @(@{ Index = 0; Effect = 6; Aura = 99; TargetA = 1; Value = $value },
      @{ Index = 1; Effect = 6; Aura = 124; TargetA = 1; Value = $value })
}

function New-SpellPower($value) {
    @(@{ Index = 0; Effect = 6; Aura = 13; TargetA = 1; Misc = 126; Value = $value },
      @{ Index = 1; Effect = 6; Aura = 135; TargetA = 1; Misc = 126; Value = $value })
}

# One effect: returned as an array of one (the comma), or PowerShell hands back the bare table
function New-Rating($mask, $value) {
    , @(@{ Index = 0; Effect = 6; Aura = 189; TargetA = 1; Misc = $mask; Value = $value })
}

# One aura with an amount: 87 damage taken (all schools), 133 maximum health, 136 healing done, 65 casting speed
function New-Aura($aura, $value) {
    , @(@{ Index = 0; Effect = 6; Aura = $aura; TargetA = 1; Misc = $(if ($aura -eq 87) { 127 } else { 0 }); Value = $value })
}

# A passive: a hidden equip aura that triggers $trigger when $procFlags happen (Sharpened Twilight Scale's row)
function New-Passive($id, $name, $icon, $procFlags, $trigger, $description) {
    @{ Id = $id; Clone = 75457; Name = $name; IconPath = "Interface\Icons\$icon"; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 6; Aura = 42; TargetA = 1; BasePoints = 0 })
       Fields = @{ 34 = $procFlags; 35 = 100; 116 = $trigger; 208 = 0 } }
}

# What a passive triggers: a buff on the wearer, of $stacks stacks at most
function New-Buff($id, $clone, $name, $icon, $duration, $effects, $auraDescription, $stacks = 1) {
    @{ Id = $id; Clone = $clone; Name = $name; IconPath = "Interface\Icons\$icon"; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $auraDescription
       AuraDescription = $auraDescription
       Effects = $effects
       Fields = @{ 40 = $duration; 49 = $stacks; 208 = 0 } }
}

# An active: the trinket's use, a buff on the wearer (Satrina's Impeding Scarab's use row), 2 min
function New-Active($id, $name, $icon, $duration, $effects, $description, $auraDescription) {
    @{ Id = $id; Clone = 67753; Name = $name; IconPath = "Interface\Icons\$icon"; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Effects = $effects
       Fields = @{ 40 = $duration; 208 = 0 } }
}

$spells = @(
    # --- The Hollow Voice (item level 477) ---
    # Éclat du Marteau béni: 1472 attack power for 15 s (Sharpened Twilight Scale), x1.68
    (New-Passive 94600 'Éclat du Marteau béni' 'Spell_Holy_SealOfSacrifice' $weaponBlows 94601 `
        "Vos attaques en mêlée et à distance ont une chance de vous conférer 2472 points de puissance d'attaque pendant 15 sec.")
    (New-Buff 94601 75456 'Martèlement sacré' 'Spell_Holy_SealOfSacrifice' 8 (New-AttackPower 2472) `
        "Puissance d'attaque augmentée de 2472.")
    # Penne du Séraphin: 600 haste rating for 20 s, x1.30
    (New-Active 94602 'Envol séraphique' 'INV_Feather_06' 18 (New-Rating $hasteRating 778) `
        'Augmente votre score de hâte de 778 pendant 20 sec.' 'Score de hâte augmenté de 778.')
    # Psautier du Néant: 861 spell power for 15 s (Charred Twilight Scale), x1.68
    (New-Passive 94603 'Psautier du Néant' 'INV_Misc_Book_13' $harmfulSpells 94604 `
        'Vos sorts de dégâts ont une chance de vous conférer 1446 points de puissance des sorts pendant 15 sec.')
    (New-Buff 94604 75473 'Psaume creux' 'INV_Misc_Book_13' 8 (New-SpellPower 1446) `
        'Puissance des sorts augmentée de 1446.')
    # Souffle du Néant: 770 spell power for 20 s (Maghia's Misguided Quill at 284), x1.68
    (New-Active 94605 'Souffle du Néant' 'Spell_Shadow_BurningSpirit' 18 (New-SpellPower 1293) `
        'Augmente votre puissance des sorts de 1293 pendant 20 sec.' 'Puissance des sorts augmentée de 1293.')
    # Chapelet de l'Archevêque: as the Psautier, set off by heals
    (New-Passive 94606 "Chapelet de l'Archevêque" 'INV_Jewelry_Necklace_31' $helpfulSpells 94607 `
        'Vos sorts de soins ont une chance de vous conférer 1446 points de puissance des sorts pendant 15 sec.')
    (New-Buff 94607 75473 "Dévotion de l'Archevêque" 'INV_Jewelry_Necklace_31' 8 (New-SpellPower 1446) `
        'Puissance des sorts augmentée de 1446.')
    # Reliquaire d'Aldric: 20% more healing for 15 s
    (New-Active 94608 'Absolution' 'Spell_Holy_SummonLightwell' 8 (New-Aura 136 20) `
        'Augmente les soins que vous prodiguez de 20% pendant 15 sec.' 'Soins prodigués augmentés de 20%.')
    # Pierre du Bastion: 15% less damage taken for 10 s
    (New-Passive 94609 'Pierre du Bastion' 'Spell_Holy_BlessingOfProtection' $blowsTaken 94610 `
        'Les attaques en mêlée qui vous touchent ont une chance de réduire les dégâts que vous subissez de 15% pendant 10 sec.')
    (New-Buff 94610 75480 'Bastion' 'Spell_Holy_BlessingOfProtection' 1 (New-Aura 87 -15) `
        'Dégâts subis réduits de 15%.')
    # Cierge de la Dernière lumière: 25% more health for 15 s
    (New-Active 94611 'Abri de lumière' 'INV_Misc_Candle_01' 8 (New-Aura 133 25) `
        'Augmente vos points de vie maximum de 25% pendant 15 sec.' 'Points de vie maximum augmentés de 25%.')

    # --- Gardien-chef Vorhan (item level 485) ---
    # Pierre à aiguiser du bourreau: every blow a stack, 50 attack power each, x1.71, up to 10, for 10 s
    (New-Passive 94612 'Pierre à aiguiser du bourreau' 'INV_Stone_WeightStone_05' $weaponBlows 94613 `
        "Chacune de vos attaques en mêlée et à distance vous confère 85 points de puissance d'attaque pendant 10 sec. Cumulable jusqu'à 10 fois.")
    (New-Buff 94613 75456 'Fil du bourreau' 'INV_Stone_WeightStone_05' 1 (New-AttackPower 85) `
        "Puissance d'attaque augmentée de 85 par application." 10)
    # Cadran du couvre-feu: 1400 attack power for 20 s, x1.71
    (New-Active 94614 'Couvre-feu' 'INV_Misc_PocketWatch_01' 18 (New-AttackPower 2391) `
        "Augmente votre puissance d'attaque de 2391 pendant 20 sec." "Puissance d'attaque augmentée de 2391.")
    # Registre d'écrou: 900 haste rating for 10 s, x1.31
    (New-Passive 94615 "Registre d'écrou" 'INV_Misc_Book_11' $harmfulSpells 94616 `
        'Vos sorts de dégâts ont une chance d''augmenter votre score de hâte de 1176 pendant 10 sec.')
    (New-Buff 94616 75473 'Appel nominal' 'INV_Misc_Book_11' 1 (New-Rating $hasteRating 1176) `
        'Score de hâte augmenté de 1176.')
    # Œil du Gardien-chef: 20% faster casts for 20 s
    (New-Active 94617 'Regard du Gardien' 'INV_Misc_Eye_02' 18 (New-Aura 65 20) `
        "Augmente votre vitesse d'incantation de 20% pendant 20 sec." "Vitesse d'incantation augmentée de 20%.")
    # Lettre de grâce: every heal a stack, 30 spell power each, x1.71, up to 10, for 10 s
    (New-Passive 94618 'Lettre de grâce' 'INV_Scroll_08' $helpfulSpells 94619 `
        "Chacun de vos sorts de soins vous confère 51 points de puissance des sorts pendant 10 sec. Cumulable jusqu'à 10 fois.")
    (New-Buff 94619 75473 'Clémence' 'INV_Scroll_08' 1 (New-SpellPower 51) `
        'Puissance des sorts augmentée de 51 par application.' 10)
    # Tampon de libération: 770 spell power for 20 s, x1.71
    (New-Active 94620 'Libération conditionnelle' 'INV_Misc_Token_ArgentDawn2' 18 (New-SpellPower 1315) `
        'Augmente votre puissance des sorts de 1315 pendant 20 sec.' 'Puissance des sorts augmentée de 1315.')
    # Maillon des fers: every blow taken a stack, 1% less damage taken each, up to 8, for 8 s
    (New-Passive 94621 'Maillon des fers' 'INV_Jewelry_Ring_45' $blowsTaken 94622 `
        "Chaque attaque en mêlée qui vous touche réduit les dégâts que vous subissez de 1% pendant 8 sec. Cumulable jusqu'à 8 fois.")
    (New-Buff 94622 75480 'Fers rivés' 'INV_Jewelry_Ring_45' 31 (New-Aura 87 -1) `
        'Dégâts subis réduits de 1% par application.' 8)
    # Verrou du cachot: 25% less damage taken for 12 s
    (New-Active 94623 'Verrouillé' 'INV_Misc_Key_15' 29 (New-Aura 87 -25) `
        'Réduit les dégâts que vous subissez de 25% pendant 12 sec.' 'Dégâts subis réduits de 25%.')
)

return $spells
