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
- **Paragon.** Points active on the board, spent as a bench bot spends them. The measured curve
  (`Power::ParagonCurve`): x2.0 at 85 points, x3.7 at 212, x5.4 at 255, x7.8 at 340, x9.6 at 425 and **flat past
  425** - what is left of the board is defence and other roles' nodes. Up to ~250 points it grows about as +0.65% a
  point compounded; the compounded rule used before went on growing (x67 at 650) and oversized everything past ~300
  points by up to seven times. **Paragon moves power far more than gear does past 284, up to 425 points.**
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

1. **Pick the profile** `(I, P)` the content is for, and the group, counted with `Power::GroupDamageDealers(damage,
   tanks, healers)`: a tank deals a third of a damage dealer, a healer a tenth (measured in L'Infini: healers dealt
   0-3k next to 12-13k). Use the raid finder's composition (`GetComposition`): 10 players are 5 damage dealers, 2 tanks
   and 3 healers, 5.97 damage dealers. Bots come at the profile itself (no margin): one player with bots and a group
   of players meet the same fight.
2. **Health (the DPS check).** Use `Power::DpsCheckHealth(I, P, dealers, seconds[, pack])`. It returns the health that
   lasts `seconds` against that group playing well.
   - For "hard", give the fight about 85-90% of the time you allow (enrage, music), so a group loses about a tenth to
     mechanics and movement.
   - For "comfortable", about 70%.
   - Example: a 10-player boss (5.97 damage dealers) for 350 / 200 with a 5-minute enrage has
     `DpsCheckHealth(350, 200, 5.97, 300)` = 54M at the limit, about 46M for "hard". Set the creature's
     health modifier so its health lands there.
3. **Damage.** Hits are a share of `ExpectedPlayerHealth(I, P)`, as the Mythic+ budgets are (`MythicTuning.h`):
   - telegraphed hit 45-70%, small or frequent hit 20-35%, group pulse 8-15%;
   - tank buster 35-50% of a tank (1.45x);
   - boss melee swing 6-10% of a tank, trash swing 4-6%.

   Use `MythicTuning::DealReferenceDamage` / `ReferenceHealth` for keys, or `Power::ExpectedPlayerHealth` directly.
4. **Declare it.** Put the profile in the content's header comment and its README, e.g.
   "tuned for 350 / 200, 10 players, 5 min". Bots of the content get that profile (the Défi's
   `BotItemLevelMargin` / `BotParagonMargin` are 0).
5. **Harder versions** step up the profile, not an arbitrary multiplier. The Défi tiers each add paragon; a key adds
   item level and paragon. The health multiplier is the power index ratio of the two profiles.

## What each piece of content asks today

| Content | Profile | Where |
|---|---|---|
| Mythic+ key `k` | `GetExpectedItemLevel(k)` (223 + 2.45 ilvl a key, one key behind, capped 370) with `GetRecommendedParagon(k)` = round(8.5 x (k - 10)) past +10. For example +20 = 270 / 85, +30 = 294 / 170, +52 = 348 / 357, +60 = 368 / 425. Creature health grows 1.08 a key to +10, then by the power index ratio over +10's. Damage follows the player health of the profile, plus a pressure of 2.5% a key past +10 (capped at 1.6). | `MythicDungeon.h` |
| Défi tier `T`, normal missions | The raid as it is at Défi I, plus 31 paragon a tier (Défi X: 279). Health `x ParagonDpsIndex(31 (T-1))` (x1.22 a tier, x6.1 at X). Damage x1.115 a tier (x2.7 at X). | `ChallengeTiers.h` |
| L'Infini, Défi `T` | 300 / 100 at Défi I, then +10 item level and +50 paragon a tier (Défi III 320 / 200, Défi X 390 / 550). Health x the tier profile's `PowerIndex` over Défi I's (x1.72 at III, x5.63 at X), damage x its `ExpectedPlayerHealth` over Défi I's (x1.15 at III, x1.72 at X), avoidable hits +10% a tier on top. Its base health is the model's hard DPS check at the Défi I profile (`HealthModifier` 1394: 19.4M, `DpsCheckHealth(300, 100, 5.97, 290 * 0.85 * 0.85)`) and its damage reference `ExpectedPlayerHealth(300, 100)` (102k); both were set by play (47.6M, 122k) while its bots were still scaled on the player. Bots: the profile, and at least a typical player's essences for their gear (`SetBotEssenceFloor`). | `ChallengeTiers.h` BossProfiles, `InfiniteGod.cpp` |
| The Hollow Voice | 460 / 650, 10 players, one difficulty (`PowerIndex` 16.1: 103k a second a damage dealer, what the bench measures). Sized on the profile's group (5.97 damage dealers: 615k a second): the Archbishop 65.7 million (`HealthModifier` 4713, the model's 58.6 + 12%, about 110 s of the 2:00 his track gives), Vel'thazar 171.0 million (12262: as hard as the Archbishop and 7.5% more, measured below), Dread Infernals 4 million. Hits are shares of `ExpectedPlayerHealth(460, 650)` (about 221k). Bots at the full profile (margins 0), their health sized to it. On its own page of the board, signing up from an equipped item level of 400, to let players see it (`signUpItemLevel`, the Dungeon Finder's average); a game master's `.defi start 930100` skips that. Pays item level 477 (`Mythic::MaxPinnacleItemLevel`), each class in its own set's look. Measured headless on 2026-10-03 (below). | `ChallengeTiers.h` BossProfiles, `HollowVoice.cpp` |
| Gardien-chef Vorhan | 450 / 600, 8 players (2 tanks, 2 healers, 4 damage dealers: `GroupDamageDealers(4, 2, 2)` 4.87), one difficulty, the tier's 3 attempts (a learnt fight). Health from the model at run time, not the template: `DpsCheckHealth(450, 600, 4.87, 205 * 0.87)` (the 205 s he can be hit, the riot aside, the "hard" share), live knob `vorhan.health_scale`. The riot's waves each about 8 s of the group's pack damage. Hits are shares of `ExpectedPlayerHealth(450, 600)`. Signing up from 420; pays his own sets at 485 (mod-legendary rolled copies, with an epic's gear bonuses). Measured 2026-10-08 (e2e/local/vorhan, bots alone, four in parallel): a group of 4 bot damage dealers deals about 59 million by the enrage - the model's 87 million leaves them the enrage; a player's group dealt far more (the first kill: 62 million in 2:54). The summary logs each player's damage (module.vorhan). | `ChallengeTiers.h` BossProfiles, `WardenVorhan.cpp` |
| Le Front du Nord, tier `T` | Open world at 80, sized for one damage dealer of the tier's profile (tanks a third): 187 / 200 / 213 / 223 at 0 paragon, loot 200 / 213 / 219 / 226. Below 223 (the measured curve's start) the damage is 223's x `(I / 223)^1.8`, measured on the bench (0.72 at 186, 0.84 at 200, 0.96 at 212). Fight lengths against that: a roaming elite 25 s, a rift wave 15 s (pack damage), its guardian 35 s, a Colosse 60 s; all grow with the participants. Hits are shares of `ExpectedPlayerHealth` of the profile (melee 2.5-4%, telegraphed 35-60%). | `frontier/Frontier.cpp` `TierDps` |
| Infinite Dungeon | **Not on the model yet.** It has its own ladder (`gearRatio^2` from item level 200 to 310, 12k-28.8k reference). | `infinite/InfiniteDungeonScaling.h` |

## The Hollow Voice, measured (2026-10-03)

Nine bots alone (`e2e/local/hollowvoice/run.ps1`, the game master watching takes no damage slot: 2 tanks, 3 healers,
4 damage dealers; `-itemMargin` / `-paragonMargin` lower the bots' profile through the live knobs
`challenge.sim_bot_item_level_margin` / `challenge.sim_bot_paragon_margin`, no build). Damage a second on the boss:

| Bots | The Archbishop (needs 554k for his 117.5 s check) | Vel'thazar (needs about 585k over his window) |
|---|---|---|
| 460 / 650 | 327k, 439k, 361k: down to 29-37% at Last Rites | 169k: 69% left at the enrage |
| 440 / 650 | 318k, 342k | |
| 460 / 425 | 162k, 102k | |
| 440 / 425 | 153k, 97k | 110k, 121k |
| 420 / 425 | 130k, 100k | |

- Twenty item levels move it about 5%, as the model says. **Paragon from 425 to 650 multiplies it by about 2.5**,
  where `ParagonCurve` is flat past 425: the curve was measured on a lone Fire mage on the bench; a fight's group of
  bots is not that.
- At the full profile the bots reach about 70% of the model's group damage on the Archbishop and under 30% on
  Vel'thazar: he was tanked against the walls, his mechanics running off the floor.
- The equipped item level asked to sign up hardly changes the outcome; the paragon does.

Then the demon held in the middle of the room (his tank's spot, `GroundIndicators::SetTankSpot`), Fervour of the
Faithful (+40% damage for 10 s to whoever holds a mechanic away from the boss) and the tanks' Exposure: on
Vel'thazar the bots dealt 310k, 299k, 465k, 325k (350k, twice what they did), the Archbishop unchanged (323k, 350k,
409k). Sized on that, as hard as the Archbishop and 7.5% more (the user's ask): the Archbishop asks 554k of a group
dealing 368k (1.50 times), the demon at 171.0 million over his 317 s asks 1.62 times the 350k.

## Measures, and re-measuring

Measured on 2026-09-30 on the combat bench (`localTools/combatBench/runBench.ps1`): four Fire mages per run, single
target and pack of five, 60 s.
- **Gear:** item levels 223, 264, 284, 330, 370 and 460 at key +10 (no paragon).
- **Paragon:** the bench gives bots the key's recommended paragon. A Fire mage at item level ~280, 60 s single
  target: 7.4k with none, 15.6k at 85 (+20), 26.7k at 212 (+35), 47.5k at 255 (+40), 82-86k at 425 (+60); at item
  level 460: 13.5k with none, 101.6k at 340 (+50), 112.6k at 425 (+60), 110.3k at 510 (+70), 93-96k at 654 (+87).
  `ParagonCurve` sits between the two gear levels. (A bench bot re-asked for another item level kept its gear once:
  read the worn item level in the row.)
- **Noise:** runs vary by up to +-20% per bot, so the curve past 284 is smoothed to `(I / 284)^0.85`.

To re-measure after a class or board change, run the same benches, then update together:
- `DpsCurve`, `ParagonCurve` and `Reference*Dps` in `PowerScaling.h`;
- the client copies: `ChallengeBoard.lua` `PARAGON_CURVE`, `GOD_TIER_HEALTH` and the tier formulas, and `MythicPlus.lua` /
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
- **Bots are not players in health.** A bot's board and gear give it a fraction of a player's health (52k at
  460 / 650 against about 221k). A challenge's bots are brought up to `ExpectedPlayerHealth` of their profile (x1.45
  for a tank) and never scaled to the player they came with (mod-playerbots `ApplyChallengeBotScaling`).
- The power index is a damage dealer's. Healing and tanking scale differently: size tank damage on the tank's 1.45x
  health, not on the index.
