-- Excroissance on the raid frame (mod-pestifere PestifereScripts.cpp).
--
-- 3.3.5 has no absorb API. UnitGetTotalAbsorbs does not exist in this client, so WotLKCompat.lua stubs it
-- to return nothing and the CompactRaidFrame backport's absorb overlay - which is otherwise complete, with
-- its shield fill, its overlay and its over-absorb glow - has nothing to draw. A shield here was real and
-- invisible: it soaked damage and no frame ever showed it.
--
-- The amount lives on the server, so the server sends it. Every time the growth changes size it whispers
-- the new number on the addon channel, and this adds it to whatever the raid frame already believes about
-- the player's absorbs. Nothing is estimated client-side: the figure drawn is the figure the server holds.

local PREFIX = "PestifereShield"

local shield = 0
local wrapped = false

-- The raid frame batches its redraws behind a dirty flag, so it is enough to poke the frames showing the
-- player. CRF_ForEachCompactUnitFrame walks all three of the addon's frame layouts (raid, grouped raid and
-- party), which is why this does not keep a registry of its own.
local function refresh()
    if type(CRF_ForEachCompactUnitFrame) ~= "function"
        or type(CompactUnitFrame_UpdateHealPrediction) ~= "function" then
        return
    end

    CRF_ForEachCompactUnitFrame(function(frame)
        local unit = frame.displayedUnit or frame.unit
        if unit and UnitExists(unit) and UnitIsUnit(unit, "player") then
            CompactUnitFrame_UpdateHealPrediction(frame)
        end
    end)
end

-- Wrapped rather than replaced, and only once the addons have run: CRFHealAbsorb.lua installs its own
-- UnitGetTotalAbsorbs over the stub, reading AbsorbsMonitor for every shield the client can work out by
-- itself. That one still answers for everyone else; this only adds what it cannot know.
local function wrap()
    if wrapped then
        return
    end
    wrapped = true

    local previous = UnitGetTotalAbsorbs
    UnitGetTotalAbsorbs = function(unit)
        local total = previous and previous(unit) or 0
        if shield > 0 and unit and UnitExists(unit) and UnitIsUnit(unit, "player") then
            total = total + shield
        end
        return total
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("PLAYER_LOGIN")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event ~= "CHAT_MSG_ADDON" then
        -- PLAYER_LOGIN is after every addon has loaded, which is the point of doing it here
        wrap()
        return
    end

    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    local amount = tonumber(message)
    if not amount then
        return
    end

    shield = amount
    wrap()
    refresh()
end)
