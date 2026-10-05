-- Our own sound engine (the client extension DLL's EvolutionsAudio, AwesomeWotlkLib): what the server asks it to play,
-- the place's acoustics fed to it, and /eva to try its sounds. The server whispers (mod-stat-growth EvolutionsAudio.h):
--   EVA <tab> P <tab> <key> [<tab> G<guid, 16 hex digits> | X<x>,<y>,<z>]   play (on that object, or at that point)
--   EVA <tab> S <tab> <guid>                                               the sounds on that object fade out
-- /eva play <key> [target]   plays a sound of the bank on you (or your target) - /eva list, /eva reload (the bank and
-- its files read again: tune the client copy of Interface\AddOns\EvolutionsAudio\sounds.txt without restarting).
local PREFIX = "EVA"
local ENVIRONMENT_INTERVAL = 0.25

local function Available()
    return EvolutionsAudio_Play ~= nil
end

local function ObjectTarget(unit)
    local guid = UnitGUID(unit)
    return guid and ("G" .. string.gsub(guid, "^0x", "")) or nil
end

local function Underwater()
    for index = 1, MIRRORTIMER_NUMTIMERS or 3 do
        local timer, _, _, scale = GetMirrorTimerInfo(index)
        if timer == "BREATH" and scale and scale < 0 then
            return true
        end
    end
    return false
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:SetScript("OnEvent", function(self, event, prefix, message)
    if prefix ~= PREFIX or not message or not Available() then
        return
    end
    local command, first, second = string.match(message, "^(%a)\t([^\t]+)\t?(.*)$")
    if command == "P" then
        EvolutionsAudio_Play(first, second ~= "" and second or nil)
    elseif command == "S" then
        EvolutionsAudio_StopOn(first)
    end
end)

local elapsed = 0
frame:SetScript("OnUpdate", function(self, delta)
    elapsed = elapsed + delta
    if elapsed < ENVIRONMENT_INTERVAL or not Available() then
        return
    end
    elapsed = 0
    EvolutionsAudio_SetEnvironment(IsIndoors() and 1 or 0, Underwater() and 1 or 0)
end)

SLASH_EVOLUTIONSAUDIO1 = "/eva"
SlashCmdList["EVOLUTIONSAUDIO"] = function(text)
    if not Available() then
        DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: the client extension is not loaded.")
        return
    end
    local command, key, where = string.match(text or "", "^(%S*)%s*(%S*)%s*(%S*)")
    if command == "play" and key ~= "" then
        local target = ObjectTarget(where == "target" and "target" or "player")
        if not EvolutionsAudio_Play(key, target) then
            DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: no sound " .. key .. " (/eva list).")
        end
    elseif command == "reload" then
        DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: " .. EvolutionsAudio_Reload() .. " sounds loaded.")
    elseif command == "list" then
        DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: " .. string.gsub(EvolutionsAudio_Keys(), ",", ", "))
    else
        DEFAULT_CHAT_FRAME:AddMessage("/eva play <key> [target], /eva list, /eva reload")
    end
end
