-- Character creation support for custom classes (see localTools/customClasses), on the retail glue package
-- (clientPatcher/vendor/retail-glue, Noa-1995's WoW Retail Interface, adapted with his permission).
-- CustomClasses.lua is generated from classes.json and lists each class by its ChrClasses id.
--
-- The package declares ten class buttons, CharacterCreateClassButton1..10, in
-- CharacterCreateClassButtonsContainer, and lays them out from Lua. A custom class gets one more button, and
-- the row is centred on however many classes the chosen race actually offers.

local STOCK_BUTTONS = 10        -- CharacterCreateClassButton1..10, declared in CharacterCreate.xml
local BUTTON_SPACING = 80       -- CharacterCreate_PositionClassButtons
local ROW_HEIGHT = 80           -- its distance above the bottom of the screen
local ICON_COLUMNS, ICON_ROWS = 4, 4
-- Our own round class icons: the package's own atlas has no room for another class
local ICON_TEXTURE = "Interface\\Glues\\CharacterCreate\\RoundClasses"

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

-- The whole row, centred on the number of classes the race offers (the package positions ten, whatever is shown)
local function centerRow(count)
    local first = CharacterCreateClassButton1
    if not first or not count or count < 1 then
        return
    end
    first:ClearAllPoints()
    first:SetPoint("CENTER", CharacterCreateFrame, "BOTTOM", -(count - 1) * BUTTON_SPACING / 2, ROW_HEIGHT)
end

-- The package paints every class from one atlas; ours comes from its own file, with the cell classes.json names
local function dressCustomButton(button, class)
    if not button then
        return
    end
    local coords = iconCoords(class.iconCell)
    for _, part in ipairs({ button:GetNormalTexture(), button:GetPushedTexture() }) do
        if part then
            part:SetTexture(ICON_TEXTURE)
            part:SetTexCoord(coords[1], coords[2], coords[3], coords[4])
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
            if class then
                dressCustomButton(_G["CharacterCreateClassButton" .. index], class)
                -- The tooltip reads this by button position, which is where the class landed in the list
                if Class_Informations and CLASS_INFORMATION[token] then
                    Class_Informations[index] = CLASS_INFORMATION[token]
                end
            end
            index = index + 1
        end
        centerRow(index - 1)
    end

    local position = CharacterCreate_PositionClassButtons
    if position then
        CharacterCreate_PositionClassButtons = function(...)
            position(...)
            centerRow(CharacterCreate and CharacterCreate.numClasses)
        end
    end
end

registerCustomClasses()
