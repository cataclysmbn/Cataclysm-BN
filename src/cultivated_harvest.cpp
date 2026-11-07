#include "cultivated_harvest.h"

#include "iexamine.h"
#include "item.h"
#include "itype.h"

auto get_cultivated_harvest( const item &seed, const cultivated_harvest_options &options ) ->
std::vector<detached_ptr<item>>
{
    auto result = iexamine::get_harvest_items( *seed.type, options.plant_count,
                  options.seed_count, options.byproducts );
    if( !seed.type->seed ) {
        return result;
    }

    for( auto &harvest : result ) {
        if( harvest->typeId() != seed.typeId() &&
            harvest->typeId() != seed.type->seed->fruit_id ) {
            continue;
        }
        for( const auto &flag : seed.type->seed->harvest_inherit_flags ) {
            if( seed.has_flag( flag ) ) {
                harvest->set_flag( flag );
            }
        }
        if( seed.type->seed->harvest_inherit_poison ) {
            harvest->poison = seed.poison;
        }
    }
    return result;
}
