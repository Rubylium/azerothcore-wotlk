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
- **Essences** come with progress and are not an input of their own. The model's player gathers 8 Growth points of
  each stat and 25 Vitality a key of progress (`ProgressKeys(I)`).

## Essences (diminishing returns, caps)

Saved essence points are never rewritten; only what they give is curved or capped (`mod-stat-growth EssenceTuning.h`,
mirrored in `PowerScaling.h`: change both together).

- **Growth** (a random stat of the class a point) and **Vitality** (40 maximum health a point at 80) have no cap
  but count with diminishing returns: `effective = ceiling * (1 - e^(-points / ceiling))`.
  - The ceiling is 600 per Growth stat and 1,250 Vitality points (+50k health).
  - The first few hundred points count almost in full.
  - Example: 1,175 agility counts as 515, and 3,510 Vitality as 1,175 (+47k health instead of +140k).
  - At the Growth ceiling a damage dealer's essences add about 15% damage (`EssenceDpsAtCeiling`).
  - Bots mirror their players' saved points through the same curve (`BotEssenceSystem.cpp`).
- **Experience** is capped at +200%, essences and the gear affix together (`MaxExperienceBonus`). At the level cap
  experience fills the paragon bar, so an uncapped bonus made paragon come far too easily. **Resource** stays capped
  at 500% and **Fortune** at 100%. An essence of a capped family becomes another family when consumed.
- A character's tooltip shows its essences, saved and effective (`FrameXML/Essences.lua`, the server's
  `HandleEssenceAddonMessage`).

## The power index

`Power::PowerIndex(I, P) = DpsIndex(I) * ParagonDpsIndex(P) * EssenceDpsIndex(ProgressKeys(I))` is a character's
damage, with the essences that item level comes with, over the reference damage dealer's at item level 284 with no
paragon and no essences (the measured bot).

| Item level \ paragon | 0 | 50 | 100 | 200 | 300 | 400 |
|---|---|---|---|---|---|---|
| 223 (Mythique 0 loot) | 0.48 | 0.66 | 0.92 | 1.76 | 3.36 | 6.42 |
| 245 (+10 gear) | 0.67 | 0.93 | 1.29 | 2.47 | 4.71 | 9.01 |
| 264 | 0.85 | 1.17 | 1.62 | 3.09 | 5.91 | 11.29 |
| 284 (best stock items) | 1.04 | 1.44 | 2.00 | 3.81 | 7.29 | 13.94 |
| 300 | 1.10 | 1.53 | 2.11 | 4.03 | 7.71 | 14.74 |
| 330 | 1.22 | 1.68 | 2.33 | 4.45 | 8.50 | 16.24 |
| 350 | 1.29 | 1.78 | 2.46 | 4.70 | 8.98 | 17.16 |
| 370 (Mythic+ loot cap) | 1.35 | 1.87 | 2.59 | 4.95 | 9.46 | 18.08 |
| 460 (raid / L'Infini cap) | 1.67 | 2.32 | 3.20 | 6.12 | 11.70 | 22.36 |

**Reference damage dealer** (bench Fire mage, the tuned class, no group buffs) at 284 with 0 paragon:
`ReferenceSingleTargetDps` 6.4k on one target, `ReferencePackDps` 15.9k on a pack of five.

**Reference player health** (`Power::ExpectedPlayerHealth(I, P)`) is for a damage dealer. A tank has about 1.45x it.
It counts gear stamina, personal-loot bonuses, the essences and Vitality that come with that progress, the board's
health nodes, and buffs:

| Profile | Single DPS | Pack DPS | Health |
|---|---|---|---|
| 245 / 0 | 4.3k | 10.7k | 61k |
| 284 / 0 | 6.7k | 16.6k | 89k |
| 284 / 100 | 12.8k | 31.7k | 93k |
| 330 / 100 | 14.9k | 37.0k | 119k |
| 350 / 200 | 30.1k | 74.7k | 134k |
| 370 / 300 | 60.5k | 150k | 150k |
| 460 / 400 | 143k | 356k | 204k |

## Sizing content

1. **Pick the profile** `(I, P)` the content is for, and the group: damage dealers, plus tanks and healers at about a
   third of a damage dealer each.
2. **Health (the DPS check).** Use `Power::DpsCheckHealth(I, P, dealers, seconds[, pack])`. It returns the health that
   lasts `seconds` against that group playing well.
   - For "hard", give the fight about 85-90% of the time you allow (enrage, music), so a group loses about a tenth to
     mechanics and movement.
   - For "comfortable", about 70%.
   - Example: a 10-player boss (6 DPS, 2 tanks, 2 healers, so 7.3 damage dealers) for 350 / 200 with a 5-minute
     enrage has `DpsCheckHealth(350, 200, 7.3, 300)` = 66M at the limit, about 57M for "hard". Set the creature's
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
| L'Infini, Défi `T` | 300 / 100 at Défi I, then +10 item level and +50 paragon a tier (Défi III 320 / 200, Défi X 390 / 550). Health x the tier profile's `PowerIndex` over Défi I's (x2.04 at III, x23.8 at X), damage x its `ExpectedPlayerHealth` over Défi I's (x1.15 at III, x1.72 at X), avoidable hits +10% a tier on top. Its base health (`HealthModifier` 3412) and damage reference (122k) were set by play at Défi I. Bots: the profile minus 9 item levels and 8 paragon, and at least a typical player's essences for their gear (`SetBotEssenceFloor`). | `ChallengeTiers.h` BossProfiles, `InfiniteGod.cpp` |
| The Hollow Voice | 460 / 650, 10 players, one difficulty (`PowerIndex` 113, x54 L'Infini's Défi I). Sized on the profile's group (six damage dealers, two tanks, two healers: 5.3 million a second): the Archbishop 505 million (`HealthModifier` 36214, about 110 s of the 2:00 his track gives), Vel'thazar 1.19 billion (85336, about 257 s). Hits are shares of `ExpectedPlayerHealth(460, 650)` (about 221k). Bots at the full profile (margins 0): it starts from a game master's `.defi start 930100` for now. | `ChallengeTiers.h` BossProfiles, `HollowVoice.cpp` |
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
- **The reference bot has no essences.** `PowerIndex` adds a typical player's (x1.04 at 284, x1.08 at 370, at most
  x1.15). Real players with far more saved essences than that sit only a little above it: the curve bounds them.
- The power index is a damage dealer's. Healing and tanking scale differently: size tank damage on the tank's 1.45x
  health, not on the index.
