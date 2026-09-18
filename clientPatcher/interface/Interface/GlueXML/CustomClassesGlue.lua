-- Character creation support for custom classes (see localTools/customClasses).
-- CustomClasses.lua is generated from classes.json and lists each class by its ChrClasses id.
-- This file gives the creation screen what it needs for them: a class button, the icon cell in the class
-- sheet, and the texts it reads when a class is hovered or selected.

local CLASS_BUTTONS = 10          -- buttons the screen XML already declares (Blizzard's classes plus one)
local BUTTON_SPACING = 110        -- matches the class row anchors in CharacterCreate.xml
local ICON_COLUMNS, ICON_ROWS = 4, 4

local function iconCoords(cell)
    local column, row = cell[1], cell[2]
    return { column / ICON_COLUMNS, (column + 1) / ICON_COLUMNS, row / ICON_ROWS, (row + 1) / ICON_ROWS }
end

-- The class row is a chain of buttons, each anchored to the previous one, so a new one continues the chain
local function ensureButton(index)
    local name = "CharCreateClassButton" .. index
    if _G[name] then return _G[name] end

    local previous = _G["CharCreateClassButton" .. (index - 1)]
    if not previous then return nil end

    local button = CreateFrame("CheckButton", name, previous:GetParent(), "CharCreateClassButtonTemplate")
    button:SetID(index)
    button:SetPoint("BOTTOM", previous, "BOTTOMLEFT", BUTTON_SPACING, 0)
    button:SetScript("OnClick", function(self)
        CharacterClass_OnClick(self, self:GetID())
    end)
    button:Hide()
    return button
end

local function registerCustomClasses()
    if not CustomClasses then return end

    local count = 0
    for _, class in pairs(CustomClasses) do
        count = count + 1
        CLASS_ICON_TCOORDS[class.token] = iconCoords(class.iconCell)
        -- The screen concatenates these directly, so they must exist for every class it can show
        _G["CLASS_" .. class.token] = _G["CLASS_" .. class.token] or class.name
        _G["CLASS_INFO_" .. class.token .. "0"] = _G["CLASS_INFO_" .. class.token .. "0"] or class.name
        _G[class.token .. "_DISABLED"] = _G[class.token .. "_DISABLED"] or CLASS_DISABLED
    end

    -- Blizzard's ten classes, the spare button the screen already has, and one per custom class
    MAX_CLASSES_PER_RACE = CLASS_BUTTONS + 1 + count
    for index = CLASS_BUTTONS + 2, MAX_CLASSES_PER_RACE do
        ensureButton(index)
    end
end

local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(self)
    registerCustomClasses()
    self:UnregisterAllEvents()
end)
registerCustomClasses()
