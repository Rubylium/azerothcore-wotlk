# Native DBC definitions for the Necromancer's first specialization, Legion.
# Behaviour and scaling live in modules/mod-necromancer; this file owns client/server spell data only.

$classMask = 4096
$skillLine = 901
# 28 = 4 is the 1000 ms casting-time index and 31 = 15 the usual interrupt flags (moving cancels it): Trait d'âme
# is a quick hard cast, the one spell the Nécromancien stands still for between his instants.
$quickCastFields = @{ 28 = 4; 31 = 15; 204 = 1; 208 = 18 }

$spells = @(
    @{ Id = 90400; Clone = 686; Name = "Trait d'âme"; Icon = 'Necromancer_SoulBolt'; FallbackIconSpell = 686; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Projette une âme noire sur l'ennemi, infligeant des dégâts d'Ombre et générant 1 Âme capturée."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 }); Fields = $quickCastFields },
    @{ Id = 90401; Clone = 2983; Name = 'Âmes capturées'; Icon = 'Necromancer_CapturedSouls'; FallbackIconSpell = 24658; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 12
       Description = "Les âmes arrachées à vos ennemis servent à relever vos morts."; AuraDescription = "À 10 âmes, Levée des morts relève une escouade."; Fields = @{ 40 = 21; 208 = 18 } },
    # Levée des morts: the one raise. The three raises of the first version made the player pick a minion, and
    # balance only ever lets one of those be right; a whole squad at 10 Âmes is the choice-free version.
    @{ Id = 90402; Clone = 46584; Name = 'Levée des morts'; Icon = 'Necromancer_RaiseSkeleton'; FallbackIconSpell = 46584; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme 10 Âmes capturées : deux guerriers squelettes, un archer maudit et un mage de peste s'extirpent du sol à vos côtés. Vos serviteurs se décomposent sans cesse : Drain d'âme et Ordre de mort les maintiennent en vie. Vous commandez jusqu'à 8 serviteurs."
       # 131 = 11833 is Summon Gargoyle's visual: the clone (Raise Dead) carries none, so the character stood
       # still. Every raise borrows the same instant summoning gesture.
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 11833; 204 = 2; 208 = 18 } },
    @{ Id = 90403; Clone = 172; Name = 'Marque funèbre'; Icon = 'Necromancer_DeathlyBrand'; FallbackIconSpell = 172; Cost = 0; Cooldown = 0; Level = 2; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Marque l'ennemi pendant 18 sec et lui inflige des dégâts d'Ombre toutes les 3 sec. Vos serviteurs infligent 15% de dégâts supplémentaires à la cible. Génère 1 âme lors de l'application et une autre si la cible meurt marquée. Peut être lancé en mouvement."
       AuraDescription = "Subit des dégâts d'Ombre et 15% de dégâts supplémentaires des serviteurs du Nécromancien."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 31 = 0; 33 = 0; 40 = 85; 49 = 1; 204 = 1; 208 = 18; 98 = 3000 } },
    @{ Id = 90404; Clone = 1454; Name = "Ponction d’âme"; Icon = 'Necromancer_SoulTap'; FallbackIconSpell = 1454; Cost = 0; Cooldown = 30000; Level = 6; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Rend instantanément 20% de votre mana maximum et génère 2 Âmes capturées. Ne coûte pas de points de vie."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 204 = 0; 208 = 18 } },
    @{ Id = 90405; Clone = 50514; Name = 'Ordre de mort'; Icon = 'Necromancer_DeathCommand'; FallbackIconSpell = 50514; Cost = 0; Cooldown = 12000; Level = 4; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Ordonne à toute votre armée de se jeter sur la cible. Vos serviteurs récupèrent aussitôt 30% de leurs points de vie et infligent 25% de dégâts supplémentaires pendant 6 sec."
       # 46 = 4 is the 30 yd range index: the clone carries a 0 yd one, which would put this melee-only. Clone
       # 50514 carries no global cooldown and that one is kept: commanding the army is free, like a pet order.
       # 131 = 8208 is Shadowfiend's visual - a servant thrown at the target, which is what this is.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 4; 131 = 8208; 204 = 1; 208 = 18 } },
    # Retired by Levée des morts; kept so the id stays reserved. Taken back from anyone who still knows it.
    @{ Id = 90406; Clone = 46584; Name = 'Réanimation : archer maudit'; Icon = 'Necromancer_RaiseDeadeye'; FallbackIconSpell = 47632; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Remplacé par Levée des morts."
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 11833; 204 = 2; 208 = 18 } },
    # The army's lifeline: a standing channel (33 = 31756 is Drain Soul's own: moving breaks it) with no cooldown.
    @{ Id = 90407; Clone = 1120; Name = "Drain d'âme"; Icon = 'Necromancer_SoulDrain'; FallbackIconSpell = 1120; Cost = 0; Cooldown = 0; Level = 8; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Canalise pendant 3 sec, infligeant des dégâts d'Ombre chaque seconde. Chaque pulsation transfuse la vie volée à vos serviteurs, qui récupèrent 6% de leurs points de vie, et vous rend 3% de votre mana maximum. La dernière pulsation génère 1 Âme."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 31 = 15; 33 = 31756; 40 = 27; 204 = 1; 208 = 18; 98 = 1000 } },
    @{ Id = 90408; Clone = 49811; Name = 'Explosion morbide'; Icon = 'Necromancer_CorpseExplosion'; FallbackIconSpell = 49811; Cost = 0; Cooldown = 6000; Level = 14; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fait exploser la Marque funèbre de la cible, infligeant des dégâts d'Ombre à un maximum de 8 ennemis dans un rayon de 10 mètres. Chaque serviteur actif augmente les dégâts de 4%."
       # 46 = 4 is the 30 yd range index: the clone carries a 0 yd one, which would put this melee-only. 205/206
       # are the global cooldown: clone 49811 has none at all, and a nuke every 6 s pays for its global.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 4; 204 = 3; 205 = 133; 206 = 1500; 208 = 18 } },
    @{ Id = 90409; Clone = 49206; Name = 'Légion frénétique'; Icon = 'Necromancer_FrenziedLegion'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 45000; Level = 18; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Déchaîne vos serviteurs pendant 12 sec : vitesse d'attaque et dégâts augmentés de 30%. Chaque attaque de serviteur a une chance de vous rendre 1% de votre mana maximum."
       # DummyAura blanks the visual so hidden buff auras carry no glow; this one is a button the player
       # presses, so it takes its clone's visual (Summon Gargoyle) back afterwards.
       AuraDescription = "Les serviteurs sont frénétiques."; DummyAura = $true; Fields = @{ 31 = 0; 40 = 29; 41 = 0; 131 = 11833; 204 = 2; 208 = 18 } },
    # Retired by Levée des morts; kept so the id stays reserved. Taken back from anyone who still knows it.
    @{ Id = 90410; Clone = 46584; Name = 'Réanimation : mage de peste'; Icon = 'Necromancer_RaisePlagueMage'; FallbackIconSpell = 47809; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Remplacé par Levée des morts."
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 11833; 204 = 3; 208 = 18 } },
    @{ Id = 90411; Clone = 47897; Name = "Moisson d'âmes"; Icon = 'Necromancer_SoulHarvest'; FallbackIconSpell = 47897; Cost = 0; Cooldown = 20000; Level = 28; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fauche jusqu'à 5 ennemis devant vous. Inflige des dégâts d'Ombre et génère 1 âme par ennemi touché, jusqu'à 5. Les cibles marquées subissent 35% de dégâts supplémentaires. Peut être lancé en mouvement."
       Fields = @{ 31 = 0; 33 = 0; 204 = 2; 208 = 18; 212 = 5 } },
    @{ Id = 90412; Clone = 48743; Name = 'Pacte sacrificiel'; Icon = 'Necromancer_SacrificialPact'; FallbackIconSpell = 48743; Cost = 0; Cooldown = 30000; Level = 34; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Sacrifie votre serviteur le plus affaibli : il explose et inflige des dégâts d'Ombre aux ennemis à moins de 8 mètres. Vous récupérez 20% de vos points de vie et 25% de votre mana maximum, puis gagnez 2 Âmes."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 204 = 0; 208 = 18 } },
    @{ Id = 90413; Clone = 47847; Name = 'Salve noire'; Icon = 'Necromancer_BlackVolley'; FallbackIconSpell = 5740; Cost = 0; Cooldown = 8000; Level = 42; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Déchaîne une salve sur la cible et jusqu'à 7 ennemis proches. Chaque ennemi touché a 20% de chances de vous accorder une âme. Peut être lancé en mouvement."
       # 205/206 are the global cooldown: clone 47847 only spends 0.5 s of it, which made this three times
       # cheaper per cast than the rest of the kit.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 33 = 0; 204 = 4; 205 = 133; 206 = 1500; 208 = 18; 212 = 8 } },
    @{ Id = 90414; Clone = 46584; Name = 'Créer une abomination'; Icon = 'Necromancer_CreateAbomination'; FallbackIconSpell = 63560; Cost = 0; Cooldown = 90000; Level = 52; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Assemble une abomination qui s'extirpe du sol et frappe très fort. Elle se décompose deux fois moins vite que vos autres serviteurs. Une seule abomination peut servir à la fois."
       # The big one gets Army of the Dead's visual rather than the ordinary summoning gesture.
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 9607; 204 = 5; 208 = 18 } },
    @{ Id = 90415; Clone = 42650; Name = 'Armée des damnés'; Icon = 'Necromancer_ArmyOfTheDamned'; FallbackIconSpell = 42650; Cost = 0; Cooldown = 180000; Level = 60; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Relève aussitôt une escouade complète sans consommer d'Âmes et rend tous leurs points de vie à vos serviteurs."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 33 = 0; 204 = 5; 208 = 18 } },

    # Hidden combat-log spells used by minion AI.
    @{ Id = 90420; Clone = 686; Name = 'Trait spectral'; Icon = 'Necromancer_SoulBolt'; FallbackIconSpell = 686; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 13; 213 = 0; 208 = 18 } },
    @{ Id = 90421; Clone = 47809; Name = 'Salve de peste'; Icon = 'Necromancer_RaisePlagueMage'; FallbackIconSpell = 47809; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 13; 213 = 0; 208 = 18 } },
    # The class passive, and the one thing holding the Necromancer's threat down. Every minion's ranged damage
    # is dealt with the player as its original caster - which is what credits it to him in the meters - so its
    # threat lands on him too, on top of his own spells and every tick of his rot. A pet class whose pets
    # generate no threat of their own needs that back, or the tank cannot hold anything he is shooting at.
    #
    # Aura 10 is SPELL_AURA_MOD_THREAT, misc 127 is every school, and the value is a percentage. Attribute
    # 0x40 (SPELL_ATTR0_PASSIVE) on top of the clone's 0x00010010 is what makes it apply itself on learning.
    @{ Id = 90422; Clone = 2983; Name = 'Maîtrise nécromantique'; Icon = 'Necromancer_CapturedSouls'; FallbackIconSpell = 24658; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $false; DummyAura = $true
       Description = "Vos serviteurs combattent en votre nom : vous générez 50% de menace en moins."
       AuraDescription = "Génère 50% de menace en moins."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_ModThreat; TargetA = 1; Value = -50; Misc = 127 })
       Fields = @{ 4 = 65616; 40 = 21; 208 = 18 } },
    @{ Id = 90424; Clone = 172; Name = 'Pourriture nécrotique'; Icon = 'Necromancer_RaisePlagueMage'; FallbackIconSpell = 47809; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "La pourriture que répand votre armée."
       AuraDescription = "Subit des dégâts d'Ombre toutes les 3 sec. Se cumule jusqu'à 5 fois."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 31 = 0; 33 = 0; 40 = 29; 46 = 13; 49 = 5; 98 = 3000; 208 = 18; 213 = 0 } },
    @{ Id = 90423; Clone = 49811; Name = 'Déflagration funèbre'; Icon = 'Necromancer_CorpseExplosion'; FallbackIconSpell = 49811; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 13; 213 = 0; 208 = 18 } },

    # Talent actives. Their rank spell is the ability itself.
    @{ Id = 90518; Clone = 49222; Name = "Fer de lance d'os"; Icon = 'NecromancerTalent_BoneSpearhead'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 15000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Projette une lance d'os qui transperce jusqu'à 5 ennemis, inflige des dégâts d'Ombre et génère une âme par cible touchée. Peut être lancée en mouvement."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 24; BasePoints = 0 }); Fields = @{ 31 = 0; 204 = 2; 208 = 18; 212 = 5 } },
    @{ Id = 90533; Clone = 42650; Name = 'Marée des tombes'; Icon = 'NecromancerTalent_GraveTide'; FallbackIconSpell = 42650; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Invoque 2 guerriers squelettes sans coût et leur donne immédiatement l'Ordre de mort."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 208 = 18 } },
    # Linceul d'os (class tree): Bone Shield's look, none of its charges or procs - the damage reduction is the
    # module's (NecromancerUnitScript). 40 = 31 is 8 sec.
    @{ Id = 90430; Clone = 49222; Name = "Linceul d'os"; IconPath = 'Interface\Icons\Spell_Shadow_AntiMagicShell'; FallbackIconSpell = 49222; Cost = 0; Cooldown = 60000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Talent. Des os tourbillonnent autour de vous : les dégâts que vous subissez sont réduits de 30% pendant 8 sec."
       AuraDescription = "Dégâts subis réduits de 30%."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_Dummy; TargetA = 1 }); Fields = @{ 31 = 0; 34 = 0; 35 = 0; 36 = 0; 40 = 31; 208 = 18 } },
    # Hurlement des damnés (class tree): the class's interrupt. Counterspell's interrupt and school lockout (40 = 32
    # is 6 sec), Spell Lock's shadowy look.
    @{ Id = 90431; Clone = 2139; Name = 'Hurlement des damnés'; IconPath = 'Interface\Icons\Spell_Shadow_DeathScream'; FallbackIconSpell = 2139; Cost = 0; Cooldown = 24000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Talent. Un hurlement d'outre-tombe interrompt l'incantation de la cible et l'empêche de lancer des sorts de cette école pendant 6 sec."
       Effects = @(@{ Index = 0; Effect = 68; TargetA = 6 }); Fields = @{ 31 = 0; 40 = 32; 208 = 18 }; Visual = @{ Clone = 5282 } },
    @{ Id = 90561; Clone = 49206; Name = 'Maître des morts'; Icon = 'NecromancerTalent_MasterOfTheDead'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Rend tous leurs points de vie à vos serviteurs, suspend leur décomposition et augmente leurs dégâts de 50% pendant 20 sec. Leurs attaques vous rendent du mana."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 208 = 18 } }
)

# Talent ranks come from the talent trees (talentTree.json), which the server and the talent window read as well
$spells += & (Join-Path $repoRoot 'localTools\talentTree\TalentRankSpells.ps1') `
    -TreePath (Join-Path $repoRoot 'localTools\necromancer\talentTree.json') -Family 18

return $spells
