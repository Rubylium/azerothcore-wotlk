local api = EvolutionsItemFrames
local bound = setmetatable({}, { __mode = "k" })

function api.bagSource(bag, slot)
    if bag == 255 then return "SetInventoryItem", "player", slot end
    if bag == -1 or bag == 254 then
        return "SetInventoryItem", "player", BankButtonIDToInvSlotID(slot)
    end
    if GetContainerItemLink(bag, slot) then return "SetBagItem", bag, slot end
end

function api.bindServerSlot(button, bag, slot, icon)
    api.bind(button, function()
        if bag == 255 then
            if slot < 19 then return "SetInventoryItem", "player", slot + 1 end
            return api.bagSource(0, slot - 23 + 1)
        end
        return api.bagSource(bag - 19 + 1, slot + 1)
    end, icon)
end

local function adapt(button)
    if not button or bound[button] then return end
    local name = button:GetName() or ""
    local source
    if name:match("^DragonUI_BagsterGuildItem") then
        source = function(self)
            local tab, slot = self:GetSlot()
            if GetGuildBankItemLink(tab, slot) then return "SetGuildBankItem", tab, slot end
        end
    elseif name:match("^DragonUI_BagsterItem%d+$") then
        source = function(self)
            -- Cached characters/banks have only template links, not the instance's enchantments.
            if not self:IsCached() then return api.bagSource(self:GetBag(), self:GetID()) end
        end
    elseif name:match("^ContainerFrame%d+Item%d+$") then
        source = function(self) return api.bagSource(self:GetParent():GetID(), self:GetID()) end
    elseif name:match("^BankFrameItem%d+$") then
        source = function(self) return api.bagSource(-1, self:GetID()) end
    elseif name:match("^Character.+Slot$") or name:match("^Inspect.+Slot$") then
        source = function(self)
            local unit = name:match("^Inspect") and (InspectFrame.unit or "target") or "player"
            local slot = self:GetID()
            if GetInventoryItemLink(unit, slot) then return "SetInventoryItem", unit, slot end
        end
    elseif name:match("^LootButton%d+$") then
        source = function(self)
            local slot = self.slot or self:GetID()
            if GetLootSlotLink(slot) then return "SetLootItem", slot end
        end
    elseif name:match("^GroupLootFrame%d+$") then
        source = function(self)
            if self.rollID then return "SetLootRollItem", self.rollID end
        end
    elseif name:match("^TradePlayerItem%d+ItemButton$") then
        source = function()
            local slot = tonumber(name:match("Item(%d+)"))
            if GetTradePlayerItemLink(slot) then return "SetTradePlayerItem", slot end
        end
    elseif name:match("^TradeRecipientItem%d+ItemButton$") then
        source = function()
            local slot = tonumber(name:match("Item(%d+)"))
            if GetTradeTargetItemLink(slot) then return "SetTradeTargetItem", slot end
        end
    elseif name:match("^OpenMailAttachmentButton%d+$") then
        source = function(self)
            if InboxFrame.openMailID then return "SetInboxItem", InboxFrame.openMailID, self:GetID() end
        end
    elseif name:match("^MailItem%d+Button$") then
        source = function(self)
            if self.index then return "SetInboxItem", self.index end
        end
    elseif name:match("^SendMailAttachment%d+$") then
        source = function(self) return "SetSendMailItem", self:GetID() end
    elseif name:match("^GuildBankColumn%d+Button%d+$") then
        source = function(self)
            local slot = self:GetID()
            if GetGuildBankItemLink(GetCurrentGuildBankTab(), slot) then
                return "SetGuildBankItem", GetCurrentGuildBankTab(), slot
            end
        end
    elseif name:match("^BrowseButton%d+Item$") or name:match("^AuctionsButton%d+Item$")
        or name:match("^BidButton%d+Item$") then
        source = function(self)
            local list = name:match("^Browse") and "list" or (name:match("^Bid") and "bidder" or "owner")
            local scroll = list == "list" and BrowseScrollFrame
                or (list == "bidder" and BidScrollFrame or AuctionsScrollFrame)
            return "SetAuctionItem", list, self:GetParent():GetID() + FauxScrollFrame_GetOffset(scroll)
        end
    elseif name == "AuctionsItemButton" then
        source = function() return "SetAuctionSellItem" end
    elseif name:match("^MerchantItem%d+ItemButton$") then
        source = function(self)
            if MerchantFrame.selectedTab == 2 then return "SetBuybackItem", self:GetID() end
        end
    end
    if source then
        bound[button] = true
        local icon = button.icon or button.Icon or _G[name .. "IconTexture"] or _G[name .. "Icon"]
            or _G[name .. "IconFrameIcon"]
        api.bind(button, source, icon)
    end
end

-- Both Blizzard and DragonUI (including Bagster's pooled bag/bank/guild slots) use this entry point.
hooksecurefunc("SetItemButtonTexture", function(button)
    if button and not bound[button] then api.apply(button, nil) end
    adapt(button)
    api.refresh()
end)

local function discover()
    for index = 1, 4 do adapt(_G["GroupLootFrame" .. index]) end
    local frame = EnumerateFrames()
    while frame do
        if frame:GetObjectType() == "Button" or frame:GetObjectType() == "CheckButton" then adapt(frame) end
        frame = EnumerateFrames(frame)
    end
    api.refresh()
end
local driver = CreateFrame("Frame")
driver:RegisterEvent("PLAYER_ENTERING_WORLD")
driver:RegisterEvent("ADDON_LOADED")
local updateHooks = {}
driver:SetScript("OnEvent", function()
    discover()
    for _, name in ipairs({ "AuctionFrameBrowse_Update", "AuctionFrameBid_Update", "AuctionFrameAuctions_Update",
        "InboxFrame_Update", "OpenMail_Update", "SendMailFrame_Update", "MerchantFrame_Update",
        "PaperDollItemSlotButton_Update", "InspectPaperDollItemSlotButton_Update", "LootFrame_Update" }) do
        if _G[name] and not updateHooks[name] then
            updateHooks[name] = true
            hooksecurefunc(name, api.refresh)
        end
    end
end)

if ActionButton_Update then
    hooksecurefunc("ActionButton_Update", function(button)
        if not button or bound[button] then return end
        bound[button] = true
        api.bind(button, function(self)
            local action = self.action
            if action and GetActionInfo(action) == "item" then return "SetAction", action end
        end, _G[(button:GetName() or "") .. "Icon"])
    end)
end

-- Hover also decorates item icons supplied by later addons without invoking their OnEnter handlers.
-- Only a real item tooltip with a recognized proc can opt into a frame.
GameTooltip:HookScript("OnTooltipSetItem", function(tooltip)
    local owner = tooltip:GetOwner()
    if not owner or owner == UIParent then return end
    adapt(owner)
    local name = owner:GetName() or ""
    local icon = owner.icon or _G[name .. "IconTexture"]
    if icon and not bound[owner] then api.apply(owner, api.resolveTooltip(tooltip), icon) end
end)
