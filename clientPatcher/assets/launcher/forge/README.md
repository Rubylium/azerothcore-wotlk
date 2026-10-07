# La Forge launcher assets

16 painted assets matching `.agents/plans/launcher-forge/launcher-forge.ASSETS.md`.
Generated with built-in imagegen: cinematic forge paintings for the scene, approved class HUD references for UI.
Prompts and generated source filenames are in `generationManifest.json`; all selected paintings are in `source/`.

- `png/`: all 16 assets at the exact requested dimensions.
- `previewReady.png`, `previewUpdating.png`, `previewClosed.png`: 1280 x 760 composed state previews.
- `statePreview.png`: the three states together; preview text/layout are illustrative, not embedded in assets.
- `sceneHot.png`, `sceneCold.png`: scene-only composites at launcher size.
- `contactSheet.png`: all 16 exports.
- `sparkFrames/`: 16 individual 512 x 512 frames, in row-major atlas order.
- `sparkPreview.gif`: animation preview, 50 ms/frame plus a black pause.
- `assetManifest.json`: canvases, blend modes, layer order, transforms and particle cell boxes.

Rebuild from the repository root: `python localTools/interface/buildLauncherForgeArt.py`.
Requires Pillow and NumPy. Assembly, validation and export are separate files under `localTools/interface/`.
This exports the asset package; no launcher code, server build or client patch is changed.

The full scene layers share a 2560 x 1520 canvas. Composite back wall, anvil, additive fire light, smoke and foreground
in that order. Hot/cold anvil exports use one crop, scale, placement and identical alpha from the hot master.
The anvil's bounding box lies in the right third above the bottom workbench zone.
The cold wall transfers the generated cold edit's broad lighting onto the hot master's original detail and geometry.
The same operation on the anvil and ingot avoids texture/geometry drift during heat-state cross-fades.
All cold exports have blue-grey ordered channels (R <= G <= B), with no orange pixels.

The left 45% and bottom 26% are dark and quiet. The anvil does not enter either zone; fire light is masked out of both.
Foreground framing props are dark silhouettes, with colours capped at 60/255 to keep their rim light restrained.
The source fire-light painting is blurred to remove accidental floor detail, then combined with registered blade glow.
Fire light is drawn additively and disabled cold. Smoke is 2560 x 760 and tiles horizontally with matching endpoints.
Position it over the upper/right scene and animate its offset/opacity in code, keeping text/workbench zones clear.

The ingot exports share the hot master's alpha and are bilaterally symmetric. Render the cold ingot beneath the hot
one, clipping the hot canvas from left to right for update progress. A 0% clip shows cold; 100% shows fully molten.
Ingot glow is 1600 x 640, with the 1200 x 400 ingot positioned at (200, 120) inside it: draw glow at (-200, -120)
relative to the button canvas. Its support is the exact exported ingot silhouette expanded into a soft outside halo.

Light-only fire, smoke, ingot glow, sparks and embers are RGB on pure black, for additive blending.
Solid pieces are straight RGBA with zero RGB under fully transparent pixels. Walls are opaque RGB.
The spark atlas is 2048 x 2048 with sixteen 512 x 512 cells; the ember atlas is 1024 x 1024 with sixteen 256 x 256
cells.
Both use row-major order and black cell gutters. Spark flash cores are registered at (256, 256) through the impact.
The late flying/fading sparks keep the last impact registration. The GIF is a preview, not a runtime dependency.
The emblem is exactly symmetric and remains readable at small icon sizes. The iron plate has four corner rivets.
The header's repeated endpoints match exactly; its plain middle and the plate's plain centre support sliced UI.

Scripted registration, masks, symmetry and exact assembly follow the user's existing authorization.
Validation checks all dimensions, shared hot/cold alpha, cold colour ordering, symmetry, dark text/workbench zones,
black additive edges and particle gutters, tile endpoints, atlas cell copies, impact registration and burst fading.
The ready/cold composites and contact sheet were visually inspected.
