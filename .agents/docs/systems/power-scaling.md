# Power scaling (item level, paragon, and how to size content)

Every piece of content is tuned for a **player profile**: an item level `I` and a number of active paragon points `P`.
"Content X is clearable but hard for item level 350 with 200 paragon" is a complete spec. The model below turns it
into numbers: health for a DPS check, hit sizes. Code: `src/server/game/Maps/PowerScaling.h` (namespace `Power`).
The Mythic+ keys (`MythicDungeon.h`) and the Défi tiers (`mod-playerbots ChallengeTiers.h`) already use it.

## The two inputs

- **Item level.** Up to 284 it is the game's own items. Above, it is generated gear (Mythic+ variants
  `MythicItemGeneration.cpp`, forged ranks `mod-forge Forge.cpp`). Generated gear grows over its base item by the
  item-level ratio `r = I / base`:
  - primary stats, stamina, attack and spell power, weapon damage, armour and block grow by `r`
    (`Power::StatExponent = 1`);
  - secondary ratings (hit, crit, haste, expertise, armour penetration, defence, dodge, parry, block, resilience) grow
    by `r^0.5` (`RatingExponent`). They become percentages that cap. With the old `r^2` on everything, crit reached
    95-100% at item level 390 and damage roughly quadrupled per 1.5x item level.
- **Paragon.** Points active on the board, spent as a bench bot spends them. Measured at about +0.65% damage per
  point, compounded (`ParagonDpsPerPoint = 1.0065`): 100 points is x1.9 and 200 points is x3.7. **Paragon moves
  power far more than gear does past 284.**

## The power index

`Power::PowerIndex(I, P) = DpsIndex(I) * ParagonDpsIndex(P)` is a character's damage over the reference damage
dealer's at item level 284 with no paragon.

| Item level \ paragon | 0 | 50 | 100 | 200 | 300 | 400 |
|---|---|---|---|---|---|---|
| 223 (Mythique 0 loot) | 0.48 | 0.66 | 0.92 | 1.75 | 3.35 | 6.41 |
| 245 (+10 gear) | 0.66 | 0.92 | 1.27 | 2.42 | 4.63 | 8.84 |
| 264 | 0.82 | 1.13 | 1.57 | 3.00 | 5.73 | 10.95 |
| 284 (best stock items) | 1.00 | 1.38 | 1.91 | 3.65 | 6.98 | 13.35 |
| 300 | 1.05 | 1.45 | 2.00 | 3.83 | 7.32 | 14.00 |
| 330 | 1.14 | 1.58 | 2.18 | 4.17 | 7.96 | 15.22 |
| 350 | 1.19 | 1.65 | 2.28 | 4.37 | 8.35 | 15.95 |
| 370 (Mythic+ loot cap) | 1.25 | 1.73 | 2.39 | 4.57 | 8.73 | 16.69 |
| 460 (raid / L'Infini cap) | 1.51 | 2.09 | 2.89 | 5.52 | 10.55 | 20.16 |

**Reference damage dealer** (bench Fire mage, the tuned class, no group buffs) at 284 with 0 paragon:
`ReferenceSingleTargetDps` 6.4k on one target, `ReferencePackDps` 15.9k on a pack of five.

**Reference player health** (`Power::ExpectedPlayerHealth(I, P)`) is for a damage dealer. A tank has about 1.45x it.
It counts gear stamina, personal-loot bonuses, the essences and Vitality that come with that progress, the board's
health nodes, and buffs:

| Profile | Single DPS | Pack DPS | Health |
|---|---|---|---|
| 245 / 0 | 4.2k | 10.5k | 62k |
| 284 / 0 | 6.4k | 15.9k | 96k |
| 284 / 100 | 12.2k | 30.4k | 100k |
| 330 / 100 | 13.9k | 34.6k | 138k |
| 350 / 200 | 27.9k | 69.4k | 161k |
| 370 / 300 | 55.9k | 139k | 186k |
| 460 / 400 | 129k | 321k | 284k |

## Sizing content

1. **Pick the profile** `(I, P)` the content is for, and the group: damage dealers, plus tanks and healers at about a
   third of a damage dealer each.
2. **Health (the DPS check).** Use `Power::DpsCheckHealth(I, P, dealers, seconds[, pack])`. It returns the health that
   lasts `seconds` against that group playing well.
   - For "hard", give the fight about 85-90% of the time you allow (enrage, music), so a group loses about a tenth to
     mechanics and movement.
   - For "comfortable", about 70%.
   - Example: a 10-player boss (6 DPS, 2 tanks, 2 healers, so 7.3 damage dealers) for 350 / 200 with a 5-minute
     enrage has `DpsCheckHealth(350, 200, 7.3, 300)` = 61.5M at the limit, about 53M for "hard". Set the creature's
     health modifier so its health lands there.
3. **Damage.** Hits are a share of `ExpectedPlayerHealth(I, P)`, as the Mythic+ budgets are (`MythicTuning.h`):
   - telegraphed hit 45-70%, small or frequent hit 20-35%, group pulse 8-15%;
   - tank buster 35-50% of a tank (1.45x);
   - boss melee swing 6-10% of a tank, trash swing 4-6%.

   Use `MythicTuning::DealReferenceDamage` / `ReferenceHealth` for keys, or `Power::ExpectedPlayerHealth` directly.
4. **Declare it.** Put the profile in the content's header comment and its README, e.g.
   "tuned for 350 / 200, 10 players, 5 min". Bots of the content get that profile minus a margin (see the Défi's
   `BotItemLevelMargin` / `BotParagonMargin`).
5. **Harder versions** step up the profile, not an arbitrary multiplier. The Défi tiers each add paragon; a key adds
   item level and paragon. The health multiplier is the power index ratio of the two profiles.

## What each piece of content asks today

| Content | Profile | Where |
|---|---|---|
| Mythic+ key `k` | `GetExpectedItemLevel(k)` (223 + 2.45 ilvl a key, one key behind, capped 370) with `GetRecommendedParagon(k)` = round(8.5 x (k - 10)) past +10. For example +20 = 270 / 85, +30 = 294 / 170, +52 = 348 / 357, +60 = 368 / 425. Creature health grows 1.08 a key to +10, then by the power index ratio over +10's. Damage follows the player health of the profile, plus a pressure of 2.5% a key past +10 (capped at 1.6). | `MythicDungeon.h` |
| Défi tier `T`, normal missions | The raid as it is at Défi I, plus 31 paragon a tier (Défi X: 279). Health `x ParagonDpsIndex(31 (T-1))` (x1.22 a tier, x6.1 at X). Damage x1.115 a tier (x2.7 at X). | `ChallengeTiers.h` |
| L'Infini, Défi `T` | 300 / 100 at Défi I, +50 paragon a tier (x1.38 health a tier). Its base health (`HealthModifier` 3412) and damage reference (122k) were set by play at Défi I. | `ChallengeTiers.h` BossProfiles, `InfiniteGod.cpp` |
| Infinite Dungeon | **Not on the model yet.** It has its own ladder (`gearRatio^2` from item level 200 to 310, 12k-28.8k reference). | `infinite/InfiniteDungeonScaling.h` |

## Measures, and re-measuring

Measured on 2026-09-30 on the combat bench (`localTools/combatBench/runBench.ps1`): four Fire mages per run, single
target and pack of five, 60 s.
- **Gear:** item levels 223, 264, 284, 330, 370 and 460 at key +10 (no paragon).
- **Paragon:** item levels 284 and 370 at keys +30 (100 paragon) and +50 (200 paragon). The bench gives bots the
  key's recommended paragon.
- **Noise:** runs vary by up to +-20% per bot, so the curve past 284 is smoothed to `(I / 284)^0.85`.

To re-measure after a class or board change, run the same benches, then update together:
- `DpsCurve`, `ParagonDpsPerPoint` and `Reference*Dps` in `PowerScaling.h`;
- the client copies: `ChallengeBoard.lua` `PARAGON_DPS_PER_POINT` and the tier formulas, and `MythicPlus.lua` /
  `ChallengeBoard.lua` `KeyParagon` (8.5 a key);
- this doc's tables (`python` over the same formulas);
- the unit tests (`src/test/server/game/Maps/PowerScalingTest.cpp`).

## Pitfalls

- **Changing generated-gear growth changes every item already owned.** Templates are rebuilt at startup. The client
  caches item records, so `SendGeneratedItemRecords` (`MythicDungeonSystem.cpp`) re-sends every generated item a
  player carries at login; without that, tooltips keep the old stats.
- **A bench row without its spec label** (`Mage · ilvl 281 · bot`, no `fire pve / aoe`) is a bot whose preset was not
  applied. Do not use it for the curve.
- The power index is a damage dealer's. Healing and tanking scale differently: size tank damage on the tank's 1.45x
  health, not on the index.
