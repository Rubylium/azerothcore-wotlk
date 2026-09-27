-- ============================================================================
-- Autora: Noa
-- ============================================================================
-- Evolutions: the Options menu (OptionsSelectFrame, opened by the login screen's "Options" link and the character
-- screen's Options button) redrawn in the login screen's look, and the Video / Sound frames it opens given the same
-- chrome. The look comes from EvolutionsGlueStyle (AccountLogin.lua, loaded before this file); every action is the
-- original one.
function OptionsSelectFrame_Hide()
	PlaySound("gsLoginChangeRealmCancel");
	OptionsSelectFrame:Hide();
end

function OptionsSelectResetSettingsButton_OnClick_Reset(self)
	PlaySound("igMainMenuOptionCheckBoxOn");
	GlueDialog_Show("RESET_SERVER_SETTINGS");
end

function OptionsSelectResetSettingsButton_OnClick_Cancel(self)
	PlaySound("igMainMenuOptionCheckBoxOn");
	GlueDialog_Show("CANCEL_RESET_SETTINGS");
end

function AccountLogin_RealmReset()
	SetCVar("realmName", "");
	AccountLogin_Exit();
end

function MovieList_Show()
    PlaySound("igMainMenuOption");
    OptionsSelectFrame:Hide();
    CinematicsFrame:Show();
    Cinematics_SetupButtons();
end

function CinematicsFrame_OnLoad(self)
    self:RegisterEvent("PLAY_MOVIE");
end

function CinematicsFrame_OnKeyDown(key)
    if key == "ESCAPE" then
        CinematicsFrame_Hide();
    end
end

function CinematicsFrame_Hide()
    PlaySound("gsLoginChangeRealmCancel");
    CinematicsFrame:Hide();
    if CharacterSelect and CharacterSelect:IsShown() then
        CharacterSelect:Show();
    else
        if CharacterSelect_Show then
            CharacterSelect_Show();
        end
    end
end

function Cinematics_SetupButtons()
    local buttons = {CinematicsButton1, CinematicsButton2, CinematicsButton3};

    for i, button in ipairs(buttons) do
        button:Show();
    end
end

function Cinematics_OnMovieFinished()
    if CharacterSelect and CharacterSelect:IsShown() then
        CharacterSelect:Show();
    else
        if CharacterSelect_Show then
            CharacterSelect_Show();
        end
    end
end

-- ============================================================================
-- THE LOOK
-- ============================================================================
local L = {
    title = "Options",
    video = "Vidéo",
    videoHint = "Résolution, qualité graphique, affichage",
    sound = "Son",
    soundHint = "Volumes, musique, ambiances, périphérique audio",
    cinematics = "Cinématiques",
    cinematicsHint = "Revoir les cinématiques du jeu",
    realmReset = "Réinitialiser le royaume et quitter",
    realmResetHint = "Oublie le royaume mémorisé et ferme le jeu",
    close = "Fermer",
}

-- The one action that cannot be taken back: a muted red, never loud
local DANGER_TEXT = { 0.88, 0.52, 0.42 }
local DANGER_HIGH = { 1, 0.66, 0.55 }
local DANGER_EDGE = { 0.62, 0.28, 0.2 }
local DANGER_LIGHT = { 1, 0.4, 0.24 }
local GOLD_LIGHT = { 1, 0.78, 0.4 }

local ARROW = "Interface\\ChatFrame\\ChatFrameExpandArrow"
local INTRO_TIME = 0.3
local INTRO_RISE = 12

local S                 -- EvolutionsGlueStyle
local rows = {}
local state = { intro = INTRO_TIME }

local function Mix(from, to, amount)
    return from[1] + (to[1] - from[1]) * amount, from[2] + (to[2] - from[2]) * amount,
        from[3] + (to[3] - from[3]) * amount
end

-- A gold (or red) hairline across a width, fading out at both ends
local function Hairline(parent, layer, color, alpha)
    local left = S.Fade(parent, layer, "HORIZONTAL", color[1], color[2], color[3], 0, alpha)
    local right = S.Fade(parent, layer, "HORIZONTAL", color[1], color[2], color[3], alpha, 0)
    left:SetHeight(1)
    right:SetHeight(1)
    return left, right
end

local function PlaceHairline(left, right, relative, point, y)
    left:SetPoint("LEFT", relative, point == "TOP" and "TOPLEFT" or "BOTTOMLEFT", 0, y)
    left:SetPoint("RIGHT", relative, point, 0, y)
    right:SetPoint("LEFT", relative, point, 0, y)
    right:SetPoint("RIGHT", relative, point == "TOP" and "TOPRIGHT" or "BOTTOMRIGHT", 0, y)
end

local function PaintRow(row)
    local lit = row.lit
    if row.link then
        row.underline:SetAlpha(0.9 * lit)
        return
    end
    local danger = row.danger
    local edge = danger and DANGER_EDGE or S.BORDER
    local light = danger and DANGER_LIGHT or GOLD_LIGHT
    local r, g, b = Mix(danger and DANGER_TEXT or S.TEXT, danger and DANGER_HIGH or S.HEADING, lit)
    row.title:SetTextColor(r, g, b)
    S.ColorEdges(row.edges, edge[1], edge[2], edge[3], 0.3 + 0.6 * lit)
    -- Never SetAlpha on a gradient in this client (it wipes it): the wash is re-laid with its new strength
    row.wash:SetGradientAlpha("HORIZONTAL", light[1], light[2], light[3], 0.16 * lit, light[1], light[2], light[3], 0)
    row.bar:SetVertexColor(r, g, b, 0.3 + 0.7 * lit)
    row.glow:SetVertexColor(light[1], light[2], light[3], 0.22 * lit)
    row.fill:SetVertexColor(0, 0, 0, row.button.pressed and 0.6 or 0.38)
    if row.arrow then
        row.arrow:ClearAllPoints()
        row.arrow:SetPoint("RIGHT", row.button, "RIGHT", -16 + 4 * lit, 0)
        row.arrow:SetVertexColor(r, g, b, 0.45 + 0.55 * lit)
    end
end

local function BuildRow(panel, button, title, hint, danger)
    local row = { button = button, lit = 0, danger = danger }
    -- Its light, on the card under it
    row.glow = S.Piece(panel, "ARTWORK", S.GLOW)
    row.glow:SetBlendMode("ADD")
    row.glow:SetPoint("TOPLEFT", button, "TOPLEFT", -22, 16)
    row.glow:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 22, -16)

    row.fill = S.Solid(button, "BACKGROUND", 0, 0, 0, 0.38)
    row.fill:SetAllPoints()
    row.wash = button:CreateTexture(nil, "BORDER")
    row.wash:SetTexture(S.WHITE)
    row.wash:SetPoint("TOPLEFT", 1, -1)
    row.wash:SetPoint("BOTTOMRIGHT", -1, 1)
    row.edges = S.Edges(button, "BORDER")
    row.bar = S.Solid(button, "ARTWORK", 1, 1, 1, 1)
    row.bar:SetPoint("TOPLEFT", 0, 0)
    row.bar:SetPoint("BOTTOMLEFT", 0, 0)
    row.bar:SetWidth(2)

    row.title = S.Text(button, S.FONT_TITLE, 17, "OVERLAY")
    row.title:SetPoint("TOPLEFT", button, "TOPLEFT", 16, -8)
    row.title:SetText(title)
    row.hint = S.Text(button, S.FONT_TEXT, 10, "OVERLAY", S.MUTED[1], S.MUTED[2], S.MUTED[3])
    row.hint:SetPoint("TOPLEFT", row.title, "BOTTOMLEFT", 1, -3)
    row.hint:SetText(hint)

    if not danger then
        row.arrow = button:CreateTexture(nil, "OVERLAY")
        row.arrow:SetTexture(ARROW)
        row.arrow:SetSize(14, 14)
    end

    button.evolutionsRow = row
    rows[#rows + 1] = row
    PaintRow(row)
    return row
end

local function BuildLink(button)
    local row = { button = button, lit = 0, link = true }
    row.underline = S.Piece(button, "OVERLAY", S.DIVIDER)
    row.underline:SetHeight(2)
    row.underline:SetPoint("BOTTOMLEFT", button:GetFontString(), "BOTTOMLEFT", 0, -3)
    row.underline:SetPoint("BOTTOMRIGHT", button:GetFontString(), "BOTTOMRIGHT", 0, -3)
    button.evolutionsRow = row
    rows[#rows + 1] = row
    PaintRow(row)
end

local function BuildMenu(self)
    local panel = OptionsSelectFrameBackground
    S.StyleDialog(self, panel)

    -- A faint warm light behind the heading, the heading and the challenge board's divider
    local light = S.Piece(panel, "ARTWORK", S.GLOW)
    light:SetBlendMode("ADD")
    light:SetVertexColor(1, 0.75, 0.4, 0.12)
    light:SetSize(300, 90)
    light:SetPoint("TOP", panel, "TOP", 0, 4)
    local heading = OptionsSelectFrameBackgroundHeaderText
    heading:SetFont(S.FONT_TITLE, 24, "")
    heading:SetTextColor(S.HEADING[1], S.HEADING[2], S.HEADING[3])
    heading:SetShadowColor(0, 0, 0, 0.9)
    heading:SetShadowOffset(1, -1)
    heading:SetText(L.title)
    local divider = S.Piece(panel, "OVERLAY", S.DIVIDER)
    divider:SetSize(260, 5)
    divider:SetPoint("TOP", panel, "TOP", 0, -56)

    local prefix = "OptionsSelectFrameBackgroundContainer"
    BuildRow(panel, _G[prefix .. "VideoOptionsButton"], L.video, L.videoHint)
    BuildRow(panel, _G[prefix .. "AudioOptionsButton"], L.sound, L.soundHint)
    BuildRow(panel, _G[prefix .. "TrailersButton"], L.cinematics, L.cinematicsHint)
    local reset = _G[prefix .. "RealmResetOptionsButton"]
    BuildRow(panel, reset, L.realmReset, L.realmResetHint, true)

    -- Hairlines: one setting the realm reset apart, one over the footer
    local left, right = Hairline(panel, "ARTWORK", S.BORDER, 0.45)
    PlaceHairline(left, right, reset, "TOP", 12)
    left, right = Hairline(panel, "ARTWORK", S.BORDER, 0.3)
    PlaceHairline(left, right, reset, "BOTTOM", -15)

    BuildLink(OptionsSelectResetSettingsButton)
    local close = OptionsSelectFrameBackgroundOkayButton
    S.StyleButton(close, "quiet")
    close:SetText(L.close)
end

-- The Video and Sound frames (OptionsFrameTemplate, optionsframetemplates.xml; the frames themselves are the stock
-- VideoOptionsFrame.xml / AudioOptionsFrame.xml): the same dark card and gold edge over the dimmed screen, the gold
-- heading and divider, gold edges on the lists and the buttons restyled. The controls inside are left as they are.
local LIST_EDGES = { "TopLeft", "TopRight", "BottomLeft", "BottomRight", "Left", "Right", "Top", "Bottom" }
local HEADER_ROOM = 20

local function StyleOptionsFrame(frame)
    if not frame or frame.evolutionsStyled then return end
    local name = frame:GetName()

    -- The template's own dimmer covers the 16:9 glue screen only: replaced by StyleDialog's full-width one
    local dimmer = _G[name .. "Backdrop"]
    if dimmer then
        for _, region in ipairs({ dimmer:GetRegions() }) do
            region:Hide()
        end
    end
    S.StyleDialog(dimmer or frame, frame)

    -- Room for the heading and its divider above the lists
    frame:SetHeight(frame:GetHeight() + HEADER_ROOM)
    local list = frame.categoryFrame or _G[name .. "CategoryFrame"]
    if list then
        list:ClearAllPoints()
        list:SetPoint("TOPLEFT", frame, "TOPLEFT", 22, -40 - HEADER_ROOM)
        for _, part in ipairs(LIST_EDGES) do
            local edge = _G[list:GetName() .. part]
            if edge then
                edge:SetVertexColor(S.BORDER[1], S.BORDER[2], S.BORDER[3], 0.75)
            end
        end
        local fill = S.Solid(list, "BACKGROUND", 0, 0, 0, 0.35)
        fill:SetPoint("TOPLEFT", 4, -4)
        fill:SetPoint("BOTTOMRIGHT", -4, 4)
        local scroll = _G[list:GetName() .. "List"]
        if scroll then
            scroll:SetBackdropBorderColor(S.BORDER[1], S.BORDER[2], S.BORDER[3], 0.5)
        end
    end
    local container = frame.panelContainer or _G[name .. "PanelContainer"]
    if container then
        container:SetBackdropBorderColor(S.BORDER[1], S.BORDER[2], S.BORDER[3], 0.75)
        local fill = S.Solid(container, "BACKGROUND", 0, 0, 0, 0.25)
        fill:SetPoint("TOPLEFT", 4, -4)
        fill:SetPoint("BOTTOMRIGHT", -4, 4)
    end

    local light = S.Piece(frame, "ARTWORK", S.GLOW)
    light:SetBlendMode("ADD")
    light:SetVertexColor(1, 0.75, 0.4, 0.1)
    light:SetSize(300, 80)
    light:SetPoint("TOP", frame, "TOP", 0, 6)
    local heading = _G[name .. "HeaderText"]
    if heading then
        heading:SetFont(S.FONT_TITLE, 22, "")
        heading:SetTextColor(S.HEADING[1], S.HEADING[2], S.HEADING[3])
        heading:SetShadowColor(0, 0, 0, 0.9)
        heading:SetShadowOffset(1, -1)
        heading:ClearAllPoints()
        heading:SetPoint("TOP", frame, "TOP", 0, -17)
    end
    local divider = S.Piece(frame, "OVERLAY", S.DIVIDER)
    divider:SetSize(260, 5)
    divider:SetPoint("TOP", frame, "TOP", 0, -46)

    -- The buttons: Okay in gold, the others quiet; right to left Apply, Cancel, Okay, and Defaults on the left
    local apply = frame.apply or _G[name .. "Apply"]
    local cancel = frame.cancel or _G[name .. "Cancel"]
    local okay = frame.okay or _G[name .. "Okay"]
    local default = frame.default or _G[name .. "Default"]
    local previous
    for _, button in ipairs({ apply or false, cancel or false, okay or false }) do
        if button then
            S.StyleButton(button, button == okay and "primary" or "quiet")
            button:SetWidth(118)
            button:SetHeight(32)
            button:ClearAllPoints()
            if previous then
                button:SetPoint("RIGHT", previous, "LEFT", -8, 0)
            else
                button:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -22, 12)
            end
            previous = button
        end
    end
    if default then
        S.StyleButton(default, "quiet")
        default:SetHeight(32)
        S.FitButton(default, 118)
        default:ClearAllPoints()
        default:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 22, 12)
    end

    -- Fade in on show, like the menu
    frame.evolutionsIntro = INTRO_TIME
    frame:HookScript("OnShow", function(self)
        self.evolutionsIntro = 0
        self:SetAlpha(0)
    end)
    local function Animate(self, elapsed)
        if self.evolutionsIntro < INTRO_TIME then
            self.evolutionsIntro = self.evolutionsIntro + math.min(elapsed or 0, 0.1)
            self:SetAlpha(S.OutCubic(S.Clamp01(self.evolutionsIntro / INTRO_TIME)))
        end
    end
    if frame:GetScript("OnUpdate") then
        frame:HookScript("OnUpdate", Animate)
    else
        frame:SetScript("OnUpdate", Animate)
    end
end

function OptionsSelect_RowHover(button, hovered)
    local row = button.evolutionsRow
    if row then
        row.hover = hovered and true or false
    end
    if not hovered then
        button.pressed = nil
    end
end

function OptionsSelectFrame_OnLoad(self)
    S = EvolutionsGlueStyle
    if not S then return end
    BuildMenu(self)
    StyleOptionsFrame(_G.VideoOptionsFrame)
    StyleOptionsFrame(_G.AudioOptionsFrame)
    state.built = true
end

function OptionsSelectFrame_OnShow(self)
    if not state.built then return end
    state.intro = 0
    self:SetAlpha(0)
    for _, row in ipairs(rows) do
        local button = row.button
        row.hover = (button.IsMouseOver and button:IsVisible() and button:IsMouseOver()) and true or false
        row.lit = row.hover and 1 or 0
        button.pressed = nil
        PaintRow(row)
    end
    OptionsSelectFrame_OnUpdate(self, 0)
end

function OptionsSelectFrame_OnUpdate(self, elapsed)
    if not state.built then return end
    elapsed = math.min(elapsed or 0, 0.1)
    if state.intro < INTRO_TIME then
        state.intro = state.intro + elapsed
        local p = S.OutCubic(S.Clamp01(state.intro / INTRO_TIME))
        self:SetAlpha(p)
        OptionsSelectFrameBackground:ClearAllPoints()
        OptionsSelectFrameBackground:SetPoint("CENTER", self, "CENTER", 0, -INTRO_RISE * (1 - p))
    end
    for _, row in ipairs(rows) do
        local target = (row.hover and row.button:IsEnabled() == 1) and 1 or 0
        local pressed = row.button.pressed and true or false
        if math.abs(row.lit - target) > 0.01 or row.shownPressed ~= pressed then
            row.lit = S.Approach(row.lit, target, elapsed, 12)
            if math.abs(row.lit - target) <= 0.01 then
                row.lit = target
            end
            row.shownPressed = pressed
            PaintRow(row)
        end
    end
end
