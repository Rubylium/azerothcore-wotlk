-- The retail-style talent window (Dragonflight's class talents) for the custom classes that have talent trees
-- (server: mod-custom-classes TalentTree.cpp, protocol at the top of it). The Oathblade first; every other class
-- keeps the WotLK talent window, which ToggleTalentFrame still opens for it.
--
-- A class with several specializations (the Pestiféré: a tank and a healer) also gets a specialization page, a tab
-- under the window: the class tree always shows on the left, and the right shows the spec tree of the specialization
-- in view. Choosing one is the server's SPEC message; each talent slot keeps its own.
--
-- The trees are TalentTreeData.lua and the art TalentTreeArt.lua, both generated. A build is edited here and only
-- sent when applied: the server checks the whole of it and answers with the state it now holds, so what is drawn
-- after an apply is always the server's. The rules below mirror TalentTree.cpp's so the window never offers a click
-- the server would refuse; they decide nothing on their own.

local PREFIX = "TalentTree"
local ART = TalentTreeArt
local GRID = ART.grid
local FRIZ = "Fonts\\FRIZQT__.TTF"
-- Retail's rank digits: a narrow sans, soft-shadowed. The 3.3.5 client has Arial Narrow.
local NARROW = "Fonts\\ARIALN.TTF"
local ROUND_CLASSES = "Interface\\Glues\\CharacterCreate\\RoundClasses"

-- Retail's proportions: 36-unit nodes on a 55-unit grid, the trees from 155 units down, a wide gap between them
-- left for Apex talents
local WIDTH, HEIGHT = 1489, 806
-- From the window's side to the centre of a tree's outer column, and from its top to the centre of row 0
local SIDE, TOP_ROW = 220, -155
local COLUMNS = 7
-- Retail's bottom bar is 82 texels tall for 1612 wide: kept in that shape, or its gradient smears
local BAR_HEIGHT = 76
-- The window fits the screen, then is drawn this much smaller: a panel, not the whole screen
local WINDOW_SCALE = 0.78
local SCALE = GRID.scale
local NODE, ICON = 40 * SCALE, 32 * SCALE
-- The retail frames are drawn at these fractions of their texel size, which gives each kind a 30-unit opening
local FRAME_SCALE = { passive = 0.8 * SCALE, active = 0.5 * SCALE, choice = 0.5 * SCALE }
local SHAPE = { passive = "circle", active = "square", choice = "choice" }
-- Where the link art's arrowhead stops, by the kind of node it points at (buildTalentTreeArt.py draws them)
local LINK_REACH = GRID.reach

local GOLD = { 1, 0.82, 0.24 }
local GREEN = { 0.3, 1, 0.35 }
local BLUE = { 0.35, 0.7, 1 }
local RED = { 1, 0.25, 0.2 }

local SOUND_OPEN, SOUND_CLOSE = "TalentScreenOpen", "TalentScreenClose"
local SOUND_LEARN, SOUND_UNLEARN = "Glyph_MinorCreate", "Glyph_MinorDestroy"
local SOUND_DENIED, SOUND_HOVER = "igQuestFailed", "GAMESCREENSMALLBUTTONMOUSEOVER"
local SOUND_APPLY, SOUND_CHOICE = "Glyph_MajorCreate", "igMainMenuOptionCheckBoxOn"

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Talents",
    points = "POINTS DISPONIBLES : %s",
    nextPoint = "Prochain point au niveau %d",
    apply = "Appliquer",
    undo = "Annuler les modifications",
    undoHint = "Revient aux talents que vous avez actuellement.",
    reset = "Réinitialiser les talents",
    resetHint = "Rend tous les points. Rien ne change avant d'appliquer.",
    search = "Rechercher",
    spec = "Spécialisation %d",
    activate = "Activer",
    viewing = "Vous consultez votre autre spécialisation : activez-la pour la modifier.",
    rank = "Rang %d/%d",
    passive = "Talent passif",
    active = "Technique active",
    choice = "Choix : un seul des deux",
    nextRank = "Rang suivant :",
    learn = "Clic gauche : apprendre",
    unlearn = "Clic droit : désapprendre",
    choose = "Clic : choisir",
    gate = "Dépensez encore %d |4point:points; dans %s pour débloquer cette rangée.",
    requiresLevel = "Niveau %d requis",
    requiresParent = "Requiert un talent précédent au rang maximum",
    noPoints = "Plus aucun point %s",
    applied = "Talents appliqués",
    combat = "Impossible de changer vos talents en combat.",
    outdated = "Les talents de votre client ne sont plus à jour : relancez le launcher.",
    cantRefund = "D'autres talents en dépendent.",
    share = "Code de talents",
    shareHint = "Copiez ce code pour partager vos talents, ou collez-en un et appuyez sur Entrée.",
    badCode = "Ce code ne correspond pas à ces talents.",
    waiting = "Chargement de vos talents…",
    noServer = "Le serveur ne répond pas pour ces talents : il n'est pas encore à jour.",
    specTab = "Spécialisation",
    talentsTab = "Talents",
    activeSpec = "Spécialisation active",
    activateSpec = "Activer",
    specChanged = "Spécialisation : %s",
    specHint = "Vos talents de chaque spécialisation sont conservés : les retrouver ne coûte rien.",
    roles = { tank = "Tank", healer = "Soigneur", damage = "Dégâts" },
    errors = {
        [1] = "Ces talents n'ont pas pu être lus : réessayez.",
        [2] = "Impossible de changer vos talents en combat.",
        [3] = "Un talent requiert le talent précédent au rang maximum.",
        [4] = "Pas assez de points dépensés pour débloquer cette rangée.",
        [5] = "Pas assez de points de talent.",
        [6] = "Votre niveau est trop bas pour ce talent.",
        [7] = "Votre spécialisation a changé entre-temps.",
        [8] = "Les talents ne sont pas disponibles.",
        [9] = "Cette spécialisation n'existe pas.",
    },
} or {
    title = "Talents",
    points = "%s POINTS AVAILABLE",
    nextPoint = "Next point at level %d",
    apply = "Apply Changes",
    undo = "Undo changes",
    undoHint = "Goes back to the talents you have now.",
    reset = "Reset talents",
    resetHint = "Takes back every point. Nothing changes until you apply.",
    search = "Search",
    spec = "Specialization %d",
    activate = "Activate",
    viewing = "You are viewing your other specialization: activate it to change it.",
    rank = "Rank %d/%d",
    passive = "Passive talent",
    active = "Active ability",
    choice = "Choice: pick one of two",
    nextRank = "Next rank:",
    learn = "Left-click to learn",
    unlearn = "Right-click to unlearn",
    choose = "Click to choose",
    gate = "Spend %d more |4point:points; in %s to unlock this row.",
    requiresLevel = "Requires level %d",
    requiresParent = "Requires a fully learned talent above it",
    noPoints = "No %s points left",
    applied = "Talents applied",
    combat = "You can't change your talents in combat.",
    outdated = "Your client's talents are out of date: restart the launcher.",
    cantRefund = "Other talents depend on this one.",
    share = "Talent code",
    shareHint = "Copy this code to share your talents, or paste one and press Enter.",
    badCode = "That code does not fit these talents.",
    waiting = "Loading your talents…",
    noServer = "The server does not answer for these talents: it is not updated yet.",
    specTab = "Specialization",
    talentsTab = "Talents",
    activeSpec = "Active specialization",
    activateSpec = "Activate",
    specChanged = "Specialization: %s",
    specHint = "Each specialization keeps its own talents: going back to one costs nothing.",
    roles = { tank = "Tank", healer = "Healer", damage = "Damage" },
    errors = {
        [1] = "Those talents could not be read: try again.",
        [2] = "You can't change your talents in combat.",
        [3] = "A talent needs the one above it fully learned.",
        [4] = "Not enough points spent to unlock that row.",
        [5] = "Not enough talent points.",
        [6] = "Your level is too low for that talent.",
        [7] = "Your specialization changed meanwhile.",
        [8] = "Talents are not available.",
        [9] = "That specialization does not exist.",
    },
}

-- Model -------------------------------------------------------------------------------------------------------------

local classId, data
local nodes, byId, trees = {}, {}, {}
-- The spec trees, in order; a class with more than one gets the specialization page
local specs = {}
local state = {
    signature = nil,        -- the server's; nil until it has spoken
    level = 1,
    spec = 1,               -- active talent group, 1-based
    specCount = 1,
    builds = { {}, {} },    -- what the server holds, per talent group
    specTrees = { 0, 0 },   -- the spec tree (its id) each talent group has chosen
    viewed = 1,             -- which group the window shows
    pending = {},           -- the active group's build as edited here
}

local frame, bar, applyButton, undoButton, resetButton, statusText, searchBox, specButtons, shareBox, portrait
local flyout, specPage
local shownAvailable = {}

local function PlayerClassId()
    local _, token = UnitClass("player")
    for id, custom in pairs(CustomClasses or {}) do
        if custom.token == token then
            return id
        end
    end
end

local function Load()
    if data then
        return true
    end
    classId = PlayerClassId()
    data = classId and TalentTreeData and TalentTreeData[classId]
    if not data then
        return false
    end

    local position = 0
    for treeIndex, tree in ipairs(data.trees) do
        -- The class tree on the left, every spec tree on the right: only the one in view is shown
        local entry = { def = tree, index = treeIndex, nodes = {}, side = tree.kind == "class" and 1 or 2 }
        trees[treeIndex] = entry
        if tree.kind == "spec" then
            tinsert(specs, entry)
        end
        for _, def in ipairs(tree.nodes) do
            position = position + 1
            local node = {
                def = def, tree = entry, position = position,
                max = def.kind == "choice" and #def.options or #def.spells,
            }
            nodes[position] = node
            byId[def.id] = node
            tinsert(entry.nodes, node)
        end
    end
    for _, build in ipairs(state.builds) do
        for index = 1, #nodes do
            build[index] = 0
        end
    end
    for index = 1, #nodes do
        state.pending[index] = 0
    end
    return true
end

local function HasTree()
    return Load()
end

local function SpecById(id)
    for _, tree in ipairs(specs) do
        if tree.def.id == id then
            return tree
        end
    end
    return specs[1]
end

-- The spec tree of a talent group (the one in view by default)
local function SpecOf(group)
    return SpecById(state.specTrees[group or state.viewed])
end

-- Drawn right now: the class tree, and the spec tree of the group in view
local function IsShownTree(tree)
    return tree.side == 1 or tree == SpecOf()
end

local function Cost(node, value)
    if value <= 0 then
        return 0
    end
    return node.def.kind == "choice" and 1 or value
end

local function IsMaxed(node, value)
    if node.def.kind == "choice" then
        return value > 0
    end
    return value >= node.max
end

local function PointsAt(tree, level)
    if level < tree.def.firstLevel then
        return 0
    end
    return floor((level - tree.def.firstLevel) / tree.def.levelStep) + 1
end

local function Spent(values, tree, belowRow)
    local spent = 0
    for _, node in ipairs(tree.nodes) do
        if not belowRow or node.def.row < belowRow then
            spent = spent + Cost(node, values[node.position] or 0)
        end
    end
    return spent
end

local function Available(values, tree)
    return PointsAt(tree, state.level) - Spent(values, tree)
end

-- Why a node could not hold a point right now, or nil: the server's Validate, node by node
local function Blocker(values, node)
    local def = node.def
    if (def.level or 0) > state.level then
        return "level"
    end
    for _, gate in ipairs(node.tree.def.gates) do
        if def.row >= gate.row and Spent(values, node.tree, gate.row) < gate.cost then
            return "gate", gate
        end
    end
    if #def.parents > 0 then
        for _, parentId in ipairs(def.parents) do
            local parent = byId[parentId]
            if parent and IsMaxed(parent, values[parent.position] or 0) then
                return nil
            end
        end
        return "parent"
    end
end

local function Validate(values)
    for _, tree in ipairs(trees) do
        if Available(values, tree) < 0 then
            return 5
        end
    end
    for _, node in ipairs(nodes) do
        if (values[node.position] or 0) > 0 then
            local reason = Blocker(values, node)
            if reason == "level" then
                return 6, node
            elseif reason == "gate" then
                return 4, node
            elseif reason == "parent" then
                return 3, node
            end
        end
    end
end

local function Copy(values)
    local copy = {}
    for index = 1, #nodes do
        copy[index] = values[index] or 0
    end
    return copy
end

local function Serialize(values)
    local digits = {}
    for index = 1, #nodes do
        digits[index] = tostring(values[index] or 0)
    end
    return table.concat(digits)
end

local function Parse(text, into)
    if not text or #text ~= #nodes then
        return false
    end
    for index = 1, #nodes do
        local value = tonumber(text:sub(index, index))
        if not value or value > nodes[index].max then
            return false
        end
        into[index] = value
    end
    return true
end

local function Viewed()
    return state.viewed == state.spec and state.pending or state.builds[state.viewed]
end

local function Editable()
    return state.viewed == state.spec and state.signature ~= nil and state.signature == data.signature
end

local function IsDirty()
    local committed = state.builds[state.spec]
    for index = 1, #nodes do
        if (state.pending[index] or 0) ~= (committed[index] or 0) then
            return true
        end
    end
    return false
end

-- Tweens: every animation of the window, driven by one clock -------------------------------------------------------

local tweens = {}
local driver = CreateFrame("Frame")
driver:Hide()

local function Tween(duration, delay, update, done, key)
    if key then
        for _, tween in ipairs(tweens) do
            if tween.key == key then
                tween.dead = true
            end
        end
    end
    tinsert(tweens, { time = -(delay or 0), duration = duration, update = update, done = done, key = key })
    driver:Show()
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

driver:SetScript("OnUpdate", function(self, elapsed)
    for index = #tweens, 1, -1 do
        local tween = tweens[index]
        if tween.dead then
            tremove(tweens, index)
        else
            tween.time = tween.time + elapsed
            if tween.time >= 0 then
                local progress = min(1, tween.time / tween.duration)
                tween.update(progress)
                if progress >= 1 then
                    tremove(tweens, index)
                    if tween.done then
                        tween.done()
                    end
                end
            end
        end
    end
    if #tweens == 0 then
        self:Hide()
    end
end)

-- Art ---------------------------------------------------------------------------------------------------------------

local function Piece(texture, name, scale)
    local piece = ART.pieces[name]
    texture:SetTexture(piece.file)
    texture:SetTexCoord(0, piece.r, 0, piece.b)
    if scale then
        texture:SetSize(piece.w * scale, piece.h * scale)
    end
end

local function Tint(texture, color, alpha)
    texture:SetVertexColor(color[1], color[2], color[3])
    if alpha then
        texture:SetAlpha(alpha)
    end
end

-- The generated icon tables are keyed by the icon's file name; the tree data carries its full path
local function IconName(path)
    return path:match("[^\\]+$")
end

local function SetShown(region, shown)
    if shown then
        region:Show()
    else
        region:Hide()
    end
end

local function NodeX(tree, column)
    local left = tree.side == 1 and SIDE or (WIDTH - SIDE - (COLUMNS - 1) * GRID.column)
    return left + column * GRID.column
end

local function NodeY(row)
    return TOP_ROW - row * GRID.row
end

-- Tooltip -----------------------------------------------------------------------------------------------------------

local function Wrap(text, color)
    GameTooltip:AddLine(text, color[1], color[2], color[3], true)
end

local function RequirementLines(values, node)
    local reason, gate = Blocker(values, node)
    if reason == "level" then
        Wrap(format(TEXT.requiresLevel, node.def.level), RED)
    elseif reason == "gate" then
        Wrap(format(TEXT.gate, gate.cost - Spent(values, node.tree, gate.row), node.tree.def.name), RED)
    elseif reason == "parent" then
        Wrap(TEXT.requiresParent, RED)
    end
    return reason
end

local function ShowNodeTooltip(button)
    local node = button.node
    local def = node.def
    local values = Viewed()
    local value = values[node.position] or 0
    local right = button:GetCenter() * button:GetEffectiveScale() >
        UIParent:GetWidth() * UIParent:GetEffectiveScale() * 0.55
    GameTooltip:SetOwner(button, right and "ANCHOR_LEFT" or "ANCHOR_RIGHT")

    if def.kind == "choice" then
        GameTooltip:AddLine(value > 0 and def.options[value].name or
            (def.options[1].name .. " / " .. def.options[2].name), 1, 1, 1)
        GameTooltip:AddLine(TEXT.choice, 0.6, 0.6, 0.6)
        for index, option in ipairs(def.options) do
            GameTooltip:AddLine(" ")
            local chosen = value == index
            GameTooltip:AddLine(option.name, chosen and GOLD[1] or 0.8, chosen and GOLD[2] or 0.8,
                chosen and GOLD[3] or 0.8)
            Wrap(option.text, chosen and { 1, 1, 1 } or { 0.65, 0.65, 0.65 })
        end
    else
        GameTooltip:AddDoubleLine(def.name, node.max > 1 and format(TEXT.rank, value, node.max) or "",
            1, 1, 1, 0.75, 0.75, 0.75)
        GameTooltip:AddLine(def.kind == "active" and TEXT.active or TEXT.passive, 0.6, 0.6, 0.6)
        Wrap(def.texts[max(1, value)], { 1, 0.82, 0 })
        if value > 0 and value < node.max then
            GameTooltip:AddLine(" ")
            GameTooltip:AddLine(TEXT.nextRank, 1, 1, 1)
            Wrap(def.texts[value + 1], { 1, 0.82, 0 })
        end
    end

    GameTooltip:AddLine(" ")
    local reason = RequirementLines(values, node)
    if Editable() then
        if not reason and Available(values, node.tree) <= 0 and value < node.max and
                not (def.kind == "choice" and value > 0) then
            Wrap(format(TEXT.noPoints, node.tree.def.name), RED)
        end
        if def.kind == "choice" then
            if not reason then
                GameTooltip:AddLine(TEXT.choose, GREEN[1], GREEN[2], GREEN[3])
            end
        elseif not reason and value < node.max and Available(values, node.tree) > 0 then
            GameTooltip:AddLine(TEXT.learn, GREEN[1], GREEN[2], GREEN[3])
        end
        if value > 0 then
            GameTooltip:AddLine(TEXT.unlearn, 0.6, 0.6, 0.6)
        end
    elseif state.viewed ~= state.spec then
        Wrap(TEXT.viewing, { 0.6, 0.6, 0.6 })
    end
    GameTooltip:Show()
end

-- Drawing -----------------------------------------------------------------------------------------------------------

local function NodeLook(values, node)
    local value = values[node.position] or 0
    local reason = Blocker(values, node)
    if value > 0 then
        return "learned", value
    end
    if reason == "level" or reason == "gate" then
        return "locked", value
    end
    if reason == "parent" then
        return "unavailable", value
    end
    if Available(values, node.tree) > 0 and Editable() then
        return "learnable", value
    end
    return "unavailable", value
end

local FRAME_COLOR = { learned = "yellow", learnable = "green", unavailable = "gray", locked = "locked" }

local function PaintNode(node)
    local button = node.button
    local values = Viewed()
    local look, value = NodeLook(values, node)
    node.look = look
    local shape = SHAPE[node.def.kind]
    Piece(button.border, "node-" .. shape .. "-" .. FRAME_COLOR[look])

    if node.def.kind == "choice" then
        local option = value > 0 and node.def.options[value]
        local options = node.def.options
        button.icon:SetTexture(option and ART.octagon[IconName(option.icon)]
            or ART.split[IconName(options[1].icon) .. "|" .. IconName(options[2].icon)])
    end
    button.icon:SetDesaturated(look == "locked" or look == "unavailable")
    button.icon:SetAlpha(look == "locked" and 0.45 or look == "unavailable" and 0.7 or 1)
    button.border:SetAlpha(look == "locked" and 0.8 or 1)

    if button.badge then
        local maxed = value >= node.max
        SetShown(button.badge, look ~= "locked")
        SetShown(button.rank, look ~= "locked")
        button.rank:SetText(value .. "/" .. node.max)
        local color = maxed and GOLD or value > 0 and GREEN or look == "learnable" and GREEN or { 0.6, 0.6, 0.6 }
        Tint(button.badge, color)
        button.rank:SetTextColor(color[1], color[2], color[3])
    end

    -- A change not applied yet keeps a soft blue ring of its own, so the edit is visible at a glance
    local committed = state.builds[state.spec][node.position] or 0
    node.changed = state.viewed == state.spec and committed ~= value
    if not node.glowBusy then
        if node.changed then
            Tint(button.glow, BLUE, 0.55)
        elseif look == "learned" and value >= node.max then
            Tint(button.glow, GOLD, 0.18)
        elseif look == "learnable" then
            Tint(button.glow, GREEN, 0.3)
        else
            button.glow:SetAlpha(0)
        end
    end
end

local function LinkColor(values, link)
    local parentMaxed = IsMaxed(link.parent, values[link.parent.position] or 0)
    local childValue = values[link.child.position] or 0
    local reason = Blocker(values, link.child)
    if parentMaxed and childValue > 0 then
        return GOLD, 1, "gold"
    elseif reason == "level" or reason == "gate" then
        return { 0.32, 0.32, 0.34 }, 0.55, "locked"
    elseif parentMaxed then
        return { 0.9, 0.9, 0.9 }, 0.85, "open"
    end
    return { 0.5, 0.5, 0.52 }, 0.6, "gray"
end

local function PaintLinks(tree, animate)
    local values = Viewed()
    for _, link in ipairs(tree.links) do
        local color, alpha, kind = LinkColor(values, link)
        if animate and kind == "gold" and link.kind ~= "gold" then
            -- Lighting up: the gold runs down the line from the parent, a spark leading it
            local from = { link.texture:GetVertexColor() }
            local spark = link.spark
            spark:Show()
            Tween(0.32, 0, function(p)
                local eased = OutCubic(p)
                link.texture:SetVertexColor(from[1] + (color[1] - from[1]) * eased,
                    from[2] + (color[2] - from[2]) * eased, from[3] + (color[3] - from[3]) * eased)
                link.texture:SetAlpha(alpha)
                spark:SetPoint("CENTER", tree.panel, "TOPLEFT", link.fromX + (link.toX - link.fromX) * eased,
                    link.fromY + (link.toY - link.fromY) * eased)
                spark:SetAlpha(p < 0.8 and 1 or (1 - p) / 0.2)
            end, function() spark:Hide() end, link)
        else
            Tint(link.texture, color, alpha)
        end
        link.kind = kind
    end
end

local function PaintGates(tree, animate)
    local values = Viewed()
    for _, gate in ipairs(tree.gates) do
        local missing = gate.def.cost - Spent(values, tree, gate.def.row)
        local open = missing <= 0
        if open and not gate.open and animate then
            -- The lock gives: it flashes open, then the gate fades away
            Piece(gate.texture, "gate-open")
            gate.flash:SetAlpha(1)
            gate.count:SetText("")
            Tween(0.9, 0, function(p)
                gate.flash:SetAlpha(1 - p)
                gate.texture:SetAlpha(1 - OutCubic(p))
            end, function()
                -- Refunded meanwhile: the gate stands again
                if gate.open then
                    gate.frame:Hide()
                end
            end, gate)
            PlaySound("igQuestListComplete")
        elseif not animate or not open then
            SetShown(gate.frame, not open)
            gate.texture:SetAlpha(1)
            Piece(gate.texture, "gate")
            gate.count:SetText(open and "" or missing)
        end
        gate.open = open
    end
end

local function PaintHeader(tree, animate)
    local values = Viewed()
    local available = Available(values, tree)
    local shown = shownAvailable[tree.index]
    tree.number:SetText(max(0, available))
    tree.number:SetTextColor(available > 0 and 1 or 0.55, available > 0 and 1 or 0.55, available > 0 and 1 or 0.55)
    if animate and shown and shown ~= available then
        Tween(0.4, 0, function(p)
            local swell = p < 0.3 and p / 0.3 or 1 - (p - 0.3) / 0.7
            tree.number:SetFont(FRIZ, 30 + 10 * swell)
        end, function() tree.number:SetFont(FRIZ, 30) end, tree.number)
    end
    shownAvailable[tree.index] = available

    local level = state.level
    if level >= 80 then
        tree.hint:SetText("")
    else
        local nextLevel = max(tree.def.firstLevel, level + 1)
        while PointsAt(tree, nextLevel) <= PointsAt(tree, level) and nextLevel < 80 do
            nextLevel = nextLevel + 1
        end
        tree.hint:SetText(PointsAt(tree, nextLevel) > PointsAt(tree, level) and format(TEXT.nextPoint, nextLevel) or "")
    end
end

local function PaintBar()
    local dirty = state.viewed == state.spec and IsDirty()
    local editable = Editable()
    applyButton:SetEnabled(dirty and editable and not UnitAffectingCombat("player"))
    SetShown(applyButton.glow, dirty and editable)
    undoButton:SetEnabled(dirty)
    undoButton:SetAlpha(dirty and 1 or 0.35)
    local empty = true
    for _, node in ipairs(nodes) do
        if IsShownTree(node.tree) and (Viewed()[node.position] or 0) > 0 then
            empty = false
            break
        end
    end
    resetButton:SetEnabled(editable and not empty)
    resetButton:SetAlpha(editable and not empty and 1 or 0.35)

    if state.signature and state.signature ~= data.signature then
        statusText:SetText(TEXT.outdated)
        statusText:SetTextColor(RED[1], RED[2], RED[3])
    elseif not state.signature then
        statusText:SetText(state.silent and TEXT.noServer or TEXT.waiting)
        statusText:SetTextColor(0.6, 0.6, 0.6)
    elseif state.viewed ~= state.spec then
        statusText:SetText(TEXT.viewing)
        statusText:SetTextColor(0.6, 0.6, 0.6)
    else
        statusText:SetText("")
    end

    for index, button in ipairs(specButtons) do
        SetShown(button, index <= state.specCount and state.specCount > 1)
        local viewed = index == state.viewed
        button.text:SetTextColor(viewed and 1 or 0.6, viewed and 0.82 or 0.6, viewed and 0 or 0.6)
        SetShown(button.selected, viewed)
        SetShown(button.activeMark, index == state.spec)
    end
    SetShown(specButtons.activate, state.specCount > 1 and state.viewed ~= state.spec)
    if shareBox and not shareBox:HasFocus() then
        shareBox:SetText(Serialize(Viewed()))
    end
end

-- The window's painting: the viewed specialization's own when it has one, else the class's. It is wider than the
-- window, so it is cut to the window's shape from its right, where the figure stands.
local function PaintBackground()
    local key = classId .. "-" .. SpecOf().def.id
    local file = ART.backgrounds[key] or ART.backgrounds[classId]
    local background = ART.pieces["background-" .. (ART.backgrounds[key] and key or classId)]
    if frame.shownBackground ~= file then
        frame.art:SetTexture(file)
        frame.shownBackground = file
    end
    local canvasW, canvasH = background.w / background.r, background.h / background.b
    local aspect = (WIDTH - 4) / (HEIGHT - 4)
    local spanW = min(background.w, background.h * aspect)
    local spanH = spanW / aspect
    frame.art:SetTexCoord((background.w - spanW) / canvasW, background.r, 0, spanH / canvasH)
end

local function Paint(animate)
    if not frame then
        return
    end
    local onTalents = not specPage or not specPage:IsShown()
    for _, tree in ipairs(trees) do
        SetShown(tree.panel, onTalents and IsShownTree(tree))
    end
    PaintBackground()
    for _, node in ipairs(nodes) do
        PaintNode(node)
    end
    for _, tree in ipairs(trees) do
        PaintLinks(tree, animate)
        PaintGates(tree, animate)
        PaintHeader(tree, animate)
    end
    PaintBar()
end

-- Animations on nodes ---------------------------------------------------------------------------------------------

local function Pop(node, from)
    local holder = node.button.holder
    Tween(0.38, 0, function(p)
        holder:SetScale(max(0.01, from + (1 - from) * OutBack(p)))
    end, function() holder:SetScale(node.button.hovered and 1.1 or 1) end, holder)
end

local function Flash(node, color, strength, duration)
    local glow = node.button.glow
    node.glowBusy = true
    Tint(glow, color)
    Tween(duration or 0.55, 0, function(p)
        glow:SetAlpha(strength * (1 - OutCubic(p)))
    end, function()
        node.glowBusy = false
        PaintNode(node)
    end, glow)
end

-- A ring of light leaves the node in its own shape and fades as it widens
local function Sheen(node)
    local ring = node.button.ring
    ring:Show()
    Tween(0.5, 0.03, function(p)
        local grow = 1 + 0.55 * OutCubic(p)
        ring:SetSize(ring.baseWidth * grow, ring.baseHeight * grow)
        ring:SetAlpha(0.9 * (1 - p))
    end, function() ring:Hide() end, ring)
end

local function Shake(node)
    local holder = node.button.holder
    Tween(0.32, 0, function(p)
        holder:SetPoint("CENTER", node.button, "CENTER", math.sin(p * 30) * 4 * (1 - p), 0)
    end, function() holder:SetPoint("CENTER", node.button, "CENTER", 0, 0) end, "shake" .. node.position)
end

local function Refuse(node, text)
    PlaySound(SOUND_DENIED)
    if node then
        Shake(node)
        Flash(node, RED, 0.9, 0.5)
    end
    if text then
        UIErrorsFrame:AddMessage(text, 1, 0.2, 0.2, 1)
    end
end

-- Nodes that became learnable light up one after the other, nearest first
local function Ripple(before)
    local woken = 0
    for _, node in ipairs(nodes) do
        if node.look == "learnable" and before[node.position] ~= "learnable" and before[node.position] ~= "learned" then
            woken = woken + 1
            local button = node.button
            local delay = 0.05 * woken
            node.glowBusy = true
            button.holder:SetAlpha(0.6)
            Tween(0.35, delay, function(p)
                button.holder:SetAlpha(0.6 + 0.4 * p)
                button.glow:SetAlpha(0.8 * (1 - p) + 0.3 * p)
            end, function()
                node.glowBusy = false
                button.holder:SetAlpha(1)
                PaintNode(node)
            end, "ripple" .. node.position)
        end
    end
end

-- Editing -----------------------------------------------------------------------------------------------------------

local function Looks()
    local looks = {}
    for _, node in ipairs(nodes) do
        looks[node.position] = node.look
    end
    return looks
end

local function Change(node, value)
    local candidate = Copy(state.pending)
    local old = candidate[node.position]
    candidate[node.position] = value
    local code, offender = Validate(candidate)
    if code then
        if value < old then
            Refuse(node, TEXT.cantRefund)
            if offender and offender ~= node then
                Flash(offender, RED, 0.9, 0.7)
            end
        else
            Refuse(node, TEXT.errors[code])
        end
        return false
    end

    local before = Looks()
    state.pending = candidate
    Paint(true)
    if value > old then
        PlaySound(SOUND_LEARN)
        Pop(node, 1.3)
        Flash(node, GOLD, 1)
        Sheen(node)
    else
        PlaySound(SOUND_UNLEARN)
        Pop(node, 0.8)
        Flash(node, { 1, 1, 1 }, 0.5, 0.35)
    end
    Ripple(before)
    return true
end

local function HideFlyout()
    if flyout then
        flyout:Hide()
    end
end

local function OpenFlyout(node)
    local button = node.button
    flyout.node = node
    flyout:ClearAllPoints()
    flyout:SetPoint("BOTTOM", button, "TOP", 0, 2)
    local value = state.pending[node.position] or 0
    for index, option in ipairs(flyout.options) do
        local def = node.def.options[index]
        option.icon:SetTexture(ART.octagon[IconName(def.icon)])
        option.def = def
        option.index = index
        Piece(option.border, "node-choice-" .. (value == index and "yellow" or "green"))
    end
    flyout:SetAlpha(0)
    flyout:SetScale(0.7)
    flyout:Show()
    Tween(0.22, 0, function(p)
        flyout:SetAlpha(p)
        flyout:SetScale(0.7 + 0.3 * OutBack(p))
    end, nil, flyout)
end

local function Click(button, mouse)
    local node = button.node
    if not Editable() then
        return Refuse(nil, state.viewed ~= state.spec and TEXT.viewing or nil)
    end
    if UnitAffectingCombat("player") then
        return Refuse(node, TEXT.combat)
    end
    local value = state.pending[node.position] or 0
    if node.def.kind == "choice" then
        if mouse == "RightButton" then
            HideFlyout()
            if value > 0 then
                Change(node, 0)
            end
            return
        end
        local reason = Blocker(state.pending, node)
        if reason then
            return Refuse(node, reason == "level" and format(TEXT.requiresLevel, node.def.level) or
                reason == "gate" and TEXT.errors[4] or TEXT.errors[3])
        end
        if value == 0 and Available(state.pending, node.tree) <= 0 then
            return Refuse(node, TEXT.errors[5])
        end
        PlaySound(SOUND_CHOICE)
        return OpenFlyout(node)
    end

    if mouse == "RightButton" then
        if value > 0 then
            Change(node, value - 1)
        end
        return
    end
    if value >= node.max then
        return
    end
    local reason = Blocker(state.pending, node)
    if reason then
        return Refuse(node, reason == "level" and format(TEXT.requiresLevel, node.def.level) or
            reason == "gate" and TEXT.errors[4] or TEXT.errors[3])
    end
    if Available(state.pending, node.tree) <= 0 then
        return Refuse(node, TEXT.errors[5])
    end
    Change(node, value + 1)
end

-- Search: matching nodes glow, the rest step back
local function ApplySearch()
    local query = searchBox and searchBox:GetText():lower() or ""
    if query == TEXT.search:lower() then
        query = ""
    end
    for _, node in ipairs(nodes) do
        local match = false
        if query ~= "" then
            local def = node.def
            local haystack = {}
            if def.kind == "choice" then
                for _, option in ipairs(def.options) do
                    tinsert(haystack, option.name)
                    tinsert(haystack, option.text)
                end
            else
                tinsert(haystack, def.name)
                tinsert(haystack, def.texts[#def.texts])
            end
            match = table.concat(haystack, " "):lower():find(query, 1, true) ~= nil
        end
        node.searched = match
        SetShown(node.button.search, match)
        node.button:SetAlpha(query ~= "" and not match and 0.35 or 1)
    end
end

-- Apply: the server answers with the state it now holds -----------------------------------------------------------

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function Apply()
    if not Editable() or not IsDirty() then
        return
    end
    if UnitAffectingCombat("player") then
        return Refuse(nil, TEXT.combat)
    end
    local code, offender = Validate(state.pending)
    if code then
        return Refuse(offender, TEXT.errors[code])
    end
    applyButton:Disable()
    state.applying = Copy(state.pending)
    Send("APPLY\t" .. (state.spec - 1) .. "\t" .. Serialize(state.pending))
end

-- The moment it lands: a burst behind each tree, embers rising, every node that changed flashing in turn
local function Celebrate(changed)
    PlaySound(SOUND_APPLY)
    PlaySound("LEVELUPSOUND")
    for _, tree in ipairs(trees) do
        -- The other specialization's tree is hidden: nothing to light up there
        if IsShownTree(tree) then
            local burst = tree.burst
            burst:Show()
            Tween(1.1, 0, function(p)
                local size = 160 + 560 * OutCubic(p)
                burst:SetSize(size, size)
                burst:SetAlpha(0.4 * (1 - p) * (1 - p))
            end, function() burst:Hide() end, burst)
            local orb = tree.orb
            orb:Show()
            Tween(1.4, 0.05, function(p)
                local size = 120 + 260 * OutCubic(p)
                orb:SetSize(size, size)
                orb:SetAlpha(p < 0.2 and p * 3 or 0.6 * (1 - p) / 0.8)
            end, function() orb:Hide() end, orb)
        end
    end
    local embers = frame.embers
    embers:Show()
    Tween(1.8, 0.1, function(p)
        embers:SetPoint("BOTTOM", frame, "BOTTOM", 0, BAR_HEIGHT - 40 + 140 * p)
        embers:SetAlpha(p < 0.25 and p * 4 * 0.9 or 0.9 * (1 - p) / 0.75)
    end, function() embers:Hide() end, embers)

    local order = 0
    for _, node in ipairs(nodes) do
        if changed[node.position] then
            order = order + 1
            local delay = 0.04 * order
            Tween(0.01, delay, function() end, function()
                Pop(node, 1.25)
                Flash(node, GOLD, 1, 0.8)
                Sheen(node)
            end)
        end
    end

    local banner = frame.banner
    banner.text:SetText(changed.bannerText or TEXT.applied)
    banner:Show()
    Tween(1.8, 0, function(p)
        banner:SetAlpha(p < 0.15 and p / 0.15 or p > 0.7 and (1 - p) / 0.3 or 1)
        banner:SetScale(1.15 - 0.15 * OutCubic(min(1, p * 3)))
    end, function() banner:Hide() end, banner)
end

-- Window ------------------------------------------------------------------------------------------------------------

local function CreateNode(tree, node)
    local def = node.def
    local button = CreateFrame("Button", nil, tree.panel)
    button:SetSize(NODE, NODE)
    button:SetPoint("CENTER", tree.panel, "TOPLEFT", NodeX(tree, def.col), NodeY(def.row))
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    button:SetFrameLevel(tree.panel:GetFrameLevel() + 4)
    button.node = node
    node.button = button

    -- Everything drawn sits on a holder centred on the button, so it can swell and shake around its own middle
    local holder = CreateFrame("Frame", nil, button)
    holder:SetSize(NODE, NODE)
    holder:SetPoint("CENTER", button, "CENTER", 0, 0)
    button.holder = holder

    local shape = SHAPE[def.kind]
    local shadow = holder:CreateTexture(nil, "BACKGROUND")
    Piece(shadow, "node-" .. shape .. "-shadow", (def.kind == "passive" and 0.8 or 0.62) * SCALE)
    shadow:SetPoint("CENTER", 0, -2)

    local glow = holder:CreateTexture(nil, "BACKGROUND", nil, 1)
    Piece(glow, "glow-" .. shape, FRAME_SCALE[def.kind] * (def.kind == "passive" and 1.05 or 1))
    glow:SetBlendMode("ADD")
    glow:SetPoint("CENTER")
    glow:SetAlpha(0)
    button.glow = glow

    local icon = holder:CreateTexture(nil, "ARTWORK")
    icon:SetSize(ICON, ICON)
    icon:SetPoint("CENTER")
    if def.kind == "passive" then
        icon:SetTexture(ART.circle[IconName(def.icon)])
    elseif def.kind == "active" then
        icon:SetTexture(def.icon)
        icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    end
    button.icon = icon

    local border = holder:CreateTexture(nil, "OVERLAY")
    Piece(border, "node-" .. shape .. "-gray", FRAME_SCALE[def.kind])
    border:SetPoint("CENTER")
    button.border = border

    local ring = holder:CreateTexture(nil, "OVERLAY", nil, 2)
    Piece(ring, "node-" .. shape .. "-yellow", FRAME_SCALE[def.kind])
    ring:SetBlendMode("ADD")
    ring:SetPoint("CENTER")
    ring.baseWidth, ring.baseHeight = ring:GetWidth(), ring:GetHeight()
    ring:Hide()
    button.ring = ring

    local search = holder:CreateTexture(nil, "OVERLAY", nil, 3)
    Piece(search, "search-match", 0.62 * SCALE)
    search:SetBlendMode("ADD")
    search:SetPoint("CENTER")
    search:Hide()
    button.search = search

    -- A rank count only means something for a node with ranks; a choice is one point, whichever option
    if node.max > 1 and def.kind ~= "choice" then
        -- On a frame of its own above the node: texture sublevels alone let the frame's border cover the number
        local top = CreateFrame("Frame", nil, holder)
        top:SetAllPoints(holder)
        top:SetFrameLevel(holder:GetFrameLevel() + 2)
        local badge = top:CreateTexture(nil, "OVERLAY")
        Piece(badge, "rank-badge", 0.5)
        badge:SetPoint("CENTER", holder, "CENTER", 14, -14)
        button.badge = badge
        local rank = top:CreateFontString(nil, "OVERLAY")
        rank:SetFont(NARROW, 10)
        rank:SetShadowOffset(1, -1)
        rank:SetShadowColor(0, 0, 0, 1)
        rank:SetPoint("CENTER", badge, "CENTER", 0, 0)
        button.rank = rank
    end

    button:SetScript("OnClick", Click)
    button:SetScript("OnEnter", function(self)
        self.hovered = true
        PlaySound(SOUND_HOVER)
        local from = self.holder:GetScale()
        Tween(0.14, 0, function(p) self.holder:SetScale(from + (1.1 - from) * OutCubic(p)) end, nil, self.holder)
        ShowNodeTooltip(self)
    end)
    button:SetScript("OnLeave", function(self)
        self.hovered = false
        local from = self.holder:GetScale()
        Tween(0.14, 0, function(p) self.holder:SetScale(from + (1 - from) * OutCubic(p)) end, nil, self.holder)
        GameTooltip:Hide()
    end)
end

local function CreateLinks(tree)
    tree.links = {}
    for _, node in ipairs(tree.nodes) do
        for _, parentId in ipairs(node.def.parents) do
            local parent = byId[parentId]
            local dx = node.def.col - parent.def.col
            local dy = node.def.row - parent.def.row
            local art = ART.links[format("link-%dx%d-%d", math.abs(dx), dy, LINK_REACH[node.def.kind])]
            if art then
                local texture = tree.panel:CreateTexture(nil, "BORDER")
                texture:SetTexture(art.file)
                texture:SetSize(art.w, art.h)
                local fromX, fromY = NodeX(tree, parent.def.col), NodeY(parent.def.row)
                -- The art runs down and to the right from its padded top-left; the other way is the same art mirrored
                if dx < 0 then
                    texture:SetTexCoord(1, 0, 0, 1)
                    texture:SetPoint("TOPLEFT", tree.panel, "TOPLEFT", fromX - (art.w - art.pad), fromY + art.pad)
                else
                    texture:SetPoint("TOPLEFT", tree.panel, "TOPLEFT", fromX - art.pad, fromY + art.pad)
                end
                local spark = tree.panel:CreateTexture(nil, "ARTWORK")
                Piece(spark, "glow-circle", 0.28)
                spark:SetBlendMode("ADD")
                Tint(spark, GOLD)
                spark:Hide()
                tinsert(tree.links, {
                    texture = texture, spark = spark, parent = parent, child = node,
                    fromX = fromX, fromY = fromY, toX = NodeX(tree, node.def.col), toY = NodeY(node.def.row),
                })
            end
        end
    end
end

local function CreateGates(tree)
    tree.gates = {}
    for _, def in ipairs(tree.def.gates) do
        local gate = CreateFrame("Frame", nil, tree.panel)
        gate:SetSize(130, 22)
        gate:SetPoint("LEFT", tree.panel, "TOPLEFT", NodeX(tree, 0) - 52, NodeY(def.row))
        gate:SetFrameLevel(tree.panel:GetFrameLevel() + 3)
        gate:EnableMouse(true)
        local texture = gate:CreateTexture(nil, "ARTWORK")
        Piece(texture, "gate", 0.75)
        texture:SetPoint("LEFT")
        local flash = gate:CreateTexture(nil, "OVERLAY")
        Piece(flash, "glow-circle", 0.5)
        flash:SetBlendMode("ADD")
        flash:SetPoint("CENTER", texture, "LEFT", 10, 0)
        Tint(flash, GOLD, 0)
        local count = gate:CreateFontString(nil, "OVERLAY")
        count:SetFont(NARROW, 14)
        count:SetShadowOffset(1, -1)
        count:SetShadowColor(0, 0, 0, 1)
        count:SetPoint("RIGHT", texture, "LEFT", -2, 0)
        count:SetTextColor(1, 0.35, 0.25)
        local entry = { frame = gate, texture = texture, flash = flash, count = count, def = def }
        gate:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            local missing = def.cost - Spent(Viewed(), tree, def.row)
            GameTooltip:AddLine(format(TEXT.gate, max(0, missing), tree.def.name), 1, 1, 1, true)
            GameTooltip:Show()
        end)
        gate:SetScript("OnLeave", function() GameTooltip:Hide() end)
        tinsert(tree.gates, entry)
    end
end

local function CreateTree(tree)
    local panel = CreateFrame("Frame", nil, frame)
    panel:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0)
    panel:SetSize(WIDTH, HEIGHT)
    panel:SetFrameLevel(frame:GetFrameLevel() + 2)
    tree.panel = panel

    local centreX = NodeX(tree, (COLUMNS - 1) / 2)
    local title = panel:CreateFontString(nil, "OVERLAY")
    title:SetFont(FRIZ, 15)
    title:SetShadowColor(0, 0, 0, 1)
    title:SetTextColor(1, 0.95, 0.85)
    title:SetShadowOffset(1, -1)
    title:SetPoint("CENTER", panel, "TOPLEFT", centreX, -60)
    title:SetText(format(TEXT.points, tree.def.name:upper()))
    tree.title = title

    local number = panel:CreateFontString(nil, "OVERLAY")
    number:SetFont(FRIZ, 30)
    number:SetShadowOffset(1, -1)
    number:SetShadowColor(0, 0, 0, 1)
    number:SetPoint("CENTER", panel, "TOPLEFT", centreX, -90)
    tree.number = number

    local hint = panel:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    hint:SetPoint("TOP", number, "BOTTOM", 0, 2)
    tree.hint = hint

    local burst = panel:CreateTexture(nil, "BACKGROUND", nil, 2)
    Piece(burst, "anim-burst")
    burst:SetBlendMode("ADD")
    burst:SetPoint("CENTER", panel, "TOPLEFT", centreX, NodeY(4.5))
    burst:Hide()
    tree.burst = burst

    local orb = panel:CreateTexture(nil, "BACKGROUND", nil, 3)
    Piece(orb, "anim-orb")
    orb:SetBlendMode("ADD")
    orb:SetPoint("CENTER", panel, "TOPLEFT", centreX, NodeY(4.5))
    orb:Hide()
    tree.orb = orb

    for _, node in ipairs(tree.nodes) do
        CreateNode(tree, node)
    end
    CreateLinks(tree)
    CreateGates(tree)
end

local function CreateFlyout()
    flyout = CreateFrame("Frame", nil, frame)
    flyout:SetSize(150, 66)
    flyout:SetFrameStrata("DIALOG")
    flyout:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    flyout:SetBackdropColor(0.03, 0.04, 0.07, 0.95)
    flyout:SetBackdropBorderColor(0.6, 0.55, 0.4, 1)
    flyout:EnableMouse(true)
    flyout:Hide()
    flyout.options = {}
    for index = 1, 2 do
        local option = CreateFrame("Button", nil, flyout)
        option:SetSize(NODE, NODE)
        option:SetPoint("CENTER", flyout, "CENTER", index == 1 and -34 or 34, 0)
        local icon = option:CreateTexture(nil, "ARTWORK")
        icon:SetSize(ICON, ICON)
        icon:SetPoint("CENTER")
        option.icon = icon
        local border = option:CreateTexture(nil, "OVERLAY")
        Piece(border, "node-choice-green", FRAME_SCALE.choice)
        border:SetPoint("CENTER")
        option.border = border
        local highlight = option:CreateTexture(nil, "HIGHLIGHT")
        Piece(highlight, "glow-choice", 0.45 * SCALE)
        highlight:SetBlendMode("ADD")
        highlight:SetPoint("CENTER")
        Tint(highlight, GOLD, 0.6)
        option:SetScript("OnEnter", function(self)
            GameTooltip:SetOwner(self, "ANCHOR_TOP")
            GameTooltip:AddLine(self.def.name, 1, 1, 1)
            GameTooltip:AddLine(self.def.text, 1, 0.82, 0, true)
            GameTooltip:Show()
        end)
        option:SetScript("OnLeave", function() GameTooltip:Hide() end)
        option:SetScript("OnClick", function(self)
            local node = flyout.node
            HideFlyout()
            GameTooltip:Hide()
            if (state.pending[node.position] or 0) ~= self.index then
                Change(node, self.index)
            end
        end)
        flyout.options[index] = option
    end
    -- It closes once the pointer has left it and its node for a moment
    local away = 0
    flyout:SetScript("OnUpdate", function(self, elapsed)
        if self:IsMouseOver() or (self.node and self.node.button:IsMouseOver()) then
            away = 0
        else
            away = away + elapsed
            if away > 0.6 then
                away = 0
                self:Hide()
            end
        end
    end)
end

local function IconButton(pieceName, size, onClick, tooltip, hint)
    local button = CreateFrame("Button", nil, bar)
    button:SetSize(size, size)
    local texture = button:CreateTexture(nil, "ARTWORK")
    Piece(texture, pieceName)
    texture:SetAllPoints()
    local highlight = button:CreateTexture(nil, "HIGHLIGHT")
    Piece(highlight, pieceName)
    highlight:SetAllPoints()
    highlight:SetBlendMode("ADD")
    highlight:SetAlpha(0.5)
    button:SetScript("OnClick", onClick)
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:AddLine(tooltip, 1, 1, 1)
        GameTooltip:AddLine(hint, 1, 0.82, 0, true)
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return button
end

local function CreateBar()
    bar = CreateFrame("Frame", nil, frame)
    bar:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 2, 2)
    bar:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -2, 2)
    bar:SetHeight(BAR_HEIGHT)
    bar:SetFrameLevel(frame:GetFrameLevel() + 10)
    local ground = bar:CreateTexture(nil, "BACKGROUND")
    Piece(ground, "bottombar")
    ground:SetAllPoints()

    applyButton = CreateFrame("Button", nil, bar, "UIPanelButtonTemplate")
    applyButton:SetSize(170, 26)
    applyButton:SetPoint("CENTER", bar, "CENTER", -40, -2)
    applyButton:SetText(TEXT.apply)
    applyButton:SetScript("OnClick", Apply)
    -- 3.3.5 buttons have no SetEnabled
    function applyButton:SetEnabled(enabled)
        if enabled then
            self:Enable()
        else
            self:Disable()
        end
    end
    -- Changes waiting: the button's own highlight breathes over it, in gold
    local glow = applyButton:CreateTexture(nil, "OVERLAY")
    glow:SetTexture("Interface\\Buttons\\UI-Panel-Button-Highlight")
    glow:SetTexCoord(0, 0.625, 0, 0.6875)
    glow:SetBlendMode("ADD")
    glow:SetAllPoints(applyButton)
    Tint(glow, GOLD, 0.6)
    glow:Hide()
    applyButton.glow = glow

    undoButton = IconButton("button-undo", 26, function()
        if IsDirty() then
            local before = Looks()
            state.pending = Copy(state.builds[state.spec])
            PlaySound(SOUND_UNLEARN)
            HideFlyout()
            Paint(true)
            Ripple(before)
        end
    end, TEXT.undo, TEXT.undoHint)
    undoButton:SetPoint("LEFT", applyButton, "RIGHT", 10, 1)
    function undoButton:SetEnabled(enabled)
        if enabled then
            self:Enable()
        else
            self:Disable()
        end
    end

    resetButton = IconButton("button-reset", 24, function()
        if not Editable() then
            return
        end
        -- What is on screen: the other specialization's talents are kept
        for _, node in ipairs(nodes) do
            if IsShownTree(node.tree) then
                state.pending[node.position] = 0
            end
        end
        PlaySound("Glyph_MajorDestroy")
        HideFlyout()
        Paint(true)
        for order, node in ipairs(nodes) do
            if IsShownTree(node.tree) then
                Tween(0.01, 0.008 * order, function() end, function() Pop(node, 0.75) end)
            end
        end
    end, TEXT.reset, TEXT.resetHint)
    resetButton:SetPoint("LEFT", undoButton, "RIGHT", 8, 0)
    function resetButton:SetEnabled(enabled)
        if enabled then
            self:Enable()
        else
            self:Disable()
        end
    end

    statusText = bar:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    statusText:SetPoint("BOTTOM", applyButton, "TOP", 0, 6)

    -- The specializations (dual specialization): each keeps its own build; the other can be looked at and activated
    specButtons = {}
    for index = 1, 2 do
        local button = CreateFrame("Button", nil, bar)
        button:SetSize(128, 24)
        button:SetPoint("LEFT", bar, "LEFT", 22 + (index - 1) * 134, -2)
        local selected = button:CreateTexture(nil, "BACKGROUND")
        selected:SetTexture("Interface\\Buttons\\WHITE8X8")
        selected:SetAllPoints()
        selected:SetGradientAlpha("VERTICAL", 1, 0.8, 0.3, 0.05, 1, 0.8, 0.3, 0.25)
        button.selected = selected
        local text = button:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        text:SetPoint("CENTER", 6, 0)
        text:SetText(format(TEXT.spec, index))
        button.text = text
        local activeMark = button:CreateTexture(nil, "OVERLAY")
        activeMark:SetTexture("Interface\\RAIDFRAME\\ReadyCheck-Ready")
        activeMark:SetSize(14, 14)
        activeMark:SetPoint("RIGHT", text, "LEFT", -3, 0)
        button.activeMark = activeMark
        button:SetScript("OnClick", function()
            if state.viewed ~= index then
                state.viewed = index
                PlaySound("igCharacterInfoTab")
                HideFlyout()
                Paint(true)
            end
        end)
        specButtons[index] = button
    end
    local activate = CreateFrame("Button", nil, bar, "UIPanelButtonTemplate")
    activate:SetSize(90, 22)
    activate:SetPoint("LEFT", specButtons[2], "RIGHT", 6, 0)
    activate:SetText(TEXT.activate)
    activate:SetScript("OnClick", function()
        if SetActiveTalentGroup then
            SetActiveTalentGroup(state.viewed)
        end
    end)
    specButtons.activate = activate

    -- The build as a code, to copy or paste
    shareBox = CreateFrame("EditBox", "TalentTreeShareBox", bar, "InputBoxTemplate")
    shareBox:SetSize(140, 20)
    shareBox:SetPoint("RIGHT", bar, "RIGHT", -26, -2)
    shareBox:SetAutoFocus(false)
    shareBox:SetFontObject("GameFontHighlightSmall")
    shareBox:SetScript("OnEnterPressed", function(self)
        local imported = {}
        if not Editable() or not Parse(self:GetText(), imported) or Validate(imported) then
            Refuse(nil, TEXT.badCode)
        else
            local before = Looks()
            state.pending = imported
            PlaySound(SOUND_LEARN)
            Paint(true)
            Ripple(before)
        end
        self:ClearFocus()
        PaintBar()
    end)
    shareBox:SetScript("OnEscapePressed", function(self)
        self:ClearFocus()
        PaintBar()
    end)
    shareBox:SetScript("OnEditFocusGained", function(self) self:HighlightText() end)
    shareBox:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:AddLine(TEXT.share, 1, 1, 1)
        GameTooltip:AddLine(TEXT.shareHint, 1, 0.82, 0, true)
        GameTooltip:Show()
    end)
    shareBox:SetScript("OnLeave", function() GameTooltip:Hide() end)
    local shareLabel = bar:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    shareLabel:SetPoint("BOTTOMLEFT", shareBox, "TOPLEFT", -4, 1)
    shareLabel:SetText(TEXT.share)

    searchBox = CreateFrame("EditBox", "TalentTreeSearchBox", bar, "InputBoxTemplate")
    searchBox:SetSize(130, 20)
    searchBox:SetPoint("RIGHT", shareBox, "LEFT", -26, 0)
    searchBox:SetAutoFocus(false)
    searchBox:SetText(TEXT.search)
    searchBox:SetTextColor(0.5, 0.5, 0.5)
    searchBox:SetScript("OnEditFocusGained", function(self)
        if self:GetText() == TEXT.search then
            self:SetText("")
            self:SetTextColor(1, 1, 1)
        end
    end)
    searchBox:SetScript("OnEditFocusLost", function(self)
        if self:GetText() == "" then
            self:SetText(TEXT.search)
            self:SetTextColor(0.5, 0.5, 0.5)
        end
    end)
    searchBox:SetScript("OnTextChanged", ApplySearch)
    searchBox:SetScript("OnEscapePressed", function(self)
        self:SetText("")
        self:ClearFocus()
    end)
    searchBox:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)
    local glass = bar:CreateTexture(nil, "OVERLAY")
    glass:SetTexture("Interface\\Common\\UI-Searchbox-Icon")
    glass:SetSize(14, 14)
    glass:SetPoint("RIGHT", searchBox, "LEFT", -6, 0)
end

local CreateSpecPage

-- The specialization page (a class with more than one) ---------------------------------------------------------------

-- UI-LFG-ICON-ROLES is a 256 texel sheet of 67 texel cells (LFGFrame.lua's GetTexCoordsForRole)
local ROLE_TOKENS = { tank = "TANK", healer = "HEALER", damage = "DAMAGER" }
local ROLE_CELLS = { TANK = { 1, 2 }, HEALER = { 2, 1 }, DAMAGER = { 2, 2 } }
local function RoleCoords(role)
    local token = ROLE_TOKENS[role] or "DAMAGER"
    if GetTexCoordsForRole then
        return GetTexCoordsForRole(token)
    end
    local cell, size = ROLE_CELLS[token], 67 / 256
    return (cell[1] - 1) * size, cell[1] * size, (cell[2] - 1) * size, cell[2] * size
end
local CARD_WIDTH, CARD_HEIGHT, CARD_GAP = 340, 560, 70

local function ShowPage(name)
    if not specPage then
        return
    end
    HideFlyout()
    local onSpec = name == "spec"
    SetShown(specPage, onSpec)
    SetShown(bar, not onSpec)
    PanelTemplates_SetTab(frame, onSpec and 1 or 2)
    Paint(false)
    if onSpec then
        specPage.Refresh(false)
        specPage.Enter()
    end
end

CreateSpecPage = function()
    specPage = CreateFrame("Frame", nil, frame)
    specPage:SetAllPoints(frame)
    specPage:SetFrameLevel(frame:GetFrameLevel() + 5)
    specPage:Hide()

    -- The painting steps back behind the cards
    local dim = specPage:CreateTexture(nil, "BACKGROUND")
    dim:SetTexture("Interface\\Buttons\\WHITE8X8")
    dim:SetPoint("TOPLEFT", 2, -2)
    dim:SetPoint("BOTTOMRIGHT", -2, 2)
    dim:SetGradientAlpha("VERTICAL", 0, 0, 0, 0.85, 0, 0, 0, 0.55)

    local heading = specPage:CreateFontString(nil, "OVERLAY")
    heading:SetFont(FRIZ, 22)
    heading:SetShadowOffset(1, -1)
    heading:SetShadowColor(0, 0, 0, 1)
    heading:SetTextColor(1, 0.95, 0.85)
    heading:SetPoint("TOP", specPage, "TOP", 0, -46)
    heading:SetText(TEXT.specTab:upper())

    local hint = specPage:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    hint:SetPoint("TOP", heading, "BOTTOM", 0, -6)
    hint:SetText(TEXT.specHint)

    local cards = {}
    local total = #specs * CARD_WIDTH + (#specs - 1) * CARD_GAP
    for index, tree in ipairs(specs) do
        local card = CreateFrame("Button", nil, specPage)
        card:SetSize(CARD_WIDTH, CARD_HEIGHT)
        card:SetPoint("TOPLEFT", specPage, "TOP", -total / 2 + (index - 1) * (CARD_WIDTH + CARD_GAP), -100)
        card.tree = tree

        local ground = card:CreateTexture(nil, "BACKGROUND")
        ground:SetTexture("Interface\\Buttons\\WHITE8X8")
        ground:SetAllPoints()
        ground:SetVertexColor(0.02, 0.03, 0.02, 1)

        -- The figure: the right of the specialization's painting
        local key = classId .. "-" .. tree.def.id
        local piece = ART.pieces["spec-" .. key]
        local art = card:CreateTexture(nil, "ARTWORK")
        art:SetPoint("TOP", card, "TOP", 0, -1)
        if piece then
            art:SetTexture(ART.specArt[key])
            local height = min(CARD_HEIGHT - 170, (CARD_WIDTH - 2) * piece.h / piece.w)
            art:SetSize(CARD_WIDTH - 2, height)
            -- Cut from the top to the height shown
            art:SetTexCoord(0, piece.r, 0, piece.b * height / ((CARD_WIDTH - 2) * piece.h / piece.w))
        else
            art:SetSize(CARD_WIDTH - 2, CARD_HEIGHT - 170)
            art:SetTexture("Interface\\Buttons\\WHITE8X8")
            art:SetVertexColor(0.05, 0.06, 0.05)
        end
        card.art = art
        local fade = card:CreateTexture(nil, "ARTWORK", nil, 2)
        fade:SetTexture("Interface\\Buttons\\WHITE8X8")
        fade:SetPoint("BOTTOMLEFT", art, "BOTTOMLEFT")
        fade:SetPoint("BOTTOMRIGHT", art, "BOTTOMRIGHT")
        fade:SetHeight(110)
        fade:SetGradientAlpha("VERTICAL", 0.02, 0.03, 0.02, 1, 0.02, 0.03, 0.02, 0)

        -- Its edge: gold for the active one, and a glow that breathes on hover
        local edges = {}
        for side = 1, 4 do
            local edge = card:CreateTexture(nil, "OVERLAY")
            edge:SetTexture("Interface\\Buttons\\WHITE8X8")
            if side <= 2 then
                edge:SetHeight(2)
                edge:SetPoint(side == 1 and "TOPLEFT" or "BOTTOMLEFT")
                edge:SetPoint(side == 1 and "TOPRIGHT" or "BOTTOMRIGHT")
            else
                edge:SetWidth(2)
                edge:SetPoint(side == 3 and "TOPLEFT" or "TOPRIGHT")
                edge:SetPoint(side == 3 and "BOTTOMLEFT" or "BOTTOMRIGHT")
            end
            edges[side] = edge
        end
        card.edges = edges
        -- Hovered, a card comes forward: its painting in full colour, its edge lit
        card.focus = 0
        function card:Focus(amount)
            self.focus = amount
            local isActive = self.isActive
            self.art:SetDesaturated(not isActive and amount < 0.5)
            self.art:SetAlpha((isActive and 1 or 0.7) + 0.3 * amount)
            for _, edge in ipairs(self.edges) do
                if isActive then
                    edge:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.85 + 0.15 * amount)
                else
                    edge:SetVertexColor(0.35 + 0.35 * amount, 0.4 + 0.45 * amount, 0.3 + 0.1 * amount,
                        0.6 + 0.4 * amount)
                end
            end
        end

        local name = card:CreateFontString(nil, "OVERLAY")
        name:SetFont(FRIZ, 26)
        name:SetShadowOffset(1, -1)
        name:SetShadowColor(0, 0, 0, 1)
        name:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        name:SetPoint("TOP", art, "BOTTOM", 0, 30)
        name:SetText(tree.def.name)

        local role = tree.def.role or "damage"
        local roleIcon = card:CreateTexture(nil, "OVERLAY")
        roleIcon:SetTexture("Interface\\LFGFrame\\UI-LFG-ICON-ROLES")
        roleIcon:SetTexCoord(RoleCoords(role))
        roleIcon:SetSize(22, 22)
        local roleText = card:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        roleText:SetText(TEXT.roles[role] or role)
        roleText:SetPoint("TOP", name, "BOTTOM", 12, -6)
        roleIcon:SetPoint("RIGHT", roleText, "LEFT", -4, 0)

        local description = card:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        description:SetWidth(CARD_WIDTH - 40)
        description:SetJustifyH("CENTER")
        description:SetPoint("TOP", roleText, "BOTTOM", -12, -10)
        description:SetTextColor(0.82, 0.8, 0.72)
        description:SetText(tree.def.description or "")

        local activate = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
        activate:SetSize(160, 28)
        activate:SetPoint("BOTTOM", card, "BOTTOM", 0, 18)
        activate:SetText(TEXT.activateSpec)
        activate:SetScript("OnClick", function()
            if UnitAffectingCombat("player") then
                return Refuse(nil, TEXT.combat)
            end
            PlaySound("igMainMenuOptionCheckBoxOn")
            activate:Disable()
            Send("SPEC\t" .. tree.def.id)
        end)
        card.activate = activate

        local active = card:CreateFontString(nil, "OVERLAY")
        active:SetFont(FRIZ, 15)
        active:SetShadowOffset(1, -1)
        active:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
        active:SetPoint("BOTTOM", card, "BOTTOM", 8, 25)
        active:SetText(TEXT.activeSpec)
        local check = card:CreateTexture(nil, "OVERLAY")
        check:SetTexture("Interface\\RAIDFRAME\\ReadyCheck-Ready")
        check:SetSize(18, 18)
        check:SetPoint("RIGHT", active, "LEFT", -5, 0)
        card.active, card.check = active, check

        card:SetScript("OnEnter", function(self)
            PlaySound(SOUND_HOVER)
            local from = self.focus
            Tween(0.2, 0, function(p) self:Focus(from + (1 - from) * p) end, nil, "focus" .. tostring(self))
        end)
        card:SetScript("OnLeave", function(self)
            local from = self.focus
            Tween(0.25, 0, function(p) self:Focus(from * (1 - p)) end, nil, "focus" .. tostring(self))
        end)
        cards[index] = card
    end

    -- Which card is the talent slot's specialization; `animate` when it just changed
    function specPage.Refresh(animate)
        local current = SpecOf(state.spec)
        for _, card in ipairs(cards) do
            local isActive = card.tree == current
            SetShown(card.activate, not isActive)
            card.activate:Enable()
            SetShown(card.active, isActive)
            SetShown(card.check, isActive)
            card.isActive = isActive
            card:Focus(card:IsMouseOver() and 1 or 0)
            -- Just chosen: the card swells a moment
            if animate and isActive then
                Tween(0.6, 0, function(p)
                    local swell = p < 0.3 and p / 0.3 or 1 - (p - 0.3) / 0.7
                    card:SetScale(1 + 0.04 * swell)
                end, function() card:SetScale(1) end, "swell" .. tostring(card))
            end
        end
    end

    -- The cards come up one after the other
    function specPage.Enter()
        for index, card in ipairs(cards) do
            card:SetAlpha(0)
            local x = -total / 2 + (index - 1) * (CARD_WIDTH + CARD_GAP)
            Tween(0.45, 0.06 + 0.1 * index, function(p)
                card:SetAlpha(p)
                card:SetPoint("TOPLEFT", specPage, "TOP", x, -100 - 30 * (1 - OutCubic(p)))
            end, function() card:SetAlpha(1) end, "enter" .. tostring(card))
        end
    end

    -- The tabs under the window, as on the character sheet
    local tabs = {}
    for index, label in ipairs({ TEXT.specTab, TEXT.talentsTab }) do
        local tab = CreateFrame("Button", "TalentTreeFrameTab" .. index, frame, "CharacterFrameTabButtonTemplate")
        tab:SetID(index)
        -- The window is drawn smaller than the screen; its tabs are drawn back up to a readable size
        tab:SetScale(1.35)
        tab:SetText(label)
        PanelTemplates_TabResize(tab, 12)
        if index == 1 then
            tab:SetPoint("TOPLEFT", frame, "BOTTOMLEFT", 24, 4)
        else
            tab:SetPoint("LEFT", tabs[index - 1], "RIGHT", -16, 0)
        end
        tab:SetScript("OnClick", function(self)
            PlaySound("igCharacterInfoTab")
            ShowPage(self:GetID() == 1 and "spec" or "talents")
        end)
        tabs[index] = tab
    end
    PanelTemplates_SetNumTabs(frame, 2)
    PanelTemplates_SetTab(frame, 2)
end

local function CreateWindow()
    frame = CreateFrame("Frame", "TalentTreeFrame", UIParent)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("CENTER", 0, 10)
    frame:SetFrameStrata("HIGH")
    frame:SetToplevel(true)
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:SetClampedToScreen(true)
    frame:Hide()

    local ground = frame:CreateTexture(nil, "BACKGROUND")
    ground:SetTexture("Interface\\Buttons\\WHITE8X8")
    ground:SetVertexColor(0.02, 0.03, 0.05)
    ground:SetPoint("TOPLEFT", 2, -2)
    ground:SetPoint("BOTTOMRIGHT", -2, 2)

    local art = frame:CreateTexture(nil, "BACKGROUND", nil, 1)
    art:SetPoint("TOPLEFT", 2, -2)
    art:SetPoint("BOTTOMRIGHT", -2, 2)
    frame.art = art
    -- Set again on every show: on a cold texture cache the first SetTexture of this large art sometimes never
    -- draws until the texture is set anew
    frame:HookScript("OnShow", function()
        frame.shownBackground = nil
        PaintBackground()
    end)

    -- The trees read over the art: a soft darkening under each
    for side = 1, 2 do
        local shade = frame:CreateTexture(nil, "BACKGROUND", nil, 2)
        shade:SetTexture("Interface\\Buttons\\WHITE8X8")
        shade:SetPoint("TOP", frame, "TOP", 0, -2)
        shade:SetPoint("BOTTOM", frame, "BOTTOM", 0, 2)
        shade:SetWidth(WIDTH / 2 - 2)
        if side == 1 then
            shade:SetPoint("LEFT", frame, "LEFT", 2, 0)
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.55, 0, 0, 0, 0.1)
        else
            shade:SetPoint("RIGHT", frame, "RIGHT", -2, 0)
            shade:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.1, 0, 0, 0, 0.35)
        end
    end

    RetailUI.ApplyNineSlice(frame, true)

    portrait = frame:CreateTexture(nil, "OVERLAY", nil, -1)
    portrait:SetTexture(ROUND_CLASSES)
    local cell = CustomClasses[classId] and CustomClasses[classId].iconCell or { 0, 0 }
    portrait:SetTexCoord(cell[1] / 4, (cell[1] + 1) / 4, cell[2] / 4, (cell[2] + 1) / 4)
    portrait:SetSize(58, 58)
    portrait:SetPoint("TOPLEFT", frame, "TOPLEFT", -6, 8)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", frame, "TOP", 0, -5)
    title:SetText(TEXT.title)

    local close = CreateFrame("Button", nil, frame)
    close:SetSize(24, 24)
    close:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -5, -5)
    close:SetFrameLevel(frame:GetFrameLevel() + 30)
    close:SetNormalTexture(RetailUIAtlas["redbutton-exit-2x"][1])
    RetailUI.SetAtlas(close:GetNormalTexture(), "redbutton-exit-2x")
    close:SetPushedTexture(RetailUIAtlas["redbutton-exit-pressed-2x"][1])
    RetailUI.SetAtlas(close:GetPushedTexture(), "redbutton-exit-pressed-2x")
    close:SetHighlightTexture(RetailUIAtlas["redbutton-highlight-2x"][1])
    RetailUI.SetAtlas(close:GetHighlightTexture(), "redbutton-highlight-2x")
    close:GetHighlightTexture():SetBlendMode("ADD")
    close:SetScript("OnClick", function() frame:Hide() end)

    local mover = CreateFrame("Frame", nil, frame)
    mover:SetPoint("TOPLEFT", frame, "TOPLEFT", 50, 0)
    mover:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -40, 0)
    mover:SetHeight(24)
    mover:SetFrameLevel(frame:GetFrameLevel() + 25)
    mover:EnableMouse(true)
    mover:RegisterForDrag("LeftButton")
    mover:SetScript("OnDragStart", function() frame:StartMoving() end)
    mover:SetScript("OnDragStop", function() frame:StopMovingOrSizing() end)

    for _, tree in ipairs(trees) do
        CreateTree(tree)
    end
    CreateBar()
    CreateFlyout()

    local embers = frame:CreateTexture(nil, "OVERLAY", nil, 5)
    Piece(embers, "anim-particles")
    embers:SetBlendMode("ADD")
    embers:SetSize(WIDTH - 40, (WIDTH - 40) * ART.pieces["anim-particles"].h / ART.pieces["anim-particles"].w)
    embers:Hide()
    frame.embers = embers

    local banner = CreateFrame("Frame", nil, frame)
    banner:SetSize(360, 50)
    banner:SetPoint("CENTER", frame, "CENTER", 0, 40)
    banner:SetFrameLevel(frame:GetFrameLevel() + 40)
    local bannerGround = banner:CreateTexture(nil, "BACKGROUND")
    bannerGround:SetTexture("Interface\\Buttons\\WHITE8X8")
    bannerGround:SetAllPoints()
    bannerGround:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0)
    local left = banner:CreateTexture(nil, "BACKGROUND")
    left:SetTexture("Interface\\Buttons\\WHITE8X8")
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMRIGHT", banner, "BOTTOM")
    left:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0, 0, 0, 0, 0.75)
    local right = banner:CreateTexture(nil, "BACKGROUND")
    right:SetTexture("Interface\\Buttons\\WHITE8X8")
    right:SetPoint("TOPLEFT", banner, "TOP")
    right:SetPoint("BOTTOMRIGHT")
    right:SetGradientAlpha("HORIZONTAL", 0, 0, 0, 0.75, 0, 0, 0, 0)
    local bannerText = banner:CreateFontString(nil, "OVERLAY")
    bannerText:SetFont(FRIZ, 22)
    bannerText:SetShadowOffset(1, -1)
    bannerText:SetShadowColor(0, 0, 0, 1)
    bannerText:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    bannerText:SetPoint("CENTER")
    banner.text = bannerText
    banner:Hide()
    frame.banner = banner

    if #specs > 1 then
        CreateSpecPage()
    end

    -- Learnable nodes breathe while there are points, the apply button pulses while there are changes
    local clock = 0
    frame:SetScript("OnUpdate", function(_, elapsed)
        clock = clock + elapsed
        -- No answer: ask once more, then say why nothing can be edited (a server without the talent trees)
        if not state.signature and state.askedAt then
            local waited = GetTime() - state.askedAt
            if waited > 2.5 and not state.askedAgain then
                state.askedAgain = true
                Send("OPEN")
            elseif waited > 6 and not state.silent then
                state.silent = true
                PaintBar()
            end
        end
        local pulse = 0.5 + 0.5 * math.sin(clock * 3)
        for _, node in ipairs(nodes) do
            if not node.glowBusy and node.button then
                if node.look == "learnable" then
                    node.button.glow:SetAlpha(0.15 + 0.3 * pulse)
                elseif node.changed then
                    node.button.glow:SetAlpha(0.3 + 0.35 * pulse)
                end
                if node.searched then
                    node.button.search:SetAlpha(0.5 + 0.5 * pulse)
                end
            end
        end
        if applyButton.glow:IsShown() then
            applyButton.glow:SetAlpha(0.15 + 0.5 * pulse)
        end
    end)

    frame:SetScript("OnShow", function()
        PlaySound(SOUND_OPEN)
        local fit = min(1, (UIParent:GetHeight() - 30) / HEIGHT, (UIParent:GetWidth() - 30) / WIDTH)
        fit = fit * WINDOW_SCALE
        frame:SetScale(fit)
        frame:SetAlpha(0)
        Tween(0.25, 0, function(p)
            frame:SetAlpha(p)
            frame:SetScale(fit * (0.95 + 0.05 * OutCubic(p)))
        end, nil, frame)
        -- The trees assemble, row by row, each node landing a moment after the one above
        for _, node in ipairs(nodes) do
            local holder = node.button.holder
            holder:SetAlpha(0)
            holder:SetScale(0.5)
            local delay = 0.08 + node.def.row * 0.035 + (node.tree.side - 1) * 0.06
            -- Its own key: hovering a node while the window opens must not cancel its fade-in halfway
            Tween(0.3, delay, function(p)
                holder:SetAlpha(min(1, p * 2))
                holder:SetScale(max(0.01, 0.5 + 0.5 * OutBack(p)))
            end, function()
                holder:SetAlpha(1)
                holder:SetScale(node.button.hovered and 1.1 or 1)
            end, "intro" .. node.position)
        end
        for _, tree in ipairs(trees) do
            for _, link in ipairs(tree.links) do
                local texture = link.texture
                local target = texture:GetAlpha()
                texture:SetAlpha(0)
                Tween(0.3, 0.12 + link.child.def.row * 0.035 + (tree.side - 1) * 0.06, function(p)
                    texture:SetAlpha(target * p)
                end)
            end
        end
        if not state.signature then
            state.askedAt, state.askedAgain, state.silent = GetTime(), nil, nil
        end
        Send("OPEN")
    end)
    frame:SetScript("OnHide", function()
        PlaySound(SOUND_CLOSE)
        HideFlyout()
        GameTooltip:Hide()
    end)

    tinsert(UISpecialFrames, "TalentTreeFrame")
end

-- Opening, and the talent key -------------------------------------------------------------------------------------

function TalentTree_Toggle()
    if not HasTree() then
        return
    end
    if not frame then
        CreateWindow()
    end
    if frame:IsShown() then
        frame:Hide()
    else
        state.viewed = state.spec
        Paint(false)
        frame:Show()
    end
end

-- The talent key and the micro button both come through ToggleTalentFrame: a class with trees opens them instead
local stockToggleTalentFrame = ToggleTalentFrame
function ToggleTalentFrame(...)
    if HasTree() then
        return TalentTree_Toggle()
    end
    return stockToggleTalentFrame(...)
end

-- Unspent points: the micro button glows until they are spent
local microGlow
local function UpdateMicroButton()
    if not TalentMicroButton or not HasTree() then
        return
    end
    if not microGlow then
        microGlow = TalentMicroButton:CreateTexture(nil, "OVERLAY")
        microGlow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        microGlow:SetBlendMode("ADD")
        microGlow:SetVertexColor(0.3, 1, 0.35)
        microGlow:SetPoint("CENTER", TalentMicroButton, "CENTER", 0, -9)
        microGlow:SetSize(52, 64)
        local clock = 0
        local pulse = CreateFrame("Frame", nil, TalentMicroButton)
        pulse:SetScript("OnUpdate", function(_, elapsed)
            clock = clock + elapsed
            microGlow:SetAlpha(0.35 + 0.35 * math.sin(clock * 3))
        end)
        microGlow.pulse = pulse
    end
    local unspent = false
    for _, tree in ipairs(trees) do
        if (tree.side == 1 or tree == SpecOf(state.spec)) and Available(state.builds[state.spec], tree) > 0 then
            unspent = true
        end
    end
    SetShown(microGlow, unspent and state.signature ~= nil)
    SetShown(microGlow.pulse, unspent and state.signature ~= nil)
end

-- Messages ----------------------------------------------------------------------------------------------------------

local function OnState(signature, spec, specCount, level, applied, first, second, firstSpec, secondSpec)
    local previousLevel = state.level
    local previousSpecTree = state.specTrees[(tonumber(spec) or 0) + 1]
    state.specTrees[1] = tonumber(firstSpec) or state.specTrees[1]
    state.specTrees[2] = tonumber(secondSpec) or state.specTrees[2]
    state.signature = tonumber(signature)
    state.spec = (tonumber(spec) or 0) + 1
    state.specCount = tonumber(specCount) or 1
    state.level = tonumber(level) or UnitLevel("player")
    local wasDirty = frame and IsDirty()
    local oldCommitted = Copy(state.builds[state.spec])
    Parse(first, state.builds[1])
    Parse(second, state.builds[2])
    if state.viewed > state.specCount then
        state.viewed = state.spec
    end

    if applied == "1" then
        local changed = {}
        for index = 1, #nodes do
            if (state.builds[state.spec][index] or 0) ~= (oldCommitted[index] or 0) then
                changed[index] = (state.builds[state.spec][index] or 0) > (oldCommitted[index] or 0)
            end
        end
        state.pending = Copy(state.builds[state.spec])
        state.applying = nil
        -- A specialization taken rather than talents applied: its name is the news
        local specTree = state.specTrees[state.spec]
        if previousSpecTree and previousSpecTree ~= 0 and specTree ~= previousSpecTree then
            changed.bannerText = format(TEXT.specChanged, SpecById(specTree).def.name)
        end
        if frame and frame:IsShown() then
            Paint(true)
            Celebrate(changed)
            if specPage and specPage:IsShown() then
                specPage.Refresh(true)
            end
        end
    elseif not wasDirty then
        state.pending = Copy(state.builds[state.spec])
        if frame and frame:IsShown() then
            Paint(state.level ~= previousLevel)
        end
    elseif frame and frame:IsShown() then
        Paint(false)
    end
    UpdateMicroButton()
end

local function OnError(code, nodeId)
    state.applying = nil
    local node = byId[tonumber(nodeId) or 0]
    Refuse(node and node.button and node or nil, TEXT.errors[tonumber(code)] or TEXT.errors[1])
    if frame and frame:IsShown() then
        PaintBar()
    end
end

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:RegisterEvent("PLAYER_ENTERING_WORLD")
listener:RegisterEvent("PLAYER_REGEN_ENABLED")
listener:RegisterEvent("PLAYER_REGEN_DISABLED")
listener:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "CHAT_MSG_ADDON" then
        if prefix ~= PREFIX or sender ~= UnitName("player") or not HasTree() then
            return
        end
        local kind, a, b, c, d, e, f, g, h, i = strsplit("\t", message)
        if kind == "S" then
            OnState(a, b, c, d, e, f, g, h, i)
        elseif kind == "E" then
            OnError(a, b)
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        if HasTree() then
            state.level = UnitLevel("player")
            Send("OPEN")
            -- Warm the background into the texture cache, so the first open draws it
            if not listener.warm then
                listener.warm = {}
                for key, file in pairs(ART.backgrounds) do
                    if key == classId or tostring(key):match("^" .. classId .. "%-") then
                        local texture = listener:CreateTexture()
                        texture:SetTexture(file)
                        tinsert(listener.warm, texture)
                    end
                end
            end
        end
    elseif frame and frame:IsShown() then
        -- Combat starts or ends: the apply button follows
        PaintBar()
    end
end)
