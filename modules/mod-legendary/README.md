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
| 1 | Marque de l'Inquisiteur | 24567, cloak | Scarlet Cathedral, Mythic+ | Direct damage brands the target: X% of it burns as Holy over 4 sec (5-10% at the floor, 25-35% at +60) |

The brand (spell 97000, `localTools/legendary/Spells.ps1`) is a real periodic aura cast by the wearer, so the combat
log and Details credit them with it on its own line. Only direct damage feeds it (`ModifyFinalDamage`: swings and
spell hits, never periodic damage); what is left of the burn rolls into the new one (the core's Ignite).

## Client

- `FrameXML/Legendary.lua`: the copy's rolls in its tooltip. The server whispers them (`LEGENDARY` prefix) by the
  copy's id - the item's property seed, which item links carry as their unique id - and the client asks for any copy
  it does not know (a link, another player's, the loot).
- `FrameXML/LegendaryFrames.lua`: each legendary's painted item frame and tooltip frame (art in
  `clientPatcher/assets/<legendary>`, exported by `localTools/interface/buildLegendaryClientArt.py`).

## Commands

`.legendary add <legendary> [item level] [power %]` (GM): a rolled copy for the selected player or yourself.
