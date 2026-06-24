#include "veh_type.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ammo.h"
#include "assign.h"
#include "calendar.h"
#include "cata_utility.h"
#include "character_functions.h"
#include "color.h"
#include "debug.h"
#include "flag.h"
#include "game_constants.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "hsv_color.h"
#include "init.h"
#include "item.h"
#include "item_group.h"
#include "itype.h"
#include "json.h"
#include "mapdata.h"
#include "output.h"
#include "player.h"
#include "requirements.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "type_id_implement.h"
#include "units.h"
#include "units_utility.h"
#include "value_ptr.h"
#include "vehicle.h"
#include "vehicle_palette.h"
#include "vehicle_part.h"
#include "vehicle_group.h"
#include "weighted_list.h"

class npc;

std::unordered_map<vproto_id, vehicle_prototype> vtypes;

namespace
{
generic_factory<vpart_info> all_vparts( "Vehicle Parts" );
}

IMPLEMENT_STRING_AND_INT_IDS( vpart_info, all_vparts );

// GENERAL GUIDELINES
// To determine mount position for parts (dx, dy), check this scheme:
//         orthogonal dir left: (Y-)
//                ^
//  back: X-   -------> forward dir: X+
//                v
//         orthogonal dir right (Y+)
//
// i.e, if you want to add a part to the back from the center of vehicle,
// use dx = -1, dy = 0;
// for the part 1 tile forward and two tiles left from the center of vehicle,
// use dx = 1, dy = -2.
//
// Internal parts should be added after external on the same mount point, i.e:
//  part {"x": 0, "y": 1, "part": "seat"},      // put a seat (it's external)
//  part {"x": 0, "y": 1, "part": "controls"},  // put controls for driver here
//  part {"x": 0, "y": 1, "seatbelt"}           // also, put a seatbelt here
// To determine, what parts can be external, and what can not, check
// vehicle_parts.json
// If you use wrong config, installation of part will fail

static const std::unordered_map<std::string, vpart_bitflags> vpart_bitflag_map = {
    { "ARMOR", VPFLAG_ARMOR },
    { "EVENTURN", VPFLAG_EVENTURN },
    { "ODDTURN", VPFLAG_ODDTURN },
    { "CONE_LIGHT", VPFLAG_CONE_LIGHT },
    { "WIDE_CONE_LIGHT", VPFLAG_WIDE_CONE_LIGHT },
    { "HALF_CIRCLE_LIGHT", VPFLAG_HALF_CIRCLE_LIGHT },
    { "CIRCLE_LIGHT", VPFLAG_CIRCLE_LIGHT },
    { "BOARDABLE", VPFLAG_BOARDABLE },
    { "AISLE", VPFLAG_AISLE },
    { "CONTROLS", VPFLAG_CONTROLS },
    { "OBSTACLE", VPFLAG_OBSTACLE },
    { "OPAQUE", VPFLAG_OPAQUE },
    { "OPENABLE", VPFLAG_OPENABLE },
    { "SEATBELT", VPFLAG_SEATBELT },
    { "WHEEL", VPFLAG_WHEEL },
    { "ROTOR", VPFLAG_ROTOR },
    { "FLOATS", VPFLAG_FLOATS },
    { "DOME_LIGHT", VPFLAG_DOME_LIGHT },
    { "AISLE_LIGHT", VPFLAG_AISLE_LIGHT },
    { "ATOMIC_LIGHT", VPFLAG_ATOMIC_LIGHT },
    { "ALTERNATOR", VPFLAG_ALTERNATOR },
    { "ENGINE", VPFLAG_ENGINE },
    { "FRIDGE", VPFLAG_FRIDGE },
    { "FREEZER", VPFLAG_FREEZER },
    { "LIGHT", VPFLAG_LIGHT },
    { "WINDOW", VPFLAG_WINDOW },
    { "CURTAIN", VPFLAG_CURTAIN },
    { "CARGO", VPFLAG_CARGO },
    { "INTERNAL", VPFLAG_INTERNAL },
    { "SOLAR_PANEL", VPFLAG_SOLAR_PANEL },
    { "WIND_TURBINE", VPFLAG_WIND_TURBINE },
    { "SPACE_HEATER", VPFLAG_SPACE_HEATER, },
    { "COOLER", VPFLAG_COOLER, },
    { "WATER_WHEEL", VPFLAG_WATER_WHEEL },
    { "RECHARGE", VPFLAG_RECHARGE },
    { "VISION", VPFLAG_EXTENDS_VISION },
    { "ENABLED_DRAINS_EPOWER", VPFLAG_ENABLED_DRAINS_EPOWER },
    { "AUTOCLAVE", VPFLAG_AUTOCLAVE },
    { "FLUIDTANK", VPFLAG_FLUIDTANK },
    { "REACTOR", VPFLAG_REACTOR },
    { "RAIL", VPFLAG_RAIL },
    { "TURRET_CONTROLS", VPFLAG_TURRET_CONTROLS },
    { "AUTOLOADER", VPFLAG_AUTOLOADER },
    { "ROOF", VPFLAG_ROOF },
    { "BALLOON", VPFLAG_BALLOON },
    { "WING", VPFLAG_WING },
    { "POWERED_BY_ENGINE", VPFLAG_POWERED_BY_ENGINE },
    { "PROPELLER", VPFLAG_PROPELLER },
    { "EXTENDABLE", VPFLAG_EXTENDABLE },
    { "NOCOLLIDE", VPFLAG_NOCOLLIDE },
    { "NOCOLLIDEABOVE", VPFLAG_NOCOLLIDEABOVE },
    { "NOCOLLIDEBELOW", VPFLAG_NOCOLLIDEBELOW },
    { "NOSMASH", VPFLAG_NOSMASH },
    { "NOFIELDS", VPFLAG_NOFIELDS },
    { "DROPPER", VPFLAG_DROPPER },
    { "LADDER", VPFLAG_LADDER }
};

auto vpart_rotating_light::arc_width() const -> units::angle
{
    return units::from_degrees( std::max( arc, 1 ) );
}

auto vpart_rotating_light::beam_count() const -> int
{
    return std::max( beams, 1 );
}

auto vpart_rotating_light::beam_spacing() const -> units::angle
{
    return units::from_degrees( 360.0 / static_cast<double>( beam_count() ) );
}

auto vpart_rotating_light::direction_at( const units::angle base_direction,
        const time_point turn ) const -> units::angle
{
    const auto period_turns = std::max( to_turns<int>( period ), 1 );
    const auto step_magnitude = std::abs( static_cast<long long>( step ) );
    const auto rotated_beam_step = step_magnitude * beam_count();
    const auto state_count = step_magnitude > 0 ?
                             static_cast<int>( 360 / std::gcd( 360LL, rotated_beam_step ) ) : 1;
    const auto elapsed_turns = std::max( to_turns<int>( turn - calendar::turn_zero ), 0 );
    const auto step_index = elapsed_turns / period_turns % state_count;
    const auto rotation_degrees = phase + step * step_index;

    return base_direction + units::from_degrees( rotation_degrees );
}

static auto load_rotating_light( const JsonObject &jo,
                                 std::optional<vpart_rotating_light> &rotating_light ) -> void
{
    if( !jo.has_member( "rotating_light" ) ) {
        return;
    }

    if( jo.has_null( "rotating_light" ) ) {
        rotating_light.reset();
        return;
    }

    if( !jo.has_object( "rotating_light" ) ) {
        jo.throw_error( "rotating_light must be an object or null", "rotating_light" );
    }

    auto data = rotating_light.value_or( vpart_rotating_light{} );
    auto jrot = jo.get_object( "rotating_light" );

    assign( jrot, "arc", data.arc, false, 1, 360 );
    assign( jrot, "step", data.step );
    assign( jrot, "phase", data.phase );
    assign( jrot, "beams", data.beams, false, 1, 16 );

    if( jrot.has_int( "period" ) ) {
        data.period = time_duration::from_turns( jrot.get_int( "period" ) );
    } else if( jrot.has_string( "period" ) ) {
        data.period = read_from_json_string<time_duration>( *jrot.get_raw( "period" ),
                      time_duration::units );
    }

    if( data.period <= 0_turns ) {
        jrot.throw_error( "period must be positive", "period" );
    }

    rotating_light = data;
}

static const std::vector<std::pair<std::string, int>> standard_terrain_mod = {{
        { "FLAT", 4 }, { "ROAD", 2 }
    }
};
static const std::vector<std::pair<std::string, int>> rigid_terrain_mod = {{
        { "FLAT", 6 }, { "ROAD", 3 }
    }
};
static const std::vector<std::pair<std::string, int>> racing_terrain_mod = {{
        { "FLAT", 5 }, { "ROAD", 2 }
    }
};
static const std::vector<std::pair<std::string, int>> off_road_terrain_mod = {{
        { "FLAT", 3 }, { "ROAD", 1 }
    }
};
static const std::vector<std::pair<std::string, int>> treads_terrain_mod = {{
        { "FLAT", 3 }
    }
};
static const std::vector<std::pair<std::string, int>> rail_terrain_mod = {{
        { "RAIL", 8 }
    }
};

static void parse_vp_reqs( const JsonObject &obj, const std::string &id, const std::string &key,
                           std::vector<std::pair<requirement_id, int>> &reqs,
                           std::map<skill_id, int> &skills, int &moves )
{

    if( !obj.has_object( key ) ) {
        return;
    }
    JsonObject src = obj.get_object( key );

    auto sk = src.get_array( "skills" );
    if( !sk.empty() ) {
        skills.clear();
    }
    for( JsonArray cur : sk ) {
        skills.emplace( skill_id( cur.get_string( 0 ) ), cur.size() >= 2 ? cur.get_int( 1 ) : 1 );
    }

    if( src.has_int( "time" ) ) {
        moves = src.get_int( "time" );
    } else if( src.has_string( "time" ) ) {
        moves = to_moves<int>( read_from_json_string<time_duration>( *src.get_raw( "time" ),
                               time_duration::units ) );
    }

    if( src.has_string( "using" ) ) {
        reqs = { { requirement_id( src.get_string( "using" ) ), 1 } };
    } else if( src.has_array( "using" ) ) {
        reqs.clear();
        for( JsonArray cur : src.get_array( "using" ) ) {
            reqs.emplace_back( requirement_id( cur.get_string( 0 ) ), cur.get_int( 1 ) );
        }
    } else {
        reqs.clear();
        // Construct a requirement to capture "components", "qualities", and
        // "tools" that might be listed.
        const requirement_id req_id( string_format( "inline_%s_%s", key.c_str(), id.c_str() ) );
        requirement_data::load_requirement( src, req_id );
        reqs.emplace_back( req_id, 1 );
    }
}

/**
 * Reads engine info from a JsonObject.
 */
void vpart_info::load_engine( std::optional<vpslot_engine> &eptr, const JsonObject &jo,
                              const itype_id &fuel_type )
{
    vpslot_engine e_info{};
    if( eptr ) {
        e_info = *eptr;
    }
    assign( jo, "backfire_threshold", e_info.backfire_threshold );
    assign( jo, "backfire_freq", e_info.backfire_freq );
    assign( jo, "noise_factor", e_info.noise_factor );
    assign( jo, "damaged_power_factor", e_info.damaged_power_factor );
    assign( jo, "m2c", e_info.m2c );
    assign( jo, "muscle_power_factor", e_info.muscle_power_factor );
    auto excludes = jo.get_array( "exclusions" );
    if( !excludes.empty() ) {
        e_info.exclusions.clear();
        for( const std::string line : excludes ) {
            e_info.exclusions.push_back( line );
        }
    }
    auto fuel_opts = jo.get_array( "fuel_options" );
    if( !fuel_opts.empty() ) {
        e_info.fuel_opts.clear();
        for( const std::string line : fuel_opts ) {
            e_info.fuel_opts.emplace_back( line );
        }
    } else if( e_info.fuel_opts.empty() && fuel_type != itype_id( "null" ) ) {
        e_info.fuel_opts.push_back( fuel_type );
    }
    eptr = e_info;
    assert( eptr );
}

void vpart_info::load_rotor( std::optional<vpslot_rotor> &roptr, const JsonObject &jo )
{
    vpslot_rotor rotor_info{};
    if( roptr ) {
        rotor_info = *roptr;
    }
    assign( jo, "rotor_diameter", rotor_info.rotor_diameter );
    roptr = rotor_info;
    assert( roptr );
}

void vpart_info::load_propeller( std::optional<vpslot_propeller> &proptr, const JsonObject &jo )
{
    vpslot_propeller propeller_info{};
    if( proptr ) {
        propeller_info = *proptr;
    }
    assign( jo, "propeller_diameter", propeller_info.propeller_diameter );
    proptr = propeller_info;
    assert( proptr );
}

void vpart_info::load_wing( std::optional<vpslot_wing> &wptr, const JsonObject &jo )
{
    vpslot_wing wing_info{};
    if( wptr ) {
        wing_info = *wptr;
    }
    assign( jo, "lift_coff", wing_info.lift_coff );
    wptr = wing_info;
    assert( wptr );
}

void vpart_info::load_balloon( std::optional<vpslot_balloon> &balptr, const JsonObject &jo )
{
    vpslot_balloon balloon_info{};
    if( balptr ) {
        balloon_info = *balptr;
    }
    assign( jo, "height", balloon_info.height );
    balptr = balloon_info;
    assert( balptr );
}

void vpart_info::load_ladder( std::optional<vpslot_ladder> &ladptr, const JsonObject &jo )
{
    vpslot_ladder lad_info{};
    if( ladptr ) {
        lad_info = *ladptr;
    }
    assign( jo, "length", lad_info.length );
    ladptr = lad_info;
    assert( ladptr );
}

void vpart_info::load_wheel( std::optional<vpslot_wheel> &whptr, const JsonObject &jo )
{
    vpslot_wheel wh_info{};
    if( whptr ) {
        wh_info = *whptr;
    }
    assign( jo, "rolling_resistance", wh_info.rolling_resistance );
    assign( jo, "contact_area", wh_info.contact_area );
    if( !jo.has_member( "copy-from" ) ) {
        // if flag presented, it is already set
        wh_info.terrain_mod = standard_terrain_mod;
        wh_info.or_rating = 0.5f;
    }
    if( jo.has_string( "wheel_type" ) ) {
        const std::string wheel_type = jo.get_string( "wheel_type" );
        if( wheel_type == "rigid" ) {
            wh_info.terrain_mod = rigid_terrain_mod;
            wh_info.or_rating = 0.1;
        } else if( wheel_type == "off-road" ) {
            wh_info.terrain_mod = off_road_terrain_mod;
            wh_info.or_rating = 0.7;
        } else if( wheel_type == "racing" ) {
            wh_info.terrain_mod = racing_terrain_mod;
            wh_info.or_rating = 0.3;
        } else if( wheel_type == "treads" ) {
            wh_info.terrain_mod = treads_terrain_mod;
            wh_info.or_rating = 0.9;
        } else if( wheel_type == "rail" ) {
            wh_info.terrain_mod = rail_terrain_mod;
            wh_info.or_rating = 0.05;
        } else if( wheel_type == "standard" ) {
            wh_info.terrain_mod = standard_terrain_mod;
            wh_info.or_rating = 0.5;
        } else {
            jo.throw_error( string_format( "Unknown wheel type '%s'", wheel_type ), "wheel_type" );
        }
    }

    whptr = wh_info;
    assert( whptr );
}

void vpart_info::load_workbench( std::optional<vpslot_workbench> &wbptr, const JsonObject &jo )
{
    vpslot_workbench wb_info{};
    if( wbptr ) {
        wb_info = *wbptr;
    }

    JsonObject wb_jo = jo.get_object( "workbench" );

    assign( wb_jo, "multiplier", wb_info.multiplier );
    assign( wb_jo, "mass", wb_info.allowed_mass );
    assign( wb_jo, "volume", wb_info.allowed_volume );

    wbptr = wb_info;
    assert( wbptr );
}

void vpart_info::load_crafter( std::optional<vpslot_crafter> &craftptr, const JsonObject &jo )
{
    vpslot_crafter craft_info{};
    if( craftptr ) {
        craft_info = *craftptr;
    }

    JsonArray tools = jo.get_array( "integrated_tools" );

    for( std::string cur : tools ) {
        craft_info.fake_parts.emplace_back( itype_id( cur ) );
    }

    craftptr = craft_info;
    assert( craftptr );
}

void vpart_info::load_converter( std::optional<vpslot_converter> &convertptr, const JsonObject &jo )
{
    vpslot_converter convert_info{};
    if( convertptr ) {
        convert_info = *convertptr;
    }
    JsonObject converter = jo.get_object( "converter" );
    assign( converter, "input", convert_info.input );
    assign( converter, "input_step", convert_info.input_step );
    assign( converter, "output", convert_info.output );
    assign( converter, "output_step", convert_info.output_step );
    assign( converter, "max_steps", convert_info.max_steps );
    assign( converter, "charge_cost", convert_info.charge_cost );
    convertptr = convert_info;
    assert( convertptr );
}

void vpart_info::load_vehicle_parts( const JsonObject &jo, const std::string &src )
{
    all_vparts.load( jo, src );
}

/**
 * Reads in a vehicle part from a JsonObject.
 */
void vpart_info::load( const JsonObject &jo, const std::string &src )
{

    assign( jo, "name", name_ );
    assign( jo, "item", item );
    assign( jo, "location", location );
    assign( jo, "durability", durability );
    assign( jo, "damage_modifier", dmg_mod );
    assign( jo, "energy_consumption", energy_consumption );
    assign( jo, "power", power );
    assign( jo, "epower", epower );
    assign( jo, "emissions", emissions );
    assign( jo, "fuel_type", fuel_type );
    assign( jo, "default_ammo", default_ammo );
    assign( jo, "folded_volume", folded_volume );
    assign( jo, "size", size );
    assign( jo, "difficulty", difficulty );
    assign( jo, "bonus", bonus );
    assign( jo, "cargo_weight_modifier", cargo_weight_modifier );
    assign( jo, "weight_modifier", weight_modifier );
    assign( jo, "flags", flags );
    assign( jo, "description", description );

    load_rotating_light( jo, rotating_light );

    assign( jo, "comfort", comfort );
    assign( jo, "floor_bedding_warmth", floor_bedding_warmth );
    assign( jo, "bonus_fire_warmth_feet", bonus_fire_warmth_feet );
    assign( jo, "default_color", default_color );
    assign( jo, "light_color", light_color );

    if( jo.has_member( "transform_terrain" ) ) {
        JsonObject jttd = jo.get_object( "transform_terrain" );
        for( const std::string pre_flag : jttd.get_array( "pre_flags" ) ) {
            transform_terrain.pre_flags.emplace( pre_flag );

        }
        transform_terrain.diggable = jttd.get_bool( "diggable", false );
        transform_terrain.post_terrain = jttd.get_string( "post_terrain", "t_null" );
        transform_terrain.post_furniture = jttd.get_string( "post_furniture", "f_null" );
        transform_terrain.post_field = jttd.get_string( "post_field", "fd_null" );
        transform_terrain.post_field_intensity = jttd.get_int( "post_field_intensity", 0 );
        if( jttd.has_int( "post_field_age" ) ) {
            transform_terrain.post_field_age = time_duration::from_turns(
                                                   jttd.get_int( "post_field_age" ) );
        } else if( jttd.has_string( "post_field_age" ) ) {
            transform_terrain.post_field_age = read_from_json_string<time_duration>(
                                                   *jttd.get_raw( "post_field_age" ), time_duration::units );
        } else {
            transform_terrain.post_field_age = 0_turns;
        }
    }

    if( jo.has_member( "requirements" ) ) {
        auto reqs = jo.get_object( "requirements" );

        parse_vp_reqs( reqs, id.str(), "install", install_reqs, install_skills,
                       install_moves );
        parse_vp_reqs( reqs, id.str(), "removal", removal_reqs, removal_skills,
                       removal_moves );
        parse_vp_reqs( reqs, id.str(), "repair",  repair_reqs,  repair_skills,
                       repair_moves );
    }

    if( jo.has_member( "symbol" ) ) {
        sym = jo.get_string( "symbol" )[ 0 ];
    }
    if( jo.has_member( "broken_symbol" ) ) {
        sym_broken = jo.get_string( "broken_symbol" )[ 0 ];
    }

    // This was used in old copy from logic, force looks like of the parent
    if( jo.has_string( "copy-from" ) ) {
        looks_like = jo.get_string( "copy-from" );
    }

    jo.read( "looks_like", looks_like );


    if( jo.has_member( "color" ) ) {
        color = color_from_string( jo.get_string( "color" ) );
    }
    if( jo.has_member( "broken_color" ) ) {
        color_broken = color_from_string( jo.get_string( "broken_color" ) );
    }

    if( jo.has_member( "breaks_into" ) ) {
        breaks_into_group = item_group::load_item_group( jo.get_member( "breaks_into" ), "collection" );
    }

    auto qual = jo.get_array( "qualities" );
    if( !qual.empty() ) {
        qualities.clear();
        for( JsonArray pair : qual ) {
            qualities[ quality_id( pair.get_string( 0 ) ) ] = pair.get_int( 1 );
        }
    }

    if( jo.has_member( "damage_reduction" ) ) {
        JsonObject dred = jo.get_object( "damage_reduction" );
        damage_reduction = load_resistances_instance( dred );
    }

    if( has_flag( "ENGINE" ) ) {
        load_engine( engine_info, jo, fuel_type );
    }

    if( has_flag( "WHEEL" ) ) {
        load_wheel( wheel_info, jo );
    }

    if( has_flag( "ROTOR" ) ) {
        load_rotor( rotor_info, jo );
    }

    if( has_flag( "PROPELLER" ) ) {
        load_propeller( propeller_info, jo );
    }

    if( has_flag( "BALLOON" ) ) {
        load_balloon( balloon_info, jo );
    }

    if( has_flag( "LADDER" ) ) {
        load_ladder( ladder_info, jo );
    }

    if( has_flag( "WING" ) ) {
        load_wing( wing_info, jo );
    }

    if( has_flag( "CONVERTER" ) ) {
        load_converter( converter_info, jo );
    }

    if( has_flag( "WORKBENCH" ) ) {
        load_workbench( workbench_info, jo );
    }

    if( has_flag( "CRAFTER" ) ) {
        load_crafter( crafter_info, jo );
    }

    if( jo.has_array( "shapes" ) ) {
        for( JsonObject obj : jo.get_array( "shapes" ) ) {
            vpart_info shape = vpart_info( *this );
            if( shape.id.str() == "" ) {
                shape.id = vpart_id( jo.get_string( "abstract" ) + "_" + obj.get_string( "direction" ) );
            } else {
                shape.id = vpart_id( shape.id.str() + "_" + obj.get_string( "direction" ) );
            }
            if( shape.looks_like != "" ) {
                shape.looks_like = shape.looks_like + "_" + obj.get_string( "direction" );
            }
            if( obj.has_string( "symbol" ) ) {
                shape.sym = obj.get_string( "symbol" )[0];
            }
            if( obj.has_string( "symbol_broken" ) ) {
                shape.sym = obj.get_string( "symbol_broken" )[0];
            }
            if( obj.has_string( "looks_like" ) ) {
                shape.looks_like = obj.get_string( "looks_like" );
            }
            all_vparts.insert( shape );
        }
    }
    // Unused field that some mods use for some reason
    // TODO: Implement or delete
    jo.get_string_array( "categories" );
}

void vpart_info::set_flag( const std::string &flag )
{
    flags.insert( flag );
    const auto iter = vpart_bitflag_map.find( flag );
    if( iter != vpart_bitflag_map.end() ) {
        bitflags.set( iter->second );
    }
}

void vpart_info::finalize_all()
{
    all_vparts.finalize();
    for( const vpart_info &vp_info : all_vparts.get_all() ) {
        const_cast<vpart_info &>( vp_info ).finalize();
    }
}

void vpart_info::finalize()
{
    if( folded_volume > 0_ml ) {
        set_flag( "FOLDABLE" );
    }

    for( const auto &f : flags ) {
        auto b = vpart_bitflag_map.find( f );
        if( b != vpart_bitflag_map.end() ) {
            bitflags.set( b->second );
        }
    }

    // Calculate and cache z-ordering based off of location
    // list_order is used when inspecting the vehicle
    if( location == "on_roof" ) {
        z_order = 9;
        list_order = 3;
    } else if( location == "on_cargo" ) {
        z_order = 8;
        list_order = 6;
    } else if( location == "center" ) {
        z_order = 7;
        list_order = 7;
    } else if( location == "under" ) {
        // Have wheels show up over frames
        z_order = 6;
        list_order = 10;
    } else if( location == "structure" ) {
        z_order = 5;
        list_order = 1;
    } else if( location == "engine_block" ) {
        // Should be hidden by frames
        z_order = 4;
        list_order = 8;
    } else if( location == "on_battery_mount" ) {
        // Should be hidden by frames
        z_order = 3;
        list_order = 10;
    } else if( location == "fuel_source" ) {
        // Should be hidden by frames
        z_order = 3;
        list_order = 9;
    } else if( location == "roof" ) {
        // Shouldn't be displayed
        z_order = -1;
        list_order = 4;
    } else if( location == "armor" ) {
        // Shouldn't be displayed (the color is used, but not the symbol)
        z_order = -2;
        list_order = 2;
    } else {
        // Everything else
        z_order = 0;
        list_order = 5;
    }
    // add the base item to the installation requirements
    // TODO: support multiple/alternative base items
    requirement_data ins;
    ins.components.push_back( { { { item, 1 } } } );

    const requirement_id ins_id( std::string( "inline_vehins_base_" ) + id.str() );
    requirement_data::save_requirement( ins, ins_id );
    install_reqs.emplace_back( ins_id, 1 );

    if( removal_moves < 0 ) {
        removal_moves = install_moves / 2;
    }
}

void vpart_info::check_consistency()
{
    all_vparts.check();
}

void vpart_info::check() const
{
    for( const auto &[skill, level] : install_skills ) {
        if( !skill.is_valid() ) {
            debugmsg( "vehicle part %s has unknown install skill %s", id.c_str(), skill.c_str() );
        }
    }

    for( const auto &[skill, level] : removal_skills ) {
        if( !skill.is_valid() ) {
            debugmsg( "vehicle part %s has unknown removal skill %s", id.c_str(), skill.c_str() );
        }
    }

    for( const auto &[skill, level] : repair_skills ) {
        if( !skill.is_valid() ) {
            debugmsg( "vehicle part %s has unknown repair skill %s", id.c_str(), skill.c_str() );
        }
    }

    for( const auto &[skill, multiplier] : install_reqs ) {
        if( !skill.is_valid() || multiplier <= 0 ) {
            debugmsg( "vehicle part %s has unknown or incorrectly specified install requirements %s",
                      id.c_str(), skill.c_str() );
        }
    }

    for( const auto &[req, multiplier] : install_reqs ) {
        if( !( req.is_null() || req.is_valid() ) || multiplier < 0 ) {
            debugmsg( "vehicle part %s has unknown or incorrectly specified removal requirements %s",
                      id.c_str(), req.c_str() );
        }
    }

    for( const auto &[req, multiplier] : repair_reqs ) {
        if( !( req.is_null() || req.is_valid() ) || multiplier < 0 ) {
            debugmsg( "vehicle part %s has unknown or incorrectly specified repair requirements %s",
                      id.c_str(), req.c_str() );
        }
    }

    if( install_moves < 0 ) {
        debugmsg( "vehicle part %s has negative installation time", id.c_str() );
    }

    if( removal_moves < 0 ) {
        debugmsg( "vehicle part %s has negative removal time", id.c_str() );
    }

    if( !item_group::group_is_defined( breaks_into_group ) ) {
        debugmsg( "Vehicle part %s breaks into non-existent item group %s.",
                  id.c_str(), breaks_into_group.c_str() );
    }
    if( sym == 0 ) {
        debugmsg( "vehicle part %s does not define a symbol", id.c_str() );
    }
    if( sym_broken == 0 ) {
        debugmsg( "vehicle part %s does not define a broken symbol", id.c_str() );
    }
    if( durability <= 0 ) {
        debugmsg( "vehicle part %s has zero or negative durability", id.c_str() );
    }
    if( dmg_mod < 0 ) {
        debugmsg( "vehicle part %s has negative damage modifier", id.c_str() );
    }
    if( folded_volume < 0_ml ) {
        debugmsg( "vehicle part %s has negative folded volume", id.c_str() );
    }
    if( has_flag( "FOLDABLE" ) && folded_volume == 0_ml ) {
        debugmsg( "vehicle part %s has folding part with zero folded volume", name() );
    }
    if( !default_ammo.is_valid() ) {
        debugmsg( "vehicle part %s has undefined default ammo %s", id.c_str(), item.c_str() );
    }
    if( size < 0_ml ) {
        debugmsg( "vehicle part %s has negative size", id.c_str() );
    }
    if( !item.is_valid() ) {
        debugmsg( "vehicle part %s uses undefined item %s", id.c_str(), item.c_str() );
    }
    const itype &base_item_type = *item;
    // Fuel type errors are serious and need fixing now
    if( !fuel_type.is_valid() ) {
        throw JsonError( string_format( "vehicle part %s uses undefined fuel %s", id.c_str(),
                                        fuel_type.c_str() ) );
    } else if( fuel_type && !fuel_type->fuel &&
               ( !base_item_type.container || !base_item_type.container->watertight ) ) {
        // HACK: Tanks are allowed to specify non-fuel "fuel",
        // because currently legacy blazemod uses it as a hack to restrict content types
        throw JsonError(
            string_format( "non-tank vehicle part %s uses non-fuel item %s as fuel, setting to null",
                           id.c_str(), fuel_type.c_str() ) );
    }
    if( has_flag( "TURRET" ) && !base_item_type.gun ) {
        debugmsg( "vehicle part %s has the TURRET flag, but is not made from a gun item", id.c_str() );
    }
    if( !emissions.empty() && !has_flag( "EMITTER" ) ) {
        debugmsg( "vehicle part %s has emissions set, but the EMITTER flag is not set", id.c_str() );
    }
    if( has_flag( "EMITTER" ) ) {
        if( emissions.empty() ) {
            debugmsg( "vehicle part %s has the EMITTER flag, but no emissions were set", id.c_str() );
        } else {
            for( const emit_id &e : emissions ) {
                if( !e.is_valid() ) {
                    debugmsg( "vehicle part %s has the EMITTER flag, but invalid emission %s was set",
                              id.c_str(), e.str().c_str() );
                }
            }
        }
    }
    if( has_flag( "ADVANCED_PLANTER" ) && !has_flag( "PLANTER" ) ) {
        debugmsg( "vehicle part %s has ADVANCED_PLANTER flag, but is missing PLANTER flag.",
                  id.c_str() );
    }
    if( has_flag( "WHEEL" ) && !base_item_type.wheel ) {
        debugmsg( "vehicle part %s has the WHEEL flag, but base item %s is not a wheel.  "
                  "THIS WILL CRASH!", id.str(), item.str() );
    }
    if( has_flag( "RAIL" ) && !has_flag( "WHEEL" ) ) {
        debugmsg( "vehicle part %s has RAIL flag, but is missing WHEEL flag.", id.c_str() );
    }
    for( auto &q : qualities ) {
        if( !q.first.is_valid() ) {
            debugmsg( "vehicle part %s has undefined tool quality %s", id.c_str(), q.first.c_str() );
        }
    }
    if( has_flag( VPFLAG_ENABLED_DRAINS_EPOWER ) && epower == 0 ) {
        debugmsg( "%s is set to drain epower, but has epower == 0", id.c_str() );
    }
    if( has_flag( VPFLAG_NOCOLLIDEABOVE ) && !has_flag( VPFLAG_NOCOLLIDE ) ) {
        debugmsg( "%s has flag NOCOLLIDEABOVE, but does not have the prerequisite flag NOCOLLIDE",
                  id.c_str() );
    }
    if( has_flag( VPFLAG_NOCOLLIDEBELOW ) && !has_flag( VPFLAG_NOCOLLIDE ) ) {
        debugmsg( "%s has flag NOCOLLIDEBELOW, but does not have the prerequisite flag NOCOLLIDE",
                  id.c_str() );
    }
    // Parts with non-zero epower must have a flag that affects epower usage
    static const std::vector<std::string> handled = {{
            "ENABLED_DRAINS_EPOWER", "SECURITY", "ENGINE",
            "ALTERNATOR", "SOLAR_PANEL", "POWER_TRANSFER",
            "PERPETUAL", "REACTOR", "WIND_TURBINE", "WATER_WHEEL"
        }
    };
    if( epower != 0 &&
    std::ranges::none_of( handled, [*this]( const std::string & flag ) {
    return has_flag( flag );
    } ) ) {
        std::string warnings_are_good_docs = enumerate_as_string( handled );
        debugmsg( "%s has non-zero epower, but lacks a flag that would make it affect epower (one of %s)",
                  id.c_str(), warnings_are_good_docs.c_str() );
    }
}

void vpart_info::reset()
{
    all_vparts.reset();
}

const std::vector<vpart_info> &vpart_info::get_all()
{
    return all_vparts.get_all();
}

std::string vpart_info::name() const
{
    if( name_.empty() ) {
        return item::nname( item );
    } else {
        return name_.translated();
    }
}

int vpart_info::format_description( std::string &msg, const nc_color &format_color,
                                    int width ) const
{
    int lines = 1;
    msg += _( "<color_white>Description</color>\n" );
    msg += "> <color_" + string_from_color( format_color ) + ">";

    std::string long_descrip;
    if( !description.empty() ) {
        long_descrip += description.translated();
    }
    for( const auto &flagid : flags ) {
        if( flagid == "ALARMCLOCK" || flagid == "WATCH" ) {
            continue;
        }
        json_flag flag = json_flag::get( flagid );
        if( !flag.info().empty() ) {
            if( !long_descrip.empty() ) {
                long_descrip += "  ";
            }
            long_descrip += _( flag.info() );
        }
    }
    if( ( has_flag( "SEAT" ) || has_flag( "BED" ) ) && !has_flag( "BELTABLE" ) ) {
        json_flag nobelt = json_flag::get( "NONBELTABLE" );
        long_descrip += "  " + _( nobelt.info() );
    }
    if( has_flag( "BOARDABLE" ) && has_flag( "OPENABLE" ) ) {
        json_flag nobelt = json_flag::get( "DOOR" );
        long_descrip += "  " + _( nobelt.info() );
    }
    if( has_flag( "TURRET" ) ) {
        //TODO!: push up
        class::item &base = *item::spawn_temporary( item );
        if( base.ammo_required() && !base.ammo_remaining() ) {
            itype_id default_ammo = base.magazine_current() ? base.common_ammo_default() : base.ammo_default();
            base.ammo_set( default_ammo );
        }
        long_descrip += string_format( _( "\nRange: %1$5d     Damage: %2$5.0f" ),
                                       base.gun_range( true ),
                                       base.gun_damage().total_damage() );
    }

    if( !long_descrip.empty() ) {
        long_descrip = replace_colors( long_descrip );
        const auto wrap_descrip = foldstring( long_descrip, width );
        msg += wrap_descrip[0];
        for( size_t i = 1; i < wrap_descrip.size(); i++ ) {
            msg += "\n  " + wrap_descrip[i];
        }
        msg += "</color>\n";
        lines += wrap_descrip.size();
    }

    // borrowed from item.cpp and adjusted
    const quality_id quality_jack( "JACK" );
    const quality_id quality_lift( "LIFT" );
    for( const auto &qual : qualities ) {
        msg += string_format(
                   _( "Has level <color_cyan>%1$d %2$s</color> quality" ), qual.second, qual.first.obj().name );
        if( qual.first == quality_jack || qual.first == quality_lift ) {
            msg += string_format( _( " and is rated at <color_cyan>%1$d %2$s</color>" ),
                                  static_cast<int>( convert_weight( qual.second * TOOL_LIFT_FACTOR ) ),
                                  weight_units() );
        }
        msg += ".\n";
        lines += 1;
    }
    return lines;
}

requirement_data vpart_info::install_requirements() const
{
    return std::accumulate( install_reqs.begin(), install_reqs.end(), requirement_data(),
    []( const requirement_data & lhs, const std::pair<requirement_id, int> &rhs ) {
        return lhs + ( *rhs.first * rhs.second );
    } );
}

requirement_data vpart_info::removal_requirements() const
{
    return std::accumulate( removal_reqs.begin(), removal_reqs.end(), requirement_data(),
    []( const requirement_data & lhs, const std::pair<requirement_id, int> &rhs ) {
        return lhs + ( *rhs.first * rhs.second );
    } );
}

requirement_data vpart_info::repair_requirements() const
{
    return std::accumulate( repair_reqs.begin(), repair_reqs.end(), requirement_data(),
    []( const requirement_data & lhs, const std::pair<requirement_id, int> &rhs ) {
        return lhs + ( *rhs.first * rhs.second );
    } );
}

bool vpart_info::is_repairable() const
{
    return !repair_requirements().is_empty();
}

static int scale_time( const std::map<skill_id, int> &sk, int mv, const Character &who )
{
    if( sk.empty() ) {
        return mv;
    }

    const int lvl = std::accumulate( sk.begin(), sk.end(), 0, [&who]( int lhs,
    const std::pair<skill_id, int> &rhs ) {
        return lhs + std::max( std::min( who.get_skill_level( rhs.first ), MAX_SKILL ) - rhs.second,
                               0 );
    } );
    // 10% per excess level (reduced proportionally if >1 skill required) with max 50% reduction
    // 10% reduction per assisting NPC
    int time_norm = mv * ( 1.0 - std::min( static_cast<double>( lvl ) / sk.size() / 10.0, 0.5 ) )
                    * ( 10 - character_funcs::get_crafting_helpers( who, 3 ).size() ) / 10;
    time_norm += who.bonus_from_enchantments( time_norm,
                 enchantment_value_id( "CONSTRUCTION_SPEED_VEH" ) );
    return time_norm;
}

int vpart_info::install_time( const Character &who ) const
{
    return scale_time( install_skills, install_moves, who );
}

int vpart_info::removal_time( const Character &who ) const
{
    return scale_time( removal_skills, removal_moves, who );
}

int vpart_info::repair_time( const Character &who ) const
{
    return scale_time( repair_skills, repair_moves, who );
}

/**
 * @name Engine specific functions
 *
 */
float vpart_info::engine_backfire_threshold() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->backfire_threshold : false;
}

int vpart_info::engine_backfire_freq() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->backfire_freq : false;
}

int vpart_info::engine_muscle_power_factor() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->muscle_power_factor : false;
}

float vpart_info::engine_damaged_power_factor() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->damaged_power_factor : false;
}

int vpart_info::engine_noise_factor() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->noise_factor : false;
}

int vpart_info::engine_m2c() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->m2c : 0;
}

std::vector<std::string> vpart_info::engine_excludes() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->exclusions : std::vector<std::string>();
}

std::vector<itype_id> vpart_info::engine_fuel_opts() const
{
    return has_flag( VPFLAG_ENGINE ) ? engine_info->fuel_opts : std::vector<itype_id>();
}

/**
 * @name Wheel specific functions
 *
 */
float vpart_info::wheel_rolling_resistance() const
{
    // caster wheels return 29, so if a part rolls worse than a caster wheel...
    return has_flag( VPFLAG_WHEEL ) ? wheel_info->rolling_resistance : 50;
}

int vpart_info::wheel_area() const
{
    return has_flag( VPFLAG_WHEEL ) ? wheel_info->contact_area : 0;
}

std::vector<std::pair<std::string, int>> vpart_info::wheel_terrain_mod() const
{
    const std::vector<std::pair<std::string, int>> null_map;
    return has_flag( VPFLAG_WHEEL ) ? wheel_info->terrain_mod : null_map;
}

float vpart_info::wheel_or_rating() const
{
    return has_flag( VPFLAG_WHEEL ) ? wheel_info->or_rating : 0.0f;
}

int vpart_info::rotor_diameter() const
{
    return has_flag( VPFLAG_ROTOR ) ? rotor_info->rotor_diameter : 0;
}

float vpart_info::balloon_height() const
{
    return has_flag( VPFLAG_BALLOON ) ? balloon_info->height : 0;
}

int vpart_info::ladder_length() const
{
    return has_flag( "LADDER" ) ? ladder_info->length : 0;
}

float vpart_info::lift_coff() const
{
    return has_flag( VPFLAG_WING ) ? wing_info->lift_coff : 0;
}

int vpart_info::propeller_diameter() const
{
    return has_flag( VPFLAG_PROPELLER ) ? propeller_info->propeller_diameter : 0;
}

int vpart_info::get_max_conversions() const
{
    return has_flag( "CONVERTER" ) ? converter_info->max_steps : 0;
}
int vpart_info::get_conversion_charges() const
{
    return has_flag( "CONVERTER" ) ? converter_info->charge_cost : 0;
}
const std::pair<itype_id, int> vpart_info::get_conversion_input() const
{
    return has_flag( "CONVERTER" ) ? std::make_pair( converter_info->input,
            converter_info->input_step ) : std::make_pair( itype_id::NULL_ID(), 0 );
}
const std::pair<itype_id, int> vpart_info::get_conversion_output() const
{
    return has_flag( "CONVERTER" ) ? std::make_pair( converter_info->output,
            converter_info->output_step ) : std::make_pair( itype_id::NULL_ID(), 0 );
}

const std::vector<itype_id> vpart_info::craftertools() const
{
    return crafter_info->fake_parts;
}

const std::optional<vpslot_workbench> &vpart_info::get_workbench_info() const
{
    return workbench_info;
}

/** @relates string_id */
template<>
const vehicle_prototype &string_id<vehicle_prototype>::obj() const
{
    const auto iter = vtypes.find( *this );
    if( iter == vtypes.end() ) {
        debugmsg( "invalid vehicle prototype id %s", c_str() );
        static const vehicle_prototype dummy = {
            "",
            std::vector<vehicle_prototype::part_def>{},
            std::vector<vehicle_item_spawn>{},
            nullptr
        };
        return dummy;
    }
    return iter->second;
}

/** @relates string_id */
template<>
bool string_id<vehicle_prototype>::is_valid() const
{
    return vtypes.contains( *this );
}

vehicle_prototype::vehicle_prototype() = default;

vehicle_prototype::vehicle_prototype( const std::string &name,
                                      const std::vector<part_def> &parts,
                                      const std::vector<vehicle_item_spawn> &item_spawns,
                                      std::unique_ptr<vehicle> &&blueprint )
    : name( name ), parts( parts ), item_spawns( item_spawns ),
      blueprint( std::move( blueprint ) )
{
}

vehicle_prototype::vehicle_prototype( vehicle_prototype && )  noexcept = default;
vehicle_prototype::~vehicle_prototype() = default;

vehicle_prototype &vehicle_prototype::operator=( vehicle_prototype && )  noexcept = default;

/**
 *Caches a vehicle definition from a JsonObject to be loaded after itypes is initialized.
 */
void vehicle_prototype::load( const JsonObject &jo )
{
    vehicle_prototype &vproto = vtypes[ vproto_id( jo.get_string( "id" ) ) ];
    // If there are already parts defined, this vehicle prototype overrides an existing one.
    // If the json contains a name, it means a completely new prototype (replacing the
    // original one), therefore the old data has to be cleared.
    // If the json does not contain a name (the prototype would have no name), it means appending
    // to the existing prototype (the parts are not cleared).
    if( !vproto.parts.empty() && jo.has_string( "name" ) ) {
        vproto = vehicle_prototype();
    }
    if( vproto.parts.empty() ) {
        vproto.name = jo.get_string( "name" );
    }

    vgroups[vgroup_id( jo.get_string( "id" ) )].add_vehicle( vproto_id( jo.get_string( "id" ) ), 100 );

    const auto add_part_obj = [&]( const JsonObject & part, tripoint_mnt_veh pos ) {
        part_def pt;
        pt.pos = pos;
        pt.part = vpart_id( part.get_string( "part" ) );

        assign( part, "ammo", pt.with_ammo, true, 0, 100 );
        assign( part, "ammo_types", pt.ammo_types, true );
        assign( part, "ammo_qty", pt.ammo_qty, true, 0 );
        assign( part, "fuel", pt.fuel, true );

        vproto.parts.push_back( pt );
    };

    const auto add_part_string = [&]( const std::string & part, tripoint_mnt_veh pos ) {
        part_def pt;
        pt.pos = pos;
        pt.part = vpart_id( part );
        vproto.parts.push_back( pt );
    };

    if( jo.has_member( "flags" ) ) {
        vproto.flags = jo.get_tags<flag_id>( "flags" );
    }

    if( jo.has_member( "color_palette" ) ) {
        vproto.color_palette = vpalette_id( jo.get_string( "color_palette" ) );
    }

    if( jo.has_member( "blueprint" ) ) {
        if( jo.has_member( "palette" ) ) {
            std::map< char, JsonArray > string_palette;
            for( const JsonMember member : jo.get_object( "palette" ) ) {
                string_palette[member.name().at( 0 )] = member.get_array();
            }
            std::map< char, std::vector<part_def> > veh_palette;
            for( auto const &character : string_palette ) {
                for( JsonValue part : character.second ) {
                    part_def pt;
                    if( part.test_string() ) {
                        pt.part = vpart_id( part.get_string() );
                    } else {
                        JsonObject realpart = part.get_object();
                        pt.part = vpart_id( realpart.get_string( "part" ) );
                        assign( realpart, "ammo", pt.with_ammo, true, 0, 100 );
                        assign( realpart, "ammo_types", pt.ammo_types, true );
                        assign( realpart, "ammo_qty", pt.ammo_qty, true, 0 );
                        assign( realpart, "fuel", pt.fuel, true );
                    }
                    if( !veh_palette.contains( character.first ) ) {
                        veh_palette[character.first] = { pt };
                    } else {
                        veh_palette[character.first].push_back( pt );
                    }
                }
            }
            // TODO: Handle Z coordinates in blueprints
            auto pnt = jo.get_object( "blueprint_origin" );
            int y = -pnt.get_int( "y", 0 );
            for( std::string row : jo.get_array( "blueprint" ) ) {
                int x = -pnt.get_int( "x", 0 ) - 1;
                for( char character : row ) {
                    x += 1;
                    if( !veh_palette.contains( character ) ) { continue; }
                    for( auto part : veh_palette.at( character ) ) {
                        part_def pt = part;
                        pt.pos = tripoint_mnt_veh( x, y, 0 );
                        vproto.parts.push_back( pt );
                    }
                }
                y += 1;
            }
        } else {
            // This still has to be optional in cases where it is used only for display purposes
            jo.get_array( "blueprint" );
        }
    }

    for( JsonObject part : jo.get_array( "parts" ) ) {
        auto pos = tripoint_mnt_veh( part.get_int( "x" ), part.get_int( "y" ),
                                     part.has_int( "z" ) ? part.get_int( "z" ) : 0 );

        if( part.has_string( "part" ) ) {
            add_part_obj( part, pos );
        } else if( part.has_array( "parts" ) ) {
            for( const JsonValue entry : part.get_array( "parts" ) ) {
                if( entry.test_string() ) {
                    std::string part_name = entry.get_string();
                    add_part_string( part_name, pos );
                } else {
                    JsonObject subpart = entry.get_object();
                    add_part_obj( subpart, pos );
                }
            }
        }
    }

    for( JsonObject spawn_info : jo.get_array( "items" ) ) {
        vehicle_item_spawn next_spawn;
        const bool do_z = spawn_info.has_int( "z" );
        next_spawn.pos.x() = spawn_info.get_int( "x" );
        next_spawn.pos.y() = spawn_info.get_int( "y" );
        next_spawn.pos.z() = do_z ? spawn_info.get_int( "z" ) : 0;

        next_spawn.chance = spawn_info.get_int( "chance" );
        if( next_spawn.chance <= 0 || next_spawn.chance > 100 ) {
            if( do_z ) {
                debugmsg( "Invalid spawn chance in %s (%d, %d, %d): %d%%",
                          vproto.name, next_spawn.pos.x(), next_spawn.pos.y(), next_spawn.pos.z(), next_spawn.chance );
            } else {
                debugmsg( "Invalid spawn chance in %s (%d, %d): %d%%",
                          vproto.name, next_spawn.pos.x(), next_spawn.pos.y(), next_spawn.chance );
            }
        }

        // constrain both with_magazine and with_ammo to [0-100]
        next_spawn.with_magazine = std::max( std::min( spawn_info.get_int( "magazine",
                                             next_spawn.with_magazine ), 100 ), 0 );
        next_spawn.with_ammo = std::max( std::min( spawn_info.get_int( "ammo", next_spawn.with_ammo ),
                                         100 ), 0 );

        if( spawn_info.has_array( "items" ) ) {
            //Array of items that all spawn together (i.e. jack+tire)
            spawn_info.read( "items", next_spawn.item_ids, true );
        } else if( spawn_info.has_string( "items" ) ) {
            //Treat single item as array
            next_spawn.item_ids.emplace_back( spawn_info.get_string( "items" ) );
        }
        if( spawn_info.has_array( "item_groups" ) ) {
            //Pick from a group of items, just like map::place_items
            for( const std::string line : spawn_info.get_array( "item_groups" ) ) {
                next_spawn.item_groups.emplace_back( line );
            }
        } else if( spawn_info.has_string( "item_groups" ) ) {
            next_spawn.item_groups.emplace_back( spawn_info.get_string( "item_groups" ) );
        }
        vproto.item_spawns.push_back( std::move( next_spawn ) );
    }
}

void vehicle_prototype::reset()
{
    vtypes.clear();
}

/**
 *Works through cached vehicle definitions and creates vehicle objects from them.
 */
void vehicle_prototype::finalize()
{
    for( auto &vp : vtypes ) {
        std::unordered_set<tripoint_mnt_veh> cargo_spots;
        vehicle_prototype &proto = vp.second;
        const vproto_id &id = vp.first;

        // Calls the default constructor to create an empty vehicle. Calling the constructor with
        // the type as parameter would make it look up the type in the map and copy the
        // (non-existing) blueprint.
        proto.blueprint = std::make_unique<vehicle>();
        vehicle &blueprint = *proto.blueprint;
        blueprint.type = id;
        blueprint.name = _( proto.name );

        blueprint.suspend_refresh();
        for( auto &pt : proto.parts ) {
            if( proto.color_palette.is_valid() && !proto.color_match.contains( pt.part.str() ) ) {
                int index = proto.color_palette->fuzzy_to_index( pt.part );
                if( index != -1 ) {
                    proto.color_match[pt.part.str()] = index;
                }
            }
            const itype *base = &*pt.part->item;

            if( !pt.part.is_valid() ) {
                debugmsg( "unknown vehicle part %s in %s", pt.part.c_str(), id.c_str() );
                continue;
            }

            if( blueprint.install_part( pt.pos, pt.part, true ) < 0 ) {
                debugmsg( "init_vehicles: '%s' part '%s'(%d) can't be installed to %d,%d",
                          blueprint.name, pt.part.c_str(),
                          blueprint.part_count(), pt.pos.x(), pt.pos.y() );
            }

            if( !base->gun ) {
                if( pt.with_ammo ) {
                    debugmsg( "init_vehicles: non-turret %s with ammo in %s", pt.part.c_str(), id.c_str() );
                }
                if( !pt.ammo_types.empty() ) {
                    debugmsg( "init_vehicles: non-turret %s with ammo_types in %s", pt.part.c_str(), id.c_str() );
                }
                if( pt.ammo_qty.first > 0 || pt.ammo_qty.second > 0 ) {
                    debugmsg( "init_vehicles: non-turret %s with ammo_qty in %s", pt.part.c_str(), id.c_str() );
                }

            } else {
                for( const itype_id &e : pt.ammo_types ) {
                    if( !e->ammo && base->gun->ammo.contains( e->ammo->type ) ) {
                        debugmsg( "init_vehicles: turret %s has invalid ammo_type %s in %s",
                                  pt.part.c_str(), e.c_str(), id.c_str() );
                    }
                }
                if( pt.ammo_types.empty() ) {
                    if( !base->gun->ammo.empty() ) {
                        pt.ammo_types.insert( ammotype( *base->gun->ammo.begin() )->default_ammotype() );
                    }
                }
            }

            if( base->container || base->magazine ) {
                if( !pt.fuel.is_valid() ) {
                    debugmsg( "init_vehicles: tank %s specified invalid fuel in %s", pt.part.c_str(), id.c_str() );
                }
            } else {
                if( !pt.fuel.is_null() ) {
                    debugmsg( "init_vehicles: non-fuel store part %s with fuel in %s", pt.part.c_str(), id.c_str() );
                }
            }

            if( pt.part.obj().has_flag( "CARGO" ) ) {
                cargo_spots.insert( pt.pos );
            }
        }
        blueprint.enable_refresh();

        for( auto &i : proto.item_spawns ) {
            if( !cargo_spots.contains( i.pos ) ) {
                debugmsg( "Invalid spawn location (no CARGO vpart) in %s (%d, %d, %d): %d%%",
                          proto.name, i.pos.x(), i.pos.y(), i.pos.z(), i.chance );
            }
            for( auto &j : i.item_ids ) {
                if( !j.is_valid() ) {
                    debugmsg( "unknown item %s in spawn list of %s", j.c_str(), id.c_str() );
                }
            }
            for( auto &j : i.item_groups ) {
                if( !j.is_valid() ) {
                    debugmsg( "unknown item group %s in spawn list of %s", j.c_str(), id.c_str() );
                }
            }
        }
    }
}

std::vector<vproto_id> vehicle_prototype::get_all()
{
    std::vector<vproto_id> result;
    result.reserve( vtypes.size() );
    for( auto &vp : vtypes ) {
        result.push_back( vp.first );
    }
    return result;
}
