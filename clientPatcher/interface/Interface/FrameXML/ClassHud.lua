-- The class HUD: one small, animated element per class that has its own resource (the Faucheur's souls first), in the
-- style of Ascension's class resources. Design and how to add a class: .agents/docs/systems/class-hud.md.
--
-- This file is the shared part: a frame the player drags anywhere (its position, scale and lock saved per character),
-- a right-click menu, the tooltip, the fade out of combat, and the feed: the server whispers the resource's state on
-- the addon channel ("<PREFIX>\t<payload>"), and a class may also read its auras when no message has come yet. A
-- class registers a definition (ClassHud_Register) and draws only its own art inside the frame it is handed.

CLASS_HUD_SETTINGS = CLASS_HUD_SETTINGS or {}
RegisterForSave("CLASS_HUD_SETTINGS")

local definitions = {}
local active
local hud
local state
local inCombat = false

local SCALES = { 0.5, 0.75, 1, 1.25, 1.5 }

local L = {
    LOCK = "Verrouiller",
    UNLOCK = "Déverrouiller",
    RESET = "Réinitialiser la position",
    SCALE = "Taille",
    HIDE_OOC = "Estomper hors combat",
    MOVE_HINT = "Clic droit : options",
    DRAG_HINT = "Glisser : déplacer",
}

-- Registers a class's HUD. definition:
--   token     class token (select(2, UnitClass("player")))
--   prefix    addon message prefix the server sends the state under
--   width, height   the frame's size at scale 1
--   anchor    default position { point, relativeTo, relativePoint, x, y }
--   title     tooltip title; tooltip(state) returns its lines
--   create(frame)        builds the class's art in the frame
--   parse(payload)       turns a message into a state (nil to ignore it)
--   fromAuras()          the state read from the player's auras (used until the first message)
--   update(frame, state, previous)   draws the state; previous is the last one drawn (nil at first)
--   isEmpty(state)       true when there is nothing to show (the frame fades out of combat)
--   isHidden(state)      optional: true when the HUD is not shown at all (state nil before the first message) - a
--                        class whose HUD belongs to one specialization only (the Warrior's Gladiateur)
function ClassHud_Register(definition)
    definitions[definition.token] = definition
end

local function settingsKey()
    local name, realm = UnitName("player"), GetRealmName()
    if not name or not realm then
        return nil
    end
    return name .. "-" .. realm
end

local function settings(create)
    local key = settingsKey()
    if not key then
        return nil
    end
    if create and not CLASS_HUD_SETTINGS[key] then
        CLASS_HUD_SETTINGS[key] = { scale = 3, fade = true }
    end
    return CLASS_HUD_SETTINGS[key]
end

local function applyPosition()
    local saved = settings()
    hud:ClearAllPoints()
    if saved and saved.point then
        hud:SetPoint(saved.point, UIParent, saved.relativePoint, saved.x, saved.y)
    else
        local anchor = active.anchor
        hud:SetPoint(anchor[1], _G[anchor[2]] or UIParent, anchor[3], anchor[4], anchor[5])
    end
end

local function applySettings()
    local saved = settings(true)
    hud:SetScale(SCALES[saved and saved.scale or 3] or 1)
    hud.locked = saved and saved.locked or false
    applyPosition()
end

local function refreshAlpha()
    local saved = settings()
    local fade = not saved or saved.fade ~= false
    local empty = active.isEmpty and state and active.isEmpty(state)
    local alpha = (fade and not inCombat and (empty or not state)) and 0.45 or 1
    UIFrameFadeIn(hud, 0.3, hud:GetAlpha(), alpha)
end

local function refreshVisibility()
    if active.isHidden and active.isHidden(state) then
        hud:Hide()
    else
        hud:Show()
    end
end

local function draw(newState)
    local previous = state
    state = newState
    active.update(hud.content, state, previous)
    refreshVisibility()
    refreshAlpha()
end

-- --- The right-click menu ---------------------------------------------------------------------------------------
local menu = CreateFrame("Frame", "ClassHudMenu", UIParent, "UIDropDownMenuTemplate")

local function initializeMenu(_, level)
    local saved = settings(true)
    local info = UIDropDownMenu_CreateInfo()
    info.text = active.title
    info.isTitle = true
    info.notCheckable = true
    UIDropDownMenu_AddButton(info, level)

    info = UIDropDownMenu_CreateInfo()
    info.text = hud.locked and L.UNLOCK or L.LOCK
    info.notCheckable = true
    info.func = function()
        saved.locked = not hud.locked
        hud.locked = saved.locked
    end
    UIDropDownMenu_AddButton(info, level)

    info = UIDropDownMenu_CreateInfo()
    info.text = L.HIDE_OOC
    info.checked = saved.fade ~= false
    info.keepShownOnClick = true
    info.func = function()
        saved.fade = saved.fade == false
        refreshAlpha()
    end
    UIDropDownMenu_AddButton(info, level)

    info = UIDropDownMenu_CreateInfo()
    info.text = L.RESET
    info.notCheckable = true
    info.func = function()
        saved.point, saved.relativePoint, saved.x, saved.y = nil, nil, nil, nil
        saved.scale = 3
        applySettings()
    end
    UIDropDownMenu_AddButton(info, level)

    info = UIDropDownMenu_CreateInfo()
    info.text = L.SCALE
    info.isTitle = true
    info.notCheckable = true
    UIDropDownMenu_AddButton(info, level)

    for index, scale in ipairs(SCALES) do
        info = UIDropDownMenu_CreateInfo()
        info.text = (scale * 100) .. "%"
        info.checked = (saved.scale or 3) == index
        info.func = function()
            saved.scale = index
            -- Keep the frame where it is on screen while it grows or shrinks
            local x, y = hud:GetCenter()
            local oldScale = hud:GetScale()
            hud:SetScale(scale)
            hud:ClearAllPoints()
            hud:SetPoint("CENTER", UIParent, "BOTTOMLEFT", x * oldScale / scale, y * oldScale / scale)
            saved.point, saved.relativePoint = "CENTER", "BOTTOMLEFT"
            saved.x, saved.y = x * oldScale / scale, y * oldScale / scale
        end
        UIDropDownMenu_AddButton(info, level)
    end
end

-- --- The frame ----------------------------------------------------------------------------------------------------
local function createHud(definition)
    hud = CreateFrame("Button", "ClassHudFrame", UIParent)
    hud:SetSize(definition.width, definition.height)
    hud:SetFrameStrata("MEDIUM")
    hud:SetMovable(true)
    hud:SetClampedToScreen(true)
    hud:EnableMouse(true)
    hud:RegisterForClicks("RightButtonUp")
    hud:RegisterForDrag("LeftButton")

    hud.content = CreateFrame("Frame", nil, hud)
    hud.content:SetAllPoints()
    definition.create(hud.content)

    hud:SetScript("OnDragStart", function(self)
        if not self.locked then
            self:StartMoving()
        end
    end)
    hud:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        local saved = settings(true)
        local point, _, relativePoint, x, y = self:GetPoint(1)
        saved.point, saved.relativePoint, saved.x, saved.y = point, relativePoint, x, y
    end)
    hud:SetScript("OnClick", function(self, button)
        if button == "RightButton" then
            GameTooltip:Hide()
            UIDropDownMenu_Initialize(menu, initializeMenu, "MENU")
            ToggleDropDownMenu(1, nil, menu, "cursor", 0, 0)
        end
    end)
    hud:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_BOTTOMRIGHT")
        GameTooltip:AddLine(definition.title, 1, 0.82, 0)
        if definition.tooltip and state then
            for _, line in ipairs(definition.tooltip(state)) do
                GameTooltip:AddLine(line, 1, 1, 1, true)
            end
        end
        GameTooltip:AddLine(" ")
        GameTooltip:AddLine(L.MOVE_HINT, 0.6, 0.6, 0.6)
        if not self.locked then
            GameTooltip:AddLine(L.DRAG_HINT, 0.6, 0.6, 0.6)
        end
        GameTooltip:Show()
    end)
    hud:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)
    hud:Hide()
end

local function activate()
    local _, token = UnitClass("player")
    local definition = token and definitions[token]
    if not definition then
        if hud then
            hud:Hide()
        end
        active = nil
        return
    end
    if active ~= definition then
        active = definition
        if not hud then
            createHud(definition)
        end
        state = nil
    end
    applySettings()
    refreshVisibility()
    if definition.fromAuras then
        draw(definition.fromAuras())
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("VARIABLES_LOADED")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("PLAYER_REGEN_DISABLED")
listener:RegisterEvent("PLAYER_REGEN_ENABLED")
listener:RegisterEvent("UNIT_AURA")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, event, arg1, arg2)
    if event == "VARIABLES_LOADED" or event == "PLAYER_ENTERING_WORLD" then
        inCombat = UnitAffectingCombat("player") and true or false
        activate()
        return
    end
    if not active or not hud then
        return
    end
    if event == "PLAYER_REGEN_DISABLED" or event == "PLAYER_REGEN_ENABLED" then
        inCombat = event == "PLAYER_REGEN_DISABLED"
        refreshAlpha()
    elseif event == "UNIT_AURA" then
        -- The auras are only the fallback: once the server has spoken, its word stands
        if arg1 == "player" and active.fromAuras and not hud.heardServer then
            draw(active.fromAuras())
        end
    elseif event == "CHAT_MSG_ADDON" and arg1 == active.prefix then
        local parsed = active.parse(arg2)
        if parsed then
            hud.heardServer = true
            draw(parsed)
        end
    end
end)

SLASH_CLASSHUD1 = "/classhud"
SlashCmdList["CLASSHUD"] = function(message)
    if not active or not hud then
        return
    end
    local saved = settings(true)
    message = strlower(strtrim(message or ""))
    if message == "reset" then
        saved.point, saved.relativePoint, saved.x, saved.y = nil, nil, nil, nil
        saved.scale = 3
        applySettings()
    elseif message == "lock" or message == "unlock" then
        saved.locked = message == "lock"
        hud.locked = saved.locked
    end
end
