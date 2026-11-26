#pragma once

#include "coordinates.h"
#include "sdl_wrappers.h"

class pixel_minimap_projector {
public:
    pixel_minimap_projector() = default;
    virtual ~pixel_minimap_projector() = default;

    virtual auto get_tile_size() const -> point = 0;
    virtual auto get_tiles_size(point tiles_count) const -> point = 0;
    virtual auto get_tile_pos(point p, point tiles_count) const -> point = 0;

    virtual auto get_chunk_rect(point p, point tiles_count) const -> SDL_Rect = 0;
};

class pixel_minimap_ortho_projector: public pixel_minimap_projector {
public:
    pixel_minimap_ortho_projector(
        point total_tiles_count, const SDL_Rect& max_screen_rect, bool square_pixels);

    auto get_tile_size() const -> point override;
    auto get_tiles_size(point tiles_count) const -> point override;
    auto get_tile_pos(point p, point tiles_count) const -> point override;

    auto get_chunk_rect(point p, point tiles_count) const -> SDL_Rect override;

private:
    point tile_size;
};

class pixel_minimap_iso_projector: public pixel_minimap_projector {
public:
    pixel_minimap_iso_projector(
        point total_tiles_count, const SDL_Rect& max_screen_rect, bool square_pixels);

    auto get_tile_size() const -> point override;
    auto get_tiles_size(point tiles_count) const -> point override;
    auto get_tile_pos(point p, point tiles_count) const -> point override;

    auto get_chunk_rect(point p, point tiles_count) const -> SDL_Rect override;

private:
    point total_tiles_count;
    point tile_size;
};
