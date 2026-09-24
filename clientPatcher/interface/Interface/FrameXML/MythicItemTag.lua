-- The Mythique+ tag on a piece of loot, the way a raid's own tag names its difficulty.
--
-- Nothing has to be asked of the server: a Mythic+ reward is recognisable from its item id alone. The
-- generator builds these entries as GeneratedItemBase * (variant + 1) + base entry, with every real item
-- below GeneratedItemBase, so anything at or above that number came out of a key
-- (src/server/game/Maps/MythicDungeon.h, and awesome_wotlk's GeneratedItems.cpp, which draws them).
-- Both sides already agree on these numbers; this reads them and says nothing the id does not already say.
--
-- The line is appended rather than slipped under the item's name. "Heroic" sits there because the client
-- draws it itself from a flag on the item, and a flag cannot carry different words for different items -
-- borrowing it would relabel every heroic raid drop in the game.

local GENERATED_ITEM_BASE = 0x10000     -- MythicDungeon.h, GeneratedItemBase

local french = GetLocale() == "frFR"
local TAG = french and "Mythique+" or "Mythic+"

-- The same green the client uses for its own difficulty tag
local TAG_R, TAG_G, TAG_B = 0.1, 1.0, 0.1

local function IsMythicItem(link)
    if not link then
        return false
    end

    local id = tonumber(link:match("item:(%d+)"))
    return id ~= nil and id >= GENERATED_ITEM_BASE
end

local function Tag(tooltip)
    -- Guarded: a tooltip that has already been tagged this time round must not collect the line twice, which
    -- happens because several of these fire OnTooltipSetItem more than once for one hover.
    if tooltip.mythicTagged then
        return
    end

    local _, link = tooltip:GetItem()
    if not IsMythicItem(link) then
        return
    end

    tooltip.mythicTagged = true
    tooltip:AddLine(TAG, TAG_R, TAG_G, TAG_B)
    tooltip:Show()
end

local function Untag(tooltip)
    tooltip.mythicTagged = nil
end

-- Every tooltip that can show an item: the one under the cursor, a link clicked in chat, and the two the
-- client uses to compare against what is already worn.
for _, name in ipairs({ "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2" }) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Tag)
        -- Cleared, not hidden: a tooltip is reused for the next item without ever going away, so hooking
        -- OnHide would leave the guard set and the tag would go missing on everything hovered after it.
        tooltip:HookScript("OnTooltipCleared", Untag)
    end
end
