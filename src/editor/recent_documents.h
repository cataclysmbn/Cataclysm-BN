#pragma once

#include "type_id.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

/// A document an editor opened, as the main menu offers it again.
struct recent_document {
    /// `map` or `vehicle`.
    std::string editor;
    /// The file the map editor opened, or the prototype the vehicle editor opened.
    std::string name;
    /// The mods the editor had loaded.
    std::vector<mod_id> mods;
    auto operator==(const recent_document&) const -> bool = default; // *NOPAD*
};

/// How many documents the editors remember.
constexpr auto recent_limit = std::size_t{10};

/// The file in the player's configuration that holds the documents the editors opened.
auto recent_documents_file() -> std::string;
/// The documents the editors opened, newest first; none when the file is missing or unreadable.
auto recent_documents() -> std::vector<recent_document>;
/// Puts `opened` first, dropping its older entry, one with the same editor and name, and the
/// entries past `recent_limit`.
auto remember_document(const recent_document& opened) -> void;
/// The mods of the world the game runs, or the core content pack outside one.
auto loaded_mods() -> std::vector<mod_id>;
/// `loaded_mods`, and last the mod whose folder holds the file at `path`, so that opening it
/// again loads the mod.
auto mods_for(const std::filesystem::path& path) -> std::vector<mod_id>;

} // namespace editor
