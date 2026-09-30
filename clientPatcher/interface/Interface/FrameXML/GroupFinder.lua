-- Group Finder: the Dungeon Finder in retail's PVEFrame shape (Blizzard_GroupFinder PVEFrame.xml, the window
-- Mists of Pandaria introduced), our retail window chrome around it (RetailUI.lua). Its blue mode column and round
-- mode buttons are retail's own art and layout (bluemenu-*, localTools/interface/buildGroupFinderArt.py); the
-- buttons - Dungeons, Raids, Mythic+ - stand in for the tabs RaidFinder.lua creates. The right side is the stock
-- Dungeon Finder panel (LFDQueueFrame), where the raid panel (RaidFinder.lua) and the Mythic+ frame (MythicPlus.lua)
-- are drawn too, untouched. Loaded after both.

local ART = "Interface\\RetailUI\\"
local MAIN = ART .. "bluemenu-main"

-- Retail's measures (PVEFrame.xml): the mode column is 224 wide, its blue ground 209 wide from (7, -23)
local SIDE_WIDTH = 224
local QUEUE_WIDTH, QUEUE_HEIGHT = 355, 440
-- The stock panel's own left margin (the old frame's border), tucked under the column
local QUEUE_TUCK = 14
local WIDTH = SIDE_WIDTH - QUEUE_TUCK + QUEUE_WIDTH
-- The stock list background (quest paper, dungeon wall) is 512 wide; the panel shows this much of it
local BACKGROUND_WIDTH = 326

-- GroupFinderGroupButtonTemplate: 203 x 60, 23 apart, the first at (10, -70)
local BUTTON_WIDTH, BUTTON_HEIGHT, BUTTON_GAP = 203, 60, 23
local BUTTON_NORMAL = { 0.00390625, 0.87890625, 0.75195313, 0.83007813 }
local BUTTON_SELECTED = { 0.00390625, 0.87890625, 0.59179688, 0.66992188 }

local french = GetLocale() == "frFR"
local MODES = {
    { name = french and "Donjons" or "Dungeons", icon = ART .. "groupfinder-icon-dungeons" },
    { name = french and "Raids" or "Raids", icon = ART .. "groupfinder-icon-raids" },
    { name = french and "Mythique+" or "Mythic+", icon = ART .. "groupfinder-icon-mythicplus" },
}

-- The window ---------------------------------------------------------------------------------------------------------

LFDParentFrame:SetWidth(WIDTH)
LFDParentFrame:SetHeight(QUEUE_HEIGHT)
LFDQueueFrame:ClearAllPoints()
LFDQueueFrame:SetPoint("TOPRIGHT", LFDParentFrame, "TOPRIGHT", 0, 0)
LFDQueueFrame:SetWidth(QUEUE_WIDTH)
LFDQueueFrame:SetHeight(QUEUE_HEIGHT)

-- The stock frame art (its border, portrait ring and title band) gives way to the retail chrome; the list
-- background keeps only the part the panel shows, whatever texture the Dungeon Finder puts on it
LFDQueueFrameLayout:Hide()
LFDQueueFrameLayout.Show = LFDQueueFrameLayout.Hide
local function CropBackground()
    LFDQueueFrameBackground:SetWidth(BACKGROUND_WIDTH)
    LFDQueueFrameBackground:SetTexCoord(0, BACKGROUND_WIDTH / 512, 0, 1)
end
CropBackground()
hooksecurefunc(LFDQueueFrameBackground, "SetTexture", CropBackground)

-- The eye in the chrome's portrait cutout: retail's portrait sits 5 px left of and 7 px above the window's corner,
-- and the eye's picture is 3 px into its frame
LFDParentFramePortrait:ClearAllPoints()
LFDParentFramePortrait:SetPoint("TOPLEFT", LFDParentFrame, "TOPLEFT", -8, 7)

-- Moved freely: out of the panel manager (which would put it back at the left edge each time it opens), dragged
-- by any empty part of it, kept where it was left (the client saves a placed frame's spot), closed by Escape
UIPanelWindows["LFDParentFrame"] = nil
tinsert(UISpecialFrames, "LFDParentFrame")
LFDParentFrame:SetMovable(true)
LFDParentFrame:EnableMouse(true)
LFDParentFrame:SetClampedToScreen(true)
LFDParentFrame:RegisterForDrag("LeftButton")
LFDParentFrame:SetScript("OnDragStart", LFDParentFrame.StartMoving)
LFDParentFrame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    self:SetUserPlaced(true)
end)
if not LFDParentFrame:IsUserPlaced() then
    LFDParentFrame:ClearAllPoints()
    LFDParentFrame:SetPoint("TOPLEFT", UIParent, "TOPLEFT", 40, -104)
end

-- The close button has no name of its own: RetailUI finds it by a global of ours
local closeName
for _, child in ipairs({ LFDParentFrame:GetChildren() }) do
    if not closeName and child:GetObjectType() == "Button" and not child:GetName() then
        _G.LFDParentFrameCloseButton = child
        closeName = "LFDParentFrameCloseButton"
    end
end

RetailUI.Register({
    frame = "LFDParentFrame", portrait = "LFDParentFramePortraitIcon", close = closeName,
    titles = { "LFDQueueFrameTitleText" },
    chrome = { 0, 0, 0, 0 },
})

-- The tabs stay (RaidFinder.lua's logic reads them) but the mode buttons replace them on screen
for index = 1, LFDParentFrame.numTabs or 3 do
    local tab = _G["LFDParentFrameTab" .. index]
    if tab then
        tab:Hide()
        tab.Show = tab.Hide
    end
end

-- The mode column: retail's blue ground, its gold corners, edges and filigree -----------------------------------------

local column = CreateFrame("Frame", "GroupFinderModeColumn", LFDParentFrame)
column:SetPoint("TOPLEFT", LFDParentFrame, "TOPLEFT", 0, 0)
column:SetWidth(SIDE_WIDTH)
column:SetHeight(QUEUE_HEIGHT)

-- The eye above the column's art, under the chrome's portrait ring (RetailUI draws the chrome 12 levels up)
LFDParentFramePortrait:SetFrameLevel(column:GetFrameLevel() + 4)

-- The column's height past retail's 428: its edges grow by as much
local EXTRA = QUEUE_HEIGHT - 428

local function Piece(layer, file, width, height, coords)
    local texture = column:CreateTexture(nil, layer)
    texture:SetTexture(file)
    texture:SetWidth(width)
    texture:SetHeight(height)
    texture:SetTexCoord(coords[1], coords[2], coords[3], coords[4])
    return texture
end

local blueBg = Piece("BACKGROUND", MAIN, 209, 399 + EXTRA, { 0.00390625, 0.82421875, 0.18554688, 0.58984375 })
blueBg:SetPoint("TOPLEFT", 7, -23)

local topFiligree = Piece("BORDER", MAIN, 185, 55, { 0.00390625, 0.72656250, 0.12988281, 0.18359375 })
topFiligree:SetPoint("TOPLEFT", blueBg, "TOPLEFT", 12, -6)
local bottomFiligree = Piece("BORDER", MAIN, 185, 55, { 0.26171875, 0.98437500, 0.06542969, 0.11914063 })
bottomFiligree:SetPoint("BOTTOMLEFT", blueBg, "BOTTOMLEFT", 12, 4)

local tlCorner = Piece("ARTWORK", MAIN, 64, 64, { 0.00390625, 0.25390625, 0.00097656, 0.06347656 })
tlCorner:SetPoint("TOPLEFT", blueBg, "TOPLEFT", 0, 0)
local trCorner = Piece("ARTWORK", MAIN, 64, 64, { 0.51953125, 0.76953125, 0.00097656, 0.06347656 })
trCorner:SetPoint("TOPLEFT", 151, -23)
local brCorner = Piece("ARTWORK", MAIN, 64, 64, { 0.00390625, 0.25390625, 0.06542969, 0.12792969 })
brCorner:SetPoint("BOTTOMLEFT", 151, 7)
local blCorner = Piece("ARTWORK", MAIN, 64, 64, { 0.26171875, 0.51171875, 0.00097656, 0.06347656 })
blCorner:SetPoint("BOTTOMLEFT", 7, 7)

-- The side edges tile down (bluemenu-vert is 128 tall), the top and bottom ones across (goldborder is 64 wide)
local EDGE_HEIGHT = 270 + EXTRA
local leftEdge = Piece("ARTWORK", ART .. "bluemenu-vert", 43, EDGE_HEIGHT,
    { 0.06250000, 0.39843750, 0, EDGE_HEIGHT / 128 })
leftEdge:SetVertTile(true)
leftEdge:SetPoint("TOPLEFT", 7, -87)
local rightEdge = Piece("ARTWORK", ART .. "bluemenu-vert", 43, EDGE_HEIGHT,
    { 0.41406250, 0.75000000, 0, EDGE_HEIGHT / 128 })
rightEdge:SetVertTile(true)
rightEdge:SetPoint("TOPLEFT", 172, -87)
local bottomEdge = Piece("ARTWORK", ART .. "bluemenu-goldborder-horiz", 80, 43,
    { 0, 80 / 64, 0.35937500, 0.69531250 })
bottomEdge:SetHorizTile(true)
bottomEdge:SetPoint("BOTTOMLEFT", blCorner, "BOTTOMRIGHT", 0, 0)
local topEdge = Piece("ARTWORK", ART .. "bluemenu-goldborder-horiz", 80, 43, { 0, 80 / 64, 0.00781250, 0.34375000 })
topEdge:SetHorizTile(true)
topEdge:SetPoint("TOPLEFT", tlCorner, "TOPRIGHT", 0, 0)

-- The mode buttons ---------------------------------------------------------------------------------------------------

local buttons = {}

local function CreateModeButton(index, mode)
    local button = CreateFrame("Button", "GroupFinderModeButton" .. index, column)
    button:SetWidth(BUTTON_WIDTH)
    button:SetHeight(BUTTON_HEIGHT)
    button:SetPoint("TOPLEFT", column, "TOPLEFT", 10, -70 - (index - 1) * (BUTTON_HEIGHT + BUTTON_GAP))

    -- The plate, a little larger than the button, centred on it
    local bg = button:CreateTexture(nil, "BACKGROUND")
    bg:SetTexture(MAIN)
    bg:SetWidth(224)
    bg:SetHeight(80)
    bg:SetPoint("CENTER")
    button.bg = bg

    local highlight = button:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetTexture(MAIN)
    highlight:SetTexCoord(BUTTON_NORMAL[1], BUTTON_NORMAL[2], BUTTON_NORMAL[3], BUTTON_NORMAL[4])
    highlight:SetWidth(224)
    highlight:SetHeight(80)
    highlight:SetPoint("CENTER")
    highlight:SetBlendMode("ADD")
    highlight:SetAlpha(0.8)

    -- The ring (bluemenuring: the ring in its first 103 x 104 pixels) over the icon, cut round beforehand
    local ring = button:CreateTexture(nil, "ARTWORK")
    ring:SetTexture(ART .. "bluemenu-ring")
    ring:SetTexCoord(1 / 128, 103 / 128, 1 / 128, 104 / 128)
    ring:SetWidth(95)
    ring:SetHeight(96)
    ring:SetPoint("LEFT", button, "LEFT", -12, -1)
    local icon = button:CreateTexture(nil, "BORDER")
    icon:SetTexture(mode.icon)
    icon:SetWidth(66)
    icon:SetHeight(66)
    icon:SetPoint("CENTER", ring, "CENTER", 0, 0)

    local name = button:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    name:SetWidth(106)
    name:SetPoint("LEFT", ring, "RIGHT", 0, 0)
    name:SetJustifyH("LEFT")
    name:SetText(mode.name)

    button:SetScript("OnClick", function()
        if GroupFinder_SelectedMode() ~= index then
            GroupFinder_SelectMode(index)
        end
    end)
    return button
end

for index, mode in ipairs(MODES) do
    buttons[index] = CreateModeButton(index, mode)
end

-- Called by RaidFinder.lua whenever a mode is shown
function GroupFinder_OnModeSelected(index)
    for buttonIndex, button in ipairs(buttons) do
        local coords = buttonIndex == index and BUTTON_SELECTED or BUTTON_NORMAL
        button.bg:SetTexCoord(coords[1], coords[2], coords[3], coords[4])
    end
end
GroupFinder_OnModeSelected(GroupFinder_SelectedMode())
