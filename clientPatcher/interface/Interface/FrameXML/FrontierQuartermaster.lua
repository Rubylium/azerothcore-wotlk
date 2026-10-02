-- Le Front du Nord's quartermaster (server: mod-stat-growth src/frontier/FrontierQuartermaster.cpp), on Krasus'
-- Landing in Dalaran: her window, opened by talking to her in place of a gossip menu. Her post painted behind, herself
-- on the right; on the left the day's three contracts, and the shop: a piece of a tier's gear for the slot chosen,
-- or an essence, paid in Éclats de givre. Built from InfiniteDungeon.lua's pieces in the challenge board's materials.
--
-- Protocol (prefix "Frontier", tab-separated, whispered to oneself):
--   server  QM <shards> <seconds to the next day> <gear price tier I> <II> <III> <IV> <essence price>
--           QMC <contract 1-3> <deed 1-4> <zone id, 0: any> <tier> <target> <progress> <done 0/1> <shards> <item level>
--           QMEND                      opens (or refreshes) the window
--           QMCLOSE                    walked away from her
--   client  QM_BUY <tier 1-4> <equipment slot>, QM_ESSENCE, QM_CLOSED
--
-- By hand: FrontierQuartermaster_Receive("QM\t42\t3600\t30\t45\t60\t80\t25"),
--          FrontierQuartermaster_Receive("QMC\t1\t2\t66\t3\t2\t1\t0\t20\t219"), FrontierQuartermaster_Receive("QMEND")

local UI = InfiniteDungeonUI
local SHARED = FrontierUIShared
if not UI or not SHARED then
    return
end

local Colored, SetAtlas, Tween, OutCubic = UI.Colored, UI.SetAtlas, UI.Tween, UI.OutCubic
local Card, Divider, Label, Heading = UI.Card, UI.Divider, UI.Label, UI.Heading
local QuietButton, CloseButton = UI.QuietButton, UI.CloseButton
local HEADING, GOLD, BORDER, SOFT, MUTED, AMBER = UI.HEADING, UI.GOLD, UI.BORDER, UI.SOFT, UI.MUTED, UI.AMBER
local ZONES, NUMERALS = SHARED.ZONES, SHARED.NUMERALS

local PREFIX = "Frontier"
local french = UI.french
local TEXT = french and {
    title = "Intendance du Front du Nord",
    who = "Ysolde Brisegivre, intendante de la Croisade d'argent",
    shards = "%d éclats de givre",
    contractsTab = "Contrats du jour",
    shopTab = "Boutique",
    deeds = { "Abattre %d élites rôdeurs", "Refermer %d failles arcaniques", "Ouvrir %d coffres du Front",
              "Abattre un Colosse" },
    anywhere = "N'importe quel palier",
    tier = "Palier %s",
    reward = "%d éclats de givre  ·  équipement de niveau %d",
    done = "Rempli, payé",
    nextDay = "Nouveaux contrats dans %s",
    gear = "Équipement du palier",
    tierButton = "Palier %s  ·  %d\n%d éclats",
    slotHint = "Une pièce de niveau %d pour %d éclats de givre, faite pour vous.",
    confirm = "%s, palier %s (niveau %d) pour %d éclats ?",
    buy = "Acheter",
    cancel = "Annuler",
    essence = "Essence",
    essenceHint = "Une essence pour vos objets, %d éclats de givre.",
    essenceButton = "Acheter  ·  %d éclats",
    lacking = "Il vous manque des éclats de givre.",
} or {
    title = "Northrend Frontier Quartermaster",
    who = "Ysolde Brisegivre, Argent Crusade quartermaster",
    shards = "%d Frost Shards",
    contractsTab = "Today's contracts",
    shopTab = "Shop",
    deeds = { "Slay %d roaming elites", "Close %d arcane rifts", "Open %d Frontier chests", "Slay a Colossus" },
    anywhere = "Any tier",
    tier = "Tier %s",
    reward = "%d Frost Shards  ·  item level %d gear",
    done = "Fulfilled, paid",
    nextDay = "New contracts in %s",
    gear = "Tier gear",
    tierButton = "Tier %s  ·  %d\n%d shards",
    slotHint = "A piece of item level %d for %d Frost Shards, made for you.",
    confirm = "%s, tier %s (item level %d) for %d shards?",
    buy = "Buy",
    cancel = "Cancel",
    essence = "Essence",
    essenceHint = "An essence for your items, %d Frost Shards.",
    essenceButton = "Buy  ·  %d shards",
    lacking = "You lack Frost Shards.",
}

local ART = "Interface\\Frontier\\"
local SHARD_ICON = "Interface\\Icons\\INV_Frontier_FrostShard"
local ESSENCE_ICON = "Interface\\Icons\\INV_Enchant_EssenceCosmicGreater"
local WIDTH, HEIGHT = 820, 540
-- The scene inside the frame (buildFrontierArt.py QUARTERMASTER_INSIDE), her place on its right
-- (QUARTERMASTER_FIGURE) and the rows her figure fills in its texture (FIGURE_BOTTOM, printed by that script)
local INSIDE_LEFT, INSIDE_TOP = 8, 26
local FIGURE_WIDTH = 300
local FIGURE_BOTTOM = 0.8438
local CONTENT_WIDTH = 470

-- The slots the shop sells for, in the window's order: the inventory slot names (GetInventorySlotInfo gives their
-- empty-slot picture and their 1-based id: the server's equipment slot plus one) and their names
local SLOTS = {
    { "HeadSlot", HEADSLOT }, { "NeckSlot", NECKSLOT }, { "ShoulderSlot", SHOULDERSLOT }, { "BackSlot", BACKSLOT },
    { "ChestSlot", CHESTSLOT }, { "WristSlot", WRISTSLOT }, { "HandsSlot", HANDSSLOT }, { "WaistSlot", WAISTSLOT },
    { "LegsSlot", LEGSSLOT }, { "FeetSlot", FEETSLOT }, { "Finger0Slot", FINGER0SLOT },
    { "Trinket0Slot", TRINKET0SLOT }, { "MainHandSlot", MAINHANDSLOT }, { "SecondaryHandSlot", SECONDARYHANDSLOT },
    { "RangedSlot", RANGEDSLOT },
}

local TIER_LOOT = { 200, 213, 219, 226 }

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

-- 4532 -> "1:15:32"
local function Clock(seconds)
    seconds = max(0, floor(seconds))
    return format("%d:%02d:%02d", floor(seconds / 3600), floor(seconds / 60) % 60, seconds % 60)
end

-- What the server last said ------------------------------------------------------------------------------------------

local state = { shards = 0, nextDay = 0, prices = { 0, 0, 0, 0 }, essence = 0, contracts = {} }
local window
local closedByServer = false

-- The window -------------------------------------------------------------------------------------------------------

local function CreateWindow()
    window = CreateFrame("Frame", "FrontierQuartermasterFrame", UIParent)
    window:SetSize(WIDTH, HEIGHT)
    window:SetPoint("CENTER", UIParent, "CENTER", 0, 30)
    window:SetFrameStrata("HIGH")
    window:SetToplevel(true)
    window:EnableMouse(true)
    window:SetMovable(true)
    window:SetClampedToScreen(true)
    window:Hide()
    tinsert(UISpecialFrames, "FrontierQuartermasterFrame")

    -- Her post, darkened under the content; herself on the right
    local scene = window:CreateTexture(nil, "BACKGROUND")
    scene:SetTexture(ART .. "Quartermaster-Backdrop")
    scene:SetPoint("TOPLEFT", INSIDE_LEFT, -INSIDE_TOP)
    scene:SetPoint("BOTTOMRIGHT", -INSIDE_LEFT, INSIDE_LEFT)
    local shade = window:CreateTexture(nil, "BORDER")
    shade:SetTexture("Interface\\Buttons\\WHITE8X8")
    shade:SetPoint("TOPLEFT", INSIDE_LEFT, -INSIDE_TOP)
    shade:SetPoint("BOTTOMLEFT", INSIDE_LEFT, INSIDE_LEFT)
    shade:SetWidth(CONTENT_WIDTH + 90)
    shade:SetGradientAlpha("HORIZONTAL", 0.03, 0.025, 0.02, 0.82, 0.03, 0.025, 0.02, 0)
    local figure = window:CreateTexture(nil, "ARTWORK")
    figure:SetTexture(ART .. "Quartermaster-Figure")
    figure:SetTexCoord(0, 1, 0, FIGURE_BOTTOM)
    figure:SetPoint("TOPRIGHT", -INSIDE_LEFT, -INSIDE_TOP)
    figure:SetPoint("BOTTOMRIGHT", -INSIDE_LEFT, INSIDE_LEFT)
    figure:SetWidth(FIGURE_WIDTH)
    RetailUI.ApplyNineSlice(window, false)

    local windowTitle = window:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    windowTitle:SetPoint("TOPLEFT", window, "TOPLEFT", 26, -4)
    windowTitle:SetPoint("TOPRIGHT", window, "TOPRIGHT", -26, -4)
    windowTitle:SetJustifyH("CENTER")
    windowTitle:SetText(TEXT.title)
    windowTitle:SetTextColor(1, 0.82, 0)
    CloseButton(window, function() window:Hide() end)

    local mover = CreateFrame("Frame", nil, window)
    mover:SetPoint("TOPLEFT", window, "TOPLEFT", 0, 16)
    mover:SetPoint("TOPRIGHT", window, "TOPRIGHT", -40, 16)
    mover:SetHeight(36)
    mover:SetFrameLevel(window:GetFrameLevel() + 16)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() window:StartMoving() end)
    mover:SetScript("OnDragStop", function() window:StopMovingOrSizing() end)

    -- Her name, and the shards the player holds
    local content = CreateFrame("Frame", nil, window)
    content:SetPoint("TOPLEFT", window, "TOPLEFT", INSIDE_LEFT + 22, -INSIDE_TOP - 16)
    content:SetSize(CONTENT_WIDTH, HEIGHT - INSIDE_TOP - 40)
    window.content = content
    local heading = Heading(content, 22)
    heading:SetPoint("TOPLEFT", content, "TOPLEFT", 0, 0)
    heading:SetText(TEXT.title)
    local who = Label(content, "GameFontHighlightSmall", MUTED)
    who:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -3)
    who:SetText(TEXT.who)
    local shardIcon = content:CreateTexture(nil, "ARTWORK")
    shardIcon:SetTexture(SHARD_ICON)
    shardIcon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
    shardIcon:SetSize(18, 18)
    shardIcon:SetPoint("TOPRIGHT", content, "TOPRIGHT", 0, -4)
    local shards = Label(content, "GameFontNormal", GOLD, "RIGHT")
    shards:SetPoint("RIGHT", shardIcon, "LEFT", -6, 0)
    window.shards = shards

    -- The two pages and their tabs
    local pages, tabs = {}, {}
    local function ShowPage(name)
        for key, page in pairs(pages) do
            UI.SetShown(page, key == name)
            UI.SetShown(tabs[key].underline, key == name)
        end
        window.page = name
    end
    for index, key in ipairs({ "contracts", "shop" }) do
        local tab = QuietButton(content, 150, 24, key == "contracts" and TEXT.contractsTab or TEXT.shopTab)
        tab:SetPoint("TOPLEFT", content, "TOPLEFT", (index - 1) * 158, -54)
        tab:SetScript("OnClick", function() ShowPage(key) end)
        local underline = Divider(tab, "OVERLAY")
        underline:SetPoint("TOPLEFT", tab, "BOTTOMLEFT", 6, -2)
        underline:SetPoint("TOPRIGHT", tab, "BOTTOMRIGHT", -6, -2)
        tab.underline = underline
        tabs[key] = tab
        local page = CreateFrame("Frame", nil, content)
        page:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -92)
        page:SetSize(CONTENT_WIDTH, HEIGHT - INSIDE_TOP - 140)
        pages[key] = page
    end
    window.pages = pages
    window.ShowPage = ShowPage

    -- Contracts: three cards and the turn of the day
    window.cards = {}
    for index = 1, 3 do
        local card = CreateFrame("Frame", nil, pages.contracts)
        card:SetSize(CONTENT_WIDTH, 98)
        card:SetPoint("TOPLEFT", pages.contracts, "TOPLEFT", 0, -(index - 1) * 106)
        Card(card, 0.82)
        local crest = card:CreateTexture(nil, "ARTWORK")
        crest:SetSize(52, 52)
        crest:SetPoint("LEFT", card, "LEFT", 10, 4)
        local title = Label(card, "GameFontNormal", HEADING)
        title:SetPoint("TOPLEFT", card, "TOPLEFT", 72, -12)
        local where = Label(card, "GameFontHighlightSmall", SOFT)
        where:SetPoint("TOPLEFT", title, "BOTTOMLEFT", 0, -3)
        local reward = Label(card, "GameFontHighlightSmall", AMBER)
        reward:SetPoint("TOPLEFT", where, "BOTTOMLEFT", 0, -3)
        local count = Label(card, "GameFontNormal", GOLD, "RIGHT")
        count:SetPoint("TOPRIGHT", card, "TOPRIGHT", -14, -12)
        local check = card:CreateTexture(nil, "OVERLAY")
        SetAtlas(check, "ui-questtracker-tracker-check-2x")
        check:SetSize(22, 22)
        check:SetPoint("TOPRIGHT", card, "TOPRIGHT", -12, -10)
        card.crest, card.title, card.where, card.reward = crest, title, where, reward
        card.count, card.check = count, check
        window.cards[index] = card
    end
    local nextDay = Label(pages.contracts, "GameFontHighlightSmall", MUTED)
    nextDay:SetPoint("TOPLEFT", pages.contracts, "TOPLEFT", 4, -3 * 106 - 4)
    window.nextDay = nextDay

    -- The shop: a tier, then a slot; an essence below
    local shop = pages.shop
    local gearLabel = Label(shop, "GameFontNormal", HEADING)
    gearLabel:SetPoint("TOPLEFT", shop, "TOPLEFT", 0, 0)
    gearLabel:SetText(TEXT.gear)
    window.tiers = {}
    window.tier = 1
    for tier = 1, 4 do
        local button = QuietButton(shop, 112, 40, "", 11)
        button:SetPoint("TOPLEFT", shop, "TOPLEFT", (tier - 1) * 119, -22)
        button:SetScript("OnClick", function()
            window.tier = tier
            window.Refresh()
        end)
        local underline = Divider(button, "OVERLAY")
        underline:SetPoint("TOPLEFT", button, "BOTTOMLEFT", 6, -2)
        underline:SetPoint("TOPRIGHT", button, "BOTTOMRIGHT", -6, -2)
        button.underline = underline
        window.tiers[tier] = button
    end

    window.slots = {}
    for index, slot in ipairs(SLOTS) do
        local id, texture = GetInventorySlotInfo(slot[1])
        local button = CreateFrame("Button", nil, shop)
        button:SetSize(44, 44)
        button:SetPoint("TOPLEFT", shop, "TOPLEFT", ((index - 1) % 8) * 52 + 8, -82 - floor((index - 1) / 8) * 52)
        button:SetBackdrop({ edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 10 })
        button:SetBackdropBorderColor(BORDER[1], BORDER[2], BORDER[3], 1)
        local icon = button:CreateTexture(nil, "ARTWORK")
        icon:SetTexture(texture)
        icon:SetPoint("TOPLEFT", 3, -3)
        icon:SetPoint("BOTTOMRIGHT", -3, 3)
        button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
        button.equipmentSlot, button.name = id - 1, slot[2]
        button:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:AddLine(self.name, HEADING[1], HEADING[2], HEADING[3])
            GameTooltip:AddLine(format(TEXT.slotHint, TIER_LOOT[window.tier], state.prices[window.tier]),
                SOFT[1], SOFT[2], SOFT[3], true)
            GameTooltip:Show()
        end)
        button:SetScript("OnLeave", function() GameTooltip:Hide() end)
        button:SetScript("OnClick", function(self)
            window.pick = self
            window.Refresh()
        end)
        window.slots[index] = button
    end

    -- The purchase to confirm
    local confirm = CreateFrame("Frame", nil, shop)
    confirm:SetSize(CONTENT_WIDTH, 40)
    confirm:SetPoint("TOPLEFT", shop, "TOPLEFT", 0, -196)
    Card(confirm, 0.85)
    local question = Label(confirm, "GameFontHighlightSmall", SOFT)
    question:SetPoint("LEFT", confirm, "LEFT", 12, 0)
    question:SetWidth(CONTENT_WIDTH - 210)
    local buy = QuietButton(confirm, 90, 24, TEXT.buy)
    buy:SetPoint("RIGHT", confirm, "RIGHT", -104, 0)
    buy:SetScript("OnClick", function()
        if window.pick then
            Send(format("QM_BUY\t%d\t%d", window.tier, window.pick.equipmentSlot))
        end
        window.pick = nil
        window.Refresh()
    end)
    local cancel = QuietButton(confirm, 90, 24, TEXT.cancel)
    cancel:SetPoint("RIGHT", confirm, "RIGHT", -8, 0)
    cancel:SetScript("OnClick", function()
        window.pick = nil
        window.Refresh()
    end)
    confirm.question, confirm.buy = question, buy
    window.confirm = confirm

    -- An essence
    local essence = CreateFrame("Frame", nil, shop)
    essence:SetSize(CONTENT_WIDTH, 56)
    essence:SetPoint("TOPLEFT", shop, "TOPLEFT", 0, -250)
    Card(essence, 0.82)
    local essenceIcon = UI.FramedIcon(essence, 38, ESSENCE_ICON)
    essenceIcon:SetPoint("LEFT", essence, "LEFT", 10, 0)
    local essenceTitle = Label(essence, "GameFontNormal", HEADING)
    essenceTitle:SetPoint("TOPLEFT", essenceIcon, "TOPRIGHT", 10, -2)
    essenceTitle:SetText(TEXT.essence)
    local essenceHint = Label(essence, "GameFontHighlightSmall", SOFT)
    essenceHint:SetPoint("TOPLEFT", essenceTitle, "BOTTOMLEFT", 0, -3)
    local essenceBuy = QuietButton(essence, 150, 24, "")
    essenceBuy:SetPoint("RIGHT", essence, "RIGHT", -10, 0)
    essenceBuy:SetScript("OnClick", function() Send("QM_ESSENCE") end)
    essence.hint, essence.buy = essenceHint, essenceBuy
    window.essence = essence

    -- The day turns while the window stays open
    local clock = 0
    window:SetScript("OnUpdate", function(_, elapsed)
        clock = clock + elapsed
        if clock >= 1 then
            clock = 0
            nextDay:SetText(format(TEXT.nextDay, Clock(state.nextDay - GetTime())))
        end
    end)
    window:SetScript("OnHide", function()
        if not closedByServer then
            Send("QM_CLOSED")
        end
        closedByServer = false
        window.pick = nil
    end)

    ShowPage("contracts")
end

-- Everything from the server's last word
local function Refresh()
    window.shards:SetText(format(TEXT.shards, state.shards))

    for index, card in ipairs(window.cards) do
        local contract = state.contracts[index]
        UI.SetShown(card, contract ~= nil)
        if contract then
            if not card.crest:SetTexture(ART .. "Crest" .. contract.tier) then
                card.crest:SetTexture(SHARD_ICON)
            end
            local zone = ZONES[contract.zone]
            card.title:SetText(format(TEXT.deeds[contract.deed] or "", contract.target))
            card.where:SetText((zone and zone.name .. "  ·  " or TEXT.anywhere .. "  ·  ")
                .. format(TEXT.tier, NUMERALS[contract.tier] or contract.tier))
            card.reward:SetText(contract.done and Colored(TEXT.done, MUTED)
                or format(TEXT.reward, contract.shards, contract.itemLevel))
            card.count:SetText(contract.done and "" or format("%d / %d", contract.progress, contract.target))
            UI.SetShown(card.check, contract.done)
            card.crest:SetDesaturated(contract.done)
            card:SetAlpha(contract.done and 0.7 or 1)
        end
    end
    window.nextDay:SetText(format(TEXT.nextDay, Clock(state.nextDay - GetTime())))

    for tier, button in ipairs(window.tiers) do
        button:SetText(format(TEXT.tierButton, NUMERALS[tier], TIER_LOOT[tier], state.prices[tier]))
        UI.SetShown(button.underline, tier == window.tier)
    end
    local price = state.prices[window.tier] or 0
    local pick = window.pick
    UI.SetShown(window.confirm, pick ~= nil)
    if pick then
        local affordable = state.shards >= price
        window.confirm.question:SetText(affordable and format(TEXT.confirm, pick.name, NUMERALS[window.tier],
            TIER_LOOT[window.tier], price) or Colored(TEXT.lacking, MUTED))
        if affordable then
            window.confirm.buy:Enable()
        else
            window.confirm.buy:Disable()
        end
    end
    for _, button in ipairs(window.slots) do
        button:SetBackdropBorderColor(unpack(button == pick and { GOLD[1], GOLD[2], GOLD[3], 1 }
            or { BORDER[1], BORDER[2], BORDER[3], 1 }))
    end
    window.essence.hint:SetText(format(TEXT.essenceHint, state.essence))
    window.essence.buy:SetText(format(TEXT.essenceButton, state.essence))
    if state.shards >= state.essence then
        window.essence.buy:Enable()
    else
        window.essence.buy:Disable()
    end
end

-- The server's messages ------------------------------------------------------------------------------------------

local pending = {}

local function Receive(message)
    local fields = { strsplit("\t", message) }
    local kind = fields[1]
    if kind == "QM" then
        pending = { shards = tonumber(fields[2]) or 0, nextDay = GetTime() + (tonumber(fields[3]) or 0),
                    prices = { tonumber(fields[4]) or 0, tonumber(fields[5]) or 0, tonumber(fields[6]) or 0,
                               tonumber(fields[7]) or 0 },
                    essence = tonumber(fields[8]) or 0, contracts = {} }
    elseif kind == "QMC" then
        local index = tonumber(fields[2])
        if index and pending.contracts then
            pending.contracts[index] = {
                deed = tonumber(fields[3]) or 1, zone = tonumber(fields[4]) or 0, tier = tonumber(fields[5]) or 1,
                target = tonumber(fields[6]) or 1, progress = tonumber(fields[7]) or 0, done = fields[8] == "1",
                shards = tonumber(fields[9]) or 0, itemLevel = tonumber(fields[10]) or 0,
            }
        end
    elseif kind == "QMEND" then
        if pending.contracts then
            state = pending
        end
        if not window then
            CreateWindow()
            window.Refresh = Refresh
        end
        Refresh()
        if not window:IsShown() then
            window:SetAlpha(0)
            window:SetScale(0.94)
            window:Show()
            Tween(0.28, 0, function(p)
                window:SetAlpha(p)
                window:SetScale(0.94 + 0.06 * OutCubic(p))
            end, nil, window)
        end
    elseif kind == "QMCLOSE" then
        if window and window:IsShown() then
            closedByServer = true
            window:Hide()
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        Receive(message)
    end
end)

FrontierQuartermaster_Receive = Receive
