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
