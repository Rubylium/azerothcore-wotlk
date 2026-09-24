-- Evolutions: the character list of the character select screen (CharacterSelect.lua calls in here).
--
-- One card per character: tinted by its class, with its round class icon, name, level and class, where it is, and
-- on the right its Prestige (the medallion of the Prestige window) and its Paragon level (points spent on the
-- board). Those two come from the server through the client extension DLL (GetCharacterEvolution, see
-- awesome_wotlk CharacterEvolution.cpp); without it the badges simply stay hidden. The first free slot offers to
-- create a character.

local FONT_TITLE = "Fonts\\MORPHEUS.TTF"
local FONT_TEXT = "Fonts\\FRIZQT__.TTF"
local WHITE = "Interface\\Buttons\\WHITE8X8"
local ROUND_ICONS = "Interface\\Glues\\CharacterCreate\\RoundClasses"      -- 4 x 4 cells
local PRESTIGE_EMBLEM = "Interface\\Prestige\\Prestige-Emblem"
local PARAGON_RING = "Interface\\Paragon\\Paragon-Node-Notable"

local PANEL_WIDTH, CARD_HEIGHT, CARD_GAP, PANEL_PADDING, HEADER_HEIGHT = 284, 62, 7, 12, 64

local GOLD = { 1, 0.82, 0.45 }
local GOLD_DIM = { 0.74, 0.6, 0.36 }
local MUTED = { 0.55, 0.6, 0.68 }

-- Stock classes: colour, cell of the round icon sheet, and every name the client may give it (French and English,
-- both genders), lower case. Custom classes come from CustomClasses (CustomClasses.lua).
local CLASSES = {
    WARRIOR = { color = { 0.78, 0.61, 0.43 }, cell = 0, names = { "guerrier", "guerrière", "warrior" } },
    MAGE = { color = { 0.41, 0.8, 0.94 }, cell = 1, names = { "mage" } },
    ROGUE = { color = { 1, 0.96, 0.41 }, cell = 2, names = { "voleur", "voleuse", "rogue" } },
    DRUID = { color = { 1, 0.49, 0.04 }, cell = 3, names = { "druide", "druidesse", "druid" } },
    HUNTER = { color = { 0.67, 0.83, 0.45 }, cell = 4, names = { "chasseur", "chasseresse", "hunter" } },
    SHAMAN = { color = { 0.0, 0.44, 0.87 }, cell = 5, names = { "chaman", "chamane", "shaman" } },
    PRIEST = { color = { 1, 1, 1 }, cell = 6, names = { "prêtre", "prêtresse", "priest" } },
    WARLOCK = { color = { 0.58, 0.51, 0.79 }, cell = 7, names = { "démoniste", "warlock" } },
    PALADIN = { color = { 0.96, 0.55, 0.73 }, cell = 8, names = { "paladin" } },
    DEATHKNIGHT = { color = { 0.77, 0.12, 0.23 }, cell = 9, names = { "chevalier de la mort", "death knight" } },
}

local classByName

-- A character's class from the name the character list gives it. Feminine names the table does not list
-- ("Pestiférée") start with the masculine one, so a prefix match finds them.
local function FindClass(className)
    if not classByName then
        classByName = {}
        for _, class in pairs(CLASSES) do
            for _, name in ipairs(class.names) do
                classByName[name] = class
            end
        end
        for _, custom in pairs(CustomClasses or {}) do
            classByName[string.lower(custom.name)] = {
                color = custom.color, cell = custom.iconCell[2] * 4 + custom.iconCell[1],
            }
        end
    end
    local key = string.lower(className or "")
    if classByName[key] then
        return classByName[key]
    end
    for name, class in pairs(classByName) do
        if string.sub(key, 1, string.len(name)) == name then
            return class
        end
    end
    return { color = { 0.8, 0.8, 0.8 } }
end

local function Text(parent, font, size, flags)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(font, size, flags or "")
    text:SetShadowColor(0, 0, 0, 1)
    text:SetShadowOffset(1, -1)
    return text
end

local function Solid(parent, layer, r, g, b, a)
    local texture = parent:CreateTexture(nil, layer)
    texture:SetTexture(WHITE)
    texture:SetVertexColor(r, g, b, a or 1)
    return texture
end

-- --- a card ------------------------------------------------------------------------------------------------------

local function SetEdges(card, r, g, b, a)
    for _, edge in ipairs(card.edges) do
        edge:SetVertexColor(r, g, b, a)
    end
end

local function RefreshLook(button)
    local card = button.evolutions
    if button.evolutionsSelected then
        SetEdges(card, GOLD[1], GOLD[2], GOLD[3], 1)
        card.shine:Show()
        card.name:SetTextColor(1, 1, 1)
    elseif button.evolutionsHovered then
        SetEdges(card, GOLD[1], GOLD[2], GOLD[3], 0.65)
        card.shine:Hide()
        card.name:SetTextColor(1, 0.9, 0.62)
    else
        SetEdges(card, GOLD_DIM[1], GOLD_DIM[2], GOLD_DIM[3], 0.3)
        card.shine:Hide()
        card.name:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    end
end

local function Build(button)
    if button.evolutions then
        return button.evolutions
    end
    -- The package's own textures stay, hidden: its code still reaches for some of them
    for _, region in ipairs({ button:GetRegions() }) do
        if region:GetObjectType() == "Texture" then
            region:SetAlpha(0)
        end
    end
    local text = _G[button:GetName() .. "ButtonText"]
    if text then text:Hide() end
    button:SetHitRectInsets(0, 0, 0, 0)

    local card = CreateFrame("Frame", nil, button)
    card:SetAllPoints()
    card:SetFrameLevel(button:GetFrameLevel())
    button.evolutions = card

    card.background = card:CreateTexture(nil, "BACKGROUND")
    card.background:SetTexture(WHITE)
    card.background:SetAllPoints()
    -- Selected: light from the left edge
    card.shine = card:CreateTexture(nil, "BORDER")
    card.shine:SetTexture(WHITE)
    card.shine:SetBlendMode("ADD")
    card.shine:SetPoint("TOPLEFT")
    card.shine:SetPoint("BOTTOMRIGHT", card, "BOTTOM", 0, 0)
    card.shine:SetGradientAlpha("HORIZONTAL", GOLD[1], GOLD[2], GOLD[3], 0.18, GOLD[1], GOLD[2], GOLD[3], 0)

    card.edges = {}
    for _, points in ipairs({ { "TOPLEFT", "TOPRIGHT" }, { "BOTTOMLEFT", "BOTTOMRIGHT" } }) do
        local edge = Solid(card, "OVERLAY", 1, 1, 1, 1)
        edge:SetPoint(points[1])
        edge:SetPoint(points[2])
        edge:SetHeight(1)
        card.edges[#card.edges + 1] = edge
    end
    for _, points in ipairs({ { "TOPLEFT", "BOTTOMLEFT" }, { "TOPRIGHT", "BOTTOMRIGHT" } }) do
        local edge = Solid(card, "OVERLAY", 1, 1, 1, 1)
        edge:SetPoint(points[1])
        edge:SetPoint(points[2])
        edge:SetWidth(1)
        card.edges[#card.edges + 1] = edge
    end

    card.icon = card:CreateTexture(nil, "ARTWORK")
    card.icon:SetTexture(ROUND_ICONS)
    card.icon:SetSize(44, 44)
    card.icon:SetPoint("LEFT", 10, 0)

    card.name = Text(card, FONT_TEXT, 14)
    card.name:SetPoint("TOPLEFT", card.icon, "TOPRIGHT", 9, 0)
    card.name:SetJustifyH("LEFT")
    card.info = Text(card, FONT_TEXT, 11)
    card.info:SetPoint("TOPLEFT", card.name, "BOTTOMLEFT", 0, -3)
    card.info:SetJustifyH("LEFT")
    card.zone = Text(card, FONT_TEXT, 10)
    card.zone:SetPoint("TOPLEFT", card.info, "BOTTOMLEFT", 0, -3)
    card.zone:SetJustifyH("LEFT")
    card.zone:SetWidth(150)
    card.zone:SetHeight(12)

    -- Right: Prestige medallion above, Paragon level below
    card.prestige = card:CreateTexture(nil, "ARTWORK")
    card.prestige:SetTexture(PRESTIGE_EMBLEM)
    card.prestige:SetSize(34, 34)
    card.prestige:SetPoint("TOPRIGHT", -10, -4)
    card.prestigeText = Text(card, FONT_TITLE, 14)
    card.prestigeText:SetPoint("CENTER", card.prestige, "CENTER", 0, 1)
    card.prestigeText:SetTextColor(1, 0.9, 0.6)
    card.paragon = card:CreateTexture(nil, "ARTWORK")
    card.paragon:SetTexture(PARAGON_RING)
    card.paragon:SetSize(14, 14)
    card.paragonText = Text(card, FONT_TEXT, 11)
    card.paragonText:SetPoint("BOTTOMRIGHT", -12, 7)
    card.paragonText:SetTextColor(0.62, 0.82, 1)
    card.paragon:SetPoint("RIGHT", card.paragonText, "LEFT", -3, 0)

    -- The free slot's content
    card.plus = Text(card, FONT_TITLE, 30)
    card.plus:SetText("+")
    card.plus:SetPoint("LEFT", 22, 0)
    card.create = Text(card, FONT_TEXT, 13)
    card.create:SetPoint("LEFT", card.plus, "RIGHT", 16, 1)

    button:HookScript("OnEnter", function(self)
        self.evolutionsHovered = true
        RefreshLook(self)
    end)
    button:HookScript("OnLeave", function(self)
        self.evolutionsHovered = false
        RefreshLook(self)
    end)
    return card
end

local function ShowCharacterParts(card, shown)
    for _, part in ipairs({ card.icon, card.name, card.info, card.zone }) do
        if shown then part:Show() else part:Hide() end
    end
    for _, part in ipairs({ card.plus, card.create }) do
        if shown then part:Hide() else part:Show() end
    end
end

local function ShowBadge(texture, text, value, format)
    if value and value > 0 then
        texture:Show()
        text:SetText(string.format(format, value))
        text:Show()
    else
        texture:Hide()
        text:Hide()
    end
end

local function RefreshBadges(button)
    local card = button.evolutions
    local prestige, paragon
    if GetCharacterEvolution and button.evolutionsIndex then
        prestige, paragon = GetCharacterEvolution(button.evolutionsIndex)
    end
    ShowBadge(card.prestige, card.prestigeText, prestige, "%d")
    ShowBadge(card.paragon, card.paragonText, paragon, "%d")
    button.evolutionsPrestige, button.evolutionsParagon = prestige, paragon
end

-- The screen draws the list as soon as the client has it, which can be a moment before the DLL has read the
-- Prestige and Paragon out of it: the badges are looked at again on the next frame
local refresher = CreateFrame("Frame")
refresher:Hide()
refresher:SetScript("OnUpdate", function(self)
    self:Hide()
    for index = 1, MAX_CHARACTERS_DISPLAYED do
        local button = _G["CharSelectCharacterButton" .. index]
        if button and button.evolutions and not button.evolutionsEmpty then
            RefreshBadges(button)
        end
    end
    EvolutionsRoster_UpdateHeadline(CharacterSelect.selectedIndex)
end)

-- --- used by CharacterSelect.lua ----------------------------------------------------------------------------------

function EvolutionsRoster_Layout()
    local frame = CharacterSelectCharacterFrame
    frame:SetWidth(PANEL_WIDTH)
    frame:SetHeight(HEADER_HEIGHT + MAX_CHARACTERS_DISPLAYED * (CARD_HEIGHT + CARD_GAP) + PANEL_PADDING)
    frame:SetBackdrop({
        bgFile = WHITE,
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    frame:SetBackdropColor(0.02, 0.025, 0.04, 0.82)
    frame:SetBackdropBorderColor(GOLD_DIM[1], GOLD_DIM[2], GOLD_DIM[3], 0.8)

    CharSelectRealmName:SetFont(FONT_TITLE, 22)
    CharSelectRealmName:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    CharSelectRealmName:ClearAllPoints()
    CharSelectRealmName:SetPoint("TOP", frame, "TOP", 0, -12)
    CharSelectRealmName:SetWidth(PANEL_WIDTH - 20)
    CharSelectRealmName:SetHeight(26)
    if not frame.evolutionsCount then
        frame.evolutionsCount = Text(frame, FONT_TEXT, 10)
        frame.evolutionsCount:SetPoint("TOP", CharSelectRealmName, "BOTTOM", 0, -2)
        frame.evolutionsCount:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    end

    for index = 1, MAX_CHARACTERS_DISPLAYED do
        local button = _G["CharSelectCharacterButton" .. index]
        button:ClearAllPoints()
        button:SetPoint("TOPLEFT", frame, "TOPLEFT", PANEL_PADDING,
            -HEADER_HEIGHT - (index - 1) * (CARD_HEIGHT + CARD_GAP))
        button:SetWidth(PANEL_WIDTH - 2 * PANEL_PADDING)
        button:SetHeight(CARD_HEIGHT)
        Build(button)
    end
    CharSelectCreateCharacterButton:Hide()
    -- Under the list, out of the last card's way
    CharacterSelectDeleteButton:ClearAllPoints()
    CharacterSelectDeleteButton:SetPoint("TOPRIGHT", frame, "BOTTOMRIGHT", -2, -6)
    -- The selected character's name sits higher, to leave room for its details line under it
    CharSelectCharacterName:ClearAllPoints()
    CharSelectCharacterName:SetPoint("BOTTOM", CharSelectEnterWorldButton, "TOP", 0, 26)
end

function EvolutionsRoster_SetCount(count)
    local frame = CharacterSelectCharacterFrame
    if frame.evolutionsCount then
        frame.evolutionsCount:SetText(string.format("%d / %d personnages", count, MAX_CHARACTERS_PER_REALM))
    end
end

function EvolutionsRoster_Paint(button, index, name, race, class, level, zone, sex, ghost)
    local card = Build(button)
    local info = FindClass(class)
    local r, g, b = info.color[1], info.color[2], info.color[3]
    ShowCharacterParts(card, true)
    button.evolutionsEmpty = nil

    card.background:SetGradientAlpha("HORIZONTAL", r * 0.22, g * 0.22, b * 0.22, 0.92, 0.035, 0.04, 0.055, 0.92)
    if info.cell then
        local column, row = info.cell % 4, math.floor(info.cell / 4)
        card.icon:SetTexCoord(column / 4, (column + 1) / 4, row / 4, (row + 1) / 4)
        card.icon:Show()
    else
        card.icon:Hide()
    end

    card.name:SetText(name)
    card.info:SetText(string.format("|cffffffff%d|r  |cff%02x%02x%02x%s|r", level, r * 255, g * 255, b * 255, class))
    if ghost then
        card.zone:SetText("|cffff5a4aMort|r  " .. (zone or ""))
    else
        card.zone:SetText(zone or "")
    end
    card.zone:SetTextColor(MUTED[1], MUTED[2], MUTED[3])

    button.evolutionsIndex = index
    RefreshBadges(button)
    refresher:Show()
    RefreshLook(button)
    button:Show()
end

-- A slot without a character: the first one offers to create one, the others stay out of the way
function EvolutionsRoster_PaintEmpty(button, canCreate)
    local card = Build(button)
    ShowCharacterParts(card, false)
    ShowBadge(card.prestige, card.prestigeText, 0)
    ShowBadge(card.paragon, card.paragonText, 0)
    button.evolutionsEmpty = true
    button.evolutionsSelected = nil
    button.evolutionsIndex = nil
    if canCreate then
        card.background:SetGradientAlpha("HORIZONTAL", 0.12, 0.1, 0.06, 0.85, 0.035, 0.04, 0.055, 0.85)
        card.plus:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        card.create:SetText("Nouveau personnage")
        card.create:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        RefreshLook(button)
        button:Show()
    else
        button:Hide()
    end
end

function EvolutionsRoster_Select(selectedIndex)
    for index = 1, MAX_CHARACTERS_DISPLAYED do
        local button = _G["CharSelectCharacterButton" .. index]
        button.evolutionsSelected = (index == selectedIndex) and not button.evolutionsEmpty or nil
        if button.evolutions then
            RefreshLook(button)
        end
    end
end

-- The selected character's name over the scene, and under it what the list shows of it
function EvolutionsRoster_UpdateHeadline(index)
    if not CharSelectCharacterName then return end
    if not CharacterSelect.evolutionsHeadline then
        local line = Text(CharacterSelectUI, FONT_TEXT, 13)
        line:SetPoint("TOP", CharSelectCharacterName, "BOTTOM", 0, -4)
        CharacterSelect.evolutionsHeadline = line
    end
    local line = CharacterSelect.evolutionsHeadline
    local button = _G["CharSelectCharacterButton" .. (index or 0)]
    if not index or index < 1 or index > GetNumCharacters() or not button then
        line:SetText("")
        return
    end
    local _, _, class, level = GetCharacterInfo(index)
    local info = FindClass(class)
    local parts = { string.format("Niveau %d |cff%02x%02x%02x%s|r", level or 0, info.color[1] * 255,
        info.color[2] * 255, info.color[3] * 255, class or "") }
    if button.evolutionsPrestige and button.evolutionsPrestige > 0 then
        parts[#parts + 1] = string.format("|cffffd27fPrestige %d|r", button.evolutionsPrestige)
    end
    if button.evolutionsParagon and button.evolutionsParagon > 0 then
        parts[#parts + 1] = string.format("|cff9fd2ffParangon %d|r", button.evolutionsParagon)
    end
    line:SetText(table.concat(parts, "   |cff6f7a88·|r   "))
end
