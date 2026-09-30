# mod-stat-growth

Adds two independently rolled drops to eligible creature corpses:

- `Ascendant Essence of Growth` (item `42590`) grants a random, permanent class-compatible stat bonus.
- `Ascendant Essence of Wisdom` (item `39163`) grants a permanent, stacking experience-gain bonus.
- `Ascendant Essence of Flow` (item `21238`) grants permanent, stacking primary-resource regeneration.
- `Ascendant Essence of Vitality` (item `41606`) grants permanent, stacking maximum health.
- `Ascendant Essence of Fortune` (item `23656`) grants permanent, stacking gold gains and loot quality.

Bonuses are stored per character in AzerothCore's `character_settings` table. Native client item IDs are used so
inventory icons work without a custom MPQ patch.

Essences are consumed automatically when looted, so they never take bag space. Essences obtained another way
are used by right-clicking them. Consuming an essence plays an essence-colored burst, the level-up visual, an achievement sound, a center-screen
announcement, and a detailed permanent-upgrade message.

Every successful essence drop rolls one tier without changing the essence's overall drop chance:

- Faint: 80%, 1x base bonus, rare quality.
- Greater: 18%, 3x base bonus, epic quality.
- Ascendant: 2%, 10x base bonus, legendary quality.

The native client item rows used for Faint and Greater icons are backed up to
`mod_stat_growth_item_backup` and `mod_stat_growth_item_locale_backup` before migration. The optional restore SQL
is included in `data/restore/stat_growth_essences_restore.sql` and is not run by the automatic updater.

Default settings:

- Growth drop chance: 10%
- Growth bonus per use: +1 stat
- Experience drop chance: 10% (independent roll)
- Experience bonus per use: +10%
- Resource drop chance: 10% (independent roll)
- Resource regeneration per use: +1%
- Vitality drop chance: 10% (independent roll)
- Maximum health per use: +1%
- Fortune drop chance: 10% (independent roll)
- Fortune (gold gains and loot quality) per use: +1%
- Stack size: 200
- Gameplay cap: none
- Greater tier chance and multiplier: 18%, 3x
- Ascendant tier chance and multiplier: 2%, 10x

Configuration is installed as `configs/modules/mod_stat_growth.conf`.

The server must have `EnablePlayerSettings = 1` in `worldserver.conf`.

## Automatic class spells

Characters automatically learn all eligible class-trainer abilities and rank upgrades for free when they level
up. Existing characters receive any missing abilities the next time they log in. Talents, professions, riding,
pet-trainer spells, and quest rewards are not included. Set `StatGrowth.AutoLearnClassSpells = 0` to disable it.

## Equipment bonuses

Looted weapons and armor can receive persistent bonuses based on their original Blizzard quality. The item name,
quality, icon and presentation are never replaced. Rare gear has a higher roll chance, more bonuses and stronger
values than common gear.

The bonus pool contains class-compatible attributes, the same effects as the Experience, Resource, Vitality
and Fortune Essences, and Leech, which heals the wearer for a percentage of the effective damage they deal
(capped by `GearBonus.MaxLeech`). Rolls are stored by item GUID, survive trading, mail and restarts, and apply only while the
item is equipped. The mandatory addon only inserts matching white stat lines into the standard tooltip; it does
not add popups, borders, custom rarity labels or renamed items.

## Smart base loot

Equipment already selected by a creature's normal loot table is progression-aware. The system does not create an
extra gear roll: it replaces unusable or obsolete uncommon-or-better equipment with an item of the same quality the
looting class can equip, prioritizing empty slots first and then the character's lowest-item-level slots. Poor and
common equipment is left as dropped. A replacement is never an item the player already owns (bags and bank
included) or one already on the same corpse. Armor uses the class's intended armor type,
caster-only stats are rejected for physical classes, and deterministic-stat templates prevent unsuitable random
suffixes. Replacement required level stays within five levels of the defeated creature's progression level and can
never exceed the player's level, so trivial creatures cannot be farmed for current-level gear.

## Fortune

Fortune never changes item counts. Its total effective bonus caps at 100%, including equipment: earned gold can
at most double. Only loot, quest and activity rewards receive this multiplier; trades, mail, auctions, vendor
sales and refunds do not. Forge costs are unchanged.

Fortune adds at most 10 percentage points to gear-affix chance and 10% to affix strength. New Fortune affixes
roll 1-2%, without scaling from the player's own Fortune; equipped Fortune contributes at most 20% in total.
Existing saved Fortune affixes are migrated down to 2%; resource affixes above 500% are capped at 500%.
The migration covers all item GUIDs, including gear in bags, banks, mail and auctions; other affixes are preserved.
`Fortune.*` configuration can lower these benefits but cannot exceed the hard limits in `EssenceTuning.h`.

Resource regeneration and rage/runic-power generation bonuses cap at 500%, including gear and mirrored bot
essences. Resource and Fortune essences consumed while capped grant a randomly selected uncapped family of
the same tier instead. Resolution happens per recipient, including shared corpse loot, old inventory items,
and each essence in a dungeon reward batch. The final grant approaching a cap is limited to the remaining room.
The character-database migration also caps saved Resource essence at 500% and Fortune essence at 100%.
Back up `character_settings` and `mod_personal_loot_roll` and stop worldserver before applying it, then restart
to refresh the in-memory values. The migration is idempotent; unspent essence items and existing gold stay intact.

## Combat rogue rework ("Crimson Duelist")

Sustained damage, no burst windows, and Energy that never blocks the rotation. The full design, numbers and
simulations are in `.agents/plans/combat-rogue-rework/combat-rogue-rework.DESIGN.md`.

- **Kit**: Sinister Strike is free and generates Energy with a chance to grant Opening (always on crit); Quick Cut
  (lights up when Opening is available), Shadow Lunge, Riposte; AoE with Crescent Slash (pure damage) and Crimson
  Sweep (bleeds); finishers Eviscerate (Battle Tempo), Slice and Dice, Sanguine Veil (lifesteal upkeep), Blood
  Waltz (AoE), and Crimson Daggerfall (15-sec AoE dagger barrage with bonus damage against the rogue's DoTs).
  Every finisher consumes up to 30 extra Energy for up to +50% damage or duration.
- **Evolutions**: the kit upgrades itself at set levels (20, 25, 30 … 75), announced in chat.
- **Talents**: the whole Combat tree is replaced (same grid, new talents on the WotLK talent spell ids). Rogues get
  a free talent reset on their first login after the update.
- **Code**: `src/CombatRogue.h` (hooks), `src/CombatRogueCommon.*` (state, talents, shared mechanics),
  `src/CombatRogueScripts.cpp` (core spell and aura scripts), `src/CombatRogueDaggerfall.*` (Daggerfall);
  SQL is in `data/sql/db-world/base/stat_growth_combat_rogue*.sql`.
- **Client data**: `localTools/patchSinisterStrike.ps1` builds the spells, talents, tooltips and icon references for
  both the server and the client DBCs.
- **Icons**: put PNGs named as in the design doc into `client-assets/source`, then run
  `clientPatcher/Build-FriendPatch.cmd` (it compiles them with `localTools/buildRogueClientAssets.ps1`). Any icon
  not generated yet falls back to a stock game icon.
- **Sounds**: custom spell sounds live in `client-assets/sounds`; the DBC generator registers them at controlled
  volume and the MPQ builder packages them under `Sound\Spells\Custom\CombatRogue`.

## Flight-master quick travel

DragonUI flight-master pins are clickable. Selecting a faction-compatible flight master starts a three-second,
interruptible Quick Travel cast and teleports to that taxi node. Travel is rejected while dead, in combat, already
travelling, casting, or inside instances and battlegrounds. The server resolves and validates the taxi node by name;
the client never supplies coordinates.

## Infinite Dungeon (Donjon infini)

Endgame rewards: Infinite Dungeon caps at ilvl 310 on floor 100; Mythic+ at ilvl 370 on key +60, with keys
limited to +99; L'Infini reaches ilvl 460 at tier X. The Forge caps non-raid upgrades at 370 and L'Infini
upgrades at 460. Existing items retain their templates and stats; exact-cap rewards use new generated blocks
after the existing Forge range, requiring the matching `awesome_wotlk` client extension.

An endless ladder of short floors for one or two real players, from level 15 (design:
`.agents/plans/infinite-dungeon/infinite-dungeon.DESIGN.md`). Code in `src/infinite/`, the core side in
`src/server/game/Maps/InfiniteDungeon.h`, the client panel, banner and map pins in
`clientPatcher/interface/Interface/FrameXML/InfiniteDungeon.lua`.

- **Entry**: Eternia (920000) stands in the ten capitals, pinned on the world map. Alone, or with a group partner
  standing near (bots never enter, and cannot be summoned in). A duo starts at the lower checkpoint of the two.
- **Floors**: each floor is a fresh instance of a dungeon room picked by the run's level
  (`InfiniteDungeonArenas.cpp`, generated by `localTools/infiniteDungeon/buildArenas.py`). The players and the run's
  creatures live in their own phase, so the dungeon's own creatures never meet them. Two trash packs (one led by an
  elite) and a boss, copies of the room's own creatures (920010-920299) at the players' level, with telegraphed
  abilities drawn by the ground indicators. The players arrive in a ring where nothing can attack them; stepping out
  starts the floor. The boss down, a portal leads down; every tenth floor is a checkpoint with a chest.
- **Two ladders**: below the level cap floors add 5% up to +75%; at the cap a second ladder starts at floor 1 and
  provides entry gear from item level 200 to 310 at floor 100, without requiring paragon. Health and damage
  use a fresh level-80 baseline and stop growing at floor 100. Numbers in
  `InfiniteDungeonScaling.h`; the monsters' health follows the roles of the run (a tank counts 60%, a healer 40%),
  their damage whether a tank is there.
- **Rewards**: experience (25 same-level kills' base experience a floor, about a dungeon's hour), gold, a 10% essence
  chance; every fifth floor one fitted item (Smart Loot at the player's level, generated item levels at the cap);
  every tenth floor a chest (essences, paragon points at the cap). Hearts dropped by the dead heal 45% / 30% mana.
- **Data**: `data/sql/db-world/base/stat_growth_infinite_dungeon*.sql`,
  `data/sql/db-characters/base/infinite_dungeon_v1.sql` (`character_infinite_dungeon`).
- **GM commands**: `.infinite start [floor]`, `.infinite floor <n>`, `.infinite arena <index>` (0-31, to look at each
  room), `.infinite clear` (kills the floor), `.infinite leave`, `.infinite checkpoint <n>`, `.infinite info`. Test
  with `.gm off`: a game master sees every phase.
