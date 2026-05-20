#include "damage.h"

#include <algorithm>
#include <cstddef>
#include <algorithm>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
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

namespace
{

struct damage_type_definition {
    damage_type type = DT_NULL;
    std::string id;
    std::string name;
    skill_id skill = skill_id::NULL_ID();
    std::optional<std::pair<damage_type, float>> derived_from;
    std::set<std::string> immune_flags;
    std::set<std::string> mon_immune_flags;
    bool physical = false;
    bool no_resist = false;
};

struct builtin_damage_type_definition {
    damage_type type;
    std::string id;
    std::string name;
    bool physical;
};

const auto builtin_damage_type_definitions = std::array{
    builtin_damage_type_definition{ .type = DT_TRUE, .id = "true", .name = "true", .physical = false },
    builtin_damage_type_definition{ .type = DT_BIOLOGICAL, .id = "biological", .name = "biological", .physical = false },
    builtin_damage_type_definition{ .type = DT_BASH, .id = "bash", .name = "bash", .physical = true },
    builtin_damage_type_definition{ .type = DT_CUT, .id = "cut", .name = "cut", .physical = true },
    builtin_damage_type_definition{ .type = DT_ACID, .id = "acid", .name = "acid", .physical = false },
    builtin_damage_type_definition{ .type = DT_STAB, .id = "stab", .name = "stab", .physical = true },
    builtin_damage_type_definition{ .type = DT_BULLET, .id = "bullet", .name = "bullet", .physical = true },
    builtin_damage_type_definition{ .type = DT_HEAT, .id = "heat", .name = "heat", .physical = false },
    builtin_damage_type_definition{ .type = DT_COLD, .id = "cold", .name = "cold", .physical = false },
    builtin_damage_type_definition{ .type = DT_DARK, .id = "dark", .name = "dark", .physical = false },
    builtin_damage_type_definition{ .type = DT_LIGHT, .id = "light", .name = "light", .physical = false },
    builtin_damage_type_definition{ .type = DT_PSI, .id = "psi", .name = "psionic", .physical = false },
    builtin_damage_type_definition{ .type = DT_ELECTRIC, .id = "electric", .name = "electric", .physical = false },
};

auto damage_type_definitions() -> std::vector<damage_type_definition> & // *NOPAD*
{
    static auto defs = std::vector<damage_type_definition> {};
    return defs;
}

auto damage_type_ids() -> std::vector<damage_type> & // *NOPAD*
{
    static auto ids = std::vector<damage_type> {};
    return ids;
}

auto damage_type_map() -> std::map<std::string, damage_type> & // *NOPAD*
{
    static auto map = std::map<std::string, damage_type> {};
    return map;
}

auto empty_damage_type_flags() -> const std::set<std::string> & // *NOPAD*
{
    static const auto flags = std::set<std::string> {};
    return flags;
}

auto find_damage_type_definition( damage_type dt ) -> const damage_type_definition * // *NOPAD*
{
    auto &defs = damage_type_definitions();
    const auto iter = std::ranges::find_if( defs, [dt]( const auto & def ) { return def.type == dt; } );
    return iter == defs.end() ? nullptr : &*iter;
}

auto find_mutable_damage_type_definition( damage_type dt ) -> damage_type_definition * // *NOPAD*
{
    auto &defs = damage_type_definitions();
    const auto iter = std::ranges::find_if( defs, [dt]( const auto & def ) { return def.type == dt; } );
    return iter == defs.end() ? nullptr : &*iter;
}

auto ensure_builtin_damage_types() -> void
{
    if( !damage_type_definitions().empty() ) {
        return;
    }
    reset_damage_types();
}

auto read_damage_type_name( const JsonObject &jo, const std::string &fallback ) -> std::string
{
    if( jo.has_string( "name" ) ) {
        return jo.get_string( "name" );
    }
    if( jo.has_object( "name" ) ) {
        return jo.get_object( "name" ).get_string( "str", fallback );
    }
    return fallback;
}

auto get_float_optional( const JsonObject &jo, const std::string &name,
                                const std::optional<float> fallback = std::nullopt ) -> std::optional<float>
{
    return jo.has_member( name ) ? std::make_optional<float>( jo.get_float( name, 0.0f ) ) : fallback;
}

} // namespace

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
    return name_by_dt( type );
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

dealt_damage_instance::dealt_damage_instance() = default;

void dealt_damage_instance::set_damage( damage_type dt, int amount )
{
    if( !damage_type_is_valid( dt ) ) {
        debugmsg( "Tried to set invalid damage type %d.", dt );
        return;
    }

    dealt_dams[dt] = amount;
}
int dealt_damage_instance::type_damage( damage_type dt ) const
{
    const auto iter = dealt_dams.find( dt );
    return iter == dealt_dams.end() ? 0 : iter->second;
}
int dealt_damage_instance::total_damage() const
{
    return std::accumulate( dealt_dams.begin(), dealt_dams.end(), 0,
    []( const int sum, const auto & entry ) { return sum + entry.second; } );
}

resistances::resistances() = default;

resistances::resistances( const item &armor, bool to_self )
{
    // Armors protect, but all items can resist
    if( to_self || armor.is_armor() ) {
        for( int i = 0; i < NUM_DT; i++ ) {
            const auto dt = static_cast<damage_type>( i );
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
    if( iter != flat.end() && !damage_type_is_no_resist( dt ) ) {
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
    ensure_builtin_damage_types();
    if( const auto *def = find_damage_type_definition( dt ); def != nullptr ) {
        return def->id;
    }
    return "DT_NULL";
}

const std::map<std::string, damage_type> &get_dt_map()
{
    ensure_builtin_damage_types();
    return damage_type_map();
}

const std::vector<damage_type> &get_all_damage_types()
{
    ensure_builtin_damage_types();
    return damage_type_ids();
}

damage_type dt_by_name( const std::string &name )
{
    ensure_builtin_damage_types();
    const auto &map = damage_type_map();
    const auto iter = map.find( name );
    return iter == map.end() ? DT_NULL : iter->second;
}

std::string name_by_dt( const damage_type &dt )
{
    ensure_builtin_damage_types();
    const auto *def = find_damage_type_definition( dt );
    if( def == nullptr ) {
        return "dt_not_found";
    }
    return pgettext( "damage type", def->name.c_str() );
}

auto damage_type_is_valid( damage_type dt ) -> bool
{
    ensure_builtin_damage_types();
    return find_damage_type_definition( dt ) != nullptr;
}

auto damage_type_is_no_resist( damage_type dt ) -> bool
{
    ensure_builtin_damage_types();
    const auto *def = find_damage_type_definition( dt );
    return def != nullptr && def->no_resist;
}

auto damage_type_immune_flags( damage_type dt ) -> const std::set<std::string> & // *NOPAD*
{
    ensure_builtin_damage_types();
    const auto *def = find_damage_type_definition( dt );
    return def == nullptr ? empty_damage_type_flags() : def->immune_flags;
}

auto damage_type_mon_immune_flags( damage_type dt ) -> const std::set<std::string> & // *NOPAD*
{
    ensure_builtin_damage_types();
    const auto *def = find_damage_type_definition( dt );
    return def == nullptr ? empty_damage_type_flags() : def->mon_immune_flags;
}

auto reset_damage_types() -> void
{
    auto &defs = damage_type_definitions();
    auto &ids = damage_type_ids();
    auto &map = damage_type_map();
    defs.clear();
    ids.clear();
    map.clear();
    for( const auto &builtin : builtin_damage_type_definitions ) {
        defs.push_back( {
            .type = builtin.type,
            .id = builtin.id,
            .name = builtin.name,
            .skill = skill_id::NULL_ID(),
            .derived_from = std::nullopt,
            .immune_flags = {},
            .mon_immune_flags = {},
            .physical = builtin.physical,
            .no_resist = false,
        } );
        ids.push_back( builtin.type );
        map[builtin.id] = builtin.type;
    }
    map["pure"] = DT_TRUE;
    map["psionic"] = DT_PSI;
}

auto load_damage_type( const JsonObject &jo, const std::string & ) -> void
{
    ensure_builtin_damage_types();
    const auto id = jo.get_string( "id" );
    auto dt = dt_by_name( id );
    if( dt == DT_NULL ) {
        dt = static_cast<damage_type>( static_cast<int>( get_all_damage_types().back() ) + 1 );
        damage_type_ids().push_back( dt );
        damage_type_map()[id] = dt;
        damage_type_definitions().push_back( {
            .type = dt,
            .id = id,
            .name = id,
            .skill = skill_id::NULL_ID(),
            .derived_from = std::nullopt,
            .immune_flags = {},
            .mon_immune_flags = {},
            .physical = false,
            .no_resist = false,
        } );
    }

    auto *def = find_mutable_damage_type_definition( dt );
    if( def == nullptr ) {
        jo.throw_error( "failed to register damage type" );
    }

    def->name = read_damage_type_name( jo, id );
    def->skill = jo.has_string( "skill" ) ? skill_id( jo.get_string( "skill" ) ) : def->skill;
    def->physical = jo.has_bool( "physical" ) ? jo.get_bool( "physical" ) : def->physical;
    def->no_resist = jo.has_bool( "no_resist" ) ? jo.get_bool( "no_resist" ) : def->no_resist;
    if( jo.has_array( "derived_from" ) ) {
        const auto ja = jo.get_array( "derived_from" );
        const auto base = dt_by_name( ja.get_string( 0 ) );
        if( base != DT_NULL ) {
            def->derived_from = std::make_pair( base, static_cast<float>( ja.get_float( 1 ) ) );
        }
    }
    if( jo.has_object( "immune_flags" ) ) {
        const auto flags = jo.get_object( "immune_flags" );
        if( flags.has_array( "character" ) ) {
            def->immune_flags.clear();
            for( const auto flag : flags.get_array( "character" ) ) {
                def->immune_flags.insert( flag );
            }
        }
        if( flags.has_array( "monster" ) ) {
            def->mon_immune_flags.clear();
            for( const auto flag : flags.get_array( "monster" ) ) {
                def->mon_immune_flags.insert( flag );
            }
        }
    }
    jo.allow_omitted_members();
}

const skill_id &skill_by_dt( damage_type dt )
{
    static const auto skill_bashing = skill_id( "bashing" );
    static const auto skill_cutting = skill_id( "cutting" );
    static const auto skill_stabbing = skill_id( "stabbing" );

    switch( dt ) {
        case DT_BASH:
            return skill_bashing;

        case DT_CUT:
            return skill_cutting;

        case DT_STAB:
            return skill_stabbing;

        default:
            ensure_builtin_damage_types();
            if( const auto *def = find_damage_type_definition( dt ); def != nullptr ) {
                return def->skill;
            }
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

    for( const auto dt : get_all_damage_types() ) {
        ret.add_damage( dt, 0.0f );
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

auto load_damage_map( const JsonObject &jo ) -> std::map<damage_type, float>
{
    const auto all_fallback = get_float_optional( jo, "all" );
    const auto physical_fallback = get_float_optional( jo, "physical", all_fallback );
    const auto non_phys_fallback = get_float_optional( jo, "non_physical", all_fallback );

    auto ret = std::map<damage_type, float> {};
    for( const JsonMember member : jo ) {
        const auto &name = member.name();
        if( name == "all" || name == "physical" || name == "non_physical" ) {
            continue;
        }
        const auto dt = dt_by_name( name );
        if( dt != DT_NULL ) {
            ret[dt] = member.get_float();
        }
    }

    for( const auto dt : get_all_damage_types() ) {
        if( ret.contains( dt ) ) {
            continue;
        }
        const auto *def = find_damage_type_definition( dt );
        const auto fallback = def != nullptr && def->physical ? physical_fallback : non_phys_fallback;
        if( fallback ) {
            ret[dt] = *fallback;
        } else if( def != nullptr && def->derived_from ) {
            const auto derived = *def->derived_from;
            if( ret.contains( derived.first ) ) {
                ret[dt] = ret[derived.first] * derived.second;
            }
        }
    }
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
        const resistances tmp = load_resistances_instance( jo_relative );
        for( const auto &[dt, amount] : tmp.flat ) {
            val.flat[dt] += amount;
        }

    } else if( proportional.has_member( name ) ) {
        err = proportional;
        JsonObject jo_proportional = err.get_member( name );
        const resistances tmp = load_resistances_instance( jo_proportional );
        for( const auto &[dt, amount] : tmp.flat ) {
            val.flat[dt] *= amount;
        }

    } else if( jo.has_object( name ) ) {
        JsonObject jo_inner = jo.get_object( name );
        val = load_resistances_instance( jo_inner );
    }

    // TODO: Check for change - ie. `strict` check support

    return true;
}
