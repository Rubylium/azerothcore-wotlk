-- Dungeon Finder: says what is missing before you try, the way the modern client does.
--
-- The server only tells the client that a dungeon is locked and a reason code, and never locks a random dungeon
-- itself: picking "random heroic" with every heroic locked is allowed, and only the join fails, with a bare
-- "you do not meet the requirements". The server (mod-stat-growth, DungeonProgressSystem.cpp) sends what the
-- Dungeon Finder compared:
--   G <tab> <your average item level> <tab> <dungeon id>:<required average>,...   (resets everything below)
--   g <tab> <your average item level> <tab> ...                                   (continues the list)
--   R <tab> <random dungeon id> <tab> <lock reason>:<value>,...                   (a random dungeon you cannot join)
-- With it, a random dungeon you cannot join is greyed out with the reasons over it and Find Group disabled, and
-- each locked dungeon of the list shows the item level it requires.

local PREFIX = "DungeonFinderLocks"
local LOCK_GEAR_TOO_LOW = 4
local LOCK_ALREADY_DONE = 6

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Conditions d'accès non remplies",
    gear = "Niveau d'objet moyen requis : %d (le vôtre : %d)",
    gearHint = "Améliorez votre équipement pour débloquer ces donjons.",
    done = "Vous êtes déjà lié à tous ces donjons pour aujourd'hui.",
    column = "niv. obj. %d",
} or {
    title = "Requirements not met",
    gear = "Average item level required: %d (yours: %d)",
    gearHint = "Improve your gear to unlock these dungeons.",
    done = "You are already saved to all of these dungeons today.",
    column = "ilvl %d",
}

-- Dungeon id -> average item level it requires, for the dungeons currently locked by gear
local required = {}
-- Random dungeon id -> { { reason = code, value = number }, ... } for the random dungeons that cannot be joined
local blockedRandom = {}
local average = 0

local function GearLine(need)
    return format(TEXT.gear, need, average)
end

-- The lowest requirement among the locked dungeons: what a random dungeon made only of those asks for
local function LowestRequirement()
    local lowest
    for _, need in pairs(required) do
        if not lowest or need < lowest then
            lowest = need
        end
    end
    return lowest
end

local function ReasonLines(block)
    local lines = {}
    for _, lock in ipairs(block) do
        if lock.reason == LOCK_GEAR_TOO_LOW and lock.value > 0 then
            tinsert(lines, GearLine(lock.value))
            tinsert(lines, TEXT.gearHint)
        elseif lock.reason == LOCK_ALREADY_DONE then
            tinsert(lines, TEXT.done)
        else
            tinsert(lines, _G["INSTANCE_UNAVAILABLE_SELF_" .. (LFG_INSTANCE_INVALID_CODES[lock.reason] or "OTHER")])
        end
    end
    return lines
end

-- The block of the random dungeon picked in the type menu, nil when it can be joined or a list is picked
local function CurrentBlock()
    local picked = LFDQueueFrame and LFDQueueFrame.type
    return type(picked) == "number" and blockedRandom[picked] or nil
end

local function IsInQueue()
    local mode = GetLFGMode()
    return mode == "queued" or mode == "rolecheck" or mode == "proposal" or mode == "listed"
end

-- -----------------------------------------------------------------------------------------------------------------
-- The blocked random dungeon: greyed out, reasons on top
-- -----------------------------------------------------------------------------------------------------------------

local blocker = CreateFrame("Frame", "DungeonFinderLocksBlocker", LFDQueueFrameRandom)
blocker:SetPoint("TOPLEFT", LFDQueueFrameRandomScrollFrame, "TOPLEFT", -6, 6)
blocker:SetPoint("BOTTOMRIGHT", LFDQueueFrameRandomScrollFrame, "BOTTOMRIGHT", 28, -6)
blocker:SetFrameLevel(LFDQueueFrameRandomScrollFrame:GetFrameLevel() + 20)
blocker:EnableMouse(true)
blocker:Hide()

local shade = blocker:CreateTexture(nil, "BACKGROUND")
shade:SetAllPoints()
shade:SetTexture(0, 0, 0, 0.62)

local lockIcon = blocker:CreateTexture(nil, "ARTWORK")
lockIcon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-LOCK")
lockIcon:SetSize(40, 40)
lockIcon:SetPoint("BOTTOM", blocker, "CENTER", 0, 34)

local title = blocker:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
title:SetPoint("TOP", lockIcon, "BOTTOM", 0, -6)
title:SetWidth(270)
title:SetTextColor(1, 0.3, 0.25)
title:SetText(TEXT.title)

local reasons = blocker:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
reasons:SetPoint("TOP", title, "BOTTOM", 0, -10)
reasons:SetWidth(260)
reasons:SetJustifyH("CENTER")
reasons:SetSpacing(4)

local wasBlocked = false

local function UpdateBlocked()
    if not LFDQueueFrame then
        return
    end

    local block = not IsInQueue() and CurrentBlock()
    if block then
        reasons:SetText(table.concat(ReasonLines(block), "\n"))
        blocker:Show()
        LFDQueueFrameFindGroupButton:Disable()
    else
        blocker:Hide()
    end

    -- Unblocked (new gear, another type picked): let the stock code decide about the button again
    if wasBlocked and not block then
        wasBlocked = false
        LFDQueueFrameFindGroupButton_Update()
    end
    wasBlocked = block and true or false
end

hooksecurefunc("LFDQueueFrameFindGroupButton_Update", UpdateBlocked)
hooksecurefunc("LFDQueueFrame_SetType", UpdateBlocked)
LFDQueueFrame:HookScript("OnShow", UpdateBlocked)

-- The disabled button still explains itself
LFDQueueFrameFindGroupButton:HookScript("OnEnter", function(button)
    local block = not IsInQueue() and CurrentBlock()
    if not block then
        return
    end

    GameTooltip:SetOwner(button, "ANCHOR_TOP")
    GameTooltip:AddLine(TEXT.title, 1, 0.3, 0.25)
    for _, line in ipairs(ReasonLines(block)) do
        GameTooltip:AddLine(line, 1, 1, 1, true)
    end
    GameTooltip:Show()
end)
LFDQueueFrameFindGroupButton:HookScript("OnLeave", function()
    GameTooltip:Hide()
end)

-- -----------------------------------------------------------------------------------------------------------------
-- The dungeon list: each locked dungeon shows the item level it requires
-- -----------------------------------------------------------------------------------------------------------------

hooksecurefunc("LFDQueueFrameSpecificListButton_SetDungeon", function(button, dungeonID)
    local need = required[dungeonID]
    if not need or LFGIsIDHeader(dungeonID) or not LFGLockList[dungeonID] then
        return
    end

    button.level:SetText(format(TEXT.column, need))
    button.level:SetFontObject(QuestDifficulty_Impossible)
    button.level:Show()
end)

-- Each row captured the stock tooltip handler when it loaded: add the numbers after it
local function AddRequirementToTooltip(button)
    local need = required[button.id]
    if need and GameTooltip:IsOwned(button) then
        GameTooltip:AddLine(GearLine(need), 1, 0.3, 0.3)
        GameTooltip:Show()
    end
end

for index = 1, NUM_LFD_CHOICE_BUTTONS or 0 do
    local button = _G["LFDQueueFrameSpecificListButton" .. index]
    if button then
        button:HookScript("OnEnter", AddRequirementToTooltip)
    end
end

-- A random dungeon the server does lock is greyed out in the type menu, its tooltip built from this message
local StockDeclinedMessage = LFDConstructDeclinedMessage
function LFDConstructDeclinedMessage(dungeonID)
    local message = StockDeclinedMessage(dungeonID)
    -- Only a gear lock, or one the server gave no reason for, is about item level
    local aboutGear = not message or message:find(INSTANCE_UNAVAILABLE_SELF_GEAR_TOO_LOW, 1, true)
    local need = required[dungeonID] or (aboutGear and LowestRequirement())
    if not need then
        return message
    end

    local line = GearLine(need)
    return message and (message .. "\n" .. line) or line
end

-- -----------------------------------------------------------------------------------------------------------------
-- Messages from the server
-- -----------------------------------------------------------------------------------------------------------------

local function Refresh()
    if LFDQueueFrame:IsVisible() and LFDQueueFrameSpecificList_Update then
        LFDQueueFrameSpecificList_Update()
    end
    UpdateBlocked()
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    local kind, first, list = strsplit("\t", message)
    if kind == "G" or kind == "g" then
        if kind == "G" then
            wipe(required)
            wipe(blockedRandom)
        end
        average = tonumber(first) or 0
        for id, need in string.gmatch(list or "", "(%d+):(%d+)") do
            required[tonumber(id)] = tonumber(need)
        end
    elseif kind == "R" then
        local block = {}
        for reason, value in string.gmatch(list or "", "(%d+):(%d+)") do
            tinsert(block, { reason = tonumber(reason), value = tonumber(value) })
        end
        -- Gear first: it is the one the player can act on
        table.sort(block, function(left, right)
            return (left.reason == LOCK_GEAR_TOO_LOW) and not (right.reason == LOCK_GEAR_TOO_LOW)
        end)
        blockedRandom[tonumber(first)] = block
    else
        return
    end

    Refresh()
end)
