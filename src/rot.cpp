#include "rot.h"

#include "item.h"
#include "map.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "veh_type.h"
#include "vpart_position.h"

namespace rot
{

auto temperature_flag_for_location( const map &m, const item &loc ) -> temperature_flag
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
            auto pos = loc.position();
            if( m.has_flag_furn( TFLAG_FREEZER, pos ) ) {
                return temperature_flag::TEMP_FREEZER;
            }
            if( m.has_flag_furn( TFLAG_FRIDGE, pos ) ) {
                return temperature_flag::TEMP_FRIDGE;
            }
            if( m.ter( pos ) == t_rootcellar ) {
                return temperature_flag::TEMP_ROOT_CELLAR;
            }
            return temperature_flag::TEMP_NORMAL;
        }
        case item_location_type::vehicle: {
            auto pos = loc.position();
            optional_vpart_position veh = m.veh_at( pos );
            if( !veh ) {
                debugmsg( "Expected vehicle at %d, %d, %d, but couldn't find any", pos.x(), pos.y(), pos.z() );
                return temperature_flag::TEMP_NORMAL;
            }
            int cargo_index = veh->vehicle().part_with_feature( veh->part_index(), VPFLAG_CARGO, true );
            if( cargo_index < 0 ) {
                return temperature_flag::TEMP_NORMAL;
            }
            return temperature_flag_for_part( veh->vehicle(), cargo_index );
        }
        case item_location_type::container:
            return temperature_flag_for_location( m, *loc.parent_item() );
        default:
            debugmsg( "Invalid item location %d", static_cast<int>( loc.where() ) );
            return temperature_flag::TEMP_NORMAL;
    }
}

auto temperature_flag_for_part( const vehicle &veh, size_t part_index ) -> temperature_flag
{
    const vehicle_part &part = veh.cpart( part_index );
    const vpart_info &info = part.info();
    if( !part.enabled ) {
        return temperature_flag::TEMP_NORMAL;
    }

    if( info.has_flag( VPFLAG_FREEZER ) ) {
        return temperature_flag::TEMP_FREEZER;
    }

    if( info.has_flag( VPFLAG_FRIDGE ) ) {
        return temperature_flag::TEMP_FRIDGE;
    }
    return temperature_flag::TEMP_NORMAL;
}

} // namespace rot
