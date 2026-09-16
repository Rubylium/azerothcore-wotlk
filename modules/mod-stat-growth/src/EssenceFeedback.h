#ifndef MOD_STAT_GROWTH_ESSENCE_FEEDBACK_H
#define MOD_STAT_GROWTH_ESSENCE_FEEDBACK_H

#include "Define.h"
#include "EssenceTierSystem.h"

#include <string_view>

class Player;

enum class EssenceVisual : uint8
{
    Growth,
    Experience,
    Resource,
    Vitality,
    Fortune
};

void PlayEssenceFeedback(
    Player* player, EssenceVisual visual, EssenceTier tier, std::string_view announcement);

#endif
