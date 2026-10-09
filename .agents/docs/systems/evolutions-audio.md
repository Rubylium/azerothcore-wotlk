# Evolutions audio (our own sound engine)

Sounds for our content are played by our own engine in the client, not by the game's: the client extension DLL
(awesome_wotlk fork, `src/AwesomeWotlkLib/EvolutionsAudio.cpp`, on miniaudio 0.11.25 in `deps/miniaudio`). The game's
way (SoundEntries rows, `SMSG_PLAY_SOUND`, kit sounds) needed a client release for every volume change, could not be
heard before a release, and some of its paths never played (2026-10-05): don't add new sounds that way.

## What it does

- 3D from the camera (the listener, every frame): a sound at a point, or following an object (it stops by itself when
  the client no longer knows the object - a drop picked up, a creature out of sight), or an interface sound.
- Distance: full volume within a sound's min distance, linear to nothing at its max.
- Walls: every 150 ms a world or spell sound tests the line from the camera to a yard above it (the game's own line test,
  `0x7A3B70`, flags `0x100111`: terrain and buildings); behind one it is low-passed (1.2 kHz) and quieter, eased.
- The place: world sounds feed a reverb (Freeverb) whose size and level follow where the player is - dry outdoors, a
  room indoors (`IsIndoors()`), long and dark under water (the breath timer) with every world sound muffled.
- The game's settings: master and effects volume, sound / effects switched off, "sound in background" (window focus).
- Kinds: `ui` (no position, no room), `world` (positioned, room, walls), `spell` (positioned, walls, but dry: no
  room, no water), `loop` (as world, repeating), `music` (below). **A spell's sound (a class's cast or impact, a
  boss's ability) is `spell`**: the place's reverb on them was heard as an echo "for no reason" (user, 2026-10-09).
  A place's own sound (doors, bells, a crowd, ambience) stays `world`.
- Sounds on the player's own character are never muffled by walls: the camera's line test hit on its side of the
  character and cut every one of them about 30 dB after its first instant (a pistol shot heard as its first frame;
  measured with the capture rig, `localTools/audio/capture`, 2026-10-09).

Not yet: the place's acoustics come from three presets, not from the game's own per-area reverb data
(`SoundProviderPreferences` through AreaTable / WMOAreaTable: needs the client's current area in the DLL).

## Music

- One at a time, stereo, no position nor room, looping without end; a new one cross-fades over the old (1 s), the
  same one already playing goes on. Lua: `EvolutionsAudio_PlayMusic(key [, fadeInMs])`,
  `EvolutionsAudio_StopMusic([fadeOutMs])` (2 s), `EvolutionsAudio_MusicPlaying()` (current key, still heard).
  Server: `EvolutionsAudio::PlayMusic(player, key [, fadeInMs])`, `StopMusic(player [, fadeOutMs])` (`M` / `N`
  whispers). `/eva music <key>`, `/eva stopmusic`.
- Volume: master x the game's music volume, its music switch, all-sound and background settings, every frame.
- The game's own music is silent while ours is heard: the glue holds it - the player's `Sound_MusicVolume` kept in
  the DLL's saved CVar `evaGameMusicVolume`, the game's set to 0, ours played at the held value - and gives it back
  after ours has faded out, on leaving the world, or at the first frame after a crash (both CVars are written to
  Config.wtf together). Moving the music slider meanwhile sets the held value; the game's snaps back to 0.
- Files: `.flac` in the bank (sample-exact; Vorbis or MP3 would pad or drift the seam), read into memory at first
  play and decoded as it plays (a decoded 3 min track would be 60 MB in a 32-bit client). Sources: OGG.
- Loudness: one gain, no limiter (dynamics and the seam untouched), towards -16 dBFS RMS - the game's zone and raid
  music sits at -11 to -19, most near -16 - never past a -1 dBFS peak.
- Loops: the file holds the intro then exactly one loop period; `"loopStart": <frame>` in the manifest is where the
  end goes back to (0: the whole file loops). Composer files often repeat their loop: find the period by
  autocorrelation (coarse) then cross-correlation at full rate (sample-exact, the same lag all through the file), the
  intro's end where a window and the one a period later stop matching, cut `[0, loopStart + period)` with the last
  200 ms cross-faded into what precedes `loopStart` (the seam then is a natural sample step), no fade. Verify: seam
  step against the steps around it, a 3-loop render correlated with the original at the seam. First user: Gardien-chef
  Vorhan (`wardenVorhan.json`: `Music.PrisonIdle` on the board page, `Music.WardenVorhan` the fight).

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
- `time`: day is 6:00-21:00 server time (`GetGameTime`, the glue). `place`: `indoors` plays only with the player
  inside (`IsIndoors()`): an inn's sounds - glasses clinking - heard from the street "don't make sense" (user,
  2026-10-06); never a tavern's recording outdoors.
- Volumes: emitters follow the game's Ambience volume and switch, every other sound its Sound Effects'; all of them
  the master volume, and keep playing in the background when `Sound_EnableSoundWhenGameIsInBG` is on (3.3.5's
  name: `...InBackground` does not exist).
- Positions: from the world database (creatures, gameobjects of the place: `creature`, `gameobject` - their
  `zoneId` is not filled, select by coordinates), heights from creatures standing there. A city's fountains, lamps,
  banners and hourly bells already have the game's own sounds: don't double them.
- Passing sounds only, never a constant bed: looping ambience (crowd murmurs, an inn's room, a forge's fire, ships
  creaking) "feels bad" - the user wants sounds that come and go (2026-10-06). A place's life is a voice or a laugh
  now and then from many points, birds overhead, a hammer at the forge. Sparingly: one voice every 1-4 s on the
  square was "way too much, weird"; about one every 4-9 s (two emitters at 8-18 s) at 0.75, much less at night.
  The engine still plays `loop` emitters - don't use them for ambience without asking.
- First zone: Stormwind (`modules/mod-stat-growth/client-assets/audio/stormwind.json`): the Trade District square,
  the bank, Cathedral Square, the Dwarven District forge, the harbour, the park, Old Town.

## Adding sounds

1. Describe them in `modules/<module>/client-assets/audio/<feature>.json`:
   `{ "sounds": { "<Feature.Name>": { "kind": "world", "files": [ "<wav>" ], "minDistance": 12, "maxDistance": 60,
   "volume": 1, "loudness": -12 } } }` - a key has no tab, `;`, slash nor space; several files play at random.
   Every file is kept in the repository, under the manifest's `sources/<manifest name>/` - the bank is built from
   the repository alone, never from a client or a local folder (user, 2026-10-06). To bring one in, name it as
   `client:<archive path>` (the game client's own, voices in its language - the speech archives are read),
   `asc:<archive path>` (the Ascension client's: retail sounds; some of its rows name files it does not ship - it
   stops on them) or any path on disk, then `python localTools/audio/buildAudio.py --vendor`: it copies each one into
   the sources folder and rewrites the manifest. A plain build refuses a file kept elsewhere. Commit the sources.
   World and loop sounds are written mono (one point in the world); ui and music keep their channels.
   `loudness` (dBFS RMS) defaults to the kind's (ui and world -12, loop -16: the game's own cues sit at -12 to -25);
   a limiter holds the peaks under -1 dBFS. Sounds from another game are often mixed far quieter: leave the default.
2. `python localTools/audio/buildAudio.py`: writes `clientPatcher/addons/EvolutionsAudio` (`Sounds/`, `sounds.txt`,
   the `.toc`), shipped by `Build-FriendPatch.ps1` as `Interface\AddOns\EvolutionsAudio`. Commit it.
3. Server (mod-stat-growth `EvolutionsAudio.h`): `EvolutionsAudio::Play(player, key)`, `PlayOn(player, key, object)`,
   `PlayAt(player, key, position)`, `StopOn(player, object)`, `PlayMusic(player, key)`, `StopMusic(player)` - to one
   player (an addon whisper, prefix `EVA`).
   An object must already be in the player's sight (a creature summoned this tick may not be: play on something
   older, as the ground loot's loops play on the bag rather than its new beam).
4. Client release (`deployWithProgress.ps1 -steps client,publish`, with `dll` when the engine changed).

## Checking what the client really plays

A sound that "cuts" or "echoes": record it, don't guess. The capture rig (`localTools/audio/capture/run.ps1`,
`-local` for the DLL just built and the repository's `sounds.txt`) logs the EVODEV character into the FX lab with
effects only, plays the keys of `steps.lua` (`EvolutionsAudio_Play(key, "G" .. guid)` - what the server's `PlayOn`
does), records the speakers (WASAPI loopback) and `analyse.py <wav>` gives each event's length, level and how bright its
attack is (muffled: almost nothing above 2 kHz). Compare with the same key played as `ui`.

## Tuning in game

- `/eva list`, `/eva play <key> [target]` (on you or your target), `/eva reload` (the bank read again).
- Volumes live: edit the installed `Interface\AddOns\EvolutionsAudio\sounds.txt` (tab-separated: key, kind, volume,
  min, max, files), `/eva reload`, listen; then carry the values into the JSON (`volume`, distances) and rebuild. The
  launcher puts the published file back at its next sync.

## Pieces

- DLL: `EvolutionsAudio.cpp` (engine, bank, the ambience's emitters, the music; Lua API `EvolutionsAudio_Play`,
  `StopOn`, `SetEnvironment`, `SetZone`, `SetDaytime`, `Reload`, `Keys`, `PlayMusic`, `StopMusic`, `MusicPlaying`),
  `MiniAudio.cpp` (miniaudio's implementation).
  Build: `localTools/buildClientDll.ps1` (or the deploy tool's `dll`).
- Client glue: `clientPatcher/interface/Interface/FrameXML/EvolutionsAudio.lua` (the server's `EVA` whispers, the
  place, the zone and the time of day every 0.25 s, the game's music held every frame, `/eva`).
- Bank builder: `localTools/audio/buildAudio.py` (`--vendor`; `clientFiles.js` reads the game client's
  archives). Sources: `modules/*/client-assets/audio/sources/`.
- Server helper: `modules/mod-stat-growth/src/EvolutionsAudio.*`.
- First user: the ground loot (`ground-loot.md`).
