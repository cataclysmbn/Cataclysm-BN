#pragma once

#if defined(TILES)

#include <string>
#include <optional>
#include <vector>

#include "sdl_wrappers.h"

#include <unordered_map>

class texture;

namespace detail
{
class texture_packer
{
    protected:
        SDL_Rect bounds;
        explicit texture_packer( const SDL_Rect &bounds ) : bounds( bounds ) {}
    public:
        virtual std::optional<SDL_Rect> pack( uint32_t w, uint32_t h ) = 0;
        virtual ~texture_packer() = 0;
};
} // namespace detail

using atlas_texture = std::pair<SDL_Texture_SharedPtr, SDL_Rect>;

class dynamic_atlas
{
    public:
        struct sprite_sheet {
            SDL_Texture_SharedPtr texture;
            std::unique_ptr<detail::texture_packer> packer;
            int atlas_width;
            int atlas_height;
            SDL_Surface_Ptr readback;
            bool dirty;
        };

        dynamic_atlas()
            : max_atlas_width( 0 ), max_atlas_height( 0 ), hint_sprite_width( 0 ), hint_sprite_height( 0 ) {}
        dynamic_atlas( const int w, const int h, const int sw = 0, const int sh = 0 )
            : max_atlas_width( w ), max_atlas_height( h ), hint_sprite_width( sw ), hint_sprite_height( sh ) {}

        auto allocate_sprite( int w, int h ) -> atlas_texture;
        void clear();

        void readback_load();
        auto readback_find( const texture &tex ) -> std::tuple<bool, SDL_Surface *, SDL_Rect>;
        void readback_dump( const std::string &s ) const;
        void readback_clear();

        auto get_staging_area( int width,
                               int height ) -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>;

        auto begin() const { return sheets.begin(); }
        auto end() const { return sheets.end(); }

        auto id_assign( size_t id, const atlas_texture &tex ) -> bool;
        auto id_search( size_t id ) -> std::optional<atlas_texture>;
    private:
        std::vector<sprite_sheet> sheets;
        std::unordered_map<size_t, std::pair<int, SDL_Rect>> sprite_ids;
        SDL_Surface_Ptr staging_surf;
        SDL_Texture_Ptr staging_tex;

        int max_atlas_width;
        int max_atlas_height;
        int hint_sprite_width;
        int hint_sprite_height;
};

#endif
