# Evolutions audio (our own sound engine)

Sounds for our content are played by our own engine in the client, not by the game's: the client extension DLL
(awesome_wotlk fork, `src/AwesomeWotlkLib/EvolutionsAudio.cpp`, on miniaudio 0.11.25 in `deps/miniaudio`). The game's
way (SoundEntries rows, `SMSG_PLAY_SOUND`, kit sounds) needed a client release for every volume change, could not be
heard before a release, and some of its paths never played (2026-10-05): don't add new sounds that way.

## What it does

- 3D from the camera (the listener, every frame): a sound at a point, or following an object (it stops by itself when
  the client no longer knows the object - a drop picked up, a creature out of sight), or an interface sound.
- Distance: full volume within a sound's min distance, linear to nothing at its max.
- Walls: every 150 ms a world sound tests the line from the camera to a yard above it (the game's own line test,
  `0x7A3B70`, flags `0x100111`: terrain and buildings); behind one it is low-passed (1.2 kHz) and quieter, eased.
- The place: world sounds feed a reverb (Freeverb) whose size and level follow where the player is - dry outdoors, a
  room indoors (`IsIndoors()`), long and dark under water (the breath timer) with every world sound muffled.
- The game's settings: master and effects volume, sound / effects switched off, "sound in background" (window focus).
- Kinds: `ui` (no position, no room), `world` (positioned, room, walls), `loop` (the same, repeating).

Not yet: the place's acoustics come from three presets, not from the game's own per-area reverb data
(`SoundProviderPreferences` through AreaTable / WMOAreaTable: needs the client's current area in the DLL).

## Ambience (zones that feel alive)

Emitters in the same JSON (`"emitters": [...]`), played by the engine itself while the player is in their zone - no
server: `{ "zones": ["Stormwind City", "Hurlevent"], "sound": "<key>", "points": [[x, y, z], ...], "interval":
[min, max], "speed": 0, "time": "any" | "day" | "night", "volume": 1 }`.
- Zones by name, as `GetRealZoneText()` gives them, in every locale played (enUS and frFR at least).
- A `loop` sound plays from the first point while the camera is within its reach (+10 yd), faded in and out over
  1.5 s, starting at a random point of its file (two alike never in step); any other sound every min to max seconds
  from one point at random (the first only after a first wait).
- `speed` (yards a second): the emitter flies round its points as a closed path, its sounds following it (gulls
  circling the harbour, pigeons over a square).
- `time`: day is 6:00-21:00 server time (`GetGameTime`, the glue).
- Positions: from the world database (creatures, gameobjects of the place: `creature`, `gameobject` - their
  `zoneId` is not filled, select by coordinates), heights from creatures standing there. A city's fountains, lamps,
  banners and hourly bells already have the game's own sounds: don't double them.
- A place's loudness comes from layering: several crowd beds (loops at different points), many voices from many
  points (greetings, farewells, vendor lines, laughs - one every second or so in a busy square), birds overhead.
- First zone: Stormwind (`modules/mod-stat-growth/client-assets/audio/stormwind.json`): the Trade District square as
  its heart, the bank, taverns, Cathedral Square, the Dwarven District forge, the harbour, the park, Old Town.

## Adding sounds

1. Describe them in `modules/<module>/client-assets/audio/<feature>.json`:
   `{ "sounds": { "<Feature.Name>": { "kind": "world", "files": [ "<wav>" ], "minDistance": 12, "maxDistance": 60,
   "volume": 1, "loudness": -12 } } }` - a key has no tab, `;`, slash nor space; several files play at random.
   A file is a path in the repository, `client:<archive path>` (the game client's own, voices in its language - the
   speech archives are read) or `asc:<archive path>` (the Ascension client's: retail sounds; some of its rows name
   files it does not ship - the build stops on them). World and loop sounds are written mono (one point in the world).
   `loudness` (dBFS RMS) defaults to the kind's (ui and world -12, loop -16: the game's own cues sit at -12 to -25);
   a limiter holds the peaks under -1 dBFS. Sounds from another game are often mixed far quieter: leave the default.
2. `python localTools/audio/buildAudio.py`: writes `clientPatcher/addons/EvolutionsAudio` (`Sounds/`, `sounds.txt`,
   the `.toc`), shipped by `Build-FriendPatch.ps1` as `Interface\AddOns\EvolutionsAudio`. Commit it.
3. Server (mod-stat-growth `EvolutionsAudio.h`): `EvolutionsAudio::Play(player, key)`, `PlayOn(player, key, object)`,
   `PlayAt(player, key, position)`, `StopOn(player, object)` - to one player (an addon whisper, prefix `EVA`).
   An object must already be in the player's sight (a creature summoned this tick may not be: play on something
   older, as the ground loot's loops play on the bag rather than its new beam).
4. Client release (`deployWithProgress.ps1 -steps client,publish`, with `dll` when the engine changed).

## Tuning in game

- `/eva list`, `/eva play <key> [target]` (on you or your target), `/eva reload` (the bank read again).
- Volumes live: edit the installed `Interface\AddOns\EvolutionsAudio\sounds.txt` (tab-separated: key, kind, volume,
  min, max, files), `/eva reload`, listen; then carry the values into the JSON (`volume`, distances) and rebuild. The
  launcher puts the published file back at its next sync.

## Pieces

- DLL: `EvolutionsAudio.cpp` (engine, bank, the ambience's emitters; Lua API `EvolutionsAudio_Play`, `StopOn`,
  `SetEnvironment`, `SetZone`, `SetDaytime`, `Reload`, `Keys`), `MiniAudio.cpp` (miniaudio's implementation).
  Build: `localTools/buildClientDll.ps1` (or the deploy tool's `dll`).
- Client glue: `clientPatcher/interface/Interface/FrameXML/EvolutionsAudio.lua` (the server's `EVA` whispers, the
  place, the zone and the time of day every 0.25 s, `/eva`).
- Bank builder: `localTools/audio/buildAudio.py` (`clientFiles.js` reads the game client's archives).
- Server helper: `modules/mod-stat-growth/src/EvolutionsAudio.*`.
- First user: the ground loot (`ground-loot.md`).
