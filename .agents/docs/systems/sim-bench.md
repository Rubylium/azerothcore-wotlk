# Simulation bench (class tuning at many times real speed)

The combat bench (`combat-bench.md`) run on world servers of its own - **simulation workers** - with the real game code
(spells, scripts, custom classes, paragon, stat overflow, playerbot AI) on a clock that runs as fast as the world
updates while a test is under way. Several workers at once, each on its own copies of the characters, playerbots and
auth databases; the live realm is never touched (no restart, no bots on it, live tuning only in the workers' memory).
**Use it for every class survey and spec balance pass** instead of the live bench.

```powershell
.\localTools\simBench\simBench.ps1 prepare -workers 4          # the built server and the databases copied (once)
.\localTools\simBench\simBench.ps1 start -workers 4            # the workers up (~20 s; they stay up)
.\localTools\simBench\simBench.ps1 sweep                       # every damage spec, single target and packs, 3 profiles
.\localTools\simBench\simBench.ps1 sweep -profiles '460:650:defi10-25:boss+pack5' -specs '4 1:Assassination;8 2:Fire'
.\localTools\simBench\simBench.ps1 tune                        # the spec balance, the factors into live_tuning
.\localTools\simBench\simBench.ps1 check -repeats 3            # turbo against real time on the same tests
.\localTools\simBench\simBench.ps1 cmd -worker 1 '.bench list' '.tune list balance0.mage'
.\localTools\simBench\simBench.ps1 stop
```

- `sweep` and `tune` take `bench.ps1`'s options and give its tables and CSVs (`var/combatBench`), so `bakeTuning.py`
  and the progress watcher work as before. The default sweep: every damage spec x2 at 258/0 (single, pack5, pack12),
  450/600 and 460/650 (the boss dummy that stays up, pack5, pack12).
- **Builds per layout** (`-preset layout`, the default here and in `bench.ps1`): the packs are fought on each spec's
  area build, the single targets on its single-target one (`.bench preset`, switched in place between runs).
- **A heat a worker**: `-lanes 0` sizes the heats so each worker takes one (a worker with fewer bots runs faster).
- `tune` moves each worker's knobs in memory, gives every worker the factors found, and writes them to
  `acore_world.live_tuning` (`-noApply` not): bake them (`bakeTuning.py --dry-run`), build, commit, deploy.
- After a server build: `prepare -noDatabases` (the binaries copied into `var/simBench/bin`), `stop`, `start`. After a
  schema change, or to take the live characters as they are now: `prepare` (the databases dumped and loaded again,
  ~3 min for 4 workers).

## How it works

- **The clock** (`Timer.h`): `getMSTime`, `GetTimeMS`, `GetGameClockNow` and so `GameTime` add a *warp*, how far a
  simulation's world loop has run ahead of the real clock. Only `WorldUpdateLoop` (`Main.cpp`) moves it: in turbo
  each update is a whole `Sim.StepMs` (10) of game time without waiting. 0 on a live server. Infrastructure keeps the
  real clock (`getRealMSTime`: the loop's pacing, the freeze detector, database deadlock retries).
- **Turbo only while a test runs** (`CombatBench.cpp UpdateSimTurbo`): bot logins, gearing and the database's answers
  happen in real time, so no game-time timeout fires early while one is awaited.
- **Everything gameplay on the game clock**: proc cooldowns and the paladin script (`GameTime::Now`), the task
  scheduler, and playerbots' `time(nullptr)` (all `GameTime::GetGameTime`). A new gameplay timer must read
  `GameTime`/`getMSTime`, never `steady_clock`/`system_clock`/`time()` - or it runs at real speed in the simulation and
  `check` shows it.
- **No player**: the worker logs its own owner in (`Sim.Owner`, Evoguerrier, a game master) without a client, in game
  master mode at the bench; the bench commands work from the console (`Console::Yes`), and what the bench tells its
  owner goes to the standard output (`[bench] ...`). `simWorkerHost.ps1` runs the server with its console on pipes
  and answers request files in the bench session's language (`.cmd`, `bots`, `run`, `settle`), so `BenchSweep.ps1`'s
  heat functions drive a worker as they drive the live session.
- **Only maps with a player update** in a simulation (`MapMgr::Update`): the continents' own creatures took most of
  an update's time. The Raid Finder's automated key groups are off in workers (`RaidFinder.AutoMythicTeams = 0`).
- Each test prints `#simspeed` (game ms, real ms, the factor, ms an update) and `#simprofile` (each world section's
  and map's time over the test) - the worker's `host.log` has them.

## Pitfalls

- **A bench bot's paragon 0 must be an override**: without one, a bot's board takes the average of its group's real
  players' - in a worker the owner (600 points), so a 258/0 sweep fought at 600 (`SetBotParagonBudgetOverride` takes
  `std::optional`; `nullopt` hands the bot back to its content).
- The workers' world database is the live one: never let a worker write it (`Updates.EnableDatabases = 0`, live
  tuning not persisted in a simulation - `LiveTuning.cpp Persist`).
- The worker's executable is `simworld.exe` (a copy): the live tools find the realm's server by the name
  `worldserver`.
- The MySQL user `acore` has rights on `acore\_sim%` (granted by `prepare`, as the local root).
