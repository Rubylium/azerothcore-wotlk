# Legendaries

Diablo-style legendaries (`modules/mod-legendary`, its README for what exists and how a copy rolls). Each copy rolls
its own item level, power strength and stats; a power is one shared mechanic (`Kind`) with its own numbers. A new
legendary is mostly data: a definition, a spell, an Item.dbc row's look, two client texts, one painted icon.

## Adding a legendary (checklist)

1. **Base item** (`Definition::baseItem`): an id below 65536 that the client's Item.dbc already has but
   `item_template` does not (the custom item ids rule: the server enforces that row's class, subclass and slot).
   Pick a row of the slot wanted; misc armour (subclass 0) lets every class wear it, its armour rolled for the
   looter's type.
2. **Definition** (`Legendary.cpp`, `Definitions`, and its `std::array` size): the next id, the base item, a `Kind`,
   the power window (bottom low/high at `Floor`, top low/high at 370 and Mythic+ +60), the slot's `Budget`
   (`HeadBudget`, `RingBudget`, ...: never hand-made stats), its source - `sourceDungeon` (a Dungeon Finder dungeon of
   the Mythic+ pool, three legendaries each) or `sourceBoss` (the creature entry whose death drops it) - and the
   `Tuning` (spells, period, targets, radius, chance, cooldown). A comment above it in the file's style: name, slot,
   base item, the power in one line with both windows.
   - Reuse a `Kind` (the README's Powers list, `Legendary.h`). A new mechanic is a new `Kind`, written once in
     `Legendary.cpp` on the hook its family uses (damage dealt, damage taken, heals, kills, the combat timer).
   - Size the numbers on the combat bench (`combat-bench.md`), not by feel; the drop item level follows
     `power-scaling.md`.
3. **Its spell(s)** (`localTools/legendary/Spells.ps1`, the `New-Damage` / `New-Heal` / `New-Burn` / `New-Shield` /
   `New-Mark` helpers): ids in 97700-97999, the next free ten (the last used is 97910; the Barbarian owns the range
   between 97005 and 97700). The wearer casts it, so Details and the combat log credit it on a line of its own. Its
   look is the source's own (the boss's spell visual) where one fits. Never let it feed a power again.
4. **The item's look** (`localTools/patchSinisterStrike.ps1`, `$legendaryItems`): the base item, the next display id
   (71025 on; 71024 is the last), the look it clones (an item its own dungeon's bosses drop in that slot) and its icon
   name.
5. **The template** (`localTools/legendary/buildLegendaryItemSql.py`, `LEGENDARIES`): the base item, the look's item,
   its French and English names; run it to regenerate `modules/mod-legendary/data/sql/db-world/base/legendary_items.sql`
   (between its markers: never by hand).
6. **The client texts** (`clientPatcher/interface/Interface/FrameXML/Legendary.lua`): `Add(id, item, DUNGEON, power,
   lore)` (or `AddUnique(id, item, BOSS, ...)`), French first then English; the power's `%s` is the rolled value. A
   new source needs its `{ French, English }` name constant.
7. **The icon**: below. Until it is painted the item shows its look's own icon.
8. **Deploy**: `localTools/deployWithProgress.ps1 -steps server,client,restart,publish` (the spells and the look are
   client data too). Test with `.legendary add <id> [item level] [power %]` or `.legendary all`.

A **Unique** (quality 6, red; the client extension DLL recolours it) is the same steps with its base item in the
generator's `UNIQUE` set, a `sourceBoss`, `AddUnique` on the client and an `INV_Unique_` icon: keep it rare (one per
pinnacle boss).

## Icons: the art direction

One painted icon per legendary, shared by the item and its power (its buff, its Details line). **No item frame and
no tooltip frame**: those stay for a few chosen items, decided with the user.

- **Delivery**: 256 x 256, square, opaque PNG (no transparency, border, frame or text), saved as
  `clientPatcher/assets/legendaryIcons/png/INV_Legendary_<NameWithoutAccents>.png` (`INV_Unique_<Name>.png` for a
  Unique). Add the name to `ICON_ONLY_NAMES` in `localTools/interface/buildLegendaryClientArt.py`, run it (64 x 64
  TGAs in `modules/mod-stat-growth/client-assets/compiled/`, committed with the PNG), then the client build.
- **Readability**: drawn at 64 px and smaller in game: one subject, a strong silhouette, readable at 32 px.
- **Style prefix**, pasted before every prompt, with a few stock Wrath icons attached as references (e.g.
  `INV_Jewelry_Ring_83`, `INV_Shoulder_105`, `INV_Misc_Bone_ElfSkull_01`, `Spell_Shadow_SoulLeech_3`; the decoded
  ones are in `clientPatcher/assets/legendaryIcons/reference/`):

  > A World of Warcraft: Wrath of the Lich King item icon, square, hand-painted in Blizzard's icon style: the subject
  > filling the frame at a slight angle, chunky readable shapes, painterly texture, dramatic light from the upper
  > left, rich saturated colour, a dark vignette at the edges, a faint legendary orange glow around the subject. No
  > border, no frame, no text. 256 x 256.

- **The subject**: the item itself (its slot read at a glance: a ring, a pair of bracers), in its source's materials
  and colours (saronite and pale souls for the Forge of Souls, titan bronze and blue lightning for Halls of
  Lightning), with its power shown on it (a fire burst, a shield bubble, lightning arcs). One or two sentences.
- **A Unique**: the same prefix, a crimson red rim light in place of the orange glow, a deep red-black background.
- Every prompt so far, as examples: `clientPatcher/assets/legendaryIcons/generationManifest.json` and the
  contact sheet beside it. Painted art is used at its own size and aspect only, never stretched.
