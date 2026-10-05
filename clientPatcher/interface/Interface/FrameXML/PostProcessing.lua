-- The world's post-processing (the client extension's PostFx: lights, shadows, fog, reflections, glow, colour), set
-- from its own category of the Video options. Every setting is a CVar the DLL reads each frame, so a slider shows its
-- effect at once; Cancel puts back what was there when the window opened. /rendu switches it on and off.
--
-- The sliders stand in four sections (light and shadow, atmosphere, image, colour) inside a scrolling area: the
-- window's own buttons stay clear of them however many there are.

-- Without the client extension there is nothing to set
if not GetCVar("postFx") then
    return
end

local french = GetLocale() == "frFR"
local TEXT = french and {
    category = "Rendu avancé",
    title = "Rendu avancé",
    subtext = "Appliqué au monde seulement : l'interface reste intacte.",
    enable = "Activer le rendu avancé",
    antiAlias = "Anticrénelage",
    presets = "Ambiances",
    natural = "Naturel",
    vivid = "Vivant",
    cinema = "Cinéma",
    lightSection = "Lumière et ombres",
    airSection = "Atmosphère",
    imageSection = "Image",
    colourSection = "Couleurs",
    note = "Les ombres, la brume et l'eau ont besoin du MSAA désactivé : l'anticrénelage le remplace. Le "
        .. "brouillard ne monte que la nuit. "
        .. "/rendu active ou coupe le rendu avancé.",
    on = "Rendu avancé activé.",
    off = "Rendu avancé désactivé.",
    ao = "Ombres de contact",
    lights = "Lumières des flammes",
    night = "Nuits sombres",
    fog = "Brouillard et halos",
    haze = "Brume lointaine",
    shafts = "Rayons de soleil",
    water = "Reflets de l'eau",
    focus = "Profondeur de champ",
    bloom = "Lueur",
    sharpen = "Netteté",
    exposure = "Exposition",
    contrast = "Contraste",
    vibrance = "Éclat des couleurs",
    saturation = "Saturation",
    warmth = "Température",
    tone = "Teinte cinéma",
    vignette = "Vignettage",
} or {
    category = "Enhanced Rendering",
    title = "Enhanced Rendering",
    subtext = "Applied to the world only: the interface stays untouched.",
    enable = "Enable enhanced rendering",
    antiAlias = "Anti-aliasing",
    presets = "Moods",
    natural = "Natural",
    vivid = "Vivid",
    cinema = "Cinematic",
    lightSection = "Light and shadow",
    airSection = "Atmosphere",
    imageSection = "Image",
    colourSection = "Colour",
    note = "Shadows, mist and water need MSAA off: the anti-aliasing replaces it. The mist only rises at night. "
        .. "/rendu toggles enhanced rendering.",
    on = "Enhanced rendering on.",
    off = "Enhanced rendering off.",
    ao = "Contact shadows",
    lights = "Firelight",
    night = "Darker nights",
    fog = "Mist and halos",
    haze = "Distant haze",
    shafts = "Sun shafts",
    water = "Water reflections",
    focus = "Depth of field",
    bloom = "Glow",
    sharpen = "Sharpness",
    exposure = "Exposure",
    contrast = "Contrast",
    vibrance = "Colour vibrance",
    saturation = "Saturation",
    warmth = "Temperature",
    tone = "Cinematic tone",
    vignette = "Vignette",
}

-- The sections and their sliders, with their CVar and range. The depth of field only shows with the camera zoomed in
-- close on the character.
local SECTIONS = {
    { title = "lightSection", settings = {
        { key = "ao", cvar = "postFxAO", min = 0, max = 100 },
        { key = "lights", cvar = "postFxLights", min = 0, max = 100 },
        { key = "night", cvar = "postFxNight", min = 0, max = 100 },
    } },
    { title = "airSection", settings = {
        { key = "fog", cvar = "postFxFog", min = 0, max = 100 },
        { key = "haze", cvar = "postFxHaze", min = 0, max = 100 },
        { key = "shafts", cvar = "postFxShafts", min = 0, max = 100 },
        { key = "water", cvar = "postFxWater", min = 0, max = 100 },
    } },
    { title = "imageSection", settings = {
        { key = "focus", cvar = "postFxDoF", min = 0, max = 100 },
        { key = "bloom", cvar = "postFxBloom", min = 0, max = 100 },
        { key = "sharpen", cvar = "postFxSharpen", min = 0, max = 100 },
    } },
    { title = "colourSection", settings = {
        { key = "exposure", cvar = "postFxExposure", min = 50, max = 150 },
        { key = "contrast", cvar = "postFxContrast", min = 0, max = 100 },
        { key = "vibrance", cvar = "postFxVibrance", min = 0, max = 100 },
        { key = "saturation", cvar = "postFxSaturation", min = 0, max = 200 },
        { key = "warmth", cvar = "postFxWarmth", min = -100, max = 100 },
        { key = "tone", cvar = "postFxTone", min = 0, max = 100 },
        { key = "vignette", cvar = "postFxVignette", min = 0, max = 100 },
    } },
}

local PRESETS = {
    natural = { postFxAO = 50, postFxLights = 60, postFxShafts = 35, postFxHaze = 25, postFxWater = 60,
        postFxDoF = 60, postFxNight = 40, postFxFog = 40, postFxBloom = 25,
        postFxSharpen = 35, postFxContrast = 15, postFxExposure = 100, postFxVibrance = 15, postFxSaturation = 100,
        postFxWarmth = 0, postFxTone = 0, postFxVignette = 15 },
    vivid = { postFxAO = 60, postFxLights = 75, postFxShafts = 50, postFxHaze = 30, postFxWater = 70,
        postFxDoF = 60, postFxNight = 45, postFxFog = 45, postFxBloom = 40,
        postFxSharpen = 50, postFxContrast = 30, postFxExposure = 100, postFxVibrance = 35, postFxSaturation = 105,
        postFxWarmth = 5, postFxTone = 20, postFxVignette = 20 },
    cinema = { postFxAO = 70, postFxLights = 85, postFxShafts = 70, postFxHaze = 45, postFxWater = 80,
        postFxDoF = 80, postFxNight = 60, postFxFog = 60, postFxBloom = 55,
        postFxSharpen = 40, postFxContrast = 40, postFxExposure = 100, postFxVibrance = 20, postFxSaturation = 95,
        postFxWarmth = 0, postFxTone = 60, postFxVignette = 40 },
}
local DEFAULT_PRESET = "natural"

-- The layout: two columns of sliders sharing the scrolling area's width (measured when the panel shows: the options
-- window is narrower than it looks), each row tall enough for its label above and its range below
local ROW_HEIGHT = 48
local HEADER_HEIGHT = 30

local panel = CreateFrame("Frame", "PostProcessingPanel", VideoOptionsFramePanelContainer)
panel:Hide()
panel.name = TEXT.category

local title = panel:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 16, -16)
title:SetText(TEXT.title)

local subtext = panel:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
subtext:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -8)
subtext:SetPoint("RIGHT", -32, 0)
subtext:SetJustifyH("LEFT")
subtext:SetText(TEXT.subtext)

local enable = CreateFrame("CheckButton", "PostProcessingPanelEnable", panel, "OptionsCheckButtonTemplate")
enable:SetPoint("TOPLEFT", subtext, "BOTTOMLEFT", -2, -8)
_G[enable:GetName() .. "Text"]:SetText(TEXT.enable)

local antiAlias = CreateFrame("CheckButton", "PostProcessingPanelAntiAlias", panel, "OptionsCheckButtonTemplate")
antiAlias:SetPoint("LEFT", enable, "LEFT", 270, 0)
_G[antiAlias:GetName() .. "Text"]:SetText(TEXT.antiAlias)

local presetLabel = panel:CreateFontString(nil, "ARTWORK", "GameFontNormal")
presetLabel:SetPoint("TOPLEFT", enable, "BOTTOMLEFT", 2, -14)
presetLabel:SetText(TEXT.presets)

-- The scrolling area under the presets, down to just above the window's buttons
local scroll = CreateFrame("ScrollFrame", "PostProcessingPanelScroll", panel, "UIPanelScrollFrameTemplate")
scroll:SetPoint("TOPLEFT", presetLabel, "BOTTOMLEFT", -6, -14)
scroll:SetPoint("BOTTOMRIGHT", -30, 10)
local content = CreateFrame("Frame", nil, scroll)
content:SetSize(300, 10)
scroll:SetScrollChild(content)

local sliders = {}
local refreshing = false

local function sliderLabel(slider, value)
    _G[slider:GetName() .. "Text"]:SetText(TEXT[slider.setting.key] .. " : " .. value)
end

local function setCVar(cvar, value)
    SetCVar(cvar, tostring(value))
end

local function refresh()
    refreshing = true
    enable:SetChecked(GetCVar("postFx") == "1")
    antiAlias:SetChecked(GetCVar("postFxAA") == "1")
    for _, slider in ipairs(sliders) do
        local value = tonumber(GetCVar(slider.setting.cvar)) or 0
        slider:SetValue(value)
        sliderLabel(slider, value)
    end
    refreshing = false
end

local function applyPreset(name)
    for cvar, value in pairs(PRESETS[name]) do
        setCVar(cvar, value)
    end
    refresh()
end

local previous = nil
local function remember()
    previous = { postFx = GetCVar("postFx"), postFxAA = GetCVar("postFxAA") }
    for _, slider in ipairs(sliders) do
        previous[slider.setting.cvar] = GetCVar(slider.setting.cvar)
    end
end

local lastButton
for _, name in ipairs({ "natural", "vivid", "cinema" }) do
    local button = CreateFrame("Button", nil, panel, "UIPanelButtonTemplate")
    button:SetSize(90, 22)
    if lastButton then
        button:SetPoint("LEFT", lastButton, "RIGHT", 6, 0)
    else
        button:SetPoint("LEFT", presetLabel, "RIGHT", 12, 0)
    end
    button:SetText(TEXT[name])
    button:SetScript("OnClick", function()
        applyPreset(name)
    end)
    lastButton = button
end

-- The sections, one under the other: a heading with a rule, then the sliders two by two (placed by layout())
local headings = {}
for _, section in ipairs(SECTIONS) do
    local heading = content:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    heading:SetText(TEXT[section.title])
    local rule = content:CreateTexture(nil, "ARTWORK")
    rule:SetTexture(1, 0.82, 0, 0.25)
    rule:SetHeight(1)
    rule:SetPoint("LEFT", heading, "RIGHT", 8, 0)
    rule:SetPoint("RIGHT", content, "RIGHT", -6, 0)
    headings[#headings + 1] = heading
    section.sliders = {}
    for _, setting in ipairs(section.settings) do
        local slider = CreateFrame("Slider", "PostProcessingPanelSlider" .. (#sliders + 1), content,
            "OptionsSliderTemplate")
        slider.setting = setting
        slider:SetMinMaxValues(setting.min, setting.max)
        slider:SetValueStep(1)
        _G[slider:GetName() .. "Low"]:SetText(setting.min)
        _G[slider:GetName() .. "High"]:SetText(setting.max)
        slider:SetScript("OnValueChanged", function(self, value)
            value = math.floor(value + 0.5)
            sliderLabel(self, value)
            if not refreshing then
                setCVar(setting.cvar, value)
            end
        end)
        sliders[#sliders + 1] = slider
        section.sliders[#section.sliders + 1] = slider
    end
end

local note = content:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
note:SetJustifyH("LEFT")
note:SetJustifyV("TOP")
note:SetText(TEXT.note)

-- Everything placed for the scrolling area's width: two columns, a slider filling most of its column
local function layout()
    local width = math.max(scroll:GetWidth(), 260)
    content:SetWidth(width)
    local column = (width - 12) / 2
    local y = -4
    for s, section in ipairs(SECTIONS) do
        headings[s]:ClearAllPoints()
        headings[s]:SetPoint("TOPLEFT", 6, y)
        y = y - HEADER_HEIGHT
        for index, slider in ipairs(section.sliders) do
            local col, row = (index - 1) % 2, math.floor((index - 1) / 2)
            slider:ClearAllPoints()
            slider:SetWidth(column - 24)
            slider:SetPoint("TOPLEFT", 12 + col * column, y - 14 - row * ROW_HEIGHT)
        end
        y = y - math.ceil(#section.sliders / 2) * ROW_HEIGHT - 10
    end
    note:ClearAllPoints()
    note:SetPoint("TOPLEFT", 6, y - 4)
    note:SetWidth(width - 12)
    content:SetHeight(-y + note:GetHeight() + 24)
end

enable:SetScript("OnClick", function(self)
    setCVar("postFx", self:GetChecked() and 1 or 0)
end)
antiAlias:SetScript("OnClick", function(self)
    setCVar("postFxAA", self:GetChecked() and 1 or 0)
end)

panel.refresh = refresh
panel.okay = function()
    previous = nil
end
panel.cancel = function()
    if previous then
        for cvar, value in pairs(previous) do
            SetCVar(cvar, value)
        end
        previous = nil
    end
end
panel.default = function()
    setCVar("postFx", 0)
    setCVar("postFxAA", 1)
    applyPreset(DEFAULT_PRESET)
end
panel:SetScript("OnShow", function()
    if not previous then
        remember()
    end
    layout()
    refresh()
end)

OptionsFrame_AddCategory(VideoOptionsFrame, panel)

SLASH_POSTPROCESSING1 = "/rendu"
SlashCmdList["POSTPROCESSING"] = function()
    local on = GetCVar("postFx") ~= "1"
    SetCVar("postFx", on and "1" or "0")
    DEFAULT_CHAT_FRAME:AddMessage(on and TEXT.on or TEXT.off, 1, 0.82, 0)
end

-- Under a roof (a cave, a dungeon's halls) the sun lights nothing, whatever the hour: the client extension is told, so
-- its lights shine there as they do at night (postFxIndoors, never saved). The client has no event for every change,
-- so it is checked twice a second as well.
local indoorsWatch = CreateFrame("Frame")
local indoorsSent, indoorsWait = nil, 0
local function sendIndoors()
    local indoors = IsIndoors() and "1" or "0"
    if indoors ~= indoorsSent and GetCVar("postFxIndoors") then
        indoorsSent = indoors
        SetCVar("postFxIndoors", indoors)
    end
end
indoorsWatch:RegisterEvent("PLAYER_ENTERING_WORLD")
indoorsWatch:RegisterEvent("ZONE_CHANGED_INDOORS")
indoorsWatch:RegisterEvent("ZONE_CHANGED")
indoorsWatch:RegisterEvent("ZONE_CHANGED_NEW_AREA")
indoorsWatch:SetScript("OnEvent", sendIndoors)
indoorsWatch:SetScript("OnUpdate", function(_, elapsed)
    indoorsWait = indoorsWait - elapsed
    if indoorsWait <= 0 then
        indoorsWait = 0.5
        sendIndoors()
    end
end)
