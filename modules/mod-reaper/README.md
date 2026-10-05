# mod-reaper

The Faucheur (Reaper), class 15: a plate scythe-wielder who harvests souls. A remake of Ascension's
Conquest-of-Azeroth Reaper on our class framework (mod-custom-classes), with Ascension's own looks, sounds, icons and
class HUD art, and every talent of its four trees.

- Class entry: `localTools/customClasses/classes.json` (template Death Knight for its formulas, runic power, plate and
  mail, two-handers, polearms, staves, swords, axes, maces and daggers, dual wield; every race). It starts with a
  worn scythe (item 4901, the stock scythe model).
- Spell data: `localTools/reaper/Spells.ps1` (ids 98000-98399 and 98900-98902, spell family 21: word 0 of its flags
  the class kit and class tree, word 1 Moisson and Âme, word 2 Domination; bit 0x40000000 of word 2 every Faucheur
  spell).
- Talent trees: `localTools/reaper/talentTree.json` (written by `.agents/plans/reaper/buildTalentTree.scratch.py`):
  the class tree and three specializations, Moisson (damage), Âme (damage, two weapons) and Domination (tank). Rank
  spells 98400-98532: modifiers and stats are real auras on the family flags; the rest are dummies this module reads.
- Looks: `localTools/reaper/ascensionVisuals.json`, Ascension's Reaper visuals imported as they are by
  `localTools/ascensionImport/importVisuals.py` into `client-assets/imported` (rows from 80000). Icons the stock
  client lacks are in `client-assets/files/Interface/Icons`.
- Class HUD: `clientPatcher/interface/Interface/FrameXML/ClassHudReaper.lua` (`.agents/docs/systems/class-hud.md`).
- Bots: `modules/mod-playerbots/src/Ai/Class/Reaper` (Moisson, Âme, Domination as a tank).

## The resource

- **Puissance runique** (the Death Knight's bar, 0-100), built by the strikes and spent by Meurtre and the big
  abilities; it drains out of combat (core `Player::Regenerate`, for any runic power class).
- **Fragments d'âme** (aura 98016, up to 3): Faucher, Complainte, Moisson des corbeaux, Faux frémissante and some
  talents give them; every third becomes an Âme moissonnée.
- **Âmes moissonnées** (aura 98017, up to 3): Meurtre, Massacre, Frappe d'âme, Sillage d'effroi, Vent de mort give one,
  Lame spectrale and Litanie sinistre three. At 3, the **Infusion d'âme** (98018).
- The consumers spend every soul held, scaling with how many, 20% stronger when they spend all 3 (the Infusion):
  Reliquaire des perdus (a Trait d'âme a second per soul), Âmes tourmentées (an Âme tourmentée per soul: 10% less
  direct damage, each hit taken spends one and heals), Fracas d'âmes (30% more damage per soul), Faux spectrale (a
  scythe per soul, 15 s).
- Every change is sent to the class HUD: `REAPER\t<souls>:<fragments>:<infused>`.

## Mechanics

- Level kit: Faucher 1, Collecteur d'âmes (the resource, passive) 1, Meurtre 3, Déchirure d'âme 5, Foulée spectrale 8,
  Marche funèbre 10, Reliquaire des perdus 12, Griffe fantôme 14, Choc d'âme 16, Vent de mort 18, Fracas d'âmes 20,
  Traqueur de mort 24, Tonte d'âme 28, Leurre de pierre d'âme 30. The rest comes from the trees.
- With two weapons Faucher and Complainte strike with both. Foulée spectrale teleports behind the target (Shadowstep's
  stock teleport) and strikes as it lands.
- **Moisson**: Moissonneur heals for 10% of all damage dealt (`reaper.harvester_pct`); Lacération funeste (given by the
  specialization) and its heal absorb, Massacre below 35%, Moisson des corbeaux, L'heure de la moisson, Champ de
  moisson (its enemies are pulled back when they leave), Faux frémissante.
- **Âme**: Déchirure d'âme also applies Âme affaiblie (10% shadow and frost damage taken); Complainte (given by the
  specialization), Chasse-mort, Arme fantomatique (25% of every melee blow again as frost), Porteur de fin, Ombre.
- **Domination**: Tourmenteur (30% armor, 5% parry, 80% more threat; spending souls takes 3 s off Âmes tourmentées);
  Frappe d'âme (given by the specialization: heals 40% of its damage and 10% of missing health), Injonction des âmes
  (the taunt Ascension's Reaper lacked), Sillage d'effroi, Requiem, Forme renforcée, Faux spectrale, Gardien spectral
  (strikes for 15 s and shields 5 allies).
- A hidden passive (98055, `spell_proc`) carries the auto attacks it lands and the blows it parries or dodges to the
  talents that react to them.

Balance numbers are live knobs (`reaper.*`, `.tune list reaper`); the attack power coefficients are in
`data/sql/db-world/base/reaper.sql`.
