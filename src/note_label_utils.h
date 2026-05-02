#pragma once

#include <string>

/// Utilities related to parsing and sanitizing overmap note labels.
namespace note_label_utils
{
/// Removes LABEL commands and surrounding separators from note text that is shown to the player.
auto strip_label_commands( std::string note_text ) -> std::string;
} // namespace note_label_utils
