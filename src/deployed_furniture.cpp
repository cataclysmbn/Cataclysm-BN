#include "deployed_furniture.h"

#include <utility>

#include "calendar.h"
#include "data_vars.h"
#include "item.h"
#include "map.h"
#include "type_id.h"

auto take_down_deployed_furniture( const take_down_deployed_furniture_options &opts ) -> void
{
    const auto furn_item = opts.here.furn( opts.furniture_pos ).obj().deployed_item;
    auto dropped_item = item::spawn( furn_item, calendar::turn );
    dropped_item->item_vars().merge( *opts.here.furn_vars( opts.furniture_pos ) );
    opts.here.add_item_or_charges( opts.drop_pos, std::move( dropped_item ) );
    opts.here.furn_set( opts.furniture_pos, f_null );
}
