CUSTOM WOTLK CLIENT PATCH

Requirements:
- A clean Wrath of the Lich King 3.3.5a client (build 12340)
- The game must be closed while patching

Installation:
1. Extract the entire ZIP.
2. Run Install-Patch.cmd.
3. Enter the path to the folder containing Wow.exe.
4. Enter the server IP or hostname supplied by the server owner.
5. Start the game using Wow.exe.

The installer backs up every replaced file under _RubyEbonBackup inside the client folder.
It does not include or install the original World of Warcraft client.

Wow.exe is patched in place (the original copy goes to the backup folder) so that it loads
AwesomeWotlkLib.dll: smooth vector font rendering and client fixes. The font renderer can be turned
off in game with: /console MSDFMode 0
Windows 10 or 11 is required for it.
