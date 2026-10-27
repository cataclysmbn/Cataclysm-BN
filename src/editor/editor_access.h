#pragma once

#include <string>

namespace editor {

/// Why the avatar cannot open the map editor; empty when it can.
auto editor_denial() -> std::string;

/// Lets the avatar open the editor until the next game is set up, without the trait, which a save
/// would keep.
auto allow_editor_this_session(bool allowed) -> void;

/// Whether the game is the main menu's editor game, which is there only for the editors.
auto in_editor_game() -> bool;

/// Ends the editor game, back to the main menu; does nothing in any other game.
auto end_editor_game() -> void;

} // namespace editor
