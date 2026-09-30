-- A character's essences on its tooltip, players and bots alike: asked from the server when the tooltip opens
-- (mod-stat-growth EssenceTierSystem.cpp HandleEssenceAddonMessage, "Essences" whispered to oneself) and kept a little
-- while. Growth and Vitality show their saved points and what they are worth after the diminishing returns
-- (.agents/docs/systems/power-scaling.md); a bot's are those it mirrors from its players.

local PREFIX = "Essences"
local KEEP_SECONDS = 30
local ASK_AGAIN_SECONDS = 1

local french = GetLocale() == "frFR"
local TEXT = french and {
    title = "Essences",
    growth = "Croissance %s (%s effectifs)",
    vitality = "Vitalité %s (%s effectifs, +%s PV)",
    rest = "Expérience +%d %%  ·  Fortune %d %%  ·  Ressource %d %%",
    mirrored = "reflète ses joueurs",
} or {
    title = "Essences",
    growth = "Growth %s (%s effective)",
    vitality = "Vitality %s (%s effective, +%s health)",
    rest = "Experience +%d%%  ·  Fortune %d%%  ·  Resource %d%%",
    mirrored = "mirrors its players",
}

-- The board's palette: gold headings, parchment text
local GOLD = { 1, 0.82, 0.42 }
local PARCHMENT = { 0.9, 0.85, 0.72 }

local cache = {}
local asked = {}

-- 140400 -> "140 400"
local function Thousands(value)
    local grouped = tostring(floor(value or 0)):reverse():gsub("(%d%d%d)", "%1 "):reverse()
    return (grouped:gsub("^ ", ""))
end

local function GuidLow(unit)
    local guid = unit and UnitGUID(unit)
    return guid and tonumber(guid:sub(-8), 16)
end

local function AddLines(tooltip, entry)
    if entry.growth == 0 and entry.vitality == 0 and entry.experience == 0 and entry.fortune == 0
        and entry.resource == 0 then
        return
    end
    tooltip:AddLine(entry.bot and (TEXT.title .. "  |cff9a8f7a(" .. TEXT.mirrored .. ")|r") or TEXT.title,
        GOLD[1], GOLD[2], GOLD[3])
    if entry.growth > 0 then
        tooltip:AddLine(format(TEXT.growth, Thousands(entry.growth), Thousands(entry.growthEffective)),
            PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    end
    if entry.vitality > 0 then
        tooltip:AddLine(format(TEXT.vitality, Thousands(entry.vitality), Thousands(entry.vitalityEffective),
            Thousands(entry.health)), PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    end
    if entry.experience > 0 or entry.fortune > 0 or entry.resource > 0 then
        tooltip:AddLine(format(TEXT.rest, entry.experience, entry.fortune, entry.resource),
            PARCHMENT[1], PARCHMENT[2], PARCHMENT[3])
    end
    tooltip:Show()
end

local function TooltipLow()
    local _, unit = GameTooltip:GetUnit()
    return unit and UnitIsPlayer(unit) and GuidLow(unit)
end

GameTooltip:HookScript("OnTooltipSetUnit", function(tooltip)
    local low = TooltipLow()
    if not low then
        return
    end
    local entry = cache[low]
    if entry and GetTime() - entry.time < KEEP_SECONDS then
        AddLines(tooltip, entry)
        return
    end
    if not asked[low] or GetTime() - asked[low] > ASK_AGAIN_SECONDS then
        asked[low] = GetTime()
        SendAddonMessage(PREFIX, "Q\t" .. low, "WHISPER", UnitName("player"))
    end
end)

local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end
    local kind, low, growth, growthEffective, vitality, vitalityEffective, health, experience, fortune, resource, bot =
        strsplit("\t", message)
    if kind ~= "S" then
        return
    end
    low = tonumber(low)
    if not low then
        return
    end
    local entry = {
        time = GetTime(),
        growth = tonumber(growth) or 0,
        growthEffective = tonumber(growthEffective) or 0,
        vitality = tonumber(vitality) or 0,
        vitalityEffective = tonumber(vitalityEffective) or 0,
        health = tonumber(health) or 0,
        experience = tonumber(experience) or 0,
        fortune = tonumber(fortune) or 0,
        resource = tonumber(resource) or 0,
        bot = bot == "1",
    }
    cache[low] = entry
    asked[low] = nil
    -- The answer comes a moment after the tooltip opened: it gets the lines if it still shows that character
    if GameTooltip:IsShown() and TooltipLow() == low then
        AddLines(GameTooltip, entry)
    end
end)
