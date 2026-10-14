#include "tile_parts.h"

#include <nlohmann/json.hpp>

namespace engine_client
{

auto to_json( const tile_part &part ) -> nlohmann::ordered_json
{
    auto result = nlohmann::ordered_json{ { "kind", part.kind }, { "text", part.text } };
    if( !part.detail.empty() ) { result["detail"] = part.detail; }
    return result;
}

} // namespace engine_client
