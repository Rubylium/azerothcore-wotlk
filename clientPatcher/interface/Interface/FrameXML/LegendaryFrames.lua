-- Legendary items' own frames (modules/mod-legendary): a painted frame around the item's icon, its holy fire
-- breathing behind, and a painted frame around its tooltip - each legendary its own, in the class HUDs' style
-- (art: clientPatcher/assets/<legendary>, built by localTools/interface/buildLegendaryClientArt.py). A legendary is
-- known by its base item: every copy of it wears the frame.
local api = EvolutionsItemFrames

local ART = {
    [24567] = {     -- Marque de l'Inquisiteur
        key = "legendaryMarqueInquisiteur",
        frame = "Interface\\ItemFrames\\Legendary-MarqueInquisiteur-Frame",
        glow = "Interface\\ItemFrames\\Legendary-MarqueInquisiteur-Glow",
        tooltip = "Interface\\ItemFrames\\Legendary-MarqueInquisiteur-Tooltip",
        glowColor = { 1, 0.62, 0.25 },
    },
}

local function ItemOf(link)
    return link and tonumber(link:match("item:(%d+)"))
end

-- The item frame ------------------------------------------------------------------------------------------------------

api.registerResolver(function(tooltip)
    local _, link = tooltip:GetItem()
    local art = ART[ItemOf(link) or 0]
    return art and art.key
end)

-- The frame's size against the icon's: its bars lie over the icon's rim, it reaches a little past it (first drawn
-- with its whole opening on the icon, about 170%, it read "too much" in game)
local FRAME_SCALE = 1.3
local GLOW_SCALE = 1.5

for _, art in pairs(ART) do
    api.registerStyle(art.key, {
        create = function(parent)
            local holder = CreateFrame("Frame", nil, parent)
            holder:SetAllPoints(parent)
            holder:EnableMouse(false)

            local glow = holder:CreateTexture(nil, "BACKGROUND")
            glow:SetTexture(art.glow)
            glow:SetBlendMode("ADD")
            glow:SetVertexColor(art.glowColor[1], art.glowColor[2], art.glowColor[3])

            local frame = holder:CreateTexture(nil, "OVERLAY")
            frame:SetTexture(art.frame)

            -- Centred on the icon, whatever the button's size
            local function Layout(self)
                local width, height = self:GetWidth(), self:GetHeight()
                if width <= 0 or height <= 0 then return end
                for texture, scale in pairs({ [frame] = FRAME_SCALE, [glow] = GLOW_SCALE }) do
                    local outX, outY = width * (scale - 1) / 2, height * (scale - 1) / 2
                    texture:ClearAllPoints()
                    texture:SetPoint("TOPLEFT", -outX, outY)
                    texture:SetPoint("BOTTOMRIGHT", outX, -outY)
                end
            end
            holder:SetScript("OnSizeChanged", Layout)
            Layout(holder)

            -- The fire breathes: the alpha set first, the pulse started from it (an Alpha animation keeps the alpha
            -- it starts from on this client)
            local breathe = glow:CreateAnimationGroup()
            breathe:SetLooping("BOUNCE")
            local fade = breathe:CreateAnimation("Alpha")
            fade:SetChange(-0.3)
            fade:SetDuration(1.6)
            fade:SetSmoothing("IN_OUT")
            local function Start()
                breathe:Stop()
                glow:SetAlpha(0.55)
                breathe:Play()
            end
            holder:SetScript("OnShow", Start)
            Start()
            return holder
        end,
    })
end

-- The copy's item level on the icon, over the painted frame: DragonUI writes the base item's under it (its item
-- level module reads GetItemInfo, the template's 289), so on a legendary its number is hidden and the copy's written
-- in the same font, above the frame
local function LevelText(button)
    local effect = button.evolutionsItemFrame
    if not effect then return end
    if not effect.legendaryLevel then
        local holder = CreateFrame("Frame", nil, effect)
        holder:SetAllPoints(effect)
        holder:SetFrameLevel(effect:GetFrameLevel() + 2)
        effect.legendaryLevel = holder:CreateFontString(nil, "OVERLAY")
        effect.legendaryLevel:SetFont("Fonts\\FRIZQT__.TTF", 12, "OUTLINE")
        effect.legendaryLevel:SetPoint("BOTTOM", effect, "BOTTOM", 0, 2)
        effect.legendaryLevel:SetTextColor(1, 0.5, 0)
    end
    local theirs = button.__DragonUI_ILvl
    if theirs then
        local font, size, flags = theirs:GetFont()
        if font then effect.legendaryLevel:SetFont(font, size, flags) end
        local point, _, relativePoint, x, y = theirs:GetPoint(1)
        if point then
            effect.legendaryLevel:ClearAllPoints()
            effect.legendaryLevel:SetPoint(point, effect, relativePoint, x, y)
        end
    end
    return effect.legendaryLevel, theirs
end

hooksecurefunc(api, "apply", function(button, key)
    if not button then return end
    local legendary = false
    for _, art in pairs(ART) do
        if art.key == key then legendary = true end
    end
    local copy = legendary and EvolutionsLegendary and EvolutionsLegendary.CopyAt(EvolutionsLegendary.ScanWhere())
    local level = copy and copy.itemLevel
    local mine, theirs = LevelText(button)
    if mine then
        mine:SetText(level or "")
        if level then mine:Show() else mine:Hide() end
    end
    if theirs then
        theirs:SetAlpha(level and 0 or 1)
    end
end)

-- The tooltip frame ---------------------------------------------------------------------------------------------------

-- The atlas (512 x 256, buildLegendaryClientArt.py): the pieces' boxes { left, right, top, bottom }
local PIECES = {
    corner = { 0, 128, 0, 128 },
    topCrest = { 128, 320, 0, 96 },
    bottomCrest = { 320, 512, 0, 96 },
    titlePlate = { 128, 384, 96, 144 },
    across = { 128, 256, 144, 168 },
    down = { 0, 24, 128, 256 },
}
-- The atlas's pixels to the screen's: its bars, 20 px thick there, 5 here
local SCALE = 0.24
-- Where the bars cross inside the corner piece, and the bars' middle in from the tooltip's edge (on its own border)
local CORNER_CROSS = 24 * SCALE
local EDGE = 3

local function Piece(parent, layer, art, box, flipX, flipY)
    local piece = parent:CreateTexture(nil, layer)
    piece:SetTexture(art.tooltip)
    -- Half a texel in, clear of the atlas's neighbours
    local left, right = (box[1] + 0.5) / 512, (box[2] - 0.5) / 512
    local top, bottom = (box[3] + 0.5) / 256, (box[4] - 0.5) / 256
    if flipX then left, right = right, left end
    if flipY then top, bottom = bottom, top end
    piece:SetTexCoord(left, right, top, bottom)
    return piece
end

local function Size(box)
    return (box[2] - box[1]) * SCALE, (box[4] - box[3]) * SCALE
end

local function CreateDress(tooltip, art)
    local dress = CreateFrame("Frame", nil, tooltip)
    dress:SetAllPoints(tooltip)
    dress:EnableMouse(false)

    local barThickness = (PIECES.across[4] - PIECES.across[3]) * SCALE
    local top = Piece(dress, "ARTWORK", art, PIECES.across)
    top:SetPoint("LEFT", dress, "TOPLEFT", EDGE, -EDGE)
    top:SetPoint("RIGHT", dress, "TOPRIGHT", -EDGE, -EDGE)
    top:SetHeight(barThickness)
    local bottom = Piece(dress, "ARTWORK", art, PIECES.across, false, true)
    bottom:SetPoint("LEFT", dress, "BOTTOMLEFT", EDGE, EDGE)
    bottom:SetPoint("RIGHT", dress, "BOTTOMRIGHT", -EDGE, EDGE)
    bottom:SetHeight(barThickness)
    local left = Piece(dress, "ARTWORK", art, PIECES.down)
    left:SetPoint("TOP", dress, "TOPLEFT", EDGE, -EDGE)
    left:SetPoint("BOTTOM", dress, "BOTTOMLEFT", EDGE, EDGE)
    left:SetWidth(barThickness)
    local right = Piece(dress, "ARTWORK", art, PIECES.down, true)
    right:SetPoint("TOP", dress, "TOPRIGHT", -EDGE, -EDGE)
    right:SetPoint("BOTTOM", dress, "BOTTOMRIGHT", -EDGE, EDGE)
    right:SetWidth(barThickness)

    -- The corner painted top left, mirrored for the others: its bars' crossing on the bars' line
    local cornerWidth, cornerHeight = Size(PIECES.corner)
    local reach = EDGE - CORNER_CROSS
    for _, corner in ipairs({
        { "TOPLEFT", reach, -reach, false, false }, { "TOPRIGHT", -reach, -reach, true, false },
        { "BOTTOMLEFT", reach, reach, false, true }, { "BOTTOMRIGHT", -reach, reach, true, true },
    }) do
        local piece = Piece(dress, "OVERLAY", art, PIECES.corner, corner[4], corner[5])
        piece:SetSize(cornerWidth, cornerHeight)
        piece:SetPoint(corner[1], dress, corner[1], corner[2], corner[3])
    end

    -- The crests, on the top and bottom bars' middle
    local crest = Piece(dress, "OVERLAY", art, PIECES.topCrest)
    crest:SetSize(Size(PIECES.topCrest))
    crest:SetPoint("CENTER", dress, "TOP", 0, -EDGE)
    local foot = Piece(dress, "OVERLAY", art, PIECES.bottomCrest)
    foot:SetSize(Size(PIECES.bottomCrest))
    foot:SetPoint("CENTER", dress, "BOTTOM", 0, EDGE)

    -- The title's band: on the tooltip itself, under its text
    local plate = Piece(tooltip, "BACKGROUND", art, PIECES.titlePlate)
    plate:SetPoint("TOPLEFT", tooltip, "TOPLEFT", 6, -6)
    plate:SetPoint("TOPRIGHT", tooltip, "TOPRIGHT", -6, -6)
    plate:SetHeight(26)
    dress.plate = plate
    dress:Hide()
    plate:Hide()
    return dress
end

local function Undress(tooltip)
    local dress = tooltip.legendaryDress
    if not dress or not dress:IsShown() then return end
    dress:Hide()
    dress.plate:Hide()
    local border = tooltip.legendaryBorder
    if border then
        tooltip:SetBackdropBorderColor(border[1], border[2], border[3], border[4])
    end
end

local function Dress(tooltip)
    local _, link = tooltip:GetItem()
    local art = ART[ItemOf(link) or 0]
    if not art then
        Undress(tooltip)
        return
    end
    tooltip.legendaryDresses = tooltip.legendaryDresses or {}
    local dress = tooltip.legendaryDresses[art.key]
    if not dress then
        dress = CreateDress(tooltip, art)
        tooltip.legendaryDresses[art.key] = dress
    end
    if tooltip.legendaryDress and tooltip.legendaryDress ~= dress then
        Undress(tooltip)
    end
    tooltip.legendaryDress = dress
    if dress:IsShown() then return end
    -- Room for the crests: a line's height above the name and below the last line
    local name = _G[tooltip:GetName() .. "TextLeft1"]
    local title = name and name:GetText()
    if title and title:sub(1, 1) ~= "\n" then
        name:SetText("\n" .. title)
    end
    tooltip:AddLine(" ")
    tooltip:Show()
    -- The tooltip's own border gives way to the painted bars, and comes back when the item goes
    tooltip.legendaryBorder = { tooltip:GetBackdropBorderColor() }
    local r, g, b = tooltip:GetBackdropBorderColor()
    tooltip:SetBackdropBorderColor(r, g, b, 0)
    dress:SetFrameLevel(tooltip:GetFrameLevel() + 2)
    dress:Show()
    dress.plate:Show()
end

for _, name in ipairs({ "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Dress)
        tooltip:HookScript("OnTooltipCleared", Undress)
        tooltip:HookScript("OnHide", Undress)
    end
end
