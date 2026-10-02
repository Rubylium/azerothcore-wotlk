# Le Front du Nord's spell data (modules/mod-stat-growth/src/frontier/Frontier.cpp, open-world content at level 80).
# Ids 97600-97699.
#
# 97600 is the tier phase: a hidden passive aura (SPELL_AURA_PHASE, misc 0x4001: the normal world's phase 1 plus the
# tier phase 0x4000) given to a level-80 character in a tier zone. Everything the Front du Nord spawns is in phase
# 0x4000 only, so levelling characters in the same zone never see it. A copy of "The Shadow Vault: Phase Shift I"
# (30181), a hidden phase aura of its own.
#
# The abilities are never cast: the script deals their damage on the areas it drew (MythicTuning::DealAbilityDamage),
# naming one of these spells so the combat log and the meters read the ability's own name.

$spells = @(
    @{ Id = 97600; Clone = 30181; Name = 'Front du Nord'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les menaces du Front du Nord vous sont visibles.'
       AuraDescription = 'Les menaces du Front du Nord vous sont visibles.'
       Effects = @(@{ Index = 0; Effect = 6; Aura = 261; TargetA = 1; Value = 0; Misc = 0x4001 })
       Fields = @{ 40 = 21 } },
    # The roaming elites' telegraphed blow, on the circle drawn under their target
    @{ Id = 97601; Clone = 59706; Name = 'Coup dévastateur'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe le sol là où se tenait la cible : quiconque reste dans la zone rougie est durement touché.' },
    # The rift guardian's: a crystal falling where a fighter stood, and a nova all around itself (arcane, 64)
    @{ Id = 97602; Clone = 59706; Name = 'Éclat de cristal'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un éclat de cristal s''abat là où se tenait un combattant.'
       Fields = @{ 225 = 64 } },
    @{ Id = 97603; Clone = 59706; Name = 'Nova arcanique'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une onde arcanique frappe tout autour du gardien de la faille.'
       Fields = @{ 225 = 64 } }
)

# The Colosses' four each, from 97604 (Frontier.cpp SPELL_COLOSSUS_FIRST): a cone toward their target, a circle under
# every fighter, a circle around themselves, and a ring that spares only the ground at their feet.
# School: 1 physical, 8 nature, 16 frost, 32 shadow.
$colossi = @(
    # Gorroth Grandes-Défenses
    @('Coup de défenses', 1, 'Gorroth balaie de ses défenses tout ce qui se tient devant lui.'),
    @('Chute de glace', 16, 'Des blocs de glace s''abattent sous chacun de ses assaillants.'),
    @('Piétinement', 1, 'Gorroth piétine le sol tout autour de lui.'),
    @('Avalanche', 16, 'Une avalanche ensevelit tout, sauf l''abri à ses pieds.'),
    # Vyskarn
    @('Souffle de givre', 16, 'Un souffle glacé devant le wyrm.'),
    @('Pluie verglaçante', 16, 'Une pluie gelée s''abat sous chacun de ses assaillants.'),
    @('Battement d''ailes', 1, 'Ses ailes déchirées balaient tout autour de lui.'),
    @('Blizzard', 16, 'Le blizzard ravage tout, sauf l''abri sous ses ailes.'),
    # Zul'Gath l'Avatar déchu
    @('Hache rituelle', 1, 'Zul''Gath abat sa hache rituelle devant lui.'),
    @('Malédiction vaudou', 32, 'Une malédiction frappe sous chacun de ses assaillants.'),
    @('Fureur du loa', 8, 'Le loa mort se déchaîne tout autour de Zul''Gath.'),
    @('Esprits serpents', 8, 'Les esprits serpents fondent sur tout, sauf l''abri à ses pieds.'),
    # Mastodonte de saronite
    @('Écrasement', 1, 'Le Mastodonte écrase tout ce qui se tient devant lui.'),
    @('Peste glaciale', 8, 'La peste jaillit sous chacun de ses assaillants.'),
    @('Onde de saronite', 32, 'Une onde de saronite frappe tout autour du Mastodonte.'),
    @('Brume du chaudron', 32, 'La brume de son chaudron noie tout, sauf l''abri à ses pieds.')
)
$id = 97604
foreach ($ability in $colossi) {
    $spells += @{ Id = $id; Clone = 59706; Name = $ability[0]; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
                  Description = $ability[2]; Fields = @{ 225 = $ability[1] } }
    $id++
}

return $spells
