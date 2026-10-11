-- The loot collection: a page of the N window (TalentTree.lua's hub) showing, for each of our own bosses and for
-- Mythic+, every static piece it drops and whether this character ever looted it (the server's: mod-legendary
-- LootCollection.cpp, prefix "Collection"; .agents/plans/loot-collection). Legendaries and Uniques are unknown until
-- seen (on any character of the account); seen, they show, not yet collected here.
--
-- Laid out as the retail Collections journal: the bosses listed as the Pet Journal lists its pets, the pieces as the
-- Toy Box shows its toys (its tiled ground and corners, its collected / uncollected borders, the Heirlooms' slot
-- headers) - retail pieces (Interface\RetailUI, localTools/interface/extractRetailUi.py) and the game's own fonts.
-- The window is drawn about 0.78 of the screen: its sizes are the window's own units, retail's a quarter larger.

local PREFIX = "Collection"
local FRIZ = "Fonts\\FRIZQT__.TTF"
local QUESTION = "Interface\\Icons\\INV_Misc_QuestionMark"
local MARBLE = RetailUIFiles["ui-background-marble"]

local NORMAL = { 1, 0.82, 0 }
local WHITE = { 1, 1, 1 }
local GREY = { 0.5, 0.5, 0.5 }
local DIM = { 0.62, 0.58, 0.5 }
local LEGENDARY = { 1, 0.5, 0 }
local LEGENDARY_DIM = { 0.72, 0.42, 0.14 }

local FRENCH = GetLocale() == "frFR"
local TEXT = FRENCH and {
    tab = "Collection",
    sections = { [0] = "Armure", [1] = "Armes", [2] = "Bijoux", [3] = "Légendaires et Uniques" },
    hiddenLegendary = "Légendaire inconnu",
    hiddenUnique = "Unique inconnu",
    hiddenHint = "Il se dévoilera la première fois que vous l'obtiendrez.",
    seen = "Obtenu sur un autre personnage",
    notCollected = "Pas encore obtenu",
    collected = "Obtenu",
    added = "|cffffd100Collection :|r %s ajouté.",
} or {
    tab = "Collection",
    sections = { [0] = "Armor", [1] = "Weapons", [2] = "Trinkets", [3] = "Legendaries and Uniques" },
    hiddenLegendary = "Unknown legendary",
    hiddenUnique = "Unknown Unique",
    hiddenHint = "It reveals itself the first time you obtain it.",
    seen = "Obtained on another character",
    notCollected = "Not obtained yet",
    collected = "Obtained",
    added = "|cffffd100Collection:|r %s added.",
}

local KIND_LEGENDARY, KIND_UNIQUE = 3, 4
local STATE_MISSING, STATE_SEEN, STATE_COLLECTED = 0, 1, 2

-- The window's layout (TalentTree.lua: 1489 x 806; its title bar 21 high)
local WIDTH = 1489
local INSET_TOP, INSET_BOTTOM, INSET_SIDE = -28, 10, 10
local LIST_W = 380
local LIST_TOP = -58        -- under the window's round portrait, which overhangs its top left corner
local ROW_W, ROW_H, ROW_GAP = 360, 79, 2
local GRID_TOP = -118
-- A boss's button in the list: its Défi board page's art (Interface\ChallengeBoard,
-- localTools/interface/buildChallengeGodArt.py) - its place as a band behind it, its figure's head and shoulders on
-- the left, the name over a shade on the right. Mythic+ keeps an icon.
local SCENE_ART = "Interface\\ChallengeBoard\\"
local SCENES = {
    [1] = { art = "ChallengeWarden" },
    [2] = { art = "ChallengeForge" },
    [3] = { art = "ChallengeVoice" },
    [4] = { art = "ChallengeGod" },
}
local FIGURE_BOTTOM = 0.6367        -- the figure fills the top of its texture (ChallengeBoard.lua *_FIGURE_BOTTOM)
local FIGURE_SHOWN = 0.4            -- the share of it shown: head and shoulders
local FIGURE_ASPECT = 444 / 565     -- the figure's painted proportions (the board's)
local BACKDROP_ASPECT = 836 / 565
local COLUMNS = 3
local CELL_H = 86
local ICON = 52
local HEADER_H = 52

local page, list, detail
local sources, order = {}, {}
local pieces = {}           -- source id -> { pieces, groups }
local receiving = {}
local selected
local fresh = {}            -- rows just collected: their "new" glow when drawn

local function Show(region, shown)
    if shown then
        region:Show()
    else
        region:Hide()
    end
end

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function Atlas(parent, layer, name, width, height)
    local texture = parent:CreateTexture(nil, layer)
    RetailUI.SetAtlas(texture, name)
    if width then
        texture:SetSize(width, height)
    end
    return texture
end

local function Text(parent, size, colour, layer)
    local text = parent:CreateFontString(nil, layer or "OVERLAY")
    text:SetFont(FRIZ, size)
    text:SetShadowOffset(1, -1)
    text:SetTextColor(unpack(colour))
    return text
end

-- The retail InsetFrameTemplate: a ground and its 3 px rim
local function Inset(parent, ground)
    local inset = CreateFrame("Frame", nil, parent)
    if ground then
        local texture = inset:CreateTexture(nil, "BACKGROUND")
        texture:SetAllPoints()
        texture:SetTexture(ground, true)
        texture:SetHorizTile(true)
        texture:SetVertTile(true)
    end
    local function corner(name, point)
        local texture = Atlas(inset, "BORDER", name, 6, 6)
        texture:SetPoint(point, inset, point)
        return texture
    end
    local tl, tr = corner("UI-Frame-InnerTopLeft", "TOPLEFT"), corner("UI-Frame-InnerTopRight", "TOPRIGHT")
    local bl, br = corner("UI-Frame-InnerBotLeftCorner", "BOTTOMLEFT"), corner("UI-Frame-InnerBotRight", "BOTTOMRIGHT")
    local function edge(name, vertical, p1, a1, r1, p2, a2, r2)
        local texture = Atlas(inset, "BORDER", name)
        if vertical then
            texture:SetWidth(3)
        else
            texture:SetHeight(3)
        end
        texture:SetPoint(p1, a1, r1)
        texture:SetPoint(p2, a2, r2)
    end
    edge("!UI-Frame-InnerLeftTile", true, "TOPLEFT", tl, "BOTTOMLEFT", "BOTTOMLEFT", bl, "TOPLEFT")
    edge("!UI-Frame-InnerRightTile", true, "TOPRIGHT", tr, "BOTTOMRIGHT", "BOTTOMRIGHT", br, "TOPRIGHT")
    edge("_UI-Frame-InnerTopTile", false, "TOPLEFT", tl, "TOPRIGHT", "TOPRIGHT", tr, "TOPLEFT")
    edge("_UI-Frame-InnerBotTile", false, "BOTTOMLEFT", bl, "BOTTOMRIGHT", "BOTTOMRIGHT", br, "BOTTOMLEFT")
    return inset
end

-- Item info the client may not have yet: asked through a hidden tooltip, the page drawn again as it comes
local scanner = CreateFrame("GameTooltip", "CollectionScanTooltip", nil, "GameTooltipTemplate")
scanner:SetOwner(WorldFrame, "ANCHOR_NONE")
local waiting = {}

local function ItemInfo(entry)
    if not entry or entry == 0 then
        return nil
    end
    local name, link, quality, _, _, _, _, _, equipLoc, icon = GetItemInfo(entry)
    if not name then
        if not waiting[entry] then
            waiting[entry] = true
            scanner:SetHyperlink("item:" .. entry)
        end
        return nil
    end
    waiting[entry] = nil
    return { name = name, link = link, quality = quality, equipLoc = equipLoc, icon = icon }
end

local function IsLegendary(piece)
    return piece.kind == KIND_LEGENDARY or piece.kind == KIND_UNIQUE
end

-- The bosses list (the Pet Journal's) ---------------------------------------------------------------------------------

local rows = {}
local Select

local function PaintRow(row)
    local source = sources[row.id]
    if not source then
        return
    end
    row.name:SetText(source.name)
    row.count:SetFormattedText("%d / %d", source.collected, source.total)
    local info = ItemInfo(source.icon)
    row.icon:SetTexture(info and info.icon or QUESTION)
    Show(row.selected, row.id == selected)
    -- Its board art, or its icon
    local scene = SCENES[row.id]
    Show(row.backdrop, scene)
    Show(row.figure, scene)
    Show(row.shade, scene)
    Show(row.icon, not scene)
    Show(row.ground, not scene)
    row.name:ClearAllPoints()
    row.count:ClearAllPoints()
    if scene then
        row.backdrop:SetTexture(SCENE_ART .. scene.art .. "-Backdrop")
        row.figure:SetTexture(SCENE_ART .. scene.art .. "-Figure")
        local light = scene.shadow or 1
        row.figure:SetVertexColor(light, light, light)
        row.name:SetPoint("TOPLEFT", row, "TOPLEFT", row.figureWidth + 4, -10)
        row.count:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", row.figureWidth + 4, 10)
    else
        row.name:SetPoint("TOPLEFT", row.icon, "TOPRIGHT", 12, -6)
        row.count:SetPoint("BOTTOMLEFT", row.icon, "BOTTOMRIGHT", 12, 6)
    end
    row.name:SetPoint("RIGHT", row, "RIGHT", -12, 0)
end

local function CreateRow(id, index)
    local row = CreateFrame("Button", nil, list)
    row.id = id
    row:SetSize(ROW_W, ROW_H)
    row:SetPoint("TOP", list, "TOP", 0, -10 - (index - 1) * (ROW_H + ROW_GAP))
    row.ground = Atlas(row, "BACKGROUND", "petlist-buttonbackground")
    row.ground:SetAllPoints()
    -- The board's art: its place, a band of it as wide as the button (its texture holds the painting stretched
    -- square); the figure's top on the left; a shade under the name
    row.backdrop = row:CreateTexture(nil, "BACKGROUND")
    row.backdrop:SetPoint("TOPLEFT", 3, -3)
    row.backdrop:SetPoint("BOTTOMRIGHT", -3, 3)
    local share = BACKDROP_ASPECT / ((ROW_W - 6) / (ROW_H - 6))
    row.backdrop:SetTexCoord(0, 1, 0.4 - share / 2, 0.4 + share / 2)
    row.figure = row:CreateTexture(nil, "BORDER")
    row.figureWidth = (ROW_H - 6) * FIGURE_ASPECT / FIGURE_SHOWN
    row.figure:SetSize(row.figureWidth, ROW_H - 6)
    row.figure:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", 3, 3)
    row.figure:SetTexCoord(0, 1, 0.02, 0.02 + FIGURE_BOTTOM * FIGURE_SHOWN)
    row.shade = row:CreateTexture(nil, "BORDER")
    row.shade:SetTexture("Interface\\Buttons\\WHITE8X8")
    row.shade:SetPoint("TOPRIGHT", -3, -3)
    row.shade:SetPoint("BOTTOMRIGHT", -3, 3)
    row.shade:SetWidth(ROW_W - row.figureWidth * 0.7)
    row.shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.2, 0, 0, 0, 0.85)
    row.selected = Atlas(row, "BORDER", "petlist-buttonselect")
    row.selected:SetAllPoints()
    row.selected:Hide()
    local highlight = Atlas(row, "HIGHLIGHT", "petlist-buttonhighlight")
    highlight:SetAllPoints()
    highlight:SetBlendMode("ADD")
    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetSize(ROW_H - 26, ROW_H - 26)
    row.icon:SetPoint("LEFT", row, "LEFT", 14, 0)
    row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    -- (a long name on two lines)
    row.name = Text(row, 15, NORMAL)
    row.name:SetHeight(40)
    row.name:SetJustifyV("TOP")
    row.name:SetJustifyH("LEFT")
    row.count = Text(row, 14, WHITE)
    row:SetScript("OnClick", function(self)
        if selected ~= self.id then
            PlaySound("igMainMenuOptionCheckBoxOn")
            Select(self.id)
        end
    end)
    rows[id] = row
    return row
end

-- The pieces (the Toy Box's) -------------------------------------------------------------------------------------------

local buttons, headers = {}, {}

-- A legendary's copy the character has, worn or in its bags: the tooltip set to it (its real rolls)
local function ShowOwnedCopy(item)
    for slot = 1, 19 do
        local link = GetInventoryItemLink("player", slot)
        if link and tonumber(link:match("item:(%d+)")) == item then
            GameTooltip:SetInventoryItem("player", slot)
            return true
        end
    end
    for bag = 0, 4 do
        for slot = 1, GetContainerNumSlots(bag) do
            local link = GetContainerItemLink(bag, slot)
            if link and tonumber(link:match("item:(%d+)")) == item then
                GameTooltip:SetBagItem(bag, slot)
                return true
            end
        end
    end
end

local function Tooltip(owner, piece, info)
    GameTooltip:SetOwner(owner, "ANCHOR_RIGHT")
    if IsLegendary(piece) and piece.state == STATE_MISSING then
        GameTooltip:AddLine(piece.kind == KIND_UNIQUE and TEXT.hiddenUnique or TEXT.hiddenLegendary,
            unpack(LEGENDARY))
        if info and info.equipLoc and _G[info.equipLoc] then
            GameTooltip:AddLine(_G[info.equipLoc], 1, 1, 1)
        end
        GameTooltip:AddLine(TEXT.hiddenHint, nil, nil, nil, true)
    elseif IsLegendary(piece) and ShowOwnedCopy(piece.entry) then
        -- Its copy on the character, rolls and all (Legendary.lua decorates it)
        GameTooltip:AddLine(" ")
    else
        GameTooltip:SetHyperlink("item:" .. piece.entry)
        if IsLegendary(piece) and piece.low and EvolutionsLegendary and EvolutionsLegendary.Describe then
            EvolutionsLegendary.Describe(GameTooltip, piece.entry, piece.low, piece.high)
        end
        GameTooltip:AddLine(" ")
        if piece.state == STATE_COLLECTED then
            GameTooltip:AddLine(TEXT.collected, 0.1, 1, 0.1)
        elseif piece.state == STATE_SEEN then
            GameTooltip:AddLine(TEXT.seen, unpack(GREY))
        else
            GameTooltip:AddLine(TEXT.notCollected, unpack(GREY))
        end
    end
    GameTooltip:Show()
end

local function CreateButton()
    local button = CreateFrame("Button", nil, detail.content)
    button:SetSize(ICON, ICON)
    button.icon = button:CreateTexture(nil, "ARTWORK")
    button.icon:SetAllPoints()
    button.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    -- The collected border round the icon, the uncollected one in its place (the Toy Box's proportions)
    button.collected = Atlas(button, "OVERLAY", "collections-itemborder-collected", ICON * 56 / 42, ICON * 56 / 42)
    button.collected:SetPoint("CENTER")
    button.uncollected = Atlas(button, "OVERLAY", "collections-itemborder-uncollected", ICON * 50 / 42,
        ICON * 50 / 42)
    button.uncollected:SetPoint("CENTER")
    button.glow = Atlas(button, "BACKGROUND", "collections-newglow", 220, 70)
    button.glow:SetPoint("LEFT", button, "RIGHT", -20, 0)
    button.glow:SetBlendMode("ADD")
    button.glow:Hide()
    button.name = Text(button, 15, WHITE)
    button.name:SetPoint("LEFT", button, "RIGHT", 14, 0)
    button.name:SetJustifyH("LEFT")
    button.name:SetHeight(44)
    button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
    button:SetScript("OnEnter", function(self)
        Tooltip(self, self.piece, ItemInfo(self.piece.entry))
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)
    button:SetScript("OnClick", function(self)
        local info = ItemInfo(self.piece.entry)
        local hidden = IsLegendary(self.piece) and self.piece.state == STATE_MISSING
        if info and not hidden and IsModifiedClick("CHATLINK") then
            ChatEdit_InsertLink(info.link)
        elseif info and not hidden and IsModifiedClick("DRESSUP") then
            DressUpItemLink(info.link)
        end
    end)
    return button
end

local function PaintButton(button, piece, nameWidth)
    button.piece = piece
    local info = ItemInfo(piece.entry)
    local hidden = IsLegendary(piece) and piece.state == STATE_MISSING
    local owned = piece.state == STATE_COLLECTED
    button.icon:SetTexture(not hidden and info and info.icon or QUESTION)
    button.icon:SetDesaturated(not owned)
    button.icon:SetAlpha(owned and 1 or 0.6)
    Show(button.collected, owned)
    Show(button.uncollected, not owned)
    button.name:SetWidth(nameWidth)
    if hidden then
        button.name:SetText(piece.kind == KIND_UNIQUE and TEXT.hiddenUnique or TEXT.hiddenLegendary)
        button.name:SetTextColor(unpack(LEGENDARY_DIM))
    else
        button.name:SetText(info and info.name or "")
        if owned and info then
            local colour = ITEM_QUALITY_COLORS[info.quality] or ITEM_QUALITY_COLORS[1]
            button.name:SetTextColor(colour.r, colour.g, colour.b)
        else
            button.name:SetTextColor(unpack(IsLegendary(piece) and LEGENDARY_DIM or DIM))
        end
    end
    if fresh[piece.row] then
        fresh[piece.row] = nil
        button.glow:Show()
        button.glowUntil = GetTime() + 4
    end
end

local function Header(index, text, y, small)
    local header = headers[index]
    if not header then
        header = {}
        header.bar = Atlas(detail.content, "ARTWORK", "collections-slotheader", 600, 30)
        header.text = Text(detail.content, 16, NORMAL)
        header.text:SetPoint("CENTER", header.bar, "CENTER", 0, 1)
        header.label = Text(detail.content, 15, NORMAL)
        headers[index] = header
    end
    header.bar:ClearAllPoints()
    header.bar:SetPoint("TOP", detail.content, "TOP", 0, y)
    header.label:ClearAllPoints()
    header.label:SetPoint("TOPLEFT", detail.content, "TOPLEFT", 24, y - 6)
    Show(header.bar, not small)
    Show(header.text, not small)
    Show(header.label, small)
    header.text:SetText(text)
    header.label:SetText(text)
    return small and 32 or HEADER_H
end

local function Grid(shown, y, width)
    local cellW = floor(width / COLUMNS)
    for index, piece in ipairs(shown) do
        local button
        for _, candidate in ipairs(buttons) do
            if not candidate.used then
                button = candidate
                break
            end
        end
        if not button then
            button = CreateButton()
            tinsert(buttons, button)
        end
        button.used = true
        local column, row = (index - 1) % COLUMNS, floor((index - 1) / COLUMNS)
        button:ClearAllPoints()
        button:SetPoint("TOPLEFT", detail.content, "TOPLEFT", 30 + column * cellW, y - 10 - row * CELL_H)
        PaintButton(button, piece, cellW - ICON - 50)
        button:Show()
    end
    return ceil(#shown / COLUMNS) * CELL_H + 6
end

local function PaintDetail()
    for _, button in ipairs(buttons) do
        button.used = false
        button:Hide()
    end
    for _, header in ipairs(headers) do
        header.bar:Hide()
        header.text:Hide()
        header.label:Hide()
    end
    local source = selected and sources[selected]
    if not source then
        return
    end
    detail.name:SetText(source.name)
    detail.bar:SetMinMaxValues(0, max(1, source.total))
    detail.bar:SetValue(source.collected)
    detail.bar.text:SetFormattedText("%d / %d", source.collected, source.total)

    local data = pieces[selected]
    if not data then
        return
    end
    local width = detail.content:GetWidth()
    local y, headerIndex = -4, 0
    for kind = 0, 2 do
        local ofKind = {}
        for _, piece in ipairs(data.pieces) do
            if piece.kind == kind then
                tinsert(ofKind, piece)
            end
        end
        if #ofKind > 0 then
            headerIndex = headerIndex + 1
            y = y - Header(headerIndex, TEXT.sections[kind], y)
            y = y - Grid(ofKind, y, width)
        end
    end
    local legendaries = {}
    for _, piece in ipairs(data.pieces) do
        if IsLegendary(piece) then
            tinsert(legendaries, piece)
        end
    end
    if #legendaries > 0 then
        headerIndex = headerIndex + 1
        y = y - Header(headerIndex, TEXT.sections[3], y)
        -- Mythic+'s by dungeon, under its name; a boss's together
        local groups = { { id = 0 } }
        for _, group in ipairs(data.groups) do
            tinsert(groups, group)
        end
        for _, group in ipairs(groups) do
            local ofGroup = {}
            for _, piece in ipairs(legendaries) do
                if piece.group == group.id then
                    tinsert(ofGroup, piece)
                end
            end
            if #ofGroup > 0 then
                if group.name then
                    headerIndex = headerIndex + 1
                    y = y - Header(headerIndex, group.name, y, true)
                end
                y = y - Grid(ofGroup, y, width)
            end
        end
    end
    detail.content:SetHeight(max(1, -y + 10))
    detail.scroll:UpdateScrollChildRect()
end

Select = function(id)
    selected = id
    for _, row in pairs(rows) do
        PaintRow(row)
    end
    detail.scroll:SetVerticalScroll(0)
    PaintDetail()
    Send("LIST\t" .. id)
end

-- The page --------------------------------------------------------------------------------------------------------

local function Corner(parent, point, flipX, flipY)
    local texture = Atlas(parent, "BORDER", "collections-background-corner", 90, 67)
    local info = RetailUIAtlas["collections-background-corner"]
    local left, right, top, bottom = info[4], info[5], info[6], info[7]
    texture:SetTexCoord(flipX and right or left, flipX and left or right, flipY and bottom or top,
        flipY and top or bottom)
    texture:SetPoint(point, parent, point, flipX and -3 or 3, flipY and 3 or -3)
end

local function Build(window)
    page = CreateFrame("Frame", nil, window)
    page:SetAllPoints(window)

    list = Inset(page, MARBLE)
    list:SetPoint("TOPLEFT", window, "TOPLEFT", INSET_SIDE, LIST_TOP)
    list:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", INSET_SIDE, INSET_BOTTOM)
    list:SetWidth(LIST_W)

    detail = Inset(page, nil)
    detail:SetPoint("TOPLEFT", window, "TOPLEFT", INSET_SIDE + LIST_W + 4, INSET_TOP)
    detail:SetPoint("BOTTOMRIGHT", window, "BOTTOMRIGHT", -INSET_SIDE, INSET_BOTTOM)
    -- The Toy Box's ground, repeated (set without the tiling flag, its last texels were stretched over the rest)
    local ground = detail:CreateTexture(nil, "BACKGROUND")
    ground:SetAllPoints()
    ground:SetTexture(RetailUIAtlas["collections-background-tile"][1], true)
    ground:SetHorizTile(true)
    ground:SetVertTile(true)
    Corner(detail, "TOPLEFT", false, false)
    Corner(detail, "TOPRIGHT", true, false)
    Corner(detail, "BOTTOMLEFT", false, true)
    Corner(detail, "BOTTOMRIGHT", true, true)

    detail.name = detail:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    detail.name:SetPoint("TOP", detail, "TOP", 0, -22)

    -- The progress bar (the Collections journal's: the skill bar's border, its fill)
    local bar = CreateFrame("StatusBar", nil, detail)
    bar:SetSize(380, 18)
    bar:SetPoint("TOP", detail.name, "BOTTOM", 0, -12)
    bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    bar:SetStatusBarColor(0.03, 0.65, 0.0)
    local barGround = bar:CreateTexture(nil, "BACKGROUND")
    barGround:SetAllPoints()
    barGround:SetTexture(0, 0, 0, 0.6)
    local border = bar:CreateTexture(nil, "OVERLAY")
    border:SetTexture("Interface\\PaperDollInfoFrame\\UI-Character-Skills-BarBorder")
    border:SetPoint("TOPLEFT", bar, "TOPLEFT", -5, 6)
    border:SetPoint("BOTTOMRIGHT", bar, "BOTTOMRIGHT", 5, -6)
    bar.text = bar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    bar.text:SetPoint("CENTER", bar, "CENTER", 0, 1)
    detail.bar = bar

    -- The pieces, scrolled (the game's scroll bar)
    local scroll = CreateFrame("ScrollFrame", "CollectionPageScroll", detail, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", detail, "TOPLEFT", 8, GRID_TOP)
    scroll:SetPoint("BOTTOMRIGHT", detail, "BOTTOMRIGHT", -34, 12)
    local content = CreateFrame("Frame", nil, scroll)
    content:SetSize(WIDTH - 2 * INSET_SIDE - LIST_W - 4 - 8 - 34, 1)
    scroll:SetScrollChild(content)
    detail.scroll, detail.content = scroll, content

    -- Items the client was still asking for: drawn again once known; a new piece's glow fading
    local recheck = 0
    page:SetScript("OnUpdate", function(_, elapsed)
        recheck = recheck + elapsed
        if recheck > 0.5 and next(waiting) then
            recheck = 0
            local missing = false
            for entry in pairs(waiting) do
                if GetItemInfo(entry) then
                    waiting[entry] = nil
                else
                    missing = true
                end
            end
            if not missing then
                for _, row in pairs(rows) do
                    PaintRow(row)
                end
                PaintDetail()
            end
        end
        local now = GetTime()
        for _, button in ipairs(buttons) do
            if button.glowUntil then
                local left = button.glowUntil - now
                if left <= 0 then
                    button.glowUntil = nil
                    button.glow:Hide()
                else
                    button.glow:SetAlpha(min(1, left / 1.5))
                end
            end
        end
    end)
    page:SetScript("OnShow", function() Send("OPEN") end)
    return page
end

TalentTreeHub.Register({ name = "collection", label = TEXT.tab, Build = Build })

-- The window open on the Collection, on a source (its server id; the first by default)
function Collection_Open(id)
    TalentTreeHub.Open("collection")
    if id and detail then
        Select(id)
    end
end

-- The server's answers ------------------------------------------------------------------------------------------------

local function Split(message)
    local fields = {}
    for field in (message .. "\t"):gmatch("([^\t]*)\t") do
        tinsert(fields, field)
    end
    return fields
end

local function Receive(message)
    local fields = Split(message)
    local command = fields[1]
    if command == "S" then
        local id = tonumber(fields[2])
        if not sources[id] then
            tinsert(order, id)
        end
        sources[id] = { id = id, collected = tonumber(fields[3]) or 0, total = tonumber(fields[4]) or 0,
                        name = fields[5] or "", icon = tonumber(fields[6]) }
    elseif command == "E" then
        if not list then
            return
        end
        for index, id in ipairs(order) do
            PaintRow(rows[id] or CreateRow(id, index))
        end
        if not selected and order[1] then
            Select(order[1])
        elseif selected then
            PaintDetail()
            Send("LIST\t" .. selected)
        end
    elseif command == "G" then
        local id = tonumber(fields[2])
        receiving[id] = receiving[id] or { pieces = {}, groups = {} }
        -- The dungeon's name as this client knows it (its own language), else the server's
        local group = tonumber(fields[3])
        local name = GetLFGDungeonInfo and GetLFGDungeonInfo(group)
        tinsert(receiving[id].groups, { id = group, name = name and name ~= "" and name or fields[4] })
    elseif command == "P" then
        local id = tonumber(fields[2])
        receiving[id] = receiving[id] or { pieces = {}, groups = {} }
        tinsert(receiving[id].pieces, { row = tonumber(fields[3]), entry = tonumber(fields[4]),
            kind = tonumber(fields[5]), state = tonumber(fields[6]), group = tonumber(fields[7]) or 0,
            low = tonumber(fields[8]), high = tonumber(fields[9]) })
    elseif command == "F" then
        local id = tonumber(fields[2])
        pieces[id] = receiving[id] or { pieces = {}, groups = {} }
        receiving[id] = nil
        if id == selected and detail then
            PaintDetail()
        end
    elseif command == "N" then
        local row = tonumber(fields[3])
        fresh[row] = true
        local info = ItemInfo(row)
        DEFAULT_CHAT_FRAME:AddMessage(format(TEXT.added, info and info.link or "?"))
        if page and page:IsVisible() then
            Send("OPEN")
        end
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix == PREFIX and sender == UnitName("player") then
        Receive(message)
    end
end)
