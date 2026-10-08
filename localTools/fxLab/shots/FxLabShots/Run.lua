-- LOCAL DEV ONLY: the step runner (see FxLabShots.lua)
local at, waited, started = 1, 0, false
local runner = CreateFrame("Frame")
runner:RegisterEvent("PLAYER_ENTERING_WORLD")
runner:SetScript("OnEvent", function()
    started = true
end)
runner:SetScript("OnUpdate", function(_, elapsed)
    local step = started and FxLabSteps and FxLabSteps[at]
    if not step then
        return
    end
    waited = waited + elapsed
    if waited < step[1] then
        return
    end
    waited = 0
    at = at + 1
    local ok, problem = pcall(function()
        if type(step[2]) == "string" then
            SendChatMessage(step[2], "SAY")
        else
            step[2]()
        end
    end)
    if not ok then
        Note("step " .. (at - 1) .. " failed: " .. tostring(problem))
    end
end)
