#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "point.h"

#if defined( TILES )
struct loading_image_cache;
struct loading_image_selection_state {
    std::vector<std::string> paths;
    std::size_t next_path = 0;
    std::string current_path;
    std::optional<std::string> current_author;
    bool lookup_attempted = false;
};
#endif

class background_pane;
class loading_image_splash;
class ui_adaptor;
class uilist;

struct loading_image_scaling_options {
    point image_size = point_zero;
    point screen_size = point_zero;
};

auto get_scaled_loading_image_size( const loading_image_scaling_options &opts ) ->
std::optional<point>;

class loading_image_splash
{
    private:
        std::unique_ptr<background_pane> ui_background;
#if defined( TILES )
        loading_image_selection_state owned_selection_state;
        loading_image_selection_state *selection_state = nullptr;
        bool selected_image_for_this_ui = false;
        std::unique_ptr<loading_image_cache> loading_image_cache_state;

        auto draw_current_loading_image() -> bool;
#endif

    public:
        loading_image_splash();
#if defined( TILES )
        explicit loading_image_splash( loading_image_selection_state &selection_state );
#endif
        ~loading_image_splash();
};

class loading_ui
{
    private:
        std::unique_ptr<uilist> menu;
        std::unique_ptr<ui_adaptor> ui;
        std::unique_ptr<loading_image_splash> ui_splash;
#if defined( TILES )
        loading_image_selection_state loading_image_selection;
#endif

        void init();
    public:
        loading_ui( bool display );
        ~loading_ui();

        /**
         * Sets the description for the menu and clears existing entries.
         */
        void new_context( const std::string &desc );
        /**
         * Adds a named entry in the current loading context.
         */
        void add_entry( const std::string &description );
        /**
         * Place the UI onto UI stack, mark current entry as processed, scroll down,
         * and redraw. (if display is enabled)
         */
        void proceed();
        /**
         * Place the UI onto UI stack and redraw it on the screen (if display is enabled).
         */
        void show();
};
