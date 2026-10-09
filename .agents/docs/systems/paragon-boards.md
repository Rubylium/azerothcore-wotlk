# Paragon boards: the planner, recommended boards and loadouts

The board (`modules/mod-stat-growth/src/ParagonSystem.cpp`, data from `localTools/paragon/buildParagonTree.py`) has six
branches (sides 0-5: Force, Puissance, Agilité, Carapace, Arcanes, Intellect), each in zones: Éveil, Ascension,
Transcendance, then the Panthéon. A zone opens only once the previous one is whole (`MissingForTier`), so a planner
can change only the order inside a zone. **Which branch comes second is the choice that counts.**

## One planner for bots and players

- `PlanBotBoard(role, budget, content)` walks the board: the branches in `SidesFor(role, content)` order, each zone
  by `NodeWeight` (`RoleUsefulness` x `ContentFactor`). `GetBotPlan` caches the walk by (plan key, role, budget). The
  plan key (`BotPlanKey`) holds the branch order and the content, so moving a knob plans again.
- Roles come from `RoleOf`: tank, healer, caster, agility (rogue, hunter, feral, enhancement), strength (the rest,
  custom classes by their base class).
- **Bots** plan for `paragon.bot_content` (0 = any, the live default; the spec balance was measured on it).
  **Players** get the same walk for raid (1) or Mythic+ (2) from the window (`PRESET\t1|2`, `ApplyPreset`), at
  `min(earned, cap)` points: it adapts to their points by construction.
- Branch orders are live knobs, three digits from 1 Force to 6 Intellect: `paragon.sides.<role>[.raid|.mythic]`.

## Measuring a branch order (simulation bench, never by hand)

Set the knobs on every worker, then sweep each content on its layouts:

```powershell
simBench.ps1 cmd -worker <n> '.tune set paragon.bot_content 2' '.tune set paragon.sides.agility.mythic 231'
simBench.ps1 sweep -copies 4 -profiles '340:300:10:pack5+pack12,460:650:defi10-25:pack5+pack12'   # mythic
simBench.ps1 sweep -copies 4 -profiles '340:300:10:boss,460:650:defi10-25:boss'                   # raid
```

Compare each spec's absolute DPS between orders, grouped by the rows' `Board` role (not against the mage, whose
own board moves with the caster order). 2026-10-09 (6 sweeps, ~1 min each): every other first or second branch
cost a role 6 to 80% of its damage. The one exception: Puissance first gave agility 2 to 8% more on packs, so
`agility.mythic` is 231.

## Applying a whole board (`ApplyBoard`)

Used for a recommended board and a loadout alike. It clears the board, then takes nodes in passes under a click's
rules (cost, required points, zone gate, reachability) until none more fits, then puts the glyphs back into their
sockets. Everything (DELETE, INSERTs, glyph rows) goes into **one transaction**: separate async writes could run
out of order and leave a cleared board in the database. Refused in combat; free, as a talent switch is.

## Loadouts (as the talent tree keeps its own)

- Table `character_paragon_loadout` (guid, slot 1-10, name of 32 characters at most, nodes and `glyph:socket` as
  comma lists). Loaded with the board.
- Protocol (addon prefix `Paragon`): client `LSAVE\t<slot>\t<name>`, `LDEL\t<slot>`, `LAPPLY\t<slot>`; server
  `LC` then `L\t<slot>\t<points>\t<name>` with every state (before `DONE`), `LSAVED\t<slot>`,
  `LA\t<slot>\t<spent>\t<total>`, `PRESETDONE\t<content>`.
- A loadout saved with more points than the player now has is applied as far as the points go (`LA` gives both).
- UI (`clientPatcher/interface/Interface/FrameXML/Paragon.lua`): a `UIDropDownMenu` (`ParagonLoadoutPicker`) beside
  Réinitialiser, with Enregistrer and Supprimer. The bottom row is full up to Glyphes: a new control needs room
  taken from elsewhere. The picker is 128 wide; longer text is cut off ("Recommandé : Mythique+" was).
- Window check: `shoot.ps1 -steps localTools/fxLab/shots/paragonLoadouts.lua`. It saves the
  dev character's board to slot 9, applies both recommended boards, then puts the board back and deletes slot 9.
