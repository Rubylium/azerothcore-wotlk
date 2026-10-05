-- Character creation support for custom classes (see localTools/customClasses), on the retail glue package
-- (clientPatcher/vendor/retail-glue, Noa-1995's WoW Retail Interface, adapted with his permission).
-- CustomClasses.lua is generated from classes.json and lists each class by its ChrClasses id.
--
-- The package declares ten class buttons, CharacterCreateClassButton1..10, in
-- CharacterCreateClassButtonsContainer, and lays them out from Lua. A custom class gets one more button. Every class,
-- stock or custom, is drawn as retail draws it: its round icon in the metal ring (RoundClasses, retail's own
-- character creation art, localTools/interface/makeRoundClassIcons.py), a shadow behind, a silver glow under the
-- pointer, the gold ring on the chosen one. The classes the chosen race offers stand in one row along the bottom,
-- evenly spaced and centred; past MAX_PER_ROW they take two rows, and the gender buttons go up as much.

local STOCK_BUTTONS = 10        -- CharacterCreateClassButton1..10, declared in CharacterCreate.xml
local BUTTON_SIZE = 56          -- the round icon, its ring included
local BUTTON_SPACING = 66       -- centre to centre
local ROW_HEIGHT = 64           -- the bottom row's centre above the bottom of the screen
local ROW_SPACING = 70          -- between rows
local MAX_PER_ROW = 15          -- 990 pixels: clear of the race columns on a 16:9 screen
local GLOW_SCALE = 1.36         -- RoundHighlight / RoundSelected are drawn this much larger than the button
local GENDER_SPACING, GENDER_HEIGHT = 310, -250     -- CharacterCreate_PositionGenderButtons
local ICON_COLUMNS, ICON_ROWS = 4, 4
local ART = "Interface\\Glues\\CharacterCreate\\"
local ICON_TEXTURE = ART .. "RoundClasses"
-- The stock classes' cells in RoundClasses (makeRoundClassIcons.py CLASS_CELLS); the custom ones come with theirs
local STOCK_CELLS = {
    WARRIOR = { 0, 0 }, MAGE = { 1, 0 }, ROGUE = { 2, 0 }, DRUID = { 3, 0 },
    HUNTER = { 0, 1 }, SHAMAN = { 1, 1 }, PRIEST = { 2, 1 }, WARLOCK = { 3, 1 },
    PALADIN = { 0, 2 }, DEATHKNIGHT = { 1, 2 },
}

local byToken = {}

-- What the class tooltip shows (the package reads Class_Informations by button position, CharacterInfo.lua).
-- The role names are the ones CharacterCreate.lua colours, so they are spelled exactly as it expects.
local CLASS_INFORMATION = {
    PESTIFERE = {
        Name = "Pestiféré",
        Description = "Porteur d'une peste qu'il maîtrise, le Pestiféré contamine ses ennemis et en tire sa force."
            .. " Sous la Carapace nécrosée, il encaisse les coups et attire la haine des monstres ; en Sangsue, il"
            .. " échange les dégâts de ses coups contre des soins pour son groupe.",
        Roles = "Dégâts de mêlée, Tank, Soigneur.",
    },
    NECROMANCER = {
        Name = "Nécromancien",
        Description = "Maître des arts funèbres, le Nécromancien lève des serviteurs temporaires et dirige"
            .. " une armée de morts au corps à corps comme à distance.",
        Roles = "Dégâts à distance.",
    },
    OATHBLADE = {
        Name = "Oathblade",
        Description = "Un escrimeur noble dont les techniques précises construisent un rythme qui explose en"
            .. " brèves séquences de coups fulgurants.",
        Roles = "Dégâts de mêlée.",
    },
    BARBARIAN = {
        Name = "Barbare",
        Description = "Venus des terres glacées du Nord, les barbares se battent comme leurs ancêtres avant eux :"
            .. " sans discipline ni pitié, portés par une rage qui ne s'éteint qu'avec le dernier ennemi. Certains"
            .. " fendent les rangs adverses à la hache, d'autres abattent leurs proies de loin sous une pluie de lances"
            .. " et de haches, et les plus vénérés appellent la force des ancêtres pour pousser leurs compagnons"
            .. " au-delà de leurs limites.",
        Roles = "Dégâts de mêlée, dégâts à distance ou soutien.",
    },
}

local function iconCoords(cell)
    local column, row = cell[1], cell[2]
    return { column / ICON_COLUMNS, (column + 1) / ICON_COLUMNS, row / ICON_ROWS, (row + 1) / ICON_ROWS }
end

-- The class row is a chain: each button hangs off the previous one, so a new one continues it
local function ensureButton(index)
    local name = "CharacterCreateClassButton" .. index
    if _G[name] then
        return _G[name]
    end

    local previous = _G["CharacterCreateClassButton" .. (index - 1)]
    if not previous then
        return nil
    end
    local button = CreateFrame("CheckButton", name, previous:GetParent(), "CharacterCreateClassButtonTemplate")
    button:SetID(index)
    button:SetPoint("LEFT", previous, "RIGHT", BUTTON_SPACING - 38, 0)
    button:Hide()
    return button
end

local classRows = 1

-- The gender buttons, above the class rows
local function placeGenderButtons()
    local raise = (classRows - 1) * ROW_SPACING
    if CharacterCreateGenderButtonMale then
        CharacterCreateGenderButtonMale:ClearAllPoints()
        CharacterCreateGenderButtonMale:SetPoint("CENTER", CharacterCreateFrame, "CENTER", -GENDER_SPACING / 2,
            GENDER_HEIGHT + raise)
    end
    if CharacterCreateGenderButtonFemale then
        CharacterCreateGenderButtonFemale:ClearAllPoints()
        CharacterCreateGenderButtonFemale:SetPoint("CENTER", CharacterCreateFrame, "CENTER", GENDER_SPACING / 2,
            GENDER_HEIGHT + raise)
    end
end

-- The classes the race offers in one row, or as few rows as fit MAX_PER_ROW, evened out, each centred, the first
-- ones on top (the package positions ten in one row, whatever is shown)
local function layoutRows(count)
    if not count or count < 1 then
        return
    end
    local rows = math.ceil(count / MAX_PER_ROW)
    local perRow = math.ceil(count / rows)
    for index = 1, count do
        local button = _G["CharacterCreateClassButton" .. index]
        if button then
            local row = math.floor((index - 1) / perRow)
            local inRow = (row == rows - 1) and (count - row * perRow) or perRow
            local column = (index - 1) % perRow
            button:ClearAllPoints()
            button:SetPoint("CENTER", CharacterCreateFrame, "BOTTOM", (column - (inRow - 1) / 2) * BUTTON_SPACING,
                ROW_HEIGHT + (rows - 1 - row) * ROW_SPACING)
        end
    end
    classRows = rows
    placeGenderButtons()
end

-- A class button as retail draws it, whatever the package gave it: the round icon in its ring at the button's size,
-- its shadow behind, the hover glow and the chosen ring (the package shows and hides checkedTexture itself), and none
-- of the package's square frames
local function dressButton(button, cell)
    if not button or not cell then
        return
    end
    local coords = iconCoords(cell)
    button:SetWidth(BUTTON_SIZE)
    button:SetHeight(BUTTON_SIZE)
    for _, part in ipairs({ button:GetNormalTexture(), button:GetPushedTexture() }) do
        if part then
            part:SetTexture(ICON_TEXTURE)
            part:SetTexCoord(coords[1], coords[2], coords[3], coords[4])
            part:ClearAllPoints()
            part:SetAllPoints(button)
        end
    end
    if button:GetPushedTexture() then
        button:GetPushedTexture():SetVertexColor(0.8, 0.8, 0.8)
    end

    if not button.roundShadow then
        button.roundShadow = button:CreateTexture(nil, "BACKGROUND")
        button.roundShadow:SetTexture(ART .. "RoundShadow")
        button.roundShadow:SetPoint("CENTER", button, "CENTER", 0, -2)
        button.roundShadow:SetWidth(BUTTON_SIZE * 1.1)
        button.roundShadow:SetHeight(BUTTON_SIZE * 1.1)
        button.roundShadow:SetDrawLayer("BACKGROUND", -1)
    end
    local glow = BUTTON_SIZE * GLOW_SCALE
    if button.staticTexture then
        button.staticTexture:SetTexture(nil)
    end
    if button.highlightTexture then
        button.highlightTexture:SetTexture(ART .. "RoundHighlight")
        button.highlightTexture:SetBlendMode("ADD")
        button.highlightTexture:SetVertexColor(1, 1, 1)
        button.highlightTexture:SetWidth(glow)
        button.highlightTexture:SetHeight(glow)
    end
    if button.checkedTexture then
        button.checkedTexture:SetTexture(ART .. "RoundSelected")
        button.checkedTexture:SetBlendMode("ADD")
        button.checkedTexture:SetWidth(glow)
        button.checkedTexture:SetHeight(glow)
    end
end

-- Only the chosen class keeps its name under its icon (retail names the others in their tooltip): names as long as
-- "Chevalier de la mort" would run into their neighbours
local function showChosenName()
    for index = 1, MAX_CLASSES_PER_RACE do
        local button = _G["CharacterCreateClassButton" .. index]
        if button and button.nameFrame then
            if CharacterCreate and CharacterCreate.selectedClass == index then
                button.nameFrame:Show()
            else
                button.nameFrame:Hide()
            end
        end
    end
end

local function registerCustomClasses()
    if not CustomClasses then
        return
    end

    local count = 0
    for _, class in pairs(CustomClasses) do
        count = count + 1
        byToken[class.token] = class
        CLASS_ICON_TCOORDS[class.token] = iconCoords(class.iconCell)
        -- The screen concatenates these directly, so they must exist for every class it can show
        _G["CLASS_" .. class.token] = _G["CLASS_" .. class.token] or class.name
        _G["CLASS_INFO_" .. class.token .. "0"] = _G["CLASS_INFO_" .. class.token .. "0"] or class.name
        _G[class.token .. "_DISABLED"] = _G[class.token .. "_DISABLED"] or CLASS_DISABLED
    end

    MAX_CLASSES_PER_RACE = STOCK_BUTTONS + count
    for index = STOCK_BUTTONS + 1, MAX_CLASSES_PER_RACE do
        ensureButton(index)
    end

    -- The client calls these; both are wrapped rather than replaced, so the package keeps doing its own work
    local enumerate = CharacterCreateEnumerateClasses
    CharacterCreateEnumerateClasses = function(...)
        enumerate(...)
        local index = 1
        for i = 1, select("#", ...), 3 do
            local token = strupper(select(i + 1, ...))
            local class = byToken[token]
            dressButton(_G["CharacterCreateClassButton" .. index], class and class.iconCell or STOCK_CELLS[token])
            -- The tooltip reads this by button position, which is where the class landed in the list
            if class and Class_Informations and CLASS_INFORMATION[token] then
                Class_Informations[index] = CLASS_INFORMATION[token]
            end
            index = index + 1
        end
        layoutRows(index - 1)
        showChosenName()
    end

    -- SetCharacterClass is where the screen records the chosen class (a click, a race that changes it)
    local setClass = SetCharacterClass
    if setClass then
        SetCharacterClass = function(...)
            setClass(...)
            showChosenName()
        end
    end

    local position = CharacterCreate_PositionClassButtons
    if position then
        CharacterCreate_PositionClassButtons = function(...)
            position(...)
            layoutRows(CharacterCreate and CharacterCreate.numClasses)
        end
    end
    -- The package puts them back at its own height after the classes: above the rows again
    local positionGender = CharacterCreate_PositionGenderButtons
    if positionGender then
        CharacterCreate_PositionGenderButtons = function(...)
            positionGender(...)
            placeGenderButtons()
        end
    end
end

registerCustomClasses()
