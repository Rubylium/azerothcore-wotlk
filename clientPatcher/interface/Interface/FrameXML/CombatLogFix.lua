-- The 3.3.5a client's combat log can jam: COMBAT_LOG_EVENT_UNFILTERED stops firing for every addon, and meters
-- such as Details freeze. The jam lives in the client, not in the UI, so /reload does not clear it; relogging or
-- CombatLogClearEntries() does. Busy fights (many bots, lots of AoE) make it more likely.
--
-- Clearing drops the entries not handed to addons yet, so it must only happen when the log is really stuck:
-- UNIT_COMBAT fires for a hit before the combat log hands that same hit out, so a hit is never proof on its own.
-- Instead, after a hit on the player or their pet, the log gets a full second to show any event;
-- only a whole second of silence after a hit counts as a jam. The loading screen, where nothing is lost, also
-- clears it.

local GRACE = 1
local CHECK_INTERVAL = 0.25

-- When the first hit still waiting for the combat log happened, nil when the log answered
local hitSince

local watchdog = CreateFrame("Frame")
watchdog:Hide()

local elapsed = 0
watchdog:SetScript("OnUpdate", function(self, delta)
    elapsed = elapsed + delta
    if elapsed < CHECK_INTERVAL then
        return
    end
    elapsed = 0

    if not hitSince then
        self:Hide()
    elseif GetTime() - hitSince > GRACE then
        hitSince = nil
        self:Hide()
        CombatLogClearEntries()
    end
end)

watchdog:RegisterEvent("COMBAT_LOG_EVENT_UNFILTERED")
watchdog:RegisterEvent("UNIT_COMBAT")
watchdog:RegisterEvent("PLAYER_ENTERING_WORLD")
watchdog:SetScript("OnEvent", function(self, event, unit)
    if event == "COMBAT_LOG_EVENT_UNFILTERED" then
        hitSince = nil
    elseif event == "UNIT_COMBAT" then
        -- Not the target: it can be fought beyond the combat log's range, which is silence without a jam
        if not hitSince and (unit == "player" or unit == "pet") then
            hitSince = GetTime()
            self:Show()
        end
    else
        hitSince = nil
        CombatLogClearEntries()
    end
end)
