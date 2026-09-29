# Cinematics (scripted camera flights)

In-world camera flights with a title card, like a raid introducing its boss: the camera leaves the player, flies a
path of keyframes at the frame rate, a title fades in over the last shot, and the camera blends back. First use:
L'Infini's intro (2026-09-29), played when the Défi group arrives in the Celestial Planetarium.

## Pieces

| Piece | Where | Role |
|---|---|---|
| Camera engine | `awesome_wotlk/src/AwesomeWotlkLib/CameraPath.cpp` (client extension DLL) | flies the camera; Lua API below |
| Interface | `clientPatcher/interface/Interface/FrameXML/Cinematics.lua` | registry, black bars, title card, UI hidden, Escape to skip, `/cinematic` |
| Triggers | wherever the moment is known client side (e.g. `ChallengeBoard.lua`, the board's arrival event) | `Cinematics.Play(name)` |

No server change is needed to play one: trigger it from a client event you already get (an addon message, a
`CHAT_MSG_ADDON`, a board event). Nothing is sent to the server; every player plays it on their own client.

## Lua API

Engine (client extension):

- `CameraPath_Play(keys [, options])` → true if started. `keys`: `{ { t, x, y, z, lx, ly, lz, fov, ease }, ... }`
  - `t`: seconds from the start (sorted for you); `x, y, z`: the camera; `lx, ly, lz`: the point it looks at (world
    coordinates of the map it plays on);
  - `fov` (degrees, optional): blended between keys; unset keeps the player's;
  - `options`: `{ blendIn = 0.6, blendOut = 0.8 }` seconds, from the player's camera onto the path and back, and
    `keyTiming = true` (below).
  - **Movement (default, the glide)**: the camera moves along the whole curve by distance, one sine ease over the
    flight - a gentle start and stop, a steady pace between, no change of pace at the keys. Only the last key's `t`
    counts (the flight's length); the keys shape the curve. This is what reads as a real camera move.
  - `keyTiming = true`: each key reached at its own `t` (and a key's `ease = true` eases its segment). The pace then
    changes at every key: it felt stop-and-go on L'Infini's preview. Only for a shot that must land at a set second.
- `CameraPath_Stop()`: leaves the path now, blending back (blendOut).
- `CameraPath_IsPlaying()`.
- Event `CAMERA_PATH_FINISHED`: the camera is the player's again (the end, a stop, or leaving the world).

Interface (`Cinematics.lua`), on top of that:

- `Cinematics.Register(name, definition)`: `keys`, `blendIn`, `blendOut` as above, and
  - `title`, `subtitle`, `titleAt` (seconds from the start), `titleFor` (how long; until the end if nil);
  - `letterbox = false` (no black bars), `keepUI = true` (leave the interface up), `unskippable = true`
    (Escape does nothing), `onFinish` (called at the end, skipped or not).
- `Cinematics.Play(name)` → false when the definition is unknown, one is already playing, or the client extension
  has no camera flights (an old DLL: the trigger then simply does nothing).
- `Cinematics.Stop()`, `Cinematics.IsPlaying()`, `/cinematic <name>` to try one.

While one plays, every key but Escape is swallowed (nothing moves the character under the camera).

## Adding a cinematic (checklist)

1. Get the coordinates: the subject's position from the server (its spawn in SQL, or `.gps` next to it), its height
   from a first screenshot run. Put the look-at points on its face, not its feet.
2. Write the definition with `Cinematics.Register` (the shared ones live at the end of `Cinematics.lua`; a system's
   own may sit in its file). 3 to 5 keys over 6 to 10 s read well. Think of the keys as the shape of one continuous
   move (a crane down, an orbit, a push in), not as stops: sharp changes of direction between keys still read as a
   jerk even at a steady pace.
3. **Indoor areas (WMO): keep every camera key inside the room.** An indoor room is only drawn when the camera is in
   it: a key outside the walls turns the screen black. Curves overshoot a little between keys; keep a margin from
   walls and ceilings.
4. Trigger it from the moment's client event. Keep it shorter than whatever follows (L'Infini's is 8.5 s + 1.2 s
   blend, under the 12 s pull countdown that starts after the arrival).
5. Check it with the screenshot rig (below), then ship it with the client patch.

## Testing without rebuilding the patch

The screenshot rig of `.agents/plans/infinite-boss/boardshot/` (gitignored; memory `glue-screenshot-loop`):

- `shoot.ps1 -addonName <Probe>` launches the test client on EVODEV / Evooath, muted, runs a dev addon, collects
  the screenshots and quits;
- a probe addon can carry a copy of `Cinematics.lua` (`CinematicsDev.lua`, listed first in its toc): it replaces the
  shipped one for the run, so a path or layout tweak costs one run, not a 4-minute patch build;
- `IntroProbe` takes L'Infini up at Défi I and films its intro; `CineProbe` plays test cinematics in Stormwind;
- a new DLL build only needs copying into the test client (`CleanWOTLK\AwesomeWotlkLib.dll`); the release ships it.

## How the engine works (for changing it)

- The world frame (`0x4FA5F0`) updates its camera every frame with `0x607B00` (cdecl `int (int, Camera*)`) and right
  after builds the view from the camera's position (`+0x08`) and orientation (`+0x14`: three rows of a world matrix,
  forward / left / up; left = cross(world up, forward)); field of view at `+0x40`.
- `CameraPath.cpp` detours `0x607B00`: the game's camera is computed and read (the blends go from and to it), then
  the path's written over it.
- Too late does nothing: writing the camera from a Direct3D `BeginScene` callback changed nothing on screen, the view
  being built before.
- Positions go through a centripetal Catmull-Rom curve (no loop nor overshoot with uneven key spacing), measured once
  (64 samples a segment) so the glide can move by distance; look-at points go through a plain Catmull-Rom curve at
  the same progress (they often all but coincide). Blends are smoothstepped.

## Pitfalls met

- A frame laid over `WorldFrame` measures in other units (38 400 high): bars and titles sized from it covered the
  screen or went off it. `Cinematics.lua` lies over `UIParent` (parentless, so it stays up when UIParent is hidden).
- The client caps a font's size (asked 54, drew about 24 px): big titles are drawn in a scaled child frame.
- Heavy `OUTLINE` muddies Morpheus once scaled; a shadow reads better.
- A probe file written in another encoding than UTF-8 loses its accents in game (é, è dropped).
