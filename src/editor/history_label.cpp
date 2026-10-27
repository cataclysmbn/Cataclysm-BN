#include "editor/history_label.h"

#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
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
    const auto verb = !had ? _("Add") : !has ? _("Remove") : _("Change");
    return words.empty() ? verb : string_format(pgettext("editor history", "%s %s"), verb, words);
}

} // namespace editor
