# The stock Hunter spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Hunter
# (localTools/hunter/talentTree.json, modules/mod-hunter). Each entry names its spells by family and English name
# (every rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the
# field numbers), and optionally new Effects, a Description and an AuraDescription.
#
# The Hunter fights with Focus (power type 2, 100 at most; the core regenerates it, and the server's ChrClasses gives
# the class that power - classes.json stockTalentTrees powerType). Every spell of the class that cost mana costs Focus
# here: shots and strikes a retail-like amount, utility nothing. Field 204 (a share of the base mana) is cleared, 42
# is the flat cost. A spell named here is only changed when it had a cost: the triggered spells that share a name
# (Volley's arrows) are left alone.

$focus = 2
$hunter = 9

function Focus([int]$cost, $extra = @{}) {
    $fields = @{ 41 = $focus; 42 = $cost; 204 = 0 }
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$edits = @(
    # --- Shots and strikes: retail-like Focus costs ---------------------------------------------------------------
    # Arcane Shot and Multi-Shot lose their cooldowns and the categories they shared with Explosive Shot and Aimed
    # Shot: Focus paces them now
    @{ Family = $hunter; Name = 'Arcane Shot'; Fields = (Focus 30 @{ 1 = 0; 30 = 0 }) },
    @{ Family = $hunter; Name = 'Multi-Shot'; Fields = (Focus 30 @{ 1 = 0; 30 = 0 }) },
    # Steady Shot costs nothing; mod-hunter gives Focus back as it lands
    @{ Family = $hunter; Name = 'Steady Shot'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Serpent Sting'; Fields = (Focus 15) },
    @{ Family = $hunter; Name = 'Scorpid Sting'; Fields = (Focus 10) },
    @{ Family = $hunter; Name = 'Viper Sting'; Fields = (Focus 10) },
    # Aimed Shot: retail's 2.5 s cast (index 19), a 12 s recharge that mod-hunter turns into charges, off Multi-Shot's
    # category
    @{ Family = $hunter; Name = 'Aimed Shot'; Fields = (Focus 35 @{ 1 = 0; 28 = 19; 29 = 12000; 30 = 0 })
       Description = "Un tir soigneusement ajusté qui inflige de lourds dégâts physiques et réduit de 50% les soins reçus par la cible pendant 10 s. Précision : 2 charges." },
    # Kill Shot: 10 s instead of 15, and usable from melee range (40 yd, range 5, without the ranged weapon's dead
    # zone): Survival executes too
    @{ Family = $hunter; Name = 'Kill Shot'; Fields = (Focus 10 @{ 30 = 10000; 46 = 5 }) },
    @{ Family = $hunter; Name = 'Chimera Shot'; Fields = (Focus 35) },
    @{ Family = $hunter; Name = 'Explosive Shot'; Fields = (Focus 25) },
    @{ Family = $hunter; Name = 'Black Arrow'; Fields = (Focus 10) },
    @{ Family = $hunter; Name = 'Volley'; Fields = (Focus 40) },
    # Raptor Strike: an instant strike on the global cooldown (not on the next swing: attributes 0x404 cleared), no
    # cooldown, Survival's spender
    @{ Family = $hunter; Name = 'Raptor Strike'; Fields = (Focus 30 @{ 1 = 0; 4 = 0x50000; 30 = 0; 205 = 133; 206 = 1500 })
       Description = "Une attaque puissante qui inflige les dégâts de l'arme plus un bonus. Survie : la technique de corps à corps qui dépense la focalisation." },
    @{ Family = $hunter; Name = 'Mongoose Bite'; Fields = (Focus 20) },
    @{ Family = $hunter; Name = 'Wing Clip'; Fields = (Focus 20) },
    @{ Family = $hunter; Name = 'Counterattack'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Tranquilizing Shot'; Fields = (Focus 10) },
    # Kill Command (34026, one rank): retail's - the pet strikes the target (mod-hunter), 30 Focus, 7.5 s, 50 yd. Its
    # script (spell_hun_kill_command) still asks for a pet; its dummy aura, which the core read, is gone
    @{ Id = 34026; Fields = (Focus 30 @{ 1 = 0; 29 = 7500; 30 = 0; 40 = 0; 46 = 37; 205 = 133; 206 = 1500 })
       Effects = @(@{ Index = 0; Effect = 3; TargetA = 6 })
       Description = "Ordonne à votre familier de se jeter sur la cible, qui subit de lourds dégâts physiques. Maîtrise des bêtes : jusqu'à 2 charges. Survie : ne coûte rien et vous rend 15 points de focalisation."
       AuraDescription = '' },
    # Rapid Fire (the haste one every spec keeps): 2 min instead of 5
    @{ Family = $hunter; Name = 'Rapid Fire'; Fields = (Focus 0 @{ 30 = 120000 }) },

    # --- Utility: free ------------------------------------------------------------------------------------------------
    @{ Family = $hunter; Name = 'Bestial Wrath'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Intimidation'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Scatter Shot'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Silencing Shot'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Wyvern Sting'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Concussive Shot'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Distracting Shot'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = "Hunter's Mark"; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Misdirection'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Feign Death'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Disengage'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Deterrence'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Flare'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Scare Beast'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Freezing Trap'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Freezing Arrow'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Frost Trap'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Immolation Trap'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Explosive Trap'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Snake Trap'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Mend Pet'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Revive Pet'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Eyes of the Beast'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = "Master's Call"; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Readiness'; Fields = (Focus 0) },
    @{ Family = $hunter; Name = 'Trueshot Aura'; Fields = (Focus 0) }
)

return $edits
