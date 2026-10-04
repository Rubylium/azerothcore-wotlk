-- What an action button shows of its spell's state, as retail does: it greys while the spell may not be cast, and the
-- spell alert (retail's own flipbooks, Interface\SpellAlert, localTools/interface/buildProcGlowArt.py) plays around it
-- while a proc makes it ready.
--
-- The client greys a button from its spell data alone. Some rules live on the server instead (mod-warrior: Execute's
-- health check moved off the client so the Gladiateur can execute a bleeding target, its Revenge behind Ouverture):
-- those buttons stayed bright while the server refused them. IsUsableAction is wrapped here so every button - the
-- stock ones and DragonUI's tint, which ask it too - knows them. The rules read the player's and the target's auras
-- and the talent window's state (TalentTree_GetSpecialization, TalentTree_HasSpell).

local GLADIATEUR = 5                    -- the Warrior's spec tree
local MASSACRE = { 95008, 95023 }       -- Arms' and Fury's: Execute below 35% instead of 20%
local EXECUTE_PCT, MASSACRE_PCT = 20, 35

-- Every rank, by spell id
local EXECUTE = { 5308, 20658, 20660, 20661, 20662, 25234, 25236, 47470, 47471 }
local REVENGE = { 6572, 6574, 7379, 11600, 11601, 25288, 25269, 30357, 57823 }
local SHIELD_SLAM = { 23922, 23923, 23924, 23925, 25258, 30356, 47487, 47488 }
local SLAM = { 1464, 8820, 11604, 11605, 25241, 25242, 47474, 47475 }

-- The procs, by aura id
local OPENING = 95160           -- Ouverture: Shield Slam opened Revenge (Gladiateur)
local COUP_DE_GRACE = 95153     -- the next Execute free and as with 100 rage
local WOUND = 95158             -- Plaie du gladiateur, on the target
local REVANCHE = 95146          -- Revanche !: a free Revenge after a dodge, a parry or a block
local SUDDEN_DEATH = 52437      -- Execute whatever the target's health
local SWORD_AND_BOARD = 50227   -- Shield Slam ready and free
local BLOODSURGE = 46916        -- Slam! : an instant Slam

local playerAuras, targetAuras = {}, {}
local shieldSlamBack = 0        -- until when Shield Slam, back early from its cooldown (the bleed), glows

local function Has(spellId)
    return playerAuras[spellId]
end

local function IsGladiateur()
    local _, class = UnitClass("player")
    return class == "WARRIOR" and TalentTree_GetSpecialization and TalentTree_GetSpecialization() == GLADIATEUR
end

local function HasTalent(list)
    if not TalentTree_HasSpell then
        return false
    end
    for _, spellId in ipairs(list) do
        if TalentTree_HasSpell(spellId) then
            return true
        end
    end
    return false
end

-- Execute: the server's check (mod-warrior OnSpellCheckCast) - the target below 20% (35% with Massacre), or Sudden
-- Death, or for the Gladiateur a target bleeding of its Plaie. With no enemy targeted the client's own rules stand.
local function ExecuteUsable()
    if Has(SUDDEN_DEATH) then
        return true
    end
    if not UnitExists("target") or not UnitCanAttack("player", "target") or UnitIsDeadOrGhost("target") then
        return true
    end
    local maximum = UnitHealthMax("target")
    if not maximum or maximum <= 0 then
        return true
    end
    local threshold = HasTalent(MASSACRE) and MASSACRE_PCT or EXECUTE_PCT
    if UnitHealth("target") / maximum * 100 < threshold then
        return true
    end
    return IsGladiateur() and targetAuras[WOUND] or false
end

local RULES = {
    { spells = EXECUTE, usable = ExecuteUsable,
      glow = function() return Has(COUP_DE_GRACE) or Has(SUDDEN_DEATH) end },
    { spells = REVENGE, usable = function() return not IsGladiateur() or Has(OPENING) end,
      glow = function() return Has(OPENING) or Has(REVANCHE) end },
    { spells = SHIELD_SLAM, glow = function() return Has(SWORD_AND_BOARD) or GetTime() < shieldSlamBack end },
    { spells = SLAM, glow = function() return Has(BLOODSURGE) end },
}

-- A spell's rule by its name in the client's language: a macro's spell is only known by its name
local ruleByName = {}
local shieldSlamName
local function IndexRules()
    for _, rule in ipairs(RULES) do
        for _, spellId in ipairs(rule.spells) do
            local name = GetSpellInfo(spellId)
            if name then
                ruleByName[name] = rule
            end
        end
    end
    shieldSlamName = GetSpellInfo(SHIELD_SLAM[1])
end
IndexRules()

local slotRules = {}
local function RuleForSlot(slot)
    local cached = slotRules[slot]
    if cached == nil then
        local name
        local kind, id, _, spellId = GetActionInfo(slot)
        if kind == "spell" then
            name = spellId and GetSpellInfo(spellId) or GetSpellName(id, BOOKTYPE_SPELL)
        elseif kind == "macro" then
            name = GetMacroSpell(id)
        end
        cached = name and ruleByName[name] or false
        slotRules[slot] = cached
    end
    return cached or nil
end

local stockIsUsableAction = IsUsableAction
function IsUsableAction(slot)
    local usable, lacking = stockIsUsableAction(slot)
    if usable then
        local rule = RuleForSlot(slot)
        if rule and rule.usable and not rule.usable() then
            return nil, nil
        end
    end
    return usable, lacking
end

-- The spell alert: retail's two flipbooks, 30 frames each, laid out 8 by 4 - the ring closing onto the button once,
-- then the border's running shine for as long as the proc lasts. Sized on the button as retail's (the start 3.3
-- times it, the loop 1.4). The burst is added to what is under it; the loop is laid over: added, it bleached the
-- icon.
local START_TIME, LOOP_TIME, FRAMES = 0.7, 1.0, 30

local function SetFrame(texture, index)
    local column, row = index % 8, math.floor(index / 8)
    texture:SetTexCoord(column / 8, (column + 1) / 8, row / 4, (row + 1) / 4)
end

local function AnimateAlert(alert, elapsed)
    alert.time = alert.time + elapsed
    if alert.time < START_TIME then
        SetFrame(alert.start, math.min(FRAMES - 1, math.floor(alert.time / START_TIME * FRAMES)))
        alert.loop:SetAlpha(math.max(0, alert.time / START_TIME * 2 - 1))
    else
        alert.start:Hide()
        alert.loop:SetAlpha(1)
    end
    SetFrame(alert.loop, math.floor((alert.time % LOOP_TIME) / LOOP_TIME * FRAMES))
end

local function Alert(button)
    local alert = button.spellAlert
    if not alert then
        alert = CreateFrame("Frame", nil, button)
        alert:SetPoint("CENTER")
        alert:SetFrameLevel(button:GetFrameLevel() + 4)
        alert.start = alert:CreateTexture(nil, "OVERLAY")
        alert.start:SetTexture("Interface\\SpellAlert\\ProcStart")
        alert.start:SetBlendMode("ADD")
        alert.start:SetPoint("CENTER")
        alert.loop = alert:CreateTexture(nil, "OVERLAY")
        alert.loop:SetTexture("Interface\\SpellAlert\\ProcLoop")
        alert.loop:SetAllPoints()
        alert:SetScript("OnUpdate", AnimateAlert)
        alert:Hide()
        button.spellAlert = alert
    end
    local size = button:GetWidth()
    alert:SetWidth(size * 1.4)
    alert:SetHeight(size * 1.4)
    alert.start:SetWidth(size * 3.3)
    alert.start:SetHeight(size * 3.3)
    return alert
end

local function ShowAlert(button, shown)
    local alert = button.spellAlert
    if shown then
        alert = Alert(button)
        if not alert:IsShown() then
            alert.time = 0
            alert.start:Show()
            alert.loop:SetAlpha(0)
            AnimateAlert(alert, 0)
            alert:Show()
        end
    elseif alert then
        alert:Hide()
    end
end

-- Every action button the stock code updates (DragonUI's are the stock ones, moved and skinned)
local buttons = {}

local function ButtonSlot(button)
    return button.action or (ActionButton_GetPagedID and ActionButton_GetPagedID(button))
end

local function RefreshButton(button)
    local slot = ButtonSlot(button)
    local rule = slot and HasAction(slot) and RuleForSlot(slot)
    ShowAlert(button, rule and rule.glow and rule.glow() or false)
    if button:IsVisible() and slot and HasAction(slot) then
        ActionButton_UpdateUsable(button)
    end
end

hooksecurefunc("ActionButton_Update", function(button)
    buttons[button] = true
    RefreshButton(button)
end)

local function ReadAuras(unit, into, mineOnly)
    wipe(into)
    for _, filter in ipairs({ "HELPFUL", "HARMFUL" }) do
        local index = 1
        while true do
            local name, _, _, _, _, _, _, caster, _, _, spellId = UnitAura(unit, index, filter)
            if not name then
                break
            end
            if spellId and (not mineOnly or caster == "player") then
                into[spellId] = true
            end
            index = index + 1
        end
    end
end

local dirty = true
local function RefreshAll()
    for button in pairs(buttons) do
        RefreshButton(button)
    end
end

local watcher = CreateFrame("Frame")
-- When Shield Slam's cooldown should end: back well before (the Gladiateur's bleed brings it back), it glows until
-- cast, at most 6 s
local shieldSlamEnd = 0
watcher:RegisterEvent("PLAYER_ENTERING_WORLD")
watcher:RegisterEvent("PLAYER_TARGET_CHANGED")
watcher:RegisterEvent("UNIT_AURA")
watcher:RegisterEvent("UNIT_HEALTH")
watcher:RegisterEvent("ACTIONBAR_SLOT_CHANGED")
watcher:RegisterEvent("UPDATE_MACROS")
watcher:RegisterEvent("SPELLS_CHANGED")
watcher:RegisterEvent("SPELL_UPDATE_COOLDOWN")
watcher:RegisterEvent("UNIT_SPELLCAST_SUCCEEDED")
watcher:SetScript("OnEvent", function(self, event, unit, spellName)
    if event == "UNIT_AURA" then
        if unit == "player" then
            ReadAuras("player", playerAuras, false)
        elseif unit == "target" then
            ReadAuras("target", targetAuras, true)
        else
            return
        end
    elseif event == "UNIT_HEALTH" then
        if unit ~= "target" then
            return
        end
    elseif event == "PLAYER_TARGET_CHANGED" then
        ReadAuras("target", targetAuras, true)
    elseif event == "PLAYER_ENTERING_WORLD" then
        IndexRules()
        ReadAuras("player", playerAuras, false)
        ReadAuras("target", targetAuras, true)
    elseif event == "ACTIONBAR_SLOT_CHANGED" then
        if unit then
            slotRules[unit] = nil
        else
            wipe(slotRules)
        end
    elseif event == "UPDATE_MACROS" or event == "SPELLS_CHANGED" then
        wipe(slotRules)
    elseif event == "SPELL_UPDATE_COOLDOWN" then
        -- Shield Slam's cooldown ending well before it should: the bleed brought it back
        local start, duration = GetSpellCooldown(shieldSlamName or "")
        if start and start > 0 and duration > 1.5 then
            shieldSlamEnd = start + duration
        elseif shieldSlamEnd - GetTime() > 0.5 then
            shieldSlamEnd = 0
            shieldSlamBack = GetTime() + 6
        else
            return
        end
    elseif event == "UNIT_SPELLCAST_SUCCEEDED" then
        if unit ~= "player" or spellName ~= shieldSlamName or shieldSlamBack == 0 then
            return
        end
        shieldSlamBack = 0
    end
    dirty = true
end)

-- One refresh per frame at most, and once a second (a glow running out, Shield Slam's ends)
local sinceRefresh = 0
watcher:SetScript("OnUpdate", function(self, elapsed)
    sinceRefresh = sinceRefresh + elapsed
    if dirty or sinceRefresh > 1 then
        dirty = false
        sinceRefresh = 0
        RefreshAll()
    end
end)

-- The talent window's state (a specialization taken, Massacre learned) changes the rules
TalentTree_OnStateChanged = function()
    dirty = true
end
