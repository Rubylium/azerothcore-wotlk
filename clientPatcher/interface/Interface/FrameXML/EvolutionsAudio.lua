-- Our own sound engine (the client extension DLL's EvolutionsAudio, AwesomeWotlkLib): what the server asks it to play,
-- the place's acoustics, the zone (its ambience) and the time of day fed to it, the game's music silenced while ours
-- plays, and /eva to try its sounds. The server whispers (mod-stat-growth EvolutionsAudio.h):
--   EVA <tab> P <tab> <key> [<tab> G<guid, 16 hex digits> | X<x>,<y>,<z>]   play (on that object, or at that point)
--   EVA <tab> S <tab> <guid>                                               the sounds on that object fade out
--   EVA <tab> M <tab> <key> [<tab> <fade in, ms>]                          that music (EvolutionsAudio_PlayMusic)
--   EVA <tab> N [<tab> <fade out, ms>]                                     the music fades out
-- /eva play <key> [target]   plays a sound of the bank on you (or your target) - /eva music <key>, /eva stopmusic,
-- /eva list, /eva reload (the bank and its files read again: tune the client copy of
-- Interface\AddOns\EvolutionsAudio\sounds.txt without restarting).
local PREFIX = "EVA"
local ENVIRONMENT_INTERVAL = 0.25
local DAY_START, DAY_END = 6, 21
-- The player's music volume while ours plays (the DLL's CVar, saved in Config.wtf with Sound_MusicVolume); -1: none
local HELD_MUSIC = "evaGameMusicVolume"

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

-- The game's own music, silent while ours is heard: its volume held (kept in HELD_MUSIC, the game's set to 0) and
-- given back once ours is heard no more - after its fade-out, on leaving the world, and at the first frame after a
-- crash mid-fight (both CVars are written to Config.wtf together, so the held one is still there). The engine plays
-- ours at the held volume. Moving the music slider meanwhile sets the held volume: ours follows, the game's stays 0.
local function HeldMusicVolume()
    local held = GetCVar(HELD_MUSIC)
    return held and tonumber(held) and tonumber(held) >= 0 and held or nil
end

local function ReleaseGameMusic()
    local held = HeldMusicVolume()
    if held then
        SetCVar("Sound_MusicVolume", held)
        SetCVar(HELD_MUSIC, "-1")
    end
end

local function UpdateGameMusic()
    if not EvolutionsAudio_MusicPlaying or not GetCVar(HELD_MUSIC) then
        return
    end
    local _, heard = EvolutionsAudio_MusicPlaying()
    if not heard then
        ReleaseGameMusic()
        return
    end
    -- Held as ours starts, or set again from the slider
    local game = GetCVar("Sound_MusicVolume") or "0"
    if not HeldMusicVolume() or (tonumber(game) or 0) > 0 then
        SetCVar(HELD_MUSIC, game)
        SetCVar("Sound_MusicVolume", "0")
    end
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_LEAVING_WORLD")
frame:SetScript("OnEvent", function(self, event, prefix, message)
    if event == "PLAYER_LEAVING_WORLD" then
        -- A loading screen or a logout: the engine stops every sound, the game's music is given back before the
        -- settings are written
        if Available() and GetCVar(HELD_MUSIC) then
            if EvolutionsAudio_StopMusic then
                EvolutionsAudio_StopMusic(0)
            end
            ReleaseGameMusic()
        end
        return
    end
    if prefix ~= PREFIX or not message or not Available() then
        return
    end
    local command, first, second = string.match(message, "^(%a)\t?([^\t]*)\t?(.*)$")
    if command == "P" and first ~= "" then
        EvolutionsAudio_Play(first, second ~= "" and second or nil)
    elseif command == "S" and first ~= "" then
        EvolutionsAudio_StopOn(first)
    elseif command == "M" and first ~= "" and EvolutionsAudio_PlayMusic then
        EvolutionsAudio_PlayMusic(first, tonumber(second))
    elseif command == "N" and EvolutionsAudio_StopMusic then
        EvolutionsAudio_StopMusic(tonumber(first))
    end
end)

local elapsed = 0
frame:SetScript("OnUpdate", function(self, delta)
    if not Available() then
        return
    end
    UpdateGameMusic()
    elapsed = elapsed + delta
    if elapsed < ENVIRONMENT_INTERVAL then
        return
    end
    elapsed = 0
    EvolutionsAudio_SetEnvironment(IsIndoors() and 1 or 0, Underwater() and 1 or 0)
    -- The zone's ambience (its emitters name it as GetRealZoneText does), by day 6:00-21:00 server time
    if EvolutionsAudio_SetZone then
        EvolutionsAudio_SetZone(GetRealZoneText() or "")
        local hour = GetGameTime()
        EvolutionsAudio_SetDaytime((hour >= DAY_START and hour < DAY_END) and 1 or 0)
    end
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
    elseif command == "music" and key ~= "" and EvolutionsAudio_PlayMusic then
        if not EvolutionsAudio_PlayMusic(key) then
            DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: no music " .. key .. " (/eva list).")
        end
    elseif command == "stopmusic" and EvolutionsAudio_StopMusic then
        EvolutionsAudio_StopMusic()
    elseif command == "reload" then
        DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: " .. EvolutionsAudio_Reload() .. " sounds loaded.")
    elseif command == "list" then
        DEFAULT_CHAT_FRAME:AddMessage("Evolutions audio: " .. string.gsub(EvolutionsAudio_Keys(), ",", ", "))
    else
        DEFAULT_CHAT_FRAME:AddMessage("/eva play <key> [target], /eva music <key>, /eva stopmusic, /eva list, "
            .. "/eva reload")
    end
end
