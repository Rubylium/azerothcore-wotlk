# Map editing

Terrain, textures, water, and the placement of objects (trees, rocks, buildings) on the 3.3.5 maps, with **Noggit**,
the community's map editor. What it saves ships to players in our patch, and the server gets the matching terrain,
collision and pathing.

```
Noggit ──saves──> clientPatcher/maps/World/Maps/<Map>/<Map>_<x>_<y>.adt
                      │
                      ├── client build (deployWithProgress -steps client,publish): packed into patch-X.MPQ
                      └── rebuildServerMaps.ps1: server/Data/maps, vmaps, mmaps (then restart the world server)
```

## Setup (done once)

- **Noggit 3** (wowdev, the editor made for 3.3.5; build test-3580) is unpacked in `.deps/noggit` (gitignored). To set
  it up again: download `noggit_3.3580.zip` from https://github.com/wowdev/noggit3/releases and unzip it there.
  *Noggit Red* (https://gitlab.com/prophecy-rp/noggit-red), the maintained fork, works on 3.3.5 too but only ships
  through its Discord: unzip it in `.deps/noggit-red` and open it with the same game and project paths if you prefer
  it.
- The server's map tools (map_extractor, vmap4_extractor, vmap4_assembler, mmaps_generator) are built: the build is
  configured with `-DTOOLS_BUILD=maps-only` and each is built on its own
  (`MSBuild build\src\tools\<tool>.vcxproj /p:Configuration=RelWithDebInfo /p:Platform=x64`). The regular server
  build still only builds the auth and world servers.

## Editing

1. `powershell -File localTools/mapEditing/openNoggit.ps1` (`-bindless` for the faster renderer, if the graphics card
   takes it). It points Noggit at the game client - through a view of it with only its real locale folder (`.view`:
   the launcher leaves an `enUS` folder with a realmlist in it, and Noggit would crash on it) - and at
   `clientPatcher/maps` as its project.
2. Pick the map on the left, double-click where to go on the minimap. Camera: right mouse to look, WASD to fly,
   shift to go faster. Tools on the left: raise/lower and flatten (terrain), paint (textures), holes, water, object
   placement (models from the client's own files).
3. **Ctrl+S** saves the tiles you touched into `clientPatcher/maps` (only those: an untouched tile stays the stock
   one). Commit them like any other asset.

## Shipping

- **Players**: a client build packs `clientPatcher/maps` into patch-X (`localTools/mpq-builder/patchFiles.js`, first of
  the archive-path folders): `deployWithProgress.ps1 -steps client,publish`.
- **The server**: `powershell -File localTools/mapEditing/rebuildServerMaps.ps1`, then restart the world server
  (`-steps restart`). It finds the edited maps and tiles itself from the saved file names (Map.dbc's directory names).
  - `-skipVmaps` when only the ground changed (no object added, moved or removed): collision is kept, much faster.
  - `-allTiles` rebuilds a map's pathing everywhere (hours for a continent) instead of only the edited tiles.
  - `-maps 0,1` forces which maps.

  Why it is needed: the server never reads the client's files; it reads heights, water and collision it extracted
  once. Without it, a raised hill is walked through, a new wall lets spells and creatures pass.

  How: the extractors only open the stock archive names (patch.MPQ to patch-5.MPQ), never our lettered patches. The
  script stages the client's stock archives (hard links, `.work`, gitignored) with the edits packed as patch-5.MPQ
  (`localTools/mpq-builder/buildMapArchive.js`), extracts there, and copies only the edited maps' files into
  `server/Data`. Its dbc folder (ours, custom) is never touched.

## Limits

- Editing an existing map is this pipeline. A **new** map (a new zone or instance) also needs rows in Map.dbc and
  AreaTable.dbc, the server's map entries and its loading screen: a separate piece of work.
- Edit the terrain of instances and continents alike, but keep big object placements to WMOs and M2s the client
  already has: a new model is a retail import (`.agents/docs/systems/retail-import.md`).
