#pragma once

#include "editor/document.h"
#include "engine_client_component.h"
#include "type_id.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/// Whether `path` lies inside `root`.
auto inside(const std::filesystem::path& path, const std::string& root) -> bool;
/// `path` from the game's directory, the one that holds `data`, when it lies inside it, as a
/// modder finds it there.
auto shown_path(const std::filesystem::path& path) -> std::string;
/// Whether `path` is a .json file in a folder of the player's mods, the only place the editors
/// make files: a file loose in the mods is no mod's, so no world loads it.
auto is_mod_file(const std::filesystem::path& path) -> bool;
/// The name of the top folder of the player's mods that holds the file at `path`; none outside
/// them or loose in them.
auto mod_folder_name(const std::filesystem::path& path) -> std::optional<std::string>;
/// The mod whose folder in the player's mods holds the file at `path`; none outside every mod.
auto mod_of(const std::filesystem::path& path) -> std::optional<mod_id>;
/// Makes the folder of the player's mods that holds the file at `path` a mod of that folder's
/// name, unless a mod holds it already; the error is what to tell the player.
auto make_mod_of(const std::filesystem::path& path) -> std::expected<void, std::string>;

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
