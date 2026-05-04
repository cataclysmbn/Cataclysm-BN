#include "vehicle_palette.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include "debug.h"
#include "hsv_color.h"
#include "json.h"
#include "map.h"
#include "memory_fast.h"
#include "options.h"
#include "point.h"
#include "rng.h"
#include "translations.h"
#include "type_id.h"
#include "units_angle.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

std::unordered_map<vpalette_id, VehiclePalette> vehicle_color_palettes;

/** @relates string_id */
template<>
const VehicleGroup &string_id<VehicleGroup>::obj() const
{
    const auto iter = vehicle_color_palettes.find( *this );
    if( iter == vgroups.end() ) {
        debugmsg( "invalid vehicle color palette id %s", c_str() );
        static const VehiclePalette dummy{};
        return dummy;
    }
    return iter->second;
}

void VehiclePalette::load( const JsonObject &jo )
{
    VehiclePalette &palette = vehicle_color_palettes[vpalette_id( jo.get_string( "id" ) )];

    for( const JsonObject obj : jo.get_array( "palette" ) ) {
        for( const std::string &id : obj.get_string_array( "fuzzy_ids" ) ) {
            fuzzy_color_match[id] = colors.size();
        }
        auto weights = weighted_int_list<RGBColor>();
        for( const JsonObject col : obj.get_array( "colors" ) ) {
            weights.add( rgb_from_hex_string( col.get_string( "color" ) ), col.get_int( "weight" ) );
        }
        colors.push_back( weights );
    }
}

int VehiclePalette::fuzzy_to_index( const vpart_id &id ) const
{
    for( auto const &[ fuzzy, index ] : fuzzy_color_match ) {
        if( id.str().contains( fuzzy ) || id.str() == fuzzy ) {
            return index;
        }
    }
}

std::vector<RGBColor> pick_colors( const int index ) const
{
    
}
void VehicleGroup::reset()
{
    vgroups.clear();
}

