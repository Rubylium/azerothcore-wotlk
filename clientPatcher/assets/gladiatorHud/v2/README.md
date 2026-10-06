# Gladiateur HUD assets v2

12 new paintings generated with the built-in imagegen tool using both Faucheur references:
`../reference/reaperHud.png` and `../reference/reaperAtlas.png`.
Final prompts and source filenames are in `generationManifest.json`.
This round implements the revised `.agents/plans/gladiator-hud/gladiator-hud.ASSETS.md` brief.

- `source/`: the original painted frame, empty helmet and its four edits, glow pair, energy and jewel states.
- `png/`: 12 exact-size exports matching the brief (non-power-of-two component canvases are intentional).
- `gladiatorHudV2Atlas.png`: all components packed into one 1024 x 512 power-of-two atlas.
- `../../../interface/Interface/ClassHud/gladiatorHudV2Atlas.blp`: matching raw BGRA BLP2, 8-bit alpha, full mip chain.
- `assetManifest.json`: pixel boxes, blend modes, HUD sizes, offsets and socket stacking order.
- `statePreview.png`: six HUD states, each enlarged and at the actual 162 x 70 display size.
- `preview0.png` through `preview5.png`: individual 162 x 70 composite states.
- `idleVsEmpowered.png`: idle and gold states enlarged four times.
- `helmetFillMask.png`: the protected hollow used for registration and fill validation.

Reproduce with `python localTools/interface/buildGladiatorHudV2Art.py` from the repository root.
Dependencies: Pillow and NumPy. This exports assets without building or patching the client.

Helmet states are edits of the empty painting. The exporter preserves its exact alpha silhouette and bronze
rim pixels across all five states, transferring generated liquid light only inside the original dark hollow.
The one-third and two-thirds states are clipped at exact thirds of that hollow's horizontal extent.
The left helmet is drawn above the middle, and the middle above the right.

The frame and its separately generated edge-energy edit use the same 512 x 224 registration.
Their left halves are mirrored for exact bilateral symmetry. Energy is constrained to the original blade
edges, excluding the boss and grips. Glows have pure black edges and are blurred into diffuse light.
These assembly adjustments use the user's existing authorization for scripted masks and registration.

Use the atlas pixel boxes divided by 1024 horizontally and 512 vertically with `SetTexCoord`.
Frame: 154 x 63, offset (0, -8). Helmets: 50 x 35, offsets (-40, 2), (-2, 2), (36, 2).
Jewel: 10 x 10, offset (0, -17). Glow: 146 x 60, offset (0, 2).
Offsets are UI coordinates (positive y goes upward); atlas pixel y goes downward.
`glowCrimson`, `glowGold` and `bladeEnergy` use `ADD`; all other pieces use normal alpha blending.
Jewel crack overlays the gold jewel. Shield Slam reuses `Interface\\ClassHud\\gladiatorShockwave`.

The v1 assets and runtime HUD are preserved. Runtime integration and in-game validation are separate.
