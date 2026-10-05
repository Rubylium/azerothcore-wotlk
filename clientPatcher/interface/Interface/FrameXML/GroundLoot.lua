-- A boss's loot on the floor (mod-stat-growth GroundLoot.cpp): each drop is a bag (or a pile of gold) only its owner
-- sees, and hovering it shows what it holds - the item's own tooltip, or the gold - instead of the bag's name. The
-- server tells what each drop holds as it leaves the corpse, and forgets it once picked up:
--   GLOOT <tab> <drop guid, 16 hex digits> <tab> item:<id>:0:0:0:0:0:<random property> | gold:<copper> | -
local PREFIX = "GLOOT"

local L = GetLocale() == "frFR" and {
    gold = "Or",
    hint = "Marchez dessus pour le ramasser.",
} or {
    gold = "Gold",
    hint = "Walk over it to pick it up.",
}

local drops = {}

-- Asked as it is announced: an item the client has never seen would show an empty tooltip on the first hover
local query = CreateFrame("GameTooltip", "GroundLootQueryTooltip", UIParent, "GameTooltipTemplate")
query:SetOwner(UIParent, "ANCHOR_NONE")

local function Key(guid)
    return guid and string.upper(string.gsub(guid, "^0x", "")) or nil
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:SetScript("OnEvent", function(self, event, prefix, message)
    if event == "PLAYER_ENTERING_WORLD" then
        drops = {}
        return
    end
    if prefix ~= PREFIX or not message then
        return
    end
    local guid, what = string.match(message, "^(%x+)\t(.+)$")
    if not guid then
        return
    end
    guid = string.upper(guid)
    if what == "-" then
        drops[guid] = nil
        return
    end
    drops[guid] = what
    if string.find(what, "^item:") then
        query:SetOwner(UIParent, "ANCHOR_NONE")
        query:SetHyperlink(what)
        query:Hide()
    end
end)

local showing
GameTooltip:HookScript("OnTooltipSetUnit", function(tooltip)
    if showing then
        return
    end
    local _, unit = tooltip:GetUnit()
    local what = drops[Key(UnitGUID(unit or "mouseover"))]
    if not what then
        return
    end
    showing = true
    local copper = string.match(what, "^gold:(%d+)$")
    if copper then
        tooltip:ClearLines()
        tooltip:AddLine(L.gold, 1, 0.82, 0)
        SetTooltipMoney(tooltip, tonumber(copper))
    else
        tooltip:SetHyperlink(what)
    end
    tooltip:AddLine(L.hint, 0.6, 0.6, 0.6)
    tooltip:Show()
    showing = nil
end)
