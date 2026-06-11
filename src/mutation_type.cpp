#include "mutation.h" // IWYU pragma: associated

#include <map>
#include <ranges>
#include <string>
#include <vector>

#include "debug.h"
#include "json.h"
#include "translations.h"

namespace
{

namespace views = std::views;
using std::ranges::to;

struct mutation_type {
    std::string id;
    bool mandatory_one = false;
    bool swap_on_conflict = false;
    int random_chance = 0;
    trait_id default_trait = trait_id::NULL_ID();
};

auto mutation_types = std::map<std::string, mutation_type> {};

} // namespace

void load_mutation_type( const JsonObject &jsobj )
{
    auto new_type = mutation_type{};
    new_type.id = jsobj.get_string( "id" );
    new_type.mandatory_one = jsobj.get_bool( "mandatory_one", false );
    new_type.swap_on_conflict = jsobj.get_bool( "swap_on_conflict", false );
    new_type.random_chance = jsobj.get_int( "random_chance", 0 );
    if( jsobj.has_string( "default_trait" ) ) {
        new_type.default_trait = trait_id( jsobj.get_string( "default_trait" ) );
    }

    mutation_types[new_type.id] = new_type;
}

void reset_mutation_types()
{
    mutation_types.clear();
}

auto mutation_type_check_consistency() -> void
{
    auto types_with_defaults = mutation_types | views::values
    | views::filter( []( const auto & type ) { return type.default_trait != trait_id::NULL_ID(); } );
    for( const auto &type : types_with_defaults ) {
        if( !type.default_trait.is_valid() ) {
            debugmsg( "mutation type %s uses undefined default trait %s", type.id, type.default_trait.c_str() );
        } else if( !type.default_trait->types.contains( type.id ) ) {
            debugmsg( "mutation type %s uses default trait %s that does not belong to that type", type.id,
                      type.default_trait.c_str() );
        }
    }
}

bool mutation_type_exists( const std::string &id )
{
    return mutation_types.contains( id );
}

bool mutation_type_is_mandatory( const std::string &id )
{
    auto it = mutation_types.find( id );
    return it != mutation_types.end() && it->second.mandatory_one;
}

bool mutation_type_swaps_on_conflict( const std::string &id )
{
    auto it = mutation_types.find( id );
    return it != mutation_types.end() && ( it->second.swap_on_conflict || it->second.mandatory_one );
}

int mutation_type_random_chance( const std::string &id )
{
    auto it = mutation_types.find( id );
    return it != mutation_types.end() ? it->second.random_chance : 0;
}

auto mutation_type_display_name( const std::string &id ) -> std::string
{
    if( id == "skin_tone" ) {
        return _( "skin color" );
    } else if( id == "eye_color" ) {
        return _( "eye color" );
    } else if( id == "hair_style" ) {
        return _( "hair style" );
    } else if( id == "hair_color" ) {
        return _( "hair color" );
    }
    return id;
}

auto get_default_mutations_for_types() -> std::vector<mutation_type_default>
{
    auto defaults = mutation_types | views::values
    | views::filter( []( const auto & type ) { return type.default_trait != trait_id::NULL_ID(); } )
    | views::transform( []( const auto & type ) {
        return mutation_type_default{ .type_id = type.id, .trait = type.default_trait };
    } );
    return defaults | to<std::vector>();
}

std::vector<std::string> get_all_mutation_type_ids()
{
    std::vector<std::string> ret;
    ret.reserve( mutation_types.size() );
    std::ranges::transform( mutation_types, std::back_inserter( ret ),
    []( const auto & kv ) { return kv.first; } );
    return ret;
}

std::vector<trait_id> get_mutations_in_type( const std::string &id )
{
    std::vector<trait_id> ret;
    for( const mutation_branch &it : mutation_branch::get_all() ) {
        if( it.types.contains( id ) ) {
            ret.push_back( it.id );
        }
    }
    return ret;
}

std::vector<trait_id> get_mutations_in_types( const std::set<std::string> &ids )
{
    std::vector<trait_id> ret;
    for( const std::string &it : ids ) {
        std::vector<trait_id> this_id = get_mutations_in_type( it );
        ret.insert( ret.end(), this_id.begin(), this_id.end() );
    }
    return ret;
}
