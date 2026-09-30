-- L'Infini: an astral silver/cyan bezel, violet outer light and four star-set corners.
local api = EvolutionsItemFrames
local bonusNames = { "Égide des astres", "Éclat d'étoile filante", "Étincelle d'éternité" }

api.registerResolver(function(tooltip)
    local name = tooltip:GetName()
    if not name then return end
    for line = 2, tooltip:NumLines() do
        local label = _G[name .. "TextLeft" .. line]
        local text = label and label:GetText()
        if text then
            for _, bonus in ipairs(bonusNames) do
                if text:find(bonus, 1, true) then return "infiniteRaid" end
            end
        end
    end
end)

api.registerStyle("infiniteRaid", {
    create = function(parent)
        local art = CreateFrame("Frame", nil, parent)
        art:SetAllPoints(parent)
        art:EnableMouse(false)
        art:SetBackdrop({ edgeFile = "Interface\\Buttons\\WHITE8X8", edgeSize = 1 })
        art:SetBackdropBorderColor(0.64, 0.88, 1, 0.95)
        local glow = art:CreateTexture(nil, "BACKGROUND")
        glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        glow:SetPoint("TOPLEFT", -9, 9)
        glow:SetPoint("BOTTOMRIGHT", 9, -9)
        glow:SetBlendMode("ADD")
        glow:SetVertexColor(0.42, 0.25, 0.95, 0.65)
        for _, corner in ipairs({ "TOPLEFT", "TOPRIGHT", "BOTTOMLEFT", "BOTTOMRIGHT" }) do
            local star = art:CreateTexture(nil, "OVERLAY")
            star:SetTexture("Interface\\Cooldown\\star4")
            star:SetBlendMode("ADD")
            star:SetSize(13, 13)
            star:SetPoint("CENTER", art, corner)
            star:SetVertexColor(0.5, 0.85, 1, 0.95)
        end
        -- Static, quiet and readable at bag size: the icon, count and cooldown remain visible.
        return art
    end,
})
