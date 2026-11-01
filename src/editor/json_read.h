#pragma once

#include <string>

namespace editor {

/// The text at `key` of the JSON `value`, else empty: a file may hold any value there.
template <typename Json> auto text_at(const Json& value, const std::string& key) -> std::string {
    return value.is_object() && value.contains(key) && value[key].is_string()
             ? value[key].template get<std::string>()
             : std::string{};
}

} // namespace editor
