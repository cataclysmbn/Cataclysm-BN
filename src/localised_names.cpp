#include "localised_names.h"

#include "monster.h"
#include "mtype.h"

namespace localised
{

auto name_of( const monster &critter, const unsigned int quantity ) -> name
{
    return { .shown = critter.name( quantity ), .text = std::nullopt };
}

} // namespace localised
