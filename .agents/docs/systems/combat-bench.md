# Combat bench (class tuning, rotations, AoE / single-target balance)

**Surveys, sweeps and the auto-tune run on the simulation bench** (`sim-bench.md`: the same bench on world servers of
its own, the real code many times faster than real time, several at once, the live realm untouched). The live bench
below stays for watching one spec in game and for what needs a client.

The in-game test ground (`.bench`, GM Island) measures damage, healing and damage taken on dummies scaled exactly
like Mythic+ creatures. **Use it for any class tuning, new class or spec, talent / rotation change, or bot AI combat
change** instead of dungeon runs. **Every damage spec at once: the sweep** (each bot alone on its own dummies, 30 at a
time, one command for every profile), and the **auto-tune** that moves the spec balance toward the Fire mage live;
one spec's own spells: the session and live tuning below. In-game usage and every command:
`modules/mod-playerbots/COMBAT_BENCH.md`. Code: `modules/mod-playerbots/src/Script/CombatBench.cpp`, measuring in
`src/server/game/Telemetry/CombatTelemetry.cpp` (`Bench*`), the sweep's driving in
`localTools/combatBench/BenchSweep.ps1`.

## Fast survey: the sweep and the auto-tune (start here)

**Use this for any survey of the classes and for the spec balance.** One command measures every damage spec at every
profile, with several bots a spec, and prints its share of the Fire mage:

```powershell
.\localTools\combatBench\bench.ps1 sweep                      # every damage spec x2 at 258/0, 450/600 and 460/650
.\localTools\combatBench\bench.ps1 sweep -profiles '460:650' -specs '4 1:Assassination;8 2:Fire' -copies 3 -repeats 2
.\localTools\combatBench\bench.ps1 sweep -profiles '460:650' -goal 1%    # a whole kill a lane (its dummies at 25%)
.\localTools\combatBench\bench.ps1 tune                       # balance0.* at 258/0, balance.* at 460/650, then 450/600
python localTools/tuning/bakeTuning.py --dry-run                 # the live overrides into the table, then build
```

- **Lanes** (`.bench lanes on`): every bench bot gets its own copy of the layout, in a phase of its own (a phase bit a
  lane, 30 at most): no AoE, cleave, chain, aura, totem or buff reaches another lane, and all lanes stand on the same
  flat ground (the geometry of a one-bot run). A lane's dummies hold its bot only. So **every spec fights solo,
  without the buffs of a party**: absolute DPS is below a party run's, compare shares within a sweep.
- **The bots** come all at once (content bots, logged in a few at a time; the driver polls `.bench list`'s
  `#benchstate` line instead of fixed sleeps), with their group role (2026-10-08 fix) and the spec's
  `StatsWeightCalculator` gear at the profile's item level. Between profiles they are geared again **in place**
  (`.bench gear <ilvl>`, empty bags, no new login) and their board set (`.bench paragon <points>`): a heat's bots are
  brought once for every profile.
- **Heats**: the default list (`BenchSweep.ps1 $DefaultSweepSpecs`: 29 damage specs, classes by id and premade specs
  by number) x `-copies 2` is two heats of 30 lanes; a spec's copies stay in the same heat and every heat carries the
  Fire mage's copies (its mean pools over all heats and runs).
- **Profiles** `ilvl:paragon[:scaling[:layouts]]`: scaling by default +10 at paragon 0, `defi10-25` above; layouts
  (`+`-separated) by default `single` on a key, `boss` (a boss dummy that stays up: one bot never brings it to its
  execute range) on a raid scaling; `-layouts single,boss` for all. `-goal 1%`: each lane fights to 1% on dummies
  at `-laneHealth` (default 25: a kill by one bot in about the time four take), its DPS over its own time; a lane
  slower than twice the median finished lane is not waited for. Compare the stays-up and kill means (below).
- **Output**: per profile, layout and spec the mean DPS, its % of the Fire mage's mean, the spread (one standard
  deviation, % of the mean), the share's standard error and n; rows not trusted are left out and listed: item level
  off by more than `-ilvlTolerance` (10), a spec other than asked, a `board tank`, a board of other points than asked,
  no damage, a bot offline. CSV: `var/combatBench/sweep-<stamp>.csv` and `-rows.csv` (every bot's row with its run
  id: the per-spell table of one is `mod_combat_bench_spell` for that run).
- **Auto-tune** (`tune`): a heat's bots brought once; at the no-paragon profile it moves `balance0.<class>.<spec>`, at
  650 `balance.<class>.<spec>` (the factor goes from one to the other by the character's points, so each end is
  measured on its own), the knob names and values read from the rows (`DescribeSpecBalance`). Each iteration runs the
  heat, takes each spec's share of the mage and steps its factor by `(target / share) ^ damping` (0.7), at most
  `-maxStep` (25%) a step; a spec within `-band` (5%) or within its standard error is settled, one whose gap grew
  twice running is stuck (look at its rows and rotation). Up to `-iterations` (4), then the `-validate` profiles
  (450/600) once. It prints old -> new and the last share, and leaves **live overrides**: bake them
  (`bakeTuning.py` now bakes the table's `{ "key", value }` entries too), build, commit, naming the sweep.
- **Noise**: a bot's run varies ~15%, bots of a spec 15-20%: a share's standard error with 2 copies and 1 repeat is
  ~7%. `-repeats 2` (or `-copies 3`) before trusting a 5% band; the stuck / in-noise statuses say when the noise wins.
- Healers and tanks are not swept: their tests stay `pulse` and the `tank` layouts (one at a time, below).
- The other agent's or a player's session: lanes use the owner's raid (up to 30 bots past `CombatBench.MaxBots`) and
  the content bot pool (8 a class a side: 2-3 copies of every spec of a class fit).

## Tuning loop (one spec): a bench session and live tuning

**Use this for any tuning pass.** A loop of change → run → read costs only the runs: no driver build, no login, no
bots brought again (the session), and no server build, restart or client release (live tuning).

```powershell
.\localTools\combatBench\bench.ps1 start                                    # log the session in (once, ~5 s)
.\localTools\combatBench\bench.ps1 bots 'mage fire aoe;warrior arms aoe'     # the bots (replaces them, ~20 s)
.\localTools\combatBench\bench.ps1 bots 'shaman resto;mage fire' -pulse 12   # a healer test
.\localTools\combatBench\bench.ps1 run -layouts single,pack5 -seconds 30     # reports + per-spell tables
.\localTools\combatBench\bench.ps1 run -tune '.tune spell 47486 1.15','.tune set warrior.colossus_factor 1.25'
.\localTools\combatBench\bench.ps1 cmd '.tune list warrior'                  # any chat command, its replies
.\localTools\combatBench\bench.ps1 stop
```

- **Live tuning** (`src/server/game/Tuning/LiveTuning.h`, `.tune`, game masters and the console):
  - `.tune spell <id> <x>` scales one spell's damage and healing at once - direct, over time, a pet's or a totem's,
    whatever the number comes from (spell data, coefficients, scripts). `1` drops it. The quick way to find the
    right factor for a spell; then put it in the spell's data or script (a multiplier does not change the tooltip)
    and reset it.
  - `.tune set <key> <value>` changes a knob: a balance number of a class module, declared
    `LiveTuning::Knob const Name("class.name", value)` (`KnobInt` / `KnobUInt` for integers) where a `constexpr`
    used to be. `.tune list <filter>` shows the knobs and their code values, `.tune reset <key>|all`.
  - Overrides are kept in the world database (`live_tuning`, `live_tuning_spell`) across restarts. When a pass is
    done, `python localTools/tuning/bakeTuning.py` writes every knob override into its declaration as the new default
    and drops it from the database (`--dry-run` first); spell multipliers it only lists. Then build and commit - the
    code stays the one place a number lives.
- The stat overflow's rates are knobs too (`overflow.crit_rate`, `overflow.precision_rate`,
  `overflow.robustness_rate`, `overflow.robustness_cap`; `.agents/docs/systems/stat-overflow.md`): they move every
  spec at high paragon at once, so settle them before the per-spec `balance.*`.
- `.bench paragon <points|auto>` sets the bench bots' paragon whatever the dummies' scaling: a raid's or a Défi's
  profile (the Hollow Voice: `bots 'mage fire 460 single'`, `.bench paragon 650`, then the dummies below).
  Measured that way (2026-10-01), melee specs had a fraction of the mage's damage at 650 points (Enhancement 50%,
  Combat 41%) while beating it at low paragon: compare classes at the content's own profile, not only at +10.
- **At a high profile, measure on a boss that lives.** The `raid` boss dummy (Défi I) dies and refills every 4-8 s
  under four bots at 450 / 600: the paragon board's execute nodes (up to +60% under 40% health, the melee boards'
  much more than the caster's) are lit half the time, and DoTs reset. Use a dummy that stays up
  (`run -layouts boss -key defi10-25 -seconds 60`) and a whole kill (`run boss defi10 1%`, about 75 s: a raw request
  line, `bench.ps1 run` takes whole seconds only), and compare their mean. On 2026-10-08 Assassination measured 92%
  of the mage on the `raid` dummy, 80% on a kill and 70% on one that stays up.
- A server restart ends the session (`bench.ps1 start` again). Its log: `var/combatBench/session/session.log`.
- Short runs (`-seconds 30`, `single,pack5`) to find the direction, then 60 s and the full layouts, repeated, to
  settle it (variance below).

## Spec balance (the last step)

`modules/mod-stat-growth/src/StatGrowthScripts.cpp` `SpecBalance`: one damage factor per class and spec (its pets'
and totems' hits too), applied before mitigation so meters show it. Two of them, since specs drift apart with
paragon: `balance0.<class>.<spec>` at no paragon (measured at +10, ilvl 258) and `balance.<class>.<spec>` at 650
points (the Hollow Voice's 460 / 650), in between by the character's points. The auto-tune above moves both. Spec: its tree, 1 to 4 (Arms 1, Fury 2).
Fix a spec's own spells first; use these to bring a spec to the Fire mage at both ends, then bake them into the
table (`.tune set balance...` live, the table's defaults in the code). Compare against the mage's **mean** over all
groups of a survey: one group's mage varies ±30% at +10.

Bench content bots (Bot/ContentBotMgr.h) come fresh: logged in, specced and geared for the bench, bags emptied. A
row at the wrong item level or without its paragon board (`.bench list` shows it; the sweep flags it) is not to be
trusted, nor a damage dealer with a `board tank`: before 2026-10-08 the bench gave its bots no group role, and the board took one from a
Dungeon Finder role left over or a stance - Retribution bots that had tanked keys fought at a quarter of the damage.
Bots of the same spec still differ by 15-20% (Assassination: Itlonk against Buslill): pool several bots' runs.

## Running it headless (one run)

```powershell
# default layouts: single,pack5,pack8,pack12 at +10, 45 s each
.\localTools\combatBench\runBench.ps1 -bots 'mage fire aoe;rogue sub aoe'
.\localTools\combatBench\runBench.ps1 -bots 'hunter bm aoe;hunter mm aoe' -layouts 'pack5,pack12' -key 15 -seconds 60
```

- Needs the local auth + world servers running (a server change must be built **and** restarted first:
  `deployWithProgress.ps1 -steps server,restart`). Check no real player is online before restarting.
- A GM client (`e2e/tools/bench`, AzerothGhost) logs in, brings the bots (`.bench bot <class> [spec] [ilvl]
  [single|aoe|auto]`, one per `;`), runs every layout, prints the reports, then a per-spell table from the telemetry
  (casts, hits, **hits per cast**, per hit, share). Full log in `var/combatBench/`.
- Healers: `-pulse <percent>` turns on the group damage pulse (`.bench pulse`) for the run; bench one healer at a
  time beside damage dealers (several healers share the damage to heal, and the quickest takes it all).
- Up to 4 bots per run (the GM fills the 5th group slot). Spec words are the playerbots premade names: `bm`/`mm`/
  `surv`, `fire`/`arcane`/`frost`, `sub`/`assa`/`combat`, `ret`/`prot`/`holy`, `frost`/`unholy`/`blood`...
  A bot line in the report without its `spec / preset` label was not taken as asked (a different bot, preset not
  applied): do not trust that row, run again.
- Setup it handles: the mandatory Gear Bonuses addon handshake (else the client is kicked after the grace time);
  AzerothGhost v1.0.8 with the logon OS-field fix, as a sibling checkout `..\AzerothGhost` (`e2e/go.work`); Go from
  PATH or `%USERPROFILE%\sdk\go*`.

## Reading the results

- **Reference: Fire mage is the tuned class.** Compare every spec as a share of the mage's DPS on the same layouts,
  in the same run. Mage at +10, ilvl ~258, 45 s (average of several runs): single ~4.6k, pack5 ~14.8k, pack8 ~25k,
  pack12 ~28k.
- **Variance is about ±15% per run** (big cooldowns land 1-2 times in 45 s). Do not tune on one run's 10% gap; repeat,
  or use 60-90 s.
- **Hits per cast reveal target caps**: a value flat across pack5 and pack12 (e.g. 3.9 for WotLK Multi-Shot, 3.3 for
  Divine Storm) means a cap, not low damage. Then fix the cap before touching numbers.
- **Share per spell shows damage distribution** (one ability taking most of the damage, a builder dealing nothing).
- `Targets:` spreads damage per dummy; zeros on a melee spec usually mean positioning.
- Telemetry rows: `mod_combat_run.run_type = 3`, `mod_combat_bench_spell` (`kind = 1` damage), exported by
  `localTools/combatTelemetry/exportCombatTelemetry.ps1`.

## Pitfalls met so far

- **Bench geometry penalises melee on `pack8`** (and cones / target-centred splashes in general): some dummies of the
  ring stay out of reach. Judge melee AoE mostly on pack5 and pack12; expect a bit more in real dungeon packs.
- **Bots are not players**: the bot's rotation can hide or create a gap (Subtlety bots counted enemies at 8 yd while
  their AoE reached 10, and played Sinister Strike on packs). When a spec looks off, check its casts in the per-spell
  table and the bot strategy (`modules/mod-playerbots/src/Ai/Class/<Class>/`) before changing numbers.
- **Check the bench bots' weapons before tuning a weapon-damage class**: the bench gears bots by
  `StatsWeightCalculator`, and a class without a weapon rule there takes whatever scores best. The Barbarian (no
  rule) got a fast one-hand sword and a shield: tuned on it, its weapon-percentage strikes came out 2.2 times too
  strong on the two-hander players use. Give a new class its weapon rule first (two-hander, dual wield, shield).
- **And its stat weights**: `StatsWeightCalculator::GenerateBasicWeights` falls back to a bear tank's (stamina,
  defence, dodge) for a class it does not name. The Oathblade wore tank plate and stamina trinkets that way, on the
  bench and on the Défi board, at about two thirds of its damage; tuned on that, its factors were far off once it
  had damage gear (2.6 times the mage at +10). The Barbarian, the Faucheur, the Necromancer and the Pestiféré got
  theirs on 2026-10-08: the Barbarian went from about 80% of the mage at 450 / 600 to 110%, and 1.75 times it at +10,
  before its balance factors were measured again. Weapon speed rules in `ApplyPreferredSpecWeapons` are off in this
  realm (`AiPlayerbot.PreferredSpecWeapons = 0`): a speed a kit needs goes in `CalculateItemTypePenalty`.
- Content bots exist only for the classes random bots may be (`RandomPlayerbotFactory::IsRandomBotClass`): the
  Pestiféré and the Necromancer (no bot AI) cannot be benched.
- **Cloned DBC spells keep their clone's scaling**: a Death Knight spell cloned from Cone of Cold scales with spell
  power the class does not have (flat damage). Scale it in the class module (`ModifySpellDamageTaken`) or from attack
  power, not only in `localTools/<class>/Spells.ps1`.
- **Bench a tank on its own** (`tank`, `tankpack` layouts). With a tank bot in the run, the damage dealers' threat
  strategy zeroes their actions once their threat nears the tank's: a Fury warrior measured 7.9k on pack12 next to a
  Protection bot and 20.5k alone.
- **A bot action typed `ActionThreatType::Aoe` is dropped by the same threat strategy** near a tank: give a damage
  dealer's area spells the single-target type, as the stock Whirlwind and Cleave have (the retail Warrior's
  Thunderous Roar and Warbreaker were never cast before that).
- **`OnSpellPrepare` fires after an instant spell has already been cast** (`Spell::prepare` casts, then calls the
  hook): anything that must act before targets or damage (`SetSpellValue(SPELLVALUE_MAX_TARGETS, ...)`, marking the
  cast) goes in `OnSpellCheckCast` (runs after the spell scripts load, before target selection).
- **A summoned guardian's travelling spell never lands**: a hit a module has a guardian cast (`CastCustomSpell` from
  the creature) with a missile speed was cast (`SPELL_CAST_OK`) but never hit - the Warlock's Wild Imps' bolts and the
  Darkglare's beam showed nothing in the per-spell table. Give such hits speed 0 (the visual still plays).
- **A bot class name used twice crashes the world server**: the Druid's `CastFlourishAction` beside the Oathblade's
  (both in mod-playerbots, different translation units) broke the one-definition rule; the linker kept one class's
  inline code for both and a bot's action read garbage (`getName` crash in `Engine::DoNextAction`). Grep the whole
  `src/` for a new bot class name before adding it, or prefix it with the class.
- The Druid's bench words: `balance`, `cat` (Combat farouche's cat builds), `bear` (its tank build), `resto`.
- Stock target caps live in the spell data (`MaxAffectedTargets`, effect `ChainTarget`); override them in
  `SpellInfoCorrections.cpp` or per cast in `OnSpellCheckCast`, and pair an uncap with a falloff past 5 targets
  (`sqrt(5 / n)`, as Hunter's Multi-Shot / Beast Cleave).
- A class module's knobs and spell multipliers tune live (`.tune`, above); a new knob or any other C++ change needs a
  server build + restart; a `Spells.ps1` tooltip or data change also needs a client release (`-steps client,publish`,
  WoW closed) - try its factor with `.tune spell` first.

## Where each class is tuned

Per-class modules: `modules/mod-mage`, `mod-rogue`, `mod-paladin`, `mod-death-knight`, `mod-hunter`,
`mod-priest`, `mod-warrior`, `mod-shaman`, `mod-warlock`, `mod-druid` (knobs and constants at the top of their
sources), shared talent-tree code in `mod-custom-classes`; spell data in
`localTools/<class>/Spells.ps1`, talent trees and presets in `localTools/<class>/talentTree.json`. Name the bench
in the commit when a change was tuned on it.
