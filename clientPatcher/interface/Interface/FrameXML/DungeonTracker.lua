-- Dungeon progress tracker: retail's objective tracker look for the boss list of the instance you are in.
-- While it is shown it takes the quest tracker's place; the quest tracker returns when you leave the instance.
-- The quest tracker is DragonUI's when that addon is on (it silences Blizzard's WatchFrame and shows its own at
-- its own position), Blizzard's otherwise. The Mythic+ timer (MythicPlus.lua) claims the same place.
-- Fed by the server (mod-stat-growth DungeonProgressSystem) over the "DungeonProgress" addon channel:
--   STATE <encounterId>:<0|1> ...   boss list in order, 1 = defeated
--   NONE                            not in an instance
-- Boss names come from DungeonTrackerNames.lua, generated from the client's own DungeonEncounter.dbc.

local PREFIX = "DungeonProgress"
local WIDTH = 235
local LINE_HEIGHT = 18
local HEADER_HEIGHT = 26
local ICON_SIZE = 16

local tracker
local entries = {}
local shown = false

local function skinTexture(texture, atlas)
    if RetailUI and RetailUI.SetAtlas then
        RetailUI.SetAtlas(texture, atlas)
    end
end

local function createTracker()
    if tracker then return tracker end

    tracker = CreateFrame("Frame", "DungeonTrackerFrame", UIParent)
    tracker:SetWidth(WIDTH)
    tracker:SetHeight(HEADER_HEIGHT)
    tracker:Hide()

    local header = tracker:CreateTexture(nil, "ARTWORK")
    skinTexture(header, "ui-questtracker-primary-objective-header-2x")
    header:SetWidth(300)
    header:SetHeight(40)
    header:SetPoint("TOPLEFT", tracker, "TOPLEFT", -14, 6)
    tracker.header = header

    local title = tracker:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOPLEFT", tracker, "TOPLEFT", 2, -4)
    title:SetJustifyH("LEFT")
    tracker.title = title

    local count = tracker:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    count:SetPoint("TOPRIGHT", tracker, "TOPRIGHT", -2, -5)
    tracker.count = count

    return tracker
end

local function acquireEntry(index)
    if entries[index] then return entries[index] end

    local entry = CreateFrame("Frame", nil, tracker)
    entry:SetWidth(WIDTH)
    entry:SetHeight(LINE_HEIGHT)
    entry.icon = entry:CreateTexture(nil, "ARTWORK")
    entry.icon:SetWidth(ICON_SIZE)
    entry.icon:SetHeight(ICON_SIZE)
    entry.icon:SetPoint("TOPLEFT", entry, "TOPLEFT", 4, -1)
    entry.text = entry:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    entry.text:SetPoint("LEFT", entry.icon, "RIGHT", 4, 0)
    entry.text:SetPoint("RIGHT", entry, "RIGHT", -4, 0)
    entry.text:SetJustifyH("LEFT")
    entries[index] = entry
    return entry
end

-- The quest tracker keeps its position while hidden, so what takes its place anchors on it: DragonUI's tracker
-- anchor (DragonUI_QuestTrackerFrame, moved by DragonUI's editor) or Blizzard's WatchFrame
function DungeonTracker_AnchorToQuestArea(region)
    region:ClearAllPoints()
    if DragonUI_QuestTrackerFrame then
        region:SetPoint("TOPRIGHT", DragonUI_QuestTrackerFrame, "TOPRIGHT", 0, 0)
    elseif WatchFrame then
        region:SetPoint("TOPLEFT", WatchFrame, "TOPLEFT", 0, 0)
    else
        region:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", -110, -220)
    end
end

local function anchorTracker()
    -- Under the Mythic+ timer while it shows (MythicPlus.lua)
    if MythicPlusTimerFrame and MythicPlusTimerFrame:IsShown() then
        tracker:ClearAllPoints()
        tracker:SetPoint("TOPRIGHT", MythicPlusTimerFrame, "BOTTOMRIGHT", -4, -8)
    else
        DungeonTracker_AnchorToQuestArea(tracker)
    end
end

-- Who keeps the quest tracker hidden: the dungeon tracker, the Mythic+ timer
local claims = {}
local dragonHooked = false
-- DragonUI wanted its tracker shown while we held the place: it comes back when we leave it
local dragonWanted = false

local function questTrackerHidden()
    return next(claims) ~= nil
end

-- With DragonUI only its own tracker is hidden: it keeps Blizzard's WatchFrame running (invisible) for the quest
-- POI buttons, and calling WatchFrame_Update from here would taint them
local function applyQuestTracker()
    local hidden = questTrackerHidden()
    local dragon = DragonUIObjectiveTracker
    if dragon and not dragonHooked then
        -- DragonUI shows its tracker again on every quest update
        dragonHooked = true
        dragon:HookScript("OnShow", function(self)
            if questTrackerHidden() then
                dragonWanted = true
                self:Hide()
            end
        end)
    end

    if dragon then
        if hidden and dragon:IsShown() then
            dragonWanted = true
            dragon:Hide()
        elseif not hidden and dragonWanted then
            dragonWanted = false
            dragon:Show()
        end
    elseif WatchFrame then
        if hidden then WatchFrame:Hide() else WatchFrame:Show() end
    end
end

function DungeonTracker_ClaimQuestArea(owner, claim)
    claims[owner] = claim and true or nil
    applyQuestTracker()
end

local function setQuestTrackerShown(show)
    DungeonTracker_ClaimQuestArea("dungeon", not show)
end

local function layout(bosses)
    createTracker()
    local defeated = 0
    for index, boss in ipairs(bosses) do
        local entry = acquireEntry(index)
        entry:ClearAllPoints()
        if index == 1 then
            entry:SetPoint("TOPLEFT", tracker, "TOPLEFT", 0, -HEADER_HEIGHT)
        else
            entry:SetPoint("TOPLEFT", entries[index - 1], "BOTTOMLEFT", 0, 0)
        end

        skinTexture(entry.icon, boss.done and "ui-questtracker-tracker-check-2x"
            or "ui-questtracker-objective-nub-2x")
        entry.text:SetText((DungeonTrackerNames and DungeonTrackerNames[boss.id]) or ("Boss " .. boss.id))
        if boss.done then
            defeated = defeated + 1
            entry.text:SetTextColor(0.5, 0.5, 0.5)
        else
            entry.text:SetTextColor(1, 0.82, 0)
        end
        entry:Show()
    end

    for index = #bosses + 1, #entries do
        entries[index]:Hide()
    end

    tracker.title:SetText(GetRealZoneText() or (GetInstanceInfo and GetInstanceInfo()) or "")
    tracker.count:SetText(defeated .. "/" .. #bosses)
    tracker:SetHeight(HEADER_HEIGHT + #bosses * LINE_HEIGHT)
    anchorTracker()
    tracker:Show()
end

local function showTracker(bosses)
    if #bosses == 0 then
        return
    end
    shown = true
    setQuestTrackerShown(false)
    layout(bosses)
end

local function hideTracker()
    shown = false
    if tracker then tracker:Hide() end
    setQuestTrackerShown(true)
end

local function parseState(message)
    local bosses = {}
    for field in string.gmatch(message, "[^\t]+") do
        local id, done = string.match(field, "^(%d+):([01])$")
        if id then
            bosses[#bosses + 1] = { id = tonumber(id), done = done == "1" }
        end
    end
    return bosses
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:SetScript("OnEvent", function(_, event, prefix, message)
    if event == "PLAYER_ENTERING_WORLD" then
        -- The server sends the state right after; until then keep the quest tracker
        if not IsInInstance() then hideTracker() end
        return
    end
    if prefix ~= PREFIX then return end

    if message == "NONE" or string.sub(message, 1, 5) ~= "STATE" then
        hideTracker()
    else
        showTracker(parseState(message))
    end
end)

-- Blizzard re-shows the quest tracker on quest and achievement updates
if hooksecurefunc then
    hooksecurefunc("WatchFrame_Update", function()
        if questTrackerHidden() and WatchFrame and not DragonUIObjectiveTracker then WatchFrame:Hide() end
    end)
end

function DungeonTracker_UpdateAnchor()
    if tracker then
        anchorTracker()
    end
end
