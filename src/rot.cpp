#include "rot.h"

#include "item.h"
#include "map/map.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

namespace rot::temp
{

auto for_tile( const tile_flags &flags ) -> temperature_flag
{
    if( flags.root_cellar ) {
        return temperature_flag::TEMP_ROOT_CELLAR;
    }
    if( flags.freezer ) {
        return temperature_flag::TEMP_FREEZER;
    }
    if( flags.fridge ) {
        return temperature_flag::TEMP_FRIDGE;
    }

    return temperature_flag::TEMP_NORMAL;
}

auto for_location( const map &m, const item &loc ) -> temperature_flag
{
    if( !loc.has_position() ) {
        return temperature_flag::TEMP_NORMAL;
    }

    switch( loc.where() ) {
        case item_location_type::character:
            return temperature_flag::TEMP_NORMAL;
        case item_location_type::monster:
            return temperature_flag::TEMP_NORMAL;
        case item_location_type::map: {
            const auto pos = loc.bub_pos();
            return for_tile( {
                .root_cellar = m.ter( pos ) == t_rootcellar,
                .fridge = m.has_flag_furn( TFLAG_FRIDGE, pos ),
                .freezer = m.has_flag_furn( TFLAG_FREEZER, pos ),
            } );
        }
        case item_location_type::vehicle: {
            // The location names its vehicle; the map cache misses vehicles not placed on it yet.
            const auto flag = loc.vehicle_storage_temperature();
            if( !flag ) {
                debugmsg( "Item %s is in a vehicle without a vehicle location", loc.tname() );
                return temperature_flag::TEMP_NORMAL;
            }
            return *flag;
        }
        case item_location_type::container: {
            const auto parent = loc.parent_item();
            if( parent == nullptr ) {
                return temperature_flag::TEMP_NORMAL;
            }
            return for_location( m, *parent );
        }
        default:
            debugmsg( "Invalid item location %d", static_cast<int>( loc.where() ) );
            return temperature_flag::TEMP_NORMAL;
    }
}

auto for_part( const vehicle &veh, const size_t part_index,
               const bool engine_heater_is_on ) -> temperature_flag
{
    const auto &part = veh.cpart( part_index );
    const auto &info = part.info();
    if( part.enabled && info.has_flag( VPFLAG_FREEZER ) ) {
        return temperature_flag::TEMP_FREEZER;
    }

    if( part.enabled && info.has_flag( VPFLAG_FRIDGE ) ) {
        return temperature_flag::TEMP_FRIDGE;
    }

    if( engine_heater_is_on ) {
        return temperature_flag::TEMP_HEATER;
    }

    return temperature_flag::TEMP_NORMAL;
}

auto for_part( const vehicle &veh, const size_t part_index ) -> temperature_flag
{
    return for_part( veh, part_index, engine_heater_is_on( veh ) );
}

auto engine_heater_is_on( const vehicle &veh ) -> bool
{
    return veh.has_part( "E_HEATER", true ) && veh.engine_on;
}

} // namespace rot::temp
