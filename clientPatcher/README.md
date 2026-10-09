# Client updates

Run `Build-FriendPatch.cmd` after client-side changes, then `Publish-Release.ps1 -skipBuild` to publish to the
launcher's GitHub releases. `Publish-Release.ps1` without `-skipBuild` performs both steps. The existing deployment
window uses this same flow.

The build writes `.build/current` directly. It no longer creates the standalone `CustomWotLKClientPatch-*.zip`.
The launcher update format is unchanged: MPQs and DLLs are individual assets, addons are deterministic ZIP bundles,
and the manifest reuses URLs for content already published. Unchanged addon bundles are reused locally too.

Generation, interface art and MPQs are cached by source content and verified output hashes, including added/deleted
files. Only changed payload files are copied. The first build populates the cache; subsequent builds reuse it.
Talent art tracks its trees, backgrounds and icons independently of spell scripts.

## The pipeline (`build/ClientGeneration.ps1`)

1. **Generators** (`build/stages.json`): sounds (`buildAudio.py`), ground indicators, Vorhan's icons and eye, the
   paragon icons, the Wow.exe patches, Details' class icons. Each is a cached step that runs again only when one of
   its inputs changed: **nothing is run by hand before a deploy**. A new generator is one entry in `stages.json`
   (its command, inputs and outputs; the progress window reads its label from there too).
2. **Class data** (`patchSinisterStrike.ps1`, the custom classes, the talent trees): cached on the files they are
   generated from (`build/ClientInputs.ps1` Get-ClassDataInputs: every `Spells.ps1`, `StockSpells.ps1`, `Looks.ps1`,
   `talentTree.json`... by name), not on every script under `localTools`. A new data file read by the spell
   generator must be named there. When it rewrites the server's DBCs it says so (`Server DBCs changed`), and the
   deploy window warns when its run has no `restart`.
3. **Our base patches** (`localTools/mpq-builder/buildPatch.js`), split by how often they change
   (`patchFiles.js` packOf), each cached on its own: `patch-Z` the DBCs, icons and talent frames (~15 MB, every
   spell edit), `patch-Y` the ground indicators (~65 MB), `patch-X` everything heavy and stable (models, music,
   sounds, maps, ~250 MB). Built in the background while the interface is built. `node buildPatch.js --verify`
   reads every file back and compares it (not done by default).
4. **Interface**: art steps, then `patch-L`, `patch-<locale>-R` (the vendor glue and our art) and
   `patch-<locale>-S` (our Lua, XML, TOC and interface DBCs, loaded over R): a Lua edit repacks a few MB.

A build whose sources and payload match the last one stops at once (`Client unchanged since build ...`).
Each run writes `.build/timings.json` (every step and tool, in seconds) and prints its slowest.
A news-only change needs no client build: `deployWithProgress.ps1 -steps publish` (news.json is read at publish).

Useful options:

- `Build-FriendPatch.ps1 -forceRebuild`: regenerate all stages and MPQs regardless of the cache.
- `Build-FriendPatch.ps1 -skipSpellData`: deliberately reuse installed patch-X/Y/Z; class/talent metadata still runs.
- `Build-FriendPatch.ps1 -skipInterfacePatches`: deliberately reuse installed interface MPQs.
- `Publish-Release.ps1 -skipBuild -prepareOnly`: validate and prepare release assets without publishing.
- `Publish-Release.ps1 -packageName <old.zip> -allowStale`: explicit rollback using an existing legacy archive.

A failed build invalidates its ready marker. Publishing the current payload verifies both source and payload hashes;
`-allowStale` only applies to explicit legacy archives. Concurrent builds/publications are rejected by a shared lock.
Release versioning still follows the latest GitHub release; `-version` overrides it.

Successful releases send a plain Discord message stating that the new launcher version is available when
`clientPatcher/release.local.json` contains `{ "discordWebhook": "<webhook URL>" }` (gitignored).
No notification is sent for `-prepareOnly` or when nothing changed. Discord failures do not fail the release.

Close WoW for a normal build. Spell data generation also updates server DBCs; restart the worldserver to load them.

Cache regression checks (Windows PowerShell 5.1 supported):

```powershell
powershell.exe -NoProfile -File clientPatcher/tests/BuildCache.Tests.ps1
node clientPatcher/tests/archiveCache.test.js
```
