-- Le Refus' screen: while Valérian (mod-stat-growth ZealotBoss.cpp, phase 3) winds time back, a clock laid over the
-- game, its hands spinning backwards faster and faster, the edges darkening to violet, then a white-gold burst as the
-- music comes back. Started by the server over EvolutionsAudio's channel (command "R", EvolutionsAudio.lua) as the
-- music cuts; on the track's own clock: the rewind in it, the music back at 6.5 s. Exactly the preview the user
-- validated (2026-10-13, version A: the clock at 72%). Pictures: localTools/valerian/buildRefusOverlay.py.

local ART = "Interface\\Valerian\\"
local LENGTH = 7.2
-- The clockwork's ticks in the rewind (found in its sound): the clock pulses on each
local TICKS = { 0.29, 0.91, 1.86, 2.27, 3.34, 3.59, 3.79, 3.97, 4.36, 4.56, 4.96, 5.16, 5.36, 5.56 }
local DIAL_ALPHA = 0.72
local HANDS_ALPHA = 0.9
local DIAL_SHARE = 0.62         -- of the screen's width

local function clamp(x, low, high)
    return math.min(high or 1, math.max(low or 0, x))
end

local function ramp(t, from, to)
    return clamp((t - from) / (to - from))
end

local function ease(x)
    return x * x * (3 - 2 * x)
end

-- The hands' angle (degrees, backwards): still, then from 60 to 900 degrees a second over the rewind
local function HandsAngle(t)
    local s = clamp(t - 1.5, 0, 4.5)
    return -(60 * s + 840 * s * s * s / (3 * 4.5 * 4.5))
end

local function TickPulse(t)
    if t >= 6 then
        return 0
    end
    local pulse = 0
    for _, tick in ipairs(TICKS) do
        if t >= tick then
            pulse = math.max(pulse, math.exp(-(t - tick) / 0.12))
        end
    end
    return pulse
end

-- A square texture turned by angle (radians) about its middle
local function Rotate(texture, angle)
    local cosine, sine = math.cos(angle), math.sin(angle)
    local function corner(x, y)
        return 0.5 + (x * cosine - y * sine) * 0.5, 0.5 + (x * sine + y * cosine) * 0.5
    end
    local ulx, uly = corner(-1, -1)
    local llx, lly = corner(-1, 1)
    local urx, ury = corner(1, -1)
    local lrx, lry = corner(1, 1)
    texture:SetTexCoord(ulx, uly, llx, lly, urx, ury, lrx, lry)
end

local frame = CreateFrame("Frame", "RefusOverlayFrame", UIParent)
frame:SetFrameStrata("FULLSCREEN")
frame:SetAllPoints(UIParent)
frame:EnableMouse(false)
frame:Hide()

-- Stacked by draw layer (a sublevel is ignored by this client)
local tint = frame:CreateTexture(nil, "BACKGROUND")
tint:SetAllPoints(frame)
tint:SetTexture(42 / 255, 22 / 255, 80 / 255)

local vignette = frame:CreateTexture(nil, "BORDER")
vignette:SetAllPoints(frame)
vignette:SetTexture(ART .. "RefusVignette")

local dial = frame:CreateTexture(nil, "ARTWORK")
dial:SetPoint("CENTER")
dial:SetTexture(ART .. "RefusClock")

local hands = frame:CreateTexture(nil, "OVERLAY")
hands:SetPoint("CENTER")
hands:SetTexture(ART .. "RefusHands")

local burst = CreateFrame("Frame", nil, frame)
burst:SetAllPoints(frame)
burst:SetFrameLevel(frame:GetFrameLevel() + 2)
local flash = burst:CreateTexture(nil, "OVERLAY")
flash:SetAllPoints(burst)
flash:SetTexture(ART .. "RefusFlash")
flash:SetBlendMode("ADD")

local started

local function Draw(t)
    local out = ease(ramp(t, 6.0, 6.6))
    local edge = ease(ramp(t, 0, 0.6)) * (1 - ramp(t, 6.0, 7.0))
    vignette:SetAlpha(edge)
    tint:SetAlpha(0.16 * edge)
    local pulse = TickPulse(t)
    local shown = ease(ramp(t, 0.2, 1.2)) * (1 - out)
    local grow = 1 + 0.35 * out + 0.025 * pulse
    local size = frame:GetWidth() * DIAL_SHARE * grow
    dial:SetSize(size, size)
    dial:SetAlpha(clamp(shown * (DIAL_ALPHA + 0.12 * pulse)))
    hands:SetSize(size, size)
    hands:SetAlpha(clamp(shown * (HANDS_ALPHA + 0.15 * pulse)))
    -- The texture's coordinates turned one way turn the picture the other: backwards on screen
    Rotate(hands, -math.rad(HandsAngle(t)))
    flash:SetAlpha(0.85 * math.exp(-((t - 6.15) / 0.18) ^ 2))
end

frame:SetScript("OnUpdate", function(self)
    local t = GetTime() - started
    if t >= LENGTH then
        self:Hide()
        return
    end
    Draw(t)
end)

-- The server's "R": from the cut
function RefusOverlay_Play()
    started = GetTime()
    Draw(0)
    frame:Show()
end
