#include "editor/session_file.h"

#include "json_formatter.h"
#include "mod_manager.h"
#include "path_info.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui.h"

#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

namespace editor {
namespace {

auto told(const std::expected<void, save_error>& result) -> std::expected<void, std::string> {
    if (result) { return {}; }
    switch (result.error()) {
        case save_error::changed_on_disk:
            return std::unexpected(
                _("The file changed on disk since it was opened; nothing was saved."));
        case save_error::exists:
            return std::unexpected(_("That file already exists; nothing was saved."));
        case save_error::unwritable:
            break;
    }
    return std::unexpected(_("The file could not be written; save it as a file in your mods."));
}

/// The folders of the player's mods from the top one down to the one that holds the file at
/// `path`; none for a file outside them or loose in them.
auto mod_folders(const std::filesystem::path& path) -> std::vector<std::filesystem::path> {
    auto error = std::error_code{};
    const auto mods = std::filesystem::weakly_canonical(PATH_INFO::user_moddir(), error);
    const auto relative =
        std::filesystem::weakly_canonical(path, error).lexically_relative(mods).parent_path();
    if (error || relative.empty() || *relative.begin() == ".." || relative == ".") { return {}; }
    auto folders = std::vector<std::filesystem::path>{};
    auto folder = mods;
    for (const auto& name : relative) {
        folder /= name;
        folders.push_back(folder);
    }
    return folders;
}

} // namespace

auto inside(const std::filesystem::path& path, const std::string& root) -> bool {
    auto error = std::error_code{};
    const auto base = std::filesystem::weakly_canonical(root, error);
    const auto relative = std::filesystem::weakly_canonical(path, error).lexically_relative(base);
    return !error && !relative.empty() && *relative.begin() != "..";
}

auto shown_path(const std::filesystem::path& path) -> std::string {
    const auto game = (std::filesystem::absolute(PATH_INFO::datadir()) / "..").lexically_normal();
    const auto relative = std::filesystem::absolute(path).lexically_normal().lexically_proximate(
        game);
    return (relative.empty() || *relative.begin() == ".." ? path : relative).generic_string();
}

auto is_mod_file(const std::filesystem::path& path) -> bool {
    return path.extension() == ".json" && !mod_folders(path).empty();
}

auto mod_folder_name(const std::filesystem::path& path) -> std::optional<std::string> {
    const auto folders = mod_folders(path);
    if (folders.empty()) { return std::nullopt; }
    return folders.front().filename().string();
}

auto mod_of(const std::filesystem::path& path) -> std::optional<mod_id> {
    for (const auto& folder : mod_folders(path)) {
        auto in = std::ifstream(folder / "modinfo.json", std::ios::binary);
        if (!in) { continue; }
        const auto read = nlohmann::json::parse(in, nullptr, false);
        for (const auto& entry : read.is_array() ? read : nlohmann::json::array({read})) {
            if (entry.is_object() && entry.value("type", "") == "MOD_INFO" && entry.contains("id")
                && entry["id"].is_string()) {
                return mod_id(entry["id"].get<std::string>());
            }
        }
    }
    return std::nullopt;
}

auto make_mod_of(const std::filesystem::path& path) -> std::expected<void, std::string> {
    const auto folders = mod_folders(path);
    if (folders.empty() || std::ranges::any_of(folders, [](const std::filesystem::path& folder) {
            return std::filesystem::exists(folder / "modinfo.json");
        })) {
        return {};
    }
    const auto name = folders.front().filename().string();
    const auto info = nlohmann::ordered_json::array(
        {{{"type", "MOD_INFO"},
          {"id", name},
          {"name", name},
          {"category", "content"},
          {"dependencies", {mod_management::get_default_core_content_pack().str()}}}});
    auto out = std::ofstream(folders.front() / "modinfo.json", std::ios::binary);
    out << format_json(info.dump()).value_or(info.dump(2));
    if (!out.flush()) {
        return std::unexpected(string_format(
            _("The file was saved, but %s could not be written to make its folder a mod."),
            (folders.front() / "modinfo.json").string()));
    }
    return {};
}

auto changed_on_disk_notice(const document& doc) -> std::string {
    const auto name = doc.path().string();
    return doc.dirty()
             ? string_format(_("%s changed on disk; saving would overwrite that change."), name)
             : string_format(_("%s changed on disk and was reloaded."), name);
}

auto save_document(document& doc) -> std::expected<void, std::string> {
    return told(doc.save()).and_then([&] { return make_mod_of(doc.path()); });
}

auto save_document_as(document& doc, const std::filesystem::path& path)
    -> std::expected<void, std::string> {
    if (!is_mod_file(path)) {
        return std::unexpected(string_format(
            _("Save as writes a .json file in your mods, %s, inside a mod's folder such as "
              "my_mod/."),
            PATH_INFO::user_moddir()));
    }
    return told(doc.save_as(path)).and_then([&] { return make_mod_of(path); });
}

auto ask_leave() -> leave_answer {
    auto menu = uilist{};
    menu.text = _("Your changes to this file are not saved.");
    menu.addentry(0, true, 's', _("Save and leave"));
    menu.addentry(1, true, 'd', _("Leave without saving"));
    menu.addentry(2, true, 'c', _("Stay"));
    menu.query();
    switch (menu.ret) {
        case 0:
            return leave_answer::save;
        case 1:
            return leave_answer::discard;
        default:
            return leave_answer::stay;
    }
}

auto undo_affordances(const undo_options& options) -> std::vector<engine_client::affordance> {
    namespace ec = engine_client;
    return {
        ec::invoke_affordance(
            {.id = options.ids.undo,
             .label = _("Undo"),
             .keys = {"CTRL+Z"},
             .enabled = options.can_undo,
             .denial = options.can_undo ? "" : _("Nothing to undo.")}),
        ec::invoke_affordance(
            {.id = options.ids.redo,
             .label = _("Redo"),
             .keys = {"CTRL+Y"},
             .enabled = options.can_redo,
             .denial = options.can_redo ? "" : _("Nothing to redo.")}),
    };
}

auto history_affordances(const document& doc, const history_ids& ids)
    -> std::vector<engine_client::affordance> {
    auto affordances = undo_affordances(
        {.ids = ids, .can_undo = doc.can_undo(), .can_redo = doc.can_redo()});
    affordances.push_back(engine_client::invoke_affordance(
        {.id = ids.save,
         .label = _("Save"),
         .keys = {"CTRL+S"},
         .enabled = doc.dirty(),
         .denial = doc.dirty() ? "" : _("Nothing changed since the file was saved.")}));
    return affordances;
}

} // namespace editor
