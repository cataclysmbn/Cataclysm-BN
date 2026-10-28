#include "editor/history_label.h"

#include "editor/row_id.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <ranges>
#include <vector>

namespace editor {

auto step_label(const document_edit& edit) -> std::string {
    auto pointer = nlohmann::ordered_json::json_pointer(edit.patch[0].value("path", ""));
    auto tokens = std::vector<std::string>{};
    for (; !pointer.empty(); pointer.pop_back()) { tokens.insert(tokens.begin(), pointer.back()); }
    // A mapgen's members sit under its `object`.
    auto member = nlohmann::ordered_json::json_pointer{};
    if (tokens.size() > 1 && tokens.front() == "object") {
        member /= tokens.front();
        tokens.erase(tokens.begin());
    }
    const auto index = [](const std::string& token) {
        return token == "-" || token.find_first_not_of("0123456789") == std::string::npos;
    };
    auto words = tokens.empty() ? std::string{} : tokens.front();
    std::ranges::replace(words, '_', ' ');
    if (!tokens.empty()) { member /= tokens.front(); }
    if (tokens.size() > 1 && !index(tokens[1])) {
        words += " " + tokens[1];
        member /= tokens[1];
    }
    const auto had = edit.before.is_object() && edit.before.contains(member);
    const auto has = edit.after.is_object() && edit.after.contains(member);
    // An entry of a list is added or removed by the patch's own verb.
    const auto op = edit.patch[0].value("op", "");
    const auto entry = tokens.size() == 2 && index(tokens[1]) && op != "replace";
    const auto verb =
        entry  ? (op == "add" ? _("Add") : _("Remove"))
        : !had ? _("Add")
        : !has ? _("Remove")
               : _("Change");
    return words.empty() ? verb : string_format(pgettext("editor history", "%s %s"), verb, words);
}

auto document_steps(
    const document& doc, const std::function<auto(const document_edit&)->std::string>& label)
    -> history_steps {
    return {
        .labels =
            doc.history() | std::views::transform([&](const document_step& step) {
                return label(step.edits.back());
            })
            | std::ranges::to<std::vector>(),
        .depth = doc.depth(),
        .saved = doc.saved_depth()};
}

auto step_rows(const history_steps& steps) -> std::vector<engine_client::row> {
    return std::views::iota(std::size_t{0}, steps.labels.size() + 1)
         | std::views::transform([&](const std::size_t depth) {
               return engine_client::row{
                   .id = index_row("step", depth),
                   .label = depth == 0 ? std::string(_("Opened")) : steps.labels[depth - 1],
                   .description = steps.saved == depth ? std::string(_("Saved")) : std::string{},
                   .selected = depth == steps.depth};
           })
         | std::ranges::to<std::vector>();
}

} // namespace editor
