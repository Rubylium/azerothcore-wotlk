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
--
-- Everything plays with PlayMusic: the music channel, at the player's own music volume, silent when their music is
-- off. Two rules keep it playing:
-- - PlayMusic replaces whatever the channel plays. It is never preceded by StopMusic: the client carries a stop out
--   after the new file has started, and a StopMusic right before a PlayMusic silenced the new track a moment after
--   it began (the god's track was heard for an instant on the pull, then nothing).
-- - Only a wipe's fade touches the music volume (Sound_MusicVolume): the player's own value is kept as the string
--   the client gave, and written back as it was once the fade is over, on logout and on reload.
-- PlayMusic's file should hold the channel over the zone's music until StopMusic; in case a subzone's music takes it
-- back, the arena's own track is asked for again whenever the subzone changes (it restarts, which a loop of ambience
-- does not mind). The god's track is never asked for again: it would start over, off the fight's timeline.

local PREFIX = "Infini"
local TRACK = "Sound\\Music\\Evolutions\\LInfini.mp3"
local TRACK_SECONDS = 306
local ARENA = "Sound\\Music\\ZoneMusic\\UlduarRaidInt\\UR_AlgalonPlanetaryHallWalk.mp3"
local FADE_OUT_SECONDS = 3
local FADE_IN_SECONDS = 3
-- A pull message this old still starts the track: past it, the music would run behind the fight
local LATE_PULL_MS = 2500

local playing              -- nil, "arena", "fight" or "fading" (the fight's track on its way out)
local trackEndsAt          -- GetTime() at which the fight's track ends (PlayMusic loops it otherwise)
local fade                 -- the fade under way: from, to, start, seconds, done
local ownVolume            -- the player's Sound_MusicVolume, as the client gave it, while a fade has it

local frame = CreateFrame("Frame")

-- The player's own music volume back, exactly, and no fade running
local function RestoreVolume()
    fade = nil
    if ownVolume then
        SetCVar("Sound_MusicVolume", ownVolume)
        ownVolume = nil
    end
end

-- Ramps the music volume from where it is to a share of the player's own
local function Fade(share, seconds, done)
    if not ownVolume then
        ownVolume = GetCVar("Sound_MusicVolume")
    end
    local own = tonumber(ownVolume) or 1
    fade = {
        from = tonumber(GetCVar("Sound_MusicVolume")) or own,
        to = share * own,
        start = GetTime(),
        seconds = seconds,
        done = done,
    }
end

local function Play(file, kind)
    PlayMusic(file)
    playing = kind
end

local function StopAll()
    RestoreVolume()
    if playing then
        StopMusic()
    end
    playing = nil
    trackEndsAt = nil
end

local function PlayArena()
    if playing == "arena" then
        return
    end
    RestoreVolume()
    trackEndsAt = nil
    Play(ARENA, "arena")
end

local function PlayFight(elapsedMs)
    RestoreVolume()
    if elapsedMs > LATE_PULL_MS then
        if playing == "arena" then
            StopAll()
        end
        return
    end
    Play(TRACK, "fight")
    trackEndsAt = GetTime() + TRACK_SECONDS - elapsedMs / 1000
end

-- The track fades out, the arena's music takes over from it at no volume and fades in to the player's own
local function Wipe()
    if playing ~= "fight" then
        PlayArena()
        return
    end
    playing = "fading"
    trackEndsAt = nil
    Fade(0, FADE_OUT_SECONDS, function()
        Play(ARENA, "arena")
        Fade(1, FADE_IN_SECONDS, RestoreVolume)
    end)
end

frame:SetScript("OnUpdate", function()
    local now = GetTime()
    if fade then
        local progress = min(1, (now - fade.start) / fade.seconds)
        SetCVar("Sound_MusicVolume", format("%.3f", fade.from + (fade.to - fade.from) * progress))
        if progress >= 1 then
            local done = fade.done
            fade = nil
            if done then
                done()
            end
        end
    end
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
frame:RegisterEvent("ZONE_CHANGED")
frame:RegisterEvent("ZONE_CHANGED_INDOORS")
frame:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            Handle(message or "")
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- A new map, a reload, a reconnection: whatever played is over; the server says what plays here
        StopAll()
        SendAddonMessage(PREFIX, "HELLO", "WHISPER", UnitName("player"))
    elseif event == "ZONE_CHANGED" or event == "ZONE_CHANGED_INDOORS" then
        if playing == "arena" and not fade then
            PlayMusic(ARENA)
        end
    else
        StopAll()
    end
end)
