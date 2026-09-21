#include "RidingInstructor.h"

#include "Chat.h"
#include "Creature.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <array>
#include <string>

// The riding instructor in Stormwind's Trade District (creature 900101): anyone, of any race, class or reputation,
// learns riding from them for the stock trainers' price and level, and gets a mount of that speed with it.
namespace
{
constexpr uint32 SKILL_RIDING = 762;

struct RidingTier
{
    uint32 spell;
    uint32 level;
    uint32 cost;                    // copper
    uint32 requiredSkill;           // riding skill needed first, 0 for none
    uint32 allianceMount;           // mount spell given with it, 0 for none
    uint32 hordeMount;
    char const* nameEn;
    char const* nameFr;
};

// Costs and levels of the stock riding trainers (npc_trainer)
constexpr std::array<RidingTier, 5> Tiers = { {
    { 33388, 20, 4 * GOLD, 0, 458, 580, "Apprentice Riding (60% ground mount)",
      "Monte d'apprenti (monture 60 %)" },
    { 33391, 40, 50 * GOLD, 75, 23229, 23250, "Journeyman Riding (100% ground mount)",
      "Monte de compagnon (monture 100 %)" },
    { 34090, 60, 250 * GOLD, 150, 32235, 32243, "Expert Riding (flying mount)", "Monte d'expert (monture volante)" },
    { 34091, 70, 5000 * GOLD, 225, 32242, 32246, "Artisan Riding (fast flying mount)",
      "Monte d'artisan (monture volante rapide)" },
    { 54197, 77, 1000 * GOLD, 225, 0, 0, "Cold Weather Flying (Northrend)", "Vol par temps froid (Norfendre)" }
} };

bool IsFrench(Player* player)
{
    return player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_frFR;
}

std::string FormatGold(uint32 copper)
{
    return Acore::StringFormat("{}", copper / GOLD);
}

void ShowTiers(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    bool const french = IsFrench(player);
    bool offered = false;

    for (uint32 index = 0; index < Tiers.size(); ++index)
    {
        RidingTier const& tier = Tiers[index];
        if (player->HasSpell(tier.spell) || player->GetSkillValue(SKILL_RIDING) < tier.requiredSkill)
            continue;

        offered = true;
        std::string text = french ? tier.nameFr : tier.nameEn;
        if (player->GetLevel() < tier.level)
        {
            text += french ? Acore::StringFormat(" - niveau {} requis", tier.level)
                           : Acore::StringFormat(" - requires level {}", tier.level);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, text, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + index);
            continue;
        }

        text += Acore::StringFormat(" - {} {}", FormatGold(tier.cost), french ? "po" : "g");
        std::string const confirm = french ? Acore::StringFormat("Apprendre {} ?", tier.nameFr)
                                           : Acore::StringFormat("Learn {}?", tier.nameEn);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, text, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + index,
                         confirm, tier.cost, false);
    }

    if (!offered)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                         french ? "Vous savez déjà tout ce que je peux enseigner."
                                : "You already know all I can teach.",
                         GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + Tiers.size());

    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
}

void Teach(Player* player, RidingTier const& tier)
{
    bool const french = IsFrench(player);
    ChatHandler chat(player->GetSession());

    if (player->GetLevel() < tier.level)
    {
        chat.SendSysMessage(french ? Acore::StringFormat("Il faut être niveau {}.", tier.level)
                                   : Acore::StringFormat("You must be level {}.", tier.level));
        return;
    }
    if (!player->HasEnoughMoney(tier.cost))
    {
        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, nullptr, 0, 0);
        return;
    }

    player->ModifyMoney(-int32(tier.cost));
    player->learnSpell(tier.spell);

    uint32 const mount = player->GetTeamId() == TEAM_HORDE ? tier.hordeMount : tier.allianceMount;
    if (mount && !player->HasSpell(mount))
        player->learnSpell(mount);

    chat.SendSysMessage(french ? Acore::StringFormat("|cff00ff00Vous avez appris : {}.|r", tier.nameFr)
                               : Acore::StringFormat("|cff00ff00You learned {}.|r", tier.nameEn));
}

class npc_stat_growth_riding_instructor : public CreatureScript
{
public:
    npc_stat_growth_riding_instructor() : CreatureScript("npc_stat_growth_riding_instructor") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowTiers(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        uint32 const index = action - GOSSIP_ACTION_INFO_DEF;
        if (index < Tiers.size())
            Teach(player, Tiers[index]);

        if (index < Tiers.size() && player->HasSpell(Tiers[index].spell))
            CloseGossipMenuFor(player);
        else
            ShowTiers(player, creature);
        return true;
    }
};
}

void AddRidingInstructorScripts()
{
    new npc_stat_growth_riding_instructor();
}
