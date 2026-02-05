#include "overmap_label.h"

#include <map>

namespace
{
auto map_labels = std::map<oter_type_str_id, std::string>{};
} // namespace

auto overmap_labels::set_label( const oter_type_str_id &id,
                                const std::optional<std::string> &label ) -> void
{
    if( label.has_value() ) {
        map_labels[id] = *label;
        return;
    }

    map_labels.erase( id );
}

auto overmap_labels::get_label( const oter_type_str_id &id ) -> std::optional<std::string>
{
    auto iter = map_labels.find( id );
    if( iter == map_labels.end() ) {
        return std::nullopt;
    }

    return iter->second;
}

auto overmap_labels::reset() -> void
{
    map_labels.clear();
}
