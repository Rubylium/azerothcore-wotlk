# The legendaries' spell data (modules/mod-legendary, .agents/plans/legendary-items/legendary-items.DESIGN.md): the
# spells their powers cast. Ids 97000-97999.
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

return $spells
