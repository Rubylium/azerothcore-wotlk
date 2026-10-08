# mod-legendary

Diablo-style legendaries (design: `.agents/plans/legendary-items/legendary-items.DESIGN.md`). A legendary is a base
item - its look, slot and quality - and every copy rolls its own:

- **item level**, from the content it dropped in;
- **power strength**, in a window the item level raises (`PowerWindow`: the definition's bottom window at its floor
  item level, its top one at 370, Mythic+ +60, and above);
- **stats**, the slot's budget at that item level (the power model, `PowerScaling.h`): the primary stat the looter's
  gear favours, stamina, spell power for casters, two secondaries drawn.

The rolls live in `character_legendary`, by item guid, all loaded at startup (a login applies worn gear before any
script could load them). Worn, a copy's stats go through the core's own item stat code
(`Player::ApplyItemStatMod`, called from the `OnPlayerAfterApplyItemBonuses` hook) and its power is counted per slot.
The Forge never takes legendaries.

## Legendaries

| Id | Name | Base item | Source | Power |
|---|---|---|---|---|
| 1 | Marque de l'Inquisiteur | 24567, cloak | Scarlet Cathedral, Mythic+ (dungeon 164) | Direct damage brands the target: X% of it burns as Holy over 4 sec (5-10% at +2, 25-35% at +60) |
| 2 | Serment de Whitemane | 996, ring | Scarlet Cathedral, Mythic+ (dungeon 164) | A killing blow leaves 1 health and heals X% of the health over 4 sec, once per 3 min (10-15% at +2, 40-50% at +60) |
| 3 | Consécration de Mograine | 21428, gloves (misc armour: every class; armour of the looter's type) | Scarlet Cathedral, Mythic+ (dungeon 164) | Every 10 sec in combat, ground where the wearer stands for 6 sec: each second X% of the attack or spell power to enemies and allies within 8 yd (5-10% at +2, 25-35% at +60) |
| 4 | Épaulières de Capacitus | 21424, shoulders | The Mechanar (192) | Melee blows taken strike back: X% as Arcane (10-15% -> 40-50%) |
| 5 | Abaque de Pathaleon | 1258, trinket | The Mechanar (192) | 15% of direct hits: X% haste for 8 sec, once per 30 sec (5-8% -> 15-20%) |
| 6 | Brassards de Sepethrea | 21432, bracers | The Mechanar (192) | Spells burn the target: X% of the damage as Fire over 4 sec (8-12% -> 30-40%) |
| 7 | Ceinture d'Ingvar | 21425, belt | Utgarde Keep (242) | Every 5th direct hit, a shadow axe: X% of AP or SP (50-70% -> 180-240%) |
| 8 | Cuirasse de Keleseth | 21420, chest | Utgarde Keep (242) | Below 35% health, X% less damage taken (10-15% -> 30-40%) |
| 9 | Collier d'Annhylde | 26541, neck | Utgarde Keep (242) | A kill: X% more damage for 10 sec (5-8% -> 15-20%) |
| 10 | Poignes de Kargath | 21437, gloves | The Shattered Halls (189) | Direct damage hits 4 more enemies within 6 yd of the target for X% (10-15% -> 35-45%) |
| 11 | Bandelettes de Nethekurse | 21433, bracers | The Shattered Halls (189) | Damage over time X% stronger (8-12% -> 30-40%) |
| 12 | Chevalière de Porung | 5828, ring | The Shattered Halls (189) | X% of direct damage heals, once a second (2-3% -> 6-8%) |
| 13 | Plastron de VanCleef | 21421, chest | The Deadmines (6) | X% more damage to enemies below 35% health (8-12% -> 30-40%) |
| 14 | Ceinture à poudre de Gilnid | 21429, belt | The Deadmines (6) | Kills explode: X% of AP or SP as Fire within 8 yd (50-80% -> 200-260%) |
| 15 | Moufles de Cookie | 21444, gloves | The Deadmines (6) | Every 5 sec in combat, the most hurt ally within 40 yd healed for X% of AP or SP (50-80% -> 200-260%) |
| 16 | Bottes du roi Dred | 18161, boots | Drak'Tharon Keep (215) | Weapon blows bleed: X% of the damage over 6 sec (5-10% -> 25-35%) |
| 17 | Robe de Novos | 21430, robe | Drak'Tharon Keep (215) | X% of a direct heal's overhealing shields the target for 10 sec, up to 20% of its health (15-25% -> 50-70%) |
| 18 | Pendentif de Tharon'ja | 27218, neck | Drak'Tharon Keep (215) | Direct heals also heal the most hurt other ally within 40 yd for X% (8-12% -> 30-40%) |
| 19 | Jambières du Dévoreur | 21423, legs | The Forge of Souls (252) | Every 10 sec in combat, a well of souls for 6 sec: X% of AP or SP as Shadow a second within 6 yd (8-13% -> 35-45%) |
| 20 | Heaume de Bronjahm | 21434, helm | The Forge of Souls (252) | A kill heals X% of the health over 4 sec (2-3% -> 6-8%) |
| 21 | Anneau de l'âme reflétée | 6673, ring | The Forge of Souls (252) | X% of direct damage also hits the enemy nearest the target, within 10 yd (10-15% -> 40-50%) |
| 22 | Étincelle d'Ionar | 8688, trinket | Halls of Lightning (212) | A direct hit leaps to 3 enemies within 10 yd for X%, once per 2 sec (20-30% -> 80-100%) |
| 23 | Poings de Loken | 21450, gloves | Halls of Lightning (212) | Every 6 sec in combat, a lightning nova: X% of AP or SP within 10 yd (30-50% -> 130-170%) |
| 24 | Chevalière de Bjarngrim | 6674, ring | Halls of Lightning (212) | Dropping below 50% health: a shield of X% of the health for 10 sec, once per minute (10-15% -> 30-40%) |
| 25 | Écho du Néant (**Unique**: quality 6, red) | 10555, ring | The Hollow Voice (Archbishop Aldric's death, 930100), item level 477 | An ability with a cooldown of 20 sec or more echoes: every other ability's remaining cooldown is cut by X% (20-30%) |
| 26 | L'Étoile captive | 16067, trinket (a ring's free Item.dbc row made a trinket) | L'Infini's death (930000), at its gear's item level for the Défi's tier (370 at Défi I, +10 a tier) | 15-20% of the damage and healing done feeds a star; every 20 sec in combat it collapses: the damage shared by the target and the enemies within 8 yd of it, the healing by the 5 most hurt allies within 40 yd |

Windows read "+2 -> +60": the bottom one where a legendary drops lowest, the top one at +60 and in raids. Legendaries
4-24 are misc armour (every class wears them, their armour rolled for the looter's type), rings, necks and trinkets:
Item.dbc rows with no template of their own, given one by `localTools/legendary/buildLegendaryItemSql.py` (copied
from the item whose look they wear, a look their own dungeon's bosses drop in that slot) and their look and icon by
`localTools/patchSinisterStrike.ps1`. Each has one painted icon, its item's and its power's alike, and no frames.

Each slot has its stat budget (`Budget`): the medians of the item level 277 epics of that slot, for each kind of
wearer - strength or agility with as much stamina (agility adds attack power, as stock agility gear does), intellect
with as much stamina and spell power - and two secondaries. A legendary has 10% more than that (`LegendaryPremium`), its armour the
slot's, and the sockets a stock epic of its slot has, with a stamina socket bonus (the base items' templates,
`localTools/legendary/buildLegendaryItemSql.py`).

Every copy gets the gear bonuses (health, fortune, leech...) a dropped item does (mod-stat-growth
`TryRollPersonalLoot`), rolled at the copy's own item level (`OnItemLevel`), as a raid item's are at its variant's.
## Gardien-chef Vorhan's sets (`SetPieces.cpp`)

Not legendaries: generated items, the raid's own kind. A piece is a top tier raid item of its slot and armour type
(mod-stat-growth `IsMythicTopBaseItem`, the bases whose variants go up the whole ladder) grown to item level 485 as
the raid and Mythic+ loot is (`GrowMythicItem`), in the set's own row: its name, look, icon and set come from the row
(`buildLegendaryItemSql.py`), its stats, armour, sockets, socket bonus, durability and "Heroic" line from the raid
item. Everything the client does with an item (tooltip, comparison, character sheet, item level, bags) reads that
record, as for any raid item; its gear bonuses roll at 485 on the regular path.

- Each raid item a row can be made from is a profile, one entry each: `Mythic::GetSetPieceItemEntry(row, profile)`,
  blocks 140-171 (the client extension's `GeneratedItems.cpp` draws them; `MythicItemTag.lua` names the set on the
  tooltip's second line). Profiles are numbered once and kept in `legendary_set_profile` (characters): a raid item
  added later takes the next free number, an item already dropped never changes.
- His win (`GiveWardenVorhanLootItem`) draws one of the looter's armour type's eight pieces or a shared one, then the
  profile that suits them as a Mythic+ reward is chosen (mod-stat-growth `SelectSuitedItem`), of the primary stat
  their gear favours first; thrown on the floor with his loot.
- Copies the legendary engine rolled before (`character_legendary` on a row) are turned into the nearest profile at
  startup (their owner's class, primary stat and secondaries); their gear bonuses stay, kept by the item's guid.
- `.setpiece list <row>` shows a row's profiles beside the Hollow Voice's 477 item of the same raid item;
  `.setpiece give [row]` gives one as his win would.

### Powers

A power is one of a handful of mechanics (`Kind`, Legendary.h), written once and taken by any number of legendaries
with their own spells and numbers (`Tuning`). Its rolled value is always a percentage:

- on direct damage dealt (`ModifyFinalDamage`: swings and spell hits, never periodic damage): brands (a burn of the
  share over the spell's duration, rolled into the next by the core's Ignite; any blow, weapon blows or spells),
  echoes (every Nth hit), cleaves, chains (with a cooldown or none), leech (healed once a second), a haste surge, and
  the amplifiers - execute and a kill's frenzy (also on damage over time);
- on damage taken: the oath and the bulwark (`OnDamage`, any blow), thorns on melee swings and the last stand
  (`ModifyFinalDamage`, `ModifyPeriodicDamageAurasTick`);
- on direct heals (`ModifyHealReceived`; a heal-over-time tick is marked on its way through
  `ModifyPeriodicDamageAurasTick` and left out): the overhealing shield and the heal splash;
- on kills, the wearer's or their pet's: the frenzy, the explosion, the soul's heal;
- on a timer in combat (`OnPlayerUpdate`): grounds (a persistent area laid and left, pulsing every second), novas and
  a heal for the most hurt ally.

Every one of them is a spell of the wearer's (`localTools/legendary/Spells.ps1`: 97000-97005 and 97700-97999, the
Barbarian's between), so the combat log and Details credit the wearer with it on a line of its own; none of them
feeds a power again. Their looks are their own dungeon's, mostly their boss's own spell visual (Loken's Lightning
Nova, the Devourer's Mirrored Soul, Cookie's Cooking...).

The Unique is quality 6, the stock client's unused "Artifact": the client extension DLL recolours it red
(awesome_wotlk `UniqueQuality.cpp`) and `Legendary.lua` names it "Unique". It drops from a boss's death
(`sourceBoss`, `LegendaryBossDropScript`) with the same luck rules, kept by the boss's entry. Its echo hooks the cast
(`OnPlayerSpellCast`: the player's own casts, not triggered, not an item's) and cuts the cooldowns with
`Player::ModifySpellCooldown`, which tells the client.

A copy also counts at its own item level for the server's average (`GLOBALHOOK_ON_ITEM_LEVEL`, called by
`Player::GetAverageItemLevel` and `GetAverageItemLevelForDF`), and shows it: in its tooltip, in place of the base
item's line, in the comparison (rewritten from the copies' rolls as the client's
`GameTooltip_ShowCompareItem` returns), and on DragonUI's item level texts (`clientPatcher/addons/DragonUI/modules/itemlevel.lua`).

## Client

- `FrameXML/Legendary.lua`: the copy's rolls in its tooltip. The server whispers them (`LEGENDARY` prefix); the client
  asks for a copy by where it sits (bag and slot: worn, bags, bank) - the 3.3.5 client leaves an item's property seed
  out of its links, so a link alone does not say which copy it is (a copy linked by someone else shows its base item).
- `FrameXML/LegendaryFrames.lua`: each legendary's painted item frame and tooltip frame (art in
  `clientPatcher/assets/<legendary>`, exported by `localTools/interface/buildLegendaryClientArt.py`).

## Commands

`.legendary add <legendary> [item level] [power %]` (GM): a rolled copy for the selected player or yourself.
`.legendary all [item level] [power %]` (GM): a copy of every legendary (a test kit; it stops when the bags are full).

## Drops

A legendary belongs to one source: the Dungeon Finder dungeon whose Mythic+ keys drop it (`sourceDungeon`); a source
may hold several, one drawn per drop. When a key of a source is completed, every real player in it rolls once:
`legendary.drop_base_pct` (2%), plus `legendary.drop_step_pct` (0.5%) for each key of that source completed without
one, never above `legendary.drop_cap_pct` (10%) - live knobs (`.tune`). The count of dry keys is kept per character and
source (`character_legendary_luck`) and reset by a drop. The copy lands on the floor with the key's loot (GroundLoot),
at the key's item level, and is rolled as it reaches the bags.
