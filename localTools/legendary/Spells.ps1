# The legendaries' spell data (modules/mod-legendary, .agents/plans/legendary-items/legendary-items.DESIGN.md): the
# spells their powers cast. Ids 97000-97005 and 97700-97999 (the Barbarian's are between).
#
# A power's damage or healing is a spell of our own, cast by the wearer, with its name and icon: the combat log (and
# Details) credits the wearer with it on a line of its own.

$spells = @(
    # Marque de l'Inquisiteur's brand: what the wearer's direct damage feeds, burning over 4 sec. Ignite's row (4 sec,
    # periodic damage) made Holy and ticking every second; it cannot crit, triggers no procs and takes no caster
    # modifier - mod-legendary sets each tick to exactly its share of the damage branded.
    @{ Id = 97000; Clone = 12654; Name = "Marque de l'Inquisiteur"; Icon = 'Legendary_MarqueInquisiteur'; FallbackIconSpell = 48135; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Le sceau de l'Inquisiteur brûle ce que vous frappez."
       AuraDescription = 'Brûle : subit des dégâts du Sacré chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 6; Aura = 3; BasePoints = 0 })
       # SchoolMask holy, every second, 4 sec (DurationIndex 35), no damage class; Ex2 CANT_CRIT; Ex3 Ignite's plus
       # SUPPRESS_CASTER_PROCS and IGNORE_CASTER_MODIFIERS
       Fields = @{ 225 = 2; 98 = 1000; 40 = 35; 213 = 0; 6 = 0x20000004; 7 = 0x30050000 }
       # The target burns in gold and orange while branded: Immolate's look (visual 46), its burning state kept (kit
       # 235), no cast; each hit that feeds the brand recasts it, so its impact is Holy Vengeance's small flash (kit
       # 121) - Holy Fire's look, first used, raised a pillar of fire on every hit
       Visual = @{ Clone = 46; Precast = 0; Cast = 0; Impact = 121 } },

    # Serment de Whitemane: a killing blow leaves the wearer at 1 health and this heals them, the rolled share of their
    # health over 4 sec (every second, exact: no crit, no caster modifier, no proc). Renew's row on the wearer;
    # Guardian Spirit's save as its look - the flare as it lands (kit 232), the angel's light while it heals (11186).
    @{ Id = 97001; Clone = 139; Name = 'Serment de Whitemane'; Icon = 'Legendary_SermentWhitemane'; FallbackIconSpell = 47788; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le serment de Whitemane vous relève.'
       AuraDescription = 'Relevé par le serment de Whitemane : récupère de la vie chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 8; BasePoints = 0 })
       Fields = @{ 225 = 2; 98 = 1000; 40 = 35; 213 = 0; 28 = 1; 6 = 0x20000000; 7 = 0x20010000 }
       Visual = @{ Clone = 12115; State = 11186 } },
    # Its 3 min: shown on the wearer as a debuff (Forbearance's row, a dummy aura)
    @{ Id = 97002; Clone = 25771; Name = 'Serment de Whitemane'; Icon = 'Legendary_SermentWhitemane'; FallbackIconSpell = 47788; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le serment de Whitemane vous a déjà relevé.'
       AuraDescription = 'Le serment de Whitemane ne peut plus vous relever.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 4; BasePoints = 0 })
       Fields = @{ 40 = 25; 131 = 0 } },

    # Consécration de Mograine: every 10 sec in combat, consecrated ground where the wearer stands, for 6 sec.
    # Consecration's own row (a persistent area at the caster's feet, 8 yd) and look, its aura a dummy: mod-legendary
    # pulses it every second with the two spells below, around the ground. (An aura on the wearer carrying the
    # ground's look, first used, made it follow them: it did not look good.)
    @{ Id = 97003; Clone = 48819; Name = 'Consécration de Mograine'; Icon = 'Legendary_ConsecrationMograine'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La terre consacrée de Mograine brûle les ennemis et soigne les alliés qui s''y tiennent.'
       Fields = @{ 40 = 32; 95 = 4; 98 = 0; 80 = 0; 28 = 1 }
       Visual = @{ Clone = 5600; Cast = 0; Impact = 0 } },
    # Its damage on each enemy in it, every second: Smite's row, instant, exact, no visual (the ground shows it)
    @{ Id = 97004; Clone = 585; Name = 'Consécration de Mograine'; Icon = 'Legendary_ConsecrationMograine'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La terre consacrée de Mograine brûle les ennemis.'
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = @{ 225 = 2; 213 = 0; 28 = 1; 6 = 0x20000000; 7 = 0x20050000; 131 = 0 } },
    # Its healing on each ally in it, every second: Flash of Light's row, instant, exact, no visual
    @{ Id = 97005; Clone = 48785; Name = 'Consécration de Mograine'; Icon = 'Legendary_ConsecrationMograine'; FallbackIconSpell = 48819; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La terre consacrée de Mograine soigne les alliés.'
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; BasePoints = 0 })
       Fields = @{ 225 = 2; 213 = 0; 28 = 1; 6 = 0x20000000; 7 = 0x20050000; 131 = 0 } }
)

# --- The other dungeons' legendaries (mod-legendary Definitions 4-24) ---------------------------------------------------
# One spell (or two) per power, built from a handful of shapes. Every one: instant, exact (no crit, no caster
# modifier, no proc of its own), reaching anywhere (the power has chosen its target), out of every class's spell family
# so no talent touches it, and drawn with its source's look - mostly its own boss's spell visual. The icon is the
# legendary's own (one painted icon per legendary, its item's), a stock one until it is painted.
# Schools: 1 physical, 2 holy, 4 fire, 8 nature, 16 frost, 32 shadow, 64 arcane.
$exact = @{ 213 = 0; 28 = 1; 46 = 13; 6 = 0x20000000; 7 = 0x20050000; 208 = 0; 209 = 0; 210 = 0; 211 = 0 }

function Merge-Fields($extra) {
    $fields = @{}
    foreach ($key in $exact.Keys) { $fields[$key] = $exact[$key] }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

# A blow on one enemy (Smite's row)
function New-Damage($id, $name, $icon, $fallback, $school, $visual, $description) {
    @{ Id = $id; Clone = 585; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 2; TargetA = 6; BasePoints = 0 })
       Fields = (Merge-Fields @{ 225 = $school; 131 = $visual }) }
}

# A heal on one ally (Flash of Light's row)
function New-Heal($id, $name, $icon, $fallback, $school, $visual, $description) {
    @{ Id = $id; Clone = 48785; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       Effects = @(@{ Index = 0; Effect = 10; TargetA = 21; BasePoints = 0 })
       Fields = (Merge-Fields @{ 225 = $school; 131 = $visual }) }
}

# A burn on one enemy, every second for its duration (Ignite's row, as the Marque's brand): mod-legendary sets each
# tick and the core's Ignite rolls what is left into the next
function New-Burn($id, $name, $icon, $fallback, $school, $duration, $visual, $description, $auraDescription) {
    @{ Id = $id; Clone = 12654; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 6; Aura = 3; BasePoints = 0 })
       Fields = @{ 225 = $school; 98 = 1000; 40 = $duration; 213 = 0; 6 = 0x20000004; 7 = 0x30050000; 208 = 0; 209 = 0; 210 = 0; 211 = 0 }
       Visual = $visual }
}

# The same burn wearing a stock visual as it is (its impact on every feed): a visual id rather than a recipe
function New-PlainBurn($id, $name, $icon, $fallback, $school, $duration, $visual, $description, $auraDescription) {
    $burn = New-Burn $id $name $icon $fallback $school $duration $null $description $auraDescription
    $burn.Remove('Visual')
    $burn.Fields[131] = $visual
    return $burn
}

# A shield (Power Word: Shield's row): absorbs what mod-legendary sets, every school
function New-Shield($id, $name, $icon, $fallback, $targetA, $duration, $visual, $description, $auraDescription) {
    @{ Id = $id; Clone = 48066; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Effects = @(@{ Index = 0; Effect = 6; TargetA = $targetA; Aura = 69; Misc = 127; BasePoints = 0 })
       Fields = (Merge-Fields @{ 225 = 2; 40 = $duration; 131 = $visual }) }
}

# A mark the wearer carries (a dummy aura): a buff mod-legendary reads, or a rest it counts down. $clone gives it a
# helpful or harmful row (a buff or a debuff) besides its look.
function New-Mark($id, $clone, $name, $icon, $fallback, $duration, $visual, $description, $auraDescription) {
    @{ Id = $id; Clone = $clone; Name = $name; Icon = $icon; FallbackIconSpell = $fallback; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = $description
       AuraDescription = $auraDescription
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 4; BasePoints = 0 })
       Fields = @{ 40 = $duration; 131 = $visual; 28 = 1; 41 = 0; 226 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } }
}

$spells += @(
    # --- The Mechanar ---
    # Épaulières de Capacitus: a melee blow taken strikes back as Arcane, on its attacker (an arcane missile's impact)
    (New-Damage 97700 'Bouclier réfléchissant' 'INV_Legendary_EpaulieresCapacitus' 35159 64 270 `
        'Le bouclier de Capacitus renvoie une part des coups reçus.'),
    # Abaque de Pathaleon: the surge, melee, ranged and spell haste for 8 sec (Arcane Power's row and look)
    @{ Id = 97710; Clone = 12042; Name = 'Calcul de Pathaleon'; Icon = 'INV_Legendary_AbaquePathaleon'; FallbackIconSpell = 12042; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Le calcul de Pathaleon accélère vos attaques et vos sorts.'
       AuraDescription = 'Hâte augmentée.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 192; BasePoints = 0 }, @{ Index = 1; Effect = 6; TargetA = 1; Aura = 65; BasePoints = 0 })
       Fields = @{ 40 = 31; 28 = 1; 41 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } },
    # Brassards de Sepethrea: spells burn as Fire over 4 sec (Immolate's burning look, no flash per feed)
    (New-Burn 97720 'Flammes de Sepethrea' 'INV_Legendary_BrassardsSepethrea' 29964 4 35 @{ Clone = 46; Precast = 0; Cast = 0; Impact = 0 } `
        'Les flammes déchaînées de Sepethrea brûlent ce que vos sorts touchent.' 'Brûle : subit des dégâts de Feu chaque seconde.'),

    # --- Utgarde Keep ---
    # Ceinture d'Ingvar: every 5th hit, a shadow axe (a shadow bolt's look)
    (New-Damage 97730 "Hache d'ombre d'Ingvar" 'INV_Legendary_CeintureIngvar' 42751 32 64 `
        "Ingvar lance sa hache d'ombre sur votre cible."),
    # Cuirasse de Keleseth: the last stand's look while it holds (Ice Barrier's shell, a buff)
    (New-Mark 97740 43039 'Tombeau de Keleseth' 'INV_Legendary_CuirasseKeleseth' 48400 21 4302 `
        'Le givre de Keleseth vous enserre quand vous faiblissez.' 'Dégâts subis réduits.'),
    # Collier d'Annhylde: the frenzy after a kill, 10 sec (Enrage's red glow, a buff)
    (New-Mark 97750 12880 "Appel d'Annhylde" 'INV_Legendary_CollierAnnhylde' 12880 1 2817 `
        "L'appel d'Annhylde exalte chaque victoire." 'Dégâts infligés augmentés.'),

    # --- The Shattered Halls ---
    # Poignes de Kargath: the blow on the enemies around (Cleave's slash)
    (New-Damage 97760 'Lames de Kargath' 'INV_Legendary_PoignesKargath' 30739 1 219 `
        'Les lames de Kargath frappent les ennemis autour de votre cible.'),
    # Chevalière de Porung: what the blows earned, healed once a second (Vampiric Embrace's look)
    (New-Heal 97780 'Soif de Porung' 'INV_Legendary_ChevalierePorung' 15290 1 3542 `
        'La soif de sang de Porung vous soigne de vos coups.'),

    # --- The Deadmines ---
    # Ceinture à poudre de Gilnid: a kill blows up, on every enemy around it (dynamite's blast)
    (New-Damage 97800 'Poudre de Gilnid' 'INV_Legendary_PoudreGilnid' 7978 4 148 `
        'La poudre de Gilnid fait exploser vos victimes.'),
    # Moufles de Cookie: the most hurt ally fed (Cookie's Cooking, the Deadmines cook's own heal)
    (New-Heal 97810 'Ragoût de Cookie' 'INV_Legendary_MouflesCookie' 5174 8 147 `
        "Cookie sert une louche à l'allié le plus blessé."),

    # --- Drak'Tharon Keep ---
    # Bottes du roi Dred: weapon blows bleed over 6 sec (Rend's blood)
    (New-PlainBurn 97820 'Griffes du roi Dred' 'INV_Legendary_BottesDred' 48920 1 32 372 `
        'Les griffes du roi Dred lacèrent ce que vos armes touchent.' 'Saigne : subit des dégâts physiques chaque seconde.'),
    # Robe de Novos: the overhealing's shield on the ally healed, 10 sec (Divine Aegis's look)
    (New-Shield 97830 'Barrière de Novos' 'INV_Legendary_RobeNovos' 47346 21 1 10895 `
        'La barrière de Novos garde ce que vos soins ont de trop.' 'Absorbe des dégâts.'),
    # Pendentif de Tharon'ja: the share of a heal on the most hurt other ally (Return Flesh, Tharon'ja's own)
    (New-Heal 97840 'Chair rendue' 'INV_Legendary_PendentifTharonja' 53463 8 10907 `
        "Le rituel de Tharon'ja rend sa chair à un autre allié."),

    # --- The Forge of Souls ---
    # Jambières du Dévoreur: a well of souls where the wearer stands, 6 sec (a creature's Desecration: a dark shadow
    # ground of 6 yd - Death and Decay's, first used, was a death knight's, too big and too bright - its aura a dummy:
    # mod-legendary pulses it every second with the blow below)
    @{ Id = 97850; Clone = 36473; Name = 'Puits des âmes'; Icon = 'INV_Legendary_JambieresDevoreur'; FallbackIconSpell = 68820; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "Le puits des âmes du Dévoreur dévore les ennemis qui s'y tiennent."
       Fields = @{ 40 = 32; 95 = 4; 98 = 0; 80 = 0; 28 = 1; 86 = 18; 41 = 0; 226 = 0; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } },
    (New-Damage 97851 'Puits des âmes' 'INV_Legendary_JambieresDevoreur' 68820 32 0 `
        'Le puits des âmes dévore les ennemis.'),
    # Heaume de Bronjahm: a kill heals over 4 sec (Renew's row; Corrupt Soul's look, Bronjahm's own)
    @{ Id = 97860; Clone = 139; Name = "Fragment d'âme"; Icon = 'INV_Legendary_HeaumeBronjahm'; FallbackIconSpell = 68839; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = "L'âme arrachée à votre victime vous soigne."
       AuraDescription = 'Récupère de la vie chaque seconde.'
       Effects = @(@{ Index = 0; Effect = 6; TargetA = 1; Aura = 8; BasePoints = 0 })
       Fields = @{ 225 = 32; 98 = 1000; 40 = 35; 213 = 0; 28 = 1; 6 = 0x20000000; 7 = 0x20010000; 131 = 12656; 208 = 0; 209 = 0; 210 = 0; 211 = 0 } },
    # Anneau de l'âme reflétée: the mirrored share on another enemy (Mirrored Soul, the Devourer's own)
    (New-Damage 97870 'Âme reflétée' 'INV_Legendary_AnneauAmeRefletee' 69051 32 14793 `
        'Votre âme reflétée inflige vos coups à un autre ennemi.'),

    # --- The Halls of Lightning ---
    # Étincelle d'Ionar: the leap of lightning (Chain Lightning's bolt)
    (New-Damage 97880 'Surcharge statique' 'INV_Legendary_EtincelleIonar' 52658 8 36 `
        "L'étincelle d'Ionar bondit vers les ennemis proches."),
    # Poings de Loken: the nova's look on the wearer (Loken's own Lightning Nova), then its blow on each enemy
    @{ Id = 97890; Clone = 52960; Name = 'Nova de foudre'; Icon = 'INV_Legendary_PoingsLoken'; FallbackIconSpell = 52960; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'Loken libère une nova de foudre autour de vous.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = (Merge-Fields @{ 225 = 8; 131 = 11674 }) },
    (New-Damage 97891 'Nova de foudre' 'INV_Legendary_PoingsLoken' 52960 8 0 `
        'La nova de foudre de Loken frappe les ennemis proches.'),
    # Chevalière de Bjarngrim: the shield below half health, 10 sec (Power Word: Shield's bubble), and its minute
    # of rest (Forbearance's row, a debuff)
    (New-Shield 97900 'Rempart de Bjarngrim' 'INV_Legendary_ChevaliereBjarngrim' 41105 1 1 784 `
        'Le rempart de Bjarngrim vous protège quand vous faiblissez.' 'Absorbe des dégâts.'),
    (New-Mark 97901 25771 'Rempart de Bjarngrim' 'INV_Legendary_ChevaliereBjarngrim' 41105 3 0 `
        'Le rempart de Bjarngrim vous a déjà protégé.' 'Le rempart de Bjarngrim ne peut plus vous protéger.'),

    # --- The Hollow Voice: the Unique ---
    # Écho du Néant: the echo's look on the wearer as the other cooldowns jump (Shadow Nova's, Sunwell's void)
    @{ Id = 97910; Clone = 45329; Name = 'Écho du Néant'; Icon = 'INV_Unique_EchoDuNeant'; FallbackIconSpell = 62660; Cost = 0; Cooldown = 0; Level = 0; Spellbook = $false
       Description = 'La Voix creuse fait écho à vos grands pouvoirs : vos autres temps de recharge raccourcissent.'
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 1; BasePoints = 0 })
       Fields = (Merge-Fields @{ 225 = 32; 131 = 10127 }) },

    # --- L'Infini: L'Étoile captive ---
    # The collapse on each enemy: a falling star as Starfall's (its star's own visual, Balance's), Arcane
    (New-Damage 97920 'Supernova' 'INV_Legendary_EtoileCaptive' 64443 64 11040 `
        "L'étoile captive s'effondre sur vos ennemis."),
    # ... and on each ally: Prayer of Mending's golden sparkle, nothing louder
    (New-Heal 97921 'Supernova' 'INV_Legendary_EtoileCaptive' 64443 64 1714 `
        "L'étoile captive s'effondre en lumière sur vos alliés."),
    # The star itself: a buff counting down to its collapse (20 sec, Sprint's helpful row, a dummy aura, no look)
    (New-Mark 97922 2983 'Étoile captive' 'INV_Legendary_EtoileCaptive' 64443 18 0 `
        "Vos dégâts et vos soins nourrissent une étoile captive." `
        "Se nourrit de vos dégâts et de vos soins. S'effondre à la fin du temps restant."),

    # --- Gardien-chef Vorhan: Sablier de Perpétuité, the Unique ---
    # The sentence on the target, every 4 sec, Shadow: Shadow Word: Death's dark blow (visual 8069)
    (New-Damage 97930 'Perpétuité' 'INV_Unique_SablierPerpetuite' 48158 32 8069 `
        'La peine de votre cible s''alourdit à chaque sentence.'),
    # Its mark on the target: a debuff whose stacks count the sentences so far (up to 5), timed by mod-legendary
    # (Sprint's row made a dummy debuff, as the warden's own debuffs)
    @{ Id = 97931; Clone = 2983; Name = 'Perpétuité'; Icon = 'INV_Unique_SablierPerpetuite'; FallbackIconSpell = 48158; Cost = 0; Cooldown = 0; Level = 0; DummyAura = $true; MaxStacks = 5; Spellbook = $false
       Description = 'Condamné à perpétuité : chaque sentence est plus lourde que la précédente.'
       AuraDescription = 'Condamné : chaque sentence est plus lourde que la précédente.'
       Fields = @{ 4 = 0x04000000; 40 = 18 } }
)

return $spells
