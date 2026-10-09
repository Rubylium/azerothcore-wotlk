# mod-warlock

The Warlock on the retail-style talent trees (localTools/warlock/talentTree.json, mod-custom-classes TalentTree.cpp):
Soul Shards, Malefic Rapture, the summoned demons (Wild Imps, Dreadstalkers, the Demonic Tyrant, the Darkglare, the
Grimoire's Felguard, the infernal), Havoc, Conflagrate's charges, and the effects of its talents and abilities that
spell data alone cannot carry. Spell data: localTools/warlock/Spells.ps1 (spell ids 95600-95899; new family flags in
word 2, 0x10000-0x40000000) and localTools/warlock/StockSpells.ps1 (the stock Warlock spells changed in place). The
summoned demons are creatures 95600-95605 (`data/sql/db-world/base/warlock_spells.sql`).

The Warlock keeps mana and its demons (its pet for the specialization: the Felhunter, the Felguard, the Imp). No spell
asks for a Soul Shard item any more: the summons, Shadowburn and Soul Fire are free of it.

## Soul Shards

Fragments d'âme (95700): an aura of up to 5 stacks, with tenths of a shard kept by this module beside it, the resource
shown the way the other reworks show theirs. Out of combat they come back to 3 (never taken down). Every specialization
fills and spends them:

- Affliction: each Curse of Agony tick has 22% (+5% a rank of Agonie tourmentée) to give one, shared out among the
  curses ticking (/ sqrt of their count); Soul Rot gives one, Haunt one with Âme hantée. Malefic Rapture, Seed of
  Corruption and Vile Taint spend one.
- Demonology: Shadow Bolt gives one, Demonbolt two. Hand of Gul'dan spends up to 3 (one Wild Imp each), Call
  Dreadstalkers 2 (1 with Demonic Calling), Grimoire: Felguard 1, Bilescourge Bombers 2.
- Destruction: Immolate's ticks give two tenths, Incinerate two (three on a critical strike), Conflagrate five (+1 or 2 with
  Braises vives), the infernal's Immolation a tenth. Chaos Bolt spends 2, Rain of Fire 3, Shadowburn 1 (given back if
  the target dies within 5 s).

Conduit d'âme gives a spent shard back now and then.

## The specializations

- **Affliction** (spec passive 95880: the damage over time effects 20% stronger): Curse of Agony, Corruption, Unstable
  Affliction (with the specialization), Haunt, Malefic Rapture (with the specialization: 1.5 s, a shard; it strikes
  every enemy within 40 yd carrying the Warlock's damage over time effects, once for each effect), Seed of Corruption
  (a shard; its blast spreads Corruption, Semer les graines plants a second seed), Soul Rot (1 min: the target and 3
  enemies near it, a shard back), Phantom Singularity (45 s: every 2 s for 16 s the enemies within 8 yd of the target)
  or Vile Taint (25 s, a shard: Curse of Agony and a taint on the enemies within 8 yd of a spot), Summon Darkglare (2
  min: the damage over time effects on the enemies near the Warlock last 8 s longer, then an eye beam every 2 s for 20
  s, stronger for each effect on the target), Nightfall, Shadow Embrace, Siphon Life, Eradication, Pandemic,
  Everlasting Affliction, Récolte sinistre, Contact de l'effroi, Crescendo tourmenté or Floraison funeste.
- **Démonologie** (95881): the Felguard (Summon Felguard with the specialization), Shadow Bolt, Demonbolt (4.5 s,
  instant on a Demonic Core), Hand of Gul'dan (1.5 s: the target and the enemies within 8 yd struck, stronger a shard,
  and a Wild Imp a shard: it follows the Warlock and throws Fel Firebolts at its target every 2 s for 15 s, and may leave
  a Demonic Core), Call Dreadstalkers (20 s: two hounds for 12 s, a bite and their swings, a Demonic Core each),
  Implosion (the Wild Imps explode on the target), Power Siphon (30 s: 2 imps for 2 cores), Bilescourge Bombers (30 s,
  2 shards: 6 s of bombs on a spot) or Demonic Strength (45 s: the Felguard whirls for 5 s), Grimoire: Felguard (2 min:
  a second Felguard for 17 s), Summon Demonic Tyrant (1 min 30 s: the other demons stay 15 s longer and deal 15% more
  while it stands, and it fires Demonfire), Demonic Empowerment, Demonic Calling, Démons intérieurs (a Wild Imp every
  12 s), Âmes sacrifiées, Gul'dan suprême or Trépas (Demonbolt's mark explodes 16 s later).
- **Destruction** (95882: Chaos Bolt always strikes critically): Immolate, Incinerate (2 s), Conflagrate (Conflagration,
  with the specialization: 2 charges a 13 s recharge, any target, Backdraft from it), Chaos Bolt (with the
  specialization: 3 s, 2 shards, no cooldown), Rain of Fire (with the specialization: 3 shards, 8 s on a spot),
  Shadowburn, Havoc (30 s: a second enemy takes the single-target spells cast at another target for 12 s), Channel
  Demonfire (25 s: 3 s of bolts at random enemies burning with Immolate), Cataclysm (30 s: a spot struck and burnt with
  Immolate), Summon Infernal (3 min: an infernal lands on a spot, stunning, and fights for 30 s), Backdraft, Fire and
  Brimstone, Éradication, Brasier rugissant, Inferno or Combustion interne, Avatar de destruction or Pluie de chaos.
- **Class tree** (Démoniste): Unending Resolve (40% less damage for 8 s, 3 min), Dark Pact (20% of the health for a
  shield of twice as much, 1 min), Shadowfury, Fel Domination, Burning Rush (50% speed for 4% of the health a second,
  cast again to stop), Soul Leech (3% of the damage the Warlock and its demons deal as a shield, up to 10% of its
  health), Grimoire of Sacrifice, Soul Link or Abyss Walker, and the WotLK utility talents (Demonic Embrace, Fel
  Concentration, Demonic Aegis, Improved Fear, Demonic Resilience, Grim Reach, Amplify Curse, Master Summoner, Fel
  Synergy, Improved Life Tap...).

## The summoned demons

The Wild Imps, the Dreadstalkers, the Demonic Tyrant, the Darkglare, the Grimoire's Felguard and the infernal are
allied guardians of the Warlock's (summon properties 61, as mod-hunter's wild beasts: the pet category would take the
pet's slot). This module holds them: the casters (imps, tyrant, eye) follow the Warlock and cast at its target, the
others fight the target it sends them at, their swings and hits worked out from the Warlock's spell power, its damage
done and the demons' bonuses (Serviteur courroucé, the Tyrant, Gangregarde enragé); their health is a share of the
Warlock's. They go when their time is up, or with the Warlock.

## Tuning

The WotLK kit keeps its numbers; the new spells carry spell power coefficients in
`data/sql/db-world/base/warlock_spells.sql`, the shares this module works out sit at the top of
`src/WarlockTalents.cpp`, and the specializations' passives scale what is left. Past five enemies each area hit takes
sqrt(5 / enemies), the casters' falloff (the Priest's, the Hunter's Multi-Shot; it was eight until the 2026-10-09
sweep). Rain of Fire's hits are scaled by `warlock.rain_of_fire_factor`: its shards come from every Immolate ticking,
so it rains more often the bigger the pack.

Measured on the combat bench (`localTools/combatBench/runBench.ps1`, 2026-10-01, key +10, item level ~244, 60 s, three
rounds of the final numbers, the Fire Mage in the same runs, no tank bot; its own single target swung 4.2k-6.1k a run,
so the shares are of its average):

| | Fire Mage | Affliction | Démonologie | Destruction |
|---|---|---|---|---|
| single (single-target build) | 5.1k | 5.8k (~115%) | 5.8k (~114%) | 5.3k (~104%) |
| pack of 5 (AoE build) | 13.0k | 14.3k (~110%) | 15.2k (~117%) | 15.8k (~121%) |
| pack of 12 (AoE build) | 31.9k | 31.7k (~99%) | 25.3k (~79%) | 37.8k (~118%) |

Every new active spell is cast and hits in those runs (Darkglare's beam, the Tyrant's Demonfire, the imps' bolts, the
Dreadstalkers' bites, the Grimoire's strikes, Havoc's copies, Channel Demonfire, the infernal's Immolation...).

Bots: the retail actions sit above the stock Warlock ones and every one checks its spell is known (mod-playerbots
`WarlockRetail.h`); the stock Rain of Fire and Hellfire channels are left out, Seed of Corruption is Affliction's (on a
shard), and the Demonology fills with Shadow Bolt. Bench words: `affli`, `demo`, `destro`.
