#include "gamemode_editor.h" // IWYU pragma: associated

#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "character_functions.h"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "overmap/omdata.h"
#include "overmap/overmap.h"
#include "overmap/overmapbuffer.h"
#include "profession.h"
#include "scenario.h"
#include "translations.h"
#include "type_id.h"
#include "weather/weather.h"

#include <array>
#include <utility>

namespace
{

/// The editor itself, and what keeps its avatar out of harm's and the creatures' way.
constexpr auto editor_traits = std::array { "DEBUG_EDITOR", "DEBUG_CLOAK", "DEBUG_NODMG", "DEBUG_LS", "DEBUG_NOTEMP", "DEBUG_NOSCENT", "DEBUG_SILENT" };

} // namespace

auto editor_game::init() -> bool
{
    auto &you = get_avatar();
    // Noon, so the field is in daylight.
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().update_weather();
    character_funcs::normalize( you );
    you.set_all_parts_hp_to_max();
    //~ name of the avatar of the main menu's map editor
    you.name = _( "Editor" );
    you.prof = profession::generic();
    // No character creation here, so the scenario saved with the avatar must be set too.
    g->scen = scenario::generic();
    for( const auto *trait : editor_traits ) {
        you.set_mutation( trait_id( trait ) );
    }

    // The first overmap is open field, all of it seen, with rock under it and air over it.
    auto &starting_om = get_primary_overmapbuffer().get( point_abs_om() );
    const auto field = oter_id( "field" );
    const auto rock = oter_id( "empty_rock" );
    const auto air = oter_id( "open_air" );
    for( auto z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        const auto &terrain = z < 0 ? rock : z == 0 ? field : air;
        for( auto x = 0; x < OMAPX; ++x ) {
            for( auto y = 0; y < OMAPY; ++y ) {
                starting_om.ter_set( tripoint_om_omt( x, y, z ), terrain );
                starting_om.seen( tripoint_om_omt( x, y, z ) ) = z == 0;
            }
        }
    }
    starting_om.clear_mon_groups();

    // The avatar stands in the middle of the overmap, in the middle of the map.
    const auto center = project_combine( point_abs_om(), tripoint_om_omt( OMAPX / 2, OMAPY / 2, 0 ) );
    auto origin = project_to<coords::sm>( center );
    origin.x() -= g_half_mapsize;
    origin.y() -= g_half_mapsize;
    g->load_map( origin.xy() );
    you.setpos( project_to<coords::ms>( origin + tripoint_rel_sm( g_half_mapsize, g_half_mapsize,
                                        0 ) ) );
    return true;
}

auto editor_game::next_action() -> std::optional<action_id>
{
    return std::exchange( opened, true ) ? std::nullopt : std::optional( ACTION_EDITOR );
}
