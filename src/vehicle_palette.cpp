#include "vehicle_palette.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include "debug.h"
#include "game_constants.h"
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

/** @relates string_id */
template<>
const VehiclePalette &string_id<VehiclePalette>::obj() const
{
    const auto iter = vehicle_color_palettes.find( *this );
    if( iter == vehicle_color_palettes.end() ) {
        debugmsg( "invalid vehicle color palette id %s", c_str() );
        static const VehiclePalette dummy{};
        return dummy;
    }
    return iter->second;
}

/** @relates string_id */
template<>
bool string_id<VehiclePalette>::is_valid() const
{
    return vehicle_color_palettes.contains( *this );
}

void VehiclePalette::load( const JsonObject &jo )
{
    VehiclePalette &palette = vehicle_color_palettes[vpalette_id( jo.get_string( "id" ) )];

    if( jo.has_bool( "clear" ) && jo.get_bool( "clear" ) ) {
        palette.fuzzy_color_match.clear();
        palette.colors.clear();
    }
    palette.id = vpalette_id( jo.get_string( "id" ) );
    for( const JsonObject obj : jo.get_array( "palette" ) ) {
        for( const std::string &id : obj.get_string_array( "fuzzy_ids" ) ) {
            palette.fuzzy_color_match[id] = palette.colors.size();
        }
        auto weights = weighted_int_list<std::string>();
        for( const JsonObject col : obj.get_array( "colors" ) ) {
            weights.add( col.get_string( "color" ), col.get_int( "weight" ) );
        }
        palette.colors.push_back( weights );
    }
}

void VehiclePalette::check()
{
    for( auto palette : vehicle_color_palettes ) {
        for( auto colorlist : palette.second.colors ) {
            for( auto colorstr : colorlist ) {
                std::optional<RGBColor> color = RGBColor::try_parse( colorstr.obj );
                if( !color ) {
                    debugmsg( "Invalid Color %s in Vehicle Palette %s", colorstr.obj, palette.first.str() );
                }
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
    vehicle_color_palettes.clear();
}

