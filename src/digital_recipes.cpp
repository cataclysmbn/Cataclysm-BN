#include "digital_recipes.h"

#include "data_vars.h"
#include "debug.h"
#include "item.h"
#include "json.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rng.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>

namespace
{
struct fixed_recipe_reference {
    recipe_id recipe;
    std::optional<int> skill_level;
};

struct random_recipe_options {
    int count = 1;
    std::string category;
    std::string subcategory;
    int min_difficulty = 0;
    int max_difficulty = INT_MAX;
    bool include_unlearnable = false;
    std::optional<int> difficulty_chance_offset;
    std::vector<recipe_id> candidates;
};

struct digital_recipe_data {
    std::vector<fixed_recipe_reference> recipes;
    std::optional<random_recipe_options> random_recipes;
};

auto read_recipe_data( const data_vars::data_set &vars ) ->
std::expected<digital_recipe_data, std::string>
{
    auto result = digital_recipe_data{};
    try {
        const auto fixed = vars.get( "digital_recipes" );
        if( !fixed.empty() ) {
            auto input = std::istringstream( fixed );
            auto json = JsonIn( input );
            for( const auto value : json.get_array() ) {
                const auto entry = value.get_object();
                const auto level = entry.has_int( "skill_level" ) ?
                                   std::optional<int>( entry.get_int( "skill_level" ) ) : std::nullopt;
                if( level && *level < 0 ) {
                    entry.throw_error( "skill_level must not be negative", "skill_level" );
                }
                const auto id = recipe_id( entry.get_string( "id" ) );
                if( !id.is_valid() ) {
                    return std::unexpected( "invalid digital recipe " + id.str() );
                }
                result.recipes.push_back( { .recipe = id, .skill_level = level } );
            }
        }
        const auto random_data = vars.get( "digital_random_recipes" );
        if( !random_data.empty() ) {
            auto input = std::istringstream( random_data );
            auto json = JsonIn( input );
            const auto random = json.get_object();
            auto options = random_recipe_options{
                .count = random.get_int( "count", 1 ),
                .category = random.get_string( "category", "" ),
                .subcategory = random.get_string( "subcategory", "" ),
                .min_difficulty = random.get_int( "min_difficulty", 0 ),
                .max_difficulty = random.get_int( "max_difficulty", INT_MAX ),
                .include_unlearnable = random.get_bool( "include_unlearnable", false ),
                .difficulty_chance_offset = std::nullopt,
                .candidates = {}
            };
            if( random.has_int( "difficulty_chance_offset" ) ) {
                options.difficulty_chance_offset = random.get_int( "difficulty_chance_offset" );
            }
            random.read( "candidates", options.candidates );
            if( options.count < 0 || options.min_difficulty < 0 ||
                options.max_difficulty < options.min_difficulty ||
                ( options.difficulty_chance_offset && *options.difficulty_chance_offset < 0 ) ) {
                random.throw_error( "invalid random recipe count, difficulty range, or chance offset" );
            }
            for( const auto &candidate : options.candidates ) {
                if( !candidate.is_valid() ) {
                    return std::unexpected( "invalid digital recipe candidate " + candidate.str() );
                }
            }
            result.random_recipes = std::move( options );
        }
    } catch( const JsonError &error ) {
        return std::unexpected( std::string( error.what() ) );
    }
    return result;
}

auto generate_recipes( const digital_recipe_data &data ) ->
std::vector<digital_recipe_reference>
{
    namespace ranges = std::ranges;
    using namespace std::views;
    auto result = std::vector<digital_recipe_reference> {};
    for( const auto &entry : data.recipes ) {
        result.push_back( { .recipe = entry.recipe,
                            .skill_level = entry.skill_level.value_or( entry.recipe->difficulty ) } );
    }
    if( !data.random_recipes ) {
        return result;
    }
    const auto &options = *data.random_recipes;
    auto candidates = recipe_dict | values
    | filter( [&options]( const auto & candidate ) {
        return ( options.include_unlearnable || !candidate.never_learn ) &&
               ( options.category.empty() || candidate.category == options.category ) &&
               ( options.subcategory.empty() || candidate.subcategory == options.subcategory ) &&
               candidate.difficulty >= options.min_difficulty &&
               candidate.difficulty <= options.max_difficulty &&
               ( options.candidates.empty() || ranges::contains( options.candidates, candidate.ident() ) );
    } ) | transform( []( const auto & candidate ) { return &candidate; } )
    | ranges::to<std::vector>();
    std::erase_if( candidates, [&result, &options]( const auto * candidate ) {
        const auto already_included = ranges::any_of( result, [candidate]( const auto & entry ) {
            return entry.recipe == candidate->ident();
        } );
        if( already_included ) {
            return true;
        }
        if( !options.difficulty_chance_offset ) {
            return false;
        }
        const auto denominator = static_cast<int>( std::clamp<int64_t>(
                                     static_cast<int64_t>( candidate->difficulty ) + *options.difficulty_chance_offset,
                                     1, INT_MAX ) );
        return !one_in( denominator );
    } );
    for( auto count = 0; count < options.count && !candidates.empty(); ++count ) {
        const auto *selected = random_entry_removed( candidates );
        result.push_back( { .recipe = selected->ident(), .skill_level = selected->difficulty } );
    }
    return result;
}
} // namespace

auto validate_digital_recipe_data( const data_vars::data_set &vars ) ->
std::expected<void, std::string>
{
    const auto data = read_recipe_data( vars );
    if( !data ) {
        return std::unexpected( data.error() );
    }
    return {};
}

auto initialize_digital_recipe_file( item &file ) -> void
{
    if( file.has_var( "DIGITAL_RECIPES" ) ||
        ( file.get_var( "digital_recipes" ).empty() &&
          file.get_var( "digital_random_recipes" ).empty() ) ) {
        return;
    }
    const auto data = read_recipe_data( file.item_vars() );
    if( !data ) {
        debugmsg( "Invalid digital recipe data on %s: %s", file.typeId().str(), data.error() );
        return;
    }
    auto buffer = std::ostringstream{};
    auto json = JsonOut( buffer );
    json.start_array();
    for( const auto &entry : generate_recipes( *data ) ) {
        json.start_object();
        json.member( "id", entry.recipe.str() );
        json.member( "skill_level", entry.skill_level );
        json.end_object();
    }
    json.end_array();
    file.set_var( "DIGITAL_RECIPES", buffer.str() );
}

auto digital_file_recipes( const item &file ) -> std::vector<digital_recipe_reference>
{
    auto result = std::vector<digital_recipe_reference> {};
    if( !file.has_var( "DIGITAL_RECIPES" ) ) {
        return result;
    }
    auto buffer = std::istringstream( file.get_var( "DIGITAL_RECIPES" ) );
    auto json = JsonIn( buffer );
    for( const auto value : json.get_array() ) {
        const auto entry = value.get_object();
        const auto id = recipe_id( entry.get_string( "id" ) );
        if( id.is_valid() ) {
            result.push_back( { .recipe = id, .skill_level = entry.get_int( "skill_level" ) } );
        }
    }
    return result;
}
