-- "Réinitialiser" on the talent window: the active specialisation is given back for free, out of combat
-- (mod-stat-growth TalentResetSystem.cpp answers the "Talents" RESET addon message).
--
-- The talent window is load-on-demand (Blizzard_TalentUI): the button is made once, when that addon loads, as
-- a child of the window, so it moves, layers and hides with it. Nothing here runs frame by frame -- a button
-- that followed the window on every frame is what used to freeze the game while the window was dragged.

local PREFIX = "Talents"

StaticPopupDialogs["EVOLUTIONS_RESET_TALENTS"] = {
    text = "Réinitialiser tous les talents de votre spécialisation active ?",
    button1 = YES,
    button2 = NO,
    OnAccept = function()
        SendAddonMessage(PREFIX, "RESET", "WHISPER", UnitName("player"))
    end,
    timeout = 0,
    whileDead = 1,
    hideOnEscape = 1,
}

local button

local function Build()
    if button or not PlayerTalentFrame then
        return
    end

    button = CreateFrame("Button", "EvolutionsTalentResetButton", PlayerTalentFrame, "UIPanelButtonTemplate")
    button:SetWidth(92)
    button:SetHeight(20)
    button:SetText("Réinitialiser")
    button:SetPoint("TOPRIGHT", PlayerTalentFrame, "TOPRIGHT", -46, -44)
    button:SetScript("OnClick", function()
        if UnitAffectingCombat("player") then
            UIErrorsFrame:AddMessage("Impossible en combat.", 1, 0.1, 0.1)
            return
        end
        StaticPopup_Show("EVOLUTIONS_RESET_TALENTS")
    end)
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText("Réinitialiser les talents")
        GameTooltip:AddLine("Rend tous les points de la spécialisation active. Gratuit, hors combat.", 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", GameTooltip_Hide)
end

-- Someone else's talents, or a pet's, are not ours to reset
hooksecurefunc("TalentFrame_Update", function(frame)
    if frame ~= PlayerTalentFrame then
        return
    end
    Build()
    if PlayerTalentFrame.inspect or PlayerTalentFrame.pet then
        button:Hide()
    else
        button:Show()
    end
end)

local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(_, _, name)
    if name == "Blizzard_TalentUI" then
        Build()
    end
end)
