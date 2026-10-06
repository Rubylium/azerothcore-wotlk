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

local function Write(tooltip, copy)
    local legendary = LEGENDARIES[copy.legendary]
    if not legendary then
        return
    end
    local layout = EvolutionsTooltip
    if layout and layout.SetSource then
        layout.SetSource(tooltip, legendary.source, 1, 0.5, 0)
    end
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
end

local function Decorate(tooltip)
    if tooltip.legendaryDone then
        return
    end
    local _, link = tooltip:GetItem()
    local seed = SeedOf(link)
    if not seed then
        return
    end
    tooltip.legendaryDone = true
    tooltip.legendarySeed = seed
    local copy = copies[seed]
    if copy then
        Write(tooltip, copy)
    elseif seed > 0 then
        tooltip:AddLine(TEXT.loading, 0.6, 0.6, 0.6)
        if not asked[seed] then
            asked[seed] = true
            SendAddonMessage(PREFIX, "Q\t" .. seed, "WHISPER", UnitName("player"))
        end
    end
    tooltip:Show()
end

local function Clear(tooltip)
    tooltip.legendaryDone = nil
    tooltip.legendarySeed = nil
end

local TOOLTIPS = { "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }
for _, name in ipairs(TOOLTIPS) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Decorate)
        tooltip:HookScript("OnTooltipCleared", Clear)
    end
end

-- A copy arrived while its tooltip waits: drawn again
local function Redraw(seed)
    for _, name in ipairs(TOOLTIPS) do
        local tooltip = _G[name]
        if tooltip and tooltip:IsShown() and tooltip.legendarySeed == seed then
            local _, link = tooltip:GetItem()
            if name == "ItemRefTooltip" and link then
                tooltip:SetHyperlink(link)
            else
                local owner = tooltip:GetOwner()
                local enter = owner and owner:GetScript("OnEnter")
                if enter then
                    enter(owner)
                end
            end
        end
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message)
    if prefix ~= PREFIX or not message then
        return
    end
    local kind, seed, legendary, itemLevel, power, low, high, armor, stats = strsplit("\t", message)
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
    Redraw(seed)
end)

-- For the item frames and other FrameXML: the copy behind a link, if known
EvolutionsLegendary = {
    CopyOf = function(link)
        local seed = SeedOf(link)
        return seed and copies[seed], seed
    end,
}
