# The stock Priest spells localTools/patchSinisterStrike.ps1 changes in place for the retail-style Priest
# (localTools/priest/talentTree.json, modules/mod-priest). Each entry names its spells by family and English name
# (every rank: a trainer's spells keep their chain) or by id, and gives the fields to write (see Spells.ps1 for the
# field numbers), and optionally new Effects, a Description and an AuraDescription.
#
# The Priest keeps mana. What changes: snappier casts (the fillers at 1.5 s, the big heals at 2 s, nothing longer),
# Devouring Plague as Shadow's Insanity spender (no mana, 6 s, mod-priest checks and spends the Insanity), shorter
# Penance, Divine Hymn and Shadowfiend cooldowns, and a cheaper Smite for Discipline's Atonement. A spell named here is
# only changed when it had a cost: the triggered spells that share a name (Penance's bolts) are left alone.

$priest = 6

function Fields($extra = @{}) {
    $fields = @{}
    foreach ($key in $extra.Keys) { $fields[$key] = $extra[$key] }
    return $fields
}

$edits = @(
    # --- Snappy casts -------------------------------------------------------------------------------------------------
    # Smite 2.5 s -> 1.5 s (index 16), and cheaper: Discipline's Atonement filler
    @{ Family = $priest; Name = 'Smite'; Fields = (Fields @{ 28 = 16; 204 = 5 }) },
    # Holy Fire 2 s -> 1.5 s
    @{ Family = $priest; Name = 'Holy Fire'; Fields = (Fields @{ 28 = 16 }) },
    # Lesser Heal 2.5 s -> 1.5 s; Heal and Greater Heal 3 s -> 2 s (index 5); Prayer of Healing 3 s -> 2 s
    @{ Family = $priest; Name = 'Lesser Heal'; Fields = (Fields @{ 28 = 16 }) },
    @{ Family = $priest; Name = 'Heal'; Fields = (Fields @{ 28 = 5 }) },
    @{ Family = $priest; Name = 'Greater Heal'; Fields = (Fields @{ 28 = 5 }) },
    @{ Family = $priest; Name = 'Prayer of Healing'; Fields = (Fields @{ 28 = 5 }) },

    # --- Shadow: Devouring Plague on Insanity -------------------------------------------------------------------------
    # No mana (field 204 cleared, 42 the flat cost), 6 s (index 32) ticking every 1.5 s (field 98). mod-priest refuses
    # the cast below 50 Insanity, spends it, and adds the plague's instant hit (93560).
    @{ Family = $priest; Name = 'Devouring Plague'; Fields = (Fields @{ 40 = 32; 42 = 0; 98 = 1500; 204 = 0 })
       Description = "Consomme 50 points de Démence pour infliger aussitôt des dégâts d'Ombre à la cible, puis toutes les 1,5 s pendant 6 s ; les dégâts périodiques vous soignent. Ombre."
       AuraDescription = "Subit des dégâts d'Ombre toutes les 1,5 s." },

    # --- Cooldowns ----------------------------------------------------------------------------------------------------
    # Penance 12 s -> 9 s (its category cooldown)
    @{ Family = $priest; Name = 'Penance'; Fields = (Fields @{ 30 = 9000 }) },
    # Divine Hymn 8 min -> 3 min
    @{ Family = $priest; Name = 'Divine Hymn'; Fields = (Fields @{ 29 = 180000 }) },
    # Shadowfiend 5 min -> 3 min (it costs nothing, so by id)
    @{ Id = 34433; Fields = (Fields @{ 29 = 180000 }) }
)

return $edits
