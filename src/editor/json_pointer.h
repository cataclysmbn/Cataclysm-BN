#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace editor {

/// The RFC 6901 pointer of the innermost value of the JSON `text` whose bytes hold `offset`; "" for
/// the whole text, or when `text` cannot be read up to `offset`.
auto pointer_at(std::string_view text, std::size_t offset) -> std::string;

} // namespace editor
