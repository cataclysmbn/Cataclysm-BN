#include "popup.h"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>

#include "cached_options.h"
#include "catacharset.h"
#include "client_interaction.h"
#include "json.h"
#include "ime.h"
#include "input.h"
#include "output.h"
#include "sdl_wrappers.h"
#include "ui_manager.h"

query_popup::query_popup()
    : cur( 0 ), default_text_color( c_white ), anykey( false ), cancel( false ), ontop( false ),
      fullscr( false )
{
}

query_popup &query_popup::context( const std::string &cat )
{
    invalidate_ui();
    category = cat;
    return *this;
}

query_popup &query_popup::option( const std::string &opt )
{
    invalidate_ui();
    options.emplace_back( opt, []( const input_event & ) {
        return true;
    } );
    return *this;
}

query_popup &query_popup::option( const std::string &opt,
                                  const std::function<bool( const input_event & )> &filter )
{
    invalidate_ui();
    options.emplace_back( opt, filter );
    return *this;
}

query_popup &query_popup::allow_anykey( bool allow )
{
    // Change does not affect cache, do not invalidate the window
    anykey = allow;
    return *this;
}

query_popup &query_popup::allow_cancel( bool allow )
{
    // Change does not affect cache, do not invalidate the window
    cancel = allow;
    return *this;
}

query_popup &query_popup::on_top( bool top )
{
    invalidate_ui();
    ontop = top;
    return *this;
}

query_popup &query_popup::full_screen( bool full )
{
    invalidate_ui();
    fullscr = full;
    return *this;
}

query_popup &query_popup::cursor( size_t pos )
{
    // Change does not affect cache, do not invalidate window
    cur = pos;
    return *this;
}

query_popup &query_popup::default_color( const nc_color &d_color )
{
    default_text_color = d_color;
    return *this;
}

std::vector<std::vector<std::string>> query_popup::fold_query(
                                       const std::string &category,
                                       const std::vector<query_option> &options,
                                       const int max_width, const int horz_padding )
{
    input_context ctxt( category );

    std::vector<std::vector<std::string>> folded_query;
    folded_query.emplace_back();

    int query_cnt = 0;
    int query_width = 0;
    for( const auto &opt : options ) {
        const auto &name = ctxt.get_action_name( opt.action );
        const auto &desc = ctxt.get_desc( opt.action, name, opt.filter );
        const int this_query_width = utf8_width( desc, true ) + horz_padding;
        ++query_cnt;
        query_width += this_query_width;
        if( query_width > max_width + horz_padding ) {
            if( query_cnt == 1 ) {
                // Each line has at least one query, so keep this query in the current line
                folded_query.back().emplace_back( desc );
                folded_query.emplace_back();
                query_cnt = 0;
                query_width = 0;
            } else {
                // Wrap this query to the next line
                folded_query.emplace_back();
                folded_query.back().emplace_back( desc );
                query_cnt = 1;
                query_width = this_query_width;
            }
        } else {
            folded_query.back().emplace_back( desc );
        }
    }

    if( folded_query.back().empty() ) {
        folded_query.pop_back();
    }

    return folded_query;
}

void query_popup::invalidate_ui() const
{
    if( win ) {
        win = {};
        folded_msg.clear();
        buttons.clear();
    }
    std::shared_ptr<ui_adaptor> ui = adaptor.lock();
    if( ui ) {
        ui->mark_resize();
    }
}

constexpr int border_width = 1;

void query_popup::init() const
{
    constexpr int horz_padding = 2;
    constexpr int vert_padding = 1;
    const int max_line_width = FULL_SCREEN_WIDTH - border_width * 2;

    // Fold message text
    folded_msg = foldstring( text, max_line_width );

    // Fold query buttons
    const auto &folded_query = fold_query( category, options, max_line_width, horz_padding );

    // Calculate size of message part
    int msg_width = 0;
    int msg_height = folded_msg.size();

    for( const auto &line : folded_msg ) {
        msg_width = std::max( msg_width, utf8_width( line, true ) );
    }

    // Calculate width with query buttons
    for( const auto &line : folded_query ) {
        if( !line.empty() ) {
            int button_width = 0;
            for( const auto &opt : line ) {
                button_width += utf8_width( opt, true );
            }
            msg_width = std::max( msg_width, button_width +
                                  horz_padding * static_cast<int>( line.size() - 1 ) );
        }
    }
    msg_width = std::min( msg_width, max_line_width );

    // Calculate height with query buttons & button positions
    buttons.clear();
    if( !folded_query.empty() ) {
        msg_height += vert_padding;
        for( const auto &line : folded_query ) {
            if( !line.empty() ) {
                int button_width = 0;
                for( const auto &opt : line ) {
                    button_width += utf8_width( opt, true );
                }
                // Right align.
                // TODO: multi-line buttons
                int button_x = std::max( 0, msg_width - button_width -
                                         horz_padding * static_cast<int>( line.size() - 1 ) );
                for( const auto &opt : line ) {
                    buttons.emplace_back( opt, point( button_x, msg_height ) );
                    button_x += utf8_width( opt, true ) + horz_padding;
                }
                msg_height += 1 + vert_padding;
            }
        }
        msg_height -= vert_padding;
    }

    // Calculate window size
    const int win_width = std::min( TERMX,
                                    fullscr ? FULL_SCREEN_WIDTH : msg_width + border_width * 2 );
    const int win_height = std::min( TERMY,
                                     fullscr ? FULL_SCREEN_HEIGHT : msg_height + border_width * 2 );
    const int win_x = ( TERMX - win_width ) / 2;
    const int win_y = ontop ? 0 : ( TERMY - win_height ) / 2;
    win = catacurses::newwin( win_height, win_width, point( win_x, win_y ) );

    std::shared_ptr<ui_adaptor> ui = adaptor.lock();
    if( ui ) {
        ui->position_from_window( win );
    }
}

void query_popup::show() const
{
    if( !win ) {
        init();
    }

    werase( win );
    draw_border( win );

    for( size_t line = 0; line < folded_msg.size(); ++line ) {
        nc_color col = default_text_color;
        print_colored_text( win, point( border_width, border_width + line ), col, col,
                            folded_msg[line] );
    }

    for( size_t ind = 0; ind < buttons.size(); ++ind ) {
        nc_color col = ind == cur ? hilite( c_white ) : c_white;
        const auto &btn = buttons[ind];
        print_colored_text( win, btn.pos + point( border_width, border_width ),
                            col, col, btn.text );
    }

    wnoutrefresh( win );
}

std::shared_ptr<ui_adaptor> query_popup::create_or_get_adaptor( bool disable_below )
{
    std::shared_ptr<ui_adaptor> ui = adaptor.lock();
    if( !ui ) {
        if( disable_below ) {
            ui = std::make_shared<ui_adaptor>( ui_adaptor::disable_uis_below{} );
        } else {
            ui = std::make_shared<ui_adaptor>();
        }
        adaptor = ui;
        ui->on_redraw( [this]( const ui_adaptor & ) {
            show();
        } );
        ui->on_screen_resize( [this]( ui_adaptor & ) {
            init();
        } );
        ui->mark_resize();
    }
    return ui;
}

auto query_popup::register_input_actions( input_context &ctxt ) const -> void
{
    if( cancel || !options.empty() ) { ctxt.register_action( "HELP_KEYBINDINGS" ); }
    if( !options.empty() ) {
        ctxt.register_action( "LEFT" );
        ctxt.register_action( "RIGHT" );
        ctxt.register_action( "CONFIRM" );
        for( const auto &opt : options ) { ctxt.register_action( opt.action ); }
    }
    if( anykey ) {
        ctxt.register_action( "ANY_INPUT" );
        ctxt.register_action( "COORDINATE" );
    }
    if( cancel ) { ctxt.register_action( "QUIT" ); }
}

auto query_popup::make_interaction( const input_context &ctxt ) const ->
game_client::interaction_snapshot // *NOPAD*
{
    auto snapshot = game_client::interaction_snapshot{
        .kind = game_client::interaction_kind::choices,
        .message = remove_color_tags( text ),
        .allow_cancel = cancel,
    };
    snapshot.choices.reserve( options.size() + ( ( anykey || cancel ) && options.empty() ? 1 : 0 ) );
    for( auto index = std::size_t{ 0 }; index < options.size(); ++index ) {
        const auto &option = options[index];
        const auto label = ctxt.get_action_name( option.action );
        snapshot.choices.push_back( {
            .id = "option:" + std::to_string( index ),
            .label = remove_color_tags( label ),
            .description = option.action,
            .selected = index == cur,
            .highlighted = index == cur,
        } );
    }
    if( ( anykey || cancel ) && options.empty() ) {
        const auto action = anykey ? "ANY_INPUT" : "QUIT";
        const auto label = ctxt.get_action_name( action );
        snapshot.choices.push_back( {
            .id = "acknowledge",
            .label = label,
            .description = action,
            .selected = true,
            .highlighted = true,
        } );
    }
    return snapshot;
}

query_popup::result query_popup::query_once()
{
    if( !anykey && !cancel && options.empty() ) {
        return { false, "ERROR", {} };
    }

    if( test_mode ) {
        return { false, "ERROR", {} };
    }

    std::shared_ptr<ui_adaptor> ui = create_or_get_adaptor();

    ui_manager::redraw();

    auto ctxt = input_context( category );
    register_input_actions( ctxt );

    result res;
    // Assign outside construction of `res` to ensure execution order
    res.wait_input = !anykey;
    do {
        const auto interaction = game_client::interaction_scope( ctxt, [this, &ctxt]() {
            return make_interaction( ctxt );
        } );
        res.action = ctxt.handle_input();
        res.evt = ctxt.get_raw_input();
    } while(
        // Always ignore mouse movement
        ( res.evt.type == input_event_t::mouse && res.evt.get_first_input() == MOUSE_MOVE ) ||
        // Ignore window losing focus in SDL
        ( res.evt.type == input_event_t::keyboard && res.evt.sequence.empty() )
    );

    return apply_response( std::move( res ) );
}

auto query_popup::apply_response( result res ) -> result
{
    if( res.evt.interaction &&
        res.evt.interaction->operation == game_client::interaction_operation::cancel ) {
        res.action = "QUIT";
        res.wait_input = false;
    } else if( res.evt.interaction &&
               res.evt.interaction->operation == game_client::interaction_operation::choose ) {
        if( res.evt.interaction->target_id == "acknowledge" ) {
            res.action = anykey ? "ANY_INPUT" : "QUIT";
            res.wait_input = false;
        } else {
            const auto selected_option = std::ranges::find_if( options, [&]( const auto & option ) {
                const auto index = static_cast<std::size_t>( &option - options.data() );
                return res.evt.interaction->target_id == "option:" + std::to_string( index );
            } );
            if( selected_option != options.end() ) {
                cur = static_cast<std::size_t>( std::distance( options.begin(), selected_option ) );
                res.action = options[cur].action;
                res.wait_input = !options[cur].filter( res.evt );
            }
        }
    } else if( cancel && res.action == "QUIT" ) {
        res.wait_input = false;
    } else if( res.action == "LEFT" ) {
        if( cur > 0 ) {
            --cur;
        } else {
            cur = options.size() - 1;
        }
    } else if( res.action == "RIGHT" ) {
        if( cur + 1 < options.size() ) {
            ++cur;
        } else {
            cur = 0;
        }
    } else if( res.action == "CONFIRM" ) {
        if( cur < options.size() ) {
            res.wait_input = false;
            res.action = options[cur].action;
        }
    } else if( res.action == "HELP_KEYBINDINGS" ) {
        // Keybindings may have changed, regenerate the UI
        init();
    } else {
        for( size_t ind = 0; ind < options.size(); ++ind ) {
            if( res.action == options[ind].action ) {
                cur = ind;
                if( options[ind].filter( res.evt ) ) {
                    res.wait_input = false;
                    break;
                }
            }
        }
    }

    return res;
}

query_popup::result query_popup::query()
{
    ime_sentry sentry( ime_sentry::disable );

    std::shared_ptr<ui_adaptor> ui = create_or_get_adaptor();

    result res;
    do {
        res = query_once();
    } while( res.wait_input );
    return res;
}

std::string query_popup::wait_text( const std::string &text, const nc_color &bar_color )
{
    static const std::array<std::string, 4> phase_icons = {{ "|", "/", "-", "\\" }};
    static size_t phase = phase_icons.size() - 1;
    phase = ( phase + 1 ) % phase_icons.size();
    return string_format( " %s %s", colorize( phase_icons[phase], bar_color ), text );
}

std::string query_popup::wait_text( const std::string &text )
{
    return wait_text( text, c_light_green );
}

query_popup::result::result()
    : wait_input( false ), action( "ERROR" )
{
}

query_popup::result::result( bool wait_input, const std::string &action, const input_event &evt )
    : wait_input( wait_input ), action( action ), evt( evt )
{
}

query_popup::query_option::query_option(
    const std::string &action,
    const std::function<bool( const input_event & )> &filter )
    : action( action ), filter( filter )
{
}

query_popup::button::button( const std::string &text, point p )
    : text( text ), pos( p )
{
}

static_popup::static_popup()
{
    ui = create_or_get_adaptor();
}

throbber_popup::throbber_popup( const std::string &msg ) : msg( msg )
{
    on_top( true );
    ui = create_or_get_adaptor( true );
}

void throbber_popup::refresh()
{
    static constexpr std::chrono::milliseconds update_interval( 500 );
    auto now = std::chrono::steady_clock::now();
    if( last_update + update_interval < now ) {
        wait_message( "%s", msg ); // re-assign the message to advance the animation
        ui_manager::redraw();
        refresh_display();
        last_update = now;
        inp_mngr.pump_events();
    }
}
