# The raid trinkets' spell data (modules/mod-stat-growth/src/RaidTrinkets.cpp): eight trinkets of the Hollow Voice
# (items 17836-17843, item level 477) and eight of Gardien-chef Vorhan (17844-17848, 17851-17853, item level 485), one
# passive and one active for each role - a fighter, a caster, a healer, a tank; two of the Traqueur d'évadés (17856,
# 17857, item level 485), a passive and an active for every damage dealer. Ids 94600-94626. Four of Valérian
# (17859-17862, item level 485), one for each role: ids 94550-94559 (free, between Vorhan's fading indicators'
# 94500-94549 and these), what their spell data cannot say modules/mod-stat-growth/src/RaidTrinketSpells.cpp's. Items,
# procs and scripts: data/sql/updates/pending_db_world (item_template, spell_proc, spell_script_names).
#
# What a trinket gives is sized as the raid's generated gear grows (power-scaling.md): a stock trinket's at item level
# 284 (the Ruby Sanctum's and Icecrown's heroic ones), its power and stats times the item level ratio (477/284 = 1.68,
# 485/284 = 1.71), its ratings times that ratio's square root (1.30, 1.31). What lasts a share of health or of damage
# needs no sizing.
# - An equip spell (a passive): a stock trinket's own proc row, hidden, its flags and its proc here; its chance and its
#   cooldown are spell_proc's. A buff it triggers: a stock trinket's buff, its amounts here.
# - A use spell (an active): a stock trinket's use, its amounts here; its 2 min are the item's.
# Every spell wears its trinket's icon. Duration indexes (SpellDuration): 31 = 8 s, 1 = 10 s, 29 = 12 s, 8 = 15 s,
# 18 = 20 s. Radius indexes (SpellRadius): 14 = 8 yd, 18 = 15 yd.

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

# One aura with an amount: 87 damage taken, 79 damage done (both all schools), 133 maximum health, 136 healing done,
# 65 casting speed
function New-Aura($aura, $value) {
    , @(@{ Index = 0; Effect = 6; Aura = $aura; TargetA = 1; Misc = $(if ($aura -eq 87 -or $aura -eq 79) { 127 } else { 0 }); Value = $value })
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

    # --- Le Traqueur d'évadés (item level 485), for every damage dealer ---
    # Croc du gangrechien: every blow or harmful spell a stack, 1% more damage done each, up to 5, for 10 s (the pack
    # closing in: a fighter's and a caster's alike, as Vorhan's whetstone's stacks)
    (New-Passive 94624 'Croc du gangrechien' 'ICON_Traqueur_Croc' ($weaponBlows -bor $harmfulSpells) 94625 `
        "Chacune de vos attaques et chacun de vos sorts de dégâts augmentent les dégâts que vous infligez de 1% pendant 10 sec. Cumulable jusqu'à 5 fois.")
    (New-Buff 94625 75456 'Meute en chasse' 'ICON_Traqueur_Croc' 1 (New-Aura 79 1) `
        'Dégâts infligés augmentés de 1% par application.' 5)
    # Cor de l'hallali: 600 critical strike rating for 20 s (as the Penne's haste), x1.31
    (New-Active 94626 "Sonner l'hallali" 'ICON_Traqueur_CorHallali' 18 (New-Rating $critRating 786) `
        'Augmente votre score de coup critique de 786 pendant 20 sec.' 'Score de coup critique augmenté de 786.')
)

# --- Valérian (item level 485), one for each role ---
# A stock class spell cloned for its looks keeps nothing of its class: no family (talents and glyphs would reach it),
# no category (it would share a cooldown with the class's own), no spell power coefficient (fields 229-231, floats: 0)
function Clear-ClassFields($fields) {
    foreach ($field in 1, 208, 209, 210, 211, 229, 230, 231) { $fields[$field] = 0 }
    $fields
}

# A spell with some more fields than its helper writes
function Add-Fields($spell, $fields) {
    foreach ($field in $fields.Keys) { $spell.Fields[$field] = $fields[$field] }
    $spell
}

$spells += @(
    # Lance de la Litanie: a blow sometimes throws a spear of light (15%, 3 s rest: spell_proc) through its target, 120%
    # of the wearer's attack power as holy damage to it and to every enemy up to 15 yd behind it on the line from the
    # wearer (RaidTrinketSpells.cpp chooses them and sizes the hit). No stock trinket at 284 hurts on a blow: the hit is
    # the wearer's own attack power, grown with the gear as it is. About a spear every 6 s on one target: about what the
    # Pierre à aiguiser's 850 attack power gives, more on a line of enemies.
    (New-Passive 94550 'Lance de la Litanie' 'INV_Spear_04' $weaponBlows 94551 `
        "Vos attaques en mêlée et à distance ont une chance de projeter une lance de lumière à travers votre cible : elle inflige des dégâts du Sacré égaux à 120% de votre puissance d'attaque à la cible et à tous les ennemis alignés jusqu'à 15 mètres derrière elle.")
    # The spear: Holy Shock's hit (48822), anywhere (13); effect 0 its target, effect 1 the enemies within 15 yd of it
    # (53: at the target, 16: the enemies around that point), which the script narrows to the line behind it
    @{ Id = 94551; Clone = 48822; Name = 'Lance de la Litanie'; IconPath = 'Interface\Icons\INV_Spear_04'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une lance de lumière qui transperce sa cible et les ennemis alignés derrière elle.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; Value = 1 },
                   @{ Index = 1; Effect = 2; TargetA = 53; Value = 1 })
       Fields = (Clear-ClassFields @{ 46 = 13; 90 = 16; 93 = 18 }) }

    # Psaume du Zélote: for 20 s each harmful spell cast (spell_proc: its cast, once) adds a verse, casts 2% faster a
    # verse up to 8 (16%, under the Œil du Gardien-chef's 20% all along); the 8th makes the next spell with a cast time
    # instant, once (RaidTrinketSpells.cpp: the verses, the charge, the verses gone with the psalm)
    # (proc flags 34, its chance 35, the verse it adds 116)
    (Add-Fields (New-Active 94552 'Psaume du Zélote' 'INV_Relics_LibramofTruth' 18 `
        @(@{ Index = 0; Effect = 6; Aura = 42; TargetA = 1; BasePoints = 0 }) `
        "Pendant 20 sec, chacun de vos sorts offensifs ajoute un verset au psaume : vitesse d'incantation augmentée de 2% par verset, jusqu'à 8. Le huitième rend instantané votre prochain sort." `
        'Chaque sort offensif ajoute un verset au psaume.') @{ 34 = $harmfulSpells; 35 = 100; 116 = 94553 })
    (New-Buff 94553 75473 'Verset du Zélote' 'INV_Relics_LibramofTruth' 18 (New-Aura 65 2) `
        "Vitesse d'incantation augmentée de 2% par verset." 8)
    # The 8th verse: casting time -100% (108: a percent modifier, 10: casting time) on one spell (field 50: a charge).
    # A modifier of no family reaches every spell; its class mask all set, so the client's cast bar knows it too.
    @{ Id = 94554; Clone = 75473; Name = 'Psaume accompli'; IconPath = 'Interface\Icons\INV_Relics_LibramofTruth'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Votre prochain sort est instantané.'
       AuraDescription = 'Votre prochain sort est instantané.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 108; TargetA = 1; Misc = 10; Value = -100 })
       Fields = @{ 40 = 1; 49 = 1; 50 = 1; 122 = 0xFFFFFFFF; 123 = 0xFFFFFFFF; 124 = 0xFFFFFFFF; 208 = 0 } }

    # Sceau du Chœur: for 12 s each direct heal (spell_proc: a helpful spell's heal, on its hit) also heals the most
    # hurt other member of the group within 40 yd, its target aside, for 35% of it (RaidTrinketSpells.cpp)
    (Add-Fields (New-Active 94555 'Sceau du Chœur' 'Spell_Holy_SealOfSalvation' 29 `
        @(@{ Index = 0; Effect = 6; Aura = 4; TargetA = 1; BasePoints = 0 }) `
        "Pendant 12 sec, chacun de vos soins directs soigne aussi l'allié le plus blessé à moins de 40 mètres pour 35% de son montant." `
        "Vos soins directs soignent aussi l'allié le plus blessé pour 35% de leur montant.") @{ 34 = $helpfulSpells; 35 = 100 })
    # The echo: Beacon of Light's heal (53652), of the amount the script hands it, never a critical (no damage class)
    @{ Id = 94556; Clone = 53652; Name = 'Écho du Chœur'; IconPath = 'Interface\Icons\Spell_Holy_SealOfSalvation'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Le chœur reprend le soin d'un allié pour le plus blessé."
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; Value = 1 })
       Fields = (Clear-ClassFields @{ 46 = 13; 213 = 0 }) }

    # Chaîne brisée: a shield of 25% of the wearer's maximum health for 10 s (Corroded Skeleton Key's Hardened Skin,
    # 71586, 6400 at item level 264; a share of health needs no sizing); when it ends, broken or run out, the chain
    # bursts for what it absorbed (RaidTrinketSpells.cpp)
    @{ Id = 94557; Clone = 71586; Name = 'Chaîne brisée'; IconPath = 'Interface\Icons\Spell_Frost_ChainsOfIce'; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $false
       Description = "Vous protège d'un bouclier absorbant des dégâts égaux à 25% de vos points de vie maximum pendant 10 sec. Quand il disparaît, la chaîne éclate et inflige aux ennemis à moins de 8 mètres des dégâts du Sacré égaux aux dégâts absorbés."
       AuraDescription = 'Absorbe des dégâts. La chaîne éclate quand le bouclier disparaît.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 69; TargetA = 1; Misc = 127; BasePoints = 0 })
       Fields = @{ 40 = 1; 208 = 0 } }
    # The burst: Holy Nova's (48078), the enemies around the wearer (22, 15) within 8 yd, of the amount the script hands
    # it (no damage class: no critical, no coefficient); 14 = 0: no form forbids it
    @{ Id = 94558; Clone = 48078; Name = 'Chaîne brisée'; IconPath = 'Interface\Icons\Spell_Frost_ChainsOfIce'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La chaîne éclate et rend les coups qu''elle a retenus.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 22; Value = 1 })
       Fields = (Clear-ClassFields @{ 14 = 0; 89 = 15; 92 = 14; 213 = 0 }) }
)

return $spells
