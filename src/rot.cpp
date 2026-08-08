#include "rot.h"

#include "item.h"
#include "locations.h"
#include "map.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "veh_type.h"
#include "vpart_position.h"

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

auto for_location( const item &loc ) -> temperature_flag
{
    if( !loc.has_position() ) {
        return temperature_flag::TEMP_NORMAL;
    }

    if( const auto *vehicle_loc = dynamic_cast<const vehicle_item_location *>
                                  ( loc.get_location() ) ) {
        return vehicle_loc->storage_temperature();
    }

    switch( loc.where() ) {
        case item_location_type::character:
            return temperature_flag::TEMP_NORMAL;
        case item_location_type::monster:
            return temperature_flag::TEMP_NORMAL;
        case item_location_type::container: {
            const auto parent = loc.parent_item();
            if( parent == nullptr ) {
                return temperature_flag::TEMP_NORMAL;
            }
            return for_location( *parent );
        }
        case item_location_type::map: {
            auto &here = loc.get_mapbuffer();
            const auto pos = loc.abs_pos();
            return for_tile( {
                .root_cellar = here.ter( pos ) == t_rootcellar,
                .fridge = here.has_flag( TFLAG_FRIDGE, pos ),
                .freezer = here.has_flag( TFLAG_FREEZER, pos ),
            } );
        }
        case item_location_type::vehicle: {
            auto &here = loc.get_mapbuffer();
            const auto pos = loc.abs_pos();
            optional_vpart_position veh = here.veh_at( pos );
            if( !veh ) {
                debugmsg( "Expected vehicle at %d, %d, %d, but couldn't find any", pos.x(), pos.y(), pos.z() );
                return temperature_flag::TEMP_NORMAL;
            }
            int cargo_index = veh->vehicle().part_with_feature( veh->part_index(), VPFLAG_CARGO, true );
            if( cargo_index < 0 ) {
                return temperature_flag::TEMP_NORMAL;
            }
            return for_part( veh->vehicle(), cargo_index );
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

} // namespace rot::temp
