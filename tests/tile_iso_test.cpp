#include "catch/catch.hpp"

#if defined(TILES)

#    include "cached_options.h"
#    include "cata_tiles.h"
#    include "cata_utility.h"
#    include "sdltiles.h"
#    include "tile_helpers.h"
#    include "type_id.h"

#    include <vector>

TEST_CASE("an_overmap_tileset_leaves_the_map_tilesets_iso_mode", "[tiles][iso]") {
    tile_context_fixture fixture(/*alias_overmap_context=*/false);
    if (!fixture.valid()) {
        WARN("skipped: this environment cannot set up a headless tile context");
        return;
    }
    const auto saved_iso = tile_iso;
    const auto restore = on_out_of_scope([&] { tile_iso = saved_iso; });
    const auto no_mods = std::vector<mod_id>{};
    tilecontext->load_tileset(
        "HitButton_iso", no_mods, /*precheck=*/false, /*force=*/true, /*pump_events=*/false);
    REQUIRE(tile_iso);

    overmap_tilecontext->load_tileset(
        "ASCIITiles", no_mods, /*precheck=*/false, /*force=*/true, /*pump_events=*/false);
    CHECK(tile_iso);
}

#endif // TILES
