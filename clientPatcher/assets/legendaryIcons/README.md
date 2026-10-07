# Dungeon legendary icons

All 21 icons from `.agents/plans/legendary-items/dungeon-legendaries.ASSETS.md`, Definitions 4-24.

- `png/`: exact requested filenames, 256 x 256 opaque RGB PNGs, no frame or border.
- `source/`: original square imagegen masters.
- `reference/`: four locally stored stock Wrath icons decoded from the TalentTree BLPs.
- `generationManifest.json`: built-in imagegen prompts, style references and source provenance.
- `contactSheet.png`: all 21 deliveries at 256 px, grouped by dungeon.
- `readabilityPreview.png`: all 21 at native 64 and 32 px.
- `validation.json`: PNG and compiled TGA dimensions, opacity, pixel equality and SHA-256 hashes.
- `clientBuild.json`: verified new icon entries in the rebuilt client archive.

The item and power share each icon. No item or tooltip frames are added for these legendaries.
The icon compiler produced all 21 opaque 64 x 64 TGAs in
`modules/mod-stat-growth/client-assets/compiled/`.

Reproduce the exports and compile them:

```powershell
python localTools/interface/buildDungeonLegendaryIcons.py
python localTools/interface/buildLegendaryClientArt.py
python localTools/interface/buildDungeonLegendaryIcons.py --verify-compiled
node localTools/mpq-builder/buildPatch.js
```

`localTools/mpq-builder/patch-Z.MPQ` was rebuilt using the existing DBCs and compiled client assets.
The archive builder verified every archived source hash; all 21 new icons are included under `Interface\\Icons`.
This delivery does not install the MPQ into the running client or publish a launcher release.
