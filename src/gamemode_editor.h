#pragma once

#include <optional>

#include "gamemode.h"

/// A game that opens the editor before the first turn is played, instead of asking for an input.
struct editor_opening_game : public special_game {
        auto next_action() -> std::optional<action_id> override;

    private:
        bool opened = false;
};

/// The main menu's map editor: a new world of open field at noon, an avatar the creatures ignore
/// with the debug traits the editor needs, and the editor opened before the first turn is played.
struct editor_game : public editor_opening_game {
    auto id() -> special_game_type override {
        return special_game_type::EDITOR;
    }
    auto init() -> bool override;
};

/// A save opened in editor mode from the main menu: the game it was, with the editor allowed for the
/// session, not by a trait a save would keep, and the avatar where the save put it.
struct editor_save_game : public editor_opening_game {
    auto init() -> bool override;
};
