#include "overlay_ordering.h"

#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "json.h"

std::map<std::string, int> base_mutation_overlay_ordering;
std::map<std::string, int> tileset_mutation_overlay_ordering;

namespace
{
constexpr auto effect_overlay_default_order = 0;
constexpr auto mutation_overlay_default_order = 9999;
constexpr auto worn_overlay_default_order = 10000;
constexpr auto wielded_overlay_default_order = 11000;

auto lookup_overlay_order( const std::map<std::string, int> &orderarray,
                           const std::string &overlay_id_string ) -> std::optional<int>
{
    const auto lookup_exact = [&]( const std::string & candidate ) -> std::optional<int> {
        if( const auto it = orderarray.find( candidate ); it != orderarray.end() )
        {
            return it->second;
        }
        return std::nullopt;
    };

    if( const auto exact_match = lookup_exact( overlay_id_string ) ) {
        return exact_match;
    }

    constexpr auto mutation_prefix = std::string_view( "mutation_" );
    if( overlay_id_string.starts_with( mutation_prefix ) ) {
        const auto legacy_id = overlay_id_string.substr( mutation_prefix.size() );
        if( const auto legacy_match = lookup_exact( legacy_id ) ) {
            return legacy_match;
        }
        constexpr auto active_prefix = std::string_view( "active_" );
        if( legacy_id.starts_with( active_prefix ) ) {
            return lookup_exact( legacy_id.substr( active_prefix.size() ) );
        }
    }

    constexpr auto active_prefix = std::string_view( "active_" );
    if( overlay_id_string.starts_with( active_prefix ) ) {
        return lookup_exact( overlay_id_string.substr( active_prefix.size() ) );
    }

    return std::nullopt;
}

auto get_default_overlay_order( const std::string &overlay_id_string ) -> int
{
    if( overlay_id_string.starts_with( "effect_" ) ) {
        return effect_overlay_default_order;
    }
    if( overlay_id_string.starts_with( "worn_" ) ) {
        return worn_overlay_default_order;
    }
    if( overlay_id_string.starts_with( "wielded_" ) ) {
        return wielded_overlay_default_order;
    }

    return mutation_overlay_default_order;
}
} // namespace

auto load_overlay_ordering( const JsonObject &jsobj ) -> void
{
    load_overlay_ordering_into_array( jsobj, base_mutation_overlay_ordering );
}

auto load_overlay_ordering_into_array( const JsonObject &jsobj,
                                       std::map<std::string, int> &orderarray ) -> void
{
    for( JsonObject ordering : jsobj.get_array( "overlay_ordering" ) ) {
        const auto order = ordering.get_int( "order" );
        for( auto &id : ordering.get_tags( "id" ) ) {
            orderarray[id] = order;
        }
    }
}

auto get_overlay_order( const std::string &overlay_id_string ) -> int
{
    auto value = get_default_overlay_order( overlay_id_string );
    if( const auto base_order = lookup_overlay_order( base_mutation_overlay_ordering,
                                overlay_id_string ) ) {
        value = *base_order;
    }
    if( const auto tileset_order = lookup_overlay_order( tileset_mutation_overlay_ordering,
                                   overlay_id_string ) ) {
        value = *tileset_order;
    }
    return value;
}

auto reset_overlay_ordering() -> void
{
    // tileset specific overlays are cleared on new tileset load
    base_mutation_overlay_ordering.clear();
}
