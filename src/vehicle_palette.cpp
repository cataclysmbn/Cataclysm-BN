#include "vehicle_palette.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include "debug.h"
#include "game_constants.h"
#include "generic_factory.h"
#include "hsv_color.h"
#include "json.h"
#include "map.h"
#include "memory_fast.h"
#include "options.h"
#include "point.h"
#include "rng.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "units_angle.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

namespace
{
generic_factory<VehiclePalette> all_palettes( "Vehicle Palettes" );
}

/** @relates string_id */
template<>
const VehiclePalette &string_id<VehiclePalette>::obj() const
{
    return all_palettes.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<VehiclePalette>::is_valid() const
{
    return all_palettes.is_valid( *this );
}

/** @relates string_id */
template<>
int_id<VehiclePalette> string_id<VehiclePalette>::id() const
{
    return all_palettes.convert( *this, int_id<VehiclePalette>( INVALID_CID ) );
}

void VehiclePalette::load_palette( const JsonObject &jo, const std::string &src )
{
    all_palettes.load( jo, src );
}

void VehiclePalette::load( const JsonObject &jo, const std::string & )
{
    if( jo.has_bool( "clear" ) && jo.get_bool( "clear" ) ) {
        fuzzy_color_match.clear();
        colors.clear();
    }
    for( const JsonObject obj : jo.get_array( "palette" ) ) {
        for( const std::string &id : obj.get_string_array( "fuzzy_ids" ) ) {
            fuzzy_color_match[id] = colors.size();
        }
        auto weights = weighted_int_list<std::string>();
        for( const JsonObject col : obj.get_array( "colors" ) ) {
            weights.add( col.get_string( "color" ), col.get_int( "weight" ) );
        }
        colors.push_back( weights );
    }
}

void VehiclePalette::check_definitions()
{
    all_palettes.check();
}
void VehiclePalette::check() const
{
    for( auto colorlist : colors ) {
        for( auto colorstr : colorlist ) {
            std::optional<RGBColor> color = RGBColor::try_parse( colorstr.obj );
            if( !color ) {
                debugmsg( "Invalid Color %s in Vehicle Palette %s", colorstr.obj, id.str() );
            }
        }
    }
}

int VehiclePalette::fuzzy_to_index( const vpart_id &id ) const
{
    for( auto const &[ fuzzy, index ] : fuzzy_color_match ) {
        if( id.str().contains( fuzzy ) || id.str() == fuzzy ) {
            return index;
        }
    }
    return -1;
}

std::vector<RGBColor> VehiclePalette::pick_colors() const
{
    std::vector<RGBColor> result;
    for( const auto &colorlist : colors ) {
        std::string colorstr = *colorlist.pick();
        std::optional<RGBColor> color = RGBColor::try_parse( colorstr );
        if( color ) {
            result.push_back( *color );
        } else {
            debugmsg( "Invalid Color %s in Vehicle Palette %s", colorstr, id );
        }
    }
    return result;
}

void VehiclePalette::reset()
{
    all_palettes.reset();
}

