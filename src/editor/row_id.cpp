#include "editor/row_id.h"

#include <charconv>

namespace editor {
namespace {

/// The whole of `text` read as a number.
template <typename T> auto number(std::string_view text) -> std::optional<T> {
    auto value = T{};
    const auto* const end = text.data() + text.size();
    const auto read = std::from_chars(text.data(), end, value);
    return read.ec == std::errc{} && read.ptr == end && !text.empty()
             ? std::optional(value)
             : std::nullopt;
}

} // namespace

auto point_row(std::string_view kind, const point& at) -> std::string {
    return std::string(kind) + ":" + std::to_string(at.x) + "," + std::to_string(at.y);
}

auto row_point(std::string_view row, std::string_view kind) -> std::optional<point> {
    const auto rest = row_rest(row, kind);
    const auto comma = rest ? rest->find(',') : std::string_view::npos;
    if (comma == std::string_view::npos) { return std::nullopt; }
    const auto x = number<int>(rest->substr(0, comma));
    const auto y = number<int>(rest->substr(comma + 1));
    return x && y ? std::optional(point(*x, *y)) : std::nullopt;
}

auto index_row(std::string_view kind, std::size_t index) -> std::string {
    return std::string(kind) + ":" + std::to_string(index);
}

auto row_index(std::string_view row, std::string_view kind) -> std::optional<std::size_t> {
    return row_rest(row, kind).and_then(number<std::size_t>);
}

auto row_rest(std::string_view row, std::string_view kind) -> std::optional<std::string_view> {
    if (!row.starts_with(kind) || row.substr(kind.size()).substr(0, 1) != ":") {
        return std::nullopt;
    }
    return row.substr(kind.size() + 1);
}

} // namespace editor
