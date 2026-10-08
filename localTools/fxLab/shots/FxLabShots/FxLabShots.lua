-- LOCAL DEV ONLY - never ship (localTools/fxLab/shots/shoot.ps1 installs it for one run, then removes it).
-- Plays the run's steps (Steps.lua: FxLabSteps, { seconds to wait, a chat line or a function }): a chat line is sent
-- as said (a "." line is a game master command), a function is called. Shot() takes a screenshot; Note(text) writes a
-- line in the chat, so the next shot shows it.
function Shot()
    Screenshot()
end

function Note(text)
    DEFAULT_CHAT_FRAME:AddMessage("|cff80ff80[FxLab]|r " .. tostring(text))
end
