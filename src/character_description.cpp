#include "character_description.h"

#include <algorithm>

#include "character.h"
#include "cursesdef.h"
#include "input.h"
#include "npc.h"
#include "npc_class.h"
#include "output.h"
#include "string_editor_window.h"
#include "translations.h"
#include "ui_manager.h"

auto get_character_description( const Character &who ) -> std::string
{
    const auto custom = who.get_value( "character_description" );
    if( !custom.empty() ) {
        return custom;
    }
    if( const auto *np = who.as_npc() ) {
        const auto template_id = string_id<npc_template> { np->idz };
        if( template_id.is_valid() && !template_id->description.empty() ) {
            return template_id->description.translated();
        }
        if( np->myclass.is_valid() ) {
            return np->myclass->get_description();
        }
    }
    return {};
}

auto set_character_description( Character &who, const std::string &description ) -> void
{
    if( description.empty() ) {
        who.remove_value( "character_description" );
    } else {
        who.set_value( "character_description", description );
    }
}

auto edit_character_description( Character &who ) -> void
{
    auto border = catacurses::window{};
    auto context = input_context{ "STRING_EDITOR" };
    context.register_action( "TEXT.CONFIRM" );
    context.register_action( "TEXT.QUIT" );
    auto ui = ui_adaptor{};
    ui.on_screen_resize( [&]( auto & adaptor ) {
        border = catacurses::newwin( TERMY, TERMX, point_zero );
        adaptor.position_from_window( border );
    } );
    ui.on_redraw( [&]( const auto & /*adaptor*/ ) {
        werase( border );
        draw_border( border, BORDER_COLOR, _( "Character description" ) );
        trim_and_print( border, point( 2, 1 ), getmaxx( border ) - 4, c_light_gray,
                        _( "Describe your character's past and history." ) );
        trim_and_print( border, point( 2, getmaxy( border ) - 2 ), getmaxx( border ) - 4,
                        c_light_gray, _( "%s: save  %s: cancel" ), context.get_desc( "TEXT.CONFIRM" ),
                        context.get_desc( "TEXT.QUIT" ) );
        wnoutrefresh( border );
    } );
    ui.mark_resize();
    auto editor = string_editor_window{ []()
    {
        return catacurses::newwin( std::max( 1, TERMY - 5 ), std::max( 1, TERMX - 4 ),
                                   point( 2, 2 ) );
    }, get_character_description( who ), { .highlight_lua = false } };
    const auto [confirmed, text] = editor.query_string();
    if( confirmed ) {
        set_character_description( who, text );
    }
}
