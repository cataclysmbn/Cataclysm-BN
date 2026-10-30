#pragma once

namespace editor {

/// Whether the editors show ids, JSON and the settings only modders who know the game's files
/// need: the player's choice, kept in the options, off at first.
auto advanced_mode() -> bool;

/// Keeps `on` as the player's choice of `advanced_mode` for every editor and the next game.
auto set_advanced_mode(bool on) -> void;

} // namespace editor
