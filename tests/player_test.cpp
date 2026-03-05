#include "catch/catch.hpp"

#include <string>
#include <array>
#include <list>
#include <memory>

#include "avatar.h"
#include "avatar_action.h"
#include "bodypart.h"
#include "catch/catch.hpp"
#include "player.h"
#include "units_temperature.h"
#include "weather.h"
#include "bodypart.h"
#include "calendar.h"
#include "item.h"
#include "player_helpers.h"
#include "map.h"
#include "map_helpers.h"
#include "weather.h"
#include "game.h"
#include "units.h"
#include "hash_utils.h"
#include "overmapbuffer.h"
#include "state_helpers.h"
#include "vpart_position.h"

static weather_type_id WEATHER_CLOUDY = weather_type_id( "cloudy" );

struct body_part_temp {
    body_part_temp( bodypart_str_id part, int temperature )
        : part( part ), temperature( temperature )
    {}
    bodypart_str_id part;
    int temperature;

    bool operator==( const body_part_temp &other ) const {
        return part == other.part && temperature == other.temperature;
    }
};

namespace std
{

template<> struct hash<body_part_temp> {
    std::size_t operator()( body_part_temp const &bpt ) const noexcept {
        auto tuple_hash = cata::auto_hash<std::tuple<const bodypart_str_id &, const int &>>();
        return tuple_hash( std::forward_as_tuple( bpt.part, bpt.temperature ) );
    }
};

} // namespace std

struct temperature_threshold {
    constexpr temperature_threshold( int v, const char *n )
        : value( v )
        , name( n )
    {}

    int value;
    const char *name;
};

#define t(x) temperature_threshold(x, #x)

constexpr std::array<temperature_threshold, 7> bodytemps = {{
        t( BODYTEMP_FREEZING ), t( BODYTEMP_VERY_COLD ), t( BODYTEMP_COLD ),
        t( BODYTEMP_NORM ),
        t( BODYTEMP_HOT ), t( BODYTEMP_VERY_HOT ), t( BODYTEMP_SCORCHING )
    }
};

#undef t

std::ostream &operator<<( std::ostream &os, const body_part_temp &bpt );
std::ostream &operator<<( std::ostream &os, const body_part_temp &bpt )
{
    // Stringify the temperature to avoid Catch adding hex
    return os << std::to_string( bpt.temperature );
}

std::ostream &operator<<( std::ostream &os, const std::vector<body_part_temp> &bpts );
std::ostream &operator<<( std::ostream &os, const std::vector<body_part_temp> &bpts )
{
    os << "[";
    for( const auto &e : bpts ) {
        os << e << ",";
    }
    return os << "]\n";
}

class temperatures_wrapper : public std::vector<int>
{
        using base_type = std::vector<int>;
    public:
        temperatures_wrapper( const base_type &base )
            : base_type( base )
        {}
};

std::ostream &operator<<( std::ostream &os, const temperatures_wrapper &arr );
std::ostream &operator<<( std::ostream &os, const temperatures_wrapper &arr )
{
    os << "[\n";
    for( size_t i = 0; i < arr.size() / 2; i++ ) {
        os << string_format( "%6d, ", arr[i] );
    }
    // Ugly split like that because otherwise it gets wrapped seemingly randomly
    os << "\n";
    for( size_t i = arr.size() / 2; i < arr.size() - 1; i++ ) {
        os << string_format( "%6d, ", arr[i] );
    }
    os << string_format( "%6d", arr[arr.size() - 1] );

    os << "\n]";
    return os;
}

static int get_temp_cur( const Character &c, const bodypart_str_id &bp )
{
    auto iter = c.get_body().find( bp );
    if( iter == c.get_body().end() ) {
        debugmsg( "%s has no %s", c.disp_name().c_str(), bp.c_str() );
        return 0;
    }

    return iter->second.get_temp_cur();
}

// Run update_bodytemp() until core body temperature settles.
static std::vector<int> converge_temperature( player &p, size_t iters,
        int start_temperature = BODYTEMP_NORM )
{
    constexpr size_t n_history = 10;
    REQUIRE( get_weather().weather_id == WEATHER_CLOUDY );
    REQUIRE( get_weather().windspeed == 0 );

    for( auto &pr : p.get_body() ) {
        pr.second.set_temp_cur( start_temperature );
        pr.second.set_temp_conv( start_temperature );
    }

    bool converged = false;

    std::unordered_set<std::vector<body_part_temp>, cata::range_hash> history( iters );
    std::list<std::vector<body_part_temp>> last_n_history;

    const auto &parts = p.get_body();

    for( size_t i = 0; i < iters; i++ ) {
        std::vector<body_part_temp> current_iter_temperature;
        current_iter_temperature.reserve( parts.size() );
        for( const auto &pr : parts ) {
            current_iter_temperature.emplace_back( pr.first, get_temp_cur( p, pr.first ) );
        }
        if( history.contains( current_iter_temperature ) ) {
            converged = true;
            break;
        }

        history.emplace( current_iter_temperature );
        last_n_history.emplace_front( current_iter_temperature );
        while( last_n_history.size() > n_history ) {
            last_n_history.pop_back();
        }
        p.update_bodytemp( get_map(), get_weather() );
    }

    std::vector<int> result;
    std::transform( p.get_body().begin(), p.get_body().end(), std::back_insert_iterator( result ),
    []( const auto & pr ) {
        return pr.second.get_temp_cur();
    } );

    CAPTURE( iters );
    CAPTURE( result );
    CAPTURE( last_n_history );
    // If it doesn't converge, it's usually very close to it anyway, so don't fail
    CHECK( converged );
    return result;
}

static void equip_clothing( player &p, const std::vector<std::string> &clothing )
{
    for( const std::string &c : clothing ) {
        p.wear_item( item::spawn( itype_id( c ), calendar::start_of_cataclysm ) );
    }
}

// TODO: Find good name
/**
 * Table of temperature ranges closest to given body temperature.
 * That is, [0] to [1] is the range where FREEZING is the closest.
 */
static std::array<int, 8> bodytemp_voronoi()
{
    std::array<int, 8> midpoints;
    midpoints[0] = INT_MIN;
    midpoints[7] = INT_MAX;
    for( int i = 0; i < 6; i++ ) {
        midpoints[i + 1] = ( bodytemps[i].value + bodytemps[i + 1].value ) / 2;
    }

    return midpoints;
}

// Run the tests for each of the temperature setpoints.
static void test_temperature_spread( player &p,
                                     const std::array<units::temperature, 7> &air_temperatures )
{
    const auto thresholds = bodytemp_voronoi();
    for( int i = 0; i < 7; i++ ) {
        get_weather().temperature = air_temperatures[i];
        get_weather().clear_temp_cache();
        CAPTURE( air_temperatures[i] );
        CAPTURE( get_weather().temperature );
        int converged_temperature = converge_temperature( p, 1500, bodytemps[i].value )[0];
        auto expected_body_temperature = bodytemps[i].name;
        CAPTURE( expected_body_temperature );
        int air_temperature_celsius = to_celsius( air_temperatures[i] );
        CAPTURE( air_temperature_celsius );
        int min_temperature = thresholds[i];
        int max_temperature = thresholds[i + 1];
        CHECK( converged_temperature > min_temperature );
        CHECK( converged_temperature < max_temperature );
    }
}

const std::vector<std::string> light_clothing = {{
        "hat_ball",
        "bandana",
        "tshirt",
        "gloves_fingerless",
        "jeans",
        "socks",
        "sneakers"
    }
};

const std::vector<std::string> heavy_clothing = {{
        "hat_knit",
        "tshirt",
        "vest",
        "trenchcoat",
        "gloves_wool",
        "long_underpants",
        "pants_army",
        "socks_wool",
        "boots"
    }
};

const std::vector<std::string> arctic_clothing = {{
        "balclava",
        "goggles_ski",
        "hat_hunting",
        "under_armor",
        "vest",
        "coat_winter",
        "gloves_liner",
        "gloves_winter",
        "long_underpants",
        "pants_fur",
        "socks_wool",
        "boots_winter",
    }
};

static void guarantee_neutral_weather( const player &p, weather_manager &weather )
{
    weather.weather_id = WEATHER_CLOUDY;
    weather.weather_override = WEATHER_CLOUDY;
    weather.windspeed = 0;
    weather.override_humidity( 0 );
    REQUIRE( !get_map().has_flag( TFLAG_SWIMMABLE, p.pos() ) );
    REQUIRE( !get_map().has_flag( TFLAG_DEEP_WATER, p.pos() ) );
    REQUIRE( !g->is_in_sunlight( p.pos() ) );
    REQUIRE( !get_map().veh_at( p.pos() ) );

    const w_point &wp = weather.get_precise();
    const oter_id &cur_om_ter = overmap_buffer.ter( p.global_omt_location() );
    bool sheltered = g->is_sheltered( p.pos() );
    double total_windpower = get_local_windpower( weather.windspeed, cur_om_ter,
                             p.pos(),
                             weather.winddirection, sheltered );
    int air_humidity = get_local_humidity( wp.humidity, weather.weather_id, sheltered );

    REQUIRE( air_humidity == 0 );
    REQUIRE( total_windpower == 0.0 );
    REQUIRE( !const_cast<player &>( p ).in_climate_control() );
    REQUIRE( !p.can_use_floor_warmth() );
    REQUIRE( get_heat_radiation( p.pos(), true ) == 0 );

    for( const std::pair<const bodypart_str_id, bodypart> &elem : p.get_body() ) {
        REQUIRE( elem.second.get_wetness() == 0 );
    }
}

TEST_CASE( "Player body temperatures within expected bounds.", "[bodytemp][slow]" )
{
    clear_all_state();
    player &dummy = get_avatar();
    guarantee_neutral_weather( dummy, get_weather() );

    SECTION( "Nude target temperatures." ) {
        test_temperature_spread( dummy, {{-19_c, -4_c, 11_c, 26_c, 41_c, 56_c, 71_c,}} );
    }

    SECTION( "Lightly clothed target temperatures" ) {
        equip_clothing( dummy, light_clothing );
        test_temperature_spread( dummy, {{-22_c, -7_c, 8_c, 24_c, 39_c, 54_c, 69_c,}} );
    }

    SECTION( "Heavily clothed target temperatures" ) {
        equip_clothing( dummy, heavy_clothing );
        test_temperature_spread( dummy, {{-39_c, -23_c, -4_c, 15_c, 33_c, 48_c, 63_c,}} );
    }

    SECTION( "Arctic gear target temperatures" ) {
        equip_clothing( dummy, arctic_clothing );
        test_temperature_spread( dummy, {{-76_c, -61_c, -43_c, -17_c, 9_c, 35_c, 50_c,}} );
    }
}

/**
 * Finds air temperatures for which body temperature is closest to exact "named" value.
 * FREEZING, HOT, etc.
 */
static std::array<units::temperature, bodytemps.size()> find_temperature_points( player &p )
{
    constexpr int min_air_temp = -200;
    constexpr int max_air_temp = 200;
    std::array<std::pair<int, int>, bodytemps.size()> value_distances;
    std::fill( value_distances.begin(), value_distances.end(), std::make_pair( 0, INT_MAX ) );
    std::vector<temperatures_wrapper> all_converged_temperatures;
    all_converged_temperatures.resize( max_air_temp - min_air_temp, temperatures_wrapper( {} ) );
    for( int i = min_air_temp; i < max_air_temp; i++ ) {
        get_weather().temperature = units::from_millidegree_celsius( i * 500 );
        get_weather().clear_temp_cache();
        all_converged_temperatures[i - min_air_temp] = converge_temperature( p, 10000 );
        int converged_torso_temp = all_converged_temperatures[i - min_air_temp][0];
        // 0 - FREEZING, 6 - SCORCHING
        for( size_t temperature_index = 0; temperature_index < bodytemps.size(); temperature_index++ ) {
            int distance_to_definition = std::abs( bodytemps[temperature_index].value - converged_torso_temp );
            if( distance_to_definition < value_distances[temperature_index].second ) {
                value_distances[temperature_index] = std::make_pair( i, distance_to_definition );
            }
        }
    }

    // Check if higher starting temperature means higher end temperature (it obviously should, but doesn't?)
    for( int air_temperature = min_air_temp + 1; air_temperature < max_air_temp; air_temperature++ ) {
        CAPTURE( air_temperature );
        CAPTURE( units::from_fahrenheit( air_temperature ) );
        size_t index = air_temperature - min_air_temp;
        CAPTURE( all_converged_temperatures[index] );
        CAPTURE( all_converged_temperatures[index - 1] );
        CHECK( all_converged_temperatures[index][0] >= all_converged_temperatures[index - 1][0] );
    }

    std::array<units::temperature, 7> points;
    std::transform( value_distances.begin(), value_distances.end(), points.begin(),
    []( const std::pair<int, int> &pr ) {
        // Round it to nearest full Celsius, for nicer display
        return units::from_celsius( std::round( units::fahrenheit_to_celsius( pr.first ) ) );
    } );

    auto sorted_copy = points;
    std::sort( sorted_copy.begin(), sorted_copy.end() );
    CHECK( sorted_copy == points );
    return points;
}

static void print_temperatures( const std::array<units::temperature, bodytemps.size()>
                                &temperatures )
{
    std::string s = "{{";
    for( auto &t : temperatures ) {
        s += std::to_string( units::to_celsius( t ) ) + "_c,";
    }
    s += "}}\n";
    cata_printf( s );
}

TEST_CASE( "Find air temperatures for given body temperatures.", "[.][bodytemp]" )
{
    clear_all_state();
    player &dummy = get_avatar();
    guarantee_neutral_weather( dummy, get_weather() );

    SECTION( "Nude target temperatures." ) {
        const auto points = find_temperature_points( dummy );
        print_temperatures( points );
    }

    SECTION( "Lightly clothed target temperatures" ) {
        equip_clothing( dummy, light_clothing );
        const auto points = find_temperature_points( dummy );
        print_temperatures( points );
    }

    SECTION( "Heavily clothed target temperatures" ) {
        equip_clothing( dummy, heavy_clothing );
        const auto points = find_temperature_points( dummy );
        print_temperatures( points );
    }

    SECTION( "Arctic gear target temperatures" ) {
        equip_clothing( dummy, arctic_clothing );
        const auto points = find_temperature_points( dummy );
        print_temperatures( points );
    }
}

// Ugly pasta, for simplicity
static int find_converging_water_temp( player &p, int expected_water, int expected_bodytemp )
{
    constexpr int tol = 100;
    REQUIRE( get_map().has_flag( TFLAG_SWIMMABLE, p.pos() ) );
    REQUIRE( get_map().has_flag( TFLAG_DEEP_WATER, p.pos() ) );
    int actual_water = expected_water;
    int step = 2 * 128;
    do {
        step /= 2;
        get_weather().water_temperature = units::from_fahrenheit( actual_water );
        get_weather().clear_temp_cache();
        const int actual_temperature = units::celsius_to_fahrenheit( get_weather().get_water_temperature(
                                           p.pos() ).value() );
        REQUIRE( actual_temperature == actual_water );

        int converged_temperature = converge_temperature( p, 10000 )[0];
        bool high_enough = expected_bodytemp - tol <= converged_temperature;
        bool low_enough  = expected_bodytemp + tol >= converged_temperature;
        if( high_enough && low_enough ) {
            return actual_water;
        }

        int direction = high_enough ? -1 : 1;
        actual_water += direction * step;
    } while( step > 0 );
    bool converged = step > 0;
    CHECK( converged );

    return actual_water;
}

// Also ugly pasta, also for simplicity
static void test_water_temperature_spread( player &p, const std::array<int, 7> &expected_temps )
{
    std::array<int, 7> actual_temps;
    actual_temps[0] = find_converging_water_temp( p, expected_temps[0], BODYTEMP_FREEZING );
    actual_temps[1] = find_converging_water_temp( p, expected_temps[1], BODYTEMP_VERY_COLD );
    actual_temps[2] = find_converging_water_temp( p, expected_temps[2], BODYTEMP_COLD );
    actual_temps[3] = find_converging_water_temp( p, expected_temps[3], BODYTEMP_NORM );
    actual_temps[4] = find_converging_water_temp( p, expected_temps[4], BODYTEMP_HOT );
    actual_temps[5] = find_converging_water_temp( p, expected_temps[5], BODYTEMP_VERY_HOT );
    actual_temps[6] = find_converging_water_temp( p, expected_temps[6], BODYTEMP_SCORCHING );
    CHECK( actual_temps == expected_temps );
}

TEST_CASE( "Player body temperatures in water.", "[.][bodytemp]" )
{
    clear_all_state();
    player &dummy = get_avatar();

    const tripoint &pos = dummy.pos();

    get_map().ter_set( pos, t_water_dp );
    REQUIRE( get_map().has_flag( TFLAG_SWIMMABLE, pos ) );
    REQUIRE( get_map().has_flag( TFLAG_DEEP_WATER, pos ) );
    REQUIRE( !g->is_in_sunlight( pos ) );
    get_weather().weather_id = WEATHER_CLOUDY;

    dummy.drench( 100, { bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ), bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ),
                         bodypart_str_id( "arm_r" ), bodypart_str_id( "head" ), bodypart_str_id( "eyes" ), bodypart_str_id( "mouth" ),
                         bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "hand_l" ), bodypart_str_id( "hand_r" )
                       }, true );

    SECTION( "Nude target temperatures." ) {
        test_water_temperature_spread( dummy, {{ 38, 53, 70, 86, 102, 118, 135 }} );
    }

    // Not supposed to be very protective under water
    SECTION( "Arctic gear target temperatures" ) {
        equip_clothing( dummy, arctic_clothing );
        test_water_temperature_spread( dummy, {{ 25, 42, 57, 76, 96, 112, 128 }} );
    }

    // Should keep warmth under water
    SECTION( "Swimming gear target temperatures" ) {
        equip_clothing( dummy, { "wetsuit_hood", "wetsuit" } );
        test_water_temperature_spread( dummy, {{ 37, 54, 69, 86, 102, 118, 134 }} );
    }
}

static void hypothermia_check( player &p, int water_temperature, time_duration expected_time,
                               int expected_temperature )
{
    get_weather().water_temperature = units::from_fahrenheit( water_temperature );
    get_weather().clear_temp_cache();
    int expected_turns = to_turns<int>( expected_time );
    int lower_bound = expected_turns * 0.8f;
    int upper_bound = expected_turns * 1.2f;

    int actual_time;
    for( actual_time = 0; actual_time < upper_bound * 2; actual_time++ ) {
        p.update_bodytemp( get_map(), get_weather() );
        if( get_temp_cur( p, body_part_head ) <= expected_temperature ) {
            break;
        }
    }

    CHECK( actual_time >= lower_bound );
    CHECK( actual_time <= upper_bound );
    CHECK( get_temp_cur( p, body_part_head ) <= expected_temperature );
}

TEST_CASE( "Water hypothermia check.", "[.][bodytemp]" )
{
    clear_all_state();
    player &dummy = get_avatar();

    const tripoint &pos = dummy.pos();

    get_map().ter_set( pos, t_water_dp );
    REQUIRE( get_map().has_flag( TFLAG_SWIMMABLE, pos ) );
    REQUIRE( get_map().has_flag( TFLAG_DEEP_WATER, pos ) );
    REQUIRE( !g->is_in_sunlight( pos ) );
    get_weather().weather_id = WEATHER_CLOUDY;

    dummy.drench( 100, { bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ), bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ),
                         bodypart_str_id( "arm_r" ), bodypart_str_id( "head" ), bodypart_str_id( "eyes" ), bodypart_str_id( "mouth" ),
                         bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "hand_l" ), bodypart_str_id( "hand_r" )
                       }, true );

    SECTION( "Cold" ) {
        hypothermia_check( dummy, units::celsius_to_fahrenheit( 20 ), 5_minutes, BODYTEMP_COLD );
    }

    SECTION( "Very cold" ) {
        hypothermia_check( dummy, units::celsius_to_fahrenheit( 10 ), 5_minutes, BODYTEMP_VERY_COLD );
    }

    SECTION( "Freezing" ) {
        hypothermia_check( dummy, units::celsius_to_fahrenheit( 0 ), 5_minutes, BODYTEMP_FREEZING );
    }
}

TEST_CASE( "player_move_through_vehicle_holes" )
{

    clear_all_state();
    player &dummy = get_avatar();

    const tripoint &pos = dummy.pos();

    get_map().add_vehicle( vproto_id( "apc" ), pos + tripoint( 2, -1, 0 ), -45_degrees, 0, 0 );

    REQUIRE( get_avatar().pos() == pos );

    avatar_action::move( get_avatar(), get_map(), point_north_west );

    CHECK( get_avatar().pos() == pos );

}
