# Native DBC definitions for the Necromancer's first specialization, Legion.
# Behaviour and scaling live in modules/mod-necromancer; this file owns client/server spell data only.

$classMask = 4096
$skillLine = 901
# 28 = 1 is the 0 ms casting-time index: the Necromancer has no hard casts, so Trait d'âme is instant like the
# rest of the kit and rides the global cooldown alone, which haste shrinks from 1.5 s down to 1.0 s.
$mobileCastFields = @{ 28 = 1; 31 = 0; 33 = 0; 204 = 1; 208 = 18 }

$spells = @(
    @{ Id = 90400; Clone = 686; Name = "Trait d'âme"; Icon = 'Necromancer_SoulBolt'; FallbackIconSpell = 686; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Projette instantanément une âme noire sur l'ennemi, infligeant des dégâts d'Ombre et générant 1 Âme capturée. Peut être lancé en mouvement."
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 }); Fields = $mobileCastFields },
    @{ Id = 90401; Clone = 2983; Name = 'Âmes capturées'; Icon = 'Necromancer_CapturedSouls'; FallbackIconSpell = 24658; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; DummyAura = $true; MaxStacks = 12
       Description = "Les âmes arrachées à vos ennemis servent à invoquer vos morts-vivants."; AuraDescription = "Accumulez jusqu'à 10 âmes. Les invocations les consomment."; Fields = @{ 40 = 21; 208 = 18 } },
    @{ Id = 90402; Clone = 46584; Name = 'Réanimation : guerrier squelette'; Icon = 'Necromancer_RaiseSkeleton'; FallbackIconSpell = 46584; Cost = 0; Cooldown = 0; Level = 1; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme 2 Âmes capturées pour invoquer pendant 30 sec un guerrier squelette qui lacère votre cible. Vous pouvez contrôler jusqu'à 8 serviteurs ordinaires."
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
    @{ Id = 90405; Clone = 50514; Name = 'Ordre de mort'; Icon = 'Necromancer_DeathCommand'; FallbackIconSpell = 50514; Cost = 0; Cooldown = 6000; Level = 4; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Ordonne à toute votre armée de se jeter sur la cible. Pendant 6 sec, vos serviteurs infligent 25% de dégâts supplémentaires et récupèrent 30% de leurs points de vie."
       # 46 = 4 is the 30 yd range index: the clone carries a 0 yd one, which would put this melee-only. Clone
       # 50514 carries no global cooldown and that one is kept: commanding the army is free, like a pet order.
       # 131 = 8208 is Shadowfiend's visual - a servant thrown at the target, which is what this is.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 46 = 4; 131 = 8208; 204 = 1; 208 = 18 } },
    @{ Id = 90406; Clone = 46584; Name = 'Réanimation : archer maudit'; Icon = 'Necromancer_RaiseDeadeye'; FallbackIconSpell = 47632; Cost = 0; Cooldown = 0; Level = 8; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme 3 Âmes capturées pour invoquer pendant 30 sec un archer maudit qui attaque à distance. Ses tirs sont particulièrement efficaces sur une cible marquée."
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 11833; 204 = 2; 208 = 18 } },
    @{ Id = 90407; Clone = 1120; Name = "Drain d'âme"; Icon = 'Necromancer_SoulDrain'; FallbackIconSpell = 1120; Cost = 0; Cooldown = 12000; Level = 10; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Canalise en mouvement pendant 4 sec, infligeant des dégâts d'Ombre chaque seconde. Chaque pulsation rend 3% de votre mana maximum et la dernière génère 2 âmes."
       Effects = @(@{ Index = 0; Effect = 6; Aura = $A_PeriodicDamage; TargetA = 6; BasePoints = 0 })
       Fields = @{ 31 = 0; 33 = 0; 40 = 35; 204 = 1; 208 = 18; 98 = 1000 } },
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
    @{ Id = 90410; Clone = 46584; Name = 'Réanimation : mage de peste'; Icon = 'Necromancer_RaisePlagueMage'; FallbackIconSpell = 47809; Cost = 0; Cooldown = 0; Level = 22; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme 4 Âmes capturées pour invoquer pendant 30 sec un mage de peste. Ses salves frappent jusqu'à 4 ennemis autour de sa cible."
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 11833; 204 = 3; 208 = 18 } },
    @{ Id = 90411; Clone = 47897; Name = "Moisson d'âmes"; Icon = 'Necromancer_SoulHarvest'; FallbackIconSpell = 47897; Cost = 0; Cooldown = 20000; Level = 28; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Fauche jusqu'à 5 ennemis devant vous. Inflige des dégâts d'Ombre et génère 1 âme par ennemi touché, jusqu'à 5. Les cibles marquées subissent 35% de dégâts supplémentaires. Peut être lancé en mouvement."
       Fields = @{ 31 = 0; 33 = 0; 204 = 2; 208 = 18; 212 = 5 } },
    @{ Id = 90412; Clone = 48743; Name = 'Pacte sacrificiel'; Icon = 'Necromancer_SacrificialPact'; FallbackIconSpell = 48743; Cost = 0; Cooldown = 60000; Level = 34; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Sacrifie votre serviteur ordinaire le plus ancien. Vous récupérez 20% de vos points de vie et 25% de votre mana maximum, puis gagnez 2 âmes."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 204 = 0; 208 = 18 } },
    @{ Id = 90413; Clone = 47847; Name = 'Salve noire'; Icon = 'Necromancer_BlackVolley'; FallbackIconSpell = 5740; Cost = 0; Cooldown = 8000; Level = 42; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Déchaîne une salve sur la cible et jusqu'à 7 ennemis proches. Chaque ennemi touché a 20% de chances de vous accorder une âme. Peut être lancé en mouvement."
       # 205/206 are the global cooldown: clone 47847 only spends 0.5 s of it, which made this three times
       # cheaper per cast than the rest of the kit.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6; BasePoints = 0 }); Fields = @{ 31 = 0; 33 = 0; 204 = 4; 205 = 133; 206 = 1500; 208 = 18; 212 = 8 } },
    @{ Id = 90414; Clone = 46584; Name = 'Créer une abomination'; Icon = 'Necromancer_CreateAbomination'; FallbackIconSpell = 63560; Cost = 0; Cooldown = 45000; Level = 52; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Consomme 8 Âmes capturées pour créer pendant 35 sec une abomination qui frappe très fort, provoque les ennemis proches et réduit de 10% les dégâts qu'ils infligent. Une seule abomination peut servir à la fois."
       # The big one gets Army of the Dead's visual rather than the ordinary summoning gesture.
       # 4 drops SPELL_ATTR0_COOLDOWN_ON_EVENT (0x02000000), inherited from Raise Dead: it holds the cooldown
       # open until the summon's "effect" ends, and a script summon raises no such event - one cast and the
       # button never comes back. 1 = 0 drops Raise Dead's cooldown category along with it.
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 1 = 0; 4 = 2147483648; 31 = 0; 33 = 0; 131 = 9607; 204 = 5; 208 = 18 } },
    @{ Id = 90415; Clone = 42650; Name = 'Armée des damnés'; Icon = 'Necromancer_ArmyOfTheDamned'; FallbackIconSpell = 42650; Cost = 0; Cooldown = 180000; Level = 60; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Invoque immédiatement 3 guerriers et 2 archers sans consommer d'âmes, puis prolonge de 15 sec tous vos serviteurs."
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
    @{ Id = 90561; Clone = 49206; Name = 'Maître des morts'; Icon = 'NecromancerTalent_MasterOfTheDead'; FallbackIconSpell = 49206; Cost = 0; Cooldown = 120000; Level = 0; Spellbook = $true; SkillLine = $skillLine; ClassMask = $classMask
       Description = "Rafraîchit la durée de tous vos serviteurs et les renforce pendant 20 sec. Leurs dégâts augmentent de 50% et leurs attaques vous rendent du mana."
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 }); Fields = @{ 31 = 0; 208 = 18 } }
)

$talents = @(
    @{ Name='Savoir interdit'; Icon='NecromancerTalent_ForbiddenKnowledge'; FallbackIcon=686; Ids=90500..90504; Values=2,4,6,8,10; Text='Augmente les dégâts de vos sorts de {0}%.' },
    @{ Name='Rituels rapides'; Icon='NecromancerTalent_SwiftRituals'; FallbackIcon=46584; Ids=90505..90507; Values=2,4,6; Text='Prolonge vos serviteurs de {0} sec et réduit le coût en mana des invocations.' },
    @{ Name="Réservoir d'âmes"; Icon='NecromancerTalent_SoulReservoir'; FallbackIcon=24658; Ids=90508..90509; Values=1,2; Text="Augmente de {0} votre maximum d’Âmes capturées." },
    @{ Name='Artisan des os'; Icon='NecromancerTalent_Bonecraft'; FallbackIcon=49222; Ids=90510..90512; Values=5,10,15; Text='Augmente les dégâts de tous vos serviteurs de {0}%.' },
    @{ Name='Économie funèbre'; Icon='NecromancerTalent_GraveEconomy'; FallbackIcon=1454; Ids=90513..90514; Values=1,2; Text='Chaque âme dépensée vous rend {0}% de votre mana maximum supplémentaire.' },
    @{ Name='Marque profonde'; Icon='NecromancerTalent_DeepBrand'; FallbackIcon=172; Ids=90515..90517; Values=10,20,30; Text='Augmente les dégâts de Marque funèbre de {0}%.' },
    @{ Name='Commandement impitoyable'; Icon='NecromancerTalent_RuthlessCommand'; FallbackIcon=50514; Ids=90519..90521; Values=5,10,15; Text="Augmente de {0}% le bonus de dégâts d'Ordre de mort." },
    @{ Name='Marche sans fin'; Icon='NecromancerTalent_EndlessMarch'; FallbackIcon=46584; Ids=90522..90524; Values=2,4,6; Text='Prolonge vos serviteurs de {0} sec.' },
    @{ Name='Tireurs de la crypte'; Icon='NecromancerTalent_CryptMarksmen'; FallbackIcon=47632; Ids=90525..90527; Values=8,16,24; Text='Augmente les dégâts de vos archers maudits de {0}%.' },
    @{ Name='Âmes débordantes'; Icon='NecromancerTalent_OverflowingSouls'; FallbackIcon=24658; Ids=90528..90529; Values=10,20; Text="Vos générateurs ont {0}% de chances d'accorder une âme supplémentaire." },
    @{ Name='Doctrine de la peste'; Icon='NecromancerTalent_PlagueDoctrine'; FallbackIcon=47809; Ids=90530..90532; Values=8,16,24; Text='Augmente les dégâts de zone de vos sorts et mages de peste de {0}%.' },
    @{ Name='Cohorte immortelle'; Icon='NecromancerTalent_UndyingCohort'; FallbackIcon=48743; Ids=90534..90536; Values=5,10,15; Text='Augmente les points de vie de vos serviteurs de {0}%.' },
    @{ Name='Légion innombrable'; Icon='NecromancerTalent_CountlessLegion'; FallbackIcon=42650; Ids=90537..90538; Values=1,2; Text='Vous pouvez contrôler {0} serviteur ordinaire supplémentaire.' },
    @{ Name='Frénésie parfaite'; Icon='NecromancerTalent_PerfectFrenzy'; FallbackIcon=49206; Ids=90539..90541; Values=5,10,15; Text='Augmente de {0}% les dégâts conférés par Légion frénétique.' },
    @{ Name='Feu des âmes'; Icon='NecromancerTalent_Soulfire'; FallbackIcon=47897; Ids=90542..90544; Values=3,6,9; Text="Augmente les dégâts de Trait d’âme, Drain d’âme et Moisson d’âmes de {0}%." },
    @{ Name='Sceau éternel'; Icon='NecromancerTalent_EternalSeal'; FallbackIcon=172; Ids=90545..90546; Values=3,6; Text='Prolonge Marque funèbre de {0} sec.' },
    @{ Name="Chair d'abomination"; Icon='NecromancerTalent_AbominationFlesh'; FallbackIcon=63560; Ids=90547..90549; Values=10,20,30; Text='Augmente les dégâts et les points de vie de votre abomination de {0}%.' },
    @{ Name='Présence du maître'; Icon='NecromancerTalent_MasterPresence'; FallbackIcon=50514; Ids=90550..90554; Values=2,4,6,8,10; Text="Augmente de {0}% les dégâts de vos serviteurs lorsqu’ils se trouvent à moins de 30 mètres." },
    @{ Name='Volonté nécrotique'; Icon='NecromancerTalent_NecroticWill'; FallbackIcon=48743; Ids=90555..90557; Values=3,6,9; Text='Réduit les dégâts que vous subissez de {0}% tant que vous contrôlez au moins 3 serviteurs.' },
    @{ Name='Moisson vorace'; Icon='NecromancerTalent_VoraciousHarvest'; FallbackIcon=47897; Ids=90558..90560; Values=5,10,15; Text="Augmente les dégâts de Moisson d’âmes de {0}% et son maximum de cibles." }
)

foreach ($talent in $talents) {
    for ($rank = 0; $rank -lt $talent.Ids.Count; ++$rank) {
        $spells += @{
            Id = $talent.Ids[$rank]; Clone = 2983; Name = $talent.Name; Icon = $talent.Icon; FallbackIconSpell = $talent.FallbackIcon
            Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false; TalentAura = $true
            Description = [string]::Format($talent.Text, $talent.Values[$rank])
            Effects = @(@{ Index = 0; Aura = $A_Dummy }); Fields = @{ 208 = 18 }
        }
    }
}

return $spells
