#include "damage.h"

#include <algorithm>
#include <cstddef>
#include <algorithm>
#include <map>
#include <numeric>
#include <utility>

#include "assign.h"
#include "cata_utility.h"
#include "debug.h"
#include "enum_conversions.h"
#include "item.h"
#include "json.h"
#include "monster.h"
#include "mtype.h"
#include "translations.h"

bool damage_unit::operator==( const damage_unit &other ) const
{
    return type == other.type &&
           amount == other.amount &&
           res_pen == other.res_pen &&
           res_mult == other.res_mult &&
           damage_multiplier == other.damage_multiplier;
}

const std::string damage_unit::get_name() const
{
    switch( type ) {
        case DT_NULL:
            return "Null";
        case DT_TRUE:
            return "True";
        case DT_BIOLOGICAL:
            return "Biological";
        case DT_BASH:
            return "Bash";
        case DT_CUT:
            return "Cut";
        case DT_ACID:
            return "Acid";
        case DT_STAB:
            return "Pierce";
        case DT_HEAT:
            return "Heat";
        case DT_COLD:
            return "Cold";
        case DT_DARK:
            return "Dark";
        case DT_LIGHT:
            return "Light";
        case DT_PSI:
            return "Psionic";
        case DT_ELECTRIC:
            return "Electric";
        case DT_BULLET:
            return "Ballistic";
        case NUM_DT:
            return std::to_string( NUM_DT );
    }
    return std::to_string( NUM_DT );
}

damage_instance::damage_instance() = default;
damage_instance damage_instance::physical( float bash, float cut, float stab, float arpen )
{
    damage_instance d;
    d.add_damage( DT_BASH, bash, arpen );
    d.add_damage( DT_CUT, cut, arpen );
    d.add_damage( DT_STAB, stab, arpen );
    return d;
}
damage_instance::damage_instance( damage_type dt, float amt, float arpen, float arpen_mult,
                                  float dmg_mult )
{
    add_damage( dt, amt, arpen, arpen_mult, dmg_mult );
}

void damage_instance::add_damage( damage_type dt, float amt, float arpen, float arpen_mult,
                                  float dmg_mult )
{
    damage_unit du( dt, amt, arpen, arpen_mult, dmg_mult );
    add( du );
}

void damage_instance::mult_damage( double multiplier, bool pre_armor )
{
    if( multiplier <= 0.0 ) {
        clear();
    }

    if( pre_armor ) {
        for( auto &elem : damage_units ) {
            elem.amount *= multiplier;
        }
    } else {
        for( auto &elem : damage_units ) {
            elem.damage_multiplier *= multiplier;
        }
    }
}
float damage_instance::type_damage( damage_type dt ) const
{
    float ret = 0;
    for( const auto &elem : damage_units ) {
        if( elem.type == dt ) {
            ret += elem.amount * elem.damage_multiplier;
        }
    }
    return ret;
}
//This returns the damage from this damage_instance. The damage done to the target will be reduced by their armor.
float damage_instance::total_damage() const
{
    float ret = 0;
    for( const auto &elem : damage_units ) {
        ret += elem.amount * elem.damage_multiplier;
    }
    return ret;
}
void damage_instance::clear()
{
    damage_units.clear();
}

bool damage_instance::empty() const
{
    return damage_units.empty();
}

void damage_instance::add( const damage_instance &added_di )
{
    for( auto &added_du : added_di.damage_units ) {
        add( added_du );
    }
}

void damage_instance::add( const damage_unit &new_du )
{
    auto iter = std::ranges::find_if( damage_units,
    [&new_du]( const damage_unit & du ) {
        return du.type == new_du.type;
    } );
    if( iter == damage_units.end() ) {
        damage_units.emplace_back( new_du );
    } else {
        damage_unit &du = *iter;
        // Actually combining two instances of damage is complex and ambiguous
        // So let's just add/multiply the values
        du.amount += new_du.amount;
        du.res_pen += new_du.res_pen;

        du.damage_multiplier *= new_du.damage_multiplier;
        du.res_mult *= new_du.res_mult;
    }
}

float damage_instance::get_armor_pen( damage_type dt ) const
{
    float ret = 0;
    for( const auto &elem : damage_units ) {
        if( elem.type == dt ) {
            ret = elem.res_pen;
        }
    }
    return ret;
}

float damage_instance::get_armor_mult( damage_type dt ) const
{
    float ret = 1;
    for( const auto &elem : damage_units ) {
        if( elem.type == dt ) {
            ret = elem.res_mult;
        }
    }
    return ret;
}

bool damage_instance::has_armor_piercing() const
{
    for( const auto &elem : damage_units ) {
        if( elem.res_pen != 0.0 || elem.res_mult != 1.0 ) {
            return true;
        }
    }
    return false;
}

std::vector<damage_unit>::iterator damage_instance::begin()
{
    return damage_units.begin();
}

std::vector<damage_unit>::const_iterator damage_instance::begin() const
{
    return damage_units.begin();
}

std::vector<damage_unit>::iterator damage_instance::end()
{
    return damage_units.end();
}

std::vector<damage_unit>::const_iterator damage_instance::end() const
{
    return damage_units.end();
}

bool damage_instance::operator==( const damage_instance &other ) const
{
    return damage_units == other.damage_units;
}

void damage_instance::deserialize( JsonIn &jsin )
{
    // TODO: Clean up
    if( jsin.test_object() ) {
        JsonObject jo = jsin.get_object();
        *this = load_damage_instance( jo );
    } else if( jsin.test_array() ) {
        *this = load_damage_instance( jsin.get_array() );
    } else {
        jsin.error( "Expected object or array for damage_instance" );
    }
}

dealt_damage_instance::dealt_damage_instance()
{
    dealt_dams.fill( 0 );
}

void dealt_damage_instance::set_damage( damage_type dt, int amount )
{
    if( dt < 0 || dt >= NUM_DT ) {
        debugmsg( "Tried to set invalid damage type %d. NUM_DT is %d", dt, NUM_DT );
        return;
    }

    dealt_dams[dt] = amount;
}
int dealt_damage_instance::type_damage( damage_type dt ) const
{
    if( static_cast<size_t>( dt ) < dealt_dams.size() ) {
        return dealt_dams[dt];
    }

    return 0;
}
int dealt_damage_instance::total_damage() const
{
    return std::accumulate( dealt_dams.begin(), dealt_dams.end(), 0 );
}

resistances::resistances() = default;

resistances::resistances( const item &armor, bool to_self )
{
    // Armors protect, but all items can resist
    if( to_self || armor.is_armor() ) {
        for( int i = 0; i < NUM_DT; i++ ) {
            damage_type dt = static_cast<damage_type>( i );
            set_resist( dt, armor.damage_resist( dt, to_self ) );
        }
    }
}

void resistances::set_resist( damage_type dt, float amount )
{
    flat[dt] = amount;
}
float resistances::type_resist( damage_type dt ) const
{
    auto iter = flat.find( dt );
    if( iter != flat.end() ) {
        return iter->second;
    }
    return 0.0f;
}
float resistances::get_effective_resist( const damage_unit &du ) const
{
    return std::max( type_resist( du.type ) - du.res_pen,
                     0.0f ) * du.res_mult;
}

resistances resistances::combined_with( const resistances &other ) const
{
    resistances ret = *this;
    for( const auto &pr : other.flat ) {
        ret.flat[ pr.first ] += pr.second;
    }

    return ret;
}

template<>
std::string io::enum_to_string<damage_type>( damage_type dt )
{
    // Using a switch instead of name_by_dt because otherwise the game freezes during launch
    switch( dt ) {
        case DT_NULL:
            return "DT_NULL";
        case DT_TRUE:
            return "DT_TRUE";
        case DT_BIOLOGICAL:
            return "DT_BIOLOGICAL";
        case DT_BASH:
            return "DT_BASH";
        case DT_CUT:
            return "DT_CUT";
        case DT_ACID:
            return "DT_ACID";
        case DT_STAB:
            return "DT_STAB";
        case DT_HEAT:
            return "DT_HEAT";
        case DT_COLD:
            return "DT_COLD";
        case DT_DARK:
            return "DT_DARK";
        case DT_LIGHT:
            return "DT_LIGHT";
        case DT_PSI:
            return "DT_PSI";
        case DT_ELECTRIC:
            return "DT_ELECTRIC";
        case DT_BULLET:
            return "DT_BULLET";
        case NUM_DT:
            break;
    }
    debugmsg( "Invalid damage_type" );
    abort();
}

static const std::map<std::string, damage_type> dt_map = {
    { translate_marker_context( "damage type", "true" ), DT_TRUE },
    { translate_marker_context( "damage type", "biological" ), DT_BIOLOGICAL },
    { translate_marker_context( "damage type", "bash" ), DT_BASH },
    { translate_marker_context( "damage type", "cut" ), DT_CUT },
    { translate_marker_context( "damage type", "acid" ), DT_ACID },
    { translate_marker_context( "damage type", "stab" ), DT_STAB },
    { translate_marker_context( "damage_type", "bullet" ), DT_BULLET },
    { translate_marker_context( "damage type", "heat" ), DT_HEAT },
    { translate_marker_context( "damage type", "cold" ), DT_COLD },
    { translate_marker_context( "damage type", "dark" ), DT_DARK },
    { translate_marker_context( "damage type", "light" ), DT_LIGHT },
    { translate_marker_context( "damage type", "psionic" ), DT_PSI },
    { translate_marker_context( "damage type", "electric" ), DT_ELECTRIC }
};

const std::map<std::string, damage_type> &get_dt_map()
{
    return dt_map;
}

damage_type dt_by_name( const std::string &name )
{
    const auto &iter = dt_map.find( name );
    if( iter == dt_map.end() ) {
        return DT_NULL;
    }

    return iter->second;
}

std::string name_by_dt( const damage_type &dt )
{
    auto iter = dt_map.cbegin();
    while( iter != dt_map.cend() ) {
        if( iter->second == dt ) {
            return pgettext( "damage type", iter->first.c_str() );
        }
        iter++;
    }
    static const std::string err_msg( "dt_not_found" );
    return err_msg;
}

const skill_id &skill_by_dt( damage_type dt )
{
    static skill_id skill_bashing( "bashing" );
    static skill_id skill_cutting( "cutting" );
    static skill_id skill_stabbing( "stabbing" );

    switch( dt ) {
        case DT_BASH:
            return skill_bashing;

        case DT_CUT:
            return skill_cutting;

        case DT_STAB:
            return skill_stabbing;

        default:
            return skill_id::NULL_ID();
    }
}

static damage_unit load_damage_unit( const JsonObject &curr )
{
    damage_type dt = dt_by_name( curr.get_string( "damage_type" ) );
    if( dt == DT_NULL ) {
        curr.throw_error( "Invalid damage type" );
    }

    float amount = curr.get_float( "amount", 0 );
    float arpen = curr.get_float( "armor_penetration", 0 );
    float armor_mul = curr.get_float( "armor_multiplier", 1.0f );
    float damage_mul = curr.get_float( "damage_multiplier", 1.0f );

    // Legacy
    float unc_armor_mul = curr.get_float( "constant_armor_multiplier", 1.0f );
    float unc_damage_mul = curr.get_float( "constant_damage_multiplier", 1.0f );

    return damage_unit( dt, amount, arpen, armor_mul * unc_armor_mul, damage_mul * unc_damage_mul );
}

static damage_unit load_damage_unit_inherit( const JsonObject &curr, const damage_instance &parent )
{
    damage_unit ret = load_damage_unit( curr );

    const std::vector<damage_unit> &parent_damage = parent.damage_units;
    auto du_iter = std::ranges::find_if( parent_damage,
    [&ret]( const damage_unit & dmg ) {
        return dmg.type == ret.type;
    } );

    if( du_iter == parent_damage.end() ) {
        return ret;
    }

    const damage_unit &parent_du = *du_iter;

    if( !curr.has_float( "amount" ) ) {
        ret.amount = parent_du.amount;
    }
    if( !curr.has_float( "armor_penetration" ) ) {
        ret.res_pen = parent_du.res_pen;
    }
    if( !curr.has_float( "armor_multiplier" ) ) {
        ret.res_mult = parent_du.res_mult;
    }
    if( !curr.has_float( "damage_multiplier" ) ) {
        ret.damage_multiplier = parent_du.damage_multiplier;
    }

    return ret;
}

static damage_instance blank_damage_instance()
{
    damage_instance ret;

    for( int i = 0; i < NUM_DT; ++i ) {
        ret.add_damage( static_cast<damage_type>( i ), 0.0f );
    }

    return ret;
}

damage_instance load_damage_instance( const JsonObject &jo )
{
    return load_damage_instance_inherit( jo, blank_damage_instance() );
}

damage_instance load_damage_instance( const JsonArray &jarr )
{
    return load_damage_instance_inherit( jarr, blank_damage_instance() );
}


damage_instance load_damage_instance_inherit( const JsonObject &jo, const damage_instance &parent )
{
    damage_instance di;
    if( jo.has_array( "values" ) ) {
        for( const JsonObject curr : jo.get_array( "values" ) ) {
            di.damage_units.push_back( load_damage_unit_inherit( curr, parent ) );
        }
    } else if( jo.has_string( "damage_type" ) ) {
        di.damage_units.push_back( load_damage_unit_inherit( jo, parent ) );
    }

    return di;
}

damage_instance load_damage_instance_inherit( const JsonArray &jarr, const damage_instance &parent )
{
    damage_instance di;
    for( const JsonObject curr : jarr ) {
        di.damage_units.push_back( load_damage_unit_inherit( curr, parent ) );
    }

    return di;
}

std::map<damage_type, float> load_damage_map( const JsonObject &jo )
{
    std::map<damage_type, float> ret;
    std::optional<float> init_val = jo.has_float( "all" ) ?
                                    jo.get_float( "all", 0.0f ) :
                                    std::optional<float>();

    auto load_if_present = [&ret, &jo]( const std::string & name, damage_type dt,
    std::optional<float> fallback ) {
        if( jo.has_float( name ) ) {
            float val = jo.get_float( name );
            ret[dt] = val;
        } else if( fallback ) {
            ret[dt] = *fallback;
        }
    };

    std::optional<float> phys = jo.has_float( "physical" ) ?
                                jo.get_float( "physical", 0.0f ) :
                                std::optional<float>();


    load_if_present( "bash", DT_BASH, phys ? *phys : init_val );
    load_if_present( "cut", DT_CUT, phys ? *phys : init_val );
    load_if_present( "stab", DT_STAB, phys ? *phys : init_val );
    load_if_present( "bullet", DT_BULLET, phys ? *phys : init_val );

    std::optional<float> non_phys = jo.has_float( "non_physical" ) ?
                                    jo.get_float( "non_physical", 0.0f ) :
                                    std::optional<float>();

    load_if_present( "biological", DT_BIOLOGICAL, non_phys ? *non_phys : init_val );
    load_if_present( "acid", DT_ACID, non_phys ? *non_phys : init_val );
    load_if_present( "heat", DT_HEAT, non_phys ? *non_phys : init_val );
    load_if_present( "cold", DT_COLD, non_phys ? *non_phys : init_val );
    load_if_present( "cold", DT_DARK, non_phys ? *non_phys : init_val );
    load_if_present( "cold", DT_LIGHT, non_phys ? *non_phys : init_val );
    load_if_present( "cold", DT_PSI, non_phys ? *non_phys : init_val );
    load_if_present( "electric", DT_ELECTRIC, non_phys ? *non_phys : init_val );

    // DT_TRUE should never be resisted
    ret[ DT_TRUE ] = 0.0f;
    return ret;
}

resistances load_resistances_instance( const JsonObject &jo )
{
    resistances ret;
    ret.flat = load_damage_map( jo );
    return ret;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             resistances &val,
             bool /*strict*/ )
{
    // Object via which to report errors which differs for proportional/relative
    // values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    if( relative.has_member( name ) ) {
        err = relative;
        JsonObject jo_relative = err.get_member( name );
        const resistances tmp = load_resistances_instance( err );
        for( size_t i = 0; i < val.flat.size(); i++ ) {
            damage_type dt = static_cast<damage_type>( i );
            auto iter = tmp.flat.find( dt );
            if( iter != tmp.flat.end() ) {
                val.flat[dt] += iter->second;
            }
        }

    } else if( proportional.has_member( name ) ) {
        err = relative;
        JsonObject jo_proportional = err.get_member( name );
        resistances tmp = load_resistances_instance( err );
        for( size_t i = 0; i < val.flat.size(); i++ ) {
            damage_type dt = static_cast<damage_type>( i );
            auto iter = tmp.flat.find( dt );
            if( iter != tmp.flat.end() ) {
                val.flat[dt] *= iter->second;
            }
        }

    } else if( jo.has_object( name ) ) {
        JsonObject jo_inner = jo.get_object( name );
        val = load_resistances_instance( jo_inner );
    }

    // TODO: Check for change - ie. `strict` check support

    return true;
}
