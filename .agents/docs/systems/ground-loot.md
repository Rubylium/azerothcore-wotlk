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
- Any other item a boss kill gives goes through `GroundLoot::ThrowItem` (shown at least of the quality it asks): the
  paragon glyph (`ParagonSystem.cpp` `GiveGlyph`, thrown as a legendary). A new kill reward must use `Throw` /
  `ThrowItem` too - "ALL drops should be on the ground" (user, 2026-10-06). Hooks giving loot at the kill call
  `GroundLoot::Open(corpse)` first (`OnParagonCreatureDeath`, the mythic rewards): the death hooks run in no set order.
- A boss that drops little still throws a handful: each player with fewer than 6 drops (Défi) or 4 (dungeon) from the
  corpse gets piles of gold for the rest, their corpse gold share plus 10 g + 5 g a tier (Défi) or 4 g + 1 g a key
  level (dungeon), split unevenly (`TopUpWithGold`). The gear thrown later comes on top.
- A touch decided before the item exists marks the drop "unique" (its own landing sound): `GivePinnacleLootItem`
  rolls the Hollow Voice's chance first and passes a touch only when it hits.
- Legendaries and Uniques (mod-legendary `RollDrops`) open the corpse first (`GroundLoot::Open`) at the boss's death
  and at a key's end: rolled before the ground loot's own death hook, `Throw` found no burst open and the item went
  to the bags unseen - no beam, no sound (2026-10-09).

## How it looks

- Per player: each drop is a `TempSummon` the player summons with `visibleBySummonerOnly`, so only they see it.
  900120 a bag (display 60002, Ascension's `ashran_loot_state`), 900121 gold (60003, the treasure goblin's coin pile),
  900122 a light beam over it (60004-60010: Ascension's moonbeam tinted white, green, blue, purple, orange, gold, and
  red for a Unique - quality 6, the client extension's `#e8332b`; it showed orange as a legendary until 2026-10-09).
- The arc is `MoveJump` from inside the corpse (no missile spell): the bag appears, jumps 250 ms later, lands about
  900 ms after that, past the corpse's reach (golden-angle spread, collision-checked). Drops leave 300 ms apart. The
  landing is timed on the jump's own spline (`movespline->Duration()`, what the client draws). Each drop plays its own
  flip as it jumps and its own landing 150 ms before touchdown, back to back (2026-10-05); one flip for the whole burst
  was heard after a landing on the Hollow Voice (2026-10-06).
- Sparkles are `SpellVisualKit`s sent to the owner alone (`SMSG_PLAY_SPELL_VISUAL` by direct message), landing and
  pickup. Sounds are our own sound engine's (`evolutions-audio.md`; the bank: `client-assets/audio/groundLoot.json`),
  to the owner alone: on the bag, a flip as it jumps and a landing per tier (white: Diablo IV's plain landing,
  green: the magic halo, blue: the rare one, epic: the legendary/set one, an epic touched or a Unique: the unique one,
  a legendary: the mythic one, gold)
  50 ms before touchdown, then its quality's loop for as long as it lies there (on the bag, not on the beam just
  summoned: the client may not have that one yet; it stops with the bag); a pickup (item, gold) as an interface sound.
  History: the game's SoundEntries way - kit sounds never heard, SMSG_PLAY_SOUND barely heard and late, creature
  loops - was replaced (2026-10-06). Diablo IV mixes its loot sounds far quieter than the game: the bank brings them
  to the game's level (-12 dBFS RMS, loops -16); "so much louder" was asked at -24/-20.
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

## Testing

`.groundloot [count]` (game master): a burst thrown out of yourself, past where you stand - with no count one drop of
each kind (white, green, blue, epic, unique epic, legendary, two gold piles), else that many at random. Real items'
tooltips; picking them up plays everything and gives nothing.

## Assets

- Looks: `localTools/groundLoot/ascensionVisuals.json`, imported by `localTools/ascensionImport/importVisuals.py`
  into `modules/mod-stat-growth/client-assets/imported` (kits 81911, 81913). The importer's `models` section (raw
  models, optionally `tint`ed) exists for it.
- Sounds: `modules/mod-stat-growth/client-assets/audio/groundLoot.json` (Diablo IV's, kept in its
  `sources/groundLoot`), built by `localTools/audio/buildAudio.py`.
- Displays: `localTools/patchSinisterStrike.ps1` (`$ownDisplays`). Creatures:
  `modules/mod-stat-growth/data/sql/db-world/base/stat_growth_ground_loot.sql`.
- Not yet: name labels on the floor (the beam and the tooltip carry the item for now).
