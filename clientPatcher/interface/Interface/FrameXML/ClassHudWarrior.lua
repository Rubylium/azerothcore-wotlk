-- The Gladiateur's HUD (mod-warrior, the Warrior's fourth specialization): a bronze arena shield, a gladius behind it.
-- Its rim fills with the blood of Plaie du gladiateur on the target - full when Execute is worth cashing it in, the rim
-- then glowing; its boss is Revenge - gold under Ouverture, cracked red with Garde brisée; the thumbs-down medallion is
-- Execute - lit while the target bleeds, burning under Coup de grâce, a laurel of light and embers around the shield.
-- Shield Slam brought back rings out of the boss; the crossed gladii on top light during Duel. Fed by the server's
-- "GLADIATOR\t<bleed %>:<opening>:<broken guard>:<execute>:<coup de grace>:<duel>:<resets>" (modules/mod-warrior), or
-- "GLADIATOR\t-" for any other Warrior: the HUD hides. Art: clientPatcher/assets/gladiatorHud (its assetManifest.json
-- gives the placements below, on the shield's 512 canvas). Framework: ClassHud.lua.

local ROOT = "Interface\\ClassHud\\"
local CANVAS = 512
local SIZE = 120
local SCALE = SIZE / CANVAS

-- Placements on the canvas: centres and sizes
local BOSS = { 256, 253, 128 }
local MEDALLION = { 373, 364, 56 }
local DUEL = { 256, 36, 96, 48 }

-- The rim's blood: 16 frames (4 x 4 cells of 512 on 2048), empty to full, clockwise from six o'clock
local BLOOD_COLUMNS, BLOOD_FRAMES = 4, 16
-- The embers: 64 frames (8 x 8 cells of 256), a loop at 20 frames a second
local EMBER_COLUMNS, EMBER_FRAMES, EMBER_FPS = 8, 64, 20

local function place(texture, parent, x, y, width, height)
    texture:SetSize(width * SCALE, (height or width) * SCALE)
    texture:SetPoint("CENTER", parent, "TOPLEFT", x * SCALE, -y * SCALE)
end

local function layer(parent, level, file, blend)
    local texture = parent:CreateTexture(nil, level)
    texture:SetTexture(ROOT .. file)
    if blend then
        texture:SetBlendMode(blend)
    end
    return texture
end

local function setCell(texture, index, columns)
    local column = index % columns
    local row = math.floor(index / columns)
    local size = 1 / columns
    texture:SetTexCoord(column * size, (column + 1) * size, row * size, (row + 1) * size)
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

-- A brief swell and flash: a state just gained
local function pop(texture)
    local group = texture:CreateAnimationGroup()
    local grow = group:CreateAnimation("Scale")
    grow:SetScale(1.2, 1.2)
    grow:SetDuration(0.1)
    grow:SetOrder(1)
    local settle = group:CreateAnimation("Scale")
    settle:SetScale(1 / 1.2, 1 / 1.2)
    settle:SetDuration(0.16)
    settle:SetOrder(2)
    return group
end

local function create(frame)
    -- The laurel of light (Coup de grâce) behind the shield
    frame.laurel = layer(frame, "BACKGROUND", "gladiatorLaurelFlare", "ADD")
    frame.laurel:SetAllPoints()
    frame.laurel:SetAlpha(0)
    frame.laurelPulse = pulse(frame.laurel, -0.35, 1.2)

    frame.shield = layer(frame, "BORDER", "gladiatorFrame")
    frame.shield:SetAllPoints()

    frame.blood = layer(frame, "ARTWORK", "gladiatorBloodFill")
    frame.blood:SetAllPoints()
    setCell(frame.blood, 0, BLOOD_COLUMNS)

    -- The rim's glow when the bleed is ripe: a slow heartbeat
    frame.rimGlow = layer(frame, "ARTWORK", "gladiatorRimGlow", "ADD")
    frame.rimGlow:SetAllPoints()
    frame.rimGlow:SetAlpha(0)
    frame.rimPulse = pulse(frame.rimGlow, -0.6, 0.9)

    -- Over the shield's face: the boss, the medallion, the crossed gladii
    local face = CreateFrame("Frame", nil, frame)
    face:SetAllPoints()
    face:SetFrameLevel(frame:GetFrameLevel() + 2)
    frame.face = face

    frame.boss = layer(face, "ARTWORK", "gladiatorBossDark")
    place(frame.boss, face, BOSS[1], BOSS[2], BOSS[3])
    frame.bossPop = pop(frame.boss)
    frame.crack = layer(face, "OVERLAY", "gladiatorBossCrack")
    place(frame.crack, face, BOSS[1], BOSS[2], BOSS[3])
    frame.crack:Hide()

    frame.medallion = layer(face, "ARTWORK", "gladiatorMedallionDark")
    place(frame.medallion, face, MEDALLION[1], MEDALLION[2], MEDALLION[3])
    frame.medallionPop = pop(frame.medallion)

    frame.duel = layer(face, "ARTWORK", "gladiatorDuelDark")
    place(frame.duel, face, DUEL[1], DUEL[2], DUEL[3], DUEL[4])

    -- Shield Slam back: a ring out of the boss, swelling and fading
    frame.shockwave = layer(face, "OVERLAY", "gladiatorShockwave", "ADD")
    place(frame.shockwave, face, BOSS[1], BOSS[2], 120)
    frame.shockwave:SetAlpha(0)
    frame.ring = frame.shockwave:CreateAnimationGroup()
    local appear = frame.ring:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local swell = frame.ring:CreateAnimation("Scale")
    swell:SetScale(3.4, 3.4)
    swell:SetDuration(0.5)
    swell:SetSmoothing("OUT")
    swell:SetOrder(2)
    local fade = frame.ring:CreateAnimation("Alpha")
    fade:SetChange(-1)
    fade:SetDuration(0.5)
    fade:SetSmoothing("IN")
    fade:SetOrder(2)

    -- The embers of Coup de grâce, over everything
    local embers = CreateFrame("Frame", nil, frame)
    embers:SetAllPoints()
    embers:SetFrameLevel(frame:GetFrameLevel() + 4)
    frame.embers = layer(embers, "OVERLAY", "gladiatorEmberFlipbook", "ADD")
    frame.embers:SetAllPoints()
    frame.embers:Hide()
    frame.emberElapsed = 0
end

local function playEmbers(frame, play)
    if play then
        frame.embers:Show()
        frame:SetScript("OnUpdate", function(self, elapsed)
            self.emberElapsed = self.emberElapsed + elapsed
            setCell(self.embers, math.floor(self.emberElapsed * EMBER_FPS) % EMBER_FRAMES, EMBER_COLUMNS)
        end)
    else
        frame.embers:Hide()
        frame:SetScript("OnUpdate", nil)
    end
end

local function update(frame, state, previous)
    if state.hidden then
        playEmbers(frame, false)
        return
    end
    local before = (previous and not previous.hidden) and previous or nil

    -- The bleed: its rim, ripe at 100
    setCell(frame.blood, math.floor(state.bleed * (BLOOD_FRAMES - 1) / 100), BLOOD_COLUMNS)
    local ripe = state.bleed >= 100
    if ripe and not (before and before.bleed >= 100) then
        UIFrameFadeIn(frame.rimGlow, 0.2, frame.rimGlow:GetAlpha(), 1)
        frame.rimPulse:Play()
    elseif not ripe and (not before or before.bleed >= 100) then
        frame.rimPulse:Stop()
        UIFrameFadeOut(frame.rimGlow, 0.3, frame.rimGlow:GetAlpha(), 0)
    end

    -- Revenge: Ouverture, Garde brisée
    frame.boss:SetTexture(ROOT .. (state.opening and "gladiatorBossGold" or "gladiatorBossDark"))
    if state.opening and before and not before.opening then
        frame.bossPop:Stop()
        frame.bossPop:Play()
    end
    if state.brokenGuard then
        frame.crack:Show()
    else
        frame.crack:Hide()
    end

    -- Execute: usable while it bleeds, Coup de grâce
    local medallion = state.coupDeGrace and "gladiatorMedallionCoupDeGrace" or
        (state.execute and "gladiatorMedallionLit" or "gladiatorMedallionDark")
    frame.medallion:SetTexture(ROOT .. medallion)
    if before and ((state.execute and not before.execute) or (state.coupDeGrace and not before.coupDeGrace)) then
        frame.medallionPop:Stop()
        frame.medallionPop:Play()
    end
    if state.coupDeGrace and not (before and before.coupDeGrace) then
        UIFrameFadeIn(frame.laurel, 0.2, frame.laurel:GetAlpha(), 1)
        frame.laurelPulse:Play()
        playEmbers(frame, true)
    elseif not state.coupDeGrace and (not before or before.coupDeGrace) then
        frame.laurelPulse:Stop()
        UIFrameFadeOut(frame.laurel, 0.35, frame.laurel:GetAlpha(), 0)
        playEmbers(frame, false)
    end

    frame.duel:SetTexture(ROOT .. (state.duel and "gladiatorDuelLit" or "gladiatorDuelDark"))

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
    width = SIZE,
    height = SIZE,
    anchor = { "TOPLEFT", "PlayerFrame", "BOTTOMLEFT", 106, 20 },
    title = "Gladiateur",
    tooltip = function(state)
        local lines = {
            string.format("Plaie du gladiateur : %d%%%s", state.bleed,
                state.bleed >= 100 and " - |cffff4030l'Exécution vaut d'être portée|r" or ""),
            "Le bord du bouclier se remplit du sang de votre cible ; plein, votre Exécution en tire le plus.",
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
        if state.duel then
            table.insert(lines, "Duel en cours.")
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
            not state.coupDeGrace and not state.duel)
    end,
})
