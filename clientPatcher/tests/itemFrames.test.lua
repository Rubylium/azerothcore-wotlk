-- Run from the repository root: lua clientPatcher/tests/itemFrames.test.lua
local frames, hooks, slots = {}, {}, {}
local frameMethods = {}
local function noop() end
for _, method in ipairs({ "EnableMouse", "SetAllPoints", "ClearAllPoints", "SetPoint", "SetBackdrop",
    "SetBackdropBorderColor", "SetTexture", "SetBlendMode", "SetVertexColor", "SetSize", "SetOwner" }) do
    frameMethods[method] = noop
end
function frameMethods:SetFrameLevel(level) self.level = level end
function frameMethods:GetFrameLevel() return self.level or 1 end
function frameMethods:GetName() return self.name end
function frameMethods:GetParent() return self.parent end
function frameMethods:GetID() return self.id or 1 end
function frameMethods:GetObjectType() return self.kind end
function frameMethods:IsShown() return self.shown ~= false end
function frameMethods:Show() self.shown = true end
function frameMethods:Hide() self.shown = false end
function frameMethods:SetScript(key, callback) self.scripts[key] = callback end
function frameMethods:HookScript(key, callback) self.scripts[key] = callback end
function frameMethods:RegisterEvent(event) self.events[event] = true end
function frameMethods:CreateTexture() return setmetatable({}, { __index = frameMethods }) end
function frameMethods:ClearLines() self.lines = {} end
function frameMethods:NumLines() return #(self.lines or {}) end
function frameMethods:setLines(lines)
    self.lines = lines or {}
    for index, text in ipairs(self.lines) do
        _G[self.name .. "TextLeft" .. index] = { GetText = function() return text end }
    end
end
function frameMethods:SetBagItem(bag, slot) self:setLines(slots[bag .. ":" .. slot]) end
function frameMethods:SetInventoryItem(unit, slot) self:setLines(slots[unit .. ":" .. slot]) end
function CreateFrame(kind, name, parent)
    local frame = setmetatable({ kind = kind, name = name, parent = parent, scripts = {}, events = {} },
        { __index = frameMethods })
    frames[#frames + 1] = frame
    if name then _G[name] = frame end
    return frame
end
function hooksecurefunc(name, callback) hooks[name] = callback end
function GetContainerItemLink(bag, slot) return slots[bag .. ":" .. slot] and "item:9123456" end
function GetInventoryItemLink(unit, slot) return slots[unit .. ":" .. slot] and "item:9123456" end
function BankButtonIDToInvSlotID(slot) return slot + 39 end
function EnumerateFrames(previous)
    if not previous then return frames[1] end
    for index, frame in ipairs(frames) do if frame == previous then return frames[index + 1] end end
end
UIParent = CreateFrame("Frame", "UIParent")
GameTooltip = CreateFrame("GameTooltip", "GameTooltip", UIParent)
local path = "clientPatcher/interface/Interface/FrameXML/"
dofile(path .. "ItemFrames.lua")
dofile(path .. "ItemFramesInfinite.lua")
dofile(path .. "ItemFramesAdapters.lua")
local function tick()
    for _, frame in ipairs(frames) do
        if frame.scripts.OnUpdate then frame.scripts.OnUpdate(frame, 0.2) end
    end
end
local function setSlot(bag, slot, proc)
    slots[bag .. ":" .. slot] = { "Identical item", proc or "Ordinary stats" }
end
local function assertFrame(button, shown)
    assert((button.evolutionsItemFrame and button.evolutionsItemFrame:IsShown() or false) == shown,
        (button.name or "anonymous") .. ": unexpected frame visibility")
end
local bag = CreateFrame("Frame", "ContainerFrame1", UIParent)
bag.id = 0
local first = CreateFrame("Button", "ContainerFrame1Item1", bag)
local second = CreateFrame("Button", "ContainerFrame1Item2", bag)
second.id = 2
setSlot(0, 1, "Égide des astres")
setSlot(0, 2)
hooks.SetItemButtonTexture(first)
hooks.SetItemButtonTexture(second)
tick()
assertFrame(first, true)
assertFrame(second, false) -- Same entry/link, different instance enchantments.
slots["0:1"] = nil
EvolutionsItemFrames.refresh()
tick()
assertFrame(first, false) -- Empty/recycled slots clear immediately on the next refresh.
setSlot(0, 1, "Éclat d'étoile filante")
local dragon = CreateFrame("Button", "DragonUI_BagsterItem20", bag)
dragon.IsCached = function() return false end
dragon.GetBag = function() return 0 end
hooks.SetItemButtonTexture(dragon)
tick()
assertFrame(dragon, true)
dragon.id = 2
hooks.SetItemButtonTexture(dragon)
tick()
assertFrame(dragon, false)
local worn = CreateFrame("Button", "CharacterHeadSlot", UIParent)
setSlot("player", 1, "Étincelle d'éternité")
hooks.SetItemButtonTexture(worn)
tick()
assertFrame(worn, true)
local forge = CreateFrame("Button", "ForgeTest", UIParent)
EvolutionsItemFrames.bindServerSlot(forge, 255, 0)
tick()
assertFrame(forge, true) -- Server equipment slots are zero-based; client slots are one-based.
EvolutionsItemFrames.bindServerSlot(forge, 255, 24)
tick()
assertFrame(forge, false)
EvolutionsItemFrames.registerStyle("future", { create = function(parent)
    return CreateFrame("Frame", nil, parent)
end })
EvolutionsItemFrames.registerResolver(function(tooltip)
    if tooltip.lines[2] == "Future proc" then return "future" end
end)
setSlot(0, 2, "Future proc")
EvolutionsItemFrames.refresh()
tick()
assertFrame(second, true)
assert(second.evolutionsItemFrame.styleKey == "future")
print("Item frame tests passed: instance isolation, recycling, DragonUI, equipment, Forge, extensibility")
