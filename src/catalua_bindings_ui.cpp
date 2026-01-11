#include "catalua_bindings.h"

#include <optional>
#include "catalua_bindings_utils.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "lua_sidebar_widgets.h"
#include "panels.h"
#include "ui.h"
#include "popup.h"
#include "string_input_popup.h"

void cata::detail::reg_ui_elements( sol::state &lua )
{
    {
        sol::usertype<uilist> ut =
            luna::new_usertype<uilist>(
                lua,
                luna::no_bases,
                luna::constructors <
                uilist()
                > ()
            );
        DOC( "Sets title which is on the top line." );
        luna::set_fx( ut, "title", []( uilist & ui, const std::string & text ) { ui.title = text; } );
        DOC( "Sets text which is in upper box." );
        luna::set_fx( ut, "text", []( uilist & ui, const std::string & input ) { ui.text = input; } );
        DOC( "Sets footer text which is in lower box. It overwrites descs of entries unless is empty." );
        luna::set_fx( ut, "footer", []( uilist & ui, const std::string & text ) { ui.footer_text = text; } );
        DOC( "Puts a lower box. Footer or entry desc appears on it." );
        luna::set_fx( ut, "desc_enabled", []( uilist & ui, bool value ) { ui.desc_enabled = value; } );
        DOC( "Adds an entry. `string` is its name, and `int` is what it returns. If `int` is `-1`, the number is decided orderly." );
        luna::set_fx( ut, "add", []( uilist & ui, int retval, const std::string & text ) { ui.addentry( retval, true, MENU_AUTOASSIGN, text ); } );
        DOC( "Adds an entry with desc(second `string`). `desc_enabled(true)` is required for showing desc." );
        luna::set_fx( ut, "add_w_desc", []( uilist & ui, int retval, const std::string & text,
        const std::string & desc ) { ui.addentry_desc( retval, true, MENU_AUTOASSIGN, text, desc ); } );
        DOC( "Adds an entry with desc and col(third `string`). col is additional text on the right of the entry name." );
        luna::set_fx( ut, "add_w_col", []( uilist & ui, int retval, const std::string & text,
        const std::string & desc, const std::string col ) { ui.addentry_col( retval, true, MENU_AUTOASSIGN, text, col, desc ); } );
        DOC( "Entries from uilist. Remember, in lua, the first element of vector is `entries[1]`, not `entries[0]`." );
        luna::set( ut, "entries", &uilist::entries );
        DOC( "Changes the color. Default color is `c_magenta`." );
        luna::set_fx( ut, "border_color", []( uilist & ui, color_id col ) { ui.border_color = get_all_colors().get( col ); } );
        DOC( "Changes the color. Default color is `c_light_gray`." );
        luna::set_fx( ut, "text_color", []( uilist & ui, color_id col ) { ui.text_color = get_all_colors().get( col ); } );
        DOC( "Changes the color. Default color is `c_green`." );
        luna::set_fx( ut, "title_color", []( uilist & ui, color_id col ) { ui.title_color = get_all_colors().get( col ); } );
        DOC( "Changes the color. Default color is `h_white`." );
        luna::set_fx( ut, "hilight_color", []( uilist & ui, color_id col ) { ui.hilight_color = get_all_colors().get( col ); } );
        DOC( "Changes the color. Default color is `c_light_green`." );
        luna::set_fx( ut, "hotkey_color", []( uilist & ui, color_id col ) { ui.hotkey_color = get_all_colors().get( col ); } );
        DOC( "Returns retval for selected entry, or a negative number on fail/cancel" );
        luna::set_fx( ut, "query", []( uilist & ui ) {
            ui.query();
            return ui.ret;
        } );
    }
    {
        DOC( "This type came from UiList." );
        sol::usertype<uilist_entry> ut =
            luna::new_usertype<uilist_entry>(
                lua,
                luna::no_bases,
                luna::no_constructor
            );
        DOC( "Entry whether it's enabled or not. Default is `true`." );
        luna::set( ut, "enable", &uilist_entry::enabled );
        DOC( "Entry text" );
        luna::set( ut, "txt", &uilist_entry::txt );
        DOC( "Entry description" );
        luna::set( ut, "desc", &uilist_entry::desc );
        DOC( "Entry text of column." );
        luna::set( ut, "ctxt",  &uilist_entry::ctxt );
        DOC( "Entry text color. Its default color is `c_red_red`, which makes color of the entry same as what `uilist` decides. So if you want to make color different, choose one except `c_red_red`." );
        luna::set_fx( ut, "txt_color", []( uilist_entry & ui_entry, color_id col ) { ui_entry.text_color = get_all_colors().get( col ); } );
    }

    {
        sol::usertype<query_popup> ut =
            luna::new_usertype<query_popup>(
                lua,
                luna::no_bases,
                luna::constructors <
                query_popup()
                > ()
            );
        luna::set_fx( ut, "message", []( query_popup & popup, sol::variadic_args va ) { popup.message( "%s", cata::detail::fmt_lua_va( va ) ); } );
        luna::set_fx( ut, "message_color", []( query_popup & popup, color_id col ) { popup.default_color( get_all_colors().get( col ) ); } );
        DOC( "Set whether to allow any key" );
        luna::set_fx( ut, "allow_any_key", []( query_popup & popup, bool val ) { popup.allow_anykey( val ); } );
        DOC( "Returns selected action" );
        luna::set_fx( ut, "query", []( query_popup & popup ) { return popup.query().action; } );
        DOC( "Returns `YES` or `NO`. If ESC pressed, returns `NO`." );
        luna::set_fx( ut, "query_yn", []( query_popup & popup ) {
            return popup
                   .context( "YESNO" )
                   .option( "YES" )
                   .option( "NO" )
                   .query()
                   .action;
        } );
        DOC( "Returns `YES`, `NO` or `QUIT`. If ESC pressed, returns `QUIT`." );
        luna::set_fx( ut, "query_ynq", []( query_popup & popup ) {
            return popup
                   .context( "YESNOQUIT" )
                   .option( "YES" )
                   .option( "NO" )
                   .option( "QUIT" )
                   .query()
                   .action;
        } );
    }

    {
        sol::usertype<string_input_popup> ut =
            luna::new_usertype<string_input_popup>(
                lua,
                luna::no_bases,
                luna::constructors <
                string_input_popup()
                > ()
            );
        DOC( "`title` is on the left of input field." );
        luna::set_fx( ut, "title", []( string_input_popup & sipop, const std::string & text ) { sipop.title( text ); } );
        DOC( "`desc` is above input field." );
        luna::set_fx( ut, "desc", []( string_input_popup & sipop, const std::string & text ) { sipop.description( text ); } );
        DOC( "Returns your input." );
        luna::set_fx( ut, "query_str", []( string_input_popup & sipop ) {
            sipop.only_digits( false );
            return sipop.query_string();
        } );
        DOC( "Returns your input, but allows numbers only." );
        luna::set_fx( ut, "query_int", []( string_input_popup & sipop ) {
            sipop.only_digits( true );
            return sipop.query_int();
        } );
    }

    {
        DOC( "Sidebar utility functions." );
        luna::userlib lib = luna::begin_lib( lua, "sidebar" );
        DOC( "Register a Lua sidebar widget. Options: id(string), name(string), height(int, use -2 to fill remaining space), order(int, 1-based), draw(function), " );
        DOC( "default_toggle(bool), redraw_every_frame(bool), panel_visible(bool|function), render(function). " );
        DOC( "draw(width, height) returns an array of entries: each entry is string or table { text=string, color=Color|string }." );
        DOC( "text may include color tags like <color_red>text</color> for multi-color lines." );
        luna::set_fx( lib, "register_widget", []( const sol::table &opts ) {
            auto get_opt_int = [&]( const char *key, const int fallback ) -> int {
                auto obj = opts.get<sol::object>( key );
                if( !obj.valid() || obj == sol::lua_nil ) {
                    return fallback;
                }
                return obj.as<int>();
            };
            auto get_opt_bool = [&]( const char *key, const bool fallback ) -> bool {
                auto obj = opts.get<sol::object>( key );
                if( !obj.valid() || obj == sol::lua_nil ) {
                    return fallback;
                }
                return obj.as<bool>();
            };
            auto get_opt_optional_int = [&]( const char *key ) -> std::optional<int> {
                auto obj = opts.get<sol::object>( key );
                if( !obj.valid() || obj == sol::lua_nil ) {
                    return std::nullopt;
                }
                return obj.as<int>();
            };
            auto panel_visible_value = std::optional<bool>{};
            auto panel_visible_fn = std::optional<sol::protected_function>{};
            auto panel_visible_obj = opts.get<sol::object>( "panel_visible" );
            if( panel_visible_obj.valid() && panel_visible_obj != sol::lua_nil ) {
                if( panel_visible_obj.is<sol::function>() ) {
                    panel_visible_fn = panel_visible_obj.as<sol::protected_function>();
                } else if( panel_visible_obj.is<bool>() ) {
                    panel_visible_value = panel_visible_obj.as<bool>();
                }
            }
            auto draw_fn = opts.get_or<sol::protected_function>( "draw", sol::lua_nil );
            auto render_fn = opts.get_or<sol::protected_function>( "render", sol::lua_nil );
            auto render_opt = render_fn == sol::lua_nil ?
                              std::optional<sol::protected_function>{} :
                              std::optional<sol::protected_function>{ render_fn };
            auto widget_opts = cata::lua_sidebar_widgets::widget_options{
                .id = opts.get_or<std::string>( "id", "" ),
                .name = opts.get_or<std::string>( "name", "" ),
                .height = get_opt_int( "height", 1 ),
                .order = get_opt_optional_int( "order" ),
                .default_toggle = get_opt_bool( "default_toggle", true ),
                .redraw_every_frame = get_opt_bool( "redraw_every_frame", false ),
                .panel_visible_value = panel_visible_value,
                .panel_visible_fn = panel_visible_fn,
                .draw = draw_fn,
                .render = render_opt,
            };
            cata::lua_sidebar_widgets::register_widget( widget_opts );
            panel_manager::get_manager().sync_lua_panels();
        } );
        DOC( "Clear all registered Lua sidebar widgets." );
        luna::set_fx( lib, "clear_widgets", []() {
            cata::lua_sidebar_widgets::clear_widgets();
            panel_manager::get_manager().sync_lua_panels();
        } );
        DOC( "Returns current sidebar layout id (e.g. classic, compact, labels)." );
        luna::set_fx( lib, "get_layout_id", []() {
            return panel_manager::get_manager().get_current_layout_id();
        } );
        luna::finalize_lib( lib );
    }
}
