#include "achievement.h"

#include "achievement_runtime.h"

#include <algorithm>
#include <cstdlib>
#include <ranges>
#include <utility>

#include "character.h"
#include "color.h"
#include "debug.h"
#include "event.h"
#include "event_statistics.h"
#include "game.h"
#include "generic_factory.h"
#include "json.h"
#include "kill_tracker.h"
#include "monstergenerator.h"
#include "skill.h"
#include "stats_tracker.h"
#include "string_formatter.h"

// Achievement state transitions are evaluated by the Lua runtime via the
// on_stat_changed hook.  C++ retains definition loading, UI formatting and the
// save/load wrapper so the serialized format remains unchanged.

namespace
{

generic_factory<achievement> achievement_factory( "achievement" );
achievements_tracker *active_tracker_instance = nullptr;

} // namespace

auto achievement_runtime::set_active_tracker( achievements_tracker *tracker ) -> void
{
    active_tracker_instance = tracker;
}

auto achievement_runtime::active_tracker() -> achievements_tracker *
{
    return active_tracker_instance;
}

/** @relates string_id */
template<>
const achievement &string_id<achievement>::obj() const
{
    return achievement_factory.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<achievement>::is_valid() const
{
    return achievement_factory.is_valid( *this );
}

namespace io
{

template<>
std::string enum_to_string<achievement_comparison>( achievement_comparison data )
{
    switch( data ) {
        // *INDENT-OFF*
        case achievement_comparison::less_equal: return "<=";
        case achievement_comparison::greater_equal: return ">=";
        case achievement_comparison::anything: return "anything";
        // *INDENT-ON*
        case achievement_comparison::last:
            break;
    }
    debugmsg( "Invalid achievement_comparison" );
    abort();
}

template<>
std::string enum_to_string<achievement::time_bound::epoch>( achievement::time_bound::epoch data )
{
    switch( data ) {
        // *INDENT-OFF*
        case achievement::time_bound::epoch::cataclysm: return "cataclysm";
        case achievement::time_bound::epoch::game_start: return "game_start";
        // *INDENT-ON*
        case achievement::time_bound::epoch::last:
            break;
    }
    debugmsg( "Invalid epoch" );
    abort();
}

} // namespace io

static nc_color color_from_completion( achievement_completion comp )
{
    switch( comp ) {
        case achievement_completion::pending:
            return c_yellow;
        case achievement_completion::completed:
            return c_light_green;
        case achievement_completion::failed:
            return c_light_gray;
        case achievement_completion::last:
            break;
    }
    debugmsg( "Invalid achievement_completion" );
    abort();
}

static time_point epoch_to_time_point( achievement::time_bound::epoch e )
{
    switch( e ) {
        case achievement::time_bound::epoch::cataclysm:
            return calendar::start_of_cataclysm;
        case achievement::time_bound::epoch::game_start:
            return calendar::start_of_game;
        case achievement::time_bound::epoch::last:
            break;
    }
    debugmsg( "Invalid epoch" );
    abort();
}

void achievement::time_bound::deserialize( JsonIn &jin )
{
    const JsonObject &jo = jin.get_object();
    if( !( jo.read( "since", epoch_ ) &&
           jo.read( "is", comparison_ ) &&
           jo.read( "target", period_ ) ) ) {
        jo.throw_error( "Mandatory field missing for achievement time_constaint" );
    }
}

void achievement::time_bound::check( const string_id<achievement> &id ) const
{
    if( comparison_ == achievement_comparison::anything ) {
        debugmsg( "Achievement %s has unconstrained \"anything\" time_constraint.  "
                  "Please change it to a consequential comparison.", id.str() );
    }
}

void achievement::add_kill_requirements( const JsonObject &jo, const std::string &src )
{
    if( !jo.has_array( "kill_requirements" ) ) {
        return;
    }

    for( const JsonValue entry : jo.get_array( "kill_requirements" ) ) {
        if( entry.test_object() ) {
            add_kill_requirement( entry, src );
        } else {
            entry.throw_error( "array element is not an object." );
        }
    }
}

void achievement::add_kill_requirement( const JsonObject &inner, const std::string & )
{
    if( inner.has_string( "monster" ) && inner.has_string( "species" ) ) {
        inner.throw_error( "Cannot have both id and species identifiers" );
    }
    if( inner.has_string( "monster" ) ) {
        const mtype_id victim = static_cast<mtype_id>( inner.get_string( "monster" ) );
        const achievement_comparison compare = inner.get_enum_value<achievement_comparison>( "is" );
        const int count = inner.get_int( "count" );

        kill_requirements_[victim] = std::make_pair( compare, count );
    } else if( inner.has_string( "species" ) ) {
        const species_id victim = static_cast<species_id>( inner.get_string( "species" ) );
        const achievement_comparison compare = inner.get_enum_value<achievement_comparison>( "is" );
        const int count = inner.get_int( "count" );

        species_kill_requirements_[victim] = std::make_pair( compare, count );
    } else {
        const achievement_comparison compare = inner.get_enum_value<achievement_comparison>( "is" );
        const int count = inner.get_int( "count" );

        kill_requirements_[mtype_id::NULL_ID()] = std::make_pair( compare, count );
    }
}

void achievement::add_skill_requirements( const JsonObject &jo, const std::string &src )
{
    if( !jo.has_array( "skill_requirements" ) ) {
        return;
    }

    for( const JsonValue entry : jo.get_array( "skill_requirements" ) ) {
        if( entry.test_object() ) {
            add_skill_requirement( entry, src );
        } else {
            entry.throw_error( "array element is not an object." );
        }
    }
}

void achievement::add_skill_requirement( const JsonObject &inner, const std::string & )
{
    const skill_id skill = static_cast<skill_id>( inner.get_string( "skill" ) );
    const achievement_comparison compare = inner.get_enum_value<achievement_comparison>( "is" );
    const int count = inner.get_int( "level" );

    skill_requirements_[skill] = std::make_pair( compare, count );
}

achievement_completion time_req_completed( const achievement &ach )
{
    if( !ach.time_constraint() ) {
        return achievement_completion::completed;
    }

    const std::optional<achievement::time_bound> &time_req = ach.time_constraint();

    time_point now = calendar::turn;
    switch( time_req->comparison() ) {
        case achievement_comparison::less_equal:
            if( now <= time_req->target() ) {
                return achievement_completion::completed;
            } else {
                return achievement_completion::failed;
            }
        case achievement_comparison::greater_equal:
            if( now >= time_req->target() ) {
                return achievement_completion::completed;
            } else {
                return achievement_completion::pending;
            }
        case achievement_comparison::anything:
            return achievement_completion::completed;
        case achievement_comparison::last:
            break;
    }
    debugmsg( "Invalid achievement_comparison" );
    abort();
}

achievement_completion kill_req_completed( const achievement &ach, const kill_tracker &kt )
{
    const std::map<mtype_id, std::pair<achievement_comparison, int>> &kill_reqs =
                ach.kill_requirements();
    const std::map<species_id, std::pair<achievement_comparison, int>> &species_kill_reqs =
                ach.species_kill_requirements();
    if( kill_reqs.empty() && species_kill_reqs.empty() ) {
        return achievement_completion::completed;
    }

    achievement_completion satisfied = achievement_completion::completed;
    for( const auto& [m_id, pair] : kill_reqs ) {
        if( satisfied == achievement_completion::failed ) {
            break;
        }
        auto& [comp, count] = pair;
        int kill_count = m_id == mtype_id::NULL_ID() ? kt.monster_kill_count() :
                         kt.kill_count( m_id );
        if( !ach_compare( comp, count, kill_count ) ) {
            switch( comp ) {
                case achievement_comparison::greater_equal:
                    // Still might reach target, check for other fail conditions.
                    satisfied = achievement_completion::pending;
                    break;
                case achievement_comparison::less_equal:
                    // Can't reduce kill count.
                    satisfied = achievement_completion::failed;
                    break;
                default:
                    break;
            }
        }
    }
    for( const auto& [fac_id, pair] : species_kill_reqs ) {
        if( satisfied == achievement_completion::failed ) {
            break;
        }
        auto& [comp, count] = pair;
        int kill_count = kt.kill_count( fac_id );
        if( !ach_compare( comp, count, kill_count ) ) {
            switch( comp ) {
                case achievement_comparison::greater_equal:
                    // Still might reach target, check for other fail conditions.
                    satisfied = achievement_completion::pending;
                    break;
                case achievement_comparison::less_equal:
                    // Can't reduce kill count.
                    satisfied = achievement_completion::failed;
                    break;
                default:
                    break;
            }
        }
    }
    return satisfied;
}

achievement_completion skill_req_completed( const achievement &ach )
{
    const std::map<skill_id, std::pair<achievement_comparison, int>> &skill_reqs =
                ach.skill_requirements();
    if( skill_reqs.empty() ) {
        return achievement_completion::completed;
    }
    Character &u = get_player_character();

    achievement_completion satisfied = achievement_completion::completed;
    for( const auto& [sk_id, pair] : skill_reqs ) {
        auto& [comp, level] = pair;
        int skill_level = u.get_skill_level( sk_id );
        if( !ach_compare( comp, level, skill_level ) ) {
            switch( comp ) {
                case achievement_comparison::greater_equal:
                    // Still might reach target, check for other fail conditions.
                    satisfied = achievement_completion::pending;
                    break;
                case achievement_comparison::less_equal:
                    // Skill level can technically be reduced by rust, but should
                    // still fail.
                    satisfied = achievement_completion::failed;
                    break;
                default:
                    break;
            }
        }
    }
    return satisfied;
}

bool ach_compare( const achievement_comparison symbol, const int target, const int to_compare )
{
    switch( symbol ) {
        case achievement_comparison::greater_equal:
            return to_compare >= target;
        case achievement_comparison::less_equal:
            return to_compare <= target;
        case achievement_comparison::anything:
            return true;
        case achievement_comparison::last:
            return true;
        default:
            return false;
    }
}

time_point achievement::time_bound::target() const
{
    return epoch_to_time_point( epoch_ ) + period_;
}

achievement_comparison achievement::time_bound::comparison() const
{
    return comparison_;
}

std::string achievement::ui_text( achievement_completion completion, const kill_tracker &kt ) const
{
    std::string requirements;
    if( time_constraint() ) {
        requirements += "  " + time_constraint()->time_ui_text( time_req_completed( *this ) ) + "\n";
    }

    if( !skill_requirements().empty() ) {
        requirements += skill_ui_text();
    }

    if( !kill_requirements().empty() || !species_kill_requirements().empty() ) {
        requirements += kill_ui_text( completion, kt );
    }

    return requirements;
}

std::string achievement::kill_ui_text( achievement_completion completion,
                                       const kill_tracker &kt ) const
{
    std::string kill_string;
    for( const auto& [m_id, pair] : kill_requirements() ) {
        auto& [comp, count] = pair;
        std::string cur_kills;
        std::string mon_name = m_id == mtype_id::NULL_ID() ? count > 1 ? "monsters" : "monster" :
                               m_id->nname( count );
        int kill_count = 0;
        std::string progress;
        bool comp_pass = false;
        // Don't waste time tallying kill count if the achievement has already failed.
        if( completion != achievement_completion::failed ) {
            kill_count = m_id == mtype_id::NULL_ID() ? kt.monster_kill_count() :
                         kt.kill_count( m_id );
            progress = string_format( ( " (%i/%i)" ), kill_count, count );
            comp_pass = ach_compare( comp, count, kill_count );
        }
        achievement_completion kill_status;

        switch( comp ) {
            case achievement_comparison::greater_equal:
                kill_status = comp_pass ? achievement_completion::completed : achievement_completion::pending;
                cur_kills = string_format( _( "Kill at least %i %s%s" ), count, mon_name, progress );
                break;
            case achievement_comparison::less_equal:
                kill_status = comp_pass ? achievement_completion::completed : achievement_completion::failed;
                if( count == 0 ) {
                    cur_kills = string_format( _( "Don't kill even a single %s" ), mon_name );
                } else {
                    cur_kills = string_format( _( "Kill no more than %i %s%s" ), count, mon_name, progress );
                }
                break;
            case achievement_comparison::anything:
                kill_status = achievement_completion::completed;
                cur_kills = string_format( "Kill any number of %s", mon_name );
                break;
            default:
                kill_status = achievement_completion::completed;
                break;
        }

        if( completion == achievement_completion::failed ) {
            kill_status = completion;
        }

        nc_color c = color_from_completion( kill_status );
        if( kill_status == achievement_completion::failed &&
            completion != achievement_completion::failed ) {
            cur_kills += _( " (failed)" );
        }
        kill_string += "  " + colorize( cur_kills, c ) + "\n";
    }
    for( const auto& [fac_id, pair] : species_kill_requirements() ) {
        auto& [comp, count] = pair;
        std::string cur_kills;
        std::string fac_name = fac_id->name.translated( count );
        int kill_count = 0;
        std::string progress;
        bool comp_pass = false;
        // Don't waste time tallying kill count if the achievement has already failed.
        if( completion != achievement_completion::failed ) {
            kill_count = kt.kill_count( fac_id );
            progress = string_format( ( " (%i/%i)" ), kill_count, count );
            comp_pass = ach_compare( comp, count, kill_count );
        }
        achievement_completion kill_status;

        switch( comp ) {
            case achievement_comparison::greater_equal:
                kill_status = comp_pass ? achievement_completion::completed : achievement_completion::pending;
                cur_kills = string_format( _( "Kill at least %i %s%s" ), count, fac_name, progress );
                break;
            case achievement_comparison::less_equal:
                kill_status = comp_pass ? achievement_completion::completed : achievement_completion::failed;
                if( count == 0 ) {
                    cur_kills = string_format( _( "Don't kill even a single %s" ), fac_name );
                } else {
                    cur_kills = string_format( _( "Kill no more than %i %s%s" ), count, fac_name, progress );
                }
                break;
            case achievement_comparison::anything:
                kill_status = achievement_completion::completed;
                cur_kills = string_format( "Kill any number of %s", fac_name );
                break;
            default:
                kill_status = achievement_completion::completed;
                break;
        }

        if( completion == achievement_completion::failed ) {
            kill_status = completion;
        }

        nc_color c = color_from_completion( kill_status );
        if( kill_status == achievement_completion::failed &&
            completion != achievement_completion::failed ) {
            cur_kills += _( " (failed)" );
        }
        kill_string += "  " + colorize( cur_kills, c ) + "\n";
    }
    return kill_string;
}

std::string achievement::skill_ui_text() const
{
    std::string skill_string;
    Character &u = get_player_character();
    for( const auto& [sk_id, pair] : skill_requirements() ) {
        auto& [comp, level] = pair;
        std::string cur_skill;
        achievement_completion skill_status;
        int sk_lvl = u.get_skill_level( sk_id );
        bool comp_pass = ach_compare( comp, level, sk_lvl );
        switch( comp ) {
            case achievement_comparison::greater_equal:
                skill_status = comp_pass ? achievement_completion::completed : achievement_completion::pending;
                cur_skill = string_format( _( "Attain skill level of %i in %s (%i/%i)" ), level, sk_id->name(),
                                           sk_lvl, level );
                break;
            case achievement_comparison::less_equal:
                skill_status = comp_pass ? achievement_completion::completed : achievement_completion::failed;
                cur_skill = string_format( _( "Stay at or below skill level of %i in %s (%i/%i)" ), level,
                                           sk_id->name(), sk_lvl, level );
                break;
            default:
                skill_status = achievement_completion::completed;
                break;
        }
        nc_color c = color_from_completion( skill_status );
        if( skill_status == achievement_completion::failed ) {
            cur_skill += _( " (failed)" );
        }
        skill_string += "  " + colorize( cur_skill, c ) + "\n";
    }
    return skill_string;
}

std::string achievement::time_bound::time_ui_text( const achievement_completion comp ) const
{

    time_point now = calendar::turn;

    nc_color c = color_from_completion( comp );

    auto translate_epoch = []( epoch e ) {
        switch( e ) {
            case epoch::cataclysm:
                return _( "time of cataclysm" );
            case epoch::game_start:
                return _( "start of game" );
            case epoch::last:
                break;
        }
        debugmsg( "Invalid epoch" );
        abort();
    };

    std::string message = [&]() {
        switch( comp ) {
            case achievement_completion::pending:
                return string_format( _( "At least %s from %s (%s remaining)" ),
                                      to_string( period_ ), translate_epoch( epoch_ ),
                                      to_string( target() - now ) );
            case achievement_completion::completed:
                switch( comparison_ ) {
                    case achievement_comparison::less_equal:
                        return string_format( _( "Within %s of %s (%s remaining)" ),
                                              to_string( period_ ), translate_epoch( epoch_ ),
                                              to_string( target() - now ) );
                    case achievement_comparison::greater_equal:
                        return string_format( _( "At least %s from %s (passed)" ),
                                              to_string( period_ ), translate_epoch( epoch_ ) );
                    case achievement_comparison::anything:
                        return std::string();
                    case achievement_comparison::last:
                        break;
                }
                debugmsg( "Invalid achievement_comparison" );
                abort();
            case achievement_completion::failed:
                return string_format( _( "Within %s of %s (passed)" ),
                                      to_string( period_ ), translate_epoch( epoch_ ) );
            case achievement_completion::last:
                break;
        }
        debugmsg( "Invalid achievement_completion" );
        abort();
    }
    ();

    return colorize( message, c );
}

void achievement::load_achievement( const JsonObject &jo, const std::string &src )
{
    achievement_factory.load( jo, src );
}

void achievement::finalize()
{
    for( achievement &a : const_cast<std::vector<achievement>&>( achievement::get_all() ) ) {
        for( achievement_requirement &req : a.requirements_ ) {
            req.finalize();
        }
    }
}

void achievement::check_consistency()
{
    achievement_factory.check();
}

const std::vector<achievement> &achievement::get_all()
{
    return achievement_factory.get_all();
}

void achievement::reset()
{
    achievement_factory.reset();
}

void achievement::load( const JsonObject &jo, const std::string &src )
{
    mandatory( jo, was_loaded, "name", name_ );
    optional( jo, was_loaded, "description", description_ );
    optional( jo, was_loaded, "hidden_by", hidden_by_ );
    optional( jo, was_loaded, "time_constraint", time_constraint_ );
    mandatory( jo, was_loaded, "requirements", requirements_ );

    if( jo.has_member( "skill_requirements" ) ) {
        add_skill_requirements( jo, src );
    }
    if( jo.has_member( "kill_requirements" ) ) {
        add_kill_requirements( jo, src );
    }
}

void achievement::check() const
{
    for( const string_id<achievement> &a : hidden_by_ ) {
        if( !a.is_valid() ) {
            debugmsg( "Achievement %s specifies hidden_by achievement %s, but the latter does not "
                      "exist.", id.str(), a.str() );
        }
    }
    if( time_constraint_ ) {
        time_constraint_->check( id );
    }
    if( !skill_requirements_.empty() ) {
        for( const auto& [sk_id, pair] : skill_requirements_ ) {
            auto &&[comp, level] = pair;
            if( !sk_id.is_valid() ) {
                debugmsg( "Achievement %s specifies invalid skill requirement %s.", id.str(), sk_id.c_str() );
            } else if( comp == achievement_comparison::anything ) {
                debugmsg( "Achievement %s specifies invalid comparator for skill requirement %s.", id.str(),
                          sk_id.c_str() );
            } else if( level < 0 ) {
                debugmsg( "Achievement %s has negative value for skill requirement %s.", id.str(), sk_id.c_str() );
            }
        }
    }
    if( !kill_requirements_.empty() ) {
        for( const auto& [m_id, pair] : kill_requirements_ ) {
            auto &&[comp, count] = pair;
            if( !m_id.is_valid() ) {
                debugmsg( "Achievement %s specifies invalid kill requirement %s.", id.str(), m_id.c_str() );
            } else if( comp == achievement_comparison::anything ) {
                debugmsg( "Achievement %s specifies invalid comparator for kill requirement %s.", id.str(),
                          m_id.c_str() );
            } else if( count < 0 ) {
                debugmsg( "Achievement %s has negative count for kill requirement %s.", id.str(), m_id.c_str() );
            }
        }
    }
    if( !species_kill_requirements_.empty() ) {
        for( const auto& [fac_id, pair] : species_kill_requirements_ ) {
            auto &&[comp, count] = pair;
            if( !fac_id.is_valid() || fac_id.is_null() ) {
                debugmsg( "Achievement %s specifies invalid kill requirement %s.", id.str(), fac_id.c_str() );
            } else if( comp == achievement_comparison::anything ) {
                debugmsg( "Achievement %s specifies invalid comparator for kill requirement %s.", id.str(),
                          fac_id.c_str() );
            } else if( count < 0 ) {
                debugmsg( "Achievement %s has negative count for kill requirement %s.", id.str(), fac_id.c_str() );
            }
        }
    }
    for( const achievement_requirement &req : requirements_ ) {
        req.check( id );
    }
}

static std::string text_for_requirement( const achievement_requirement &req,
        const cata_variant &current_value )
{
    bool is_satisfied = req.satisifed_by( current_value );
    nc_color c = is_satisfied ? c_green : c_yellow;
    int current = current_value.get<int>();
    int target;
    std::string result;
    if( req.comparison == achievement_comparison::anything ) {
        target = 1;
        result = string_format( _( "Triggered by " ) );
    } else {
        target = req.target;
        result = string_format( _( "%s/%s " ), current, target );
    }
    result += req.statistic->description().translated( target );
    return colorize( result, c );
}

namespace io
{
template<>
std::string enum_to_string<achievement_completion>( achievement_completion data )
{
    switch( data ) {
        // *INDENT-OFF*
        case achievement_completion::pending: return "pending";
        case achievement_completion::completed: return "completed";
        case achievement_completion::failed: return "failed";
        // *INDENT-ON*
        case achievement_completion::last:
            break;
    }
    debugmsg( "Invalid achievement_completion" );
    abort();
}

} // namespace io

std::string achievement_state::ui_text( const achievement *ach, const kill_tracker &kt ) const
{
    // First: the achievement name and description
    nc_color c = color_from_completion( completion );
    std::string result = colorize( ach->name(), c ) + "\n";
    if( !ach->description().empty() ) {
        result += "  " + colorize( ach->description(), c ) + "\n";
    }

    if( completion == achievement_completion::completed ) {
        std::string message = string_format(
                                  _( "Completed %s" ), to_string( last_state_change ) );
        result += "  " + colorize( message, c ) + "\n";
    } else {
        // Next: time constraint, kill requirements & skill requirements if any
        result += ach->ui_text( completion, kt );
    }

    // Next: the requirements
    const std::vector<achievement_requirement> &reqs = ach->requirements();
    // If these two vectors are of different sizes then the definition must
    // have changed since it was complated / failed, so we don't print any
    // requirements info.
    if( final_values.size() == reqs.size() ) {
        for( size_t i = 0; i < final_values.size(); ++i ) {
            result += "  " + text_for_requirement( reqs[i], final_values[i] ) + "\n";
        }
    }

    return result;
}

void achievement_state::serialize( JsonOut &jsout ) const
{
    jsout.start_object();
    jsout.member_as_string( "completion", completion );
    jsout.member( "last_state_change", last_state_change );
    jsout.end_object();
}

void achievement_state::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    jo.read( "completion", completion );
    jo.read( "last_state_change", last_state_change );
}

namespace
{

auto current_value_for_requirement( const achievement_requirement &req,
                                    stats_tracker &stats ) -> cata_variant
{
    return req.statistic->value( stats );
}

auto current_values_for_achievement( const achievement &ach,
                                     stats_tracker &stats ) -> std::vector<cata_variant>
{
    return ach.requirements()
    | std::views::transform( [&]( const achievement_requirement & req ) {
        return current_value_for_requirement( req, stats );
    } )
    | std::ranges::to<std::vector>();
}

auto has_failed_requirement( const achievement &ach, stats_tracker &stats ) -> bool
{
    return std::ranges::any_of( ach.requirements(), [&]( const achievement_requirement & req ) {
        return req.becomes_false && !req.satisifed_by( current_value_for_requirement( req, stats ) );
    } );
}

} // namespace

achievements_tracker::achievements_tracker(
    stats_tracker &stats, kill_tracker &kt,
    const std::function<void( const achievement * )> &achievement_attained_callback ) :
    stats_( &stats ),
    kill_tracker_( &kt ),
    achievement_attained_callback_( achievement_attained_callback ),
    previous_active_tracker_( achievement_runtime::active_tracker() )
{
    achievement_runtime::set_active_tracker( this );
    activate_statistics();
}

achievements_tracker::~achievements_tracker()
{
    if( achievement_runtime::active_tracker() == this ) {
        achievement_runtime::set_active_tracker( previous_active_tracker_ );
    }
}

auto achievements_tracker::stats() -> stats_tracker &
{
    return *stats_;
}

auto achievements_tracker::stats() const -> const stats_tracker &
{
    return *stats_;
}

auto achievements_tracker::kills() const -> const kill_tracker *
{
    return kill_tracker_;
}

auto achievements_tracker::valid_achievements() const -> std::vector<const achievement *>
{
    return achievement::get_all()
    | std::views::transform( []( const achievement & ach ) {
        return &ach;
    } )
    | std::ranges::to<std::vector>();
}

auto achievements_tracker::current_values_for( const achievement &ach ) const ->
std::vector<cata_variant>
{
    return current_values_for_achievement( ach, *stats_ );
}

auto achievements_tracker::activate_statistics() -> void
{
    auto tracked_statistics = achievement::get_all()
    | std::views::transform( []( const achievement & ach ) {
        return ach.requirements()
        | std::views::transform( []( const achievement_requirement & req ) {
            return req.statistic;
        } );
    } )
    | std::views::join;

    for( const string_id<event_statistic> &statistic : tracked_statistics ) {
        stats_->activate_stat( statistic );
    }
}

auto achievements_tracker::has_failed( const achievement &ach ) const -> bool
{
    return time_req_completed( ach ) == achievement_completion::failed ||
           skill_req_completed( ach ) == achievement_completion::failed ||
           kill_req_completed( ach, *kill_tracker_ ) == achievement_completion::failed ||
           has_failed_requirement( ach, *stats_ );
}

auto achievements_tracker::pending_ui_text_for( const achievement &ach ) const -> std::string
{
    if( has_failed( ach ) ) {
        return achievement_state{
            achievement_completion::failed,
            calendar::turn,
            current_values_for( ach )
        }.ui_text( &ach, *kill_tracker_ );
    }

    auto result = colorize( ach.name(),
                            color_from_completion( achievement_completion::pending ) ) + "\n";
    if( !ach.description().empty() ) {
        result += "  " + colorize( ach.description(), c_yellow ) + "\n";
    }

    result += ach.ui_text( achievement_completion::pending, *kill_tracker_ );
    for( const achievement_requirement &req : ach.requirements() ) {
        result += "  " + text_for_requirement( req, current_value_for_requirement( req, *stats_ ) ) + "\n";
    }

    return result;
}

auto achievements_tracker::report_achievement( const achievement *a,
        achievement_completion comp ) -> void
{
    if( comp == achievement_completion::pending || achievements_status_.contains( a->id ) ) {
        return;
    }

    achievements_status_.emplace(
        a->id,
    achievement_state{
        comp,
        calendar::turn,
        current_values_for( *a )
    }
    );

    if( comp == achievement_completion::completed ) {
        achievement_attained_callback_( a );
    }
}

auto achievements_tracker::report_achievement( const string_id<achievement> &id,
        achievement_completion comp ) -> void
{
    if( !id.is_valid() ) {
        return;
    }

    report_achievement( &id.obj(), comp );
}

auto achievements_tracker::recorded_completion( const string_id<achievement> &id ) const ->
achievement_completion
{
    const auto iter = achievements_status_.find( id );
    return iter == achievements_status_.end() ? achievement_completion::pending :
           iter->second.completion;
}

auto achievements_tracker::is_completed( const string_id<achievement> &id ) const ->
achievement_completion
{
    const auto recorded = recorded_completion( id );
    if( recorded != achievement_completion::pending ) {
        return recorded;
    }

    if( id.is_valid() && has_failed( id.obj() ) ) {
        return achievement_completion::failed;
    }

    return achievement_completion::pending;
}

auto achievements_tracker::is_hidden( const achievement *ach ) const -> bool
{
    if( is_completed( ach->id ) == achievement_completion::completed ) {
        return false;
    }

    for( const string_id<achievement> &hidden_by : ach->hidden_by() ) {
        if( is_completed( hidden_by ) != achievement_completion::completed ) {
            return true;
        }
    }

    return false;
}

auto achievements_tracker::ui_text_for( const achievement *ach ) const -> std::string
{
    const auto iter = achievements_status_.find( ach->id );
    if( iter != achievements_status_.end() ) {
        return iter->second.ui_text( ach, *kill_tracker_ );
    }

    return pending_ui_text_for( *ach );
}

auto achievements_tracker::clear() -> void
{
    achievements_status_.clear();
    activate_statistics();
}

auto achievements_tracker::serialize( JsonOut &jsout ) const -> void
{
    jsout.start_object();
    jsout.member( "achievements_status", achievements_status_ );
    jsout.end_object();
}

auto achievements_tracker::deserialize( JsonIn &jsin ) -> void
{
    JsonObject jo = jsin.get_object();
    jo.read( "achievements_status", achievements_status_ );
    activate_statistics();
}

void achievement_requirement::deserialize( JsonIn &jin )
{
    const JsonObject &jo = jin.get_object();
    if( !jo.read( "event_statistic", statistic ) ||
        !jo.read( "is", comparison ) ||
        ( comparison != achievement_comparison::anything &&
          !jo.read( "target", target ) ) ) {
        jo.throw_error( "Mandatory field missing for achievement requirement" );
    }
}

void achievement_requirement::finalize()
{
    switch( comparison ) {
        case achievement_comparison::less_equal:
            becomes_false = is_increasing( statistic->monotonicity() );
            return;
        case achievement_comparison::greater_equal:
            becomes_false = is_decreasing( statistic->monotonicity() );
            return;
        case achievement_comparison::anything:
            becomes_false = true;
            return;
        case achievement_comparison::last:
            break;
    }
    debugmsg( "Invalid achievement_comparison" );
    abort();
}

void achievement_requirement::check( const string_id<achievement> &id ) const
{
    if( !statistic.is_valid() ) {
        debugmsg( "score %s refers to invalid statistic %s", id.str(), statistic.str() );
    }
}

bool achievement_requirement::satisifed_by( const cata_variant &v ) const
{
    int value = v.get<int>();
    switch( comparison ) {
        case achievement_comparison::less_equal:
            return value <= target;
        case achievement_comparison::greater_equal:
            return value >= target;
        case achievement_comparison::anything:
            return true;
        case achievement_comparison::last:
            break;
    }
    debugmsg( "Invalid achievement_requirement comparison value" );
    abort();
}
