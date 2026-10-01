# mod-barbarian

The Barbarian (Barbare), class 14: an energy fighter in mail who rages, bleeds its prey with Carnage and finishes the
wounded. A remake of Ascension's Conquest-of-Azeroth Barbarian on our class framework (mod-custom-classes), its looks
and sounds imported from the Ascension client.

- Class entry: `localTools/customClasses/classes.json` (template Rogue for its formulas, mail from level 1, axes, maces,
  polearms, swords, thrown and shields; the warrior races).
- Spell data: `localTools/barbarian/Spells.ps1` (ids 97100-97299, talent ranks 97300-97499, spell family 20).
- Talent trees: `localTools/barbarian/talentTree.json` (a class tree and the Brutalité specialization; Chasseur de têtes
  and Ascendance to come).
- Looks: `localTools/barbarian/ascensionVisuals.json`, imported by `localTools/ascensionImport/importVisuals.py` into
  `client-assets/imported` (rows numbered from 75000, appended by `localTools/patchSinisterStrike.ps1`; files shipped
  in patch-Z by `localTools/mpq-builder/patchFiles.js`). Icons the stock client lacks are in `client-assets/files`.

## Mechanics

- **Enragé**: the enrage auras carry the Enrage dispel type, so the core raises AURA_STATE_ENRAGE; Fracas,
  Déchaînement and Outrage ask for it (their client buttons grey out too). Every enrage also gives 20% more energy
  regeneration.
- **Carnage**: a bleed of up to 10 stacks, 12 s, ticking every 3 s for a share of attack power per stack
  (`barbarian.carnage_tick_ap`). Frappe barbare adds 2, Tourbillon barbare 1, Déchaînement 3, Empaler 5 below 35%.
- **Brutalité**: auto attacks have 15% chances to enrage (Rage sanguinaire, 6 s).
- Abilities by level: Frappe barbare 1, Tourbillon barbare 3, Rage débridée 5, Prise du poignet 7; the rest come from
  the talent trees and the specialization (Brutalité: Fracas and Déchaînement).

Balance numbers are live knobs (`barbarian.*`, `.tune list barbarian`).
