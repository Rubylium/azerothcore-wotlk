-- Group Finder: the Dungeon Finder in retail's landscape shape (Mists of Pandaria's PVEFrame), our retail window
-- chrome around it (RetailUI.lua). A column of round mode buttons on the left - Dungeons, Raids, Mythic+ - stands
-- in for the tabs RaidFinder.lua creates; the right side is the stock Dungeon Finder panel (LFDQueueFrame), where
-- the raid panel (RaidFinder.lua) and the Mythic+ frame (MythicPlus.lua) are drawn too, untouched. Loaded after
-- both.

local SIDE_WIDTH = 230              -- the mode column, left of the stock panel
local QUEUE_WIDTH, QUEUE_HEIGHT = 355, 440
local MODE_HEIGHT = 74
local MODE_TOP = -12                -- the first mode button, from the top of the column
local ICON_SIZE = 50
-- The minimap button's gold ring (Interface\Minimap\MiniMap-TrackingBorder): 53 x 53 around a 20 x 20 icon placed
-- (7, -6) into it, scaled to the icon here
local RING_SCALE = ICON_SIZE / 20
-- The stock background of the list area (quest paper, dungeon wall): 512 wide, of which the panel shows this much
local BACKGROUND_WIDTH = 326

local french = GetLocale() == "frFR"
local MODES = {
    {
        name = french and "Donjons" or "Dungeons",
        detail = french and "Aléatoire, spécifique, mythique" or "Random, specific, mythic",
        icon = "Interface\\Icons\\Achievement_Dungeon_UtgardeKeep_Heroic",
    },
    {
        name = french and "Raids" or "Raids",
        detail = french and "Recherche de raid, avec bots" or "Raid Finder, with bots",
        icon = "Interface\\Icons\\Achievement_Boss_LichKing",
    },
    {
        name = french and "Mythique+" or "Mythic+",
        detail = french and "Votre clé, votre score" or "Your keystone, your rating",
        icon = "Interface\\Icons\\INV_Relics_Hourglass",
    },
}

-- The window: wider, the stock panel on its right
LFDParentFrame:SetWidth(SIDE_WIDTH + QUEUE_WIDTH)
LFDQueueFrame:ClearAllPoints()
LFDQueueFrame:SetPoint("TOPRIGHT", LFDParentFrame, "TOPRIGHT", 0, 0)
LFDQueueFrame:SetWidth(QUEUE_WIDTH)
LFDQueueFrame:SetHeight(QUEUE_HEIGHT)

-- The stock frame art (its own border, portrait ring and title band) gives way to the retail chrome; its list
-- background keeps only the part the panel shows, whatever texture the Dungeon Finder puts on it
LFDQueueFrameLayout:Hide()
LFDQueueFrameLayout.Show = LFDQueueFrameLayout.Hide
local function CropBackground()
    LFDQueueFrameBackground:SetWidth(BACKGROUND_WIDTH)
    LFDQueueFrameBackground:SetTexCoord(0, BACKGROUND_WIDTH / 512, 0, 1)
end
CropBackground()
hooksecurefunc(LFDQueueFrameBackground, "SetTexture", CropBackground)

-- The eye sits in the retail chrome's portrait cutout
LFDParentFramePortrait:ClearAllPoints()
LFDParentFramePortrait:SetPoint("TOPLEFT", LFDParentFrame, "TOPLEFT", -4, -3)

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
    chrome = { 8, -10, -2, 0 },
})

-- The tabs stay (the Dungeon Finder's logic reads them) but the mode buttons replace them on screen
for index = 1, LFDParentFrame.numTabs or 3 do
    local tab = _G["LFDParentFrameTab" .. index]
    if tab then
        tab:Hide()
        tab.Show = tab.Hide
    end
end

-- The mode column --------------------------------------------------------------------------------------------------

local column = CreateFrame("Frame", "GroupFinderModeColumn", LFDParentFrame)
column:SetPoint("TOPLEFT", LFDParentFrame, "TOPLEFT", 12, -32)
column:SetPoint("BOTTOMRIGHT", LFDQueueFrame, "BOTTOMLEFT", 2, 6)

-- A darker ground under the buttons, fading downwards, and a gold rule between the column and the panel
local ground = column:CreateTexture(nil, "BACKGROUND")
ground:SetAllPoints()
ground:SetTexture(1, 1, 1)
ground:SetGradientAlpha("VERTICAL", 0.02, 0.04, 0.08, 0.55, 0.06, 0.11, 0.2, 0.75)
local rule = column:CreateTexture(nil, "BORDER")
rule:SetTexture(0.55, 0.43, 0.2, 0.8)
rule:SetWidth(1)
rule:SetPoint("TOPRIGHT")
rule:SetPoint("BOTTOMRIGHT")

local buttons = {}

local function CreateModeButton(index, mode)
    local button = CreateFrame("Button", "GroupFinderModeButton" .. index, column)
    button:SetHeight(MODE_HEIGHT)
    button:SetPoint("LEFT", column, "LEFT", 4, 0)
    button:SetPoint("RIGHT", column, "RIGHT", -4, 0)
    button:SetPoint("TOP", column, "TOP", 0, MODE_TOP - (index - 1) * (MODE_HEIGHT + 6))

    -- The selection: retail's blue bar across the button; lighter under the mouse
    local selected = button:CreateTexture(nil, "BACKGROUND")
    selected:SetTexture("Interface\\QuestFrame\\UI-QuestLogTitleHighlight")
    selected:SetBlendMode("ADD")
    selected:SetVertexColor(0.24, 0.56, 0.95)
    selected:SetAllPoints()
    selected:Hide()
    button.selected = selected
    local hover = button:CreateTexture(nil, "BACKGROUND")
    hover:SetTexture("Interface\\QuestFrame\\UI-QuestLogTitleHighlight")
    hover:SetBlendMode("ADD")
    hover:SetVertexColor(0.24, 0.56, 0.95, 0.35)
    hover:SetAllPoints()
    button:SetHighlightTexture(hover)

    -- The round icon in its gold ring
    local icon = button:CreateTexture(nil, "ARTWORK")
    icon:SetWidth(ICON_SIZE)
    icon:SetHeight(ICON_SIZE)
    icon:SetPoint("LEFT", button, "LEFT", 12, 0)
    SetPortraitToTexture(icon, mode.icon)
    local ring = button:CreateTexture(nil, "OVERLAY")
    ring:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
    ring:SetWidth(53 * RING_SCALE)
    ring:SetHeight(53 * RING_SCALE)
    ring:SetPoint("TOPLEFT", icon, "TOPLEFT", -7 * RING_SCALE, 6 * RING_SCALE)

    local name = button:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    name:SetPoint("LEFT", icon, "RIGHT", 14, 7)
    name:SetPoint("RIGHT", button, "RIGHT", -6, 0)
    name:SetJustifyH("LEFT")
    name:SetText(mode.name)
    local detail = button:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    detail:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -3)
    detail:SetPoint("RIGHT", button, "RIGHT", -6, 0)
    detail:SetJustifyH("LEFT")
    detail:SetTextColor(0.72, 0.68, 0.6)
    detail:SetText(mode.detail)

    button:SetScript("OnClick", function()
        if GroupFinder_SelectedMode and GroupFinder_SelectedMode() == index then
            return
        end
        GroupFinder_SelectMode(index)
    end)
    return button
end

for index, mode in ipairs(MODES) do
    buttons[index] = CreateModeButton(index, mode)
end

-- Called by RaidFinder.lua whenever a mode is shown
function GroupFinder_OnModeSelected(index)
    for buttonIndex, button in ipairs(buttons) do
        if buttonIndex == index then
            button.selected:Show()
        else
            button.selected:Hide()
        end
    end
end
GroupFinder_OnModeSelected(GroupFinder_SelectedMode and GroupFinder_SelectedMode() or 1)
