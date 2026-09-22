-- Prestige (mod-stat-growth PrestigeSystem.cpp).
--
-- The keeper in Stormwind opens this. The server sends the numbers and whether the reset is allowed; the
-- button only asks. PRESTIGE is the confirmation, and the server checks the level, combat and instance again.

local PREFIX = "Prestige"

local WINDOW_WIDTH, WINDOW_HEIGHT = 520, 512

-- The halo behind the medallion is a sequence of pre-turned frames: a texture cannot be rotated in 3.3.5,
-- so buildPrestigeArt.py bakes the rotation in and this cycles them. The spokes repeat every twelfth of a
-- turn, so the sequence loops seamlessly once it has covered one wedge.
local RAY_FRAMES = 24
local RAY_PERIOD = 2.6          -- seconds for the halo to turn by one spoke

-- Both columns sit on one grid, so a tick and a dash start their text in the same place.
local COLUMN_WIDTH = 214
local COLUMN_LEFT = 32
local COLUMN_RIGHT = 520 - 32 - 214
local COLUMN_TOP = -300
local ROW_HEIGHT = 21
local MARK_INSET = 24           -- where a row's text begins, past its marker
local ART = "Interface\\Prestige\\"

local SOUND_OPEN = "AchievementMenuOpen"
local SOUND_CLOSE = "AchievementMenuClose"
local SOUND_DENIED = "igQuestFailed"
local SOUND_PRESTIGE = "LEVELUPSOUND"

local KEEP = {
    "Essences",
    "Tableau de parangon",
    "Quêtes, réputation, or",
    "Métiers et montures",
    "Techniques raciales",
    "Heirlooms équipés",
}

local LOSE = {
    "Niveau ramené à 1",
    "Expérience remise à zéro",
    "Sorts au-dessus du niveau 1",
    "Les deux spécialisations",
    "Glyphes",
    "Équipement au-dessus du niveau 1",
}

local REASONS = {
    [1] = "Niveau maximum requis.",
    [2] = "Impossible en combat.",
    [3] = "Impossible tant que vous êtes mort.",
    [4] = "Sortez de l'instance.",
}

local state = {
    prestige = 0,
    cap = 50,
    nextCap = 60,
    earned = 0,
    spent = 0,
    level = 1,
    can = 0,
    reason = 1,
    pending = false,
    confirming = false,
    suppressCloseSound = false,
}

local frame, numberText, capText, capNextText, capArrow, bankText, requirementText
local prestigeButton, confirmText, confirmButton, cancelButton

local function bankLine()
    local rising = state.nextCap - state.cap
    local banked = state.earned - state.cap
    if banked < 0 then banked = 0 end
    if rising < 0 then rising = 0 end
    local gain = banked < rising and banked or rising
    if gain <= 0 then
        return nil
    end
    if gain == 1 and banked == 1 then
        return "1 point en réserve devient disponible."
    end
    if gain == 1 then
        return string.format("%d points en réserve, 1 devient disponible.", banked)
    end
    return string.format("%d points en réserve, %d deviennent disponibles.", banked, gain)
end

local function refresh()
    if not frame then return end

    numberText:SetFontObject(state.prestige >= 100 and "GameFontNormalLarge" or "GameFontNormalHuge")
    numberText:SetText(tostring(state.prestige))
    numberText:SetTextColor(1, 0.86, 0.45)

    capText:SetFormattedText("%d", state.cap)
    capText:ClearAllPoints()
    if state.nextCap > state.cap then
        capArrow:Show()
        capNextText:Show()
        capNextText:SetFormattedText("%d", state.nextCap)
        capText:SetPoint("RIGHT", capArrow, "LEFT", -10, 1)
    else
        capArrow:Hide()
        capNextText:Hide()
        capText:SetPoint("CENTER", capArrow, "CENTER", 0, 1)
    end

    local bank = bankLine()
    if bank then
        bankText:SetText(bank)
        bankText:Show()
    else
        bankText:Hide()
    end

    local allowed = state.can == 1 and not state.pending
    if allowed then
        requirementText:SetText("Vous êtes au niveau maximum.")
        requirementText:SetTextColor(0.55, 0.9, 0.5)
    else
        requirementText:SetText(REASONS[state.reason] or "Prestige indisponible.")
        requirementText:SetTextColor(0.95, 0.4, 0.35)
    end

    local showingConfirm = state.confirming and allowed
    if showingConfirm then
        prestigeButton:Hide()
        confirmText:Show()
        confirmButton:Show()
        cancelButton:Show()
    else
        prestigeButton:Show()
        confirmText:Hide()
        confirmButton:Hide()
        cancelButton:Hide()
    end
    if allowed then
        prestigeButton:Enable()
    else
        prestigeButton:Disable()
    end
    if state.pending then
        confirmButton:Disable()
    else
        confirmButton:Enable()
    end
end

local function setConfirming(confirming)
    state.confirming = confirming
    refresh()
end

-- Every row is a frame of a known height, and both the marker and the text anchor to its LEFT. That is
-- what keeps the two columns level: a tick is a 14 pixel texture and a dash is a 10 pixel bar, and anchoring
-- each to the row's middle lines them up with each other and with their text, which hand-placed offsets and
-- a punctuation glyph never did.
local function createColumn(title, lines, anchorX, color, ticked)
    local header = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    header:SetPoint("TOPLEFT", frame, "TOPLEFT", anchorX, -278)
    header:SetText(title)
    header:SetTextColor(color[1], color[2], color[3])

    for index, line in ipairs(lines) do
        local row = CreateFrame("Frame", nil, frame)
        row:SetSize(COLUMN_WIDTH, ROW_HEIGHT)
        row:SetPoint("TOPLEFT", frame, "TOPLEFT", anchorX, COLUMN_TOP - (index - 1) * ROW_HEIGHT)

        local mark = row:CreateTexture(nil, "OVERLAY")
        if ticked then
            RetailUI.SetAtlas(mark, "ui-questtracker-tracker-check-2x", true)
            mark:SetSize(14, 14)
            mark:SetPoint("LEFT", row, "LEFT", 1, 0)
        else
            mark:SetTexture(1, 1, 1, 1)
            mark:SetSize(10, 2)
            mark:SetPoint("LEFT", row, "LEFT", 3, 0)
            -- only the drawn bar takes the column's colour; the tick art already carries its own
            mark:SetVertexColor(color[1], color[2], color[3], 0.95)
        end

        local text = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        text:SetPoint("LEFT", row, "LEFT", MARK_INSET, 0)
        text:SetPoint("RIGHT", row, "RIGHT", 0, 0)
        text:SetJustifyH("LEFT")
        text:SetText(line)
        text:SetTextColor(0.85, 0.82, 0.72)
    end
end

local function createFrame()
    frame = CreateFrame("Frame", "PrestigeFrame", UIParent)
    frame:SetSize(WINDOW_WIDTH, WINDOW_HEIGHT)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("HIGH")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:Hide()

    local ground = frame:CreateTexture(nil, "BACKGROUND")
    ground:SetTexture(RetailUIFiles["ui-background-rock"], true)
    ground:SetHorizTile(true)
    ground:SetVertTile(true)
    ground:SetPoint("TOPLEFT", 2, -21)
    ground:SetPoint("BOTTOMRIGHT", -2, 2)

    local marble = frame:CreateTexture(nil, "BORDER")
    marble:SetTexture(RetailUIFiles["ui-background-marble"], true)
    marble:SetHorizTile(true)
    marble:SetVertTile(true)
    marble:SetPoint("TOPLEFT", 6, -24)
    marble:SetPoint("BOTTOMRIGHT", -6, 6)

    RetailUI.ApplyNineSlice(frame, false)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -4)
    title:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -26, -4)
    title:SetJustifyH("CENTER")
    title:SetText("Prestige")
    title:SetTextColor(1, 0.82, 0)

    local closeButton = CreateFrame("Button", nil, frame)
    closeButton:SetSize(24, 24)
    closeButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -5, -5)
    closeButton:SetFrameLevel(frame:GetFrameLevel() + 20)
    closeButton:SetNormalTexture(RetailUIAtlas["redbutton-exit-2x"][1])
    RetailUI.SetAtlas(closeButton:GetNormalTexture(), "redbutton-exit-2x")
    closeButton:SetPushedTexture(RetailUIAtlas["redbutton-exit-pressed-2x"][1])
    RetailUI.SetAtlas(closeButton:GetPushedTexture(), "redbutton-exit-pressed-2x")
    closeButton:SetHighlightTexture(RetailUIAtlas["redbutton-highlight-2x"][1])
    RetailUI.SetAtlas(closeButton:GetHighlightTexture(), "redbutton-highlight-2x")
    closeButton:GetHighlightTexture():SetBlendMode("ADD")
    closeButton:SetScript("OnClick", function() frame:Hide() end)

    local mover = CreateFrame("Frame", nil, frame)
    mover:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(frame:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)

    local emblem = CreateFrame("Frame", nil, frame)
    emblem:SetSize(128, 128)
    emblem:SetPoint("TOP", frame, "TOP", 0, -40)
    emblem:SetFrameLevel(frame:GetFrameLevel() + 4)

    local glow = frame:CreateTexture(nil, "BACKGROUND")
    glow:SetTexture(ART .. "Prestige-Emblem-Glow")
    glow:SetBlendMode("ADD")
    glow:SetSize(202, 202)
    glow:SetPoint("CENTER", emblem, "CENTER", 0, 0)

    local rays = frame:CreateTexture(nil, "BORDER")
    rays:SetTexture(ART .. "Prestige-Rays-00")
    rays:SetBlendMode("ADD")
    rays:SetSize(176, 176)
    rays:SetPoint("CENTER", emblem, "CENTER", 0, 0)

    local emblemTexture = emblem:CreateTexture(nil, "ARTWORK")
    emblemTexture:SetAllPoints()
    emblemTexture:SetTexture(ART .. "Prestige-Emblem")

    -- The halo turns and the glow breathes against it, so the frame is never quite still. Both are driven
    -- from one clock: the rays advance a frame at a time and the glow swells on the opposite phase.
    local elapsedTotal, rayFrame = 0, -1
    frame:SetScript("OnUpdate", function(_, elapsed)
        elapsedTotal = elapsedTotal + elapsed

        local index = math.floor(elapsedTotal / RAY_PERIOD * RAY_FRAMES) % RAY_FRAMES
        if index ~= rayFrame then
            rayFrame = index
            rays:SetTexture(ART .. string.format("Prestige-Rays-%02d", index))
        end

        local pulse = 0.5 + 0.5 * math.sin(elapsedTotal * 1.5)
        glow:SetAlpha(0.52 + 0.34 * pulse)
        local swell = 196 + 14 * pulse
        glow:SetSize(swell, swell)
        rays:SetAlpha(0.42 + 0.30 * (1 - pulse))
    end)

    local numberFrame = CreateFrame("Frame", nil, emblem)
    numberFrame:SetAllPoints()
    numberFrame:SetFrameLevel(emblem:GetFrameLevel() + 3)
    numberText = numberFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    numberText:SetPoint("CENTER", numberFrame, "CENTER", 0, -1)

    local currentLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    currentLabel:SetPoint("TOP", emblem, "BOTTOM", 0, -2)
    currentLabel:SetText("Prestige actuel")
    currentLabel:SetTextColor(0.7, 0.65, 0.5)

    local capLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    capLabel:SetPoint("TOP", currentLabel, "BOTTOM", 0, -10)
    capLabel:SetText("Plafond de parangon")
    capLabel:SetTextColor(0.65, 0.78, 0.95)

    -- The cap and the cap it becomes sit either side of a drawn chevron. It is art because the client's
    -- fonts have no arrow glyph: U+2192 rendered as a question mark in game.
    local capRow = CreateFrame("Frame", nil, frame)
    capRow:SetSize(240, 26)
    capRow:SetPoint("TOP", capLabel, "BOTTOM", 0, -4)

    capArrow = capRow:CreateTexture(nil, "ARTWORK")
    capArrow:SetTexture(ART .. "Prestige-Arrow")
    capArrow:SetSize(24, 24)
    capArrow:SetPoint("CENTER", capRow, "CENTER", 0, 0)

    capText = capRow:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    capText:SetPoint("RIGHT", capArrow, "LEFT", -10, 1)
    capText:SetTextColor(1, 0.84, 0.4)

    capNextText = capRow:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    capNextText:SetPoint("LEFT", capArrow, "RIGHT", 10, 1)
    capNextText:SetTextColor(0.6, 1, 0.55)

    bankText = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    bankText:SetPoint("TOP", capRow, "BOTTOM", 0, -2)
    bankText:SetTextColor(0.75, 0.75, 0.7)

    local divider = frame:CreateTexture(nil, "ARTWORK")
    divider:SetTexture(ART .. "Prestige-Divider")
    divider:SetSize(448, 4)
    divider:SetPoint("TOP", frame, "TOP", 0, -258)

    createColumn("Vous conservez", KEEP, COLUMN_LEFT, { 0.55, 0.85, 0.5 }, true)
    createColumn("Vous perdez", LOSE, COLUMN_RIGHT, { 0.9, 0.45, 0.38 }, false)

    requirementText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    requirementText:SetPoint("LEFT", frame, "BOTTOMLEFT", 24, 28)
    requirementText:SetJustifyH("LEFT")

    prestigeButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    prestigeButton:SetSize(120, 24)
    prestigeButton:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -24, 16)
    prestigeButton:SetText("Prestige")
    prestigeButton:SetScript("OnClick", function()
        if state.can ~= 1 or state.pending then
            PlaySound(SOUND_DENIED)
            return
        end
        setConfirming(true)
    end)

    confirmText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    confirmText:SetPoint("LEFT", frame, "BOTTOMLEFT", 24, 56)
    confirmText:SetText("Cette action est définitive.")
    confirmText:SetTextColor(0.95, 0.75, 0.4)
    confirmText:Hide()

    confirmButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    confirmButton:SetSize(110, 24)
    confirmButton:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -140, 16)
    confirmButton:SetText("Confirmer")
    confirmButton:Hide()
    confirmButton:SetScript("OnClick", function()
        if state.pending then return end
        state.pending = true
        refresh()
        SendAddonMessage(PREFIX, "PRESTIGE", "WHISPER", UnitName("player"))
    end)

    cancelButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    cancelButton:SetSize(110, 24)
    cancelButton:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -24, 16)
    cancelButton:SetText("Annuler")
    cancelButton:Hide()
    cancelButton:SetScript("OnClick", function()
        if state.pending then return end
        setConfirming(false)
    end)

    tinsert(UISpecialFrames, "PrestigeFrame")
    frame:SetScript("OnHide", function()
        state.confirming = false
        state.pending = false
        if state.suppressCloseSound then
            state.suppressCloseSound = false
        else
            PlaySound(SOUND_CLOSE)
        end
    end)
end

local function show()
    if not frame then
        createFrame()
    end
    refresh()
    if not frame:IsShown() then
        frame:Show()
        PlaySound(SOUND_OPEN)
    end
end

local function handle(message)
    local command, rest = message:match("^(%S+)\t(.*)$")
    if not command then return end

    if command == "STATE" then
        local prestige, cap, nextCap, earned, spent, level, can, reason =
            rest:match("^(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
        if not prestige then return end
        state.prestige = tonumber(prestige)
        state.cap = tonumber(cap)
        state.nextCap = tonumber(nextCap)
        state.earned = tonumber(earned)
        state.spent = tonumber(spent)
        state.level = tonumber(level)
        state.can = tonumber(can)
        state.reason = tonumber(reason)
        state.pending = false
        show()
        return
    end

    if command == "ERROR" then
        state.pending = false
        state.confirming = false
        state.can = 0
        state.reason = tonumber(rest) or 1
        PlaySound(SOUND_DENIED)
        if frame then
            refresh()
        end
        return
    end

    if command == "DONE" then
        state.pending = false
        state.suppressCloseSound = true
        PlaySound(SOUND_PRESTIGE)
        if frame then
            frame:Hide()
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event ~= "CHAT_MSG_ADDON" or prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    handle(message)
end)

SLASH_PRESTIGE1 = "/prestige"
SlashCmdList["PRESTIGE"] = function()
    SendAddonMessage(PREFIX, "OPEN", "WHISPER", UnitName("player"))
end
