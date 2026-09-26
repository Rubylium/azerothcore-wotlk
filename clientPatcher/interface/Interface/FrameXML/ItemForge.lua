-- The Forge (server: modules/mod-forge Forge.cpp, protocol at the top of it). The player brings a piece of high-end
-- gear to the blacksmith and pays him in gold; it goes back on the anvil and comes out 4 item levels higher, up to
-- 8 times. Opened with .forge for now.
--
-- Left: every piece the Forge takes, worn first, then the bags. Right: the anvil, with the piece picked, what it
-- will become, and the blacksmith's price. Every sound is the smithy's: bellows when the forge opens, metal set down
-- on the anvil, coins handed over, the hammer three times, and the hiss of the quench.

local PREFIX = "Forge"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local GENERATED_ITEM_BASE = 0x10000     -- MythicDungeon.h: forged items are generated entries

local SOUND_BELLOW_IN = "Sound\\Doodad\\BellowIn.wav"
local SOUND_BELLOW_OUT = "Sound\\Doodad\\BellowOut.wav"
local SOUND_PUT_DOWN = "Sound\\interface\\PickUp\\PutDownLArgeMEtal.wav"
local SOUND_COINS = "Sound\\Interface\\LootCoinSmall.wav"
local SOUND_HAMMER = { "Sound\\Doodad\\DalaranForgeArmsHammer1.wav", "Sound\\Doodad\\DalaranForgeArmsHammer2.wav",
    "Sound\\Doodad\\DalaranForgeArmsHammer3.wav" }
local SOUND_QUENCH = "Sound\\Doodad\\JewelCraft_Grinder01Steam1.wav"
local SOUND_FORGE_FIRE = "Sound\\Doodad\\BE_Forge01.wav"

local WIDTH, HEIGHT = 760, 500
local LIST_WIDTH, ROW_HEIGHT = 300, 46
local HAMMER_STRIKES, HAMMER_GAP = 3, 0.42

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "La Forge",
    heading = "La Forge",
    intro = "Confiez votre équipement au maître forgeron : contre de l'or, il le remet sur l'enclume et le rend plus "
        .. "puissant. Chaque passage à la forge ajoute 4 niveaux d'objet, jusqu'à %d fois.",
    list = "Votre équipement",
    empty = "Rien à forger.\n\nLe forgeron ne travaille que l'équipement des défis les plus rudes : "
        .. "épique, de niveau d'objet 200 ou plus, et le butin du Mythique+.",
    worn = "Porté",
    bags = "Sacs",
    pick = "Choisissez une pièce d'équipement.",
    itemLevel = "Niveau d'objet",
    rank = "Rang %d/%d",
    price = "Prix du forgeron",
    purse = "Votre bourse",
    forge = "Forger",
    working = "Le forgeron est à l'œuvre…",
    maxed = "Le forgeron ne peut rien en tirer de plus.",
    done = "%s sort de la forge : niveau d'objet %d !",
    preview = "Après la forge",
    errors = {
        [1] = "Cette pièce n'est plus là.",
        [2] = "Le forgeron ne travaille pas cette pièce.",
        [3] = "Le forgeron ne peut rien en tirer de plus.",
        [4] = "Vous n'avez pas assez d'or.",
        [5] = "Impossible en combat.",
        [6] = "Impossible une fois mort.",
    },
} or {
    title = "The Forge",
    heading = "The Forge",
    intro = "Hand your gear to the master blacksmith: for gold, he puts it back on the anvil and makes it stronger. "
        .. "Every trip to the forge adds 4 item levels, up to %d times.",
    list = "Your gear",
    empty = "Nothing to forge.\n\nThe blacksmith only works gear from the hardest challenges: epic, item level 200 "
        .. "or higher, and Mythic+ loot.",
    worn = "Worn",
    bags = "Bags",
    pick = "Pick a piece of gear.",
    itemLevel = "Item level",
    rank = "Rank %d/%d",
    price = "Blacksmith's price",
    purse = "Your purse",
    forge = "Forge",
    working = "The blacksmith is at work…",
    maxed = "The blacksmith can get nothing more out of it.",
    done = "%s comes out of the forge: item level %d!",
    preview = "After the forge",
    errors = {
        [1] = "That piece is no longer there.",
        [2] = "The blacksmith does not work that piece.",
        [3] = "The blacksmith can get nothing more out of it.",
        [4] = "You do not have enough gold.",
        [5] = "Not while in combat.",
        [6] = "Not while dead.",
    },
}

local state = { items = {}, maxRank = 8, selected = nil, working = false }
local incoming
local frame, rows, listChild, emptyText, anvil

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function SetShown(region, shown)
    if shown then region:Show() else region:Hide() end
end

local function SetEnabled(button, enabled)
    if enabled then button:Enable() else button:Disable() end
end

local function SetAtlas(texture, name)
    local atlas = RetailUIAtlas[name]
    texture:SetTexture(atlas[1])
    texture:SetTexCoord(atlas[4], atlas[5], atlas[6], atlas[7])
end

local function Key(bag, slot)
    return bag .. ":" .. slot
end

-- Tweens: every animation of the window, driven by one clock ------------------------------------------------------

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
            local p = min(tween.time / tween.duration, 1)
            if tween.update then
                tween.update(p)
            end
            if p >= 1 then
                tremove(tweens, index)
                if tween.done then
                    tween.done()
                end
            end
        end
    end
end)

-- Where an item lies, the way the default UI names it ------------------------------------------------------------

local BACKPACK_START, BACKPACK_END = 23, 38     -- INVENTORY_SLOT_ITEM_START / END - 1
local BAG_START = 19                            -- INVENTORY_SLOT_BAG_START

local function SetItemTooltip(tooltip, item)
    if item.bag == 255 and item.slot < BAG_START then
        tooltip:SetInventoryItem("player", item.slot + 1)
    elseif item.bag == 255 then
        tooltip:SetBagItem(0, item.slot - BACKPACK_START + 1)
    else
        tooltip:SetBagItem(item.bag - BAG_START + 1, item.slot + 1)
    end
end

local function IsWorn(item)
    return item.bag == 255 and item.slot < BAG_START
end

-- The item's name and icon; a forged entry the client has not learnt yet comes a moment later
local function ItemInfo(entry)
    local name, _, quality, _, _, _, _, _, _, icon = GetItemInfo(entry)
    return name, quality or 4, icon or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function QualityColor(quality)
    local color = ITEM_QUALITY_COLORS[quality] or ITEM_QUALITY_COLORS[4]
    return color.r, color.g, color.b
end

-- Rank pips: one ember per rank, lit once forged ------------------------------------------------------------------

local function CreatePips(parent, size, gap)
    local pips = {}
    for index = 1, 8 do
        local pip = parent:CreateTexture(nil, "OVERLAY")
        pip:SetTexture("Interface\\COMMON\\Indicator-Gray")
        pip:SetSize(size, size)
        pip:SetPoint("LEFT", parent, "LEFT", (index - 1) * (size + gap), 0)
        pips[index] = pip
    end
    pips.width = 8 * size + 7 * gap
    return pips
end

local function SetPips(pips, rank, maxRank)
    for index = 1, 8 do
        local pip = pips[index]
        SetShown(pip, index <= maxRank)
        if index <= rank then
            pip:SetTexture("Interface\\COMMON\\Indicator-Yellow")
            pip:SetVertexColor(1, 0.72, 0.3)
        else
            pip:SetTexture("Interface\\COMMON\\Indicator-Gray")
            pip:SetVertexColor(0.7, 0.62, 0.5)
        end
    end
end

-- The list ---------------------------------------------------------------------------------------------------------

local RefreshAnvil

local function Select(key, quiet)
    state.selected = key
    if not quiet then
        PlaySoundFile(SOUND_PUT_DOWN)
    end
    for _, row in ipairs(rows) do
        SetShown(row.selectedGlow, row.key == key)
    end
    RefreshAnvil(true)
end

local function CreateRow(index)
    local row = CreateFrame("Button", nil, listChild)
    row:SetSize(LIST_WIDTH - 28, ROW_HEIGHT - 4)
    row:SetPoint("TOPLEFT", listChild, "TOPLEFT", 0, -(index - 1) * ROW_HEIGHT)

    local ground = row:CreateTexture(nil, "BACKGROUND")
    ground:SetAllPoints()
    ground:SetTexture(0.06, 0.045, 0.03, 0.85)

    local highlight = row:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetAllPoints()
    highlight:SetTexture(1, 0.75, 0.35, 0.08)

    local selectedGlow = row:CreateTexture(nil, "BORDER")
    selectedGlow:SetAllPoints()
    selectedGlow:SetTexture(1, 0.6, 0.2, 0.16)
    selectedGlow:Hide()
    row.selectedGlow = selectedGlow

    local edge = row:CreateTexture(nil, "ARTWORK")
    edge:SetTexture(0.75, 0.6, 0.35, 0.6)
    edge:SetPoint("BOTTOMLEFT")
    edge:SetPoint("BOTTOMRIGHT")
    edge:SetHeight(1)

    local icon = row:CreateTexture(nil, "ARTWORK")
    icon:SetSize(34, 34)
    icon:SetPoint("LEFT", 4, 0)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    row.icon = icon

    local name = row:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    name:SetPoint("TOPLEFT", icon, "TOPRIGHT", 8, -1)
    name:SetPoint("RIGHT", row, "RIGHT", -6, 0)
    name:SetJustifyH("LEFT")
    row.name = name

    local detail = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    detail:SetPoint("BOTTOMLEFT", icon, "BOTTOMRIGHT", 8, 1)
    detail:SetTextColor(0.8, 0.74, 0.62)
    row.detail = detail

    local pipHolder = CreateFrame("Frame", nil, row)
    pipHolder:SetSize(100, 8)
    pipHolder:SetPoint("BOTTOMRIGHT", row, "BOTTOMRIGHT", -8, 5)
    row.pips = CreatePips(pipHolder, 8, 2)
    pipHolder:SetWidth(row.pips.width)

    row:SetScript("OnClick", function(self)
        if not state.working then
            Select(self.key)
        end
    end)
    row:SetScript("OnEnter", function(self)
        local item = state.items[self.key]
        if item then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            SetItemTooltip(GameTooltip, item)
            GameTooltip:Show()
        end
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return row
end

local function OrderedKeys()
    local keys = {}
    for key in pairs(state.items) do
        tinsert(keys, key)
    end
    sort(keys, function(a, b)
        local itemA, itemB = state.items[a], state.items[b]
        if IsWorn(itemA) ~= IsWorn(itemB) then
            return IsWorn(itemA)
        end
        if itemA.bag ~= itemB.bag then
            return itemA.bag > itemB.bag
        end
        return itemA.slot < itemB.slot
    end)
    return keys
end

local function RefreshList()
    local keys = OrderedKeys()
    for index, key in ipairs(keys) do
        local row = rows[index] or CreateRow(index)
        rows[index] = row
        local item = state.items[key]
        local name, quality, icon = ItemInfo(item.entry)
        row.key = key
        row.icon:SetTexture(icon)
        row.name:SetText(name or "…")
        row.name:SetTextColor(QualityColor(quality))
        row.detail:SetText(format("%s %d · %s", TEXT.itemLevel, item.itemLevel,
            IsWorn(item) and TEXT.worn or TEXT.bags))
        SetPips(row.pips, item.rank, state.maxRank)
        SetShown(row.selectedGlow, key == state.selected)
        row:Show()
    end
    for index = #keys + 1, #rows do
        rows[index]:Hide()
    end
    listChild:SetHeight(max(1, #keys * ROW_HEIGHT))
    SetShown(emptyText, #keys == 0)

    if state.selected and not state.items[state.selected] then
        state.selected = nil
    end
    if not state.selected and keys[1] then
        Select(keys[1], true)
    else
        RefreshAnvil(false)
    end
end

-- The anvil --------------------------------------------------------------------------------------------------------

function RefreshAnvil(animate)
    local item = state.selected and state.items[state.selected]
    SetShown(anvil.content, item ~= nil)
    SetShown(anvil.pick, item == nil)
    if not item then
        anvil.preview:Hide()
        return
    end

    local name, quality, icon = ItemInfo(item.entry)
    anvil.icon:SetTexture(icon)
    anvil.name:SetText(name or "…")
    anvil.name:SetTextColor(QualityColor(quality))
    SetPips(anvil.pips, item.rank, state.maxRank)
    anvil.rank:SetText(format(TEXT.rank, item.rank, state.maxRank))
    anvil.money:SetText(GetCoinTextureString(GetMoney()))

    local maxed = item.nextEntry == 0
    if maxed then
        anvil.levels:SetText(format("|cffffe0a0%d|r", item.itemLevel))
        anvil.cost:SetText("")
        anvil.status:SetText(TEXT.maxed)
    else
        anvil.levels:SetText(format("|cffc8b89a%d|r  |cffb08850>|r  |cffffd060%d|r", item.itemLevel,
            item.nextItemLevel))
        anvil.cost:SetText(GetCoinTextureString(item.cost))
        anvil.status:SetText(state.working and TEXT.working or "")
    end
    SetShown(anvil.costLabel, not maxed)
    SetEnabled(anvil.button, not maxed and not state.working and GetMoney() >= item.cost)

    -- What it becomes: the next rank's own tooltip, beside the anvil
    local preview = anvil.preview
    if maxed then
        preview:Hide()
    else
        preview:SetOwner(frame, "ANCHOR_NONE")
        preview:ClearAllPoints()
        preview:SetPoint("TOPLEFT", frame, "TOPRIGHT", -6, -60)
        preview:SetHyperlink("item:" .. item.nextEntry)
        preview:AddLine(" ")
        preview:AddLine(TEXT.preview, 1, 0.62, 0.25)
        preview:Show()
    end

    if animate then
        anvil.icon:SetAlpha(0)
        Tween(0.25, 0, function(p)
            anvil.icon:SetAlpha(p)
            local size = 58 + 14 * (1 - OutBack(p))
            anvil.icon:SetSize(size, size)
        end)
    end
end

-- The blacksmith at work: three blows of the hammer, sparks on each, then the quench, and the piece comes out
-- glowing with its new item level
local function Spark()
    local spark = anvil.spark
    spark:SetAlpha(1)
    Tween(0.35, 0, function(p)
        local size = 60 + 90 * OutCubic(p)
        spark:SetSize(size, size)
        spark:SetAlpha(1 - p)
    end)
    local icon = anvil.iconHolder
    Tween(0.12, 0, function(p)
        icon:ClearAllPoints()
        icon:SetPoint("CENTER", anvil.content, "TOP", 0, -104 - 5 * (1 - p))
    end)
end

local function AnimateForge(item, onDone)
    state.working = true
    PlaySoundFile(SOUND_FORGE_FIRE)
    anvil.fire:SetAlpha(0)
    Tween(0.5, 0, function(p) anvil.fire:SetAlpha(0.35 + 0.65 * p) end)
    for strike = 1, HAMMER_STRIKES do
        Tween(0.01, 0.25 + (strike - 1) * HAMMER_GAP, nil, function()
            PlaySoundFile(SOUND_HAMMER[strike])
            Spark()
        end)
    end
    local quench = 0.25 + HAMMER_STRIKES * HAMMER_GAP + 0.15
    Tween(0.01, quench, nil, function()
        PlaySoundFile(SOUND_QUENCH)
        -- The list that came with the news is shown now, out of the quench
        RefreshList()
        onDone()
    end)
    Tween(0.8, quench, function(p)
        anvil.fire:SetAlpha(1 - p)
        anvil.glow:SetAlpha(1 - p)
        local size = 70 + 60 * OutCubic(p)
        anvil.glow:SetSize(size, size)
    end, function()
        state.working = false
        RefreshAnvil(false)
    end)
end

-- The window -------------------------------------------------------------------------------------------------------

local function CreateForge()
    frame = CreateFrame("Frame", "ItemForgeFrame", UIParent)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("CENTER", -120, 20)
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

    -- The smithy's sign: the blacksmith's hammer and the heading beside it
    local emblem = frame:CreateTexture(nil, "OVERLAY")
    emblem:SetTexture("Interface\\Icons\\Trade_BlackSmithing")
    emblem:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    emblem:SetSize(40, 40)
    emblem:SetPoint("TOPLEFT", frame, "TOPLEFT", 34, -40)
    local emblemBorder = frame:CreateTexture(nil, "OVERLAY", nil, 1)
    emblemBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    emblemBorder:SetPoint("TOPLEFT", emblem, "TOPLEFT", -13, 13)
    emblemBorder:SetPoint("BOTTOMRIGHT", emblem, "BOTTOMRIGHT", 13, -13)

    local heading = frame:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 28)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(1, 0.86, 0.55)
    heading:SetPoint("TOPLEFT", emblem, "TOPRIGHT", 14, 2)
    heading:SetText(TEXT.heading)

    local intro = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    intro:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 2, -4)
    intro:SetWidth(WIDTH - 150)
    intro:SetJustifyH("LEFT")
    intro:SetTextColor(0.85, 0.8, 0.7)
    frame.intro = intro

    -- The list of gear, down the left
    local listLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    listLabel:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -104)
    listLabel:SetText(TEXT.list)

    local listBox = CreateFrame("Frame", nil, frame)
    listBox:SetPoint("TOPLEFT", frame, "TOPLEFT", 20, -122)
    listBox:SetSize(LIST_WIDTH, HEIGHT - 146)
    listBox:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 12, insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    listBox:SetBackdropColor(0.02, 0.015, 0.01, 0.75)
    listBox:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)

    local scroll = CreateFrame("ScrollFrame", "ItemForgeListScroll", listBox, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", listBox, "TOPLEFT", 6, -6)
    scroll:SetPoint("BOTTOMRIGHT", listBox, "BOTTOMRIGHT", -26, 6)
    listChild = CreateFrame("Frame", nil, scroll)
    listChild:SetSize(LIST_WIDTH - 32, 1)
    scroll:SetScrollChild(listChild)
    rows = {}

    emptyText = listBox:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    emptyText:SetPoint("TOPLEFT", listBox, "TOPLEFT", 16, -30)
    emptyText:SetPoint("TOPRIGHT", listBox, "TOPRIGHT", -16, -30)
    emptyText:SetJustifyH("CENTER")
    emptyText:SetTextColor(0.85, 0.78, 0.62)
    emptyText:SetText(TEXT.empty)
    emptyText:Hide()

    -- The anvil, on the right: the forge's embers glow under it while the blacksmith works
    anvil = CreateFrame("Frame", nil, frame)
    anvil:SetPoint("TOPLEFT", listBox, "TOPRIGHT", 16, 0)
    anvil:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -20, 24)
    anvil:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 12, insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    anvil:SetBackdropColor(0.05, 0.03, 0.02, 0.85)
    anvil:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)

    local embers = anvil:CreateTexture(nil, "BORDER")
    embers:SetTexture("Interface\\Buttons\\WHITE8X8")
    embers:SetPoint("BOTTOMLEFT", 4, 4)
    embers:SetPoint("BOTTOMRIGHT", -4, 4)
    embers:SetHeight(150)
    embers:SetGradientAlpha("VERTICAL", 0.55, 0.2, 0.04, 0.3, 0.55, 0.2, 0.04, 0)

    local fire = anvil:CreateTexture(nil, "BORDER", nil, 1)
    fire:SetTexture("Interface\\Buttons\\WHITE8X8")
    fire:SetPoint("BOTTOMLEFT", 4, 4)
    fire:SetPoint("BOTTOMRIGHT", -4, 4)
    fire:SetHeight(300)
    fire:SetGradientAlpha("VERTICAL", 1, 0.45, 0.08, 0.55, 1, 0.45, 0.08, 0)
    fire:SetAlpha(0)
    anvil.fire = fire

    local pick = anvil:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    pick:SetPoint("CENTER")
    pick:SetTextColor(0.85, 0.78, 0.62)
    pick:SetText(TEXT.pick)
    anvil.pick = pick

    local content = CreateFrame("Frame", nil, anvil)
    content:SetAllPoints()
    anvil.content = content

    local name = content:CreateFontString(nil, "OVERLAY")
    name:SetFont(MORPHEUS, 20)
    name:SetShadowOffset(1, -1)
    name:SetPoint("TOP", content, "TOP", 0, -20)
    name:SetWidth(360)
    anvil.name = name

    local iconHolder = CreateFrame("Frame", nil, content)
    iconHolder:SetSize(80, 80)
    iconHolder:SetPoint("CENTER", content, "TOP", 0, -104)
    anvil.iconHolder = iconHolder

    local glow = iconHolder:CreateTexture(nil, "BACKGROUND")
    glow:SetTexture("Interface\\Cooldown\\star4")
    glow:SetBlendMode("ADD")
    glow:SetVertexColor(1, 0.6, 0.2)
    glow:SetPoint("CENTER")
    glow:SetSize(70, 70)
    glow:SetAlpha(0)
    anvil.glow = glow

    local icon = iconHolder:CreateTexture(nil, "ARTWORK")
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    icon:SetSize(58, 58)
    icon:SetPoint("CENTER")
    anvil.icon = icon

    local iconBorder = iconHolder:CreateTexture(nil, "OVERLAY")
    iconBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    iconBorder:SetPoint("TOPLEFT", icon, "TOPLEFT", -18, 18)
    iconBorder:SetPoint("BOTTOMRIGHT", icon, "BOTTOMRIGHT", 18, -18)

    local spark = iconHolder:CreateTexture(nil, "OVERLAY", nil, 2)
    spark:SetTexture("Interface\\Cooldown\\star4")
    spark:SetBlendMode("ADD")
    spark:SetVertexColor(1, 0.8, 0.4)
    spark:SetPoint("CENTER")
    spark:SetSize(60, 60)
    spark:SetAlpha(0)
    anvil.spark = spark

    iconHolder:EnableMouse(true)
    iconHolder:SetScript("OnEnter", function(self)
        local item = state.selected and state.items[state.selected]
        if item then
            GameTooltip:SetOwner(self, "ANCHOR_LEFT")
            SetItemTooltip(GameTooltip, item)
            GameTooltip:Show()
        end
    end)
    iconHolder:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local levelLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    levelLabel:SetPoint("TOP", iconHolder, "BOTTOM", 0, -14)
    levelLabel:SetText(TEXT.itemLevel)

    local levels = content:CreateFontString(nil, "OVERLAY")
    levels:SetFont(MORPHEUS, 30)
    levels:SetShadowOffset(1, -1)
    levels:SetPoint("TOP", levelLabel, "BOTTOM", 0, -4)
    anvil.levels = levels

    local pipHolder = CreateFrame("Frame", nil, content)
    pipHolder:SetSize(100, 12)
    anvil.pips = CreatePips(pipHolder, 12, 4)
    pipHolder:SetWidth(anvil.pips.width)
    pipHolder:SetPoint("TOP", levels, "BOTTOM", 0, -12)

    local rank = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    rank:SetPoint("TOP", pipHolder, "BOTTOM", 0, -6)
    rank:SetTextColor(0.85, 0.78, 0.62)
    anvil.rank = rank

    local costLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    costLabel:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 24, 84)
    costLabel:SetText(TEXT.price)
    anvil.costLabel = costLabel

    local cost = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightLarge")
    cost:SetPoint("LEFT", costLabel, "RIGHT", 10, 0)
    anvil.cost = cost

    local purseLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    purseLabel:SetPoint("TOPLEFT", costLabel, "BOTTOMLEFT", 0, -8)
    purseLabel:SetText(TEXT.purse)
    purseLabel:SetTextColor(0.8, 0.74, 0.62)

    local money = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    money:SetPoint("LEFT", purseLabel, "RIGHT", 8, 0)
    anvil.money = money

    local button = CreateFrame("Button", nil, content, "UIPanelButtonTemplate")
    button:SetSize(170, 30)
    button:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -24, 22)
    button:SetText(TEXT.forge)
    button:SetScript("OnClick", function()
        local item = state.selected and state.items[state.selected]
        if not item or state.working or item.nextEntry == 0 then
            return
        end
        PlaySoundFile(SOUND_COINS)
        state.pending = state.selected
        SetEnabled(button, false)
        anvil.status:SetText(TEXT.working)
        Send(format("U\t%d\t%d\t%d", item.bag, item.slot, item.entry))
    end)
    anvil.button = button

    local status = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    status:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 24, 30)
    status:SetPoint("RIGHT", button, "LEFT", -12, 0)
    status:SetJustifyH("LEFT")
    status:SetTextColor(1, 0.72, 0.35)
    anvil.status = status

    -- The next rank's tooltip, shown beside the window
    anvil.preview = CreateFrame("GameTooltip", "ItemForgePreviewTooltip", frame, "GameTooltipTemplate")

    frame:SetScript("OnShow", function()
        PlaySoundFile(SOUND_BELLOW_IN)
        Tween(0.01, 0.55, nil, function() PlaySoundFile(SOUND_BELLOW_OUT) end)
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
        anvil.preview:Hide()
    end)
    -- A forged entry the client did not know yet has its name a moment later
    local wait = 0
    frame:SetScript("OnUpdate", function(_, elapsed)
        wait = wait + elapsed
        if wait < 1 then
            return
        end
        wait = 0
        for _, row in ipairs(rows) do
            if row:IsShown() and row.name:GetText() == "…" then
                RefreshList()
                return
            end
        end
    end)
    frame:RegisterEvent("PLAYER_MONEY")
    frame:SetScript("OnEvent", function()
        if not state.working then
            RefreshAnvil(false)
        end
    end)

    tinsert(UISpecialFrames, "ItemForgeFrame")
end

-- Messages ---------------------------------------------------------------------------------------------------------

local function ShowError(code)
    PlaySound("igQuestFailed")
    UIErrorsFrame:AddMessage(TEXT.errors[tonumber(code)] or TEXT.errors[1], 1, 0.3, 0.2, 1)
    state.pending = nil
    if frame and frame:IsShown() then
        RefreshAnvil(false)
    end
end

-- The blacksmith is done: the piece keeps its place, its entry and rank move on
local function OnForged(bag, slot, entry, rank, itemLevel)
    local key = Key(bag, slot)
    state.pending = nil
    if not frame or not frame:IsShown() then
        return
    end
    state.selected = key
    AnimateForge(state.items[key], function()
        local name = ItemInfo(entry)
        local message = format(TEXT.done, name or "", itemLevel)
        UIErrorsFrame:AddMessage(message, 1, 0.72, 0.35, 1)
        -- The level jumps up out of the quench
        Tween(0.35, 0, function(p)
            anvil.levels:SetFont(MORPHEUS, 30 + 14 * (1 - OutCubic(p)))
        end)
    end)
end

local function Handle(message)
    local kind, a, b, c, d, e, f, g, h = strsplit("\t", message)
    if kind == "O" then
        incoming = {}
    elseif kind == "I" and incoming then
        local item = {
            bag = tonumber(a) or 0, slot = tonumber(b) or 0, entry = tonumber(c) or 0, rank = tonumber(d) or 0,
            itemLevel = tonumber(e) or 0, nextEntry = tonumber(f) or 0, nextItemLevel = tonumber(g) or 0,
            cost = tonumber(h) or 0,
        }
        incoming[Key(item.bag, item.slot)] = item
    elseif kind == "E" then
        state.items = incoming or {}
        incoming = nil
        state.maxRank = tonumber(b) or state.maxRank
        if not frame then
            if a ~= "1" then
                return
            end
            CreateForge()
        end
        frame.intro:SetText(format(TEXT.intro, state.maxRank))
        if a == "1" and not frame:IsShown() then
            frame:Show()
        end
        -- While the hammer falls, the new list waits for the quench (AnimateForge)
        if frame:IsShown() and not state.working then
            RefreshList()
        end
    elseif kind == "D" then
        OnForged(tonumber(a) or 0, tonumber(b) or 0, tonumber(c) or 0, tonumber(d) or 0, tonumber(e) or 0)
    elseif kind == "X" then
        ShowError(a)
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        Handle(message)
    end
end)
