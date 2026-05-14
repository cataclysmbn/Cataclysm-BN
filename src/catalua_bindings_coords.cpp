#include "catalua_bindings.h"

namespace cata::detail::lua_coords
{

auto reg_lua_point_coord( sol::state &lua ) -> void;
auto reg_lua_tripoint_coord( sol::state &lua ) -> void;
auto reg_raw_point( sol::state &lua ) -> void;
auto reg_raw_tripoint( sol::state &lua ) -> void;

} // namespace cata::detail::lua_coords

auto cata::detail::reg_point_tripoint( sol::state &lua ) -> void
{
    lua_coords::reg_lua_point_coord( lua );
    lua_coords::reg_lua_tripoint_coord( lua );
    lua_coords::reg_raw_point( lua );
    lua_coords::reg_raw_tripoint( lua );
}
