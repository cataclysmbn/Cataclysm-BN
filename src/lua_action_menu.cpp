#include "lua_action_menu.h"

#include <algorithm>
#include <cstdlib>
#include <ranges>
#include <sstream>
#include <utility>

#include "catalua_impl.h"
#include "debug.h"
#include "fstream_utils.h"
#include "input.h"
#include "json.h"

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

auto normalize_description( const entry_options &opts ) -> std::string
{
    if( !opts.description.empty() ) {
        return opts.description;
    }

    if( !opts.name.empty() ) {
        return opts.name;
    }

    return opts.id;
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

auto hotkey_to_string( const int hotkey ) -> std::string
{
    if( hotkey < 0 ) {
        return "";
    }

    return inp_mngr.get_keyname( hotkey, input_event_t::keyboard, true );
}

auto get_macros_json_output_path() -> const char *
{
    const auto *output_path = std::getenv( "CATA_AVAILABLE_MACROS_JSON" );
    if( output_path == nullptr || output_path[0] == '\0' ) {
        return nullptr;
    }

    return output_path;
}

auto write_entries_json( std::ostream &output_stream ) -> void
{
    const auto &entries = entries_storage();
    auto jsout = JsonOut( output_stream, true );
    jsout.start_object();
    jsout.member( "macros" );
    jsout.start_array();
    for( const auto &entries_entry : entries ) {
        jsout.start_object();
        jsout.member( "id", entries_entry.id );
        jsout.member( "name", entries_entry.name );
        jsout.member( "description", entries_entry.description );
        jsout.member( "category", entries_entry.category_id );
        jsout.member( "hotkey", hotkey_to_string( entries_entry.hotkey ) );
        jsout.end_object();
    }
    jsout.end_array();
    jsout.end_object();
}

auto dump_entries_to_json() -> void
{
    const auto *output_path = get_macros_json_output_path();
    if( output_path == nullptr ) {
        return;
    }

    const auto write_succeeded = write_to_file( output_path, []( std::ostream & output_stream ) {
        write_entries_json( output_stream );
    }, "" );
    if( !write_succeeded ) {
        debugmsg( "Failed to write available Lua action menu macros JSON to '%s'.", output_path );
    }
}
} // namespace

auto register_entry( const entry_options &opts ) -> void
{
    if( opts.id.empty() ) {
        debugmsg( "Lua action menu entry id must not be empty." );
        return;
    }
    if( opts.fn == sol::lua_nil ) {
        debugmsg( "Lua action menu entry '%s' has no fn.", opts.id );
        return;
    }

    auto entry_name = opts.name.empty() ? opts.id : opts.name;
    auto new_entry = entry{
        .id = opts.id,
        .name = std::move( entry_name ),
        .description = normalize_description( opts ),
        .category_id = normalize_category( opts.category_id ),
        .hotkey = parse_hotkey( opts.hotkey ),
        .fn = opts.fn,
    };

    auto &entries = entries_storage();
    auto existing = std::ranges::find( entries, opts.id, &entry::id );
    if( existing != entries.end() ) {
        *existing = std::move( new_entry );
        dump_entries_to_json();
        return;
    }
    entries.push_back( std::move( new_entry ) );
    dump_entries_to_json();
}

auto clear_entries() -> void
{
    entries_storage().clear();
    dump_entries_to_json();
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
        auto res = match->fn();
        check_func_result( res );
    } catch( const std::runtime_error &err ) {
        debugmsg( "Failed to run lua action menu entry '%s': %s", id, err.what() );
        return false;
    }

    return true;
}

auto list_entries_json() -> std::string
{
    auto output_stream = std::ostringstream();
    write_entries_json( output_stream );
    return output_stream.str();
}
} // namespace cata::lua_action_menu
