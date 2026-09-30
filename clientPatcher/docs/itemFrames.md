# Item frames

`ItemFrames.lua`, `ItemFramesInfinite.lua` and `ItemFramesAdapters.lua` load from FrameXML in the interface MPQ.
They work with addons disabled and with the DragonUI version bundled by `Build-FriendPatch.ps1`.

L'Infini's style uses the instance tooltip's proc name, matching the existing Mythic item tag. It never infers a
proc from an item entry, item level, icon, or a shared hyperlink. Existing proc items work without a database
migration. Native tooltip contexts preserve the distinction between otherwise identical items.

The adapters cover bags, bank, character/inspect slots, loot/rolls, mail attachments, trade, guild bank, auction
rows/sell slot, buyback, item actions, DragonUI Bagster and the Forge list/anvil. Frames refresh after inventory
and UI changes, including reused addon buttons. Template-only previews and DragonUI's offline character cache
cannot prove instance procs; they are intentionally not classified from a template link.

## Adding a style

Register a unique key with `EvolutionsItemFrames.registerStyle(key, { create = function(parent) ... end })`.
The factory returns a noninteractive frame anchored to its parent. Register a tooltip classifier with
`registerResolver(function(tooltip) ... return key end)`. Resolvers run in registration order; the first known
style wins. Keep classification separate from art and source adapters.

For a new item widget, call `bind(button, source, iconTexture)` once. `source(button)` returns the native tooltip
method and up to two arguments, for example `"SetBagItem", bag, slot`; return nil for an empty/unknown slot.
The callback must read the widget's current slot because addon buttons are commonly recycled. `refresh()` queues
a batched refresh. `bindServerSlot(button, bag, slot, iconTexture)` accepts the Forge's server inventory numbering.

No item-button scripts or protected attributes are replaced. The service owns a separate mouse-transparent
overlay. Styling does not alter rarity, stats, cooldowns or saved items.

Validation: `lua clientPatcher/tests/itemFrames.test.lua` from the repository root.
