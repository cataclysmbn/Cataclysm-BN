#include "activity_type.h"

#include <functional>
#include <map>
#include <utility>

#include "activity_actor.h"
#include "activity_handlers.h"
#include "assign.h"
#include "debug.h"
#include "enum_conversions.h"
#include "json.h"
#include "sounds.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "game.h"
#include "player_activity.h"
#include "item.h"
// activity_type functions
static std::map< activity_id, activity_type > activity_type_all;

/** @relates string_id */
template<>
const activity_type &string_id<activity_type>::obj() const
{
    const auto found = activity_type_all.find( *this );
    if( found == activity_type_all.end() ) {
        debugmsg( "Tried to get invalid activity_type: %s", c_str() );
        static const activity_type null_activity_type {};
        return null_activity_type;
    }
    return found->second;
}

/** @relates string_id */
template<>
bool string_id<activity_type>::is_valid() const
{
    const auto found = activity_type_all.find( *this );
    return found != activity_type_all.end();
}


void activity_type::load( const JsonObject &jo )
{
    activity_type result;

    result.id_ = activity_id( jo.get_string( "id" ) );
    assign( jo, "rooted", result.rooted_, true );
    assign( jo, "verb", result.verb_, true );
    assign( jo, "suspendable", result.suspendable_, true );
    assign( jo, "no_resume", result.no_resume_, true );
    assign( jo, "special", result.special_, false );
    assign( jo, "multi_activity", result.multi_activity_, false );
    assign( jo, "refuel_fires", result.refuel_fires, false );
    assign( jo, "auto_needs", result.auto_needs, false );
    assign( jo, "morale_blocked", result.morale_blocked_, false );
    assign( jo, "verbose_tooltip", result.verbose_tooltip_, false );
    if( jo.has_member( "complex_moves" ) ) {
        result.complex_moves_ = true;
        auto c_moves = jo.get_object( "complex_moves" );
        result.bench_affected_ = c_moves.get_bool( "bench", false );
        result.light_affected_ = c_moves.get_bool( "light", false );
        result.speed_affected_ = c_moves.get_bool( "speed", false );
        result.morale_affected_ = c_moves.get_bool( "morale", false );

        int jvalue = c_moves.get_int( "max_assistants", 0 );
        if( jvalue >= 0 || jvalue > 32 ) {
            result.max_assistants_ = jvalue;
        } else {
            debugmsg( "Forbidden value of max_assistants - %s. Value sould be between 0 and 32", jvalue );
        }

        c_moves.allow_omitted_members();
        if( c_moves.has_bool( "skills" ) ) {
            assign( c_moves, "skills", result.skill_affected_, false );
        } else if( c_moves.has_array( "skills" ) ) {
            result.skill_affected_ = true;
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
                result.skills.emplace_back(
                    activity_req<skill_id>( skill, mod, threshold )
                );
            }
        }

        if( c_moves.has_bool( "qualities" ) ) {
            assign( c_moves, "qualities", result.tools_affected_, false );
        } else if( c_moves.has_array( "qualities" ) ) {
            result.tools_affected_ = true;
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
                result.qualities.emplace_back(
                    activity_req<quality_id>( quality, mod, threshold )
                );
            }
        }

        if( c_moves.has_bool( "stats" ) ) {
            assign( c_moves, "stats", result.stats_affected_, false );
        } else if( c_moves.has_array( "stats" ) ) {
            result.stats_affected_ = true;
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
                    result.stats.emplace_back(
                        activity_req<character_stat>( stat, mod, threshold )
                    );
                }

            }
        }
    }

    if( activity_type_all.contains( result.id_ ) ) {
        debugmsg( "Redefinition of %s", result.id_.c_str() );
    } else {
        activity_type_all.insert( { result.id_, result } );
    }
}

void activity_type::check_consistency()
{
    for( const auto &pair : activity_type_all ) {
        if( pair.second.verb_.empty() ) {
            debugmsg( "%s doesn't have a verb", pair.first.c_str() );
        }
        const bool has_actor = activity_actors::deserialize_functions.contains( pair.second.id_ );
        const bool has_turn_func = activity_handlers::do_turn_functions.contains( pair.second.id_ );

        if( pair.second.special_ && !( has_turn_func || has_actor ) ) {
            debugmsg( "%s needs a do_turn function or activity actor if it expects a special behaviour.",
                      pair.second.id_.c_str() );
        }
        for( auto &skill : pair.second.skills )
            if( !skill.req.is_valid() ) {
                debugmsg( "Unknown skill id %s", skill.req.str() );
            }

        for( auto &quality : pair.second.qualities )
            if( !quality.req.is_valid() ) {
                debugmsg( "Unknown quality id %s", quality.req.str() );
            }
    }

    for( const auto &pair : activity_handlers::do_turn_functions ) {
        if( !activity_type_all.contains( pair.first ) ) {
            debugmsg( "The do_turn function %s doesn't correspond to a valid activity_type.",
                      pair.first.c_str() );
        }
    }

    for( const auto &pair : activity_handlers::finish_functions ) {
        if( !activity_type_all.contains( pair.first ) ) {
            debugmsg( "The finish_function %s doesn't correspond to a valid activity_type",
                      pair.first.c_str() );
        }
    }
}

void activity_type::call_do_turn( player_activity *act, player *p ) const
{
    const auto &pair = activity_handlers::do_turn_functions.find( id_ );
    if( pair != activity_handlers::do_turn_functions.end() ) {
        pair->second( act, p );
    }
}

bool activity_type::call_finish( player_activity *act, player *p ) const
{
    const auto &pair = activity_handlers::finish_functions.find( id_ );
    if( pair != activity_handlers::finish_functions.end() ) {
        pair->second( act, p );
        // kill activity sounds at finish
        sfx::end_activity_sounds();
        if( !act->tools.empty() ) {
            g->remove_fake_item( &*act->tools.front() );
        }
        return true;
    }
    return false;
}

void activity_type::reset()
{
    activity_type_all.clear();
}

std::string activity_type::stop_phrase() const
{
    return string_format( _( "Stop %s?" ), verb_ );
}
