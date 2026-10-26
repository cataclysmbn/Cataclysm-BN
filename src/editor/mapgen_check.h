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

/// Loads one mapgen entry (`"type": "mapgen"`) from `text` with the game's own loader, finalizes
/// and checks it as the game does after loading data, and registers nothing; what the loader
/// reported, in order. A palette entry is loaded and added with every palette it names, as a
/// mapgen using it would, so that a loop through it is reported.
auto check_mapgen(std::string_view text) -> std::vector<loader_message>;

/// While it lives, each palette entry of `texts` stands in for the game's palette of its id, as if
/// the game had loaded it, so that a mapgen reads the palettes of its own file as they are edited.
/// What loading them said is dropped: checking a palette reports it.
class palette_stand_ins {
public:
    explicit palette_stand_ins(const std::vector<std::string>& texts);
    ~palette_stand_ins() { restore_(); }
    palette_stand_ins(const palette_stand_ins&) = delete;
    auto operator=(const palette_stand_ins&) -> palette_stand_ins& = delete;

private:
    std::function<auto()->void> restore_;
};

} // namespace editor
