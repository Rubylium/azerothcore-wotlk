# L'Infini's spell data (modules/mod-stat-growth/src/InfiniteGod.cpp, the Défi board's god fight). Ids 90740-90755;
# 90729-90732 are its ring-shaped ground indicators (localTools/groundIndicators/shapes.json).
#
# The abilities are never cast: the script deals their damage on the areas it drew (MythicTuning::DealAbilityDamage),
# naming one of these spells so the combat log and the meters read the ability's own name. Each is a copy of one of
# Algalon's spells, its school set (field 225: 2 holy, 4 fire, 32 shadow, 64 arcane). Until this data is in the
# server's Spell.dbc the script names the stock spell it copies instead.
#
# Two debuffs show what the script adds to every hit a player takes: dummy auras, stacked by the script, shown as
# debuffs (SPELL_ATTR0_AURA_IS_DEBUFF, field 4).

$arcane = 64
$shadow = 32
$fire = 4
$holy = 2
$debuff = 0x04000000

$spells = @(
    @{ Id = 90740; Clone = 64395; Name = 'Double fauchage cosmique'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Deux cônes à la fois, un vers chaque tank : lourd pour le tank visé, dévastateur pour quiconque d''autre s''y trouve.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90741; Clone = 64412; Name = 'Frappes jumelles'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe les deux tanks à la fois.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90742; Clone = 64596; Name = 'Pluie d''étoiles'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des étoiles s''abattent là où se tenaient quelques joueurs.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90743; Clone = 64443; Name = 'Onde de gravité'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 90744; Clone = 64443; Name = 'Big Bang'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le Planétarium, sauf aux pieds de L''Infini.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90745; Clone = 64122; Name = 'Singularité'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un trou noir attire les joueurs vers son puits.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 90746; Clone = 64122; Name = 'Étoile effondrée'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Partagée entre trois joueurs ou plus, sinon elle explose sur tout le groupe.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 90747; Clone = 64596; Name = 'Constellation'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des lignes d''étoiles traversent le Planétarium.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90748; Clone = 64122; Name = 'Néant dévorant'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le bord du Planétarium dévore quiconque s''y tient.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 90749; Clone = 64596; Name = 'Pluie de météores'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des météores s''abattent sur le Planétarium.'
       Fields = @{ 225 = $fire } },
    @{ Id = 90750; Clone = 64443; Name = 'Supernova'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout autour de L''Infini, sauf trois couloirs.'
       Fields = @{ 225 = $holy } },
    @{ Id = 90751; Clone = 64443; Name = 'Jugement divin'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe les marqués, et plus durement quiconque se tient près d''eux.'
       Fields = @{ 225 = $holy } },
    @{ Id = 90752; Clone = 64487; Name = 'La Fin des Temps'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Toutes les 2 secondes, 45% des points de vie maximum de chacun.'
       Fields = @{ 225 = $arcane } },
    @{ Id = 90753; Clone = 2983; Name = 'Fin imminente'; FallbackIconSpell = 64487; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Chaque impact de La Fin des Temps en ajoute une charge.'
       AuraDescription = 'Dégâts subis augmentés de 20% par charge.'
       Fields = @{ 4 = $debuff; 40 = 1 } },
    @{ Id = 90754; Clone = 2983; Name = 'Poids de l''éternité'; FallbackIconSpell = 64412; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 50; Spellbook = $false
       Description = 'Une charge toutes les 10 secondes face au dieu révélé.'
       AuraDescription = 'Dégâts subis augmentés de 3% par charge.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    @{ Id = 90755; Clone = 64443; Name = 'Éclat d''éternité'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'L''énergie de L''Infini frappe tout le groupe.'
       Fields = @{ 225 = $arcane } }
)

return $spells
