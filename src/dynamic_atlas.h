#pragma once

#if defined(TILES)

#include <string>
#include <optional>
#include <vector>
#include <functional>

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
            SDL_Surface_Ptr surface;
            std::unique_ptr<detail::texture_packer> packer;
            int atlas_width;
            int atlas_height;
            bool dirty;
        };
        using sprite_callback = std::function<void( SDL_Surface *, const SDL_Rect * )>;

        dynamic_atlas()
            : max_atlas_width( 0 ), max_atlas_height( 0 ), hint_sprite_width( 0 ),
              hint_sprite_height( 0 ), is_batching( false ) {}
        dynamic_atlas( const int w, const int h, const int sw = 0, const int sh = 0 )
            : max_atlas_width( w ), max_atlas_height( h ), hint_sprite_width( sw ),
              hint_sprite_height( sh ), is_batching( false ) {}

        auto find_sprite( size_t id ) -> std::optional<atlas_texture>;
        auto create_sprite( int w, int h, const std::optional<size_t> &id,
                            const sprite_callback & ) -> atlas_texture;
        auto get_or_create_sprite( int w, int h, const std::optional<size_t> &id,
                                   const sprite_callback & ) -> atlas_texture;
        void clear();

        void readback_load();
        auto readback_find( const texture &tex ) -> std::tuple<bool, SDL_Surface *, SDL_Rect>;
        void readback_dump( const std::string &s );
        void readback_clear();

        void start_batch();
        void end_batch();

        auto get_staging_area( int width,
                               int height ) -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>;

        auto begin() const { return sheets.begin(); }
        auto end() const { return sheets.end(); }

    private:
        struct staging_area {
            SDL_Surface_Ptr surf;
            SDL_Texture_Ptr tex;
        };

        auto assign_id_internal( size_t id, const atlas_texture &tex ) -> bool;
        auto allocate_sprite_internal( int w, int h ) -> atlas_texture;
        auto update_staging_area( staging_area &staging, int width, int height ) const
        -> std::tuple<SDL_Texture *, SDL_Surface *, SDL_Rect>;
        std::vector<sprite_sheet> sheets;
        std::unordered_map<size_t, std::pair<int, SDL_Rect>> sprite_ids;

        staging_area user_staging;
        staging_area local_staging;

        int max_atlas_width;
        int max_atlas_height;
        int hint_sprite_width;
        int hint_sprite_height;
        bool is_batching;
};

#endif
