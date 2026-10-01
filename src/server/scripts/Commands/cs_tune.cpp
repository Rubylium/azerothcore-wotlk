/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// .tune: live tuning (src/server/game/Tuning/LiveTuning.h) - the code's knobs and spell multipliers changed while the
// server runs, and the hooks that apply the spell multipliers
//
//  .tune list [filter]            knobs whose key holds filter (all without one), then the spell multipliers
//  .tune get <key>
//  .tune set <key> <value>        at once, saved; the default value drops the override
//  .tune reset <key> | all        back to the code's value (all: every knob and spell multiplier)
//  .tune spell <spell id> [x]     a spell's damage and healing multiplier (1 drops it); without x, shows it
//  .tune reload                   the overrides again from the world database

#include "Chat.h"
#include "CommandScript.h"
#include "LiveTuning.h"
#include "Log.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "UnitScript.h"
#include "WorldScript.h"

using namespace Acore::ChatCommands;

namespace
{
// At most this many knobs a list: the rest is a filter away
constexpr std::size_t ListLimit = 80;

std::string SpellName(uint32 spellId)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    return spellInfo && spellInfo->SpellName[0] ? spellInfo->SpellName[0] : "?";
}

void ShowKnob(ChatHandler* handler, LiveTuning::KnobBase const* knob)
{
    if (knob->IsOverridden())
        handler->PSendSysMessage("{} = {} (code: {})", knob->Key(), knob->Value(), knob->Default());
    else
        handler->PSendSysMessage("{} = {}", knob->Key(), knob->Value());
}
}

class tune_commandscript : public CommandScript
{
public:
    tune_commandscript() : CommandScript("tune_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable tuneTable =
        {
            { "list",   HandleList,   SEC_GAMEMASTER, Console::Yes },
            { "get",    HandleGet,    SEC_GAMEMASTER, Console::Yes },
            { "set",    HandleSet,    SEC_GAMEMASTER, Console::Yes },
            { "reset",  HandleReset,  SEC_GAMEMASTER, Console::Yes },
            { "spell",  HandleSpell,  SEC_GAMEMASTER, Console::Yes },
            { "reload", HandleReload, SEC_GAMEMASTER, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "tune", tuneTable },
        };
        return commandTable;
    }

    static bool HandleList(ChatHandler* handler, Optional<std::string> filter)
    {
        std::size_t shown = 0;
        std::size_t matching = 0;
        for (LiveTuning::KnobBase const* knob : LiveTuning::Knobs())
        {
            if (filter && std::string_view(knob->Key()).find(*filter) == std::string_view::npos)
                continue;
            if (++matching <= ListLimit)
            {
                ShowKnob(handler, knob);
                ++shown;
            }
        }
        if (matching > shown)
            handler->PSendSysMessage("... and {} more: narrow the filter", matching - shown);
        if (!matching)
            handler->PSendSysMessage("No knob{}.", filter ? " matches " + *filter : "");

        for (auto const& [spellId, multiplier] : LiveTuning::SpellMultipliers())
            handler->PSendSysMessage("spell {} ({}) x{}", spellId, SpellName(spellId), multiplier);
        return true;
    }

    static bool HandleGet(ChatHandler* handler, std::string key)
    {
        LiveTuning::KnobBase const* knob = LiveTuning::FindKnob(key);
        if (!knob)
        {
            handler->PSendSysMessage("No knob {} (.tune list <filter>).", key);
            return false;
        }
        ShowKnob(handler, knob);
        return true;
    }

    static bool HandleSet(ChatHandler* handler, std::string key, double value)
    {
        if (!LiveTuning::SetOverride(key, value))
        {
            handler->PSendSysMessage("No knob {} (.tune list <filter>).", key);
            return false;
        }
        ShowKnob(handler, LiveTuning::FindKnob(key));
        LOG_INFO("server.tuning", "Live tuning: {} set {} to {}", handler->GetNameLink(), key, value);
        return true;
    }

    static bool HandleReset(ChatHandler* handler, std::string key)
    {
        if (key == "all")
        {
            for (LiveTuning::KnobBase const* knob : LiveTuning::Knobs())
                if (knob->IsOverridden())
                    LiveTuning::ResetOverride(knob->Key());
            for (auto const& [spellId, multiplier] : LiveTuning::SpellMultipliers())
                LiveTuning::SetSpellMultiplier(spellId, 1.0f);
            handler->PSendSysMessage("Every knob and spell back to the code's values.");
            return true;
        }
        if (!LiveTuning::ResetOverride(key))
        {
            handler->PSendSysMessage("No knob {} (.tune list <filter>).", key);
            return false;
        }
        ShowKnob(handler, LiveTuning::FindKnob(key));
        return true;
    }

    static bool HandleSpell(ChatHandler* handler, uint32 spellId, Optional<float> multiplier)
    {
        if (!sSpellMgr->GetSpellInfo(spellId))
        {
            handler->PSendSysMessage("No spell {}.", spellId);
            return false;
        }
        if (multiplier)
        {
            if (*multiplier < 0.0f)
            {
                handler->PSendSysMessage("A multiplier is 0 or more.");
                return false;
            }
            LiveTuning::SetSpellMultiplier(spellId, *multiplier);
            LOG_INFO("server.tuning", "Live tuning: {} set spell {} to x{}", handler->GetNameLink(), spellId,
                     *multiplier);
        }
        handler->PSendSysMessage("spell {} ({}) x{}", spellId, SpellName(spellId),
                                 LiveTuning::SpellMultiplier(spellId));
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        auto const [knobs, spells] = LiveTuning::Load();
        handler->PSendSysMessage("Live tuning reloaded: {} knob overrides, {} spell multipliers.", knobs, spells);
        return true;
    }
};

// The spell multipliers, on every damage and heal a spell does: direct (after its caster's bonuses), over time, a
// pet's or a totem's (their own spells)
class live_tuning_unitscript : public UnitScript
{
public:
    live_tuning_unitscript() : UnitScript("live_tuning_unitscript", true, { UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
        UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK, UNITHOOK_MODIFY_HEAL_RECEIVED }) { }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* /*attacker*/, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!spellInfo)
            return;
        if (float const multiplier = LiveTuning::SpellMultiplier(spellInfo->Id); multiplier != 1.0f)
            damage = int32(float(damage) * multiplier);
    }

    void ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* /*attacker*/, uint32& damage,
                                       SpellInfo const* spellInfo) override
    {
        if (!spellInfo)
            return;
        float const multiplier = LiveTuning::SpellMultiplier(spellInfo->Id);
        // A heal over time comes through here too, then through ModifyHealReceived: scaled there, once
        if (multiplier == 1.0f || (spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL) &&
            !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE) && !spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH)))
            return;
        damage = uint32(float(damage) * multiplier);
    }

    void ModifyHealReceived(Unit* /*target*/, Unit* /*healer*/, uint32& heal, SpellInfo const* spellInfo) override
    {
        if (!spellInfo)
            return;
        if (float const multiplier = LiveTuning::SpellMultiplier(spellInfo->Id); multiplier != 1.0f)
            heal = uint32(float(heal) * multiplier);
    }
};

class live_tuning_worldscript : public WorldScript
{
public:
    live_tuning_worldscript() : WorldScript("live_tuning_worldscript", { WORLDHOOK_ON_STARTUP }) { }

    void OnStartup() override
    {
        auto const [knobs, spells] = LiveTuning::Load();
        LOG_INFO("server.loading", ">> Live tuning: {} knobs, {} overridden, {} spell multipliers",
                 LiveTuning::Knobs().size(), knobs, spells);
    }
};

void AddSC_tune_commandscript()
{
    new tune_commandscript();
    new live_tuning_unitscript();
    new live_tuning_worldscript();
}
