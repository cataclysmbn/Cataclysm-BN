#include "relicgen.h"

#include "assign.h"
#include "calendar.h"
#include "debug.h"
#include "detached_ptr.h"
#include "enchantments/enchantment.h"
#include "enums.h"
#include "flag.h"
#include "itype.h"
#include "generic_factory.h"
#include "json.h"
#include "monster.h"
#include "mtype.h"
#include "relic.h"
#include "type_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>
#include <vector>

namespace
{
generic_factory<relicgen> all_relicgens( "relicgens" );
}

IMPLEMENT_STRING_AND_INT_IDS( relicgen, all_relicgens );

detached_ptr<item> relicgen::generate_relic() const
{
    detached_ptr<item> result = item::spawn( *itypes.pick(), calendar::turn_zero, -1 );
    std::string name = result->type_name();
    if( !name_prefix.empty() ) {
        name = string_format( "%s %s", name_prefix.pick()->translated(), name );
        result->set_var( "name", name );
    }
    if( !name_postfix.empty() ) {
        name = string_format( "%s %s", name, name_postfix.pick()->translated() );
        result->set_var( "name", name );
    }
    result->relic_data = cata::make_value<relic>( generate_relicdata() );
    result->set_flag( flag_SECRET_ENCHANTMENTS );
    return std::move( result );
}

relic relicgen::generate_relicdata() const
{
    int power = rng( min_power, max_power );
    int used_power = 0;
    relic result;
    int max_points = rng( min_power, max_power );
    std::vector<int> remaining_iterations;
    for( const relicgen_set &set : effects ) {
        remaining_iterations.push_back( rng( set.min_count, set.max_count ) );
    }
    bool effect_added = true;
    while( effect_added ) {
        effect_added = false;
        for( int i = 0; i < effects.size(); i++ ) {
            if( remaining_iterations[i] >= 0 ) {
                relicgen_part part = *effects[i].effects.pick();
                used_power += part.cost;
                if( power < used_power ) {
                    // Give everything a chance
                    continue;
                }
                for( relic_recharge &recharge : part.recharges ) {
                    result.add_recharge_scheme( recharge );
                }
                for( fake_spell &spell : part.actives ) {
                    result.add_active_effect( spell );
                }
                for( enchantment &ench : part.enchantments ) {
                    result.add_passive_effect( ench );
                }
                effect_added = true;
                remaining_iterations[i] -= 1;
            }
        }
    }
    return result;
}

void relicgen_set::load( const JsonObject &jo )
{
    mandatory( jo, false, "min_count", min_count );
    mandatory( jo, false, "max_count", max_count );
    for( JsonObject jobj : jo.get_array( "effects" ) ) {
        int weight = jobj.get_int( "weight" );
        relicgen_part part;
        part.load( jobj );
        effects.add( part, weight );
    }
}

void relicgen_part::finalize()
{
    for( enchantment &ench : enchantments ) {
        ench.finalize();
    }
}

void relicgen_part::check() const
{
    for( const enchantment &ench : enchantments ) {
        ench.check();
    }
}



void relicgen_part::load( const JsonObject &jo )
{
    mandatory( jo, false, "cost", cost );
    if( jo.has_array( "recharge_scheme" ) ) {
        for( JsonObject jobj : jo.get_array( "recharge_scheme" ) ) {
            relic_recharge recharge;
            recharge.load( jobj );
            recharges.push_back( recharge );
        }
    }
    jo.read( "active_effects", actives );

    if( jo.has_array( "enchantments" ) ) {
        for( JsonObject jobj : jo.get_array( "enchantments" ) ) {
            enchantment ench;
            ench.load( jobj );
            if( !ench.id.is_empty() ) {
                ench = ench.id.obj();
            }
            add_enchantment( ench );
        }
    }
}

void relicgen_part::add_enchantment( enchantment &nench )
{
    for( enchantment &ench : enchantments ) {
        if( ench.add( nench ) ) {
            return;
        }
    }
    enchantments.emplace_back( enchantment( nench ) );
}

void relicgen::load( const JsonObject &jo, const std::string &src )
{
    optional( jo, was_loaded, "min_power", min_power, 1 );
    optional( jo, was_loaded, "max_power", max_power, 1 );
    for( JsonObject jobj : jo.get_array( "effects" ) ) {
        relicgen_set set;
        set.load( jobj );
        effects.push_back( set );
    }
    for( JsonObject jobj : jo.get_array( "itypes" ) ) {
        int weight = jobj.get_int( "weight" );
        itype_id itype = itype_id( jobj.get_string( "itype" ) );
        itypes.add( itype, weight );
    }

    if( jo.has_array( "name_prefix" ) ) {
        for( JsonObject jobj : jo.get_array( "name_prefix" ) ) {
            translation prefix;
            optional( jobj, false, "prefix", prefix );
            name_prefix.add( prefix, jobj.get_int( "weight" ) );
        }
    }
    if( jo.has_array( "name_postfix" ) ) {
        for( JsonObject jobj : jo.get_array( "name_postfix" ) ) {
            translation postfix;
            optional( jobj, false, "postfix", postfix );
            name_postfix.add( postfix, jobj.get_int( "weight" ) );
        }
    }
}

void relicgen::load_relicgens( const JsonObject &jo, const std::string &src )
{
    all_relicgens.load( jo, src );
}

void relicgen::finalize_all()
{
    all_relicgens.finalize();
    for( const relicgen &bd : all_relicgens.get_all() ) {
        const_cast<relicgen &>( bd ).finalize();
    }
}

void relicgen_set::finalize()
{
    for( const auto &[val, weight] : effects ) {
        val.check();
    }
}

void relicgen::finalize()
{
    for( const auto &val : effects ) {
        val.check();
    }
}

void relicgen_set::check() const
{
    for( const auto &[val, weight] : effects ) {
        val.check();
    }
}
void relicgen::check() const
{
    for( const auto &val : effects ) {
        val.check();
    }
    for( const auto &[type, weight] : itypes ) {
        if( !type.is_valid() ) {
            debugmsg( "Invalid itype %s in relicgen %s", type, id );
        }
    }
}

void relicgen::check_consistency()
{
    all_relicgens.check();
}

void relicgen::reset()
{
    all_relicgens.reset();
}

std::vector<relicgen> relicgen::get_all()
{
    return all_relicgens.get_all();
}

const relicgen_id relicgen::relicgen_from_natural( artifact_natural_property prop )
{
    switch( prop ) {
        case ARTPROP_WRIGGLING:
            return relicgen_id( "wriggling" );
        case ARTPROP_GLOWING:
            return relicgen_id( "glowing" );
        case ARTPROP_HUMMING:
            return relicgen_id( "humming" );
        case ARTPROP_MOVING:
            return relicgen_id( "moving" );
        case ARTPROP_WHISPERING:
            return relicgen_id( "whispering" );
        case ARTPROP_BREATHING:
            return relicgen_id( "breathing" );
        case ARTPROP_DEAD:
            return relicgen_id( "dead" );
        case ARTPROP_ITCHY:
            return relicgen_id( "itchy" );
        case ARTPROP_GLITTERING:
            return relicgen_id( "glittering" );
        case ARTPROP_ELECTRIC:
            return relicgen_id( "electric" );
        case ARTPROP_SLIMY:
            return relicgen_id( "slimy" );
        case ARTPROP_ENGRAVED:
            return relicgen_id( "engraved" );
        case ARTPROP_CRACKLING:
            return relicgen_id( "crackling" );
        case ARTPROP_WARM:
            return relicgen_id( "warm" );
        case ARTPROP_RATTLING:
            return relicgen_id( "rattling" );
        case ARTPROP_SCALED:
            return relicgen_id( "scaled" );
        case ARTPROP_FRACTAL:
            return relicgen_id( "fractal" );
        case ARTPROP_NULL:
        case ARTPROP_MAX:
            return relicgen_id( "default" );
    }
}
