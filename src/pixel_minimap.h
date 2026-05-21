#pragma once

#include <map>
#include <memory>
#include <string>

#include "coordinates.h"
#include "sdl_wrappers.h"
#include "sdl_geometry.h"

class pixel_minimap_projector;

enum class pixel_minimap_type {
    ortho,
    iso
};

enum class pixel_minimap_mode {
    solid,
    squares,
    dots
};

struct pixel_minimap_settings {
    pixel_minimap_mode mode = pixel_minimap_mode::solid;
    int brightness = 100;
    int beacon_size = 2;
    int beacon_blink_interval = 0;
    bool square_pixels = true;
    bool scale_to_fit = false;
};

class pixel_minimap
{
    public:
        pixel_minimap( const SDL_Renderer_Ptr &renderer, const GeometryRenderer_Ptr &geometry );
        ~pixel_minimap();

        void set_type( pixel_minimap_type type );
        void set_settings( const pixel_minimap_settings &settings );

        void draw( const SDL_Rect &screen_rect, const tripoint_bub_ms &center );
        bool has_animated_elements() const;
        void reset();

    private:
        struct submap_cache;

        submap_cache &get_cache_at( const tripoint_abs_sm &abs_sm_pos );

        void set_screen_rect( const SDL_Rect &screen_rect );

        void draw_beacon( const SDL_Rect &rect, const SDL_Color &color );

        void process_cache( const tripoint_bub_ms &center );

        void flush_cache_updates();
        void update_cache_at( const tripoint_bub_sm &pos );
        void prepare_cache_for_updates( const tripoint_bub_ms &center );
        void clear_unused_cache();

        void render( const tripoint_bub_ms &center );
        void render_cache( const tripoint_bub_ms &center );
        void render_critters( const tripoint_bub_ms &center );

        std::unique_ptr<pixel_minimap_projector> create_projector( const SDL_Rect &max_screen_rect ) const;

    private:
        const SDL_Renderer_Ptr &renderer;
        const GeometryRenderer_Ptr &geometry;

        pixel_minimap_type type;
        pixel_minimap_settings settings;

        point pixel_size;

        //track the previous viewing area to determine if the minimap cache needs to be cleared
        tripoint_abs_sm cached_center_sm;
        std::string cached_dimension_id;
        // track presence of animated beacons to determine whether the minimap needs to be animated
        bool cached_has_animated_beacons = true;

        SDL_Rect screen_rect;
        SDL_Rect main_tex_clip_rect;
        SDL_Rect screen_clip_rect;

        SDL_Texture_Ptr main_tex;

        std::unique_ptr<pixel_minimap_projector> projector;
        int built_mapsize = 0;

        //the minimap texture pool which is used to reduce new texture allocation spam
        class shared_texture_pool;
        std::unique_ptr<shared_texture_pool> tex_pool;

        std::map<tripoint_abs_sm, submap_cache> cache;
};
