#include "procgen/proc_recipe.h"

#include <algorithm>
#include <array>
#include <ranges>

#include "itype.h"
#include "recipe.h"

namespace
{

static const flag_id flag_RAW( "RAW" );
static const material_id material_bone( "bone" );
static const material_id material_copper( "copper" );
static const material_id material_iron( "iron" );
static const material_id material_stone( "stone" );
static const material_id material_steel( "steel" );
static const material_id material_wood( "wood" );

auto has_food_role( const proc::part_fact &fact, const std::string &role ) -> bool
{
    return std::ranges::find( fact.tag, role ) != fact.tag.end();
}

auto has_material( const proc::part_fact &fact, const material_id &material ) -> bool
{
    return std::ranges::find( fact.mat, material ) != fact.mat.end();
}

auto type_has_flag( const proc::part_fact &fact, const flag_id &flag ) -> bool
{
    return !fact.id.is_null() && fact.id->has_flag( flag );
}

auto stew_requires_cutting( const std::vector<proc::part_fact> &facts ) -> bool
{
    return std::ranges::find_if( facts, []( const proc::part_fact & fact ) {
        return ( has_food_role( fact, "veg" ) || has_food_role( fact, "meat" ) ) &&
               type_has_flag( fact, flag_RAW );
    } ) != facts.end();
}

auto stew_tool_requirements( const std::vector<proc::part_fact> &facts ) -> requirement_data
{
    if( !stew_requires_cutting( facts ) ) {
        return {};
    }

    return requirement_data( {}, {
        { quality_requirement( quality_id( "CUT" ), 1, 1 ) }
    }, {} );
}

auto melee_weapon_requires_cutting( const std::vector<proc::part_fact> &facts ) -> bool
{
    return std::ranges::any_of( facts, []( const proc::part_fact & fact ) {
        return has_material( fact, material_wood ) || has_material( fact, material_bone );
    } );
}

auto melee_weapon_requires_hammering( const std::vector<proc::part_fact> &facts ) -> bool
{
    return std::ranges::any_of( facts, []( const proc::part_fact & fact ) {
        return has_material( fact, material_steel ) || has_material( fact, material_iron ) ||
               has_material( fact, material_copper ) || has_material( fact, material_stone );
    } );
}

auto melee_weapon_tool_requirements( const std::vector<proc::part_fact> &facts ) -> requirement_data
{
    auto qualities = std::vector<std::vector<quality_requirement>> {};
    if( melee_weapon_requires_cutting( facts ) ) {
        qualities.push_back( { quality_requirement( quality_id( "CUT" ), 1, 1 ) } );
    }
    if( melee_weapon_requires_hammering( facts ) ) {
        qualities.push_back( { quality_requirement( quality_id( "HAMMER" ), 1, 1 ) } );
    }
    return requirement_data( {}, qualities, {} );
}

auto is_melee_weapon_proc( const recipe &rec ) -> bool
{
    static const auto melee_weapon_proc_ids = std::array {
        proc::schema_id( "sword" ),
        proc::schema_id( "axe" ),
        proc::schema_id( "spear" ),
        proc::schema_id( "knife" ),
    };
    return std::ranges::find( melee_weapon_proc_ids, rec.proc_id() ) != melee_weapon_proc_ids.end();
}

} // namespace

auto proc::recipe_requirements( const recipe &rec,
                                const std::vector<part_fact> &facts ) -> requirement_data
{
    auto reqs = rec.simple_requirements();
    if( !rec.is_proc() ) {
        return reqs;
    }

    if( rec.proc_id() == proc::schema_id( "stew" ) ) {
        reqs = reqs + stew_tool_requirements( facts );
    } else if( is_melee_weapon_proc( rec ) ) {
        reqs = reqs + melee_weapon_tool_requirements( facts );
    }

    return reqs;
}

auto proc::recipe_preview_description( const recipe &rec ) -> std::string
{
    if( !rec.builder_desc().translated().empty() ) {
        return rec.builder_desc().translated();
    }
    if( rec.proc_id() == proc::schema_id( "sandwich" ) ) {
        return _( "Choose breads and fillings to assemble a procedural sandwich." );
    }
    if( rec.proc_id() == proc::schema_id( "stew" ) ) {
        return _( "Choose a base and ingredients to simmer a procedural stew." );
    }
    if( rec.proc_id() == proc::schema_id( "trail_mix" ) ) {
        return _( "Choose nuts, dried fruit, and optional sweets for a procedural trail mix." );
    }
    if( rec.proc_id() == proc::schema_id( "sword" ) ) {
        return _( "Choose weapon parts and materials to assemble a procedural melee weapon." );
    }
    return _( "Choose parts to assemble this procedural item." );
}
