# Item Forge PNG delivery

Revised 15-asset brief: the window shell, Forge button and close button use the stock UI.

Final files: `png/`. Original imagegen paintings and final prompts: `source/`.
Review sheet: `preview/contactSheet.png`. Checks and SHA-256 hashes: `validation.json`.

| PNG | Canvas | In-game size | Mode |
|---|---|---|---|
| stageCold, stageLit | 868 x 480 | 434 x 240 | RGB opaque |
| fireLight | 868 x 480 | 434 x 240 | RGB additive |
| medallion | 120 x 120 | 60 x 60 | RGBA |
| rowPlate, rowSelected | 536 x 84 | 268 x 42 | RGBA |
| gaugeFill | 464 x 20 | 232 x 10 | RGB opaque |
| rankCold, rankLit, rankGold | 48 x 48 | 24 x 24 | RGBA |
| rankFlare | 96 x 96 | 48 x 48 | RGB additive |
| pieceHeat | 116 x 116 | 58 x 58 | RGB additive |
| steam | 1024 x 512 | 128 x 128 per frame | RGB additive |
| masterpieceBanner | 960 x 220 | 480 x 110 | RGBA |
| goldenBurst | 440 x 440 | 220 x 220 | RGB additive |

Every final canvas is exact. Pack these deliveries unchanged; do not resize or stretch pieces in the atlas.
RGBA files use straight alpha with zero RGB under fully transparent pixels.
Additive files use pure-black backgrounds and black canvas/cell boundaries.
Steam contains eight 256 x 256 cells, four columns, two rows, row-major, played once.

Generated native canvases are retained in `source/`. Assembly registers their paintings to the delivery slots.
Solid pieces retain their proportions. The cold stage is the geometry master; the lit edit transfers illumination
outside the furnace. Both use the same stage registration. The empty icon area is deliberately quiet and dark.
The banner socket is registered to the guide, with its painted trim outside the full dark text rectangle.
Row and ember states share the master's alpha exactly.

Guide coordinates, in delivery pixels:

- Stage name strip: `(0, 0, 868, 68)`.
- Furnace mouth: `(240, 68, 388, 184)`.
- Empty item region: `(376, 182, 116, 116)`; anvil face at `y=300..320`.
- Hammer region: `(580, 192, 240, 180)`; hammer supplied by code.
- Banner icon: `(52, 46, 128, 128)`, centre `(116, 110)`.
- Banner text: `(224, 28, 700, 164)`.

The original hammer, sparks and embers remain in `reference/launcherReuse/` at their original sizes.
The revised forging hammer is `png/hammer.png` (RGBA, 1448 x 1086); its generation prompt is
`source/hammerGeneration.json`. The texture packer scales it uniformly to 228 x 171 and registers the
handle pivot at `(0.88, 0.80)` before writing `ForgeHammer.blp`.
No painted window frame or custom button is delivered. This pack does not modify Lua or build BLP atlases.

Reproduce the PNG exports with Pillow and NumPy:

```powershell
python localTools/interface/buildItemForgeArt.py
```

The assembler validates canvas sizes, image modes, shared state alpha, transparent pixel RGB,
additive boundaries, steam cell isolation and fade-out, the banner text zone, and reused asset dimensions.
