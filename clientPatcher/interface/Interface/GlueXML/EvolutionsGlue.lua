-- Evolutions bits of the login screen: where the community buttons point. Loaded early (GlueXML.toc, right
-- after GlueStrings.lua) because AccountLogin.xml calls these from its buttons' OnLoad.
--
-- A link left empty hides its button, so only the ones that exist are shown.

EvolutionsLinks = {
    facebook = "",
    tiktok = "",
    youtube = "",
    discord = "",
}

function EvolutionsLink_OnLoad(button, key)
    button.evolutionsLink = EvolutionsLinks[key]
    if not button.evolutionsLink or button.evolutionsLink == "" then
        button:Hide()
    end
end

function EvolutionsLink_Open(button)
    if button.evolutionsLink and button.evolutionsLink ~= "" then
        LaunchURL(button.evolutionsLink)
    end
end
