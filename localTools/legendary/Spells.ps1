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
       Visual = @{ Clone = 46; Precast = 0; Cast = 0; Impact = 121 } }
)

return $spells
