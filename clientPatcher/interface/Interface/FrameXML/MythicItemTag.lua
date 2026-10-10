-- Where a generated item comes from, on its tooltip's second line as a raid names its difficulty there
-- (ItemTooltipLayout.lua puts it in place of the client's "Heroic"), and its boss's touch moved where it reads well.
--
-- Nothing has to be asked of the server: a generated item is recognisable from its id alone. The generator builds
-- these entries as GeneratedItemBase * (variant + 1) + base entry, with every real item below GeneratedItemBase
-- (src/server/game/Maps/MythicDungeon.h, and awesome_wotlk's GeneratedItems.cpp, which draws them). A boss's touch is
-- told by its bonus's name among the lines (ItemTooltipLayout.lua EvolutionsTooltip.touches).

local layout = EvolutionsTooltip

local GENERATED_ITEM_BASE = 0x10000     -- MythicDungeon.h, GeneratedItemBase
local MYTHIC_VARIANTS = 128             -- MythicDungeon.h, GeneratedItemVariants
local FORGE_RANKS = 8                   -- MythicDungeon.h, ForgeRanks: the Forge's ranks come after the variants
local PINNACLE_VARIANT = 48             -- MythicDungeon.h, MaxPinnacleItemLevel (477): the Hollow Voice's loot
local SET_PIECE_BLOCK = 140             -- MythicDungeon.h, FirstSetPieceBlock: set pieces after the caps
local SET_PIECE_PROFILES = 32           -- MythicDungeon.h, SetPieceProfiles

local french = GetLocale() == "frFR"
local TAG = french and "Mythique+" or "Mythic+"
local VOICE_TAG = french and "La Voix creuse" or "The Hollow Voice"
local GOD_TAG = "L'Infini"
local FORGE_TAG = french and "Forgé %d/%d" or "Forged %d/%d"

-- Set pieces (mod-legendary SetPieces.cpp): a raid item's stats on a set's own row; their source is their set, by
-- that row (the entry's base)
local SETS = {}
local function AddSet(set, rows)
    for _, row in ipairs(rows) do
        SETS[row] = french and ("Ensemble · " .. set[1]) or ("Set · " .. set[2])
    end
end
AddSet({ "Harnois du Gardien-chef", "Head Warden's Battlegear" },
    { 13710, 13711, 13712, 13713, 13714, 13715, 13716, 13717 })
AddSet({ "Mailles du Porte-chaînes", "Chainbearer's Mail" },
    { 13672, 13673, 13674, 13675, 13676, 13677, 13678, 13679 })
AddSet({ "Cuirs du Traqueur d'évadés", "Escape-Hunter's Leathers" },
    { 13680, 13681, 13682, 13683, 13684, 13685, 13686, 13687 })
AddSet({ "Atours du Lieur de sceaux", "Sealbinder's Regalia" },
    { 13688, 13689, 13690, 13691, 13692, 13693, 13694, 13695 })
AddSet({ "Geôle des Flammes infernales", "Hellfire Gaol" }, { 13696, 13697, 12187 })
-- His weapons (SetPieces.cpp WeaponRows)
AddSet({ "Geôle des Flammes infernales", "Hellfire Gaol" },
    { 5551, 5552, 5553, 5554, 5555, 5556, 5557, 5558, 5560, 5561, 4899, 4900, 4912, 4956, 4985, 4996 })

-- The source of a generated item, and its colour: the client's own green for a difficulty, the Forge's orange, the
-- touching boss's own
local function SourceOf(tooltip, link)
    local id = link and tonumber(link:match("item:(%d+)"))
    if not id or id < GENERATED_ITEM_BASE then
        return nil
    end

    local block = floor(id / GENERATED_ITEM_BASE)
    if block >= SET_PIECE_BLOCK then
        local set = block < SET_PIECE_BLOCK + SET_PIECE_PROFILES and SETS[id % GENERATED_ITEM_BASE]
        if set then
            return set, 0.64, 0.21, 0.93
        end
        return nil
    end
    local _, touch = layout.FindTouch(tooltip)
    if touch == "infini" then
        return GOD_TAG, 0.5, 0.75, 1
    elseif touch == "voice" or block - 1 == PINNACLE_VARIANT then
        return VOICE_TAG, 0.85, 0.7, 1
    elseif block <= MYTHIC_VARIANTS or (block > MYTHIC_VARIANTS + FORGE_RANKS
        and block <= MYTHIC_VARIANTS + FORGE_RANKS + 3) then
        return TAG, 0.1, 1, 0.1
    elseif block <= MYTHIC_VARIANTS + FORGE_RANKS then
        return format(FORGE_TAG, block - MYTHIC_VARIANTS, FORGE_RANKS), 1, 0.62, 0.25
    end
end

local function Tag(tooltip)
    -- Guarded: several of these fire OnTooltipSetItem more than once for one hover
    if tooltip.mythicTagged then
        return
    end
    local _, link = tooltip:GetItem()
    local text, r, g, b = SourceOf(tooltip, link)
    if not text then
        return
    end
    tooltip.mythicTagged = true
    layout.SetSource(tooltip, text, r, g, b)
    layout.PlaceTouch(tooltip)
end

local function Untag(tooltip)
    tooltip.mythicTagged = nil
end

-- Every tooltip that can show an item: the one under the cursor, a link clicked in chat, and the ones the client
-- uses to compare against what is already worn
for _, name in ipairs({ "GameTooltip", "ItemRefTooltip", "ShoppingTooltip1", "ShoppingTooltip2", "ShoppingTooltip3" }) do
    local tooltip = _G[name]
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Tag)
        -- Cleared, not hidden: a tooltip is reused for the next item without ever going away
        tooltip:HookScript("OnTooltipCleared", Untag)
    end
end
