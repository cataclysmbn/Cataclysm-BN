#include "tile_parts.h"

#include "monster.h"
#include "npc.h"
#include "string_utils.h"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace engine_client
{

auto to_part( const fact &value ) -> tile_part
{
    const auto text = trim_whitespaces( value.text );
    return {
        .kind = value.kind,
        .text = value.label + ( value.color && !text.empty() ? colorize( text, *value.color ) : text ),
        .detail = value.detail,
        .id = value.id,
        .values = value.values,
    };
}

auto to_parts( const std::vector<fact> &facts ) -> std::vector<tile_part>
{
    auto parts = std::vector<tile_part> {};
    std::ranges::transform( facts, std::back_inserter( parts ), []( const auto & value ) { return to_part( value ); } );
    return parts;
}

auto creature_facts( const Creature &who ) -> std::vector<fact>
{
    if( const auto *const person = dynamic_cast<const npc *>( &who ) ) { return person->info_facts(); }
    if( const auto *const beast = dynamic_cast<const monster *>( &who ) ) { return beast->info_facts(); }
    if( const auto *const person = dynamic_cast<const Character *>( &who ) ) { return person->info_facts(); }
    return {};
}

auto to_json( const tile_part &part ) -> nlohmann::ordered_json
{
    auto result = nlohmann::ordered_json{ { "kind", part.kind }, { "text", part.text } };
    if( !part.detail.empty() ) { result["detail"] = part.detail; }
    if( !part.id.empty() ) { result["id"] = part.id; }
    if( !part.values.empty() ) {
        auto &values = result["values"] = nlohmann::ordered_json::object();
        for( const auto &[key, value] : part.values ) { values[key] = value; }
    }
    return result;
}

} // namespace engine_client
