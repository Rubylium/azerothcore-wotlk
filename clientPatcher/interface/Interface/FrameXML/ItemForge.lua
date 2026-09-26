-- The Forge (server: modules/mod-forge Forge.cpp, protocol at the top of it). The player brings a piece of high-end
-- gear to the master smith and pays him in gold; it goes back on the anvil and comes out 4 item levels higher, up to
-- 8 times. Opened by talking to the smith at his anvil in each capital, or with .forge.
--
-- It is meant to feel like an event, and every sound is the smithy's: metal set down on the anvil, coins handed over,
-- the hammer three times (the smith strikes in the world at the same moments), and the hiss of the quench. Every
-- second rank rings out; a masterwork strikes a fourth, golden blow and takes two ranks; the eighth makes the piece a
-- masterpiece, announced across the screen. The Forge is marked on the world map, where the smith stands.
--
-- Forged gear shows it outside the window too: its icon smoulders in the bags and on the character sheet, more with
-- every rank, and its tooltip tells what the smith was paid for it. The smith remembers his customers: the window
-- shows the player's standing with him, what it brings, and how far the next one is.

local PREFIX = "Forge"
local MORPHEUS = "Fonts\\MORPHEUS.ttf"
local FONT = GameFontNormal:GetFont()
local GENERATED_ITEM_BASE = 0x10000     -- MythicDungeon.h
local MYTHIC_VARIANTS = 128             -- MythicDungeon.h, GeneratedItemVariants
local MAX_RANK = 8                      -- MythicDungeon.h, ForgeRanks

-- Sound entries (SoundEntries.dbc), never raw files: PlaySound follows the game's volume settings and each entry's
-- own volume, where PlaySoundFile plays a file at full volume whatever the player has set
local SOUND_OPEN = "igQuestListOpen"
local SOUND_CLOSE = "igQuestListClose"
local SOUND_PUT_DOWN = "PutDownLargeMetal"
local SOUND_COINS = "LOOTWINDOWCOINSOUND"
local SOUND_HAMMER = "DalaranForgeArmsHammerOneshots"      -- a different blow each time
local SOUND_QUENCH = "JewelCraft_Grinder01Steam"
local SOUND_MILESTONE = "LEVELUPSOUND"
local SOUND_MASTERWORK = "igQuestListComplete"
local SOUND_MASTERPIECE = "AchievementSound"

local WIDTH, HEIGHT = 780, 540
local LIST_WIDTH, ROW_HEIGHT = 300, 46
-- The server's smith strikes at the same moments (Forge.cpp HammerStrikes)
local HAMMER_DELAY, HAMMER_GAP, HAMMER_STRIKES = 0.25, 0.42, 3

-- The smith's standings (Forge.cpp Standings): the gold paid in all, the discount, the masterwork chance
local STANDINGS = {
    { gold = 0, discount = 0, masterwork = 0 },
    { gold = 5000, discount = 5, masterwork = 0 },
    { gold = 15000, discount = 5, masterwork = 5 },
    { gold = 35000, discount = 10, masterwork = 5 },
    { gold = 75000, discount = 10, masterwork = 10 },
}

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "La Forge",
    heading = "La Forge",
    intro = "Confiez votre équipement au maître forgeron : contre de l'or, il le remet sur l'enclume et le rend "
        .. "plus puissant. Chaque passage à la forge ajoute 4 niveaux d'objet, jusqu'à %d fois.",
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
    invested = "Déjà investi : %s",
    forge = "Forger",
    working = "Le forgeron est à l'œuvre…",
    maxed = "Un chef-d'œuvre : le forgeron ne peut rien en tirer de plus.",
    done = "%s sort de la forge : niveau d'objet %d !",
    gains = "La forge y ajoutera",
    preview = "Après la forge",
    masterwork = "Coup de maître ! Deux rangs pour le prix d'un.",
    milestone = "Rang %d : la pièce prend du caractère.",
    masterpiece = "Chef-d'œuvre !",
    masterpieceLine = "Forgé %d/%d · niveau d'objet %d",
    standingLabel = "Renommée auprès du forgeron",
    standingNames = { "Client de passage", "Client régulier", "Client estimé", "Ami de la forge",
        "Légende de l'enclume" },
    standingNext = "%s / %s po",
    standingTop = "%s po versés",
    perkNone = "Payez le forgeron : il se souviendra de vous.",
    perkDiscount = "Remise de %d %%",
    perkMasterwork = "Coup de maître : %d %%",
    standingUp = "Renommée : %s",
    tooltipInvested = "Or investi à la forge : %s",
    tooltipRank = "Forgé %d/%d",
    mapTitle = "La Forge",
    mapSmith = "Durgan Frappe-Braise, maître forgeron",
    mapHint = "Améliorez votre équipement contre de l'or.",
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
    invested = "Already invested: %s",
    forge = "Forge",
    working = "The blacksmith is at work…",
    maxed = "A masterpiece: the blacksmith can get nothing more out of it.",
    done = "%s comes out of the forge: item level %d!",
    gains = "The forge will add",
    preview = "After the forge",
    masterwork = "A masterwork! Two ranks for the price of one.",
    milestone = "Rank %d: the piece takes on character.",
    masterpiece = "Masterpiece!",
    masterpieceLine = "Forged %d/%d · item level %d",
    standingLabel = "Standing with the blacksmith",
    standingNames = { "Passing customer", "Regular", "Valued customer", "Friend of the forge", "Legend of the anvil" },
    standingNext = "%s / %s g",
    standingTop = "%s g paid",
    perkNone = "Pay the blacksmith: he will remember you.",
    perkDiscount = "%d%% discount",
    perkMasterwork = "Masterwork: %d%%",
    standingUp = "Standing: %s",
    tooltipInvested = "Gold invested at the forge: %s",
    tooltipRank = "Forged %d/%d",
    mapTitle = "The Forge",
    mapSmith = "Durgan Emberstrike, master blacksmith",
    mapHint = "Upgrade your gear for gold.",
    errors = {
        [1] = "That piece is no longer there.",
        [2] = "The blacksmith does not work that piece.",
        [3] = "The blacksmith can get nothing more out of it.",
        [4] = "You do not have enough gold.",
        [5] = "Not while in combat.",
        [6] = "Not while dead.",
    },
}

local state = { items = {}, maxRank = MAX_RANK, selected = nil, working = false, spent = 0, standing = 0 }
local incoming
local frame, rows, listChild, emptyText, anvil, standing, banner

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

-- A number the way the player's language writes it: 12 345 or 12,345
local function Thousands(value)
    local text = tostring(floor(value))
    local separator = french and " " or ","
    local result = text:reverse():gsub("(%d%d%d)", "%1" .. separator):reverse()
    return (result:gsub("^" .. separator, ""))
end

local function Gold(copper)
    return Thousands(floor((copper or 0) / 10000))
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

-- Where an item lies ------------------------------------------------------------------------------------------------
-- The server names a place by bag and slot: bag 255 is the character, its slots 0-18 worn and 23-38 the backpack;
-- bags 19-22 are the four bags. The default UI numbers them otherwise; these convert.

local BACKPACK_START = 23
local BAG_START = 19

local function SetItemTooltip(tooltip, item)
    if item.bag == 255 and item.slot < BAG_START then
        tooltip:SetInventoryItem("player", item.slot + 1)
    elseif item.bag == 255 then
        tooltip:SetBagItem(0, item.slot - BACKPACK_START + 1)
    else
        tooltip:SetBagItem(item.bag - BAG_START + 1, item.slot + 1)
    end
end

local function KeyOfContainer(container, slot)
    if container == 0 then
        return Key(255, BACKPACK_START + slot - 1)
    end
    return Key(BAG_START + container - 1, slot - 1)
end

local function KeyOfInventory(inventorySlot)
    return Key(255, inventorySlot - 1)
end

local function IsWorn(item)
    return item.bag == 255 and item.slot < BAG_START
end

-- An item's forged ranks: a real item's entry says it; a Mythic+ variant's, only the server's list knows
local function RankOf(itemId, key)
    if not itemId or itemId < GENERATED_ITEM_BASE then
        return 0
    end
    local block = floor(itemId / GENERATED_ITEM_BASE)
    if block > MYTHIC_VARIANTS then
        return min(block - MYTHIC_VARIANTS, MAX_RANK)
    end
    local item = key and state.items[key]
    return item and item.rank or 0
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

-- The smith's standing ----------------------------------------------------------------------------------------------

local function StandingPerks(index)
    local tier = STANDINGS[index]
    local perks = {}
    if tier.discount > 0 then
        tinsert(perks, format(TEXT.perkDiscount, tier.discount))
    end
    if tier.masterwork > 0 then
        tinsert(perks, format(TEXT.perkMasterwork, tier.masterwork))
    end
    return #perks > 0 and table.concat(perks, "  ·  ") or TEXT.perkNone
end

local function RefreshStanding()
    if not standing then
        return
    end
    local index = state.standing + 1
    local gold = floor(state.spent / 10000)
    local next = STANDINGS[index + 1]
    standing.name:SetText(TEXT.standingNames[index])
    standing.perks:SetText(StandingPerks(index))
    if next then
        local from = STANDINGS[index].gold
        standing.fill:SetWidth(max(1, standing.bar:GetWidth() * min(1, (gold - from) / (next.gold - from))))
        standing.value:SetText(format(TEXT.standingNext, Thousands(gold), Thousands(next.gold)))
    else
        standing.fill:SetWidth(standing.bar:GetWidth())
        standing.value:SetText(format(TEXT.standingTop, Thousands(gold)))
    end
end

-- Rank pips: one ember per rank, lit once forged ------------------------------------------------------------------

-- A pip is a small bronze frame around an ember that lights up: drawn, since the client has no indicator art
local function CreatePips(parent, size, gap)
    local pips = {}
    for index = 1, MAX_RANK do
        local frame = parent:CreateTexture(nil, "ARTWORK")
        frame:SetTexture(0.55, 0.42, 0.24, 1)
        frame:SetSize(size, size)
        frame:SetPoint("LEFT", parent, "LEFT", (index - 1) * (size + gap), 0)
        local ember = parent:CreateTexture(nil, "OVERLAY")
        ember:SetPoint("TOPLEFT", frame, "TOPLEFT", 1, -1)
        ember:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -1, 1)
        pips[index] = { frame = frame, ember = ember }
    end
    pips.width = MAX_RANK * size + (MAX_RANK - 1) * gap
    return pips
end

local function SetPips(pips, rank, maxRank)
    for index = 1, MAX_RANK do
        local pip = pips[index]
        SetShown(pip.frame, index <= maxRank)
        SetShown(pip.ember, index <= maxRank)
        if index > rank then
            pip.ember:SetTexture(0.1, 0.07, 0.05, 1)
        elseif rank >= MAX_RANK then
            pip.ember:SetTexture(1, 0.86, 0.4, 1)
        else
            pip.ember:SetTexture(1, 0.55, 0.15, 1)
        end
    end
end

-- What the next rank adds: the stats of the two entries, compared ---------------------------------------------------

local function StatGains(fromEntry, toEntry)
    local before = GetItemStats("item:" .. fromEntry)
    local after = GetItemStats("item:" .. toEntry)
    if not before or not after then
        return nil
    end
    local gains = {}
    for stat, value in pairs(after) do
        local gain = value - (before[stat] or 0)
        local name = _G[stat]
        if gain > 0.05 and name then
            tinsert(gains, { name = name, gain = gain })
        end
    end
    sort(gains, function(a, b) return a.gain > b.gain end)
    return gains
end

local function ShowGains(item)
    local lines = anvil.gains
    local gains = item.nextEntry ~= 0 and StatGains(item.entry, item.nextEntry)
    anvil.gainsPending = item.nextEntry ~= 0 and not gains
    for index, line in ipairs(lines) do
        local gain = gains and gains[index]
        if gain then
            local amount = gain.gain >= 10 and format("%d", gain.gain + 0.5) or format("%.1f", gain.gain)
            line:SetText(format("|cffffd060+%s|r %s", amount, gain.name))
            line:Show()
        else
            line:Hide()
        end
    end
    SetShown(anvil.gainsLabel, gains and #gains > 0)
end

-- The list ---------------------------------------------------------------------------------------------------------

local RefreshAnvil

local function Select(key, quiet)
    state.selected = key
    if not quiet then
        PlaySound(SOUND_PUT_DOWN)
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
    if not frame then
        return
    end
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
    RefreshStanding()

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
    anvil.invested:SetText(item.invested > 0 and format(TEXT.invested, GetCoinTextureString(item.invested)) or "")
    anvil.masterGlow:SetAlpha(item.rank >= MAX_RANK and 0.55 or 0)

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
    ShowGains(item)

    -- What it becomes: the next rank's own tooltip, beside the window
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

-- The smith at work -------------------------------------------------------------------------------------------------

-- One blow of the hammer: sparks off the piece, the piece jolts on the anvil
local function Strike(index, golden)
    PlaySound(SOUND_HAMMER)
    local spark = anvil.spark
    if golden then
        spark:SetVertexColor(1, 0.92, 0.55)
    else
        spark:SetVertexColor(1, 0.75, 0.35)
    end
    Tween(0.35, 0, function(p)
        local size = 60 + (golden and 150 or 90) * OutCubic(p)
        spark:SetSize(size, size)
        spark:SetAlpha(1 - p)
    end)
    local holder = anvil.iconHolder
    Tween(0.12, 0, function(p)
        holder:ClearAllPoints()
        holder:SetPoint("CENTER", anvil.content, "TOP", 0, -82 - 5 * (1 - p))
    end)
end

-- The piece comes out of the quench: a flash as big as the moment
local function Flash(size, red, green, blue)
    local glow = anvil.glow
    glow:SetVertexColor(red, green, blue)
    Tween(0.9, 0, function(p)
        glow:SetAlpha(1 - p)
        glow:SetSize(70 + size * OutCubic(p), 70 + size * OutCubic(p))
    end)
end

-- A piece of the banner: the masterpiece, or a new standing with the smith
local function ShowBanner(icon, title, line, detail, red, green, blue)
    if not banner then
        banner = CreateFrame("Frame", "ItemForgeBanner", UIParent)
        banner:SetSize(480, 104)
        banner:SetPoint("TOP", UIParent, "TOP", 0, -150)
        banner:SetFrameStrata("DIALOG")
        banner:SetBackdrop({
            bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = false, edgeSize = 14, insets = { left = 3, right = 3, top = 3, bottom = 3 },
        })
        banner:SetBackdropColor(0.04, 0.03, 0.02, 0.92)
        banner:SetBackdropBorderColor(1, 0.82, 0.35, 1)
        banner:Hide()

        local shine = banner:CreateTexture(nil, "BACKGROUND", nil, 1)
        shine:SetTexture("Interface\\AchievementFrame\\UI-Achievement-Alert-Glow")
        shine:SetTexCoord(0, 0.782, 0, 0.782)
        shine:SetBlendMode("ADD")
        shine:SetVertexColor(1, 0.55, 0.15)
        shine:SetAlpha(0.6)
        shine:SetPoint("TOPLEFT", 4, -4)
        shine:SetPoint("BOTTOMRIGHT", -4, 4)

        local star = banner:CreateTexture(nil, "ARTWORK")
        star:SetTexture("Interface\\Cooldown\\star4")
        star:SetBlendMode("ADD")
        star:SetSize(150, 150)
        star:SetPoint("CENTER", banner, "LEFT", 58, 0)
        banner.star = star

        local bannerIcon = banner:CreateTexture(nil, "OVERLAY")
        bannerIcon:SetSize(56, 56)
        bannerIcon:SetPoint("CENTER", banner, "LEFT", 58, 0)
        bannerIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        banner.icon = bannerIcon

        local border = banner:CreateTexture(nil, "OVERLAY", nil, 1)
        border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
        border:SetPoint("TOPLEFT", bannerIcon, "TOPLEFT", -17, 17)
        border:SetPoint("BOTTOMRIGHT", bannerIcon, "BOTTOMRIGHT", 17, -17)

        local bannerTitle = banner:CreateFontString(nil, "OVERLAY")
        bannerTitle:SetFont(MORPHEUS, 30)
        bannerTitle:SetShadowOffset(1, -1)
        bannerTitle:SetPoint("TOPLEFT", banner, "TOPLEFT", 110, -14)
        banner.title = bannerTitle

        local bannerLine = banner:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
        bannerLine:SetPoint("TOPLEFT", bannerTitle, "BOTTOMLEFT", 0, -4)
        bannerLine:SetPoint("RIGHT", banner, "RIGHT", -16, 0)
        bannerLine:SetJustifyH("LEFT")
        banner.line = bannerLine

        local bannerDetail = banner:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        bannerDetail:SetPoint("TOPLEFT", bannerLine, "BOTTOMLEFT", 0, -4)
        bannerDetail:SetTextColor(0.85, 0.78, 0.62)
        banner.detail = bannerDetail
    end

    banner.icon:SetTexture(icon)
    banner.title:SetText(title)
    banner.title:SetTextColor(1, 0.86, 0.45)
    banner.line:SetText(line)
    banner.line:SetTextColor(red, green, blue)
    banner.detail:SetText(detail)
    banner.shown = (banner.shown or 0) + 1
    local showing = banner.shown
    banner:Show()
    banner:SetAlpha(0)
    Tween(0.35, 0, function(p)
        banner:SetAlpha(p)
        banner:SetScale(1.25 - 0.25 * OutBack(p))
        banner.star:SetAlpha(1 - 0.6 * p)
    end)
    Tween(0.8, 4.2, function(p)
        if banner.shown == showing then
            banner:SetAlpha(1 - p)
        end
    end, function()
        if banner.shown == showing then
            banner:Hide()
        end
    end)
end

local function AnimateForge(item, result, onDone)
    state.working = true
    anvil.fire:SetAlpha(0)
    Tween(0.5, 0, function(p) anvil.fire:SetAlpha(0.35 + 0.65 * p) end)
    local strikes = HAMMER_STRIKES + (result.masterwork and 1 or 0)
    for strike = 1, strikes do
        Tween(0.01, HAMMER_DELAY + (strike - 1) * HAMMER_GAP, nil, function()
            Strike(strike, strike > HAMMER_STRIKES)
        end)
    end
    local quench = HAMMER_DELAY + strikes * HAMMER_GAP + 0.15
    Tween(0.01, quench, nil, function()
        PlaySound(SOUND_QUENCH)
        -- The list that came with the news is shown now, out of the quench
        RefreshList()
        onDone()
    end)
    Tween(0.8, quench, function(p) anvil.fire:SetAlpha(1 - p) end, function()
        state.working = false
        RefreshAnvil(false)
    end)
end

-- The window -------------------------------------------------------------------------------------------------------

local function CreateStanding(parent)
    standing = CreateFrame("Frame", nil, parent)
    standing:SetSize(250, 64)
    standing:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -30, -38)

    local label = standing:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    label:SetPoint("TOPLEFT")
    label:SetText(TEXT.standingLabel)

    local name = standing:CreateFontString(nil, "OVERLAY")
    name:SetFont(FONT, 15)
    name:SetShadowOffset(1, -1)
    name:SetTextColor(1, 0.86, 0.55)
    name:SetPoint("TOPLEFT", label, "BOTTOMLEFT", 0, -2)
    standing.name = name

    local bar = CreateFrame("Frame", nil, standing)
    bar:SetSize(250, 12)
    bar:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -4)
    bar:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 8, insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    bar:SetBackdropColor(0, 0, 0, 0.7)
    bar:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)
    standing.bar = bar

    local fill = bar:CreateTexture(nil, "ARTWORK")
    fill:SetTexture("Interface\\TargetingFrame\\UI-StatusBar")
    fill:SetVertexColor(0.95, 0.66, 0.2)
    fill:SetPoint("TOPLEFT", 2, -2)
    fill:SetPoint("BOTTOMLEFT", 2, 2)
    fill:SetWidth(1)
    standing.fill = fill

    local value = bar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    value:SetPoint("CENTER")
    standing.value = value

    local perks = standing:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    perks:SetPoint("TOPLEFT", bar, "BOTTOMLEFT", 0, -3)
    perks:SetTextColor(0.85, 0.78, 0.62)
    standing.perks = perks
end

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
    intro:SetWidth(360)
    intro:SetJustifyH("LEFT")
    intro:SetTextColor(0.85, 0.8, 0.7)
    frame.intro = intro

    CreateStanding(frame)

    -- The list of gear, down the left
    local listLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    listLabel:SetPoint("TOPLEFT", frame, "TOPLEFT", 26, -112)
    listLabel:SetText(TEXT.list)

    local listBox = CreateFrame("Frame", nil, frame)
    listBox:SetPoint("TOPLEFT", frame, "TOPLEFT", 20, -130)
    listBox:SetSize(LIST_WIDTH, HEIGHT - 154)
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

    -- The forge's embers: a soft glow at the foot of the anvil, and the fire that rises while the smith works
    local embers = anvil:CreateTexture(nil, "BORDER")
    embers:SetTexture("Interface\\AchievementFrame\\UI-Achievement-Alert-Glow")
    embers:SetTexCoord(0, 0.782, 0.39, 0.782)
    embers:SetBlendMode("ADD")
    embers:SetVertexColor(0.8, 0.32, 0.06)
    embers:SetAlpha(0.45)
    embers:SetPoint("BOTTOMLEFT", 4, 4)
    embers:SetPoint("BOTTOMRIGHT", -4, 4)
    embers:SetHeight(110)

    local fire = anvil:CreateTexture(nil, "BORDER", nil, 1)
    fire:SetTexture("Interface\\AchievementFrame\\UI-Achievement-Alert-Glow")
    fire:SetTexCoord(0, 0.782, 0.39, 0.782)
    fire:SetBlendMode("ADD")
    fire:SetVertexColor(1, 0.45, 0.1)
    fire:SetPoint("BOTTOMLEFT", 4, 4)
    fire:SetPoint("BOTTOMRIGHT", -4, 4)
    fire:SetHeight(260)
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
    name:SetFont(FONT, 17)
    name:SetShadowOffset(1, -1)
    name:SetPoint("TOP", content, "TOP", 0, -16)
    name:SetWidth(380)
    anvil.name = name

    local iconHolder = CreateFrame("Frame", nil, content)
    iconHolder:SetSize(80, 80)
    iconHolder:SetPoint("CENTER", content, "TOP", 0, -82)
    anvil.iconHolder = iconHolder

    -- A masterpiece's golden radiance, behind it for good
    local masterGlow = iconHolder:CreateTexture(nil, "BACKGROUND")
    masterGlow:SetTexture("Interface\\Cooldown\\star4")
    masterGlow:SetBlendMode("ADD")
    masterGlow:SetVertexColor(1, 0.85, 0.4)
    masterGlow:SetPoint("CENTER")
    masterGlow:SetSize(130, 130)
    masterGlow:SetAlpha(0)
    anvil.masterGlow = masterGlow

    local glow = iconHolder:CreateTexture(nil, "BACKGROUND", nil, 1)
    glow:SetTexture("Interface\\Cooldown\\star4")
    glow:SetBlendMode("ADD")
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
    levelLabel:SetPoint("TOP", iconHolder, "BOTTOM", 0, -4)
    levelLabel:SetText(TEXT.itemLevel)

    local levels = content:CreateFontString(nil, "OVERLAY")
    levels:SetFont(FONT, 24)
    levels:SetShadowOffset(1, -1)
    levels:SetPoint("TOP", levelLabel, "BOTTOM", 0, -2)
    anvil.levels = levels

    local pipHolder = CreateFrame("Frame", nil, content)
    pipHolder:SetSize(100, 12)
    anvil.pips = CreatePips(pipHolder, 12, 4)
    pipHolder:SetWidth(anvil.pips.width)
    pipHolder:SetPoint("TOP", levels, "BOTTOM", 0, -8)

    local rank = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    rank:SetPoint("TOP", pipHolder, "BOTTOM", 0, -5)
    rank:SetTextColor(0.85, 0.78, 0.62)
    anvil.rank = rank

    -- What the next rank adds, stat by stat
    local gainsLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    gainsLabel:SetPoint("TOP", rank, "BOTTOM", 0, -10)
    gainsLabel:SetText(TEXT.gains)
    anvil.gainsLabel = gainsLabel
    anvil.gains = {}
    for index = 1, 6 do
        local line = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        local column, row = (index - 1) % 2, floor((index - 1) / 2)
        line:SetPoint("TOPLEFT", gainsLabel, "BOTTOM", column == 0 and -170 or 10, -5 - row * 14)
        line:SetWidth(150)
        line:SetJustifyH(column == 0 and "RIGHT" or "LEFT")
        line:SetTextColor(0.9, 0.84, 0.7)
        anvil.gains[index] = line
    end

    local costLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    costLabel:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 22, 66)
    costLabel:SetText(TEXT.price)
    anvil.costLabel = costLabel

    local cost = content:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    cost:SetPoint("LEFT", costLabel, "RIGHT", 8, 0)
    anvil.cost = cost

    local purseLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    purseLabel:SetPoint("TOPLEFT", costLabel, "BOTTOMLEFT", 0, -7)
    purseLabel:SetText(TEXT.purse)
    purseLabel:SetTextColor(0.8, 0.74, 0.62)

    local money = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    money:SetPoint("LEFT", purseLabel, "RIGHT", 8, 0)
    anvil.money = money

    local invested = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    invested:SetPoint("TOPLEFT", purseLabel, "BOTTOMLEFT", 0, -5)
    invested:SetTextColor(0.8, 0.74, 0.62)
    anvil.invested = invested

    local button = CreateFrame("Button", nil, content, "UIPanelButtonTemplate")
    button:SetSize(170, 30)
    button:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -20, 18)
    button:SetText(TEXT.forge)
    button:SetScript("OnClick", function()
        local item = state.selected and state.items[state.selected]
        if not item or state.working or item.nextEntry == 0 then
            return
        end
        PlaySound(SOUND_COINS)
        SetEnabled(button, false)
        anvil.status:SetText(TEXT.working)
        Send(format("U\t%d\t%d\t%d", item.bag, item.slot, item.entry))
    end)
    anvil.button = button

    local status = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    -- What the smith is doing, or has just done: over the button, clear of the price
    status:SetPoint("BOTTOMRIGHT", button, "TOPRIGHT", 0, 8)
    status:SetWidth(230)
    status:SetJustifyH("RIGHT")
    status:SetTextColor(1, 0.72, 0.35)
    anvil.status = status

    -- The next rank's tooltip, shown beside the window
    anvil.preview = CreateFrame("GameTooltip", "ItemForgePreviewTooltip", frame, "GameTooltipTemplate")

    frame:SetScript("OnShow", function()
        PlaySound(SOUND_OPEN)
        frame:SetAlpha(0)
        frame:SetScale(0.94)
        Tween(0.22, 0, function(p)
            frame:SetAlpha(p)
            frame:SetScale(0.94 + 0.06 * OutCubic(p))
        end)
    end)
    frame:SetScript("OnHide", function()
        PlaySound(SOUND_CLOSE)
        GameTooltip:Hide()
        anvil.preview:Hide()
    end)
    -- A forged entry the client did not know yet has its name and stats a moment later
    local wait = 0
    frame:SetScript("OnUpdate", function(_, elapsed)
        wait = wait + elapsed
        if wait < 1 or state.working then
            return
        end
        wait = 0
        if anvil.gainsPending then
            RefreshAnvil(false)
        end
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

-- Forged gear outside the window: its icon smoulders --------------------------------------------------------------
-- A glow around the icon of every forged piece, in the bags and on the character sheet, growing with its rank: a
-- faint ember, then a warm glow, then a flame that breathes, then a masterpiece's golden radiance.

local glowing = {}

local function IconGlow(button, rank)
    local glow = button.forgeGlow
    if rank <= 0 then
        if glow then
            glow:Hide()
            glowing[glow] = nil
        end
        return
    end
    if not glow then
        glow = button:CreateTexture(nil, "OVERLAY")
        glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        glow:SetBlendMode("ADD")
        glow:SetPoint("CENTER")
        button.forgeGlow = glow
    end
    local size = button:GetWidth() * 1.85
    glow:SetSize(size, size)
    if rank >= MAX_RANK then
        glow:SetVertexColor(1, 0.86, 0.4)
        glow.base, glow.pulse = 0.85, 0.15
    elseif rank >= 5 then
        glow:SetVertexColor(1, 0.5, 0.12)
        glow.base, glow.pulse = 0.7, 0.2
    elseif rank >= 3 then
        glow:SetVertexColor(1, 0.45, 0.1)
        glow.base, glow.pulse = 0.5, 0
    else
        glow:SetVertexColor(0.9, 0.35, 0.08)
        glow.base, glow.pulse = 0.28, 0
    end
    glow:SetAlpha(glow.base)
    glow:Show()
    glowing[glow] = glow.pulse > 0 or nil
end

-- The flames breathe
local breath = 0
driver:HookScript("OnUpdate", function(_, elapsed)
    breath = breath + elapsed
    local wave = 0.5 + 0.5 * math.sin(breath * 3)
    for glow in pairs(glowing) do
        if glow:IsVisible() then
            glow:SetAlpha(glow.base - glow.pulse + 2 * glow.pulse * wave)
        end
    end
end)

hooksecurefunc("ContainerFrame_Update", function(container)
    local bag = container:GetID()
    local name = container:GetName()
    for index = 1, container.size or 0 do
        local button = _G[name .. "Item" .. index]
        if button then
            local slot = button:GetID()
            IconGlow(button, RankOf(GetContainerItemID(bag, slot), KeyOfContainer(bag, slot)))
        end
    end
end)

hooksecurefunc("PaperDollItemSlotButton_Update", function(button)
    local slot = button:GetID()
    IconGlow(button, RankOf(GetInventoryItemID("player", slot), KeyOfInventory(slot)))
end)

local function RefreshIcons()
    for index = 1, NUM_CONTAINER_FRAMES do
        local container = _G["ContainerFrame" .. index]
        if container and container:IsShown() then
            ContainerFrame_Update(container)
        end
    end
    if PaperDollFrame and PaperDollFrame:IsShown() then
        for _, slotName in ipairs({ "HeadSlot", "NeckSlot", "ShoulderSlot", "BackSlot", "ChestSlot", "WristSlot",
                "HandsSlot", "WaistSlot", "LegsSlot", "FeetSlot", "Finger0Slot", "Finger1Slot", "Trinket0Slot",
                "Trinket1Slot", "MainHandSlot", "SecondaryHandSlot", "RangedSlot" }) do
            local button = _G["Character" .. slotName]
            if button then
                PaperDollItemSlotButton_Update(button)
            end
        end
    end
end

-- The tooltip of a forged piece the player carries: what it cost, and a Mythic+ piece's rank (a real item's is its
-- entry, already tagged by MythicItemTag.lua)
local function TagTooltip(tooltip, key)
    local item = state.items[key]
    if not item or item.invested <= 0 then
        return
    end
    if item.entry >= GENERATED_ITEM_BASE and floor(item.entry / GENERATED_ITEM_BASE) <= MYTHIC_VARIANTS then
        tooltip:AddLine(format(TEXT.tooltipRank, item.rank, state.maxRank), 1, 0.62, 0.25)
    end
    tooltip:AddLine(format(TEXT.tooltipInvested, GetCoinTextureString(item.invested)), 0.85, 0.78, 0.62)
    tooltip:Show()
end

hooksecurefunc(GameTooltip, "SetBagItem", function(tooltip, container, slot)
    TagTooltip(tooltip, KeyOfContainer(container, slot))
end)
hooksecurefunc(GameTooltip, "SetInventoryItem", function(tooltip, unit, slot)
    if unit == "player" then
        TagTooltip(tooltip, KeyOfInventory(slot))
    end
end)

-- The Forge on the world map ---------------------------------------------------------------------------------------
-- An anvil where the master smith stands, on each capital's map, on the zones around them and on the continents. The
-- spots are his spawns (mod-forge forge_master.sql) placed on each map with WorldMapArea.dbc's bounds, by the map's
-- area id (GetCurrentMapAreaID).

local MAP_SPOTS = {
    [301] = { { 0.6370, 0.3652 } },                         -- Stormwind City
    [321] = { { 0.8142, 0.2167 } },                         -- Orgrimmar
    [341] = { { 0.4959, 0.4384 } },                         -- Ironforge
    [30] = { { 0.2649, 0.2071 } },                          -- Elwynn Forest
    [27] = { { 0.5904, 0.2813 } },                          -- Dun Morogh
    [14] = { { 0.4309, 0.7217 }, { 0.4732, 0.5885 } },      -- Eastern Kingdoms
    [13] = { { 0.5948, 0.4373 } },                          -- Kalimdor
}
local CITY_MAPS = { [301] = true, [321] = true, [341] = true }

local mapPins = {}

local function CreateMapPin(index)
    local pin = CreateFrame("Button", nil, WorldMapButton)
    pin:SetFrameLevel(WorldMapButton:GetFrameLevel() + 6)
    local icon = pin:CreateTexture(nil, "OVERLAY")
    icon:SetAllPoints()
    icon:SetTexture("Interface\\Minimap\\Tracking\\Repair")
    pin:SetScript("OnEnter", function(self)
        WorldMapTooltip:SetOwner(self, "ANCHOR_RIGHT")
        WorldMapTooltip:AddLine(TEXT.mapTitle, 1, 0.82, 0)
        WorldMapTooltip:AddLine(TEXT.mapSmith, 1, 1, 1)
        WorldMapTooltip:AddLine(TEXT.mapHint, 0.85, 0.78, 0.62)
        WorldMapTooltip:Show()
    end)
    pin:SetScript("OnLeave", function() WorldMapTooltip:Hide() end)
    mapPins[index] = pin
    return pin
end

local function UpdateMapPins()
    local area = GetCurrentMapAreaID()
    local spots = MAP_SPOTS[area] or {}
    local width, height = WorldMapButton:GetWidth(), WorldMapButton:GetHeight()
    local size = CITY_MAPS[area] and 22 or 16
    for index, spot in ipairs(spots) do
        local pin = mapPins[index] or CreateMapPin(index)
        pin:SetSize(size, size)
        pin:ClearAllPoints()
        pin:SetPoint("CENTER", WorldMapButton, "TOPLEFT", spot[1] * width, -spot[2] * height)
        pin:Show()
    end
    for index = #spots + 1, #mapPins do
        mapPins[index]:Hide()
    end
end

local mapWatcher = CreateFrame("Frame")
mapWatcher:RegisterEvent("WORLD_MAP_UPDATE")
mapWatcher:SetScript("OnEvent", UpdateMapPins)
WorldMapFrame:HookScript("OnShow", UpdateMapPins)
-- The map changes size between its full and windowed views
for _, name in ipairs({ "WorldMapFrame_SetFullMapView", "WorldMapFrame_SetQuestMapView", "WorldMap_ToggleSizeUp",
        "WorldMap_ToggleSizeDown" }) do
    if _G[name] then
        hooksecurefunc(name, UpdateMapPins)
    end
end

-- Messages ---------------------------------------------------------------------------------------------------------

local function ShowError(code)
    PlaySound("igQuestFailed")
    UIErrorsFrame:AddMessage(TEXT.errors[tonumber(code)] or TEXT.errors[1], 1, 0.3, 0.2, 1)
    if frame and frame:IsShown() then
        RefreshAnvil(false)
    end
end

-- The smith is done: the piece keeps its place, its entry and rank move on. The bigger the moment, the bigger the
-- show - a masterwork, every second rank, the masterpiece, a new standing with the smith.
local function OnForged(result)
    if not frame or not frame:IsShown() then
        return
    end
    local key = Key(result.bag, result.slot)
    state.selected = key
    AnimateForge(state.items[key], result, function()
        local name, _, icon = ItemInfo(result.entry)
        UIErrorsFrame:AddMessage(format(TEXT.done, name or "", result.itemLevel), 1, 0.72, 0.35, 1)
        Tween(0.35, 0, function(p)
            anvil.levels:SetFont(FONT, 24 + 12 * (1 - OutCubic(p)))
        end)

        if result.rank >= MAX_RANK then
            PlaySound(SOUND_MASTERPIECE)
            Flash(220, 1, 0.9, 0.5)
            ShowBanner(icon, TEXT.masterpiece, name or "", format(TEXT.masterpieceLine, result.rank, MAX_RANK,
                result.itemLevel), QualityColor(select(2, ItemInfo(result.entry))))
        elseif result.masterwork then
            PlaySound(SOUND_MASTERWORK)
            Flash(180, 1, 0.9, 0.55)
            anvil.status:SetText(TEXT.masterwork)
        elseif result.rank % 2 == 0 then
            PlaySound(SOUND_MILESTONE)
            Flash(140, 1, 0.7, 0.3)
            anvil.status:SetText(format(TEXT.milestone, result.rank))
        else
            Flash(70, 1, 0.6, 0.2)
        end

        if result.newStanding > 0 then
            Tween(0.01, result.rank >= MAX_RANK and 5.2 or 0.6, nil, function()
                PlaySound(SOUND_MASTERWORK)
                ShowBanner("Interface\\Icons\\Trade_BlackSmithing",
                    format(TEXT.standingUp, TEXT.standingNames[result.newStanding + 1]), StandingPerks(
                    result.newStanding + 1), "", 1, 0.82, 0.35)
            end)
        end
    end)
end

local function Handle(message)
    local kind, a, b, c, d, e, f, g, h, i = strsplit("\t", message)
    if kind == "O" then
        incoming = {}
    elseif kind == "I" and incoming then
        local item = {
            bag = tonumber(a) or 0, slot = tonumber(b) or 0, entry = tonumber(c) or 0, rank = tonumber(d) or 0,
            itemLevel = tonumber(e) or 0, nextEntry = tonumber(f) or 0, nextItemLevel = tonumber(g) or 0,
            cost = tonumber(h) or 0, invested = tonumber(i) or 0,
        }
        incoming[Key(item.bag, item.slot)] = item
    elseif kind == "E" then
        state.items = incoming or {}
        incoming = nil
        state.maxRank = tonumber(b) or state.maxRank
        state.spent = tonumber(c) or 0
        state.standing = tonumber(d) or 0
        RefreshIcons()
        if a == "1" and not frame then
            CreateForge()
        end
        if not frame then
            return
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
        OnForged({
            bag = tonumber(a) or 0, slot = tonumber(b) or 0, entry = tonumber(c) or 0, rank = tonumber(d) or 0,
            itemLevel = tonumber(e) or 0, masterwork = f == "1", newStanding = tonumber(g) or 0,
        })
    elseif kind == "X" then
        ShowError(a)
    end
end

-- The list is asked for at login and whenever the gear moves, so the icons and tooltips know every forged piece's
-- rank and cost; at most once every two seconds
local pendingAsk
local asker = CreateFrame("Frame")
asker:Hide()
asker:SetScript("OnUpdate", function(self, elapsed)
    pendingAsk = pendingAsk - elapsed
    if pendingAsk <= 0 then
        self:Hide()
        Send("L")
    end
end)

local function AskSoon(delay)
    if not asker:IsShown() then
        pendingAsk = delay or 2
        asker:Show()
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("BAG_UPDATE")
listener:RegisterEvent("PLAYER_EQUIPMENT_CHANGED")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and sender == UnitName("player") then
            Handle(message)
        end
    elseif not state.working then
        AskSoon(event == "PLAYER_ENTERING_WORLD" and 3 or 2)
    end
end)
