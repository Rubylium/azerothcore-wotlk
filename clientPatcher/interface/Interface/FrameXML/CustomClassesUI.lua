-- In-game support for custom classes (see localTools/customClasses).
-- The interface and addons look classes up by their class token: class colors, localized names, icon
-- coordinates and the sort order. A class the client does not know about makes them return nil, which is
-- where addons such as Details or unit frame addons throw errors. Registering them here prevents that.

local ICON_COLUMNS, ICON_ROWS = 4, 4

local function register()
    if not CustomClasses then return end

    for _, class in pairs(CustomClasses) do
        local token = class.token
        local red, green, blue = class.color[1], class.color[2], class.color[3]

        if RAID_CLASS_COLORS then
            RAID_CLASS_COLORS[token] = RAID_CLASS_COLORS[token]
                or { r = red, g = green, b = blue, colorStr = string.format("ff%02x%02x%02x", red * 255,
                    green * 255, blue * 255) }
        end
        if CUSTOM_CLASS_COLORS then
            CUSTOM_CLASS_COLORS[token] = CUSTOM_CLASS_COLORS[token] or RAID_CLASS_COLORS[token]
        end

        if CLASS_ICON_TCOORDS then
            local column, row = class.iconCell[1], class.iconCell[2]
            CLASS_ICON_TCOORDS[token] = CLASS_ICON_TCOORDS[token]
                or { column / ICON_COLUMNS, (column + 1) / ICON_COLUMNS, row / ICON_ROWS, (row + 1) / ICON_ROWS }
        end

        if LOCALIZED_CLASS_NAMES_MALE then
            LOCALIZED_CLASS_NAMES_MALE[token] = LOCALIZED_CLASS_NAMES_MALE[token] or class.name
        end
        if LOCALIZED_CLASS_NAMES_FEMALE then
            LOCALIZED_CLASS_NAMES_FEMALE[token] = LOCALIZED_CLASS_NAMES_FEMALE[token] or class.name
        end

        if CLASS_SORT_ORDER and not tContains(CLASS_SORT_ORDER, token) then
            tinsert(CLASS_SORT_ORDER, token)
            if CLASS_SORT_ORDER_INDEX then
                CLASS_SORT_ORDER_INDEX[token] = #CLASS_SORT_ORDER
            end
        end
    end
end

-- Dungeon Finder roles are not handled here: Wow.exe decides them from its own class table, which the
-- patcher extends for the custom classes (clientPatcher/template/Patch-WowExe.ps1). Overriding
-- GetAvailableRoles or SetLFGRoles in Lua cannot work: the engine filters stored roles and checks the join
-- against that table regardless of what the interface shows.

-- Details (the damage meter) keeps its own class tables, keyed by class token and by spec id, and reads them with
-- unpack(). The custom classes' and specializations' entries, for those tables (DetailsCustomClasses uses these too).
CustomClassesDetails = {}

-- The texture coordinates of the class in Details' 4x4 class icon sheet. The patcher paints the class's icon
-- into one of the sheet's free cells, whole or a quadrant of it (localTools/interface/buildDetailsClassIcons.py:
-- { column, row } or { column, row, quadrant }, 0 top left to 3 bottom right); without one, the unknown cell
-- keeps the bar correct.
local function detailsIconCoords(class)
    local cell = class.detailsCell
    if not cell then
        return { 0.75, 1, 0.75, 1 }
    end
    local left, top, size = cell[1] / 4, cell[2] / 4, 1 / 4
    if cell[3] then
        size = 1 / 8
        left = left + (cell[3] % 2) * size
        top = top + math.floor(cell[3] / 2) * size
    end
    return { left, left + size, top, top + size }
end

-- Writes the custom classes into one set of class tables.
--
-- The colour is always written, never "kept if already there". class_colors is saved in Details' profile, so
-- the first colour a custom class was ever registered with was written into every player's SavedVariables -
-- and a keep-if-present check then preserved that first colour forever. When the Oathblade moved from its
-- warrior tan to bright blue, Details went on drawing tan for anyone who had logged in before. The class
-- colour is the server's to decide, and CustomClasses is where it is decided.
function CustomClassesDetails.addClasses(colors, coords)
    if not CustomClasses then
        return
    end
    for _, class in pairs(CustomClasses) do
        if type(colors) == "table" then
            colors[class.token] = { class.color[1], class.color[2], class.color[3] }
        end
        if type(coords) == "table" then
            coords[class.token] = detailsIconCoords(class)
        end
    end
end

-- A custom class's specializations: Details knows a spec by a number, and draws its icon from the 8x8 spec sheet at
-- class_specs_coords[spec id]. The patcher paints each spec into a free cell (buildDetailsClassIcons.py, from the
-- class's detailsSpecs); its id is class id * 100 + tab + 1, clear of every Blizzard spec id.
function CustomClassesDetails.addSpecs(specCoords)
    if type(specCoords) ~= "table" or not CustomClasses then
        return
    end
    for classId, class in pairs(CustomClasses) do
        for index, cell in ipairs(class.specCells or {}) do
            local left, top = cell[1] / 8, cell[2] / 8
            specCoords[classId * 100 + index] = { left, left + 1 / 8, top, top + 1 / 8 }
        end
    end
    -- A stock class's specializations past its three tabs (the Warrior's Gladiateur, tab 3): the same ids
    for classId, specs in pairs(CustomStockSpecs or {}) do
        for tab, cell in pairs(specs) do
            local left, top = cell[1] / 8, cell[2] / 8
            specCoords[classId * 100 + tab + 1] = { left, left + 1 / 8, top, top + 1 / 8 }
        end
    end
end

-- Details draws the fights it saved the moment it loads (its own ADDON_LOADED), before DetailsCustomClasses - which
-- depends on it - is even loaded: a custom class's or spec's bar errored there (classe_damage.lua SetClassIcon). This
-- frame, made before any addon, hears Details' ADDON_LOADED first: the entries go into its default profile and every
-- saved one before it reads them.
local function teachDetails()
    if not _detalhes then
        return
    end
    local profiles = { _detalhes.default_profile }
    if type(_detalhes_global) == "table" and type(_detalhes_global.__profiles) == "table" then
        for _, profile in pairs(_detalhes_global.__profiles) do
            tinsert(profiles, profile)
        end
    end
    for _, profile in ipairs(profiles) do
        if type(profile) == "table" then
            CustomClassesDetails.addClasses(profile.class_colors, profile.class_coords)
            CustomClassesDetails.addSpecs(profile.class_specs_coords)
        end
    end
end

-- The raid window's class buttons (Blizzard_RaidUI) are made for the stock classes only: RaidClassButton1-10, then
-- pets, main tanks and main assists on 11-13. It numbers them from CLASS_SORT_ORDER as it loads, so the custom classes
-- registered above took 11 and up - the pets' buttons, and buttons that do not exist (an error on every raid
-- update). The custom classes have no button there: they are counted in their groups only.
local function fixRaidClassButtons()
    if not RAID_CLASS_BUTTONS or not CustomClasses then
        return
    end
    for _, class in pairs(CustomClasses) do
        RAID_CLASS_BUTTONS[class.token] = nil
    end
end

local loader = CreateFrame("Frame")
loader:RegisterEvent("PLAYER_LOGIN")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(self, event, addon)
    if event == "PLAYER_LOGIN" then
        register()
    elseif addon == "Details" then
        teachDetails()
    elseif addon == "Blizzard_RaidUI" then
        fixRaidClassButtons()
    end
end)
register()
