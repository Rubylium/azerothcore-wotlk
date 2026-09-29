-- Cinematics: scripted camera flights with a title card, the way a raid introduces its boss. The camera itself is
-- flown by the client extension (awesome_wotlk CameraPath.cpp: CameraPath_Play / CameraPath_Stop /
-- CameraPath_IsPlaying, event CAMERA_PATH_FINISHED); this file adds what goes around it: black bars at the top and
-- bottom, the interface hidden, a title and subtitle fading in over the shot, Escape to skip.
-- Documentation: .agents/docs/systems/cinematics.md.
--
--   Cinematics.Register(name, definition)
--     definition.keys       the camera's path: { { t = seconds, x, y, z = camera, lx, ly, lz = the point it looks
--                           at, fov = degrees (optional) }, ... }, world coordinates of the map it plays on. The
--                           camera glides along it like a dolly (a gentle start and stop, a steady pace between);
--                           the last key's t is the flight's length.
--     definition.keyTiming  true: each key reached at its own t instead (a key's ease = true then eases its segment)
--     definition.blendIn    seconds from the player's camera to the path (0.6 if nil)
--     definition.blendOut   seconds from the path back to the player's camera (0.8 if nil)
--     definition.title      big gold words (Morpheus), and definition.subtitle under them (optional)
--     definition.titleAt    seconds from the start the title fades in at (and definition.titleFor: how long it
--                           stays, until the end if nil)
--     definition.letterbox  false to leave the screen without black bars
--     definition.keepUI     true to leave the interface up
--     definition.unskippable true: Escape does not end it
--     definition.onFinish   called once it is over, skipped or not
--   Cinematics.Play(name)   plays it; false when the client extension has no camera flights (an old one)
--   Cinematics.Stop()       ends it now (the camera blends back)
--   Cinematics.IsPlaying()
--   /cinematic <name>       plays one, to try it out

Cinematics = Cinematics or {}

local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local FRIZ = "Fonts\\FRIZQT__.TTF"
local BAR_SHARE = 0.11          -- each black bar, of the screen's height
local BARS_IN, BARS_OUT = 0.6, 0.6
local TITLE_IN, TITLE_OUT = 0.8, 0.9
local TITLE_SCALE = 2.8

local definitions = {}
local current               -- the definition playing
local started               -- GetTime() at its start
local uiWasShown
local titleShown

-- Over the whole screen, without a parent so it stays up while the interface is hidden. Laid over UIParent (hidden
-- or not, it keeps its place): WorldFrame measures in other units (38400 high), bars and title went off screen.
local frame = CreateFrame("Frame", "CinematicsFrame", nil)
frame:SetFrameStrata("FULLSCREEN_DIALOG")
frame:SetAllPoints(UIParent)
frame:Hide()

local top = frame:CreateTexture(nil, "BACKGROUND")
top:SetTexture(0, 0, 0, 1)
top:SetPoint("TOPLEFT")
top:SetPoint("TOPRIGHT")
local bottom = frame:CreateTexture(nil, "BACKGROUND")
bottom:SetTexture(0, 0, 0, 1)
bottom:SetPoint("BOTTOMLEFT")
bottom:SetPoint("BOTTOMRIGHT")

-- The title card, drawn in a scaled frame: the client caps a font's size (asked 54, it drew about 24 pixels high)
local card = CreateFrame("Frame", nil, frame)
card:SetScale(TITLE_SCALE)
card:SetSize(1, 1)
local title = card:CreateFontString(nil, "OVERLAY")
title:SetFont(MORPHEUS, 32)
title:SetTextColor(1, 0.86, 0.5)
title:SetShadowColor(0, 0, 0, 1)
title:SetShadowOffset(2, -2)
title:SetPoint("BOTTOM", card, "BOTTOM", 0, 0)
local subtitle = card:CreateFontString(nil, "OVERLAY")
subtitle:SetFont(FRIZ, 11)
subtitle:SetTextColor(1, 0.82, 0.3)
subtitle:SetShadowColor(0, 0, 0, 1)
subtitle:SetShadowOffset(1, -1)
subtitle:SetPoint("TOP", title, "BOTTOM", 0, -3)

-- Its regions, for anyone checking how it draws
Cinematics.frame, Cinematics.top, Cinematics.bottom, Cinematics.title, Cinematics.subtitle =
    frame, top, bottom, title, subtitle

local function Smooth(p)
    p = max(0, min(1, p))
    return p * p * (3 - 2 * p)
end

local function BarHeight(share)
    return max(1, frame:GetHeight() * BAR_SHARE * share)
end

-- The title sits in the lower third, above the bottom bar
local function PlaceTitle()
    card:ClearAllPoints()
    card:SetPoint("BOTTOM", frame, "BOTTOM", 0, frame:GetHeight() * (BAR_SHARE + 0.08) / TITLE_SCALE)
end

local function EndEffects()
    if not current then
        return
    end
    local finished = current
    current = nil
    frame:SetScript("OnUpdate", function(self, elapsed)
        self.out = (self.out or 0) + elapsed
        local p = Smooth(self.out / BARS_OUT)
        top:SetHeight(BarHeight(1 - p))
        bottom:SetHeight(BarHeight(1 - p))
        title:SetAlpha(min(title:GetAlpha(), 1 - p))
        subtitle:SetAlpha(min(subtitle:GetAlpha(), 1 - p))
        if p >= 1 then
            self.out = nil
            self:SetScript("OnUpdate", nil)
            self:Hide()
        end
    end)
    if uiWasShown then
        UIParent:Show()
    end
    uiWasShown = nil
    if finished.onFinish then
        finished.onFinish()
    end
end

local function OnUpdate(self, elapsed)
    if not current then
        return
    end
    local t = GetTime() - started
    local bars = current.letterbox == false and 0 or Smooth(t / BARS_IN)
    top:SetHeight(BarHeight(bars))
    bottom:SetHeight(BarHeight(bars))

    local at = current.titleAt or 0
    local lasts = current.titleFor or math.huge
    local alpha = 0
    if current.title and t >= at then
        alpha = Smooth((t - at) / TITLE_IN)
        if t > at + lasts then
            alpha = 1 - Smooth((t - at - lasts) / TITLE_OUT)
        end
    end
    title:SetAlpha(alpha)
    subtitle:SetAlpha(alpha)
end

function Cinematics.Register(name, definition)
    definitions[name] = definition
end

function Cinematics.IsPlaying()
    return current ~= nil
end

function Cinematics.Stop()
    if CameraPath_Stop and current then
        CameraPath_Stop()
    end
end

function Cinematics.Play(name)
    local definition = definitions[name]
    if not definition or not CameraPath_Play or current then
        return false
    end
    if not CameraPath_Play(definition.keys, { blendIn = definition.blendIn, blendOut = definition.blendOut,
        keyTiming = definition.keyTiming }) then
        return false
    end

    current = definition
    started = GetTime()
    title:SetText(definition.title or "")
    subtitle:SetText(definition.subtitle or "")
    title:SetAlpha(0)
    subtitle:SetAlpha(0)
    top:SetHeight(1)
    bottom:SetHeight(1)
    PlaceTitle()
    if not definition.keepUI and UIParent:IsShown() then
        uiWasShown = true
        UIParent:Hide()
    end
    frame:EnableKeyboard(not definition.unskippable)
    frame:SetScript("OnUpdate", OnUpdate)
    frame:Show()
    return true
end

-- Escape skips; every other key is held while it plays (nothing moves the character under the camera)
frame:SetScript("OnKeyDown", function(_, key)
    if key == "ESCAPE" and current and not current.unskippable then
        Cinematics.Stop()
    end
end)

frame:RegisterEvent("CAMERA_PATH_FINISHED")
frame:SetScript("OnEvent", function(self, event)
    if event == "CAMERA_PATH_FINISHED" then
        self:EnableKeyboard(false)
        EndEffects()
    end
end)

SLASH_CINEMATIC1 = "/cinematic"
SlashCmdList["CINEMATIC"] = function(name)
    name = strtrim(name or "")
    if not Cinematics.Play(name) then
        DEFAULT_CHAT_FRAME:AddMessage("Cinematic: nothing to play as '" .. name .. "'.", 1, 0.5, 0.3)
    end
end

-- The cinematics ---------------------------------------------------------------------------------------------------

-- L'Infini (mod-stat-growth InfiniteGod.cpp), the Défi board's god: as the group arrives in the Celestial
-- Planetarium (ChallengeBoard.lua, its arrival event), before the pull's countdown. The god stands in the middle of
-- the platform (1632.7, -302.8, 417.3), facing the way in, where the group lands (1632.3, -280.5). The camera stays
-- inside the dome: an indoor room is drawn only with the camera inside it (outside, the screen went black). The god
-- is about 13 yards tall: its face is near z 430. One slow pan, not a cut of shots: an arc across its front (115° to
-- 65° around it, the way in at 90°), easing in from 28 to 22 yards and rising a little, its upper body in the middle.
local french = GetLocale() == "frFR"
Cinematics.Register("LInfini", {
    keys = {
        { t = 0, x = 1620.9, y = -277.4, z = 422.0, lx = 1632.7, ly = -302.8, lz = 427.0 },
        { t = 4.5, x = 1632.7, y = -277.8, z = 424.5, lx = 1632.7, ly = -302.8, lz = 428.5 },
        { t = 9.0, x = 1642.0, y = -282.9, z = 427.0, lx = 1632.7, ly = -302.8, lz = 430.0 },
    },
    blendIn = 1.2,
    blendOut = 1.4,
    title = "L'Infini",
    subtitle = french and "Gardien du Planétarium céleste" or "Keeper of the Celestial Planetarium",
    titleAt = 5.0,
})
