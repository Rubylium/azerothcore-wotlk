-- The Infinite Dungeon's windows (server: mod-stat-growth src/infinite/InfiniteDungeonSystem.cpp), built from the
-- pieces of InfiniteDungeon.lua (InfiniteDungeonUI) in the challenge board's materials:
-- - Eternia's window, opened by talking to her in place of a gossip menu: the ladder the character climbs, its
--   checkpoint and record, the next descent (its step, paragon and item level at 80), go down alone, start over from
--   floor 1, go down with the group partner, the deepest floor per class, and what every floor holds
-- - the portal's choice, when the player walks into the portal: down (one floor, or two or three after a fast
--   clear: its destination and "+N"), or out of the dungeon
-- - the confirmation before leaving the dungeon (the tracker's button, or the portal's choice)
--
-- Protocol (prefix "Infinite", tab-separated, whispered to oneself):
--   server  KEEPER <ladder> <level> <checkpoint> <best> <checkpoint at 80> <best at 80> <start floor> <step>
--                  <paragon> <item level> <can start 0/1> <why not>
--           KEEPDUO <state> <partner> <partner class> <partner checkpoint> <ladder> <start floor> <why not>
--                   state: 0 nobody in the group, 1 ready, 2 the partner cannot come now
--           KEEPREC <class> <name> <floor>          one per class, deepest first
--           KEEPEND                                  opens (or refreshes) the window
--           KEEPER_CLOSE                             walked away from Eternia, or the descent began
--           PORTAL <next floor> <ladder> <step> <paragon> <gear floor 0/1> <checkpoint floor 0/1>
--                  <chest unopened 0/1> <players> <floors down>       floors down: 1-3 (missing: 1)
--           PORTALOFF                                stepped out of the portal
--   client  KEEPER_GO <solo|restart|duo|duorestart>, KEEPER_REFRESH, KEEPER_CLOSED, PORTAL_DESCEND, RUN_LEAVE
--
-- By hand (screenshots, tests), through InfiniteDungeon_Receive:
--   "KEEPER\t1\t80\t30\t34\t20\t27\t21\t4\t0\t216\t1\t", "KEEPDUO\t1\tAlexis\t2\t10\t1\t11\t",
--   "KEEPREC\t1\tGromm\t42", "KEEPEND", "PORTAL\t12\t1\t2\t0\t0\t0\t0\t1\t3"

local UI = InfiniteDungeonUI
if not UI then
    return
end

local Send, SetShown, SetAtlas, Colored = UI.Send, UI.SetShown, UI.SetAtlas, UI.Colored
local Tween, OutCubic, Rotate, RotateAtlas = UI.Tween, UI.OutCubic, UI.Rotate, UI.RotateAtlas
local Card, FramedIcon, Divider, Label, Heading, Bar = UI.Card, UI.FramedIcon, UI.Divider, UI.Label, UI.Heading, UI.Bar
local QuietButton, CloseButton = UI.QuietButton, UI.CloseButton
local ART, FRIZ = UI.ART, UI.FRIZ
local HEADING, GOLD, BORDER, PARCHMENT, SOFT = UI.HEADING, UI.GOLD, UI.BORDER, UI.PARCHMENT, UI.SOFT
local MUTED, AMBER, EMBER, PARAGON_PURPLE = UI.MUTED, UI.AMBER, UI.EMBER, UI.PARAGON_PURPLE
local CHECKPOINT_FLOORS, GEAR_FLOORS, LADDER_GEARING = UI.CHECKPOINT_FLOORS, UI.GEAR_FLOORS, UI.LADDER_GEARING

local WHITE = "Interface\\Buttons\\WHITE8X8"
local HIGHLIGHT = "Interface\\QuestFrame\\UI-QuestTitleHighlight"
-- The round class icons, a 4x4 sheet with the custom classes' cells too (the talent window's, TalentTree.lua): the
-- character creation sheet does not load in the game
local ROUND_CLASSES = "Interface\\Glues\\CharacterCreate\\RoundClasses"
local STOCK_CELLS = {
    WARRIOR = { 0, 0 }, PALADIN = { 0, 2 }, HUNTER = { 0, 1 }, ROGUE = { 2, 0 }, PRIEST = { 2, 1 },
    DEATHKNIGHT = { 1, 2 }, SHAMAN = { 1, 1 }, MAGE = { 1, 0 }, WARLOCK = { 3, 1 }, DRUID = { 3, 0 },
}
local DUO_ICON = "Interface\\Icons\\Spell_Holy_PrayerOfFortitude"
local BUBBLE_ICON = "Interface\\Icons\\Spell_Holy_PowerWordShield"
local CHECKPOINT_ICON = "Interface\\Icons\\INV_Misc_Rune_01"

-- The game's class ids (ChrClasses.dbc); the custom classes carry their own (CustomClasses.lua)
local CLASS_TOKENS = { "WARRIOR", "PALADIN", "HUNTER", "ROGUE", "PRIEST", "DEATHKNIGHT", "SHAMAN", "MAGE", "WARLOCK",
    nil, "DRUID" }

local TEXT = UI.french and {
    title = "Eternia",
    heading = "Donjon infini",
    subtitle = "Eternia, gardienne du Donjon infini",
    intro = "Des étages sans fin : quelques ennemis, un gardien, puis un portail vers le bas. Franchissez un étage "
        .. "vite et le portail vous fait descendre de deux ou trois étages.",
    ladderLevelling = "ÉCHELLE DE MONTÉE EN NIVEAU",
    ladderGearing = "ÉCHELLE D'ÉQUIPEMENT (NIVEAU 80)",
    checkpoint = "Point de passage : étage %d",
    noCheckpoint = "Pas encore de point de passage",
    record = "Record : étage %d",
    noRecord = "Aucun étage franchi pour l'instant",
    toCheckpoint = "Vers le point de passage de l'étage %d",
    nextDescent = "PROCHAINE DESCENTE",
    floor = "Étage %d",
    step = "Palier %d",
    paragon = "Parangon conseillé : %d",
    noParagon = "Aucun parangon requis",
    itemLevel = "Objets de niveau %d",
    gearAt = "Équipement à l'étage %d",
    checkpointAt = "Point de passage à l'étage %d",
    otherLevelling = "Montée en niveau : point de passage %d, record %d",
    otherGearing = "Équipement (niveau 80) : point de passage %d, record %d",
    gearingLocked = "L'échelle d'équipement s'ouvre au niveau 80",
    descend = "Descendre",
    descendAlone = "Seul, depuis l'étage %d",
    restart = "Recommencer à l'étage 1",
    duoTitle = "Descendre à deux",
    duoWith = "Avec %s · départ à l'étage %d",
    duoLowest = "Le plus bas des deux points de passage (le vôtre : %d, le sien : %d)",
    duoButton = "Descendre à deux",
    recordsTitle = "Les plus profonds",
    recordsSub = "par classe, au niveau 80",
    recordsEmpty = "Personne n'est encore descendu au niveau 80.\nLa première place attend.",
    floorShort = "étage %d",
    remindHeartsTitle = "Cœurs de soin",
    remindHearts = "45 % de vie, 30 % de mana",
    remindBubbleTitle = "Cercle de protection",
    remindBubble = "L'étage commence en sortant",
    remindCheckpointTitle = "Points de passage",
    remindCheckpoint = "Tous les 10 étages",
    remindGearTitle = "Équipement",
    remindGear = "Sûr tous les 5 étages, 20 % sinon",
    confirmTitle = "Recommencer à l'étage 1 ?",
    confirmText = "Votre point de passage (étage %d) est conservé : une descente normale repartira toujours de "
        .. "l'étage %d.",
    confirmSolo = "Recommencer seul",
    confirmDuo = "À deux",
    cancel = "Annuler",
    pending = "Le Donjon infini s'ouvre…",
    portalKicker = "LE PORTAIL",
    portalIntro = "Il mène un étage plus bas",
    portalSwift = "Descente rapide : il mène %d étages plus bas",
    portalKickerSwift = "LE PORTAIL · +%d",
    portalGear = "Son gardien offre une pièce d'équipement",
    portalCheckpoint = "Étage du point de passage",
    portalStep = "Palier %d · parangon conseillé %d",
    portalStepOnly = "Palier %d",
    portalChest = "Le coffre du point de passage n'est pas encore ouvert",
    portalDuo = "Votre partenaire descend avec vous",
    portalLeave = "Quitter le donjon",
    leaveTitle = "Quitter le Donjon infini ?",
    leaveSolo = "La descente s'achève ici et vous revenez à la surface. Votre point de passage est conservé.",
    leaveDuo = "Vous revenez à la surface ; votre partenaire peut poursuivre la descente. Votre point de passage "
        .. "est conservé.",
    leaveConfirm = "Quitter",
    leaveStay = "Rester",
} or {
    title = "Eternia",
    heading = "Infinite Dungeon",
    subtitle = "Eternia, keeper of the Infinite Dungeon",
    intro = "Floors without end: a few foes, a guardian, then a portal further down. Clear a floor quickly and the "
        .. "portal takes you two or three floors down.",
    ladderLevelling = "LEVELLING LADDER",
    ladderGearing = "GEARING LADDER (LEVEL 80)",
    checkpoint = "Checkpoint: floor %d",
    noCheckpoint = "No checkpoint yet",
    record = "Best: floor %d",
    noRecord = "No floor cleared yet",
    toCheckpoint = "Towards the checkpoint of floor %d",
    nextDescent = "NEXT DESCENT",
    floor = "Floor %d",
    step = "Step %d",
    paragon = "Recommended paragon: %d",
    noParagon = "No paragon needed",
    itemLevel = "Item level %d",
    gearAt = "Gear on floor %d",
    checkpointAt = "Checkpoint on floor %d",
    otherLevelling = "Levelling: checkpoint %d, best %d",
    otherGearing = "Gearing (level 80): checkpoint %d, best %d",
    gearingLocked = "The gearing ladder opens at level 80",
    descend = "Go down",
    descendAlone = "Alone, from floor %d",
    restart = "Start over from floor 1",
    duoTitle = "Go down as two",
    duoWith = "With %s · starting on floor %d",
    duoLowest = "The lower of the two checkpoints (yours: %d, theirs: %d)",
    duoButton = "Go down as two",
    recordsTitle = "The deepest",
    recordsSub = "by class, at level 80",
    recordsEmpty = "Nobody has gone down at level 80 yet.\nThe first place is waiting.",
    floorShort = "floor %d",
    remindHeartsTitle = "Healing hearts",
    remindHearts = "45% health, 30% mana",
    remindBubbleTitle = "Protective circle",
    remindBubble = "The floor starts as you leave it",
    remindCheckpointTitle = "Checkpoints",
    remindCheckpoint = "Every 10 floors",
    remindGearTitle = "Gear",
    remindGear = "Sure every 5 floors, else 20%",
    confirmTitle = "Start over from floor 1?",
    confirmText = "Your checkpoint (floor %d) is kept: an ordinary descent will always start again from floor %d.",
    confirmSolo = "Start over alone",
    confirmDuo = "As two",
    cancel = "Cancel",
    pending = "The Infinite Dungeon opens…",
    portalKicker = "THE PORTAL",
    portalIntro = "It leads one floor further down",
    portalSwift = "A swift descent: it leads %d floors down",
    portalKickerSwift = "THE PORTAL · +%d",
    portalGear = "Its guardian gives a piece of gear",
    portalCheckpoint = "A checkpoint floor",
    portalStep = "Step %d · recommended paragon %d",
    portalStepOnly = "Step %d",
    portalChest = "The checkpoint chest has not been opened yet",
    portalDuo = "Your partner comes down with you",
    portalLeave = "Leave the dungeon",
    leaveTitle = "Leave the Infinite Dungeon?",
    leaveSolo = "The descent ends here and you go back to the surface. Your checkpoint is kept.",
    leaveDuo = "You go back to the surface; your partner may go on down. Your checkpoint is kept.",
    leaveConfirm = "Leave",
    leaveStay = "Stay",
}

local function Number(value, default)
    return tonumber(value) or default or 0
end

local function ClassToken(classId)
    classId = tonumber(classId)
    if not classId then
        return nil
    end
    if CustomClasses and CustomClasses[classId] then
        return CustomClasses[classId].token
    end
    return CLASS_TOKENS[classId]
end

local function ClassColor(token)
    local color = token and RAID_CLASS_COLORS and RAID_CLASS_COLORS[token]
    if color then
        return { color.r, color.g, color.b }
    end
    return PARCHMENT
end

-- A class icon in its frame, from the round class icons (the custom classes' cells from CustomClasses)
local function SetClassIcon(holder, token)
    local cell = token and STOCK_CELLS[token]
    if not cell and token then
        for _, custom in pairs(CustomClasses or {}) do
            if custom.token == token then
                cell = custom.iconCell
            end
        end
    end
    if cell then
        holder.icon:SetTexture(ROUND_CLASSES)
        holder.icon:SetTexCoord(cell[1] / 4, (cell[1] + 1) / 4, cell[2] / 4, (cell[2] + 1) / 4)
    else
        holder.icon:SetTexture(DUO_ICON)
        holder.icon:SetTexCoord(0.06, 0.94, 0.06, 0.94)
    end
end

-- Building blocks ---------------------------------------------------------------------------------------------------

-- The board's window: rock, a tinted parchment over it, the metal frame, the title, the close button, dragged by
-- its top edge; Escape closes it
local function Window(name, width, height, title)
    local frame = CreateFrame("Frame", name, UIParent)
    frame:SetSize(width, height)
    frame:SetFrameStrata("HIGH")
    frame:SetToplevel(true)
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:Hide()
    tinsert(UISpecialFrames, name)

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

    local windowTitle = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    windowTitle:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -4)
    windowTitle:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -26, -4)
    windowTitle:SetJustifyH("CENTER")
    windowTitle:SetText(title)
    windowTitle:SetTextColor(1, 0.82, 0)
    frame.windowTitle = windowTitle

    CloseButton(frame, function() frame:Hide() end)

    local mover = CreateFrame("Frame", nil, frame)
    mover:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -40, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(frame:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)
    return frame
end

-- Opens a window the board's way: a fade and a slight growth, OutCubic
local function OpenWindow(frame, key)
    frame:SetAlpha(0)
    frame:SetScale(0.94)
    frame:Show()
    Tween(0.28, 0, function(p)
        frame:SetAlpha(p)
        frame:SetScale(0.94 + 0.06 * OutCubic(p))
    end, nil, key)
end

-- The emblem on its breathing glow, the swirl turning over it (and the star behind, when wanted)
local function Emblem(frame, size, withStar)
    local glow = frame:CreateTexture(nil, "ARTWORK")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetSize(size * 2.4, size * 2.4)
    glow:SetBlendMode("ADD")
    local star
    if withStar then
        star = frame:CreateTexture(nil, "ARTWORK", nil, 1)
        SetAtlas(star, "ChallengeMode-SpikeyStar")
        star:SetSize(size * 1.9, size * 1.9)
        star:SetBlendMode("ADD")
        star:SetAlpha(0.35)
        star:SetPoint("CENTER", glow, "CENTER")
    end
    local emblem = frame:CreateTexture(nil, "OVERLAY")
    emblem:SetTexture(ART .. "Emblem")
    emblem:SetSize(size, size)
    emblem:SetPoint("CENTER", glow, "CENTER")
    local swirl = frame:CreateTexture(nil, "OVERLAY", nil, 1)
    swirl:SetTexture(ART .. "Swirl")
    swirl:SetSize(size * 0.74, size * 0.74)
    swirl:SetBlendMode("ADD")
    swirl:SetAlpha(0.7)
    swirl:SetPoint("CENTER", glow, "CENTER")
    return { glow = glow, star = star, emblem = emblem, swirl = swirl }
end

local function AnimateEmblem(art, now)
    Rotate(art.swirl, -now * 0.9)
    art.glow:SetAlpha(0.5 + 0.22 * math.sin(now * 1.8))
    if art.star then
        RotateAtlas(art.star, "ChallengeMode-SpikeyStar", now * 0.25)
    end
end

-- The prominent action: a dark-gold plate in a gold edge, its name in the readable font, on a warm glow that
-- breathes. Muted and silent when it cannot be used (it keeps its mouse, so its tooltip can say why).
local function GoldButton(parent, width, height, fontSize)
    local button = CreateFrame("Button", nil, parent)
    button:SetSize(width, height)
    button:SetBackdrop({
        bgFile = WHITE,
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    button:SetBackdropColor(0.08, 0.05, 0.02, 1)

    local glow = parent:CreateTexture(nil, "ARTWORK")
    SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("TOPLEFT", button, "TOPLEFT", -34, 26)
    glow:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 34, -26)
    button.glow = glow

    local fill = button:CreateTexture(nil, "BORDER")
    fill:SetTexture(WHITE)
    fill:SetPoint("TOPLEFT", 4, -4)
    fill:SetPoint("BOTTOMRIGHT", -4, 4)
    button.fill = fill
    local shine = button:CreateTexture(nil, "ARTWORK")
    shine:SetTexture(WHITE)
    shine:SetPoint("TOPLEFT", 4, -4)
    shine:SetPoint("TOPRIGHT", -4, -4)
    shine:SetHeight(height * 0.45)
    shine:SetGradientAlpha("VERTICAL", 1, 0.9, 0.6, 0, 1, 0.9, 0.6, 0.12)
    button.shine = shine

    local text = button:CreateFontString(nil, "OVERLAY")
    text:SetFont(FRIZ, fontSize or 15)
    text:SetShadowOffset(1, -1)
    text:SetPoint("CENTER", button, "CENTER", 0, 0)
    button.text = text

    local highlight = button:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetTexture(HIGHLIGHT)
    highlight:SetBlendMode("ADD")
    highlight:SetPoint("TOPLEFT", 4, -4)
    highlight:SetPoint("BOTTOMRIGHT", -4, 4)
    highlight:SetAlpha(0.5)
    button.highlight = highlight

    button:SetScript("OnMouseDown", function(self)
        if self.active then
            text:SetPoint("CENTER", self, "CENTER", 1, -1)
        end
    end)
    button:SetScript("OnMouseUp", function(self)
        text:SetPoint("CENTER", self, "CENTER", 0, 0)
    end)

    function button:SetLabel(label)
        text:SetText(label)
    end

    function button:SetActive(active)
        self.active = active
        if active then
            self:SetBackdropBorderColor(GOLD[1], GOLD[2], GOLD[3], 1)
            fill:SetGradient("VERTICAL", 0.2, 0.12, 0.03, 0.46, 0.31, 0.09)
            text:SetTextColor(HEADING[1], HEADING[2], HEADING[3])
            highlight:SetAlpha(0.5)
            shine:Show()
            glow:Show()
        else
            self:SetBackdropBorderColor(MUTED[1], MUTED[2], MUTED[3], 0.8)
            fill:SetGradient("VERTICAL", 0.07, 0.06, 0.05, 0.14, 0.12, 0.1)
            text:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
            highlight:SetAlpha(0.08)
            shine:Hide()
            glow:Hide()
        end
    end

    function button:Breathe(now)
        if self.active then
            glow:SetAlpha(0.35 + 0.2 * math.sin(now * 2.2))
        end
    end

    button:SetActive(true)
    return button
end

-- The secondary actions: the quiet plate of InfiniteDungeon.lua, in the gold button's family (never a stock red
-- button beside it)
local function PanelButton(parent, width, height, label)
    return QuietButton(parent, width, height, label)
end

local function SetButtonEnabled(button, enabled)
    if enabled then
        button:Enable()
    else
        button:Disable()
    end
end

local function Tooltip(region, title, line)
    region:SetScript("OnEnter", function(self)
        local text = type(line) == "function" and line() or line
        if not text or text == "" then
            return
        end
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(title, HEADING[1], HEADING[2], HEADING[3])
        GameTooltip:AddLine(text, SOFT[1], SOFT[2], SOFT[3], true)
        GameTooltip:Show()
    end)
    region:SetScript("OnLeave", function() GameTooltip:Hide() end)
end

-- -----------------------------------------------------------------------------------------------------------------
-- Eternia's window
-- -----------------------------------------------------------------------------------------------------------------

local WIDTH, HEIGHT = 700, 606
local LEFT_X, LEFT_WIDTH = 22, 404
local RIGHT_X = LEFT_X + LEFT_WIDTH + 12
local RIGHT_WIDTH = WIDTH - RIGHT_X - 22
local RECORD_ROWS = 10

local keeper = { ladder = 0, level = 0, checkpoint = { 0, 0 }, best = { 0, 0 }, start = 1, step = 0, paragon = 0,
    itemLevel = 0, can = false, reason = "" }
local duo = { state = 0, name = "", class = 0, checkpoint = 0, ladder = 0, start = 0, reason = "" }
local records = {}
local incoming = {}             -- the records of the message being read, until KEEPEND
local pending = false           -- a descent was asked for: the buttons wait for the server
local closedByServer = false
local window

local function Ask(mode)
    if pending then
        return
    end
    pending = true
    PlaySound("igMainMenuOptionCheckBoxOn")
    Send("KEEPER_GO\t" .. mode)
    window.refresh()
end

local function ReminderItem(parent, icon, title, text)
    local item = CreateFrame("Frame", nil, parent)
    item:SetSize(160, 36)
    local holder = FramedIcon(item, 30, icon)
    holder:SetPoint("LEFT", item, "LEFT", 0, 0)
    local heading = Label(item, "GameFontNormalSmall", GOLD)
    heading:SetPoint("TOPLEFT", holder, "TOPRIGHT", 7, -2)
    heading:SetPoint("RIGHT", item, "RIGHT", 0, 0)
    heading:SetText(title)
    local line = Label(item, "GameFontHighlightSmall", SOFT)
    line:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -2)
    line:SetPoint("RIGHT", item, "RIGHT", 0, 0)
    line:SetText(text)
    return item
end

-- An information line with a small framed icon
local function InfoLine(parent, icon)
    local row = CreateFrame("Frame", nil, parent)
    row:SetSize(186, 22)
    row.icon = FramedIcon(row, 20, icon)
    row.icon:SetPoint("LEFT", row, "LEFT", 0, 0)
    row.text = Label(row, "GameFontHighlightSmall", PARCHMENT)
    row.text:SetPoint("LEFT", row.icon, "RIGHT", 6, 0)
    row.text:SetPoint("RIGHT", row, "RIGHT", 0, 0)
    return row
end

local function CreateLadderCard(frame)
    local card = CreateFrame("Frame", nil, frame)
    card:SetPoint("TOPLEFT", frame, "TOPLEFT", LEFT_X, -134)
    card:SetSize(LEFT_WIDTH, 214)
    Card(card)

    card.kicker = Label(card, "GameFontNormalSmall", GOLD)
    card.kicker:SetPoint("TOPLEFT", card, "TOPLEFT", 16, -14)

    card.ladderIcon = FramedIcon(card, 40)
    card.ladderIcon:SetPoint("TOPRIGHT", card, "TOPRIGHT", -14, -12)
    local iconGlow = card:CreateTexture(nil, "BORDER")
    SetAtlas(iconGlow, "ChallengeMode-SoftYellowGlow")
    iconGlow:SetBlendMode("ADD")
    iconGlow:SetAlpha(0.35)
    iconGlow:SetPoint("TOPLEFT", card.ladderIcon, "TOPLEFT", -18, 18)
    iconGlow:SetPoint("BOTTOMRIGHT", card.ladderIcon, "BOTTOMRIGHT", 18, -18)

    card.checkpoint = Heading(card, 22, PARCHMENT)
    card.checkpoint:SetPoint("TOPLEFT", card.kicker, "BOTTOMLEFT", 0, -6)
    card.record = Label(card, "GameFontHighlight", SOFT)
    card.record:SetPoint("TOPLEFT", card.checkpoint, "BOTTOMLEFT", 0, -4)

    card.bar = Bar(card, LEFT_WIDTH - 32, CHECKPOINT_FLOORS)
    card.bar:SetPoint("TOPLEFT", card, "TOPLEFT", 16, -88)
    card.barLabel = Label(card, "GameFontHighlightSmall", SOFT)
    card.barLabel:SetPoint("TOPLEFT", card.bar, "BOTTOMLEFT", 0, -4)
    card.barCount = Label(card, "GameFontHighlightSmall", PARCHMENT, "RIGHT")
    card.barCount:SetPoint("TOPRIGHT", card.bar, "BOTTOMRIGHT", 0, -4)

    local divider = Divider(card)
    divider:SetPoint("TOPLEFT", card, "TOPLEFT", 8, -120)
    divider:SetPoint("TOPRIGHT", card, "TOPRIGHT", -8, -120)

    local nextLabel = Label(card, "GameFontNormalSmall", GOLD)
    nextLabel:SetPoint("TOPLEFT", card, "TOPLEFT", 16, -134)
    nextLabel:SetText(TEXT.nextDescent)
    card.nextFloor = Heading(card, 24, HEADING)
    card.nextFloor:SetPoint("TOPLEFT", nextLabel, "BOTTOMLEFT", 0, -3)
    card.nextStep = Label(card, "GameFontHighlightSmall", AMBER)
    card.nextStep:SetPoint("TOPLEFT", card.nextFloor, "BOTTOMLEFT", 0, -1)

    card.info1 = InfoLine(card)
    card.info1:SetPoint("TOPLEFT", card, "TOPLEFT", LEFT_WIDTH - 200, -134)
    card.info2 = InfoLine(card)
    card.info2:SetPoint("TOPLEFT", card.info1, "BOTTOMLEFT", 0, -4)

    card.other = Label(card, "GameFontDisableSmall", MUTED)
    card.other:SetPoint("BOTTOMLEFT", card, "BOTTOMLEFT", 16, 10)
    card.other:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", -16, 10)
    return card
end

local function CreateActions(frame)
    local actions = CreateFrame("Frame", nil, frame)
    actions:SetPoint("TOPLEFT", frame, "TOPLEFT", LEFT_X, -356)
    actions:SetSize(LEFT_WIDTH, 64)

    local descend = GoldButton(actions, 206, 40, 17)
    descend:SetPoint("TOPLEFT", actions, "TOPLEFT", 4, -4)
    descend:SetLabel(TEXT.descend)
    descend:SetScript("OnClick", function(self)
        if self.active then
            Ask("solo")
        else
            PlaySound("igQuestFailed")
        end
    end)
    Tooltip(descend, TEXT.descend, function() return not descend.active and keeper.reason or nil end)
    actions.descend = descend

    actions.caption = Label(actions, "GameFontHighlightSmall", SOFT, "CENTER")
    actions.caption:SetPoint("TOP", descend, "BOTTOM", 0, -5)

    local restart = PanelButton(actions, 176, 24, TEXT.restart)
    restart:SetPoint("LEFT", descend, "RIGHT", 16, 0)
    restart:SetScript("OnClick", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        window.showConfirm(true)
    end)
    actions.restart = restart

    actions.reason = Label(actions, "GameFontNormalSmall", EMBER)
    actions.reason:SetPoint("TOPLEFT", restart, "BOTTOMLEFT", 0, -6)
    actions.reason:SetPoint("RIGHT", actions, "RIGHT", -4, 0)
    actions.reason:SetHeight(28)
    actions.reason:SetJustifyV("TOP")
    return actions
end

local function CreateDuoCard(frame)
    local card = CreateFrame("Frame", nil, frame)
    card:SetPoint("TOPLEFT", frame, "TOPLEFT", LEFT_X, -428)
    card:SetSize(LEFT_WIDTH, 80)
    Card(card)

    card.icon = FramedIcon(card, 42)
    card.icon:SetPoint("LEFT", card, "LEFT", 14, 0)
    card.title = Label(card, "GameFontNormal", GOLD)
    card.title:SetPoint("TOPLEFT", card.icon, "TOPRIGHT", 10, 0)
    card.title:SetText(TEXT.duoTitle)
    card.line = Label(card, "GameFontHighlightSmall", PARCHMENT)
    card.line:SetPoint("TOPLEFT", card.title, "BOTTOMLEFT", 0, -3)
    card.line:SetPoint("RIGHT", card, "RIGHT", -150, 0)
    card.detail = Label(card, "GameFontHighlightSmall", MUTED)
    card.detail:SetPoint("TOPLEFT", card.line, "BOTTOMLEFT", 0, -2)
    card.detail:SetPoint("RIGHT", card, "RIGHT", -150, 0)
    card.detail:SetHeight(26)
    card.detail:SetJustifyV("TOP")

    card.button = PanelButton(card, 132, 24, TEXT.duoButton)
    card.button:SetPoint("RIGHT", card, "RIGHT", -12, 0)
    card.button:SetScript("OnClick", function() Ask("duo") end)
    return card
end

local function RecordRow(parent, index)
    local row = CreateFrame("Frame", nil, parent)
    row:SetSize(RIGHT_WIDTH - 20, 28)
    if index % 2 == 0 then
        local band = row:CreateTexture(nil, "BACKGROUND")
        band:SetTexture(WHITE)
        band:SetAllPoints()
        band:SetVertexColor(1, 0.9, 0.7, 0.04)
    end
    row.rank = Label(row, "GameFontNormal", MUTED, "RIGHT")
    row.rank:SetPoint("LEFT", row, "LEFT", 4, 0)
    row.rank:SetWidth(18)
    row.rank:SetJustifyH("RIGHT")
    row.rank:SetText(tostring(index))
    row.icon = FramedIcon(row, 22)
    row.icon:SetPoint("LEFT", row, "LEFT", 28, 0)
    row.floor = Label(row, "GameFontHighlightSmall", PARCHMENT, "RIGHT")
    row.floor:SetPoint("RIGHT", row, "RIGHT", -6, 0)
    row.name = Label(row, "GameFontHighlight", PARCHMENT)
    row.name:SetPoint("LEFT", row.icon, "RIGHT", 7, 0)
    row.name:SetPoint("RIGHT", row.floor, "LEFT", -6, 0)
    row.mine = row:CreateTexture(nil, "BORDER")
    SetAtlas(row.mine, "ChallengeMode-SoftYellowGlow")
    row.mine:SetBlendMode("ADD")
    row.mine:SetPoint("TOPLEFT", row, "TOPLEFT", -6, 6)
    row.mine:SetPoint("BOTTOMRIGHT", row, "BOTTOMRIGHT", 6, -6)
    row.mine:SetAlpha(0.25)
    row.mine:Hide()
    return row
end

local function CreateRecordsCard(frame)
    local card = CreateFrame("Frame", nil, frame)
    card:SetPoint("TOPLEFT", frame, "TOPLEFT", RIGHT_X, -134)
    card:SetSize(RIGHT_WIDTH, 374)
    Card(card)

    local title = Heading(card, 20, HEADING)
    title:SetPoint("TOP", card, "TOP", 0, -12)
    title:SetText(TEXT.recordsTitle)
    local sub = Label(card, "GameFontHighlightSmall", SOFT, "CENTER")
    sub:SetPoint("TOP", title, "BOTTOM", 0, -2)
    sub:SetText(TEXT.recordsSub)
    local divider = Divider(card)
    divider:SetPoint("TOPLEFT", card, "TOPLEFT", 8, -54)
    divider:SetPoint("TOPRIGHT", card, "TOPRIGHT", -8, -54)

    card.rows = {}
    for index = 1, RECORD_ROWS do
        local row = RecordRow(card, index)
        row:SetPoint("TOPLEFT", card, "TOPLEFT", 10, -66 - (index - 1) * 28)
        card.rows[index] = row
    end
    card.empty = Label(card, "GameFontHighlightSmall", MUTED, "CENTER")
    card.empty:SetPoint("TOP", card, "TOP", 0, -140)
    card.empty:SetWidth(RIGHT_WIDTH - 30)
    card.empty:SetText(TEXT.recordsEmpty)
    return card
end

local function CreateConfirm(frame)
    local confirm = CreateFrame("Frame", nil, frame)
    confirm:SetSize(LEFT_WIDTH, 152)
    confirm:SetFrameStrata("DIALOG")
    confirm:SetFrameLevel(frame:GetFrameLevel() + 30)
    confirm:EnableMouse(true)
    Card(confirm, 0.98)
    -- Solid: the buttons under it must not show through
    local solid = confirm:CreateTexture(nil, "BACKGROUND", nil, -8)
    solid:SetPoint("TOPLEFT", 4, -4)
    solid:SetPoint("BOTTOMRIGHT", -4, 4)
    solid:SetTexture(0.035, 0.028, 0.02, 1)
    confirm:Hide()

    local title = Heading(confirm, 20, AMBER)
    title:SetPoint("TOPLEFT", confirm, "TOPLEFT", 18, -16)
    title:SetText(TEXT.confirmTitle)
    confirm.text = Label(confirm, "GameFontHighlight", SOFT)
    confirm.text:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -8)
    confirm.text:SetWidth(LEFT_WIDTH - 36)
    confirm.text:SetJustifyH("LEFT")

    confirm.solo = PanelButton(confirm, 136, 24, TEXT.confirmSolo)
    confirm.solo:SetPoint("BOTTOMLEFT", confirm, "BOTTOMLEFT", 16, 16)
    confirm.solo:SetScript("OnClick", function() Ask("restart") end)
    confirm.duo = PanelButton(confirm, 110, 24, TEXT.confirmDuo)
    confirm.duo:SetPoint("LEFT", confirm.solo, "RIGHT", 8, 0)
    confirm.duo:SetScript("OnClick", function() Ask("duorestart") end)
    confirm.cancel = PanelButton(confirm, 110, 24, TEXT.cancel)
    confirm.cancel:SetPoint("BOTTOMRIGHT", confirm, "BOTTOMRIGHT", -16, 16)
    confirm.cancel:SetScript("OnClick", function() window.showConfirm(false) end)
    return confirm
end

local function CreateKeeperWindow()
    window = Window("InfiniteDungeonKeeperFrame", WIDTH, HEIGHT, TEXT.title)
    window:SetPoint("CENTER", UIParent, "CENTER", 0, 20)

    -- The header: the emblem on its star and glow, the name, Eternia, what the dungeon is
    local art = Emblem(window, 76, true)
    art.glow:SetPoint("CENTER", window, "TOPLEFT", 78, -80)
    window.art = art
    local heading = Heading(window, 30, HEADING)
    heading:SetPoint("TOPLEFT", window, "TOPLEFT", 140, -40)
    heading:SetText(TEXT.heading)
    local subtitle = Label(window, "GameFontNormal", GOLD)
    subtitle:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 2, -2)
    subtitle:SetText(TEXT.subtitle)
    local intro = Label(window, "GameFontHighlightSmall", SOFT)
    intro:SetPoint("TOPLEFT", subtitle, "BOTTOMLEFT", 0, -6)
    intro:SetWidth(WIDTH - 170)
    intro:SetText(TEXT.intro)
    local divider = Divider(window)
    divider:SetPoint("TOPLEFT", window, "TOPLEFT", 20, -122)
    divider:SetPoint("TOPRIGHT", window, "TOPRIGHT", -20, -122)

    window.ladder = CreateLadderCard(window)
    window.actions = CreateActions(window)
    window.duo = CreateDuoCard(window)
    window.records = CreateRecordsCard(window)
    window.confirm = CreateConfirm(window)

    -- What every floor holds, along the bottom
    local bottom = Divider(window)
    bottom:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", 20, 62)
    bottom:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -20, 62)
    local reminders = {
        { ART .. "Heart", TEXT.remindHeartsTitle, TEXT.remindHearts },
        { BUBBLE_ICON, TEXT.remindBubbleTitle, TEXT.remindBubble },
        { CHECKPOINT_ICON, TEXT.remindCheckpointTitle, TEXT.remindCheckpoint },
        { UI.GEAR_ICON, TEXT.remindGearTitle, TEXT.remindGear },
    }
    local slot = (WIDTH - 44) / #reminders
    for index, reminder in ipairs(reminders) do
        local item = ReminderItem(window, reminder[1], reminder[2], reminder[3])
        item:SetWidth(slot - 10)
        item:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", 24 + (index - 1) * slot, 20)
    end

    window.showConfirm = function(show)
        local confirm = window.confirm
        if show then
            confirm:Show()
            Tween(0.25, 0, function(p)
                confirm:SetPoint("TOPLEFT", window, "TOPLEFT", LEFT_X, -356 - 30 * (1 - OutCubic(p)))
                confirm:SetAlpha(p)
            end, nil, "keeperConfirm")
        elseif confirm:IsShown() then
            Tween(0.18, 0, function(p)
                confirm:SetAlpha(1 - p)
            end, function() confirm:Hide() end, "keeperConfirm")
        end
    end

    window:SetScript("OnUpdate", function()
        local now = GetTime()
        AnimateEmblem(art, now)
        window.actions.descend:Breathe(now)
    end)
    window:SetScript("OnHide", function()
        window.confirm:Hide()
        pending = false
        PlaySound("igCharacterInfoClose")
        -- The server stops watching the distance
        if closedByServer then
            closedByServer = false
        else
            Send("KEEPER_CLOSED")
        end
    end)
    window:RegisterEvent("PARTY_MEMBERS_CHANGED")
    window:SetScript("OnEvent", function()
        if window:IsShown() and not pending then
            Send("KEEPER_REFRESH")
        end
    end)
end

local function RefreshKeeper()
    local gearing = keeper.ladder == LADDER_GEARING
    local index = gearing and 2 or 1
    local checkpoint, best = keeper.checkpoint[index], keeper.best[index]

    -- The ladder card
    local card = window.ladder
    card.kicker:SetText(gearing and TEXT.ladderGearing or TEXT.ladderLevelling)
    card.ladderIcon.icon:SetTexture(gearing and UI.GEAR_ICON or UI.EXPERIENCE_ICON)
    card.checkpoint:SetText(checkpoint > 0 and format(TEXT.checkpoint, checkpoint) or TEXT.noCheckpoint)
    card.record:SetText(best > 0 and format(TEXT.record, best) or TEXT.noRecord)
    local nextCheckpoint = checkpoint + CHECKPOINT_FLOORS
    local inBlock = max(0, min(CHECKPOINT_FLOORS, best - checkpoint))
    card.barLabel:SetText(format(TEXT.toCheckpoint, nextCheckpoint))
    card.barCount:SetText(format("%d/%d", inBlock, CHECKPOINT_FLOORS))
    card.bar:SetValue(inBlock / CHECKPOINT_FLOORS, window:IsShown())

    card.nextFloor:SetText(format(TEXT.floor, keeper.start))
    local gearFloor = ceil(keeper.start / GEAR_FLOORS) * GEAR_FLOORS
    local checkpointFloor = ceil(keeper.start / CHECKPOINT_FLOORS) * CHECKPOINT_FLOORS
    if gearing then
        card.nextStep:SetText(format(TEXT.step, keeper.step + 1))
        card.info1.icon.icon:SetTexture(UI.PARAGON_ICON)
        if keeper.paragon > 0 then
            card.info1.text:SetText(Colored(format(TEXT.paragon, keeper.paragon), PARAGON_PURPLE))
        else
            card.info1.text:SetText(Colored(TEXT.noParagon, SOFT))
        end
        card.info2.icon.icon:SetTexture(UI.GEAR_ICON)
        card.info2.text:SetText(format(TEXT.itemLevel, keeper.itemLevel))
    else
        card.nextStep:SetText("")
        card.info1.icon.icon:SetTexture(UI.GEAR_ICON)
        card.info1.text:SetText(format(TEXT.gearAt, gearFloor))
        card.info2.icon.icon:SetTexture(CHECKPOINT_ICON)
        card.info2.text:SetText(format(TEXT.checkpointAt, checkpointFloor))
    end

    -- The other ladder, in a line
    if gearing then
        card.other:SetText(format(TEXT.otherLevelling, keeper.checkpoint[1], keeper.best[1]))
    elseif keeper.best[2] > 0 then
        card.other:SetText(format(TEXT.otherGearing, keeper.checkpoint[2], keeper.best[2]))
    else
        card.other:SetText(TEXT.gearingLocked)
    end

    -- The actions
    local actions = window.actions
    local can = keeper.can and not pending
    actions.descend:SetActive(can)
    actions.caption:SetText(pending and Colored(TEXT.pending, GOLD) or format(TEXT.descendAlone, keeper.start))
    SetButtonEnabled(actions.restart, can and keeper.start > 1)
    actions.reason:SetText(not keeper.can and keeper.reason or "")

    -- The duo
    local duoCard = window.duo
    local token = duo.state ~= 0 and ClassToken(duo.class) or nil
    SetClassIcon(duoCard.icon, token)
    if duo.state == 1 then
        duoCard.icon:SetAlpha(1)
        duoCard.title:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        duoCard.line:SetText(format(TEXT.duoWith, Colored(duo.name, ClassColor(token)), duo.start))
        duoCard.detail:SetText(format(TEXT.duoLowest, keeper.checkpoint[duo.ladder == LADDER_GEARING and 2 or 1],
            duo.checkpoint))
        duoCard.detail:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    else
        duoCard.icon:SetAlpha(0.55)
        duoCard.title:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        duoCard.line:SetText(duo.state == 2 and Colored(duo.name, ClassColor(token)) or "")
        duoCard.detail:SetText(duo.reason)
        duoCard.detail:SetTextColor(SOFT[1], SOFT[2], SOFT[3])
        if duo.state ~= 2 then
            -- No one named: the reason takes the name's line
            duoCard.line:SetText(Colored(duo.reason, SOFT))
            duoCard.detail:SetText("")
        end
    end
    SetButtonEnabled(duoCard.button, can and duo.state == 1)

    -- The records
    local recordsCard = window.records
    local me = UnitName("player")
    for rowIndex, row in ipairs(recordsCard.rows) do
        local record = records[rowIndex]
        if record then
            local recordToken = ClassToken(record.class)
            SetClassIcon(row.icon, recordToken)
            row.name:SetText(Colored(record.name, ClassColor(recordToken)))
            row.floor:SetText(format(TEXT.floorShort, record.floor))
            local color = rowIndex == 1 and HEADING or MUTED
            row.rank:SetTextColor(color[1], color[2], color[3])
            SetShown(row.mine, record.name == me)
            row:Show()
        else
            row:Hide()
        end
    end
    SetShown(recordsCard.empty, #records == 0)

    -- The confirmation's text and duo button
    local confirm = window.confirm
    local keptCheckpoint = keeper.checkpoint[index]
    confirm.text:SetText(format(TEXT.confirmText, keptCheckpoint, keptCheckpoint + 1))
    SetShown(confirm.duo, duo.state == 1)
    SetButtonEnabled(confirm.solo, can)
    SetButtonEnabled(confirm.duo, can and duo.state == 1)
end

local function OpenKeeper()
    if not window then
        CreateKeeperWindow()
        window.refresh = RefreshKeeper
    end
    pending = false
    RefreshKeeper()
    if not window:IsShown() then
        window.confirm:Hide()
        OpenWindow(window, "keeperOpen")
        PlaySound("igCharacterInfoOpen")
        -- The rows come in one after the other
        for index, row in ipairs(window.records.rows) do
            if row:IsShown() then
                row:SetAlpha(0)
                Tween(0.25, 0.12 + 0.04 * index, function(p) row:SetAlpha(OutCubic(p)) end, nil, row)
            end
        end
    end
end

local function CloseKeeper()
    if window and window:IsShown() then
        closedByServer = true
        window:Hide()
    end
end

local handlers = UI.handlers

handlers.KEEPER = function(ladder, level, checkpoint, best, checkpointMax, bestMax, start, step, paragon, itemLevel,
        can, reason)
    keeper.ladder = Number(ladder)
    keeper.level = Number(level)
    keeper.checkpoint = { Number(checkpoint), Number(checkpointMax) }
    keeper.best = { Number(best), Number(bestMax) }
    keeper.start = Number(start, 1)
    keeper.step = Number(step)
    keeper.paragon = Number(paragon)
    keeper.itemLevel = Number(itemLevel)
    keeper.can = can == "1"
    keeper.reason = reason or ""
    incoming = {}
end

handlers.KEEPDUO = function(state, name, class, checkpoint, ladder, start, reason)
    duo.state = Number(state)
    duo.name = name or ""
    duo.class = Number(class)
    duo.checkpoint = Number(checkpoint)
    duo.ladder = Number(ladder)
    duo.start = Number(start)
    duo.reason = reason or ""
end

handlers.KEEPREC = function(class, name, floor)
    tinsert(incoming, { class = Number(class), name = name or "", floor = Number(floor) })
end

handlers.KEEPEND = function()
    records = incoming
    incoming = {}
    OpenKeeper()
end

handlers.KEEPER_CLOSE = CloseKeeper

-- -----------------------------------------------------------------------------------------------------------------
-- The portal's choice, and leaving the dungeon
-- -----------------------------------------------------------------------------------------------------------------

local portal, leave
local lastPortal

local function CreatePortal()
    portal = Window("InfiniteDungeonPortalFrame", 380, 262, TEXT.heading)
    portal:SetPoint("CENTER", UIParent, "CENTER", 0, 110)

    local art = Emblem(portal, 56, true)
    art.glow:SetPoint("CENTER", portal, "TOP", 0, -66)
    portal.art = art

    local kicker = Label(portal, "GameFontNormalSmall", GOLD, "CENTER")
    kicker:SetPoint("TOP", portal, "TOP", 0, -102)
    kicker:SetText(TEXT.portalKicker)
    portal.kicker = kicker
    portal.title = Heading(portal, 28, HEADING)
    portal.title:SetPoint("TOP", kicker, "BOTTOM", 0, -2)
    local divider = Divider(portal, "OVERLAY")
    divider:SetWidth(240)
    divider:SetPoint("TOP", portal.title, "BOTTOM", 0, -1)
    portal.line = Label(portal, "GameFontHighlightSmall", SOFT, "CENTER")
    portal.line:SetPoint("TOP", divider, "BOTTOM", 0, -1)
    portal.line:SetWidth(340)
    portal.detail = Label(portal, "GameFontHighlightSmall", AMBER, "CENTER")
    portal.detail:SetPoint("TOP", portal.line, "BOTTOM", 0, -3)
    portal.detail:SetWidth(340)

    portal.descend = GoldButton(portal, 170, 36, 16)
    portal.descend:SetPoint("BOTTOMLEFT", portal, "BOTTOMLEFT", 26, 20)
    portal.descend:SetLabel(TEXT.descend)
    portal.descend:SetScript("OnClick", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        Send("PORTAL_DESCEND")
        portal:Hide()
    end)
    portal.leave = PanelButton(portal, 140, 24, TEXT.portalLeave)
    portal.leave:SetPoint("BOTTOMRIGHT", portal, "BOTTOMRIGHT", -26, 26)
    portal.leave:SetScript("OnClick", function()
        portal:Hide()
        UI.ConfirmLeave()
    end)

    portal:SetScript("OnUpdate", function()
        local now = GetTime()
        AnimateEmblem(art, now)
        portal.descend:Breathe(now)
    end)
    portal:SetScript("OnHide", function() PlaySound("igQuestListClose") end)
end

local function ShowPortal(data)
    if not portal then
        CreatePortal()
    end
    portal.title:SetText(format(TEXT.floor, data.next))
    local swift = data.down > 1
    portal.kicker:SetText(swift and format(TEXT.portalKickerSwift, data.down) or TEXT.portalKicker)

    local lines = { swift and Colored(format(TEXT.portalSwift, data.down), GOLD) or TEXT.portalIntro }
    if data.ladder == LADDER_GEARING then
        tinsert(lines, data.paragon > 0 and format(TEXT.portalStep, data.step + 1, data.paragon) or
            format(TEXT.portalStepOnly, data.step + 1))
    end
    portal.line:SetText(table.concat(lines, "  ·  "))

    local details = {}
    if data.checkpoint then
        tinsert(details, Colored(TEXT.portalCheckpoint, GOLD))
    end
    if data.gear then
        tinsert(details, Colored(TEXT.portalGear, GOLD))
    end
    if data.chest then
        tinsert(details, Colored(TEXT.portalChest, EMBER))
    end
    if data.players > 1 then
        tinsert(details, Colored(TEXT.portalDuo, MUTED))
    end
    portal.detail:SetText(table.concat(details, "\n"))

    if leave and leave:IsShown() then
        leave:Hide()
    end
    if not portal:IsShown() then
        OpenWindow(portal, "portalOpen")
        PlaySound("igQuestListOpen")
    end
end

local function CreateLeave()
    leave = Window("InfiniteDungeonLeaveFrame", 380, 196, TEXT.heading)
    leave:SetPoint("CENTER", UIParent, "CENTER", 0, 110)

    local title = Heading(leave, 22, AMBER)
    title:SetPoint("TOP", leave, "TOP", 0, -42)
    title:SetText(TEXT.leaveTitle)
    local divider = Divider(leave, "OVERLAY")
    divider:SetWidth(260)
    divider:SetPoint("TOP", title, "BOTTOM", 0, -2)
    leave.text = Label(leave, "GameFontHighlight", SOFT, "CENTER")
    leave.text:SetPoint("TOP", divider, "BOTTOM", 0, -4)
    leave.text:SetWidth(330)

    local confirm = PanelButton(leave, 140, 26, TEXT.leaveConfirm)
    confirm:SetPoint("BOTTOMLEFT", leave, "BOTTOMLEFT", 36, 22)
    confirm:SetScript("OnClick", function()
        PlaySound("igQuestLogAbandonQuest")
        Send("RUN_LEAVE")
        leave:Hide()
    end)
    local stay = PanelButton(leave, 140, 26, TEXT.leaveStay)
    stay:SetPoint("BOTTOMRIGHT", leave, "BOTTOMRIGHT", -36, 22)
    stay:SetScript("OnClick", function() leave:Hide() end)
    leave:SetScript("OnHide", function() PlaySound("igQuestListClose") end)
end

function UI.ConfirmLeave()
    if not leave then
        CreateLeave()
    end
    leave.text:SetText((UI.hud.players or 1) > 1 and TEXT.leaveDuo or TEXT.leaveSolo)
    if portal and portal:IsShown() then
        portal:Hide()
    end
    if not leave:IsShown() then
        OpenWindow(leave, "leaveOpen")
        PlaySound("igQuestListOpen")
    end
end

-- The tracker's portal line: the choice again, while the portal is open
function UI.ReopenPortal()
    local hud = UI.hud
    if lastPortal and hud.state == UI.STATE_CLEARED and lastPortal.next > hud.floor then
        ShowPortal(lastPortal)
    end
end

handlers.PORTAL = function(nextFloor, ladder, step, paragon, gear, checkpoint, chest, players, down)
    lastPortal = { next = Number(nextFloor, 1), ladder = Number(ladder), step = Number(step),
        paragon = Number(paragon), gear = gear == "1", checkpoint = checkpoint == "1", chest = chest == "1",
        players = Number(players, 1), down = Number(down, 1) }
    ShowPortal(lastPortal)
end

handlers.PORTALOFF = function()
    if portal and portal:IsShown() then
        portal:Hide()
    end
end

-- In a run (the tracker's HUD): Eternia's window has no place; the portal's choice only while the floor is cleared
function UI.onHud(state)
    CloseKeeper()
    if state ~= UI.STATE_CLEARED then
        lastPortal = nil
        if portal and portal:IsShown() then
            portal:Hide()
        end
    end
    if state == UI.STATE_FALLEN and leave and leave:IsShown() then
        leave:Hide()
    end
end

function UI.onEnd()
    lastPortal = nil
    if portal and portal:IsShown() then
        portal:Hide()
    end
    if leave and leave:IsShown() then
        leave:Hide()
    end
end
