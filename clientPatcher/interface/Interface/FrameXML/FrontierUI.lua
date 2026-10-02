-- Le Front du Nord (mod-stat-growth src/frontier/Frontier.cpp): open-world content at level 80. Northrend's zones
-- are ranked in four tiers; this shows the tier on entering a zone (a banner) and puts the zone's content on the
-- world map and the minimap. The server whispers on the "Frontier" addon prefix:
--   ZONE <tier 0-4> <zone id> <loot item level>   entering a tier zone (tier 0: leaving one)
--   PINS <zone id> <kind>:<x>:<y>,...             the zone's content in world coordinates (E a roaming elite, R a rift)
--   RIFT <stage> <wave> <waves> <alive> <total> <seconds left>
--                                                 a rift near the player: stage 1 waiting, 2 waves, 3 guardian, 4
--                                                 closed, 5 collapsed; the tracker hides when it stops hearing of it
--   COLOSSUS <state> <colossus 1-4> <zone id> <x> <y> <seconds> <loot item level>
--                                                 a world boss, to every level-80 player: state 1 coming (seconds
--                                                 until it comes), 2 here (until it leaves), 3 slain, 4 gone unfought
--
-- Built on the Infinite Dungeon's pieces (InfiniteDungeonUI: the banner's look, its helpers and palette).

local UI = InfiniteDungeonUI
if not UI then
    return
end

local PREFIX = "Frontier"
local french = UI.french
local TEXT = french and {
    kicker = "Front du Nord",
    tier = "Palier %s",
    gear = "Équipement de niveau %d",
    content = "Élites rôdeurs",
    eliteTitle = "Élite rôdeur",
    eliteHint = "Un défi pour un joueur seul, qui grandit avec le groupe. Éclats de givre, et une chance "
        .. "d'équipement du palier.",
    riftTitle = "Faille arcanique",
    riftHint = "Approchez-vous pour l'ouvrir : trois vagues, puis son gardien. Elle grandit avec le groupe.",
    riftWaiting = "Approchez-vous pour l'ouvrir",
    riftWave = "Vague %d / %d",
    riftLeft = "%d ennemi(s) restant(s)",
    riftGuardian = "Le gardien de la faille",
    riftClosed = "Faille refermée !",
    riftCollapsed = "La faille s'est effondrée",
    colossusKicker = "Colosse du Front du Nord",
    colossusArrives = "Arrive dans %s",
    colossusChatComing = "%s arrive dans %s",
    colossusChatHere = "%s est arrivé",
    colossusChatReward = "(palier %s, équipement de niveau %d garanti)",
    minutes = "%d min",
    seconds = "%d s",
    colossusLeaves = "Repart dans %s",
    colossusReward = "Palier %s  ·  équipement de niveau %d garanti",
    colossusHint = "Un colosse pour un joueur seul comme pour un groupe : il grandit avec ceux qui le combattent.",
    colossi = { "Gorroth Grandes-Défenses", "Vyskarn", "Zul'Gath l'Avatar déchu", "Mastodonte de saronite" },
} or {
    kicker = "Northrend Frontier",
    tier = "Tier %s",
    gear = "Item level %d gear",
    content = "Roaming elites",
    eliteTitle = "Roaming elite",
    eliteHint = "A challenge for one player that grows with the group. Frost shards, and a chance of the tier's gear.",
    riftTitle = "Arcane rift",
    riftHint = "Come near to open it: three waves, then its guardian. It grows with the group.",
    riftWaiting = "Come near to open it",
    riftWave = "Wave %d / %d",
    riftLeft = "%d enemy(ies) left",
    riftGuardian = "The rift's guardian",
    riftClosed = "Rift closed!",
    riftCollapsed = "The rift collapsed",
    colossusKicker = "Northrend Frontier Colossus",
    colossusArrives = "Arrives in %s",
    colossusChatComing = "%s arrives in %s",
    colossusChatHere = "%s has arrived",
    colossusChatReward = "(tier %s, item level %d gear guaranteed)",
    minutes = "%d min",
    seconds = "%d s",
    colossusLeaves = "Leaves in %s",
    colossusReward = "Tier %s  ·  item level %d gear guaranteed",
    colossusHint = "A colossus for one player as for a group: it grows with those who fight it.",
    colossi = { "Gorroth Greattusk", "Vyskarn", "Zul'Gath the Fallen Avatar", "The Saronite Juggernaut" },
}
local NUMERALS = { "I", "II", "III", "IV" }

-- The tier zones: their name, their map (GetMapInfo), WorldMapArea.dbc bounds (left / right are world y, top / bottom
-- world x) and the icon the banner falls back on without its tier crest (Interface\Frontier\Crest<n>)
local ZONES = {
    [3537] = { name = french and "Toundra Boréenne" or "Borean Tundra",
               map = "BoreanTundra", left = 8570.83, right = 2806.25, top = 4897.92, bottom = 1054.17,
               icon = "Interface\\Icons\\Achievement_Zone_BoreanTundra_01" },
    [495] = { name = french and "Fjord Hurlant" or "Howling Fjord",
              map = "HowlingFjord", left = -1397.92, right = -7443.75, top = 3116.67, bottom = -914.58,
              icon = "Interface\\Icons\\Achievement_Zone_HowlingFjord_01" },
    [65] = { name = french and "Désolation des dragons" or "Dragonblight",
             map = "Dragonblight", left = 3627.08, right = -1981.25, top = 5575.0, bottom = 1835.42,
             icon = "Interface\\Icons\\Achievement_Zone_Dragonblight_01" },
    [394] = { name = french and "Les Grisonnes" or "Grizzly Hills",
              map = "GrizzlyHills", left = -1110.42, right = -6360.42, top = 5516.67, bottom = 2016.67,
              icon = "Interface\\Icons\\Achievement_Zone_GrizzlyHills_01" },
    [66] = { name = french and "Zul'Drak" or "Zul'Drak",
             map = "ZulDrak", left = -600.0, right = -5593.75, top = 7668.75, bottom = 4339.58,
             icon = "Interface\\Icons\\Achievement_Zone_ZulDrak_01" },
    [3711] = { name = french and "Bassin de Sholazar" or "Sholazar Basin",
               map = "SholazarBasin", left = 6929.17, right = 2572.92, top = 7287.5, bottom = 4383.33,
               icon = "Interface\\Icons\\Achievement_Zone_Sholazar_01" },
    [67] = { name = french and "Les pics Foudroyés" or "The Storm Peaks",
             map = "TheStormPeaks", left = 1841.67, right = -5270.83, top = 10197.9, bottom = 5456.25,
             icon = "Interface\\Icons\\Achievement_Zone_StormPeaks_01" },
    [210] = { name = french and "La Couronne de glace" or "Icecrown",
              map = "IcecrownGlacier", left = 5443.75, right = -827.08, top = 9427.08, bottom = 5245.83,
              icon = "Interface\\Icons\\Achievement_Zone_IceCrown_01" },
}
local ZONE_BY_MAP = {}
for id, zone in pairs(ZONES) do
    ZONE_BY_MAP[zone.map] = id
end

local ELITE_ICON = "Interface\\TargetingFrame\\UI-RaidTargetingIcon_8"
-- The rift's own icon (localTools/interface/buildFrontierArt.py), a stock portal should it be missing
local RIFT_ICON = "Interface\\Frontier\\RiftIcon"
local RIFT_ICON_STOCK = "Interface\\Icons\\Spell_Arcane_PortalDalaran"

-- The banner ---------------------------------------------------------------------------------------------------------

local banner
local BANNER_IN, BANNER_OUT, BANNER_HOLD = 0.5, 0.7, 4
local DIVIDER_WIDTH = 300

local function CreateBanner()
    banner = CreateFrame("Frame", "FrontierBanner", UIParent)
    banner:SetSize(560, 190)
    banner:SetPoint("TOP", UIParent, "TOP", 0, -96)
    banner:SetFrameStrata("MEDIUM")
    banner:Hide()

    -- A dark band fading out to both sides, lined in gold (the Infinite Dungeon's banner)
    local band = CreateFrame("Frame", nil, banner)
    band:SetPoint("TOPLEFT", banner, "TOPLEFT", 0, -60)
    band:SetPoint("TOPRIGHT", banner, "TOPRIGHT", 0, -60)
    band:SetHeight(112)
    local left = band:CreateTexture(nil, "BACKGROUND")
    left:SetTexture("Interface\\Buttons\\WHITE8X8")
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMRIGHT", band, "BOTTOM")
    left:SetGradientAlpha("HORIZONTAL", 0.03, 0.04, 0.06, 0, 0.03, 0.04, 0.06, 0.85)
    local right = band:CreateTexture(nil, "BACKGROUND")
    right:SetTexture("Interface\\Buttons\\WHITE8X8")
    right:SetPoint("TOPLEFT", band, "TOP")
    right:SetPoint("BOTTOMRIGHT")
    right:SetGradientAlpha("HORIZONTAL", 0.03, 0.04, 0.06, 0.85, 0.03, 0.04, 0.06, 0)
    for _, edge in ipairs({ "TOP", "BOTTOM" }) do
        local line = UI.Divider(band)
        line:SetPoint(edge .. "LEFT", band, edge .. "LEFT", 40, edge == "TOP" and 4 or -4)
        line:SetPoint(edge .. "RIGHT", band, edge .. "RIGHT", -40, edge == "TOP" and 4 or -4)
    end

    -- The tier's crest over the band's top edge, on a slow star and a cold glow
    local glow = banner:CreateTexture(nil, "BACKGROUND")
    UI.SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetSize(200, 200)
    glow:SetBlendMode("ADD")
    glow:SetVertexColor(0.6, 0.8, 1)
    glow:SetPoint("CENTER", banner, "TOP", 0, -56)
    banner.glow = glow
    local star = banner:CreateTexture(nil, "BORDER")
    UI.SetAtlas(star, "ChallengeMode-SpikeyStar")
    star:SetSize(140, 140)
    star:SetBlendMode("ADD")
    star:SetAlpha(0.35)
    star:SetPoint("CENTER", glow, "CENTER")
    banner.star = star
    local crest = banner:CreateTexture(nil, "ARTWORK")
    crest:SetSize(64, 64)
    crest:SetPoint("CENTER", glow, "CENTER")
    banner.crest = crest

    local kicker = UI.Label(band, "GameFontNormal", UI.GOLD, "CENTER")
    kicker:SetPoint("TOP", band, "TOP", 0, -16)
    banner.kicker = kicker
    local title = UI.Heading(band, 32)
    title:SetJustifyH("CENTER")
    title:SetPoint("TOP", kicker, "BOTTOM", 0, -3)
    banner.title = title
    local divider = UI.Divider(band, "OVERLAY")
    divider:SetWidth(DIVIDER_WIDTH)
    divider:SetPoint("TOP", title, "BOTTOM", 0, -1)
    banner.divider = divider
    local subtitle = UI.Label(band, "GameFontHighlight", UI.SOFT, "CENTER")
    subtitle:SetPoint("TOP", divider, "BOTTOM", 0, -1)
    banner.subtitle = subtitle
    local detail = UI.Label(band, "GameFontHighlightSmall", UI.AMBER, "CENTER")
    detail:SetPoint("TOP", subtitle, "BOTTOM", 0, -4)
    banner.detail = detail

    banner:SetScript("OnUpdate", function(self)
        local now = GetTime()
        local age = now - self.shownAt
        local alpha, ease
        if age < BANNER_IN then
            ease = UI.OutCubic(age / BANNER_IN)
            alpha = ease
        elseif now < self.hideAt then
            ease, alpha = 1, 1
        else
            local out = (now - self.hideAt) / BANNER_OUT
            if out >= 1 then
                self:Hide()
                return
            end
            ease, alpha = 1, 1 - UI.OutCubic(out)
        end
        self:SetAlpha(alpha)
        local drift = now > self.hideAt and 12 * (now - self.hideAt) / BANNER_OUT or 0
        self:SetPoint("TOP", UIParent, "TOP", 0, -96 + drift)
        kicker:SetPoint("TOP", band, "TOP", 0, -16 + 14 * (1 - ease))
        divider:SetWidth(max(1, DIVIDER_WIDTH * ease))
        local size = 64 * (1.4 - 0.4 * ease)
        crest:SetSize(size, size)
        UI.RotateAtlas(star, "ChallengeMode-SpikeyStar", now * 0.3)
        glow:SetAlpha(0.6 + 0.2 * math.sin(now * 2.2))
    end)
end

local function ShowBanner(tier, zoneId, itemLevel)
    if not banner then
        CreateBanner()
    end
    local zone = ZONES[zoneId]
    if not banner.crest:SetTexture("Interface\\Frontier\\Crest" .. tier) and zone then
        banner.crest:SetTexture(zone.icon)
    end
    banner.kicker:SetText(TEXT.kicker)
    banner.title:SetText(format(TEXT.tier, NUMERALS[tier] or tier))
    banner.subtitle:SetText(GetZoneText())
    banner.detail:SetText(format(TEXT.gear, itemLevel) .. "  ·  " .. TEXT.content)
    banner.shownAt = GetTime()
    banner.hideAt = banner.shownAt + BANNER_IN + BANNER_HOLD
    banner:SetAlpha(0)
    banner:Show()
end

-- What the server last said ---------------------------------------------------------------------------------------

local currentZone = 0
local pins = {}         -- zone id -> { { kind, x, y }, ... }
local colossi = {}      -- coming / here -> { index, zone, x, y, at (when it comes or leaves), loot, here }

-- The Colosses' stock icons, for their map pins (made round); their chat line shows their painted heads
-- (Interface\Frontier\ColossusIcon<n>, localTools/interface/buildFrontierArt.py)
local COLOSSUS_ICONS = {
    "Interface\\Icons\\Ability_Mount_Mammoth_White",
    "Interface\\Icons\\Achievement_Boss_Sapphiron_01",
    "Interface\\Icons\\Achievement_Boss_GalDarah",
    "Interface\\Icons\\Achievement_Boss_Patchwerk",
}

-- A zone's pins: what the server pinned there, and a Colosse coming or here
local function ZonePins(zoneId)
    local list = {}
    for _, entry in ipairs(pins[zoneId] or {}) do
        tinsert(list, entry)
    end
    for _, colossus in pairs(colossi) do
        if colossus.zone == zoneId then
            tinsert(list, { kind = "C", x = colossus.x, y = colossus.y, colossus = colossus })
        end
    end
    return list
end

-- The world map's size of a pin, and the minimap's
local PIN_SIZES = { E = { 18, 14 }, R = { 24, 18 }, C = { 34, 26 } }

local function PinSize(kind, onMinimap)
    local sizes = PIN_SIZES[kind] or PIN_SIZES.E
    return sizes[onMinimap and 2 or 1]
end

-- The map pins -----------------------------------------------------------------------------------------------------

local function ShowPinTooltip(tooltip, owner, entry)
    tooltip:SetOwner(owner, "ANCHOR_RIGHT")
    local colossus = entry and entry.colossus
    if colossus then
        tooltip:AddLine(TEXT.colossi[colossus.index] or "", UI.HEADING[1], UI.HEADING[2], UI.HEADING[3])
        tooltip:AddLine(TEXT.colossusKicker, UI.GOLD[1], UI.GOLD[2], UI.GOLD[3])
        local when = colossus.here and format(TEXT.colossusLeaves, UI.FormatClock(colossus.at - GetTime()))
            or format(TEXT.colossusArrives, UI.FormatClock(colossus.at - GetTime()))
        tooltip:AddLine(when, UI.SOFT[1], UI.SOFT[2], UI.SOFT[3])
        tooltip:AddLine(format(TEXT.colossusReward, NUMERALS[colossus.index] or "", colossus.loot),
            UI.AMBER[1], UI.AMBER[2], UI.AMBER[3])
        tooltip:AddLine(TEXT.colossusHint, UI.SOFT[1], UI.SOFT[2], UI.SOFT[3], true)
    else
        local rift = entry and entry.kind == "R"
        tooltip:AddLine(rift and TEXT.riftTitle or TEXT.eliteTitle, UI.HEADING[1], UI.HEADING[2], UI.HEADING[3])
        tooltip:AddLine(rift and TEXT.riftHint or TEXT.eliteHint, UI.SOFT[1], UI.SOFT[2], UI.SOFT[3], true)
    end
    tooltip:Show()
end

local function CreatePin(parent, tooltipName)
    local pin = CreateFrame("Button", nil, parent)
    local icon = pin:CreateTexture(nil, "ARTWORK")
    pin.icon = icon
    -- A Colosse's ring: the tracking button's gold border (its ring fills the top left 54 / 64 of it)
    local border = pin:CreateTexture(nil, "OVERLAY")
    border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
    border:SetPoint("TOPLEFT")
    border:Hide()
    pin.border = border
    pin:SetScript("OnEnter", function(self) ShowPinTooltip(_G[tooltipName], self, self.entry) end)
    pin:SetScript("OnLeave", function() _G[tooltipName]:Hide() end)
    return pin
end

-- A pin's look for what it marks, at its size
local function SetPinEntry(pin, entry, size)
    pin.entry = entry
    pin:SetSize(size, size)
    local look = entry.colossus and "C" .. entry.colossus.index or entry.kind
    if pin.look == look and pin.lookSize == size then
        return
    end
    pin.look, pin.lookSize = look, size
    pin.icon:ClearAllPoints()
    if entry.colossus then
        -- Its icon made round, in the ring (the tracking button's proportions: a 20 icon in a 32 button)
        SetPortraitToTexture(pin.icon, COLOSSUS_ICONS[entry.colossus.index] or COLOSSUS_ICONS[1])
        pin.icon:SetTexCoord(0, 1, 0, 1)
        pin.icon:SetSize(size * 20 / 32, size * 20 / 32)
        pin.icon:SetPoint("TOPLEFT", pin, "TOPLEFT", size * 6 / 32, -size * 6 / 32)
        pin.border:SetSize(size * 54 / 32, size * 54 / 32)
        pin.border:Show()
        return
    end
    pin.border:Hide()
    pin.icon:SetAllPoints()
    if entry.kind == "R" then
        if pin.icon:SetTexture(RIFT_ICON) then
            pin.icon:SetTexCoord(0, 1, 0, 1)
        else
            pin.icon:SetTexture(RIFT_ICON_STOCK)
            pin.icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        end
    else
        pin.icon:SetTexture(ELITE_ICON)
        pin.icon:SetTexCoord(0, 1, 0, 1)
    end
end

-- Where a world position falls on a zone's map, 0-1 from its top left; nil when off it
local function MapPosition(zone, x, y)
    local px = (zone.left - y) / (zone.left - zone.right)
    local py = (zone.top - x) / (zone.top - zone.bottom)
    if px < 0 or px > 1 or py < 0 or py > 1 then
        return nil
    end
    return px, py
end

local mapPins = {}

local function UpdateMapPins()
    local zoneId = ZONE_BY_MAP[GetMapInfo() or ""]
    local zone = zoneId and ZONES[zoneId]
    local width, height = WorldMapButton:GetWidth(), WorldMapButton:GetHeight()
    local pinScale = UIParent:GetEffectiveScale() / WorldMapButton:GetEffectiveScale()
    local shown = 0
    for _, entry in ipairs(zone and ZonePins(zoneId) or {}) do
        local px, py = MapPosition(zone, entry.x, entry.y)
        if px then
            shown = shown + 1
            local pin = mapPins[shown]
            if not pin then
                pin = CreatePin(WorldMapButton, "WorldMapTooltip")
                mapPins[shown] = pin
            end
            SetPinEntry(pin, entry, PinSize(entry.kind))
            pin:SetFrameLevel(WorldMapButton:GetFrameLevel() + (entry.colossus and 8 or 6))
            pin:SetScale(pinScale)
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

-- The minimap: the pins within its reach, turned with it when it turns ------------------------------------------

local MINIMAP_YARDS = { [0] = 466.6667, 400, 333.3333, 266.6667, 200, 133.3333 }
local minimapPins = {}

local function UpdateMinimapPins()
    local zone = ZONES[currentZone]
    local shown = 0
    if zone and not WorldMapFrame:IsShown() then
        local px, py = GetPlayerMapPosition("player")
        if (px ~= 0 or py ~= 0) and ZONE_BY_MAP[GetMapInfo() or ""] == currentZone then
            local playerX = zone.top - py * (zone.top - zone.bottom)
            local playerY = zone.left - px * (zone.left - zone.right)
            local perYard = Minimap:GetWidth() / (MINIMAP_YARDS[Minimap:GetZoom()] or 466.6667)
            local radius = Minimap:GetWidth() / 2 - 8
            local rotate = GetCVar("rotateMinimap") == "1"
            local facing = rotate and GetPlayerFacing() or 0
            for _, entry in ipairs(ZonePins(currentZone)) do
                local east, south = playerY - entry.y, playerX - entry.x
                if rotate then
                    local sine, cosine = math.sin(facing), math.cos(facing)
                    east, south = east * cosine - south * sine, east * sine + south * cosine
                end
                local x, y = east * perYard, -south * perYard
                if x * x + y * y <= radius * radius then
                    shown = shown + 1
                    local pin = minimapPins[shown]
                    if not pin then
                        pin = CreatePin(Minimap, "GameTooltip")
                        minimapPins[shown] = pin
                    end
                    SetPinEntry(pin, entry, PinSize(entry.kind, true))
                    pin:SetFrameLevel(Minimap:GetFrameLevel() + (entry.colossus and 7 or 5))
                    pin:ClearAllPoints()
                    pin:SetPoint("CENTER", Minimap, "CENTER", x, y)
                    pin:Show()
                end
            end
        end
    end
    for index = shown + 1, #minimapPins do
        minimapPins[index]:Hide()
    end
end

local minimapClock = 0
local minimapWatcher = CreateFrame("Frame")
minimapWatcher:SetScript("OnUpdate", function(_, elapsed)
    minimapClock = minimapClock + elapsed
    if minimapClock >= 0.2 then
        minimapClock = 0
        UpdateMinimapPins()
    end
end)

-- The rift tracker: the Infinite Dungeon's card, under the quest tracker's corner -----------------------------------

local tracker
local TRACKER_TIMEOUT = 3
local STAGE_WAITING, STAGE_WAVES, STAGE_GUARDIAN, STAGE_CLOSED = 1, 2, 3, 4

local function CreateTracker()
    tracker = CreateFrame("Frame", "FrontierRiftTracker", UIParent)
    tracker:SetSize(250, 84)
    tracker:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", -90, -230)
    UI.Card(tracker, 0.88)
    tracker:Hide()

    local icon = UI.FramedIcon(tracker, 34)
    icon:SetPoint("TOPLEFT", tracker, "TOPLEFT", 10, -10)
    if not icon.icon:SetTexture(RIFT_ICON) then
        icon.icon:SetTexture(RIFT_ICON_STOCK)
    end
    local title = UI.Label(tracker, "GameFontNormal", UI.GOLD)
    title:SetPoint("TOPLEFT", icon, "TOPRIGHT", 8, -2)
    title:SetText(TEXT.riftTitle)
    local clock = UI.Label(tracker, "GameFontHighlightSmall", UI.SOFT, "RIGHT")
    clock:SetPoint("TOPRIGHT", tracker, "TOPRIGHT", -12, -12)
    local stage = UI.Label(tracker, "GameFontHighlightSmall", UI.SOFT)
    stage:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -3)
    local bar = UI.Bar(tracker, 226, 4)
    bar:SetPoint("BOTTOM", tracker, "BOTTOM", 0, 12)
    tracker.clock, tracker.stage, tracker.bar = clock, stage, bar

    tracker:SetScript("OnUpdate", function(self)
        if GetTime() - self.heardAt > TRACKER_TIMEOUT then
            self:Hide()
        end
    end)
end

-- The stage, the enemies left and the time left; the bar is the three waves and the guardian
local function ShowRift(stage, wave, waves, alive, total, left)
    if not tracker then
        CreateTracker()
    end
    tracker.heardAt = GetTime()
    local text, progress = "", 0
    if stage == STAGE_WAITING then
        text = TEXT.riftWaiting
    elseif stage == STAGE_WAVES then
        text = format(TEXT.riftWave, wave, waves) .. "  ·  " .. format(TEXT.riftLeft, alive)
        progress = (wave - 1 + (total > 0 and (total - alive) / total or 0)) / (waves + 1)
    elseif stage == STAGE_GUARDIAN then
        text = TEXT.riftGuardian
        progress = waves / (waves + 1)
    elseif stage == STAGE_CLOSED then
        text = UI.Colored(TEXT.riftClosed, UI.GOLD)
        progress = 1
    else
        text = UI.Colored(TEXT.riftCollapsed, UI.MUTED)
    end
    tracker.stage:SetText(text)
    tracker.clock:SetText(left > 0 and UI.FormatClock(left) or "")
    tracker.bar:SetValue(progress, tracker:IsShown())
    tracker:Show()
end

-- The Colosse's news: one line in the chat, its painted head in front ----------------------------------------------

-- 600 -> "10 min", 45 -> "45 s"
local function Duration(seconds)
    seconds = max(0, floor(seconds + 0.5))
    if seconds >= 60 then
        return format(TEXT.minutes, floor(seconds / 60 + 0.5))
    end
    return format(TEXT.seconds, seconds)
end

local function AnnounceColossus(colossus)
    local zone = ZONES[colossus.zone]
    local name = UI.Colored(TEXT.colossi[colossus.index] or "", UI.HEADING)
    local line = (colossus.here and format(TEXT.colossusChatHere, name)
        or format(TEXT.colossusChatComing, name, Duration(colossus.at - GetTime())))
        .. "  ·  " .. (zone and zone.name or "")
    local reward = format(TEXT.colossusChatReward, NUMERALS[colossus.index] or "", colossus.loot)
    DEFAULT_CHAT_FRAME:AddMessage(format("|TInterface\\Frontier\\ColossusIcon%d:16:16|t %s %s %s",
        colossus.index, UI.Colored(TEXT.kicker .. (french and " :" or ":"), UI.GOLD), UI.Colored(line, UI.SOFT),
        UI.Colored(reward, UI.MUTED)))
end

local COLOSSUS_COMING, COLOSSUS_HERE = 1, 2

-- A Colosse heard of, come, slain or gone: a chat line for the first two, the pins for all
local function ReceiveColossus(state, index, zoneId, x, y, seconds, loot)
    local colossus = { index = index, zone = zoneId, x = x, y = y, at = GetTime() + seconds, loot = loot,
                       here = state == COLOSSUS_HERE }
    if state == COLOSSUS_COMING then
        colossi.coming = colossus
        AnnounceColossus(colossus)
    elseif state == COLOSSUS_HERE then
        colossi.here = colossus
        if colossi.coming and colossi.coming.index == index then
            colossi.coming = nil
        end
        AnnounceColossus(colossus)
    else
        colossi.here = nil
    end
    if WorldMapFrame:IsShown() then
        UpdateMapPins()
    end
end

-- The server's messages ------------------------------------------------------------------------------------------

local function Receive(message)
    local kind, a, b, c = strsplit("\t", message)
    if kind == "ZONE" then
        local tier, zoneId, itemLevel = tonumber(a) or 0, tonumber(b) or 0, tonumber(c) or 0
        if tier > 0 and zoneId ~= currentZone then
            ShowBanner(tier, zoneId, itemLevel)
        end
        currentZone = tier > 0 and zoneId or 0
    elseif kind == "PINS" then
        local zoneId = tonumber(a) or 0
        local list = {}
        for entry in string.gmatch(b or "", "[^,]+") do
            local pinKind, x, y = strsplit(":", entry)
            tinsert(list, { kind = pinKind, x = tonumber(x) or 0, y = tonumber(y) or 0 })
        end
        pins[zoneId] = list
        if WorldMapFrame:IsShown() then
            UpdateMapPins()
        end
    elseif kind == "RIFT" then
        local _, stage, wave, waves, alive, total, left = strsplit("\t", message)
        ShowRift(tonumber(stage) or 0, tonumber(wave) or 0, tonumber(waves) or 3, tonumber(alive) or 0,
            tonumber(total) or 0, tonumber(left) or 0)
    elseif kind == "COLOSSUS" then
        local _, state, index, zoneId, x, y, seconds, loot = strsplit("\t", message)
        ReceiveColossus(tonumber(state) or 0, tonumber(index) or 1, tonumber(zoneId) or 0, tonumber(x) or 0,
            tonumber(y) or 0, tonumber(seconds) or 0, tonumber(loot) or 0)
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        Receive(message)
    end
end)

-- For testing by hand: FrontierUI_Receive("ZONE\t2\t65\t213")
FrontierUI_Receive = Receive
