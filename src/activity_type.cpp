#include "activity_type.h"

#include <functional>
#include <map>
#include <utility>

#include "activity_actor.h"
#include "activity_handlers.h"
#include "assign.h"
#include "debug.h"
#include "flag.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "sounds.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "game.h"
#include "player_activity.h"
#include "item.h"
namespace
{
generic_factory<activity_type> all_activities( "Activity Types" );
}

/** @relates string_id */
template<>
const activity_type &string_id<activity_type>::obj() const
{
    all_activities.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<activity_type>::is_valid() const
{
    return all_activities.is_valid( *this );
}

namespace io
{
template<>
std::string enum_to_string<activity_bubble_effect>( activity_bubble_effect data )
{
    switch( data ) {
        // *INDENT-OFF*
        case activity_bubble_effect::none:   return "none";
        case activity_bubble_effect::mobile: return "mobile";
        case activity_bubble_effect::idle:   return "idle";
        case activity_bubble_effect::last:   break;
        // *INDENT-ON*
    }
    debugmsg( "Invalid activity_bubble_effect", data );
    return "none";
}
} // namespace io


void activity_type::load_activities( const JsonObject &jo, const std::string &src )
{
    all_activities.load( jo, src );
}

void activity_type::load( const JsonObject &jo, const std::string &src )
{
    assign( jo, "rooted", rooted_, true );
    assign( jo, "verb", verb_, true );
    assign( jo, "suspendable", suspendable_, true );
    assign( jo, "no_resume", no_resume_, true );
    assign( jo, "special", special_, false );
    assign( jo, "multi_activity", multi_activity_, false );
    assign( jo, "refuel_fires", refuel_fires, false );
    assign( jo, "auto_needs", auto_needs, false );
    assign( jo, "morale_blocked", morale_blocked_, false );
    assign( jo, "verbose_tooltip", verbose_tooltip_, false );
    bubble_effect_ = jo.get_enum_value<activity_bubble_effect>( "bubble_size_effect",
                     activity_bubble_effect::none );
    if( jo.has_member( "complex_moves" ) ) {
        complex_moves_ = true;
        auto c_moves = jo.get_object( "complex_moves" );
        bench_affected_ = c_moves.get_bool( "bench", false );
        light_affected_ = c_moves.get_bool( "light", false );
        speed_affected_ = c_moves.get_bool( "speed", false );
        morale_affected_ = c_moves.get_bool( "morale", false );

        max_assistants_ = c_moves.get_int( "max_assistants", 0 );

        c_moves.allow_omitted_members();
        if( c_moves.has_bool( "skills" ) ) {
            assign( c_moves, "skills", skill_affected_, false );
        } else if( c_moves.has_array( "skills" ) ) {
            skill_affected_ = true;
            for( JsonArray skillobj : c_moves.get_array( "skills" ) ) {
                std::string skill_s = skillobj.get_string( 0 );
                auto skill = skill_id( skill_s );
                float mod = 1.0f;
                int threshold = 0;
                if( skillobj.size() > 1 ) {
                    mod = skillobj.get_float( 1 );
                }
                if( skillobj.size() > 2 ) {
                    threshold = skillobj.get_int( 2 );
                }
                skills.emplace_back(
                    activity_req<skill_id>( skill, mod, threshold )
                );
            }
        }

        if( c_moves.has_bool( "qualities" ) ) {
            assign( c_moves, "qualities", tools_affected_, false );
        } else if( c_moves.has_array( "qualities" ) ) {
            tools_affected_ = true;
            for( JsonArray q_obj : c_moves.get_array( "qualities" ) ) {
                std::string quality_s = q_obj.get_string( 0 );
                auto quality = quality_id( quality_s );
                int mod = 10;
                int threshold = 0;
                if( q_obj.size() > 1 ) {
                    mod = q_obj.get_float( 1 );
                }
                if( q_obj.size() > 2 ) {
                    threshold = q_obj.get_int( 2 );
                }
                qualities.emplace_back(
                    activity_req<quality_id>( quality, mod, threshold )
                );
            }
        }

        if( c_moves.has_bool( "stats" ) ) {
            assign( c_moves, "stats", stats_affected_, false );
        } else if( c_moves.has_array( "stats" ) ) {
            stats_affected_ = true;
            for( JsonArray stat_obj : c_moves.get_array( "stats" ) ) {
                auto stat = io::string_to_enum_fallback( stat_obj.get_string( 0 ), character_stat::DUMMY_STAT );
                if( stat == character_stat::DUMMY_STAT ) {
                    debugmsg( "Unknown stat %s", stat_obj.get_string( 0 ) );
                } else {
                    float mod = 1.0f;
                    int threshold = 8;
                    if( stat_obj.size() > 1 ) {
                        mod = stat_obj.get_float( 1 );
                    }
                    if( stat_obj.size() > 2 ) {
                        threshold = stat_obj.get_int( 2 );
                    }
                    stats.emplace_back(
                        activity_req<character_stat>( stat, mod, threshold )
                    );
                }
            }
        }
    }
}

void activity_type::check_consistency()
{
    all_activities.check();
}
void activity_type::check() const
{
    if( max_assistants_ < 0 || max_assistants_ > 32 ) {
        debugmsg( "Forbidden value of max_assistants - %s. Value sould be between 0 and 32",
                  max_assistants_ );
    }
    if( verb_.empty() ) {
        debugmsg( "%s doesn't have a verb", id.c_str() );
    }
    const bool has_actor = activity_actors::deserialize_functions.contains( id );
    const bool has_turn_func = activity_handlers::do_turn_functions.contains( id );

    if( special_ && !( has_turn_func || has_actor ) ) {
        debugmsg( "%s needs a do_turn function or activity actor if it expects a special behaviour.",
                  id.c_str() );
    }
    for( auto &skill : skills ) {
        if( !skill.req.is_valid() ) {
            debugmsg( "Unknown skill id %s", skill.req.str() );
        }
    }
    for( auto &quality : qualities ) {
        if( !quality.req.is_valid() ) {
            debugmsg( "Unknown quality id %s", quality.req.str() );
        }
    }
    for( const auto &pair : activity_handlers::do_turn_functions ) {
        if( !pair.first.is_valid() ) {
            debugmsg( "The do_turn function %s doesn't correspond to a valid activity_type.",
                      pair.first.c_str() );
        }
    }

    for( const auto &pair : activity_handlers::finish_functions ) {
        if( !pair.first.is_valid() ) {
            debugmsg( "The finish_function %s doesn't correspond to a valid activity_type",
                      pair.first.c_str() );
        }
    }
}

void activity_type::call_do_turn( player_activity *act, player *p ) const
{
    const auto &pair = activity_handlers::do_turn_functions.find( id );
    if( pair != activity_handlers::do_turn_functions.end() ) {
        pair->second( act, p );
    }
}

bool activity_type::call_finish( player_activity *act, player *p ) const
{
    const auto &pair = activity_handlers::finish_functions.find( id );
    if( pair != activity_handlers::finish_functions.end() ) {
        pair->second( act, p );
        // kill activity sounds at finish
        sfx::end_activity_sounds();
        if( !act->get_tools().empty() ) {
            auto &tool = *act->get_tools().front();
            if( tool.has_flag( flag_TEMPORARY_ITEM ) ) {
                g->remove_fake_item( &tool );
            }
        }
        return true;
    }
    return false;
}

void activity_type::reset()
{
    all_activities.reset();
}

std::string activity_type::stop_phrase() const
{
    return string_format( _( "Stop %s?" ), verb_ );
}
