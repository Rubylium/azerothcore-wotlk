-- Mythic+: the Dungeon Finder's third tab (key, score, best run per dungeon, key activation), then the epic parts:
-- the keystone ready check, the countdown, the dungeon timer and the results. The server side is mod-playerbots,
-- Script/MythicPlus.cpp (keys, scores, timer) and Script/RaidFinder.cpp (queue, group, bots), which document the
-- addon messages (prefix "RaidFinder", whispered to oneself). The tab itself is created by RaidFinder.lua.
-- Art and sounds are retail's Challenge Mode ones (localTools/interface/extractRetailUi.py).

local PREFIX = "RaidFinder"
local ROLE_TANK, ROLE_HEALER, ROLE_DAMAGE = 2, 4, 8
local STATE_NONE, STATE_QUEUED, STATE_READY, STATE_RUNNING = 0, 1, 2, 3
local MYTHIC_ID_BASE = 100000
local SOUND = "Sound\\Interface\\MythicPlus\\"
local KEYSTONE_ICON = "Interface\\Icons\\INV_Relics_Hourglass"
local FONT = "Fonts\\FRIZQT__.TTF"
local COLUMNS = 4

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Mythique+",
    key = "Clé mythique",
    score = "Score Mythique+",
    random = "Donjon : aléatoire",
    chosen = "Donjon : %s",
    chooseTip = "Cliquez sur un donjon pour le choisir, ou laissez vide pour un donjon au hasard.",
    activate = "Activer la clé",
    enter = "Rejoindre le donjon",
    quit = "Quitter le donjon",
    never = "Jamais terminé",
    best = "Meilleur : +%d",
    inTime = "dans les temps",
    overTime = "hors délai",
    bestTime = "Temps : %s",
    limit = "Temps imparti : %s",
    dungeonScore = "Score : %d",
    queued = "Préparation du groupe...\n%s",
    readyWait = "En attente des autres joueurs...",
    running = "Clé en cours : %s +%d",
    busy = "Vous êtes déjà en file pour un raid ou un donjon mythique.",
    roles = "|cff66bbffTank|r %d/%d   |cff66ff66Soins|r %d/%d   |cffff6666Dégâts|r %d/%d",
    noRole = "Choisissez au moins un rôle.",
    proposal = "Votre clé est prête",
    proposalText = "Entrer dans %s ?",
    accept = "Entrer",
    decline = "Refuser",
    go = "C'est parti !",
    deaths = "%d |4mort:morts; (+%d s)",
    timed = "Mythique +%d réussi !",
    overTimeTitle = "Mythique +%d terminé hors délai",
    upgrade = "Clé améliorée de %d |4niveau:niveaux; : +%d",
    depleted = "Clé épuisée : +%d",
    record = "Nouveau record !",
    time = "Temps : %s / %s",
    scoreLine = "Score Mythique+ : %s",
    abandoned = "La clé a été abandonnée.",
    startKey = "Lancer la clé",
    autoStart = "Départ automatique dans %s",
    waitingPlayers = "En attente des joueurs...",
    barrierTip = "Préparez-vous dans la barrière : elle tombe quand la clé est lancée.",
    tabTip = "Votre clé fixe le niveau du donjon. Terminez-le dans le temps imparti pour l'améliorer ; "
        .. "le butin et les essences tombent sur le dernier boss.",
} or {
    title = "Mythic+",
    key = "Mythic Keystone",
    score = "Mythic+ Rating",
    random = "Dungeon: random",
    chosen = "Dungeon: %s",
    chooseTip = "Click a dungeon to pick it, or leave none picked for a random one.",
    activate = "Activate Keystone",
    enter = "Enter the dungeon",
    quit = "Leave the dungeon",
    never = "Never completed",
    best = "Best: +%d",
    inTime = "in time",
    overTime = "over time",
    bestTime = "Time: %s",
    limit = "Time limit: %s",
    dungeonScore = "Rating: %d",
    queued = "Assembling the group...\n%s",
    readyWait = "Waiting for the other players...",
    running = "Keystone under way: %s +%d",
    busy = "You are already queued for a raid or a mythic dungeon.",
    roles = "|cff66bbffTank|r %d/%d   |cff66ff66Heal|r %d/%d   |cffff6666Damage|r %d/%d",
    noRole = "Choose at least one role.",
    proposal = "Your keystone is ready",
    proposalText = "Enter %s?",
    accept = "Enter",
    decline = "Decline",
    go = "Go!",
    deaths = "%d |4death:deaths; (+%d s)",
    timed = "Mythic +%d completed!",
    overTimeTitle = "Mythic +%d completed over time",
    upgrade = "Keystone upgraded by %d |4level:levels;: +%d",
    depleted = "Keystone depleted: +%d",
    record = "New record!",
    time = "Time: %s / %s",
    scoreLine = "Mythic+ Rating: %s",
    abandoned = "The keystone was abandoned.",
    startKey = "Start keystone",
    autoStart = "Starts by itself in %s",
    waitingPlayers = "Waiting for the players...",
    barrierTip = "Get ready inside the barrier: it falls when the keystone is started.",
    tabTip = "Your keystone sets the dungeon's level. Complete it in time to upgrade it; loot and essences drop "
        .. "from the last boss.",
}

local key, score = 2, 0
-- dungeon id -> { level, timed, duration, limit, score }, in the server's order
local bests, order = {}, {}
local chosen
local status = { state = STATE_NONE, raid = 0, level = -1, accepted = false }
local timer = { active = false }

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

local function SetAtlas(texture, name, width, height)
    RetailUI.SetAtlas(texture, name)
    if width then
        texture:SetWidth(width)
        texture:SetHeight(height or width)
    end
end

local function DungeonName(dungeonId)
    return (dungeonId and GetLFGDungeonInfo(dungeonId)) or ""
end

local function ShortDungeonName(dungeonId)
    local name = DungeonName(dungeonId)
    return string.match(name, " %- (.+)$") or name
end

-- The Dungeon Finder's own art of the dungeon, as its queue frame shows it
local function DungeonBackground(dungeonId)
    local textureName = dungeonId and select(10, GetLFGDungeonInfo(dungeonId))
    if textureName and textureName ~= "" then
        return "Interface\\LFGFrame\\UI-LFG-BACKGROUND-" .. textureName
    end
    return "Interface\\LFGFrame\\UI-LFG-BACKGROUND-Dungeon"
end

local function DungeonIcon(dungeonId)
    local textureName = dungeonId and select(10, GetLFGDungeonInfo(dungeonId))
    if textureName and textureName ~= "" then
        return "Interface\\LFGFrame\\LFGIcon-" .. textureName
    end
    return "Interface\\LFGFrame\\LFGIcon-Dungeon"
end

local function FormatTime(seconds)
    seconds = max(0, floor(seconds or 0))
    return format("%d:%02d", floor(seconds / 60), seconds % 60)
end

local function RolesMask()
    local _, tank, healer, damage = GetLFGRoles()
    return (tank and ROLE_TANK or 0) + (healer and ROLE_HEALER or 0) + (damage and ROLE_DAMAGE or 0)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Colors: the rating goes grey, white, green, blue, purple, orange then pink as it climbs; key levels follow the
-- same scale
-- -----------------------------------------------------------------------------------------------------------------

local RATING_COLORS = {
    { 0, 0.62, 0.62, 0.62 },
    { 200, 1, 1, 1 },
    { 450, 0.12, 1, 0 },
    { 750, 0, 0.44, 0.87 },
    { 1100, 0.64, 0.21, 0.93 },
    { 1500, 1, 0.5, 0 },
    { 2000, 1, 0.3, 0.55 },
}

local function RatingColor(rating)
    local previous = RATING_COLORS[1]
    for _, stop in ipairs(RATING_COLORS) do
        if rating <= stop[1] then
            local span = stop[1] - previous[1]
            local t = span > 0 and (rating - previous[1]) / span or 1
            return previous[2] + (stop[2] - previous[2]) * t, previous[3] + (stop[3] - previous[3]) * t,
                previous[4] + (stop[4] - previous[4]) * t
        end
        previous = stop
    end
    return previous[2], previous[3], previous[4]
end

local function LevelColor(level)
    return RatingColor((level or 0) * 80)
end

local function Colored(text, r, g, b)
    return format("|cff%02x%02x%02x%s|r", r * 255, g * 255, b * 255, text)
end

-- Spins a square atlas piece by rewriting its texture coordinates (angle in radians)
local function SetRotation(texture, atlas, angle)
    local info = RetailUIAtlas[atlas]
    local half = info[5] / 2
    local cosine, sine = math.cos(angle), math.sin(angle)
    local function corner(x, y)
        return half + (x * cosine - y * sine) * half, half + (x * sine + y * cosine) * half
    end
    local ulx, uly = corner(-1, -1)
    local llx, lly = corner(-1, 1)
    local urx, ury = corner(1, -1)
    local lrx, lry = corner(1, 1)
    texture:SetTexCoord(ulx, uly, llx, lly, urx, ury, lrx, lry)
end

-- -----------------------------------------------------------------------------------------------------------------
-- The tab
-- -----------------------------------------------------------------------------------------------------------------

local frame = CreateFrame("Frame", "MythicPlusFrame", LFDQueueFrame)
frame:SetPoint("TOPLEFT", LFDQueueFrame, "TOPLEFT", 20, -122)
frame:SetPoint("BOTTOMRIGHT", LFDQueueFrame, "BOTTOMRIGHT", -8, 34)
frame:SetFrameLevel(LFDQueueFrame:GetFrameLevel() + 6)
frame:EnableMouse(true)
frame:Hide()

local background = frame:CreateTexture(nil, "BACKGROUND")
SetAtlas(background, "ChallengeMode-MainTabBg")
background:SetAllPoints(frame)

-- The keystone: slot, hourglass, glow and runes that spin up when it is activated
local slot = CreateFrame("Frame", nil, frame)
slot:SetWidth(64)
slot:SetHeight(64)
slot:SetPoint("TOPLEFT", frame, "TOPLEFT", 6, -6)

local slotRunes = slot:CreateTexture(nil, "BACKGROUND")
SetAtlas(slotRunes, "ChallengeMode-Runes-Large", 72)
slotRunes:SetPoint("CENTER")
slotRunes:SetBlendMode("ADD")
slotRunes:SetAlpha(0)
local slotBackground = slot:CreateTexture(nil, "BORDER")
SetAtlas(slotBackground, "ChallengeMode-KeystoneSlotBG", 56)
slotBackground:SetPoint("CENTER")
local slotIcon = slot:CreateTexture(nil, "ARTWORK")
slotIcon:SetTexture(KEYSTONE_ICON)
slotIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
slotIcon:SetWidth(36)
slotIcon:SetHeight(36)
slotIcon:SetPoint("CENTER")
local slotFrame = slot:CreateTexture(nil, "OVERLAY")
SetAtlas(slotFrame, "ChallengeMode-KeystoneSlotFrame", 64)
slotFrame:SetPoint("CENTER")
local slotGlow = slot:CreateTexture(nil, "OVERLAY")
SetAtlas(slotGlow, "ChallengeMode-KeystoneSlotFrameGlow", 64)
slotGlow:SetPoint("CENTER")
slotGlow:SetBlendMode("ADD")
slotGlow:SetAlpha(0)

local keyLabel = frame:CreateFontString(nil, "ARTWORK", "GameFontNormal")
keyLabel:SetPoint("TOPLEFT", slot, "TOPRIGHT", 4, -10)
keyLabel:SetText(TEXT.key)
local keyText = frame:CreateFontString(nil, "ARTWORK")
keyText:SetFont(FONT, 26, "OUTLINE")
keyText:SetPoint("TOPLEFT", keyLabel, "BOTTOMLEFT", 0, -3)

local scoreLabel = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
scoreLabel:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -10, -14)
scoreLabel:SetText(TEXT.score)
local scoreText = frame:CreateFontString(nil, "ARTWORK")
scoreText:SetFont(FONT, 22, "OUTLINE")
scoreText:SetPoint("TOPRIGHT", scoreLabel, "BOTTOMRIGHT", 0, -4)

local divider = frame:CreateTexture(nil, "ARTWORK")
SetAtlas(divider, "ChallengeMode-ThinDivider")
divider:SetHeight(3)
divider:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -76)
divider:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -8, -76)

-- The dungeons: icon, best level and name, in the retail frame; the picked one glows
local tiles = {}
local UpdateTab

local function OnTileEnter(tile)
    local dungeonId = tile.dungeon
    local best = bests[dungeonId]
    GameTooltip:SetOwner(tile, "ANCHOR_RIGHT")
    GameTooltip:SetText(DungeonName(dungeonId), 1, 1, 1)
    if best and best.level > 0 then
        local r, g, b = LevelColor(best.level)
        if not best.timed then
            r, g, b = 0.6, 0.6, 0.6
        end
        GameTooltip:AddLine(format(TEXT.best, best.level) .. " (" .. (best.timed and TEXT.inTime or TEXT.overTime)
            .. ")", r, g, b)
        GameTooltip:AddLine(format(TEXT.bestTime, FormatTime(best.duration)), 1, 1, 1)
        GameTooltip:AddLine(format(TEXT.dungeonScore, best.score), RatingColor(best.score * 8))
    else
        GameTooltip:AddLine(TEXT.never, 0.6, 0.6, 0.6)
    end
    if best then
        GameTooltip:AddLine(format(TEXT.limit, FormatTime(best.limit)), 1, 0.82, 0)
    end
    GameTooltip:AddLine(TEXT.chooseTip, 0.8, 0.8, 0.8, true)
    GameTooltip:Show()
end

local function OnTileClick(tile)
    if status.state ~= STATE_NONE then
        return
    end
    chosen = chosen ~= tile.dungeon and tile.dungeon or nil
    PlaySound(chosen and "igMainMenuOptionCheckBoxOn" or "igMainMenuOptionCheckBoxOff")
    UpdateTab()
end

local function CreateTile(index)
    local tile = CreateFrame("Button", nil, frame)
    tile:SetWidth(74)
    tile:SetHeight(72)
    local column, row = (index - 1) % COLUMNS, floor((index - 1) / COLUMNS)
    tile:SetPoint("TOPLEFT", frame, "TOPLEFT", 6 + column * 78, -84 - row * 74)

    tile.glow = tile:CreateTexture(nil, "BACKGROUND")
    SetAtlas(tile.glow, "ChallengeMode-SoftYellowGlow", 70)
    tile.glow:SetPoint("CENTER", tile, "TOP", 0, -22)
    tile.glow:SetBlendMode("ADD")
    tile.icon = tile:CreateTexture(nil, "ARTWORK")
    tile.icon:SetWidth(38)
    tile.icon:SetHeight(38)
    tile.icon:SetPoint("CENTER", tile, "TOP", 0, -22)
    tile.border = tile:CreateTexture(nil, "OVERLAY")
    SetAtlas(tile.border, "ChallengeMode-DungeonIconFrame", 48)
    tile.border:SetPoint("CENTER", tile.icon, "CENTER")
    tile.level = tile:CreateFontString(nil, "OVERLAY")
    tile.level:SetFont(FONT, 15, "OUTLINE")
    tile.level:SetPoint("CENTER", tile.icon, "BOTTOM", 0, 2)
    tile.name = tile:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    tile.name:SetWidth(76)
    tile.name:SetHeight(24)
    tile.name:SetPoint("TOP", tile.icon, "BOTTOM", 0, -7)
    tile.name:SetJustifyV("TOP")

    local highlight = tile:CreateTexture(nil, "HIGHLIGHT")
    SetAtlas(highlight, "ChallengeMode-KeystoneSlotFrameGlow", 54)
    highlight:SetPoint("CENTER", tile.icon, "CENTER")
    highlight:SetBlendMode("ADD")

    tile:SetScript("OnEnter", OnTileEnter)
    tile:SetScript("OnLeave", function() GameTooltip:Hide() end)
    tile:SetScript("OnClick", OnTileClick)
    tiles[index] = tile
    return tile
end

local choiceText = frame:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
choiceText:SetPoint("TOP", frame, "TOP", 0, -236)

local statusText = frame:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
statusText:SetPoint("TOP", frame, "TOP", 0, -232)
statusText:SetWidth(300)
statusText:SetJustifyH("CENTER")
statusText:SetSpacing(3)

local mainButton = CreateFrame("Button", "MythicPlusMainButton", frame, "UIPanelButtonTemplate2")
mainButton:SetWidth(135)
mainButton:SetHeight(22)
mainButton:SetPoint("BOTTOMLEFT", LFDQueueFrame, "BOTTOMLEFT", 18, 6)

local closeButton = CreateFrame("Button", "MythicPlusCloseButton", frame, "UIPanelButtonTemplate2")
closeButton:SetWidth(112)
closeButton:SetHeight(22)
closeButton:SetPoint("BOTTOMRIGHT", LFDQueueFrame, "BOTTOMRIGHT", -7, 6)

-- The keystone slot lights up and its runes spin while the key is being activated
local activation = 0
slot:SetScript("OnUpdate", function(_, elapsed)
    if activation <= 0 then
        return
    end
    activation = max(0, activation - elapsed)
    local strength = min(1, activation)
    slotGlow:SetAlpha(strength)
    slotRunes:SetAlpha(strength * 0.9)
    SetRotation(slotRunes, "ChallengeMode-Runes-Large", GetTime() * 1.5)
end)

local function Activate()
    local roles = RolesMask()
    if roles == 0 then
        UIErrorsFrame:AddMessage(TEXT.noRole, 1, 0.1, 0.1)
        return
    end
    activation = 2.5
    PlaySoundFile(SOUND .. "KeystoneInsert.ogg")
    Send(format("JOINPLUS\t%d\t%d", roles, chosen and (MYTHIC_ID_BASE + chosen) or 0))
end

mainButton:SetScript("OnClick", function()
    if status.state == STATE_NONE then
        Activate()
    elseif status.state == STATE_RUNNING then
        Send("ENTER")
    else
        Send("LEAVE")
    end
end)

closeButton:SetScript("OnClick", function()
    if status.state == STATE_RUNNING and status.level > 0 then
        Send("QUIT")
    else
        HideUIPanel(LFDParentFrame)
    end
end)

function UpdateTab()
    local r, g, b = LevelColor(key)
    keyText:SetText(Colored("+" .. key, r, g, b))
    scoreText:SetText(Colored(tostring(score), RatingColor(score)))

    for index, dungeonId in ipairs(order) do
        local tile = tiles[index] or CreateTile(index)
        tile.dungeon = dungeonId
        tile.icon:SetTexture(DungeonIcon(dungeonId))
        tile.name:SetText(ShortDungeonName(dungeonId))
        local best = bests[dungeonId]
        if best and best.level > 0 then
            local lr, lg, lb = LevelColor(best.level)
            if not best.timed then
                lr, lg, lb = 0.6, 0.6, 0.6
            end
            tile.level:SetText(Colored("+" .. best.level, lr, lg, lb))
            tile.icon:SetDesaturated(false)
        else
            tile.level:SetText("")
            tile.icon:SetDesaturated(true)
        end
        if chosen == dungeonId then
            tile.glow:Show()
        else
            tile.glow:Hide()
        end
        tile:Show()
    end
    for index = #order + 1, #tiles do
        tiles[index]:Hide()
    end

    choiceText:SetText(chosen and format(TEXT.chosen, DungeonName(chosen)) or TEXT.random)
    if status.state == STATE_NONE then
        choiceText:Show()
    else
        choiceText:Hide()
    end

    local mythicPlus = status.level and status.level > 0
    local dungeon = status.raid and status.raid > MYTHIC_ID_BASE and status.raid - MYTHIC_ID_BASE or nil
    mainButton:Enable()
    closeButton:SetText(CLOSE)
    if status.state == STATE_NONE then
        mainButton:SetText(TEXT.activate)
        statusText:SetText("")
    elseif not mythicPlus then
        mainButton:SetText(TEXT.activate)
        mainButton:Disable()
        statusText:SetText(TEXT.busy)
    elseif status.state == STATE_RUNNING then
        mainButton:SetText(TEXT.enter)
        closeButton:SetText(TEXT.quit)
        statusText:SetText(format(TEXT.running, DungeonName(dungeon), status.level))
    else
        mainButton:SetText(LEAVE_QUEUE)
        statusText:SetText(status.state == STATE_READY and status.accepted and TEXT.readyWait or
            format(TEXT.queued, status.counts or ""))
    end
end

frame:SetScript("OnShow", function()
    Send("LIST")
    UpdateTab()
end)

-- -----------------------------------------------------------------------------------------------------------------
-- Ready check: the keystone, its runes and the dungeon, with Challenge Mode sounds
-- -----------------------------------------------------------------------------------------------------------------

local proposal = CreateFrame("Frame", "MythicPlusProposalFrame", UIParent)
proposal:SetWidth(360)
proposal:SetHeight(330)
proposal:SetPoint("CENTER", UIParent, "CENTER", 0, 90)
proposal:SetFrameStrata("DIALOG")
proposal:SetToplevel(true)
proposal:EnableMouse(true)
-- A solid ground under everything: retail's panel art fades out towards its top
proposal:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Gold-Border",
    edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
})
proposal:SetBackdropColor(0.04, 0.04, 0.05, 1)
proposal:Hide()
tinsert(UISpecialFrames, "MythicPlusProposalFrame")

local proposalBackground = proposal:CreateTexture(nil, "BACKGROUND")
SetAtlas(proposalBackground, "ChallengeMode-MainTabBg")
proposalBackground:SetPoint("TOPLEFT", 11, -12)
proposalBackground:SetPoint("BOTTOMRIGHT", -12, 11)

local PREVIEW_HEIGHT = 170
local proposalPreview = proposal:CreateTexture(nil, "BORDER")
proposalPreview:SetPoint("TOPLEFT", 11, -12)
proposalPreview:SetPoint("TOPRIGHT", -12, -12)
proposalPreview:SetHeight(PREVIEW_HEIGHT)
proposalPreview:SetTexCoord(0, 0.640625, 0, 0.8)
-- Darker towards the bottom, so the picture melts into the frame
local proposalShade = proposal:CreateTexture(nil, "ARTWORK")
proposalShade:SetTexture(0, 0, 0, 1)
proposalShade:SetPoint("BOTTOMLEFT", proposalPreview, "BOTTOMLEFT")
proposalShade:SetPoint("BOTTOMRIGHT", proposalPreview, "BOTTOMRIGHT")
proposalShade:SetHeight(PREVIEW_HEIGHT * 0.6)
proposalShade:SetGradientAlpha("VERTICAL", 0, 0, 0, 0.9, 0, 0, 0, 0)

local proposalRunes = proposal:CreateTexture(nil, "ARTWORK")
SetAtlas(proposalRunes, "ChallengeMode-Runes-Large", 136)
proposalRunes:SetPoint("CENTER", proposal, "TOP", 0, -104)
proposalRunes:SetBlendMode("ADD")
local proposalGlow = proposal:CreateTexture(nil, "ARTWORK")
SetAtlas(proposalGlow, "ChallengeMode-Runes-GlowLarge", 140)
proposalGlow:SetPoint("CENTER", proposalRunes, "CENTER")
proposalGlow:SetBlendMode("ADD")
local proposalSlotBackground = proposal:CreateTexture(nil, "OVERLAY")
SetAtlas(proposalSlotBackground, "ChallengeMode-KeystoneSlotBG", 62)
proposalSlotBackground:SetPoint("CENTER", proposalRunes, "CENTER")
local proposalIcon = proposal:CreateTexture(nil, "OVERLAY")
proposalIcon:SetTexture(KEYSTONE_ICON)
proposalIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
proposalIcon:SetWidth(38)
proposalIcon:SetHeight(38)
proposalIcon:SetPoint("CENTER", proposalRunes, "CENTER")
local proposalSlot = proposal:CreateTexture(nil, "OVERLAY")
SetAtlas(proposalSlot, "ChallengeMode-KeystoneSlotFrame", 70)
proposalSlot:SetPoint("CENTER", proposalRunes, "CENTER")

local proposalTitle = proposal:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
proposalTitle:SetPoint("TOP", proposal, "TOP", 0, -20)
proposalTitle:SetText(TEXT.proposal)
local proposalLevel = proposal:CreateFontString(nil, "OVERLAY")
proposalLevel:SetFont(FONT, 34, "OUTLINE")
proposalLevel:SetPoint("TOP", proposal, "TOP", 0, -12 - PREVIEW_HEIGHT - 4)
local proposalDungeon = proposal:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
proposalDungeon:SetPoint("TOP", proposalLevel, "BOTTOM", 0, -6)
local proposalText = proposal:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
proposalText:SetPoint("TOP", proposalDungeon, "BOTTOM", 0, -8)
proposalText:SetWidth(300)

local acceptButton = CreateFrame("Button", nil, proposal, "UIPanelButtonTemplate")
acceptButton:SetWidth(120)
acceptButton:SetHeight(24)
acceptButton:SetPoint("BOTTOMRIGHT", proposal, "BOTTOM", -6, 20)
acceptButton:SetText(TEXT.accept)
local declineButton = CreateFrame("Button", nil, proposal, "UIPanelButtonTemplate")
declineButton:SetWidth(120)
declineButton:SetHeight(24)
declineButton:SetPoint("BOTTOMLEFT", proposal, "BOTTOM", 6, 20)
declineButton:SetText(TEXT.decline)

acceptButton:SetScript("OnClick", function()
    PlaySoundFile(SOUND .. "KeystoneInsert.ogg")
    Send("ACCEPT")
    acceptButton:Disable()
    proposalText:SetText(TEXT.readyWait)
end)
declineButton:SetScript("OnClick", function()
    PlaySound("LFG_Denied")
    Send("DECLINE")
    proposal:Hide()
end)

proposal:SetScript("OnUpdate", function()
    local now = GetTime()
    SetRotation(proposalRunes, "ChallengeMode-Runes-Large", now * 0.35)
    proposalGlow:SetAlpha(0.45 + 0.35 * math.sin(now * 3))
end)

local function ShowProposal()
    local dungeon = status.raid - MYTHIC_ID_BASE
    local r, g, b = LevelColor(status.level)
    proposalLevel:SetText(Colored("+" .. status.level, r, g, b))
    proposalDungeon:SetText(DungeonName(dungeon))
    proposalPreview:SetTexture(DungeonBackground(dungeon))
    proposalText:SetText(format(TEXT.proposalText, DungeonName(dungeon)))
    acceptButton:Enable()
    proposal:Show()
    PlaySoundFile(SOUND .. "ChallengeStart.ogg")
    PlaySound("ReadyCheck")
end

-- -----------------------------------------------------------------------------------------------------------------
-- Countdown: the numbers over a spinning star, then the dome opens
-- -----------------------------------------------------------------------------------------------------------------

local countdown = CreateFrame("Frame", "MythicPlusCountdownFrame", UIParent)
countdown:SetWidth(160)
countdown:SetHeight(160)
countdown:SetPoint("CENTER", UIParent, "CENTER", 0, 160)
countdown:SetFrameStrata("HIGH")
countdown:Hide()

local countdownStar = countdown:CreateTexture(nil, "BACKGROUND")
SetAtlas(countdownStar, "ChallengeMode-SpikeyStar", 120)
countdownStar:SetPoint("CENTER")
countdownStar:SetBlendMode("ADD")
local countdownGlow = countdown:CreateTexture(nil, "BORDER")
SetAtlas(countdownGlow, "ChallengeMode-SoftYellowGlow", 130)
countdownGlow:SetPoint("CENTER")
countdownGlow:SetBlendMode("ADD")
local countdownNumber = countdown:CreateFontString(nil, "OVERLAY")
countdownNumber:SetFont(FONT, 44, "THICKOUTLINE")
countdownNumber:SetPoint("CENTER")
countdownNumber:SetTextColor(1, 0.82, 0)
local countdownCaption = countdown:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
countdownCaption:SetPoint("TOP", countdown, "BOTTOM", 0, 10)

local countdownEnd, shownNumber, goUntil = 0, nil, 0

local function PopNumber(text)
    countdownNumber:SetText(text)
    countdown.popAt = GetTime()
end

countdown:SetScript("OnUpdate", function(self)
    local now = GetTime()
    SetRotation(countdownStar, "ChallengeMode-SpikeyStar", now * 0.8)
    local pop = max(0, 1 - (now - (self.popAt or 0)) / 0.3)
    countdownNumber:SetFont(FONT, 44 + 18 * pop, "THICKOUTLINE")

    if goUntil > 0 then
        local left = goUntil - now
        self:SetAlpha(min(1, max(0, left)))
        if left <= 0 then
            goUntil = 0
            self:Hide()
        end
        return
    end

    local remaining = ceil(countdownEnd - now)
    if remaining <= 0 then
        PopNumber(TEXT.go)
        PlaySoundFile(SOUND .. "CountdownEnd.ogg")
        PlaySoundFile(SOUND .. "DomeOpen.ogg")
        goUntil = now + 2
    elseif remaining ~= shownNumber then
        shownNumber = remaining
        PopNumber(tostring(remaining))
        PlaySoundFile(SOUND .. "CountdownTick.ogg")
    end
end)

local function StartCountdown(seconds)
    countdownEnd = GetTime() + seconds
    shownNumber, goUntil = nil, 0
    countdownCaption:SetText(format("%s +%d", DungeonName(timer.dungeon), timer.level))
    countdown:SetAlpha(1)
    countdown:Show()
end

-- -----------------------------------------------------------------------------------------------------------------
-- The timer, above the dungeon tracker: level, dungeon, clock, chest thresholds and deaths
-- -----------------------------------------------------------------------------------------------------------------

local BAR_WIDTH = 200

local timerFrame = CreateFrame("Frame", "MythicPlusTimerFrame", UIParent)
timerFrame:SetWidth(245)
timerFrame:SetHeight(82)
timerFrame:Hide()

local timerBackground = timerFrame:CreateTexture(nil, "BACKGROUND")
SetAtlas(timerBackground, "ChallengeMode-Timer")
timerBackground:SetAllPoints(timerFrame)

local timerLevel = timerFrame:CreateFontString(nil, "OVERLAY")
timerLevel:SetFont(FONT, 15, "OUTLINE")
timerLevel:SetPoint("TOPLEFT", timerFrame, "TOPLEFT", 18, -12)
local timerDungeon = timerFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
timerDungeon:SetPoint("LEFT", timerLevel, "RIGHT", 6, 0)
timerDungeon:SetPoint("RIGHT", timerFrame, "RIGHT", -16, 0)
timerDungeon:SetJustifyH("LEFT")

local timerClock = timerFrame:CreateFontString(nil, "OVERLAY")
timerClock:SetFont(FONT, 20, "OUTLINE")
timerClock:SetPoint("TOPLEFT", timerLevel, "BOTTOMLEFT", 0, -4)
local timerLimit = timerFrame:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
timerLimit:SetPoint("BOTTOMLEFT", timerClock, "BOTTOMRIGHT", 4, 2)
local timerDeaths = timerFrame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
timerDeaths:SetPoint("RIGHT", timerFrame, "RIGHT", -18, 0)
timerDeaths:SetPoint("TOP", timerClock, "TOP", 0, -4)
local deathIcon = timerFrame:CreateTexture(nil, "OVERLAY")
deathIcon:SetTexture("Interface\\TargetingFrame\\UI-TargetingFrame-Skull")
deathIcon:SetWidth(14)
deathIcon:SetHeight(14)
deathIcon:SetPoint("RIGHT", timerDeaths, "LEFT", -2, 0)

local barBackground = timerFrame:CreateTexture(nil, "ARTWORK")
SetAtlas(barBackground, "ChallengeMode-TimerBG", BAR_WIDTH, 10)
barBackground:SetPoint("BOTTOMLEFT", timerFrame, "BOTTOMLEFT", 22, 16)
local barFill = timerFrame:CreateTexture(nil, "OVERLAY")
SetAtlas(barFill, "ChallengeMode-TimerFill", BAR_WIDTH, 10)
barFill:SetPoint("LEFT", barBackground, "LEFT")
local fillInfo = RetailUIAtlas["ChallengeMode-TimerFill"]

-- +3 at 60% of the time, +2 at 80%
local thresholds = {}
for index, share in ipairs({ 0.6, 0.8 }) do
    local tick = timerFrame:CreateTexture(nil, "OVERLAY")
    tick:SetTexture(1, 0.82, 0, 0.9)
    tick:SetWidth(2)
    tick:SetHeight(12)
    tick:SetPoint("LEFT", barBackground, "LEFT", BAR_WIDTH * share - 1, 0)
    local label = timerFrame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    label:SetPoint("BOTTOM", tick, "TOP", 0, 1)
    thresholds[index] = { share = share, upgrade = 4 - index, label = label, tick = tick }
end

-- Before the key is started: the group waits in the barrier; its owner starts it from here
local timerWait = timerFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
timerWait:SetPoint("BOTTOMLEFT", timerFrame, "BOTTOMLEFT", 22, 16)
timerWait:SetPoint("RIGHT", timerFrame, "RIGHT", -18, 0)
timerWait:SetJustifyH("LEFT")
local startButton = CreateFrame("Button", "MythicPlusStartButton", timerFrame, "UIPanelButtonTemplate")
startButton:SetWidth(96)
startButton:SetHeight(20)
startButton:SetPoint("RIGHT", timerFrame, "RIGHT", -16, 0)
startButton:SetPoint("TOP", timerClock, "TOP", 0, 0)
startButton:SetText(TEXT.startKey)
startButton:SetScript("OnClick", function(self)
    PlaySoundFile(SOUND .. "KeystoneInsert.ogg")
    Send("STARTKEY")
    self:Disable()
end)

local function SetBarShown(shown)
    for _, region in ipairs({ barBackground, barFill, timerDeaths, deathIcon }) do
        if shown then region:Show() else region:Hide() end
    end
    for _, threshold in ipairs(thresholds) do
        if shown then
            threshold.tick:Show()
            threshold.label:Show()
        else
            threshold.tick:Hide()
            threshold.label:Hide()
        end
    end
end

local warned, expired = false, false

local function TimerElapsed()
    if timer.finished then
        return timer.elapsed
    end
    if (timer.countdown or -1) ~= 0 then
        return 0
    end
    return timer.elapsed + (GetTime() - timer.receivedAt)
end

timerFrame:SetScript("OnUpdate", function()
    local waiting = not timer.finished and (timer.countdown or -1) < 0
    if waiting then
        SetBarShown(false)
        timerWait:Show()
        timerClock:SetText(FormatTime(timer.limit))
        timerClock:SetTextColor(0.7, 0.7, 0.7)
        if (timer.autoStart or -1) >= 0 then
            timerWait:SetText(format(TEXT.autoStart, FormatTime(timer.autoStart - (GetTime() - timer.receivedAt))))
        else
            timerWait:SetText(TEXT.waitingPlayers)
        end
        if timer.owner then startButton:Show() else startButton:Hide() end
        return
    end
    SetBarShown(true)
    timerWait:Hide()
    startButton:Hide()

    local elapsed = TimerElapsed()
    local limit = max(1, timer.limit or 1)
    local over = elapsed > limit
    timerClock:SetText(FormatTime(elapsed))
    if timer.finished then
        timerClock:SetTextColor(timer.timed and 0.1 or 0.6, timer.timed and 1 or 0.6, timer.timed and 0.1 or 0.6)
    elseif over then
        timerClock:SetTextColor(1, 0.2, 0.2)
    else
        timerClock:SetTextColor(1, 1, 1)
    end

    local share = min(1, elapsed / limit)
    if share <= 0 then
        barFill:Hide()
    else
        barFill:Show()
        barFill:SetWidth(BAR_WIDTH * share)
        barFill:SetTexCoord(fillInfo[4], fillInfo[5] * share, fillInfo[6], fillInfo[7])
        if over then
            barFill:SetVertexColor(1, 0.25, 0.25)
        else
            barFill:SetVertexColor(1, 1, 1)
        end
    end

    for _, threshold in ipairs(thresholds) do
        local left = limit * threshold.share - elapsed
        if left > 0 then
            threshold.label:SetText(format("+%d %s", threshold.upgrade, FormatTime(left)))
            threshold.label:SetTextColor(1, 0.82, 0)
        else
            threshold.label:SetText("+" .. threshold.upgrade)
            threshold.label:SetTextColor(0.5, 0.5, 0.5)
        end
    end

    if not timer.finished and not warned and limit - elapsed <= 60 and limit - elapsed > 0 then
        warned = true
        PlaySoundFile(SOUND .. "TimerWarning.ogg")
    end
    if not timer.finished and not expired and over then
        expired = true
        PlaySoundFile(SOUND .. "TimeExpired.ogg")
    end
end)

-- Where the quest tracker is (DragonUI's or Blizzard's, see DungeonTracker.lua), which it hides meanwhile
local function UpdateTimerShown()
    local show = timer.active and IsInInstance()
    if show then
        DungeonTracker_AnchorToQuestArea(timerFrame)
        timerFrame:Show()
    else
        timerFrame:Hide()
    end
    DungeonTracker_ClaimQuestArea("mythicplus", show)
    DungeonTracker_UpdateAnchor()
end

local function RefreshTimer()
    local r, g, b = LevelColor(timer.level)
    timerLevel:SetText(Colored("+" .. timer.level, r, g, b))
    timerDungeon:SetText(DungeonName(timer.dungeon))
    timerLimit:SetText("/ " .. FormatTime(timer.limit))
    timerDeaths:SetText(format(TEXT.deaths, timer.deaths or 0, (timer.deaths or 0) * 5))
    UpdateTimerShown()
end

-- -----------------------------------------------------------------------------------------------------------------
-- Results: the star, the chest for a timed key, the new key and rating
-- -----------------------------------------------------------------------------------------------------------------

local result = CreateFrame("Button", "MythicPlusResultFrame", UIParent)
result:SetWidth(420)
result:SetHeight(220)
result:SetPoint("CENTER", UIParent, "CENTER", 0, 150)
result:SetFrameStrata("HIGH")
result:Hide()

local resultGlow = result:CreateTexture(nil, "BACKGROUND")
SetAtlas(resultGlow, "ChallengeMode-SoftYellowGlow", 170)
resultGlow:SetPoint("CENTER", result, "TOP", 0, -60)
resultGlow:SetBlendMode("ADD")
local resultStar = result:CreateTexture(nil, "BORDER")
SetAtlas(resultStar, "ChallengeMode-SpikeyStar", 130)
resultStar:SetPoint("CENTER", resultGlow, "CENTER")
resultStar:SetBlendMode("ADD")
local resultChest = result:CreateTexture(nil, "ARTWORK")
SetAtlas(resultChest, "ChallengeMode-Chest", 96, 71)
resultChest:SetPoint("CENTER", resultGlow, "CENTER")

local resultTitle = result:CreateFontString(nil, "OVERLAY")
resultTitle:SetFont(FONT, 20, "THICKOUTLINE")
resultTitle:SetPoint("TOP", resultGlow, "CENTER", 0, -46)
local resultLines = result:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
resultLines:SetPoint("TOP", resultTitle, "BOTTOM", 0, -8)
resultLines:SetSpacing(4)

local resultUntil = 0
result:SetScript("OnUpdate", function(self)
    local now = GetTime()
    SetRotation(resultStar, "ChallengeMode-SpikeyStar", -now * 0.5)
    resultGlow:SetAlpha(0.6 + 0.3 * math.sin(now * 2.5))
    local left = resultUntil - now
    self:SetAlpha(min(1, max(0, left)))
    if left <= 0 then
        self:Hide()
    end
end)
result:SetScript("OnClick", function(self)
    self:Hide()
end)

local function ShowResult(timed, level, dungeon, elapsed, limit, upgrade, newKey, newScore, record)
    local r, g, b = LevelColor(level)
    if timed then
        resultTitle:SetText(Colored(format(TEXT.timed, level), r, g, b))
        resultChest:Show()
        resultStar:Show()
    else
        resultTitle:SetText(Colored(format(TEXT.overTimeTitle, level), 0.7, 0.7, 0.7))
        resultChest:Hide()
        resultStar:Hide()
    end

    local lines = { DungeonName(dungeon), format(TEXT.time, FormatTime(elapsed), FormatTime(limit)) }
    if timed and upgrade > 0 then
        local kr, kg, kb = LevelColor(newKey)
        tinsert(lines, (format(TEXT.upgrade, upgrade, newKey):gsub("%+%d+$", Colored("+" .. newKey, kr, kg, kb))))
    elseif not timed then
        tinsert(lines, Colored(format(TEXT.depleted, newKey), 1, 0.3, 0.3))
    end
    if record then
        tinsert(lines, Colored(TEXT.record, 1, 0.82, 0))
    end
    tinsert(lines, format(TEXT.scoreLine, Colored(tostring(newScore), RatingColor(newScore))))
    resultLines:SetText(table.concat(lines, "\n"))

    resultUntil = GetTime() + 10
    result:SetAlpha(1)
    result:Show()

    if timed then
        PlaySoundFile(SOUND .. "NewRecord.ogg")
        PlaySound("LEVELUPSOUND")
    else
        PlaySoundFile(SOUND .. "TimeExpired.ogg")
    end
    for _, line in ipairs(lines) do
        DEFAULT_CHAT_FRAME:AddMessage("|cffffd100" .. TEXT.title .. ":|r " .. line)
    end
end

-- -----------------------------------------------------------------------------------------------------------------
-- Messages from the server
-- -----------------------------------------------------------------------------------------------------------------

local function ReadStatus(state, raid, counts, accepted, level)
    local previous = status.state
    status.state = tonumber(state) or STATE_NONE
    status.raid = tonumber(raid) or 0
    status.accepted = accepted == "1"
    status.level = tonumber(level) or -1
    local tanks, needTanks, healers, needHealers, damage, needDamage =
        string.match(counts or "", "(%d+):(%d+),(%d+):(%d+),(%d+):(%d+)")
    status.counts = format(TEXT.roles, tonumber(tanks) or 0, tonumber(needTanks) or 0, tonumber(healers) or 0,
        tonumber(needHealers) or 0, tonumber(damage) or 0, tonumber(needDamage) or 0)

    if status.level > 0 and status.state == STATE_READY and not status.accepted then
        if not proposal:IsShown() then
            ShowProposal()
        end
    elseif proposal:IsShown() and not (status.state == STATE_READY and status.accepted) then
        proposal:Hide()
    end

    if status.level > 0 and status.state == STATE_RUNNING and previous ~= STATE_RUNNING and previous ~= STATE_NONE then
        PlaySoundFile(SOUND .. "DomeOpen.ogg")
    end

    -- Out of every run: no timer any more
    if status.state == STATE_NONE and timer.active and not IsInInstance() then
        timer.active = false
        UpdateTimerShown()
    end
    if frame:IsShown() then
        UpdateTab()
    end
end

local function ReadTimer(level, dungeon, limit, elapsed, deaths, countdownSeconds, owner, autoStart)
    local previousDeaths = timer.deaths
    local wasCounting = timer.active and (timer.countdown or -1) > 0
    timer.active = true
    timer.finished = false
    timer.level = tonumber(level) or 0
    timer.dungeon = tonumber(dungeon)
    timer.limit = tonumber(limit) or 0
    timer.elapsed = tonumber(elapsed) or 0
    timer.deaths = tonumber(deaths) or 0
    local wasWaiting = timer.waiting
    timer.countdown = tonumber(countdownSeconds) or -1
    timer.waiting = timer.countdown < 0
    timer.owner = owner == "1"
    timer.autoStart = tonumber(autoStart) or -1
    if timer.countdown < 0 then
        startButton:Enable()
        if not wasWaiting then
            DEFAULT_CHAT_FRAME:AddMessage("|cffffd100" .. TEXT.title .. ":|r " .. TEXT.barrierTip)
        end
    end
    timer.receivedAt = GetTime()
    if timer.elapsed == 0 then
        warned, expired = false, false
    end

    if timer.countdown > 0 and not wasCounting then
        StartCountdown(timer.countdown)
    end
    if previousDeaths and timer.deaths > previousDeaths then
        PlaySound("RaidWarning")
    end
    RefreshTimer()
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("ZONE_CHANGED_NEW_AREA")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event ~= "CHAT_MSG_ADDON" then
        UpdateTimerShown()
        return
    end
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    local kind, a, b, c, d, e, f, g, h, i = strsplit("\t", message)
    if kind == "K" then
        key, score = tonumber(a) or 2, tonumber(b) or 0
        wipe(order)
        wipe(bests)
    elseif kind == "B" then
        for item in string.gmatch(a or "", "[^;]+") do
            local dungeon, level, timed, duration, limit, dungeonScore = strsplit(":", item)
            dungeon = tonumber(dungeon)
            if dungeon then
                tinsert(order, dungeon)
                bests[dungeon] = { level = tonumber(level) or 0, timed = timed == "1",
                    duration = tonumber(duration) or 0, limit = tonumber(limit) or 0,
                    score = tonumber(dungeonScore) or 0 }
            end
        end
    elseif kind == "S" then
        ReadStatus(a, b, c, d, e)
        return
    elseif kind == "T" then
        ReadTimer(a, b, c, d, e, f, g, h)
        return
    elseif kind == "E" then
        timer.finished = true
        timer.timed = a == "1"
        timer.elapsed = tonumber(e) or 0
        ShowResult(a == "1", tonumber(b) or 0, tonumber(c), tonumber(d) or 0, tonumber(e) or 0, tonumber(f) or 0,
            tonumber(g) or 0, tonumber(h) or 0, i == "1")
        return
    elseif kind == "X" then
        timer.active = false
        countdown:Hide()
        UpdateTimerShown()
        DEFAULT_CHAT_FRAME:AddMessage("|cffffd100" .. TEXT.title .. ":|r " .. TEXT.abandoned)
        return
    else
        return
    end

    if frame:IsShown() then
        UpdateTab()
    end
end)
