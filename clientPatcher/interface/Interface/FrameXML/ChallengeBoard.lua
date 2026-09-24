-- The challenge board (server: mod-playerbots ChallengeBoard.cpp, protocol at the top of it). Using the board in
-- Stormwind's Trade District opens it: four missions that change every 20 minutes, each a challenge against one raid
-- boss, fought at once with bots. A challenge won leaves its reward here: gold and a satchel.
-- A banner at the top of the screen follows the challenge itself, from the group being assembled to the way home.

local PREFIX = "Challenge"
local SOUND = "Sound\\Interface\\MythicPlus\\"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local SATCHEL = 4573
local SATCHEL_ICON = "Interface\\Icons\\INV_Misc_Bag_07"
local ROLE_TANK, ROLE_HEALER, ROLE_DAMAGE = 2, 4, 8

local WIDTH, HEIGHT = 840, 540
local CARD_WIDTH, CARD_HEIGHT, CARD_GAP = 184, 322, 16
local ROTATION_SECONDS = 20 * 60

local STATE_OPEN, STATE_UNDERWAY, STATE_WON, STATE_CLAIMED = 0, 1, 2, 3
-- RaidFinder::ChallengeEvent
local EVENT_ARRIVED, EVENT_KILLED, EVENT_RETURNING, EVENT_WIPED, EVENT_WON, EVENT_FAILED = 1, 2, 3, 4, 5, 6
local EVENT_PULLING = 7

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Tableau des défis",
    heading = "Missions",
    intro = "Chaque défi vous mène droit devant un boss de raid : des compagnons complètent votre groupe, "
        .. "vous l'affrontez sur-le-champ, et la récompense vous attend ici.",
    refresh = "Nouvelles missions dans",
    challenge = "DÉFI",
    players = "%d joueurs · %s",
    normal = "Normal",
    heroic = "Héroïque",
    heroicTag = "HÉROÏQUE",
    itemLevel = "Butin de niveau d'objet %d",
    paragon = "+%d Parangon",
    essences = "+%d essences",
    rewards = "Récompenses",
    take = "Relever le défi",
    claim = "Récupérer",
    underway = "En cours",
    claimed = "Accomplie",
    role = "Votre rôle",
    quit = "Abandonner le défi",
    waiting = "Récompenses en attente",
    locked = "Les missions s'ouvrent au niveau 60.\nRevenez plus tard, aventurier.",
    empty = "Aucune mission pour le moment.",
    satchel = "Sacoche du défi",
    satchelHint = "De l'or, et peut-être un butin de plus du boss.",
    accepted = "Défi accepté",
    gold = "or",
    assembling = "Constitution du groupe",
    ready = "Le groupe est prêt : en route !",
    travel = "En route vers %s…",
    fight = "Affrontez %s !",
    killed = "%s est vaincu ! Tirages du butin…",
    returning = "Retour au tableau dans %d",
    pulling = "Le tank engage le combat dans %d",
    wiped = "Le groupe est tombé. Tentatives restantes : %d",
    won = "Défi réussi ! Votre récompense vous attend au tableau.",
    failed = {
        [1] = "Défi échoué : trop de tentatives.",
        [2] = "Défi échoué : le temps est écoulé.",
        [3] = "Défi annulé : le boss est introuvable.",
    },
    errors = {
        [1] = "Approchez-vous du tableau.",
        [2] = "Les missions s'ouvrent au niveau 60.",
        [3] = "Cette mission n'est plus au tableau.",
        [4] = "Mission déjà accomplie sur ce tableau.",
        [5] = "Vous êtes déjà dans un défi, un raid ou une file d'attente.",
        [6] = "Seul le chef du groupe peut relever un défi.",
        [7] = "Impossible pour le moment (champ de bataille, recherche de donjon…).",
        [8] = "Choisissez au moins un rôle.",
        [9] = "Votre groupe est trop nombreux pour ce défi.",
        [10] = "Ce défi n'est pas disponible.",
        [11] = "Aucune récompense à récupérer.",
        [12] = "Vos sacs sont pleins.",
        [13] = "Un membre du groupe n'a pas le niveau requis.",
    },
} or {
    title = "Challenge Board",
    heading = "Missions",
    intro = "Each challenge takes you straight before a raid boss: companions fill your group, you fight it at "
        .. "once, and the reward waits for you here.",
    refresh = "New missions in",
    challenge = "CHALLENGE",
    players = "%d players · %s",
    normal = "Normal",
    heroic = "Heroic",
    heroicTag = "HEROIC",
    itemLevel = "Drops item level %d",
    paragon = "+%d Paragon",
    essences = "+%d essences",
    rewards = "Rewards",
    take = "Take the challenge",
    claim = "Claim",
    underway = "Under way",
    claimed = "Completed",
    role = "Your role",
    quit = "Abandon the challenge",
    waiting = "Rewards waiting",
    locked = "Missions open at level 60.\nCome back later, adventurer.",
    empty = "No missions for now.",
    satchel = "Challenge Satchel",
    satchelHint = "Gold, and maybe one more of the boss's spoils.",
    accepted = "Challenge accepted",
    gold = "gold",
    assembling = "Assembling the group",
    ready = "The group is ready: on your way!",
    travel = "On your way to %s…",
    fight = "Face %s!",
    killed = "%s is defeated! Rolling for loot…",
    returning = "Back to the board in %d",
    pulling = "The tank pulls in %d",
    wiped = "The group fell. Attempts left: %d",
    won = "Challenge won! Your reward waits at the board.",
    failed = {
        [1] = "Challenge failed: too many attempts.",
        [2] = "Challenge failed: time ran out.",
        [3] = "Challenge cancelled: the boss could not be found.",
    },
    errors = {
        [1] = "Get closer to the board.",
        [2] = "Missions open at level 60.",
        [3] = "This mission is no longer on the board.",
        [4] = "Mission already completed on this board.",
        [5] = "You are already in a challenge, a raid or a queue.",
        [6] = "Only the group leader can take a challenge up.",
        [7] = "Not possible right now (battleground, Dungeon Finder…).",
        [8] = "Pick at least one role.",
        [9] = "Your group is too large for this challenge.",
        [10] = "This challenge is not available.",
        [11] = "No reward to claim.",
        [12] = "Your bags are full.",
        [13] = "A group member is below the required level.",
    },
}

local state = {
    rotation = 0,
    left = 0,
    receivedAt = 0,
    bracket = 0,
    challenge = 0,
    missions = {},
    rewards = {},
    shownRotation = nil,
}

local frame, cards, rewardRows, emptyText, timerText, timerFill, errorText, quitButton, roleButtons
local banner
local incoming = { missions = {}, rewards = {} }

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function DungeonName(dungeonId)
    return (dungeonId and dungeonId > 0 and GetLFGDungeonInfo(dungeonId)) or ""
end

-- Raids whose Dungeon Finder entry has no art at all (Onyxia's Lair, entries 46 and 257: its texture name is empty):
-- art shipped with the patch, laid out like the Dungeon Finder's own (Interface\LFGFrame)
local BACKGROUND_BY_DUNGEON = {
    [46] = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-ONYXIASLAIR",
    [257] = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-ONYXIASLAIR",
}
local BACKGROUND_FALLBACK = "Interface\\LFGFrame\\UI-LFG-BACKGROUND-GENERICDUNGEON"

local function DungeonTexture(dungeonId, kind)
    local textureName = dungeonId and dungeonId > 0 and select(10, GetLFGDungeonInfo(dungeonId))
    if textureName and textureName ~= "" then
        return "Interface\\LFGFrame\\" .. kind .. textureName
    end
    return "Interface\\LFGFrame\\" .. kind .. "Raid"
end

-- The raid's art; a raid without any gets a generic piece rather than an empty card
local function SetBackground(texture, dungeonId)
    local textureName = dungeonId and dungeonId > 0 and select(10, GetLFGDungeonInfo(dungeonId))
    if BACKGROUND_BY_DUNGEON[dungeonId] then
        texture:SetTexture(BACKGROUND_BY_DUNGEON[dungeonId])
    elseif textureName and textureName ~= "" then
        texture:SetTexture(DungeonTexture(dungeonId, "UI-LFG-BACKGROUND-"))
    else
        texture:SetTexture(BACKGROUND_FALLBACK)
    end
end

local function FormatTime(seconds)
    seconds = max(0, floor(seconds or 0))
    return format("%d:%02d", floor(seconds / 60), seconds % 60)
end

local function Money(copper)
    return GetCoinTextureString(copper or 0)
end

-- 3.3.5 has neither SetShown nor SetEnabled
local function SetShown(region, shown)
    if shown then
        region:Show()
    else
        region:Hide()
    end
end

local function SetEnabled(button, enabled)
    if enabled then
        button:Enable()
    else
        button:Disable()
    end
end

local function SetAtlas(texture, name)
    local atlas = RetailUIAtlas[name]
    texture:SetTexture(atlas[1])
    texture:SetTexCoord(atlas[4], atlas[5], atlas[6], atlas[7])
end

-- Tweens: every animation of the board and the banner, driven by one clock ---------------------------------------

local tweens = {}
local driver = CreateFrame("Frame")

local function Tween(duration, delay, update, done)
    tinsert(tweens, { time = -(delay or 0), duration = duration, update = update, done = done })
end

local function OutCubic(p)
    local inverse = 1 - p
    return 1 - inverse * inverse * inverse
end

local function OutBack(p)
    local c = 1.70158
    local q = p - 1
    return 1 + (c + 1) * q * q * q + c * q * q
end

driver:SetScript("OnUpdate", function(_, elapsed)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        tween.time = tween.time + elapsed
        if tween.time >= 0 then
            local progress = min(1, tween.time / tween.duration)
            tween.update(progress)
            if progress >= 1 then
                tremove(tweens, index)
                if tween.done then
                    tween.done()
                end
            end
        end
    end
end)

-- Roles -------------------------------------------------------------------------------------------------------------

local function RolesMask()
    local _, tank, healer, damage = GetLFGRoles()
    return (tank and ROLE_TANK or 0) + (healer and ROLE_HEALER or 0) + (damage and ROLE_DAMAGE or 0)
end

local function RefreshRoles()
    if not roleButtons then
        return
    end

    local _, tank, healer, damage = GetLFGRoles()
    local chosen = { TANK = tank, HEALER = healer, DAMAGER = damage }
    local canTank, canHeal, canDamage = true, true, true
    if GetAvailableRoles then
        canTank, canHeal, canDamage = GetAvailableRoles()
    end
    local available = { TANK = canTank, HEALER = canHeal, DAMAGER = canDamage }

    for role, button in pairs(roleButtons) do
        local on = chosen[role] and available[role]
        button.icon:SetDesaturated(not on)
        button.icon:SetAlpha(available[role] and (on and 1 or 0.45) or 0.15)
        SetShown(button.ring, on and true or false)
        SetEnabled(button, available[role] and true or false)
    end
end

local function ToggleRole(role)
    local leader, tank, healer, damage = GetLFGRoles()
    if role == "TANK" then
        tank = not tank
    elseif role == "HEALER" then
        healer = not healer
    else
        damage = not damage
    end
    SetLFGRoles(leader, tank, healer, damage)
    PlaySound(RolesMask() > 0 and "igMainMenuOptionCheckBoxOn" or "igMainMenuOptionCheckBoxOff")
    RefreshRoles()
end

-- Messages under the cards ------------------------------------------------------------------------------------------

local function ShowError(code)
    local text = TEXT.errors[tonumber(code)] or TEXT.errors[10]
    PlaySound("igQuestFailed")
    UIErrorsFrame:AddMessage(text, 1, 0.1, 0.1, 1)
    if errorText then
        errorText:SetText(text)
        errorText:SetAlpha(1)
        Tween(0.6, 3, function(p) errorText:SetAlpha(1 - p) end)
    end
end

-- Cards -------------------------------------------------------------------------------------------------------------

local function CardTooltipSatchel(owner)
    GameTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. SATCHEL)
    GameTooltip:AddLine(TEXT.satchelHint, 0.2, 1, 0.2, true)
    GameTooltip:Show()
end

local function CreateCard(index)
    local card = CreateFrame("Frame", nil, frame)
    card:SetSize(CARD_WIDTH, CARD_HEIGHT)
    card:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    card:SetBackdropColor(0.04, 0.03, 0.02, 0.92)
    card:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)
    card:EnableMouse(true)

    -- A reward waiting: a warm glow breathes behind the card
    local glow = frame:CreateTexture(nil, "BORDER")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("TOPLEFT", card, "TOPLEFT", -34, 34)
    glow:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", 34, -34)
    glow:Hide()
    card.glow = glow

    local art = card:CreateTexture(nil, "ARTWORK")
    art:SetPoint("TOPLEFT", 5, -5)
    art:SetPoint("TOPRIGHT", -5, -5)
    art:SetHeight(110)
    art:SetTexCoord(0, 0.640625, 0, 0.8)
    card.art = art

    local shade = card:CreateTexture(nil, "ARTWORK", nil, 1)
    shade:SetTexture("Interface\\Buttons\\WHITE8X8")
    shade:SetPoint("BOTTOMLEFT", art, "BOTTOMLEFT")
    shade:SetPoint("BOTTOMRIGHT", art, "BOTTOMRIGHT")
    shade:SetHeight(56)
    shade:SetGradientAlpha("VERTICAL", 0.04, 0.03, 0.02, 1, 0.04, 0.03, 0.02, 0)

    local kind = card:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    kind:SetPoint("TOPLEFT", art, "TOPLEFT", 8, -7)
    kind:SetText(TEXT.challenge)
    kind:SetTextColor(1, 0.82, 0.3)
    kind:SetShadowOffset(1, -1)
    card.kind = kind

    -- The raid's icon, straddling the bottom of the art in a thin gold frame
    local iconFrame = CreateFrame("Frame", nil, card)
    iconFrame:SetSize(44, 44)
    iconFrame:SetPoint("CENTER", art, "BOTTOMLEFT", 30, 2)
    iconFrame:SetBackdrop({
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 10,
    })
    iconFrame:SetBackdropBorderColor(0.85, 0.7, 0.4, 1)
    local icon = iconFrame:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 3, -3)
    icon:SetPoint("BOTTOMRIGHT", -3, 3)
    icon:SetTexCoord(0.06, 0.94, 0.06, 0.94)
    card.icon = icon

    local name = card:CreateFontString(nil, "OVERLAY")
    name:SetFont(MORPHEUS, 17)
    name:SetShadowOffset(1, -1)
    name:SetTextColor(1, 0.9, 0.7)
    name:SetPoint("TOPLEFT", art, "BOTTOMLEFT", 8, -24)
    name:SetPoint("TOPRIGHT", art, "BOTTOMRIGHT", -8, -24)
    name:SetJustifyH("LEFT")
    name:SetHeight(38)
    name:SetJustifyV("TOP")
    card.name = name

    local raid = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    raid:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -2)
    raid:SetPoint("TOPRIGHT", name, "BOTTOMRIGHT", 0, -2)
    raid:SetJustifyH("LEFT")
    raid:SetTextColor(0.75, 0.7, 0.6)
    card.raid = raid

    local size = card:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    size:SetPoint("TOPLEFT", raid, "BOTTOMLEFT", 0, -3)
    size:SetJustifyH("LEFT")
    card.size = size

    -- What the boss drops in this mode, in the epic colour, as one more line about the mission
    local itemLevel = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    itemLevel:SetPoint("TOPLEFT", size, "BOTTOMLEFT", 0, -3)
    itemLevel:SetJustifyH("LEFT")
    itemLevel:SetTextColor(0.75, 0.45, 1)
    card.itemLevel = itemLevel

    local divider = card:CreateTexture(nil, "ARTWORK")
    SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetHeight(10)
    divider:SetPoint("TOPLEFT", itemLevel, "BOTTOMLEFT", -6, -6)
    divider:SetPoint("RIGHT", card, "RIGHT", -8, 0)

    local rewardLabel = card:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    rewardLabel:SetPoint("TOPLEFT", divider, "BOTTOMLEFT", 6, -4)
    rewardLabel:SetText(TEXT.rewards)

    local gold = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    gold:SetPoint("TOPLEFT", rewardLabel, "BOTTOMLEFT", 0, -8)
    card.gold = gold

    -- Paragon points, when the mission carries some: between the gold and the satchel, in the paragon purple
    local paragon = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    paragon:SetPoint("RIGHT", card, "RIGHT", -52, 0)
    paragon:SetPoint("TOP", gold, "TOP", 0, -1)
    paragon:SetJustifyH("RIGHT")
    paragon:SetTextColor(0.64, 0.21, 0.93)
    card.paragon = paragon

    -- Essences, under the paragon points, in the essences' green
    local essences = card:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    essences:SetPoint("TOPRIGHT", paragon, "BOTTOMRIGHT", 0, -2)
    essences:SetJustifyH("RIGHT")
    essences:SetTextColor(0.3, 1, 0.45)
    card.essences = essences

    local satchel = CreateFrame("Button", nil, card)
    satchel:SetSize(30, 30)
    satchel:SetPoint("RIGHT", card, "RIGHT", -14, 0)
    satchel:SetPoint("TOP", rewardLabel, "BOTTOM", 0, -2)
    local satchelIcon = satchel:CreateTexture(nil, "ARTWORK")
    satchelIcon:SetAllPoints()
    satchelIcon:SetTexture(SATCHEL_ICON)
    local satchelBorder = satchel:CreateTexture(nil, "OVERLAY")
    satchelBorder:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    satchelBorder:SetBlendMode("ADD")
    satchelBorder:SetVertexColor(0.64, 0.21, 0.93)
    satchelBorder:SetPoint("TOPLEFT", -13, 13)
    satchelBorder:SetPoint("BOTTOMRIGHT", 13, -13)
    satchel:SetScript("OnEnter", CardTooltipSatchel)
    satchel:SetScript("OnLeave", function() GameTooltip:Hide() end)
    card.satchel = satchel
    card.satchelIcon = satchelIcon

    local button = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    button:SetSize(CARD_WIDTH - 28, 24)
    button:SetPoint("BOTTOM", card, "BOTTOM", 0, 14)
    card.button = button

    local status = card:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    status:SetPoint("BOTTOM", card, "BOTTOM", 0, 20)
    card.status = status

    local check = card:CreateTexture(nil, "OVERLAY", nil, 3)
    SetAtlas(check, "ui-questtracker-tracker-check-2x")
    check:SetSize(46, 46)
    check:SetPoint("CENTER", art, "CENTER", 0, 0)
    check:Hide()
    card.check = check

    -- Hovering an open mission lights the card
    local highlight = card:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    highlight:SetBlendMode("ADD")
    highlight:SetPoint("TOPLEFT", 4, -4)
    highlight:SetPoint("BOTTOMRIGHT", -4, 4)
    highlight:SetAlpha(0.18)

    card:SetScript("OnEnter", function(self)
        if self.mission and self.mission.state == STATE_OPEN then
            PlaySound("GAMESCREENSMALLBUTTONMOUSEOVER")
        end
    end)

    -- The stamp shown when a challenge is accepted, and the reward popping out when it is claimed
    local stamp = card:CreateFontString(nil, "OVERLAY")
    stamp:SetFont(MORPHEUS, 22, "OUTLINE")
    stamp:SetTextColor(1, 0.82, 0.2)
    stamp:SetPoint("CENTER", art, "CENTER", 0, 0)
    stamp:SetAlpha(0)
    card.stamp = stamp

    local burst = card:CreateTexture(nil, "OVERLAY", nil, 4)
    SetAtlas(burst, "ChallengeMode-SpikeyStar")
    burst:SetBlendMode("ADD")
    burst:SetPoint("CENTER", satchel, "CENTER")
    burst:SetAlpha(0)
    card.burst = burst

    local popIcon = card:CreateTexture(nil, "OVERLAY", nil, 5)
    popIcon:SetTexture(SATCHEL_ICON)
    popIcon:SetPoint("CENTER", satchel, "CENTER")
    popIcon:SetAlpha(0)
    card.popIcon = popIcon

    local popText = card:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    popText:SetPoint("BOTTOM", satchel, "TOP", 0, 6)
    popText:SetAlpha(0)
    card.popText = popText

    card.index = index
    card:Hide()
    return card
end

local function LayoutCards(count)
    local total = count * CARD_WIDTH + max(0, count - 1) * CARD_GAP
    local left = (WIDTH - total) / 2
    for index, card in ipairs(cards) do
        card.x = left + (index - 1) * (CARD_WIDTH + CARD_GAP)
        card.y = -118
        card:ClearAllPoints()
        card:SetPoint("TOPLEFT", frame, "TOPLEFT", card.x, card.y)
    end
end

local function FillCard(card, mission)
    card.mission = mission
    SetBackground(card.art, mission.dungeon)
    card.icon:SetTexture(DungeonTexture(mission.dungeon, "LFGIcon-"))
    card.name:SetText(mission.name)
    card.raid:SetText(DungeonName(mission.dungeon))
    local heroic = mission.difficulty >= 2
    card.size:SetText(format(TEXT.players, mission.players, heroic and TEXT.heroic or TEXT.normal))
    card.kind:SetText(heroic and (TEXT.challenge .. "  |cffff8000" .. TEXT.heroicTag .. "|r") or TEXT.challenge)
    card.itemLevel:SetText(mission.itemLevel > 0 and format(TEXT.itemLevel, mission.itemLevel) or "")
    card.gold:SetText(Money(mission.gold))
    card.paragon:SetText(mission.paragon > 0 and format(TEXT.paragon, mission.paragon) or "")
    card.essences:SetText(mission.essences > 0 and format(TEXT.essences, mission.essences) or "")

    local done = mission.state == STATE_CLAIMED
    card.art:SetDesaturated(done)
    card.icon:SetDesaturated(done)
    SetShown(card.check, done)
    SetShown(card.glow, mission.state == STATE_WON)
    card:SetBackdropBorderColor(unpack(mission.state == STATE_WON and { 1, 0.82, 0.25, 1 } or
        mission.state == STATE_UNDERWAY and { 0.35, 0.65, 1, 1 } or { 0.75, 0.6, 0.35, 1 }))

    local button = card.button
    button:SetScript("OnClick", nil)
    card.status:SetText("")
    if mission.state == STATE_OPEN then
        button:Show()
        button:SetText(TEXT.take)
        SetEnabled(button, state.challenge == 0)
        button:SetScript("OnClick", function()
            local roles = RolesMask()
            if roles == 0 then
                return ShowError(8)
            end
            PlaySound("igMainMenuOptionCheckBoxOn")
            state.starting = mission.boss
            Send("START\t" .. mission.boss .. "\t" .. roles)
        end)
    elseif mission.state == STATE_WON then
        button:Show()
        button:SetText(TEXT.claim)
        SetEnabled(button, true)
        button:SetScript("OnClick", function()
            Send("CLAIM\t" .. state.rotation .. "\t" .. mission.boss)
        end)
    else
        button:Hide()
        card.status:SetText(mission.state == STATE_UNDERWAY and TEXT.underway or TEXT.claimed)
        if mission.state == STATE_UNDERWAY then
            card.status:SetTextColor(0.45, 0.75, 1)
        else
            card.status:SetTextColor(0.3, 1, 0.3)
        end
    end
end

-- Cards drop in one after the other: each falls a little and fades in
local function AnimateCardsIn()
    for index, card in ipairs(cards) do
        if card.mission then
            card:SetAlpha(0)
            Tween(0.42, (index - 1) * 0.08, function(p)
                local eased = OutBack(p)
                card:SetAlpha(min(1, p * 1.6))
                card:SetPoint("TOPLEFT", frame, "TOPLEFT", card.x, card.y + 26 * (1 - eased))
            end)
        end
    end
end

local function Refresh(animate)
    if not frame then
        return
    end

    local shown = min(#state.missions, #cards)
    LayoutCards(max(shown, 1))
    for index, card in ipairs(cards) do
        local mission = state.missions[index]
        if mission then
            FillCard(card, mission)
            card:Show()
        else
            card.mission = nil
            card:Hide()
        end
    end

    if state.bracket == 0 then
        emptyText:SetText(TEXT.locked)
        emptyText:Show()
    elseif shown == 0 then
        emptyText:SetText(TEXT.empty)
        emptyText:Show()
    else
        emptyText:Hide()
    end

    -- Rewards won on another board
    for index, row in ipairs(rewardRows) do
        local reward = state.rewards[index]
        if reward then
            row.icon:SetTexture(DungeonTexture(reward.dungeon, "LFGIcon-"))
            row.text:SetText(reward.name .. "  " .. Money(reward.gold) .. (reward.paragon > 0 and
                ("  |cffa335ee" .. format(TEXT.paragon, reward.paragon) .. "|r") or "") .. (reward.essences > 0 and
                ("  |cff4dff73" .. format(TEXT.essences, reward.essences) .. "|r") or ""))
            row.button:SetScript("OnClick", function()
                Send("CLAIM\t" .. reward.rotation .. "\t" .. reward.boss)
            end)
            row:Show()
        else
            row:Hide()
        end
    end
    SetShown(rewardRows.label, #state.rewards > 0)

    SetShown(quitButton, state.challenge ~= 0)
    RefreshRoles()

    if animate then
        AnimateCardsIn()
    end
end

-- The window --------------------------------------------------------------------------------------------------------

local function CreateBoard()
    frame = CreateFrame("Frame", "ChallengeBoardFrame", UIParent)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("CENTER", 0, 20)
    frame:SetFrameStrata("HIGH")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:Hide()

    local ground = frame:CreateTexture(nil, "BACKGROUND")
    ground:SetTexture(RetailUIFiles["ui-background-rock"], true)
    ground:SetHorizTile(true)
    ground:SetVertTile(true)
    ground:SetPoint("TOPLEFT", 2, -21)
    ground:SetPoint("BOTTOMRIGHT", -2, 2)

    local parchment = frame:CreateTexture(nil, "BORDER")
    SetAtlas(parchment, "questbg-parchment")
    parchment:SetPoint("TOPLEFT", 8, -26)
    parchment:SetPoint("BOTTOMRIGHT", -8, 8)
    parchment:SetVertexColor(0.34, 0.28, 0.22)

    RetailUI.ApplyNineSlice(frame, false)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -4)
    title:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -26, -4)
    title:SetJustifyH("CENTER")
    title:SetText(TEXT.title)
    title:SetTextColor(1, 0.82, 0)

    local closeButton = CreateFrame("Button", nil, frame)
    closeButton:SetSize(24, 24)
    closeButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -5, -5)
    closeButton:SetFrameLevel(frame:GetFrameLevel() + 20)
    closeButton:SetNormalTexture(RetailUIAtlas["redbutton-exit-2x"][1])
    RetailUI.SetAtlas(closeButton:GetNormalTexture(), "redbutton-exit-2x")
    closeButton:SetPushedTexture(RetailUIAtlas["redbutton-exit-pressed-2x"][1])
    RetailUI.SetAtlas(closeButton:GetPushedTexture(), "redbutton-exit-pressed-2x")
    closeButton:SetHighlightTexture(RetailUIAtlas["redbutton-highlight-2x"][1])
    RetailUI.SetAtlas(closeButton:GetHighlightTexture(), "redbutton-highlight-2x")
    closeButton:GetHighlightTexture():SetBlendMode("ADD")
    closeButton:SetScript("OnClick", function() frame:Hide() end)

    local mover = CreateFrame("Frame", nil, frame)
    mover:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -40, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(frame:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)

    local heading = frame:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 28)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(1, 0.86, 0.55)
    heading:SetPoint("TOPLEFT", frame, "TOPLEFT", 34, -40)
    heading:SetText(TEXT.heading)

    local intro = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    intro:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 2, -4)
    intro:SetWidth(470)
    intro:SetJustifyH("LEFT")
    intro:SetTextColor(0.85, 0.8, 0.7)
    intro:SetText(TEXT.intro)

    -- The board's clock: a watch, the time left before the missions change beside it, and a bar running down with it
    -- underneath both. The watch has a column of its own: the label used to run under it.
    local timerIcon = frame:CreateTexture(nil, "OVERLAY")
    timerIcon:SetTexture("Interface\\Icons\\INV_Misc_PocketWatch_01")
    timerIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    timerIcon:SetSize(38, 38)
    timerIcon:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -196, -42)

    local timerIconBorder = frame:CreateTexture(nil, "OVERLAY", nil, 1)
    timerIconBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    timerIconBorder:SetPoint("TOPLEFT", timerIcon, "TOPLEFT", -12, 12)
    timerIconBorder:SetPoint("BOTTOMRIGHT", timerIcon, "BOTTOMRIGHT", 12, -12)

    local timerLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    timerLabel:SetPoint("TOPLEFT", timerIcon, "TOPRIGHT", 10, 0)
    timerLabel:SetJustifyH("LEFT")
    timerLabel:SetText(TEXT.refresh)

    timerText = frame:CreateFontString(nil, "OVERLAY")
    timerText:SetFont(MORPHEUS, 22)
    timerText:SetShadowOffset(1, -1)
    timerText:SetTextColor(1, 1, 1)
    timerText:SetPoint("TOPLEFT", timerLabel, "BOTTOMLEFT", 0, -1)

    local timerBar = CreateFrame("Frame", nil, frame)
    timerBar:SetSize(202, 12)
    timerBar:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -32, -86)
    timerBar:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 8,
        insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    timerBar:SetBackdropColor(0, 0, 0, 0.65)
    timerBar:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)

    timerFill = timerBar:CreateTexture(nil, "ARTWORK")
    timerFill:SetTexture("Interface\\Buttons\\WHITE8X8")
    timerFill:SetGradient("HORIZONTAL", 0.75, 0.45, 0.1, 1, 0.85, 0.35)
    timerFill:SetHeight(6)
    timerFill:SetPoint("LEFT", timerBar, "LEFT", 3, 0)
    timerFill.full = 196

    local divider = frame:CreateTexture(nil, "ARTWORK")
    SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetHeight(12)
    divider:SetPoint("TOPLEFT", frame, "TOPLEFT", 24, -100)
    divider:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -24, -100)

    cards = {}
    for index = 1, 4 do
        cards[index] = CreateCard(index)
    end

    emptyText = frame:CreateFontString(nil, "OVERLAY")
    emptyText:SetFont(MORPHEUS, 20)
    emptyText:SetTextColor(0.9, 0.8, 0.6)
    emptyText:SetPoint("CENTER", frame, "CENTER", 0, 10)
    emptyText:Hide()

    -- Footer: the role the challenge takes you in, and the rewards waiting from other boards
    local roleLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    roleLabel:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 30, 54)
    roleLabel:SetText(TEXT.role)

    roleButtons = {}
    local previous
    for _, role in ipairs({ "TANK", "HEALER", "DAMAGER" }) do
        local button = CreateFrame("Button", nil, frame)
        button:SetSize(38, 38)
        if previous then
            button:SetPoint("LEFT", previous, "RIGHT", 8, 0)
        else
            button:SetPoint("TOPLEFT", roleLabel, "BOTTOMLEFT", -2, -4)
        end
        local ring = button:CreateTexture(nil, "BACKGROUND")
        SetAtlas(ring, "ChallengeMode-SoftYellowGlow")
        ring:SetBlendMode("ADD")
        ring:SetPoint("TOPLEFT", -10, 10)
        ring:SetPoint("BOTTOMRIGHT", 10, -10)
        button.ring = ring
        local icon = button:CreateTexture(nil, "ARTWORK")
        icon:SetAllPoints()
        icon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-ROLES")
        icon:SetTexCoord(GetTexCoordsForRole(role))
        button.icon = icon
        button:SetScript("OnClick", function() ToggleRole(role) end)
        button:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_TOP")
            GameTooltip:SetText(_G[role] or role)
            GameTooltip:Show()
        end)
        button:SetScript("OnLeave", function() GameTooltip:Hide() end)
        roleButtons[role] = button
        previous = button
    end

    quitButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    quitButton:SetSize(170, 24)
    quitButton:SetPoint("LEFT", previous, "RIGHT", 24, 0)
    quitButton:SetText(TEXT.quit)
    quitButton:SetScript("OnClick", function()
        PlaySound("igQuestLogAbandonQuest")
        Send("QUIT")
    end)
    quitButton:Hide()

    errorText = frame:CreateFontString(nil, "OVERLAY", "GameFontRedSmall")
    errorText:SetPoint("BOTTOM", frame, "BOTTOM", 0, 20)
    errorText:SetAlpha(0)

    rewardRows = {}
    local rewardLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    rewardLabel:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -30, 76)
    rewardLabel:SetText(TEXT.waiting)
    rewardRows.label = rewardLabel
    for index = 1, 3 do
        local row = CreateFrame("Frame", nil, frame)
        row:SetSize(300, 22)
        row:SetPoint("TOPRIGHT", rewardLabel, "BOTTOMRIGHT", 0, -2 - (index - 1) * 22)
        local button = CreateFrame("Button", nil, row, "UIPanelButtonTemplate")
        button:SetSize(90, 20)
        button:SetPoint("RIGHT")
        button:SetText(TEXT.claim)
        row.button = button
        local icon = row:CreateTexture(nil, "ARTWORK")
        icon:SetSize(18, 18)
        icon:SetPoint("RIGHT", button, "LEFT", -6, 0)
        row.icon = icon
        local text = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        text:SetPoint("RIGHT", icon, "LEFT", -6, 0)
        text:SetJustifyH("RIGHT")
        row.text = text
        row:Hide()
        rewardRows[index] = row
    end

    -- The clock ticks, the waiting rewards breathe, and a new board is asked for when the time runs out
    local clock = 0
    frame:SetScript("OnUpdate", function(_, elapsed)
        clock = clock + elapsed
        local left = state.left - (GetTime() - state.receivedAt)
        timerText:SetText(FormatTime(left))
        timerFill:SetWidth(max(1, timerFill.full * max(0, left) / ROTATION_SECONDS))
        if left <= 0 and not state.asked then
            state.asked = true
            Send("OPEN")
        end

        local pulse = 0.5 + 0.5 * math.sin(clock * 3)
        for _, card in ipairs(cards) do
            if card.glow:IsShown() then
                card.glow:SetAlpha(0.35 + 0.45 * pulse)
            end
        end
    end)

    frame:SetScript("OnShow", function()
        PlaySound("igQuestListOpen")
        frame:SetAlpha(0)
        frame:SetScale(0.94)
        Tween(0.22, 0, function(p)
            frame:SetAlpha(p)
            frame:SetScale(0.94 + 0.06 * OutCubic(p))
        end)
    end)
    frame:SetScript("OnHide", function()
        PlaySound("igQuestListClose")
        GameTooltip:Hide()
    end)

    tinsert(UISpecialFrames, "ChallengeBoardFrame")
end

local function ShowBoard(open)
    if not frame then
        CreateBoard()
    end

    local newRotation = state.shownRotation ~= state.rotation
    local wasShown = frame:IsShown()
    if open and not wasShown then
        state.shownRotation = state.rotation
        frame:Show()
        Refresh(true)
    elseif wasShown then
        -- A new board while it is open: the old cards leave, the new ones drop in
        if newRotation then
            state.shownRotation = state.rotation
            PlaySound("igCharacterInfoTab")
        end
        Refresh(newRotation)
    end
end

-- Claiming: the satchel jumps out of its card in a burst of light, and the gold with it
local function AnimateClaim(boss, gold, paragon, essences)
    PlaySound("igQuestListComplete")
    PlaySound("LOOTWINDOWCOINSOUND")
    if not frame or not frame:IsShown() then
        return
    end

    for _, card in ipairs(cards) do
        if card.mission and card.mission.boss == boss then
            card.popText:SetText("+" .. Money(gold) .. (paragon > 0 and
                ("\n|cffa335ee" .. format(TEXT.paragon, paragon) .. "|r") or "") .. ((essences or 0) > 0 and
                ("\n|cff4dff73" .. format(TEXT.essences, essences) .. "|r") or ""))
            Tween(0.9, 0, function(p)
                local grow = OutBack(min(1, p * 1.6))
                card.popIcon:SetSize(30 + 34 * grow, 30 + 34 * grow)
                card.popIcon:SetAlpha(p < 0.6 and 1 or (1 - p) / 0.4)
                card.burst:SetSize(40 + 140 * p, 40 + 140 * p)
                card.burst:SetAlpha(0.9 * (1 - p))
                card.popText:SetAlpha(p < 0.7 and 1 or (1 - p) / 0.3)
                card.popText:SetPoint("BOTTOM", card.satchel, "TOP", 0, 6 + 40 * OutCubic(p))
            end)
        end
    end
end

local function AnimateAccepted(boss)
    PlaySound("WriteQuest")
    if not frame or not frame:IsShown() then
        return
    end

    for _, card in ipairs(cards) do
        if card.mission and card.mission.boss == boss then
            card.stamp:SetText(TEXT.accepted)
            Tween(0.35, 0, function(p)
                card.stamp:SetAlpha(p)
                card.stamp:SetFont(MORPHEUS, 22 + 20 * (1 - OutCubic(p)), "OUTLINE")
            end)
            Tween(0.6, 1.4, function(p) card.stamp:SetAlpha(1 - p) end)
        end
    end
end

-- The banner: the challenge under way, at the top of the screen ----------------------------------------------------

local function CreateBanner()
    banner = CreateFrame("Frame", "ChallengeBanner", UIParent)
    banner:SetSize(460, 64)
    banner:SetPoint("TOP", UIParent, "TOP", 0, -120)
    banner:SetFrameStrata("MEDIUM")
    banner:Hide()

    local back = banner:CreateTexture(nil, "BACKGROUND")
    back:SetTexture("Interface\\Buttons\\WHITE8X8")
    back:SetAllPoints()
    back:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.8)
    local backRight = banner:CreateTexture(nil, "BACKGROUND")
    backRight:SetTexture("Interface\\Buttons\\WHITE8X8")
    backRight:SetPoint("TOPLEFT", banner, "TOP")
    backRight:SetPoint("BOTTOMRIGHT")
    backRight:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.8, 0, 0, 0, 0)
    back:SetPoint("BOTTOMRIGHT", banner, "BOTTOM")

    for _, edge in ipairs({ "TOP", "BOTTOM" }) do
        local line = banner:CreateTexture(nil, "ARTWORK")
        SetAtlas(line, "ChallengeMode-ThinDivider")
        line:SetHeight(10)
        line:SetPoint(edge .. "LEFT", banner, edge .. "LEFT", 0, edge == "TOP" and 4 or -4)
        line:SetPoint(edge .. "RIGHT", banner, edge .. "RIGHT", 0, edge == "TOP" and 4 or -4)
    end

    local icon = banner:CreateTexture(nil, "ARTWORK")
    icon:SetSize(40, 40)
    icon:SetPoint("LEFT", banner, "LEFT", 70, 0)
    banner.icon = icon

    local title = banner:CreateFontString(nil, "OVERLAY")
    title:SetFont(MORPHEUS, 20)
    title:SetShadowOffset(1, -1)
    title:SetTextColor(1, 0.86, 0.5)
    title:SetPoint("TOPLEFT", icon, "TOPRIGHT", 12, 2)
    title:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    title:SetJustifyH("LEFT")
    banner.title = title

    local text = banner:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    text:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -3)
    text:SetPoint("RIGHT", banner, "RIGHT", -60, 0)
    text:SetJustifyH("LEFT")
    banner.text = text

    banner:SetScript("OnUpdate", function(self)
        if self.countdownEnd then
            local left = math.ceil(self.countdownEnd - GetTime())
            if left ~= self.countdownShown and left >= 0 then
                self.countdownShown = left
                self.text:SetText(format(self.countdownFormat or TEXT.returning, left))
                if left <= 3 and left > 0 then
                    PlaySoundFile(SOUND .. "CountdownTick.ogg")
                end
            end
        end
        if self.hideAt and GetTime() >= self.hideAt then
            self.hideAt = nil
            Tween(0.5, 0, function(p) self:SetAlpha(1 - p) end, function() self:Hide() end)
        end
    end)
end

local function FindMission(boss)
    for _, mission in ipairs(state.missions) do
        if mission.boss == boss then
            return mission
        end
    end
end

-- Every line shows for this long, then the banner fades: it only speaks when something happens
local BANNER_SECONDS = 5

local function ShowBanner(title, text, dungeon, hideAfter)
    if not banner then
        CreateBanner()
    end

    banner.title:SetText(title or "")
    banner.text:SetText(text or "")
    banner.countdownEnd = nil
    if dungeon and dungeon > 0 then
        banner.icon:SetTexture(DungeonTexture(dungeon, "LFGIcon-"))
    end
    banner.hideAt = GetTime() + (hideAfter or BANNER_SECONDS)

    if not banner:IsShown() then
        banner:SetAlpha(0)
        banner:Show()
    end
    -- Each new line pops: the banner swells for an instant and settles
    Tween(0.3, 0, function(p)
        banner:SetAlpha(max(banner:GetAlpha(), p))
        banner:SetScale(1.08 - 0.08 * OutCubic(p))
    end)
end

local function OnEvent(event, boss, value, name)
    local mission = FindMission(boss)
    local dungeon = mission and mission.dungeon or banner and banner.dungeon
    if banner then
        banner.dungeon = dungeon or banner.dungeon
    end

    if event == EVENT_ARRIVED then
        PlaySoundFile(SOUND .. "ChallengeStart.ogg")
        ShowBanner(name, format(TEXT.fight, name), dungeon)
    elseif event == EVENT_KILLED then
        PlaySoundFile(SOUND .. "NewRecord.ogg")
        ShowBanner(name, format(TEXT.killed, name), dungeon)
    elseif event == EVENT_PULLING then
        -- The bot tank pulls when this runs out: the last seconds tick, as the Mythic+ countdown does
        PlaySound("ReadyCheck")
        ShowBanner(name, format(TEXT.pulling, value), dungeon, value + 1)
        banner.countdownFormat = TEXT.pulling
        banner.countdownEnd = GetTime() + value
        banner.countdownShown = value
    elseif event == EVENT_RETURNING then
        ShowBanner(name, format(TEXT.returning, value), dungeon, value + 1)
        banner.countdownFormat = TEXT.returning
        banner.countdownEnd = GetTime() + value
        banner.countdownShown = value
    elseif event == EVENT_WIPED then
        PlaySound("RaidWarning")
        ShowBanner(name, format(TEXT.wiped, value), dungeon)
    elseif event == EVENT_WON then
        PlaySound("LEVELUPSOUND")
        state.challenge = 0
        ShowBanner(name, TEXT.won, dungeon, 6)
    elseif event == EVENT_FAILED then
        PlaySound("igQuestFailed")
        state.challenge = 0
        ShowBanner(name, TEXT.failed[value] or TEXT.failed[1], dungeon, 6)
    end
end

-- The Raid Finder's status, while a challenge is being assembled: how full the group is
local function OnRaidFinderStatus(runState, counts)
    if state.challenge == 0 then
        return
    end

    local mission = FindMission(state.challenge)
    local name = mission and mission.name or ""
    local runStateNumber = tonumber(runState) or 0
    if runStateNumber == 1 or runStateNumber == 2 then
        local tanks, needTanks, healers, needHealers, damage, needDamage =
            string.match(counts or "", "(%d+):(%d+),(%d+):(%d+),(%d+):(%d+)")
        local have = (tonumber(tanks) or 0) + (tonumber(healers) or 0) + (tonumber(damage) or 0)
        local need = (tonumber(needTanks) or 0) + (tonumber(needHealers) or 0) + (tonumber(needDamage) or 0)
        ShowBanner(name, format("%s : %d/%d", TEXT.assembling, have, need), mission and mission.dungeon)
    elseif runStateNumber == 3 and banner and banner:IsShown() and banner.stage ~= "travel" then
        banner.stage = "travel"
        ShowBanner(name, format(TEXT.travel, name), mission and mission.dungeon)
    end
end

-- Messages ----------------------------------------------------------------------------------------------------------

local function Handle(message)
    local kind, a, b, c, d, e, f, g, h, i, j, k = strsplit("\t", message)
    if kind == "B" then
        incoming.rotation = tonumber(a) or 0
        incoming.left = tonumber(b) or 0
        incoming.bracket = tonumber(c) or 0
        incoming.challenge = tonumber(d) or 0
        incoming.missions = {}
        incoming.rewards = {}
    elseif kind == "M" then
        tinsert(incoming.missions, {
            kind = tonumber(a) or 1,
            boss = tonumber(b) or 0,
            dungeon = tonumber(c) or 0,
            players = tonumber(d) or 10,
            gold = tonumber(e) or 0,
            state = tonumber(f) or 0,
            difficulty = tonumber(g) or 0,
            itemLevel = tonumber(h) or 0,
            paragon = tonumber(i) or 0,
            name = j or "",
            essences = tonumber(k) or 0,
        })
    elseif kind == "R" then
        tinsert(incoming.rewards, {
            rotation = tonumber(a) or 0,
            boss = tonumber(b) or 0,
            dungeon = tonumber(c) or 0,
            gold = tonumber(d) or 0,
            paragon = tonumber(e) or 0,
            name = f or "",
            essences = tonumber(g) or 0,
        })
    elseif kind == "E" then
        local previousChallenge = state.challenge
        state.rotation = incoming.rotation or 0
        state.left = incoming.left or 0
        state.receivedAt = GetTime()
        state.bracket = incoming.bracket or 0
        state.challenge = incoming.challenge or 0
        state.missions = incoming.missions
        state.rewards = incoming.rewards
        state.asked = false
        -- The answer to this player's own START: the card gets its stamp
        if state.challenge ~= 0 and previousChallenge == 0 and state.starting == state.challenge then
            state.starting = nil
            AnimateAccepted(state.challenge)
            if banner then
                banner.stage = nil
            end
        end
        if a == "1" then
            ShowBoard(true)
        elseif frame and frame:IsShown() then
            ShowBoard(false)
        end
    elseif kind == "H" then
        if frame and frame:IsShown() then
            frame:Hide()
        end
    elseif kind == "P" then
        OnEvent(tonumber(a) or 0, tonumber(b) or 0, tonumber(c) or 0, d or "")
    elseif kind == "C" then
        AnimateClaim(tonumber(b) or 0, tonumber(c) or 0, tonumber(d) or 0, tonumber(e) or 0)
    elseif kind == "X" then
        state.starting = nil
        ShowError(a)
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if sender ~= UnitName("player") then
        return
    end

    if prefix == PREFIX then
        Handle(message)
    elseif prefix == "RaidFinder" then
        local kind, runState, _, counts = strsplit("\t", message)
        if kind == "S" then
            OnRaidFinderStatus(runState, counts)
        end
    end
end)
