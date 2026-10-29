#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/// What the game's loader said about a mapgen entry.
struct loader_message {
    std::string text;
    /// The byte of the loaded text it names, when it names one.
    std::optional<std::size_t> offset = std::nullopt;
};

/// What the game's loader said as a sentence for a player: without the "Json error:
/// <source>:<line>: <column>: " that names its input, the text it quotes after, or the object a
/// missing field is missing from; a line it names leads the sentence.
auto plain_loader_text(std::string_view message) -> std::string;

/// Loads one mapgen entry (`"type": "mapgen"`) from `text` with the game's own loader, finalizes
/// and checks it as the game does after loading data, and registers nothing; what the loader
/// reported, in order. A palette entry is loaded and added with every palette it names, as a
/// mapgen using it would, so that a loop through it is reported.
auto check_mapgen(std::string_view text) -> std::vector<loader_message>;

/// Whether an overmap terrain, overmap special or building the game loaded places the overmap
/// terrain `name`, so that the game generates its mapgen in play.
auto terrain_placed(const std::string& name) -> bool;

/// While it lives, each palette and nested chunk entry of `texts` stands in for the game's ones of
/// its id, as if the game had loaded it, so that a mapgen reads the palettes and places the chunks
/// of its own file as they are edited. What loading them said is dropped: checking one reports it.
class file_stand_ins {
public:
    explicit file_stand_ins(const std::vector<std::string>& texts);
    ~file_stand_ins() {
        restore_palettes_();
        restore_chunks_();
    }
    file_stand_ins(const file_stand_ins&) = delete;
    auto operator=(const file_stand_ins&) -> file_stand_ins& = delete;
    /// Leaves the chunks as the game's own when it ends, for a mapgen applied with them; the
    /// mapgen read the palettes when it was loaded.
    auto keep_chunks() -> void {
        restore_chunks_ = [] {};
    }

private:
    std::function<auto()->void> restore_palettes_;
    std::function<auto()->void> restore_chunks_;
};

} // namespace editor
