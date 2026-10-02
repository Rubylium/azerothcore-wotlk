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

return $spells
