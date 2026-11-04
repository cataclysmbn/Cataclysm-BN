#include "localised_names.h"

#include "monster.h"
#include "mtype.h"

namespace localised
{

auto name_of( const monster &critter, const unsigned int quantity ) -> name
{
    const auto kind = name_of( critter.type->name_translation(), static_cast<int>( quantity ) );
    if( critter.unique_name.empty() ) { return kind; }
    // The kind and the monster's own name, joined as `monster::name` joins them.
    const auto joined = kind.text ? std::optional{ localised_text{ .key = "", .args = { nested( *kind.text ), std::string{ ": " }, critter.unique_name } } } :
                        std::nullopt;
    return { .shown = critter.name( quantity ), .text = joined };
}

} // namespace localised
