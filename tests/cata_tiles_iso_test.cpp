#if defined(TILES)

#    include "cached_options.h"
#    include "cata_tiles.h"
#    include "cata_utility.h"
#    include "catch/catch.hpp"
#    include "sdl_geometry.h"
#    include "sdl_wrappers.h"
#    include "sdltiles.h"
#    include "tile_helpers.h"

#    include <memory>
#    include <vector>

// Checks that each tile context keeps the projection of its own tileset.
// Loads HitButton_iso into the map context, then ASCIITiles (ortho) into a separate
// overmap context, as OVERMAP_TILES does. Moving one tile east must still move the
// iso context's screen position right by half and up by a quarter tile width,
// and the global tile_iso must still follow the map context.
TEST_CASE("iso_tile_context_keeps_iso_after_ortho_context_loads", "[tiles][iso]") {
    tile_context_fixture fixture(/*alias_overmap_context=*/true);
    if (!fixture.valid()) {
        WARN("skipped: this environment cannot set up a headless tile context");
        return;
    }
    const auto saved_tile_iso = tile_iso;
    const on_out_of_scope restore_tile_iso([&] { tile_iso = saved_tile_iso; });
    const std::vector<mod_id> no_mods;
    auto& map_tiles = *tilecontext;
    map_tiles.load_tileset(
        "HitButton_iso", no_mods, /*precheck=*/false, /*force=*/true,
        /*pump_events=*/false);
    GeometryRenderer_Ptr geometry = std::make_unique<DefaultGeometryRenderer>();
    auto overmap_tiles = cata_tiles(get_sdl_renderer(), geometry);
    overmap_tiles.load_tileset(
        "ASCIITiles", no_mods, /*precheck=*/false, /*force=*/true,
        /*pump_events=*/false);

    CHECK(tile_iso);
    const auto origin = map_tiles.player_to_screen(point_bub_ms(0, 0));
    const auto east = map_tiles.player_to_screen(point_bub_ms(1, 0));
    CHECK(east.x - origin.x == map_tiles.get_tile_width() / 2);
    CHECK(east.y - origin.y == -map_tiles.get_tile_width() / 4);

    const auto ortho_origin = overmap_tiles.player_to_screen(point_bub_ms(0, 0));
    const auto ortho_east = overmap_tiles.player_to_screen(point_bub_ms(1, 0));
    CHECK(ortho_east.x - ortho_origin.x == overmap_tiles.get_tile_width());
    CHECK(ortho_east.y == ortho_origin.y);
}

#endif // TILES
