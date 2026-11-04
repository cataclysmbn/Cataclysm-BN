#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "point.h"

class loading_image_renderer;
struct loading_image_selection_state {
    std::vector<std::string> paths;
    std::size_t next_path = 0;
    std::string current_path;
    std::optional<std::string> current_author;
    bool lookup_attempted = false;
};

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
        loading_image_selection_state owned_selection_state;
        loading_image_selection_state *selection_state = nullptr;
        std::unique_ptr<loading_image_renderer> image_renderer;

    public:
        loading_image_splash();
        explicit loading_image_splash( loading_image_selection_state &selection_state );
        ~loading_image_splash();
};

class loading_ui
{
    private:
        std::unique_ptr<uilist> menu;
        std::unique_ptr<ui_adaptor> ui;
        std::unique_ptr<loading_image_splash> ui_splash;
        loading_image_selection_state loading_image_selection;
        bool reported = false;

        void init();
        /// Tell the loading observer, if any, what the screen shows now.
        void report();
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
