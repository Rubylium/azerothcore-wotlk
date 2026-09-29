-- L'Infini's music (server: mod-stat-growth InfiniteGod.cpp, the Défi board's god fight in Ulduar's Celestial
-- Planetarium). The fight runs on the timeline of its track, so the server says when to play it, with addon messages
-- whispered to oneself under the prefix "Infini":
--   A              the arena: out of combat in the Planetarium, a tense stock track of Algalon's hall, looping
--   P <tab> <ms>   the pull: the god's track from its start (<ms> after the pull; a client that comes in later, a
--                  reload or a reconnection mid-fight, cannot seek into it and stays silent until the fight ends)
--   W              a wipe: the track fades out, then the arena's music fades in
--   K              the kill: the track plays to its end, then stops
--   S              stop, at once
-- and asks the server where it stands (HELLO) whenever it enters the world. Leaving the instance stops everything.
-- A fade ramps the music volume (Sound_MusicVolume) down and back: the player's own setting is restored after every
-- fade, and on logout or reload.

local PREFIX = "Infini"
local TRACK = "Sound\\Music\\Evolutions\\LInfini.mp3"
local TRACK_SECONDS = 306
local ARENA = "Sound\\Music\\ZoneMusic\\UlduarRaidInt\\UR_AlgalonPlanetaryHallWalk.mp3"
local FADE_OUT_SECONDS = 3
local FADE_IN_SECONDS = 3
-- A pull message this old still starts the track: past it, the music would run behind the fight
local LATE_PULL_MS = 2500

local playing              -- nil, "arena" or "fight"
local trackEndsAt          -- GetTime() at which the fight's track ends (it loops otherwise)
local fade                 -- the fade under way: from, to, start, seconds, done
local ownVolume            -- the player's music volume while a fade has it

local frame = CreateFrame("Frame")

local function SetMusicVolume(volume)
    SetCVar("Sound_MusicVolume", format("%.3f", max(0, min(1, volume))))
end

-- The fade under way is dropped and the player's volume put back
local function RestoreVolume()
    fade = nil
    if ownVolume then
        SetMusicVolume(ownVolume)
        ownVolume = nil
    end
end

local function StopAll()
    RestoreVolume()
    if playing then
        StopMusic()
    end
    playing = nil
    trackEndsAt = nil
end

local function Fade(to, seconds, done)
    if not ownVolume then
        ownVolume = tonumber(GetCVar("Sound_MusicVolume")) or 1
    end
    local from = tonumber(GetCVar("Sound_MusicVolume")) or ownVolume
    fade = { from = from, to = to * ownVolume, start = GetTime(), seconds = seconds, done = done }
end

local function PlayArena()
    if playing == "arena" then
        return
    end
    StopAll()
    playing = "arena"
    ownVolume = tonumber(GetCVar("Sound_MusicVolume")) or 1
    SetMusicVolume(0)
    PlayMusic(ARENA)
    Fade(1, FADE_IN_SECONDS, RestoreVolume)
end

local function PlayFight(elapsedMs)
    StopAll()
    if elapsedMs > LATE_PULL_MS then
        return
    end
    playing = "fight"
    trackEndsAt = GetTime() + TRACK_SECONDS - elapsedMs / 1000
    PlayMusic(TRACK)
end

-- The track fades out, and the arena's tense music comes back in
local function Wipe()
    if playing ~= "fight" then
        PlayArena()
        return
    end
    playing = "fading"
    trackEndsAt = nil
    Fade(0, FADE_OUT_SECONDS, function()
        StopMusic()
        playing = nil
        RestoreVolume()
        PlayArena()
    end)
end

frame:SetScript("OnUpdate", function()
    local now = GetTime()
    if fade then
        local progress = min(1, (now - fade.start) / fade.seconds)
        SetMusicVolume(fade.from + (fade.to - fade.from) * progress)
        if progress >= 1 then
            local done = fade.done
            fade = nil
            if done then
                done()
            end
        end
    end
    -- PlayMusic loops: the fight's track stops at its end, a kill's too
    if trackEndsAt and now >= trackEndsAt then
        trackEndsAt = nil
        if playing == "fight" then
            StopMusic()
            playing = nil
        end
    end
end)

local function Handle(message)
    local kind, value = strsplit("\t", message)
    if kind == "A" then
        if playing ~= "fight" and playing ~= "fading" then
            PlayArena()
        end
    elseif kind == "P" then
        PlayFight(tonumber(value) or 0)
    elseif kind == "W" then
        Wipe()
    elseif kind == "K" then
        -- The track plays out (trackEndsAt); nothing comes after it
        if playing ~= "fight" then
            StopAll()
        end
    elseif kind == "S" then
        StopAll()
    end
end

frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:RegisterEvent("PLAYER_LEAVING_WORLD")
frame:RegisterEvent("PLAYER_LOGOUT")
frame:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            Handle(message or "")
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- A new map, a reload, a reconnection: whatever played is over; the server says what plays here
        StopAll()
        SendAddonMessage(PREFIX, "HELLO", "WHISPER", UnitName("player"))
    else
        StopAll()
    end
end)
