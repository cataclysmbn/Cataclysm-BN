#include "loading_ui.h"
#include "loading_ui_client.h"
#include "client_presentation.h"
#include "cached_options.h"
#include "color.h"
#include "input.h"
#include "options.h"
#include "path_info.h"
#include "output.h"
#include "ui.h"
#include "ui_manager.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

auto get_scaled_loading_image_size( const loading_image_scaling_options &opts ) ->
std::optional<point>
{
    if( opts.image_size.x <= 0 || opts.image_size.y <= 0 || opts.screen_size.x <= 0 ||
        opts.screen_size.y <= 0 ) {
        return std::nullopt;
    }

    const auto height_scale = static_cast<double>( opts.screen_size.y ) /
                              static_cast<double>( opts.image_size.y );

    return point( std::max( 1, static_cast<int>( std::lround( opts.image_size.x * height_scale ) ) ),
                  opts.screen_size.y );
}

loading_image_splash::loading_image_splash() : loading_image_splash( owned_selection_state ) {}

loading_image_splash::loading_image_splash( loading_image_selection_state &state )
    : selection_state( &state ), image_renderer( game_client::make_loading_image_renderer() )
{
    ui_background = std::make_unique<background_pane>( [this]() {
        if( image_renderer ) { image_renderer->draw( *selection_state ); }
    } );
}
loading_image_splash::~loading_image_splash() { game_client::presentation().clear_display(); }

loading_ui::loading_ui( bool display )
{
    if( display && !test_mode ) {
        menu = std::make_unique<uilist>();
        menu->settext( _( "Loading" ) );
    }
}

loading_ui::~loading_ui()
{
    if( reported ) {
        game_client::notify_loading( { .done = true } );
    }
    game_client::presentation().clear_display();
}

void loading_ui::add_entry( const std::string &description )
{
    if( menu != nullptr ) {
        menu->addentry( menu->entries.size(), true, 0, description );
    }
}

void loading_ui::new_context( const std::string &desc )
{
    if( menu != nullptr ) {
        menu->reset();
        menu->settext( desc );
        ui = nullptr;
        ui_splash = nullptr;
    }
}

void loading_ui::init()
{
    if( menu != nullptr && ui == nullptr ) {
        ui_splash = std::make_unique<loading_image_splash>( loading_image_selection );

        ui = std::make_unique<ui_adaptor>();
        ui->on_screen_resize( [this]( ui_adaptor & ui ) { menu->reposition( ui ); } );
        menu->reposition( *ui );
        ui->on_redraw( [this]( ui_adaptor & ui ) {
            if( !get_option<bool>( "LOADING_PROGRESS_COMPACT" ) || menu->entries.empty() ) {
                menu->show( ui );
                return;
            }

            const int last = static_cast<int>( menu->entries.size() ) - 1;
            const int sel = std::clamp( menu->selected, 0, last );
            const int width = std::min( TERMX, 40 );
            const int row_width = width - 2;
            const double frac = last > 0 ? static_cast<double>( sel ) / last : 1.0;
            const int filled = std::clamp( static_cast<int>( std::lround( frac * row_width ) ), 0, row_width );

            catacurses::window w = catacurses::newwin( 4, width, point( ( TERMX - width ) / 2, TERMY - 4 ) );
            werase( w );
            draw_border( w, c_magenta );

            trim_and_print( w, point( 1, 1 ), row_width, c_white, menu->text );
            std::string row = utf8_truncate( remove_color_tags( menu->entries[sel].txt ), row_width );
            row += std::string( row_width - utf8_width( row, true ), ' ' );

            const utf8_wrapper row_wrapper( row );
            int col = 0;
            for( size_t i = 0; i < row_wrapper.length(); ++i ) {
                const std::string ch = row_wrapper.substr( i, 1 ).str();
                mvwprintz( w, point( 1 + col, 2 ), col < filled ? h_white : c_light_gray, "%s", ch );
                col += utf8_width( ch, true );
            }

            wnoutrefresh( w );
        } );
    }
}

void loading_ui::report()
{
    if( menu == nullptr || !game_client::has_loading_observer() ) {
        return;
    }
    reported = true;
    auto progress = game_client::loading_progress{ .title = menu->text };
    for( const auto &entry : menu->entries ) {
        progress.entries.push_back( remove_color_tags( entry.txt ) );
    }
    progress.index = static_cast<std::size_t>( std::max( menu->selected, 0 ) );
    if( !loading_image_selection.current_path.empty() ) {
        // The client knows the base path, not the working directory this path was found from.
        const auto path = std::filesystem::path( loading_image_selection.current_path ).lexically_normal();
        const auto relative = path.lexically_relative( std::filesystem::path(
                                  PATH_INFO::base_path() ).lexically_normal() );
        progress.image = { .path = ( relative.empty() || *relative.begin() == ".." ? path : relative ).generic_string(),
                           .author = loading_image_selection.current_author
                         };
    }
    game_client::notify_loading( progress );
}

void loading_ui::proceed()
{
    init();

    if( menu != nullptr && !menu->entries.empty() ) {
        if( menu->selected >= 0 && menu->selected < static_cast<int>( menu->entries.size() ) ) {
            // TODO: Color it red if it errored hard, yellow on warnings
            menu->entries[menu->selected].text_color = c_green;
        }

        if( menu->selected + 1 < static_cast<int>( menu->entries.size() ) ) {
            menu->scrollby( 1 );
        }
    }

    show();
}

void loading_ui::show()
{
    init();

    if( menu != nullptr ) {
        ui_manager::redraw();
        refresh_display();
        inp_mngr.pump_events();
        report();
    }
}
