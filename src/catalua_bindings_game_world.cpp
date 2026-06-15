#include "catalua_bindings_game_internal.h"
#include "catalua_bindings_utils.h"
#include "catalua_coord.h"
#include "catalua_luna_doc.h"

#include <stdexcept>
#include <vector>

#include "game.h"
#include "iexamine.h"
#include "line.h"
#include "overmapbuffer.h"
#include "player.h"

namespace
{

auto direction_from_relative_delta( const cata::detail::lua_coords::lua_tripoint_coord &delta ) ->
direction
{
    if( delta.origin != coords::origin::relative ) {
        throw std::runtime_error( "direction_from expects a relative TripointCoord delta" );
    }
    return direction_from( delta.raw );
}

auto overmap_terrain_cardinal_directions() -> std::vector<tripoint_rel_omt>
{
    return std::vector<tripoint_rel_omt> {
        tripoint_rel_omt::north(), tripoint_rel_omt::south(), tripoint_rel_omt::east(),
        tripoint_rel_omt::west(), tripoint_rel_omt::above(), tripoint_rel_omt::below()
    };
}

} // namespace

namespace cata::detail
{

auto reg_game_api_world_helpers( luna::userlib &lib ) -> void
{
    DOC( "Get the global overmap buffer" );
    luna::set_fx( lib, "get_overmap_buffer", []() -> overmapbuffer & { return get_active_overmapbuffer(); } );
    DOC( "Run a built-in examine action at a position." );
    luna::set_fx( lib, "call_builtin_examine",
    []( const std::string & examine_id, player & who, const tripoint_bub_ms & pos ) -> void {
        iexamine_function_from_string( examine_id )( who, pos );
    } );

    DOC( "Get direction from a relative tripoint coordinate delta." );
    luna::set_fx( lib, "direction_from", &direction_from_relative_delta );

    DOC( "Get direction name from direction enum" );
    luna::set_fx( lib, "direction_name", []( direction dir ) -> std::string { return direction_name( dir ); } );

    DOC( "Get the six cardinal overmap-terrain direction offsets (N, S, E, W, Up, Down)." );
    luna::set_fx( lib, "six_cardinal_directions", &overmap_terrain_cardinal_directions );
}

} // namespace cata::detail
