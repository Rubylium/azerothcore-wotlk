# Oathblade

Class 10 is a sword-based melee DPS class with its own spell family (19) and spellbook skill line (902). It
borrows Rogue combat/stat formulas but no Rogue spells or trainers.

Its talents are a retail-style talent tree, not WotLK's: an Oathblade class tree (defence, utility, resources)
and a Swordcraft spec tree (single-target, area and Flawless Form branches), 36 and 35 points at level 80 against
50 and 49 ranks, so a build has to choose. The trees are `localTools/oathblade/talentTree.json`; the engine is
mod-custom-classes `TalentTree.cpp` and the window `FrameXML/TalentTree.lua`. The rank spells are generated from
the same JSON (ids 90900-90961 kept from the old tree, new ones at 91000+), and their effects live in `src/`,
read with `GetTalentValue` next to the values their tooltips quote.

The permanent class entry, race choices, starting sword, equipment proficiencies, class colour and creation
icon are generated from `localTools/customClasses/classes.json`. Native spell/talent DBC rows are authored in
`localTools/oathblade/Spells.ps1`; combat, Flow/Tempo and level unlocks live in `src/`. The world update in
`data/sql/updates/pending_db_world/` binds the spell scripts.

Swift Cut is free and restores Energy, so the rotation never waits for an auto-attack. Alternating Swift Cut
and Precise Thrust gains extra Flow. Finishers spend 2-5 Flow. From level 12, techniques and finishers build
Tempo; 12 Tempo automatically starts an eight-second Flawless Form burst. One AoE builder and two AoE finishers
make the same loop work in dungeons. Baseline abilities unlock through level 70 on login/level-up without a
trainer, and downleveling removes abilities above the new level.

Only the class creation icon is custom art. Spell and talent icons deliberately use stock WotLK icons.
The supplied OGGs are packaged under `Sound/Spells/Custom/Oathblade`: quick 1H hits for builders, metal hits
for Reversal, heavier 2H hits for ordinary finishers, one AoE sound per cast, and the
`finished_big_hit` variants for Final Edict / Grand Flourish.
