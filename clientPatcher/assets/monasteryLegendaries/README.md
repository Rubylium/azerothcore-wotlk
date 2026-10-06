# Scarlet Cathedral legendary assets 2 and 3

18 paintings generated with built-in imagegen for `monastery-legendaries-2-3.ASSETS.md`.
Selected source paintings, prompts and final exact-size PNG exports live in the two requested folders:

- `../sermentWhitemane/`: marble, pale gold, halo, pearl and resurrection wings.
- `../consecrationMograine/`: dark steel, crimson enamel, gold rivets, hammers and consecration runes.

Each folder contains `source/`, nine exports in `png/`, `generationManifest.json`, `assetManifest.json`,
`preview.png` and `contactSheet.png`. Whitemane's last corner correction is recorded in `cornerEdit.json`.
`preview.png` here compares both composed sets, with 50 px icons inside 65 px frames (the requested 130% scale).
Preview text illustrates title contrast and is not embedded into any asset.

All four approved HUD references were attached for generation. The approved Inquisiteur item frame and corner
were combined into `reference/inquisiteurPieces.png` to fit the generator's five-reference limit.
The two item/power icon pairs instead used stock WotLK Holy Smite and Searing Light references from the prior set.

Rebuild from the repository root: `python localTools/interface/buildMonasteryLegendaryArt.py`.
Requires Pillow and NumPy. Painting registration and assembly live in `monasteryLegendaryAssembly.py`;
export and composed previews live in `buildMonasteryLegendaryArt.py`, both under `localTools/interface/`.
They reuse the existing Inquisiteur symmetry, seamless-bar and glow helpers.

UI pieces are straight RGBA; light-only glows are RGB on pure black for `ADD`; icons are opaque RGB squares.
Frames and glows share one 464 x 464 resize inside a 512 x 512 canvas, at offset (24, 24), preserving registration
and giving halos and corner ornaments breathing room. Frames and glows are bilaterally symmetric.
The icon opening remains about 60% of the canvas width, with local crest intrusions.

Both bars occupy 48 px canvases. Whitemane's painted bar thickness is 28 px; Mograine's is 36 px.
The exported vertical bar is the horizontal painting rotated 90 degrees, retaining left-side lighting.
Separately generated vertical paintings remain in `source/`; reusing one profile ensures identical material joins.
Horizontal and vertical tiles have exactly identical repeated endpoints. Corner bar centres are (48, 48);
both crest bars have centre y=96. Their outgoing edge pixels match the bar endpoints exactly.
Render every tooltip border piece at the same scale. Mirror the top-left corner for the other three corners.
The bottom crest is deliberately smaller than the top crest. Neither set includes a title plate.

Scripted symmetry, registration, masks and seam assembly follow the user's prior precise-assembly authorization.
Checks cover all 18 exact dimensions, clean transparent interiors, black glow edges, symmetry, matching outgoing
corner/crest profiles and seamless repeated endpoints. Contact sheets and the 130% previews were inspected.
This exports the requested PNG assets only; no server build, runtime wiring or client patch is performed.
