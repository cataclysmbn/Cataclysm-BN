#pragma once

#include <string>

namespace editor {

/// Why the avatar cannot open the map editor; empty when it can.
auto editor_denial() -> std::string;

/// Lets the avatar open the editor until the next game is set up, without the trait, which a save
/// would keep.
auto allow_editor_this_session(bool allowed) -> void;

} // namespace editor
