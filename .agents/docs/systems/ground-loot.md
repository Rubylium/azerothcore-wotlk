# Ground loot

Diablo-style boss loot: a moment after a dungeon boss (or a Défi's boss) dies, its loot flips out of the corpse onto
the floor, each player's own, and they walk over it (or click it) to pick it up. It replaces need/greed rolls there.

## Where it applies

- `GroundLoot::Open(corpse)` (`modules/mod-stat-growth/src/GroundLoot.cpp`), at the boss's death (its own
  `UnitScript`, and called first by the mythic rewards so the order of the death hooks does not matter).
- A boss: in a Défi's instance its board boss (`GetChallengeBossOf`), else in a 5-man dungeon `IsMythicDungeonBoss`
  (a dungeon boss, a world-boss rank, a listed one). Not raids outside the Défi, not the Infinite Dungeon's floors.
- Real players only. Bots get nothing on the floor (and no corpse loot either).

## What is thrown

- The corpse's loot, shared out at the kill: each item to one eligible player (one who can use it if any), a
  free-for-all item to each, the gold in equal shares. Eligible: the loot recipient or its group. The corpse is then
  emptied (quest items stay on it, lootable as usual).
- Mythic items given within 20 s of the kill (`GiveSelectedMythicItem` asks `GroundLoot::Throw` first): the Mythique 0
  item, the Mythic+ end-of-key item (the last boss throws it), the Défi's god / pinnacle gear (given at the Killed event
  now, not at Won). Anything given that way later, or with no kill behind it, goes straight to the bags.
- A touch decided before the item exists marks the drop "unique" (its own landing sound): `GivePinnacleLootItem`
  rolls the Hollow Voice's chance first and passes a touch only when it hits.

## How it looks

- Per player: each drop is a `TempSummon` the player summons with `visibleBySummonerOnly`, so only they see it.
  900120 a bag (display 60002, Ascension's `ashran_loot_state`), 900121 gold (60003, the treasure goblin's coin pile),
  900122 a light beam over it (60004-60009: Ascension's moonbeam tinted white, green, blue, purple, orange, gold).
- The arc is `MoveJump` from inside the corpse (no missile spell): the bag appears, jumps 250 ms later, lands about
  900 ms after that, past the corpse's reach (golden-angle spread, collision-checked). Drops leave 300 ms apart. The
  landing is timed on the jump's own spline (`movespline->Duration()`, what the client draws), the burst's sound goes
  with the first jump and each landing's 150 ms before touchdown: the user wants them back to back (2026-10-05).
- Sparkles are `SpellVisualKit`s sent to the owner alone (`SMSG_PLAY_SPELL_VISUAL` by direct message), landing and
  pickup. Sounds are played by the server to the owner alone, by SoundEntries id (`PlayDirectSound`, the gold's landing
  `PlayDistanceSound` on its bag): the burst, a landing per tier (item, epic, unique, legendary, gold), a pickup (item,
  gold). A kit's own sound was never heard when the server played the kit (2026-10-05, v1.0.309): don't put the
  sounds back in the kits. Each beam plays its quality's ambient loop for as long as it stands: its display's
  `CreatureSoundData.LoopSoundID` (client DBC only). Levels: Diablo IV's loot sounds, mixed for its own engine, were
  barely heard as they came (its loops 20 dB under the rest); every row is at volume 1, the import `normalize`s the
  cues to -12 dBFS RMS and the loops to -16 (a limiter holds the peaks under -1 dBFS). Asked "so much louder" at
  -24/-20 dBFS RMS and volume 0.45-0.9 (2026-10-05).
- The tooltip: the server whispers `GLOOT\t<guid hex>\t<item link | gold:<copper> | ->` as each drop leaves the corpse;
  `clientPatcher/interface/Interface/FrameXML/GroundLoot.lua` swaps the unit tooltip for the item's (or the gold).

## Pickup and leftovers

- Walking within 2.5 yd (alive), or right-clicking it (gossip), gives it as corpse loot would: the loot hooks
  (`OnPlayerLootItem`: essences consumed, personal-loot bonuses rolled), "You receive loot", the money notify.
- Bags full: it stays on the floor (an "inventory full" error, every 5 s at most).
- Left 3 minutes, or the player leaves the instance, or logs out (`OnPlayerBeforeLogout`, before the save): it goes to
  the bags, the mailbox when full.
- The Défi waits for it (`RaidFinder.cpp` `UpdateChallengeLoot`): the way home starts once no player has any loot
  pending (at least 3 s after the kill), or 60 s after the kill whatever is left.

## Assets

- `localTools/groundLoot/ascensionVisuals.json`, imported by `localTools/ascensionImport/importVisuals.py` into
  `modules/mod-stat-growth/client-assets/imported` (kits 81911, 81913; sounds 81920-81932). The importer's `models` (raw
  models, optionally `tint`ed) and `sounds` (WAVs of our own, here Diablo IV's from `data/custom/diabloLootSounds`,
  local, optionally `normalize`d) sections exist for it.
- Displays, sound loops: `localTools/patchSinisterStrike.ps1` (`$ownDisplays`). Creatures:
  `modules/mod-stat-growth/data/sql/db-world/base/stat_growth_ground_loot.sql`.
- Not yet: name labels on the floor (the beam and the tooltip carry the item for now).
