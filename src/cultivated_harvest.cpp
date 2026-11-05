#include "cultivated_harvest.h"

#include "flag.h"
#include "iexamine.h"
#include "item.h"
#include "itype.h"

auto get_cultivated_harvest( const item &seed, const cultivated_harvest_options &options ) ->
std::vector<detached_ptr<item>>
{
    auto result = iexamine::get_harvest_items( *seed.type, options.plant_count,
                  options.seed_count, options.byproducts );
    if( !seed.type->seed ||
        seed.type->seed->required_terrain_flag != "MUSHROOM_PLANTABLE" ) {
        return result;
    }

    for( auto &harvest : result ) {
        if( harvest->typeId() != seed.typeId() &&
            harvest->typeId() != seed.type->seed->fruit_id ) {
            continue;
        }
        for( const auto &flag : { flag_HIDDEN_POISON, flag_HIDDEN_HALLU } ) {
            if( seed.has_flag( flag ) ) {
                harvest->set_flag( flag );
            }
        }
        if( seed.has_flag( flag_HIDDEN_POISON ) ) {
            harvest->poison = seed.poison;
        }
    }
    return result;
}
