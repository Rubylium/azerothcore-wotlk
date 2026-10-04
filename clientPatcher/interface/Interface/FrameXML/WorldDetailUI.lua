-- The Effects panel of the Video options reaches as far as the client extension lets the world be drawn
-- (awesome_wotlk WorldDetail.cpp): view distance to the engine's 1583 yards on every map, objects' detail to 3,
-- the grass's density to 256 and its radius to 300 yards. The panel's title row shows the game's memory, as all of it
-- costs some: a 32-bit client has 4 GB of address space at most.

-- Without the client extension the stock limits stand
if not WorldDetail_GetMemory then
    return
end

local french = GetLocale() == "frFR"
local TEXT = french and {
    memory = "Mémoire : %d / 4 096 Mo · bloc libre : %d Mo",
    low = "Presque pleine : baissez la distance de vue ou le décor au sol.",
} or {
    memory = "Memory: %d / 4,096 MB · free block: %d MB",
    low = "Nearly full: lower the view distance or the ground clutter.",
}

-- The widened sliders: their CVar, range and step
local SLIDERS = {
    { name = "VideoOptionsEffectsPanelViewDistance", cvar = "farclip", min = 177, max = 1583, step = 140.6 },
    { name = "VideoOptionsEffectsPanelEnvironmentDetail", cvar = "environmentDetail", min = 0.5, max = 3, step = 0.25 },
    { name = "VideoOptionsEffectsPanelClutterDensity", cvar = "groundEffectDensity", min = 16, max = 256, step = 16 },
    { name = "VideoOptionsEffectsPanelClutterRadius", cvar = "groundEffectDist", min = 70, max = 300, step = 10 },
}

local function Widen()
    for _, entry in ipairs(SLIDERS) do
        local slider = _G[entry.name]
        local option = EffectsPanelOptions and EffectsPanelOptions[entry.cvar]
        if option then
            option.minValue, option.maxValue, option.valueStep = entry.min, entry.max, entry.step
        end
        if slider then
            slider:SetMinMaxValues(entry.min, entry.max)
            slider:SetValueStep(entry.step)
            local value = tonumber(GetCVar(entry.cvar))
            if value then
                slider:SetValue(value)
            end
        end
    end
end

-- The memory line, kept current while the panel shows
local memory
local function ShowMemory()
    local used, largestFree = WorldDetail_GetMemory()
    if not used then
        return
    end
    memory:SetText(format(TEXT.memory, used, largestFree))
    memory.low = largestFree < 256 or used > 3600
    if memory.low then
        memory:SetTextColor(1, 0.55, 0.2)
    else
        memory:SetTextColor(0.85, 0.75, 0.55)
    end
end

local panel = VideoOptionsEffectsPanel
if panel then
    -- On the title's row, at the right: the panel has no free line below its sliders
    memory = panel:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    memory:SetPoint("TOPRIGHT", -16, -20)
    memory:SetJustifyH("RIGHT")
    -- Nearly full: why, on hover
    local hover = CreateFrame("Frame", nil, panel)
    hover:SetAllPoints(memory)
    hover:EnableMouse(true)
    hover:SetScript("OnEnter", function(self)
        if memory.low then
            GameTooltip:SetOwner(self, "ANCHOR_BOTTOMLEFT")
            GameTooltip:SetText(TEXT.low, 1, 0.82, 0, true)
            GameTooltip:Show()
        end
    end)
    hover:SetScript("OnLeave", function() GameTooltip:Hide() end)
    local elapsedSince = 0
    panel:HookScript("OnShow", function()
        elapsedSince = 0
        ShowMemory()
    end)
    panel:HookScript("OnUpdate", function(self, elapsed)
        elapsedSince = elapsedSince + elapsed
        if elapsedSince > 1 then
            elapsedSince = 0
            ShowMemory()
        end
    end)
end

-- After the panel's own PLAYER_ENTERING_WORLD (it sets the view distance slider's step from its range)
local loader = CreateFrame("Frame")
loader:RegisterEvent("PLAYER_ENTERING_WORLD")
loader:SetScript("OnEvent", function()
    -- The old continents' view distance is held at 791 yards unless this is on
    if GetCVar("farClipOverride") ~= "1" then
        SetCVar("farClipOverride", "1")
    end
    Widen()
end)
