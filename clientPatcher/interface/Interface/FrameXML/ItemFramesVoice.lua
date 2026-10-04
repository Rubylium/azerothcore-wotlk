-- The Hollow Voice's touched gear (one piece in four: Ailes du Séraphin, Égide d'Aldric, Murmure de Vel'thazar): a
-- painted frame of Aldric's white gold, cracked by the demon's void (localTools/interface/buildItemFrameArt.py), and
-- the void's violet breathing behind it.
local api = EvolutionsItemFrames
local layout = EvolutionsTooltip

local FRAME = "Interface\\ItemFrames\\ItemFrame-Voice"
local OPENING = 0.5872  -- the frame's empty middle, as a share of its texture (buildItemFrameArt.py prints it)
local INSET = 4         -- how far the frame's inner edge sits inside the icon: its bars lie on the icon's rim

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

        local glow = art:CreateTexture(nil, "BACKGROUND")
        glow:SetTexture("Interface\\Buttons\\UI-ActionButton-Border")
        glow:SetBlendMode("ADD")
        glow:SetVertexColor(0.55, 0.12, 0.85, 0.5)

        local frame = art:CreateTexture(nil, "OVERLAY")
        frame:SetTexture(FRAME)

        -- The texture is laid so its opening lands just inside the icon, whatever the button's size
        local function Layout(self)
            local width, height = self:GetWidth(), self:GetHeight()
            if width <= 0 or height <= 0 then return end
            local outX = ((width - 2 * INSET) / OPENING - width) / 2
            local outY = ((height - 2 * INSET) / OPENING - height) / 2
            frame:ClearAllPoints()
            frame:SetPoint("TOPLEFT", -outX, outY)
            frame:SetPoint("BOTTOMRIGHT", outX, -outY)
            glow:ClearAllPoints()
            glow:SetPoint("TOPLEFT", -outX - 6, outY + 6)
            glow:SetPoint("BOTTOMRIGHT", outX + 6, -outY - 6)
        end
        art:SetScript("OnSizeChanged", Layout)
        Layout(art)

        local breathe = glow:CreateAnimationGroup()
        breathe:SetLooping("BOUNCE")
        local fade = breathe:CreateAnimation("Alpha")
        fade:SetChange(-0.6)
        fade:SetDuration(1.8)
        fade:SetSmoothing("IN_OUT")
        breathe:Play()
        -- A hidden frame's animation stops with it
        art:SetScript("OnShow", function() breathe:Play() end)

        return art
    end,
})
