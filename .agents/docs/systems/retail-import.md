# Retail item models in the 3.3.5 client

Retail WoW item looks (weapons, shoulders) are converted into 3.3.5a client files by `localTools/retailImport/`
(technical reference: its `README.md`). Proven in game on 2026-09-29: the Legion Subtlety artifact daggers (display
70001, items 19313/19314) and the rogue Tier 21 Mythic shoulders (display 70002, item 16144) render correctly.

## What can be imported

| Kind | Status |
|---|---|
| One-hand / two-hand weapons, shields, off-hands, ranged | ✅ `slot: weapon` |
| Shoulders (left + right models) | ✅ `slot: shoulder` |
| Helmets | ✅ `slot: head`: one model per 3.3.5 race and gender (20), moved back onto the 3.3.5 head (below), the hair/ears it hides from a stock helmet (`helmetVis`, or the clone's) |
| Chest, legs, hands, feet, wrist, waist | ✅ `slot: body`: the body textures only (see below) |
| Cloaks | ✅ `slot: cape`: the cape texture, its cut hem filled (below) |
| Collections models (retail Legion+: chest, legs, gloves, boots, belt buckle; Midnight: helmets too) | ❌ 3.3.5 has no slot that draws a model rigged to the character skeleton |
| Models newer than Shadowlands | ⚠️ outside MultiConverter's range; the structural check fails the import rather than shipping a crashing model |

Conversion losses: particle emitters are dropped (a flame, a mist), retail shader effects become two 3.3.5 passes
(glows and reflections close, not identical), only the Stand sequence is kept (stock item models have one).

### Body armour

Retail still paints armour on the character texture region by region, with the same region names 3.3.5 uses
(`…_Chest_TU`, `…_Sleeve_AU`, `…_Boot_FO`): `ItemDisplayInfoMaterialRes` gives each display one material per
`ComponentSection` (0-7 are 3.3.5's ArmUpper … Foot, in the order of ItemDisplayInfo.dbc's Texture[8]; 8-10, Accessory
and Scalp, have no 3.3.5 region and are dropped). The importer:
- writes each region's files as `Item\TextureComponents\<Region>Texture\<name>_<U|M|F>.blp`, the field holding
  `<name>` (the client adds the suffix: `ComponentTextureFileData.GenderIndex` 0 → M, 1 → F, else U);
- re-encodes them **palettized** (256 colours, 8-bit alpha when there is any, full mip chain), the format of every stock
  body texture (`BodyTexture.cs`). Retail ships them DXT-compressed;
- keeps them at most twice the stock size (256 wide: a torso upper 256x128 against stock's 128x64), as Ascension's
  conversions do: Legion's come at that size, Midnight's at four times it (512x256) and are scaled down
  (`halveBodyTextures` in items.json brings every one to the stock size);
- gives a texture a name of its own when retail's would collide with another file's (Midnight names them after a
  model's FileDataID, the same name for other colours: `6756705_be_m_al_1077519`);
- takes the display's first three geoset groups from retail (boots 2, cape 1, ...).

### Helmets: the head moved

Retail gave every race a new model and moved its helmets with it: a retail helmet sits forward and higher than the
3.3.5 head (HuM: 0.061 forward, 0.193 up; seen in game as "too high"). The move is the same for every helmet of a race
and gender, measured on stock helmets retail still ships (`helm_mail_raidhunter_h_01`, `helm_plate_raidwarrior_h_01`,
`helm_leather_raidrogue_h_01`, the client's copy against retail's): a translation, plus a scale for tauren (1.35 male,
1.25 female) and gnomes (1.15). The importer applies the inverse to each head model (`Importer.HeadFits`,
`ClassicM2.Refit`: vertices, pivots, attachments, bounds). Ascension corrected only its HuM conversions, by hand, by
the same amount; the rest of its helmets float.

### Capes: no transparency

3.3.5 draws the cape opaque (the character models' cape batches have blend mode 0). Retail cuts a cape's hem out with
the texture's alpha (`cape_leather_raidrogue_r_01_mythic2long`: a diagonal at the bottom), and 3.3.5 shows the
colour under the cut: white. The importer fills the cut by repeating the cape's own pattern downwards at the period
that matches it best (`BodyTexture.CapeForClassic`), as Ascension's copy does by hand, and writes the cape
palettized. The layout is stock's, nothing else to change: half a cape the model mirrors, the clasp in the top left
corner and a transparent strip under it that no cape samples.

### Belts

A Legion+ belt's look is its collections model (the buckle and straps). Its region textures (`belt_tl`, `belt_lu`)
repeat the waistband the leggings already paint: the T20 belt adds nothing visible over the T20 legs, in Ascension's
conversion too. A belt of a set therefore "doesn't show" next to its own leggings; over other legs it does.

### Modern sets: collections

From Legion on, retail adds "collections" models to body pieces: geometry rigged to the character skeleton (a Legion
chest's straps; the Midnight rogue set's chest, legs, gloves, boots, belt and its whole mask: 36 bones, 15 000
vertices), one per race and gender. 3.3.5 draws no model on those slots, and the geometry is shaped for retail's newer
bodies, so the importer leaves them out and says so. A Legion set loses little (its look is in the textures, as
Ascension's conversions show); a Dragonflight/Midnight one loses its 3D parts, and a collections-only helmet entirely.

### Ascension as the reference

Ascension's client (`D:\ascension-live`, read by `localTools/ascensionImport/ascensionArchives.js`) holds about 61 000
converted retail item displays, 447 of them full sets. For a set it converted, it is the answer to compare against:
- `ascensionArchives.js dbc <out> ItemDisplayInfo` and `files <list.json> <out>` read its rows and files;
- imported sets: Fanged Slayer's Mythic (test items 1020-1027, 1162) and the Grim Jest Mythic, Midnight season 1
  rogue (905-909, 1163, 4853, 7248: shoulders, cloak and body textures; no mask); every other class's Tomb of
  Sargeras tier in its Mythic look (displays 70020-70100), the Hollow Voice's gear (below);
- the Fanged Slayer's Mythic set (Tomb of Sargeras rogue) is its displays 67441-67448: our import of it
  (70003-70011) matches them model for model (vertices, bones, textures, materials, global loops) and texture for
  texture (palettized, 256x128, same alpha).

### The Hollow Voice's class sets

The board's pinnacle raid pays item level 477, and the gear-looks system (`localTools/mythicAppearance`, its tier at
477) shows each class its own Tomb of Sargeras (Legion T20) set in the Mythic look, as the Cruel Gladiator's sets
wear it: one retail item batch, 144780-145030, the same look on all eight armour slots and the cloak. The custom
classes wear the set of a class of their armour (`CUSTOM_CLASSES` there). A look reaches the client through a real
item: the rogue's are its test items, the others "Hollow Voice look" items (16102-17835 in items.json) that no one is
given.

## Adding a look (checklist)

1. **Once per machine**: `powershell -File localTools/retailImport/setup.ps1` (pinned MultiConverter checkout,
   listfile, build into the gitignored `.deps`). Retail must be installed at `C:\Program Files (x86)\World of
   Warcraft` (read only, never written).
2. **Find the retail display**:
   - `RetailImport.exe probe <retail item id>` lists an item's appearances (modifier 0 normal, 1 heroic, 3 mythic,
     4 LFR): display id, models, their textures, the body textures by region, icon;
   - `probe-model <fdid>` goes from a model file (found in the listfile) to its items, `probe-texture <fdid>` from a
     texture: a tier set's belt, boots and bracers are often other items sharing its textures (a PvP set);
   - `look <text>` lists, slot by slot, every display drawn from the files whose name holds the text, with the items
     wearing it: `look raidwarlockmythic_r_01` is a whole set, belt and bracers included. `find <text>` lists the
     items whose name holds it;
   - a set's files share a name in the listfile (`leather_raidroguemythic_r_01`, `leather_raidroguemidnight_d_01`).
   - Prefer Legion to Shadowlands pieces.
3. **Add a `displays` entry** to `localTools/retailImport/items.json`:
   - `id`: next free in the reserved **70001-79999** range (the client's highest stock ItemDisplayInfo id is 68742);
   - `retailDisplay`;
   - `slot` (`weapon` / `shoulder` / `head` / `body` / `cape`);
   - for a helmet, `helmetVis` [male, female]: HelmetGeosetVisData ids of a stock helmet of the same cover (247 / 369
     hide the hair as a hood does), or none to keep the clone's;
   - `clone`: a stock 3.3.5 display of the same slot, for sounds and flags (the ICC rogue tier's for leather:
     helmet 64429, chest 64435, legs 64432, gloves 64944, boots 64437, belt 64430, bracers 64439, cloak 64304);
   - a `note`.
4. **Give it a carrier item** if it needs one (a test item, or an item the gear-looks system can point at):
   - `python localTools/retailImport/freeItemIds.py --class 2` lists entries below 65536 that the client Item.dbc
     has and nothing on the server uses (memory: custom item ids);
   - add it to `items` with class, subclass, inventory type, sheath, `statsFrom` (a stock item whose stats it copies)
     and names in both languages.
5. **Convert**: `RetailImport.exe import`, then `python localTools/retailImport/preview.py`.
   - **Look at every `.deps/previews/*.png` with the Read tool**: the converted model must match its retail
     original (shape, textures, left/right shoulders).
   - The import fails on any structural error. Never ship around it.
6. **Commit**: the assets under `modules/mod-stat-growth/client-assets/compiled/retail-items/`,
   `retailItems.generated.json`, `items.json` and `stat_growth_retail_items.sql`.
7. **Release, in this order, with the user's go**:
   - the client patch and release (`deployWithProgress.ps1 -steps client,...`, WoW closed). It also writes the
     server's Item.dbc and ItemDisplayInfo.dbc;
   - **then** restart the worldserver (the SQL applies, and the server enforces Item.dbc's class, slot and display
     over item_template);
   - the user tests with `.additem <entry>`.

## Pitfalls

- `.m2`, `.skin` and `.anim` are **binary** in `.gitattributes`. They were once treated as text, which silently
  corrupted committed models. Check `git diff --stat` shows `Bin` for them before committing.
- The client reads ItemDisplayInfo.dbc from `Patch-D.MPQ` (124 HD weapon remakes), not the stock one. The pipeline
  builds on that effective copy (`mpq-builder/extractEffectiveClientFile.js`). Never patch the stock table.
- Model textures (weapons, shoulders, helmets) are copied as is (BLP2 DXT1/DXT5), except their "has mipmaps"
  byte: don't re-encode them. Body textures and capes are the exceptions: palettized like stock ones (above).
- Whatever retail cuts out with alpha must be checked against how 3.3.5 draws that part: an opaque batch (the cape)
  shows the colour under the cut.
- MultiConverter has no license: its source stays in the gitignored checkout, with attribution in the README. It
  had bugs the tool works around (texture name order, unknown global flags, and the blend override array it writes
  at 0x130 over whatever data starts there: the model name, a helmet's global loops, the first sequence of the
  paladin's T20 helmets; `ClassicM2.RestoreUnderHeader` moves any of them out and takes the bytes back from
  retail); keep that pass (`M2.cs`) when updating the pin.
- One retail texture can serve two slots (the warlock's T20 helmet and shoulders): the client looks for it in each
  model's folder, so it is copied once per folder.
- A new look is invisible to the gear-looks system (`localTools/mythicAppearance/buildMythicAppearance.py`,
  `mythic_appearance_tier`) until a carrier item exists. Weapons go in `WEAPONS`; armour needs a per-slot override
  (tiers are picked by item set).

## Open work (to look into later)

Asked by the user on 2026-09-29 ("keep the limits in touch, we need to look into that later"):

1. **More free item ids**:
   - Today a carrier item must be an existing client Item.dbc row with no template: about 500 left.
   - The patch script already appends DBC rows (`Add-DbcRecordCopy` in `patchSinisterStrike.ps1`). Doing it for
     Item.dbc opens every unused id below 65536: 8 729 above the highest stock item (56806) plus the gaps, about
     19 900 in all, with no DLL change.
   - Then teach `freeItemIds.py` to list them.
   - Only if that ever runs out: move the Mythic+ generated range (`Mythic::GeneratedItemBase`, the client DLL's
     GeneratedItems) far higher, which needs a migration of every generated item instance.
2. **Particles**:
   - Legion+ emitters are dropped (the T21 shoulders lose their flame).
   - Port the particle layout to 3.3.5's M2Particle, or rebuild each emitter from a stock one.
3. ~~Helmets~~ and 4. ~~Body armour~~: done on 2026-10-02 (above). Open: whether the stock client composites
   retail-size (2x) body textures as well as Ascension's does, to check in game; `halveBodyTextures` is the fallback.
   Collections models (rigged to the character skeleton) would need the client DLL to draw them, on the classic
   bodies: a research project, not a converter feature.
5. **Newer models**: Dragonflight/The War Within M2s are outside MultiConverter's range. Our own converter would also
   remove the dependency on an unlicensed, unmaintained library.
6. **Gear-looks wiring**: feed imported weapons and shoulders into `mythic_appearance_tier` for the high tiers.
