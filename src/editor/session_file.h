#pragma once

#include "editor/document.h"
#include "engine_client_component.h"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

/// Whether `path` lies inside `root`.
auto inside(const std::filesystem::path& path, const std::string& root) -> bool;
/// Whether `path` is a .json file in the player's mods, the only place the editors make files.
auto is_mod_file(const std::filesystem::path& path) -> bool;

/// Writes `doc`; the error is what to tell the player.
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
/// Undo, Redo and Save of `doc` with their keys, each enabled when it would do something.
auto history_affordances(const document& doc, const history_ids& ids)
    -> std::vector<engine_client::affordance>;

} // namespace editor
