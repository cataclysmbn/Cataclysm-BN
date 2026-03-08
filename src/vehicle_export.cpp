#include <algorithm>
#include "json.h"
#include "string_id.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "json_export.h"
#include "vpart_position.h"
#include "vpart_range.h"

namespace
{

using refs = std::vector<vpart_reference>;

auto grouped_refs( const class vehicle &v ) ->  std::vector<refs>
{
    std::vector<refs> part_group;
    for( const auto &p : v.get_all_parts() ) {
        const auto &last = part_group.rbegin();
        if( last != part_group.rend() && p.mount() == last->begin()->mount() ) {
            last->emplace_back( p );
        } else {
            part_group.emplace_back( refs{ p } );
        }
    }
    return part_group;
}

auto is_plain_id( const vpart_reference &vpr ) -> bool
{
    const auto &p = vpr.part();
    return !p.is_turret() && ( !p.is_tank() || p.ammo_current().is_null() );
}

auto json_part_write( JsonOut &json, const vpart_reference &vpr ) -> void
{
    const auto &p = vpr.part();
    const auto &id  = p.info().get_id();
    const auto &ammo_type = p.ammo_current();

    if( id == vpart_id( "door_lock" ) ) {
        return;
    }
    json.member( "part", id );
    if( p.is_tank() ) {
        json.member( "fuel", ammo_type );
    } else if( p.is_turret() ) {
        json.member( "ammo", 50 );
        json.member( "ammo_types", std::array{ ammo_type } );
        json.member( "ammo_qty", std::array{ 0, p.ammo_capacity() } );
    }
}

auto json_parts_write( JsonOut &json, const refs &parts ) -> void
{
    const auto &first = parts.front();

    if( parts.size() == 1 ) {
        json_part_write( json, first );
    } else {
        json.member( "parts" );
        json.start_array();
        for( const auto &p : parts ) {
            if( is_plain_id( p ) ) {
                json.write( p.info().get_id() );
            } else {
                json.start_object();
                json_part_write( json, p );
                json.end_object();
            }
        }
        json.end_array();
    }
}

} // namespace

auto json_export::vehicle( JsonOut &json, const class vehicle &v ) -> void
{
    const auto part_group = grouped_refs( v );

    json.start_object();
    json.member( "id", v.type );
    json.member( "type", "vehicle" );
    json.member( "name", v.name );
    // TODO: calculate blueprints from parts
    // json.member( "blueprint", "wip" );

    json.member( "parts" );
    json.start_array();
    for( const auto &parts : part_group ) {
        const auto [x, y] = parts.begin()->mount();

        json.start_object();
        json.member( "x",  x );
        json.member( "y", y );
        json_parts_write( json, parts );
        json.end_object();
    }
    json.end_array();
    json.end_object();
}
