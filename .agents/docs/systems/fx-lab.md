# FX lab

A plain gray room to look at spell visuals in: nothing on the floor but a measuring grid, gray walls, an even
neutral light. Preview a spell's kits, a ground indicator, an imported or new visual there before putting it in a
fight. Game masters only.

## Where

- Map 451, `development` (the old Programmer Isle): a stock test map nothing uses, written anew. Only its tile `30_26`
  exists. Not QA_DVD (606), tried first: nothing wrong with it, but nothing gained either.
- The floor is flat at height 0: 333 x 333 yards (10 x 10 map chunks), its middle at **(2933.33, 800, 0)**. Around
  it a plateau 40 yards up, one vertex (4.17 yards) away from the floor: the wall. Past the plateau, nothing.
- Measuring: a faint line every cell (4.17 yards, the terrain texture's repeat - it cannot be 5), a strong line on
  every chunk edge (33.3 yards) crossing at the middle, rings round the middle every 5 yards out to 40 (the tens
  stronger).
- Light: its own gray Light row (2600, LightParams 950), the same at every hour, no skybox, no clouds, no fog in the
  room. `.daytime` changes only the sun's angle (wall shading).
- No area id (no zone name), no water, no objects, no creatures but the ones the commands place.

## Commands (`modules/mod-stat-growth/src/FxLab.cpp`)

- `.fxlab` - to the middle (`.tele FxLab` too), every buff and debuff the last test left taken off (a boss's debuff, a
  carried look such as Vorhan's seat number; passives stay). `.fxlab back` - where it was typed from (until a restart).
- `.fxlab dummy [big]` - the Adaptive AoE Training Dummy (900100: no damage taken, the player's level) 10 yards
  ahead, facing you; `big` three times its size. Gone after 2 hours or with `.fxlab clear` (only your own; it clears
  your buffs and debuffs too).
- `.fxlab shape <key|spell> [radius] [seconds]` - a ground indicator of `localTools/groundIndicators/shapes.json`
  (read when asked: a new shape needs no restart; `.fxlab shapes [filter]` lists them). Lines and cones start at
  your feet, pointing at your target or ahead; anything else lies on your target or just ahead of you. Defaults:
  5 yards, 6 s (at most 120). Models built to their size (pieces, curtains, billboards) and carried looks (worn by
  your target, or you) ignore the radius.
- `.fxlab cast <spell>` - you cast the spell, triggered (no cost, class or combo point needed), on your target, else
  your nearest dummy: a whole spell's look without selecting anything (the screenshot runs cannot select: targeting
  is protected in the dev addon).
- `.fxlab kit <SpellVisualKit id>` - plays the kit (`SMSG_PLAY_SPELL_VISUAL`) on your target, else your nearest
  dummy, else you. A whole spell: `.cast` it on a dummy as usual.

## Rebuilding it

Everything comes from `localTools/fxLab/buildFxLab.py` (sizes, grays, wall height, light colours are constants at
its top):

1. `python localTools/fxLab/buildFxLab.py` writes `clientPatcher/maps/World/Maps/development/` (the WDT with its one
   tile, an empty WDL, the ADT written from nothing), `clientPatcher/maps/Tileset/Evolutions/FxLab*.blp`, and the
   light rows into `server/Data/dbc/Light*.dbc`.
2. Server terrain: `powershell -File localTools/mapEditing/rebuildServerMaps.ps1 -maps 451 -skipVmaps` (no
   objects, so no collision to extract).
3. Client and server: `localTools/deployWithProgress.ps1 -steps client,restart,publish`.

The client build adds the light rows again itself (`buildFxLab.py --light-only`, from
`clientPatcher/build/ClientGeneration.ps1`; `server/` is not in git) and ships the four Light tables in patch-Z
(`localTools/mpq-builder/patchFiles.js`). The ADT is a plain v18 file Noggit can open too, but a Noggit edit is
overwritten by the next `buildFxLab.py` run: change the generator instead.

## Limits

- The map's minimap and loading screen are the stock development ones.
- Terrain cannot be perfectly vertical: the wall is a one-cell slope (about 84 degrees).
- Terrain textures: 512 x 512 DXT3 BLPs with an `_s` specular twin, as the stock tilesets. A BLP whose header gives
  the wrong size (the writer once recorded the last mipmap's 1 x 1) draws bright green on terrain, while the
  interface still shows it: the green floor of the first version.

## Seeing it from here

`localTools/fxLab/shots/shoot.ps1 -steps <file.lua>` logs the EVODEV account's `Evoguerrier` into the world on the dev
client (`CleanWOTLK`), runs the steps (chat lines - game master commands - and Lua functions, `Shot()` for a
screenshot, `Note(text)` into the chat), then quits and collects the screenshots (`.agents/plans/fx-lab/shots`). The
dev addon is installed for the run only. Never run it while someone plays on that client: the client patch is rebuilt
under it.
