# mod-priest

The Priest on the retail-style talent trees (localTools/priest/talentTree.json, mod-custom-classes TalentTree.cpp):
Atonement, the Holy Words, Insanity, and the effects of its talents and abilities that spell data alone cannot carry.
Spell data: localTools/priest/Spells.ps1 (spell ids 93400-93699; new family flags in word 2, 0x10000-0x40000000) and
localTools/priest/StockSpells.ps1 (the stock Priest spells changed in place).

## The specializations

- **Discipline** heals through Atonement (Expiation, 93501, 15 s): Power Word: Shield, Power Word: Radiance, Flash
  Heal and Renew put it on an ally, and every damage the Priest (or its Mindbender / Shadowfiend) deals heals each
  ally carrying it for 35% of the damage (+10% / +20% with Expiation renforcée). The damage of a quarter second is
  healed at once (spell 93502), so the combat log stays readable. Penance (9 s, damage or heal), Power Word: Radiance
  (2 s cast, 2 charges on 18 s: the target and the 4 most injured allies near it, Atonement for 60% of its duration),
  Schism (+15% damage from the Priest for 9 s), Purge the Wicked (or Ombres pénitentes on Shadow Word: Pain) spread
  by Penance, Mindbender and Shadow Covenant as the ramp, Rapture, Evangelism, Pain Suppression and Power Word:
  Barrier (25% less damage to allies within 10 yd for 10 s).
- **Holy** (Sacré): Holy Word: Serenity (1 min; Flash Heal, Heal and Greater Heal bring it 6 s closer) and Holy Word:
  Sanctify (1 min; Prayer of Healing 6 s, Renew 2 s), Holy Word: Chastise (Smite 4 s), Apotheosis (Holy Words four
  times as fast and free for 20 s). Echo of Light, the mastery: 20% of every heal (+5% a rank of Écho renforcé) rolls
  into a heal over 6 s on the target. Prayer of Mending, Circle of Healing, Guardian Spirit, Divine Hymn (3 min) and
  Lightwell stay the WotLK spells.
- **Shadow** (Ombre) fights with Insanity (Démence, 93500): an aura of up to 100 stacks, the resource shown the way the
  other reworks show theirs (no class HUD). Mind Blast 8, Vampiric Touch 5, Shadow Word: Pain 4, Shadow Word: Death 5,
  each Mind Flay tick 3, each Mind Sear tick 1 (3 a tick at most), Void Bolt 12, each Void Torrent tick 8, Shadow Crash
  6, Dark Ascension 30, each fiend hit 2, each apparition 1 (2 with Esprits propices). Devouring Plague (every stock
  rank) spends 50 (35 during Voidform or Dark Ascension with Maître des ombres; free with Dévoreur d'esprit): no mana,
  an instant hit (93560) and 4 ticks in 6 s. Void Eruption (Voidform: +20% damage, +10% spell haste, Void Bolt usable,
  Mind Blast reset) or Dark Ascension, Shadowy Apparitions, Psychic Link, Shadow Word: Death as an execute (x2.5 below
  20%), Void Torrent, Mindgames. Packs: Shadow Crash puts Vampiric Touch on 5 enemies (9 with Ombres murmurantes),
  Détresse adds Shadow Word: Pain, then Psychic Link, the apparitions and Mind Sear. Insanity fades 10 s after combat.
- **Class tree**: Desperate Prayer, Power Word: Life (below 35%), Leap of Faith, Power Infusion, Vampiric Embrace,
  Inner Focus, Divine Star or Halo, Mindgames or Image translucide, and the WotLK utility talents (Unbreakable Will,
  Body and Soul, Improved Psychic Scream, Twin Disciplines...).

Charges (shown as the stacks of an aura, like the Hunter's): Power Word: Radiance (2, Discipline), Mind Blast (2 with
Pensées du Vide), Holy Word: Serenity (2 with Faiseur de miracles).

## Snappy casts

StockSpells.ps1: Smite 1.5 s (and cheaper, Discipline's filler), Holy Fire 1.5 s, Lesser Heal 1.5 s, Heal, Greater Heal
and Prayer of Healing 2 s; the new spells are instant or 1.5 s (Power Word: Radiance 2 s, Void Torrent a 3 s channel).
Penance 9 s, Divine Hymn 3 min, Shadowfiend 3 min.

## Tuning assumptions

The WotLK kit keeps its numbers; the new spells are sized against it at ~3000 spell power (raid gear before this
server's paragon, which scales everything alike), with their coefficients in
`data/sql/db-world/base/priest_spells.sql` and the shares of the amounts this module works out at the top of
`src/PriestTalents.cpp`:

- Healing is aimed at the reworked Holy Paladin (Word of Glory ~7k, Light of Dawn ~3.5k on 5-7 allies). Holy Word:
  Serenity ~12k (x1.5 below 35% with Guide de la lumière) every ~15-20 s with the reductions, Sanctify ~4.5k on up to
  6 allies, Radiance ~3.3k on 5, Power Word: Life ~11.5k. Discipline's Atonement at 35% on 5 atoned allies turns
  4k damage a second into ~7k healing a second over the group.
- Shadow aims at the Fire Mage (the combat bench reference) on packs of 5-12 and at the Mage / Rogue on a single
  target. Single target: Mind Blast, Void Bolt (1000 + 1.3 SP every 6 s in Voidform), Devouring Plague (0.9 SP hit
  + the stock ticks x1.5), the damage over time, Mind Flay filling. Packs: every Mind Blast, Void Bolt and Devouring
  Plague sends 0.3 SP at each enemy with Vampiric Touch (12 at most), Psychic Link copies 20-35% of the Mind Blast,
  Mind Flay, Void Torrent and Shadow Word: Death hits to them, Mind Sear reaches all of them; past five enemies each
  of these hits takes sqrt(5 / enemies) (the Hunter's Multi-Shot falloff), like Void Eruption's splash and Divine
  Star / Halo's damage.
- Cooldowns stay short: the key tools 15-45 s, the big ones 1-2 min.

Nothing here was measured yet: run the combat bench (`localTools/combatBench/runBench.ps1`, the priest words are
`disc`, `holy` and `shadow`) and read `localTools/combatTelemetry` before retuning.
