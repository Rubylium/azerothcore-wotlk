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

Useful options:

- `Build-FriendPatch.ps1 -forceRebuild`: regenerate all stages and MPQs regardless of the cache.
- `Build-FriendPatch.ps1 -skipSpellData`: deliberately reuse installed patch-Z; class/talent metadata still runs.
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
