# Friend client patch

Run `Build-FriendPatch.cmd` after client-side changes. It:

1. Runs `localTools\patchSinisterStrike.ps1` to regenerate the custom spell data (server and local client DBCs).
2. Rebuilds `patch-Z.MPQ` with `localTools\mpq-builder` and copies it into the local client's `Data` folder.
3. Packages the MPQ and the custom addons into `dist\CustomWotLKClientPatch-<version>.zip`.

The version defaults to the newest `x.y.z` package in `dist` with the last number increased (1.0.2 -> 1.0.3). Pass
`-version 1.1.0` to choose one, or `-skipSpellData` to package the current `patch-Z.MPQ` without regenerating it.

Spell data changes also affect the server: restart the worldserver after building so it loads the new DBCs.

The friend extracts the ZIP and runs `Install-Patch.cmd`. The installer asks for their WotLK folder and your
reachable server IP/hostname.
