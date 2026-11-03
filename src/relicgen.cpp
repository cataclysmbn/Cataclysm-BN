#include "relicgen.h"

#include "assign.h"
#include "debug.h"
#include "detached_ptr.h"
#include "enchantments/enchantment.h"
#include "enchantments/enchantment_condition.h"
#include "enchantments/enchantment_flag.h"
#include "enchantments/enchantment_value.h"
#include "enchantments/enchantment_vision.h"
#include "enums.h"
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
    detached_ptr<item> result = item::spawn( *itypes.pick() );
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
    return std::move( result );
}

relic relicgen::generate_relicdata() const
{
    int power = rng( min_power, max_power );
    int used_power = 0;
    relic result;
    while( true ) {
        relicgen_part part = *effects.pick();
        used_power += part.cost;
        if( power < used_power ) {
            break;
        }
        for( relic_recharge &recharge : part.recharges ) {
            result.add_recharge_scheme( recharge );
        }
        for( fake_spell &spell : part.actives ) {
            result.add_active_effect( spell );
        }
        result.add_passive_effect( enchantment( part ) );
    }
    return result;
}

void relicgen_part::finalize()
{
    // Once again this is entirely copied from enchantments
    const char *ench_desc = "A relicgen part";
    std::vector<std::string> problems;
    // These checks are done here because mutation cannot occur in the last stage
    auto val_add_copy = values_add;
    for( const auto& [ench_val_id, val] : val_add_copy ) {
        if( ench_val_id->id != ench_val_id ) {
            problems.push_back( string_format(
                                    "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                                    "%s",
                                    ench_val_id.str(), ench_val_id->id.str() ) );
            values_add.erase( ench_val_id );
            if( values_add.contains( ench_val_id->id ) ) {
                values_add[ench_val_id->id] += val;
            } else {
                values_add[ench_val_id->id] = val;
            }
        }
    }
    auto val_max_copy = values_max;
    for( const auto& [ench_val_id, val] : val_max_copy ) {
        if( ench_val_id->id != ench_val_id ) {
            problems.push_back( string_format(
                                    "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                                    "%s",
                                    ench_val_id.str(), ench_val_id->id.str() ) );
            values_max.erase( ench_val_id );
            if( values_max.contains( ench_val_id->id ) ) {
                values_max[ench_val_id->id] += val;
            } else {
                values_max[ench_val_id->id] = val;
            }
        }
    }
    auto val_mult_copy = values_multiply;
    for( const auto& [ench_val_id, val] : val_mult_copy ) {
        if( ench_val_id->id != ench_val_id ) {
            problems.push_back( string_format(
                                    "\nenchantment value %s is using legacy enchantment name automatically migrated to "
                                    "%s",
                                    ench_val_id.str(), ench_val_id->id.str() ) );
            values_multiply.erase( ench_val_id );
            if( values_multiply.contains( ench_val_id->id ) ) {
                values_multiply[ench_val_id->id] += val;
            } else {
                values_multiply[ench_val_id->id] = val;
            }
        }
    }
    auto flags_copy = flags;
    for( const auto& [ench_flag_id, _] : flags ) {
        auto parents = ench_flag_id->get_parents();
        for( const auto parent : parents ) { flags[parent] = 1; }
    }

    if( !problems.empty() ) {
        debugmsg( "%s has: %s", ench_desc,
                  enumerate_as_string( problems, enumeration_conjunction::none ) );
    }
}

void relicgen_part::check() const
{
    // TODO: Where was it declared? CONTEXT!
    const char *ench_desc = "An relicgen part";
    std::vector<std::string> problems;
    std::set<enchantment_condition_type> cond_types;
    for( const auto &ench_cond_id : conditions ) {
        if( !ench_cond_id.is_valid() ) {
            problems.push_back(
                string_format( "\nenchantment condition %s is invalid", ench_cond_id.str() ) );
        } else {
            cond_types.insert( ench_cond_id->cond_type );
        }
    }
    for( const auto& [ench_flag, _] : flags ) {
        if( !ench_flag.is_valid() ) {
            problems.push_back( string_format( "\nenchantment flag %s is invalid", ench_flag.str() ) );
        }
    }
    auto val_add_copy = values_add;
    for( const auto& [ench_val_id, val] : val_add_copy ) {
        if( !ench_val_id.is_valid() ) {
            problems.push_back(
                string_format( "\nenchantment value %s is invalid", ench_val_id.str() ) );
        } else {
            if( !ench_val_id->can_add ) {
                problems.push_back(
                    string_format( "\nenchantment value %s cannot be added to", ench_val_id.str() ) );
            }
            for( const auto &cond_type : ench_val_id->unsupported_conditions ) {
                if( cond_types.contains( cond_type ) ) {
                    problems.push_back( string_format(
                                            "\nenchantment value %s does not support condition type %s",
                                            ench_val_id.str(), io::enum_to_string( cond_type ) )

                                      );
                }
            }
        }
    }
    auto val_max_copy = values_max;
    for( const auto& [ench_val_id, val] : val_max_copy ) {
        if( !ench_val_id.is_valid() ) {
            problems.push_back(
                string_format( "\nenchantment value %s is invalid", ench_val_id.str() ) );
        } else {
            if( !ench_val_id->can_max ) {
                problems.push_back(
                    string_format( "\nenchantment value %s cannot use max", ench_val_id.str() ) );
            }
            for( const auto &cond_type : ench_val_id->unsupported_conditions ) {
                if( cond_types.contains( cond_type ) ) {
                    problems.push_back( string_format(
                                            "\nenchantment value %s does not support condition type %s",
                                            ench_val_id.str(), io::enum_to_string( cond_type ) )

                                      );
                }
            }
        }
    }
    auto val_mult_copy = values_multiply;
    for( const auto& [ench_val_id, val] : val_mult_copy ) {
        if( !ench_val_id.is_valid() ) {
            problems.push_back(
                string_format( "\nenchantment value %s is invalid", ench_val_id.str() ) );
        } else {
            if( !ench_val_id->can_mult ) {
                problems.push_back(
                    string_format( "\nenchantment value %s cannot be added to", ench_val_id.str() ) );
            }

            for( const auto &cond_type : ench_val_id->unsupported_conditions ) {
                if( cond_types.contains( cond_type ) ) {
                    problems.push_back( string_format(
                                            "\nenchantment value %s does not support condition type %s",
                                            ench_val_id.str(), io::enum_to_string( cond_type ) )

                                      );
                }
            }
        }
    }
    if( !problems.empty() ) {
        debugmsg( "%s has: %s", ench_desc,
                  enumerate_as_string( problems, enumeration_conjunction::none ) );
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

    jo.read( "active_effect", actives );
    // NOTE: This section is practically copied from enchantment::load
    jo.read( "hit_you_effect", hit_you_effect );
    jo.read( "hit_me_effect", hit_me_effect );

    if( jo.has_object( "intermittent_activation" ) ) {
        JsonObject jobj = jo.get_object( "intermittent_activation" );
        for( const JsonObject effect_obj : jobj.get_array( "effects" ) ) {
            time_duration freq = read_from_json_string <
                                 time_duration > ( *effect_obj.get_raw( "frequency" ), time_duration::units );
            if( effect_obj.has_array( "spell_effects" ) ) {
                for( const JsonObject fake_spell_obj : effect_obj.get_array( "spell_effects" ) ) {
                    fake_spell fake;
                    fake.load( fake_spell_obj );
                    intermittent_activation[freq].push_back( fake );
                }
            } else if( effect_obj.has_object( "spell_effects" ) ) {
                fake_spell fake;
                JsonObject fake_spell_obj = effect_obj.get_object( "spell_effects" );
                fake.load( fake_spell_obj );
                intermittent_activation[freq].push_back( fake );
            }
        }
    }

    optional( jo, false, "conditions", conditions );
    if( jo.has_string( "has" ) ) { conditions.insert( enchantment_condition_id( jo.get_string( "has" ) ) ); }
    if( jo.has_string( "condition" ) ) {
        conditions.insert( enchantment_condition_id( jo.get_string( "condition" ) ) );
    }

    for( JsonObject jsobj : jo.get_array( "ench_effects" ) ) {
        ench_effects.emplace( efftype_id( jsobj.get_string( "effect" ) ), jsobj.get_int( "intensity" ) );
    }

    optional( jo, false, "fake_items", fake_items, auto_flags_reader<itype_id> {} );
    optional( jo, false, "immune_effects", immune_effects, auto_flags_reader<efftype_id> {} );
    optional( jo, false, "immune_fields", immune_fields, auto_flags_reader<field_type_id> {} );
    optional( jo, false, "special_vision", special_visions,
              auto_flags_reader<enchantment_vision_id> {} );

    if( jo.has_array( "values" ) ) {
        for( const JsonObject value_obj : jo.get_array( "values" ) ) {
            enchantment_value_id value = enchantment_value_id( value_obj.get_string( "value" ) );
            const int add = value_obj.get_int( "add", 0 );
            const double mult = value_obj.get_float( "multiply", 0.0 );
            const double max = value_obj.get_int( "max", 0 );
            if( add != 0 ) { values_add.emplace( value, add ); }
            if( mult != 0.0 ) {
                // Limit precision to minimize inconsistencies between platforms / compilers
                const double mul = static_cast<int>( std::round( mult * 100'000 ) ) / 100'000.0;
                values_multiply.emplace( value, mul );
            }
            if( max != 0 ) { values_max.emplace( value, max ); }
        }
    }
    if( jo.has_array( "flags" ) ) {
        for( const auto flag : jo.get_string_array( "flags" ) ) {
            flags[enchantment_flag_id( flag )] = 1;
        }
    }
}

void relicgen::load( const JsonObject &jo, const std::string &src )
{
    optional( jo, was_loaded, "min_power", min_power );
    optional( jo, was_loaded, "max_power", max_power );
    for( JsonObject jobj : jo.get_array( "effects" ) ) {
        int weight = jobj.get_int( "weight" );
        relicgen_part part;
        part.load( jobj );
        effects.add( part, weight );
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

void relicgen::finalize()
{
    for( const auto &[val, weight] : effects ) {
        val.check();
    }
}

void relicgen::check() const
{
    for( const auto &[val, weight] : effects ) {
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

