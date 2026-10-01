# mod-shaman

The Shaman on the retail-style talent trees (localTools/shaman/talentTree.json, mod-custom-classes TalentTree.cpp):
Maelstrom, Maelstrom Weapon, Lava Surge, Crash Lightning, Riptide's charges, the healing totems, and the effects of its
talents and abilities that spell data alone cannot carry. Spell data: localTools/shaman/Spells.ps1 (spell ids
95300-95599; new family flags in word 2, 0x8000-0x10000000) and localTools/shaman/StockSpells.ps1 (the stock Shaman
spells changed in place).

The Shaman keeps mana and its WotLK totems. The shocks no longer share a cooldown: Earth Shock and Frost Shock have
none, Flame Shock keeps its own 6 s; Chain Lightning has no cooldown either and reaches 5 enemies.

## The specializations

- **Élémentaire** (spec passive 95580: Earth Shock +150%; Lightning Bolt, Chain Lightning, Lava Burst and Frost
  Shock -15%, the procs and the spenders carry the specialization) fights with Maelstrom (Maelström, 95400): an aura of up to 100 stacks (150 with Maelström gonflé), the resource
  shown the way the other reworks show theirs. Lightning Bolt 8, each enemy Chain Lightning hits 4, Lava Burst 10,
  Icefury 25, each Icefury-empowered Frost Shock 8, each Lightning Overload 3. Earth Shock (60, no mana), Earthquake
  (Séisme, 60: a shake every second for 6 s within 8 yd of the chosen spot) and Elemental Blast (90, a 2 s bolt and 6%
  spell haste or critical strike for 10 s) spend it. Flame Shock's ticks have 15% (+5% a rank of Déferlante amplifiée)
  to bring Lava Surge: the next Lava Burst instant, a charge given back. Lava Burst has 2 charges with Écho des
  éléments. Stormkeeper (1 min: the next 2 Lightning Bolts or Chain Lightnings instant and 150% stronger, 3 with
  Tempête éternelle), Icefury (25 s: the next 4 Frost Shocks twice as strong) or Elemental Mastery, Master of the
  Elements (Lava Burst makes the next Nature or Frost spell 20% stronger), Echoes of the Great Sundering (Earth Shock
  and Elemental Blast make the next Earthquake twice as strong), Ascension (3 min: Lava Burst without a cooldown for
  15 s, and a Lava Burst at every enemy with the Shaman's Flame Shock), Totem of Wrath, the primordial Fire Elemental
  (2 min 30 s). Maelstrom fades 15 s after combat.
- **Amélioration** (95581: Chain Lightning +100%; Stormstrike, Lava Lash, Crash Lightning, Ice Strike and Sundering
  -20%; Lightning Bolt, Chain Lightning and the heals half the mana; Dual Wield, Stormstrike and Lava Lash come with the specialization)
  charges Maelstrom Weapon (Arme du Maelström, 95402): 20% of the auto attacks (35% with Vents indomptés, every one
  during Doom Winds), each Stormstrike, Lava Lash and Ice Strike with Assaut élémentaire (2 with Maelström primordial)
  and the Feral Spirit wolves every 3 s; up to 5 stacks, 10 with Maelström déchaîné. A Lightning Bolt, Chain Lightning
  or heal spends up to 5: 20% faster and 12% stronger a stack (5 stacks: instant and 60% stronger). Crash
  Lightning (12 s, nature damage within 8 yd): two enemies or more, Stormstrike, Lava Lash and Ice Strike also hit up to
  6 enemies near their target for 80% of their damage for 12 s. Assaut en fusion: Lava Lash spreads Flame Shock to 4
  enemies within 10 yd. Ice Strike (15 s) empowers the next Frost Shock (x2), Sundering (40 s) strikes and stuns
  around the Shaman, Doom Winds (1 min), Hot Hand (Lava Lash x2 and 75% quicker for 8 s), Stormflurry (Stormstrike may
  strike again), Legacy of the Frost Witch (5 stacks spent: Stormstrike again and 5% physical damage), Ascension
  (3 min: the winds strike the target and the enemies near it, Stormstrike 60% quicker for 15 s), Feral Spirit.
- **Restauration** (95582: healing +15%, Chain Heal and Riptide +10% again; Riptide comes with the specialization):
  Riptide (2 charges with Écho de la marée), Healing Rain (1.5 s, aimed at a spot: up to 6 allies within 10 yd every 2
  s for 10 s), Unleash Life (15 s: a heal, and the next Healing Wave, Lesser Healing Wave or Chain Heal 35% stronger),
  Cloudburst Totem (30 s: 30% of the healing for 15 s, then shared among 6 injured allies), Spirit Link Totem (3 min:
  for 6 s the group within 12 yd takes 10% less and shares its health evenly every second), Healing Tide Totem (3 min:
  the group within 40 yd every 2 s for 10 s), Wellspring or Tidal Force, Ascension (3 min: half of every heal copied
  to up to 5 injured allies for 15 s) or Earthen Wall Totem (1 min: a shield of twice the spell power on the group
  within 20 yd), Deluge (the wave heals stronger on a target with Riptide), Torrent, Mana Tide, Cleanse Spirit, Tidal
  Waves, and the WotLK healing talents.
- **Class tree** (Chaman): Earth Shield, Astral Shift (40% less damage for 12 s, 2 min), Spirit Walk or Gust of Wind,
  Thunderstorm, Nature's Swiftness, Capacitor Totem (a 3 s stun within 8 yd after 2 s), Lightning Lasso or Earthgrab
  Totem, Ancestral Guidance (10 s: 25% of the damage and healing done heals 3 injured allies), Elemental Orbit (an Earth
  Shield on an ally puts one on the Shaman too, beside its Lightning or Water Shield), and the WotLK utility talents
  (Toughness, Anticipation, Ancestral Knowledge, Elemental Warding, Elemental Precision, Totemic Focus...).

Charges (shown as the stacks of an aura, like the other reworks'): Lava Burst (2, Elemental with Écho des éléments),
Riptide (2, Restoration with Écho de la marée).

The "totems" of the new kit (Capacitor, Cloudburst, Spirit Link, Healing Tide, Earthen Wall) are held by this module on
the spot they were dropped at; they summon no creature. Dual wielding goes with Enhancement's Dual Wield: the module
takes it back when the Shaman changes specialization.

## Damage and healing that lands later

Lightning Bolt, Elemental Blast and Icefury travel: their hit lands after their cast has spent the procs that boost
it (Maelstrom Weapon, Stormkeeper, Master of the Elements), so the cast records its share and the hit takes it. The
other spells read the procs on their hit, which comes before their cast spends them.

## Tuning

The WotLK kit keeps its numbers; the new spells carry spell power or attack power coefficients in
`data/sql/db-world/base/shaman_spells.sql`, the shares this module works out sit at the top of `src/ShamanTalents.cpp`,
and the specializations' passives scale what is left. Past eight enemies each area hit (Crash Lightning, Sundering,
Earthquake's shakes, Ascension's winds) takes sqrt(8 / enemies): the casters' falloff starts at five, but these areas
are 8 yd across, so a big pack is already only partly in reach.

Measured on the combat bench (`localTools/combatBench/runBench.ps1`, 2026-10-01, key +10, item level ~244, 60 s, two
rounds each, the Fire Mage in the same runs, no tank bot; the shaman words are `ele`, `enh` and `resto`):

| | Fire Mage | Élémentaire | Amélioration |
|---|---|---|---|
| single (single-target build) | 5.35k | 5.0k (94%) | 6.2k (115%) |
| pack of 5 (AoE build) | 14.9k | 14.6k (98%) | 13.8k (93%) |
| pack of 12 (AoE build) | 30.5k | 20.8k (68%) | 17.9k (59%) |

Restoration, alone with a Fire Mage and a Fury Warrior under a 15% group pulse (`-pulse 15`, single layout, 60 s):
about 4.6k healing a second at 26% overheal, next to 5.2k for the Holy Priest and 3.7k for the Holy Paladin in the same
setup; Riptide, the waves, Healing Rain, Wellspring, Cloudburst and Ancestral Guidance all heal. Spirit Link, Healing
Tide and Ascension wait for a group in real danger.

Bots: the retail actions sit above the stock Shaman ones and every one checks its spell is known (mod-playerbots
`ShamanRetail.h`); Earth Shock is Elemental's spender only, and Chain Lightning counts as single-target for the threat
strategy. Bench words: `ele`, `enh`, `resto`.
