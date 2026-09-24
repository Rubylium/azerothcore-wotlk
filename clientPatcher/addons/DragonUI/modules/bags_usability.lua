-- Copyright (c) 2026 NeticSoul. Licensed under the MIT License; see LICENSE.
--
-- Evolutions: replaces DragonUI's modules/bags_usability.lua (clientPatcher/Build-FriendPatch.ps1 ships it over the
-- client's copy). The original decided armor and weapon proficiency from hardcoded stock class tables: warriors
-- without mail before level 40, and every custom class (Oathblade, Pestiféré, ...) unknown, so all their gear was
-- tinted red. The tooltip is the truth here: the server resends the proficiencies from the character's real skills
-- (modules/mod-custom-classes/src/ProficiencySync.cpp), and the client reddens exactly what cannot be worn.

local addon = select(2, ...)

-- ============================================================================
-- BAG ITEM USABILITY TINT
-- ============================================================================
-- Not IsUsableItem: that also reports false for usable items on cooldown or out of range.

local unusableTintCache = {}
local scanTip, scanTipName

-- Level/skill/class/race/reputation/proficiency, equippable or not. Returns nil if tooltip empty (uncached).
local function TooltipHasRedRequirement(link, bag, slot)
    if not scanTip then
        scanTip = CreateFrame("GameTooltip", "DragonUIUnusableScanTip", nil, "GameTooltipTemplate")
        scanTipName = scanTip:GetName()
    end
    scanTip:SetOwner(UIParent, "ANCHOR_NONE")
    scanTip:ClearLines()
    if bag ~= nil and slot ~= nil then
        scanTip:SetBagItem(bag, slot)
    else
        scanTip:SetHyperlink(link)
    end
    local numLines = scanTip:NumLines() or 0
    if numLines < 2 then
        scanTip:Hide()
        return nil
    end
    local redCode = RED_FONT_COLOR_CODE or "|cffff2020"
    local function IsRed(fs)
        if not fs or not fs:IsShown() then return false end
        local text = fs:GetText()
        if text and text:find(redCode, 1, true) then return true end
        local r, g, b = fs:GetTextColor()
        return r and r > 0.9 and g < 0.2 and b < 0.2 or false
    end
    for i = 2, numLines do
        -- Weapon/armor subtype sits on the RIGHT of its line and is what reddens for a proficiency the class lacks.
        if IsRed(_G[scanTipName .. "TextLeft" .. i]) or IsRed(_G[scanTipName .. "TextRight" .. i]) then
            scanTip:Hide()
            return true
        end
    end
    scanTip:Hide()
    return false
end

function addon:IsUnusableItemTintEnabled()
    local bags = self.db and self.db.profile and self.db.profile.bags
    return bags and bags.tint_unusable and true or false
end

function addon:ClearUnusableItemTintCache()
    wipe(unusableTintCache)
end

function addon:IsItemUnusableForTint(link, bag, slot)
    if not link then return false end
    local itemID = link:match("item:(%d+)")
    if itemID and unusableTintCache[itemID] ~= nil then
        return unusableTintCache[itemID]
    end

    local unusable
    local cacheable = true
    local reqLevel = select(5, GetItemInfo(link))
    if reqLevel and reqLevel > UnitLevel("player") then
        unusable = true
    else
        local red = TooltipHasRedRequirement(link, bag, slot)
        if red == nil then
            cacheable = false
            unusable = false
        else
            unusable = red
        end
    end

    if itemID and cacheable then
        unusableTintCache[itemID] = unusable
    end
    return unusable
end

function addon:RefreshUnusableItemTints()
    wipe(unusableTintCache)
    for i = 1, (NUM_CONTAINER_FRAMES or 13) do
        local frame = _G["ContainerFrame" .. i]
        if frame and frame:IsShown() and ContainerFrame_Update then
            ContainerFrame_Update(frame)
        end
    end
    if BankFrame and BankFrame:IsShown() and BankFrameItemButton_Update then
        for i = 1, 28 do
            local button = _G["BankFrameItem" .. i]
            if button then BankFrameItemButton_Update(button) end
        end
    end
    -- addon.BagsterModule.frames = inventory/bank frames only (not RegisterModule.frames).
    local frames = self.BagsterModule and self.BagsterModule.frames
    if frames then
        for i = 1, 2 do
            local frame = frames[i]
            local items = frame and frame.itemFrame and frame.itemFrame.items
            if items then
                for _, item in pairs(items) do
                    if item.UpdateSlotColor then
                        item:UpdateSlotColor()
                    end
                end
            end
        end
    end
end

-- A proficiency arrives with the character's skills (on login, and when one is learned). Anything tinted before
-- then was judged on the old ones, so look again. Weapon skill-ups raise the same event mid-fight, so the look is
-- taken once, a second after the last of them.
local skillFrame = CreateFrame("Frame")
local refreshIn
skillFrame:RegisterEvent("SKILL_LINES_CHANGED")
skillFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
skillFrame:SetScript("OnEvent", function()
    refreshIn = 1
end)
skillFrame:SetScript("OnUpdate", function(_, elapsed)
    if not refreshIn then return end
    refreshIn = refreshIn - elapsed
    if refreshIn <= 0 then
        refreshIn = nil
        addon:RefreshUnusableItemTints()
    end
end)
