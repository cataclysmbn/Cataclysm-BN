#include "mutation.h" // IWYU pragma: associated

#include "json.h"

struct mutation_type {
    std::string id;
    bool mandatory_one = false;
    bool swap_on_conflict = false;
    int random_chance = 0;
};

std::map<std::string, mutation_type> mutation_types;

void load_mutation_type( const JsonObject &jsobj )
{
    mutation_type new_type;
    new_type.id = jsobj.get_string( "id" );
    new_type.mandatory_one = jsobj.get_bool( "mandatory_one", false );
    new_type.swap_on_conflict = jsobj.get_bool( "swap_on_conflict", false );
    new_type.random_chance = jsobj.get_int( "random_chance", 0 );

    mutation_types[new_type.id] = new_type;
}

void reset_mutation_types()
{
    mutation_types.clear();
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
