# The Hollow Voice's spell data (modules/mod-stat-growth/src/HollowVoice.cpp: Archbishop Aldric Dawnmantle, then
# Vel'thazar, the Hollow Voice). Ids 94000-94049; its sigils on the ground are ground indicator shapes (90734-90737,
# localTools/groundIndicators/shapes.json).
#
# As L'Infini's (localTools/infiniteBoss/Spells.ps1): the abilities are never cast. The script deals their damage on
# the areas it drew (MythicTuning::DealAbilityDamage) naming one of these, so the combat log and the meters read the
# ability's name; each copies a stock spell for its icon, its school set (field 225: 2 holy, 32 shadow, 4 fire).
# The debuffs are dummy auras the script stacks and reads. The looks worn by a boss or a player (the Archbishop's
# wings, the bubbles) are dummy auras too, their stock spell's visual put back (field 131) without what it does.

$holy = 2
$shadow = 32
$fire = 4
$debuff = 0x04000000

$spells = @(
    # --- Archbishop Aldric Dawnmantle ---
    @{ Id = 94000; Clone = 20271; Name = 'Jugement'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe la cible de l''Archevêque et la condamne.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94001; Clone = 2983; Name = 'Condamné'; FallbackIconSpell = 20184; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Chaque Jugement en ajoute une charge.'
       AuraDescription = 'Dégâts du Sacré subis augmentés de 15% par charge.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    @{ Id = 94002; Clone = 53595; Name = 'Marteaux bénis'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des marteaux tournoient en spirale autour de l''Archevêque.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94003; Clone = 48817; Name = 'Courroux de la chaire'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des anneaux de feu sacré s''élargissent depuis l''Archevêque.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94004; Clone = 48819; Name = 'Allées consacrées'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une allée sur deux s''embrase à travers la salle.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94005; Clone = 53385; Name = 'Lumière de l''aube'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un large cône de lumière vers un joueur.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94006; Clone = 64843; Name = 'Chœur des fidèles'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Chaque tour de lumière doit être tenue, sinon elle éclate sur tout le groupe.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94007; Clone = 642; Name = 'Prière d''absolution'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'L''Archevêque prie sous une égide de lumière. Brisez-la, ou il se soigne.'
       AuraDescription = 'Protégé par une égide de lumière.'
       Fields = @{ 40 = 21; 131 = 154 } },
    @{ Id = 94008; Clone = 48806; Name = 'Sentence d''exécution'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Partagée entre tous ceux qui se tiennent avec le marqué.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94009; Clone = 853; Name = 'Verdict des fidèles'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un marteau géant s''abat : un défenseur doit le recevoir, sinon il frappe tout le groupe.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94010; Clone = 31884; Name = 'Séraphin'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'L''Archevêque déploie ses ailes.'
       AuraDescription = 'Ailes de lumière.'
       Fields = @{ 40 = 21; 131 = 7880 } },
    @{ Id = 94011; Clone = 48801; Name = 'Sillage de cendres'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des vagues de lumière déferlent devant l''Archevêque.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94012; Clone = 48817; Name = 'Derniers sacrements'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le jugement de la Lumière.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94013; Clone = 48078; Name = 'Rayonnement sacré'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe.'
       Fields = @{ 225 = $holy } },
    @{ Id = 94014; Clone = 2983; Name = 'Sentence d''exécution'; FallbackIconSpell = 48806; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'La sentence s''abat sur vous : partagez-la.'
       AuraDescription = 'La sentence s''abat sur vous : partagez-la avec le groupe.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    # A blessed hammer: Eadric's thrown hammer (66904, Trial of the Champion) over an invisible stalker, looks only
    @{ Id = 94015; Clone = 66904; Name = 'Marteau béni'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Un marteau béni.'; AuraDescription = 'Un marteau béni.'
       Fields = @{ 40 = 21; 131 = 14215 } },
    # A light to carry (Aldric's Last Prayer), and a held tower's: a pillar of light (Beam of Light, 57772) over its
    # stalker, looks only, without the stock kit's looping sound (HV_LightPillar: it went on after the tower)
    @{ Id = 94016; Clone = 57772; Name = 'Lumière d''Aldric'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'La lumière de l''Archevêque.'; AuraDescription = 'La lumière de l''Archevêque.'
       Fields = @{ 40 = 21 }; Visual = @{ Clone = 12479; State = 'HV_LightPillar' } },

    # --- Vel'thazar, the Hollow Voice ---
    @{ Id = 94020; Clone = 31306; Name = 'Nuée charognarde'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un cône de nuée vers un joueur.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94021; Clone = 48160; Name = 'Marque vampirique'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe la cible de Vel''thazar et la marque : à trois charges, il se nourrit du coup.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94022; Clone = 2983; Name = 'Marque vampirique'; FallbackIconSpell = 48160; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Chaque Marque vampirique en ajoute une charge.'
       AuraDescription = 'À trois charges, Vel''thazar se soigne de chaque Marque vampirique.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    @{ Id = 94023; Clone = 62660; Name = 'Écho creux'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Les rites de l''Archevêque reviennent, inversés.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94024; Clone = 6215; Name = 'Murmure du doute'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe les marqués, et plus durement quiconque se tient près d''eux.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94025; Clone = 46605; Name = 'Dernière lumière'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le néant frappe quiconque est hors de la lumière.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94026; Clone = 46680; Name = 'Lances cauchemardesques'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une lance du néant jusqu''au marqué : quiconque se tient sur sa ligne est transpercé.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94027; Clone = 1122; Name = 'Chute infernale'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un infernal s''écrase : à partager, sinon il éclate sur tout le groupe.'
       Fields = @{ 225 = $fire } },
    @{ Id = 94028; Clone = 45329; Name = 'Inspiration du néant'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Vel''thazar aspire tout vers lui, puis frappe tout ce qui est proche.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94029; Clone = 45996; Name = 'Néant dévorant'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le bord de la salle dévore quiconque s''y tient.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94030; Clone = 642; Name = 'Dernière prière d''Aldric'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'L''Archevêque résiste de l''intérieur et emprisonne le démon.'
       AuraDescription = 'Emprisonné par la dernière prière d''Aldric.'
       Fields = @{ 40 = 21; 131 = 154 } },
    @{ Id = 94031; Clone = 31884; Name = 'Bénédiction d''Aldric'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La lumière d''Aldric : dégâts et soins augmentés de 20% pendant 20 s.'
       AuraDescription = 'Dégâts et soins augmentés de 20%.' },
    # Fervour of the Faithful: Aldric's Blessing at +40% for 10 s (fields 80-81: Avenging Wrath's two effects, 40 = 1:
    # 10 s), without its wings (131): many players gain it at once, at every tower and shared impact held
    @{ Id = 94045; Clone = 31884; Name = 'Ferveur des fidèles'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une mécanique tenue loin du boss : dégâts et soins augmentés de 40% pendant 10 s.'
       AuraDescription = 'Dégâts et soins augmentés de 40%.'
       Fields = @{ 40 = 1; 80 = 39; 81 = 39; 131 = 0 } },
    # --- Cast bars (HollowVoice.cpp CastBar): what a boss is about to do, the bar ending where it lands. Holy Light (the
    # Archbishop's) or Shadow Bolt (the demon's) for their casting looks, turned into a self-cast dummy (71 effect 3,
    # 86 target the caster, no aura), never interrupted (31, 214) and on no cooldown (205, 206); the server sets each
    # cast's length (HollowVoiceCastBars). Icons: the ability's own.
    @{ Id = 94141; Clone = 635; Name = 'Lumière de l''aube'; FallbackIconSpell = 53385; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un large cône de lumière vers un joueur.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94142; Clone = 635; Name = 'Courroux de la chaire'; FallbackIconSpell = 48817; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des anneaux de feu sacré s''élargissent depuis l''Archevêque.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94143; Clone = 635; Name = 'Allées consacrées'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une allée sur deux s''embrase à travers la salle.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94144; Clone = 635; Name = 'Nef consacrée'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une grille de lumière s''embrase à travers la salle.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94145; Clone = 635; Name = 'Verdict des fidèles'; FallbackIconSpell = 853; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un marteau géant : un tank le prend.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94146; Clone = 635; Name = 'Sillage de cendres'; FallbackIconSpell = 48801; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Trois cônes de cendres, l''un après l''autre.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94147; Clone = 686; Name = 'Nuée charognarde'; FallbackIconSpell = 31306; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Un cône de charognards vers un joueur.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94148; Clone = 686; Name = 'Écho creux'; FallbackIconSpell = 62660; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le cœur et l''anneau autour du démon.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94149; Clone = 686; Name = 'Allées de lécho'; FallbackIconSpell = 62660; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une allée sur deux s''assombrit à travers la salle.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94150; Clone = 686; Name = 'Trame du néant'; FallbackIconSpell = 62660; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Deux grilles de néant à travers la salle.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94151; Clone = 686; Name = 'Salve de lances'; FallbackIconSpell = 46680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des lances du démon vers chaque joueur.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94152; Clone = 686; Name = 'Lances cauchemardesques'; FallbackIconSpell = 46680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Une lance vers chacun des deux joueurs marqués.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94153; Clone = 686; Name = 'Inspiration du néant'; FallbackIconSpell = 45329; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le démon aspire la salle : éloignez-vous de lui.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94154; Clone = 686; Name = 'Croix du néant'; FallbackIconSpell = 46680; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Deux croix de néant autour du démon.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    @{ Id = 94155; Clone = 686; Name = 'Voix de la ruine'; FallbackIconSpell = 46605; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Tout le groupe dans l''égide du tank.'
       Fields = @{ 16 = 0; 31 = 0; 71 = 3; 72 = 0; 73 = 0; 86 = 1; 87 = 0; 88 = 0; 89 = 0; 95 = 0; 96 = 0; 97 = 0; 205 = 0; 206 = 0; 213 = 0; 214 = 0 } },
    # A tank's Exposure (HollowVoice.cpp Expose): struck by a mechanic that is not a tank's, once, twice
    @{ Id = 94046; Clone = 2983; Name = 'Exposition'; FallbackIconSpell = 25771; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 3; Spellbook = $false
       Description = 'Un tank frappé par une mécanique qui n''est pas la sienne.'
       AuraDescription = 'Frappé par une mécanique à éviter : la même vous frappera bien plus fort une deuxième fois, et vous tuera la troisième.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    @{ Id = 94032; Clone = 2983; Name = 'Lumière d''Aldric'; FallbackIconSpell = 53563; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'Vous portez la lumière d''Aldric jusqu''au démon.'
       AuraDescription = 'Vous portez la lumière d''Aldric jusqu''au démon.'
       Fields = @{ 40 = 21 } },
    @{ Id = 94033; Clone = 45329; Name = 'Sermon creux'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Chaque tour du néant doit être tenue, sinon elle éclate sur tout le groupe.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94034; Clone = 46605; Name = 'Voix de la ruine'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe : sous la dernière égide d''Aldric, tenue par un défenseur, deux fois moins.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94036; Clone = 46161; Name = 'Pulsation creuse'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94037; Clone = 46605; Name = 'Silence'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La fin.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94038; Clone = 46680; Name = 'Croix du néant'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Des lignes de néant traversent la salle.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94039; Clone = 47241; Name = 'Déchirement'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le démon s''arrache au corps de l''Archevêque.'
       Fields = @{ 225 = $shadow } },
    @{ Id = 94040; Clone = 61290; Name = 'Nuée tourbillonnante'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Trois nuées tournent autour de Vel''thazar.'
       Fields = @{ 225 = $shadow } },
    # His true form: an NPC Shadowform's look (16592) without its damage changes
    @{ Id = 94041; Clone = 16592; Name = 'Forme véritable'; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; Spellbook = $false
       Description = 'La forme véritable de Vel''thazar.'; AuraDescription = 'La forme véritable de Vel''thazar.'
       Fields = @{ 40 = 21; 131 = 3619 } },
    # The healers' work in the quiet moments between mechanics: a raid-wide hit in pulses (HollowVoice.cpp Litany)
    @{ Id = 94042; Clone = 48078; Name = 'Litanie de pénitence'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe, par vagues.'
       Fields = @{ 225 = $holy } },
    # The spinning swarms' bite: each hit a charge for a few seconds, each charge making the next hit hurt more
    # (HollowVoice.cpp SpinSwarmStackPct) - one crossing is nothing, a second soon after is a lot, a third kills
    @{ Id = 94044; Clone = 2983; Name = 'Morsures de la nuée'; FallbackIconSpell = 61290; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 10; Spellbook = $false
       Description = 'Chaque morsure des nuées tourbillonnantes en ajoute une charge.'
       AuraDescription = 'Les nuées tourbillonnantes vous infligent 50% de dégâts en plus par charge.'
       Fields = @{ 4 = $debuff; 40 = 21 } },
    @{ Id = 94043; Clone = 46161; Name = 'Lamentation creuse'; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Frappe tout le groupe, par vagues.'
       Fields = @{ 225 = $shadow } }
)

return $spells
