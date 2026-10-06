-- The Gladiateur's HUD (mod-warrior, the Warrior's fourth specialization), on the Faucheur's model: two gladii lying
-- mirrored, three gladiator's helmets over them. The helmets fill with the blood of Plaie du gladiateur on the target -
-- each one a third of the way to "ripe", the next one by thirds - and at three full helmets a crimson glow beats behind
-- them: Execute is worth cashing the bleed in. Under Coup de grâce they turn molten gold, a gold glow and energy along
-- the blades. The jewel in the gladii's boss is Revenge: gold under Ouverture, cracked red with Garde brisée; Shield
-- Slam coming back rings out of it. Fed by the server's
-- "GLADIATOR\t<bleed %>:<opening>:<broken guard>:<execute>:<coup de grace>:<duel>:<resets>" (modules/mod-warrior), or
-- "GLADIATOR\t-" for any other Warrior: the HUD hides. Art: clientPatcher/assets/gladiatorHud/v2 (its
-- assetManifest.json gives the boxes and placements below). Framework: ClassHud.lua.

local ATLAS = "Interface\\ClassHud\\gladiatorHudV2Atlas"
local SHOCKWAVE = "Interface\\ClassHud\\gladiatorShockwave"
local ATLAS_WIDTH, ATLAS_HEIGHT = 1024, 512

-- The atlas's pieces: their pixel boxes { left, right, top, bottom }
local PIECES = {
    frame = { 0, 512, 0, 224 },
    bladeEnergy = { 512, 1024, 0, 224 },
    empty = { 0, 152, 228, 336 },
    oneThird = { 156, 308, 228, 336 },
    twoThirds = { 312, 464, 228, 336 },
    full = { 468, 620, 228, 336 },
    gold = { 624, 776, 228, 336 },
    glowCrimson = { 0, 212, 340, 448 },
    glowGold = { 216, 428, 340, 448 },
    jewelDark = { 432, 496, 340, 404 },
    jewelGold = { 500, 564, 340, 404 },
    jewelCrack = { 568, 632, 340, 404 },
}

-- The painted helmet keeps its own proportions (about 1.1 wide for 1 tall) inside its 152 x 108 slot
local HELMET_WIDTH, HELMET_HEIGHT = 56, 40
local HELMET_OFFSETS = { -35, -1, 33 }
-- Each helmet a third of the way to ripe, filled by thirds: nine steps in all
local THIRDS = 3

local function setPiece(texture, piece)
    local box = PIECES[piece]
    texture:SetTexture(ATLAS)
    texture:SetTexCoord(box[1] / ATLAS_WIDTH, box[2] / ATLAS_WIDTH, box[3] / ATLAS_HEIGHT, box[4] / ATLAS_HEIGHT)
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

local function createHelmet(parent, index)
    local helmet = CreateFrame("Frame", nil, parent)
    helmet:SetSize(HELMET_WIDTH, HELMET_HEIGHT)
    helmet:SetPoint("CENTER", parent, "CENTER", HELMET_OFFSETS[index], 2)
    -- The left one over the middle one, the middle one over the right one
    helmet:SetFrameLevel(parent:GetFrameLevel() + 4 - index)

    helmet.fill = helmet:CreateTexture(nil, "ARTWORK")
    helmet.fill:SetAllPoints()
    setPiece(helmet.fill, "empty")
    helmet.pop = pop(helmet.fill, 1.25)

    -- Filled: a flash of its blood over it
    helmet.flash = helmet:CreateTexture(nil, "OVERLAY")
    helmet.flash:SetPoint("CENTER")
    helmet.flash:SetSize(HELMET_WIDTH * 1.3, HELMET_HEIGHT * 1.3)
    setPiece(helmet.flash, "full")
    helmet.flash:SetBlendMode("ADD")
    helmet.flash:SetAlpha(0)
    helmet.burst = helmet.flash:CreateAnimationGroup()
    local show = helmet.burst:CreateAnimation("Alpha")
    show:SetChange(0.8)
    show:SetDuration(0.08)
    show:SetOrder(1)
    local fade = helmet.burst:CreateAnimation("Alpha")
    fade:SetChange(-0.8)
    fade:SetDuration(0.4)
    fade:SetOrder(2)

    -- Emptied (Execute cashed the bleed in): its blood fades upwards
    helmet.ghost = helmet:CreateTexture(nil, "OVERLAY")
    helmet.ghost:SetAllPoints()
    setPiece(helmet.ghost, "full")
    helmet.ghost:SetAlpha(0)
    helmet.vanish = helmet.ghost:CreateAnimationGroup()
    local appear = helmet.vanish:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local away = helmet.vanish:CreateAnimation("Alpha")
    away:SetChange(-1)
    away:SetDuration(0.45)
    away:SetOrder(2)
    local rise = helmet.vanish:CreateAnimation("Translation")
    rise:SetOffset(0, 10)
    rise:SetDuration(0.45)
    rise:SetOrder(2)
    return helmet
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
    -- Behind everything: crimson when the bleed is ripe, gold under Coup de grâce
    frame.glowCrimson, frame.crimsonPulse = glow(frame, "glowCrimson", -0.6, 0.9)
    frame.glowGold, frame.goldPulse = glow(frame, "glowGold", -0.45, 1.4)

    frame.border = frame:CreateTexture(nil, "BORDER")
    frame.border:SetPoint("CENTER", 0, -8)
    frame.border:SetSize(154, 63)
    setPiece(frame.border, "frame")

    -- Coup de grâce: energy along the blades, flickering
    frame.energy = frame:CreateTexture(nil, "ARTWORK")
    frame.energy:SetPoint("CENTER", 0, -8)
    frame.energy:SetSize(154, 63)
    setPiece(frame.energy, "bladeEnergy")
    frame.energy:SetBlendMode("ADD")
    frame.energy:SetAlpha(0)
    frame.energyPulse = pulse(frame.energy, -0.55, 0.7)

    frame.helmets = {}
    for index = 1, 3 do
        frame.helmets[index] = createHelmet(frame, index)
    end

    -- The jewel in the boss, over the helmets
    local boss = CreateFrame("Frame", nil, frame)
    boss:SetAllPoints()
    boss:SetFrameLevel(frame:GetFrameLevel() + 5)
    frame.jewel = boss:CreateTexture(nil, "ARTWORK")
    frame.jewel:SetPoint("CENTER", 0, -17)
    frame.jewel:SetSize(10, 10)
    setPiece(frame.jewel, "jewelDark")
    frame.jewelPop = pop(frame.jewel, 1.5)
    frame.crack = boss:CreateTexture(nil, "OVERLAY")
    frame.crack:SetPoint("CENTER", 0, -17)
    frame.crack:SetSize(10, 10)
    setPiece(frame.crack, "jewelCrack")
    frame.crack:Hide()

    -- Shield Slam back: a gold ring out of the jewel, swelling and fading
    frame.shockwave = boss:CreateTexture(nil, "OVERLAY")
    frame.shockwave:SetTexture(SHOCKWAVE)
    frame.shockwave:SetBlendMode("ADD")
    frame.shockwave:SetPoint("CENTER", 0, -17)
    frame.shockwave:SetSize(24, 24)
    frame.shockwave:SetAlpha(0)
    frame.ring = frame.shockwave:CreateAnimationGroup()
    local appear = frame.ring:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local swell = frame.ring:CreateAnimation("Scale")
    swell:SetScale(4, 4)
    swell:SetDuration(0.5)
    swell:SetSmoothing("OUT")
    swell:SetOrder(2)
    local fade = frame.ring:CreateAnimation("Alpha")
    fade:SetChange(-1)
    fade:SetDuration(0.5)
    fade:SetSmoothing("IN")
    fade:SetOrder(2)
end

-- The bleed in ninths: three thirds a helmet
local function ninths(bleed)
    return math.floor(math.min(100, bleed) * 9 / 100 + 0.0001)
end

local function helmetPiece(index, filled, gold)
    if gold then
        return "gold"
    end
    local inHelmet = filled - (index - 1) * THIRDS
    if inHelmet >= THIRDS then
        return "full"
    elseif inHelmet == 2 then
        return "twoThirds"
    elseif inHelmet == 1 then
        return "oneThird"
    end
    return "empty"
end

-- A glow and its pulse in or out; was: whether it was shown before (nil: the first draw)
local function showGlow(texture, animation, show, was)
    if show and not was then
        UIFrameFadeIn(texture, 0.25, texture:GetAlpha(), 1)
        animation:Play()
    elseif not show and was ~= false then
        animation:Stop()
        UIFrameFadeOut(texture, 0.3, texture:GetAlpha(), 0)
    end
end

local function update(frame, state, previous)
    if state.hidden then
        return
    end
    local before = (previous and not previous.hidden) and previous or nil

    -- The helmets: the bleed in ninths, every one gold under Coup de grâce
    local filled = ninths(state.bleed)
    local filledBefore = before and ninths(before.bleed) or filled
    for index, helmet in ipairs(frame.helmets) do
        local piece = helmetPiece(index, filled, state.coupDeGrace)
        setPiece(helmet.fill, piece)
        local full = filled >= index * THIRDS
        local wasFull = filledBefore >= index * THIRDS
        if before and full and not wasFull then
            setPiece(helmet.flash, piece)
            helmet.pop:Stop()
            helmet.pop:Play()
            helmet.burst:Stop()
            helmet.burst:Play()
        elseif before and wasFull and not full and not state.coupDeGrace then
            setPiece(helmet.ghost, before.coupDeGrace and "gold" or "full")
            helmet.vanish:Stop()
            helmet.vanish:Play()
        end
    end

    -- Ripe: three full helmets, the crimson glow beating; Coup de grâce: the gold glow and the blades' energy
    local ripe = state.bleed >= 100 and not state.coupDeGrace
    local wasRipe = before and (before.bleed >= 100 and not before.coupDeGrace)
    local wasGold = before and before.coupDeGrace
    showGlow(frame.glowCrimson, frame.crimsonPulse, ripe, before and wasRipe or nil)
    showGlow(frame.glowGold, frame.goldPulse, state.coupDeGrace, before and wasGold or nil)
    showGlow(frame.energy, frame.energyPulse, state.coupDeGrace, before and wasGold or nil)

    -- Revenge: the jewel
    setPiece(frame.jewel, state.opening and "jewelGold" or "jewelDark")
    if state.opening and before and not before.opening then
        frame.jewelPop:Stop()
        frame.jewelPop:Play()
    end
    if state.brokenGuard then
        frame.crack:Show()
    else
        frame.crack:Hide()
    end

    -- Shield Slam back: the count went up since the last message
    if before and state.resets > before.resets then
        frame.ring:Stop()
        frame.ring:Play()
    end
end

local function parse(payload)
    if payload == "-" then
        return { hidden = true }
    end
    local bleed, opening, brokenGuard, execute, coupDeGrace, duel, resets = strsplit(":", payload or "")
    if not resets then
        return nil
    end
    return {
        bleed = math.max(0, math.min(100, tonumber(bleed) or 0)),
        opening = opening == "1",
        brokenGuard = brokenGuard == "1",
        execute = execute == "1",
        coupDeGrace = coupDeGrace == "1",
        duel = duel == "1",
        resets = tonumber(resets) or 0,
    }
end

ClassHud_Register({
    token = "WARRIOR",
    prefix = "GLADIATOR",
    width = 162,
    height = 70,
    anchor = { "TOPLEFT", "PlayerFrame", "BOTTOMLEFT", 86, 8 },
    title = "Gladiateur",
    tooltip = function(state)
        local lines = {
            string.format("Plaie du gladiateur : %d%%%s", state.bleed,
                state.bleed >= 100 and " - |cffff4030l'Exécution vaut d'être portée|r" or ""),
            "Les casques se remplissent du sang de votre cible ; pleins, votre Exécution en tire le plus.",
        }
        if state.opening then
            table.insert(lines, "|cffe8c25aOuverture : Vengeance est gratuite.|r")
        end
        if state.brokenGuard then
            table.insert(lines, "|cffff4030Garde brisée : votre prochaine Vengeance est renforcée.|r")
        end
        if state.coupDeGrace then
            table.insert(lines, "|cffe8c25aCoup de grâce : votre prochaine Exécution est gratuite et à pleine rage.|r")
        end
        return lines
    end,
    create = create,
    parse = parse,
    update = update,
    -- Only the Gladiateur has one: hidden until the server says so, and for any other specialization
    isHidden = function(state)
        return not state or state.hidden
    end,
    isEmpty = function(state)
        return state.hidden or (state.bleed == 0 and not state.opening and not state.brokenGuard and
            not state.coupDeGrace)
    end,
})
