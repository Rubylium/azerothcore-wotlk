-- The red areas drawn under enemy abilities in mythic dungeons (mod-stat-growth, GroundIndicators.cpp) are painted on
-- the ground by the client's "Projected Textures" option: with it off they are not drawn at all. It is turned on
-- whenever the player enters the world, so switching it off in the video options only lasts until the next loading
-- screen.
local frame = CreateFrame("Frame")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:SetScript("OnEvent", function()
    if GetCVar("projectedTextures") ~= "1" then
        SetCVar("projectedTextures", "1")
    end
end)
