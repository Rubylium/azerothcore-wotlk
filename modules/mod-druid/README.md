# mod-druid

The Druid on the retail-style talent trees (localTools/druid/talentTree.json, mod-custom-classes TalentTree.cpp):
Astral Power and the Eclipses, Starfall and Fury of Elune, Primal Wrath and the cat's bleeds, Ironfur and the bear,
Efflorescence, Cenarion Ward, Germination and Flourish, and the effects of its talents and abilities that spell data
alone cannot carry. Spell data: localTools/druid/Spells.ps1 (spell ids 95900-96199; new family flags in word 2,
0x400000-0x80000000) and localTools/druid/StockSpells.ps1 (the stock Druid spells changed in place).

The Druid keeps mana, its forms, energy and combo points (cat) and rage (bear). Shred works from any side (its
behind-the-target requirement is cleared in `data/sql/db-world/base/druid_spells.sql`), Tiger's Fury gives 15% physical
damage (and 50 energy to Combat farouche), Swipe (Cat) strikes all around the cat within 8 yd (it was a cone), Force of
Nature comes back in 1 min, Tranquility in 3 min and Frenzied Regeneration in 1 min 30 s.

## Three spec trees, four roles

The client draws exactly three spec trees, in the WotLK tabs' order: Équilibre, Combat farouche, Restauration. Combat
farouche holds both the cat and the bear, as in WotLK. Its first node is a choice, Voie du prédateur (5% physical
damage) or Voie du gardien (10% stamina); its single-target and AoE presets are the cat's, and it has a third, a tank
preset (`"kind": "tank"` in the tree's presets, `TankBuild` in custom_talent_tree, "Recommandé : tank" in the window)
that takes the guardian's path and the bear's nodes (Cuir épais, Réaction naturelle, Crinière étagée, Encorner,
Gardien galactique, Protecteur de la meute, Endurance d'Ursoc, Incarnation : Gardien d'Ursoc).

A bot whose build holds a tank-build node that neither other preset holds (the guardian's path) is a tank:
TalentTree.cpp keeps it on the tank build wherever it goes (the AoE build in dungeons is the cat's), and the bots
(mod-playerbots `DruidRetail::IsBearTank`) play it as a bear, where they read WotLK's Thick Hide before. The bots'
premade specs stay balance 0, bear 1, resto 2, cat 3: the factory and the combat bench put "bear" on Combat farouche's
tank build (preset 3 of `SetTalentBuildPreset`) and "cat" on its cat builds. A WotLK bear bot (Thick Hide) moved to the
trees takes the tank build.

## The specializations

- **Équilibre** (spec passive 96180) fights with Astral Power (Puissance astrale, 96000): an aura of up to 100 stacks,
  the resource shown the way the other reworks show theirs. Wrath 8, Starfire 10, Moonfire 2, Sunfire 2, Stellar
  Flare 8, a shooting star 3, each strike of Fury of Elune 5, Équilibre de la nature 1 a second, Astral Communion 60.
  Starsurge (Éruption stellaire, 40, astral damage) and Starfall (Météores, 50: for 8 s, every second, the enemies
  within 30 yd of the Druid that fight, and the pack around its target) spend it; it fades 15 s after combat. Two
  Wraths out of an Eclipse bring the Lunar Eclipse (15 s: Starfire 20% quicker, arcane damage 15% more, its splash
  wider), two Starfires the Solar one (15 s: Wrath 20% quicker, nature damage 15% more); Celestial Alignment (3 min,
  20 s, 10% haste) or Incarnation : Élu d'Élune (3 min, 30 s, 10% haste and spell critical strike) put up both.
  Starfire splashes 30% of its damage on the enemies within 8 yd of its target (60% in the Lunar Eclipse). Sunfire
  (Éclat solaire: nature damage and a 12 s effect; with Éclat solaire amélioré on the enemies within 8 yd of the target
  too), Stellar Flare (Embrasement stellaire, 1.5 s, a 24 s effect), Shooting Stars (10% of Moonfire's and Sunfire's
  ticks), Starlord (3% haste a stack, 3), Twin Moons (Moonfire on a second enemy, 10% more), Soul of the Forest (an
  Eclipse gives 10 or 20 Astral Power), Force of Nature (the stock treants, 1 min), Fury of Elune (1 min, 8 s around
  the target), Starweaver or Orbit Breaker (every 15 shooting stars, a full moon), and the WotLK Balance talents
  (Moonfury, Wrath of Cenarius, Vengeance, Nature's Grace, Earth and Moon...).
- **Combat farouche** (96181; Mangle in both forms, both Thrashes and Ironfur come with the specialization):
  - the cat: Thrash (Rosser (félin), 40 energy: physical damage and a 15 s bleed within 8 yd, a combo point; Swipe
    gives one too), Feral Frenzy (45 s: 350% weapon damage, a bleed and 5 combo points), Primal Wrath (a finishing move,
    20 energy: the enemies within 8 yd struck for 5% of the attack power a combo point and bled as by a Rip of those
    points), Bloodtalons (three
    different techniques within 4 s: the next 2 Rips, Ferocious Bites or Primal Wraths 30% stronger), Predatory
    Swiftness (a finishing move, 20% a combo point: the next Regrowth instant and free), Soul of the Forest (5 energy a
    combo point spent), Berserk, Incarnation : Avatar d'Ashamane (3 min, 20 s: the techniques 25% cheaper, physical
    damage 15% more), and the WotLK feral talents (Ferocity, Savage Fury, Predatory Strikes, Primal Fury, Predatory
    Instincts, Rend and Tear, King of the Jungle, Leader of the Pack, Primal Gore...);
  - the bear: it takes 50% less damage in bear form (this module, the bench's tuning), Thrash (Rosser (ours), 6 s:
    the same, its bleed stacking 3 times, 5 rage), Ironfur (Fourrure de fer, 40 rage, off the global cooldown: 25%
    armor a stack for 8 s, 3 stacks; Crinière étagée doubles it now and then), Gore (Thrash, Swipe, Maul and
    Moonfire: 15% to bring Mangle back, which then gives 4 more rage), Galactic Guardian (5%
    of the bear's hits: a free Moonfire and 8 rage), Survival Instincts, Frenzied Regeneration, Incarnation : Gardien
    d'Ursoc (3 min, 30 s: 30% maximum health, 10% damage, Mangle and Thrash without a cooldown), and the WotLK bear
    talents (Thick Hide, Natural Reaction, Protector of the Pack, Survival of the Fittest in the class tree...).
- **Restauration** (96182: healing 10% more; Swiftmend and Wild Growth come with the specialization): Efflorescence
  (30 s on a spot: every 2 s the 3 most injured allies within 10 yd; one at a time, its aura on the Druid shows it),
  Cenarion Ward (30 s: the ally's first damage taken puts a heal over time on it), Ironbark (1 min 30 s: 20% less
  damage for 12 s on an ally), Flourish (Épanouissement, 1 min 30 s: the Druid's heals over time on the group within 60
  yd last 8 s longer), Germination (a Rejuvenation on an ally who has one also gives a second one), Abundance
  (Regrowth 6% stronger for each of the Druid's Rejuvenations on the group, 5 at most), Soul of the Forest (Swiftmend:
  the next Regrowth 150% stronger), Cultivation (Rejuvenation 40% stronger below 60% health), Croissance exubérante
  (Wild Growth a sixth target: the Glyph of Wild Growth's aura, held while the talent is), Incarnation : Arbre de vie
  (3 min, 30 s: healing 15% more, Rejuvenation 30% cheaper, Regrowth instant), Nature's Swiftness, and the WotLK healing
  talents.
- **Class tree** (Druide): Renewal (30% of the maximum health, every form, 1 min 30 s), Wild Charge (in bear form
  Feral Charge, in cat form its leap), Ursol's Vortex (10 s of a 50% snare within 8 yd of a spot) or Mass Entanglement
  (roots on the target and the enemies within 10 yd), Typhoon, Stampeding Roar (the group within 15 yd 60% faster for 8
  s), Mighty Bash (a 4 s stun in every form), Heart of the Wild (Cœur sauvage, 5 min: 20% damage and healing for 45 s)
  or Improved Barkskin, Omen of Clarity, and the WotLK utility talents (Feral Swiftness, Furor, Naturalist, Survival of
  the Fittest, Master Shapeshifter, Intensity, Nature's Majesty...).

## Tuning

The WotLK kit keeps its numbers; the new spells carry spell power and attack power coefficients in
`data/sql/db-world/base/druid_spells.sql`, the shares this module works out sit at the top of `src/DruidTalents.cpp`
(with the cat's and the bear's own damage factors, the cat's pack techniques' factor and the bear's damage taken), and
the specializations' passives scale what is left (Balance: Wrath and Starfire 45% less, Starsurge 35% less). Past eight
enemies each area hit takes sqrt(8 / enemies), as the Shaman's and the Warlock's.

Measured on the combat bench (`localTools/combatBench/runBench.ps1`, 2026-10-01, key +10, item level ~244, 60 s, two
rounds, beside the Fire Mage of the same run, whose own single target swung 3.5k-4.1k):

| Layout | Fire Mage | Équilibre | Combat farouche (cat) |
| --- | --- | --- | --- |
| single | 3.5k-4.1k | ~111% | ~112% |
| pack of 5 | 13.1k-13.9k | ~119% | ~99% |
| pack of 12 | 27.2k-30.5k | ~90% | ~57% |

Starfall's coefficient went down a little after these rounds (0.17 to 0.15) for the pack of 5. The cat is a melee: the
bench's rings leave part of a big pack out of reach.

The bear alone (`tank` and `tankpack` layouts): 3.3k damage a second on the boss at 0.9k damage taken a second, without
dying (Protection Warrior 2.7k / 0.8k, Blood Death Knight 1.6k / 1.3k in the same setup); 6.7k on the pack of 5 at 1.7k
taken (Warrior 6.3k / 1.6k, Death Knight 2.8k / 2.1k; all three died once there, without a healer).

Restoration, alone with a Fire Mage and a Fury Warrior under a 15% group pulse (`-pulse 15`, single layout, 60 s): about
4.5k-4.8k healing a second at ~20% overheal, next to 5.0k for the Holy Priest in the same setup; Efflorescence,
Cenarion Ward (with a tank), Flourish and the Tree of Life all heal.

Bots: the retail actions sit above the stock Druid ones and every one checks its spell is known (mod-playerbots
`DruidRetail.h`); the cat and the bear are told apart by the guardian's path (`DruidRetail::IsBearTank`). Bench words:
`balance`, `cat`, `bear` (the tank build), `resto`.
