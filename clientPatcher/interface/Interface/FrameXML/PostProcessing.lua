-- The world's post-processing (the client extension's PostFx: ambient occlusion, glow, sharpening, colour grading),
-- set from its own category of the Video options. Every setting is a CVar the DLL reads each frame, so a slider shows
-- its effect at once; Cancel puts back what was there when the window opened. /rendu switches it on and off.

-- Without the client extension there is nothing to set
if not GetCVar("postFx") then
    return
end

local french = GetLocale() == "frFR"
local TEXT = french and {
    category = "Rendu avancé",
    title = "Rendu avancé",
    subtext = "Ombres de contact, lueur, netteté et étalonnage des couleurs, appliqués au monde seulement : l'interface reste intacte.",
    enable = "Activer le rendu avancé",
    presets = "Ambiances",
    natural = "Naturel",
    vivid = "Vivant",
    cinema = "Cinéma",
    note = "Les ombres de contact demandent le multi-échantillonnage (MSAA) désactivé dans Résolution. /rendu active ou coupe le rendu.",
    on = "Rendu avancé activé.",
    off = "Rendu avancé désactivé.",
    ao = "Ombres de contact",
    bloom = "Lueur",
    sharpen = "Netteté",
    contrast = "Contraste",
    vibrance = "Éclat des couleurs",
    saturation = "Saturation",
    warmth = "Température",
    tone = "Teinte cinéma",
    vignette = "Vignettage",
    exposure = "Exposition",
} or {
    category = "Enhanced Rendering",
    title = "Enhanced Rendering",
    subtext = "Contact shadows, glow, sharpening and colour grading, applied to the world only: the interface stays untouched.",
    enable = "Enable enhanced rendering",
    presets = "Moods",
    natural = "Natural",
    vivid = "Vivid",
    cinema = "Cinematic",
    note = "Contact shadows need multisampling (MSAA) turned off under Resolution. /rendu turns the rendering on or off.",
    on = "Enhanced rendering on.",
    off = "Enhanced rendering off.",
    ao = "Contact shadows",
    bloom = "Glow",
    sharpen = "Sharpness",
    contrast = "Contrast",
    vibrance = "Colour vibrance",
    saturation = "Saturation",
    warmth = "Temperature",
    tone = "Cinematic tone",
    vignette = "Vignette",
    exposure = "Exposure",
}

-- The sliders, in two columns, with their CVar and range
local SETTINGS = {
    { key = "ao", cvar = "postFxAO", min = 0, max = 100 },
    { key = "bloom", cvar = "postFxBloom", min = 0, max = 100 },
    { key = "sharpen", cvar = "postFxSharpen", min = 0, max = 100 },
    { key = "contrast", cvar = "postFxContrast", min = 0, max = 100 },
    { key = "exposure", cvar = "postFxExposure", min = 50, max = 150 },
    { key = "vibrance", cvar = "postFxVibrance", min = 0, max = 100 },
    { key = "saturation", cvar = "postFxSaturation", min = 0, max = 200 },
    { key = "warmth", cvar = "postFxWarmth", min = -100, max = 100 },
    { key = "tone", cvar = "postFxTone", min = 0, max = 100 },
    { key = "vignette", cvar = "postFxVignette", min = 0, max = 100 },
}

local PRESETS = {
    natural = { postFxAO = 50, postFxBloom = 25, postFxSharpen = 35, postFxContrast = 15, postFxExposure = 100,
        postFxVibrance = 15, postFxSaturation = 100, postFxWarmth = 0, postFxTone = 0, postFxVignette = 15 },
    vivid = { postFxAO = 60, postFxBloom = 40, postFxSharpen = 50, postFxContrast = 30, postFxExposure = 100,
        postFxVibrance = 35, postFxSaturation = 105, postFxWarmth = 5, postFxTone = 20, postFxVignette = 20 },
    cinema = { postFxAO = 70, postFxBloom = 55, postFxSharpen = 40, postFxContrast = 40, postFxExposure = 100,
        postFxVibrance = 20, postFxSaturation = 95, postFxWarmth = 0, postFxTone = 60, postFxVignette = 40 },
}
local DEFAULT_PRESET = "natural"

local panel = CreateFrame("Frame", "PostProcessingPanel", VideoOptionsFramePanelContainer)
panel:Hide()
panel.name = TEXT.category

local title = panel:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 16, -16)
title:SetText(TEXT.title)

local subtext = panel:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
subtext:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -8)
subtext:SetPoint("RIGHT", -32, 0)
subtext:SetHeight(28)       -- two lines: a fixed height lets it wrap instead of cutting it off
subtext:SetJustifyH("LEFT")
subtext:SetJustifyV("TOP")
subtext:SetText(TEXT.subtext)

local enable = CreateFrame("CheckButton", "PostProcessingPanelEnable", panel, "OptionsCheckButtonTemplate")
enable:SetPoint("TOPLEFT", subtext, "BOTTOMLEFT", -2, -8)
_G[enable:GetName() .. "Text"]:SetText(TEXT.enable)

local presetLabel = panel:CreateFontString(nil, "ARTWORK", "GameFontNormal")
presetLabel:SetPoint("TOPLEFT", enable, "BOTTOMLEFT", 2, -16)
presetLabel:SetText(TEXT.presets)

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
    previous = { postFx = GetCVar("postFx") }
    for _, setting in ipairs(SETTINGS) do
        previous[setting.cvar] = GetCVar(setting.cvar)
    end
end

local lastButton
for _, name in ipairs({ "natural", "vivid", "cinema" }) do
    local button = CreateFrame("Button", nil, panel, "UIPanelButtonTemplate")
    button:SetSize(80, 22)
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

for index, setting in ipairs(SETTINGS) do
    local column, row = (index - 1) >= 5 and 1 or 0, (index - 1) % 5
    local slider = CreateFrame("Slider", "PostProcessingPanelSlider" .. index, panel, "OptionsSliderTemplate")
    slider.setting = setting
    slider:SetWidth(150)
    slider:SetPoint("TOPLEFT", presetLabel, "BOTTOMLEFT", 8 + column * 180, -38 - row * 46)
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
    sliders[index] = slider
end

local note = panel:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
note:SetPoint("TOPLEFT", sliders[5], "BOTTOMLEFT", -8, -24)
note:SetPoint("RIGHT", -32, 0)
note:SetHeight(28)
note:SetJustifyH("LEFT")
note:SetJustifyV("TOP")
note:SetText(TEXT.note)

enable:SetScript("OnClick", function(self)
    setCVar("postFx", self:GetChecked() and 1 or 0)
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
    applyPreset(DEFAULT_PRESET)
end
panel:SetScript("OnShow", function()
    if not previous then
        remember()
    end
    refresh()
end)

OptionsFrame_AddCategory(VideoOptionsFrame, panel)

SLASH_POSTPROCESSING1 = "/rendu"
SlashCmdList["POSTPROCESSING"] = function()
    local on = GetCVar("postFx") ~= "1"
    SetCVar("postFx", on and "1" or "0")
    DEFAULT_CHAT_FRAME:AddMessage(on and TEXT.on or TEXT.off, 1, 0.82, 0)
end
