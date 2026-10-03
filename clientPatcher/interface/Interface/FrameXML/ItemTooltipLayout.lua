-- The layout of our items' tooltips, so they read as WoW's own: what the server's systems add is put where the client
-- would put such a line, not wherever it happened to be added.
--
--   Name
--   <source>                 La Voix creuse / L'Infini / Mythique+ / Forgé x/8, where the client writes "Heroic"
--   Lié, slot, armour, stats, sockets, durability, requirements
--   Équipé : ...             the item's own equip and use lines
--   +69 Puissance ...        its gear bonuses (the Gear Bonuses addon), green, with them
--
--   <bonus name>             a boss's touch (L'Infini, the Hollow Voice): five enchantment lines the client draws
--   Équipé : ...             right under the stats, moved here as one block, apart
--   « lore »
--   <Maj clic-droit pour sertir>, Prix de vente
--
-- The tooltip's lines are rewritten in place, never rebuilt: a line moved keeps its font, colours and what is
-- anchored to it (a socket's icon, the sell price's coins).

EvolutionsTooltip = EvolutionsTooltip or {}
local api = EvolutionsTooltip

-- A boss's touch on an item: the name each bonus's first enchantment line carries (localTools/patchSinisterStrike.ps1)
api.touches = {
    { source = "infini", names = { "Égide des astres", "Éclat d'étoile filante", "Étincelle d'éternité" } },
    { source = "voice", names = { "Ailes du Séraphin", "Égide d'Aldric", "Murmure de Vel'thazar" } },
}
api.TOUCH_LINES = 5

local function Line(tooltip, side, index)
    return _G[tooltip:GetName() .. "Text" .. side .. index]
end

local function StartsWith(text, template)
    if not text or not template then
        return false
    end
    local marker = template:find("%", 1, true)
    local prefix = marker and template:sub(1, marker - 1) or template
    return prefix ~= "" and text:sub(1, #prefix) == prefix
end

-- The item's own "Équipé :", "Utiliser :" and "Chances quand vous touchez :" lines; a touch's carry their colour codes
function api.IsOwnEffectLine(text)
    return text ~= nil and not text:find("^|c") and (StartsWith(text, ITEM_SPELL_TRIGGER_ONEQUIP)
        or StartsWith(text, ITEM_SPELL_TRIGGER_ONUSE) or StartsWith(text, ITEM_SPELL_TRIGGER_ONPROC))
end

-- A gear bonus line (the Gear Bonuses addon): "+69 Puissance d'attaque", "+11% Points de vie maximum"
function api.IsGearBonusLine(text)
    return text ~= nil and text:find("^%+%d+%%? ") ~= nil
end

-- The first line of a boss's touch, and which: nil when the item has none
function api.FindTouch(tooltip)
    for index = 2, tooltip:NumLines() do
        local text = Line(tooltip, "Left", index):GetText()
        if text then
            for _, touch in ipairs(api.touches) do
                for _, name in ipairs(touch.names) do
                    if text:find(name, 1, true) then
                        return index, touch.source
                    end
                end
            end
        end
    end
end

-- Rewrites the tooltip so that its line k shows what line order[k] showed (0: a blank line), moving what is anchored
-- to the lines with them
function api.Rearrange(tooltip, order)
    local count = tooltip:NumLines()
    while tooltip:NumLines() < #order do
        tooltip:AddLine(" ")
    end

    local saved = {}
    for index = 1, count do
        saved[index] = {}
        for _, side in ipairs({ "Left", "Right" }) do
            local line = Line(tooltip, side, index)
            if line then
                local r, g, b = line:GetTextColor()
                saved[index][side] = { text = line:GetText(), font = line:GetFontObject(), shown = line:IsShown(),
                    r = r, g = g, b = b }
            end
        end
    end

    -- What hangs on a line: the sell price's coins, a socket's icon
    local anchored = {}
    local name = tooltip:GetName()
    local regions = {}
    for index = 1, tooltip.numMoneyFrames or 0 do
        regions[#regions + 1] = _G[name .. "MoneyFrame" .. index]
    end
    for index = 1, 10 do
        regions[#regions + 1] = _G[name .. "Texture" .. index]
    end
    for _, region in ipairs(regions) do
        if region and region:IsShown() and region:GetNumPoints() > 0 then
            local point, relativeTo, relativePoint, x, y = region:GetPoint(1)
            local relativeName = relativeTo and relativeTo.GetName and relativeTo:GetName()
            local side, index = relativeName and relativeName:match("Text(%a+)(%d+)$")
            if index then
                anchored[#anchored + 1] = { region = region, point = point, relativePoint = relativePoint, x = x,
                    y = y, side = side, old = tonumber(index) }
            end
        end
    end

    local newIndexOf = {}
    for index, old in ipairs(order) do
        if old > 0 then
            newIndexOf[old] = index
        end
        for _, side in ipairs({ "Left", "Right" }) do
            local line = Line(tooltip, side, index)
            local from = old > 0 and saved[old] and saved[old][side]
            if line then
                if from then
                    if from.font then
                        line:SetFontObject(from.font)
                    end
                    line:SetText(from.text)
                    line:SetTextColor(from.r, from.g, from.b)
                    if from.shown then line:Show() else line:Hide() end
                elseif side == "Left" then
                    line:SetFontObject(GameTooltipText)
                    line:SetText(" ")
                    line:Show()
                else
                    line:SetText(nil)
                    line:Hide()
                end
            end
        end
    end

    for _, entry in ipairs(anchored) do
        local target = newIndexOf[entry.old] and Line(tooltip, entry.side, newIndexOf[entry.old])
        if target then
            entry.region:ClearAllPoints()
            entry.region:SetPoint(entry.point, target, entry.relativePoint, entry.x, entry.y)
        end
    end
    tooltip:Show()
end

-- The source, on the second line: in place of the client's "Heroic" (a flag the generated items inherit from the item
-- they are made from), or slipped in there
function api.SetSource(tooltip, text, r, g, b)
    local second = Line(tooltip, "Left", 2)
    if not second then
        return
    end
    if second:GetText() == ITEM_HEROIC or second:GetText() == text then
        second:SetText(text)
        second:SetTextColor(r, g, b)
        return
    end
    local order = { 1, 0 }
    for index = 2, tooltip:NumLines() do
        order[#order + 1] = index
    end
    api.Rearrange(tooltip, order)
    second:SetText(text)
    second:SetTextColor(r, g, b)
end

-- A boss's touch moved under the item's own effects and gear bonuses, a blank line before it
function api.PlaceTouch(tooltip)
    local first = api.FindTouch(tooltip)
    if not first or tooltip.evolutionsTouchPlaced then
        return
    end
    local count = tooltip:NumLines()
    local last = math.min(first + api.TOUCH_LINES - 1, count)

    -- After the last of the item's own effect lines and the gear bonuses after them; with none, before the socketing
    -- hint and the sell price
    local after
    for index = 2, count do
        if index < first or index > last then
            local text = Line(tooltip, "Left", index):GetText()
            if api.IsOwnEffectLine(text) or (after and index == after + 1 and api.IsGearBonusLine(text)) then
                after = index
            end
        end
    end
    if not after then
        for index = count, 2, -1 do
            if index < first or index > last then
                after = index
                break
            end
        end
        for index = 2, count do
            local text = Line(tooltip, "Left", index):GetText()
            if (index < first or index > last) and (StartsWith(text, ITEM_SOCKETABLE) or StartsWith(text, SELL_PRICE)) then
                after = index - 1
                break
            end
        end
        -- Right after the block's own place: it stays there
        if after and after >= first and after <= last then
            after = first - 1
        end
    end
    if not after or after < 1 then
        return
    end

    local order = {}
    for index = 1, count do
        if index < first or index > last then
            order[#order + 1] = index
            if index == after then
                order[#order + 1] = 0
                for moved = first, last do
                    order[#order + 1] = moved
                end
            end
        end
    end
    tooltip.evolutionsTouchPlaced = true
    api.Rearrange(tooltip, order)
end

for _, name in ipairs({ "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipCleared", function(self)
            self.evolutionsTouchPlaced = nil
        end)
    end
end
