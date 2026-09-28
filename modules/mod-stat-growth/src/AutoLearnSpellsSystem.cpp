#include "AutoLearnSpellsSystem.h"

#include "ObjectMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Trainer.h"

#include <array>

namespace
{
// What no trainer teaches: the hunter's pet spells come from the level 10 quests (Taming the Beast), so they are
// given with the rest from that level on
constexpr uint8 HunterPetLevel = 10;
constexpr std::array<uint32, 6> HunterPetSpells = {
    1515,   // Tame Beast
    883,    // Call Pet
    2641,   // Dismiss Pet
    982,    // Revive Pet
    6991,   // Feed Pet
    5149,   // Beast Training
};

uint32 LearnHunterPetSpells(Player* player)
{
    if (player->getClass() != CLASS_HUNTER || player->GetLevel() < HunterPetLevel)
        return 0;

    uint32 learned = 0;
    for (uint32 spellId : HunterPetSpells)
        if (sSpellMgr->GetSpellInfo(spellId) && !player->HasSpell(spellId))
        {
            player->learnSpell(spellId, false);
            ++learned;
        }
    return learned;
}
}

uint32 AutoLearnClassSpells(Player* player)
{
    if (!player)
        return 0;

    uint32 learnedCount = LearnHunterPetSpells(player);
    bool learnedSpell;
    std::vector<Trainer::Trainer const*> const& trainers = sObjectMgr->GetClassTrainers(player->getClass());

    do
    {
        learnedSpell = false;
        for (Trainer::Trainer const* trainer : trainers)
        {
            if (!trainer->IsTrainerValidForPlayer(player))
                continue;

            for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
            {
                if (!trainer->CanTeachSpell(player, &trainerSpell))
                    continue;

                if (trainerSpell.IsCastable())
                    player->CastSpell(player, trainerSpell.SpellId, true);
                else
                    player->learnSpell(trainerSpell.SpellId, false);

                if (!trainer->CanTeachSpell(player, &trainerSpell))
                {
                    ++learnedCount;
                    learnedSpell = true;
                }
            }
        }
    } while (learnedSpell);

    return learnedCount;
}
