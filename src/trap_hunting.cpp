#include "trap_hunting.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <ranges>

#include "debug.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/submap.h"
#include "mongroup.h"
#include "mtype.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "rng.h"
#include "trap.h"
#include "translations.h"
#include "type_id_implement.h"

namespace
{

generic_factory<hunting_habitat> habitat_factory( "hunting_habitat" );
generic_factory<hunting_bait> bait_factory( "hunting_bait" );

} // namespace

// Specializations must precede the helpers below, which instantiate obj().
IMPLEMENT_STRING_AND_INT_IDS( hunting_habitat, habitat_factory )
IMPLEMENT_STRING_AND_INT_IDS( hunting_bait, bait_factory )

namespace
{

auto habitat_by_land_use = std::map<overmap_land_use_code_id, hunting_habitat_id> {};
auto bait_by_item = std::map<itype_id, hunting_bait_id> {};

const auto var_bait_pos = std::string( "hunting_bait_pos" );
const auto var_bait_checked = std::string( "hunting_bait_checked" );
const auto var_bait_skill = std::string( "hunting_bait_skill" );
const auto var_bait_cover = std::string( "hunting_bait_cover" );

/// Tiles scanned around the trap when judging local cover.
constexpr auto cover_scan_radius = 6;
/// Sites with less cover than this draw no wildlife at all.
constexpr auto min_cover = 0.1;
/// Within this many tiles an observer scares all prey away.
constexpr auto presence_near = 6;
/// Beyond this many tiles an observer no longer matters.
constexpr auto presence_far = 30;

struct habitat_share {
    hunting_habitat_id habitat;
    /// Neighbourhood weight times the habitat's catch rate.
    double weight = 0.0;
};

struct habitat_pool {
    std::vector<habitat_share> shares;
    /// Expected catches per day, averaged over the neighbourhood.
    double rate = 0.0;
};

/// Habitats on the overmap tile at @p center and its eight neighbours; the center counts double.
auto habitat_pool_at( const dimension_id &dim, const tripoint_abs_omt &center ) -> habitat_pool
{
    auto &omb = get_overmapbuffer( dim );
    auto pool = habitat_pool{};
    auto neighbourhood_weight = 0.0;
    for( auto dy = -1; dy <= 1; dy++ ) {
        for( auto dx = -1; dx <= 1; dx++ ) {
            const auto weight = dx == 0 && dy == 0 ? 2.0 : 1.0;
            neighbourhood_weight += weight;
            const auto &ter = omb.ter( center + tripoint_rel_omt( dx, dy, 0 ) );
            const auto habitat = hunting_habitat::for_land_use( ter->get_land_use_code() );
            if( !habitat ) {
                continue;
            }
            const auto share_weight = weight * habitat->obj().catch_rate;
            const auto existing = std::ranges::find( pool.shares, *habitat, &habitat_share::habitat );
            if( existing != pool.shares.end() ) {
                existing->weight += share_weight;
            } else {
                pool.shares.push_back( { .habitat = *habitat, .weight = share_weight } );
            }
        }
    }
    const auto weights = pool.shares | std::views::transform( &habitat_share::weight );
    pool.rate = std::ranges::fold_left( weights, 0.0, std::plus<> {} ) / neighbourhood_weight;
    return pool;
}

auto season_name( season_type season ) -> std::string
{
    switch( season ) {
        case SPRING:
            return "SPRING";
        case SUMMER:
            return "SUMMER";
        case AUTUMN:
            return "AUTUMN";
        case WINTER:
            return "WINTER";
        default:
            return "";
    }
}

/// Whether a monster group entry is active at @p when. Time of day conditions are ignored
/// because a catch is resolved over a span of time rather than at a single moment.
auto entry_active_at( const MonsterGroupEntry &entry, const time_point &when ) -> bool
{
    if( calendar::start_of_cataclysm + entry.starts > when ) {
        return false;
    }
    if( !entry.lasts_forever() && calendar::start_of_cataclysm + entry.ends < when ) {
        return false;
    }
    const auto is_season = []( const std::string & cond ) {
        return cond == "SPRING" || cond == "SUMMER" || cond == "AUTUMN" || cond == "WINTER";
    };
    const auto current = season_name( season_of_year( when ) );
    const auto has_season_limit = std::ranges::any_of( entry.conditions, is_season );
    return !has_season_limit || std::ranges::contains( entry.conditions, current );
}

struct prey_candidates_options {
    const habitat_pool &pool;
    const trap_hunting_data &trap;
    const hunting_bait &bait;
    time_point when;
};

struct prey_candidate {
    mtype_id prey;
    /// Share of the habitat pool's weight, see @ref habitat_share::weight.
    double weight = 0.0;
};

/// Prey active at @p opts.when that the trap can hold and the bait lures. Unsuitable prey is left
/// out without renormalizing, so a poor trap or bait catches less instead of catching other prey.
auto prey_candidates( const prey_candidates_options &opts ) -> std::vector<prey_candidate>
{
    auto candidates = std::vector<prey_candidate> {};
    for( const auto &share : opts.pool.shares ) {
        const auto &group = MonsterGroupManager::GetMonsterGroup( share.habitat->prey_group );
        const auto frequencies = group.monsters | std::views::transform( &MonsterGroupEntry::frequency );
        const auto group_total = std::ranges::fold_left( frequencies, 0, std::plus<> {} );
        if( group_total <= 0 ) {
            continue;
        }
        for( const auto &entry : group.monsters ) {
            if( !entry.name.is_valid() || entry.name.is_null() || !entry_active_at( entry, opts.when ) ) {
                continue;
            }
            const auto &prey = entry.name.obj();
            if( !opts.trap.can_hold( prey.size ) || !opts.bait.lures( prey ) ) {
                continue;
            }
            const auto weight = share.weight * entry.frequency / group_total;
            candidates.push_back( { .prey = entry.name, .weight = weight } );
        }
    }
    return candidates;
}

auto total_weight( const std::vector<prey_candidate> &candidates ) -> double
{
    const auto weights = candidates | std::views::transform( &prey_candidate::weight );
    return std::ranges::fold_left( weights, 0.0, std::plus<> {} );
}

auto pick_candidate( const std::vector<prey_candidate> &candidates ) -> std::optional<mtype_id>
{
    auto roll = rng_float( 0.0, total_weight( candidates ) );
    for( const auto &candidate : candidates ) {
        if( roll < candidate.weight ) {
            return candidate.prey;
        }
        roll -= candidate.weight;
    }
    return candidates.empty() ? std::nullopt : std::optional( candidates.back().prey );
}

/// Start of the season after the one containing @p t.
auto next_season_start( const time_point &t ) -> time_point
{
    const auto seasons_passed = std::floor( ( t - calendar::turn_zero ) / calendar::season_length() );
    return calendar::turn_zero + calendar::season_length() * ( static_cast<int>( seasons_passed ) + 1 );
}

struct catch_result {
    mtype_id prey;
    time_point caught_at;
};

struct roll_catch_options {
    const habitat_pool &pool;
    const trap_hunting_data &trap;
    const hunting_bait &bait;
    /// Catch rate multiplier from everything but the habitat.
    double multiplier = 0.0;
    time_point from;
    time_point to;
};

/// Roll for the first catch in [from, to). Suitable prey arrive as a Poisson process thinned by
/// trap size and bait preference; seasons change the prey mix, so each season is rolled in turn.
auto roll_catch( const roll_catch_options &opts ) -> std::optional<catch_result>
{
    const auto pool_total = std::ranges::fold_left( opts.pool.shares
                            | std::views::transform( &habitat_share::weight ), 0.0, std::plus<> {} );
    if( pool_total <= 0.0 || opts.multiplier <= 0.0 ) {
        return std::nullopt;
    }
    auto segment_start = opts.from;
    while( segment_start < opts.to ) {
        const auto segment_end = std::min( opts.to, next_season_start( segment_start ) );
        const auto candidates = prey_candidates( {
            .pool = opts.pool,
            .trap = opts.trap,
            .bait = opts.bait,
            .when = segment_start,
        } );
        // pool.rate covers all prey in the neighbourhood; keep only the suitable share of it.
        const auto suitable = total_weight( candidates ) / pool_total;
        const auto rate = opts.pool.rate * suitable * opts.multiplier;
        const auto delay = trap_hunting::catch_delay( {
            .rate_per_day = rate,
            .exposure = segment_end - segment_start,
            .roll = rng_float( 0.0, 1.0 ),
        } );
        if( delay ) {
            const auto prey = pick_candidate( candidates );
            if( prey ) {
                return catch_result{ .prey = *prey, .caught_at = segment_start + *delay };
            }
        }
        segment_start = segment_end;
    }
    return std::nullopt;
}

auto bait_last_checked( const item &bait ) -> time_point
{
    return calendar::turn_zero + time_duration::from_turns( bait.get_var( var_bait_checked, 0 ) );
}

/// When @p bait, last checked at @p from, stops luring prey because it spoiled.
auto bait_fresh_until( const item &bait, const time_point &from ) -> std::optional<time_point>
{
    if( !bait.goes_bad() ) {
        return std::nullopt;
    }
    const auto remaining = std::max( 0.0, 1.0 - bait.get_relative_rot() );
    const auto shelf_turns = to_turns<double>( bait.get_shelf_life() );
    return from + time_duration::from_turns( std::lround( shelf_turns * remaining ) );
}

struct spring_trap_options {
    mapbuffer &buffer;
    tripoint_abs_ms pos;
    const trap &sprung;
    time_point when;
    mapbuffer_lookup_options lookup;
};

/// Prey set off a trap that does not survive triggering: remove it and leave what it was built
/// from, as a disarm would.
auto spring_trap( const spring_trap_options &opts ) -> void
{
    for( const auto &[type, quantity, charges] : opts.sprung.get_drops() ) {
        for( auto i = 0; i < quantity; i++ ) {
            auto drop = item::spawn( type, opts.when );
            if( drop->count_by_charges() ) {
                drop->charges = charges;
            }
            opts.buffer.add_item( opts.pos, std::move( drop ), opts.lookup );
        }
    }
    opts.buffer.set_trap( opts.pos, tr_null, opts.lookup );
}

} // namespace

auto trap_hunting_data::load( const JsonObject &jo ) -> void
{
    min_size = jo.get_enum_value<creature_size>( "min_size", min_size );
    max_size = jo.get_enum_value<creature_size>( "max_size", max_size );
    efficiency = jo.get_float( "efficiency", efficiency );
}

auto hunting_habitat::load( const JsonObject &jo, const std::string & ) -> void
{
    mandatory( jo, was_loaded, "land_use_codes", land_use_codes,
               string_id_reader<overmap_land_use_code> {} );
    mandatory( jo, was_loaded, "prey_group", prey_group );
    mandatory( jo, was_loaded, "catch_rate", catch_rate );
}

auto hunting_habitat::check() const -> void
{
    std::ranges::for_each( land_use_codes, [&]( const auto & code ) {
        if( !code.is_valid() ) {
            debugmsg( "hunting_habitat %s has unknown land use code %s", id.str(), code.str() );
        }
    } );
    if( !MonsterGroupManager::isValidMonsterGroup( prey_group ) ) {
        debugmsg( "hunting_habitat %s has unknown prey group %s", id.str(), prey_group.str() );
    }
    if( catch_rate < 0.0 ) {
        debugmsg( "hunting_habitat %s has negative catch_rate %g", id.str(), catch_rate );
    }
}

auto hunting_habitat::load_hunting_habitat( const JsonObject &jo, const std::string &src ) -> void
{
    habitat_factory.load( jo, src );
}

auto hunting_habitat::finalize() -> void
{
    habitat_by_land_use.clear();
    for( const auto &habitat : habitat_factory.get_all() ) {
        for( const auto &code : habitat.land_use_codes ) {
            const auto [existing, inserted] = habitat_by_land_use.emplace( code, habitat.id );
            if( !inserted ) {
                debugmsg( "land use code %s is claimed by both hunting_habitat %s and %s", code.str(),
                          existing->second.str(), habitat.id.str() );
            }
        }
    }
}

auto hunting_habitat::check_consistency() -> void
{
    habitat_factory.check();
}

auto hunting_habitat::reset() -> void
{
    habitat_factory.reset();
    habitat_by_land_use.clear();
}

auto hunting_habitat::get_all() -> const std::vector<hunting_habitat> &
{
    return habitat_factory.get_all();
}

auto hunting_habitat::for_land_use( const overmap_land_use_code_id &code ) ->
std::optional<hunting_habitat_id>
{
    const auto found = habitat_by_land_use.find( code );
    if( found == habitat_by_land_use.end() ) {
        return std::nullopt;
    }
    return found->second;
}

auto hunting_bait::load( const JsonObject &jo, const std::string & ) -> void
{
    mandatory( jo, was_loaded, "items", items, string_id_reader<itype> {} );
    optional( jo, was_loaded, "attraction", attraction, 1.0 );
    optional( jo, was_loaded, "species", species, string_id_reader<species_type> {} );
    optional( jo, was_loaded, "monsters", monsters, string_id_reader<mtype> {} );
}

auto hunting_bait::check() const -> void
{
    std::ranges::for_each( items, [&]( const auto & type ) {
        if( !type.is_valid() ) {
            debugmsg( "hunting_bait %s has unknown item %s", id.str(), type.str() );
        }
    } );
    std::ranges::for_each( species, [&]( const auto & spec ) {
        if( !spec.is_valid() ) {
            debugmsg( "hunting_bait %s has unknown species %s", id.str(), spec.str() );
        }
    } );
    std::ranges::for_each( monsters, [&]( const auto & mon ) {
        if( !mon.is_valid() ) {
            debugmsg( "hunting_bait %s has unknown monster %s", id.str(), mon.str() );
        }
    } );
    if( attraction <= 0.0 ) {
        debugmsg( "hunting_bait %s has non-positive attraction %g", id.str(), attraction );
    }
}

auto hunting_bait::lures( const mtype &prey ) const -> bool
{
    if( species.empty() && monsters.empty() ) {
        return true;
    }
    return monsters.contains( prey.id ) || std::ranges::any_of( species, [&]( const auto & spec ) {
        return prey.in_species( spec );
    } );
}

auto hunting_bait::load_hunting_bait( const JsonObject &jo, const std::string &src ) -> void
{
    bait_factory.load( jo, src );
}

auto hunting_bait::finalize() -> void
{
    bait_by_item.clear();
    for( const auto &bait : bait_factory.get_all() ) {
        for( const auto &type : bait.items ) {
            const auto [existing, inserted] = bait_by_item.emplace( type, bait.id );
            if( !inserted ) {
                debugmsg( "item %s is listed by both hunting_bait %s and %s", type.str(),
                          existing->second.str(), bait.id.str() );
            }
        }
    }
}

auto hunting_bait::check_consistency() -> void
{
    bait_factory.check();
}

auto hunting_bait::reset() -> void
{
    bait_factory.reset();
    bait_by_item.clear();
}

auto hunting_bait::get_all() -> const std::vector<hunting_bait> &
{
    return bait_factory.get_all();
}

auto hunting_bait::for_item( const itype_id &type ) -> std::optional<hunting_bait_id>
{
    const auto found = bait_by_item.find( type );
    if( found == bait_by_item.end() ) {
        return std::nullopt;
    }
    return found->second;
}

namespace trap_hunting
{

auto evaluate_site( const map &here,
                    const tripoint_bub_ms &pos ) -> std::expected<site_quality, std::string>
{
    if( pos.z() < 0 ) {
        return std::unexpected( _( "No wild animal would wander this far underground." ) );
    }
    if( !here.is_outside( pos ) ) {
        return std::unexpected( _( "No wild animal would wander under a roof." ) );
    }
    const auto pool = habitat_pool_at( here.get_bound_dimension(),
                                       project_to<coords::omt>( map_local_to_abs( here, pos ) ) );
    if( pool.rate <= 0.0 ) {
        return std::unexpected( _( "There is no wildlife around here worth baiting for." ) );
    }

    const auto is_natural = [&]( const tripoint_bub_ms & p ) {
        return here.has_flag( "PLOWABLE", p ) || here.has_flag( "SHRUB", p ) ||
               here.has_flag( "TREE", p ) || here.has_flag( "YOUNG", p ) || here.has_flag( "FLOWER", p );
    };
    const auto is_cover = [&]( const tripoint_bub_ms & p ) {
        return here.has_flag( "SHRUB", p ) || here.has_flag( "TREE", p ) || here.has_flag( "YOUNG", p );
    };
    const auto area = here.points_in_radius( pos, cover_scan_radius );
    const auto total = static_cast<double>( std::ranges::distance( area ) );
    const auto natural = static_cast<double>( std::ranges::count_if( area, is_natural ) );
    const auto sheltered = static_cast<double>( std::ranges::count_if( area, is_cover ) );
    const auto cover = total <= 0.0 ? 0.0 :
                       natural / total * ( 0.6 + 0.4 * std::min( 1.0, sheltered / total * 4.0 ) );
    if( cover < min_cover ) {
        return std::unexpected( _( "The ground here is too barren to draw any wildlife." ) );
    }
    return site_quality{ .habitat_rate = pool.rate, .cover = cover };
}

auto mark_as_bait( item &it, const bait_mark_options &opts ) -> void
{
    it.set_var( var_bait_pos, opts.pos );
    it.set_var( var_bait_checked, to_turn<int>( opts.placed_at ) );
    it.set_var( var_bait_skill, opts.trapper_skill );
    it.set_var( var_bait_cover, opts.cover );
}

auto release_bait( item &it ) -> void
{
    it.erase_var( var_bait_pos );
    it.erase_var( var_bait_checked );
    it.erase_var( var_bait_skill );
    it.erase_var( var_bait_cover );
}

auto has_bait_tags( const item &it ) -> bool
{
    return it.has_var( var_bait_pos );
}

auto is_bait_for( const item &it, const tripoint_abs_ms &pos ) -> bool
{
    return has_bait_tags( it ) && it.get_var( var_bait_pos, tripoint_abs_ms::zero() ) == pos &&
           hunting_bait::for_item( it.typeId() ).has_value();
}

auto find_bait( map &here, const tripoint_bub_ms &pos ) -> item *
{
    const auto abs_pos = map_local_to_abs( here, pos );
    for( auto *const it : here.i_at( pos ) ) {
        if( is_bait_for( *it, abs_pos ) ) {
            return it;
        }
    }
    return nullptr;
}

auto presence_factor( int distance ) -> double
{
    const auto span = static_cast<double>( presence_far - presence_near );
    return std::clamp( ( distance - presence_near ) / span, 0.0, 1.0 );
}

auto skill_factor( int trapper_skill ) -> double
{
    return std::clamp( 0.75 + 0.05 * trapper_skill, 0.75, 1.5 );
}

auto catch_chance( double rate_per_day, time_duration exposure ) -> double
{
    if( rate_per_day <= 0.0 || exposure <= 0_turns ) {
        return 0.0;
    }
    return -std::expm1( -rate_per_day * to_days<double>( exposure ) );
}

auto catch_delay( const catch_roll_options &opts ) -> std::optional<time_duration>
{
    if( opts.roll >= catch_chance( opts.rate_per_day, opts.exposure ) ) {
        return std::nullopt;
    }
    // Inverse of the exponential CDF; roll < chance guarantees the delay fits in the exposure.
    const auto delay_days = -std::log1p( -opts.roll ) / opts.rate_per_day;
    const auto delay = time_duration::from_turns( std::lround( delay_days * to_turns<double>
                       ( 1_days ) ) );
    return std::min( delay, opts.exposure );
}

auto resolve( const resolve_options &opts ) -> std::optional<mtype_id>
{
    const auto lookup = mapbuffer_lookup_options{ .mode = opts.lookup_mode };
    const auto trap_here = opts.buffer.get_trap( opts.pos, lookup );
    if( !trap_here || !trap_here->obj().hunting ) {
        return std::nullopt;
    }
    const auto &hunting = *trap_here->obj().hunting;
    auto *const items = opts.buffer.get_items( opts.pos, lookup );
    if( items == nullptr ) {
        return std::nullopt;
    }
    const auto bait_it = std::ranges::find_if( *items, [&]( const item * it ) {
        return is_bait_for( *it, opts.pos );
    } );
    if( bait_it == items->end() ) {
        return std::nullopt;
    }
    auto &bait = **bait_it;
    const auto &bait_type = hunting_bait::for_item( bait.typeId() )->obj();

    const auto last_checked = bait_last_checked( bait );
    const auto fresh_until = bait_fresh_until( bait, last_checked );
    const auto exposure_end = fresh_until ? std::min( opts.now, *fresh_until ) : opts.now;
    bait.set_var( var_bait_checked, to_turn<int>( opts.now ) );
    if( exposure_end <= last_checked ) {
        return std::nullopt;
    }

    const auto pool = habitat_pool_at( opts.buffer.get_dimension_id(),
                                       project_to<coords::omt>( opts.pos ) );
    const auto cover = bait.get_var( var_bait_cover, 0.0 );
    const auto skill = skill_factor( bait.get_var( var_bait_skill, 0 ) );
    const auto multiplier = bait_type.attraction * hunting.efficiency * cover * skill * opts.presence;
    const auto caught = roll_catch( {
        .pool = pool,
        .trap = hunting,
        .bait = bait_type,
        .multiplier = multiplier,
        .from = last_checked,
        .to = exposure_end,
    } );
    if( !caught ) {
        return std::nullopt;
    }

    opts.buffer.erase_item( opts.pos, { .it = bait_it, .lookup = lookup } );
    opts.buffer.add_item( opts.pos, item::make_corpse( caught->prey, caught->caught_at ), lookup );
    const auto &sprung = trap_here->obj();
    if( sprung.remove_trap_when_triggered() ) {
        spring_trap( {
            .buffer = opts.buffer,
            .pos = opts.pos,
            .sprung = sprung,
            .when = caught->caught_at,
            .lookup = lookup,
        } );
    }
    return caught->prey;
}

auto process_simulated( mapbuffer &buffer, const tripoint_abs_ms &observer ) -> void
{
    auto baited = std::vector<tripoint_abs_ms> {};
    buffer.for_each_simulated_submap( [&]( const tripoint_abs_sm & sm_pos, submap & sm ) {
        for( const auto &local : sm.trap_cache ) {
            if( sm.get_trap( local ).obj().hunting ) {
                baited.push_back( project_combine( sm_pos, local ) );
            }
        }
    } );
    for( const auto &pos : baited ) {
        resolve( {
            .buffer = buffer,
            .pos = pos,
            .now = calendar::turn,
            .presence = presence_factor( rl_dist( observer, pos ) ),
            .lookup_mode = mapbuffer_lookup_mode::simulated_only,
        } );
    }
}

auto release_bait_at( mapbuffer &buffer, const tripoint_abs_ms &pos,
                      mapbuffer_lookup_mode mode ) -> void
{
    auto *const items = buffer.get_items( pos, { .mode = mode } );
    if( items == nullptr ) {
        return;
    }
    std::ranges::for_each( *items | std::views::filter( []( const item * it ) {
        return has_bait_tags( *it );
    } ), []( item * it ) { release_bait( *it ); } );
}

} // namespace trap_hunting
