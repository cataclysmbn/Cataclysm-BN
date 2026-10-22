#pragma once

#include <optional>

#include "gamemode.h"

/// The main menu's map editor: a new world of open field at noon, an avatar the creatures ignore
/// with the debug traits the editor needs, and the editor opened before the first turn is played.
struct editor_game : public special_game {
        auto id() -> special_game_type override {
            return special_game_type::EDITOR;
        }
        auto init() -> bool override;
        auto next_action() -> std::optional<action_id> override;

    private:
        bool opened = false;
};
