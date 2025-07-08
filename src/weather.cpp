#include "weather.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "assign.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_utility.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "item_contents.h"
#include "map.h"
#include "math_defines.h"
#include "messages.h"
#include "options.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "point.h"
#include "point_float.h"
#include "regional_settings.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "units.h"
#include "units_temperature.h"
#include "vpart_position.h"
#include "weather_gen.h"
#include "profile.h"

static const activity_id ACT_WAIT_WEATHER( "ACT_WAIT_WEATHER" );

static const bionic_id bio_sunglasses( "bio_sunglasses" );

static const efftype_id effect_glare( "glare" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_snow_glare( "snow_glare" );

static const itype_id itype_water( "water" );
static const itype_id itype_water_acid( "water_acid" );
static const itype_id itype_water_acid_weak( "water_acid_weak" );

static const trait_id trait_CEPH_VISION( "CEPH_VISION" );
static const trait_id trait_FEATHERS( "FEATHERS" );

static const flag_id json_flag_RAIN_PROTECT( "RAIN_PROTECT" );
static const flag_id json_flag_RAINPROOF( "RAINPROOF" );
static const flag_id json_flag_SUN_GLASSES( "SUN_GLASSES" );

/**
 * \defgroup Weather "Weather and its implications."
 * @{
 */

weather_manager &get_weather()
{
    return *g->weather_manager_ptr;
}

static bool is_player_outside()
{
    return get_map().is_outside( point( get_player_character().posx(),
                                        get_player_character().posy() ) ) && g->get_levz() >= 0;
}

void glare( const weather_type_id &w )
{
    //General prepequisites for glare
    if( !is_player_outside() || !g->is_in_sunlight( g->u.pos() ) || g->u.in_sleep_state() ||
        g->u.worn_with_flag( json_flag_SUN_GLASSES ) ||
        g->u.has_bionic( bio_sunglasses ) ||
        g->u.is_blind() ) {
        return;
    }

    time_duration dur = 0_turns;
    const efftype_id *effect = nullptr;
    season_type season = season_of_year( calendar::turn );
    if( season == WINTER ) {
        //Winter snow glare: for both clear & sunny weather
        effect = &effect_snow_glare;
        dur = g->u.has_effect( *effect ) ? 1_turns : 2_turns;
    } else if( w->sun_intensity == sun_intensity_type::high ) {
        //Sun glare: only for bright sunny weather
        effect = &effect_glare;
        dur = g->u.has_effect( *effect ) ? 1_turns : 2_turns;
    }
    //apply final glare effect
    if( dur > 0_turns && effect != nullptr ) {
        //enhance/reduce by some traits
        if( g->u.has_trait( trait_CEPH_VISION ) ) {
            dur = dur * 2;
        }
        g->u.add_env_effect( *effect, body_part_eyes, 2, dur );
    }
}

int incident_sunlight( const weather_type_id &wtype, const time_point &t )
{
    return std::max<float>( 0.0f, sunlight( t, false ) + wtype->light_modifier );
}

inline void proc_weather_sum( const weather_type_id wtype, weather_sum &data,
                              const time_point &t, const time_duration &tick_size )
{
    int amount = 0;
    if( wtype->rains ) {
        switch( wtype->precip ) {
            case precip_class::very_light:
                amount = 1 * to_turns<int>( tick_size );
                break;
            case precip_class::light:
                amount = 4 * to_turns<int>( tick_size );
                break;
            case precip_class::medium:
                amount = 6 * to_turns<int>( tick_size );
                break;
            case precip_class::heavy:
                amount = 8 * to_turns<int>( tick_size );
                break;
            default:
                break;
        }
    }
    if( wtype->acidic ) {
        data.acid_amount += amount;
    } else {
        data.rain_amount += amount;
    }


    // TODO: Change this sunlight "sampling" here into a proper interpolation
    const float tick_sunlight = incident_sunlight( wtype, t );
    data.sunlight += tick_sunlight * to_turns<int>( tick_size );
}

const weather_type_id &current_weather( const tripoint &location, const time_point &t )
{
    const weather_manager &weather = get_weather();
    const auto wgen = weather.get_cur_weather_gen();
    if( weather.weather_override ) {
        return weather.weather_override;
    }
    return wgen.get_weather_conditions( location, t, g->get_seed() );
}

weather_sum sum_conditions( const time_point &start, const time_point &end,
                            const tripoint &location )
{
    time_duration tick_size = 0_turns;
    weather_sum data;

    for( time_point t = start; t < end; t += tick_size ) {
        const time_duration diff = end - t;
        if( diff < 10_turns ) {
            tick_size = 1_turns;
        } else if( diff > 7_days ) {
            tick_size = 1_hours;
        } else {
            tick_size = 1_minutes;
        }

        weather_type_id wtype = current_weather( location, t );
        proc_weather_sum( wtype, data, t, tick_size );
        const weather_manager &weather = get_weather();
        data.wind_amount += get_local_windpower( weather.windspeed,
                            // TODO: fix point types
                            overmap_buffer.ter( tripoint_abs_omt( ms_to_omt_copy( location ) ) ),
                            location,
                            weather.winddirection, false ) * to_turns<int>( tick_size );
    }
    return data;
}

/**
 * Determine what a funnel has filled out of game, using funnelcontainer.bday as a starting point.
 */
void retroactively_fill_from_funnel( item &it, const trap &tr, const time_point &start,
                                     const time_point &end, const tripoint &pos )
{
    if( start > end || !tr.is_funnel() ) {
        return;
    }

    // bday == last fill check
    it.set_birthday( end );
    weather_sum data = sum_conditions( start, end, pos );

    // Technically 0.0 division is OK, but it will be cleaner without it
    if( data.rain_amount > 0 ) {
        const int rain = roll_remainder( 1.0 / tr.funnel_turns_per_charge( data.rain_amount ) );
        it.add_rain_to_container( false, rain );
        // add_msg(m_debug, "Retroactively adding %d water from turn %d to %d", rain, startturn, endturn);
    }

    if( data.acid_amount > 0 ) {
        const int acid = roll_remainder( 1.0 / tr.funnel_turns_per_charge( data.acid_amount ) );
        it.add_rain_to_container( true, acid );
    }
}

/**
 * Add charge(s) of rain to given container, possibly contaminating it.
 */
void item::add_rain_to_container( bool acid, int charges )
{
    if( charges <= 0 ) {
        return;
    }
    detached_ptr<item> ret = item::spawn( acid ? itype_water_acid : itype_water, calendar::turn );
    const int capa = get_remaining_capacity_for_liquid( *ret, true );
    if( contents.empty() ) {
        // This is easy. Just add 1 charge of the rain liquid to the container.
        if( !acid ) {
            // Funnels aren't always clean enough for water. // TODO: disinfectant squeegie->funnel
            ret->poison = one_in( 10 ) ? 1 : 0;
        }
        ret->charges = std::min( charges, capa );
        put_in( std::move( ret ) );
    } else {
        // The container already has a liquid.
        item &liq = contents.front();
        int orig = liq.charges;
        int added = std::min( charges, capa );
        if( capa > 0 ) {
            liq.charges += added;
        }

        if( liq.typeId() == ret->typeId() || liq.typeId() == itype_water_acid_weak ) {
            // The container already contains this liquid or weakly acidic water.
            // Don't do anything special -- we already added liquid.
        } else {
            // The rain is different from what's in the container.
            // Turn the container's liquid into weak acid with a probability
            // based on its current volume.

            // If it's raining acid and this container started with 7
            // charges of water, the liquid will now be 1/8th acid or,
            // equivalently, 1/4th weak acid (the rest being water). A
            // stochastic approach gives the liquid a 1 in 4 (or 2 in
            // liquid.charges) chance of becoming weak acid.
            const bool transmute = x_in_y( 2 * added, liq.charges );

            if( transmute ) {
                contents.front().convert( itype_water_acid_weak );
            } else if( liq.typeId() == itype_water ) {
                // The container has water, and the acid rain didn't turn it
                // into weak acid. Poison the water instead, assuming 1
                // charge of acid would act like a charge of water with poison 5.
                int total_poison = liq.poison * orig + 5 * added;
                liq.poison = total_poison / liq.charges;
                int leftover_poison = total_poison - liq.poison * liq.charges;
                if( leftover_poison > rng( 0, liq.charges ) ) {
                    liq.poison++;
                }
            }
        }
    }
}

double funnel_charges_per_turn( const double surface_area_mm2, const double rain_depth_mm_per_hour )
{
    // 1mm rain on 1m^2 == 1 liter water == 1000ml
    // 1 liter == 4 volume
    // 1 volume == 250ml: containers
    // 1 volume == 200ml: water
    // How many charges of water can we collect in a turn (usually <1.0)?
    if( rain_depth_mm_per_hour == 0.0 ) {
        return 0.0;
    }

    // Calculate once, because that part is expensive
    // FIXME: make non-static
    //TODO!: yeah... push up
    item &water = *item::spawn_temporary( itype_water, calendar::start_of_cataclysm );
    // 250ml
    static const double charge_ml = static_cast<double>( to_gram( water.weight() ) ) /
                                    water.charges;

    const double vol_mm3_per_hour = surface_area_mm2 * rain_depth_mm_per_hour;
    const double vol_mm3_per_turn = vol_mm3_per_hour / to_turns<int>( 1_hours );

    const double ml_to_mm3 = 1000;
    const double charges_per_turn = vol_mm3_per_turn / ( charge_ml * ml_to_mm3 );

    return charges_per_turn;
}

double trap::funnel_turns_per_charge( double rain_depth_mm_per_hour ) const
{
    // 1mm rain on 1m^2 == 1 liter water == 1000ml
    // 1 liter == 4 volume
    // 1 volume == 250ml: containers
    // 1 volume == 200ml: water
    // How many turns should it take for us to collect 1 charge of rainwater?
    // "..."
    if( rain_depth_mm_per_hour == 0.0 ) {
        return 0.0;
    }

    const double surface_area_mm2 = M_PI * ( funnel_radius_mm * funnel_radius_mm );
    const double charges_per_turn = funnel_charges_per_turn( surface_area_mm2, rain_depth_mm_per_hour );

    if( charges_per_turn > 0.0 ) {
        return 1.0 / charges_per_turn;
    }

    return 0.0;
}

/**
 * Main routine for filling funnels from weather effects.
 */
static void fill_funnels( int rain_depth_mm_per_hour, bool acid, const trap &tr )
{
    const double turns_per_charge = tr.funnel_turns_per_charge( rain_depth_mm_per_hour );
    // Give each funnel on the map a chance to collect the rain.
    const std::vector<tripoint> &funnel_locs = g->m.trap_locations( tr.loadid );
    for( const tripoint &loc : funnel_locs ) {
        units::volume maxcontains = 0_ml;
        if( one_in( turns_per_charge ) ) {
            // FIXME:
            //add_msg("%d mm/h %d tps %.4f: fill",int(calendar::turn),rain_depth_mm_per_hour,turns_per_charge);
            // This funnel has collected some rain! Put the rain in the largest
            // container here which is either empty or contains some mixture of
            // impure water and acid.
            map_stack items = g->m.i_at( loc );
            auto container = items.end();
            for( auto candidate_container = items.begin(); candidate_container != items.end();
                 ++candidate_container ) {
                if( ( *candidate_container )->is_funnel_container( maxcontains ) ) {
                    container = candidate_container;
                }
            }

            if( container != items.end() ) {
                ( *container )->add_rain_to_container( acid, 1 );
                ( *container )->set_age( 0_turns );
            }
        }
    }
}

/**
 * Fill funnels and makeshift funnels from weather effects.
 * @see fill_funnels
 */
static void fill_water_collectors( int mmPerHour, bool acid )
{
    for( auto &e : trap::get_funnels() ) {
        fill_funnels( mmPerHour, acid, *e );
    }
}

/**
 * Main routine for wet effects caused by weather.
 * Drenching the player is applied after checks against worn and held items.
 *
 * The warmth of armor is considered when determining how much drench happens per tick.
 *
 * Note that this is not the only place where drenching can happen.
 * For example, moving or swimming into water tiles will also cause drenching.
 * @see fill_water_collectors
 * @see map::decay_fields_and_scent
 * @see player::drench
 */
void weather_effect::wet_player( int amount )
{
    Character &target = get_avatar();
    if( !is_player_outside() ||
        target.has_trait( trait_FEATHERS ) ||
        target.primary_weapon().has_flag( json_flag_RAIN_PROTECT ) ||
        ( !one_in( 50 ) && target.worn_with_flag( json_flag_RAINPROOF ) ) ) {
        return;
    }
    // Coarse correction to get us back to previously intended soaking rate.
    if( !calendar::once_every( 6_seconds ) ) {
        return;
    }
    std::map<bodypart_id, std::vector<const item *>> clothing_map;
    const auto &all_bps = target.get_all_body_parts();
    for( const bodypart_id &bp : all_bps ) {
        clothing_map.emplace( bp, std::vector<const item *>() );
    }
    for( const item * const &it : target.worn ) {
        // TODO: Port body part set id changes
        const body_part_set &covered = it->get_covered_body_parts();
        for( const bodypart_id &bp : all_bps ) {
            if( covered.test( bp.id() ) ) {
                clothing_map[bp.id()].emplace_back( it );
            }
        }
    }
    std::map<bodypart_id, int> warmth_bp = target.warmth( clothing_map );
    const int warmth_delay = warmth_bp[body_part_torso] * 0.8 +
                             warmth_bp[body_part_head] * 0.2;
    if( rng( 0, 100 - amount + warmth_delay ) > 10 ) {
        // Thick clothing slows down (but doesn't cap) soaking
        return;
    }

    const bodypart &torso = target.get_part( body_part_torso );
    body_part_set drenched_parts{ { bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ), bodypart_str_id( "arm_r" ), bodypart_str_id( "head" ) } };
    if( torso.get_wetness() * 100 >= torso.get_drench_capacity() * 50 ) {
        // Once upper body is 50%+ drenched, start soaking the legs too
        drenched_parts.unify_set( { { bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ) } } );
    }

    g->u.drench( amount, drenched_parts, false );
}

/**
 * Thunder.
 * Flavor messages. Very wet.
 */
void weather_effect::thunder( int intensity )
{
    if( !g->u.has_effect( effect_sleep ) && !g->u.is_deaf() && one_in( intensity ) ) {
        if( g->get_levz() >= 0 ) {
            add_msg( _( "You hear a distant rumble of thunder." ) );
            sfx::play_variant_sound( "environment", "thunder_far", 80, random_direction() );
        } else if( one_in( std::max( roll_remainder( 2.0f * g->get_levz() /
                                     g->u.mutation_value( "hearing_modifier" ) ), 1 ) ) ) {
            add_msg( _( "You hear a rumble of thunder from above." ) );
            sfx::play_variant_sound( "environment", "thunder_far",
                                     ( 80 * g->u.mutation_value( "hearing_modifier" ) ), random_direction() );
        }
    }
}

/**
 * Lightning.
 * Chance of lightning illumination for the current turn when aboveground. Thunder.
 *
 * This used to manifest actual lightning on the map, causing fires and such, but since such effects
 * only manifest properly near the player due to the "reality bubble", this was causing undesired metagame tactics
 * such as players leaving their shelter for a more "expendable" area during lightning storms.
 */
void weather_effect::lightning( int intensity )
{
    if( one_in( intensity ) ) {
        if( g->get_levz() >= 0 ) {
            add_msg( _( "A flash of lightning illuminates your surroundings!" ) );
            sfx::play_variant_sound( "environment", "thunder_near", 100, random_direction() );
            get_weather().lightning_active = true;
        }
    } else {
        get_weather().lightning_active = false;
    }
}
/**
 * Acid drizzle.
 * Causes minor pain only.
 */
void weather_effect::light_acid( int intensity )
{
    if( calendar::once_every( time_duration::from_seconds( intensity ) ) && is_player_outside() ) {
        if( g->u.primary_weapon().has_flag( json_flag_RAIN_PROTECT ) && !one_in( 3 ) ) {
            add_msg( _( "Your %s protects you from the acidic drizzle." ), g->u.primary_weapon().tname() );
        } else {
            if( g->u.worn_with_flag( json_flag_RAINPROOF ) && !one_in( 4 ) ) {
                add_msg( _( "Your clothing protects you from the acidic drizzle." ) );
            } else {
                bool has_helmet = false;
                if( g->u.is_wearing_power_armor( &has_helmet ) && ( has_helmet || !one_in( 4 ) ) ) {
                    add_msg( _( "Your power armor protects you from the acidic drizzle." ) );
                } else {
                    add_msg( m_warning, _( "The acid rain stings, but is mostly harmless for now…" ) );
                    if( one_in( 10 ) && ( g->u.get_pain() < 10 ) ) {
                        g->u.mod_pain( 1 );
                    }
                }
            }
        }
    }
}

/**
 * Acid rain.
 * Causes major pain. Damages non acid-proof mobs. Very wet (acid).
 */
void weather_effect::acid( int intensity )
{
    if( !( calendar::once_every( time_duration::from_seconds( intensity ) ) && is_player_outside() ) ) {
        return;
    }

    auto &you = get_avatar();
    if( you.primary_weapon().has_flag( json_flag_RAIN_PROTECT ) && one_in( 4 ) ) {
        return add_msg( _( "Your umbrella protects you from the acid rain." ) );
    }

    if( you.worn_with_flag( json_flag_RAINPROOF ) && one_in( 2 ) ) {
        return add_msg( _( "Your clothing protects you from the acid rain." ) );
    }

    bool has_helmet = false;
    if( you.is_wearing_power_armor( &has_helmet ) && ( has_helmet || !one_in( 2 ) ) ) {
        return add_msg( _( "Your power armor protects you from the acid rain." ) );
    }

    add_msg( m_bad, _( "The acid rain burns!" ) );
    if( one_in( 2 ) && ( you.get_pain() < 100 ) ) {
        you.mod_pain( rng( 1, 5 ) );
    }
}

double precip_mm_per_hour( precip_class const p )
// Precipitation rate expressed as the rainfall equivalent if all
// the precipitation were rain (rather than snow).
{
    return
        p == precip_class::very_light ? 0.5 :
        p == precip_class::light ? 1 :
        p == precip_class::medium ? 2 :
        p == precip_class::heavy ? 4 :
        0;
}

void handle_weather_effects( const weather_type_id &w )
{
    ZoneScoped;
    if( w->rains && w->precip != precip_class::none ) {
        fill_water_collectors( precip_mm_per_hour( w->precip ),
                               w->acidic );
        int wetness = 0;
        time_duration decay_time = 60_turns;
        if( w->precip == precip_class::very_light ) {
            wetness = 5;
            decay_time = 5_turns;
        } else if( w->precip == precip_class::light ) {
            wetness = 30;
            decay_time = 15_turns;
        } else if( w->precip == precip_class::medium ) {
            wetness = 45;
            decay_time = 30_turns;
        } else if( w->precip == precip_class::heavy ) {
            decay_time = 45_turns;
            wetness = 60;
        }
        g->m.decay_fields_and_scent( decay_time );
        weather_effect::wet_player( wetness );
    }
    glare( w );

    for( const auto &effect : w->effects ) {
        effect.first( effect.second );
    }
}

static std::string to_string( const weekdays &d )
{
    static const std::array<std::string, 7> weekday_names = {{
            translate_marker( "Sunday" ), translate_marker( "Monday" ),
            translate_marker( "Tuesday" ), translate_marker( "Wednesday" ),
            translate_marker( "Thursday" ), translate_marker( "Friday" ),
            translate_marker( "Saturday" )
        }
    };
    static_assert( static_cast<int>( weekdays::SUNDAY ) == 0,
                   "weekday_names array is out of sync with weekdays enumeration values" );
    return _( weekday_names[ static_cast<int>( d ) ] );
}


constexpr int NUM_FORECAST_PERIODS = 6;

struct forecast_period {
    units::temperature temp_high = -100_f;
    units::temperature temp_low = 100_f;
    const weather_type *type = nullptr;
    int type_priority = 1;
    weekdays week_day = weekdays::MONDAY;
    bool is_day = false;
};

// Script from Wikipedia:
// Current time
// The current time is hour/minute Eastern Standard Time
// Local conditions
// At 8 AM in Falls City, it was sunny. The temperature was 60 degrees, the dewpoint 59,
// and the relative humidity 97%. The wind was west at 6 miles (9.7 km) an hour.
// The pressure was 30.00 inches (762 mm) and steady.
// Regional conditions
// Across eastern Nebraska, southwest Iowa, and northwest Missouri, skies ranged from
// sunny to mostly sunny. It was 60 at Beatrice, 59 at Lincoln, 59 at Nebraska City, 57 at Omaha,
// 59 at Red Oak, and 62 at St. Joseph."
// Forecast
// TODAY...MOSTLY SUNNY. HIGHS IN THE LOWER 60S. NORTHEAST WINDS 5 TO 10 MPH WITH GUSTS UP TO 25 MPH.
// TONIGHT...MOSTLY CLEAR. LOWS IN THE UPPER 30S. NORTHEAST WINDS 5 TO 10 MPH.
// MONDAY...MOSTLY SUNNY. HIGHS IN THE LOWER 60S. NORTHEAST WINDS 10 TO 15 MPH.
// MONDAY NIGHT...PARTLY CLOUDY. LOWS AROUND 40. NORTHEAST WINDS 5 TO 10 MPH.

// 0% - No mention of precipitation
// 10% - No mention of precipitation, or isolated/slight chance
// 20% - Isolated/slight chance
// 30% - (Widely) scattered/chance
// 40% or 50% - Scattered/chance
// 60% or 70% - Numerous/likely
// 80%, 90% or 100% - No additional modifiers (i.e. "showers and thunderstorms")
/**
 * Generate textual weather forecast for the specified radio tower.
 */
std::string weather_forecast( const point_abs_sm &abs_sm_pos )
{
    std::string weather_report;
    // Local conditions
    const auto cref = overmap_buffer.closest_city( tripoint_abs_sm( abs_sm_pos, 0 ) );
    const std::string city_name = cref ? cref.city->name : std::string( _( "#####" ) );
    // Current time
    const weather_manager &weather = get_weather();
    weather_report += string_format(
                          //~ %1$s: time of day, %2$s: hour of day, %3$s: city name, %4$s: weather name, %5$s: temperature value
                          _( "for %1$s:\nCurrently %2$s, %3$s.\nLater " ),
                          city_name,
                          print_temperature( get_weather().temperature ), get_weather().weather_id->name
                      );

    //weather_report += ", the dewpoint ???, and the relative humidity ???.  ";
    //weather_report += "The wind was <direction> at ? mi/km an hour.  ";
    //weather_report += "The pressure was ??? in/mm and steady/rising/falling.";
    // TODO: wind direction and speed

    // Regional conditions (simulated by choosing a random range containing the current conditions).
    // Adjusted for weather volatility based on how many weather changes are coming up.
    //weather_report += "Across <region>, skies ranged from <cloudiest> to <clearest>.  ";
    // TODO: Add fake reports for nearby cities
    // TODO: fix point types
    const tripoint abs_ms_pos = tripoint( project_to<coords::ms>( abs_sm_pos ).raw(), 0 );

    const time_point now_hour = calendar::turn - time_duration::from_minutes( minute_of_hour<int>
                                ( calendar::turn ) );
    bool now_is_day = is_day( now_hour );

    int last_idx = -1;
    bool last_is_day = now_is_day;
    time_point last_hour = now_hour;

    std::array<forecast_period, NUM_FORECAST_PERIODS> periods = { {} };

    const auto &wgen = weather.get_cur_weather_gen();
    while( true ) {
        last_hour += 1_hours;
        // Some types of weather may happen only during the night or during the day,
        // so skip dusk and dawn just to be safe.
        if( is_dusk( last_hour ) || is_dawn( last_hour ) ) {
            continue;
        }
        bool new_is_day = is_day( last_hour );
        if( new_is_day != last_is_day ) {
            last_is_day = new_is_day;
            last_idx += 1;
            if( last_idx >= NUM_FORECAST_PERIODS ) {
                break;
            }
            periods[last_idx].is_day = last_is_day;
            periods[last_idx].week_day = day_of_week( last_hour );
        }
        if( last_idx < 0 ) {
            continue;
        }

        forecast_period &period = periods[last_idx];

        w_point w = wgen.get_weather( abs_ms_pos, last_hour, g->get_seed() );
        const weather_type_id &new_type = wgen.get_weather_conditions( w );
        int new_priority = wgen.forecast_priority( new_type );
        if( !period.type || new_priority > period.type_priority ) {
            period.type_priority = new_priority;
            period.type = &new_type.obj();
        }
        period.temp_high = std::max( period.temp_high, w.temperature );
        period.temp_low = std::min( period.temp_low, w.temperature );
    }

    for( int i = 0; i < NUM_FORECAST_PERIODS; i++ ) {
        const forecast_period &period = periods[i];
        std::string day;
        if( i == 0 ) {
            if( period.is_day ) {
                day = _( "today" );
            } else {
                day = _( "tonight" );
            }
        } else {
            if( period.is_day ) {
                day = to_string( period.week_day );
            } else {
                //~ %s is day of week (e.g. Friday Night)
                day = string_format( pgettext( "Forecast", "%s Night" ), to_string( period.week_day ) );
            }
        }
        weather_report += string_format(
                              //~ %1 is day or night of week (e.g. "Monday", or "Friday Night"),
                              //~ %2 is weather type, %3 and %4 are temperatures.
                              _( "%1$s, between %3$s and %4$s, %2$s.\n" ),
                              day, period.type->name,
                              print_temperature( period.temp_high ),
                              print_temperature( period.temp_low )
                          );
    }

    return weather_report;
}

/**
 * Print temperature (and convert to Celsius if Celsius display is enabled.)
 */
std::string print_temperature( double fahrenheit, int decimals )
{
    return print_temperature( units::from_fahrenheit( fahrenheit ), decimals );
}

std::string print_temperature( units::temperature temperature, int decimals )
{
    const auto text = [&]( const double value ) {
        return string_format( "%.*f", decimals, value );
    };

    if( get_option<std::string>( "USE_CELSIUS" ) == "celsius" ) {
        return string_format( pgettext( "temperature in Celsius", "%s°C" ),
                              text( units::to_celsius<double>( temperature ) ) );
    } else if( get_option<std::string>( "USE_CELSIUS" ) == "kelvin" ) {
        return string_format( pgettext( "temperature in Kelvin", "%sK" ),
                              text( units::to_kelvins<double>( temperature ) ) );
    } else {
        return string_format( pgettext( "temperature in Fahrenheit", "%sF" ),
                              text( units::to_fahrenheit<double>( temperature ) ) );
    }
}

/**
 * Print relative humidity (no conversions.)
 */
std::string print_humidity( double humidity, int decimals )
{
    const std::string ret = string_format( "%.*f", decimals, humidity );
    return string_format( pgettext( "humidity in percent", "%s%%" ), ret );
}

/**
 * Print pressure (no conversions.)
 */
std::string print_pressure( double pressure, int decimals )
{
    const std::string ret = string_format( "%.*f", decimals, pressure / 10 );
    return string_format( pgettext( "air pressure in kPa", "%s kPa" ), ret );
}

static double local_windchill_lowtemp( double temperature_f, double, double wind_mph )
{
    /// Model 1, cold wind chill (only valid for temps below 50F)
    /// Is also used as a standard in North America.

    // This model fails when wind is less than 3 mph
    double wind_mph_lowcapped = std::max( 3.0, wind_mph );

    // Temperature is removed at the end, because get_local_windchill is meant to calculate the difference.
    // Source : http://en.wikipedia.org/wiki/Wind_chill#North_American_and_United_Kingdom_wind_chill_index
    return 35.74
           + 0.6215 * temperature_f
           - 35.75 * std::pow( wind_mph_lowcapped, 0.16 )
           + 0.4275 * temperature_f * std::pow( wind_mph_lowcapped, 0.16 )
           - temperature_f;
}

static double local_windchill_hightemp( double temperature_f, double humidity, double wind_mph )
{
    /// Model 2, warm wind chill

    // Source : http://en.wikipedia.org/wiki/Wind_chill#Australian_Apparent_Temperature
    // Convert to meters per second.
    double wind_meters_per_sec = wind_mph * 0.44704;
    double temperature_c = units::fahrenheit_to_celsius( temperature_f );

    // Cap the vapor pressure term to 50C of extra heat, as this term
    // otherwise grows logistically to an asymptotic value of about 2e7
    // for large values of temperature. This is presumably due to the
    // model being designed for reasonable ambient temperature values,
    // rather than extremely high ones.
    double windchill_c = 0.33 * std::min<float>( 150.00, humidity / 100.00 * 6.105 *
                         std::exp( 17.27 * temperature_c / ( 237.70 + temperature_c ) ) )
                         - 0.70 * wind_meters_per_sec
                         - 4.00;
    // Convert to Fahrenheit, but omit the '+ 32' because we are only dealing with a piece of the felt air temperature equation.
    return windchill_c * 9 / 5;
}

int get_local_windchill( double temperature_f, double humidity, double wind_mph )
{
    // The function must be continuous and strictly non-decreasing with temperature
    constexpr double low_temp = 30.0;
    constexpr double high_temp = 70.0;
    if( temperature_f >= high_temp ) {
        return std::ceil( local_windchill_hightemp( temperature_f, humidity, wind_mph ) );
    }

    // lerp-ing both functions results in a non-monotonous function
    double windchill_f_hightemp = local_windchill_hightemp( high_temp, humidity, wind_mph );
    double windchill_f_lowtemp = std::min( windchill_f_hightemp, local_windchill_lowtemp( low_temp,
                                           humidity, wind_mph ) );

    double t = ( temperature_f - low_temp ) / ( high_temp - low_temp );
    return std::ceil( lerp( std::min( windchill_f_lowtemp, windchill_f_hightemp ),
                            windchill_f_hightemp,
                            t ) );
}

nc_color get_wind_color( double windpower )
{
    nc_color windcolor;
    if( windpower < 1 ) {
        windcolor = c_dark_gray;
    } else if( windpower < 3 ) {
        windcolor = c_dark_gray;
    } else if( windpower < 7 ) {
        windcolor = c_light_gray;
    } else if( windpower < 12 ) {
        windcolor = c_light_gray;
    } else if( windpower < 18 ) {
        windcolor = c_blue;
    } else if( windpower < 24 ) {
        windcolor = c_blue;
    } else if( windpower < 31 ) {
        windcolor = c_light_blue;
    } else if( windpower < 38 ) {
        windcolor = c_light_blue;
    } else if( windpower < 46 ) {
        windcolor = c_cyan;
    } else if( windpower < 54 ) {
        windcolor = c_cyan;
    } else if( windpower < 63 ) {
        windcolor = c_light_cyan;
    } else if( windpower < 72 ) {
        windcolor = c_light_cyan;
    } else if( windpower > 72 ) {
        windcolor = c_white;
    }
    return windcolor;
}

std::string get_shortdirstring( int angle )
{
    std::string dirstring;
    int dirangle = angle;
    if( dirangle <= 23 || dirangle > 338 ) {
        dirstring = _( "N" );
    } else if( dirangle <= 68 ) {
        dirstring = _( "NE" );
    } else if( dirangle <= 113 ) {
        dirstring = _( "E" );
    } else if( dirangle <= 158 ) {
        dirstring = _( "SE" );
    } else if( dirangle <= 203 ) {
        dirstring = _( "S" );
    } else if( dirangle <= 248 ) {
        dirstring = _( "SW" );
    } else if( dirangle <= 293 ) {
        dirstring = _( "W" );
    } else {
        dirstring = _( "NW" );
    }
    return dirstring;
}

std::string get_dirstring( int angle )
{
    // Convert angle to cardinal directions
    std::string dirstring;
    int dirangle = angle;
    if( dirangle <= 23 || dirangle > 338 ) {
        dirstring = _( "North" );
    } else if( dirangle <= 68 ) {
        dirstring = _( "North-East" );
    } else if( dirangle <= 113 ) {
        dirstring = _( "East" );
    } else if( dirangle <= 158 ) {
        dirstring = _( "South-East" );
    } else if( dirangle <= 203 ) {
        dirstring = _( "South" );
    } else if( dirangle <= 248 ) {
        dirstring = _( "South-West" );
    } else if( dirangle <= 293 ) {
        dirstring = _( "West" );
    } else {
        dirstring = _( "North-West" );
    }
    return dirstring;
}

std::string get_wind_arrow( int dirangle )
{
    std::string wind_arrow;
    if( dirangle < 0 || dirangle >= 360 ) {
        wind_arrow.clear();
    } else if( dirangle <= 23 || dirangle > 338 ) {
        wind_arrow = "\u21D3";
    } else if( dirangle <= 68 ) {
        wind_arrow = "\u21D9";
    } else if( dirangle <= 113 ) {
        wind_arrow = "\u21D0";
    } else if( dirangle <= 158 ) {
        wind_arrow = "\u21D6";
    } else if( dirangle <= 203 ) {
        wind_arrow = "\u21D1";
    } else if( dirangle <= 248 ) {
        wind_arrow = "\u21D7";
    } else if( dirangle <= 293 ) {
        wind_arrow = "\u21D2";
    } else {
        wind_arrow = "\u21D8";
    }
    return wind_arrow;
}

int get_local_humidity( double humidity, const weather_type_id &weather, bool sheltered )
{
    int tmphumidity = humidity;
    if( sheltered ) {
        // Norm for a house?
        tmphumidity = humidity * ( 100 - humidity ) / 100 + humidity;
    } else if( weather->rains &&
               weather->precip >= precip_class::light ) {
        tmphumidity = 100;
    }

    return tmphumidity;
}

double get_local_windpower( double windpower, const oter_id &omter, const tripoint &location,
                            const int &winddirection, bool sheltered )
{
    /**
    *  A player is sheltered if he is underground, in a car, or indoors.
    **/
    if( sheltered ) {
        return 0;
    }
    rl_vec2d windvec = convert_wind_to_coord( winddirection );
    int tmpwind = static_cast<int>( windpower );
    tripoint triblocker( location + point( windvec.x, windvec.y ) );
    // Over map terrain may modify the effect of wind.
    if( is_ot_match( "forest", omter, ot_match_type::type ) ||
        is_ot_match( "forest_water", omter, ot_match_type::type ) ) {
        tmpwind = tmpwind / 2;
    }
    if( location.z > 0 ) {
        tmpwind = tmpwind + ( location.z * std::min( 5, tmpwind ) );
    }
    // An adjacent wall will block wind
    if( is_wind_blocker( triblocker ) ) {
        tmpwind = tmpwind / 10;
    }
    return static_cast<double>( tmpwind );
}

bool is_wind_blocker( const tripoint &location )
{
    return g->m.has_flag( "BLOCK_WIND", location );
}

// Description of Wind Speed - https://en.wikipedia.org/wiki/Beaufort_scale
std::string get_wind_desc( double windpower )
{
    std::string winddesc;
    if( windpower < 1 ) {
        winddesc = _( "Calm Air" );
    } else if( windpower <= 3 ) {
        winddesc = _( "Light Air" );
    } else if( windpower <= 7 ) {
        winddesc = _( "Light Breeze" );
    } else if( windpower <= 12 ) {
        winddesc = _( "Gentle Breeze" );
    } else if( windpower <= 18 ) {
        winddesc = _( "Moderate Breeze" );
    } else if( windpower <= 24 ) {
        winddesc = _( "Fresh Breeze" );
    } else if( windpower <= 31 ) {
        winddesc = _( "Strong Breeze" );
    } else if( windpower <= 38 ) {
        winddesc = _( "Moderate Gale" );
    } else if( windpower <= 46 ) {
        winddesc = _( "Gale" );
    } else if( windpower <= 54 ) {
        winddesc = _( "Strong Gale" );
    } else if( windpower <= 63 ) {
        winddesc = _( "Whole Gale" );
    } else if( windpower <= 72 ) {
        winddesc = _( "Violent Storm" );
    } else if( windpower > 72 ) {
        // Anything above Whole Gale is very unlikely to happen and has no additional effects.
        winddesc = _( "Hurricane" );
    }
    return winddesc;
}

rl_vec2d convert_wind_to_coord( const int angle )
{
    static const std::array<std::pair<int, rl_vec2d>, 9> outputs = {{
            { 330, rl_vec2d( 0, -1 ) },
            { 301, rl_vec2d( -1, -1 ) },
            { 240, rl_vec2d( -1, 0 ) },
            { 211, rl_vec2d( -1, 1 ) },
            { 150, rl_vec2d( 0, 1 ) },
            { 121, rl_vec2d( 1, 1 ) },
            { 60, rl_vec2d( 1, 0 ) },
            { 31, rl_vec2d( 1, -1 ) },
            { 0, rl_vec2d( 0, -1 ) }
        }
    };
    for( const std::pair<int, rl_vec2d> &val : outputs ) {
        if( angle >= val.first ) {
            return val.second;
        }
    }
    return rl_vec2d( 0, 0 );
}

bool warm_enough_to_plant( const tripoint &pos )
{
    // semi-appropriate temperature for most plants
    // exclude underground areas as we check that later
    return ( get_weather().get_temperature( pos ) >= 10_c || pos.z < 0 );
}

bool warm_enough_to_plant( const tripoint_abs_omt &pos )
{
    return ( get_weather().get_temperature( pos ) >= 10_c || pos.z() < 0 );
}

weather_manager::weather_manager()
{
    lightning_active = false;
    weather_override = weather_type_id::NULL_ID();
    nextweather = calendar::before_time_starts;
    temperature = 0_c;
    weather_id = weather_type_id::NULL_ID();
}

weather_manager::~weather_manager() = default;

const weather_generator &weather_manager::get_cur_weather_gen() const
{
    const overmap &om = g->get_cur_om();
    const regional_settings &settings = om.get_settings();
    return settings.weather;
}

void weather_manager::update_weather()
{
    ZoneScoped;

    w_point &w = weather_precise;
    winddirection = wind_direction_override ? *wind_direction_override : w.winddirection;
    windspeed = windspeed_override ? *windspeed_override : w.windpower;
    if( weather_id && calendar::turn < nextweather ) {
        return;
    }

    const weather_generator &weather_gen = get_cur_weather_gen();
    w = weather_gen.get_weather( g->u.global_square_location(), calendar::turn, g->get_seed() );
    weather_type_id old_weather = weather_id;
    weather_id = weather_override ? weather_override : weather_gen.get_weather_conditions( w );
    if( !g->u.has_artifact_with( AEP_BAD_WEATHER ) ) {
        weather_override = weather_type_id::NULL_ID();
    }

    sfx::do_ambient();
    temperature = w.temperature;
    lightning_active = false;
    // Check weather every few turns, instead of every turn.
    // TODO: predict when the weather changes and use that time.
    nextweather = calendar::turn + 5_minutes;
    if( weather_id != old_weather && weather_id->dangerous &&
        g->get_levz() >= 0 && get_map().is_outside( g->u.pos() )
        && !g->u.has_activity( ACT_WAIT_WEATHER ) ) {
        g->cancel_activity_or_ignore_query( distraction_type::weather_change,
                                            string_format( _( "The weather changed to %s!" ), weather_id->name ) );
    }

    if( weather_id != old_weather && g->u.has_activity( ACT_WAIT_WEATHER ) ) {
        g->u.assign_activity( ACT_WAIT_WEATHER, 0, 0 );
    }

    if( weather_id->sight_penalty !=
        old_weather->sight_penalty ) {
        for( int i = -OVERMAP_DEPTH; i <= OVERMAP_HEIGHT; i++ ) {
            get_map().set_transparency_cache_dirty( i );
        }
        get_map().set_seen_cache_dirty( tripoint_zero );
    }

    water_temperature = weather_gen.get_water_temperature(
                            tripoint_abs_ms( g->u.global_square_location() ),
                            calendar::turn, calendar::config, g->get_seed() ) ;
}

void weather_manager::set_nextweather( time_point t )
{
    nextweather = t;
    update_weather();
}

auto weather_manager::get_temperature( const tripoint &location ) const -> units::temperature
{
    const auto &cached = temperature_cache.find( location );
    if( cached != temperature_cache.end() ) {
        return cached->second;
    }

    // local modifier
    int temp_mod = 0;

    if( !g->new_game ) {
        temp_mod += get_heat_radiation( location, false );
        temp_mod += get_convection_temperature( location );
    }

    const int added_f = g->new_game ? 0 : g->m.get_temperature( location ) + temp_mod;
    const int base_f = units::to_fahrenheit(
                           location.z < 0 ? temperatures::annual_average : temperature );

    // Hack: adding temperatures between temperatures makes no sense
    return units::from_celsius( std::round( units::fahrenheit_to_celsius( base_f + added_f ) ) );
}

auto weather_manager::get_temperature( const tripoint_abs_omt &location ) const ->
units::temperature
{
    if( location.z() < 0 ) {
        return temperatures::annual_average;
    }

    tripoint abs_ms = project_to<coords::ms>( location ).raw();
    w_point w = get_cur_weather_gen().get_weather( abs_ms, calendar::turn, g->get_seed() );
    return w.temperature;
}

auto weather_manager::get_water_temperature( const tripoint & ) const -> units::temperature
{
    return water_temperature;
}

void weather_manager::clear_temp_cache()
{
    temperature_cache.clear();
}

namespace weather
{

bool is_sheltered( const map &m, const tripoint &p )
{
    const optional_vpart_position vp = m.veh_at( p );

    return ( !m.is_outside( p ) ||
             p.z < 0 ||
             ( vp && vp->is_inside() ) );
}

bool is_in_sunlight( const map &m, const tripoint &p, const weather_type_id &weather )
{
    // TODO: Remove that game reference and include light in weather data
    return m.is_outside( p ) && g->light_level( p.z ) >= 40 && !is_night( calendar::turn ) &&
           weather->sun_intensity >= sun_intensity_type::light;
}

} // namespace weather

///@}
