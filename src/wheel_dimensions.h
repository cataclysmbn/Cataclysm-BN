#pragma once

#include <optional>
#include <string>

class JsonObject;

namespace wheel_dimensions
{

inline constexpr auto legacy_millimeters_per_inch = 25;

auto from_legacy_inches( int legacy_inches ) -> int;
auto read_from_json( const JsonObject &jo, const std::string &member ) -> std::optional<int>;
auto format_for_display( int millimeters ) -> std::string;

} // namespace wheel_dimensions
