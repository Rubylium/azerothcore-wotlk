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

local loader = CreateFrame("Frame")
loader:RegisterEvent("PLAYER_LOGIN")
loader:SetScript("OnEvent", function(self)
    register()
    self:UnregisterAllEvents()
end)
register()
