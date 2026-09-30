-- Shared item decoration service. Resolvers inspect an instance tooltip, never an item ID:
-- two copies of the same generated item can carry different raid procs.
EvolutionsItemFrames = { styles = {}, resolvers = {} }
local api = EvolutionsItemFrames
local tracked = setmetatable({}, { __mode = "k" })
local scan = CreateFrame("GameTooltip", "EvolutionsItemFrameScan", UIParent, "GameTooltipTemplate")
local driver = CreateFrame("Frame")
local pending, elapsed = true, 0

function api.registerStyle(key, style)
    api.styles[key] = style
    pending = true
end

function api.registerResolver(resolve)
    table.insert(api.resolvers, resolve)
    pending = true
end

function api.resolveTooltip(tooltip)
    for _, resolve in ipairs(api.resolvers) do
        local key = resolve(tooltip)
        if key and api.styles[key] then return key end
    end
end

function api.apply(button, key, icon)
    local effect = button.evolutionsItemFrame
    local style = key and api.styles[key]
    if not style then
        if effect then effect:Hide() end
        return
    end
    if not effect then
        effect = CreateFrame("Frame", nil, button)
        effect:EnableMouse(false)
        effect.styles = {}
        button.evolutionsItemFrame = effect
    end
    if effect.styleKey ~= key then
        if effect.art then effect.art:Hide() end
        effect.styles[key] = effect.styles[key] or style.create(effect)
        effect.art = effect.styles[key]
        effect.art:Show()
        effect.styleKey = key
    end
    effect:ClearAllPoints()
    effect:SetAllPoints(icon or button)
    -- Above addon rarity glows; no changes to the button's scripts, texture or protected attributes.
    effect:SetFrameLevel(button:GetFrameLevel() + 8)
    effect:Show()
end

-- A source returns a native GameTooltip setter and its arguments, or nil for an empty/unknown slot.
-- It is evaluated afresh after slot changes, so recycled buttons cannot retain another item's frame.
function api.bind(button, source, icon)
    if not button then return end
    local fresh = not tracked[button]
    tracked[button] = { source = source, icon = icon }
    api.apply(button, nil)
    pending = true
    if fresh then
        button:HookScript("OnShow", function() pending = true end)
    end
end

function api.refresh()
    pending = true
end

local function update(button, binding)
    local method, a, b = binding.source(button)
    local key
    if method and scan[method] then
        scan:SetOwner(UIParent, "ANCHOR_NONE")
        scan:ClearLines()
        scan[method](scan, a, b)
        key = api.resolveTooltip(scan)
        scan:Hide()
    end
    api.apply(button, key, binding.icon)
end

driver:SetScript("OnUpdate", function(_, delta)
    elapsed = elapsed + delta
    if elapsed < 0.15 or not pending then return end
    elapsed, pending = 0, false
    for button, binding in pairs(tracked) do
        if button:IsShown() then update(button, binding) end
    end
end)
for _, event in ipairs({ "BAG_UPDATE", "UNIT_INVENTORY_CHANGED", "PLAYER_EQUIPMENT_CHANGED",
    "PLAYER_ENTERING_WORLD", "BANKFRAME_OPENED", "PLAYERBANKSLOTS_CHANGED", "MAIL_INBOX_UPDATE",
    "TRADE_UPDATE", "TRADE_PLAYER_ITEM_CHANGED", "TRADE_TARGET_ITEM_CHANGED", "GUILDBANKBAGSLOTS_CHANGED",
    "AUCTION_ITEM_LIST_UPDATE", "AUCTION_OWNED_LIST_UPDATE", "AUCTION_BIDDER_LIST_UPDATE",
    "INSPECT_TALENT_READY", "GET_ITEM_INFO_RECEIVED", "LOOT_OPENED", "LOOT_SLOT_CLEARED",
    "ACTIONBAR_SLOT_CHANGED", "UPDATE_INVENTORY_ALERTS" }) do
    driver:RegisterEvent(event)
end
driver:SetScript("OnEvent", api.refresh)
