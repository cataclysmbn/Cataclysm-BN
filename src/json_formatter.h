#pragma once

#include <expected>
#include <string>
#include <string_view>

/// JSON text in the data style `style-json` checks, ending in a newline; strings keep their escapes as written.
/// The error names what could not be read.
auto format_json( std::string_view text ) -> std::expected<std::string, std::string>;
