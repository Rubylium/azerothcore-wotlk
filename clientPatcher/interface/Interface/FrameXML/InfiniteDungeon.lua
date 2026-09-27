-- The Infinite Dungeon (server: mod-stat-growth src/infinite/InfiniteDungeonSystem.cpp). An endless ladder of short
-- floors entered from Eternia, a keeper in every capital. While a run is under way a small panel takes the quest
-- tracker's place with the floor, the last checkpoint and what the floor asks for; a banner at the top of the screen
-- greets each floor and each floor cleared. Eternia is marked on the world map in every capital.
--
-- Protocol: prefix "Infinite", tab-separated, whispered to oneself.
--   server  HUD <floor> <checkpoint> <ladder> <step> <paragon> <state> <arena> <level> <players>
--           ARRIVE <floor> <arena> <ladder> <step> <paragon>
--           CLEAR <floor> <checkpoint reached 0/1>
--           END <reason> <floor>              reason: 0 left, 1 everyone fell, 2 never arrived
--   client  STATE                              asks for the panel again (entering the world)
-- ladder: 0 levelling, 1 gearing (level 80, in steps of five floors with a recommended paragon).
-- state: 0 on the way, 1 in the bubble, 2 fighting, 3 cleared, 4 fallen (InfiniteDungeonSystem.cpp FloorState).

local PREFIX = "Infinite"
local SOUND = "Sound\\Interface\\MythicPlus\\"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local FRIZ = "Fonts\\FRIZQT__.TTF"
local ICON = "Interface\\Icons\\Spell_Holy_BorrowedTime"

-- The challenge board's palette (ChallengeBoard.lua, Prestige.lua)
local GOLD = { 1, 0.82, 0.3 }
local BORDER = { 0.75, 0.6, 0.35 }
local PARCHMENT = { 1, 0.9, 0.7 }
local MUTED = { 0.62, 0.57, 0.5 }

local LADDER_GEARING = 1
local STATE_TRAVELLING, STATE_BUBBLE, STATE_FIGHTING, STATE_CLEARED, STATE_FALLEN = 0, 1, 2, 3, 4
local REASON_FALLEN = 1

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Donjon infini",
    floor = "Étage %d",
    checkpoint = "Point de passage : %d",
    noCheckpoint = "Aucun point de passage",
    nextCheckpoint = "prochain : %d",
    step = "Palier %d",
    paragon = "parangon conseillé %d",
    travelling = "En route vers l'étage…",
    bubble = "Sortez du cercle pour commencer",
    fighting = "Vainquez le gardien",
    cleared = "Le portail est ouvert",
    fallen = "La descente s'achève…",
    arrived = "Étage %d",
    clearedTitle = "Étage %d franchi",
    checkpointReached = "Point de passage atteint : votre progression est gardée",
    portalOpen = "Le portail mène plus bas",
    endTitle = "La descente s'achève",
    endText = "Étage %d atteint",
    mapTitle = "Donjon infini",
    mapKeeper = "Eternia, gardienne du Donjon infini",
    mapHint = "Dès le niveau 15, seul ou avec un partenaire de groupe",
} or {
    title = "Infinite Dungeon",
    floor = "Floor %d",
    checkpoint = "Checkpoint: %d",
    noCheckpoint = "No checkpoint yet",
    nextCheckpoint = "next: %d",
    step = "Step %d",
    paragon = "recommended paragon %d",
    travelling = "On the way down…",
    bubble = "Step out of the circle to begin",
    fighting = "Defeat the guardian",
    cleared = "The portal is open",
    fallen = "The descent ends…",
    arrived = "Floor %d",
    clearedTitle = "Floor %d cleared",
    checkpointReached = "Checkpoint reached: your progress is kept",
    portalOpen = "The portal leads further down",
    endTitle = "The descent ends",
    endText = "Reached floor %d",
    mapTitle = "Infinite Dungeon",
    mapKeeper = "Eternia, keeper of the Infinite Dungeon",
    mapHint = "From level 15, alone or with a group partner",
}

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

-- Tweens: the banner's animations -------------------------------------------------------------------------------

local tweens = {}
local driver = CreateFrame("Frame")

local function Tween(duration, delay, update, done)
    tinsert(tweens, { time = -(delay or 0), duration = duration, update = update, done = done })
end

local function OutCubic(p)
    local inverse = 1 - p
    return 1 - inverse * inverse * inverse
end

driver:SetScript("OnUpdate", function(_, elapsed)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        tween.time = tween.time + elapsed
        if tween.time >= 0 then
            local progress = min(1, tween.time / tween.duration)
            tween.update(progress)
            if progress >= 1 then
                tremove(tweens, index)
                if tween.done then
                    tween.done()
                end
            end
        end
    end
end)

-- The panel -------------------------------------------------------------------------------------------------------

local panel

local function CreatePanel()
    panel = CreateFrame("Frame", "InfiniteDungeonFrame", UIParent)
    panel:SetSize(236, 92)
    panel:SetFrameStrata("LOW")
    panel:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    panel:SetBackdropColor(0.04, 0.03, 0.02, 0.92)
    panel:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 1)
    panel:Hide()

    local icon = panel:CreateTexture(nil, "ARTWORK")
    icon:SetSize(30, 30)
    icon:SetPoint("TOPLEFT", panel, "TOPLEFT", 10, -10)
    icon:SetTexture(ICON)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)

    local title = panel:CreateFontString(nil, "OVERLAY")
    title:SetFont(MORPHEUS, 14)
    title:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    title:SetShadowOffset(1, -1)
    title:SetPoint("TOPLEFT", icon, "TOPRIGHT", 8, 1)
    title:SetText(TEXT.title)

    local floor = panel:CreateFontString(nil, "OVERLAY")
    floor:SetFont(FRIZ, 17, "OUTLINE")
    floor:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    floor:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -2)
    panel.floor = floor

    local checkpoint = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    checkpoint:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    checkpoint:SetPoint("TOPLEFT", icon, "BOTTOMLEFT", 0, -8)
    checkpoint:SetPoint("RIGHT", panel, "RIGHT", -10, 0)
    checkpoint:SetJustifyH("LEFT")
    panel.checkpoint = checkpoint

    local step = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    step:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    step:SetPoint("TOPLEFT", checkpoint, "BOTTOMLEFT", 0, -2)
    step:SetPoint("RIGHT", panel, "RIGHT", -10, 0)
    step:SetJustifyH("LEFT")
    panel.step = step

    local status = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    status:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    status:SetPoint("TOPLEFT", step, "BOTTOMLEFT", 0, -2)
    status:SetPoint("RIGHT", panel, "RIGHT", -10, 0)
    status:SetJustifyH("LEFT")
    panel.status = status
end

local function PlacePanel()
    if DungeonTracker_AnchorToQuestArea then
        DungeonTracker_AnchorToQuestArea(panel)
    else
        panel:ClearAllPoints()
        panel:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", -110, -220)
    end
end

local function SetPanelShown(shown)
    if not panel then
        if not shown then
            return
        end
        CreatePanel()
    end
    if shown then
        PlacePanel()
        panel:Show()
    else
        panel:Hide()
    end
    -- The panel takes the quest tracker's place, like the Mythic+ timer (DungeonTracker.lua)
    if DungeonTracker_ClaimQuestArea then
        DungeonTracker_ClaimQuestArea("infinite", shown)
    end
end

local STATUS = {
    [STATE_TRAVELLING] = "travelling",
    [STATE_BUBBLE] = "bubble",
    [STATE_FIGHTING] = "fighting",
    [STATE_CLEARED] = "cleared",
    [STATE_FALLEN] = "fallen",
}

local function UpdatePanel(floor, checkpoint, ladder, step, paragon, runState)
    SetPanelShown(true)
    panel.floor:SetText(format(TEXT.floor, floor))
    local nextCheckpoint = (math.floor((floor - 1) / 10) + 1) * 10
    if checkpoint > 0 then
        panel.checkpoint:SetText(format(TEXT.checkpoint, checkpoint) .. " · " .. format(TEXT.nextCheckpoint,
            nextCheckpoint))
    else
        panel.checkpoint:SetText(TEXT.noCheckpoint .. " · " .. format(TEXT.nextCheckpoint, nextCheckpoint))
    end
    if ladder == LADDER_GEARING then
        local text = format(TEXT.step, step + 1)
        if paragon > 0 then
            text = text .. " · " .. format(TEXT.paragon, paragon)
        end
        panel.step:SetText(text)
    else
        panel.step:SetText("")
    end
    panel.status:SetText(TEXT[STATUS[runState] or "fighting"])
    if runState == STATE_BUBBLE or runState == STATE_CLEARED then
        panel.status:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    else
        panel.status:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    end
end

-- The banner: a floor reached, a floor cleared ---------------------------------------------------------------------

local banner
local BANNER_SECONDS = 5

local function CreateBanner()
    banner = CreateFrame("Frame", "InfiniteDungeonBanner", UIParent)
    banner:SetSize(460, 64)
    banner:SetPoint("TOP", UIParent, "TOP", 0, -120)
    banner:SetFrameStrata("MEDIUM")
    banner:Hide()

    local back = banner:CreateTexture(nil, "BACKGROUND")
    back:SetTexture("Interface\\Buttons\\WHITE8X8")
    back:SetPoint("TOPLEFT")
    back:SetPoint("BOTTOMRIGHT", banner, "BOTTOM")
    back:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.8)
    local backRight = banner:CreateTexture(nil, "BACKGROUND")
    backRight:SetTexture("Interface\\Buttons\\WHITE8X8")
    backRight:SetPoint("TOPLEFT", banner, "TOP")
    backRight:SetPoint("BOTTOMRIGHT")
    backRight:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.8, 0, 0, 0, 0)

    for _, edge in ipairs({ "TOP", "BOTTOM" }) do
        local line = banner:CreateTexture(nil, "ARTWORK")
        line:SetTexture("Interface\\Buttons\\WHITE8X8")
        line:SetHeight(1)
        line:SetPoint(edge .. "LEFT", banner, edge .. "LEFT", 40, 0)
        line:SetPoint(edge .. "RIGHT", banner, edge .. "RIGHT", -40, 0)
        line:SetVertexColor(BORDER[1], BORDER[2], BORDER[3], 0.8)
    end

    local icon = banner:CreateTexture(nil, "ARTWORK")
    icon:SetSize(40, 40)
    icon:SetPoint("LEFT", banner, "LEFT", 70, 0)
    icon:SetTexture(ICON)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)

    local title = banner:CreateFontString(nil, "OVERLAY")
    title:SetFont(MORPHEUS, 20)
    title:SetShadowOffset(1, -1)
    title:SetTextColor(1, 0.86, 0.5)
    title:SetPoint("TOPLEFT", icon, "TOPRIGHT", 12, 2)
    title:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    title:SetJustifyH("LEFT")
    banner.title = title

    local text = banner:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    text:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    text:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -3)
    text:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    text:SetJustifyH("LEFT")
    banner.text = text

    banner:SetScript("OnUpdate", function(self)
        if self.hideAt and GetTime() >= self.hideAt then
            self.hideAt = nil
            Tween(0.5, 0, function(p) self:SetAlpha(1 - p) end, function() self:Hide() end)
        end
    end)
end

local function ShowBanner(title, text, sound)
    if not banner then
        CreateBanner()
    end
    banner.title:SetText(title or "")
    banner.text:SetText(text or "")
    banner.hideAt = GetTime() + BANNER_SECONDS
    if not banner:IsShown() then
        banner:SetAlpha(0)
        banner:Show()
    end
    Tween(0.3, 0, function(p)
        banner:SetAlpha(max(banner:GetAlpha(), p))
        banner:SetScale(1.08 - 0.08 * OutCubic(p))
    end)
    if sound then
        PlaySoundFile(sound)
    end
end

-- Messages --------------------------------------------------------------------------------------------------------

local function Handle(message)
    local kind, a, b, c, d, e, f, g = strsplit("\t", message)
    if kind == "HUD" then
        UpdatePanel(tonumber(a) or 1, tonumber(b) or 0, tonumber(c) or 0, tonumber(d) or 0, tonumber(e) or 0,
            tonumber(f) or STATE_FIGHTING)
    elseif kind == "ARRIVE" then
        local ladder, step, paragon = tonumber(c) or 0, tonumber(d) or 0, tonumber(e) or 0
        local text = b or ""
        if ladder == LADDER_GEARING then
            text = text .. " · " .. format(TEXT.step, step + 1)
            if paragon > 0 then
                text = text .. " · " .. format(TEXT.paragon, paragon)
            end
        end
        ShowBanner(format(TEXT.arrived, tonumber(a) or 1), text, SOUND .. "ChallengeStart.ogg")
    elseif kind == "CLEAR" then
        local checkpoint = b == "1"
        ShowBanner(format(TEXT.clearedTitle, tonumber(a) or 1),
            checkpoint and TEXT.checkpointReached or TEXT.portalOpen,
            checkpoint and SOUND .. "NewRecord.ogg" or nil)
    elseif kind == "END" then
        SetPanelShown(false)
        if tonumber(a) == REASON_FALLEN then
            ShowBanner(TEXT.endTitle, format(TEXT.endText, tonumber(b) or 1))
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "PLAYER_ENTERING_WORLD" then
        Send("STATE")
        return
    end
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    Handle(message)
end)

-- The keepers on the world map ------------------------------------------------------------------------------------
-- Eternia's spawns (mod-stat-growth stat_growth_infinite_dungeon.sql), by map, and the maps each shows on: its
-- capital, the zone around it and its continent. Maps are known by their file name (GetMapInfo) and placed with
-- their WorldMapArea.dbc bounds: a map's left and right edges are world Y, its top and bottom world X. Dalaran's own
-- map has no bounds (it is drawn as floors): its keeper shows on Crystalsong Forest and Northrend.

local KEEPERS = {
    { map = 0, x = -8820.38, y = 624.32 },      -- Stormwind
    { map = 0, x = -4798.60, y = -1104.15 },    -- Ironforge
    { map = 1, x = 9940.17, y = 2514.60 },      -- Darnassus
    { map = 530, x = -3919.57, y = -11549.66 }, -- The Exodar
    { map = 1, x = 2072.97, y = -4824.27 },     -- Orgrimmar
    { map = 0, x = 1596.67, y = 231.13 },       -- Undercity
    { map = 1, x = -1257.70, y = 24.30 },       -- Thunder Bluff
    { map = 530, x = 9525.23, y = -7216.82 },   -- Silvermoon City
    { map = 571, x = 5614.28, y = 693.16 },     -- Dalaran
    { map = 530, x = -2013.00, y = 5365.55 },   -- Shattrath City
}

local MAP_BOUNDS = {
    Stormwind = { map = 0, left = 1722.92, right = -14.58, top = -7995.83, bottom = -9154.17, city = true },
    Ironforge = { map = 0, left = -713.59, right = -1504.22, top = -4569.24, bottom = -5096.85, city = true },
    Undercity = { map = 0, left = 873.19, right = -86.18, top = 1877.95, bottom = 1237.84, city = true },
    Darnassis = { map = 1, left = 2938.36, right = 1880.03, top = 10238.32, bottom = 9532.59, city = true },
    Ogrimmar = { map = 1, left = -3680.60, right = -5083.21, top = 2273.88, bottom = 1338.46, city = true },
    ThunderBluff = { map = 1, left = 516.67, right = -527.08, top = -850.00, bottom = -1545.83, city = true },
    TheExodar = { map = 530, left = -11066.37, right = -12123.14, top = -3609.68, bottom = -4314.37, city = true },
    SilvermoonCity = { map = 530, left = -6400.75, right = -7612.21, top = 10153.71, bottom = 9346.94, city = true },
    ShattrathCity = { map = 530, left = 6135.26, right = 4829.01, top = -1473.95, bottom = -2344.79, city = true },
    Elwynn = { map = 0, left = 1535.42, right = -1935.42, top = -7939.58, bottom = -10254.17 },
    DunMorogh = { map = 0, left = 1802.08, right = -3122.92, top = -3877.08, bottom = -7160.42 },
    Tirisfal = { map = 0, left = 3033.33, right = -1485.42, top = 3837.50, bottom = 825.00 },
    Teldrassil = { map = 1, left = 3814.58, right = -1277.08, top = 11831.25, bottom = 8437.50 },
    Durotar = { map = 1, left = -1962.50, right = -7250.00, top = 1808.33, bottom = -1716.67 },
    Mulgore = { map = 1, left = 2047.92, right = -3089.58, top = -272.92, bottom = -3697.92 },
    AzuremystIsle = { map = 530, left = -10500.00, right = -14570.83, top = -2793.75, bottom = -5508.33 },
    EversongWoods = { map = 530, left = -4487.50, right = -9412.50, top = 11041.67, bottom = 7758.33 },
    TerokkarForest = { map = 530, left = 7083.33, right = 1683.33, top = -1000.00, bottom = -4600.00 },
    CrystalsongForest = { map = 571, left = 1443.75, right = -1279.17, top = 6502.08, bottom = 4687.50 },
    Azeroth = { map = 0, left = 18171.97, right = -22569.21, top = 11176.34, bottom = -15973.34 },
    Kalimdor = { map = 1, left = 17066.60, right = -19733.21, top = 12799.90, bottom = -11733.30 },
    Expansion01 = { map = 530, left = 12996.04, right = -4468.04, top = 5821.36, bottom = -5821.36 },
    Northrend = { map = 571, left = 9217.15, right = -8534.25, top = 10593.38, bottom = -1240.89 },
}

-- Where a world position falls on a map, 0-1 from its top left; nil when off it
local function MapPosition(bounds, x, y)
    local px = (bounds.left - y) / (bounds.left - bounds.right)
    local py = (bounds.top - x) / (bounds.top - bounds.bottom)
    if px < 0 or px > 1 or py < 0 or py > 1 then
        return nil
    end
    return px, py
end

local function ShowKeeperTooltip(owner)
    WorldMapTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    WorldMapTooltip:AddLine(TEXT.mapTitle, GOLD[1], GOLD[2], GOLD[3])
    WorldMapTooltip:AddLine(TEXT.mapKeeper, 1, 1, 1)
    WorldMapTooltip:AddLine(TEXT.mapHint, 0.85, 0.78, 0.62)
    WorldMapTooltip:Show()
end

local function CreatePin()
    local pin = CreateFrame("Button", nil, WorldMapButton)
    local ring = pin:CreateTexture(nil, "BACKGROUND")
    ring:SetTexture("Interface\\Buttons\\WHITE8X8")
    ring:SetVertexColor(BORDER[1], BORDER[2], BORDER[3], 1)
    ring:SetPoint("TOPLEFT", -1, 1)
    ring:SetPoint("BOTTOMRIGHT", 1, -1)
    local icon = pin:CreateTexture(nil, "OVERLAY")
    icon:SetAllPoints()
    icon:SetTexture(ICON)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    pin:SetScript("OnEnter", function(self) ShowKeeperTooltip(self) end)
    pin:SetScript("OnLeave", function() WorldMapTooltip:Hide() end)
    return pin
end

local mapPins = {}

local function UpdateMapPins()
    local bounds = MAP_BOUNDS[GetMapInfo() or ""]
    local width, height = WorldMapButton:GetWidth(), WorldMapButton:GetHeight()
    -- The same size on screen however the map is scaled (DragonUI scales the canvas)
    local pinScale = UIParent:GetEffectiveScale() / WorldMapButton:GetEffectiveScale()
    local size = bounds and bounds.city and 20 or 14
    local shown = 0
    for _, keeper in ipairs(KEEPERS) do
        local px, py
        if bounds and bounds.map == keeper.map then
            px, py = MapPosition(bounds, keeper.x, keeper.y)
        end
        if px then
            shown = shown + 1
            local pin = mapPins[shown]
            if not pin then
                pin = CreatePin()
                mapPins[shown] = pin
            end
            pin:SetFrameLevel(WorldMapButton:GetFrameLevel() + 6)
            pin:SetScale(pinScale)
            pin:SetSize(size, size)
            pin:ClearAllPoints()
            pin:SetPoint("CENTER", WorldMapButton, "TOPLEFT", px * width / pinScale, -py * height / pinScale)
            pin:Show()
        end
    end
    for index = shown + 1, #mapPins do
        mapPins[index]:Hide()
    end
end

local mapWatcher = CreateFrame("Frame")
mapWatcher:RegisterEvent("WORLD_MAP_UPDATE")
mapWatcher:SetScript("OnEvent", function()
    if WorldMapFrame:IsShown() then
        UpdateMapPins()
    end
end)
WorldMapFrame:HookScript("OnShow", UpdateMapPins)
-- The map changes size between its full and windowed views
for _, name in ipairs({ "WorldMapFrame_SetFullMapView", "WorldMapFrame_SetQuestMapView", "WorldMap_ToggleSizeUp",
        "WorldMap_ToggleSizeDown" }) do
    if _G[name] then
        hooksecurefunc(name, UpdateMapPins)
    end
end
