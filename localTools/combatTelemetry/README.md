# Combat telemetry

The server records completed/depleted/abandoned Mythic+ runs and successful raid boss kills. Damage is attributed
to the owning player for pets and guardians, while `pet_damage` and `pet_hits` keep that contribution visible.
Human and bot samples are explicitly separated by `is_bot`.

No database work happens per hit. Runs are aggregated in memory and written in one asynchronous transaction when
they finish.

Export the last 30 days:

```powershell
.\localTools\combatTelemetry\exportCombatTelemetry.ps1
```

Export another window:

```powershell
.\localTools\combatTelemetry\exportCombatTelemetry.ps1 -days 7
```

TSV files are written to `var/combatTelemetry` and contain raw runs, participants, class/spec summaries, ability
breakdowns, target breakdowns, route events, and `pulls.tsv`: the Mythic+ tank route's pulls (`route_pull_*`: packs,
mobs, seconds to kill, gather point reached, out of line of sight, deaths, the tank's lowest health, the adapted pull
budget) and the pulls of player tanks watched passively (`real_pull_*`).

Combat bench tests (the Terrain d'essai, `modules/mod-playerbots/COMBAT_BENCH.md`) are runs of `run_type` 3 (result 5
when the test ran its course, 6 when stopped early). They read the combat log itself, so beside the usual tables they
fill `mod_combat_bench_participant` and `mod_combat_bench_spell` (casts, crits, healing, overhealing, damage taken),
exported as `benchParticipants.tsv` and `benchSpells.tsv`.
