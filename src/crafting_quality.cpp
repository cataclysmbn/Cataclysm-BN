#include "crafting_quality.h"

#include <algorithm>

#include "character.h"
#include "recipe.h"
#include "requirements.h"

auto crafting_quality_speed_multiplier( const Character &who, const recipe &rec ) -> float
{
    const auto &crafting_inv = const_cast<Character &>( who ).crafting_inventory();
    const auto &quality_reqs = rec.simple_requirements().get_qualities();
    auto total_multiplier = 1.0f;

    std::ranges::for_each( quality_reqs, [&]( const auto &quality_group ) {
        auto best_multiplier = 1.0f;
        std::ranges::for_each( quality_group, [&]( const auto &quality_req ) {
            const auto &quality = quality_req.type.obj();
            const auto bonus_per_level = quality.crafting_speed_bonus_per_level;
            if( bonus_per_level <= 0.0f ) {
                return;
            }

            const auto available_level = crafting_inv.max_quality( quality_req.type );
            if( available_level <= quality_req.level ) {
                return;
            }

            const auto extra_levels = available_level - quality_req.level;
            const auto candidate_multiplier = 1.0f + bonus_per_level * extra_levels;
            best_multiplier = std::max( best_multiplier, candidate_multiplier );
        } );

        total_multiplier *= best_multiplier;
    } );

    return total_multiplier;
}
