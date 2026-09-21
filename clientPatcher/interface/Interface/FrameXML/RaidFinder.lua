-- Raid Finder: a "Raids" tab in the Dungeon Finder to queue for specific raids, with bots filling the places no
-- player takes when allowed. The server side is mod-playerbots, Script/RaidFinder.cpp, which documents the addon
-- messages (prefix "RaidFinder", whispered to oneself).
-- The tab is the Dungeon Finder itself: same frame, art, role buttons and list rows (LFGSpecificChoiceTemplate).
-- While it is open the dungeon widgets (type menu, lists, buttons) are parked in a hidden frame, so nothing the
-- Dungeon Finder updates in the background can show through.
-- Mythic dungeons ("Mythique", see MythicDungeon.h on the server) share the same queue and panel: they are an entry
-- of the Dungeon Finder's type menu, which then shows the panel with the menu kept above it.

local PREFIX = "RaidFinder"
local ROW_HEIGHT = 16
local NUM_ROWS = 14
local ROLE_TANK, ROLE_HEALER, ROLE_DAMAGE = 2, 4, 8
local LOCK_GEAR_TOO_LOW, LOCK_RAID_LOCKED, LOCK_MISSING_ACHIEVEMENT = 4, 6, 1034

local STATE_NONE, STATE_QUEUED, STATE_READY, STATE_RUNNING = 0, 1, 2, 3

local french = GetLocale() == "frFR"
local TEXT = french and {
    dungeons = "Donjons",
    raids = "Raids",
    title = "Recherche de raid",
    choose = "Choisissez vos raids :",
    bots = "Compléter avec des bots",
    botsTip = "Des bots prennent les places qu'aucun joueur ne prend, le raid part tout de suite.",
    find = "Trouver un raid",
    enter = "Rejoindre le raid",
    quit = "Quitter le raid",
    players = "%d joueurs",
    heroic = "Mode héroïque",
    queued = "En file : %s  %s",
    ready = "Votre raid est prêt : %s",
    running = "Raid en cours : %s",
    roles = "|cff66bbffTank|r %d/%d   |cff66ff66Soins|r %d/%d   |cffff6666Dégâts|r %d/%d",
    noRole = "Choisissez au moins un rôle.",
    noSelection = "Choisissez au moins un raid.",
    gear = "Niveau d'objet moyen requis : %d",
    saved = "Vous êtes déjà sauvegardé pour ce raid : le Raid Finder a besoin d'une autre instance.",
    achievement = "Terminez d'abord ce raid en mode normal (haut fait requis).",
    partyQueued = "Votre chef de groupe vous a inscrit à la recherche de raid.",
    mythic = "Donjons mythiques",
    mythicPlus = "Mythique+",
    findMythic = "Trouver un groupe",
    enterMythic = "Rejoindre le donjon",
    quitMythic = "Quitter le donjon",
    readyMythic = "Votre groupe est prêt : %s",
    runningMythic = "Donjon mythique en cours : %s",
    noSelectionMythic = "Choisissez au moins un donjon.",
    mythicTip = "Mythique : niveau 80, un cran au-dessus de l'héroïque.\n"
        .. "Chaque boss donne à chaque joueur un objet épique de niveau 213.\n"
        .. "Pas de butin sur les monstres, pas de verrouillage.",
    messages = {
        "Aucun raid disponible parmi votre sélection.",
        "Un des raids choisis est verrouillé pour vous ou un membre du groupe.",
        "Seul le chef de groupe peut s'inscrire.",
        "Impossible : vous êtes déjà en file (donjon, champ de bataille) ou dans un groupe spécial.",
        "Un membre du groupe est parti ou déconnecté : vous quittez la file.",
        "Le raid a commencé mais vous ne pouvez pas y être envoyé maintenant : utilisez « Rejoindre le raid ».",
        "Un joueur a refusé : recherche d'un remplaçant.",
        "Vous êtes déjà dans un raid du Raid Finder.",
        "Choisissez au moins un rôle.",
        "Vous avez quitté la file du Raid Finder.",
    },
} or {
    dungeons = "Dungeons",
    raids = "Raids",
    title = "Raid Finder",
    choose = "Choose your raids:",
    bots = "Fill the raid with bots",
    botsTip = "Bots take the places no player takes, so the raid starts right away.",
    find = "Find a raid",
    enter = "Enter the raid",
    quit = "Leave the raid",
    players = "%d players",
    heroic = "Heroic mode",
    queued = "In queue: %s  %s",
    ready = "Your raid is ready: %s",
    running = "Raid under way: %s",
    roles = "|cff66bbffTank|r %d/%d   |cff66ff66Heal|r %d/%d   |cffff6666Damage|r %d/%d",
    noRole = "Choose at least one role.",
    noSelection = "Choose at least one raid.",
    gear = "Average item level required: %d",
    saved = "You are already saved to this raid: the Raid Finder needs another instance.",
    achievement = "Clear this raid in normal mode first (achievement required).",
    partyQueued = "Your party leader queued you for the Raid Finder.",
    mythic = "Mythic Dungeons",
    mythicPlus = "Mythic+",
    findMythic = "Find Group",
    enterMythic = "Enter the dungeon",
    quitMythic = "Leave the dungeon",
    readyMythic = "Your group is ready: %s",
    runningMythic = "Mythic dungeon under way: %s",
    noSelectionMythic = "Choose at least one dungeon.",
    mythicTip = "Mythic: level 80, a step above heroic.\nEach boss gives every player an item level 213 epic.\n"
        .. "No loot from trash, no lockout.",
    messages = {
        "No raid of your choice is available.",
        "One of the chosen raids is locked for you or a party member.",
        "Only the party leader can queue.",
        "Not possible: you are already queued (dungeon, battleground) or in a special group.",
        "A party member left or went offline: you left the queue.",
        "The raid started but you cannot be sent in now: use \"Enter the raid\".",
        "A player declined: looking for a replacement.",
        "You are already in a Raid Finder raid.",
        "Choose at least one role.",
        "You left the Raid Finder queue.",
    },
}

-- raid id -> { id, dungeon, name, size, heroic, minLevel, maxLevel, expansion, lock, value, mythic }
local raids = {}
local selected = {}
local collapsed = {}
-- Rows shown: { header = expansion } or a raid
local display = {}
local status = { state = STATE_NONE, raid = 0, tanks = 0, needTanks = 0, healers = 0, needHealers = 0, damage = 0,
    needDamage = 0, accepted = false }
local queuedSince
-- What the panel shows: nil (hidden), "raid" (the Raids tab) or "mythic" (the Dungeon Finder's mythic type)
local mode
local mythicSelected = false
local MYTHIC_VALUE = "mythic"

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function RaidName(raid)
    local name = raid and GetLFGDungeonInfo(raid.dungeon)
    return name or (raid and ("Raid " .. raid.id)) or ""
end

local function LockText(raid)
    if raid.lock == LOCK_GEAR_TOO_LOW and raid.value > 0 then
        return format(TEXT.gear, raid.value)
    elseif raid.lock == LOCK_RAID_LOCKED then
        return TEXT.saved
    elseif raid.lock == LOCK_MISSING_ACHIEVEMENT then
        return TEXT.achievement
    end
    return _G["INSTANCE_UNAVAILABLE_SELF_" .. (LFG_INSTANCE_INVALID_CODES[raid.lock] or "OTHER")]
end

local function RolesMask()
    local _, tank, healer, damage = GetLFGRoles()
    return (tank and ROLE_TANK or 0) + (healer and ROLE_HEALER or 0) + (damage and ROLE_DAMAGE or 0)
end

local function IsBusy()
    return status.state ~= STATE_NONE
end

-- The text for a raid, or its mythic dungeon version
local function Text(key, raid)
    return (raid and raid.mythic and TEXT[key .. "Mythic"]) or TEXT[key]
end

local function InMode(raid)
    return (raid.mythic and true or false) == (mode == "mythic")
end

-- -----------------------------------------------------------------------------------------------------------------
-- The tab: the Dungeon Finder's own widgets parked while it is open
-- -----------------------------------------------------------------------------------------------------------------

local panel = CreateFrame("Frame", "RaidFinderFrame", LFDQueueFrame)
panel:SetAllPoints(LFDQueueFrame)
panel:SetFrameLevel(LFDQueueFrame:GetFrameLevel() + 5)
panel:Hide()

local parking = CreateFrame("Frame")
parking:Hide()

local STOCK_WIDGETS = { "LFDQueueFrameTypeDropDown", "LFDQueueFrameRandom", "LFDQueueFrameSpecific",
    "LFDQueueFrameFindGroupButton", "LFDQueueFrameCancelButton", "LFDQueueFrameCooldownFrame",
    "LFDQueueFramePartyBackfill", "LFDQueueFrameNoLFDWhileLFR", "DungeonBotsQueueCheckButton" }
local parked = {}
local stockBackground

local function ParkStockWidgets(park, except)
    for _, name in ipairs(STOCK_WIDGETS) do
        local widget = _G[name]
        local parkThis = park and name ~= except
        if widget and parkThis and not parked[name] then
            parked[name] = { parent = widget:GetParent(), level = widget:GetFrameLevel() }
            widget:SetParent(parking)
        elseif widget and not parkThis and parked[name] then
            widget:SetParent(parked[name].parent)
            widget:SetFrameLevel(parked[name].level)
            parked[name] = nil
        elseif widget and not parkThis and widget:GetParent() == parking then
            -- Parked without a record left (an error cut a tab switch short): back to the Dungeon Finder
            widget:SetParent(LFDQueueFrame)
        end
    end

    -- The stock list shows on the dungeon wall; the random dungeon on quest paper
    if park then
        stockBackground = stockBackground or LFDQueueFrameBackground:GetTexture()
        LFDQueueFrameBackground:SetTexture("Interface\\LFGFrame\\UI-LFG-BACKGROUND-DUNGEONWALL")
    elseif stockBackground then
        LFDQueueFrameBackground:SetTexture(stockBackground)
        stockBackground = nil
    end
end

local function CreateTab(index, text)
    local tab = CreateFrame("Button", "LFDParentFrameTab" .. index, LFDParentFrame, "CharacterFrameTabButtonTemplate")
    tab:SetID(index)
    tab:SetText(text)
    tab:SetScript("OnShow", function(self)
        PanelTemplates_TabResize(self, 0)
    end)
    return tab
end

local dungeonTab = CreateTab(1, TEXT.dungeons)
dungeonTab:SetPoint("CENTER", LFDParentFrame, "BOTTOMLEFT", 60, -12)
local raidTab = CreateTab(2, TEXT.raids)
raidTab:SetPoint("LEFT", dungeonTab, "RIGHT", -16, 0)
-- The Mythic+ tab's content is MythicPlusFrame (MythicPlus.lua, loaded after this file)
local mythicPlusTab = CreateTab(3, TEXT.mythicPlus)
mythicPlusTab:SetPoint("LEFT", raidTab, "RIGHT", -16, 0)

LFDParentFrame.numTabs = 3
PanelTemplates_SetTab(LFDParentFrame, 1)

local stockTitle = LFDQueueFrameTitleText:GetText()
local LayoutPanel, UpdateControls

local function ShowPanel(newMode)
    mode = newMode
    ParkStockWidgets(true, newMode == "mythic" and "LFDQueueFrameTypeDropDown" or nil)
    LFDQueueFrameTitleText:SetText(newMode == "raid" and TEXT.title or stockTitle)
    if newMode == "mythic" then
        UIDropDownMenu_SetText(LFDQueueFrameTypeDropDown, TEXT.mythic)
    end
    LayoutPanel()
    if panel:IsShown() then
        UpdateControls()
    else
        panel:Show()
    end
    Send("LIST")
end

local function HidePanel()
    mode = nil
    ParkStockWidgets(false)
    LFDQueueFrameTitleText:SetText(stockTitle)
    panel:Hide()
    -- The stock type puts its own list (random or specific) and background back
    if LFDQueueFrame_SetType then
        LFDQueueFrame_SetType(LFDQueueFrame.type or "specific")
    end
    if LFDQueueFrame_Update then
        LFDQueueFrame_Update()
    end
end

local selectedTab = 1

-- The Dungeon Finder's own widgets are parked or given back first, and the Mythic+ frame shown last: nothing that
-- goes wrong in it can leave the Dungeons tab without its widgets
local function SelectTab(index)
    selectedTab = index
    PanelTemplates_SetTab(LFDParentFrame, index)
    if MythicPlusFrame and index ~= 3 then
        MythicPlusFrame:Hide()
    end

    if index == 3 then
        mode = nil
        panel:Hide()
        ParkStockWidgets(true)
        LFDQueueFrameTitleText:SetText(TEXT.mythicPlus)
    elseif index == 2 then
        ShowPanel("raid")
    elseif mythicSelected then
        ShowPanel("mythic")
    else
        HidePanel()
    end

    if MythicPlusFrame and index == 3 then
        MythicPlusFrame:Show()
    end
end

dungeonTab:SetScript("OnClick", function()
    PlaySound("igCharacterInfoTab")
    SelectTab(1)
end)
raidTab:SetScript("OnClick", function()
    PlaySound("igCharacterInfoTab")
    SelectTab(2)
end)
mythicPlusTab:SetScript("OnClick", function()
    PlaySound("igCharacterInfoTab")
    SelectTab(3)
end)

LFDParentFrame:HookScript("OnShow", function()
    SelectTab(selectedTab)
end)

-- -----------------------------------------------------------------------------------------------------------------
-- "Mythic Dungeons" in the Dungeon Finder's type menu. Picking a stock type leaves the mythic panel first.
-- -----------------------------------------------------------------------------------------------------------------

local function SelectMythic()
    mythicSelected = true
    UIDropDownMenu_SetSelectedValue(LFDQueueFrameTypeDropDown, MYTHIC_VALUE)
    ShowPanel("mythic")
end

local function LeaveMythic()
    if mythicSelected then
        mythicSelected = false
        if mode == "mythic" then
            HidePanel()
        end
    end
end

local stockTypeInitialize = LFDQueueFrameTypeDropDown_Initialize
function LFDQueueFrameTypeDropDown_Initialize(self, level)
    local stockAddButton = UIDropDownMenu_AddButton
    UIDropDownMenu_AddButton = function(info, buttonLevel)
        if info.func then
            local stockFunc = info.func
            info.func = function(...)
                LeaveMythic()
                return stockFunc(...)
            end
        end
        if mythicSelected then
            info.checked = nil
        end
        return stockAddButton(info, buttonLevel)
    end
    stockTypeInitialize(self, level)
    UIDropDownMenu_AddButton = stockAddButton

    local info = UIDropDownMenu_CreateInfo()
    info.text = TEXT.mythic
    info.value = MYTHIC_VALUE
    info.checked = mythicSelected
    info.func = SelectMythic
    info.tooltipTitle = TEXT.mythic
    info.tooltipText = TEXT.mythicTip
    UIDropDownMenu_AddButton(info, level)
end
UIDropDownMenu_Initialize(LFDQueueFrameTypeDropDown, LFDQueueFrameTypeDropDown_Initialize)

-- The Dungeon Finder resets its menu text when it updates
local function KeepMythicText()
    if mythicSelected then
        UIDropDownMenu_SetText(LFDQueueFrameTypeDropDown, TEXT.mythic)
    end
end
hooksecurefunc("LFDQueueFrame_SetType", KeepMythicText)
LFDQueueFrameTypeDropDown:HookScript("OnShow", KeepMythicText)

-- -----------------------------------------------------------------------------------------------------------------
-- The raid list: the Dungeon Finder's specific list, rows and all
-- -----------------------------------------------------------------------------------------------------------------

local header = panel:CreateFontString(nil, "ARTWORK", "GameFontNormal")
header:SetPoint("TOPLEFT", 30, -132)
header:SetText(TEXT.choose)

local rows = {}
local UpdateList

-- The raids of an expansion that can be picked, and how many of them are
local function ExpansionState(expansion)
    local open, picked = 0, 0
    for _, raid in pairs(raids) do
        if not raid.mythic and raid.expansion == expansion and raid.lock == 0 then
            open = open + 1
            if selected[raid.id] then
                picked = picked + 1
            end
        end
    end
    return open, picked
end

local function OnEnableClick(button)
    local row = button:GetParent()
    local item = row.item
    if not item or IsBusy() then
        return
    end

    if item.header then
        local open, picked = ExpansionState(item.header)
        local pick = picked < open
        for _, raid in pairs(raids) do
            if not raid.mythic and raid.expansion == item.header and raid.lock == 0 then
                selected[raid.id] = pick or nil
            end
        end
    elseif item.lock == 0 then
        selected[item.id] = not selected[item.id] or nil
    end
    PlaySound(button:GetChecked() and "igMainMenuOptionCheckBoxOn" or "igMainMenuOptionCheckBoxOff")
    UpdateList()
end

local function OnExpandClick(button)
    local item = button:GetParent().item
    if item and item.header then
        collapsed[item.header] = not collapsed[item.header] or nil
        UpdateList()
    end
end

local function OnRowEnter(row)
    local raid = row.item
    if not raid or raid.header then
        return
    end
    GameTooltip:SetOwner(row, "ANCHOR_RIGHT")
    GameTooltip:SetText(RaidName(raid), 1, 1, 1)
    GameTooltip:AddLine(format(TEXT.players, raid.size), 1, 0.82, 0)
    if raid.mythic then
        GameTooltip:AddLine(TEXT.mythicTip, 1, 1, 1, true)
    end
    if raid.heroic then
        GameTooltip:AddLine(TEXT.heroic, 1, 0.53, 0)
    end
    if raid.lock > 0 then
        GameTooltip:AddLine(LockText(raid), 1, 0.3, 0.3, true)
    end
    GameTooltip:Show()
end

for index = 1, NUM_ROWS do
    local row = CreateFrame("Frame", "RaidFinderListButton" .. index, panel, "LFGSpecificChoiceTemplate")
    row:EnableMouse(true)
    row:SetScript("OnEnter", OnRowEnter)
    row.enableButton:SetScript("OnClick", OnEnableClick)
    row.expandOrCollapseButton:SetScript("OnClick", OnExpandClick)
    rows[index] = row
end

local scroll = CreateFrame("ScrollFrame", "RaidFinderListScrollFrame", panel, "FauxScrollFrameTemplate")

local scrollTop = scroll:CreateTexture(nil, "BACKGROUND")
scrollTop:SetTexture("Interface\\PaperDollInfoFrame\\UI-Character-ScrollBar")
scrollTop:SetSize(31, 256)
scrollTop:SetPoint("TOPLEFT", scroll, "TOPRIGHT", -2, 5)
scrollTop:SetTexCoord(0, 0.484375, 0, 1)
local scrollBottom = scroll:CreateTexture(nil, "BACKGROUND")
scrollBottom:SetTexture("Interface\\PaperDollInfoFrame\\UI-Character-ScrollBar")
scrollBottom:SetSize(31, 106)
scrollBottom:SetPoint("BOTTOMLEFT", scroll, "BOTTOMRIGHT", -2, -2)
scrollBottom:SetTexCoord(0.515625, 1, 0, 0.4140625)

scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, ROW_HEIGHT, UpdateList)
end)

-- The raids list under its header; the mythic list under the type menu, as the stock dungeon list, with no
-- headers and fewer rows
local MYTHIC_ROWS = 12

local function VisibleRows()
    return mode == "mythic" and MYTHIC_ROWS or NUM_ROWS
end

function LayoutPanel()
    local top = mode == "mythic" and -165 or -150
    for index, row in ipairs(rows) do
        row:ClearAllPoints()
        row:SetPoint("TOPLEFT", LFDQueueFrame, "TOPLEFT", 25, top - (index - 1) * ROW_HEIGHT)
    end
    scroll:ClearAllPoints()
    scroll:SetPoint("TOPLEFT", rows[1], "TOPLEFT", 0, 7)
    scroll:SetPoint("BOTTOMRIGHT", rows[VisibleRows()], "BOTTOMRIGHT", 1, -7)
    if mode == "mythic" then
        header:Hide()
    else
        header:Show()
    end
end
LayoutPanel()

local function BuildDisplay()
    wipe(display)
    local sorted = {}
    for _, raid in pairs(raids) do
        if InMode(raid) then
            raid.name = RaidName(raid)
            tinsert(sorted, raid)
        end
    end

    if mode == "mythic" then
        table.sort(sorted, function(left, right)
            return left.name < right.name
        end)
        for _, raid in ipairs(sorted) do
            tinsert(display, raid)
        end
        return
    end

    table.sort(sorted, function(left, right)
        if left.expansion ~= right.expansion then
            return left.expansion > right.expansion
        elseif left.name ~= right.name then
            return left.name < right.name
        elseif left.size ~= right.size then
            return left.size < right.size
        end
        return not left.heroic and right.heroic
    end)

    local expansion
    for _, raid in ipairs(sorted) do
        if raid.expansion ~= expansion then
            expansion = raid.expansion
            tinsert(display, { header = expansion })
        end
        if not collapsed[expansion] then
            tinsert(display, raid)
        end
    end
end

local function SetHeaderRow(row, expansion)
    row.instanceName:SetText(_G["EXPANSION_NAME" .. expansion] or tostring(expansion))
    row.instanceName:SetFontObject(QuestDifficulty_Header)
    row.instanceName:SetPoint("LEFT", 40, 0)
    row.instanceName:SetPoint("RIGHT", row, "RIGHT", 0, 0)
    row.level:Hide()
    row.heroicIcon:Hide()

    row.expandOrCollapseButton:Show()
    row.expandOrCollapseButton:SetNormalTexture(collapsed[expansion] and "Interface\\Buttons\\UI-PlusButton-UP" or
        "Interface\\Buttons\\UI-MinusButton-UP")

    local open, picked = ExpansionState(expansion)
    if open == 0 then
        row.enableButton:Hide()
        row.lockedIndicator:Show()
    else
        row.enableButton:Show()
        row.lockedIndicator:Hide()
    end
    if picked > 0 and picked < open then
        row.enableButton:SetCheckedTexture("Interface\\Buttons\\UI-MultiCheck-Up")
        row.enableButton:SetDisabledCheckedTexture("Interface\\Buttons\\UI-MultiCheck-Disabled")
    else
        row.enableButton:SetCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check")
        row.enableButton:SetDisabledCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check-Disabled")
    end
    row.enableButton:SetChecked(picked > 0)
end

local function SetRaidRow(row, raid)
    -- The size leads, so a long name that gets cut still shows it; mythic dungeons are all five players
    row.instanceName:SetText(raid.mythic and raid.name or format("%d  %s", raid.size, raid.name))
    if raid.minLevel == raid.maxLevel then
        row.level:SetText(format(LFD_LEVEL_FORMAT_SINGLE or "(%d)", raid.minLevel))
    else
        row.level:SetText(format(LFD_LEVEL_FORMAT_RANGE, raid.minLevel, raid.maxLevel))
    end
    row.level:Show()
    row.instanceName:SetPoint("RIGHT", row.level, "LEFT", -10, 0)
    if raid.heroic then
        row.heroicIcon:Show()
        row.instanceName:SetPoint("LEFT", row.heroicIcon, "RIGHT", 0, 1)
    else
        row.heroicIcon:Hide()
        row.instanceName:SetPoint("LEFT", 40, 0)
    end

    local color = GetQuestDifficultyColor(raid.minLevel)
    row.level:SetFontObject(color.font)
    row.instanceName:SetFontObject(IsBusy() and QuestDifficulty_Header or color.font)
    row.expandOrCollapseButton:Hide()

    if raid.lock > 0 then
        row.enableButton:Hide()
        row.lockedIndicator:Show()
    else
        row.enableButton:Show()
        row.lockedIndicator:Hide()
    end
    row.enableButton:SetCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check")
    row.enableButton:SetDisabledCheckedTexture("Interface\\Buttons\\UI-CheckBox-Check-Disabled")
    row.enableButton:SetChecked(selected[raid.id] and true or false)
end

function UpdateList()
    BuildDisplay()
    local visible = VisibleRows()
    FauxScrollFrame_Update(scroll, #display, visible, ROW_HEIGHT)
    local offset = FauxScrollFrame_GetOffset(scroll)
    local wide = not scroll:IsShown()

    for index, row in ipairs(rows) do
        local item = index <= visible and display[index + offset] or nil
        row.item = item
        if item then
            row:Show()
            row:SetWidth(wide and 315 or 295)
            if item.header then
                SetHeaderRow(row, item.header)
            else
                SetRaidRow(row, item)
            end
            if IsBusy() then
                row.enableButton:Disable()
            else
                row.enableButton:Enable()
            end
        else
            row:Hide()
        end
    end
end

-- -----------------------------------------------------------------------------------------------------------------
-- Bots, status and buttons, where the Dungeon Finder has its own
-- -----------------------------------------------------------------------------------------------------------------

local mainButton = CreateFrame("Button", "RaidFinderMainButton", panel, "UIPanelButtonTemplate2")
mainButton:SetSize(135, 22)
mainButton:SetPoint("BOTTOMLEFT", LFDQueueFrame, "BOTTOMLEFT", 18, 6)

local closeButton = CreateFrame("Button", "RaidFinderCloseButton", panel, "UIPanelButtonTemplate2")
closeButton:SetSize(112, 22)
closeButton:SetPoint("BOTTOMRIGHT", LFDQueueFrame, "BOTTOMRIGHT", -7, 6)
closeButton:SetText(CLOSE)
closeButton:SetScript("OnClick", function()
    HideUIPanel(LFDParentFrame)
end)

local quitButton = CreateFrame("Button", "RaidFinderQuitButton", panel, "UIPanelButtonTemplate2")
quitButton:SetSize(112, 22)
quitButton:SetPoint("BOTTOMRIGHT", LFDQueueFrame, "BOTTOMRIGHT", -7, 6)
quitButton:SetText(TEXT.quit)
quitButton:SetScript("OnClick", function()
    Send("QUIT")
end)

local bots = CreateFrame("CheckButton", "RaidFinderBotsCheckButton", panel, "UICheckButtonTemplate")
bots:SetSize(24, 24)
bots:SetPoint("BOTTOMLEFT", mainButton, "TOPLEFT", 0, 7)
bots:SetChecked(true)
local botsLabel = bots:CreateFontString(nil, "ARTWORK", "GameFontNormal")
botsLabel:SetPoint("LEFT", bots, "RIGHT", 3, 1)
botsLabel:SetText(TEXT.bots)
bots:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    GameTooltip:SetText(TEXT.bots, 1, 0.82, 0)
    GameTooltip:AddLine(TEXT.botsTip, 1, 1, 1, true)
    GameTooltip:Show()
end)
bots:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

-- In the queue, the status takes the place of the bots option
local statusText = panel:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
statusText:SetPoint("BOTTOMLEFT", mainButton, "TOPLEFT", 4, 7)
statusText:SetPoint("RIGHT", LFDQueueFrame, "RIGHT", -12, 0)
statusText:SetJustifyH("LEFT")
statusText:SetSpacing(3)

local function Join()
    local roles = RolesMask()
    if roles == 0 then
        UIErrorsFrame:AddMessage(TEXT.noRole, 1, 0.1, 0.1)
        return
    end

    -- In list order: the server takes the first raid that can be formed
    local ids = {}
    for _, item in ipairs(display) do
        if not item.header and item.lock == 0 and selected[item.id] then
            tinsert(ids, item.id)
        end
    end
    for id in pairs(selected) do
        local raid = raids[id]
        if raid and not raid.mythic and mode ~= "mythic" and raid.lock == 0 and collapsed[raid.expansion] then
            tinsert(ids, id)
        end
    end
    if #ids == 0 then
        UIErrorsFrame:AddMessage(mode == "mythic" and TEXT.noSelectionMythic or TEXT.noSelection, 1, 0.1, 0.1)
        return
    end

    Send(format("JOIN\t%d\t%d\t%s", roles, bots:GetChecked() and 1 or 0, table.concat(ids, ",")))
end

mainButton:SetScript("OnClick", function()
    if status.state == STATE_NONE then
        Join()
    elseif status.state == STATE_RUNNING then
        Send("ENTER")
    else
        Send("LEAVE")
    end
end)

function UpdateControls()
    local raid = raids[status.raid]

    if status.state == STATE_NONE then
        mainButton:SetText(mode == "mythic" and TEXT.findMythic or TEXT.find)
        statusText:SetText("")
        bots:Show()
    else
        bots:Hide()
        if status.state == STATE_RUNNING then
            mainButton:SetText(Text("enter", raid))
            statusText:SetText(format(Text("running", raid), RaidName(raid)))
        elseif status.state == STATE_READY then
            mainButton:SetText(LEAVE_QUEUE)
            statusText:SetText(format(Text("ready", raid), RaidName(raid)))
        else
            mainButton:SetText(LEAVE_QUEUE)
            local waited = queuedSince and floor(GetTime() - queuedSince) or 0
            statusText:SetText(format(TEXT.queued, RaidName(raid), format("%d:%02d", floor(waited / 60), waited % 60))
                .. "\n" .. format(TEXT.roles, status.tanks, status.needTanks, status.healers, status.needHealers,
                    status.damage, status.needDamage))
        end
    end

    if status.state == STATE_RUNNING then
        closeButton:Hide()
        quitButton:SetText(Text("quit", raid))
        quitButton:Show()
    else
        quitButton:Hide()
        closeButton:Show()
    end
    UpdateList()
end

panel:SetScript("OnShow", UpdateControls)

local elapsed = 0
panel:SetScript("OnUpdate", function(_, delta)
    elapsed = elapsed + delta
    if elapsed >= 1 then
        elapsed = 0
        if status.state == STATE_QUEUED then
            UpdateControls()
        end
    end
end)

-- -----------------------------------------------------------------------------------------------------------------
-- Ready check
-- -----------------------------------------------------------------------------------------------------------------

StaticPopupDialogs["RAID_FINDER_READY"] = {
    text = "%s",
    button1 = ENTER_DUNGEON,
    button2 = LEAVE_QUEUE,
    OnAccept = function()
        PlaySound("LFG_RoleCheck")
        Send("ACCEPT")
    end,
    OnCancel = function(_, _, reason)
        if reason == "clicked" then
            PlaySound("LFG_Denied")
            Send("DECLINE")
        end
    end,
    timeout = 90,
    whileDead = 1,
    hideOnEscape = false,
}

-- -----------------------------------------------------------------------------------------------------------------
-- Messages from the server
-- -----------------------------------------------------------------------------------------------------------------

local function ReadList(body, reset)
    if reset then
        wipe(raids)
    end
    for item in string.gmatch(body or "", "[^;]+") do
        local id, dungeon, size, heroic, minLevel, maxLevel, expansion, lock, value, mythic = strsplit(":", item)
        id = tonumber(id)
        if id then
            raids[id] = {
                id = id,
                dungeon = tonumber(dungeon),
                size = tonumber(size) or 0,
                heroic = heroic == "1",
                minLevel = tonumber(minLevel) or 0,
                maxLevel = tonumber(maxLevel) or 0,
                expansion = tonumber(expansion) or 0,
                lock = tonumber(lock) or 0,
                value = tonumber(value) or 0,
                mythic = mythic == "1",
            }
            if raids[id].lock > 0 then
                selected[id] = nil
            end
        end
    end
    UpdateControls()
end

local function ReadStatus(state, raid, counts, accepted, level)
    local previous = status.state
    status.state = tonumber(state) or STATE_NONE
    status.raid = tonumber(raid) or 0
    status.accepted = accepted == "1"
    -- A Mythic+ run has its own ready check and sounds (MythicPlus.lua)
    local mythicPlus = (tonumber(level) or -1) > 0
    local tanks, needTanks, healers, needHealers, damage, needDamage =
        string.match(counts or "", "(%d+):(%d+),(%d+):(%d+),(%d+):(%d+)")
    status.tanks, status.needTanks = tonumber(tanks) or 0, tonumber(needTanks) or 0
    status.healers, status.needHealers = tonumber(healers) or 0, tonumber(needHealers) or 0
    status.damage, status.needDamage = tonumber(damage) or 0, tonumber(needDamage) or 0

    if status.state == STATE_QUEUED and previous ~= STATE_QUEUED and previous ~= STATE_READY then
        queuedSince = GetTime()
    elseif status.state == STATE_NONE or status.state == STATE_RUNNING then
        queuedSince = nil
    end

    -- The Dungeon Finder's sounds: in the queue, raid started, out of the queue
    if status.state ~= previous and not mythicPlus then
        if status.state == STATE_QUEUED and previous == STATE_NONE then
            PlaySound("PVPENTERQUEUE")
        elseif status.state == STATE_RUNNING and previous ~= STATE_NONE then
            PlaySound("PVPTHROUGHQUEUE")
        elseif status.state == STATE_NONE and (previous == STATE_QUEUED or previous == STATE_READY) then
            PlaySound("LFG_Denied")
        end
    end

    if status.state == STATE_READY and not status.accepted and not mythicPlus then
        if not StaticPopup_Visible("RAID_FINDER_READY") then
            PlaySound("ReadyCheck")
            local raid = raids[status.raid]
            StaticPopup_Show("RAID_FINDER_READY", format(Text("ready", raid), RaidName(raid)))
        end
    else
        StaticPopup_Hide("RAID_FINDER_READY")
    end

    UpdateControls()
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "PLAYER_ENTERING_WORLD" then
        Send("LIST")
        return
    end

    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    local kind, first, second, third, fourth, fifth = strsplit("\t", message)
    if kind == "L" or kind == "l" then
        ReadList(first, kind == "L")
    elseif kind == "S" then
        ReadStatus(first, second, third, fourth, fifth)
    elseif kind == "Q" then
        DEFAULT_CHAT_FRAME:AddMessage(TEXT.partyQueued, 1, 0.82, 0)
        Send("ROLES\t" .. RolesMask())
    elseif kind == "M" then
        local text = TEXT.messages[tonumber(first) or 0]
        if text then
            UIErrorsFrame:AddMessage(text, 1, 0.3, 0.3)
            DEFAULT_CHAT_FRAME:AddMessage("|cffff8844" .. TEXT.title .. ":|r " .. text)
        end
    end
end)
