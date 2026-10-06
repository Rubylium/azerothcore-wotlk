# Marque de l'Inquisiteur assets

Ten PNG assets matching `.agents/plans/legendary-items/marque-inquisiteur.ASSETS.md`.
Generated with built-in imagegen, using both Faucheur references and the approved Gladiateur/Pestifere atlases.
The icons used stock WotLK Holy Smite and Searing Light references decoded from existing local BLPs.
All prompts and source paths are recorded in `generationManifest.json`; selected paintings are in `source/`.
The cloak's final dark-vignette edit is documented in `iconEdit.json`.

- `png/`: all ten assets at the exact requested sizes.
- `preview.png`: assembled tooltip and item frame at 256, 100 and 50 pixels; preview text is illustrative only.
- `contactSheet.png`: all ten exports, including the light-only glow.
- `assetManifest.json`: sizes, modes and border registration metadata.

Rebuild from the repository root: `python localTools/interface/buildMarqueInquisiteurArt.py`.
Requires Pillow and NumPy. Assembly and export are separate modules under `localTools/interface/`.

UI pieces use straight RGBA, with genuinely transparent interiors. The glow is RGB on pure black for `ADD`.
The two icons are opaque RGB squares. Only the requested PNG package is exported; runtime wiring remains separate.

Frame and glow share their original canvas transform. The glow is constrained to the frame's exterior silhouette.
The empty opening spans about 64% of the width; the top/bottom ornaments intrude locally.
All horizontally symmetric UI pieces are mirrored exactly. The top-left corner is intentionally directional:
mirror it horizontally/vertically for the other corners.

Horizontal bar: 256 x 48, painted thickness 40, centred at y=24. Vertical bar: its 90-degree rotation, lit left.
The separately generated vertical painting is retained as a source; export reuses the horizontal painting so
the materials and cross-section match exactly. Both repeated endpoints match pixel for pixel.
Corner bars have centres x=48 and y=48, matching the tiled bar's cross-section at both outgoing edges.
Both crest bars have centre y=96. Render corners, crests and bars at the same pixel scale to preserve thickness.
The crests' outgoing ends share the bar's exact endpoint pixels. The title plate fades on every edge and has
maximum opacity 22%, keeping orange/white title text readable.

Scripted registration, symmetry, blending and masks follow the user's existing precise-assembly authorization.
Checks cover ten exact dimensions, symmetry, clear central opening, black glow edges, seamless repeated bars,
and matching corner/crest endpoints. The composed preview was visually inspected; no server/client build was run.
