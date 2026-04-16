#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"

#include "achievement.h"
#include "achievement_runtime.h"
#include "calendar.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"
#include "character.h"
#include "debug.h"
#include "event_statistics.h"
#include "kill_tracker.h"
#include "skill.h"
#include "stats_tracker.h"
#include "type_id.h"

namespace
{

auto comparison_string( const achievement_comparison comparison ) -> std::string
{
    switch( comparison ) {
        case achievement_comparison::less_equal:
            return "<=";
        case achievement_comparison::greater_equal:
            return ">=";
        case achievement_comparison::anything:
            return "anything";
        case achievement_comparison::last:
            break;
    }
    debugmsg( "Invalid achievement_comparison" );
    return "anything";
}

auto completion_string( const achievement_completion completion ) -> std::string
{
    switch( completion ) {
        case achievement_completion::pending:
            return "pending";
        case achievement_completion::completed:
            return "completed";
        case achievement_completion::failed:
            return "failed";
        case achievement_completion::last:
            break;
    }
    debugmsg( "Invalid achievement_completion" );
    return "pending";
}

auto completion_from_string( const std::string &completion ) -> achievement_completion
{
    if( completion == "completed" ) {
        return achievement_completion::completed;
    }
    if( completion == "failed" ) {
        return achievement_completion::failed;
    }
    if( completion == "pending" ) {
        return achievement_completion::pending;
    }

    debugmsg( "Invalid achievement completion string: %s", completion );
    return achievement_completion::pending;
}

auto tracker() -> achievements_tracker *
{
    return achievement_runtime::active_tracker();
}

auto add_hidden_by( sol::state_view lua, sol::table &entry,
                    const achievement &ach ) -> void
{
    auto hidden_by = lua.create_table();
    auto index = 1;
    for( const string_id<achievement> &hidden_id : ach.hidden_by() ) {
        hidden_by[index++] = hidden_id.str();
    }
    entry["hidden_by"] = hidden_by;
}

auto add_time_constraint( sol::state_view lua, sol::table &entry,
                          const achievement &ach ) -> void
{
    if( !ach.time_constraint() ) {
        return;
    }

    entry["time_constraint"] = lua.create_table_with(
                                   "comparison", comparison_string( ach.time_constraint()->comparison() ),
                                   "target_turn", to_turn<int>( ach.time_constraint()->target() )
                               );
}

auto add_skill_requirements( sol::state_view lua, sol::table &entry,
                             const achievement &ach ) -> void
{
    auto requirements = lua.create_table();
    auto index = 1;
    for( const auto &[skill_id, requirement] : ach.skill_requirements() ) {
        const auto &[comparison, level] = requirement;
        requirements[index++] = lua.create_table_with(
                                    "skill_id", skill_id.str(),
                                    "comparison", comparison_string( comparison ),
                                    "level", level
                                );
    }
    entry["skill_requirements"] = requirements;
}

auto add_kill_requirements( sol::state_view lua, sol::table &entry,
                            const achievement &ach ) -> void
{
    auto requirements = lua.create_table();
    auto index = 1;
    for( const auto &[monster_id, requirement] : ach.kill_requirements() ) {
        const auto &[comparison, count] = requirement;
        auto row = lua.create_table_with(
                       "comparison", comparison_string( comparison ),
                       "count", count,
                       "kind", monster_id == mtype_id::NULL_ID() ? "all" : "monster"
                   );
        if( monster_id != mtype_id::NULL_ID() ) {
            row["id"] = monster_id.str();
        }
        requirements[index++] = row;
    }
    for( const auto &[species_id, requirement] : ach.species_kill_requirements() ) {
        const auto &[comparison, count] = requirement;
        requirements[index++] = lua.create_table_with(
                                    "id", species_id.str(),
                                    "comparison", comparison_string( comparison ),
                                    "count", count,
                                    "kind", "species"
                                );
    }
    entry["kill_requirements"] = requirements;
}

auto add_requirements( sol::state_view lua, sol::table &entry,
                       const achievement &ach ) -> void
{
    auto requirements = lua.create_table();
    auto index = 1;
    for( const achievement_requirement &req : ach.requirements() ) {
        auto row = lua.create_table_with(
                       "statistic_id", req.statistic.str(),
                       "comparison", comparison_string( req.comparison ),
                       "becomes_false", req.becomes_false
                   );
        if( req.comparison != achievement_comparison::anything ) {
            row["target"] = req.target;
        }
        requirements[index++] = row;
    }
    entry["requirements"] = requirements;
}

} // namespace

void cata::detail::reg_achievement_api( sol::state &lua )
{
    DOC( "Achievement runtime helpers for Lua-driven tracking." );
    auto lib = luna::begin_lib( lua, "achievement_api" );

    luna::set_fx( lib, "definitions", []( sol::this_state lua_this ) {
        sol::state_view lua_state( lua_this );
        auto out = lua_state.create_table();
        auto index = 1;
        for( const achievement &ach : achievement::get_all() ) {
            auto entry = lua_state.create_table_with( "id", ach.id.str() );
            add_hidden_by( lua_state, entry, ach );
            add_time_constraint( lua_state, entry, ach );
            add_skill_requirements( lua_state, entry, ach );
            add_kill_requirements( lua_state, entry, ach );
            add_requirements( lua_state, entry, ach );
            out[index++] = entry;
        }
        return out;
    } );

    luna::set_fx( lib, "state", []( const std::string & achievement_id ) -> std::string {
        const auto *active = tracker();
        if( active == nullptr )
        {
            return completion_string( achievement_completion::pending );
        }
        return completion_string( active->recorded_completion( string_id<achievement>( achievement_id ) ) );
    } );

    luna::set_fx( lib, "report", []( const std::string & achievement_id,
    const std::string & completion ) -> void {
        auto *active = tracker();
        if( active == nullptr )
        {
            return;
        }
        active->report_achievement( string_id<achievement>( achievement_id ),
                                    completion_from_string( completion ) );
    } );

    luna::set_fx( lib, "stat_value", []( const std::string & statistic_id ) -> int {
        auto *active = tracker();
        if( active == nullptr )
        {
            return 0;
        }
        const auto id = string_id<event_statistic>( statistic_id );
        if( !id.is_valid() )
        {
            return 0;
        }
        return active->stats().value_of( id ).get<int>();
    } );

    luna::set_fx( lib, "skill_level", []( const std::string & skill_name ) -> int {
        const auto id = skill_id( skill_name );
        if( !id.is_valid() )
        {
            return 0;
        }
        return get_player_character().get_skill_level( id );
    } );

    luna::set_fx( lib, "total_kill_count", []() -> int {
        const auto *active = tracker();
        return active == nullptr ? 0 : active->kills()->monster_kill_count();
    } );

    luna::set_fx( lib, "monster_kill_count", []( const std::string & monster_name ) -> int {
        const auto *active = tracker();
        const auto id = mtype_id( monster_name );
        if( active == nullptr || !id.is_valid() )
        {
            return 0;
        }
        return active->kills()->kill_count( id );
    } );

    luna::set_fx( lib, "species_kill_count", []( const std::string & species_name ) -> int {
        const auto *active = tracker();
        const auto id = species_id( species_name );
        if( active == nullptr || !id.is_valid() || id.is_null() )
        {
            return 0;
        }
        return active->kills()->kill_count( id );
    } );

    luna::set_fx( lib, "current_turn", []() -> int {
        return to_turn<int>( calendar::turn );
    } );
    luna::set_fx( lib, "game_start_turn", []() -> int {
        return to_turn<int>( calendar::start_of_game );
    } );
    luna::set_fx( lib, "cataclysm_start_turn", []() -> int {
        return to_turn<int>( calendar::start_of_cataclysm );
    } );

    luna::finalize_lib( lib );
}
