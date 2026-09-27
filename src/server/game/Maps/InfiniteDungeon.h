#ifndef ACORE_INFINITE_DUNGEON_H
#define ACORE_INFINITE_DUNGEON_H

#include "DataMap.h"
#include "Define.h"

// The Infinite Dungeon (mod-stat-growth src/infinite/InfiniteDungeon.cpp): an endless ladder of short floors, each a
// fresh instance of a dungeon map where the run's own creatures are spawned. What the core and the other modules need
// to know of it:
//  - a player in a run carries a Pass: the run sends it from floor to floor itself, so the dungeon's own entry rules
//    (level, keys, attunements, the hourly instance limit) do not apply (MapMgr::PlayerCannotEnter, InstanceMap::Add)
//  - the players and everything of the run live in PhaseMask, so the dungeon's own creatures, which stay where they
//    are, neither see nor meet them. mod-playerbots reads it to keep bots from being summoned into a run.
namespace InfiniteDungeon
{
constexpr uint32 PhaseMask = 0x40000000;

constexpr char const* PassKey = "InfiniteDungeonPass";

struct Pass : public DataMap::Base
{
};

inline bool HasPass(DataMap const& data)
{
    return data.Get<Pass>(PassKey) != nullptr;
}
}

#endif
