-- ==============================================================================================
-- Evolutions: the login screen. Based on Noa's login screen (retail glue package), redrawn.
--
-- The art (clientPatcher/assets/login, built into Interface\Glues\Evolutions\LoginBackdrop by
-- localTools/interface/buildGlueArt.py) fills the screen at any proportions without being stretched, drifting
-- and zooming very slowly and leaning a little away from the pointer. The braziers flicker and throw embers, the
-- city's beam breathes. Over it: the server's name at the top, the login card lower in the middle (account,
-- password, remember options, the gold login button), and the quiet actions along the bottom edge.
--
-- Everything the frames do (logging in, saving the account, auto-login, dialogs, cinematics, the PIN pad) is the
-- original code, unchanged; only how the screen looks is new.
-- ==============================================================================================
local Config = {
    AUTO_LOGIN_DELAY = 3.0,
    SCROLL_THRESHOLD = 20,
    UNDEAD_AMBIENCE_FADE_TIME = 4.0,
    SFX_STOP_TIME = 1.0,
}

local LoginState = {
    autoLoginTimer = nil, autoLoginDelay = Config.AUTO_LOGIN_DELAY, autoLoginAttempted = false,
}

local UICache = {
    accountEdit = nil, passwordEdit = nil, saveAccountName = nil, savePassword = nil, autoLogin = nil, autoLoginText = nil, loginButton = nil, versionText = nil, realmName = nil, upgradeButton = nil, tosFrame = nil, tosAccept = nil, tosDecline = nil
}

-- ==================== LOOK ====================
local ART = "Interface\\Glues\\Evolutions\\"
local RETAIL = "Interface\\RetailUI\\"
local FONT_TITLE = "Fonts\\MORPHEUS.TTF"
local FONT_TEXT = "Fonts\\FRIZQT__.TTF"
local WHITE = "Interface\\Buttons\\WHITE8X8"

-- The challenge board's pieces (FrameXML/RetailUIAtlas.lua; the files ship in the same patch, the glue screens
-- read them by path since they have no atlas table)
local GLOW = { RETAIL .. "challengemode-softyellowglow", 0, 0.804688, 0, 0.804688 }
local DIVIDER = { RETAIL .. "challengemode-thindivider", 0, 0.712891, 0, 0.75 }
local PARCHMENT = { RETAIL .. "questbg-parchment", 0, 0.583984, 0, 0.794922 }

-- The picture: its size, and the texture it sits in (top-left corner, see buildGlueArt.py)
-- The ultrawide art (buildGlueArt.py login): 3642x1024, in two 2048x1024 tiles side by side
local ART_WIDTH, ART_HEIGHT = 3642, 1024
local ART_TILE_WIDTH = 2048
local ART_ASPECT = ART_WIDTH / ART_HEIGHT

-- Points of the picture, in its pixels: the two brazier flames and the foot of the city's beam
local BRAZIERS = { { 1060, 612 }, { 2490, 612 } }
local BEACON = { 1839, 360 }
local EMBERS_PER_BRAZIER = 7

-- The logo: its size in its texture (buildGlueArt.py), and how much of the screen's height it takes
local LOGO_TEXTURE_SIZE = 1024
local LOGO_WIDTH, LOGO_HEIGHT = 1024, 750
local LOGO_SCREEN_SHARE = 0.30
local LOGO_TOP = -10

-- The challenge board's palette: gold headings and frames on dark, parchment text
local HEADING = { 1, 0.86, 0.55 }
local BORDER = { 0.75, 0.6, 0.35 }
local TEXT = { 0.85, 0.8, 0.7 }
local MUTED = { 0.62, 0.57, 0.5 }

-- The card (from the top of the card, downwards)
local CARD_WIDTH = 340
local CARD_TOP = 372             -- above the bottom of the screen
local FIELD_WIDTH, FIELD_HEIGHT = 284, 34
local ACCOUNT_Y, PASSWORD_Y = -86, -148
local OPTIONS_Y, OPTION_ROW = -198, 20
local BUTTON_HEIGHT, BUTTON_GAP, BOTTOM_PAD = 40, 16, 24

local L = {
    heading = "Connexion",
    account = "COMPTE",
    password = "MOT DE PASSE",
    accountHint = "Nom de compte",
    passwordHint = "Mot de passe",
    saveAccount = "Mémoriser le compte",
    savePassword = "Mémoriser le mot de passe",
    autoLogin = "Connexion automatique",
    login = "Se connecter",
    options = "Options",
    cinematics = "Cinématiques",
    quit = "Quitter",
    realm = "Royaume",
    noRealm = "Aucun royaume récent",
    version = "Version",
    resetPending = "Les paramètres seront réinitialisés au prochain démarrage.",
}

local scene = {}        -- the art and its light
local ui = {}           -- the card, the title, the bottom edge
local fields = {}
local links = {}
local optionRows = {}
local intro, clock = 0, 0
local parallaxX, parallaxY = 0, 0
local layoutKey
local cardHeight, cardTargetHeight

-- ==================== CONFIGURACIÓN ADICIONAL ====================
local function InitializeUICache()
    UICache.accountEdit = _G["AccountLoginAccountEdit"]
    UICache.passwordEdit = _G["AccountLoginPasswordEdit"]
    UICache.saveAccountName = _G["AccountLoginSaveAccountName"]
    UICache.savePassword = _G["AccountLoginSavePassword"]
    UICache.autoLogin = _G["AccountLoginAutoLogin"]
    UICache.autoLoginText = _G["AccountLoginAutoLoginText"]
    UICache.loginButton = _G["AccountLoginLoginButton"]
    UICache.versionText = _G["AccountLoginVersion"]
    UICache.realmName = _G["AccountLoginRealmName"]
    UICache.upgradeButton = _G["AccountLoginUpgradeAccountButton"]
    UICache.tosFrame = _G["TOSFrame"]
    UICache.tosAccept = _G["TOSAccept"]
    UICache.tosDecline = _G["TOSDecline"]
end
-- ============================================================================
-- DIÁLOGOS
-- ============================================================================
GlueDialogTypes["REMEMBER_PASSWORD"] = {
    text = SAVE_PASSWORD_NOTICE,
    button1 = OKAY,
    button2 = CANCEL,
    OnAccept = function()
        UICache.autoLoginText:Show()
        UICache.autoLogin:Show()
    end,
    OnCancel = function()
        UICache.savePassword:SetChecked(0)
        UICache.autoLoginText:Hide()
        UICache.autoLogin:Hide()
        UICache.autoLogin:SetChecked(0)
    end,
}

GlueDialogTypes["AUTO_LOGIN"] = {
    text = SAVE_AUTOLOGIN_NOTICE,
    button1 = OKAY,
    button2 = CANCEL,
    OnAccept = function()
    end,
    OnCancel = function()
        UICache.autoLogin:SetChecked(0)
    end,
}
-- ============================================================================
-- SMALL HELPERS
-- ============================================================================
local function Clamp01(value)
    if value < 0 then return 0 end
    if value > 1 then return 1 end
    return value
end

local function OutCubic(p)
    local inverse = 1 - p
    return 1 - inverse * inverse * inverse
end

local function Smooth(value)
    return value * value * (3 - 2 * value)
end

local function Approach(current, target, elapsed, speed)
    return current + (target - current) * math.min(elapsed * speed, 1)
end

local function Text(parent, font, size, layer, r, g, b)
    local text = parent:CreateFontString(nil, layer or "OVERLAY")
    text:SetFont(font, size, "")
    text:SetShadowColor(0, 0, 0, 0.9)
    text:SetShadowOffset(1, -1)
    if r then
        text:SetTextColor(r, g, b)
    end
    return text
end

local function Solid(parent, layer, r, g, b, a)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    texture:SetTexture(WHITE)
    texture:SetVertexColor(r, g, b, a or 1)
    return texture
end

-- A gradient strip: alpha a1 at its start, a2 at its end (HORIZONTAL: left to right, VERTICAL: bottom to top).
-- Never call SetAlpha on it: in this client that wipes the gradient.
local function Fade(parent, layer, orientation, r, g, b, a1, a2)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    texture:SetTexture(WHITE)
    texture:SetGradientAlpha(orientation, r, g, b, a1, r, g, b, a2)
    return texture
end

local function Piece(parent, layer, piece)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    texture:SetTexture(piece[1])
    texture:SetTexCoord(piece[2], piece[3], piece[4], piece[5])
    return texture
end

-- A 1-pixel frame around a region: four lines, returned so they can be recoloured together
local function Edges(parent, layer)
    local edges = {}
    local specs = {
        { "TOPLEFT", "TOPRIGHT", true }, { "BOTTOMLEFT", "BOTTOMRIGHT", true },
        { "TOPLEFT", "BOTTOMLEFT", false }, { "TOPRIGHT", "BOTTOMRIGHT", false },
    }
    for _, spec in ipairs(specs) do
        local edge = Solid(parent, layer or "BORDER", 1, 1, 1, 1)
        edge:SetPoint(spec[1])
        edge:SetPoint(spec[2])
        if spec[3] then edge:SetHeight(1) else edge:SetWidth(1) end
        edges[#edges + 1] = edge
    end
    return edges
end

local function ColorEdges(edges, r, g, b, a)
    for _, edge in ipairs(edges) do
        edge:SetVertexColor(r, g, b, a)
    end
end

-- The part of the picture that fills a width x height box, like CSS "cover", enlarged by zoom and centred on
-- focusX / focusY (0-1 across the picture). Returns that part as fractions of the picture.
local function Cover(width, height, zoom, focusX, focusY)
    local u, v = 1, 1
    if width / height > ART_ASPECT then
        v = ART_ASPECT / (width / height)
    else
        u = (width / height) / ART_ASPECT
    end
    u, v = u / zoom, v / zoom
    local left = math.min(math.max(focusX - u / 2, 0), 1 - u)
    local top = math.min(math.max(focusY - v / 2, 0), 1 - v)
    return left, top, u, v
end
-- ============================================================================
-- THE SCENE: the art, its shade and its light
-- ============================================================================
local function SpawnEmber(ember, initial)
    ember.life = 2.4 + math.random() * 1.8
    ember.time = initial and math.random() * ember.life or 0
    ember.offsetX = (math.random() - 0.5) * 70
    ember.offsetY = math.random() * 24 - 12
    ember.rise = 120 + math.random() * 150
    ember.sway = 6 + math.random() * 14
    ember.frequency = 1.2 + math.random() * 1.6
    ember.phase = math.random() * 6.28
    ember.size = 6 + math.random() * 6
    ember.texture:SetVertexColor(1, 0.55 + math.random() * 0.25, 0.22)
end

-- The window's own shape. The client keeps the glue screens at 16:9 at most, centred: on a wider window (21:9, 32:9)
-- the picture is laid on a stage as wide as the window, overflowing both sides, or black bars frame it.
local function WindowAspect()
    local width, height = string.match(GetCVar("gxResolution") or "", "(%d+)x(%d+)")
    width, height = tonumber(width), tonumber(height)
    if width and height and height > 0 then
        return width / height
    end
end

local function BuildScene(owner)
    local self = CreateFrame("Frame", nil, owner)
    self:SetFrameLevel(owner:GetFrameLevel())
    self:SetPoint("CENTER", owner, "CENTER")
    self:SetWidth(owner:GetWidth() > 0 and owner:GetWidth() or 1024)
    self:SetHeight(owner:GetHeight() > 0 and owner:GetHeight() or 768)
    scene.stage = self
    scene.art = self:CreateTexture("AccountLoginBackground", "BACKGROUND")
    scene.art:SetTexture(ART .. "LoginBackdrop1")
    scene.art2 = self:CreateTexture(nil, "BACKGROUND")
    scene.art2:SetTexture(ART .. "LoginBackdrop2")

    -- Darker towards the edges and corners, and under the title and the card so they read over the bright sky
    local vignette = self:CreateTexture(nil, "BORDER")
    vignette:SetTexture(ART .. "LoginVignette")
    vignette:SetAllPoints()
    local top = Fade(self, "BORDER", "VERTICAL", 0, 0, 0, 0, 0.62)
    top:SetPoint("TOPLEFT")
    top:SetPoint("TOPRIGHT")
    top:SetHeight(250)
    local bottom = Fade(self, "BORDER", "VERTICAL", 0, 0, 0, 0.82, 0)
    bottom:SetPoint("BOTTOMLEFT")
    bottom:SetPoint("BOTTOMRIGHT")
    bottom:SetHeight(320)

    -- The braziers' flicker and the beam's breath: the soft glow, added over the picture
    scene.lights = {}
    for index, point in ipairs(BRAZIERS) do
        local light = Piece(self, "ARTWORK", GLOW)
        light:SetBlendMode("ADD")
        light:SetVertexColor(1, 0.55, 0.22)
        scene.lights[#scene.lights + 1] = { texture = light, x = point[1], y = point[2], size = 300, seed = index * 1.7 }
    end
    local beacon = Piece(self, "ARTWORK", GLOW)
    beacon:SetBlendMode("ADD")
    beacon:SetVertexColor(1, 0.85, 0.55)
    scene.lights[#scene.lights + 1] = { texture = beacon, x = BEACON[1], y = BEACON[2], size = 230, beacon = true }

    scene.embers = {}
    for side = 1, #BRAZIERS do
        for _ = 1, EMBERS_PER_BRAZIER do
            local ember = { side = side, texture = Piece(self, "OVERLAY", GLOW) }
            ember.texture:SetBlendMode("ADD")
            SpawnEmber(ember, true)
            scene.embers[#scene.embers + 1] = ember
        end
    end
end

-- The part of the picture on screen (Cover), to put things on points of the picture
local view = {}

-- Centres a texture on a point of the picture (in its pixels), size in picture pixels too
local function Place(texture, x, y, size)
    texture:ClearAllPoints()
    texture:SetPoint("CENTER", view.frame, "TOPLEFT", (x / ART_WIDTH - view.left) / view.u * view.width,
        -(y / ART_HEIGHT - view.top) / view.v * view.height)
    texture:SetWidth(size * view.pixel)
    texture:SetHeight(size * view.pixel)
end

local function UpdateScene(owner, elapsed, brightness)
    local height = owner:GetHeight()
    if not height or height <= 0 then
        return
    end
    -- The stage: as wide as the window, never narrower than the glue screen
    local self = scene.stage
    local width = math.max(owner:GetWidth(), height * (WindowAspect() or 0))
    if math.abs(self:GetWidth() - width) > 0.5 or math.abs(self:GetHeight() - height) > 0.5 then
        self:SetWidth(width)
        self:SetHeight(height)
    end

    -- A slow drift and zoom over 80 seconds, there and back, and a small lean away from the pointer
    local phase = (clock / 40) % 2
    phase = Smooth(phase < 1 and phase or 2 - phase)
    local cursorX, cursorY = GetCursorPosition()
    local scale = self:GetEffectiveScale()
    if cursorX and scale and scale > 0 then
        parallaxX = Approach(parallaxX, Clamp01(cursorX / scale / width) - 0.5, elapsed, 1.5)
        parallaxY = Approach(parallaxY, Clamp01(cursorY / scale / height) - 0.5, elapsed, 1.5)
    end
    local zoom = 1.04 + 0.04 * phase
    local left, top, u, v = Cover(width, height, zoom, 0.5 + 0.014 * (phase - 0.5) * 2 - 0.02 * parallaxX,
        0.45 - 0.006 * (phase - 0.5) * 2 + 0.014 * parallaxY)
    -- The picture laid at its size on screen, its two tiles side by side from its top left corner (which is off the
    -- screen by the part cut away)
    local pixel = width / (u * ART_WIDTH)
    local x, y = -left * ART_WIDTH * pixel, top * ART_HEIGHT * pixel
    for index, tile in ipairs({ scene.art, scene.art2 }) do
        tile:ClearAllPoints()
        tile:SetPoint("TOPLEFT", self, "TOPLEFT", x + (index - 1) * ART_TILE_WIDTH * pixel, y)
        tile:SetWidth(ART_TILE_WIDTH * pixel)
        tile:SetHeight(ART_HEIGHT * pixel)
        tile:SetVertexColor(brightness, brightness, brightness)
    end

    view.frame, view.left, view.top, view.u, view.v, view.width, view.height = self, left, top, u, v, width, height
    view.pixel = width / (u * ART_WIDTH)

    for _, light in ipairs(scene.lights) do
        local alpha
        if light.beacon then
            alpha = 0.16 + 0.05 * math.sin(clock * 0.9)
        else
            alpha = 0.24 + 0.06 * math.sin(clock * 7.3 + light.seed) + 0.04 * math.sin(clock * 13.1 + 2 * light.seed)
        end
        Place(light.texture, light.x, light.y, light.size)
        light.texture:SetAlpha(alpha * brightness)
    end

    for _, ember in ipairs(scene.embers) do
        ember.time = ember.time + elapsed
        if ember.time >= ember.life then
            SpawnEmber(ember, false)
        end
        local p = ember.time / ember.life
        local source = BRAZIERS[ember.side]
        local x = source[1] + ember.offsetX + math.sin(ember.phase + ember.time * ember.frequency) * ember.sway * p
        local y = source[2] - 20 + ember.offsetY - ember.rise * p
        Place(ember.texture, x, y, ember.size)
        local fade = (p < 0.15) and (p / 0.15) or ((1 - p) / 0.85)
        ember.texture:SetAlpha(0.9 * fade * brightness)
    end
end
-- ============================================================================
-- THE TITLE, THE CARD AND THE BOTTOM EDGE
-- ============================================================================
-- The server's logo, top centre, a fixed share of the screen's height (so the same look at any resolution), on a
-- faint warm light
local function BuildLogo()
    local brand = CreateFrame("Frame", nil, AccountLoginUI)
    brand:SetPoint("TOP", AccountLoginUI, "TOP", 0, LOGO_TOP)
    ui.brand = brand

    local glow = Piece(brand, "BACKGROUND", GLOW)
    glow:SetBlendMode("ADD")
    glow:SetVertexColor(1, 0.75, 0.4, 0.18)
    glow:SetPoint("TOPLEFT", brand, "TOPLEFT", -70, 30)
    glow:SetPoint("BOTTOMRIGHT", brand, "BOTTOMRIGHT", 70, -20)

    local logo = brand:CreateTexture("AccountLoginLogo", "ARTWORK")
    logo:SetTexture(ART .. "LoginLogo")
    logo:SetTexCoord(0, LOGO_WIDTH / LOGO_TEXTURE_SIZE, 0, LOGO_HEIGHT / LOGO_TEXTURE_SIZE)
    logo:SetAllPoints()
end

local function SizeLogo()
    local height = math.floor(AccountLoginUI:GetHeight() * LOGO_SCREEN_SHARE + 0.5)
    if height > 0 and height ~= ui.logoHeight then
        ui.logoHeight = height
        ui.brand:SetSize(height * LOGO_WIDTH / LOGO_HEIGHT, height)
    end
end

local function BuildField(card, edit, label, hint)
    local field = { edit = edit, lit = 0, hover = false, focus = false }

    local fill = Solid(edit, "BACKGROUND", 0, 0, 0, 0.55)
    fill:SetAllPoints()
    field.edges = Edges(edit, "BORDER")
    -- A gold line under the field when it has the focus
    field.accent = Piece(edit, "BORDER", DIVIDER)
    field.accent:SetHeight(3)
    field.accent:SetPoint("BOTTOMLEFT", 0, 0)
    field.accent:SetPoint("BOTTOMRIGHT", 0, 0)
    -- And a soft light around it, on the card (so under the field)
    field.glow = Piece(card, "ARTWORK", GLOW)
    field.glow:SetBlendMode("ADD")
    field.glow:SetPoint("TOPLEFT", edit, "TOPLEFT", -22, 16)
    field.glow:SetPoint("BOTTOMRIGHT", edit, "BOTTOMRIGHT", 22, -16)

    field.label = Text(card, FONT_TEXT, 10, "OVERLAY")
    field.label:SetPoint("BOTTOMLEFT", edit, "TOPLEFT", 1, 5)
    field.label:SetText(label)

    local placeholder = _G[edit:GetName() .. "Fill"]
    placeholder:SetFont(FONT_TEXT, 12, "")
    placeholder:SetTextColor(0.5, 0.46, 0.4)
    placeholder:SetText(hint)

    edit:SetTextColor(1, 0.95, 0.85)
    edit.evolutionsField = field
    fields[#fields + 1] = field
    return field
end

local function RefreshField(field)
    local lit = field.lit
    local hover = (field.hover and not field.focus) and 1 or 0
    local r = BORDER[1] + (HEADING[1] - BORDER[1]) * lit
    local g = BORDER[2] + (HEADING[2] - BORDER[2]) * lit
    local b = BORDER[3] + (HEADING[3] - BORDER[3]) * lit
    ColorEdges(field.edges, r, g, b, 0.45 + 0.25 * hover + 0.55 * lit)
    field.accent:SetVertexColor(1, 1, 1, lit)
    field.glow:SetVertexColor(1, 0.75, 0.35, 0.3 * lit)
    field.label:SetTextColor(MUTED[1] + (HEADING[1] - MUTED[1]) * lit, MUTED[2] + (HEADING[2] - MUTED[2]) * lit,
        MUTED[3] + (HEADING[3] - MUTED[3]) * lit)
end

local function BuildOption(check, label, text)
    check:SetFrameLevel(check:GetParent():GetFrameLevel() + 2)
    local box = Solid(check, "BACKGROUND", 0, 0, 0, 0.6)
    box:SetAllPoints()
    ColorEdges(Edges(check, "BORDER"), BORDER[1], BORDER[2], BORDER[3], 0.85)

    check:SetCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check")
    local mark = check:GetCheckedTexture()
    mark:ClearAllPoints()
    mark:SetPoint("CENTER", check, "CENTER", 1, 1)
    mark:SetSize(22, 22)
    mark:SetVertexColor(HEADING[1], HEADING[2], HEADING[3])

    check:SetHighlightTexture(GLOW[1])
    local light = check:GetHighlightTexture()
    light:SetTexCoord(GLOW[2], GLOW[3], GLOW[4], GLOW[5])
    light:SetBlendMode("ADD")
    light:SetVertexColor(1, 0.78, 0.4, 0.7)
    light:ClearAllPoints()
    light:SetPoint("TOPLEFT", check, "TOPLEFT", -9, 9)
    light:SetPoint("BOTTOMRIGHT", check, "BOTTOMRIGHT", 9, -9)

    label:SetText(text)
    optionRows[#optionRows + 1] = { check = check, label = label }
end

local function SetButtonFace(button)
    local state = button.pressed and 3 or (button.hovered and 2 or 1)
    if button.faceState == state then return end
    button.faceState = state
    -- bottom colour, then top colour
    local faces = {
        { 0.36, 0.25, 0.1, 0.58, 0.42, 0.19 },
        { 0.46, 0.33, 0.13, 0.72, 0.54, 0.25 },
        { 0.28, 0.19, 0.08, 0.44, 0.31, 0.14 },
    }
    local face = faces[state]
    button.face:SetGradientAlpha("VERTICAL", face[1], face[2], face[3], 1, face[4], face[5], face[6], 1)
end

local function BuildLoginButton(card, button)
    button.face = button:CreateTexture(nil, "BACKGROUND")
    button.face:SetTexture(WHITE)
    button.face:SetPoint("TOPLEFT", 1, -1)
    button.face:SetPoint("BOTTOMRIGHT", -1, 1)
    local sheen = Fade(button, "BORDER", "VERTICAL", 1, 0.95, 0.8, 0, 0.16)
    sheen:SetPoint("TOPLEFT", 1, -1)
    sheen:SetPoint("BOTTOMRIGHT", button, "RIGHT", -1, 0)
    ColorEdges(Edges(button, "BORDER"), HEADING[1], HEADING[2], HEADING[3], 0.9)

    -- Its light, on the card under it
    button.glow = Piece(card, "ARTWORK", GLOW)
    button.glow:SetBlendMode("ADD")
    button.glow:SetPoint("TOPLEFT", button, "TOPLEFT", -40, 26)
    button.glow:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 40, -26)
    button.lit = 0

    button:SetText(L.login)
    SetButtonFace(button)
end

local function BuildCard()
    local card = AccountLoginCard
    ui.card = card
    card:SetWidth(CARD_WIDTH)
    -- No box: the fields stand on the art like the logo and the links, over a soft shadow with no edge that keeps
    -- them readable on the bright plaza
    card:SetBackdrop(nil)

    local shadowFrame = CreateFrame("Frame", nil, AccountLoginUI)
    shadowFrame:SetFrameLevel(AccountLoginUI:GetFrameLevel())
    shadowFrame:SetPoint("TOPLEFT", card, "TOPLEFT", -150, 90)
    shadowFrame:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", 150, -90)
    local shadow = Piece(shadowFrame, "BACKGROUND", GLOW)
    shadow:SetAllPoints()
    shadow:SetVertexColor(0, 0, 0, 0.74)
    ui.cardShadow = shadowFrame

    local heading = Text(card, FONT_TITLE, 22, "OVERLAY", HEADING[1], HEADING[2], HEADING[3])
    heading:SetPoint("TOP", card, "TOP", 0, -20)
    heading:SetText(L.heading)
    local divider = Piece(card, "OVERLAY", DIVIDER)
    divider:SetSize(250, 5)
    divider:SetPoint("TOP", card, "TOP", 0, -50)

    local account, password = AccountLoginAccountEdit, AccountLoginPasswordEdit
    for _, edit in ipairs({ account, password }) do
        edit:ClearAllPoints()
        edit:SetSize(FIELD_WIDTH, FIELD_HEIGHT)
    end
    account:SetPoint("TOP", card, "TOP", 0, ACCOUNT_Y)
    password:SetPoint("TOP", card, "TOP", 0, PASSWORD_Y)
    BuildField(card, account, L.account, L.accountHint)
    BuildField(card, password, L.password, L.passwordHint)

    BuildOption(AccountLoginSaveAccountName, AccountLoginSaveAccountNameText, L.saveAccount)
    BuildOption(AccountLoginSavePassword, AccountLoginSavePasswordText, L.savePassword)
    BuildOption(AccountLoginAutoLogin, AccountLoginAutoLoginText, L.autoLogin)

    local button = AccountLoginLoginButton
    button:ClearAllPoints()
    button:SetSize(FIELD_WIDTH, BUTTON_HEIGHT)
    button:SetPoint("BOTTOM", card, "BOTTOM", 0, BOTTOM_PAD)
    BuildLoginButton(card, button)
end

local function BuildLinks()
    local order = {
        { AccountLoginExitButton, L.quit },
        { OptionsButton, L.options },
        { AccountLoginCinematicsButton, L.cinematics },
    }
    local previous
    for _, entry in ipairs(order) do
        local button, label = entry[1], entry[2]
        button:SetText(label)
        button:SetWidth(button:GetTextWidth() + 16)
        button:SetHeight(24)
        button:ClearAllPoints()
        if previous then
            local dot = Text(AccountLoginUI, FONT_TEXT, 12, "OVERLAY", MUTED[1], MUTED[2], MUTED[3])
            dot:SetText("·")
            dot:SetPoint("RIGHT", previous, "LEFT", -2, 0)
            button:SetPoint("RIGHT", dot, "LEFT", -2, 0)
            links[#links + 1] = { dot = dot }
        else
            button:SetPoint("BOTTOMRIGHT", AccountLoginUI, "BOTTOMRIGHT", -22, 16)
        end
        local underline = Piece(button, "OVERLAY", DIVIDER)
        underline:SetHeight(2)
        underline:SetPoint("BOTTOMLEFT", button, "BOTTOMLEFT", 4, 2)
        underline:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", -4, 2)
        underline:SetAlpha(0)
        local link = { button = button, underline = underline, lit = 0 }
        button:HookScript("OnEnter", function() link.hover = true end)
        button:HookScript("OnLeave", function() link.hover = false end)
        links[#links + 1] = link
        previous = button
    end

    -- The community buttons (only those with a link are shown), bottom left, above the realm
    local x = 20
    for _, name in ipairs({ "AccountLoginDiscord", "AccountLoginYoutube", "AccountLoginTikTok", "AccountLoginFacebook" }) do
        local button = _G[name]
        if button and button:IsShown() then
            button:ClearAllPoints()
            button:SetSize(30, 30)
            button:SetPoint("BOTTOMLEFT", AccountLoginUI, "BOTTOMLEFT", x, 54)
            x = x + 36
        end
    end

    UICache.realmName:SetFont(FONT_TEXT, 11, "")
    UICache.realmName:SetTextColor(TEXT[1], TEXT[2], TEXT[3])
    UICache.versionText:SetFont(FONT_TEXT, 10, "")
    UICache.versionText:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    AccountLoginUIResetFrameText:SetText(L.resetPending)
end

-- Stacks the ticked options' rows (a row appears once the one above is ticked) and sizes the card to them
function AccountLogin_LayoutCard()
    local y, count = OPTIONS_Y, 0
    for _, row in ipairs(optionRows) do
        if row.check:IsShown() then
            row.check:ClearAllPoints()
            row.check:SetPoint("TOPLEFT", ui.card, "TOPLEFT", (CARD_WIDTH - FIELD_WIDTH) / 2 + 1, y)
            row.label:ClearAllPoints()
            row.label:SetPoint("LEFT", row.check, "RIGHT", 8, 0)
            -- The label clicks the box too
            row.check:SetHitRectInsets(0, -(row.label:GetStringWidth() + 10), -3, -3)
            y = y - OPTION_ROW
            count = count + 1
        end
    end
    cardTargetHeight = -OPTIONS_Y + math.max(count, 1) * OPTION_ROW - (OPTION_ROW - 14) + BUTTON_GAP
        + BUTTON_HEIGHT + BOTTOM_PAD
    if not cardHeight then
        cardHeight = cardTargetHeight
        ui.card:SetHeight(cardHeight)
    end
end

local function OptionsKey()
    local key = 0
    for index, row in ipairs(optionRows) do
        if row.check:IsShown() then
            key = key + 2 ^ index
        end
    end
    return key
end

local function UpdateInterface(elapsed)
    -- The title, then the card rising into place, then the bottom edge
    SizeLogo()
    if intro < 2 then
        local brand = OutCubic(Clamp01((intro - 0.35) / 1.2))
        ui.brand:SetAlpha(brand)
        ui.brand:ClearAllPoints()
        ui.brand:SetPoint("TOP", AccountLoginUI, "TOP", 0, LOGO_TOP + 10 * (1 - brand))

        local card = OutCubic(Clamp01((intro - 0.55) / 0.8))
        ui.card:SetAlpha(card)
        ui.cardShadow:SetAlpha(card)
        ui.card:ClearAllPoints()
        ui.card:SetPoint("TOP", AccountLoginUI, "BOTTOM", 0, CARD_TOP - 24 * (1 - card))

        local footer = OutCubic(Clamp01((intro - 0.9) / 0.6))
        for _, link in ipairs(links) do
            (link.button or link.dot):SetAlpha(footer)
        end
        UICache.realmName:SetAlpha(footer)
        UICache.versionText:SetAlpha(footer)
    end

    local key = OptionsKey()
    if key ~= layoutKey then
        layoutKey = key
        AccountLogin_LayoutCard()
    end
    if cardHeight and math.abs(cardHeight - cardTargetHeight) > 0.2 then
        cardHeight = Approach(cardHeight, cardTargetHeight, elapsed, 12)
        ui.card:SetHeight(cardHeight)
    end

    for _, field in ipairs(fields) do
        local target = field.focus and 1 or 0
        local hovered = field.hover and not field.focus
        if math.abs(field.lit - target) > 0.01 or field.shownHover ~= hovered then
            field.lit = Approach(field.lit, target, elapsed, 10)
            field.shownHover = hovered
            RefreshField(field)
        end
    end

    local button = AccountLoginLoginButton
    SetButtonFace(button)
    local target = button.hovered and 1 or 0
    if math.abs(button.lit - target) > 0.01 or not button.litShown then
        button.lit = Approach(button.lit, target, elapsed, 8)
        button.litShown = true
        button.glow:SetVertexColor(1, 0.78, 0.4, 0.22 + 0.4 * button.lit)
    end

    for _, link in ipairs(links) do
        if link.button then
            local wanted = (link.hover and link.button:IsEnabled() == 1) and 1 or 0
            if math.abs(link.lit - wanted) > 0.01 then
                link.lit = Approach(link.lit, wanted, elapsed, 10)
                link.underline:SetAlpha(0.9 * link.lit)
            end
        end
    end
end

local function BuildLook(self)
    if scene.art then return end
    BuildScene(self)
    BuildLogo()
    BuildCard()
    BuildLinks()

    if VirtualKeypadFrame then
        VirtualKeypadFrame:SetBackdropColor(0.04, 0.03, 0.02, 0.95)
        VirtualKeypadFrame:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3])
    end
end

function AccountLogin_FieldFocus(edit, focused)
    local field = edit.evolutionsField
    if field then field.focus = focused and true or false end
end

function AccountLogin_FieldHover(edit, hovered)
    local field = edit.evolutionsField
    if field then field.hover = hovered and true or false end
end

function AccountLogin_ButtonHover(button, hovered)
    button.hovered = hovered and true or nil
    if not hovered then
        button.pressed = nil
    end
end
-- ============================================================================
-- ACTUALIZACIÓN DE LA ESCENA
-- ============================================================================
function AccountLogin_OnUpdate(self, elapsed)
    if LoginState.autoLoginTimer and LoginState.autoLoginTimer < LoginState.autoLoginDelay then
        LoginState.autoLoginTimer = LoginState.autoLoginTimer + elapsed
        if LoginState.autoLoginTimer >= LoginState.autoLoginDelay then
            AccountLogin_Login()
            LoginState.autoLoginTimer = nil
        end
    end

    if not scene.art then return end
    -- A long pause (loading, a movie) must not make the embers jump
    elapsed = math.min(elapsed or 0, 0.1)
    clock = clock + elapsed
    intro = intro + elapsed
    UpdateScene(self, elapsed, OutCubic(Clamp01(intro / 1.4)))
    UpdateInterface(elapsed)
end
-- ============================================================================
-- INICIALIZACIÓN Y EVENTOS
-- ============================================================================
-- A dialog over the glue screens: the screen behind it dimmed, and its panel a solid dark card with the gold edge
-- (the stock dialog is see-through with a red alert edge, and the login card read through it)
local function StyleDialog(overlayParent, panel)
    if not overlayParent or not panel or panel.evolutionsStyled then return end
    panel.evolutionsStyled = true
    -- Past both sides of the glue screen, which the client keeps at 16:9 at most: the whole of a wider window dims
    local dim = overlayParent:CreateTexture(nil, "BACKGROUND")
    dim:SetPoint("TOPLEFT", overlayParent, "TOPLEFT", -3000, 0)
    dim:SetPoint("BOTTOMRIGHT", overlayParent, "BOTTOMRIGHT", 3000, 0)
    dim:SetTexture(0, 0, 0, 0.55)
    panel:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    panel:SetBackdropColor(0.04, 0.03, 0.02, 0.95)
    panel:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3])
    local fill = panel:CreateTexture(nil, "BACKGROUND")
    fill:SetPoint("TOPLEFT", 4, -4)
    fill:SetPoint("BOTTOMRIGHT", -4, 4)
    fill:SetTexture(0.04, 0.03, 0.02, 0.9)
end

function AccountLogin_OnLoad(self)
    InitializeUICache()
    StyleDialog(_G.GlueDialog, _G.GlueDialogBackground)
    StyleDialog(_G.CinematicsFrame, _G.CinematicsBackground)
    -- The stock wrong-account texts send players to Blizzard's old site
    _G.LOGIN_UNKNOWN_ACCOUNT = "Nom de compte ou mot de passe incorrect. Vérifiez l'orthographe et réessayez."
    _G.LOGIN_INCORRECT_PASSWORD = _G.LOGIN_UNKNOWN_ACCOUNT

    AccountLogin_SetupServerAlert()

    UICache.tosFrame.noticeType = "EULA"
    self:RegisterEvent("SHOW_SERVER_ALERT")
    self:RegisterEvent("SHOW_SURVEY_NOTIFICATION")
    self:RegisterEvent("CLIENT_ACCOUNT_MISMATCH")
    self:RegisterEvent("CLIENT_TRIAL")
    self:RegisterEvent("SCANDLL_ERROR")
    self:RegisterEvent("SCANDLL_FINISHED")

    local versionType, buildType, version, internalVersion, date = GetBuildInfo()
    UICache.versionText:SetText(L.version .. " " .. tostring(version) .. " (" .. tostring(internalVersion) .. ")")

    BuildLook(self)

    AcceptTOS()
    AcceptEULA()
end

function AccountLogin_OnShow(self)
    self:Show()
    self:SetAlpha(1)
    WorldOfWarcraftRating:Hide()
    BuildLook(self)
    intro = 0
    AccountLoginUI:Show()
    AccountLoginUI:SetAlpha(1)

    local accountName, password = unpack(string_explode(GetSavedAccountName(), "#&|&#"))
    UICache.accountEdit:SetText(accountName or "")
    UICache.passwordEdit:SetText(password or "")

    PlayGlueAmbience("GlueScreenUndead", Config.UNDEAD_AMBIENCE_FADE_TIME)
    AccountLogin_ShowUserAgreements()

    local serverName = GetServerName()
    if serverName then
        UICache.realmName:SetText("|cff9e9180" .. L.realm .. "|r   |cffffdb8c" .. serverName .. "|r")
    else
        UICache.realmName:SetText("|cff9e9180" .. L.noRealm .. "|r")
    end

    if accountName == "" then
        AccountLogin_FocusAccountName()
    else
        AccountLogin_FocusPassword()
    end

    if UICache.savePassword:GetChecked() then
        UICache.autoLoginText:Show()
        UICache.autoLogin:Show()
    else
        UICache.autoLoginText:Hide()
        UICache.autoLogin:Hide()
    end

    if IsTrialAccount() then
        UICache.upgradeButton:Show()
    else
        UICache.upgradeButton:Hide()
    end
    ACCOUNT_MSG_NUM_AVAILABLE = 0
    ACCOUNT_MSG_PRIORITY = 0
    ACCOUNT_MSG_HEADERS_LOADED = false
    ACCOUNT_MSG_BODY_LOADED = false
    ACCOUNT_MSG_CURRENT_INDEX = nil

    layoutKey = nil
    cardHeight = nil
    AccountLogin_LayoutCard()
    layoutKey = OptionsKey()
    UpdateInterface(0)

    AccountLogin_CheckAutoLogin()
    self:SetScript("OnUpdate", AccountLogin_OnUpdate)
end

function AccountLogin_OnHide(self)
    StopAllSFX(Config.SFX_STOP_TIME)
    if not UICache.saveAccountName:GetChecked() then
        SetSavedAccountList("")
    end
    self:SetScript("OnUpdate", nil)
    StopGlueAmbience()
end

function AccountLogin_FocusPassword()
    UICache.passwordEdit:SetFocus()
end

function AccountLogin_FocusAccountName()
    UICache.accountEdit:SetFocus()
end

function AccountLogin_OnKeyDown(key)
    if key == "ESCAPE" then
        if ConnectionHelpFrame:IsShown() then
            ConnectionHelpFrame:Hide()
            AccountLoginUI:Show()
        elseif SurveyNotificationFrame:IsShown() then
        else
            AccountLogin_Exit()
        end
    elseif key == "ENTER" then
        if not TOSAccepted() then
            return
        elseif TOSFrame:IsShown() or ConnectionHelpFrame:IsShown() then
            return
        elseif SurveyNotificationFrame:IsShown() then
            AccountLogin_SurveyNotificationDone(1)
        end
        AccountLogin_Login()
    elseif key == "PRINTSCREEN" then
        Screenshot()
    end
end

function AccountLogin_OnEvent(event, arg1, arg2, arg3)
    if event == "SHOW_SERVER_ALERT" then
        -- Evolutions: no news panel on the login screen
    elseif event == "SHOW_SURVEY_NOTIFICATION" then
        AccountLogin_ShowSurveyNotification()
    elseif event == "CLIENT_ACCOUNT_MISMATCH" then
        local accountExpansionLevel = arg1
        local installationExpansionLevel = arg2
        if accountExpansionLevel == 1 then
            GlueDialog_Show("CLIENT_ACCOUNT_MISMATCH", CLIENT_ACCOUNT_MISMATCH_BC)
        else
            GlueDialog_Show("CLIENT_ACCOUNT_MISMATCH", CLIENT_ACCOUNT_MISMATCH_LK)
        end
    elseif event == "CLIENT_TRIAL" then
        GlueDialog_Show("CLIENT_TRIAL")
    elseif event == "SCANDLL_ERROR" then
        GlueDialog:Hide()
        ScanDLLContinueAnyway()
        AccountLoginUI:Show()
    elseif event == "SCANDLL_FINISHED" then
        if arg1 == "OK" then
            GlueDialog:Hide()
            AccountLoginUI:Show()
        else
            AccountLogin.hackURL = _G["SCANDLL_URL_"..arg1]
            AccountLogin.hackName = arg2
            AccountLogin.hackType = arg1
            local formatString = _G["SCANDLL_MESSAGE_"..arg1]
            if arg3 == 1 then
                formatString = _G["SCANDLL_MESSAGE_HACKNOCONTINUE"]
            end
            local msg = format(formatString, AccountLogin.hackName, AccountLogin.hackURL)
            if arg3 == 1 then
                GlueDialog_Show("SCANDLL_HACKFOUND_NOCONTINUE", msg)
            else
                GlueDialog_Show("SCANDLL_HACKFOUND", msg)
            end
            PlaySoundFile("Sound\\Creature\\MobileAlertBot\\MobileAlertBotIntruderAlert01.wav")
        end
    end
end
-- ============================================================================
-- SISTEMA DE LOGIN CON GUARDADO DE CONTRASEÑA
-- ============================================================================
function AccountLogin_Login()
    PlaySound("gsLogin")
    local accountName = UICache.accountEdit:GetText()
    local password = UICache.passwordEdit:GetText()
    local savedData = ""

    if UICache.saveAccountName:GetChecked() then
        if UICache.savePassword:GetChecked() then
            local autoLoginFlag = UICache.autoLogin:GetChecked() and "1" or "0"
            savedData = accountName.."#&|&#"..password.."#&|&#"..autoLoginFlag
        else
            local autoLoginFlag = UICache.autoLogin:GetChecked() and "1" or "0"
            savedData = accountName.."#&|&#".."".."#&|&#"..autoLoginFlag
        end
    else
        if UICache.autoLogin:GetChecked() then
            GlueDialog_Show("AUTO_LOGIN_NEEDS_ACCOUNT")
            UICache.autoLogin:SetChecked(0)
        end
        SetSavedAccountName("")
        SetUsesToken(false)
    end

    if savedData ~= "" then
        SetSavedAccountName(savedData)
    end
    DefaultServerLogin(accountName, password)
end
-- ============================================================================
-- SISTEMA DE AUTOLOGIN
-- ============================================================================
function AccountLogin_CheckAutoLogin()
    if not LoginState.autoLoginAttempted then
        LoginState.autoLoginAttempted = true
        local savedAccountInfo = GetSavedAccountName()

        if savedAccountInfo and savedAccountInfo ~= "" then
            local accountData = string_explode(savedAccountInfo, "#&|&#")
            local accountName = accountData[1] or ""
            local password = accountData[2] or ""
            local autoLogin = accountData[3] or "0"

            if autoLogin == "1" and accountName ~= "" and password ~= "" then
                LoginState.autoLoginTimer = 0
                LoginState.autoLoginDelay = Config.AUTO_LOGIN_DELAY
            else
                LoginState.autoLoginTimer = nil
            end
        end
    end
end
-- ============================================================================
-- SERVER ALERT FRAME (Evolutions: no news panel on the login screen)
-- ============================================================================
function AccountLogin_ToggleServerAlert()
end

function AccountLogin_SetupServerAlert()
    if ServerAlertFrame then
        ServerAlertFrame:Hide()
        ServerAlertFrame.isAnimating = false
        ServerAlertFrame.fadeInfo = nil
    end
end
-- ============================================================================
-- FUNCIONES ADICIONALES
-- ============================================================================
function AccountLogin_TOS()
    if not GlueDialog:IsShown() then
        PlaySound("gsLoginNewAccount")
        AccountLoginUI:Hide()
        UICache.tosFrame:Show()
        TOSScrollFrameScrollBar:SetValue(0)
        TOSScrollFrame:Show()
        TOSFrameTitle:SetText(TOS_FRAME_TITLE)
        TOSText:Show()
    end
end

function AccountLogin_ManageAccount()
    PlaySound("gsLoginNewAccount")
    LaunchURL(AUTH_NO_TIME_URL)
end

function AccountLogin_LaunchCommunitySite()
    PlaySound("gsLoginNewAccount")
    LaunchURL(COMMUNITY_URL)
end

function CharacterSelect_UpgradeAccount()
    PlaySound("gsLoginNewAccount")
    LaunchURL(AUTH_NO_TIME_URL)
end

function AccountLogin_Credits()
    CreditsFrame.creditsType = 3
    PlaySound("gsTitleCredits")
    SetGlueScreen("credits")
end

function AccountLogin_Cinematics()
    if not GlueDialog:IsShown() then
        PlaySound("gsLoginNewAccount")
        MOVIE_RETURN_SCREEN = "login"
        if CinematicsFrame.numMovies > 1 then
            CinematicsFrame:Show()
        else
            MovieFrame.version = 1
            SetGlueScreen("movie")
        end
    end
end

function AccountLogin_Options()
    PlaySound("gsTitleOptions")
end

function AccountLogin_Exit()
    QuitGame()
end

function AccountLogin_ShowSurveyNotification()
    GlueDialog:Hide()
    AccountLoginUI:Hide()
    SurveyNotificationAccept:Enable()
    SurveyNotificationDecline:Enable()
    SurveyNotificationFrame:Show()
end

function AccountLogin_SurveyNotificationDone(accepted)
    SurveyNotificationFrame:Hide()
    SurveyNotificationAccept:Disable()
    SurveyNotificationDecline:Disable()
    SurveyNotificationDone(accepted)
    AccountLoginUI:Show()
end

function AccountLogin_ShowUserAgreements()
    TOSScrollFrame:Hide()
    EULAScrollFrame:Hide()
    TerminationScrollFrame:Hide()
    ScanningScrollFrame:Hide()
    ContestScrollFrame:Hide()
    TOSText:Hide()
    EULAText:Hide()
    TerminationText:Hide()
    ScanningText:Hide()

    if not EULAAccepted() then
        if ShowEULANotice() then
            TOSNotice:SetText(EULA_NOTICE)
            TOSNotice:Show()
        end
        AccountLoginUI:Hide()
        TOSFrame.noticeType = "EULA"
        TOSFrameTitle:SetText(EULA_FRAME_TITLE)
        TOSFrameHeader:SetWidth(TOSFrameTitle:GetWidth())
        EULAScrollFrame:Show()
        EULAText:Show()
        TOSFrame:Show()
    elseif not TOSAccepted() then
        if ShowTOSNotice() then
            TOSNotice:SetText(TOS_NOTICE)
            TOSNotice:Show()
        end
        AccountLoginUI:Hide()
        TOSFrame.noticeType = "TOS"
        TOSFrameTitle:SetText(TOS_FRAME_TITLE)
        TOSFrameHeader:SetWidth(TOSFrameTitle:GetWidth())
        TOSScrollFrame:Show()
        TOSText:Show()
        TOSFrame:Show()
    elseif not IsScanDLLFinished() then
        AccountLoginUI:Hide()
        TOSFrame:Hide()
        local dllURL = ""
        if IsWindowsClient() then
            dllURL = SCANDLL_URL_WIN32_SCAN_DLL
        end
        ScanDLLStart(SCANDLL_URL_LAUNCHER_TXT, dllURL)
    else
        AccountLoginUI:Show()
        TOSFrame:Hide()
    end
end

function AccountLogin_UpdateAcceptButton(scrollFrame, isAcceptedFunc, noticeType)
    local scrollbar = _G[scrollFrame:GetName().."ScrollBar"]
    local min, max = scrollbar:GetMinMaxValues()

    if scrollbar:GetValue() >= max - Config.SCROLL_THRESHOLD then
        UICache.tosAccept:Enable()
    else
        if not isAcceptedFunc() and UICache.tosFrame.noticeType == noticeType then
            UICache.tosAccept:Disable()
        end
    end
end
-- ============================================================================
-- FUNCIONES DE CINEMATICS
-- ============================================================================
function CinematicsFrame_OnLoad(self)
    CinematicsBackground:SetBackdropColor(0.04, 0.03, 0.02, 0.95)
    CinematicsBackground:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3])
    StyleDialog(CinematicsFrame, CinematicsBackground)

    local numMovies = GetClientExpansionLevel()
    CinematicsFrame.numMovies = numMovies
    if numMovies < 2 then
        return
    end

    for i = 1, numMovies do
        _G["CinematicsButton"..i]:Show()
    end
    CinematicsBackground:SetHeight(numMovies * 40 + 70)
end

function CinematicsFrame_OnKeyDown(key)
    if key == "PRINTSCREEN" then
        Screenshot()
    else
        PlaySound("igMainMenuOptionCheckBoxOff")
        CinematicsFrame:Hide()
    end
end

function Cinematics_PlayMovie(self)
    CinematicsFrame:Hide()
    PlaySound("gsTitleOptionOK")
    MovieFrame.version = self:GetID()
    SetGlueScreen("movie")
end
-- ============================================================================
-- FUNCIONES AUXILIARES
-- ============================================================================
function string_explode(str, div)
    assert(type(str) == "string" and type(div) == "string", "invalid arguments")
    local o = {}
    while true do
        local pos1, pos2 = str:find(div, 1, true)
        if not pos1 then
            o[#o+1] = str
            break
        end
        o[#o+1], str = str:sub(1, pos1-1), str:sub(pos2+1)
    end
    return o
end
-- ============================================================================
-- TOKEN SYSTEM - FUNCIONES SIMPLES
-- ============================================================================
function TokenEnterDialog_Okay(self)
    local editBox = TokenEnterDialogBackgroundEdit
    if not editBox then return end

    local text = editBox:GetText()
    if not text or string.len(text) < 6 then return end

    TokenEntered(text)
    TokenEnterDialog:Hide()
end

function TokenEnterDialog_Cancel(self)
    if TokenEnterDialog then
        TokenEnterDialog:Hide()
    end
    CancelLogin()
end

function TokenEntry_Okay(self)
    TokenEnterDialog_Okay(self)
end

function TokenEntry_Cancel(self)
    TokenEnterDialog_Cancel(self)
end

function TokenEntryOkayButton_OnLoad(self)
    self:RegisterEvent("PLAYER_ENTER_TOKEN")
end

function TokenEntryOkayButton_OnEvent(self, event)
    if event == "PLAYER_ENTER_TOKEN" then
        if AccountLoginSaveAccountName:GetChecked() then
            if GetUsesToken() then
                if AccountLoginTokenEdit:GetText() ~= "" then
                    TokenEntered(AccountLoginTokenEdit:GetText())
                    return
                end
            else
                SetUsesToken(true)
            end
        end
        self:Show()
    end
end

function TokenEntryOkayButton_OnShow()
    if TokenEnterDialogBackgroundEdit then
        TokenEnterDialogBackgroundEdit:SetText("")
        TokenEnterDialogBackgroundEdit:SetFocus()
    end
end

function TokenEntryOkayButton_OnKeyDown(self, key)
    if key == "ENTER" then
        TokenEntry_Okay(self)
    elseif key == "ESCAPE" then
        TokenEntry_Cancel(self)
    end
end
-- ============================================================================
