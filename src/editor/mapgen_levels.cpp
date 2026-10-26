#include "editor/mapgen_levels.h"

namespace editor {

auto entry_terrains(const nlohmann::ordered_json& /*value*/)
    -> std::vector<std::vector<std::string>> {
    return {};
}
auto terrain_above(const std::string& /*om_terrain*/, int /*dz*/) -> std::optional<std::string> {
    return std::nullopt;
}
auto terrain_level(const std::string& /*om_terrain*/) -> std::optional<int> { return std::nullopt; }
auto square_making(
    const std::vector<document_object>& /*entries*/, const std::string& /*om_terrain*/)
    -> std::optional<entry_square> {
    return std::nullopt;
}
auto stacked_squares(
    const std::vector<document_object>& /*entries*/, std::size_t /*entry*/, int /*dz*/)
    -> std::vector<stacked_square> {
    return {};
}

} // namespace editor
