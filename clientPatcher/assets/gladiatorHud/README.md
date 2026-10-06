# Gladiateur HUD assets

14 textures, generated with the built-in imagegen tool from the asset brief in
`.agents/plans/gladiator-hud/gladiator-hud.ASSETS.md`. Final prompts are in `generationManifest.json`.
The user authorized scripted registration, radial masks and animation assembly on the painted artwork.

- `source/`: original selected paintings, with genuine alpha for solid pieces and black for additive effects.
- `png/`: exact power-of-two exports; `bloodFrames/` contains the 16 registered blood states.
- `../../interface/Interface/ClassHud/gladiator*.blp`: client textures.
- `assetManifest.json`: placement coordinates, atlas layout, state percentages and blend modes.
- `statePreview.png`: three composite states at enlarged and 130 px canvas sizes.
- `emberLoop.gif`: seamless 64-frame animation preview at 20 fps.

Reproduce with `python localTools/interface/buildGladiatorHudArt.py` from the repository root
(Pillow and NumPy required). This packages assets only; no client patch or server build is invoked.

The registered shield canvas is 512 x 512, centre (256, 256), outer diameter 380 px. The blood occupies
the recessed groove at radius 162 px, width 8 px. Fill frames advance clockwise from six o'clock in
6.666667% steps, frame 0 completely empty, frame 15 full. The 2048 x 2048 atlas is 4 x 4 cells of 512.
Use `floor(clamp(percent, 0, 100) * 15 / 100)` for a conservative fill frame.

Boss textures are 128 x 128; medallions 128 x 128, drawn at 56 x 56 in shield-canvas coordinates;
Duel textures are 128 x 64. Their placements are recorded in `assetManifest.json`.
Glows, laurel, embers and shockwave use `ADD`; other layers use normal alpha blending.
Rim glow and laurel share the frame's registration. Shockwave is a 256 x 256 ring for scaling/fading from the boss.

Small textures are raw BGRA BLP2 with 8-bit alpha and complete mip chains. The two 2048 x 2048 atlases
use the existing repository DXT3 encoder and complete mip chains (4-bit compressed alpha).
Transparent PNGs retain straight alpha. Ember animation reuses extracted painted sprites and periodic
rise/drift trajectories with zero intensity at respawn, avoiding independent-frame jitter and loop popping.

PNG dimensions, blood progression/direction, additive black corners, BLP headers/mip ranges and
the ember loop boundary are checked during delivery. Composite registration is visually inspected.
Runtime HUD wiring and in-game validation remain separate work.
