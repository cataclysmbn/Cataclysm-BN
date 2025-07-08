#pragma once

#include "point.h"
#include "sdl_wrappers.h"

class pixel_minimap_projector
{
    public:
        pixel_minimap_projector() = default;
        virtual ~pixel_minimap_projector() = default;

        virtual point get_tile_size() const = 0;
        virtual point get_tiles_size( point tiles_count ) const = 0;
        virtual point get_tile_pos( point p, point tiles_count ) const = 0;

        virtual SDL_Rect get_chunk_rect( point p, point tiles_count ) const = 0;
};

class pixel_minimap_ortho_projector : public pixel_minimap_projector
{
    public:
        pixel_minimap_ortho_projector( point total_tiles_count,
                                       const SDL_Rect &max_screen_rect,
                                       bool square_pixels );

        point get_tile_size() const override;
        point get_tiles_size( point tiles_count ) const override;
        point get_tile_pos( point p, point tiles_count ) const override;

        SDL_Rect get_chunk_rect( point p, point tiles_count ) const override;

    private:
        point tile_size;
};

class pixel_minimap_iso_projector : public pixel_minimap_projector
{
    public:
        pixel_minimap_iso_projector( point total_tiles_count,
                                     const SDL_Rect &max_screen_rect,
                                     bool square_pixels );

        point get_tile_size() const override;
        point get_tiles_size( point tiles_count ) const override;
        point get_tile_pos( point p, point tiles_count ) const override;

        SDL_Rect get_chunk_rect( point p, point tiles_count ) const override;

    private:
        point total_tiles_count;
        point tile_size;
};


