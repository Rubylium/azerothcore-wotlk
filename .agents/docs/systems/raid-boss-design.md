# Raid boss design (the Défi board's own bosses)

How to build a boss fight the user accepts, learned on Gardien-chef Vorhan (`WardenVorhan.cpp`), the Hollow Voice
(`HollowVoice.cpp`) and Vrogar (`ForgeMaster.cpp`, plan `.agents/plans/escape-hunter`: three reworks before it was
right). The base class is `Defi::BossAI` (`DefiBoss.h`); the floor's shapes, the bots' dodging and the impacts are
`GroundIndicators` (`GroundIndicators.cpp`). Sizing a boss's health and damage: `power-scaling.md`.

## Process

1. **Talk first.** Propose the design as a table in the plan doc (patterns, timeline on the music, warnings, what a
   mistake costs) and get the user's agreement before any code. A redesign starts the same way, never with code.
2. Build the skeleton with the existing visuals (red shapes, stock icons): art comes once the fight is right.
3. Check every pattern in the FX lab (`fx-lab.md`): the boss's rehearsal (`.<boss> lab`, `.<boss> cast <pattern>`)
   draws it for real, harms nobody and logs the share of the arena left safe; screenshots with `shoot.ps1`.
4. One bot run (`e2e/local`) to see the bots survive it - minutes, not hours. Then the user tests in game.
5. Commit each finished step; deploy after every change (check no real player is online **first, alone**).

## The fight

- **A flow, not a pile.** One pattern on the floor at a time, a steady cycle, rests between puzzles. Two layers in
  turns: the WoW layer (dodges while he is tanked) and the FFXIV layer (placement puzzles: a wipe the first time,
  easy once learned - the safe spot is learned, never explained).
- **The boss is locked in the middle**, rooted all fight (`HoldMiddle`); never anchor a pattern on a boss the tanks
  drag around. His only moves are scripted ones that are themselves a tell (Vrogar's walk to the trough).
- **Warnings:** a shape's first red at least 3.1 s (8 beats at 155 BPM) before it strikes; a follow-up of a pattern
  already read may come at 2.3 s; a wave carrying a mechanic (spread, soak) 4.6 s. Successive rings strike at least
  1.5 s apart. Freeze-then-strike (the chains): about 2 s.
- **Never overlap** two patterns' red (a cone and three donuts at once "looks very bad"): split them into two
  patterns. A pattern never starts before the last one's last strike.
- **Never flash** information (a shape blinked to be remembered): tells stay visible while they matter. The one
  exception asked for: a hidden spread shown only its last 0.5 s, as a surprise twist (soak, then spread).
- **No text** during the fight (no emotes or whispers explaining it): visual tells only.
- **Puzzles say who goes where**: marks over heads (the game's raid icons until custom art; the same icon on the
  floor spot), never "bots know, players guess". As many spots as players standing: nothing waits for the dead.
- **Size the geometry** so it is exactly right: a spread's safe area holds exactly its spots (Vrogar: half the arena
  holds 5 spots of 10.4 yd, a sixth cannot fit - computed by packing, not guessed).
- **An intermission gives everyone something** to do (spreads, soaks on healers splitting the group), with its
  mechanic waves given more warning than the plain ones.
- Arena: walled and lethal (`ShowCurtainRing`), the wall rising a few seconds after the pull, players still outside
  drawn in, the wall killing 1.5 s after (a teleport needs its client's answer). The first pattern starts after
  the draw-in. Props are creatures with stock models (displays added in `patchSinisterStrike.ps1`): an object blocks
  players.

## Damage and health

- A dodge missed costs a share of the victim's **own** health (75%), with a debuff making a second mistake lethal
  (`Ébouillanté`); a puzzle missed kills (`Doom`). Mistakes matter at any gear.
- Soaks share a total between those in them (a missing player: the others take more - they plan defensives); a debuff
  doubling the next share makes two soaks at once lethal.
- **Paced health** (`StartPace`, the Hollow Voice's first phase): the bar comes down with the music, 1% only at the
  check; over-dps ends the fight 30 s early at most; the boss never sits at 1%. An intermission is outside the pace.
- The fight lasts its music; every step lands on a beat (`Defi::BeatGrid`, `AtBeat`), big hits on the track's.

## What players see

- Shapes: `GroundIndicators` (drawn instantly for a boss: `DrawInstantly`). **A shape that moves is one drawing from
  its tell to its strike**, never a still one swapped for a moving one (it fades out and in): a frozen sweep keeps its
  drawing (`ShowSweepingRectangle` stopAfterMs); one that holds, turns, turns back: `ShowTurningRectangle` /
  `ShowTurningCone` (a plan of `Turn`s, the bots told each part as it comes); a cone that follows then freezes:
  `ShowTrackingCone` followMs. All of them are turned by the client (the DLL's `SmoothTurns`, its plan whispered once
  as the shape appears: `GroundIndicators.cpp` `TellTurns`), never by facing packets (they came unevenly: it
  jittered); the server keeps its own facing quietly and sends it once where the plan ends. A new kind of moving shape
  sends its plan the same way.
- Carried looks (circles round players, marks over them) are drawn at their own size on every race and creature
  (their effect rows pinned to scale 1).
- Painted floor pictures (shapes.json `texture` and `image`), three things the client does (seen in the FX lab on
  Valérian's phase 2):
  - **It draws them back to front by their carrier's distance to the camera**, never in the order they were put on:
    a small picture over a big one (a numeral on its part, a pool of light on a veil) is hidden whenever its carrier
    lies farther than the big one's (past the boss). Paint the detail into the big picture
    (`valerianArt.py` `with_numeral`), or cut a hole in it the small one shows through (the Éclipse's veil).
  - **It mirrors them**: a picture whose handedness matters (a numeral, a clock) is mirrored when built
    (`mirrored_painting`, `with_numeral`); a symmetric one needs nothing.
  - **A decal (`ShowDecal`, kind `image`) lies a quarter turn from its carrier's facing**: the picture's +x along
    facing + pi / 2. Turn the carrier by - pi / 2 for a picture laid out along its own x (the veil's holes), or paint
    it symmetric about an axis that does not matter.
- **Every strike has a 3D impact** where it lands: retail effects converted for 3.3.5 (Ascension's client,
  `localTools/ascensionImport/importVisuals.py`, one import folder per boss: `client-assets/imported<Boss>`), in the
  fight's colours (tinted), compared in the lab before keeping. **An impact never leaves its shape**: a cone, a
  line or a ring gets its effect reshaped to it (`localTools/shapedEffects`: the source model's emitters thrown over
  the arc, spread over the strip or born on the ring; one kit per form in `shapedEffects.json`, played from the
  form's origin turned a quarter left of its axis, `ForgeMaster` `ShapedImpact`/`ShapedCone`/`ShapedStrip`). A
  180 degree cone does not reshape well (a streak along its edges): wide cones are 90/45 degree pieces side by side;
  a long wide strip is tiles flush with its ends. Small circles keep a stock-shaped burst sized from its footprint
  measured in the lab against the 5 yd rings.
- **The reference look: Valérian's phase 2** (`ZealotBoss.cpp`, `localTools/valerian/valerianArt.py`, its brief
  `.agents/plans/gaol-third/gaol-third.ASSETS-p2.md`; the user called it a masterpiece). Every boss is drawn that
  way:
  - **No red left**: every shape a painted look (`shapes.json` `VL_*`). One brief per phase, in that file's format:
    the boss's own colour language (here white-gold divine light, its counter colour told apart by value and texture,
    not hue: violet-black shadow), a style prefix pasted in every prompt, each file's exact size and framing.
  - **Fills, not paintings, for floor areas**: seamless squares the build cuts into each shape (`divine`,
    `starfall`, `choir`). Arena cut-outs (halves, quadrants, slices, wedges, thirds) keep a clean rim; cones and lines
    bleed at their sides (ragged footprint, faded strip edges).
  - **Two layers**: the base and its glow alone (`blend` add), the glow breathing faster as the strike comes
    (`pulseRamp` over the tell). A line strikes as a flare of its own painting (`VL_LanceFlare`, `VL_TendrilFlare`:
    `alphaKeys`) - a stock kit ignored its facing.
  - **Information painted in**: numerals inside their parts (never a decal of their own), a veil with its holes cut
    out, a beacon where a star counts; marks over heads (billboards) for who carries what, a flipbook for a change
    (the seal burning into the other).
  - **Moving shapes one drawing**, turned by the client (see above); the turn's plan shared with the bots.
  - **Checked in the FX lab** (the `wow-dev-client` MCP: a top-down `camera`, `screenshot_burst` timed on the
    pattern), each pattern and its strike, before the user sees it.
  - **Never a boss's look on players' things**: a legendary or a trinket wearing a mechanic's visual teaches the group
    to ignore (or fear) it.
- Art scope: a boss's art is what players see in the fight (shapes, marks, walls, impacts); ability icons stay
  stock; never item art unless asked. A board page or gate for the boss on the Défi board (`ChallengeBoard.lua`).

## What players hear (`evolutions-audio.md`)

- A warning as a pattern starts, one impact as it strikes; nothing doubled (several sources: each player hears the
  nearest, once). Impacts tied to a spot fade with distance (6-45 yd); arena-wide ones are heard everywhere.
- Levelled on their loudest 0.4 s (`loudnessWindowMs`) at -9 dBFS RMS: whole-file levelling made impacts with quiet
  tails far too loud, -16 too quiet.
- Let the user pick: an audition page (each mechanic, its current sound and candidates) and they choose. Diablo IV's
  sounds: the CASC pipeline in `data/custom/diabloLootSounds/scripts` (event names in CoreTOC group 71).

## The bots must play it

Bots read the same areas the players see (`FindEscape`). What they need:
- A rank 3 creature is a boss: its tank dodges its circles (the trash rule held it in place).
- No tank spot inside red (they walk back in); the off-tank behind the boss, never in a cone on the tank.
- Areas struck in turn (rings): out of what strikes first, into what strikes last, never through what strikes on
  the way; a chosen way out kept while it stays one; no charge or leap into red.
- Puzzles assign spots (`SetUnitSpot`): bots go there first, ignoring the circles others carry. A hidden tell is
  told to the bots from the moment players can read it (the dip, not the strike).
- A creature's guid is its map's: anything keyed by an owner's guid must also match its instance.
- Check with the log, not by eye: every miss and a beat before every strike, where each bot stood, what it did, and
  whether it saw a way out (`Probe`).
