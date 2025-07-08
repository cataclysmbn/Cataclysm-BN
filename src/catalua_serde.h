#pragma once

#include "catalua_sol_fwd.h"

class JsonObject;
class JsonOut;

namespace cata
{

void serialize_lua_table( const sol::table &t, JsonOut &jsout );
void deserialize_lua_table( sol::table t, JsonObject &obj );

} // namespace cata


