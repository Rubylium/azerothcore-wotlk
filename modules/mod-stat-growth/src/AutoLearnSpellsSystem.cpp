#include "AutoLearnSpellsSystem.h"

#include "ObjectMgr.h"
#include "Player.h"
#include "Trainer.h"

uint32 AutoLearnClassSpells(Player* player)
{
    if (!player)
        return 0;

    uint32 learnedCount = 0;
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
