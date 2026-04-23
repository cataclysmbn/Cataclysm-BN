#pragma once

#include <memory>
#include <optional>
#include <string>

class background_pane;
class loading_image_splash;
class ui_adaptor;
class uilist;

class loading_image_splash
{
    private:
        std::unique_ptr<background_pane> ui_background;
        std::string loading_image_path;
        std::optional<std::string> loading_image_author;
        bool loading_image_lookup_attempted = false;

    public:
        loading_image_splash();
        ~loading_image_splash();
};

class loading_ui
{
    private:
        std::unique_ptr<uilist> menu;
        std::unique_ptr<ui_adaptor> ui;
        std::unique_ptr<loading_image_splash> ui_splash;

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
