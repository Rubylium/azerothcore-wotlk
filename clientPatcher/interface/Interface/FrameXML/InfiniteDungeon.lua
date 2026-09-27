-- The Infinite Dungeon (server: mod-stat-growth src/infinite/InfiniteDungeonSystem.cpp). An endless ladder of short
-- floors entered from Eternia, a keeper in every capital. In the style of the challenge board and the Mythic+ frames
-- (ChallengeBoard.lua, MythicPlus.lua), with the same materials:
-- - the tracker, in the quest tracker's place like the Mythic+ timer: the floor, the step and its paragon at the
--   cap, what the floor asks for (the foes, the guardian, the portal), a bar to the next checkpoint, the next gear
--   floor, the hearts, the falls and the partner. It folds down to its header.
-- - the banners at the top of the screen: each floor reached (the checkpoint and gear floors have their own), each
--   floor cleared with what it gave, and the checkpoint reached with its chest
-- - a toast when the checkpoint chest is opened, and the run's summary when it is over (floors, record, rewards)
-- - Eternia's pin on the world map and the minimap, in every capital and the zone around it
--
-- Protocol: prefix "Infinite", tab-separated, whispered to oneself (InfiniteDungeonSystem.cpp documents it too).
--   server  HUD <floor> <checkpoint> <ladder> <step> <paragon> <state> <arena> <level> <players> <best>
--           ARRIVE <floor> <arena> <ladder> <step> <paragon>
--           PROG <foes down> <foes> <boss down> <hearts lying> <hearts taken> <deaths> <partner> <partner state>
--           REWARD <floor> <gold copper> <experience> <essences> <gear 0/1>        before CLEAR
--           CLEAR <floor> <checkpoint reached 0/1>
--           CHEST <essences> <paragon>
--           SUMMARY <ladder> <start> <floor> <cleared> <best> <checkpoint> <gold> <experience> <essences> <items>
--                   <paragon>                                                   before END
--           END <reason> <floor>              reason: 0 left, 1 everyone fell, 2 never arrived
--           PORTAL, PORTALOFF, KEEPER, KEEPDUO, KEEPREC, KEEPEND, KEEPER_CLOSE: InfiniteDungeonKeeper.lua
--   client  STATE                              asks for the tracker again (entering the world)
--           KEEPER_GO, KEEPER_REFRESH, KEEPER_CLOSED, PORTAL_DESCEND, RUN_LEAVE: InfiniteDungeonKeeper.lua
-- ladder: 0 levelling, 1 gearing (level 80, in steps of five floors with a recommended paragon).
-- state: 0 on the way, 1 in the bubble, 2 fighting, 3 cleared, 4 fallen (InfiniteDungeonSystem.cpp FloorState).
-- partner state: 0 none, 1 alive on the floor, 2 dead, 3 not on the floor.
--
-- InfiniteDungeon_Receive(message) feeds a message by hand (screenshots, tests): "HUD\t12\t10\t...".

local PREFIX = "Infinite"
local SOUND = "Sound\\Interface\\MythicPlus\\"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
-- Drawn by localTools/interface/buildInfiniteDungeonArt.py
local ART = "Interface\\InfiniteDungeon\\InfiniteDungeon-"
local PIN_TIP = 20 / 512            -- the pin's tip, above the bottom of its texture
local COIN_ICON = "Interface\\Icons\\INV_Misc_Coin_01"
local EXPERIENCE_ICON = "Interface\\Icons\\Spell_Holy_SurgeOfLight"
local ESSENCE_ICON = "Interface\\Icons\\INV_Enchant_EssenceMagicLarge"
local GEAR_ICON = "Interface\\Icons\\INV_Chest_Chain_05"
local PARAGON_ICON = "Interface\\Icons\\ParagonNode_Awakening"
local SKULL = "Interface\\TargetingFrame\\UI-TargetingFrame-Skull"

-- InfiniteDungeonScaling.h
local CHECKPOINT_FLOORS, GEAR_FLOORS = 10, 5
local LADDER_GEARING = 1
local STATE_TRAVELLING, STATE_BUBBLE, STATE_FIGHTING, STATE_CLEARED, STATE_FALLEN = 0, 1, 2, 3, 4
local REASON_LEFT, REASON_FALLEN = 0, 1
local PARTNER_NONE, PARTNER_ALIVE, PARTNER_DEAD = 0, 1, 2

-- The challenge board's palette (ChallengeBoard.lua, Prestige.lua): gold on dark, parchment text, no bright colours
local HEADING = { 1, 0.86, 0.55 }
local GOLD = { 1, 0.82, 0.3 }
local BORDER = { 0.75, 0.6, 0.35 }
local ICON_BORDER = { 0.85, 0.7, 0.4 }
local PARCHMENT = { 1, 0.9, 0.7 }
local SOFT = { 0.85, 0.8, 0.7 }
local MUTED = { 0.62, 0.57, 0.5 }
local AMBER = { 0.96, 0.74, 0.42 }
local EMBER = { 0.85, 0.53, 0.37 }
local PARAGON_PURPLE = { 0.72, 0.5, 0.95 }

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Donjon infini",
    kicker = "DONJON INFINI",
    floor = "Étage %d",
    record = "Record %d",
    step = "Palier %d",
    paragon = "parangon conseillé %d",
    travelling = "En route vers l'étage…",
    bubble = "Sortez du cercle pour commencer",
    fighting = "Vainquez le gardien de l'étage",
    cleared = "Le portail est ouvert : descendez quand vous voulez",
    fallen = "La descente s'achève…",
    foes = "Ennemis vaincus",
    guardian = "Gardien de l'étage",
    portal = "Empruntez le portail",
    openChest = "Ouvrez le coffre du point de passage",
    nextCheckpoint = "Point de passage : étage %d",
    checkpointHere = "Point de passage à cet étage",
    gearHere = "Le gardien offre une pièce d'équipement",
    gearNext = "Équipement : étage %d (dans %d)",
    heartsLine = "%d au sol · %d pris",
    partnerAlive = "en vie",
    partnerDead = "tombé : de retour à l'étage suivant",
    partnerAway = "hors de l'étage",
    kickerGear = "ÉTAGE D'ÉQUIPEMENT",
    kickerCheckpoint = "ÉTAGE DU POINT DE PASSAGE",
    arriveGear = "Son gardien vous offrira une pièce d'équipement",
    arriveCheckpoint = "Franchissez-le : votre progression sera gardée",
    clearedTitle = "Étage %d franchi",
    portalOpen = "Le portail mène plus bas",
    kickerCleared = "ÉTAGE %d FRANCHI",
    checkpointTitle = "Point de passage atteint",
    checkpointText = "Vous reprendrez à l'étage %d",
    checkpointChest = "Ouvrez le coffre du point de passage",
    experience = "%s expérience",
    essences = "%d |4essence:essences;",
    gearPiece = "une pièce d'équipement",
    items = "%d |4pièce:pièces; d'équipement",
    paragonPoints = "%d |4point:points; de parangon",
    chestToast = "Coffre du point de passage",
    endFallen = "La descente s'achève",
    endLeft = "Retour à la surface",
    ladderLevelling = "Échelle de progression · depuis l'étage %d",
    ladderGearing = "Échelle d'équipement (niveau 80) · depuis l'étage %d",
    statFloor = "Étage atteint",
    statCleared = "Étages franchis",
    statBest = "Record",
    rewards = "Récompenses de la descente",
    noRewards = "Aucune récompense cette fois : le premier gardien attend encore.",
    checkpointLine = "Point de passage : étage %d · la prochaine descente commence à l'étage %d",
    noCheckpointLine = "Pas encore de point de passage : il en vient un tous les dix étages",
    close = "Fermer",
    summaryChat = "étage %d atteint, %d |4étage franchi:étages franchis;.",
    mapTitle = "Donjon infini - Eternia",
    mapKeeper = "Eternia, gardienne du Donjon infini",
    mapHint = "Dès le niveau 15, seul ou avec un partenaire de groupe",
    mapHint2 = "Chaque étage : quelques ennemis, un gardien, un portail vers le bas",
    leave = "Quitter le donjon",
    portalHint = "Cliquez pour rouvrir le choix du portail",
} or {
    title = "Infinite Dungeon",
    kicker = "INFINITE DUNGEON",
    floor = "Floor %d",
    record = "Best %d",
    step = "Step %d",
    paragon = "recommended paragon %d",
    travelling = "On the way down…",
    bubble = "Step out of the circle to begin",
    fighting = "Defeat the floor's guardian",
    cleared = "The portal is open: go down when you are ready",
    fallen = "The descent ends…",
    foes = "Foes defeated",
    guardian = "The floor's guardian",
    portal = "Take the portal",
    openChest = "Open the checkpoint chest",
    nextCheckpoint = "Checkpoint: floor %d",
    checkpointHere = "Checkpoint on this floor",
    gearHere = "The guardian gives a piece of gear",
    gearNext = "Gear: floor %d (in %d)",
    heartsLine = "%d on the ground · %d taken",
    partnerAlive = "alive",
    partnerDead = "fallen: back on the next floor",
    partnerAway = "off the floor",
    kickerGear = "GEAR FLOOR",
    kickerCheckpoint = "CHECKPOINT FLOOR",
    arriveGear = "Its guardian will give you a piece of gear",
    arriveCheckpoint = "Clear it: your progress will be kept",
    clearedTitle = "Floor %d cleared",
    portalOpen = "The portal leads further down",
    kickerCleared = "FLOOR %d CLEARED",
    checkpointTitle = "Checkpoint reached",
    checkpointText = "You will start again from floor %d",
    checkpointChest = "Open the checkpoint chest",
    experience = "%s experience",
    essences = "%d |4essence:essences;",
    gearPiece = "a piece of gear",
    items = "%d |4piece:pieces; of gear",
    paragonPoints = "%d paragon |4point:points;",
    chestToast = "Checkpoint chest",
    endFallen = "The descent ends",
    endLeft = "Back to the surface",
    ladderLevelling = "Levelling ladder · from floor %d",
    ladderGearing = "Gearing ladder (level 80) · from floor %d",
    statFloor = "Floor reached",
    statCleared = "Floors cleared",
    statBest = "Record",
    rewards = "What the descent gave",
    noRewards = "No rewards this time: the first guardian still waits.",
    checkpointLine = "Checkpoint: floor %d · the next descent starts on floor %d",
    noCheckpointLine = "No checkpoint yet: one comes every ten floors",
    close = "Close",
    summaryChat = "reached floor %d, %d |4floor:floors; cleared.",
    mapTitle = "Infinite Dungeon - Eternia",
    mapKeeper = "Eternia, keeper of the Infinite Dungeon",
    mapHint = "From level 15, alone or with a group partner",
    mapHint2 = "Each floor: a few foes, a guardian, a portal further down",
    leave = "Leave the dungeon",
    portalHint = "Click to open the portal's choice again",
}

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

-- 3.3.5 has no SetShown
local function SetShown(region, shown)
    if shown then
        region:Show()
    else
        region:Hide()
    end
end

local function SetAtlas(texture, name, useSize)
    RetailUI.SetAtlas(texture, name, useSize)
end

local function Colored(text, color)
    return format("|cff%02x%02x%02x%s|r", color[1] * 255, color[2] * 255, color[3] * 255, text)
end

-- 12345 -> "12 345" / "12,345"
local function Thousands(value)
    local text = tostring(floor(value or 0))
    local separator = french and " " or ","
    local result = text:reverse():gsub("(%d%d%d)", "%1" .. separator):reverse()
    return (result:gsub("^" .. separator, ""))
end

-- A square texture turned about its centre (angle in radians), by rewriting its coordinates
local function Rotate(texture, angle)
    local cosine, sine = math.cos(angle), math.sin(angle)
    local function corner(x, y)
        return 0.5 + (x * cosine - y * sine) * 0.5, 0.5 + (x * sine + y * cosine) * 0.5
    end
    local ulx, uly = corner(-1, -1)
    local llx, lly = corner(-1, 1)
    local urx, ury = corner(1, -1)
    local lrx, lry = corner(1, 1)
    texture:SetTexCoord(ulx, uly, llx, lly, urx, ury, lrx, lry)
end

-- The same for an atlas piece, inside its own coordinates (MythicPlus.lua SetRotation)
local function RotateAtlas(texture, atlas, angle)
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

-- Tweens: every animation, driven by one clock (Prestige.lua) ---------------------------------------------------

local tweens = {}
local driver = CreateFrame("Frame")
driver:Hide()

local function Tween(duration, delay, update, done, key)
    if key then
        for _, tween in ipairs(tweens) do
            if tween.key == key then
                tween.dead = true
            end
        end
    end
    tinsert(tweens, { time = -(delay or 0), duration = duration, update = update, done = done, key = key })
    driver:Show()
end

local function OutCubic(p)
    local inverse = 1 - p
    return 1 - inverse * inverse * inverse
end

driver:SetScript("OnUpdate", function(self, elapsed)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        if tween.dead then
            tremove(tweens, index)
        else
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
    end
    if #tweens == 0 then
        self:Hide()
    end
end)

-- Building blocks: the challenge board's card, framed icon, divider and bar -----------------------------------------

local function Card(frame, alpha)
    frame:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    frame:SetBackdropColor(0.04, 0.03, 0.02, alpha or 0.92)
    frame:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 1)
end

local function FramedIcon(parent, size, texture)
    local holder = CreateFrame("Frame", nil, parent)
    holder:SetSize(size, size)
    holder:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10 })
    holder:SetBackdropBorderColor(ICON_BORDER[1], ICON_BORDER[2], ICON_BORDER[3], 1)
    local icon = holder:CreateTexture(nil, "ARTWORK")
    icon:SetPoint("TOPLEFT", 3, -3)
    icon:SetPoint("BOTTOMRIGHT", -3, 3)
    icon:SetTexCoord(0.06, 0.94, 0.06, 0.94)
    if texture then
        icon:SetTexture(texture)
    end
    holder.icon = icon
    return holder
end

local function Divider(parent, layer)
    local divider = parent:CreateTexture(nil, layer or "ARTWORK")
    SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetHeight(10)
    return divider
end

local function Label(parent, template, color, justify)
    local text = parent:CreateFontString(nil, "OVERLAY", template or "GameFontHighlightSmall")
    if color then
        text:SetTextColor(color[1], color[2], color[3])
    end
    text:SetJustifyH(justify or "LEFT")
    return text
end

local function Heading(parent, size, color)
    local text = parent:CreateFontString(nil, "OVERLAY")
    text:SetFont(MORPHEUS, size)
    text:SetShadowOffset(1, -1)
    color = color or HEADING
    text:SetTextColor(color[1], color[2], color[3])
    return text
end

-- The board's timer bar: a thin gold frame, an amber-to-gold fill, cut in segments
local function Bar(parent, width, segments)
    local bar = CreateFrame("Frame", nil, parent)
    bar:SetSize(width, 12)
    bar:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 8,
        insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    bar:SetBackdropColor(0, 0, 0, 0.65)
    bar:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 1)

    local fill = bar:CreateTexture(nil, "ARTWORK")
    fill:SetTexture("Interface\\Buttons\\WHITE8X8")
    fill:SetGradient("HORIZONTAL", 0.75, 0.45, 0.1, 1, 0.85, 0.35)
    fill:SetHeight(6)
    fill:SetPoint("LEFT", bar, "LEFT", 3, 0)
    bar.fill = fill
    bar.full = width - 6

    local spark = bar:CreateTexture(nil, "OVERLAY")
    SetAtlas(spark, "ChallengeMode-SoftYellowGlow")
    spark:SetSize(18, 18)
    spark:SetBlendMode("ADD")
    spark:SetPoint("CENTER", fill, "RIGHT", 0, 0)
    bar.spark = spark

    for index = 1, (segments or 1) - 1 do
        local cut = bar:CreateTexture(nil, "OVERLAY")
        cut:SetTexture(0, 0, 0, 0.7)
        cut:SetSize(1, 6)
        cut:SetPoint("LEFT", bar, "LEFT", 3 + bar.full * index / segments, 0)
    end

    bar.value = 0
    bar.segments = segments or 1
    function bar:SetValue(value, animate)
        value = max(0, min(1, value or 0))
        local from = self.value
        self.value = value
        local function apply(share)
            if share <= 0.001 then
                self.fill:Hide()
                self.spark:Hide()
            else
                self.fill:Show()
                self.fill:SetWidth(self.full * share)
                SetShown(self.spark, share < 0.999)
            end
        end
        if animate and from ~= value then
            Tween(0.6, 0, function(p) apply(from + (value - from) * OutCubic(p)) end, nil, self)
        else
            apply(value)
        end
    end
    return bar
end

-- The pieces above, for the keeper's window and the portal's choice (InfiniteDungeonKeeper.lua), which also takes
-- the messages this file does not know (handlers) and hears of the tracker's state (onHud, onEnd)
InfiniteDungeonUI = {
    french = french, Send = Send, SetShown = SetShown, SetAtlas = SetAtlas, Colored = Colored, Rotate = Rotate,
    RotateAtlas = RotateAtlas, Tween = Tween, OutCubic = OutCubic, Card = Card, FramedIcon = FramedIcon,
    Divider = Divider, Label = Label, Heading = Heading, Bar = Bar, Thousands = Thousands,
    ART = ART, MORPHEUS = MORPHEUS, SOUND = SOUND, GEAR_ICON = GEAR_ICON, PARAGON_ICON = PARAGON_ICON,
    EXPERIENCE_ICON = EXPERIENCE_ICON, ESSENCE_ICON = ESSENCE_ICON,
    CHECKPOINT_FLOORS = CHECKPOINT_FLOORS, GEAR_FLOORS = GEAR_FLOORS, LADDER_GEARING = LADDER_GEARING,
    STATE_CLEARED = STATE_CLEARED, STATE_FALLEN = STATE_FALLEN,
    HEADING = HEADING, GOLD = GOLD, BORDER = BORDER, ICON_BORDER = ICON_BORDER, PARCHMENT = PARCHMENT, SOFT = SOFT,
    MUTED = MUTED, AMBER = AMBER, EMBER = EMBER, PARAGON_PURPLE = PARAGON_PURPLE,
    handlers = {},
}

-- What the server last said -----------------------------------------------------------------------------------------

local hud = { floor = 1, checkpoint = 0, ladder = 0, step = 0, paragon = 0, state = STATE_TRAVELLING, arena = "",
    level = 0, players = 1, best = 0 }
local progress = { foesDown = 0, foes = 0, bossDown = false, hearts = 0, heartsTaken = 0, deaths = 0, partner = "",
    partnerState = PARTNER_NONE }
InfiniteDungeonUI.hud = hud
local chestOpenedOn = 0     -- the floor whose chest was opened
local pendingReward         -- the floor's REWARD, shown by its CLEAR
local pendingSummary        -- the run's SUMMARY, shown by its END

local function FloorsInBlock()
    local done = hud.floor - 1 - hud.checkpoint + (hud.state == STATE_CLEARED and 1 or 0)
    return max(0, min(CHECKPOINT_FLOORS, done))
end

local function NextGearFloor()
    local floor = hud.floor
    if floor % GEAR_FLOORS == 0 and hud.state == STATE_CLEARED then
        floor = floor + 1
    end
    return ceil(floor / GEAR_FLOORS) * GEAR_FLOORS
end

-- -----------------------------------------------------------------------------------------------------------------
-- The tracker
-- -----------------------------------------------------------------------------------------------------------------

local TRACKER_WIDTH = 250
local tracker
local collapsed = false

local STATUS = {
    [STATE_TRAVELLING] = "travelling",
    [STATE_BUBBLE] = "bubble",
    [STATE_FIGHTING] = "fighting",
    [STATE_CLEARED] = "cleared",
    [STATE_FALLEN] = "fallen",
}

-- A line of the tracker: a frame of its own height, stacked by LayoutTracker
local function Row(height)
    local row = CreateFrame("Frame", nil, tracker)
    row:SetSize(TRACKER_WIDTH, height)
    row.height = height
    return row
end

-- An objective, the quest tracker's way: a nub, then a check once done
local function Objective(row)
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetSize(14, 14)
    row.icon:SetPoint("LEFT", row, "LEFT", 11, 0)
    row.text = Label(row, "GameFontHighlightSmall", SOFT)
    row.text:SetPoint("LEFT", row.icon, "RIGHT", 5, 0)
    row.count = Label(row, "GameFontHighlightSmall", PARCHMENT, "RIGHT")
    row.count:SetPoint("RIGHT", row, "RIGHT", -12, 0)
    row.text:SetPoint("RIGHT", row.count, "LEFT", -6, 0)
    return row
end

local function SetObjective(row, text, count, done, active)
    local wasDone = row.done
    row.done = done
    SetAtlas(row.icon, done and "ui-questtracker-tracker-check-2x" or "ui-questtracker-objective-nub-2x")
    row.text:SetText(text)
    row.count:SetText(count or "")
    if done then
        row.text:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        row.count:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    elseif active then
        row.text:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        row.count:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    else
        row.text:SetTextColor(SOFT[1], SOFT[2], SOFT[3])
        row.count:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    end
    -- Done just now: the check pops
    if done and wasDone == false then
        Tween(0.35, 0, function(p)
            local size = 14 + 10 * (1 - OutCubic(p))
            row.icon:SetSize(size, size)
        end, nil, row)
    end
end

local LayoutTracker

local function CreateTracker()
    tracker = CreateFrame("Frame", "InfiniteDungeonTracker", UIParent)
    tracker:SetWidth(TRACKER_WIDTH)
    tracker:SetFrameStrata("LOW")
    Card(tracker)
    tracker:Hide()

    -- The header: the emblem and its turning swirl, the floor in large, the record
    local header = Row(50)
    local emblemGlow = header:CreateTexture(nil, "BACKGROUND")
    SetAtlas(emblemGlow, "ChallengeMode-SoftYellowGlow")
    emblemGlow:SetSize(70, 70)
    emblemGlow:SetBlendMode("ADD")
    emblemGlow:SetAlpha(0.45)
    local emblem = header:CreateTexture(nil, "ARTWORK")
    emblem:SetTexture(ART .. "Emblem")
    emblem:SetSize(42, 42)
    emblem:SetPoint("TOPLEFT", header, "TOPLEFT", 7, -5)
    emblemGlow:SetPoint("CENTER", emblem, "CENTER")
    local swirl = header:CreateTexture(nil, "OVERLAY")
    swirl:SetTexture(ART .. "Swirl")
    swirl:SetSize(30, 30)
    swirl:SetPoint("CENTER", emblem, "CENTER")
    swirl:SetBlendMode("ADD")
    swirl:SetAlpha(0.55)
    header.glow, header.swirl = emblemGlow, swirl

    local kicker = Label(header, "GameFontNormalSmall", GOLD)
    kicker:SetPoint("TOPLEFT", emblem, "TOPRIGHT", 8, -2)
    kicker:SetText(TEXT.kicker)
    local floorText = Heading(header, 24)
    floorText:SetPoint("TOPLEFT", kicker, "BOTTOMLEFT", 0, -1)
    header.floor = floorText

    local toggle = CreateFrame("Button", nil, header)
    toggle:SetSize(14, 14)
    toggle:SetPoint("TOPRIGHT", header, "TOPRIGHT", -9, -9)
    toggle:SetHighlightTexture("Interface\\Buttons\\UI-PlusButton-Hilight", "ADD")
    toggle:SetScript("OnClick", function()
        collapsed = not collapsed
        PlaySound(collapsed and "igMainMenuOptionCheckBoxOff" or "igMainMenuOptionCheckBoxOn")
        LayoutTracker(true)
    end)
    header.toggle = toggle

    local record = Label(header, "GameFontHighlightSmall", MUTED, "RIGHT")
    -- On the kicker's line, left of the fold button: the big floor number below has the width to itself
    record:SetPoint("RIGHT", toggle, "LEFT", -6, 0)
    header.record = record
    tracker.header = header

    local arena = Row(15)
    arena.text = Label(arena, "GameFontHighlightSmall", SOFT)
    arena.text:SetPoint("LEFT", arena, "LEFT", 12, 0)
    arena.text:SetPoint("RIGHT", arena, "RIGHT", -12, 0)
    tracker.arena = arena

    local step = Row(15)
    step.text = Label(step, "GameFontHighlightSmall", AMBER)
    step.text:SetPoint("LEFT", step, "LEFT", 12, 0)
    step.text:SetPoint("RIGHT", step, "RIGHT", -12, 0)
    tracker.step = step

    -- What to do now; it glows when the players are the ones to act (the bubble, the portal)
    local status = Row(22)
    status.glow = status:CreateTexture(nil, "BACKGROUND")
    SetAtlas(status.glow, "ChallengeMode-SoftYellowGlow")
    status.glow:SetBlendMode("ADD")
    status.glow:SetPoint("TOPLEFT", status, "TOPLEFT", -10, 14)
    status.glow:SetPoint("BOTTOMRIGHT", status, "BOTTOMRIGHT", 10, -14)
    status.glow:Hide()
    status.text = Label(status, "GameFontNormalSmall", GOLD)
    status.text:SetPoint("LEFT", status, "LEFT", 12, 0)
    status.text:SetPoint("RIGHT", status, "RIGHT", -12, 0)
    tracker.status = status

    local function DividerRow()
        local row = Row(10)
        local line = Divider(row)
        line:SetPoint("LEFT", row, "LEFT", 6, 0)
        line:SetPoint("RIGHT", row, "RIGHT", -6, 0)
        return row
    end

    tracker.divider1 = DividerRow()
    tracker.foes = Objective(Row(17))
    tracker.boss = Objective(Row(17))
    tracker.portal = Objective(Row(17))
    tracker.chest = Objective(Row(17))
    tracker.divider2 = DividerRow()

    -- To the next checkpoint: one segment a floor
    local barLabel = Row(16)
    barLabel.text = Label(barLabel, "GameFontHighlightSmall", SOFT)
    barLabel.text:SetPoint("LEFT", barLabel, "LEFT", 12, 0)
    barLabel.count = Label(barLabel, "GameFontHighlightSmall", PARCHMENT, "RIGHT")
    barLabel.count:SetPoint("RIGHT", barLabel, "RIGHT", -12, 0)
    tracker.barLabel = barLabel

    local barRow = Row(16)
    barRow.bar = Bar(barRow, TRACKER_WIDTH - 24, CHECKPOINT_FLOORS)
    barRow.bar:SetPoint("CENTER", barRow, "CENTER", 0, 0)
    tracker.bar = barRow

    local gear = Row(22)
    gear.icon = FramedIcon(gear, 18, GEAR_ICON)
    gear.icon:SetPoint("LEFT", gear, "LEFT", 10, 0)
    gear.text = Label(gear, "GameFontHighlightSmall", SOFT)
    gear.text:SetPoint("LEFT", gear.icon, "RIGHT", 6, 0)
    gear.text:SetPoint("RIGHT", gear, "RIGHT", -10, 0)
    tracker.gear = gear

    tracker.divider3 = DividerRow()

    -- The hearts, and the falls on the right
    local footer = Row(18)
    footer.heart = footer:CreateTexture(nil, "ARTWORK")
    footer.heart:SetTexture(ART .. "Heart")
    footer.heart:SetSize(16, 16)
    footer.heart:SetPoint("LEFT", footer, "LEFT", 10, 0)
    footer.hearts = Label(footer, "GameFontHighlightSmall", SOFT)
    footer.hearts:SetPoint("LEFT", footer.heart, "RIGHT", 5, 0)
    footer.deaths = Label(footer, "GameFontHighlightSmall", PARCHMENT, "RIGHT")
    footer.deaths:SetPoint("RIGHT", footer, "RIGHT", -12, 0)
    footer.skull = footer:CreateTexture(nil, "ARTWORK")
    footer.skull:SetTexture(SKULL)
    footer.skull:SetSize(14, 14)
    footer.skull:SetPoint("RIGHT", footer.deaths, "LEFT", -3, 0)
    tracker.footer = footer

    local partner = Row(17)
    partner.dot = partner:CreateTexture(nil, "ARTWORK")
    partner.dot:SetTexture("Interface\\Buttons\\WHITE8X8")
    partner.dot:SetSize(6, 6)
    partner.dot:SetPoint("LEFT", partner, "LEFT", 15, 0)
    partner.text = Label(partner, "GameFontHighlightSmall", SOFT)
    partner.text:SetPoint("LEFT", partner.dot, "RIGHT", 8, 0)
    partner.text:SetPoint("RIGHT", partner, "RIGHT", -12, 0)
    tracker.partner = partner

    -- Out of the dungeon at any time, after a confirmation (InfiniteDungeonKeeper.lua)
    local leave = Row(30)
    local leaveButton = CreateFrame("Button", nil, leave)
    leaveButton:SetSize(150, 20)
    leaveButton:SetPoint("CENTER", leave, "CENTER", 0, -2)
    leaveButton:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 10,
        insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    leaveButton:SetBackdropColor(0.04, 0.03, 0.02, 0.9)
    leaveButton:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 0.8)
    local leaveText = Label(leaveButton, "GameFontNormalSmall", SOFT, "CENTER")
    leaveText:SetPoint("CENTER", leaveButton, "CENTER", 0, 0)
    leaveText:SetText(TEXT.leave)
    local leaveGlow = leaveButton:CreateTexture(nil, "HIGHLIGHT")
    leaveGlow:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    leaveGlow:SetBlendMode("ADD")
    leaveGlow:SetPoint("TOPLEFT", 3, -3)
    leaveGlow:SetPoint("BOTTOMRIGHT", -3, 3)
    leaveGlow:SetAlpha(0.35)
    leaveButton:SetScript("OnEnter", function() leaveText:SetTextColor(GOLD[1], GOLD[2], GOLD[3]) end)
    leaveButton:SetScript("OnLeave", function() leaveText:SetTextColor(SOFT[1], SOFT[2], SOFT[3]) end)
    leaveButton:SetScript("OnClick", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        if InfiniteDungeonUI.ConfirmLeave then
            InfiniteDungeonUI.ConfirmLeave()
        end
    end)
    tracker.leave = leave

    -- The portal's line opens its choice again (after Escape, say)
    local portalRow = tracker.portal
    portalRow:EnableMouse(true)
    portalRow:SetScript("OnMouseUp", function()
        if InfiniteDungeonUI.ReopenPortal then
            InfiniteDungeonUI.ReopenPortal()
        end
    end)
    portalRow:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:SetText(TEXT.portalHint, SOFT[1], SOFT[2], SOFT[3])
        GameTooltip:Show()
    end)
    portalRow:SetScript("OnLeave", function() GameTooltip:Hide() end)

    tracker.order = { header, arena, step, status, tracker.divider1, tracker.foes, tracker.boss, tracker.portal,
        tracker.chest, tracker.divider2, barLabel, barRow, gear, tracker.divider3, footer, partner, leave }
    -- Folded: the header, what to do now and the bar
    tracker.folded = { [header] = true, [status] = true, [barRow] = true }

    -- The swirl turns, the call to act breathes
    tracker:SetScript("OnUpdate", function()
        local now = GetTime()
        Rotate(swirl, -now * 0.9)
        emblemGlow:SetAlpha(0.35 + 0.15 * math.sin(now * 1.6))
        if status.glow:IsShown() then
            status.glow:SetAlpha(0.18 + 0.14 * math.sin(now * 3))
        end
    end)
end

-- Stacks the lines the tracker has now, folded or not
function LayoutTracker(animate)
    local y = 4
    for _, row in ipairs(tracker.order) do
        if row.wanted and (not collapsed or tracker.folded[row]) then
            row:ClearAllPoints()
            row:SetPoint("TOPLEFT", tracker, "TOPLEFT", 0, -y)
            row:Show()
            y = y + row.height
        else
            row:Hide()
        end
    end
    local height = y + 6
    local toggle = tracker.header.toggle
    local texture = collapsed and "Interface\\Buttons\\UI-PlusButton-Up" or "Interface\\Buttons\\UI-MinusButton-Up"
    toggle:SetNormalTexture(texture)
    toggle:SetPushedTexture(collapsed and "Interface\\Buttons\\UI-PlusButton-Down" or
        "Interface\\Buttons\\UI-MinusButton-Down")
    SetShown(tracker.header.record, not collapsed and hud.best > 0)

    local from = tracker:GetHeight() or height
    if animate and from > 0 and from ~= height then
        Tween(0.22, 0, function(p) tracker:SetHeight(from + (height - from) * OutCubic(p)) end, nil, "trackerHeight")
    else
        tracker:SetHeight(height)
    end
end

local function PlaceTracker()
    if DungeonTracker_AnchorToQuestArea then
        DungeonTracker_AnchorToQuestArea(tracker)
    else
        tracker:ClearAllPoints()
        tracker:SetPoint("TOPRIGHT", UIParent, "TOPRIGHT", -110, -220)
    end
end

local function SetTrackerShown(shown)
    if not tracker then
        if not shown then
            return
        end
        CreateTracker()
    end
    if shown then
        if not tracker:IsShown() then
            PlaceTracker()
            tracker:SetAlpha(0)
            tracker:Show()
        end
        -- Fades in, or back in when it was fading out
        tracker.hiding = nil
        local from = tracker:GetAlpha()
        if from < 1 then
            Tween(0.35, 0, function(p) tracker:SetAlpha(from + (1 - from) * OutCubic(p)) end, nil, "trackerFade")
        end
    elseif tracker:IsShown() and not tracker.hiding then
        tracker.hiding = true
        local from = tracker:GetAlpha()
        Tween(0.35, 0, function(p) tracker:SetAlpha(from * (1 - p)) end, function()
            tracker.hiding = nil
            tracker:Hide()
        end, "trackerFade")
    end
    -- It takes the quest tracker's place, like the Mythic+ timer (DungeonTracker.lua)
    if DungeonTracker_ClaimQuestArea then
        DungeonTracker_ClaimQuestArea("infinite", shown)
    end
end

local function RefreshTracker(animate)
    if not tracker then
        return
    end
    local header = tracker.header
    local previousFloor = header.shownFloor
    header.shownFloor = hud.floor
    header.floor:SetText(format(TEXT.floor, hud.floor))
    if animate and previousFloor and previousFloor ~= hud.floor then
        Tween(0.4, 0, function(p) header.floor:SetFont(MORPHEUS, 24 + 10 * (1 - OutCubic(p))) end, nil, "floorPop")
    end
    header.record:SetText(format(TEXT.record, hud.best))
    header.wanted = true

    tracker.arena.text:SetText(hud.arena or "")
    tracker.arena.wanted = hud.arena ~= nil and hud.arena ~= ""

    if hud.ladder == LADDER_GEARING then
        local text = format(TEXT.step, hud.step + 1)
        if hud.paragon > 0 then
            text = text .. " · " .. format(TEXT.paragon, hud.paragon)
        end
        tracker.step.text:SetText(text)
        tracker.step.wanted = true
    else
        tracker.step.wanted = false
    end

    local status = tracker.status
    local previousState = status.shownState
    status.shownState = hud.state
    status.text:SetText(TEXT[STATUS[hud.state] or "fighting"])
    local acting = hud.state == STATE_BUBBLE or hud.state == STATE_CLEARED
    local color = acting and GOLD or hud.state == STATE_FALLEN and EMBER or SOFT
    status.text:SetTextColor(color[1], color[2], color[3])
    SetShown(status.glow, acting)
    status.wanted = true
    if animate and previousState and previousState ~= hud.state then
        Tween(0.35, 0, function(p)
            status.text:SetAlpha(OutCubic(p))
        end, nil, "statusFade")
    end

    local building = hud.state == STATE_TRAVELLING
    tracker.divider1.wanted = not building
    tracker.foes.wanted = not building and progress.foes > 0
    SetObjective(tracker.foes, TEXT.foes, format("%d/%d", progress.foesDown, progress.foes),
        progress.foes > 0 and progress.foesDown >= progress.foes, hud.state == STATE_FIGHTING)
    tracker.boss.wanted = not building
    SetObjective(tracker.boss, TEXT.guardian, progress.bossDown and "1/1" or "0/1", progress.bossDown,
        hud.state == STATE_FIGHTING and progress.foesDown >= progress.foes)
    local cleared = hud.state == STATE_CLEARED
    tracker.portal.wanted = cleared
    SetObjective(tracker.portal, TEXT.portal, nil, false, true)
    local checkpointFloor = hud.floor % CHECKPOINT_FLOORS == 0
    tracker.chest.wanted = cleared and checkpointFloor
    SetObjective(tracker.chest, TEXT.openChest, nil, chestOpenedOn == hud.floor, chestOpenedOn ~= hud.floor)
    tracker.divider2.wanted = true

    local done = FloorsInBlock()
    local nextCheckpoint = hud.checkpoint + CHECKPOINT_FLOORS
    tracker.barLabel.text:SetText(checkpointFloor and not cleared and TEXT.checkpointHere or
        format(TEXT.nextCheckpoint, nextCheckpoint))
    tracker.barLabel.count:SetText(format("%d/%d", done, CHECKPOINT_FLOORS))
    tracker.barLabel.wanted = true
    tracker.bar.bar:SetValue(done / CHECKPOINT_FLOORS, animate)
    tracker.bar.wanted = true

    local gearFloor = NextGearFloor()
    if gearFloor == hud.floor and not cleared then
        tracker.gear.text:SetText(TEXT.gearHere)
        tracker.gear.text:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    else
        tracker.gear.text:SetText(format(TEXT.gearNext, gearFloor, gearFloor - hud.floor))
        tracker.gear.text:SetTextColor(SOFT[1], SOFT[2], SOFT[3])
    end
    tracker.gear.wanted = true

    tracker.divider3.wanted = true
    local footer = tracker.footer
    footer.hearts:SetText(format(TEXT.heartsLine, progress.hearts, progress.heartsTaken))
    footer.deaths:SetText(tostring(progress.deaths))
    footer.skull:SetAlpha(progress.deaths > 0 and 1 or 0.45)
    footer.wanted = true

    local partner = tracker.partner
    partner.wanted = progress.partnerState ~= PARTNER_NONE and progress.partner ~= ""
    if partner.wanted then
        local stateText, stateColor
        if progress.partnerState == PARTNER_ALIVE then
            stateText, stateColor = TEXT.partnerAlive, AMBER
        elseif progress.partnerState == PARTNER_DEAD then
            stateText, stateColor = TEXT.partnerDead, EMBER
        else
            stateText, stateColor = TEXT.partnerAway, MUTED
        end
        partner.text:SetText(Colored(progress.partner, PARCHMENT) .. "  " .. Colored(stateText, stateColor))
        partner.dot:SetVertexColor(stateColor[1], stateColor[2], stateColor[3], 1)
    end

    tracker.leave.wanted = hud.state ~= STATE_FALLEN

    LayoutTracker(animate)
end

-- -----------------------------------------------------------------------------------------------------------------
-- The banner: a floor reached, a floor cleared, a checkpoint reached
-- -----------------------------------------------------------------------------------------------------------------

local banner
local BANNER_IN, BANNER_OUT = 0.5, 0.7
local DIVIDER_WIDTH = 340

local function CreateBanner()
    banner = CreateFrame("Frame", "InfiniteDungeonBanner", UIParent)
    banner:SetSize(620, 200)
    banner:SetPoint("TOP", UIParent, "TOP", 0, -78)
    banner:SetFrameStrata("MEDIUM")
    banner:Hide()

    -- A dark band fading out to both sides, in two halves (the challenge banner), lined in gold
    local band = CreateFrame("Frame", nil, banner)
    band:SetPoint("TOPLEFT", banner, "TOPLEFT", 0, -62)
    band:SetPoint("TOPRIGHT", banner, "TOPRIGHT", 0, -62)
    band:SetHeight(122)
    local left = band:CreateTexture(nil, "BACKGROUND")
    left:SetTexture("Interface\\Buttons\\WHITE8X8")
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMRIGHT", band, "BOTTOM")
    left:SetGradientAlpha("HORIZONTAL", 0.04, 0.03, 0.02, 0, 0.04, 0.03, 0.02, 0.85)
    local right = band:CreateTexture(nil, "BACKGROUND")
    right:SetTexture("Interface\\Buttons\\WHITE8X8")
    right:SetPoint("TOPLEFT", band, "TOP")
    right:SetPoint("BOTTOMRIGHT")
    right:SetGradientAlpha("HORIZONTAL", 0.04, 0.03, 0.02, 0.85, 0.04, 0.03, 0.02, 0)
    for _, edge in ipairs({ "TOP", "BOTTOM" }) do
        local line = Divider(band)
        line:SetPoint(edge .. "LEFT", band, edge .. "LEFT", 40, edge == "TOP" and 4 or -4)
        line:SetPoint(edge .. "RIGHT", band, edge .. "RIGHT", -40, edge == "TOP" and 4 or -4)
    end
    banner.band = band

    -- The emblem over the band's top edge, on a slow star and a warm glow; the chest takes its place at a checkpoint
    local glow = banner:CreateTexture(nil, "BACKGROUND")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetSize(220, 220)
    glow:SetBlendMode("ADD")
    glow:SetPoint("CENTER", banner, "TOP", 0, -58)
    banner.glow = glow
    local star = banner:CreateTexture(nil, "BORDER")
    SetAtlas(star, "ChallengeMode-SpikeyStar")
    star:SetSize(150, 150)
    star:SetBlendMode("ADD")
    star:SetPoint("CENTER", glow, "CENTER")
    banner.star = star
    local emblem = banner:CreateTexture(nil, "ARTWORK")
    emblem:SetTexture(ART .. "Emblem")
    emblem:SetSize(68, 68)
    emblem:SetPoint("CENTER", glow, "CENTER")
    banner.emblem = emblem
    local swirl = banner:CreateTexture(nil, "OVERLAY")
    swirl:SetTexture(ART .. "Swirl")
    swirl:SetSize(50, 50)
    swirl:SetBlendMode("ADD")
    swirl:SetPoint("CENTER", glow, "CENTER")
    banner.swirl = swirl
    local chest = banner:CreateTexture(nil, "ARTWORK")
    SetAtlas(chest, "ChallengeMode-Chest")
    chest:SetSize(96, 71)
    chest:SetPoint("CENTER", glow, "CENTER", 0, 4)
    banner.chest = chest

    local kicker = Label(band, "GameFontNormal", GOLD, "CENTER")
    kicker:SetPoint("TOP", band, "TOP", 0, -18)
    banner.kicker = kicker
    local title = Heading(band, 34)
    title:SetJustifyH("CENTER")
    title:SetPoint("TOP", kicker, "BOTTOM", 0, -3)
    banner.title = title
    local divider = Divider(band, "OVERLAY")
    divider:SetWidth(DIVIDER_WIDTH)
    divider:SetPoint("TOP", title, "BOTTOM", 0, -1)
    banner.divider = divider
    local subtitle = Label(band, "GameFontHighlight", SOFT, "CENTER")
    subtitle:SetPoint("TOP", divider, "BOTTOM", 0, -1)
    banner.subtitle = subtitle
    local detail = Label(band, "GameFontHighlightSmall", AMBER, "CENTER")
    detail:SetPoint("TOP", subtitle, "BOTTOM", 0, -4)
    detail:SetWidth(560)
    banner.detail = detail

    banner:SetScript("OnUpdate", function(self)
        local now = GetTime()
        local age = now - self.shownAt
        local alpha, ease
        if age < BANNER_IN then
            ease = OutCubic(age / BANNER_IN)
            alpha = ease
        elseif now < self.hideAt then
            ease, alpha = 1, 1
        else
            local out = (now - self.hideAt) / BANNER_OUT
            if out >= 1 then
                self:Hide()
                return
            end
            ease, alpha = 1, 1 - OutCubic(out)
        end
        self:SetAlpha(alpha)
        -- In: the title settles from above, the divider draws out, the emblem lands; out: a drift upwards
        local drift = now > self.hideAt and 12 * (now - self.hideAt) / BANNER_OUT or 0
        self:SetPoint("TOP", UIParent, "TOP", 0, -78 + drift)
        kicker:SetPoint("TOP", band, "TOP", 0, -18 + 14 * (1 - ease))
        divider:SetWidth(max(1, DIVIDER_WIDTH * ease))
        local emblemSize = 68 * (1.4 - 0.4 * ease)
        emblem:SetSize(emblemSize, emblemSize)
        swirl:SetSize(emblemSize * 0.74, emblemSize * 0.74)
        chest:SetSize(96 * (1.3 - 0.3 * ease), 71 * (1.3 - 0.3 * ease))
        Rotate(swirl, -now * 1.2)
        RotateAtlas(star, "ChallengeMode-SpikeyStar", now * 0.35)
        glow:SetAlpha(self.glowStrength * (0.75 + 0.25 * math.sin(now * 2.2)))
    end)
end

-- variant: "floor", "cleared" or "checkpoint"
local function ShowBanner(variant, kicker, title, subtitle, detail, hold, sound)
    if not banner then
        CreateBanner()
    end
    local checkpoint = variant == "checkpoint"
    banner.kicker:SetText(kicker or "")
    banner.title:SetText(title or "")
    banner.subtitle:SetText(subtitle or "")
    banner.detail:SetText(detail or "")
    SetShown(banner.chest, checkpoint)
    SetShown(banner.emblem, not checkpoint)
    SetShown(banner.swirl, not checkpoint)
    SetShown(banner.star, variant ~= "cleared")
    banner.star:SetAlpha(checkpoint and 0.8 or 0.35)
    banner.glowStrength = checkpoint and 1 or variant == "floor" and 0.7 or 0.45
    banner.shownAt = GetTime()
    banner.hideAt = banner.shownAt + BANNER_IN + (hold or 4)
    banner:SetAlpha(0)
    banner:Show()
    if sound then
        PlaySoundFile(sound)
    end
end

-- The rewards of a floor, on one line: "+1g 20s · +3 450 experience · +1 essence · a piece of gear"
local function RewardLine(reward)
    if not reward then
        return nil
    end
    local parts = {}
    if reward.gold > 0 then
        tinsert(parts, GetCoinTextureString(reward.gold))
    end
    if reward.experience > 0 then
        tinsert(parts, "+" .. format(TEXT.experience, Thousands(reward.experience)))
    end
    if reward.essences > 0 then
        tinsert(parts, "+" .. format(TEXT.essences, reward.essences))
    end
    if reward.gear then
        tinsert(parts, Colored(TEXT.gearPiece, GOLD))
    end
    return table.concat(parts, "  ·  ")
end

local function ShowArrival(floor, arena, ladder, step, paragon)
    local kicker, detail = TEXT.kicker, nil
    local steps
    if ladder == LADDER_GEARING then
        steps = format(TEXT.step, step + 1)
        if paragon > 0 then
            steps = steps .. " · " .. format(TEXT.paragon, paragon)
        end
    end
    if floor % CHECKPOINT_FLOORS == 0 then
        kicker, detail = TEXT.kickerCheckpoint, TEXT.arriveCheckpoint
    elseif floor % GEAR_FLOORS == 0 then
        kicker, detail = TEXT.kickerGear, TEXT.arriveGear
    end
    if steps then
        detail = detail and (steps .. "  ·  " .. detail) or steps
    end
    ShowBanner("floor", kicker, format(TEXT.floor, floor), arena, detail, 4, SOUND .. "ChallengeStart.ogg")
end

local function ShowCleared(floor, checkpoint)
    local reward = pendingReward and pendingReward.floor == floor and pendingReward or nil
    pendingReward = nil
    local line = RewardLine(reward)
    if checkpoint then
        local detail = TEXT.checkpointChest
        if line and line ~= "" then
            detail = line .. "\n" .. Colored(detail, GOLD)
        end
        ShowBanner("checkpoint", format(TEXT.kickerCleared, floor), TEXT.checkpointTitle,
            format(TEXT.checkpointText, floor + 1), detail, 6, SOUND .. "NewRecord.ogg")
    else
        ShowBanner("cleared", TEXT.kicker, format(TEXT.clearedTitle, floor), TEXT.portalOpen, line, 4,
            reward and reward.gear and SOUND .. "NewRecord.ogg" or nil)
    end
end

-- -----------------------------------------------------------------------------------------------------------------
-- The toast: the checkpoint chest opened (Prestige.lua's)
-- -----------------------------------------------------------------------------------------------------------------

local toast
local TOAST_TOP = -300

local function CreateToast()
    toast = CreateFrame("Frame", nil, UIParent)
    toast:SetSize(420, 46)
    toast:SetPoint("TOP", UIParent, "TOP", 0, TOAST_TOP)
    toast:SetFrameStrata("HIGH")
    toast:Hide()
    local left = toast:CreateTexture(nil, "BACKGROUND")
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMRIGHT", toast, "BOTTOM")
    left:SetTexture("Interface\\Buttons\\WHITE8X8")
    left:SetGradientAlpha("HORIZONTAL", 0.4, 0.3, 0.1, 0, 0.4, 0.3, 0.1, 0.55)
    local right = toast:CreateTexture(nil, "BACKGROUND")
    right:SetPoint("TOPLEFT", toast, "TOP")
    right:SetPoint("BOTTOMRIGHT")
    right:SetTexture("Interface\\Buttons\\WHITE8X8")
    right:SetGradientAlpha("HORIZONTAL", 0.4, 0.3, 0.1, 0.55, 0.4, 0.3, 0.1, 0)
    local icon = toast:CreateTexture(nil, "ARTWORK")
    SetAtlas(icon, "ChallengeMode-icon-chest")
    icon:SetSize(28, 29)
    local title = Heading(toast, 18)
    title:SetPoint("TOP", toast, "TOP", 16, -5)
    local text = Label(toast, "GameFontHighlightSmall", PARCHMENT, "CENTER")
    text:SetPoint("TOP", title, "BOTTOM", 0, -2)
    icon:SetPoint("RIGHT", title, "LEFT", -8, -6)
    toast.title, toast.text = title, text
end

local function ShowChestToast(essences, paragon)
    if not toast then
        CreateToast()
    end
    local parts = {}
    if essences > 0 then
        tinsert(parts, "+" .. format(TEXT.essences, essences))
    end
    if paragon > 0 then
        tinsert(parts, Colored("+" .. format(TEXT.paragonPoints, paragon), PARAGON_PURPLE))
    end
    toast.title:SetText(TEXT.chestToast)
    toast.text:SetText(table.concat(parts, "  ·  "))
    toast:Show()
    PlaySound("LOOTWINDOWCOINSOUND")
    Tween(3.6, 0, function(p)
        local alpha = p < 0.12 and p / 0.12 or (p > 0.75 and (1 - p) / 0.25 or 1)
        toast:SetAlpha(alpha)
        toast:SetPoint("TOP", UIParent, "TOP", 0, TOAST_TOP + 16 * OutCubic(min(1, p * 4)))
    end, function() toast:Hide() end, "toast")
end

-- -----------------------------------------------------------------------------------------------------------------
-- The summary: the run is over
-- -----------------------------------------------------------------------------------------------------------------

local summary
local SUMMARY_WIDTH, SUMMARY_HEIGHT = 420, 420

local function StatBlock(parent)
    local block = CreateFrame("Frame", nil, parent)
    block:SetSize(118, 64)
    Card(block, 0.85)
    block.glow = parent:CreateTexture(nil, "BORDER")
    SetAtlas(block.glow, "ChallengeMode-SoftYellowGlow")
    block.glow:SetBlendMode("ADD")
    block.glow:SetPoint("TOPLEFT", block, "TOPLEFT", -24, 24)
    block.glow:SetPoint("BOTTOMRIGHT", block, "BOTTOMRIGHT", 24, -24)
    block.glow:Hide()
    block.value = Heading(block, 28, PARCHMENT)
    block.value:SetPoint("TOP", block, "TOP", 0, -8)
    block.label = Label(block, "GameFontNormalSmall", MUTED, "CENTER")
    block.label:SetPoint("BOTTOM", block, "BOTTOM", 0, 10)
    return block
end

local function RewardRow(parent)
    local row = CreateFrame("Frame", nil, parent)
    row:SetSize(176, 26)
    row.icon = FramedIcon(row, 24)
    row.icon:SetPoint("LEFT", row, "LEFT", 0, 0)
    row.text = Label(row, "GameFontHighlight", PARCHMENT)
    row.text:SetPoint("LEFT", row.icon, "RIGHT", 7, 0)
    row.text:SetPoint("RIGHT", row, "RIGHT", 0, 0)
    return row
end

local function CreateSummary()
    summary = CreateFrame("Frame", "InfiniteDungeonSummary", UIParent)
    summary:SetSize(SUMMARY_WIDTH, SUMMARY_HEIGHT)
    summary:SetPoint("CENTER", UIParent, "CENTER", 0, 60)
    summary:SetFrameStrata("HIGH")
    summary:EnableMouse(true)
    summary:SetMovable(true)
    summary:SetClampedToScreen(true)
    summary:Hide()
    tinsert(UISpecialFrames, "InfiniteDungeonSummary")

    -- The board's window: rock, a tinted parchment over it, the metal frame
    local ground = summary:CreateTexture(nil, "BACKGROUND")
    ground:SetTexture(RetailUIFiles["ui-background-rock"], true)
    ground:SetHorizTile(true)
    ground:SetVertTile(true)
    ground:SetPoint("TOPLEFT", 2, -21)
    ground:SetPoint("BOTTOMRIGHT", -2, 2)
    local parchment = summary:CreateTexture(nil, "BORDER")
    SetAtlas(parchment, "questbg-parchment")
    parchment:SetPoint("TOPLEFT", 8, -26)
    parchment:SetPoint("BOTTOMRIGHT", -8, 8)
    parchment:SetVertexColor(0.34, 0.28, 0.22)
    RetailUI.ApplyNineSlice(summary, false)

    local windowTitle = summary:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    windowTitle:SetPoint("TOPLEFT", summary, "TOPLEFT", 26, -4)
    windowTitle:SetPoint("TOPRIGHT", summary, "TOPRIGHT", -26, -4)
    windowTitle:SetJustifyH("CENTER")
    windowTitle:SetText(TEXT.title)
    windowTitle:SetTextColor(1, 0.82, 0)

    local closeButton = CreateFrame("Button", nil, summary)
    closeButton:SetSize(24, 24)
    closeButton:SetPoint("TOPRIGHT", summary, "TOPRIGHT", -5, -5)
    closeButton:SetFrameLevel(summary:GetFrameLevel() + 20)
    closeButton:SetNormalTexture(RetailUIAtlas["redbutton-exit-2x"][1])
    SetAtlas(closeButton:GetNormalTexture(), "redbutton-exit-2x")
    closeButton:SetPushedTexture(RetailUIAtlas["redbutton-exit-pressed-2x"][1])
    SetAtlas(closeButton:GetPushedTexture(), "redbutton-exit-pressed-2x")
    closeButton:SetHighlightTexture(RetailUIAtlas["redbutton-highlight-2x"][1])
    SetAtlas(closeButton:GetHighlightTexture(), "redbutton-highlight-2x")
    closeButton:GetHighlightTexture():SetBlendMode("ADD")
    closeButton:SetScript("OnClick", function() summary:Hide() end)

    local mover = CreateFrame("Frame", nil, summary)
    mover:SetPoint("TOPLEFT", summary, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", summary, "TOPRIGHT", -40, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(summary:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() summary:StartMoving() end)
    mover:SetScript("OnDragStop", function() summary:StopMovingOrSizing() end)

    -- The emblem on its glow (and the star for a record), the heading and the ladder
    local glow = summary:CreateTexture(nil, "ARTWORK")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetSize(150, 150)
    glow:SetBlendMode("ADD")
    glow:SetPoint("CENTER", summary, "TOP", 0, -70)
    local star = summary:CreateTexture(nil, "ARTWORK", nil, 1)
    SetAtlas(star, "ChallengeMode-SpikeyStar")
    star:SetSize(118, 118)
    star:SetBlendMode("ADD")
    star:SetPoint("CENTER", glow, "CENTER")
    local emblem = summary:CreateTexture(nil, "OVERLAY")
    emblem:SetTexture(ART .. "Emblem")
    emblem:SetSize(62, 62)
    emblem:SetPoint("CENTER", glow, "CENTER")
    local swirl = summary:CreateTexture(nil, "OVERLAY", nil, 1)
    swirl:SetTexture(ART .. "Swirl")
    swirl:SetSize(46, 46)
    swirl:SetBlendMode("ADD")
    swirl:SetPoint("CENTER", glow, "CENTER")
    summary.glow, summary.star, summary.swirl = glow, star, swirl

    local heading = Heading(summary, 28)
    heading:SetPoint("TOP", summary, "TOP", 0, -106)
    summary.heading = heading
    local ladder = Label(summary, "GameFontHighlightSmall", SOFT, "CENTER")
    ladder:SetPoint("TOP", heading, "BOTTOM", 0, -3)
    summary.ladder = ladder

    -- Three numbers: the floor reached, the floors cleared, the record
    summary.stats = {}
    for index = 1, 3 do
        local block = StatBlock(summary)
        block:SetPoint("TOP", summary, "TOP", (index - 2) * 128, -164)
        summary.stats[index] = block
    end
    summary.stats[1].label:SetText(TEXT.statFloor)
    summary.stats[2].label:SetText(TEXT.statCleared)
    summary.stats[3].label:SetText(TEXT.statBest)

    local divider = Divider(summary)
    divider:SetPoint("TOPLEFT", summary, "TOPLEFT", 24, -238)
    divider:SetPoint("TOPRIGHT", summary, "TOPRIGHT", -24, -238)

    local rewardsLabel = Label(summary, "GameFontNormalSmall", GOLD)
    rewardsLabel:SetPoint("TOPLEFT", summary, "TOPLEFT", 36, -254)
    rewardsLabel:SetText(TEXT.rewards)
    summary.rewards = {}
    for index = 1, 6 do
        local row = RewardRow(summary)
        local column, line = (index - 1) % 2, floor((index - 1) / 2)
        row:SetPoint("TOPLEFT", summary, "TOPLEFT", 36 + column * 184, -272 - line * 30)
        summary.rewards[index] = row
    end
    local noRewards = Label(summary, "GameFontHighlightSmall", MUTED, "CENTER")
    noRewards:SetPoint("TOP", summary, "TOP", 0, -284)
    noRewards:SetWidth(SUMMARY_WIDTH - 60)
    noRewards:SetText(TEXT.noRewards)
    summary.noRewards = noRewards

    local checkpoint = Label(summary, "GameFontHighlightSmall", SOFT, "CENTER")
    checkpoint:SetPoint("BOTTOM", summary, "BOTTOM", 0, 48)
    checkpoint:SetWidth(SUMMARY_WIDTH - 50)
    summary.checkpoint = checkpoint

    local button = CreateFrame("Button", nil, summary, "UIPanelButtonTemplate")
    button:SetSize(130, 24)
    button:SetPoint("BOTTOM", summary, "BOTTOM", 0, 16)
    button:SetText(TEXT.close)
    button:SetScript("OnClick", function() summary:Hide() end)

    summary:SetScript("OnUpdate", function()
        local now = GetTime()
        Rotate(swirl, -now * 0.9)
        if star:IsShown() then
            RotateAtlas(star, "ChallengeMode-SpikeyStar", now * 0.3)
        end
        glow:SetAlpha(0.55 + 0.25 * math.sin(now * 2))
        for _, block in ipairs(summary.stats) do
            if block.glow:IsShown() then
                block.glow:SetAlpha(0.3 + 0.25 * math.sin(now * 2.5))
            end
        end
    end)
    summary:SetScript("OnShow", function()
        PlaySound("igQuestListOpen")
        summary:SetAlpha(0)
        summary:SetScale(0.94)
        Tween(0.22, 0, function(p)
            summary:SetAlpha(p)
            summary:SetScale(0.94 + 0.06 * OutCubic(p))
        end, nil, "summaryOpen")
    end)
    summary:SetScript("OnHide", function() PlaySound("igQuestListClose") end)
end

-- A number that runs to its value instead of appearing (Prestige.lua CountTo)
local function CountUp(fontString, to, key)
    Tween(0.8, 0.15, function(p)
        fontString:SetText(tostring(floor(to * OutCubic(p) + 0.5)))
    end, nil, key)
end

local function ShowSummary(data, reason)
    if not summary then
        CreateSummary()
    end
    local fallen = reason == REASON_FALLEN
    summary.heading:SetText(fallen and TEXT.endFallen or TEXT.endLeft)
    summary.ladder:SetText(format(data.ladder == LADDER_GEARING and TEXT.ladderGearing or TEXT.ladderLevelling,
        data.start))

    -- The deepest floor this run cleared; the record glows when it is that one
    local deepest = data.start + data.cleared - 1
    local atRecord = data.cleared > 0 and data.best > 0 and data.best == deepest
    summary.stats[1].value:SetText("0")
    summary.stats[2].value:SetText("0")
    summary.stats[3].value:SetText("0")
    CountUp(summary.stats[1].value, data.floor, "stat1")
    CountUp(summary.stats[2].value, data.cleared, "stat2")
    CountUp(summary.stats[3].value, data.best, "stat3")
    SetShown(summary.stats[3].glow, atRecord)
    summary.stats[3].value:SetTextColor((atRecord and HEADING or PARCHMENT)[1], (atRecord and HEADING or PARCHMENT)[2],
        (atRecord and HEADING or PARCHMENT)[3])
    SetShown(summary.star, atRecord)

    local rows = {}
    if data.gold > 0 then
        tinsert(rows, { COIN_ICON, GetCoinTextureString(data.gold) })
    end
    if data.experience > 0 then
        tinsert(rows, { EXPERIENCE_ICON, "+" .. format(TEXT.experience, Thousands(data.experience)) })
    end
    if data.essences > 0 then
        tinsert(rows, { ESSENCE_ICON, "+" .. format(TEXT.essences, data.essences) })
    end
    if data.items > 0 then
        tinsert(rows, { GEAR_ICON, Colored(format(TEXT.items, data.items), GOLD) })
    end
    if data.paragon > 0 then
        tinsert(rows, { PARAGON_ICON, Colored("+" .. format(TEXT.paragonPoints, data.paragon), PARAGON_PURPLE) })
    end
    for index, row in ipairs(summary.rewards) do
        local reward = rows[index]
        if reward then
            row.icon.icon:SetTexture(reward[1])
            row.text:SetText(reward[2])
            row:SetAlpha(0)
            row:Show()
            Tween(0.3, 0.25 + 0.08 * index, function(p) row:SetAlpha(OutCubic(p)) end, nil, row)
        else
            row:Hide()
        end
    end
    SetShown(summary.noRewards, #rows == 0)

    if data.checkpoint > 0 then
        summary.checkpoint:SetText(format(TEXT.checkpointLine, data.checkpoint, data.checkpoint + 1))
    else
        summary.checkpoint:SetText(TEXT.noCheckpointLine)
    end

    if summary:IsShown() then
        summary:GetScript("OnShow")(summary)
    else
        summary:Show()
    end
    if atRecord then
        PlaySoundFile(SOUND .. "NewRecord.ogg")
    end
    DEFAULT_CHAT_FRAME:AddMessage("|cffffd24d" .. TEXT.title .. ":|r " .. format(TEXT.summaryChat, data.floor,
        data.cleared))
end

-- -----------------------------------------------------------------------------------------------------------------
-- Messages
-- -----------------------------------------------------------------------------------------------------------------

local function Number(value, default)
    return tonumber(value) or default or 0
end

local function Handle(message)
    local kind, a, b, c, d, e, f, g, h, i, j, k = strsplit("\t", message)
    if kind == "HUD" then
        local previousFloor = hud.floor
        hud.floor = Number(a, 1)
        hud.checkpoint = Number(b)
        hud.ladder = Number(c)
        hud.step = Number(d)
        hud.paragon = Number(e)
        hud.state = Number(f, STATE_FIGHTING)
        hud.arena = g or ""
        hud.level = Number(h)
        hud.players = Number(i, 1)
        hud.best = Number(j)
        if hud.floor ~= previousFloor then
            progress.foesDown, progress.foes, progress.bossDown, progress.hearts = 0, 0, false, 0
        end
        local fresh = not (tracker and tracker:IsShown())
        SetTrackerShown(true)
        RefreshTracker(not fresh)
        if InfiniteDungeonUI.onHud then
            InfiniteDungeonUI.onHud(hud.state)
        end
    elseif kind == "PROG" then
        local previousDeaths = progress.deaths
        progress.foesDown = Number(a)
        progress.foes = Number(b)
        progress.bossDown = c == "1"
        progress.hearts = Number(d)
        progress.heartsTaken = Number(e)
        progress.deaths = Number(f)
        progress.partner = g or ""
        progress.partnerState = Number(h)
        if progress.deaths > previousDeaths and tracker and tracker:IsShown() then
            PlaySound("RaidWarning")
        end
        RefreshTracker(true)
    elseif kind == "ARRIVE" then
        ShowArrival(Number(a, 1), b or "", Number(c), Number(d), Number(e))
    elseif kind == "REWARD" then
        pendingReward = { floor = Number(a), gold = Number(b), experience = Number(c), essences = Number(d),
            gear = e == "1" }
    elseif kind == "CLEAR" then
        ShowCleared(Number(a, 1), b == "1")
    elseif kind == "CHEST" then
        chestOpenedOn = hud.floor
        ShowChestToast(Number(a), Number(b))
        RefreshTracker(true)
    elseif kind == "SUMMARY" then
        pendingSummary = { ladder = Number(a), start = Number(b, 1), floor = Number(c, 1), cleared = Number(d),
            best = Number(e), checkpoint = Number(f), gold = Number(g), experience = Number(h), essences = Number(i),
            items = Number(j), paragon = Number(k), at = GetTime() }
    elseif kind == "END" then
        SetTrackerShown(false)
        if InfiniteDungeonUI.onEnd then
            InfiniteDungeonUI.onEnd()
        end
        local reason = Number(a)
        if pendingSummary and GetTime() - pendingSummary.at < 10 and Number(b) > 0 and
                (reason == REASON_LEFT or reason == REASON_FALLEN) then
            ShowSummary(pendingSummary, reason)
        end
        pendingSummary = nil
        pendingReward = nil
    elseif InfiniteDungeonUI.handlers[kind] then
        -- The keeper's window and the portal's choice (InfiniteDungeonKeeper.lua)
        InfiniteDungeonUI.handlers[kind](select(2, strsplit("\t", message)))
    end
end

-- By hand, for screenshots and tests: InfiniteDungeon_Receive("ARRIVE\t12\tUtgarde Keep\t1\t2\t6")
function InfiniteDungeon_Receive(message)
    Handle(message)
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "PLAYER_ENTERING_WORLD" then
        Send("STATE")
        return
    end
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    Handle(message)
end)

-- -----------------------------------------------------------------------------------------------------------------
-- Eternia on the maps
-- -----------------------------------------------------------------------------------------------------------------
-- Her spawns (mod-stat-growth stat_growth_infinite_dungeon.sql), shown as a gold map pin on the world map (her
-- capital, the zone around it, the continent) and on the minimap in her capital and its zone, on the minimap's edge
-- pointing the way when she is out of its reach (ItemForge.lua's smith does the same). Maps are known by their file
-- name (GetMapInfo) and placed with their WorldMapArea.dbc bounds: a map's left and right edges are world Y, its top
-- and bottom world X. Dalaran's own map has no bounds (it is drawn as floors): its keeper shows on Crystalsong
-- Forest and Northrend.

local KEEPERS = {
    { map = 0, x = -8820.38, y = 624.32, maps = { Stormwind = true, Elwynn = true } },
    { map = 0, x = -4798.60, y = -1104.15, maps = { Ironforge = true, DunMorogh = true } },
    { map = 1, x = 9940.17, y = 2514.60, maps = { Darnassis = true, Teldrassil = true } },
    { map = 530, x = -3919.57, y = -11549.66, maps = { TheExodar = true, AzuremystIsle = true } },
    { map = 1, x = 2072.97, y = -4824.27, maps = { Ogrimmar = true, Durotar = true } },
    { map = 0, x = 1596.67, y = 231.13, maps = { Undercity = true, Tirisfal = true } },
    { map = 1, x = -1257.70, y = 24.30, maps = { ThunderBluff = true, Mulgore = true } },
    { map = 530, x = 9525.23, y = -7216.82, maps = { SilvermoonCity = true, EversongWoods = true } },
    { map = 571, x = 5614.28, y = 693.16, maps = { CrystalsongForest = true } },
    { map = 530, x = -2013.00, y = 5365.55, maps = { ShattrathCity = true, TerokkarForest = true } },
}

local MAP_BOUNDS = {
    Stormwind = { map = 0, left = 1722.92, right = -14.58, top = -7995.83, bottom = -9154.17, city = true },
    Ironforge = { map = 0, left = -713.59, right = -1504.22, top = -4569.24, bottom = -5096.85, city = true },
    Undercity = { map = 0, left = 873.19, right = -86.18, top = 1877.95, bottom = 1237.84, city = true },
    Darnassis = { map = 1, left = 2938.36, right = 1880.03, top = 10238.32, bottom = 9532.59, city = true },
    Ogrimmar = { map = 1, left = -3680.60, right = -5083.21, top = 2273.88, bottom = 1338.46, city = true },
    ThunderBluff = { map = 1, left = 516.67, right = -527.08, top = -850.00, bottom = -1545.83, city = true },
    TheExodar = { map = 530, left = -11066.37, right = -12123.14, top = -3609.68, bottom = -4314.37, city = true },
    SilvermoonCity = { map = 530, left = -6400.75, right = -7612.21, top = 10153.71, bottom = 9346.94, city = true },
    ShattrathCity = { map = 530, left = 6135.26, right = 4829.01, top = -1473.95, bottom = -2344.79, city = true },
    Elwynn = { map = 0, left = 1535.42, right = -1935.42, top = -7939.58, bottom = -10254.17 },
    DunMorogh = { map = 0, left = 1802.08, right = -3122.92, top = -3877.08, bottom = -7160.42 },
    Tirisfal = { map = 0, left = 3033.33, right = -1485.42, top = 3837.50, bottom = 825.00 },
    Teldrassil = { map = 1, left = 3814.58, right = -1277.08, top = 11831.25, bottom = 8437.50 },
    Durotar = { map = 1, left = -1962.50, right = -7250.00, top = 1808.33, bottom = -1716.67 },
    Mulgore = { map = 1, left = 2047.92, right = -3089.58, top = -272.92, bottom = -3697.92 },
    AzuremystIsle = { map = 530, left = -10500.00, right = -14570.83, top = -2793.75, bottom = -5508.33 },
    EversongWoods = { map = 530, left = -4487.50, right = -9412.50, top = 11041.67, bottom = 7758.33 },
    TerokkarForest = { map = 530, left = 7083.33, right = 1683.33, top = -1000.00, bottom = -4600.00 },
    CrystalsongForest = { map = 571, left = 1443.75, right = -1279.17, top = 6502.08, bottom = 4687.50 },
    Azeroth = { map = 0, left = 18171.97, right = -22569.21, top = 11176.34, bottom = -15973.34 },
    Kalimdor = { map = 1, left = 17066.60, right = -19733.21, top = 12799.90, bottom = -11733.30 },
    Expansion01 = { map = 530, left = 12996.04, right = -4468.04, top = 5821.36, bottom = -5821.36 },
    Northrend = { map = 571, left = 9217.15, right = -8534.25, top = 10593.38, bottom = -1240.89 },
}

-- Where a world position falls on a map, 0-1 from its top left; nil when off it
local function MapPosition(bounds, x, y)
    local px = (bounds.left - y) / (bounds.left - bounds.right)
    local py = (bounds.top - x) / (bounds.top - bounds.bottom)
    if px < 0 or px > 1 or py < 0 or py > 1 then
        return nil
    end
    return px, py
end

local function ShowKeeperTooltip(tooltip, owner)
    tooltip:SetOwner(owner, "ANCHOR_RIGHT")
    tooltip:AddLine(TEXT.mapTitle, HEADING[1], HEADING[2], HEADING[3])
    tooltip:AddLine(TEXT.mapKeeper, 1, 1, 1)
    tooltip:AddLine(TEXT.mapHint, SOFT[1], SOFT[2], SOFT[3], true)
    tooltip:AddLine(TEXT.mapHint2, MUTED[1], MUTED[2], MUTED[3], true)
    tooltip:Show()
end

-- The pin: its tip on the spot, a warm glow and a slight lift under the mouse
local function CreatePin(parent, tooltipName)
    local pin = CreateFrame("Button", nil, parent)
    local glow = pin:CreateTexture(nil, "BACKGROUND")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("TOPLEFT", pin, "TOPLEFT", -8, 6)
    glow:SetPoint("BOTTOMRIGHT", pin, "BOTTOMRIGHT", 8, -2)
    glow:Hide()
    local icon = pin:CreateTexture(nil, "OVERLAY")
    icon:SetTexture(ART .. "Pin")
    icon:SetAllPoints()
    pin.icon, pin.glow = icon, glow
    pin:SetScript("OnEnter", function(self)
        glow:Show()
        icon:ClearAllPoints()
        icon:SetPoint("TOPLEFT", self, "TOPLEFT", -2, 3)
        icon:SetPoint("BOTTOMRIGHT", self, "BOTTOMRIGHT", 2, 1)
        ShowKeeperTooltip(_G[tooltipName], self)
    end)
    pin:SetScript("OnLeave", function(self)
        glow:Hide()
        icon:SetAllPoints(self)
        _G[tooltipName]:Hide()
    end)
    return pin
end

-- Puts a pin's tip on (x, y) of its parent's TOPLEFT
local function PlacePin(pin, parent, x, y, size)
    pin:SetSize(size, size)
    pin:ClearAllPoints()
    pin:SetPoint("BOTTOM", parent, "TOPLEFT", x, y - size * PIN_TIP)
end

local mapPins = {}

local function UpdateMapPins()
    local bounds = MAP_BOUNDS[GetMapInfo() or ""]
    local width, height = WorldMapButton:GetWidth(), WorldMapButton:GetHeight()
    -- The same size on screen however the map is scaled (DragonUI scales the canvas)
    local pinScale = UIParent:GetEffectiveScale() / WorldMapButton:GetEffectiveScale()
    -- Larger on a capital's map, smaller on a continent's
    local size = not bounds and 22 or bounds.city and 34 or (bounds.left - bounds.right) < 8000 and 28 or 22
    local shown = 0
    for _, keeper in ipairs(KEEPERS) do
        local px, py
        if bounds and bounds.map == keeper.map then
            px, py = MapPosition(bounds, keeper.x, keeper.y)
        end
        if px then
            shown = shown + 1
            local pin = mapPins[shown]
            if not pin then
                pin = CreatePin(WorldMapButton, "WorldMapTooltip")
                mapPins[shown] = pin
            end
            pin:SetFrameLevel(WorldMapButton:GetFrameLevel() + 6)
            pin:SetScale(pinScale)
            PlacePin(pin, WorldMapButton, px * width / pinScale, -py * height / pinScale, size)
            pin:Show()
        end
    end
    for index = shown + 1, #mapPins do
        mapPins[index]:Hide()
    end
end

local mapWatcher = CreateFrame("Frame")
mapWatcher:RegisterEvent("WORLD_MAP_UPDATE")
mapWatcher:SetScript("OnEvent", function()
    if WorldMapFrame:IsShown() then
        UpdateMapPins()
    end
end)
WorldMapFrame:HookScript("OnShow", UpdateMapPins)
-- The map changes size between its full and windowed views
for _, name in ipairs({ "WorldMapFrame_SetFullMapView", "WorldMapFrame_SetQuestMapView", "WorldMap_ToggleSizeUp",
        "WorldMap_ToggleSizeDown" }) do
    if _G[name] then
        hooksecurefunc(name, UpdateMapPins)
    end
end

-- The minimap ------------------------------------------------------------------------------------------------------

-- The minimap's diameter in yards at each zoom level, indoors and outdoors (the client's own figures)
local MINIMAP_YARDS = {
    indoor = { [0] = 300, 240, 180, 120, 80, 50 },
    outdoor = { [0] = 466.6667, 400, 333.3333, 266.6667, 200, 133.3333 },
}
local MINIMAP_PIN = 22

local minimapPin = CreatePin(Minimap, "GameTooltip")
minimapPin:Hide()

-- Indoors and outdoors keep a zoom each; which one the minimap is on shows when they differ (briefly moved
-- otherwise). Asked again only when the place or the zoom changes.
local indoors = false
local function UpdateIndoors()
    local zoom = Minimap:GetZoom()
    if GetCVar("minimapZoom") == GetCVar("minimapInsideZoom") then
        Minimap:SetZoom(zoom < 2 and zoom + 1 or zoom - 1)
    end
    indoors = tonumber(GetCVar("minimapZoom")) ~= Minimap:GetZoom()
    Minimap:SetZoom(zoom)
end

-- Where the player is, in world yards; nil away from the keepers' maps. The world map is put back on the player's
-- zone when it closes and when the zone changes; in between, reading the position disturbs nothing.
local playerMap, playerX, playerY
local function UpdatePlayerPosition()
    if WorldMapFrame:IsShown() then
        return
    end
    local name = GetMapInfo()
    local bounds = MAP_BOUNDS[name or ""]
    local px, py = GetPlayerMapPosition("player")
    if not bounds or (px == 0 and py == 0) then
        playerMap = nil
        return
    end
    playerMap = name
    playerX = bounds.top - py * (bounds.top - bounds.bottom)
    playerY = bounds.left - px * (bounds.left - bounds.right)
end

local function UpdateMinimapPin()
    UpdatePlayerPosition()
    local keeper
    for _, candidate in ipairs(KEEPERS) do
        if playerMap and candidate.maps[playerMap] then
            keeper = candidate
        end
    end
    if not keeper then
        minimapPin:Hide()
        return
    end

    -- East and south of the player, in yards, turned with the minimap when it turns
    local east = playerY - keeper.y
    local south = playerX - keeper.x
    if GetCVar("rotateMinimap") == "1" then
        local facing = GetPlayerFacing()
        local sine, cosine = math.sin(facing), math.cos(facing)
        east, south = east * cosine - south * sine, east * sine + south * cosine
    end

    local zoom = Minimap:GetZoom()
    local yards = (indoors and MINIMAP_YARDS.indoor or MINIMAP_YARDS.outdoor)[zoom] or 466.6667
    local perYard = Minimap:GetWidth() / yards
    local x, y = east * perYard, -south * perYard
    local radius = Minimap:GetWidth() / 2 - 10
    local distance = math.sqrt(x * x + y * y)
    -- Out of the minimap's reach: on its edge, smaller, pointing the way
    local size = MINIMAP_PIN
    if distance > radius then
        x, y = x * radius / distance, y * radius / distance
        size = MINIMAP_PIN - 6
        minimapPin:SetAlpha(0.8)
    else
        minimapPin:SetAlpha(1)
    end
    local half = Minimap:GetWidth() / 2
    minimapPin:SetFrameLevel(Minimap:GetFrameLevel() + 5)
    PlacePin(minimapPin, Minimap, half + x, -half + y, size)
    minimapPin:Show()
end

local minimapClock = 0
local minimapWatcher = CreateFrame("Frame")
minimapWatcher:RegisterEvent("PLAYER_ENTERING_WORLD")
minimapWatcher:RegisterEvent("ZONE_CHANGED")
minimapWatcher:RegisterEvent("ZONE_CHANGED_INDOORS")
minimapWatcher:RegisterEvent("ZONE_CHANGED_NEW_AREA")
minimapWatcher:RegisterEvent("MINIMAP_UPDATE_ZOOM")
minimapWatcher:SetScript("OnEvent", function()
    if not WorldMapFrame:IsShown() then
        SetMapToCurrentZone()
    end
    UpdateIndoors()
    UpdateMinimapPin()
end)
WorldMapFrame:HookScript("OnHide", SetMapToCurrentZone)
minimapWatcher:SetScript("OnUpdate", function(_, elapsed)
    minimapClock = minimapClock + elapsed
    if minimapClock >= 0.1 then
        minimapClock = 0
        UpdateMinimapPin()
    end
end)
