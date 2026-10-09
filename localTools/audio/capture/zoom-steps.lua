-- Camera distance: the same keys on the character with the camera close, then zoomed all the way out, then close;
-- then as ui sounds (no position: the reference level).
local function on(key)
    return function()
        local guid = string.gsub(UnitGUID("player") or "", "^0x", "")
        EvolutionsAudio_Play(key, "G" .. guid)
    end
end
FxLabSteps = {
    { 3, function() if StopCinematic then StopCinematic() end end },
    { 1, ".fxlab" },
    { 6, function() CameraZoomIn(50); CameraZoomOut(8) end },
    { 3, on("Outlaw.PistolShot") }, { 4, on("Outlaw.BetweenTheEyes") },
    { 4, function() SetCVar("cameraDistanceMaxFactor", "4"); CameraZoomOut(60) end },
    { 4, on("Outlaw.PistolShot") }, { 4, on("Outlaw.BetweenTheEyes") },
    { 4, function() CameraZoomIn(60); CameraZoomOut(8) end },
    { 3, on("Outlaw.PistolShot") }, { 4, on("Outlaw.BetweenTheEyes") },
    { 4, function() EvolutionsAudio_Play("Outlaw.PistolShot") end },
    { 4, function() EvolutionsAudio_Play("Outlaw.BetweenTheEyes") end },
    { 5, ForceQuit },
}
