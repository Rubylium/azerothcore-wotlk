# mod-death-knight

The Death Knight on the retail-style talent trees (localTools/deathknight/talentTree.json, mod-custom-classes
TalentTree.cpp): the effects of its talents and abilities that spell data alone cannot carry. Spell data:
localTools/deathknight/Spells.ps1 (spell ids 92500-92799).

## Tuning assumptions

The WotLK kit keeps its numbers; the new spells are sized against it at ~5000 attack power (raid gear before this
server's paragon, which scales everything alike), with their attack power coefficients in
`data/sql/db-world/base/death_knight_spell_bonus.sql`:

- Single target stays WotLK's (Obliterate/Frost Strike +10% in Frost, Festering Strike and its wounds replacing
  Blood Strike in Unholy, Soul Reaper as an execute). Epidemic and Glacial Advance hit no harder than Death Coil and
  Frost Strike on one enemy: they pay off on packs.
- AoE is what changed, as the Mage's did: Death and Decay is free of runes in every spec (Blood 15 s, Unholy 20 s),
  Howling Blast has no cooldown in Frost, Blood Boil has charges and spreads Blood Plague, and each spec gets cleaving
  tools (Heart Strike +3 targets and Scourge Strike +4/8 in Death and Decay, Frostscythe, Remorseless Winter,
  Frostwyrm's Fury, Bonestorm, Consumption, Epidemic, Outbreak).
- The Mage bot's M+ numbers come from Arcane Explosion on packs; a Death Knight in a pack now has a comparable area
  rotation, so a reworked DK bot is expected in the same order of magnitude (60-70k at +37-39). This is an estimate
  from the coefficients, not a measurement: run keys and read `localTools/combatTelemetry` before retuning.
