-- RetailUI: retail (Dragonflight) window chrome for the classic 3.3.5 panels, loaded as part of FrameXML from
-- the client interface patch. Self-contained: the art is Interface\RetailUI (extracted from the retail client by
-- localTools/interface/extractRetailUi.py) and RetailUIAtlas.lua describes it.
--
-- It only touches the windows listed in RetailWindows.lua; everything DragonUI already reskins stays DragonUI's.

RetailUI = {}
local RUI = RetailUI

RUI.specs = {}
RUI.order = {}

local ROCK = RetailUIFiles["ui-background-rock"]
local MARBLE = RetailUIFiles["ui-background-marble"]

function RUI.SetAtlas(texture, name, useSize)
    local info = RetailUIAtlas[name]
    if not info then return end
    texture:SetTexture(info[1])
    texture:SetTexCoord(info[4], info[5], info[6], info[7])
    texture:SetHorizTile(info[8])
    texture:SetVertTile(info[9])
    if useSize then
        texture:SetWidth(info[2])
        texture:SetHeight(info[3])
    end
end

-- Retail PortraitFrameTemplate nine-slice (NineSliceLayouts.lua): the top-left corner carries the portrait
-- cutout and overhangs 13 px left / 16 px up; edges run corner to corner.
local PORTRAIT_LAYOUT = {
    TopLeftCorner = { "ui-frame-portraitmetal-cornertopleft-2x", "TOPLEFT", -13, 16 },
    TopRightCorner = { "ui-frame-metal-cornertopright-2x", "TOPRIGHT", 4, 16 },
    BottomLeftCorner = { "ui-frame-metal-cornerbottomleft-2x", "BOTTOMLEFT", -13, -3 },
    BottomRightCorner = { "ui-frame-metal-cornerbottomright-2x", "BOTTOMRIGHT", 4, -3 },
}

local function applyNineSlice(container, hasPortrait)
    local pieces = {}
    for key, piece in pairs(PORTRAIT_LAYOUT) do
        local atlas = piece[1]
        if key == "TopLeftCorner" and not hasPortrait then atlas = "ui-frame-metal-cornertopleft-2x" end
        local texture = container:CreateTexture(nil, "OVERLAY")
        RUI.SetAtlas(texture, atlas, true)
        texture:SetPoint(piece[2], container, piece[2], piece[3], piece[4])
        pieces[key] = texture
    end

    local function edge(atlas, p1, a1, r1, x1, p2, a2, r2, x2)
        local texture = container:CreateTexture(nil, "OVERLAY")
        RUI.SetAtlas(texture, atlas, true)
        texture:SetPoint(p1, a1, r1, x1, 0)
        texture:SetPoint(p2, a2, r2, x2, 0)
        return texture
    end
    edge("_ui-frame-metal-edgetop-2x", "TOPLEFT", pieces.TopLeftCorner, "TOPRIGHT", -4,
        "TOPRIGHT", pieces.TopRightCorner, "TOPLEFT", 4)
    edge("_ui-frame-metal-edgebottom-2x", "BOTTOMLEFT", pieces.BottomLeftCorner, "BOTTOMRIGHT", 0,
        "BOTTOMRIGHT", pieces.BottomRightCorner, "BOTTOMLEFT", 0)
    edge("!ui-frame-metal-edgeleft-2x", "TOPLEFT", pieces.TopLeftCorner, "BOTTOMLEFT", 0,
        "BOTTOMLEFT", pieces.BottomLeftCorner, "TOPLEFT", 0)
    edge("!ui-frame-metal-edgeright-2x", "TOPRIGHT", pieces.TopRightCorner, "BOTTOMRIGHT", 0,
        "BOTTOMRIGHT", pieces.BottomRightCorner, "TOPRIGHT", 0)
end

-- Big corner art every classic panel is painted with. Item slot / label art is small or named otherwise.
local CHROME_MIN_SIZE = 100
local CHROME_SUFFIXES = { "topleft$", "topright$", "botleft$", "botright$", "bottomleft$", "bottomright$",
    "botleftpatch$", "botrightpatch$" }

local function isClassicChrome(texture)
    local file = texture:GetTexture()
    if type(file) ~= "string" then return false end
    file = file:lower()
    if file:find("^interface\\retailui") or file:find("^interface\\addons") then return false end
    if (texture:GetWidth() or 0) < CHROME_MIN_SIZE and (texture:GetHeight() or 0) < CHROME_MIN_SIZE then
        return false
    end
    for _, suffix in ipairs(CHROME_SUFFIXES) do
        if file:find(suffix) then return true end
    end
    return false
end

-- Blizzard re-shows some of this art on tab switches, so hiding means neutering Show itself.
local function neuter(region)
    if region.ruiHidden then return end
    region.ruiHidden = true
    region:Hide()
    region.Show = region.Hide
end

local function sweepChildren(frame, depth)
    if depth > 4 then return end
    for _, child in ipairs({ frame:GetChildren() }) do
        if not child.ruiOwned then
            for _, region in ipairs({ child:GetRegions() }) do
                if region:GetObjectType() == "Texture" and isClassicChrome(region) then neuter(region) end
            end
            sweepChildren(child, depth + 1)
        end
    end
end

local function findPortrait(frame, spec)
    if spec.portrait and _G[spec.portrait] then return _G[spec.portrait] end
    for _, region in ipairs({ frame:GetRegions() }) do
        if region:GetObjectType() == "Texture" then
            local file = region:GetTexture()
            local width = region:GetWidth() or 0
            if type(file) == "string" and (file:lower():find("icon") or file:lower():find("portrait"))
                and width >= 50 and width <= 66 then
                return region
            end
        end
    end
end

local function anchorRect(region, frame, values)
    region:ClearAllPoints()
    region:SetPoint("TOPLEFT", frame, "TOPLEFT", values[1], values[2])
    region:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", values[3], values[4])
end

local function modernizeCloseButton(button, holder)
    if not button then return end
    -- Never reparent it: UIPanelCloseButton closes with HideUIPanel(self:GetParent()), so a button moved
    -- onto our chrome would hide the chrome and leave the window open. Anchoring works across parents;
    -- the frame level is what keeps it above the new artwork.
    button:SetFrameLevel(holder.top:GetFrameLevel() + 2)
    button:SetWidth(24)
    button:SetHeight(24)
    button:ClearAllPoints()
    button:SetPoint("TOPRIGHT", holder, "TOPRIGHT", 1, 0)
    local function dress(texture, atlas, blend)
        if not texture then return end
        RUI.SetAtlas(texture, atlas)
        texture:ClearAllPoints()
        texture:SetAllPoints(button)
        if blend then texture:SetBlendMode(blend) end
    end
    dress(button:GetNormalTexture(), "redbutton-exit-2x")
    dress(button:GetPushedTexture(), "redbutton-exit-pressed-2x")
    dress(button:GetDisabledTexture(), "redbutton-exit-disabled-2x")
    dress(button:GetHighlightTexture(), "redbutton-highlight-2x", "ADD")
end

local function moveTitle(name, holder, offsetX)
    local text = _G[name]
    if not text then return end
    pcall(text.SetParent, text, holder.top)
    text:SetDrawLayer("OVERLAY")
    text:ClearAllPoints()
    text:SetPoint("TOPLEFT", holder, "TOPLEFT", 58 + (offsetX or 0), -4)
    text:SetPoint("TOPRIGHT", holder, "TOPRIGHT", -26, -4)
    text:SetJustifyH("CENTER")
end

local function paintInset(frame, holder, spec)
    if not spec.insetStyle then return end
    local inset = CreateFrame("Frame", nil, frame)
    inset.ruiOwned = true
    inset:SetFrameLevel(frame:GetFrameLevel())
    holder.inset = inset

    local ground = frame:CreateTexture(nil, "BORDER")
    if spec.insetStyle == "parchment" then
        RUI.SetAtlas(ground, "questbg-parchment")
    else
        ground:SetTexture(MARBLE, true)
        ground:SetHorizTile(true)
        ground:SetVertTile(true)
    end
    ground:SetAllPoints(inset)

    -- Retail InsetFrameTemplate rim: 6 px corners joined by 3 px tiles
    local function corner(atlas, point)
        local texture = frame:CreateTexture(nil, "ARTWORK")
        RUI.SetAtlas(texture, atlas, true)
        texture:SetPoint(point, inset, point, 0, 0)
        return texture
    end
    local tl, tr = corner("UI-Frame-InnerTopLeft", "TOPLEFT"), corner("UI-Frame-InnerTopRight", "TOPRIGHT")
    local bl = corner("UI-Frame-InnerBotLeftCorner", "BOTTOMLEFT")
    local br = corner("UI-Frame-InnerBotRight", "BOTTOMRIGHT")
    local function edge(atlas, vertical, p1, a1, r1, p2, a2, r2)
        local texture = frame:CreateTexture(nil, "ARTWORK")
        RUI.SetAtlas(texture, atlas)
        if vertical then texture:SetWidth(3) else texture:SetHeight(3) end
        texture:SetPoint(p1, a1, r1)
        texture:SetPoint(p2, a2, r2)
    end
    edge("!UI-Frame-InnerLeftTile", true, "TOPLEFT", tl, "BOTTOMLEFT", "BOTTOMLEFT", bl, "TOPLEFT")
    edge("!UI-Frame-InnerRightTile", true, "TOPRIGHT", tr, "BOTTOMRIGHT", "BOTTOMRIGHT", br, "TOPRIGHT")
    edge("_UI-Frame-InnerTopTile", false, "TOPLEFT", tl, "TOPRIGHT", "TOPRIGHT", tr, "TOPLEFT")
    edge("_UI-Frame-InnerBotTile", false, "BOTTOMLEFT", bl, "BOTTOMRIGHT", "BOTTOMRIGHT", br, "BOTTOMLEFT")
end

-- Live tuning (/rui) lasts for the session; the values it prints go back into RetailWindows.lua.
local tuning = {}

local function rect(spec, key)
    return tuning[spec.frame] and tuning[spec.frame][key] or spec[key]
end

function RUI.Layout(spec)
    local frame = _G[spec.frame]
    local holder = frame and frame.ruiHolder
    if not holder then return end
    anchorRect(holder, frame, rect(spec, "chrome"))
    if holder.inset then anchorRect(holder.inset, frame, rect(spec, "inset")) end
    if RUI.editing then RUI.ShowOutlines(spec) end
end

function RUI.Skin(spec)
    local frame = _G[spec.frame]
    if not frame or frame.ruiHolder then return end

    -- Layers (3.3.5 has no texture sub-levels): rock BACKGROUND < streaks / inset ground BORDER < inset rim
    -- ARTWORK < portrait OVERLAY; border pieces and title live on child frames above the content.
    -- The retail window rectangle. Border pieces sit above the content (they only cover the edges and the
    -- title band); grounds are the frame's own BACKGROUND textures, under everything.
    local holder = CreateFrame("Frame", nil, frame)
    holder.ruiOwned = true
    holder:SetFrameLevel(frame:GetFrameLevel() + 12)
    frame.ruiHolder = holder
    holder.top = CreateFrame("Frame", nil, holder)
    holder.top.ruiOwned = true
    holder.top:SetAllPoints(holder)
    holder.top:SetFrameLevel(holder:GetFrameLevel() + 2)

    local portrait = findPortrait(frame, spec)
    local keep = {}
    for _, name in ipairs(spec.keep or {}) do
        if _G[name] then keep[_G[name]] = true end
    end
    for _, region in ipairs({ frame:GetRegions() }) do
        if region:GetObjectType() == "Texture" and region ~= portrait and not keep[region] then neuter(region) end
    end
    if portrait then portrait:SetDrawLayer("OVERLAY") end
    sweepChildren(frame, 1)
    for _, name in ipairs(spec.hide or {}) do
        if _G[name] then neuter(_G[name]) end
    end

    local rock = frame:CreateTexture(nil, "BACKGROUND")
    rock:SetTexture(ROCK, true)
    rock:SetHorizTile(true)
    rock:SetVertTile(true)
    rock:SetPoint("TOPLEFT", holder, "TOPLEFT", 2, -21)
    rock:SetPoint("BOTTOMRIGHT", holder, "BOTTOMRIGHT", -2, 2)

    local streaks = frame:CreateTexture(nil, "BORDER")
    RUI.SetAtlas(streaks, "_UI-Frame-TopTileStreaks")
    streaks:SetHeight(43)
    streaks:SetPoint("TOPLEFT", holder, "TOPLEFT", 6, -21)
    streaks:SetPoint("TOPRIGHT", holder, "TOPRIGHT", -2, -21)

    applyNineSlice(holder, portrait ~= nil)
    paintInset(frame, holder, spec)
    modernizeCloseButton(spec.close and _G[spec.close], holder)
    for _, title in ipairs(spec.titles or {}) do moveTitle(title, holder, spec.titleOffset) end
    RUI.Layout(spec)
end

function RUI.Register(spec)
    RUI.specs[spec.frame] = spec
    RUI.order[#RUI.order + 1] = spec.frame
end

-- Windows from load-on-demand Blizzard addons appear later: hook again on every ADDON_LOADED.
local function hookAll()
    for _, name in ipairs(RUI.order) do
        local frame = _G[name]
        if frame and not frame.ruiHooked then
            frame.ruiHooked = true
            local spec = RUI.specs[name]
            -- Skinned on first show, once the client has laid the window out and resolved its textures
            frame:HookScript("OnShow", function() RUI.Skin(spec) end)
            if frame:IsShown() then RUI.Skin(spec) end
        end
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_LOGIN")
events:RegisterEvent("ADDON_LOADED")
events:SetScript("OnEvent", hookAll)

-- Tuning ------------------------------------------------------------------------------------------------
local function outline(frame, key, r, g, b)
    frame.ruiOutlines = frame.ruiOutlines or {}
    local box = frame.ruiOutlines[key]
    if not box then
        box = CreateFrame("Frame", nil, frame)
        box:SetFrameLevel(frame:GetFrameLevel() + 30)
        box:SetBackdrop({ edgeFile = "Interface\\Buttons\\WHITE8X8", edgeSize = 1 })
        frame.ruiOutlines[key] = box
    end
    box:SetBackdropBorderColor(r, g, b, 1)
    box:Show()
    return box
end

function RUI.ShowOutlines(spec)
    local frame = _G[spec.frame]
    if not frame or not frame.ruiHolder then return end
    anchorRect(outline(frame, "chrome", 1, 0.2, 0.2), frame, rect(spec, "chrome"))
    if frame.ruiHolder.inset then anchorRect(outline(frame, "inset", 0.2, 1, 0.2), frame, rect(spec, "inset")) end
end

local function printRects(spec)
    local chrome, inset = rect(spec, "chrome"), rect(spec, "inset")
    DEFAULT_CHAT_FRAME:AddMessage(("|cff33ff99RetailUI|r %s chrome %d %d %d %d%s"):format(spec.frame, chrome[1],
        chrome[2], chrome[3], chrome[4],
        inset and (" | inset %d %d %d %d"):format(inset[1], inset[2], inset[3], inset[4]) or ""))
end

SLASH_RETAILUI1 = "/rui"
SlashCmdList.RETAILUI = function(message)
    local command, name, a, b, c, d = strsplit(" ", strtrim(message or ""))
    command = (command or ""):lower()
    local spec = name and RUI.specs[name]
    if command == "edit" then
        RUI.editing = not RUI.editing
        for _, frameName in ipairs(RUI.order) do
            local frame = _G[frameName]
            if RUI.editing then
                RUI.ShowOutlines(RUI.specs[frameName])
                if frame and frame:IsShown() then printRects(RUI.specs[frameName]) end
            elseif frame and frame.ruiOutlines then
                for _, box in pairs(frame.ruiOutlines) do box:Hide() end
            end
        end
        DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99RetailUI|r outlines "
            .. (RUI.editing and "on (red: window, green: inset)" or "off"))
    elseif (command == "chrome" or command == "inset") and spec and tonumber(d) then
        tuning[name] = tuning[name] or {}
        tuning[name][command] = { tonumber(a), tonumber(b), tonumber(c), tonumber(d) }
        RUI.Layout(spec)
        printRects(spec)
    elseif command == "reset" and spec then
        tuning[name] = nil
        RUI.Layout(spec)
        printRects(spec)
    else
        DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99RetailUI|r /rui edit | /rui chrome <Frame> left top right bottom"
            .. " | /rui inset <Frame> left top right bottom | /rui reset <Frame>")
    end
end
