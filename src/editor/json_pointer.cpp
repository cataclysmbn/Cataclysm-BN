#include "editor/json_pointer.h"

#include "json.h"

#include <sstream>

namespace editor {
namespace {

auto escaped(const std::string& token) -> std::string {
    auto out = std::string{};
    for (const auto c : token) {
        if (c == '~') {
            out += "~0";
        } else if (c == '/') {
            out += "~1";
        } else {
            out += c;
        }
    }
    return out;
}

/// Reads the value at the stream's position; `found` becomes the pointer of the innermost value
/// holding `offset`.
auto visit(JsonIn& jsin, const std::string& pointer, const std::size_t offset, std::string& found)
    -> void {
    jsin.eat_whitespace();
    const auto begin = static_cast<std::size_t>(jsin.tell());
    if (offset < begin) {
        jsin.skip_value();
        return;
    }
    if (jsin.test_object()) {
        found = pointer;
        jsin.start_object();
        while (!jsin.end_object()) {
            jsin.eat_whitespace();
            const auto key = static_cast<std::size_t>(jsin.tell());
            const auto member = pointer + "/" + escaped(jsin.get_member_name());
            // A loader names a member by its key, which is not a value.
            if (key <= offset) { found = member; }
            visit(jsin, member, offset, found);
        }
    } else if (jsin.test_array()) {
        found = pointer;
        jsin.start_array();
        for (auto index = 0; !jsin.end_array(); ++index) {
            visit(jsin, pointer + "/" + std::to_string(index), offset, found);
        }
    } else {
        jsin.skip_value();
        if (offset < static_cast<std::size_t>(jsin.tell())) { found = pointer; }
    }
}

} // namespace

auto pointer_at(const std::string_view text, const std::size_t offset) -> std::string {
    auto in = std::istringstream(std::string(text));
    auto found = std::string{};
    try {
        auto jsin = JsonIn(in);
        visit(jsin, "", offset, found);
    } catch (const JsonError&) { return found; }
    return found;
}

} // namespace editor
