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

The brand (spell 97000, `localTools/legendary/Spells.ps1`) is a real periodic aura cast by the wearer, so the combat
log and Details credit them with it on its own line. Only direct damage feeds it (`ModifyFinalDamage`: swings and
spell hits, never periodic damage); what is left of the burn rolls into the new one (the core's Ignite).

## Client

- `FrameXML/Legendary.lua`: the copy's rolls in its tooltip. The server whispers them (`LEGENDARY` prefix); the client
  asks for a copy by where it sits (bag and slot: worn, bags, bank) - the 3.3.5 client leaves an item's property seed
  out of its links, so a link alone does not say which copy it is (a copy linked by someone else shows its base item).
- `FrameXML/LegendaryFrames.lua`: each legendary's painted item frame and tooltip frame (art in
  `clientPatcher/assets/<legendary>`, exported by `localTools/interface/buildLegendaryClientArt.py`).

## Commands

`.legendary add <legendary> [item level] [power %]` (GM): a rolled copy for the selected player or yourself.

## Drops

A legendary belongs to one source: the Dungeon Finder dungeon whose Mythic+ keys drop it (`sourceDungeon`); a source
may hold several, one drawn per drop. When a key of a source is completed, every real player in it rolls once:
`legendary.drop_base_pct` (2%), plus `legendary.drop_step_pct` (0.5%) for each key of that source completed without
one, never above `legendary.drop_cap_pct` (3%) - live knobs (`.tune`). The count of dry keys is kept per character and
source (`character_legendary_luck`) and reset by a drop. The copy lands on the floor with the key's loot (GroundLoot),
at the key's item level, and is rolled as it reaches the bags.
