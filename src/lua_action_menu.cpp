#include "lua_action_menu.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "catalua_impl.h"
#include "debug.h"
#include "input.h"

namespace cata::lua_action_menu
{
namespace
{
auto entries_storage() -> std::vector<entry> & // *NOPAD*
{
    static auto entries = std::vector<entry> {};
    return entries;
}

auto normalize_category( const std::string &category_id ) -> std::string
{
    return category_id.empty() ? std::string{ "misc" } :
           category_id;
}

auto parse_hotkey( const std::optional<std::string> &hotkey ) -> int
{
    if( !hotkey || hotkey->empty() ) {
        return -1;
    }

    const auto keycode = inp_mngr.get_keycode( *hotkey );
    if( keycode == 0 ) {
        debugmsg( "Lua action menu hotkey '%s' is not a known key name.", *hotkey );
        return -1;
    }

    return keycode;
}
} // namespace

auto register_entry( const entry_options &opts ) -> void
{
    if( opts.id.empty() ) {
        debugmsg( "Lua action menu entry id must not be empty." );
        return;
    }
    if( opts.callback == sol::lua_nil ) {
        debugmsg( "Lua action menu entry '%s' has no callback.", opts.id );
        return;
    }

    auto entry_name = opts.name.empty() ? opts.id : opts.name;
    auto new_entry = entry{
        .id = opts.id,
        .name = std::move( entry_name ),
        .category_id = normalize_category( opts.category_id ),
        .hotkey = parse_hotkey( opts.hotkey ),
        .callback = opts.callback,
    };

    auto &entries = entries_storage();
    auto existing = std::ranges::find( entries, opts.id, &entry::id );
    if( existing != entries.end() ) {
        *existing = std::move( new_entry );
        return;
    }
    entries.push_back( std::move( new_entry ) );
}

auto clear_entries() -> void
{
    entries_storage().clear();
}

auto get_entries() -> const std::vector<entry> & // *NOPAD*
{
    return entries_storage();
}

auto run_entry( const std::string &id ) -> bool
{
    auto &entries = entries_storage();
    auto match = std::ranges::find( entries, id, &entry::id );
    if( match == entries.end() ) {
        debugmsg( "Lua action menu entry '%s' not found.", id );
        return false;
    }

    try {
        auto res = match->callback();
        check_func_result( res );
    } catch( const std::runtime_error &err ) {
        debugmsg( "Failed to run lua action menu entry '%s': %s", id, err.what() );
        return false;
    }

    return true;
}
} // namespace cata::lua_action_menu
