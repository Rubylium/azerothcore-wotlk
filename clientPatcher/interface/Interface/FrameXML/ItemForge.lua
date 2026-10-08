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

local WIDTH, HEIGHT = 780, 640
local LIST_WIDTH, ROW_WIDTH, ROW_HEIGHT = 300, 268, 46

-- The painted art (clientPatcher/assets/itemForge, BLPs by localTools/interface/buildItemForgeTextures.py). Every
-- piece is painted at twice the size it is drawn and drawn at exactly its own proportions: never stretched.
local ART = "Interface\\ItemForge\\"
local ATLAS = ART .. "ForgeAtlas"       -- 1024 x 512: left, top, width, height of each piece, in its pixels
local PIECES = {
    medallion = { 4, 4, 120, 120 }, rankCold = { 132, 4, 48, 48 }, rankLit = { 184, 4, 48, 48 },
    rankGold = { 236, 4, 48, 48 }, rankFlare = { 288, 4, 96, 96 }, pieceHeat = { 388, 4, 116, 116 },
    rowPlate = { 4, 132, 536, 84 }, rowSelected = { 4, 220, 536, 84 }, gaugeFill = { 4, 308, 464, 20 },
    goldenBurst = { 580, 4, 440, 440 },
}
-- The anvil stage (868 x 480 on 1024 x 512). On it, from its top left: the piece lying on the anvil's top face, the
-- end of the hammer's handle (the hammer turns around it), and the coals the embers leave from.
local STAGE_WIDTH, STAGE_HEIGHT = 434, 240
local PIECE_X, PIECE_Y, PIECE_SIZE = 217, 115, 58
local HAMMER_SIZE = 270                 -- the hammer's square (512 on its texture), its handle's end at the centre
local HAMMER_X, HAMMER_Y = 286, 34
-- The hammer's angles, anticlockwise in degrees from the way it is painted: at rest, raised, on the piece, back up
local HAMMER_REST, HAMMER_RAISED, HAMMER_HIT, HAMMER_REBOUND = 15, -10, 70, 45
local COALS = { 140, 116, 155, 12 }     -- left, top, width, height
local EMBERS = 10
local SPARK_SIZE, STEAM_SIZE = 160, 128 -- a cell of each sheet (256 on its texture)
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

-- A piece of the atlas on a texture, and the size it is drawn at (half its painting, or that times a scale)
local function SetPiece(texture, name)
    local piece = PIECES[name]
    texture:SetTexture(ATLAS)
    texture:SetTexCoord(piece[1] / 1024, (piece[1] + piece[3]) / 1024, piece[2] / 512, (piece[2] + piece[4]) / 512)
end

local function PieceSize(name, scale)
    local piece = PIECES[name]
    return piece[3] / 2 * (scale or 1), piece[4] / 2 * (scale or 1)
end

-- One cell of a flipbook sheet of square cells, counted from 0 left to right then down
local function SetCell(texture, columns, lines, index)
    local column, line = index % columns, floor(index / columns)
    texture:SetTexCoord(column / columns, (column + 1) / columns, line / lines, (line + 1) / lines)
end

-- A square texture turned around its centre by its coordinates, anticlockwise in degrees: the painting stays whole
-- as long as it keeps within the square's inscribed circle (the hammer's does)
local function Turn(texture, degrees)
    local angle = math.rad(degrees)
    local c, s = math.cos(angle), math.sin(angle)
    local function corner(x, y)
        return 0.5 + x * c - y * s, 0.5 + x * s + y * c
    end
    local ulx, uly = corner(-0.5, -0.5)
    local llx, lly = corner(-0.5, 0.5)
    local urx, ury = corner(0.5, -0.5)
    local lrx, lry = corner(0.5, 0.5)
    texture:SetTexCoord(ulx, uly, llx, lly, urx, ury, lrx, lry)
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
    -- The Forge's own blocks say the rank; a Mythic+ variant's (the ladder's and the three caps') only the server's
    -- list knows; a set piece (the blocks after the caps) is never forged
    if block > MYTHIC_VARIANTS + MAX_RANK + 3 then
        return 0
    elseif block > MYTHIC_VARIANTS and block <= MYTHIC_VARIANTS + MAX_RANK then
        return block - MYTHIC_VARIANTS
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

-- The molten metal in the standing's groove, cut to how far it has run: shown as far as it goes, never stretched
local GAUGE_WIDTH, GAUGE_HEIGHT = PieceSize("gaugeFill")

local function SetGauge(fraction)
    local width = max(1, floor(GAUGE_WIDTH * fraction + 0.5))
    local piece = PIECES.gaugeFill
    standing.fill:SetWidth(width)
    standing.fill:SetTexCoord(piece[1] / 1024, (piece[1] + width * 2) / 1024, piece[2] / 512,
        (piece[2] + piece[4]) / 512)
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
        SetGauge(min(1, (gold - from) / (next.gold - from)))
        standing.value:SetText(format(TEXT.standingNext, Thousands(gold), Thousands(next.gold)))
    else
        SetGauge(1)
        standing.value:SetText(format(TEXT.standingTop, Thousands(gold)))
    end
end

-- Rank embers: one coal per rank, burning once forged, gold for a masterpiece ---------------------------------------

-- The rank track: eight coals of a size, side by side
local function CreateTrack(parent, size, gap)
    local track = {}
    for index = 1, MAX_RANK do
        local cell = parent:CreateTexture(nil, "ARTWORK")
        SetPiece(cell, "rankCold")
        cell:SetSize(size, size)
        cell:SetPoint("LEFT", parent, "LEFT", (index - 1) * (size + gap), 0)
        track[index] = cell
    end
    return track
end

local function TrackWidth(size, gap)
    return MAX_RANK * size + (MAX_RANK - 1) * gap
end

-- How many coals an item's track shows: every rank, or only the ones it has once it can go no further (a Mythic+
-- item stopped by its item level cap short of the last rank) - never ranks it cannot have
local function TrackLength(item)
    return item.nextEntry == 0 and item.rank or state.maxRank
end

local function SetTrack(track, rank, maxRank)
    for index = 1, MAX_RANK do
        local cell = track[index]
        SetShown(cell, index <= maxRank)
        SetPiece(cell, index > rank and "rankCold" or rank >= MAX_RANK and "rankGold" or "rankLit")
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
    -- A quiet dark row; the one on the anvil sits on the painted iron plate (268 x 42), heated along its edges
    local row = CreateFrame("Button", nil, listChild)
    row:SetSize(PieceSize("rowPlate"))
    row:SetPoint("TOPLEFT", listChild, "TOPLEFT", 0, -(index - 1) * ROW_HEIGHT)

    local ground = row:CreateTexture(nil, "BACKGROUND")
    ground:SetPoint("TOPLEFT", 4, -3)
    ground:SetPoint("BOTTOMRIGHT", -4, 3)
    ground:SetTexture(0.06, 0.045, 0.03, 0.7)

    local edge = row:CreateTexture(nil, "BACKGROUND")
    edge:SetPoint("BOTTOMLEFT", ground, "BOTTOMLEFT")
    edge:SetPoint("BOTTOMRIGHT", ground, "BOTTOMRIGHT")
    edge:SetHeight(1)
    edge:SetTexture(0.75, 0.6, 0.35, 0.35)

    local selectedGlow = row:CreateTexture(nil, "BORDER")
    selectedGlow:SetAllPoints()
    SetPiece(selectedGlow, "rowSelected")
    selectedGlow:Hide()
    row.selectedGlow = selectedGlow

    local highlight = row:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetAllPoints(ground)
    highlight:SetTexture(1, 0.75, 0.35, 0.08)

    -- Between the plate's riveted ends
    local icon = row:CreateTexture(nil, "ARTWORK")
    icon:SetSize(26, 26)
    icon:SetPoint("LEFT", 26, 0)
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    row.icon = icon

    local name = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    name:SetPoint("TOPLEFT", icon, "TOPRIGHT", 7, 0)
    name:SetPoint("RIGHT", row, "RIGHT", -24, 0)
    name:SetJustifyH("LEFT")
    row.name = name

    local detail = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    detail:SetPoint("BOTTOMLEFT", icon, "BOTTOMRIGHT", 7, 0)
    detail:SetTextColor(0.8, 0.74, 0.62)
    row.detail = detail

    local rank = row:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    rank:SetPoint("BOTTOMRIGHT", row, "BOTTOMRIGHT", -26, 9)
    row.rank = rank

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
        local length = TrackLength(item)
        row.rank:SetText(format("%d/%d", item.rank, length))
        if item.rank >= MAX_RANK then
            row.rank:SetTextColor(1, 0.86, 0.45)
        elseif item.rank > 0 then
            row.rank:SetTextColor(1, 0.6, 0.25)
        else
            row.rank:SetTextColor(0.55, 0.5, 0.42)
        end
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
    SetTrack(anvil.track, item.rank, TrackLength(item))
    anvil.rank:SetText(format(TEXT.rank, item.rank, TrackLength(item)))
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
            local size = PIECE_SIZE + 14 * (1 - OutBack(p))
            anvil.icon:SetSize(size, size)
        end)
    end
end

-- The smith at work -------------------------------------------------------------------------------------------------
-- He takes up the hammer, and each blow lands on the piece: sparks burst off it, it jolts on the anvil and glows a
-- little hotter, the fire flares. Then the quench: steam, and the piece cools back to its own colour.

-- A flipbook sheet played once on a texture: its cells one after another, then gone
local function PlaySheet(texture, columns, lines, frames, duration)
    texture:SetAlpha(1)
    SetCell(texture, columns, lines, 0)
    Tween(duration, 0, function(p)
        SetCell(texture, columns, lines, min(frames - 1, floor(p * frames)))
    end, function()
        texture:SetAlpha(0)
    end)
end

-- How hot the piece glows, from 0 (its own colour) to 1 (white-hot)
local function SetHeat(heat)
    anvil.heat:SetAlpha(0.6 * heat)
    anvil.heat:SetVertexColor(1, 0.55 + 0.45 * heat, 0.25 + 0.6 * heat * heat)
end

-- One blow landing: sparks off the piece (golden on a masterwork), the piece jolts and heats, the fire flares
local function Strike(golden)
    PlaySound(SOUND_HAMMER)
    if golden then
        anvil.sparks:SetVertexColor(1, 0.9, 0.55)
    else
        anvil.sparks:SetVertexColor(1, 1, 1)
    end
    PlaySheet(anvil.sparks, 4, 4, 16, 0.5)
    anvil.heatLevel = min(1, anvil.heatLevel + (golden and 0.4 or 0.3))
    SetHeat(anvil.heatLevel)
    anvil.fireBoost = 0.35
    local holder = anvil.iconHolder
    Tween(0.12, 0, function(p)
        holder:ClearAllPoints()
        holder:SetPoint("CENTER", anvil.stage, "TOPLEFT", PIECE_X, -PIECE_Y - 4 * (1 - p))
    end)
end

-- One swing landing at `delay`: the hammer raised from where it is, brought down onto the piece, bouncing back up
local function Swing(delay, from, golden)
    local hammer = anvil.hammer
    Tween(0.15, delay - 0.27, function(p)
        Turn(hammer, from + (HAMMER_RAISED - from) * OutCubic(p))
    end)
    Tween(0.12, delay - 0.12, function(p)
        Turn(hammer, HAMMER_RAISED + (HAMMER_HIT - HAMMER_RAISED) * p * p)
    end, function()
        Strike(golden)
    end)
    Tween(0.15, delay, function(p)
        Turn(hammer, HAMMER_HIT + (HAMMER_REBOUND - HAMMER_HIT) * OutCubic(p))
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

-- Newly forged ranks flaring as their coals catch, one after the other
local function FlareRanks(from, to)
    for index = from + 1, min(to, MAX_RANK) do
        local cell = anvil.track[index]
        Tween(0.6, 0.15 * (index - from - 1), function(p)
            local flare = anvil.flare
            flare:ClearAllPoints()
            flare:SetPoint("CENTER", cell, "CENTER")
            local size = 24 + 40 * OutCubic(p)
            flare:SetSize(size, size)
            flare:SetAlpha(1 - p)
        end)
    end
end

-- A piece of the banner: the masterpiece, or a new standing with the smith
local function ShowBanner(icon, title, line, detail, red, green, blue)
    if not banner then
        -- The painted banner plate (960 x 220 on its 1024 x 256 texture), drawn at 480 x 110; its round socket's
        -- centre and its dark face are where the painting has them
        banner = CreateFrame("Frame", "ItemForgeBanner", UIParent)
        banner:SetSize(480, 110)
        banner:SetPoint("TOP", UIParent, "TOP", 0, -150)
        banner:SetFrameStrata("DIALOG")
        banner:Hide()

        local burst = banner:CreateTexture(nil, "BACKGROUND")
        SetPiece(burst, "goldenBurst")
        burst:SetBlendMode("ADD")
        burst:SetSize(PieceSize("goldenBurst"))
        burst:SetPoint("CENTER", banner, "TOPLEFT", 47.5, -54)
        banner.star = burst

        local plate = banner:CreateTexture(nil, "BORDER")
        plate:SetTexture(ART .. "ForgeBanner")
        plate:SetTexCoord(0, 960 / 1024, 0, 220 / 256)
        plate:SetAllPoints()

        local bannerIcon = banner:CreateTexture(nil, "ARTWORK")
        bannerIcon:SetSize(38, 38)
        bannerIcon:SetPoint("CENTER", banner, "TOPLEFT", 47.5, -54)
        bannerIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
        banner.icon = bannerIcon

        local bannerTitle = banner:CreateFontString(nil, "OVERLAY")
        bannerTitle:SetFont(MORPHEUS, 22)
        bannerTitle:SetShadowOffset(1, -1)
        bannerTitle:SetPoint("TOPLEFT", banner, "TOPLEFT", 112, -31)
        bannerTitle:SetPoint("RIGHT", banner, "LEFT", 448, 0)
        bannerTitle:SetJustifyH("LEFT")
        banner.title = bannerTitle

        local bannerLine = banner:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        bannerLine:SetPoint("TOPLEFT", bannerTitle, "BOTTOMLEFT", 0, -2)
        bannerLine:SetPoint("RIGHT", banner, "LEFT", 448, 0)
        bannerLine:SetJustifyH("LEFT")
        banner.line = bannerLine

        local bannerDetail = banner:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        bannerDetail:SetPoint("TOPLEFT", bannerLine, "BOTTOMLEFT", 0, -2)
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
    anvil.heatLevel = 0
    Turn(anvil.hammer, HAMMER_REST)
    Tween(0.2, 0, function(p) anvil.hammer:SetAlpha(p) end)
    local strikes = HAMMER_STRIKES + (result.masterwork and 1 or 0)
    for strike = 1, strikes do
        Swing(HAMMER_DELAY + (strike - 1) * HAMMER_GAP, strike == 1 and HAMMER_REST or HAMMER_REBOUND,
            strike > HAMMER_STRIKES)
    end
    local quench = HAMMER_DELAY + strikes * HAMMER_GAP + 0.15
    -- The hammer is put down; the piece hisses in the quench and cools back to its own colour
    Tween(0.3, quench - 0.1, function(p) anvil.hammer:SetAlpha(1 - p) end)
    Tween(0.01, quench, nil, function()
        PlaySound(SOUND_QUENCH)
        PlaySheet(anvil.steam, 4, 2, 8, 0.9)
        -- The list that came with the news is shown now, out of the quench
        RefreshList()
        onDone()
    end)
    Tween(1.4, quench, function(p) SetHeat(anvil.heatLevel * (1 - p)) end, function()
        state.working = false
        RefreshAnvil(false)
    end)
end

-- Embers rising off the coals while the forge burns, each from a random spot, swaying, fading
local function CreateEmbers(parent)
    local embers = {}
    for index = 1, EMBERS do
        local ember = parent:CreateTexture(nil, "OVERLAY")
        ember:SetTexture(ART .. "ForgeEmbers")
        ember:SetBlendMode("ADD")
        SetCell(ember, 4, 4, (index - 1) % 16)
        ember:SetAlpha(0)
        ember.life = -random() * 3
        ember.span = 0
        embers[index] = ember
    end
    return embers
end

local function UpdateEmbers(embers, elapsed, heat)
    for _, ember in ipairs(embers) do
        ember.life = ember.life + elapsed
        if ember.life >= ember.span then
            ember.life = 0
            ember.span = 2.4 + random() * 1.8
            ember.x = COALS[1] + random() * COALS[3]
            ember.y = COALS[2] + random() * COALS[4]
            ember.rise = 50 + random() * 50
            ember.sway = (random() - 0.5) * 30
            local size = 8 + random() * 7
            ember:SetSize(size, size)
        end
        if ember.life < 0 then
            ember:SetAlpha(0)
        else
            local p = ember.life / ember.span
            ember:ClearAllPoints()
            ember:SetPoint("CENTER", ember:GetParent(), "TOPLEFT", ember.x + ember.sway * math.sin(p * math.pi),
                -(ember.y - ember.rise * p))
            ember:SetAlpha(heat * (p < 0.15 and p / 0.15 or (1 - p) / 0.85))
        end
    end
end

-- The window -------------------------------------------------------------------------------------------------------

local function CreateStanding(parent)
    standing = CreateFrame("Frame", nil, parent)
    standing:SetSize(GAUGE_WIDTH + 4, 64)
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
    bar:SetSize(GAUGE_WIDTH + 4, GAUGE_HEIGHT + 4)
    bar:SetPoint("TOPLEFT", name, "BOTTOMLEFT", 0, -4)
    bar:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 8, insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    bar:SetBackdropColor(0, 0, 0, 0.7)
    bar:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)
    standing.bar = bar

    local fill = bar:CreateTexture(nil, "ARTWORK")
    SetPiece(fill, "gaugeFill")
    fill:SetPoint("TOPLEFT", 2, -2)
    fill:SetSize(1, GAUGE_HEIGHT)
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

    -- The smithy's sign: the painted medallion, hammer over anvil, and the heading beside it
    local emblem = frame:CreateTexture(nil, "OVERLAY")
    SetPiece(emblem, "medallion")
    emblem:SetSize(PieceSize("medallion"))
    emblem:SetPoint("TOPLEFT", frame, "TOPLEFT", 24, -30)

    local heading = frame:CreateFontString(nil, "OVERLAY")
    heading:SetFont(MORPHEUS, 28)
    heading:SetShadowOffset(1, -1)
    heading:SetTextColor(1, 0.86, 0.55)
    heading:SetPoint("TOPLEFT", emblem, "TOPRIGHT", 12, -6)
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
    listChild:SetSize(ROW_WIDTH, 1)
    scroll:SetScrollChild(listChild)
    rows = {}

    emptyText = listBox:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    emptyText:SetPoint("TOPLEFT", listBox, "TOPLEFT", 16, -30)
    emptyText:SetPoint("TOPRIGHT", listBox, "TOPRIGHT", -16, -30)
    emptyText:SetJustifyH("CENTER")
    emptyText:SetTextColor(0.85, 0.78, 0.62)
    emptyText:SetText(TEXT.empty)
    emptyText:Hide()

    -- The anvil, on the right: the painted smithy on top, the piece lying on its anvil; what the forge will do under it
    anvil = CreateFrame("Frame", nil, frame)
    anvil:SetPoint("TOPLEFT", listBox, "TOPRIGHT", 6, 0)
    anvil:SetPoint("BOTTOM", frame, "BOTTOM", 0, 24)
    anvil:SetWidth(STAGE_WIDTH + 6)
    anvil:SetBackdrop({
        bgFile = "Interface\\Buttons\\WHITE8X8", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = false, edgeSize = 12, insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    anvil:SetBackdropColor(0.05, 0.03, 0.02, 0.85)
    anvil:SetBackdropBorderColor(0.75, 0.6, 0.35, 1)

    -- The stage (868 x 480 painted, on 1024 x 512): cold, and lit over it as the forge wakes; the fire's light and
    -- the embers on top. Draw layers, not sublevels: 3.3.5 ignores those.
    local stage = CreateFrame("Frame", nil, anvil)
    stage:SetSize(STAGE_WIDTH, STAGE_HEIGHT)
    stage:SetPoint("TOPLEFT", anvil, "TOPLEFT", 3, -3)
    anvil.stage = stage

    local cold = stage:CreateTexture(nil, "BACKGROUND")
    cold:SetTexture(ART .. "ForgeStageCold")
    cold:SetTexCoord(0, 868 / 1024, 0, 480 / 512)
    cold:SetAllPoints()

    local lit = stage:CreateTexture(nil, "BORDER")
    lit:SetTexture(ART .. "ForgeStageLit")
    lit:SetTexCoord(0, 868 / 1024, 0, 480 / 512)
    lit:SetAllPoints()
    lit:SetAlpha(0)
    anvil.lit = lit

    local fire = stage:CreateTexture(nil, "ARTWORK")
    fire:SetTexture(ART .. "ForgeFireLight")
    fire:SetTexCoord(0, 868 / 1024, 0, 480 / 512)
    fire:SetBlendMode("ADD")
    fire:SetAllPoints()
    fire:SetAlpha(0)
    anvil.fire = fire

    anvil.embers = CreateEmbers(stage)
    anvil.clock, anvil.fireBoost, anvil.heatLevel = 0, 0, 0
    stage:SetScript("OnUpdate", function(_, elapsed)
        anvil.clock = anvil.clock + elapsed
        anvil.fireBoost = max(0, anvil.fireBoost - elapsed * 1.2)
        local heat = lit:GetAlpha()
        local flicker = 0.75 + 0.15 * math.sin(anvil.clock * 7.3) + 0.1 * math.sin(anvil.clock * 17.9)
        fire:SetAlpha(min(1, heat * flicker * (state.working and 1 or 0.7) + anvil.fireBoost))
        UpdateEmbers(anvil.embers, elapsed, heat)
    end)

    local pick = anvil:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    pick:SetPoint("CENTER", anvil, "TOP", 0, -(STAGE_HEIGHT + 120))
    pick:SetTextColor(0.85, 0.78, 0.62)
    pick:SetText(TEXT.pick)
    anvil.pick = pick

    local content = CreateFrame("Frame", nil, anvil)
    content:SetAllPoints()
    content:SetFrameLevel(stage:GetFrameLevel() + 2)
    anvil.content = content

    -- The piece's name in the stage's dark strip at its top
    local name = content:CreateFontString(nil, "OVERLAY")
    name:SetFont(FONT, 15)
    name:SetShadowOffset(1, -1)
    name:SetPoint("TOP", stage, "TOP", 0, -6)
    name:SetWidth(STAGE_WIDTH - 40)
    anvil.name = name

    -- The piece on the anvil's top face
    local iconHolder = CreateFrame("Frame", nil, content)
    iconHolder:SetSize(PIECE_SIZE + 22, PIECE_SIZE + 22)
    iconHolder:SetPoint("CENTER", stage, "TOPLEFT", PIECE_X, -PIECE_Y)
    anvil.iconHolder = iconHolder

    -- A masterpiece's golden radiance, behind it for good
    local masterGlow = iconHolder:CreateTexture(nil, "BACKGROUND")
    SetPiece(masterGlow, "goldenBurst")
    masterGlow:SetBlendMode("ADD")
    masterGlow:SetPoint("CENTER")
    masterGlow:SetSize(130, 130)
    masterGlow:SetAlpha(0)
    anvil.masterGlow = masterGlow

    local glow = iconHolder:CreateTexture(nil, "BACKGROUND")
    SetPiece(glow, "goldenBurst")
    glow:SetBlendMode("ADD")
    glow:SetPoint("CENTER")
    glow:SetSize(70, 70)
    glow:SetAlpha(0)
    anvil.glow = glow

    local setting = iconHolder:CreateTexture(nil, "BORDER")
    setting:SetTexture(0.78, 0.62, 0.32, 1)
    setting:SetSize(PIECE_SIZE + 4, PIECE_SIZE + 4)
    setting:SetPoint("CENTER")
    anvil.setting = setting

    local backing = iconHolder:CreateTexture(nil, "BORDER")
    backing:SetTexture(0, 0, 0, 1)
    backing:SetSize(PIECE_SIZE + 2, PIECE_SIZE + 2)
    backing:SetPoint("CENTER")
    anvil.backing = backing

    local icon = iconHolder:CreateTexture(nil, "ARTWORK")
    icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    icon:SetSize(PIECE_SIZE, PIECE_SIZE)
    icon:SetPoint("CENTER")
    anvil.icon = icon

    -- The heat of the work on the piece (116 x 116 painted, over the 58 x 58 icon)
    local heat = iconHolder:CreateTexture(nil, "OVERLAY")
    SetPiece(heat, "pieceHeat")
    heat:SetBlendMode("ADD")
    heat:SetSize(PieceSize("pieceHeat"))
    heat:SetPoint("CENTER", icon, "CENTER")
    heat:SetAlpha(0)
    anvil.heat = heat

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

    -- Over the piece: the hammer (its square turned around its handle's end), the sparks of each blow, the steam
    local work = CreateFrame("Frame", nil, content)
    work:SetAllPoints(stage)
    work:SetFrameLevel(iconHolder:GetFrameLevel() + 2)

    local hammer = work:CreateTexture(nil, "ARTWORK")
    hammer:SetTexture(ART .. "ForgeHammer")
    hammer:SetSize(HAMMER_SIZE, HAMMER_SIZE)
    hammer:SetPoint("CENTER", stage, "TOPLEFT", HAMMER_X, -HAMMER_Y)
    hammer:SetAlpha(0)
    Turn(hammer, HAMMER_REST)
    anvil.hammer = hammer

    local sparks = work:CreateTexture(nil, "OVERLAY")
    sparks:SetTexture(ART .. "ForgeSparks")
    sparks:SetBlendMode("ADD")
    sparks:SetSize(SPARK_SIZE, SPARK_SIZE)
    sparks:SetPoint("CENTER", stage, "TOPLEFT", PIECE_X, -(PIECE_Y - PIECE_SIZE / 2))
    sparks:SetAlpha(0)
    anvil.sparks = sparks

    -- The steam rises from the bottom of its cells: their bottom on the anvil's face
    local steam = work:CreateTexture(nil, "OVERLAY")
    steam:SetTexture(ART .. "ForgeSteam")
    steam:SetBlendMode("ADD")
    steam:SetSize(STEAM_SIZE, STEAM_SIZE)
    steam:SetPoint("BOTTOM", stage, "TOPLEFT", PIECE_X, -(PIECE_Y + PIECE_SIZE / 2 + 6))
    steam:SetAlpha(0)
    anvil.steam = steam

    -- Under the stage: the item level, the ranks, what the next one adds, the price and the button
    local levelLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    levelLabel:SetPoint("TOP", stage, "BOTTOM", 0, -10)
    levelLabel:SetText(TEXT.itemLevel)

    local levels = content:CreateFontString(nil, "OVERLAY")
    levels:SetFont(FONT, 24)
    levels:SetShadowOffset(1, -1)
    levels:SetPoint("TOP", levelLabel, "BOTTOM", 0, -2)
    anvil.levels = levels

    local trackHolder = CreateFrame("Frame", nil, content)
    trackHolder:SetSize(TrackWidth(24, 4), 24)
    trackHolder:SetPoint("TOP", levels, "BOTTOM", 0, -8)
    anvil.track = CreateTrack(trackHolder, 24, 4)

    -- A rank catching: its coal flares (96 x 96 painted)
    local flare = trackHolder:CreateTexture(nil, "OVERLAY")
    SetPiece(flare, "rankFlare")
    flare:SetBlendMode("ADD")
    flare:SetAlpha(0)
    anvil.flare = flare

    local rank = content:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    rank:SetPoint("TOP", trackHolder, "BOTTOM", 0, -4)
    rank:SetTextColor(0.85, 0.78, 0.62)
    anvil.rank = rank

    -- What the next rank adds, stat by stat
    local gainsLabel = content:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    gainsLabel:SetPoint("TOP", rank, "BOTTOM", 0, -8)
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
    costLabel:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 22, 58)
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
    button:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -20, 16)
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
        -- The smith stirs the fire: the cold smithy lights up
        anvil.lit:SetAlpha(0)
        Tween(1.2, 0.25, function(p) anvil.lit:SetAlpha(OutCubic(p)) end)
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

-- The Forge on the maps ---------------------------------------------------------------------------------------------
-- An anvil where the master smith stands: for now Stormwind's only, on Stormwind's own map and on the minimap while
-- in the city (no other map, no zone or continent), pointing the way from the minimap's edge when he is out of its
-- reach. Maps are known
-- by their file name (GetMapInfo), and placed with their WorldMapArea.dbc bounds: a map's left and right edges are
-- world Y, its top and bottom world X. Works with the default map and minimap, and with DragonUI's, which keep the
-- same frames (WorldMapButton, Minimap).

-- The smith's spawn shown (mod-forge forge_master.sql), and the maps it shows on
local SMITHS = {
    { continent = 0, x = -8418.887, y = 616.072, maps = { Stormwind = true } },
}

local MAP_BOUNDS = {
    Stormwind = { continent = 0, left = 1722.92, right = -14.58, top = -7995.83, bottom = -9154.17, city = true },
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

local function ShowSmithTooltip(tooltip, owner)
    tooltip:SetOwner(owner, "ANCHOR_RIGHT")
    tooltip:AddLine(TEXT.mapTitle, 1, 0.82, 0)
    tooltip:AddLine(TEXT.mapSmith, 1, 1, 1)
    tooltip:AddLine(TEXT.mapHint, 0.85, 0.78, 0.62)
    tooltip:Show()
end

local function CreateAnvilPin(parent, tooltip)
    local pin = CreateFrame("Button", nil, parent)
    local icon = pin:CreateTexture(nil, "OVERLAY")
    icon:SetAllPoints()
    icon:SetTexture("Interface\\Minimap\\Tracking\\Repair")
    pin.icon = icon
    pin:SetScript("OnEnter", function(self) ShowSmithTooltip(_G[tooltip], self) end)
    pin:SetScript("OnLeave", function() _G[tooltip]:Hide() end)
    return pin
end

-- The world map ----------------------------------------------------------------------------------------------------

local mapPins = {}

local function UpdateMapPins()
    local bounds = MAP_BOUNDS[GetMapInfo() or ""]
    local width, height = WorldMapButton:GetWidth(), WorldMapButton:GetHeight()
    -- The same size on screen however the map is scaled (DragonUI scales the canvas)
    local pinScale = UIParent:GetEffectiveScale() / WorldMapButton:GetEffectiveScale()
    local size = bounds and bounds.city and 22 or 16
    local shown = 0
    for _, smith in ipairs(SMITHS) do
        local px, py
        if bounds and bounds.continent == smith.continent then
            px, py = MapPosition(bounds, smith.x, smith.y)
        end
        if px then
            shown = shown + 1
            local pin = mapPins[shown]
            if not pin then
                pin = CreateAnvilPin(WorldMapButton, "WorldMapTooltip")
                mapPins[shown] = pin
            end
            pin:SetFrameLevel(WorldMapButton:GetFrameLevel() + 6)
            pin:SetScale(pinScale)
            pin:SetSize(size, size)
            pin:ClearAllPoints()
            pin:SetPoint("CENTER", WorldMapButton, "TOPLEFT", px * width / pinScale, -py * height / pinScale)
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

local minimapPin = CreateAnvilPin(Minimap, "GameTooltip")
minimapPin:SetSize(16, 16)
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

-- The maps the minimap pin shows on
local SMITH_MAPS = {}
for _, smith in ipairs(SMITHS) do
    for name in pairs(smith.maps) do
        SMITH_MAPS[name] = true
    end
end

-- Where the player is, in world yards; nil away from the smith's maps. The world map is put back on the player's
-- zone when it closes and when the zone changes; in between, reading the position disturbs nothing.
local playerMap, playerX, playerY
local function UpdatePlayerPosition()
    if WorldMapFrame:IsShown() then
        return
    end
    local name = GetMapInfo()
    local bounds = MAP_BOUNDS[name or ""]
    local px, py = GetPlayerMapPosition("player")
    if not bounds or not SMITH_MAPS[name] or (px == 0 and py == 0) then
        playerMap = nil
        return
    end
    playerMap = name
    playerX = bounds.top - py * (bounds.top - bounds.bottom)
    playerY = bounds.left - px * (bounds.left - bounds.right)
end

local function UpdateMinimapPin()
    UpdatePlayerPosition()
    local smith
    for _, candidate in ipairs(SMITHS) do
        if playerMap and candidate.maps[playerMap] then
            smith = candidate
        end
    end
    if not smith then
        minimapPin:Hide()
        return
    end

    -- East and south of the player, in yards, turned with the minimap when it turns
    local east = playerY - smith.y
    local south = playerX - smith.x
    if GetCVar("rotateMinimap") == "1" then
        local facing = GetPlayerFacing()
        local sine, cosine = math.sin(facing), math.cos(facing)
        east, south = east * cosine - south * sine, east * sine + south * cosine
    end

    local zoom = Minimap:GetZoom()
    local yards = (indoors and MINIMAP_YARDS.indoor or MINIMAP_YARDS.outdoor)[zoom] or 466.6667
    local perYard = Minimap:GetWidth() / yards
    local x, y = east * perYard, -south * perYard
    local radius = Minimap:GetWidth() / 2 - 8
    local distance = math.sqrt(x * x + y * y)
    -- Out of the minimap's reach: on its edge, pointing the way
    if distance > radius then
        x, y = x * radius / distance, y * radius / distance
        minimapPin:SetAlpha(0.75)
    else
        minimapPin:SetAlpha(1)
    end
    minimapPin:SetFrameLevel(Minimap:GetFrameLevel() + 5)
    minimapPin:ClearAllPoints()
    minimapPin:SetPoint("CENTER", Minimap, "CENTER", x, y)
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
    local before = state.items[key] and state.items[key].rank or 0
    AnimateForge(state.items[key], result, function()
        FlareRanks(before, result.rank)
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
