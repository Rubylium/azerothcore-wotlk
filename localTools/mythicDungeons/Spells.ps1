# The Mythic+ dungeons' reworked bosses' spell data (modules/mod-stat-growth/src/mythic/*.cpp): what they hit with,
# their cast bars and their debuffs, so the combat log, the death recap and the cast bars read as the fight.
# Ids 94700-94799.
#
# - A hit (New-Hit): never cast; the script deals its damage on the area it drew (MythicTuning::DealReferenceDamage),
#   naming the spell. A copy of Cosmic Smash, its school set (field 225: 1 physical, 32 shadow).
# - A cast bar (New-Cast): a dummy on the boss himself, cast for as long as the ability takes to land, cloned from the
#   boss's own spell for its animation (field 28, SpellCastTimes: 5 = 2 s, 19 = 2.5 s, 14 = 3 s, 15 = 4 s). Field 31,
#   InterruptFlags: 15 for a cast the players can interrupt, 0 for one they cannot.
# - A debuff or a buff (New-Aura): a real aura with its amount, put on by the script (AddAura), stacked and timed.

$physical = 1
$shadow = 32
$debuff = 0x04000000

function New-Hit($id, $name, $fallback, $school, $description) {
    @{ Id = $id; Clone = 64596; Name = $name; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Fields = @{ 225 = $school } }
}

# visual: the row's look to keep (-1, the clone's own) or another (0: none); icon: the spell whose icon it shows (its
# clone's when 0)
function New-Cast($id, $clone, $name, $castTime, $interruptible, $description, $visual = -1, $icon = 0) {
    $fields = @{ 28 = $castTime; 31 = $(if ($interruptible) { 15 } else { 0 }); 29 = 0; 30 = 0; 1 = 0; 208 = 0; 225 = 1 }
    if ($visual -ge 0) { $fields[131] = $visual }
    @{ Id = $id; Clone = $clone; Name = $name; FallbackIconSpell = $(if ($icon) { $icon } else { $clone }); Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = $fields }
}

# An aura with an amount: 87 damage taken (all schools), 118 healing received, 79 damage done (all schools).
# Duration indexes: 32 = 6 s, 18 = 20 s, 21 = until taken off.
function New-Aura($id, $name, $fallback, $aura, $value, $duration, $stacks, $harmful, $description) {
    # Sprint's row: its speed lines (visual 6) taken off
    $fields = @{ 40 = $duration; 49 = $stacks; 208 = 0; 131 = 0 }
    if ($harmful) { $fields[4] = $debuff }
    @{ Id = $id; Clone = 2983; Name = $name; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       AuraDescription = $description
       Effects = @(@{ Index = 0; Effect = 6; Aura = $aura; TargetA = 1; Misc = $(if ($aura -eq 118) { 0 } else { 127 }); Value = $value })
       Fields = $fields }
}

$spells = @(
    # --- Utgarde Keep: Ingvar the Plunderer (UtgardeKeep.cpp, boss_ingvar_evolutions) ---
    # Phase one, the Plunderer
    (New-Cast 94700 42669 'Fracas' 14 $false 'Ingvar lève sa hache et l''abat devant lui.')
    (New-Hit 94701 'Fracas' 42669 $physical 'La hache d''Ingvar s''abat devant lui.')
    (New-Hit 94702 'Hache lancée' 42749 $physical 'Une hache lancée par Ingvar s''abat sur vous et ceux qui vous entourent.')
    (New-Cast 94703 42708 'Rugissement titubant' 14 $true 'Un rugissement qui ébranle tout le groupe. Interrompez-le !')
    (New-Hit 94704 'Rugissement titubant' 42708 $physical 'Le rugissement d''Ingvar ébranle tout le groupe.')
    # Smite's row, its look taken off: Charge's own row may only be cast out of combat
    (New-Cast 94705 585 'Charge du berserker' 19 $false 'Ingvar va charger tout ce qui se trouve sur son chemin.' 0 11578)
    (New-Hit 94706 'Charge du berserker' 11578 $physical 'Ingvar vous a renversé dans sa charge.')
    # Phase two, undead
    (New-Cast 94707 42723 'Fracas ténébreux' 15 $false 'Ingvar abat sa hache sur tout ce qui se trouve devant lui.')
    (New-Hit 94708 'Fracas ténébreux' 42723 $shadow 'Tout ce qui se trouvait devant Ingvar a été frappé.')
    (New-Hit 94709 "Hache de l'ombre" 42749 $shadow 'La hache d''Ingvar revient vers lui et frappe tout ce qui se trouve sur son passage.')
    (New-Cast 94710 42729 "Rugissement d'effroi" 5 $false 'Un rugissement qui glace le groupe d''effroi.')
    (New-Hit 94711 "Rugissement d'effroi" 42729 $shadow 'Le rugissement d''Ingvar frappe tout le groupe.')
    (New-Aura 94712 'Effroi' 42729 87 5 18 10 $true 'Dégâts subis augmentés de 5% par application.')
    (New-Hit 94713 'Frappe du malheur' 42730 $shadow 'Un coup qui maudit sa cible.')
    (New-Aura 94714 'Frappe du malheur' 42730 118 -50 32 1 $true 'Soins reçus réduits de 50%.')
    (New-Hit 94715 'Tourbillon des âmes' 42729 $shadow 'Les âmes tourbillonnent autour d''Ingvar.')
    (New-Aura 94716 'Âmes dévorées' 47855 79 10 21 3 $false 'Chaque âme vrykule qui a rejoint Ingvar augmente ses dégâts de 10%.')
)

return $spells
