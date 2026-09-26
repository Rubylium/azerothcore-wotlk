# mod-forge

The Forge: high-end gear (epics and legendaries of item level 200 and up, and every Mythic+ item) brought to the
blacksmith comes back 4 item levels higher for gold, up to 8 times. Open it with `.forge` (the client's
ItemForge.lua).

A real item's ranks are generated at startup like the Mythic+ variants, with entries shared with the client
extension (src/server/game/Maps/MythicDungeon.h, awesome_wotlk GeneratedItems.cpp): the client draws a forged item
with its base item's look. A forged Mythic+ item becomes the next Mythic+ variant; its rank is kept in the
characters table character_item_forge.
