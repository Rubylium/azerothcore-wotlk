# mod-barbarian

The Barbarian (Barbare), class 14: an energy fighter in mail who rages, bleeds its prey with Carnage and finishes the
wounded. A remake of Ascension's Conquest-of-Azeroth Barbarian on our class framework (mod-custom-classes), its looks
and sounds imported from the Ascension client.

- Class entry: `localTools/customClasses/classes.json` (template Rogue for its formulas, mail from level 1, axes, maces,
  polearms, swords, thrown and shields; the warrior races).
- Spell data: `localTools/barbarian/Spells.ps1` (ids 97100-97299, talent ranks 97300-97599, spell family 20: word 0
  of its flags the class and Brutalité abilities, word 1 the Chasseur de têtes and Ascendance ones).
- Talent trees: `localTools/barbarian/talentTree.json`: a class tree and three specializations, Brutalité (melee),
  Chasseur de têtes (ranged) and Ascendance (support).
- Looks: `localTools/barbarian/ascensionVisuals.json`, imported by `localTools/ascensionImport/importVisuals.py` into
  `client-assets/imported` (rows numbered from 75000, appended by `localTools/patchSinisterStrike.ps1`; files shipped
  in patch-Z by `localTools/mpq-builder/patchFiles.js`). Icons the stock client lacks are in `client-assets/files`.

## Mechanics

- **Enragé**: the enrage auras carry the Enrage dispel type, so the core raises AURA_STATE_ENRAGE; Fracas,
  Déchaînement and Outrage ask for it (their client buttons grey out too). Every enrage also gives 20% more energy
  regeneration.
- **Carnage**: a bleed of up to 10 stacks, 12 s, ticking every 3 s for a share of attack power per stack
  (`barbarian.carnage_tick_ap`). Frappe barbare adds 2, Tourbillon barbare 1, Déchaînement 3, Empaler 5 below 35%.
- **Brutalité**: auto attacks have 15% chances to enrage (Rage sanguinaire, 6 s); Tourbillon barbare deals 45% more.
  Fracas and Déchaînement.
- **Chasseur de têtes**: throws its own melee weapon up to 30 yd (melee damage class, its weapon's damage; the blow
  lands with the missile). In combat it throws an axe at its target on its own every 2 s (`barbarian.auto_throw_ms`),
  and regenerates 25% more energy. Lancer d'arme and Lance du chasseur de têtes; Hache berserker marks armor,
  Étripeur bleeds, Danse des haches spreads every throw to five more enemies.
- **Ascendance**: the support, as Augmentation on retail - but its buffs are counted. Puissance ancestrale (4 nearest
  allies, 12 s, extended a second by Frappe ancestrale and Coup de fût up to 20 s), Santé ! (one ally, 18 s), Chant
  des ancêtres (the raid, 15 s) and Éclaboussures make it deal a share of every hit of a buffed ally as **Écho
  ancestral** (8%, 6%, 5%: `barbarian.*_echo_pct`): its own damage, so Details credits the buffs to it. Its raid auras
  (Présence ancestrale: 10% attack power and 3% physical crit; Fureur ancestrale: 20% melee haste and 5% casting speed)
  are in the stacking groups of the WotLK buffs they match. 10% more frost damage, 40% less from Tourbillon barbare
  (its damage is in the echoes); its Tankard fills a charge every 3 s in combat (5, 7 with Chope pleine), Coup de fût
  empties it into its frost (15% a charge) and heals 1% a charge.
- Abilities by level: Frappe barbare 1, Tourbillon barbare 3, Rage débridée 5, Prise du poignet 7; the rest come from
  the talent trees and the specialization (Brutalité: Fracas and Déchaînement).

Balance numbers are live knobs (`barbarian.*`, `.tune list barbarian`). Tuned on the combat bench against the Fire
mage (`.agents/docs/systems/combat-bench.md`): Brutalité and Chasseur de têtes about even on one target, Brutalité
ahead on packs; Ascendance lower on its own, its echoes carrying it as the group's damage grows.
