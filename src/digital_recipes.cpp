#include "digital_recipes.h"

#include "debug.h"
#include "generic_factory.h"
#include "item.h"
#include "json.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rng.h"
#include "string_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>

namespace
{
struct digital_recipe_group_data;
using digital_group_id = string_id<digital_recipe_group_data>;

struct fixed_recipe_reference {
    recipe_id recipe;
    std::optional<int> skill_level;
};

struct random_recipe_options {
    int count = 1;
    std::string category;
    int min_difficulty = 0;
    int max_difficulty = INT_MAX;
    bool include_unlearnable = false;
    std::optional<int> difficulty_chance_offset;
    std::vector<recipe_id> candidates;
};

struct digital_recipe_group_data {
    digital_group_id id;
    bool was_loaded = false;
    std::vector<fixed_recipe_reference> recipes;
    std::optional<random_recipe_options> random_recipes;

    auto load( const JsonObject &jo, const std::string &src ) -> void;
    auto check() const -> void;
};

auto recipe_groups = generic_factory<digital_recipe_group_data>( "digital recipe group" );
} // namespace

IMPLEMENT_STRING_AND_INT_IDS( digital_recipe_group_data, recipe_groups );

auto digital_recipe_group_data::load( const JsonObject &jo, const std::string &/*src*/ ) -> void
{
    if( jo.has_array( "recipes" ) ) {
        recipes.clear();
        for( const auto value : jo.get_array( "recipes" ) ) {
            const auto entry = value.get_object();
            const auto level = entry.has_int( "skill_level" ) ?
                               std::optional<int>( entry.get_int( "skill_level" ) ) : std::nullopt;
            if( level && *level < 0 ) {
                entry.throw_error( "skill_level must not be negative", "skill_level" );
            }
            recipes.push_back( { .recipe = recipe_id( entry.get_string( "id" ) ), .skill_level = level } );
        }
    }
    if( jo.has_object( "random_recipes" ) ) {
        const auto random = jo.get_object( "random_recipes" );
        auto options = random_recipe_options{
            .count = random.get_int( "count", 1 ),
            .category = random.get_string( "category", "" ),
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
        random_recipes = std::move( options );
    }
    if( recipes.empty() && !random_recipes ) {
        jo.throw_error( "digital recipe group needs recipes or random_recipes" );
    }
}

auto digital_recipe_group_data::check() const -> void
{
    for( const auto &entry : recipes ) {
        if( !entry.recipe.is_valid() ) {
            debugmsg( "Digital recipe group %s references invalid recipe %s", id.str(), entry.recipe.str() );
        }
    }
    if( random_recipes ) {
        for( const auto &candidate : random_recipes->candidates ) {
            if( !candidate.is_valid() ) {
                debugmsg( "Digital recipe group %s references invalid candidate %s", id.str(),
                          candidate.str() );
            }
        }
    }
}

namespace
{
auto generate_recipes( const digital_recipe_group_data &group ) ->
std::vector<digital_recipe_reference>
{
    namespace ranges = std::ranges;
    using namespace std::views;
    auto result = std::vector<digital_recipe_reference> {};
    for( const auto &entry : group.recipes ) {
        if( entry.recipe.is_valid() ) {
            result.push_back( { .recipe = entry.recipe,
                                .skill_level = entry.skill_level.value_or( entry.recipe->difficulty ) } );
        }
    }
    if( !group.random_recipes ) {
        return result;
    }
    const auto &options = *group.random_recipes;
    auto candidates = recipe_dict | values
    | filter( [&options]( const auto & candidate ) {
        return ( options.include_unlearnable || !candidate.never_learn ) &&
               ( options.category.empty() || candidate.category == options.category ) &&
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

auto load_digital_recipe_group( const JsonObject &jo, const std::string &src ) -> void
{
    recipe_groups.load( jo, src );
}

auto reset_digital_recipe_groups() -> void { recipe_groups.reset(); }
auto check_digital_recipe_groups() -> void { recipe_groups.check(); }
auto digital_recipe_group_exists( const std::string &id ) -> bool
{
    return recipe_groups.is_valid( digital_group_id( id ) );
}

auto initialize_digital_recipe_file( item &file ) -> void
{
    const auto group = file.get_property_string( "digital_recipe_group" );
    if( group.empty() || file.has_var( "DIGITAL_RECIPES" ) ) {
        return;
    }
    if( !digital_recipe_group_exists( group ) ) {
        debugmsg( "Invalid digital recipe group %s on %s", group, file.typeId().str() );
        return;
    }
    auto buffer = std::ostringstream{};
    auto json = JsonOut( buffer );
    json.start_array();
    for( const auto &entry : generate_recipes( recipe_groups.obj( digital_group_id( group ) ) ) ) {
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
