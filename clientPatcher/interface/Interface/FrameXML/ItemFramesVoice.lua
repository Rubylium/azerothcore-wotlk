-- The Hollow Voice's touched gear (one piece in four: Ailes du Séraphin, Égide d'Aldric, Murmure de Vel'thazar): a
-- painted frame of Aldric's white gold, cracked by the demon's void (localTools/interface/buildItemFrameArt.py), and
-- the void's violet breathing behind it.
local api = EvolutionsItemFrames
local layout = EvolutionsTooltip

local FRAME = "Interface\\ItemFrames\\ItemFrame-Voice"
local OPENING = 0.5872  -- the frame's empty middle, as a share of its texture (buildItemFrameArt.py prints it)
local INSET = 4         -- how far the frame's inner edge sits inside the icon: its bars lie on the icon's rim

api.registerResolver(function(tooltip)
    if not tooltip:GetName() then return end
    local _, touch = layout.FindTouch(tooltip)
    if touch == "voice" then return "hollowVoice" end
end)

api.registerStyle("hollowVoice", {
    create = function(parent)
        local art = CreateFrame("Frame", nil, parent)
        art:SetAllPoints(parent)
        art:EnableMouse(false)

        local glow = art:CreateTexture(nil, "BACKGROUND")
        glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        glow:SetBlendMode("ADD")
        glow:SetVertexColor(0.55, 0.12, 0.85, 0.5)

        local frame = art:CreateTexture(nil, "OVERLAY")
        frame:SetTexture(FRAME)

        -- The texture is laid so its opening lands just inside the icon, whatever the button's size
        local function Layout(self)
            local width, height = self:GetWidth(), self:GetHeight()
            if width <= 0 or height <= 0 then return end
            local outX = ((width - 2 * INSET) / OPENING - width) / 2
            local outY = ((height - 2 * INSET) / OPENING - height) / 2
            frame:ClearAllPoints()
            frame:SetPoint("TOPLEFT", -outX, outY)
            frame:SetPoint("BOTTOMRIGHT", outX, -outY)
            glow:ClearAllPoints()
            glow:SetPoint("TOPLEFT", -outX - 6, outY + 6)
            glow:SetPoint("BOTTOMRIGHT", outX + 6, -outY - 6)
        end
        art:SetScript("OnSizeChanged", Layout)
        Layout(art)

        local breathe = glow:CreateAnimationGroup()
        breathe:SetLooping("BOUNCE")
        local fade = breathe:CreateAnimation("Alpha")
        fade:SetChange(-0.6)
        fade:SetDuration(1.8)
        fade:SetSmoothing("IN_OUT")
        breathe:Play()
        -- A hidden frame's animation stops with it
        art:SetScript("OnShow", function() breathe:Play() end)

        return art
    end,
})

-- The same frame around the item's tooltip: its corners, its two crests and its bars cut from one painting
-- (buildItemFrameArt.py, the atlas's layout there), laid along the tooltip's edge in place of its own border
local TOOLTIP = "Interface\\ItemFrames\\Tooltip-Voice"
local TOOLTIP_SCALE = 1 / 6     -- the painting's pixels to the screen's: its bars, 21 px there, about 3.5 here
local TOOLTIP_EDGE = 3          -- the bars' middle, in from the tooltip's edge, where its own border runs
local CORNER = 320 * TOOLTIP_SCALE
local CREST = 270 * TOOLTIP_SCALE
local BAR = 30 * TOOLTIP_SCALE

local function Piece(parent, layer, left, right, top, bottom)
    local piece = parent:CreateTexture(nil, layer)
    piece:SetTexture(TOOLTIP)
    piece:SetTexCoord(left / 256, right / 256, top / 128, bottom / 128)
    return piece
end

local function CreateTooltipFrame(tooltip)
    local dress = CreateFrame("Frame", nil, tooltip)
    dress:SetAllPoints(tooltip)
    dress:EnableMouse(false)
    local edge = TOOLTIP_EDGE

    -- The bars, under the ornaments; their texels taken half a texel in, clear of the atlas's neighbours
    local top = Piece(dress, "ARTWORK", 128.5, 191.5, 64.5, 79.5)
    top:SetPoint("LEFT", dress, "TOPLEFT", edge, -edge)
    top:SetPoint("RIGHT", dress, "TOPRIGHT", -edge, -edge)
    top:SetHeight(BAR)
    local bottom = Piece(dress, "ARTWORK", 128.5, 191.5, 64.5, 79.5)
    bottom:SetPoint("LEFT", dress, "BOTTOMLEFT", edge, edge)
    bottom:SetPoint("RIGHT", dress, "BOTTOMRIGHT", -edge, edge)
    bottom:SetHeight(BAR)
    local left = Piece(dress, "ARTWORK", 192.5, 207.5, 64.5, 127.5)
    left:SetPoint("TOP", dress, "TOPLEFT", edge, -edge)
    left:SetPoint("BOTTOM", dress, "BOTTOMLEFT", edge, edge)
    left:SetWidth(BAR)
    local right = Piece(dress, "ARTWORK", 192.5, 207.5, 64.5, 127.5)
    right:SetPoint("TOP", dress, "TOPRIGHT", -edge, -edge)
    right:SetPoint("BOTTOM", dress, "BOTTOMRIGHT", -edge, edge)
    right:SetWidth(BAR)

    for index, corner in ipairs({ { "TOPLEFT", edge, -edge }, { "TOPRIGHT", -edge, -edge },
        { "BOTTOMLEFT", edge, edge }, { "BOTTOMRIGHT", -edge, edge } }) do
        local piece = Piece(dress, "OVERLAY", (index - 1) * 64, index * 64, 0, 64)
        piece:SetSize(CORNER, CORNER)
        piece:SetPoint("CENTER", dress, corner[1], corner[2], corner[3])
    end
    local crown = Piece(dress, "OVERLAY", 0, 64, 64, 128)
    crown:SetSize(CREST, CREST)
    crown:SetPoint("CENTER", dress, "TOP", 0, -edge)
    local void = Piece(dress, "OVERLAY", 64, 128, 64, 128)
    void:SetSize(CREST, CREST)
    void:SetPoint("CENTER", dress, "BOTTOM", 0, edge)
    dress:Hide()
    return dress
end

local function Undress(tooltip)
    local dress = tooltip.evolutionsVoiceFrame
    if not dress or not dress:IsShown() then return end
    dress:Hide()
    local border = tooltip.evolutionsVoiceBorder
    if border then
        tooltip:SetBackdropBorderColor(border[1], border[2], border[3], border[4])
    end
end

local function Dress(tooltip)
    local _, touch = layout.FindTouch(tooltip)
    if touch ~= "voice" then
        Undress(tooltip)
        return
    end
    tooltip.evolutionsVoiceFrame = tooltip.evolutionsVoiceFrame or CreateTooltipFrame(tooltip)
    local dress = tooltip.evolutionsVoiceFrame
    if dress:IsShown() then return end
    -- The tooltip's own border gives way to the frame's bars, and comes back when the item goes
    tooltip.evolutionsVoiceBorder = { tooltip:GetBackdropBorderColor() }
    local r, g, b = tooltip:GetBackdropBorderColor()
    tooltip:SetBackdropBorderColor(r, g, b, 0)
    dress:SetFrameLevel(tooltip:GetFrameLevel() + 2)
    dress:Show()
end

for _, name in ipairs({ "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Dress)
        tooltip:HookScript("OnTooltipCleared", Undress)
        tooltip:HookScript("OnHide", Undress)
    end
end
