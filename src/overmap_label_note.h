#pragma once

#include <optional>
#include <string>

namespace overmap_label_note
{
auto make_note( const std::string &label ) -> std::string;
auto extract_label( const std::string &note ) -> std::optional<std::string>;
auto is_label_only( const std::string &note ) -> bool;
} // namespace overmap_label_note
