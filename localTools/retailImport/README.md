# retailImport: retail item models in the 3.3.5 client

Step-by-step procedure: `.agents/docs/systems/retail-import.md`.

Converts item looks from the local retail install (`C:\Program Files (x86)\World of Warcraft`, read only) into
3.3.5a client files, plus the DBC rows and test items that use them: weapons, shoulders, helmets (a model per race
and gender), body armour (its region textures, palettized as stock ones) and cloaks. Retail's collections models
(rigged to the character skeleton) are left out: 3.3.5 cannot draw them (`.agents/docs/systems/retail-import.md`).

## Pieces

- `RetailImport/` (.NET 10 console):
  - reads retail files by FileDataID with [TACTSharp](https://github.com/wowdev/TACTSharp) (wowdev, MIT) and the
    retail DB2s with [DBCD](https://github.com/wowdev/DBCD) (wowdev, MIT) + [WoWDBDefs](https://github.com/wowdev/WoWDBDefs);
    names come from the [community listfile](https://github.com/wowdev/wow-listfile);
  - converts models with the M2/skin converters of [MultiConverter](https://github.com/MaxtorCoder/MultiConverter)
    (MaxtorCoder, originally by Adspartan), compiled from a pinned checkout in `.deps` - the project has no
    license, so its source is not copied here. MultiConverter strips the Legion chunks, fixes cameras and
    animations, rewrites the skins' shader ids into 3.3.5 texture units and blend overrides, and sets version 264;
  - then its own pass (`M2.cs`): 3.3.5 names for the textures a model hardcodes (MultiConverter's own naming
    reorders duplicate texture ids, so it never sees the TXID chunk), only the global flags 3.3.5 knows, Stand as
    the only sequence (what stock item models hold), and a structural check of the model and its skins (every
    array inside the file, every batch's material, texture, texture unit, transparency and UV lookup in range);
  - copies the BLPs as they are (retail item textures are BLP2 DXT1/DXT5 a 3.3.5 client reads), with the
    "has mipmaps" byte set to what the file holds (retail stores other values there).
- `items.json`: what to import (retail ItemDisplayInfo id, slot, new display id) and the test items.
- `retailItems.generated.json`: the ItemDisplayInfo.dbc / Item.dbc rows, installed by `localTools/patchSinisterStrike.ps1`.
- `preview.py`: renders each converted model next to its retail original (`.deps/previews/*.png`).

Outputs: `modules/mod-stat-growth/client-assets/compiled/retail-items/<archive path>` (shipped in patch-Z by
`localTools/mpq-builder/patchFiles.js`) and `modules/mod-stat-growth/data/sql/db-world/base/stat_growth_retail_items.sql`.

## Use

```powershell
powershell -File localTools/retailImport/setup.ps1        # once: MultiConverter checkout, listfile, build
$tool = 'localTools/retailImport/RetailImport/bin/Release/net10.0/RetailImport.exe'
& $tool probe 128476          # an item's appearances: retail display ids, model/texture FileDataIDs, icons
& $tool probe-model 1627181   # the items (and displays) using a model file, e.g. one found in the listfile
& $tool probe-texture 1549343 # the items using a texture file (a set's belt, boots, bracers on other items)
& $tool import                # convert everything in items.json
python localTools/retailImport/freeItemIds.py --class 2   # free item entries to carry a new look
python localTools/retailImport/preview.py
```

Then, like any client data change: `clientPatcher/Build-FriendPatch.ps1` (runs patchSinisterStrike.ps1, which also
writes the server's Item.dbc and ItemDisplayInfo.dbc) and a release, and a worldserver restart for the SQL and the
server DBCs (the server enforces Item.dbc's class, slot and display over item_template).

## Ids

- ItemDisplayInfo: 70001-79999 (the client's highest stock id is 68742). The table is built on the client's
  effective copy - Patch-D's, with 124 HD weapon remakes - extracted once by `mpq-builder/extractEffectiveClientFile.js`.
- Items: entries below 65536 already in the client's Item.dbc with no item_template (65536+ are Mythic+ generated
  items for the client DLL). Their Item.dbc rows get the class, slot and display.

## Limits

- Particle emitters are dropped (MultiConverter does not convert the Legion+ particle layout). Ribbons are kept.
- Retail shader effects become two 3.3.5 passes (MultiConverter's mapping), so glows and reflections are close,
  not identical.
- Not handled: .anim files (a Stand kept outside the model), external skeletons (SKID), collections models,
  models with more than 65535 vertices in one submesh.
- Adapted to the 3.3.5 character: helmets are moved back onto the 3.3.5 head per race and gender
  (`Importer.HeadFits`, `ClassicM2.Refit`), and a cape's cut hem, which the opaque 3.3.5 cape would show white, is
  filled with its own pattern (`BodyTexture.CapeForClassic`).
- Up to Shadowlands per MultiConverter; later models may add chunks or layouts it does not know. The structural
  check fails the import rather than shipping a model the client may crash on.

## Gear looks (not wired)

`localTools/mythicAppearance/buildMythicAppearance.py` picks real item entries (`mythic_appearance_tier.appearance_entry`)
and checks them against item_template, Item.dbc and ItemDisplayInfo.dbc. An imported look can be used there once
it has such a carrier item (a test item here): add its entry to `WEAPONS` for weapons, or as a per-slot override for
armour (tiers are chosen by item set today).
