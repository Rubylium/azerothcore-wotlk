-- The Hollow Voice's touched gear (one piece in four: Ailes du Séraphin, Égide d'Aldric, Murmure de Vel'thazar): a
-- pale gold bezel, the void's violet light behind it and four holy sparks at its corners - Aldric's light around the
-- demon's shadow. L'Infini's is cyan and starry (ItemFramesInfinite.lua).
local api = EvolutionsItemFrames
local layout = EvolutionsTooltip

api.registerResolver(function(tooltip)
    if not tooltip:GetName() then return end
    local _, touch = layout.FindTouch(tooltip)
    if touch == "voice" then return "hollowVoice" end
end)

api.registerStyle("hollowVoice", {
    create = function(parent)
        local art = CreateFrame("Frame", nil, parent)
        art:SetAllPoints(parent)
        art:EnableMouse(false)
        art:SetBackdrop({ edgeFile = "Interface\\Buttons\\WHITE8X8", edgeSize = 1 })
        art:SetBackdropBorderColor(1, 0.86, 0.55, 0.95)
        local glow = art:CreateTexture(nil, "BACKGROUND")
        glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        glow:SetPoint("TOPLEFT", -10, 10)
        glow:SetPoint("BOTTOMRIGHT", 10, -10)
        glow:SetBlendMode("ADD")
        glow:SetVertexColor(0.55, 0.12, 0.85, 0.75)
        for _, corner in ipairs({ "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }) do
            local spark = art:CreateTexture(nil, "OVERLAY")
            spark:SetTexture("Interface\\Cooldown\\star4")
            spark:SetBlendMode("ADD")
            spark:SetSize(14, 14)
            spark:SetPoint("CENTER", art, corner)
            spark:SetVertexColor(1, 0.85, 0.5, 0.95)
        end
        return art
    end,
})
