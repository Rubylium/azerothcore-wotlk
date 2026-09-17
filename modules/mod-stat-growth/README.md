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

Fortune never changes item counts. Each Fortune percent gives every equipment drop a chance to be replaced by a
class-appropriate item one quality higher (poor and common become uncommon, up to epic), and makes gear bonuses more
likely and stronger when the item is looted. Gold gains are still increased by the Fortune percentage. All rates
are configurable under `Fortune.*`.

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
