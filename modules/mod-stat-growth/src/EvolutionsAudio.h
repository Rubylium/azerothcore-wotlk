#ifndef MOD_STAT_GROWTH_EVOLUTIONS_AUDIO_H
#define MOD_STAT_GROWTH_EVOLUTIONS_AUDIO_H

#include "Define.h"
#include <string_view>

class Player;
class WorldObject;
struct Position;

// Our own sound engine, in the client (the client extension DLL's EvolutionsAudio, fed by FrameXML
// EvolutionsAudio.lua): a sound of its bank played for one player, by its key. The bank is built from
// modules/*/client-assets/audio/*.json by localTools/audio/buildAudio.py; a key's kind (ui, world, loop, music) says
// how it is heard. See .agents/docs/systems/evolutions-audio.md.
namespace EvolutionsAudio
{
// Where the player's camera is: an interface sound (a world one heard as one)
void Play(Player* player, std::string_view key);
// From that object, following it - a loop until stopped or the object leaves the player's sight
void PlayOn(Player* player, std::string_view key, WorldObject const* source);
// From that point
void PlayAt(Player* player, std::string_view key, Position const& where);
// The sounds following that object fade out
void StopOn(Player* player, WorldObject const* source);
// A music (kind music): looping until stopped, the game's own music silent meanwhile; over the one already playing
// (cross-faded), the same one going on. fadeInMs 0: the engine's (1 s)
void PlayMusic(Player* player, std::string_view key, uint32 fadeInMs = 0);
// The music fades out (fadeOutMs 0: the engine's, 2 s), the game's own music back after it
void StopMusic(Player* player, uint32 fadeOutMs = 0);
}

#endif
