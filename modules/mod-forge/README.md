# mod-forge

The Forge: high-end gear (epics and legendaries of item level 200 and up, and every Mythic+ item) brought to the
blacksmith comes back 4 item levels higher for gold, up to 8 times. Open it with `.forge` (the client's
ItemForge.lua).

A real item's ranks are generated at startup like the Mythic+ variants, with entries shared with the client
extension (src/server/game/Maps/MythicDungeon.h, awesome_wotlk GeneratedItems.cpp): the client draws a forged item
with its base item's look. A forged Mythic+ item becomes the next Mythic+ variant; its rank is kept in the
characters table character_item_forge.

Armour cosmetics use a brief, feet-only ember trail while moving on foot outside combat. Starting at 24 worn
armour/jewellery ranks, the effect lasts at most 1.2 seconds, with 30-45 seconds between appearances; 72 ranks
reduce the interval to 25-35 seconds and 104 ranks to 20-30 seconds. It stops immediately when idle, mounted,
flying, swimming, stealthed or invisible. Legacy full-body auras are removed on login and equipment refresh.
Weapon enchantment glows are unchanged. The trail reuses the shipped client effect, so no client update is needed.
