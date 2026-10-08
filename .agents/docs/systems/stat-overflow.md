# Stat overflow (stats past their caps)

Paragon's flat primary stats put characters far past the stock caps: an agility role at 600 points holds ~16,600
agility (~280% melee crit), a healer ~165% spell crit, hit / expertise / defence / armour penetration ratings sit past
their caps, a tank's armour about three times the 75% cap. **The caps stay; every point past one converts into a
second bonus**, worth a share of a point below it, and the character sheet shows it.

Code: `src/server/game/Combat/StatOverflow.{h,cpp}` (the maths, the per-hit factors, the sheet's summary), hooked in
`Unit.cpp` / `Spell.cpp` / `SpellAuraEffects.cpp`; `mod-stat-growth StatOverflowSystem.cpp` (Robustesse on hits taken,
the summary sent to the client); client `clientPatcher/addons/DragonUI/modules/characterpanel/sidebar.lua`. Unit
tests: `src/test/server/game/Combat/StatOverflowTest.cpp`.

## The three bonuses

| Bonus | Points (per 1% past the cap) | Gives | Rate (knob) |
|---|---|---|---|
| Critique | crit chance past 100%: the chance the roll used, against that target | `R_crit`% more critical **bonus** (the part a crit adds over a normal hit) | `overflow.crit_rate` 0.4 |
| Précision | hit past the cap against the target (melee or spell, as the attack rolls); expertise past the dodge cap (the parry cap where the target can parry the attack); armour penetration past 100% x 0.5 | `R_prec`% more damage | `overflow.precision_rate` 0.4 |
| Robustesse | against a level-83 boss: avoidance (miss + dodge + parry + block) past 100%; defence past crit immunity (the boss's crit chance below 0, in %); armour past 75% (the share of what the cap lets through that the excess would still stop) | `R_rob`% less damage taken, at most `overflow.robustness_cap` | `overflow.robustness_rate` 0.3, cap 20 |

- **Critique.** `bonus' = bonus x (1 + R_crit x (chance - 100) / 100)`. Melee and ranged x2 at 200% crit become
  x2.4; a spell's x1.5 at 165% becomes x1.63; talents' bigger crits grow the same way. White swings use the
  attacker's crit chance against the target (`GetUnitCriticalChance`), not what the combat table left of it. Direct
  hits, damage and healing over time (the chance the aura rolls with), heals. Players' hits and their pets', totems'
  and guardians' (whatever crit chance their hit rolled).
- **Précision.** `damage x (1 + R_prec x points / 100)`, before armour, on white swings, spells and damage over time
  of players (not pets). Hit and expertise are computed as the roll computes them (`MeleeSpellRawMissChance`,
  `MagicSpellRawHitChance`, the target's dodge and parry less the skill difference): a dual-wield white swing has 19%
  more to cover than an ability. Spells without a hit roll (damage class none) get nothing. Armour penetration counts
  on hits armour reduces.
- **Robustesse.** On every hit a player takes (`OnDamage`, after absorbs, before the paragon board's reduction and
  procs). Computed against the reference boss whatever hits the player, so the sheet and the fight agree. It
  multiplies with the board's `ReductionPct` (capped 25%): at most 1 - 0.75 x 0.8 = **40%** less together.
- **Dodge and parry on the sheet** are now the chances combat rolls (`m_realDodge` / `m_realParry`, after diminishing
  returns, `StatSystem.cpp`); they used to be the undiminished sums (300% dodge on a character that dodges 60%).

## The rates

Aim: a point past a cap is worth about a third to a half of a point below it.

- Crit: below the cap, +1% crit adds 1% of the critical bonus to the average hit (`(M - 1) / 100`); past it, a point
  adds `R_crit`% of the bonus. The ratio is `R_crit` itself, the same for x2 and x1.5 crits: **0.4**.
- Hit and expertise: below the cap a point lands 1% more of the swings (1% to 1.09% damage); past it `R_prec`%:
  **0.4** (ratio 0.37-0.4).
- Armour penetration: near its cap, against a stock level-83 boss's 10,643 armour, 1% is ~0.54% physical damage
  (armour 1,550 -> 1,459 of `A / (A + 15,232)`), so it counts half a point: 0.5 x 0.4 = 0.2%, a ratio of 0.37.
- Avoidance: below the cap a point stops 1% of the boss's swings, ~0.65% of a tank's intake (melee ~65% of it); past
  it `R_rob` = 0.3% of **all** damage taken: ratio ~0.46. Defence: 0.04% crit a skill point both sides, same ratio.
- Armour: near the cap 1,000 armour lets 1.5% less physical damage through; past it the same 1,000 armour is 1.5
  points, 0.45% less of all damage: ratio ~0.45 (~0.3 of a point on all damage).
- Examples: 280% melee crit: bonus +72%, a crit x2.72, about +36% damage. 165% spell crit: +26%, x1.63, +8.7%
  healing. A tank at three times the armour cap: 60 points, 18% less damage taken (with avoidance or defence past
  theirs, the 20% cap).

## The character sheet

The server sends each real player (not bots) two addon messages, prefix `Overflow`, when the summary changes (checked
every second) and when the sheet asks (`Q`, on entering the world): `C` (crit chances, bonuses, hit caps and excess,
expertise caps, armour penetration, precision by attack kind; the rates) and `R` (avoidance, defence, armour,
points, reduction, rate and cap), every figure against a level-83 world boss (`BuildSummary`, fields in
`FormatCritPrecision` / `FormatRobustness` order, read in the same order by `sidebar.lua`'s `CRIT_FIELDS` /
`ROBUSTNESS_FIELDS`: change both together).

The DragonUI sidebar override (shipped over the stock file by `Build-FriendPatch.ps1`):
- crit rows past 100% show **100.00%** in gold; their tooltip adds the excess and the critical multiplier it gives;
- the hit, expertise, armour, defence, dodge, parry and block rows' tooltips add the cap against a level-83 boss and
  what lies past it;
- a **Surplus** section (shown once a summary came): Critique `+X %` and Précision `+X %` of the character's leading
  combat kind (melee from behind, ranged or spells; the tooltip lists all), Robustesse `-X %` damage taken, with the
  breakdown on each tooltip. Palette: gold figures, parchment text, muted brown for zero.

## Tuning (live)

`.tune set overflow.crit_rate <x>`, `overflow.precision_rate`, `overflow.robustness_rate`, `overflow.robustness_cap`
(no build; the sheet follows within a second, as the summary carries the rates). Bake with
`localTools/tuning/bakeTuning.py`. The other constants (`ArmorPenetrationWeight`, the reference boss) are in
`StatOverflow.h`, mirrored in the sheet's texts only.

## Pitfalls

- **The power model predates the overflow.** `DpsCurve` / `ParagonCurve` and the spec balance were measured without
  it; high-paragon melee and crit-heavy specs gain the most. Re-measure on the bench (combat-bench.md) before trusting
  `PowerIndex` past ~300 points, then re-bake `balance.*`.
- Bots get the bonuses too (they are players), but their gear choice still treats past-cap hit, expertise and defence
  as worthless (`mod-playerbots StatsWeightCalculator::ApplyOverflowPenalty`).
- A new crit path must pass the chance it rolled (`SpellCriticalDamageBonus` / `SpellCriticalHealingBonus` /
  `CalculateSpellDamageTaken` take it, 0 = no overflow), or its crits will not grow.
