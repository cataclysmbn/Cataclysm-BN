#pragma once

#include <optional>
#include <string>

#include "type_id.h"

namespace overmap_labels
{
auto set_label( const oter_type_str_id &id, const std::optional<std::string> &label ) -> void;
auto get_label( const oter_type_str_id &id ) -> std::optional<std::string>;
auto reset() -> void;
} // namespace overmap_labels
