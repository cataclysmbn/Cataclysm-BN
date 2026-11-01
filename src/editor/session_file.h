#pragma once

#include "editor/document.h"
#include "engine_client_component.h"
#include "type_id.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

/// Whether `path` lies inside `root`.
auto inside(const std::filesystem::path& path, const std::string& root) -> bool;
/// `path` from the game's directory, the one that holds `data`, when it lies inside it, as a
/// modder finds it there.
auto shown_path(const std::filesystem::path& path) -> std::string;
/// The file at `path` as the player names it: from the player's mods when it is in them.
auto mods_path(const std::filesystem::path& path) -> std::string;
/// Whether `path` is a .json file in a folder of the player's mods, the only place the editors
/// make files: a file loose in the mods is no mod's, so no world loads it.
auto is_mod_file(const std::filesystem::path& path) -> bool;
/// The name of the top folder of the player's mods that holds the file at `path`; none outside
/// them or loose in them.
auto mod_folder_name(const std::filesystem::path& path) -> std::optional<std::string>;
/// The mod whose folder in the player's mods holds the file at `path`; none outside every mod.
auto mod_of(const std::filesystem::path& path) -> std::optional<mod_id>;
/// Whether the file at `path` may make its folder of the player's mods a mod: refused, with what to
/// tell the player, when that folder's name is no mod id or another mod's, or a mod lies below it.
auto check_mod_folder(const std::filesystem::path& path) -> std::expected<void, std::string>;
/// Makes the folder of the player's mods that holds the file at `path` a mod whose id is that
/// folder's name and whose name is `name`, else the id, unless a mod holds it already; the error
/// is what to tell the player.
auto make_mod_of(const std::filesystem::path& path, const std::string& name = {})
    -> std::expected<void, std::string>;
/// `words` as an id: its letters and digits in lower case, the rest as single underscores.
auto id_of_words(std::string_view words) -> std::string;
/// `id` as words: underscores as spaces, the first letter upper case.
auto words_of_id(std::string_view id) -> std::string;
/// The text at `key` of the JSON `value`, else empty: a file may hold any value there.
template <typename Json> auto text_at(const Json& value, const std::string& key) -> std::string {
    return value.is_object() && value.contains(key) && value[key].is_string()
             ? value[key].template get<std::string>()
             : std::string{};
}
/// What the editors call the file at `path`: the name of its first entry and of its mod for a file
/// of the player's mods, else its path.
auto document_title(const std::filesystem::path& path) -> std::string;
/// A mod at the top of the player's mods: its folder and its name.
struct player_mod {
    std::string folder;
    std::string name;
};
/// The mods at the top of the player's mods, by name.
auto player_mods() -> std::vector<player_mod>;
/// `base`, else `base_2`, `base_3` and on: the first name not `taken`.
auto free_name(const std::string& base, const std::function<bool(const std::string&)>& taken)
    -> std::string;

/// Writes `doc` and makes the folder of the player's mods it is in a mod; the error is what to
/// tell the player.
auto save_document(document& doc) -> std::expected<void, std::string>;
/// Writes `doc` to a new file in the player's mods, which it is from then on.
auto save_document_as(document& doc, const std::filesystem::path& path)
    -> std::expected<void, std::string>;

/// What to say of `doc`'s file changed on disk: kept with its edit, or read again.
auto changed_on_disk_notice(const document& doc) -> std::string;

/// What the player answers to leaving a file with unsaved changes.
enum class leave_answer { save, discard, stay };
auto ask_leave() -> leave_answer;

/// The ids of an editor's Undo, Redo and Save.
struct history_ids {
    std::string undo;
    std::string redo;
    std::string save;
};
/// Undo and Redo with their keys, each enabled when it would do something and else saying why not.
struct undo_options {
    history_ids ids;
    bool can_undo = false;
    bool can_redo = false;
};
auto undo_affordances(const undo_options& options) -> std::vector<engine_client::affordance>;
/// Undo, Redo and Save of `doc` with their keys, each enabled when it would do something and else
/// saying why not.
auto history_affordances(const document& doc, const history_ids& ids)
    -> std::vector<engine_client::affordance>;

} // namespace editor
