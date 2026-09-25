-- Prestige (mod-stat-growth PrestigeSystem.cpp and PrestigeShop.cpp, whose headers document the messages).
--
-- The keeper in Stormwind opens this window. On the left, the character's prestige and the account's éclats de
-- prestige - the currency a character earns while it levels again after a prestige. On the right, two pages: the
-- prestige itself (what it gives, keeps and takes, and the reset), and the heirloom shop the éclats pay for.
-- Every number and every permission comes from the server; the buttons only ask.

local PREFIX = "Prestige"

local WIDTH, HEIGHT = 880, 580
local LEFT_WIDTH = 286
local ART = "Interface\\Prestige\\"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local SHARD_ICON = "Interface\\Icons\\INV_Enchant_ShardPrismaticLarge"
local HEIRLOOM_COLOR = { 0.9, 0.8, 0.5 }
-- The challenge board's palette: gold headings and frames on dark, parchment text, muted greys for what is not
-- available. Colour is left to the icons and the heirloom gold.
local GOLD = { 1, 0.82, 0.3 }
local BORDER = { 0.75, 0.6, 0.35 }
local PARCHMENT = { 1, 0.9, 0.7 }
local MUTED = { 0.62, 0.57, 0.5 }

-- The halo behind the medallion is a sequence of pre-turned frames: a texture cannot be rotated in 3.3.5, so
-- buildPrestigeArt.py bakes the rotation in and this cycles them.
local RAY_FRAMES = 24
local RAY_PERIOD = 2.6

-- The heirloom grid
local CARD_WIDTH, CARD_HEIGHT, CARD_GAP = 160, 108, 8
local COLUMNS = 3

local SOUND_OPEN = "AchievementMenuOpen"
local SOUND_CLOSE = "AchievementMenuClose"
local SOUND_DENIED = "igQuestFailed"
local SOUND_PRESTIGE = "LEVELUPSOUND"
local SOUND_TAB = "igCharacterInfoTab"
local SOUND_BUY = "igQuestListComplete"
local SOUND_SHARDS = "igQuestRewardSelect"

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Prestige",
    current = "Prestige actuel",
    cap = "Plafond de parangon",
    shards = "Éclats de prestige",
    shardsTip = "Gagnés en remontant de niveau après un prestige, et partagés par tous les personnages du compte.",
    nextShard = "%d / %d ennemis avant le prochain éclat",
    earn = "Comment en gagner",
    perLevel = "+%d par niveau gagné",
    perKills = "+1 tous les %d ennemis vaincus",
    runBonus = "+%d au retour au niveau maximum",
    onlyAfter = "Uniquement après un premier prestige.",
    tabPrestige = "Prestige",
    tabShop = "Héritages",
    heading = "Recommencer le voyage",
    intro = "Revenez au niveau 1 avec tout ce qui compte : votre tableau de parangon grandit, et chaque niveau regagné rapporte des éclats de prestige.",
    gain = "Vous gagnez",
    keep = "Vous conservez",
    lose = "Vous perdez",
    gains = { "+%d au plafond de parangon", "+5 points de parangon", "Des éclats à chaque niveau", "Des héritages à acheter" },
    keeps = { "Essences", "Tableau de parangon", "Quêtes, réputation, or", "Métiers et montures", "Héritages équipés" },
    loses = { "Niveau ramené à 1", "Sorts au-dessus du niveau 1", "Spécialisations et glyphes", "Équipement de haut niveau" },
    ready = "Vous êtes au niveau maximum.",
    reasons = { "Niveau maximum requis.", "Impossible en combat.", "Impossible tant que vous êtes mort.", "Sortez de l'instance." },
    unavailable = "Prestige indisponible.",
    prestige = "Prestige",
    confirmTitle = "Êtes-vous certain ?",
    confirmText = "Votre personnage reviendra au niveau 1. Cette action est définitive.",
    confirm = "Confirmer",
    cancel = "Annuler",
    shopHeading = "Héritages",
    shopIntro = "L'équipement qui grandit avec son porteur. Lié au compte : chacun de vos personnages peut en profiter.",
    usableOnly = "Seulement ce que je peux utiliser",
    categories = { "Tout", "Épaules", "Torses", "Armes", "Bijoux", "Divers" },
    buy = "Acheter",
    sure = "Confirmer ?",
    unusable = "Inutilisable",
    loading = "Chargement…",
    empty = "Aucun héritage dans cette catégorie.",
    bought = "%s ajouté à vos sacs.",
    failures = { "Pas assez d'éclats de prestige.", "Vos sacs sont pleins.", "Ce personnage ne peut pas l'utiliser.", "Cet objet n'est plus proposé." },
    gained = "+%d éclats de prestige",
} or {
    title = "Prestige",
    current = "Current prestige",
    cap = "Paragon cap",
    shards = "Prestige shards",
    shardsTip = "Earned while levelling again after a prestige, and shared by every character of the account.",
    nextShard = "%d / %d enemies to the next shard",
    earn = "How to earn them",
    perLevel = "+%d per level gained",
    perKills = "+1 every %d enemies slain",
    runBonus = "+%d on reaching the maximum level again",
    onlyAfter = "Only after a first prestige.",
    tabPrestige = "Prestige",
    tabShop = "Heirlooms",
    heading = "Begin the journey again",
    intro = "Return to level 1 with everything that matters: your paragon board grows, and every level regained pays prestige shards.",
    gain = "You gain",
    keep = "You keep",
    lose = "You lose",
    gains = { "+%d paragon cap", "+5 paragon points", "Shards at every level", "Heirlooms to buy" },
    keeps = { "Essences", "Paragon board", "Quests, reputation, gold", "Professions and mounts", "Equipped heirlooms" },
    loses = { "Level back to 1", "Spells above level 1", "Specializations and glyphs", "High-level gear" },
    ready = "You are at the maximum level.",
    reasons = { "Maximum level required.", "Not while in combat.", "Not while dead.", "Leave the instance first." },
    unavailable = "Prestige unavailable.",
    prestige = "Prestige",
    confirmTitle = "Are you sure?",
    confirmText = "Your character will return to level 1. This cannot be undone.",
    confirm = "Confirm",
    cancel = "Cancel",
    shopHeading = "Heirlooms",
    shopIntro = "Gear that grows with its wearer. Bound to the account: every one of your characters can use it.",
    usableOnly = "Only what I can use",
    categories = { "All", "Shoulders", "Chests", "Weapons", "Trinkets", "Other" },
    buy = "Buy",
    sure = "Confirm?",
    unusable = "Unusable",
    loading = "Loading…",
    empty = "No heirloom in this category.",
    bought = "%s added to your bags.",
    failures = { "Not enough prestige shards.", "Your bags are full.", "This character cannot use it.", "No longer on offer." },
    gained = "+%d prestige shards",
}

-- What the server last said
local state = {
    prestige = 0, cap = 50, nextCap = 60, earned = 0, spent = 0, level = 1, can = 0, reason = 1,
    shards = 0, killProgress = 0, killsPerShard = 20, perLevel = 10, runBonus = 150,
    offers = {},
    pending = false, confirming = false, suppressCloseSound = false,
    page = "prestige", category = 1, usableOnly = true,
}

local frame, pages, tabs = nil, {}, {}
local ui = {}

-- Tweens: every animation of the window, driven by one clock ------------------------------------------------------

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

local function OutBack(p)
    local c = 1.70158
    local q = p - 1
    return 1 + (c + 1) * q * q * q + c * q * q
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

-- Small building blocks --------------------------------------------------------------------------------------------

local function Panel(parent, r, g, b, alpha)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    panel:SetBackdropColor(0.03, 0.025, 0.02, alpha or 0.85)
    panel:SetBackdropBorderColor(r or 0.6, g or 0.5, b or 0.3, 1)
    return panel
end

local function Divider(parent, width)
    local divider = parent:CreateTexture(nil, "ARTWORK")
    RetailUI.SetAtlas(divider, "ChallengeMode-ThinDivider")
    divider:SetSize(width, 10)
    return divider
end

local function Label(parent, template, r, g, b)
    local text = parent:CreateFontString(nil, "OVERLAY", template or "GameFontHighlightSmall")
    if r then
        text:SetTextColor(r, g, b)
    end
    return text
end

-- A number that runs to its new value instead of jumping
local function CountTo(fontString, from, to, key, format)
    Tween(0.6, 0, function(p)
        local value = floor(from + (to - from) * OutCubic(p) + 0.5)
        fontString:SetText(format and format:format(value) or tostring(value))
    end, nil, key)
end

-- Left column: the medallion and the éclats ------------------------------------------------------------------------

local function CreateMedallion()
    local emblem = CreateFrame("Frame", nil, frame)
    emblem:SetSize(128, 128)
    emblem:SetPoint("TOP", frame, "TOPLEFT", 20 + LEFT_WIDTH / 2, -40)
    emblem:SetFrameLevel(frame:GetFrameLevel() + 4)

    local glow = frame:CreateTexture(nil, "BORDER")
    glow:SetTexture(ART .. "Prestige-Emblem-Glow")
    glow:SetBlendMode("ADD")
    glow:SetSize(202, 202)
    glow:SetPoint("CENTER", emblem, "CENTER")

    local rays = frame:CreateTexture(nil, "BORDER", nil, 1)
    rays:SetTexture(ART .. "Prestige-Rays-00")
    rays:SetBlendMode("ADD")
    rays:SetSize(176, 176)
    rays:SetPoint("CENTER", emblem, "CENTER")

    local art = emblem:CreateTexture(nil, "ARTWORK")
    art:SetAllPoints()
    art:SetTexture(ART .. "Prestige-Emblem")

    local numberFrame = CreateFrame("Frame", nil, emblem)
    numberFrame:SetAllPoints()
    numberFrame:SetFrameLevel(emblem:GetFrameLevel() + 3)
    ui.number = numberFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    ui.number:SetPoint("CENTER", numberFrame, "CENTER", 0, -1)
    ui.number:SetTextColor(1, 0.86, 0.45)

    -- The halo turns and the glow breathes against it, from one clock
    local clock, rayFrame = 0, -1
    emblem:SetScript("OnUpdate", function(_, elapsed)
        clock = clock + elapsed
        local index = floor(clock / RAY_PERIOD * RAY_FRAMES) % RAY_FRAMES
        if index ~= rayFrame then
            rayFrame = index
            rays:SetTexture(ART .. format("Prestige-Rays-%02d", index))
        end
        local pulse = 0.5 + 0.5 * math.sin(clock * 1.5)
        glow:SetAlpha(0.52 + 0.34 * pulse)
        glow:SetSize(196 + 14 * pulse, 196 + 14 * pulse)
        rays:SetAlpha(0.6 + 0.35 * (1 - pulse))
    end)
    ui.emblem = emblem

    local current = Label(frame, "GameFontNormalSmall", 0.7, 0.65, 0.5)
    current:SetPoint("TOP", emblem, "BOTTOM", 0, -2)
    current:SetText(TEXT.current)

    local capLabel = Label(frame, "GameFontNormalSmall", 0.7, 0.65, 0.5)
    capLabel:SetPoint("TOP", current, "BOTTOM", 0, -10)
    capLabel:SetText(TEXT.cap)

    -- The cap and the one the next prestige brings, either side of a drawn chevron (the fonts have no arrow)
    local capRow = CreateFrame("Frame", nil, frame)
    capRow:SetSize(220, 26)
    capRow:SetPoint("TOP", capLabel, "BOTTOM", 0, -3)
    ui.capArrow = capRow:CreateTexture(nil, "ARTWORK")
    ui.capArrow:SetTexture(ART .. "Prestige-Arrow")
    ui.capArrow:SetSize(22, 22)
    ui.capArrow:SetPoint("CENTER")
    ui.capText = Label(capRow, "GameFontNormalLarge", 1, 0.84, 0.4)
    ui.capNext = Label(capRow, "GameFontNormalLarge", PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    ui.capNext:SetPoint("LEFT", ui.capArrow, "RIGHT", 10, 1)
    ui.capRow = capRow
end

local function CreateShardPanel()
    local panel = Panel(frame, HEIRLOOM_COLOR[1] * 0.8, HEIRLOOM_COLOR[2] * 0.8, HEIRLOOM_COLOR[3] * 0.8, 0.8)
    panel:SetPoint("TOPLEFT", frame, "TOPLEFT", 22, -266)
    panel:SetSize(LEFT_WIDTH - 4, 286)
    ui.shardPanel = panel

    -- The éclat: an icon in its own glow, which flares when éclats come in
    local iconHolder = CreateFrame("Frame", nil, panel)
    iconHolder:SetSize(46, 46)
    iconHolder:SetPoint("TOPLEFT", panel, "TOPLEFT", 16, -16)
    local flare = panel:CreateTexture(nil, "BORDER")
    RetailUI.SetAtlas(flare, "ChallengeMode-SoftYellowGlow")
    flare:SetBlendMode("ADD")
    flare:SetPoint("TOPLEFT", iconHolder, "TOPLEFT", -22, 22)
    flare:SetPoint("BOTTOMRIGHT", iconHolder, "BOTTOMRIGHT", 22, -22)
    flare:SetAlpha(0.35)
    ui.shardFlare = flare
    local icon = iconHolder:CreateTexture(nil, "ARTWORK")
    icon:SetAllPoints()
    icon:SetTexture(SHARD_ICON)
    icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    local border = iconHolder:CreateTexture(nil, "OVERLAY")
    border:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    border:SetBlendMode("ADD")
    border:SetVertexColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    border:SetPoint("TOPLEFT", -18, 18)
    border:SetPoint("BOTTOMRIGHT", 18, -18)
    iconHolder:EnableMouse(true)
    iconHolder:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(TEXT.shards, HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
        GameTooltip:AddLine(TEXT.shardsTip, 1, 1, 1, true)
        GameTooltip:Show()
    end)
    iconHolder:SetScript("OnLeave", function() GameTooltip:Hide() end)

    ui.shards = panel:CreateFontString(nil, "OVERLAY")
    ui.shards:SetFont(MORPHEUS, 30)
    ui.shards:SetShadowOffset(1, -1)
    ui.shards:SetTextColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    ui.shards:SetPoint("TOPLEFT", iconHolder, "TOPRIGHT", 14, 2)
    ui.shards:SetText("0")

    local shardsLabel = Label(panel, "GameFontNormalSmall", 0.8, 0.72, 0.55)
    shardsLabel:SetPoint("TOPLEFT", ui.shards, "BOTTOMLEFT", 0, -1)
    shardsLabel:SetText(TEXT.shards)

    -- Kills towards the next éclat
    local bar = CreateFrame("StatusBar", nil, panel)
    bar:SetSize(LEFT_WIDTH - 40, 12)
    bar:SetPoint("TOP", panel, "TOP", 0, -80)
    bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    bar:SetStatusBarColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2] * 0.9, HEIRLOOM_COLOR[3] * 0.6)
    bar:SetMinMaxValues(0, 1)
    local barBack = bar:CreateTexture(nil, "BACKGROUND")
    barBack:SetAllPoints()
    barBack:SetTexture(0, 0, 0, 0.6)
    local barBorder = CreateFrame("Frame", nil, bar)
    barBorder:SetPoint("TOPLEFT", -3, 3)
    barBorder:SetPoint("BOTTOMRIGHT", 3, -3)
    barBorder:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10 })
    barBorder:SetBackdropBorderColor(0.6, 0.5, 0.3, 1)
    local spark = bar:CreateTexture(nil, "OVERLAY")
    spark:SetTexture("Interface\\CastingBar\\UI-CastingBar-Spark")
    spark:SetBlendMode("ADD")
    spark:SetSize(16, 28)
    ui.barSpark = spark
    ui.bar = bar
    ui.barText = Label(panel, "GameFontHighlightSmall", 0.8, 0.78, 0.7)
    ui.barText:SetPoint("TOP", bar, "BOTTOM", 0, -5)

    local divider = Divider(panel, LEFT_WIDTH - 40)
    divider:SetPoint("TOP", bar, "BOTTOM", 0, -26)

    local earn = Label(panel, "GameFontNormal", 1, 0.82, 0.3)
    earn:SetPoint("TOPLEFT", panel, "TOPLEFT", 18, -128)
    earn:SetText(TEXT.earn)

    ui.earnRows = {}
    local icons = { "Interface\\Icons\\Spell_Holy_SurgeOfLight", "Interface\\Icons\\Ability_Warrior_Rampage",
        "Interface\\Icons\\Achievement_Level_80" }
    for index = 1, 3 do
        local row = CreateFrame("Frame", nil, panel)
        row:SetSize(LEFT_WIDTH - 40, 30)
        row:SetPoint("TOPLEFT", panel, "TOPLEFT", 18, -146 - (index - 1) * 34)
        local rowIcon = row:CreateTexture(nil, "ARTWORK")
        rowIcon:SetSize(24, 24)
        rowIcon:SetPoint("LEFT")
        rowIcon:SetTexture(icons[index])
        rowIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        local text = Label(row, "GameFontHighlight", 0.9, 0.87, 0.78)
        text:SetPoint("LEFT", rowIcon, "RIGHT", 10, 0)
        text:SetPoint("RIGHT", row, "RIGHT")
        text:SetJustifyH("LEFT")
        ui.earnRows[index] = text
    end

    local note = Label(panel, "GameFontDisableSmall")
    note:SetPoint("BOTTOM", panel, "BOTTOM", 0, 14)
    note:SetText(TEXT.onlyAfter)
end

-- The prestige page ------------------------------------------------------------------------------------------------

local function Column(page, title, lines, x, width, color, kind)
    local panel = Panel(page, BORDER[1], BORDER[2], BORDER[3], 0.85)
    panel:SetPoint("TOPLEFT", page, "TOPLEFT", x, -104)
    panel:SetSize(width, 214)

    local header = Label(panel, "GameFontNormal", GOLD[1], GOLD[2], GOLD[3])
    header:SetPoint("TOP", panel, "TOP", 0, -12)
    header:SetText(title)

    local rows = {}
    for index, line in ipairs(lines) do
        local row = CreateFrame("Frame", nil, panel)
        row:SetSize(width - 24, 30)
        row:SetPoint("TOPLEFT", panel, "TOPLEFT", 12, -34 - (index - 1) * 34)
        -- Drawn markers in the column's tone: a plus to gain, a dot to keep, a dash to lose
        local mark = row:CreateTexture(nil, "OVERLAY")
        mark:SetTexture(1, 1, 1, 1)
        mark:SetVertexColor(color[1], color[2], color[3], 0.95)
        if kind == "keep" then
            mark:SetSize(5, 5)
            mark:SetPoint("LEFT", row, "LEFT", 5, 0)
        else
            mark:SetSize(10, 2)
            mark:SetPoint("LEFT", row, "LEFT", 3, 0)
        end
        if kind == "gain" then
            local stem = row:CreateTexture(nil, "OVERLAY")
            stem:SetTexture(1, 1, 1, 1)
            stem:SetVertexColor(color[1], color[2], color[3], 0.95)
            stem:SetSize(2, 10)
            stem:SetPoint("CENTER", mark, "CENTER")
        end
        local text = Label(row, "GameFontHighlightSmall", 0.88, 0.85, 0.76)
        text:SetPoint("LEFT", row, "LEFT", 24, 0)
        text:SetPoint("RIGHT", row, "RIGHT")
        text:SetHeight(30)
        text:SetJustifyH("LEFT")
        text:SetJustifyV("MIDDLE")
        text:SetText(line)
        rows[index] = text
        row:SetAlpha(0)
        row.index = index
        rows[index].row = row
    end
    panel.rows = rows
    return panel
end

local function CreatePrestigePage(page)
    local heading = page:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 24)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(1, 0.88, 0.6)
    heading:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8)
    heading:SetText(TEXT.heading)

    local intro = Label(page, "GameFontHighlight", 0.82, 0.78, 0.68)
    intro:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -8)
    intro:SetWidth(540)
    intro:SetJustifyH("LEFT")
    intro:SetText(TEXT.intro)

    local width = 172
    ui.gainColumn = Column(page, TEXT.gain, TEXT.gains, 0, width, GOLD, "gain")
    ui.keepColumn = Column(page, TEXT.keep, TEXT.keeps, width + 12, width, PARCHMENT, "keep")
    ui.loseColumn = Column(page, TEXT.lose, TEXT.loses, 2 * (width + 12), width, MUTED, "lose")

    ui.requirement = Label(page, "GameFontNormal")
    ui.requirement:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 6, 44)

    -- The button, on its own glow when the reset is allowed
    local buttonGlow = page:CreateTexture(nil, "BORDER")
    RetailUI.SetAtlas(buttonGlow, "ChallengeMode-SoftYellowGlow")
    buttonGlow:SetBlendMode("ADD")
    ui.buttonGlow = buttonGlow

    local button = CreateFrame("Button", nil, page, "UIPanelButtonTemplate")
    button:SetSize(180, 30)
    button:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", -4, 36)
    button:SetText(TEXT.prestige)
    button:SetScript("OnClick", function()
        if state.can ~= 1 or state.pending then
            PlaySound(SOUND_DENIED)
            return
        end
        ui.showConfirm(true)
    end)
    buttonGlow:SetPoint("TOPLEFT", button, "TOPLEFT", -26, 20)
    buttonGlow:SetPoint("BOTTOMRIGHT", button, "BOTTOMRIGHT", 26, -20)
    ui.prestigeButton = button

    -- The confirmation slides up over the bottom of the page
    local confirm = Panel(page, 0.85, 0.55, 0.25, 0.97)
    confirm:SetSize(540, 108)
    confirm:SetFrameLevel(page:GetFrameLevel() + 20)
    confirm:Hide()
    local confirmTitle = confirm:CreateFontString(nil, "OVERLAY")
    confirmTitle:SetFont(MORPHEUS, 20)
    confirmTitle:SetTextColor(1, 0.75, 0.35)
    confirmTitle:SetPoint("TOPLEFT", confirm, "TOPLEFT", 18, -14)
    confirmTitle:SetText(TEXT.confirmTitle)
    local confirmText = Label(confirm, "GameFontHighlight", 0.9, 0.85, 0.75)
    confirmText:SetPoint("TOPLEFT", confirmTitle, "BOTTOMLEFT", 0, -6)
    confirmText:SetText(TEXT.confirmText)
    local confirmButton = CreateFrame("Button", nil, confirm, "UIPanelButtonTemplate")
    confirmButton:SetSize(130, 26)
    confirmButton:SetPoint("BOTTOMRIGHT", confirm, "BOTTOMRIGHT", -152, 14)
    confirmButton:SetText(TEXT.confirm)
    confirmButton:SetScript("OnClick", function()
        if state.pending then return end
        state.pending = true
        ui.refresh()
        SendAddonMessage(PREFIX, "PRESTIGE", "WHISPER", UnitName("player"))
    end)
    ui.confirmButton = confirmButton
    local cancelButton = CreateFrame("Button", nil, confirm, "UIPanelButtonTemplate")
    cancelButton:SetSize(130, 26)
    cancelButton:SetPoint("BOTTOMRIGHT", confirm, "BOTTOMRIGHT", -14, 14)
    cancelButton:SetText(TEXT.cancel)
    cancelButton:SetScript("OnClick", function()
        if not state.pending then ui.showConfirm(false) end
    end)
    ui.confirm = confirm

    ui.showConfirm = function(show)
        state.confirming = show
        if show then
            confirm:Show()
            Tween(0.3, 0, function(p)
                confirm:SetPoint("BOTTOM", page, "BOTTOM", 0, -60 + 90 * OutBack(p))
                confirm:SetAlpha(p)
            end, nil, "confirm")
        else
            Tween(0.2, 0, function(p)
                confirm:SetPoint("BOTTOM", page, "BOTTOM", 0, 30 - 60 * p)
                confirm:SetAlpha(1 - p)
            end, function() confirm:Hide() end, "confirm")
        end
        ui.refresh()
    end
end

-- The heirloom shop ------------------------------------------------------------------------------------------------

-- Which of the filter's categories an item belongs to, by where it is worn
local CATEGORY_BY_SLOT = {
    INVTYPE_SHOULDER = 2,
    INVTYPE_CHEST = 3, INVTYPE_ROBE = 3,
    INVTYPE_WEAPON = 4, INVTYPE_2HWEAPON = 4, INVTYPE_WEAPONMAINHAND = 4, INVTYPE_WEAPONOFFHAND = 4,
    INVTYPE_RANGED = 4, INVTYPE_RANGEDRIGHT = 4, INVTYPE_THROWN = 4,
    INVTYPE_TRINKET = 5, INVTYPE_FINGER = 5, INVTYPE_NECK = 5,
}

-- Asks the client's item cache for every offer, so names and icons arrive; the grid redraws when they do
local cacheTooltip = CreateFrame("GameTooltip", "PrestigeCacheTooltip", UIParent, "GameTooltipTemplate")
cacheTooltip:SetOwner(UIParent, "ANCHOR_NONE")

local function ItemData(item)
    local name, link, _, _, _, itemType, itemSubType, _, equipLoc, icon = GetItemInfo(item)
    if not name then
        cacheTooltip:SetHyperlink("item:" .. item)
        return nil
    end
    return { name = name, link = link, icon = icon, equipLoc = equipLoc, subType = itemSubType, type = itemType }
end

local function Visible()
    local list = {}
    for _, offer in ipairs(state.offers) do
        local data = ItemData(offer.item)
        local category = data and (CATEGORY_BY_SLOT[data.equipLoc] or 6) or 6
        if (not state.usableOnly or offer.usable) and (state.category == 1 or category == state.category) then
            tinsert(list, offer)
        end
    end
    return list
end

local cards = {}
local cardHolder, scrollFrame

local function CardTooltip(card)
    if not card.offer then return end
    GameTooltip:SetOwner(card, "ANCHOR_RIGHT")
    GameTooltip:SetHyperlink("item:" .. card.offer.item)
    GameTooltip:Show()
end

local function ResetBuyButton(card)
    card.armed = nil
    card.buy:SetText(TEXT.buy)
end

local function CreateCard(index)
    local card = Panel(cardHolder, BORDER[1], BORDER[2], BORDER[3], 0.92)
    card:SetSize(CARD_WIDTH, CARD_HEIGHT)
    card:EnableMouse(true)

    local glow = card:CreateTexture(nil, "BACKGROUND", nil, -1)
    RetailUI.SetAtlas(glow, "ChallengeMode-SoftYellowGlow")
    glow:SetBlendMode("ADD")
    glow:SetPoint("TOPLEFT", -20, 20)
    glow:SetPoint("BOTTOMRIGHT", 20, -20)
    glow:SetAlpha(0)
    card.glow = glow

    local iconFrame = CreateFrame("Frame", nil, card)
    iconFrame:SetSize(40, 40)
    iconFrame:SetPoint("TOPLEFT", card, "TOPLEFT", 12, -12)
    card.icon = iconFrame:CreateTexture(nil, "ARTWORK")
    card.icon:SetAllPoints()
    card.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    local iconBorder = iconFrame:CreateTexture(nil, "OVERLAY")
    iconBorder:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
    iconBorder:SetBlendMode("ADD")
    iconBorder:SetVertexColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    iconBorder:SetPoint("TOPLEFT", -15, 15)
    iconBorder:SetPoint("BOTTOMRIGHT", 15, -15)

    card.name = Label(card, "GameFontNormalSmall", HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    card.name:SetPoint("TOPLEFT", iconFrame, "TOPRIGHT", 8, 1)
    card.name:SetPoint("RIGHT", card, "RIGHT", -8, 0)
    card.name:SetHeight(26)
    card.name:SetJustifyH("LEFT")
    card.name:SetJustifyV("TOP")

    card.slot = Label(card, "GameFontDisableSmall")
    card.slot:SetPoint("TOPLEFT", card.name, "BOTTOMLEFT", 0, -1)
    card.slot:SetPoint("RIGHT", card, "RIGHT", -8, 0)
    card.slot:SetJustifyH("LEFT")

    -- The price: an éclat and a number
    local costIcon = card:CreateTexture(nil, "ARTWORK")
    costIcon:SetTexture(SHARD_ICON)
    costIcon:SetSize(16, 16)
    costIcon:SetPoint("BOTTOMLEFT", card, "BOTTOMLEFT", 12, 14)
    costIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    card.cost = Label(card, "GameFontNormal")
    card.cost:SetPoint("LEFT", costIcon, "RIGHT", 5, 0)

    local buy = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    buy:SetSize(84, 22)
    buy:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", -10, 10)
    buy:SetText(TEXT.buy)
    buy:SetScript("OnClick", function()
        local offer = card.offer
        if not offer or not offer.usable or state.shards < offer.cost then
            PlaySound(SOUND_DENIED)
            return
        end
        -- Two clicks: the first arms the button, the second buys
        if not card.armed then
            card.armed = true
            buy:SetText(TEXT.sure)
            Tween(3, 0, function() end, function() ResetBuyButton(card) end, "arm" .. index)
            return
        end
        ResetBuyButton(card)
        state.buying = offer.item
        SendAddonMessage(PREFIX, "BUY\t" .. offer.item, "WHISPER", UnitName("player"))
    end)
    buy:SetScript("OnEnter", function() CardTooltip(card) end)
    buy:SetScript("OnLeave", function() GameTooltip:Hide() end)
    card.buy = buy

    card.unusable = Label(card, "GameFontDisableSmall", MUTED[1], MUTED[2], MUTED[3])
    card.unusable:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", -14, 16)
    card.unusable:SetText(TEXT.unusable)

    -- The purchase: a white flash over the card
    local flash = card:CreateTexture(nil, "OVERLAY", nil, 7)
    flash:SetTexture("Interface\\Buttons\\WHITE8X8")
    flash:SetPoint("TOPLEFT", 4, -4)
    flash:SetPoint("BOTTOMRIGHT", -4, 4)
    flash:SetBlendMode("ADD")
    flash:SetVertexColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    flash:SetAlpha(0)
    card.flash = flash

    -- Hovering lifts the card's glow and brightens its border
    card:SetScript("OnEnter", function(self)
        CardTooltip(self)
        self:SetBackdropBorderColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3], 1)
        Tween(0.18, 0, function(p) glow:SetAlpha(0.55 * p) end, nil, "hover" .. index)
    end)
    card:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
        self:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 1)
        local from = glow:GetAlpha()
        Tween(0.25, 0, function(p) glow:SetAlpha(from * (1 - p)) end, nil, "hover" .. index)
    end)

    cards[index] = card
    return card
end

local function LayoutShop(animate)
    if not cardHolder then return end
    local list = Visible()
    local missing = false
    for index, offer in ipairs(list) do
        local card = cards[index] or CreateCard(index)
        card.offer = offer
        local column = (index - 1) % COLUMNS
        local row = floor((index - 1) / COLUMNS)
        card:ClearAllPoints()
        card:SetPoint("TOPLEFT", cardHolder, "TOPLEFT", column * (CARD_WIDTH + CARD_GAP), -row * (CARD_HEIGHT + CARD_GAP))

        local data = ItemData(offer.item)
        if data then
            card.icon:SetTexture(data.icon)
            card.name:SetText(data.name)
            local slot = data.equipLoc ~= "" and _G[data.equipLoc] or data.type
            card.slot:SetText(data.subType and data.subType ~= "" and slot and (slot .. " · " .. data.subType) or slot or "")
        else
            missing = true
            card.icon:SetTexture("Interface\\Icons\\INV_Misc_QuestionMark")
            card.name:SetText(TEXT.loading)
            card.slot:SetText("")
        end

        local affordable = state.shards >= offer.cost
        card.cost:SetText(offer.cost)
        if affordable then
            card.cost:SetTextColor(1, 0.92, 0.6)
        else
            card.cost:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        end
        card.icon:SetDesaturated(not offer.usable)
        card:SetAlpha(offer.usable and 1 or 0.6)
        if offer.usable then
            card.buy:Show()
            card.unusable:Hide()
            if affordable then card.buy:Enable() else card.buy:Disable() end
        else
            card.buy:Hide()
            card.unusable:Show()
        end
        card:Show()

        if animate then
            local target = offer.usable and 1 or 0.6
            card:SetAlpha(0)
            Tween(0.35, 0.03 * index, function(p)
                card:SetAlpha(target * p)
                card:SetPoint("TOPLEFT", cardHolder, "TOPLEFT", column * (CARD_WIDTH + CARD_GAP),
                    -row * (CARD_HEIGHT + CARD_GAP) - 14 * (1 - OutCubic(p)))
            end, nil, "enter" .. index)
        end
    end
    for index = #list + 1, #cards do
        cards[index]:Hide()
        cards[index].offer = nil
    end

    local rows = ceil(#list / COLUMNS)
    cardHolder:SetHeight(max(1, rows * (CARD_HEIGHT + CARD_GAP)))
    if #list == 0 then ui.shopEmpty:Show() else ui.shopEmpty:Hide() end
    if scrollFrame then
        scrollFrame:UpdateScrollChildRect()
    end

    -- Names still on their way from the server: look again shortly
    if missing then
        Tween(0.5, 0, function() end, function() LayoutShop(false) end, "itemcache")
    end
end

local function CreateShopPage(page)
    local heading = page:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 24)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
    heading:SetPoint("TOPLEFT", page, "TOPLEFT", 4, -8)
    heading:SetText(TEXT.shopHeading)

    local intro = Label(page, "GameFontHighlightSmall", 0.82, 0.78, 0.68)
    intro:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -6)
    intro:SetWidth(510)
    intro:SetJustifyH("LEFT")
    intro:SetText(TEXT.shopIntro)

    -- Categories: a row of small buttons, the chosen one lit
    ui.categoryButtons = {}
    local previous
    for index, label in ipairs(TEXT.categories) do
        local button = CreateFrame("Button", nil, page)
        button:SetHeight(22)
        local text = Label(button, "GameFontNormalSmall")
        text:SetPoint("CENTER")
        text:SetText(label)
        button:SetWidth(text:GetStringWidth() + 22)
        button:SetBackdrop({
            bgFile = "Interface\\Buttons\\WHITE8X8",
            edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            edgeSize = 10, insets = { left = 2, right = 2, top = 2, bottom = 2 },
        })
        if previous then
            button:SetPoint("LEFT", previous, "RIGHT", 6, 0)
        else
            button:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -72)
        end
        button.text = text
        button:SetScript("OnClick", function()
            PlaySound(SOUND_TAB)
            state.category = index
            ui.refreshCategories()
            LayoutShop(true)
        end)
        ui.categoryButtons[index] = button
        previous = button
    end
    ui.refreshCategories = function()
        for index, button in ipairs(ui.categoryButtons) do
            local chosen = index == state.category
            button:SetBackdropColor(chosen and 0.35 or 0.06, chosen and 0.28 or 0.05, chosen and 0.12 or 0.04, 0.9)
            button:SetBackdropBorderColor(chosen and 1 or 0.5, chosen and 0.85 or 0.42, chosen and 0.45 or 0.28, 1)
            button.text:SetTextColor(chosen and 1 or 0.8, chosen and 0.92 or 0.72, chosen and 0.6 or 0.55)
        end
    end

    local usable = CreateFrame("CheckButton", "PrestigeShopUsableOnly", page, "UICheckButtonTemplate")
    usable:SetSize(22, 22)
    usable:SetPoint("TOPRIGHT", page, "TOPRIGHT", -196, -10)
    _G[usable:GetName() .. "Text"]:SetText(TEXT.usableOnly)
    usable:SetChecked(state.usableOnly)
    usable:SetScript("OnClick", function(self)
        state.usableOnly = self:GetChecked() and true or false
        PlaySound(SOUND_TAB)
        LayoutShop(true)
    end)

    scrollFrame = CreateFrame("ScrollFrame", "PrestigeShopScroll", page, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", page, "TOPLEFT", 0, -104)
    scrollFrame:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", -24, 28)
    cardHolder = CreateFrame("Frame", nil, scrollFrame)
    cardHolder:SetSize(COLUMNS * (CARD_WIDTH + CARD_GAP), 1)
    scrollFrame:SetScrollChild(cardHolder)

    ui.shopEmpty = Label(page, "GameFontDisable")
    ui.shopEmpty:SetPoint("CENTER", scrollFrame, "CENTER")
    ui.shopEmpty:SetText(TEXT.empty)
    ui.shopEmpty:Hide()

    -- What a purchase or a refusal says, along the bottom
    ui.shopStatus = Label(page, "GameFontNormal")
    ui.shopStatus:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 4, 6)
end

-- Pages and tabs ---------------------------------------------------------------------------------------------------

local function ShowPage(name, animate)
    state.page = name
    for key, page in pairs(pages) do
        if key == name then
            page:Show()
            if animate then
                page:SetAlpha(0)
                Tween(0.25, 0, function(p)
                    page:SetAlpha(p)
                    page:SetPoint("TOPLEFT", frame, "TOPLEFT", LEFT_WIDTH + 44 + 16 * (1 - OutCubic(p)), -34)
                end, nil, "page")
            end
        else
            page:Hide()
        end
    end
    PanelTemplates_SetTab(frame, name == "prestige" and 1 or 2)
    if name == "shop" then
        ui.refreshCategories()
        LayoutShop(animate)
    else
        -- The three columns' lines come in one after another
        for _, column in ipairs({ ui.gainColumn, ui.keepColumn, ui.loseColumn }) do
            for index, text in ipairs(column.rows) do
                local row = text.row
                if animate then
                    row:SetAlpha(0)
                    Tween(0.3, 0.05 * index, function(p) row:SetAlpha(p) end, nil, "row" .. tostring(row))
                else
                    row:SetAlpha(1)
                end
            end
        end
    end
end

-- Refresh ----------------------------------------------------------------------------------------------------------

local shownShards

local function RefreshShards(animate)
    if not frame then return end
    if animate and shownShards and shownShards ~= state.shards then
        CountTo(ui.shards, shownShards, state.shards, "shards")
        Tween(0.9, 0, function(p) ui.shardFlare:SetAlpha(0.35 + 0.65 * math.sin(p * math.pi)) end, nil, "flare")
    else
        ui.shards:SetText(state.shards)
    end
    shownShards = state.shards

    local per = max(1, state.killsPerShard)
    local target = state.killProgress / per
    local from = ui.bar:GetValue()
    Tween(0.5, 0, function(p)
        local value = from + (target - from) * OutCubic(p)
        ui.bar:SetValue(value)
        ui.barSpark:SetPoint("CENTER", ui.bar, "LEFT", ui.bar:GetWidth() * value, 0)
    end, nil, "bar")
    ui.barText:SetFormattedText(TEXT.nextShard, state.killProgress, per)
    ui.earnRows[1]:SetFormattedText(TEXT.perLevel, state.perLevel)
    ui.earnRows[2]:SetFormattedText(TEXT.perKills, per)
    ui.earnRows[3]:SetFormattedText(TEXT.runBonus, state.runBonus)
end

local function Refresh()
    if not frame then return end

    ui.number:SetFontObject(state.prestige >= 100 and "GameFontNormalLarge" or "GameFontNormalHuge")
    ui.number:SetText(state.prestige)
    ui.capText:SetText(state.cap)
    ui.capText:ClearAllPoints()
    if state.nextCap > state.cap then
        ui.capArrow:Show()
        ui.capNext:Show()
        ui.capNext:SetText(state.nextCap)
        ui.capText:SetPoint("RIGHT", ui.capArrow, "LEFT", -10, 1)
    else
        ui.capArrow:Hide()
        ui.capNext:Hide()
        ui.capText:SetPoint("CENTER", ui.capArrow, "CENTER", 0, 1)
    end
    ui.gainColumn.rows[1]:SetFormattedText(TEXT.gains[1], max(0, state.nextCap - state.cap))

    local allowed = state.can == 1 and not state.pending
    if allowed then
        ui.requirement:SetText(TEXT.ready)
        ui.requirement:SetTextColor(PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
        ui.prestigeButton:Enable()
        ui.buttonGlow:Show()
    else
        ui.requirement:SetText(TEXT.reasons[state.reason] or TEXT.unavailable)
        ui.requirement:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        ui.prestigeButton:Disable()
        ui.buttonGlow:Hide()
    end
    if state.pending then ui.confirmButton:Disable() else ui.confirmButton:Enable() end
    if state.confirming and not allowed and not state.pending then
        state.confirming = false
        ui.confirm:Hide()
    end
end
ui.refresh = Refresh

-- The window -------------------------------------------------------------------------------------------------------

local function CreateWindow()
    frame = CreateFrame("Frame", "PrestigeFrame", UIParent)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("CENTER", 0, 10)
    frame:SetFrameStrata("HIGH")
    frame:SetToplevel(true)
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

    local marble = frame:CreateTexture(nil, "BACKGROUND", nil, 1)
    marble:SetTexture(RetailUIFiles["ui-background-marble"], true)
    marble:SetHorizTile(true)
    marble:SetVertTile(true)
    marble:SetPoint("TOPLEFT", 6, -24)
    marble:SetPoint("BOTTOMRIGHT", -6, 6)

    -- The left column sits on a darker, warm band
    local band = frame:CreateTexture(nil, "BACKGROUND", nil, 2)
    band:SetTexture("Interface\\Buttons\\WHITE8X8")
    band:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -24)
    band:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 8, 8)
    band:SetWidth(LEFT_WIDTH + 24)
    band:SetGradientAlpha("HORIZONTAL", 0.1, 0.06, 0.02, 0.75, 0.05, 0.03, 0.01, 0.25)

    RetailUI.ApplyNineSlice(frame, false)

    local title = Label(frame, "GameFontNormalLarge", 1, 0.82, 0)
    title:SetPoint("TOP", frame, "TOP", 0, -4)
    title:SetText(TEXT.title)

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
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -32, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(frame:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)

    CreateMedallion()
    CreateShardPanel()

    for _, name in ipairs({ "prestige", "shop" }) do
        local page = CreateFrame("Frame", nil, frame)
        page:SetPoint("TOPLEFT", frame, "TOPLEFT", LEFT_WIDTH + 44, -34)
        page:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -20, 14)
        page:Hide()
        pages[name] = page
    end
    CreatePrestigePage(pages.prestige)
    CreateShopPage(pages.shop)

    -- The tabs under the window, as on the character sheet
    for index, label in ipairs({ TEXT.tabPrestige, TEXT.tabShop }) do
        local tab = CreateFrame("Button", "PrestigeFrameTab" .. index, frame, "CharacterFrameTabButtonTemplate")
        tab:SetID(index)
        tab:SetText(label)
        PanelTemplates_TabResize(tab, 12)
        if index == 1 then
            tab:SetPoint("TOPLEFT", frame, "BOTTOMLEFT", 24, 4)
        else
            tab:SetPoint("LEFT", tabs[index - 1], "RIGHT", -16, 0)
        end
        tab:SetScript("OnClick", function(self)
            PlaySound(SOUND_TAB)
            ShowPage(self:GetID() == 1 and "prestige" or "shop", true)
        end)
        tabs[index] = tab
    end
    PanelTemplates_SetNumTabs(frame, 2)

    tinsert(UISpecialFrames, "PrestigeFrame")
    frame:SetScript("OnHide", function()
        state.pending = false
        if state.confirming then
            state.confirming = false
            ui.confirm:Hide()
        end
        if state.suppressCloseSound then
            state.suppressCloseSound = false
        else
            PlaySound(SOUND_CLOSE)
        end
    end)
end

local function Open()
    if not frame then
        CreateWindow()
    end
    Refresh()
    RefreshShards(false)
    if not frame:IsShown() then
        frame:SetAlpha(0)
        frame:SetScale(0.94)
        frame:Show()
        PlaySound(SOUND_OPEN)
        Tween(0.28, 0, function(p)
            frame:SetAlpha(p)
            frame:SetScale(0.94 + 0.06 * OutBack(p))
        end, nil, "open")
        -- The medallion lands with a small bounce
        Tween(0.5, 0.05, function(p) ui.emblem:SetScale(0.7 + 0.3 * OutBack(p)) end, nil, "emblem")
        ShowPage(state.page, true)
    end
end

-- A toast at the top of the screen when éclats come in, whether the window is open or not -----------------------

local toast = CreateFrame("Frame", nil, UIParent)
toast:SetSize(320, 44)
toast:SetPoint("TOP", UIParent, "TOP", 0, -150)
toast:SetFrameStrata("HIGH")
toast:Hide()
-- A warm band fading out to both sides, in two halves
local toastLeft = toast:CreateTexture(nil, "BACKGROUND")
toastLeft:SetPoint("TOPLEFT")
toastLeft:SetPoint("BOTTOMRIGHT", toast, "BOTTOM")
toastLeft:SetTexture("Interface\\Buttons\\WHITE8X8")
toastLeft:SetGradientAlpha("HORIZONTAL", 0.4, 0.3, 0.1, 0, 0.4, 0.3, 0.1, 0.55)
local toastRight = toast:CreateTexture(nil, "BACKGROUND")
toastRight:SetPoint("TOPLEFT", toast, "TOP")
toastRight:SetPoint("BOTTOMRIGHT")
toastRight:SetTexture("Interface\\Buttons\\WHITE8X8")
toastRight:SetGradientAlpha("HORIZONTAL", 0.4, 0.3, 0.1, 0.55, 0.4, 0.3, 0.1, 0)
local toastIcon = toast:CreateTexture(nil, "ARTWORK")
toastIcon:SetSize(30, 30)
toastIcon:SetTexture(SHARD_ICON)
toastIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
local toastText = toast:CreateFontString(nil, "OVERLAY")
toastText:SetFont(MORPHEUS, 20)
toastText:SetShadowOffset(1, -1)
toastText:SetTextColor(HEIRLOOM_COLOR[1], HEIRLOOM_COLOR[2], HEIRLOOM_COLOR[3])
toastText:SetPoint("CENTER", toast, "CENTER", 18, 0)
toastIcon:SetPoint("RIGHT", toastText, "LEFT", -8, 0)

local function Toast(amount)
    toastText:SetFormattedText(TEXT.gained, amount)
    toast:Show()
    Tween(3.2, 0, function(p)
        local alpha = p < 0.12 and p / 0.12 or (p > 0.75 and (1 - p) / 0.25 or 1)
        toast:SetAlpha(alpha)
        toast:SetPoint("TOP", UIParent, "TOP", 0, -150 + 16 * OutCubic(min(1, p * 4)))
    end, function() toast:Hide() end, "toast")
end

-- Messages ---------------------------------------------------------------------------------------------------------

local function ParseShop(list)
    state.offers = {}
    for item, cost, usable in list:gmatch("(%d+):(%d+):(%d)") do
        local offer = { item = tonumber(item), cost = tonumber(cost), usable = usable == "1" }
        tinsert(state.offers, offer)
        ItemData(offer.item)
    end
    -- Usable first, cheapest first
    table.sort(state.offers, function(a, b)
        if a.usable ~= b.usable then return a.usable end
        if a.cost ~= b.cost then return a.cost < b.cost end
        return a.item < b.item
    end)
end

local function Status(text, r, g, b)
    if not ui.shopStatus then return end
    ui.shopStatus:SetText(text)
    ui.shopStatus:SetTextColor(r, g, b)
    Tween(4, 0, function(p) ui.shopStatus:SetAlpha(p > 0.8 and (1 - p) / 0.2 or 1) end, nil, "status")
end

local function Handle(message)
    local command, rest = message:match("^(%S+)\t?(.*)$")
    if not command then return end

    if command == "STATE" then
        local prestige, cap, nextCap, earned, spent, level, can, reason =
            rest:match("^(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
        if not prestige then return end
        state.prestige, state.cap, state.nextCap = tonumber(prestige), tonumber(cap), tonumber(nextCap)
        state.earned, state.spent, state.level = tonumber(earned), tonumber(spent), tonumber(level)
        state.can, state.reason = tonumber(can), tonumber(reason)
        state.pending = false
        Open()
        return
    end

    if command == "SHARDS" then
        local shards, progress, per, gained, perLevel, runBonus =
            rest:match("^(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
        if not shards then return end
        state.shards, state.killProgress, state.killsPerShard = tonumber(shards), tonumber(progress), tonumber(per)
        state.perLevel, state.runBonus = tonumber(perLevel), tonumber(runBonus)
        gained = tonumber(gained)
        if gained > 0 then
            Toast(gained)
            PlaySound(SOUND_SHARDS)
        end
        if frame and frame:IsShown() then
            RefreshShards(true)
            if state.page == "shop" then LayoutShop(false) end
        end
        return
    end

    if command == "SHOP" then
        ParseShop(rest)
        if frame and frame:IsShown() and state.page == "shop" then LayoutShop(false) end
        return
    end

    if command == "BOUGHT" then
        local item, left = rest:match("^(%d+)\t(%d+)$")
        item = tonumber(item)
        state.shards = tonumber(left) or state.shards
        state.buying = nil
        PlaySound(SOUND_BUY)
        for _, card in ipairs(cards) do
            if card:IsShown() and card.offer and card.offer.item == item then
                Tween(0.7, 0, function(p) card.flash:SetAlpha(0.7 * (1 - p)) end, nil, "flash" .. item)
            end
        end
        local data = ItemData(item)
        Status(TEXT.bought:format(data and data.link or ("item " .. item)), PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
        RefreshShards(true)
        LayoutShop(false)
        return
    end

    if command == "BUYFAIL" then
        state.buying = nil
        PlaySound(SOUND_DENIED)
        Status(TEXT.failures[tonumber(rest)] or TEXT.failures[4], MUTED[1], MUTED[2], MUTED[3])
        return
    end

    if command == "ERROR" then
        state.pending = false
        state.confirming = false
        state.can = 0
        state.reason = tonumber(rest) or 1
        PlaySound(SOUND_DENIED)
        if frame then
            ui.confirm:Hide()
            Refresh()
        end
        return
    end

    if command == "DONE" then
        state.pending = false
        state.suppressCloseSound = true
        PlaySound(SOUND_PRESTIGE)
        if frame then
            frame:Hide()
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event ~= "CHAT_MSG_ADDON" or prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    Handle(message)
end)

SLASH_PRESTIGE1 = "/prestige"
SlashCmdList["PRESTIGE"] = function()
    SendAddonMessage(PREFIX, "OPEN", "WHISPER", UnitName("player"))
end
