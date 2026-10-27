#include "editor/session_file.h"

#include "path_info.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui.h"

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
    return path.extension() == ".json" && inside(path, PATH_INFO::user_moddir());
}

auto changed_on_disk_notice(const document& doc) -> std::string {
    const auto name = doc.path().string();
    return doc.dirty()
             ? string_format(_("%s changed on disk; saving would overwrite that change."), name)
             : string_format(_("%s changed on disk and was reloaded."), name);
}

auto save_document(document& doc) -> std::expected<void, std::string> { return told(doc.save()); }

auto save_document_as(document& doc, const std::filesystem::path& path)
    -> std::expected<void, std::string> {
    if (!is_mod_file(path)) {
        return std::unexpected(string_format(
            _("Save as writes a .json file in your mods, %s."), PATH_INFO::user_moddir()));
    }
    return told(doc.save_as(path));
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

auto history_affordances(const document& doc, const history_ids& ids)
    -> std::vector<engine_client::affordance> {
    namespace ec = engine_client;
    return {
        ec::invoke_affordance(
            {.id = ids.undo, .label = _("Undo"), .keys = {"CTRL+Z"}, .enabled = doc.can_undo()}),
        ec::invoke_affordance(
            {.id = ids.redo, .label = _("Redo"), .keys = {"CTRL+Y"}, .enabled = doc.can_redo()}),
        ec::invoke_affordance(
            {.id = ids.save, .label = _("Save"), .keys = {"CTRL+S"}, .enabled = doc.dirty()}),
    };
}

} // namespace editor
