# Pestifere tank HUD assets

14 paintings generated with built-in imagegen, matching the approved Gladiateur v2 painted style.
Both Faucheur references and the Gladiateur atlas were supplied to generation calls.
Final prompts are recorded in `generationManifest.json`; selected paintings are retained in `source/`.
Brief: `.agents/plans/pestifere-hud/pestifere-hud.ASSETS.md`. This package is for the tank tree only.

- `png/`: all 14 component exports, at the exact sizes requested in the brief.
- `pestifereHudAtlas.png`: all components in a 1024 x 512 power-of-two atlas, with four-pixel cell gutters.
- `../../interface/Interface/ClassHud/pestifereHudAtlas.blp`: matching raw BGRA BLP2, 8-bit alpha, full mip chain.
- `assetManifest.json`: atlas boxes, component placements, liquid bounds, plague order and boil stack mapping.
- `statePreview.png`: six states enlarged and at native 162 x 70 size.
- `preview0.png` through `preview5.png`: individual native-size composite states.
- `boilPreview.png`: flat, small, swollen and ripe boil stages.
- `flaskFillMask.png`, `boilFaceMask.png`: protected transfer regions for the liquid and pustule states.

Reproduce with `python localTools/interface/buildPestifereHudArt.py` from the repository root.
Dependencies: Pillow and NumPy; shared registration, preview and BLP helpers live in `localTools/interface/`.
This packages assets only, without building the server, patching the client or changing runtime HUD code.

The flask paintings are edits of the same empty flask. Assembly registers liquid paintings to its proportions,
then transfers liquid only into its connected dark hollow. Alpha, glass rim, neck collar and cork are identical
across all four states. The bone ring and alpha are likewise preserved across the four boil stages.
Scripted registration and masks follow the user's existing authorization from the Gladiateur asset work.

Frame and bile drips share the same 512 x 224 canvas. Both are exactly mirrored; drips are constrained to
the cleaver heads and the area below their cutting edges. Bubbles retain the empty flask's registration.
All light effects have black backgrounds. Violet smoke stays dark; bile stays yellow-green.

Use atlas boxes divided by 1024 horizontally and 512 vertically with `SetTexCoord`.
Frame: 154 x 63, offset (0, -8). Flasks: 40 x 37, offsets (-40, 2), (-2, 2), (36, 2).
The left flask draws above the middle, and the middle above the right.
Boil: 12 x 12, offset (0, -17). Offsets are UI coordinates, positive y upward; atlas pixel y goes downward.

Liquid duration lowers the visible liquid surface while keeping the original glass:
`surfaceY = fillBottom - (fillBottom - fillTop) * durationFraction`.
`flaskFillBounds` is `[left, right, top, bottom]` inside the 132 x 120 flask canvas.
Clip the full state below this surface over the empty state; crop geometry and texture coordinates together.
The preview exporter demonstrates this with 100%, 65% and 35% liquid levels.

Plague order is bone / flesh / bile. Pourriture stacks select flat (0), small (1-2), swollen (3-5), ripe (6).
`boiling`, `glowToxic`, `glowSepulcre`, `bileDrips` and `splash` use `ADD`; other pieces use normal alpha blending.
Avatar's overlay belongs only on carried plagues; glow, flicker, throbbing and splash motion are animated in code.
The static effects here are registered layers, with no independently generated animation frames.

Delivery checks cover exact PNG dimensions, shared socket alpha/rims, mirror symmetry, partial liquid clipping,
black effect edges, atlas pixel copies and a pixel-exact BLP first mip with valid complete mip ranges.
In-game validation and runtime integration remain separate work.
