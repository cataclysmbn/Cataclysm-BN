#pragma once

#include <string>

class Character;

/// Player-authored text takes precedence over an NPC template or class biography.
auto get_character_description( const Character &who ) -> std::string;
/// Store player-authored text in the character's persistent values.
auto set_character_description( Character &who, const std::string &description ) -> void;
/// Open the multiline biography editor; canceling preserves the existing text.
auto edit_character_description( Character &who ) -> void;
