#include "proc_fact.h"

#include <algorithm>

#include "item.h"
#include "itype.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

auto normalized_hp( const item &it ) -> float
{
    const auto max_damage = it.max_damage();
    if( max_damage <= 0 ) {
        return 1.0f;
    }

    const auto cur_damage = std::clamp( it.damage(), 0, max_damage );
    return static_cast<float>( max_damage - cur_damage ) / static_cast<float>( max_damage );
}

} // namespace

auto proc::normalize_part_fact( const item &it, const part_ix ix ) -> part_fact
{
    return part_fact{
        .ix = ix,
        .id = it.typeId(),
        .mat = it.made_of(),
        .mass_g = static_cast<int>( units::to_gram( it.weight() ) ),
        .volume_ml = units::to_milliliter( it.volume() ),
        .kcal = it.is_comestible() ? it.get_comestible()->default_nutrition.kcal : 0,
        .hp = normalized_hp( it ),
        .chg = it.count_by_charges() ? it.charges : 0,
    };
}
