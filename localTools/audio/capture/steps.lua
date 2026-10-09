-- An example run (edit the keys): Outlaw sounds through EvolutionsAudio, played on the character as the server's
-- PlayOn does, 4 s apart; then the same key as a ui sound to compare.
local function on(key)
    return function()
        local guid = string.gsub(UnitGUID("player") or "", "^0x", "")
        EvolutionsAudio_Play(key, "G" .. guid)
    end
end
local function ui(key)
    return function() EvolutionsAudio_Play(key) end
end
FxLabSteps = {
    { 3, function() if StopCinematic then StopCinematic() end end },
    { 1, ".fxlab" },
    { 8, function() Note("start") end },
    { 4, on("Outlaw.PistolShot") }, { 4, on("Outlaw.PistolShot") }, { 4, on("Outlaw.PistolShot") },
    { 4, on("Outlaw.BetweenTheEyes") }, { 4, on("Outlaw.BetweenTheEyes") },
    { 4, on("Outlaw.SinisterStrike") }, { 4, on("Outlaw.SinisterStrike") },
    { 4, ui("Outlaw.PistolShot") }, { 4, ui("Outlaw.PistolShot") },
    { 4, on("Outlaw.Dispatch") },
    { 5, ForceQuit },
}
