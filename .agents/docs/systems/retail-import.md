# Retail item models in the 3.3.5 client

Retail WoW item looks (weapons, shoulders) are converted into 3.3.5a client files by `localTools/retailImport/`
(technical reference: its `README.md`). Proven in game on 2026-09-29: the Legion Subtlety artifact daggers (display
70001, items 19313/19314) and the rogue Tier 21 Mythic shoulders (display 70002, item 16144) render correctly.

## What can be imported

| Kind | Status |
|---|---|
| One-hand / two-hand weapons, shields, off-hands, ranged | ✅ one line in `items.json` |
| Shoulders (left + right models) | ✅ one line |
| Helmets | ❌ per race/gender models not handled yet |
| Chest, legs, hands, feet, wrist, waist, back | ❌ retail body textures use the HD character layout (ItemDisplayInfoMaterialRes); they would need repacking into 3.3.5's texture regions |
| Models newer than Shadowlands | ⚠️ outside MultiConverter's range; the structural check fails the import rather than shipping a crashing model |

Conversion losses: particle emitters are dropped (a flame, a mist), retail shader effects become two 3.3.5 passes
(glows and reflections close, not identical), only the Stand sequence is kept (stock item models have one).

## Adding a look (checklist)

1. **Once per machine**: `powershell -File localTools/retailImport/setup.ps1` (pinned MultiConverter checkout,
   listfile, build into the gitignored `.deps`). Retail must be installed at `C:\Program Files (x86)\World of
   Warcraft` (read only, never written).
2. **Find the retail display**:
   - `RetailImport.exe probe <retail item id>` lists an item's appearances: display id, model and texture
     FileDataIDs, icon;
   - `probe-model <fdid>` goes from a model file (found in the listfile) to its items.
   - Prefer Legion to Shadowlands pieces.
3. **Add a `displays` entry** to `localTools/retailImport/items.json`:
   - `id`: next free in the reserved **70001-79999** range (the client's highest stock ItemDisplayInfo id is 68742);
   - `retailDisplay`;
   - `slot` (`weapon` / `shoulder`);
   - `clone`: a stock 3.3.5 display of the same kind, for sounds and flags;
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
- Retail BLPs are copied as is (BLP2 DXT1/DXT5), except their "has mipmaps" byte. Don't re-encode them.
- MultiConverter has no license: its source stays in the gitignored checkout, with attribution in the README. It
  had three bugs the tool works around (texture name order, overwritten model name, unknown global flags); keep
  that pass (`M2.cs`) when updating the pin.
- A new look is invisible to the gear-looks system (`localTools/mythicAppearance/buildMythicAppearance.py`,
  `mythic_appearance_tier`) until a carrier item exists. Weapons go in `WEAPONS`; armour needs a per-slot override
  (tiers are picked by item set).
