#include "client_display.h"
#include "client_presentation.h"
#include "client_render_hooks.h"

namespace game_client {
auto load_mod_tileset(
    const JsonObject& object, const std::string& source, const std::string& base_path,
    const std::string& full_path) -> void {
    presentation().load_mod_tileset(
        {.object = object, .source = source, .base_path = base_path, .full_path = full_path});
}
auto reset_mod_tileset() -> void { presentation().reset_mod_tileset(); }
auto load_tileset() -> void { presentation().load_tileset(); }
auto find_projectile_sprite(const std::string& id, const tile_category category)
    -> std::optional<std::string> {
    return presentation().projectile_sprite(id, category);
}
auto reset_minimap() -> void { presentation().reset_minimap(); }
auto on_options_changed() -> void { presentation().options_changed(); }
} // namespace game_client
