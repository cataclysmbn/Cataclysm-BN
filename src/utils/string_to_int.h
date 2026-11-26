#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <system_error>

namespace string_utils {

inline auto string_to_int(const std::string& value) -> std::optional<int> {
    if (value.empty()) { return std::nullopt; }

    auto parsed = 0;
    const auto* first = value.data();
    const auto* last = first + value.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last) { return std::nullopt; }
    return parsed;
}

} // namespace string_utils
