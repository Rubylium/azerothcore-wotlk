#include "EssenceFeedback.h"

#include "Player.h"
#include "WorldSession.h"

namespace
{
constexpr uint32 LEVEL_UP_VISUAL_KIT = 6375;
constexpr uint32 RESURRECTION_VISUAL_KIT = 6334;
constexpr uint32 QUEST_COMPLETE_SOUND = 619;
constexpr uint32 LEVEL_UP_SOUND = 888;
constexpr uint32 ACHIEVEMENT_SOUND = 12891;

uint32 GetEssenceVisualKit(EssenceVisual visual)
{
    switch (visual)
    {
        case EssenceVisual::Growth:
            return 7761; // Green
        case EssenceVisual::Experience:
            return 7760; // Blue
        case EssenceVisual::Resource:
            return 7965; // Arcane blue
        case EssenceVisual::Vitality:
            return 7762; // Red
        case EssenceVisual::Fortune:
            return 7763; // Gold
    }

    return LEVEL_UP_VISUAL_KIT;
}
}

void PlayEssenceFeedback(Player* player, EssenceVisual visual, EssenceTier tier, std::string_view announcement)
{
    if (!player)
        return;

    player->SendPlaySpellVisual(GetEssenceVisualKit(visual));
    if (tier != EssenceTier::Faint)
        player->SendPlaySpellVisual(LEVEL_UP_VISUAL_KIT);
    if (tier == EssenceTier::Ascendant)
        player->SendPlaySpellVisual(RESURRECTION_VISUAL_KIT);

    uint32 sound = QUEST_COMPLETE_SOUND;
    if (tier == EssenceTier::Greater)
        sound = LEVEL_UP_SOUND;
    else if (tier == EssenceTier::Ascendant)
        sound = ACHIEVEMENT_SOUND;

    player->PlayDirectSound(sound, player);
    player->GetSession()->SendAreaTriggerMessage(announcement);
}
