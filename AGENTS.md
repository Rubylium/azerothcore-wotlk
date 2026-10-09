# AGENTS.md

AzerothCore is a C++ MMORPG server emulator for World of Warcraft 3.3.5a (WotLK), built with CMake, backed by MySQL.

## Agent rules

- **Do not configure or build unless explicitly asked.** Builds are slow and rarely needed for code changes.
- **Never edit SQL files outside `data/sql/updates/pending_db_*/` unless explicitly requested.** `data/sql/base/`, `data/sql/archive/`, and `data/sql/updates/db_*/` are immutable.
- Formatting follows `.editorconfig`: UTF-8, LF, max 120 cols, trailing newline, no trailing whitespace; 4-space indent for C++ (tabs forbidden), 2-space for JSON/YAML/sh/ts/js.
- **Prefer live-stack e2e to debug/validate player-visible behaviour** when a local auth+world+MySQL stack is available (protocol, combat, quests, loot, death, multi-bot). See `e2e/README.md` and AzerothGhost `e2e/LLM_GUIDE.md`. Do not invent e2e for pure unit-sized logic — see `.agents/docs/e2e-policy.md`.
- **Scratch e2e only under `e2e/local/`** (gitignored). Never commit throwaway debug tests. Promote keepers into `e2e/suites/` or `e2e/smoke/`.
- Planning docs go in `.agents/plans/<task-slug>/` (gitignored), named `<task-slug>.<TYPE>.md` (`PLAN`, `REQUIREMENTS`, `ANALYSIS`, …).
- **Credit upstream authors.** Code, a mechanism, or data mirrored from another core (TrinityCore, cMaNGOS, …) is committed with `--author` naming the original commit's author (extra sources as `Co-authored-by`), even when rewritten against AC or confirmed by own sniffs; find them in the upstream file's commit history.

## Mandatory reading per task

Read the matching doc(s) BEFORE starting the task:

- Compiling, configuring, or running tests → `.agents/docs/build.md`
- Writing or modifying C++ → `.agents/docs/cpp-guidelines.md`
  - Script work (under `src/server/scripts/`) → also `.agents/docs/cpp-scripts.md`
- Creating or modifying SQL → `.agents/docs/sql-guidelines.md`
  - SmartAI work (`smart_scripts` data) → also `.agents/docs/cpp-scripts.md`
- Reviewing a changeset or PR → `.agents/docs/code-review.md`
- Self-reviewing, or opening or updating a PR → also `.agents/docs/self-review-rules.md`
- Touching a subsystem that has a doc in `.agents/docs/systems/` → read that doc too
- Content difficulty (a boss, a key, a tier, a DPS check), item levels, rewards or paragon → `.agents/docs/systems/power-scaling.md` (size content for an item level and a paragon, never by hand)
- Class tuning, a new class or spec, talent / rotation / bot combat changes → `.agents/docs/systems/combat-bench.md` and `.agents/docs/systems/sim-bench.md` (measure on the simulation bench, many times real speed, never on dungeon runs)
- Stat caps, combat ratings, crit / hit / expertise / avoidance / armour maths, or the character sheet's stats → `.agents/docs/systems/stat-overflow.md` (past a cap a stat converts into a second bonus)
- Importing retail item models (weapons, shoulders) into the 3.3.5 client, or any custom item/display id → `.agents/docs/systems/retail-import.md`
- A class's own resource display (the draggable class HUD, its server feed) → `.agents/docs/systems/class-hud.md`
- Any new sound (playing one from the server, a sound bank, its volume, 3D, echo) → `.agents/docs/systems/evolutions-audio.md` (our own engine; never SoundEntries rows)
- A legendary or Unique item (a new one, its power, its icon art) → `.agents/docs/systems/legendaries.md`
- Boss loot thrown on the floor (the Diablo-style ground loot: its drops, sounds, Défi wait) → `.agents/docs/systems/ground-loot.md`
- Map editing (terrain, water, object placement with Noggit; the server's maps, vmaps and mmaps) → `localTools/mapEditing/README.md`
- Previewing spell visuals, kits or ground indicators in game (the plain gray FX lab, `.fxlab`, map 606) → `.agents/docs/systems/fx-lab.md`
- Camera flights / intro cinematics (a boss introduction, a scripted camera, the client extension's CameraPath) → `.agents/docs/systems/cinematics.md`
- Writing, debugging, or changing live-stack e2e (`e2e/`) → `e2e/README.md`, `.agents/docs/e2e-policy.md`, and AzerothGhost `e2e/LLM_GUIDE.md` (scratch work → `e2e/local/`)
- Capturing a lesson or adding/updating agent docs → `.agents/docs/README.md`

## Repository layout

- `src/common/` — networking (Asio), crypto, config, logging, shared utilities.
- `src/server/game/` — core gameplay; compiled into worldserver.
- `src/server/scripts/` — content scripts grouped by region (`EasternKingdoms/`, `Northrend/`, …), class (`Spells/spell_mage.cpp`, …), and domain (`Commands/`, `Pet/`, `OutdoorPvP/`, `World/`).
- `src/server/database/` — DB abstraction and schema updater.
- `src/server/shared/` — code shared by auth and world servers.
- `src/server/apps/{authserver,worldserver}/` — entry points (ports 3724 and 8085).
- `src/test/` — unit tests + mocks.
- `e2e/` — live-stack Go e2e (AzerothGhost harness); see `e2e/README.md`. Scratch/debug tests: `e2e/local/` (gitignored).
- `data/sql/` — `base/` (historical schema), `updates/db_*/` (merged), `updates/pending_db_*/` (in-flight), `custom/` (gitignored).
- `modules/` — external modules (see below).
- `apps/` — helper scripts; `apps/codestyle/` holds the lint scripts.
- `conf/dist/` — distributed config templates; `conf/*.conf` is gitignored.
- `deps/` — vendored third-party dependencies.

## Modules

External modules live in `modules/`, each a subdir with its own `CMakeLists.txt`. Disable with `-DDISABLED_AC_MODULES="mod1;mod2"`. See `modules/how_to_make_a_module.md`.

## Persisting lessons

When a user correction reveals a lesson that generalizes, offer to persist it into these docs (placement per `.agents/docs/README.md`): use the `/self-improve` skill if installed, otherwise suggest the user to install it and read this page: https://www.azerothcore.org/wiki/agentic-engineering
