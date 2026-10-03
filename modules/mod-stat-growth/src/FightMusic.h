#ifndef MOD_STAT_GROWTH_FIGHT_MUSIC_H
#define MOD_STAT_GROWTH_FIGHT_MUSIC_H

#include "ObjectGuid.h"

#include <mutex>
#include <unordered_map>

// The fight music each player was last sent: a fight's boss sends its track on the pull (L'Infini, the Hollow Voice)
// and ends it with a silence, wherever its players are by then (home already after a wipe, the track still playing).
// A fight ending silences only the players whose music is still its own: one who went on to another fight keeps that
// fight's track. Two fights overlapped once - a player left one, pulled the next, and the first one's wipe silenced the
// new track 30 s in.
namespace FightMusic
{
inline std::mutex Lock;
inline std::unordered_map<ObjectGuid, ObjectGuid> Owners;      // player -> the boss whose track they hear

// fight sent its track to player
inline void Claim(ObjectGuid player, ObjectGuid fight)
{
    std::lock_guard<std::mutex> guard(Lock);
    Owners[player] = fight;
}

// fight ends its track: whether player still hears it (and so is to be sent its silence); it is forgotten either way
inline bool Release(ObjectGuid player, ObjectGuid fight)
{
    std::lock_guard<std::mutex> guard(Lock);
    auto const found = Owners.find(player);
    if (found == Owners.end() || found->second != fight)
        return false;
    Owners.erase(found);
    return true;
}
}

#endif
