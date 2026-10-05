-- The Faucheur's HUD (mod-reaper): its three Âmes moissonnées on Ascension's Reaper art. A soul's skull fills green;
-- the next one shows the fragments gathered towards it (one or two thirds); at 3 souls the Infusion d'âme turns the
-- skulls purple, a glow wakes behind them and the infusion flipbook plays on top. Animated: a skull pops in when its
-- soul is harvested and fades when it is consumed. Fed by the server's "REAPER\t<souls>:<fragments>:<infused>"
-- (modules/mod-reaper), or by the resource auras until the first message. Framework: ClassHud.lua.

local ATLAS = "Interface\\ClassHud\\ReaperAtlas"
local FLIPBOOK = "Interface\\ClassHud\\ReaperInfusedFlipbook"
local SOUL_SPELL, FRAGMENT_SPELL, INFUSION_SPELL = 98017, 98016, 98018

-- Ascension's atlas, 512x512: its pieces' pixel boxes { left, right, top, bottom }
local PIECES = {
    frame = { 0, 512, 0, 225 },
    empty = { 0, 151, 298, 405 },
    infused = { 151, 302, 298, 405 },
    glow = { 302, 512, 299, 406 },
    full = { 0, 151, 405, 512 },
    oneShard = { 151, 302, 405, 512 },
    twoShards = { 302, 453, 405, 512 },
}

-- The flipbook: 2048x2048, 8x8 frames of 256 pixels, played at 60 frames a second
local FLIPBOOK_COLUMNS, FLIPBOOK_FRAMES, FLIPBOOK_FPS = 8, 64, 60

local SOUL_WIDTH, SOUL_HEIGHT = 50, 35
local SOUL_SPACING = 38

local function setPiece(texture, piece)
    local box = PIECES[piece]
    texture:SetTexture(ATLAS)
    texture:SetTexCoord(box[1] / 512, box[2] / 512, box[3] / 512, box[4] / 512)
end

local function createSoul(parent, index)
    local soul = CreateFrame("Frame", nil, parent)
    soul:SetSize(SOUL_WIDTH, SOUL_HEIGHT)
    soul:SetPoint("CENTER", parent, "CENTER", (index - 2) * SOUL_SPACING - 2, 2)
    soul:SetFrameLevel(parent:GetFrameLevel() + 4 - index)

    soul.background = soul:CreateTexture(nil, "ARTWORK")
    soul.background:SetAllPoints()
    setPiece(soul.background, "empty")

    soul.fill = soul:CreateTexture(nil, "OVERLAY")
    soul.fill:SetAllPoints()
    setPiece(soul.fill, "full")
    soul.fill:Hide()

    -- Harvested: the skull swells and settles, a flash over it
    soul.flash = soul:CreateTexture(nil, "OVERLAY")
    soul.flash:SetPoint("CENTER")
    soul.flash:SetSize(SOUL_WIDTH * 1.3, SOUL_HEIGHT * 1.3)
    setPiece(soul.flash, "full")
    soul.flash:SetBlendMode("ADD")
    soul.flash:SetAlpha(0)

    soul.pop = soul.fill:CreateAnimationGroup()
    local grow = soul.pop:CreateAnimation("Scale")
    grow:SetScale(1.25, 1.25)
    grow:SetDuration(0.12)
    grow:SetOrder(1)
    local settle = soul.pop:CreateAnimation("Scale")
    settle:SetScale(0.8, 0.8)
    settle:SetDuration(0.18)
    settle:SetOrder(2)

    soul.burst = soul.flash:CreateAnimationGroup()
    local show = soul.burst:CreateAnimation("Alpha")
    show:SetChange(0.9)
    show:SetDuration(0.08)
    show:SetOrder(1)
    local fade = soul.burst:CreateAnimation("Alpha")
    fade:SetChange(-0.9)
    fade:SetDuration(0.4)
    fade:SetOrder(2)

    -- Consumed: the skull fades away rather than vanishing
    soul.ghost = soul:CreateTexture(nil, "OVERLAY")
    soul.ghost:SetAllPoints()
    setPiece(soul.ghost, "full")
    soul.ghost:SetAlpha(0)
    soul.vanish = soul.ghost:CreateAnimationGroup()
    local appear = soul.vanish:CreateAnimation("Alpha")
    appear:SetChange(1)
    appear:SetDuration(0)
    appear:SetOrder(1)
    local away = soul.vanish:CreateAnimation("Alpha")
    away:SetChange(-1)
    away:SetDuration(0.45)
    away:SetOrder(2)
    local rise = soul.vanish:CreateAnimation("Translation")
    rise:SetOffset(0, 10)
    rise:SetDuration(0.45)
    rise:SetOrder(2)
    return soul
end

local function create(frame)
    -- The infusion's glow behind everything
    frame.glow = frame:CreateTexture(nil, "BACKGROUND")
    frame.glow:SetPoint("CENTER", 0, 2)
    frame.glow:SetSize(176, 90)
    setPiece(frame.glow, "glow")
    frame.glow:SetBlendMode("ADD")
    frame.glow:SetAlpha(0)

    frame.border = frame:CreateTexture(nil, "BORDER")
    frame.border:SetPoint("CENTER", 0, -8)
    frame.border:SetSize(153.6, 63)
    setPiece(frame.border, "frame")

    frame.souls = {}
    for index = 1, 3 do
        frame.souls[index] = createSoul(frame, index)
    end

    -- The infusion's flipbook, over the souls
    local flipFrame = CreateFrame("Frame", nil, frame)
    flipFrame:SetAllPoints()
    flipFrame:SetFrameLevel(frame:GetFrameLevel() + 6)
    frame.flipbook = flipFrame:CreateTexture(nil, "OVERLAY")
    frame.flipbook:SetSize(174 * 0.9, 174 * 0.8)
    frame.flipbook:SetPoint("CENTER", 0, -3)
    frame.flipbook:SetTexture(FLIPBOOK)
    frame.flipbook:SetBlendMode("ADD")
    frame.flipbook:Hide()
    frame.flipElapsed = 0

    frame.glowPulse = frame.glow:CreateAnimationGroup()
    frame.glowPulse:SetLooping("BOUNCE")
    local pulse = frame.glowPulse:CreateAnimation("Alpha")
    pulse:SetChange(-0.45)
    pulse:SetDuration(0.9)
    pulse:SetSmoothing("IN_OUT")
end

local function setFlipFrame(texture, index)
    local column = index % FLIPBOOK_COLUMNS
    local row = math.floor(index / FLIPBOOK_COLUMNS)
    local size = 1 / FLIPBOOK_COLUMNS
    texture:SetTexCoord(column * size, (column + 1) * size, row * size, (row + 1) * size)
end

local function playFlipbook(frame, play)
    if play then
        frame.flipbook:Show()
        frame:SetScript("OnUpdate", function(self, elapsed)
            self.flipElapsed = self.flipElapsed + elapsed
            setFlipFrame(self.flipbook, math.floor(self.flipElapsed * FLIPBOOK_FPS) % FLIPBOOK_FRAMES)
        end)
    else
        frame.flipbook:Hide()
        frame:SetScript("OnUpdate", nil)
    end
end

local function update(frame, state, previous)
    local before = previous and previous.souls or state.souls
    for index, soul in ipairs(frame.souls) do
        local filled = index <= state.souls
        setPiece(soul.fill, state.infused and "infused" or "full")
        setPiece(soul.flash, state.infused and "infused" or "full")
        if filled then
            soul.fill:Show()
        else
            soul.fill:Hide()
        end
        -- The next skull to fill shows the fragments gathered towards it
        if not filled and index == state.souls + 1 and state.fragments > 0 then
            setPiece(soul.background, state.fragments >= 2 and "twoShards" or "oneShard")
        else
            setPiece(soul.background, "empty")
        end
        if previous and filled and index > before then
            soul.pop:Stop()
            soul.pop:Play()
            soul.burst:Stop()
            soul.burst:Play()
        elseif previous and not filled and index <= before then
            setPiece(soul.ghost, previous.infused and "infused" or "full")
            soul.vanish:Stop()
            soul.vanish:Play()
        end
    end

    local wasInfused = previous and previous.infused
    if state.infused and not wasInfused then
        UIFrameFadeIn(frame.glow, 0.25, frame.glow:GetAlpha(), 1)
        frame.glowPulse:Play()
        playFlipbook(frame, true)
    elseif not state.infused and (wasInfused or previous == nil) then
        frame.glowPulse:Stop()
        UIFrameFadeOut(frame.glow, 0.3, frame.glow:GetAlpha(), 0)
        playFlipbook(frame, false)
    end
end

local function clamp(value)
    value = tonumber(value) or 0
    return math.max(0, math.min(3, value))
end

local function parse(payload)
    local souls, fragments, infused = strsplit(":", payload or "")
    if not souls then
        return nil
    end
    return { souls = clamp(souls), fragments = clamp(fragments), infused = tonumber(infused) == 1 }
end

local function auraStacks(spellId)
    local name = GetSpellInfo(spellId)
    if not name then
        return 0, false
    end
    local present, _, _, count = UnitAura("player", name)
    if not present then
        return 0, false
    end
    return (count and count > 0) and count or 1, true
end

local function fromAuras()
    local souls = auraStacks(SOUL_SPELL)
    local fragments = auraStacks(FRAGMENT_SPELL)
    local _, infused = auraStacks(INFUSION_SPELL)
    return { souls = clamp(souls), fragments = clamp(fragments), infused = infused }
end

ClassHud_Register({
    token = "REAPER",
    prefix = "REAPER",
    width = 162,
    height = 70,
    anchor = { "TOPLEFT", "PlayerFrame", "BOTTOMLEFT", 86, 8 },
    title = "Âmes moissonnées",
    tooltip = function(state)
        local lines = {
            string.format("Âmes : %d / 3   Fragments : %d / 3", state.souls, state.fragments),
            "3 Fragments d'âme forment une Âme moissonnée ; 3 Âmes moissonnées donnent l'Infusion d'âme.",
        }
        if state.infused then
            table.insert(lines,
                "|cffb070ffInfusion d'âme : vos techniques qui consomment vos âmes sont 20% plus efficaces.|r")
        end
        return lines
    end,
    create = create,
    parse = parse,
    fromAuras = fromAuras,
    update = update,
    isEmpty = function(state)
        return state.souls == 0 and state.fragments == 0
    end,
})
