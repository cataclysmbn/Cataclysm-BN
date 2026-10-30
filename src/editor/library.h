#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// A piece the player keeps to paste again in any world or mod, as Factorio's blueprint library.
struct library_piece {
    std::string name;
    /// What the editor that saved it pastes.
    nlohmann::ordered_json data;
};

/// The file in the player's configuration that holds the pieces of `kind`, such as `vehicle`.
auto library_file(const std::string& kind) -> std::string;
/// The pieces of `kind` in the order they were saved; none when the file is missing or unreadable.
auto library_pieces(const std::string& kind) -> std::vector<library_piece>;
/// Keeps `piece` among those of `kind`, in place of one of its name.
auto keep_piece(const std::string& kind, const library_piece& piece) -> void;

/// `data` of `kind` as text to share, which `piece_of_text` reads back.
auto piece_text(const std::string& kind, const nlohmann::ordered_json& data) -> std::string;
/// What `piece_text` made of a piece of `kind`; none for other text.
auto piece_of_text(const std::string& kind, const std::string& text)
    -> std::optional<nlohmann::ordered_json>;

} // namespace editor
