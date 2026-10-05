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

## Adding sounds

1. Describe them in `modules/<module>/client-assets/audio/<feature>.json`:
   `{ "sounds": { "<Feature.Name>": { "kind": "world", "files": [ "<wav>" ], "minDistance": 12, "maxDistance": 60,
   "volume": 1, "loudness": -12 } } }` - a key has no tab, `;`, slash nor space; several files play at random.
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

- DLL: `EvolutionsAudio.cpp` (engine, bank, Lua API `EvolutionsAudio_Play/StopOn/SetEnvironment/Reload/Keys`),
  `MiniAudio.cpp` (miniaudio's implementation). Build: `localTools/buildClientDll.ps1` (or the deploy tool's `dll`).
- Client glue: `clientPatcher/interface/Interface/FrameXML/EvolutionsAudio.lua` (the server's `EVA` whispers, the
  place every 0.25 s, `/eva`).
- Bank builder: `localTools/audio/buildAudio.py`. Server helper: `modules/mod-stat-growth/src/EvolutionsAudio.*`.
- First user: the ground loot (`ground-loot.md`).
