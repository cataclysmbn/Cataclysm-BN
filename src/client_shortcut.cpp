#include "client_shortcut.h"

#include <algorithm>

#include "json.h"

namespace
{
auto loaded() -> std::vector<client_shortcut> & // *NOPAD*
{
    static auto shortcuts = std::vector<client_shortcut> {};
    return shortcuts;
}
} // namespace

namespace client_shortcuts
{
auto load( const JsonObject &jo, const std::string & /*src*/ ) -> void
{
    add( {
        .id = jo.get_string( "id" ),
        .action = jo.get_string( "action" ),
        .icon = jo.get_string( "icon" ),
        .glyph = jo.get_string( "glyph", "" ),
        .order = jo.get_int( "order", 0 ),
    } );
}
auto add( client_shortcut entry ) -> void
{
    auto &shortcuts = loaded();
    const auto same = std::ranges::find( shortcuts, entry.id, &client_shortcut::id );
    if( same != shortcuts.end() ) {
        *same = std::move( entry );
    } else {
        shortcuts.push_back( std::move( entry ) );
    }
    std::ranges::stable_sort( shortcuts, std::ranges::less{}, &client_shortcut::order );
}
auto reset() -> void { loaded().clear(); }
auto all() -> const std::vector<client_shortcut> & { return loaded(); } // *NOPAD*
} // namespace client_shortcuts
