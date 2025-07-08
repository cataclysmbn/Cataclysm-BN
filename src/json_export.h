#pragma once

#include "json.h"

class vehicle;

namespace json_export
{
auto vehicle( JsonOut &json, const vehicle &v ) -> void;

} // namespace json_export


