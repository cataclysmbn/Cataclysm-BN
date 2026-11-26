#include "tile_helpers.h"

#if defined(TILES)

#    include "cached_options.h"
#    include "cata_tiles.h"
#    include "game.h"
#    include "sdl_geometry.h"
#    include "sdl_wrappers.h"
#    include "sdltiles.h"
#    include "type_id.h"

#    include <SDL3/SDL.h>
#    include <exception>
#    include <utility>
#    include <vector>

// defined in sdltiles.cpp with no header declaration (cata_tiles.cpp redeclares them
// the same way); set_draw_scale divides by them and only init_interface() sets them
extern int fontwidth;
extern int fontheight;

struct tile_context_fixture::impl {
    SDL_Window_Ptr window;
    GeometryRenderer_Ptr geometry;
    std::shared_ptr<cata_tiles> tiles;
    std::shared_ptr<cata_tiles> overmap_tiles;
    std::shared_ptr<cata_tiles> saved_tilecontext;
    std::shared_ptr<cata_tiles> saved_overmap_tilecontext;
    float saved_zoom = DEFAULT_TILESET_ZOOM;
    int saved_fontwidth = 0;
    int saved_fontheight = 0;
    bool saved_use_tiles = false;
    bool saved_use_tiles_overmap = false;
    bool installed_renderer = false;
    bool globals_installed = false;
};

tile_context_fixture::tile_context_fixture(const bool alias_overmap_context)
    : pimpl_(std::make_unique<impl>()) {
    pimpl_->saved_fontwidth = std::exchange(fontwidth, 8);
    pimpl_->saved_fontheight = std::exchange(fontheight, 16);
    if (!SDL_WasInit(SDL_INIT_VIDEO)) { return; }
    // dynamic_atlas creates textures through the get_sdl_renderer() global, not the
    // renderer handed to cata_tiles, so a headless one must be installed there
    if (!get_sdl_renderer()) {
        pimpl_->window.reset(SDL_CreateWindow("cata_test_tiles", 640, 480, SDL_WINDOW_HIDDEN));
        if (!pimpl_->window) { return; }
        auto renderer = SDL_Renderer_Ptr(SDL_CreateRenderer(pimpl_->window.get(), "software"));
        if (!renderer) { return; }
        set_sdl_renderer(std::move(renderer));
        pimpl_->installed_renderer = true;
    }
    // smallest shipped tileset; the tests only assert relative scale changes
    const std::vector<mod_id> no_mods;
    try {
        pimpl_->geometry = std::make_unique<DefaultGeometryRenderer>();
        pimpl_->tiles = std::make_shared<cata_tiles>(get_sdl_renderer(), pimpl_->geometry);
        pimpl_->tiles->load_tileset(
            "ASCIITiles", no_mods, /*precheck=*/false, /*force=*/true,
            /*pump_events=*/false);
        if (alias_overmap_context) {
            pimpl_->overmap_tiles = pimpl_->tiles;
        } else {
            pimpl_->overmap_tiles =
                std::make_shared<cata_tiles>(get_sdl_renderer(), pimpl_->geometry);
            pimpl_->overmap_tiles->load_tileset(
                "ASCIITiles", no_mods, /*precheck=*/false,
                /*force=*/true, /*pump_events=*/false);
        }
    } catch (const std::exception&) {
        // leave the fixture invalid; the destructor uninstalls the renderer in safe order
        return;
    }

    if (g) { pimpl_->saved_zoom = g->get_zoom(); }
    pimpl_->saved_tilecontext = std::exchange(tilecontext, pimpl_->tiles);
    pimpl_->saved_overmap_tilecontext = std::exchange(overmap_tilecontext, pimpl_->overmap_tiles);
    pimpl_->saved_use_tiles = std::exchange(use_tiles, true);
    pimpl_->saved_use_tiles_overmap = std::exchange(use_tiles_overmap, true);
    pimpl_->globals_installed = true;
    valid_ = true;
}

tile_context_fixture::~tile_context_fixture() {
    if (pimpl_->globals_installed) {
        // restore the main zoom and reset the overmap zoom (it has no getter to save)
        // while the fixture contexts are still installed so the rescale is safe
        if (g) {
            g->set_zoom(pimpl_->saved_zoom);
            g->reset_overmap_zoom();
        }
        tilecontext = std::move(pimpl_->saved_tilecontext);
        overmap_tilecontext = std::move(pimpl_->saved_overmap_tilecontext);
        use_tiles = pimpl_->saved_use_tiles;
        use_tiles_overmap = pimpl_->saved_use_tiles_overmap;
    }
    // destroy tile contexts before the renderer, and the renderer before the window
    pimpl_->overmap_tiles.reset();
    pimpl_->tiles.reset();
    if (pimpl_->installed_renderer) { set_sdl_renderer(SDL_Renderer_Ptr()); }
    fontwidth = pimpl_->saved_fontwidth;
    fontheight = pimpl_->saved_fontheight;
}

#else // TILES

// The fixture is only usable in the tiles test binary.
struct tile_context_fixture::impl {};
tile_context_fixture::tile_context_fixture(const bool /*alias_overmap_context*/) {}
tile_context_fixture::~tile_context_fixture() = default;

#endif // TILES
