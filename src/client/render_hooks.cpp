#include "client_display.h"
#include "client_presentation.h"
#include "client_render_hooks.h"
#include "json.h"

#include <algorithm>
#include <vector>

namespace game_client {
namespace {
auto declared() -> std::vector<mod_tileset_declaration>& {
    static auto list = std::vector<mod_tileset_declaration>{};
    return list;
}
} // namespace
auto declared_mod_tilesets() -> const std::vector<mod_tileset_declaration>& { return declared(); }
auto load_mod_tileset(
    const JsonObject& object, const std::string& source, const std::string& base_path,
    const std::string& full_path) -> void {
    const auto index =
        1 + std::ranges::count(declared(), full_path, &mod_tileset_declaration::full_path);
    declared().push_back(
        {.base_path = base_path,
         .full_path = full_path,
         .index = static_cast<int>(index),
         .compatibility = object.get_string_array("compatibility")});
    presentation().load_mod_tileset(
        {.object = object, .source = source, .base_path = base_path, .full_path = full_path});
}
auto reset_mod_tileset() -> void {
    declared().clear();
    presentation().reset_mod_tileset();
}
auto load_tileset() -> void { presentation().load_tileset(); }
auto find_projectile_sprite(const std::string& id, const tile_category category)
    -> std::optional<std::string> {
    return presentation().projectile_sprite(id, category);
}
auto reset_minimap() -> void { presentation().reset_minimap(); }
auto on_options_changed() -> void { presentation().options_changed(); }
} // namespace game_client
