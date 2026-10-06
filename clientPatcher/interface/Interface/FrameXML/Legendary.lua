-- Legendary items (modules/mod-legendary, .agents/plans/legendary-items/legendary-items.DESIGN.md): every copy rolls
-- its own item level, power and stats, which its base item's record cannot carry. The server tells them, by the copy's
-- id - the item's property seed, which an item link carries as its unique id - and this writes them into the item's
-- tooltip, in the client's own wording:
--   LEGENDARY <tab> C <tab> seed <tab> legendary <tab> item level <tab> power x10 <tab> low x10 <tab> high x10 <tab>
--     armour <tab> type=value,...
-- A copy not known yet (a link, another player's, the loot) is asked for: LEGENDARY <tab> Q <tab> seed.

local PREFIX = "LEGENDARY"
local french = GetLocale() == "frFR"

-- Per legendary: its base item, where it drops, its power's wording (%s: the rolled value) and its lore
local LEGENDARIES = {
    [1] = {
        item = 24567,
        source = french and "Légendaire · Cathédrale écarlate, Mythique+" or "Legendary · Scarlet Cathedral, Mythic+",
        power = french
            and "Vos dégâts directs marquent la cible : elle brûle pour %s des dégâts infligés, en dégâts du Sacré "
                .. "sur 4 sec."
            or "Your direct damage brands the target: it burns for %s of the damage dealt as Holy damage over "
                .. "4 sec.",
        lore = french and "« Le sceau ardent de l'Inquisiteur Fairbanks. Ce qu'il marque ne cesse plus de brûler. »"
            or "\"Inquisitor Fairbanks' burning seal. What it marks never stops burning.\"",
    },
}
local BASE_ITEMS = {}
for id, legendary in pairs(LEGENDARIES) do
    BASE_ITEMS[legendary.item] = id
end

local TEXT = {
    itemLevel = french and "Niveau d'objet %d" or "Item Level %d",
    window = french and "Puissance : %s - %s sur cette copie" or "Strength: %s - %s on this copy",
    loading = french and "Propriétés en cours de lecture..." or "Reading its properties...",
}

-- ITEM_MOD_* (the server's stat types) to the client's own wording; ratings and powers are "Equip:" lines
local STATS = {
    [3] = "ITEM_MOD_AGILITY", [4] = "ITEM_MOD_STRENGTH", [5] = "ITEM_MOD_INTELLECT", [6] = "ITEM_MOD_SPIRIT",
    [7] = "ITEM_MOD_STAMINA",
}
local EQUIP_STATS = {
    [31] = "ITEM_MOD_HIT_RATING", [32] = "ITEM_MOD_CRIT_RATING", [36] = "ITEM_MOD_HASTE_RATING",
    [37] = "ITEM_MOD_EXPERTISE_RATING", [44] = "ITEM_MOD_ARMOR_PENETRATION_RATING", [45] = "ITEM_MOD_SPELL_POWER",
}

local copies = {}       -- seed -> the copy's rolls
local asked = {}        -- seed -> asked already

local function Percent(tenths)
    local text = format("%.1f %%", tenths / 10)
    return french and text:gsub("%.", ",") or text:gsub(" ", "")
end

-- The copy behind a link: its base item and its unique id (the 8th field)
local function SeedOf(link)
    if not link then
        return nil
    end
    local id, unique = link:match("item:(%d+):%-?%d+:%-?%d+:%-?%d+:%-?%d+:%-?%d+:%-?%d+:(%-?%d+)")
    id, unique = tonumber(id), tonumber(unique)
    if id and BASE_ITEMS[id] and unique and unique > 0 then
        return unique, id
    end
    if id and BASE_ITEMS[id] then
        return 0, id
    end
end

-- The stock lines the rolls go in front of, where the client puts an item's stats: durability, the level required,
-- the sell price
local function IsTailLine(text)
    if not text then
        return false
    end
    local function Starts(template)
        if not template then
            return false
        end
        local head = template:match("^([^%%]*)")
        return head ~= "" and text:sub(1, #head) == head
    end
    return Starts(DURABILITY_TEMPLATE) or Starts(ITEM_MIN_LEVEL) or Starts(SELL_PRICE)
end

-- The rolls (lines after `before`) moved up in front of the first tail line
local function PlaceRolls(tooltip, before)
    local layout = EvolutionsTooltip
    local name = tooltip:GetName()
    if not (layout and layout.Rearrange and name) then
        return
    end
    local tail
    for index = 2, before do
        local line = _G[name .. "TextLeft" .. index]
        if line and IsTailLine(line:GetText()) then
            tail = index
            break
        end
    end
    if not tail then
        return
    end
    local order = {}
    for index = 1, tail - 1 do
        order[#order + 1] = index
    end
    for index = before + 1, tooltip:NumLines() do
        order[#order + 1] = index
    end
    for index = tail, before do
        order[#order + 1] = index
    end
    layout.Rearrange(tooltip, order)
end

local function Write(tooltip, copy)
    local legendary = LEGENDARIES[copy.legendary]
    if not legendary then
        return
    end
    local layout = EvolutionsTooltip
    if layout and layout.SetSource then
        layout.SetSource(tooltip, legendary.source, 1, 0.5, 0)
    end
    local before = tooltip:NumLines()
    tooltip:AddLine(format(TEXT.itemLevel, copy.itemLevel), 1, 0.82, 0)
    if copy.armor > 0 then
        tooltip:AddLine(format(ARMOR_TEMPLATE, copy.armor), 1, 1, 1)
    end
    for _, stat in ipairs(copy.stats) do
        local name = STATS[stat.type]
        if name and _G[name] then
            tooltip:AddLine(format(_G[name], 43, stat.value), 1, 1, 1)
        end
    end
    for _, stat in ipairs(copy.stats) do
        local name = EQUIP_STATS[stat.type]
        if name and _G[name] then
            tooltip:AddLine(ITEM_SPELL_TRIGGER_ONEQUIP .. " " .. format(_G[name], stat.value), 0, 1, 0, true)
        end
    end
    tooltip:AddLine(ITEM_SPELL_TRIGGER_ONEQUIP .. " " .. format(legendary.power, Percent(copy.power)), 1, 0.5, 0, true)
    tooltip:AddLine(format(TEXT.window, Percent(copy.low), Percent(copy.high)), 0.6, 0.6, 0.6)
    tooltip:AddLine(legendary.lore, 1, 0.82, 0, true)
    PlaceRolls(tooltip, before)
end

-- Where a tooltip's item sits, as the server counts it: "bag:slot" (bag 255 the character's own slots: worn 0-18,
-- backpack 23-38, bank 39-66; bags 19-22, bank bags 67-73). Set by the setters below, which the tooltip's item
-- event fires inside of: they decorate it again once they know.
local function WhereOfContainer(bag, slot)
    if bag == 0 then
        return "255:" .. (23 + slot - 1)
    elseif bag == -1 then
        return "255:" .. (39 + slot - 1)
    elseif bag and bag >= 1 and bag <= 4 then
        return (19 + bag - 1) .. ":" .. (slot - 1)
    elseif bag and bag >= 5 and bag <= 11 then
        return (67 + bag - 5) .. ":" .. (slot - 1)
    end
end

local whereSeed = {}    -- "bag:slot" -> seed, until the bags change
local askedWhere = {}

local function Decorate(tooltip)
    if tooltip.legendaryDone then
        return
    end
    local _, link = tooltip:GetItem()
    local seed = SeedOf(link)
    if not seed then
        return
    end
    local where = tooltip.legendaryWhere
    -- A comparison tooltip shows what is worn: the equipped copy of that base item
    if seed == 0 and not where and (tooltip:GetName() or ""):find("^ShoppingTooltip") then
        local _, base = SeedOf(link)
        for slot = 1, 19 do
            local worn = GetInventoryItemLink("player", slot)
            if worn and tonumber(worn:match("item:(%d+)")) == base then
                where = "255:" .. (slot - 1)
                tooltip.legendaryWhere = where
                break
            end
        end
    end
    if seed == 0 and where then
        seed = whereSeed[where] or 0
    end
    if seed == 0 and not where then
        -- Not known yet: the setter's hook comes next, with where it sits
        return
    end
    tooltip.legendaryDone = true
    tooltip.legendarySeed = seed
    local copy = seed > 0 and copies[seed]
    if copy then
        Write(tooltip, copy)
    else
        tooltip:AddLine(TEXT.loading, 0.6, 0.6, 0.6)
        if seed > 0 and not asked[seed] then
            asked[seed] = true
            SendAddonMessage(PREFIX, "Q\t" .. seed, "WHISPER", UnitName("player"))
        elseif seed == 0 and where and not askedWhere[where] then
            askedWhere[where] = true
            local bag, slot = where:match("(%d+):(%d+)")
            SendAddonMessage(PREFIX, "W\t" .. bag .. "\t" .. slot, "WHISPER", UnitName("player"))
        end
    end
    tooltip:Show()
end

local function Clear(tooltip)
    tooltip.legendaryDone = nil
    tooltip.legendarySeed = nil
    tooltip.legendaryWhere = nil
    tooltip.legendarySetter = nil
end

local TOOLTIPS = { "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }
for _, name in ipairs(TOOLTIPS) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Decorate)
        tooltip:HookScript("OnTooltipCleared", Clear)
        hooksecurefunc(tooltip, "SetBagItem", function(self, bag, slot)
            self.legendaryWhere = WhereOfContainer(bag, slot)
            self.legendarySetter = { "SetBagItem", bag, slot }
            Decorate(self)
        end)
        hooksecurefunc(tooltip, "SetInventoryItem", function(self, unit, slot)
            if unit and UnitIsUnit(unit, "player") and slot then
                self.legendaryWhere = "255:" .. (slot - 1)
                self.legendarySetter = { "SetInventoryItem", unit, slot }
                Decorate(self)
            end
        end)
    end
end

-- The item frames' hidden scan tooltip (ItemFrames.lua): where its item sits, for LegendaryFrames.lua
local scan = _G.EvolutionsItemFrameScan
if scan then
    scan:HookScript("OnTooltipCleared", function(self) self.legendaryWhere = nil end)
    hooksecurefunc(scan, "SetBagItem", function(self, bag, slot) self.legendaryWhere = WhereOfContainer(bag, slot) end)
    hooksecurefunc(scan, "SetInventoryItem", function(self, unit, slot)
        if unit and UnitIsUnit(unit, "player") and slot then
            self.legendaryWhere = "255:" .. (slot - 1)
        end
    end)
end

-- A copy arrived while its tooltip waits: drawn again, by the setter that showed it
local function Redraw(seed, where)
    for _, name in ipairs(TOOLTIPS) do
        local tooltip = _G[name]
        if tooltip and tooltip:IsShown() and (tooltip.legendarySeed == seed or
            (where and tooltip.legendaryWhere == where)) then
            local setter = tooltip.legendarySetter
            local _, link = tooltip:GetItem()
            if setter then
                tooltip[setter[1]](tooltip, setter[2], setter[3])
            elseif link then
                tooltip:SetHyperlink(link)
            end
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("BAG_UPDATE")
listener:RegisterEvent("PLAYERBANKSLOTS_CHANGED")
listener:RegisterEvent("UNIT_INVENTORY_CHANGED")
listener:SetScript("OnEvent", function(_, event, prefix, message)
    if event ~= "CHAT_MSG_ADDON" then
        -- Items moved: where they sit is asked again
        wipe(whereSeed)
        wipe(askedWhere)
        return
    end
    if prefix ~= PREFIX or not message then
        return
    end
    local kind, seed, legendary, itemLevel, power, low, high, armor, stats, where = strsplit("\t", message)
    if kind ~= "C" then
        return
    end
    seed = tonumber(seed)
    if not seed then
        return
    end
    local copy = {
        legendary = tonumber(legendary) or 0, itemLevel = tonumber(itemLevel) or 0, power = tonumber(power) or 0,
        low = tonumber(low) or 0, high = tonumber(high) or 0, armor = tonumber(armor) or 0, stats = {},
    }
    for pair in (stats or ""):gmatch("[^,]+") do
        local statType, value = pair:match("(%d+)=(%-?%d+)")
        if statType then
            copy.stats[#copy.stats + 1] = { type = tonumber(statType), value = tonumber(value) }
        end
    end
    copies[seed] = copy
    if where and where ~= "" then
        whereSeed[where] = seed
    end
    Redraw(seed, where)
    if EvolutionsItemFrames then
        EvolutionsItemFrames.refresh()
    end
end)

-- For the item frames and other FrameXML: the copy behind a link, or at a place ("bag:slot"), if known; a place's
-- copy is asked for when it is not
EvolutionsLegendary = {
    CopyOf = function(link)
        local seed = SeedOf(link)
        return seed and copies[seed], seed
    end,
    CopyAt = function(where)
        local seed = where and whereSeed[where]
        if seed then
            return copies[seed]
        end
        if where and not askedWhere[where] then
            askedWhere[where] = true
            local bag, slot = where:match("(%d+):(%d+)")
            if bag then
                SendAddonMessage(PREFIX, "W\t" .. bag .. "\t" .. slot, "WHISPER", UnitName("player"))
            end
        end
    end,
}
