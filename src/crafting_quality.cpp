#include "crafting_quality.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "character.h"
#include "filter_utils.h"
#include "item.h"
#include "itype.h"
#include "recipe.h"
#include "requirements.h"

namespace
{
auto best_quality_multiplier_for_req( const inventory &crafting_inv,
                                      const quality_requirement &quality_req ) -> float
{
    auto best_multiplier = 1.0f;
    const auto &quality = quality_req.type.obj();
    const auto per_level_multiplier = quality.crafting_speed_bonus_per_level;

    crafting_inv.visit_items( [&]( const item * itm ) {
        const auto item_quality = itm->get_quality( quality_req.type );
        if( item_quality < quality_req.level ) {
            return VisitResponse::NEXT;
        }

        const auto item_multiplier = itm->type->crafting_speed_modifier;
        auto effective_multiplier = item_multiplier;
        if( item_multiplier == 1.0f && per_level_multiplier > 0.0f ) {
            const auto effective_level = std::max( quality_req.level,
                                                   quality.crafting_speed_level_offset );
            const auto extra_levels = item_quality - effective_level;
            if( extra_levels <= 0 ) {
                return VisitResponse::NEXT;
            }
            const auto quality_multiplier = std::pow( per_level_multiplier, extra_levels );
            effective_multiplier = item_multiplier * quality_multiplier;
        }
        best_multiplier = std::max( best_multiplier, effective_multiplier );
        return VisitResponse::NEXT;
    } );

    return best_multiplier;
}

auto best_tool_multiplier_for_group( const inventory &crafting_inv,
                                     const std::vector<tool_comp> &tool_group ) -> float
{
    auto best_multiplier = 1.0f;
    std::ranges::for_each( tool_group, [&]( const tool_comp & tool ) {
        if( !tool.has( crafting_inv, return_true<item> ) ) {
            return;
        }
        const auto &tool_type = tool.type.obj();
        best_multiplier = std::max( best_multiplier, tool_type.crafting_speed_modifier );
    } );

    return best_multiplier;
}

auto crafting_tools_speed_multiplier( const inventory &crafting_inv,
                                      const requirement_data &requirements ) -> float
{
    const auto &quality_reqs = requirements.get_qualities();
    const auto &tool_reqs = requirements.get_tools();
    auto total_multiplier = 1.0f;

    std::ranges::for_each( quality_reqs, [&]( const auto & quality_group ) {
        auto best_multiplier = 1.0f;
        std::ranges::for_each( quality_group, [&]( const auto & quality_req ) {
            best_multiplier = std::max( best_multiplier,
                                        best_quality_multiplier_for_req( crafting_inv, quality_req ) );
        } );
        total_multiplier *= best_multiplier;
    } );

    std::ranges::for_each( tool_reqs, [&]( const auto & tool_group ) {
        total_multiplier *= best_tool_multiplier_for_group( crafting_inv, tool_group );
    } );

    return total_multiplier;
}
} // namespace

auto crafting_tools_speed_multiplier( const Character &who, const recipe &rec ) -> float
{
    const auto &crafting_inv = const_cast<Character &>( who ).crafting_inventory();
    return crafting_tools_speed_multiplier( crafting_inv, rec.simple_requirements() );
}

auto crafting_tools_speed_multiplier( const Character &who,
                                      const requirement_data &requirements ) -> float
{
    const auto &crafting_inv = const_cast<Character &>( who ).crafting_inventory();
    return crafting_tools_speed_multiplier( crafting_inv, requirements );
}
