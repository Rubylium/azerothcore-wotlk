#ifndef MOD_STAT_GROWTH_EVOLUTIONS_AUDIO_H
#define MOD_STAT_GROWTH_EVOLUTIONS_AUDIO_H

#include <string_view>

class Player;
class WorldObject;
struct Position;

// Our own sound engine, in the client (the client extension DLL's EvolutionsAudio, fed by FrameXML
// EvolutionsAudio.lua): a sound of its bank played for one player, by its key. The bank is built from
// modules/*/client-assets/audio/*.json by localTools/audio/buildAudio.py; a key's kind (ui, world, loop) says how it
// is heard. See .agents/docs/systems/evolutions-audio.md.
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
}

#endif
