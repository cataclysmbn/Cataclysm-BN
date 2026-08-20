#include "mapgen_color_palette.h"

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
#include "type_id_implement.h"
#include "units_angle.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

namespace
{
generic_factory<MapgenColorPalette> all_palettes( "Mapgen Palettes" );
}

IMPLEMENT_STRING_AND_INT_IDS( MapgenColorPalette, all_palettes );

void MapgenColorPalette::load_palette( const JsonObject &jo, const std::string &src )
{
    all_palettes.load( jo, src );
}

int MapgenColorPalette::next_id = 0;

mpalette_id MapgenColorPalette::define_new_palette( const JsonObject &obj )
{
    MapgenColorPalette pal = MapgenColorPalette();
    pal.load( obj, "" );
    pal.id = get_unique_id();
    all_palettes.insert( pal );
    return pal.id;
}

mpalette_id MapgenColorPalette::get_unique_id()
{
    static const std::string unique_prefix = "\u01F7 ";
    while( true ) {
        const mpalette_id new_group = mpalette_id( unique_prefix + std::to_string( next_id++ ) );
        if( !new_group.is_valid() ) {
            return new_group;
        }
    }
}
void MapgenColorPalette::load( const JsonObject &jo, const std::string & )
{
    if( jo.has_bool( "clear" ) && jo.get_bool( "clear" ) ) {
        colors.clear();
    }
    for( const JsonValue colval : jo.get_array( "colors" ) ) {
        if( colval.test_object() ) {
            const JsonObject col = colval.get_object();
            colors.add( col.get_string( "color" ), col.get_int( "weight", 100 ) );
        } else {
            colors.add( colval.get_string(), 100 );
        }
    }
}

void MapgenColorPalette::check_definitions()
{
    all_palettes.check();
}
void MapgenColorPalette::check() const
{
    for( auto colorstr : colors ) {
        std::optional<RGBColor> color = RGBColor::try_parse( colorstr.obj );
        if( !color ) {
            debugmsg( "Invalid Color %s in Mapgen Palette %s", colorstr.obj, id.str() );
        }
    }
}

std::optional<RGBColor> MapgenColorPalette::pick_color( unsigned int seed ) const
{
    std::string colorstr = *colors.pick( seed );
    std::optional<RGBColor> color = RGBColor::try_parse( colorstr );
    return color;
}

void MapgenColorPalette::reset()
{
    all_palettes.reset();
}

