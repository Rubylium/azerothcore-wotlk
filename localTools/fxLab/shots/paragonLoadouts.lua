local function send(text) SendAddonMessage("Paragon", text, "WHISPER", UnitName("player")) end
local function describe(tag)
    Note(tag .. ": picker '" .. tostring(UIDropDownMenu_GetText(ParagonLoadoutPicker)) .. "'")
end

FxLabSteps = {
    { 3, function() if StopCinematic then StopCinematic() end end },
    { 2, function() send("OPEN") end },
    { 2, function() describe("opened") end },
    { 0.5, Shot },
    -- the board as it is, saved first so the test gives it back
    { 0.5, function() send("LSAVE\t9\tAvant le test") end },
    { 1.5, function() describe("saved") end },
    { 0.5, function() ToggleDropDownMenu(1, nil, ParagonLoadoutPicker) end },
    { 1, Shot },
    { 0.5, function() CloseDropDownMenus(); send("PRESET\t2") end },
    { 2, function() describe("mythic preset") end },
    { 0.5, Shot },
    { 0.5, function() send("PRESET\t1") end },
    { 2, function() describe("raid preset") end },
    { 0.5, Shot },
    { 0.5, function() send("LAPPLY\t9") end },
    { 2, function() describe("restored") end },
    { 0.5, Shot },
    { 0.5, function() send("LDEL\t9") end },
    { 1.5, function() describe("deleted") end },
    { 0.5, Shot },
    { 1.0, ForceQuit },
}
