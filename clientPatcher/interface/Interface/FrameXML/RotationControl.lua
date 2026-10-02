-- The rotation's panel: while ".cheat rotation" is on (mod-playerbots PlayerbotRotationCommand.cpp), a small card to
-- pause it, resume it or turn it off. The server whispers the state on the "Rotation" addon prefix (STATE 0 off,
-- 1 running, 2 paused); the buttons whisper PAUSE, RESUME and OFF back, and HELLO asks for the state after a login
-- or a reload. Dragged anywhere with the left button; the client keeps where it was left (a named, user-placed frame).

local PREFIX = "Rotation"
local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Rotation",
    running = "Active",
    paused = "En pause",
    pause = "Pause",
    resume = "Reprendre",
    stop = "Arrêter",
    hint = "Clic gauche et glisser pour déplacer.",
} or {
    title = "Rotation",
    running = "Running",
    paused = "Paused",
    pause = "Pause",
    resume = "Resume",
    stop = "Stop",
    hint = "Left-click and drag to move.",
}

-- The challenge board's palette (ChallengeBoard.lua)
local GOLD = { 1, 0.82, 0.3 }
local PARCHMENT = { 1, 0.9, 0.7 }
local MUTED = { 0.62, 0.57, 0.5 }

local function Send(command)
    SendAddonMessage(PREFIX, command, "WHISPER", UnitName("player"))
end

local frame = CreateFrame("Frame", "EvolutionsRotationControl", UIParent)
frame:SetSize(196, 74)
frame:SetPoint("TOP", UIParent, "TOP", 0, -140)
frame:SetFrameStrata("MEDIUM")
frame:SetClampedToScreen(true)
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    self:SetUserPlaced(true)
end)
frame:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
frame:SetBackdropColor(0.04, 0.03, 0.02, 0.92)
frame:SetBackdropBorderColor(0.75, 0.6, 0.35)
frame:Hide()

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
title:SetPoint("TOPLEFT", 12, -10)
title:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
title:SetText(TEXT.title)

local status = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
status:SetPoint("TOPRIGHT", -12, -12)

local toggle = CreateFrame("Button", "EvolutionsRotationControlToggle", frame, "UIPanelButtonTemplate")
toggle:SetSize(84, 22)
toggle:SetPoint("BOTTOMLEFT", 10, 10)

local stop = CreateFrame("Button", "EvolutionsRotationControlStop", frame, "UIPanelButtonTemplate")
stop:SetSize(84, 22)
stop:SetPoint("BOTTOMRIGHT", -10, 10)
stop:SetText(TEXT.stop)
stop:SetScript("OnClick", function() Send("OFF") end)

local paused = false

local function Show(state)
    if state == "0" then
        frame:Hide()
        return
    end
    paused = state == "2"
    if paused then
        status:SetText(TEXT.paused)
        status:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        toggle:SetText(TEXT.resume)
    else
        status:SetText(TEXT.running)
        status:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
        toggle:SetText(TEXT.pause)
    end
    frame:Show()
end

toggle:SetScript("OnClick", function() Send(paused and "RESUME" or "PAUSE") end)

frame:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_TOP")
    GameTooltip:SetText(TEXT.title, GOLD[1], GOLD[2], GOLD[3])
    GameTooltip:AddLine(TEXT.hint, PARCHMENT[1], PARCHMENT[2], PARCHMENT[3], 1)
    GameTooltip:Show()
end)
frame:SetScript("OnLeave", GameTooltip_Hide)

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "PLAYER_ENTERING_WORLD" then
        Send("HELLO")
        return
    end
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    local kind, state = strsplit("\t", message)
    if kind == "STATE" then
        Show(state)
    end
end)
