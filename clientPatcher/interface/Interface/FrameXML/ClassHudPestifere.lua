-- The Pestiféré's HUD (mod-pestifere, the tank; a Sangsue healer has none yet), on the Faucheur's model: two corroded
-- cleavers lying mirrored, three flasks over them - the plagues it carries (Virulence): Carapace nécrosée bone-grey,
-- Chair putride flesh-red, Peste virulente bile-green, each one's liquid lowering as it runs out (out of combat only:
-- in a fight they last as long as they are carried). The boil in the cleavers' knot is Pourriture on the target -
-- flat, small, swollen, and ripe at 6, throbbing: Détonation now; it splashes when a Détonation blows something up.
-- Avatar de la peste: the flasks boil over, a toxic glow behind them. Riposte purulente usable (a dodge, parry or
-- block just now): bile drips from the blades. Sépulcre (damage held back): a violet smoke behind, thicker as it
-- owes more. Fed by the server's
-- "PESTIFERE\t<carapace %>:<chair %>:<peste %>:<pourriture>:<avatar>:<riposte>:<sepulcre %>:<detonations>:<ripostes>"
-- (modules/mod-pestifere SyncHud), or "PESTIFERE\t-" for a healer: the HUD hides. Art:
-- clientPatcher/assets/pestifereHud (its assetManifest.json gives the boxes and placements below), painted from
-- .agents/plans/pestifere-hud/pestifere-hud.ASSETS.md. Framework: ClassHud.lua.

local ATLAS = "Interface\\ClassHud\\pestifereHudAtlas"
local ATLAS_WIDTH, ATLAS_HEIGHT = 1024, 512

-- The atlas's pieces: their pixel boxes { left, right, top, bottom }
local PIECES = {
    frame = { 0, 512, 0, 224 },
    bileDrips = { 512, 1024, 0, 224 },
    flaskEmpty = { 0, 132, 228, 348 },
    flaskBone = { 136, 268, 228, 348 },
    flaskFlesh = { 272, 404, 228, 348 },
    flaskBile = { 408, 540, 228, 348 },
    boiling = { 544, 676, 228, 348 },
    splash = { 680, 808, 228, 356 },
    glowToxic = { 0, 212, 360, 468 },
    glowSepulcre = { 216, 428, 360, 468 },
    boilFlat = { 432, 496, 360, 424 },
    boilSmall = { 500, 564, 360, 424 },
    boilSwollen = { 568, 632, 360, 424 },
    boilRipe = { 636, 700, 360, 424 },
}

-- The flask keeps its painted proportions (132 x 120); its liquid spans these fractions of it, top to bottom
-- (assetManifest.json "flaskFillBounds": rows 43 to 102 of 120)
local FLASK_WIDTH, FLASK_HEIGHT = 40, 37
local FLASK_OFFSETS = { -40, -2, 36 }
local LIQUID_TOP, LIQUID_BOTTOM = 43 / 120, 102 / 120
-- Left to right: the payload's order, the plagues' spell ids'
local PLAGUES = {
    { field = "carapace", piece = "flaskBone", name = "Carapace nécrosée" },
    { field = "chair", piece = "flaskFlesh", name = "Chair putride" },
    { field = "peste", piece = "flaskBile", name = "Peste virulente" },
}
local BOIL_SIZE = 12
local BOIL_Y = -17
local POURRITURE_MAX = 6
-- The ripe boil's throb: how much bigger at its peak, and how long a beat lasts (seconds)
local THROB_SCALE, THROB_PERIOD = 0.18, 0.9
-- Sépulcre's smoke at its thickest once it owes this share of the maximum health
local SEPULCRE_FULL = 50

local function setPiece(texture, piece, top, bottom)
    local box = PIECES[piece]
    top, bottom = top or 0, bottom or 1
    local height = box[4] - box[3]
    texture:SetTexture(ATLAS)
    texture:SetTexCoord(box[1] / ATLAS_WIDTH, box[2] / ATLAS_WIDTH, (box[3] + height * top) / ATLAS_HEIGHT,
        (box[3] + height * bottom) / ATLAS_HEIGHT)
end

local function pulse(texture, change, duration)
    local group = texture:CreateAnimationGroup()
    group:SetLooping("BOUNCE")
    local alpha = group:CreateAnimation("Alpha")
    alpha:SetChange(change)
    alpha:SetDuration(duration)
    alpha:SetSmoothing("IN_OUT")
    return group
end

-- A brief swell and settle: a state just gained
local function pop(texture, scale)
    local group = texture:CreateAnimationGroup()
    local grow = group:CreateAnimation("Scale")
    grow:SetScale(scale, scale)
    grow:SetDuration(0.12)
    grow:SetOrder(1)
    local settle = group:CreateAnimation("Scale")
    settle:SetScale(1 / scale, 1 / scale)
    settle:SetDuration(0.18)
    settle:SetOrder(2)
    return group
end

-- An additive copy flashing in and out
local function burst(texture, peak, duration)
    texture:SetAlpha(0)
    local group = texture:CreateAnimationGroup()
    local show = group:CreateAnimation("Alpha")
    show:SetChange(peak)
    show:SetDuration(0.08)
    show:SetOrder(1)
    local fade = group:CreateAnimation("Alpha")
    fade:SetChange(-peak)
    fade:SetDuration(duration)
    fade:SetOrder(2)
    return group
end

local function createFlask(parent, index)
    local flask = CreateFrame("Frame", nil, parent)
    flask:SetSize(FLASK_WIDTH, FLASK_HEIGHT)
    flask:SetPoint("CENTER", parent, "CENTER", FLASK_OFFSETS[index], 2)
    -- The left one over the middle one, the middle one over the right one
    flask:SetFrameLevel(parent:GetFrameLevel() + 4 - index)
    local piece = PLAGUES[index].piece

    -- Layers, not sublevels (3.3.5 ignores those): the glass, its liquid, the effects over them
    flask.glass = flask:CreateTexture(nil, "BORDER")
    flask.glass:SetAllPoints()
    setPiece(flask.glass, "flaskEmpty")

    -- The liquid: the full flask cut from the top down to the time the plague has left
    flask.liquid = flask:CreateTexture(nil, "ARTWORK")
    flask.liquid:SetPoint("BOTTOMLEFT")
    flask.liquid:SetPoint("BOTTOMRIGHT")
    flask.liquid:SetHeight(FLASK_HEIGHT)
    setPiece(flask.liquid, piece)
    flask.liquid:Hide()
    flask.pop = pop(flask.liquid, 1.25)

    -- Inoculated: a flash of its liquid over it
    flask.flash = flask:CreateTexture(nil, "OVERLAY")
    flask.flash:SetPoint("CENTER")
    flask.flash:SetSize(FLASK_WIDTH * 1.3, FLASK_HEIGHT * 1.3)
    setPiece(flask.flash, piece)
    flask.flash:SetBlendMode("ADD")
    flask.burst = burst(flask.flash, 0.8, 0.4)

    -- Avatar: boiling over
    flask.boiling = flask:CreateTexture(nil, "OVERLAY")
    flask.boiling:SetAllPoints()
    setPiece(flask.boiling, "boiling")
    flask.boiling:SetBlendMode("ADD")
    flask.boiling:SetAlpha(0)
    flask.boilPulse = pulse(flask.boiling, -0.5, 0.35)

    -- Lost (spent by Détonation or Purge cathartique, or run out): its liquid fades upwards
    flask.ghost = flask:CreateTexture(nil, "OVERLAY")
    flask.ghost:SetAllPoints()
    setPiece(flask.ghost, piece)
    flask.ghost:SetAlpha(0)
    flask.vanish = flask.ghost:CreateAnimationGroup()
    local appear = flask.vanish:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local away = flask.vanish:CreateAnimation("Alpha")
    away:SetChange(-1)
    away:SetDuration(0.45)
    away:SetOrder(2)
    local rise = flask.vanish:CreateAnimation("Translation")
    rise:SetOffset(0, 10)
    rise:SetDuration(0.45)
    rise:SetOrder(2)
    return flask
end

local function glow(frame, piece, change, duration)
    local texture = frame:CreateTexture(nil, "BACKGROUND")
    texture:SetPoint("CENTER", 0, 2)
    texture:SetSize(146, 60)
    setPiece(texture, piece)
    texture:SetBlendMode("ADD")
    texture:SetAlpha(0)
    return texture, pulse(texture, change, duration)
end

local function create(frame)
    -- Behind everything: Sépulcre's smoke, the Avatar's toxic glow over it
    frame.glowSepulcre, frame.sepulcrePulse = glow(frame, "glowSepulcre", -0.35, 1.6)
    frame.glowToxic, frame.toxicPulse = glow(frame, "glowToxic", -0.5, 0.9)

    frame.border = frame:CreateTexture(nil, "BORDER")
    frame.border:SetPoint("CENTER", 0, -8)
    frame.border:SetSize(154, 63)
    setPiece(frame.border, "frame")

    -- Riposte purulente usable: bile dripping from the blades
    frame.drips = frame:CreateTexture(nil, "ARTWORK")
    frame.drips:SetPoint("CENTER", 0, -8)
    frame.drips:SetSize(154, 63)
    setPiece(frame.drips, "bileDrips")
    frame.drips:SetBlendMode("ADD")
    frame.drips:SetAlpha(0)
    frame.dripPulse = pulse(frame.drips, -0.55, 0.8)
    -- Riposte purulente cast (the rotation casts it the moment it is usable): a burst of bile, held, then dripping off
    frame.dripBurst = frame:CreateTexture(nil, "ARTWORK")
    frame.dripBurst:SetPoint("CENTER", 0, -8)
    frame.dripBurst:SetSize(154, 63)
    setPiece(frame.dripBurst, "bileDrips")
    frame.dripBurst:SetBlendMode("ADD")
    frame.dripBurst:SetAlpha(0)
    frame.riposteBurst = frame.dripBurst:CreateAnimationGroup()
    local rise = frame.riposteBurst:CreateAnimation("Alpha")
    rise:SetChange(1)
    rise:SetDuration(0.1)
    rise:SetOrder(1)
    local hold = frame.riposteBurst:CreateAnimation("Alpha")
    hold:SetChange(0)
    hold:SetDuration(0.5)
    hold:SetOrder(2)
    local drop = frame.riposteBurst:CreateAnimation("Alpha")
    drop:SetChange(-1)
    drop:SetDuration(0.8)
    drop:SetSmoothing("IN")
    drop:SetOrder(3)

    frame.flasks = {}
    for index = 1, 3 do
        frame.flasks[index] = createFlask(frame, index)
    end

    -- The boil in the knot, over the flasks
    local knot = CreateFrame("Frame", nil, frame)
    knot:SetAllPoints()
    knot:SetFrameLevel(frame:GetFrameLevel() + 5)
    frame.boil = knot:CreateTexture(nil, "ARTWORK")
    frame.boil:SetPoint("CENTER", 0, BOIL_Y)
    frame.boil:SetSize(BOIL_SIZE, BOIL_SIZE)
    setPiece(frame.boil, "boilFlat")
    frame.boilPop = pop(frame.boil, 1.35)
    -- Ripe: it throbs, resized by hand on every frame - a looping Scale animation flashed the boil huge for a frame
    -- at each turn (BOUNCE) or each restart (REPEAT) on this client
    frame.throbber = CreateFrame("Frame", nil, knot)
    frame.throbber:Hide()
    frame.throbber:SetScript("OnUpdate", function(self, elapsed)
        self.time = (self.time or 0) + elapsed
        local swell = 1 + THROB_SCALE * (1 - math.cos(self.time * 2 * math.pi / THROB_PERIOD)) / 2
        frame.boil:SetSize(BOIL_SIZE * swell, BOIL_SIZE * swell)
    end)

    -- Détonation: bile bursting out of the boil
    frame.splash = knot:CreateTexture(nil, "OVERLAY")
    frame.splash:SetPoint("CENTER", 0, BOIL_Y)
    frame.splash:SetSize(28, 28)
    setPiece(frame.splash, "splash")
    frame.splash:SetBlendMode("ADD")
    frame.splash:SetAlpha(0)
    frame.burst = frame.splash:CreateAnimationGroup()
    local appear = frame.burst:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local swell = frame.burst:CreateAnimation("Scale")
    swell:SetScale(2.5, 2.5)
    swell:SetDuration(0.4)
    swell:SetSmoothing("OUT")
    swell:SetOrder(2)
    local fade = frame.burst:CreateAnimation("Alpha")
    fade:SetChange(-1)
    fade:SetDuration(0.4)
    fade:SetSmoothing("IN")
    fade:SetOrder(2)
end

local function boilPiece(stacks)
    if stacks >= POURRITURE_MAX then
        return "boilRipe"
    elseif stacks >= 3 then
        return "boilSwollen"
    elseif stacks >= 1 then
        return "boilSmall"
    end
    return "boilFlat"
end

-- A glow and its pulse in or out; was: whether it was shown before (nil: the first draw); alpha its strength. The
-- pulse owns the alpha: set first, then the pulse (re)started from it. A fade-in under a starting Alpha animation left
-- the glow at the 0 it started from on this client - never seen in game.
local function showGlow(texture, animation, show, was, alpha)
    alpha = alpha or 1
    if show and (not was or texture.strength ~= alpha) then
        animation:Stop()
        texture:SetAlpha(alpha)
        texture.strength = alpha
        animation:Play()
    elseif not show and was ~= false then
        animation:Stop()
        texture:SetAlpha(0)
        texture.strength = nil
    end
end

-- Whether a state was on at the last message: nil on the first draw
local function was(before, on)
    if not before then
        return nil
    end
    return on and true or false
end

-- The liquid's height in the flask for the time a plague has left
local function setLiquid(flask, piece, left)
    local level = LIQUID_BOTTOM - (LIQUID_BOTTOM - LIQUID_TOP) * left / 100
    -- At 100% the whole painting: the cork and the neck with it
    if left >= 100 then
        level = 0
    end
    flask.liquid:SetHeight(math.max(1, FLASK_HEIGHT * (1 - level)))
    setPiece(flask.liquid, piece, level, 1)
    flask.liquid:Show()
end

local function update(frame, state, previous)
    if state.hidden then
        return
    end
    local before = (previous and not previous.hidden) and previous or nil

    -- The flasks: the plagues carried, their liquid the time they have left; boiling under the Avatar
    for index, flask in ipairs(frame.flasks) do
        local plague = PLAGUES[index]
        local left = state[plague.field]
        local leftBefore = before and before[plague.field] or 0
        if left > 0 then
            setLiquid(flask, plague.piece, left)
            if before and leftBefore == 0 then
                flask.pop:Stop()
                flask.pop:Play()
                flask.burst:Stop()
                flask.burst:Play()
            end
        else
            flask.liquid:Hide()
            if before and leftBefore > 0 then
                flask.vanish:Stop()
                flask.vanish:Play()
            end
        end
        showGlow(flask.boiling, flask.boilPulse, state.avatar and left > 0,
            was(before, before and before.avatar and leftBefore > 0))
    end

    showGlow(frame.glowToxic, frame.toxicPulse, state.avatar, was(before, before and before.avatar))
    showGlow(frame.drips, frame.dripPulse, state.riposte, was(before, before and before.riposte))
    if before and (state.ripostes > before.ripostes or (state.riposte and not before.riposte)) then
        frame.riposteBurst:Stop()
        frame.riposteBurst:Play()
    end
    showGlow(frame.glowSepulcre, frame.sepulcrePulse, state.sepulcre > 0, was(before, before and before.sepulcre > 0),
        0.35 + 0.65 * math.min(1, state.sepulcre / SEPULCRE_FULL))

    -- Pourriture on the target: the boil
    local piece = boilPiece(state.pourriture)
    setPiece(frame.boil, piece)
    local ripe = state.pourriture >= POURRITURE_MAX
    -- Not when it ripens: the throb takes over, two scales at once on one texture compound
    if before and not ripe and piece ~= boilPiece(before.pourriture) and state.pourriture > before.pourriture then
        frame.boilPop:Stop()
        frame.boilPop:Play()
    end
    if ripe and not frame.throbber:IsShown() then
        frame.boilPop:Stop()
        frame.throbber.time = 0
        frame.throbber:Show()
    elseif not ripe and frame.throbber:IsShown() then
        frame.throbber:Hide()
        frame.boil:SetSize(BOIL_SIZE, BOIL_SIZE)
    end

    -- Détonation blew something up since the last message
    if before and state.detonations > before.detonations then
        frame.burst:Stop()
        frame.burst:Play()
    end
end

local function parse(payload)
    if payload == "-" then
        return { hidden = true }
    end
    local carapace, chair, peste, pourriture, avatar, riposte, sepulcre, detonations, ripostes =
        strsplit(":", payload or "")
    if not detonations then
        return nil
    end
    local function percent(value)
        return math.max(0, math.min(100, tonumber(value) or 0))
    end
    return {
        carapace = percent(carapace),
        chair = percent(chair),
        peste = percent(peste),
        pourriture = math.max(0, math.min(POURRITURE_MAX, tonumber(pourriture) or 0)),
        avatar = avatar == "1",
        riposte = riposte == "1",
        sepulcre = percent(sepulcre),
        detonations = tonumber(detonations) or 0,
        ripostes = tonumber(ripostes) or 0,
    }
end

ClassHud_Register({
    token = "PESTIFERE",
    prefix = "PESTIFERE",
    width = 162,
    height = 70,
    anchor = { "TOPLEFT", "PlayerFrame", "BOTTOMLEFT", 86, 8 },
    title = "Pestiféré",
    tooltip = function(state)
        local carried = {}
        for _, plague in ipairs(PLAGUES) do
            if state[plague.field] > 0 then
                table.insert(carried, plague.name)
            end
        end
        local lines = {
            string.format("Virulence : %d%s", #carried + ((state.avatar and #carried > 0) and 1 or 0),
                #carried > 0 and (" - " .. table.concat(carried, ", ")) or ""),
            string.format("Pourriture sur la cible : %d / %d%s", state.pourriture, POURRITURE_MAX,
                state.pourriture >= POURRITURE_MAX and " - |cffb8e02aprête à éclater|r" or ""),
            "Les fioles sont vos pestes ; le furoncle, la Pourriture que votre Détonation fera éclater.",
        }
        if state.avatar then
            table.insert(lines, "|cffb8e02aAvatar de la peste : vos pestes comptent une de plus.|r")
        end
        if state.riposte then
            table.insert(lines, "|cffb8e02aRiposte purulente est utilisable.|r")
        end
        if state.sepulcre > 0 then
            table.insert(lines, string.format("|cffa070c0Sépulcre : %d%% de votre vie encore à subir.|r",
                state.sepulcre))
        end
        return lines
    end,
    create = create,
    parse = parse,
    update = update,
    -- The tank's: hidden until the server says so, and for a Sangsue healer
    isHidden = function(state)
        return not state or state.hidden
    end,
    isEmpty = function(state)
        return state.hidden or (state.carapace == 0 and state.chair == 0 and state.peste == 0 and
            state.pourriture == 0 and not state.avatar and not state.riposte and state.sepulcre == 0)
    end,
})
