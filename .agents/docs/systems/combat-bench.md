# Combat bench (class tuning, rotations, AoE / single-target balance)

The in-game test ground (`.bench`, GM Island) measures damage, healing and damage taken on dummies scaled exactly
like Mythic+ creatures. **Use it for any class tuning, new class or spec, talent / rotation change, or bot AI combat
change** instead of dungeon runs: a full comparison takes ~6 minutes. In-game usage and every command:
`modules/mod-playerbots/COMBAT_BENCH.md`. Code: `modules/mod-playerbots/src/Script/CombatBench.cpp`, measuring in
`src/server/game/Telemetry/CombatTelemetry.cpp` (`Bench*`).

## Running it headless

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
- **Cloned DBC spells keep their clone's scaling**: a Death Knight spell cloned from Cone of Cold scales with spell
  power the class does not have (flat damage). Scale it in the class module (`ModifySpellDamageTaken`) or from attack
  power, not only in `localTools/<class>/Spells.ps1`.
- **`OnSpellPrepare` fires after an instant spell has already been cast** (`Spell::prepare` casts, then calls the
  hook): anything that must act before targets or damage (`SetSpellValue(SPELLVALUE_MAX_TARGETS, ...)`, marking the
  cast) goes in `OnSpellCheckCast` (runs after the spell scripts load, before target selection).
- Stock target caps live in the spell data (`MaxAffectedTargets`, effect `ChainTarget`); override them in
  `SpellInfoCorrections.cpp` or per cast in `OnSpellCheckCast`, and pair an uncap with a falloff past 5 targets
  (`sqrt(5 / n)`, as Hunter's Multi-Shot / Beast Cleave).
- Tuning in C++ constants needs a server build + restart only; a `Spells.ps1` tooltip or data change also needs a
  client release (`-steps client,publish`, WoW closed).

## Where each class is tuned

Per-class modules: `modules/mod-mage`, `mod-rogue`, `mod-paladin`, `mod-death-knight`, `mod-hunter`,
`mod-priest` (constants at the top of their sources), shared talent-tree code in `mod-custom-classes`; spell data in
`localTools/<class>/Spells.ps1`, talent trees and presets in `localTools/<class>/talentTree.json`. Name the bench
in the commit when a change was tuned on it.
