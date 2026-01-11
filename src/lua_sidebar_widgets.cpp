#include "lua_sidebar_widgets.h"

#include <ranges>
#include <utility>

#include "debug.h"

namespace cata::lua_sidebar_widgets
{
namespace
{
auto widgets_storage() -> std::vector<widget_entry> & // *NOPAD*
{
    static auto widgets = std::vector<widget_entry> {};
    return widgets;
}

auto normalize_name( const std::string &id, const std::string &name ) -> std::string
{
    return name.empty() ? id : name;
}
} // namespace

auto register_widget( const widget_options &opts ) -> void
{
    if( opts.id.empty() ) {
        debugmsg( "Lua sidebar widget id must not be empty." );
        return;
    }
    if( opts.draw == sol::lua_nil ) {
        debugmsg( "Lua sidebar widget '%s' has no draw callback.", opts.id );
        return;
    }
    if( opts.height <= 0 && opts.height != -2 ) {
        debugmsg( "Lua sidebar widget '%s' has non-positive height.", opts.id );
        return;
    }

    auto entry_name = normalize_name( opts.id, opts.name );
    auto new_entry = widget_entry{
        .id = opts.id,
        .name = std::move( entry_name ),
        .height = opts.height,
        .order = opts.order,
        .default_toggle = opts.default_toggle,
        .redraw_every_frame = opts.redraw_every_frame,
        .panel_visible_value = opts.panel_visible_value,
        .panel_visible_fn = opts.panel_visible_fn,
        .draw = opts.draw,
        .render = opts.render,
    };

    auto &widgets = widgets_storage();
    auto existing = std::ranges::find( widgets, opts.id, &widget_entry::id );
    if( existing != widgets.end() ) {
        *existing = std::move( new_entry );
        return;
    }

    widgets.push_back( std::move( new_entry ) );
}

auto clear_widgets() -> void
{
    widgets_storage().clear();
}

auto get_widgets() -> const std::vector<widget_entry> & // *NOPAD*
{
    return widgets_storage();
}

auto find_widget( const std::string_view id ) -> const widget_entry * // *NOPAD*
{
    auto &widgets = widgets_storage();
    auto match = std::ranges::find( widgets, id, &widget_entry::id );
    if( match == widgets.end() ) {
        return nullptr;
    }
    return &*match;
}
} // namespace cata::lua_sidebar_widgets
