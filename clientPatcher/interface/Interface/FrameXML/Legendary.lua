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
    [2] = {
        item = 996,
        source = french and "Légendaire · Cathédrale écarlate, Mythique+" or "Legendary · Scarlet Cathedral, Mythic+",
        power = french
            and "Un coup fatal vous laisse à 1 point de vie, et le serment vous rend %s de votre vie en 4 sec. "
                .. "Une fois toutes les 3 min."
            or "A killing blow leaves you at 1 health instead, and the oath restores %s of your health over "
                .. "4 sec. Once every 3 min.",
        lore = french and "« Relève-toi, mon champion ! » Le serment de Whitemane ne laisse tomber personne."
            or "\"Arise, my champion!\" Whitemane's oath lets no one fall.",
    },
    [3] = {
        item = 21428,
        source = french and "Légendaire · Cathédrale écarlate, Mythique+" or "Legendary · Scarlet Cathedral, Mythic+",
        power = french
            and "Toutes les 10 sec en combat, le sol se consacre sous vos pieds pendant 6 sec : chaque seconde, il "
                .. "inflige aux ennemis qui s'y tiennent et rend aux alliés qui s'y tiennent %s de votre puissance "
                .. "d'attaque ou des sorts."
            or "Every 10 sec in combat, the ground beneath you is consecrated for 6 sec: every second it deals to "
                .. "enemies and heals allies standing in it for %s of your attack or spell power.",
        lore = french and "Là où le Commandant écarlate pose les poings, la terre devient sainte."
            or "Where the Scarlet Commander sets his fists, the ground turns holy.",
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

-- Long lines broken by hand at word boundaries: a wrapped tooltip line stretched the tooltip (and its painted
-- frame) to the line's whole length
local WRAP = 60

local function Wrapped(text)
    local lines, current = {}, ""
    for word in text:gmatch("%S+") do
        if current ~= "" and #current + 1 + #word > WRAP then
            lines[#lines + 1] = current
            current = word
        else
            current = current == "" and word or (current .. " " .. word)
        end
    end
    lines[#lines + 1] = current
    return table.concat(lines, "\n")
end

local function Write(tooltip, copy)
    local legendary = LEGENDARIES[copy.legendary]
    if not legendary then
        return
    end
    -- Added after the stock lines, never moved among them: the sell price's coins hang on their line and stayed
    -- behind when lines moved. The sell price itself goes last, after the rolls.
    local name = tooltip:GetName()
    local money = name and _G[name .. "MoneyFrame1"]
    local price = money and money:IsShown() and money.staticMoney
    if price and GameTooltip_ClearMoney then
        GameTooltip_ClearMoney(tooltip)
    end
    tooltip:AddLine(legendary.source, 1, 0.5, 0)
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
            tooltip:AddLine(Wrapped(ITEM_SPELL_TRIGGER_ONEQUIP .. " " .. format(_G[name], stat.value)), 0, 1, 0)
        end
    end
    tooltip:AddLine(Wrapped(ITEM_SPELL_TRIGGER_ONEQUIP .. " " .. format(legendary.power, Percent(copy.power))),
        1, 0.5, 0)
    tooltip:AddLine(format(TEXT.window, Percent(copy.low), Percent(copy.high)), 0.6, 0.6, 0.6)
    tooltip:AddLine(Wrapped(legendary.lore), 1, 0.82, 0)
    if price and SetTooltipMoney then
        SetTooltipMoney(tooltip, price, nil, format("%s:", SELL_PRICE))
    end
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
-- Kept apart from the tooltip: hiding it clears it before the frames read where it was. Reset as each scan starts
-- (ItemFrames.lua sets the scan's owner first).
local scanWhere
local scan = _G.EvolutionsItemFrameScan
if scan then
    hooksecurefunc(scan, "SetOwner", function() scanWhere = nil end)
    hooksecurefunc(scan, "SetBagItem", function(_, bag, slot) scanWhere = WhereOfContainer(bag, slot) end)
    hooksecurefunc(scan, "SetInventoryItem", function(_, unit, slot)
        if unit and UnitIsUnit(unit, "player") and slot then
            scanWhere = "255:" .. (slot - 1)
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
    ScanWhere = function()
        return scanWhere
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
