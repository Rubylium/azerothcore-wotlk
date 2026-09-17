-- RetailUI window specs (loaded after RetailUI.lua). Windows DragonUI reskins are deliberately absent.
-- chrome / inset: offsets from the window's TOPLEFT (left, top) and BOTTOMRIGHT (right, bottom).
-- Classic panels are 384x512 textures whose visible window ends ~30 px from the right and 45-75 px from the
-- bottom (their HitRectInsets); the portrait sits at (7, -6), which is exactly where retail puts it relative to a
-- window whose top-left corner is (12, -13). Values are starting points: tune in game with /rui (session only),
-- then copy the printed values here.
local RW = RetailUI

-- NPC dialogs: dark quest text needs parchment
RW.Register({
    frame = "GossipFrame", portrait = "GossipFramePortrait", close = "GossipFrameCloseButton",
    titles = { "GossipFrameNpcNameText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "parchment", inset = { 18, -72, -40, 102 },
})
RW.Register({
    frame = "QuestFrame", portrait = "QuestFramePortrait", close = "QuestFrameCloseButton",
    titles = { "QuestFrameNpcNameText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "parchment", inset = { 18, -72, -40, 102 },
})
RW.Register({
    frame = "PetitionFrame", portrait = "PetitionFramePortrait", close = "PetitionFrameCloseButton",
    titles = { "PetitionFrameNpcNameText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "parchment", inset = { 18, -72, -40, 102 },
})
RW.Register({
    frame = "GuildRegistrarFrame", portrait = "GuildRegistrarFramePortrait", close = "GuildRegistrarFrameCloseButton",
    titles = { "GuildRegistrarFrameNpcNameText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "parchment", inset = { 18, -72, -40, 102 },
})
RW.Register({
    frame = "ItemTextFrame", close = "ItemTextCloseButton", titles = { "ItemTextTitleText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "parchment", inset = { 18, -72, -40, 102 },
})

-- Lists and slots: dark marble
RW.Register({
    frame = "MerchantFrame", portrait = "MerchantFramePortrait", close = "MerchantFrameCloseButton",
    titles = { "MerchantNameText" },
    chrome = { 12, -13, -37, 62 }, insetStyle = "marble", inset = { 18, -70, -43, 88 },
})
RW.Register({
    frame = "SpellBookFrame", portrait = "SpellBookFrameIcon", close = "SpellBookCloseButton",
    titles = { "SpellBookTitleText" },
    chrome = { 12, -13, -32, 72 }, insetStyle = "marble", inset = { 18, -72, -40, 80 },
})
RW.Register({
    frame = "DressUpFrame", portrait = "DressUpFramePortrait", close = "DressUpFrameCloseButton",
    titles = { "DressUpFrameTitleText" },
    keep = { "DressUpBackgroundTopLeft", "DressUpBackgroundTopRight", "DressUpBackgroundBotLeft",
        "DressUpBackgroundBotRight" },
    -- No inset: the race backdrop behind the model is the ground
    chrome = { 12, -13, -32, 47 },
})
