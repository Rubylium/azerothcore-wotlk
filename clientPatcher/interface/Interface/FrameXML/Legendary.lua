-- Legendary items (modules/mod-legendary, .agents/plans/legendary-items/legendary-items.DESIGN.md): every copy rolls
-- its own item level, power and stats, which its base item's record cannot carry. The server tells them, by the copy's
-- id - the item's property seed, which an item link carries as its unique id - and this writes them into the item's
-- tooltip, in the client's own wording:
--   LEGENDARY <tab> C <tab> seed <tab> legendary <tab> item level <tab> power x10 <tab> low x10 <tab> high x10 <tab>
--     armour <tab> type=value,...
-- A copy not known yet (a link, another player's, the loot) is asked for: LEGENDARY <tab> Q <tab> seed.

local PREFIX = "LEGENDARY"
local french = GetLocale() == "frFR"

-- Item quality 6, the stock "Artifact", is our Unique: a legendary above the others (the Hollow Voice's), red - the
-- client extension DLL recolours it (awesome_wotlk UniqueQuality.cpp)
ITEM_QUALITY6_DESC = "Unique"

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

-- The other dungeons' (mod-legendary Definitions 4-24), three each: { French, English } for every text
local function Add(id, item, dungeon, power, lore)
    LEGENDARIES[id] = {
        item = item,
        source = french and ("Légendaire · " .. dungeon[1] .. ", Mythique+")
            or ("Legendary · " .. dungeon[2] .. ", Mythic+"),
        power = french and power[1] or power[2],
        lore = french and lore[1] or lore[2],
    }
end

-- The Unique: quality 6, red (the client extension DLL recolours it), from a boss rather than a key
local function AddUnique(id, item, boss, power, lore)
    LEGENDARIES[id] = {
        item = item,
        unique = true,
        source = french and ("Unique · " .. boss[1]) or ("Unique · " .. boss[2]),
        power = french and power[1] or power[2],
        lore = french and lore[1] or lore[2],
    }
end

-- A boss's: legendary (not Unique), from its death
local function AddBoss(id, item, boss, power, lore)
    LEGENDARIES[id] = {
        item = item,
        source = french and ("Légendaire · " .. boss[1]) or ("Legendary · " .. boss[2]),
        power = french and power[1] or power[2],
        lore = french and lore[1] or lore[2],
    }
end

local MECHANAR = { "Le Méchanar", "The Mechanar" }
local UTGARDE = { "Donjon d'Utgarde", "Utgarde Keep" }
local SHATTERED_HALLS = { "Les Salles brisées", "The Shattered Halls" }
local DEADMINES = { "Les Mortemines", "The Deadmines" }
local DRAK_THARON = { "Donjon de Drak'Tharon", "Drak'Tharon Keep" }
local FORGE_OF_SOULS = { "La Forge des âmes", "The Forge of Souls" }
local HALLS_OF_LIGHTNING = { "Les salles de Foudre", "Halls of Lightning" }

Add(4, 21424, MECHANAR,
    { "Les coups de mêlée que vous subissez renvoient %s des dégâts à leur auteur, en dégâts des Arcanes.",
      "Melee blows you take strike back at their attacker for %s of the damage as Arcane damage." },
    { "« Polarité inversée. » Ce qui frappe Capacitus se frappe lui-même.",
      "\"Polarity shift.\" What strikes Capacitus strikes itself." })
Add(5, 1258, MECHANAR,
    { "Vos coups directs ont 15 %% de chances d'augmenter votre hâte de %s pendant 8 sec. Pas plus d'une fois "
        .. "toutes les 30 sec.",
      "Your direct hits have a 15%% chance to raise your haste by %s for 8 sec. No more than once every 30 sec." },
    { "Pathaleon a tout calculé. Même vous.", "Pathaleon has calculated everything. Even you." })
Add(6, 21432, MECHANAR,
    { "Vos sorts embrasent la cible : elle brûle pour %s des dégâts infligés, en dégâts de Feu sur 4 sec.",
      "Your spells set the target ablaze: it burns for %s of the damage dealt as Fire damage over 4 sec." },
    { "Les flammes déchaînées de la nethermancienne ne s'éteignent jamais tout à fait.",
      "The nethermancer's raging flames never quite go out." })

Add(7, 21425, UTGARDE,
    { "Tous les 5 coups directs, Ingvar lance sa hache d'ombre sur la cible : %s de votre puissance d'attaque "
        .. "ou des sorts en dégâts d'Ombre.",
      "Every 5th direct hit, Ingvar hurls his shadow axe at the target: %s of your attack or spell power as "
        .. "Shadow damage." },
    { "Le Pilleur tombe et se relève. Sa hache, elle, ne tombe jamais.",
      "The Plunderer falls and rises again. His axe never falls at all." })
Add(8, 21420, UTGARDE,
    { "Sous 35 %% de vie, vous subissez %s de dégâts en moins.",
      "Below 35%% health, you take %s less damage." },
    { "Le prince Keleseth enferme ce qu'il veut garder dans un tombeau de givre.",
      "Prince Keleseth keeps what he wants in a tomb of frost." })
Add(9, 26541, UTGARDE,
    { "Quand vous tuez un ennemi, vos dégâts augmentent de %s pendant 10 sec.",
      "When you kill an enemy, your damage is increased by %s for 10 sec." },
    { "Annhylde l'Appeleuse relève les morts. Elle exalte aussi ceux qui les font.",
      "Annhylde the Caller raises the dead. She also exalts those who make them." })

Add(10, 21437, SHATTERED_HALLS,
    { "Vos dégâts directs frappent aussi jusqu'à 4 autres ennemis à moins de 6 m de la cible pour %s "
        .. "des dégâts infligés.",
      "Your direct damage also strikes up to 4 other enemies within 6 yd of the target for %s of the damage "
        .. "dealt." },
    { "Kargath n'a plus de mains. Il a mieux.", "Kargath has no hands left. He has something better." })
Add(11, 21433, SHATTERED_HALLS,
    { "Vos dégâts périodiques sont augmentés de %s.", "Your damage over time is increased by %s." },
    { "Le rituel de Nethekurse fait durer la douleur.", "Nethekurse's ritual makes the pain last." })
Add(12, 5828, SHATTERED_HALLS,
    { "%s de vos dégâts directs vous soignent.", "%s of your direct damage heals you." },
    { "La garde de sang de Porung ne boit jamais assez.", "Porung's blood guard never drinks enough." })

Add(13, 21421, DEADMINES,
    { "Vos dégâts sont augmentés de %s contre les ennemis sous 35 %% de vie.",
      "Your damage is increased by %s against enemies below 35%% health." },
    { "« Personne ne quitte la Confrérie. » VanCleef finit toujours ce qu'il commence.",
      "\"No one leaves the Brotherhood.\" VanCleef always finishes what he starts." })
Add(14, 21429, DEADMINES,
    { "Les ennemis que vous tuez explosent : %s de votre puissance d'attaque ou des sorts en dégâts de Feu aux "
        .. "ennemis à moins de 8 m.",
      "Enemies you kill explode: %s of your attack or spell power as Fire damage to enemies within 8 yd." },
    { "Gilnid ne compte plus ses doigts. Il compte ses barils.",
      "Gilnid no longer counts his fingers. He counts his barrels." })
Add(15, 21444, DEADMINES,
    { "Toutes les 5 sec en combat, l'allié le plus blessé à moins de 40 m est soigné pour %s de votre puissance "
        .. "d'attaque ou des sorts.",
      "Every 5 sec in combat, the most injured ally within 40 yd is healed for %s of your attack or spell power." },
    { "Cookie ne laisse personne repartir le ventre vide.", "Cookie lets no one leave on an empty stomach." })

Add(16, 18161, DRAK_THARON,
    { "Vos coups d'arme font saigner la cible : %s des dégâts infligés en dégâts physiques sur 6 sec.",
      "Your weapon blows make the target bleed for %s of the damage dealt as Physical damage over 6 sec." },
    { "Le roi Dred n'a jamais lâché une proie.", "King Dred has never let go of his prey." })
Add(17, 21430, DRAK_THARON,
    { "%s des soins directs en excès que vous prodiguez protègent la cible pendant 10 sec, jusqu'à 20 %% de sa vie.",
      "%s of the overhealing of your direct heals shields the target for 10 sec, up to 20%% of its health." },
    { "La barrière de Novos tient tant qu'il reste quelqu'un à protéger.",
      "Novos' barrier holds as long as someone is left to protect." })
Add(18, 27218, DRAK_THARON,
    { "Vos soins directs soignent aussi l'autre allié le plus blessé à moins de 40 m pour %s du soin.",
      "Your direct heals also heal the most injured other ally within 40 yd for %s of the heal." },
    { "Tharon'ja a vu la chair revenir aux os. Il sait la rendre.",
      "Tharon'ja has seen flesh return to the bone. He knows how to give it back." })

Add(19, 21423, FORGE_OF_SOULS,
    { "Toutes les 10 sec en combat, un puits des âmes s'ouvre sous vos pieds pendant 6 sec : chaque seconde, il "
        .. "inflige aux ennemis qui s'y tiennent %s de votre puissance d'attaque ou des sorts en dégâts d'Ombre.",
      "Every 10 sec in combat, a well of souls opens beneath you for 6 sec: every second it deals %s of your "
        .. "attack or spell power as Shadow damage to enemies standing in it." },
    { "Le Dévoreur d'âmes a faim. Il a toujours faim.", "The Devourer of Souls is hungry. Always hungry." })
Add(20, 21434, FORGE_OF_SOULS,
    { "Quand vous tuez un ennemi, son âme vous rend %s de votre vie en 4 sec.",
      "When you kill an enemy, its soul restores %s of your health over 4 sec." },
    { "Bronjahm façonne les âmes. Celle-ci vous appartient.",
      "Bronjahm shapes souls. This one belongs to you." })
Add(21, 6673, FORGE_OF_SOULS,
    { "%s de vos dégâts directs frappent aussi l'ennemi le plus proche de la cible, à moins de 10 m.",
      "%s of your direct damage also strikes the enemy nearest the target, within 10 yd." },
    { "Ce que vous infligez, votre reflet l'inflige aussi.", "What you inflict, your reflection inflicts too." })

Add(22, 8688, HALLS_OF_LIGHTNING,
    { "Vos dégâts directs bondissent vers jusqu'à 3 ennemis proches de la cible, à moins de 10 m, pour %s des "
        .. "dégâts infligés. Pas plus d'une fois toutes les 2 sec.",
      "Your direct damage leaps to up to 3 enemies near the target, within 10 yd, for %s of the damage dealt. "
        .. "No more than once every 2 sec." },
    { "Ionar n'est jamais tout à fait parti. Une étincelle suffit.",
      "Ionar is never quite gone. A spark is enough." })
Add(23, 21450, HALLS_OF_LIGHTNING,
    { "Toutes les 6 sec en combat, une nova de foudre inflige aux ennemis à moins de 10 m %s de votre puissance "
        .. "d'attaque ou des sorts en dégâts de Nature.",
      "Every 6 sec in combat, a lightning nova deals %s of your attack or spell power as Nature damage to "
        .. "enemies within 10 yd." },
    { "Loken frappe le sol, et les salles tremblent.", "Loken strikes the ground, and the halls shake." })
Add(24, 6674, HALLS_OF_LIGHTNING,
    { "Quand un coup vous fait passer sous 50 %% de vie, un rempart absorbe des dégâts à hauteur de %s de votre "
        .. "vie pendant 10 sec. Une fois par minute.",
      "When a blow takes you below 50%% health, a bulwark absorbs damage equal to %s of your health for 10 sec. "
        .. "Once per minute." },
    { "Le général Bjarngrim change de posture. Jamais de camp.",
      "General Bjarngrim changes his stance. Never his side." })

AddUnique(25, 10555, { "La Voix creuse", "The Hollow Voice" },
    { "Quand vous utilisez une technique dont le temps de recharge est d'au moins 20 sec, la Voix lui fait écho : "
        .. "le temps de recharge restant de vos autres techniques est réduit de %s.",
      "When you use an ability with a cooldown of 20 sec or more, the Voice echoes it: the remaining cooldown of "
        .. "your other abilities is reduced by %s." },
    { "« Tu m'as entendu, n'est-ce pas ? » Vel'thazar ne s'est jamais tu. Il a seulement changé de maître.",
      "\"You heard me, didn't you?\" Vel'thazar never fell silent. He only changed masters." })

AddBoss(26, 16067, { "L'Infini, Défi", "The Infinite, Challenge" },
    { "%s de vos dégâts et de vos soins nourrissent une étoile captive. Toutes les 20 sec en combat, elle "
        .. "s'effondre : ses dégâts se partagent entre votre cible et les ennemis à moins de 8 m d'elle, ses soins "
        .. "entre les 5 alliés les plus blessés à moins de 40 m.",
      "%s of your damage and healing feeds a captive star. Every 20 sec in combat it collapses: its damage is "
        .. "shared by your target and the enemies within 8 yd of it, its healing by the 5 most injured allies "
        .. "within 40 yd." },
    { "L'Infini tenait les étoiles dans sa main avant que le premier mortel ne marche. Celle-ci, il l'a laissée "
        .. "tomber.",
      "The Infinite held the stars in its hand before the first mortal walked. This one, it let fall." })

AddUnique(27, 17855, { "Gardien-chef Vorhan", "Head Warden Vorhan" },
    { "Toutes les 4 sec en combat, votre cible reçoit une sentence : une part des dégâts que vous lui avez infligés "
        .. "depuis la précédente, plus lourde à chaque sentence sur la même cible, jusqu'à %s à la cinquième. "
        .. "Changer de cible remet la peine à zéro.",
      "Every 4 sec in combat, your target is sentenced: a share of the damage you dealt it since the last "
        .. "sentence, heavier with each sentence on the same target, up to %s from the fifth. Changing target "
        .. "starts the sentence over." },
    { "À la Geôle, on ne retourne jamais le sablier. Chaque grain tombé alourdit la peine.",
      "In the Gaol, the hourglass is never turned. Every grain that falls makes the sentence heavier." })

AddUnique(28, 17858, { "Le Traqueur d'évadés", "The Escape-Hunter" },
    { "L'ennemi que vous frappez devient votre proie (votre cible d'abord) : vous et votre groupe lui infligez %s "
        .. "de dégâts en plus tant que sa marque dure.",
      "The enemy you strike becomes your quarry (your target first): you and your group deal %s more damage to it "
        .. "while its mark lasts." },
    { "Un croc pour chaque évadé repris. Le Traqueur n'a jamais manqué de place sur le cordon.",
      "A fang for every escapee brought back. The Hunter never ran out of room on the cord." })

-- Gardien-chef Vorhan's sets are no legendaries: generated items as the raid's (mod-legendary SetPieces.cpp), their
-- stats in their own record and their set named by MythicItemTag.lua

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
    [37] = "ITEM_MOD_EXPERTISE_RATING", [38] = "ITEM_MOD_ATTACK_POWER", [44] = "ITEM_MOD_ARMOR_PENETRATION_RATING",
    [45] = "ITEM_MOD_SPELL_POWER",
}

-- The same stats as GetItemStats names them (its keys, the comparison's): a copy's rolls compared with any item
local STAT_KEYS = {
    [3] = "ITEM_MOD_AGILITY_SHORT", [4] = "ITEM_MOD_STRENGTH_SHORT", [5] = "ITEM_MOD_INTELLECT_SHORT",
    [6] = "ITEM_MOD_SPIRIT_SHORT", [7] = "ITEM_MOD_STAMINA_SHORT", [31] = "ITEM_MOD_HIT_RATING_SHORT",
    [32] = "ITEM_MOD_CRIT_RATING_SHORT", [36] = "ITEM_MOD_HASTE_RATING_SHORT",
    [37] = "ITEM_MOD_EXPERTISE_RATING_SHORT", [38] = "ITEM_MOD_ATTACK_POWER_SHORT",
    [44] = "ITEM_MOD_ARMOR_PENETRATION_RATING_SHORT", [45] = "ITEM_MOD_SPELL_POWER_SHORT",
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
    -- The client's own item level line is the base item's (every copy shares its template): it says the copy's
    -- instead, in place. Without one, the copy's goes with the rolls.
    local stockLevel = false
    if name and ITEM_LEVEL then
        local _, _, _, baseLevel = GetItemInfo(legendary.item)
        local stock = baseLevel and format(ITEM_LEVEL, baseLevel)
        for index = 2, tooltip:NumLines() do
            local line = _G[name .. "TextLeft" .. index]
            if stock and line and line:GetText() == stock then
                line:SetText(format(ITEM_LEVEL, copy.itemLevel))
                stockLevel = true
                break
            end
        end
    end
    if legendary.unique then
        tooltip:AddLine(legendary.source, 0.91, 0.2, 0.17)
    else
        tooltip:AddLine(legendary.source, 1, 0.5, 0)
    end
    if not stockLevel then
        tooltip:AddLine(format(TEXT.itemLevel, copy.itemLevel), 1, 0.82, 0)
    end
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
    if legendary.power then
        tooltip:AddLine(Wrapped(ITEM_SPELL_TRIGGER_ONEQUIP .. " " .. format(legendary.power, Percent(copy.power))),
            1, 0.5, 0)
        tooltip:AddLine(format(TEXT.window, Percent(copy.low), Percent(copy.high)), 0.6, 0.6, 0.6)
    end
    if legendary.lore then
        tooltip:AddLine(Wrapped(legendary.lore), 1, 0.82, 0)
    end
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
    if seed == 0 and not where and (tooltip:GetName() or ""):find("ShoppingTooltip%d$") then
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

-- The comparison ("If you replace this item...") is the client's own, from the two items' records: a legendary's base
-- item has no stats (every copy rolls its own), so a copy compared there counted as nothing. Once the client has
-- written it, it is written again from the copies' rolls: what the hovered item would change, worn in place of the
-- compared one.
local function StatsOf(tooltip)
    local _, link = tooltip:GetItem()
    if not link then
        return nil
    end
    local copy = tooltip.legendarySeed and copies[tooltip.legendarySeed]
    if not copy then
        local _, base = SeedOf(link)
        if base then
            return nil      -- a legendary whose rolls are not known yet: nothing honest to compare
        end
        return GetItemStats(link) or {}
    end
    local stats = GetItemStats(link) or {}
    for _, stat in ipairs(copy.stats) do
        local key = STAT_KEYS[stat.type]
        if key then
            stats[key] = (stats[key] or 0) + stat.value
        end
    end
    if copy.armor > 0 then
        stats.RESISTANCE0_NAME = copy.armor
    end
    return stats
end

local function RewriteComparison(hovered, shopping)
    if not hovered or not shopping or not shopping:IsShown() or not ITEM_DELTA_DESCRIPTION then
        return
    end
    -- Only where a legendary copy is one of the two: the client's comparison is right for every other item
    if not shopping.legendarySeed and not hovered.legendarySeed then
        return
    end
    local new, worn = StatsOf(hovered), StatsOf(shopping)
    if not new or not worn then
        return
    end
    local name = shopping:GetName()
    local header
    for index = 2, shopping:NumLines() do
        local line = _G[name .. "TextLeft" .. index]
        if line and line:GetText() == ITEM_DELTA_DESCRIPTION then
            header = index
            break
        end
    end

    local changes = {}
    for key, value in pairs(new) do
        local delta = value - (worn[key] or 0)
        if delta ~= 0 and _G[key] then
            changes[#changes + 1] = { key = key, delta = delta }
        end
    end
    for key, value in pairs(worn) do
        if new[key] == nil and value ~= 0 and _G[key] then
            changes[#changes + 1] = { key = key, delta = -value }
        end
    end
    if #changes == 0 and not header then
        return
    end
    sort(changes, function(a, b)
        if a.delta ~= b.delta then
            return a.delta > b.delta
        end
        return a.key < b.key
    end)

    -- No comparison from the client: its header, after a blank line, as it writes it
    if not header then
        shopping:AddLine(" ")
        shopping:AddLine(ITEM_DELTA_DESCRIPTION, 1, 0.82, 0, true)
        header = shopping:NumLines()
    end
    local index = header
    for _, change in ipairs(changes) do
        index = index + 1
        local text = format("%s%d %s", change.delta > 0 and "+" or "", change.delta, _G[change.key])
        local red, green, blue = 0, 1, 0
        if change.delta < 0 then
            red, green, blue = 1, 0.13, 0.13
        end
        local line = _G[name .. "TextLeft" .. index]
        if line and index <= shopping:NumLines() then
            line:SetText(text)
            line:SetTextColor(red, green, blue)
        else
            shopping:AddLine(text, red, green, blue)
        end
    end
    -- The client's own lines past ours are spent
    for rest = index + 1, shopping:NumLines() do
        local line = _G[name .. "TextLeft" .. rest]
        if line then
            line:SetText(nil)
        end
    end
    shopping:Show()
end

-- The client's comparison is GameTooltip_ShowCompareItem (FrameXML): its comparison tooltips are set, decorated and
-- given their stat changes inside it, and it runs again and again while the compare key is held. Ours is written as
-- it returns, every time: never a frame of the client's.
local function ShoppingTooltipsOf(tooltip)
    if tooltip and tooltip.shoppingTooltips then
        return tooltip.shoppingTooltips
    end
    if tooltip == ItemRefTooltip then
        return { ItemRefShoppingTooltip1, ItemRefShoppingTooltip2, ItemRefShoppingTooltip3 }
    end
    return { ShoppingTooltip1, ShoppingTooltip2, ShoppingTooltip3 }
end

if GameTooltip_ShowCompareItem then
    hooksecurefunc("GameTooltip_ShowCompareItem", function(self)
        local hovered = self or GameTooltip
        for _, shopping in ipairs(ShoppingTooltipsOf(hovered)) do
            RewriteComparison(hovered, shopping)
        end
    end)
end

local TOOLTIPS = { "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3",
    "ItemRefShoppingTooltip1", "ItemRefShoppingTooltip2", "ItemRefShoppingTooltip3" }
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
    local compare = {}
    for _, name in ipairs(TOOLTIPS) do
        local tooltip = _G[name]
        if tooltip and tooltip:IsShown() and (tooltip.legendarySeed == seed or
            (where and tooltip.legendaryWhere == where)) then
            local setter = tooltip.legendarySetter
            local _, link = tooltip:GetItem()
            if name:find("ShoppingTooltip%d$") then
                -- A comparison tooltip is drawn again by its comparison (a link alone would lose "equipped")
                compare[name:find("^ItemRef") and ItemRefTooltip or GameTooltip] = true
            elseif setter then
                tooltip[setter[1]](tooltip, setter[2], setter[3])
            elseif link then
                tooltip:SetHyperlink(link)
            end
        end
    end
    for owner in pairs(compare) do
        if owner:IsShown() and GameTooltip_ShowCompareItem then
            GameTooltip_ShowCompareItem(owner)
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
    for _, callback in ipairs(EvolutionsLegendary.listeners) do
        callback()
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
    listeners = {},
}

-- Item levels for the interface's own item level texts (DragonUI's itemlevel module, clientPatcher/addons): a copy's
-- level is its roll, not its base item's - every copy shares that one item id, so GetItemInfo can never say it. A
-- copy is known by where it sits; nil when the link is no legendary, or the copy is not known yet (asked for, and
-- the listeners called once it arrives).
function EvolutionsLegendary.LevelAt(link, where)
    local id = link and tonumber(link:match("item:(%d+)"))
    if not id or not BASE_ITEMS[id] or not where then
        return nil
    end
    local copy = EvolutionsLegendary.CopyAt(where)
    return copy and copy.itemLevel
end

-- Where an item button's item sits, as the server counts it: the bags', the bank's, the character sheet's, and
-- DragonUI's own bag buttons (which know their bag)
function EvolutionsLegendary.WhereOfButton(button)
    local name = button and button.GetName and button:GetName() or ""
    if button and button.GetBag and button.GetID and not (button.IsCached and button:IsCached()) then
        return WhereOfContainer(button:GetBag(), button:GetID())
    elseif name:match("^ContainerFrame%d+Item%d+$") then
        return WhereOfContainer(button:GetParent():GetID(), button:GetID())
    elseif name:match("^BankFrameItem%d+$") then
        return WhereOfContainer(-1, button:GetID())
    elseif name:match("^Character.+Slot$") then
        return "255:" .. (button:GetID() - 1)
    end
end

-- Called (no arguments) whenever a copy's rolls arrive: what shows them can draw them again
function EvolutionsLegendary.OnCopy(callback)
    tinsert(EvolutionsLegendary.listeners, callback)
end
