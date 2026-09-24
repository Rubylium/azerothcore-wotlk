-- Evolutions: realm selection.
--
-- Icecrown Citadel's gate behind everything, drifting slowly, the logo and a title above, and one card per realm,
-- centred however many there are. A card shows the realm's name, whether it is up, its type, how many characters
-- the account has there and how full it is. Click a card to select it, double-click (or Rejoindre) to join.
-- The art is built by localTools/interface/buildGlueArt.py.

local ART = "Interface\\Glues\\Evolutions\\"
local FONT_TITLE = "Fonts\\MORPHEUS.TTF"
local FONT_TEXT = "Fonts\\FRIZQT__.TTF"
local WHITE = "Interface\\Buttons\\WHITE8X8"

-- The pictures' own proportions (width / height), see buildGlueArt.py
local BACKDROP_ASPECT = 2.44
local CARD_ART_ASPECT = 0.64

local CARD_WIDTH, CARD_HEIGHT, CARD_GAP = 290, 430, 36
-- Below the title's divider
local CARD_TOP_OFFSET = -26
-- How far the selected card's light reaches past it (buildGlueArt.py draws the light for this)
local CARD_GLOW_MARGIN = 28
local MAX_CARDS = 5

local GOLD = { 1, 0.82, 0.45 }
local GOLD_DIM = { 0.74, 0.6, 0.36 }
local MUTED = { 0.58, 0.64, 0.72 }

local L = {
    title = "Choix du royaume",
    subtitle = "Choisissez le monde dans lequel votre légende s'écrira.",
    join = "Rejoindre",
    online = "En ligne",
    offline = "Hors ligne",
    locked = "Verrouillé",
    characters = "PERSONNAGES",
    population = "POPULATION",
    expansion = "Wrath of the Lich King",
    pve = "JcE", pvp = "JcJ", rp = "JdR", rppvp = "JdR-JcJ",
}

REALM_BUTTON_HEIGHT = 16;
MAX_REALMS_DISPLAYED = MAX_CARDS;

local cards = {}
local backdrop, logo, title, subtitle, divider
local tabs = {}
local drift, intro = 0, 0

-- --- small helpers ----------------------------------------------------------------------------------------------

local function Text(parent, font, size, layer, flags)
    local text = parent:CreateFontString(nil, layer or "OVERLAY")
    text:SetFont(font, size, flags or "")
    text:SetShadowColor(0, 0, 0, 0.9)
    text:SetShadowOffset(1, -1)
    return text
end

local function Solid(parent, layer, r, g, b, a)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    texture:SetTexture(WHITE)
    texture:SetVertexColor(r, g, b, a or 1)
    return texture
end

-- A gradient strip: alpha a1 at its start, a2 at its end (HORIZONTAL: left to right, VERTICAL: bottom to top)
local function Fade(parent, layer, orientation, r, g, b, a1, a2)
    local texture = parent:CreateTexture(nil, layer or "ARTWORK")
    texture:SetTexture(WHITE)
    texture:SetGradientAlpha(orientation, r, g, b, a1, r, g, b, a2)
    return texture
end

-- Shows the part of a picture (of the given proportions) that fills a box of width x height, like CSS "cover",
-- enlarged by zoom and centred on focusX / focusY (0-1 across the picture)
local function Cover(texture, aspect, width, height, zoom, focusX, focusY)
    if width <= 0 or height <= 0 then return end
    local u, v = 1, 1
    if width / height > aspect then
        v = aspect / (width / height)
    else
        u = (width / height) / aspect
    end
    u, v = u / zoom, v / zoom
    local left = math.min(math.max((focusX or 0.5) - u / 2, 0), 1 - u)
    local top = math.min(math.max((focusY or 0.5) - v / 2, 0), 1 - v)
    texture:SetTexCoord(left, left + u, top, top + v)
end

-- A horizontal line that fades out at both ends
local function Divider(parent, width, r, g, b, a)
    local frame = CreateFrame("Frame", nil, parent)
    frame:SetSize(width, 1)
    local left = Fade(frame, "ARTWORK", "HORIZONTAL", r, g, b, 0, a)
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMRIGHT", frame, "BOTTOM")
    local right = Fade(frame, "ARTWORK", "HORIZONTAL", r, g, b, a, 0)
    right:SetPoint("TOPLEFT", frame, "TOP")
    right:SetPoint("BOTTOMRIGHT")
    return frame
end

local function Smooth(value)
    return value * value * (3 - 2 * value)
end

-- --- a realm card ------------------------------------------------------------------------------------------------

local function SetBorderColor(card, r, g, b, a)
    for _, edge in ipairs(card.edges) do
        edge:SetVertexColor(r, g, b, a)
    end
end

local function SetGlow(card, alpha)
    card.glow:SetVertexColor(GOLD[1], GOLD[2], GOLD[3], 0.85 * alpha)
end

local function RefreshCardLook(card)
    if card.selected then
        SetBorderColor(card, GOLD[1], GOLD[2], GOLD[3], 1)
    elseif card.hovered then
        SetBorderColor(card, GOLD[1], GOLD[2], GOLD[3], 0.7)
    else
        SetBorderColor(card, GOLD_DIM[1], GOLD_DIM[2], GOLD_DIM[3], 0.35)
    end
    card.name:SetTextColor(card.selected and 1 or 0.92, card.selected and 0.88 or 0.9, card.selected and 0.62 or 0.86)
end

local function CreateCard(index)
    local card = CreateFrame("Button", "RealmListRealmButton" .. index, RealmList)
    card:SetSize(CARD_WIDTH, CARD_HEIGHT)
    card:RegisterForClicks("LeftButtonUp")

    -- A soft light around the selected card, rounded at the corners (RealmCardGlow, buildGlueArt.py), on a frame of
    -- its own under the card
    card:SetFrameLevel(RealmList:GetFrameLevel() + 3)
    local halo = CreateFrame("Frame", nil, card)
    halo:SetFrameLevel(RealmList:GetFrameLevel() + 2)
    halo:SetPoint("TOPLEFT", -CARD_GLOW_MARGIN, CARD_GLOW_MARGIN)
    halo:SetPoint("BOTTOMRIGHT", CARD_GLOW_MARGIN, -CARD_GLOW_MARGIN)
    card.glow = halo:CreateTexture(nil, "BACKGROUND")
    card.glow:SetTexture(ART .. "RealmCardGlow")
    card.glow:SetBlendMode("ADD")
    card.glow:SetAllPoints()
    SetGlow(card, 0)

    local base = Solid(card, "BACKGROUND", 0.03, 0.04, 0.06, 0.96)
    base:SetPoint("TOPLEFT", 1, -1)
    base:SetPoint("BOTTOMRIGHT", -1, 1)

    card.art = card:CreateTexture(nil, "BORDER")
    card.art:SetTexture(ART .. "RealmCard")
    card.art:SetPoint("TOPLEFT", 1, -1)
    card.art:SetPoint("BOTTOMRIGHT", -1, 1)
    card.zoom, card.targetZoom = 1, 1
    Cover(card.art, CARD_ART_ASPECT, CARD_WIDTH, CARD_HEIGHT, 1, 0.5, 0.3)

    -- The picture fades into the card's dark lower half, where the text is
    local lower = Fade(card, "ARTWORK", "VERTICAL", 0.02, 0.03, 0.05, 1, 0)
    lower:SetPoint("BOTTOMLEFT", 1, 1)
    lower:SetPoint("TOPRIGHT", card, "RIGHT", -1, 40)
    local upper = Fade(card, "ARTWORK", "VERTICAL", 0.02, 0.03, 0.05, 0, 0.75)
    upper:SetPoint("TOPLEFT", 1, -1)
    upper:SetPoint("BOTTOMRIGHT", card, "TOPRIGHT", -1, -70)

    card.edges = {}
    local edgeSpec = {
        { "TOPLEFT", "TOPRIGHT", 0, -1 }, { "BOTTOMLEFT", "BOTTOMRIGHT", 0, 1 },
    }
    for _, spec in ipairs(edgeSpec) do
        local edge = Solid(card, "OVERLAY", 1, 1, 1, 1)
        edge:SetPoint(spec[1])
        edge:SetPoint(spec[2])
        edge:SetHeight(1)
        card.edges[#card.edges + 1] = edge
    end
    for _, point in ipairs({ "LEFT", "RIGHT" }) do
        local edge = Solid(card, "OVERLAY", 1, 1, 1, 1)
        edge:SetPoint("TOP" .. point)
        edge:SetPoint("BOTTOM" .. point)
        edge:SetWidth(1)
        card.edges[#card.edges + 1] = edge
    end

    -- Status (up / down) and type, over the top of the picture
    card.statusDot = card:CreateTexture(nil, "OVERLAY")
    card.statusDot:SetSize(14, 14)
    card.statusDot:SetPoint("TOPLEFT", 14, -14)
    card.status = Text(card, FONT_TEXT, 11)
    card.status:SetPoint("LEFT", card.statusDot, "RIGHT", 4, 0)
    card.kind = Text(card, FONT_TEXT, 11)
    card.kind:SetPoint("TOPRIGHT", -16, -16)
    card.kind:SetTextColor(GOLD[1], GOLD[2], GOLD[3])

    card.name = Text(card, FONT_TITLE, 32)
    card.name:SetPoint("BOTTOM", card, "BOTTOM", 0, 150)
    card.name:SetWidth(CARD_WIDTH - 24)
    card.sub = Text(card, FONT_TEXT, 11)
    card.sub:SetPoint("TOP", card.name, "BOTTOM", 0, -6)
    card.sub:SetTextColor(MUTED[1], MUTED[2], MUTED[3])

    card.divider = Divider(card, CARD_WIDTH - 60, GOLD[1], GOLD[2], GOLD[3], 0.55)
    card.divider:SetPoint("TOP", card.sub, "BOTTOM", 0, -16)

    -- Two figures side by side: label above, value below
    card.stats = {}
    for column = 1, 2 do
        local x = (column == 1) and -CARD_WIDTH / 4 or CARD_WIDTH / 4
        local label = Text(card, FONT_TEXT, 9)
        label:SetPoint("TOP", card.divider, "BOTTOM", x, -16)
        label:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
        local value = Text(card, FONT_TEXT, 17)
        value:SetPoint("TOP", label, "BOTTOM", 0, -6)
        card.stats[column] = { label = label, value = value }
    end
    local separator = Solid(card, "OVERLAY", GOLD_DIM[1], GOLD_DIM[2], GOLD_DIM[3], 0.3)
    separator:SetSize(1, 34)
    separator:SetPoint("TOP", card.divider, "BOTTOM", 0, -14)

    card:SetScript("OnClick", function(self) RealmSelectButton_OnClick(self, self:GetID()) end)
    card:SetScript("OnDoubleClick", function(self) RealmSelectButton_OnDoubleClick(self, self:GetID()) end)
    card:SetScript("OnEnter", function(self)
        self.hovered = true
        self.targetZoom = 1.06
        RefreshCardLook(self)
    end)
    card:SetScript("OnLeave", function(self)
        self.hovered = false
        self.targetZoom = 1
        RefreshCardLook(self)
    end)
    card:SetScript("OnUpdate", function(self, elapsed)
        -- The picture leans in under the pointer, the card lifts, and the selected one glows
        if math.abs(self.zoom - self.targetZoom) > 0.0005 then
            self.zoom = self.zoom + (self.targetZoom - self.zoom) * math.min(elapsed * 6, 1)
            Cover(self.art, CARD_ART_ASPECT, CARD_WIDTH, CARD_HEIGHT, self.zoom, 0.5, 0.3)
        end
        local lift = self.hovered and 6 or 0
        self.lift = (self.lift or 0) + (lift - (self.lift or 0)) * math.min(elapsed * 10, 1)
        local glowTarget = self.selected and 1 or 0
        if math.abs((self.glowAlpha or 0) - glowTarget) > 0.01 then
            self.glowAlpha = (self.glowAlpha or 0) + (glowTarget - (self.glowAlpha or 0)) * math.min(elapsed * 8, 1)
            SetGlow(self, self.glowAlpha)
        end
        if self.baseX then
            self:SetPoint("TOP", divider, "BOTTOM", self.baseX, CARD_TOP_OFFSET + self.lift)
        end
    end)

    card:Hide()
    return card
end

-- --- the screen --------------------------------------------------------------------------------------------------

local function Build(self)
    if backdrop then return end

    local black = Solid(self, "BACKGROUND", 0, 0, 0, 1)
    black:SetAllPoints()
    backdrop = self:CreateTexture(nil, "BACKGROUND")
    backdrop:SetTexture(ART .. "RealmBackdrop")
    backdrop:SetAllPoints()

    -- Darken it enough for the cards to stand out, most at the edges
    local shade = Solid(self, "BORDER", 0.01, 0.02, 0.03, 0.45)
    shade:SetAllPoints()
    local top = Fade(self, "BORDER", "VERTICAL", 0, 0, 0, 0, 0.92)
    top:SetPoint("TOPLEFT")
    top:SetPoint("TOPRIGHT")
    top:SetHeight(260)
    local bottom = Fade(self, "BORDER", "VERTICAL", 0, 0, 0, 0.95, 0)
    bottom:SetPoint("BOTTOMLEFT")
    bottom:SetPoint("BOTTOMRIGHT")
    bottom:SetHeight(220)
    local left = Fade(self, "BORDER", "HORIZONTAL", 0, 0, 0, 0.85, 0)
    left:SetPoint("TOPLEFT")
    left:SetPoint("BOTTOMLEFT")
    left:SetWidth(320)
    local right = Fade(self, "BORDER", "HORIZONTAL", 0, 0, 0, 0, 0.85)
    right:SetPoint("TOPRIGHT")
    right:SetPoint("BOTTOMRIGHT")
    right:SetWidth(320)

    logo = self:CreateTexture(nil, "ARTWORK")
    logo:SetTexture("Interface\\Glues\\Common\\Glues-WoW-WotLKLogo")
    logo:SetSize(190, 95)
    title = Text(self, FONT_TITLE, 30)
    title:SetText(L.title)
    title:SetTextColor(GOLD[1], GOLD[2], GOLD[3])
    title:SetPoint("TOP", self, "TOP", 0, -112)
    subtitle = Text(self, FONT_TEXT, 13)
    subtitle:SetText(L.subtitle)
    subtitle:SetTextColor(MUTED[1], MUTED[2], MUTED[3])
    subtitle:SetPoint("TOP", title, "BOTTOM", 0, -6)
    divider = Divider(self, 420, GOLD[1], GOLD[2], GOLD[3], 0.6)
    divider:SetPoint("TOP", subtitle, "BOTTOM", 0, -12)

    for index = 1, MAX_CARDS do
        cards[index] = CreateCard(index)
    end

    RealmListOkButton:SetText(L.join)
end

function RealmList_Layout(self)
    if not backdrop then return end
    local width, height = self:GetWidth(), self:GetHeight()
    Cover(backdrop, BACKDROP_ASPECT, width, height, 1 + 0.05 * Smooth(drift), 0.42 + 0.06 * drift, 0.5)
end

local function PositionCards(count)
    local total = count * CARD_WIDTH + (count - 1) * CARD_GAP
    for index, card in ipairs(cards) do
        if index <= count then
            card.baseX = -total / 2 + CARD_WIDTH / 2 + (index - 1) * (CARD_WIDTH + CARD_GAP)
            card:ClearAllPoints()
            card:SetPoint("TOP", divider, "BOTTOM", card.baseX, CARD_TOP_OFFSET)
        end
    end
end

local function RealmType(pvp, rp)
    if pvp and rp then return L.rppvp end
    if rp then return L.rp end
    if pvp then return L.pvp end
    return L.pve
end

-- How full the realm is, and in what colour
local function RealmLoad(load)
    if load == 2.0 then return LOAD_FULL, 1, 0.3, 0.25 end
    if load == -3.0 then return LOAD_RECOMMENDED, 0.45, 0.75, 1 end
    if load == -2.0 then return LOAD_NEW, 0.4, 1, 0.45 end
    if load > 0 then return LOAD_HIGH, 1, 0.45, 0.3 end
    if load < 0 then return LOAD_LOW, 0.4, 1, 0.45 end
    return LOAD_MEDIUM, 1, 0.82, 0.3
end

local function UpdateTabs(...)
    local count = select("#", ...)
    for index = 1, math.max(count, #tabs) do
        local tab = tabs[index]
        if index <= count and count > 1 then
            if not tab then
                tab = CreateFrame("Button", nil, RealmList)
                tab:SetSize(160, 24)
                tab.text = Text(tab, FONT_TEXT, 12)
                tab.text:SetPoint("CENTER")
                tab.line = Solid(tab, "OVERLAY", GOLD[1], GOLD[2], GOLD[3], 1)
                tab.line:SetPoint("BOTTOMLEFT", 24, 0)
                tab.line:SetPoint("BOTTOMRIGHT", -24, 0)
                tab.line:SetHeight(2)
                tab:SetScript("OnClick", function(self) RealmListTab_OnClick(self) end)
                tabs[index] = tab
            end
            tab:SetID(index)
            tab.text:SetText(select(index, ...))
            tab.disabled = IsInvalidTournamentRealmCategory(index)
            local selected = RealmList.selectedCategory == index
            tab.text:SetTextColor(selected and GOLD[1] or MUTED[1], selected and GOLD[2] or MUTED[2],
                selected and GOLD[3] or MUTED[3])
            if selected then tab.line:Show() else tab.line:Hide() end
            tab:ClearAllPoints()
            tab:SetPoint("TOP", divider, "BOTTOM", (index - (count + 1) / 2) * 170, -10)
            tab:Show()
        elseif tab then
            tab:Hide()
        end
    end
end

function RealmList_OnLoad(self)
    self:RegisterEvent("OPEN_REALM_LIST");
    self.currentRealm = 0;
    self.offset = 0;
end

function RealmList_OnEvent(self, event)
    if ( event == "OPEN_REALM_LIST" ) then
        if ( self:IsShown() ) then
            RealmListUpdate();
        else
            self:Show();
        end
    end
end

function RealmListUpdate()
    if ( not RealmList.selectedCategory ) then
        RealmList.selectedCategory = 1;
    end
    RealmList.refreshTime = RealmListUpdateRate();
    UpdateTabs(GetRealmCategories());

    local numRealms = GetNumRealms(RealmList.selectedCategory);
    local shown = math.min(numRealms, MAX_CARDS);
    PositionCards(shown);
    RealmListOkButton:Disable();

    for index = 1, MAX_CARDS do
        local card = cards[index];
        local name, numCharacters, invalidRealm, realmDown, currentRealm, pvp, rp, load, locked, major, minor,
            revision = GetRealmInfo(RealmList.selectedCategory, index);
        if ( index > shown or not name ) then
            card:Hide();
        else
            card:SetID(index);
            card.name:SetText(name);
            card.sub:SetText(L.expansion .. "  |cff6f7a88·|r  " .. (major and (major .. "." .. minor .. "." .. revision)
                or "3.3.5a"));
            card.kind:SetText(RealmType(pvp, rp));

            if ( realmDown ) then
                card.statusDot:SetTexture("Interface\\FriendsFrame\\StatusIcon-Offline");
                card.status:SetText(L.offline);
                card.status:SetTextColor(1, 0.45, 0.4);
            elseif ( locked ) then
                card.statusDot:SetTexture("Interface\\FriendsFrame\\StatusIcon-Away");
                card.status:SetText(L.locked);
                card.status:SetTextColor(1, 0.82, 0.3);
            else
                card.statusDot:SetTexture("Interface\\FriendsFrame\\StatusIcon-Online");
                card.status:SetText(L.online);
                card.status:SetTextColor(0.55, 1, 0.55);
            end

            card.stats[1].label:SetText(L.characters);
            card.stats[1].value:SetText(numCharacters or 0);
            card.stats[1].value:SetTextColor(1, 1, 1);
            local loadText, r, g, b = RealmLoad(load or 0);
            if ( realmDown ) then
                loadText, r, g, b = REALM_DOWN, 0.6, 0.6, 0.6;
            end
            card.stats[2].label:SetText(L.population);
            card.stats[2].value:SetText(loadText);
            card.stats[2].value:SetTextColor(r, g, b);
            card.realmDown = realmDown;
            card.isFull = (load == 2.0) and numCharacters == 0;
            card.realmName = name;
            card.name:SetAlpha(invalidRealm and 0.5 or 1);

            local selected;
            if ( RealmList.selectedName ) then
                selected = (name == RealmList.selectedName);
            else
                selected = (currentRealm == 1) or (RealmList.currentRealm == index);
            end
            card.selected = selected;
            if ( selected ) then
                RealmList.currentRealm = index;
                RealmList.showRealmIsFullDialog = card.isFull and 1 or nil;
                if ( not realmDown ) then
                    RealmListOkButton:Enable();
                end
            end
            RefreshCardLook(card);
            card:Show();
        end
    end

    -- Nothing selected yet: the first realm that is up
    if ( not RealmListOkButton:IsEnabled() ) then
        for index = 1, shown do
            if ( cards[index]:IsShown() and not cards[index].realmDown ) then
                RealmList.currentRealm = index;
                cards[index].selected = true;
                RefreshCardLook(cards[index]);
                RealmListOkButton:Enable();
                break;
            end
        end
    end
    RealmList.selectedName = nil;
end

function RealmList_OnKeyDown(key)
    if ( key == "ESCAPE" ) then
        RealmList_OnCancel();
    elseif ( key == "ENTER" ) then
        RealmList_OnOk();
    elseif ( key == "LEFT" or key == "RIGHT" ) then
        local count = GetNumRealms(RealmList.selectedCategory);
        local target = RealmList.currentRealm + (key == "LEFT" and -1 or 1);
        if ( target >= 1 and target <= math.min(count, MAX_CARDS) ) then
            RealmSelectButton_OnClick(cards[target], target);
        end
    elseif ( key == "PRINTSCREEN" ) then
        Screenshot();
    end
end

function RealmList_OnOk()
    if ( not RealmListOkButton:IsEnabled() ) then
        return;
    end
    PlaySound("gsLoginChangeRealmOK");
    RealmList:Hide();
    -- If trying to join a Full realm then popup a dialog
    if ( RealmList.showRealmIsFullDialog ) then
        GlueDialog_Show("REALM_IS_FULL");
        return;
    end
    if ( RealmList.currentRealm > 0 ) then
        ChangeRealm(RealmList.selectedCategory, RealmList.currentRealm);
    end
end

function RealmList_OnCancel()
    PlaySound("gsLoginChangeRealmCancel");
    RealmList:Hide();
    RealmListDialogCancelled();
    local serverName, isPVP, isRP, isDown = GetServerName();

    if ( (GetNumRealms(RealmList.selectedCategory) == 0) or (isDown) ) then
        SetGlueScreen("realmwizard");
    end
end

function RealmSelectButton_OnClick(self, id)
    if ( IsInvalidLocale(RealmList.selectedCategory) ) then
        --Display popup explaining locale specific realms
        GlueDialog_Show("REALM_LOCALE_WARNING");
    else
        if ( RealmList.currentRealm ~= id ) then
            PlaySound("gsCharacterCreationClass");
        end
        RealmList.refreshTime = RealmListUpdateRate();
        RealmList.currentRealm = id;
        RealmList.selectedName = self.realmName;
        RealmListUpdate();
    end
end

function RealmSelectButton_OnDoubleClick(self, id)
    if ( IsInvalidLocale(RealmList.selectedCategory) ) then
        --Display popup explaining locale specific realms
        GlueDialog_Show("REALM_LOCALE_WARNING");
    elseif ( not self.realmDown ) then
        RealmList.currentRealm = id;
        RealmList.selectedName = self.realmName;
        RealmListUpdate();
        RealmList_OnOk();
    end
end

function RealmList_OnShow(self)
    Build(self)
    intro = 0
    RealmList_Layout(self)
    RealmListUpdate();
    self.refreshTime = RealmListUpdateRate();
    local selectedCategory = GetSelectedCategory();
    if ( selectedCategory == 0 ) then
        selectedCategory = 1;
    end
    RealmList.selectedCategory = selectedCategory;
    RealmListUpdate();
end

function RealmList_OnHide()
    CancelRealmListQuery()
end

function RealmList_OnUpdate(self, elapsed)
    -- The picture drifts and breathes over a minute; everything fades in on opening, the cards last
    drift = (drift + elapsed / 60) % 2
    local phase = drift < 1 and drift or 2 - drift
    Cover(backdrop, BACKDROP_ASPECT, self:GetWidth(), self:GetHeight(), 1 + 0.05 * Smooth(phase),
        0.42 + 0.06 * Smooth(phase), 0.5)
    if intro < 1 then
        intro = math.min(intro + elapsed / 0.9, 1)
        local eased = Smooth(intro)
        logo:SetAlpha(eased)
        logo:ClearAllPoints()
        logo:SetPoint("TOP", self, "TOP", 0, -14 + 12 * (1 - eased))
        title:SetAlpha(eased)
        subtitle:SetAlpha(eased)
        divider:SetAlpha(eased)
        local cardsIn = Smooth(math.max(0, (intro - 0.25) / 0.75))
        for _, card in ipairs(cards) do
            card:SetAlpha(cardsIn)
        end
    end

    if ( self.refreshTime ) then
        self.refreshTime = self.refreshTime - elapsed;
        if ( self.refreshTime <= 0 ) then
            self.refreshTime = nil;
            RequestRealmList();
        end
    end

    -- Account Msg stuff
    if ( (ACCOUNT_MSG_NUM_AVAILABLE > 0) and not GlueDialog:IsShown() ) then
        if ( ACCOUNT_MSG_HEADERS_LOADED ) then
            if ( ACCOUNT_MSG_BODY_LOADED ) then
                local dialogString = AccountMsg_GetHeaderSubject( ACCOUNT_MSG_CURRENT_INDEX ).."\n\n"..AccountMsg_GetBody();
                GlueDialog_Show("ACCOUNT_MSG", dialogString);
            end
        end
    end
end

function RealmListTab_OnClick(tab)
    if ( tab.disabled ) then
        if ( IsTournamentRealmCategory(tab:GetID()) ) then
            GlueDialog_Show("REALM_TOURNAMENT_WARNING");
        end
        return;
    end
    RealmList.selectedCategory = tab:GetID();
    RealmList.currentRealm = 0;
    RealmListUpdate();
end
