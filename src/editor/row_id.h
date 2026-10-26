#pragma once

#include "point.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace editor {

/// The id of the row of `kind` at `at`, e.g. "square:3,4".
auto point_row(std::string_view kind, const point& at) -> std::string;
/// The point a row id of `kind` names.
auto row_point(std::string_view row, std::string_view kind) -> std::optional<point>;
/// The id of the row of `kind` at `index`, e.g. "part:2".
auto index_row(std::string_view kind, std::size_t index) -> std::string;
/// The index a row id of `kind` names.
auto row_index(std::string_view row, std::string_view kind) -> std::optional<std::size_t>;
/// What a row id of `kind` names after its "kind:".
auto row_rest(std::string_view row, std::string_view kind) -> std::optional<std::string_view>;

} // namespace editor
