#include "catch/catch.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "filesystem.h"
#include "input.h"
#include "path_info.h"

namespace
{

class user_keybindings_file_guard
{
    public:
        ~user_keybindings_file_guard() {
            remove_file( PATH_INFO::user_keybindings() );
            inp_mngr.init();
        }
};

auto write_user_keybindings( const std::string &entry ) -> void
{
    auto file = std::ofstream( PATH_INFO::user_keybindings(), std::ios::binary );
    REQUIRE( file.good() );
    file << "[\n" << entry << "\n]\n";
}

auto read_user_keybindings() -> std::string
{
    auto file = std::ifstream( PATH_INFO::user_keybindings(), std::ios::binary );
    REQUIRE( file.good() );
    return { std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() };
}

auto npc_trade_right_keys() -> std::vector<char>
{
    auto ctxt = input_context( "NPC_TRADE" );
    return ctxt.keys_bound_to( "RIGHT" );
}

auto contains_key( const std::vector<char> &keys, const char key ) -> bool
{
    return std::ranges::find( keys, key ) != keys.end();
}

} // namespace

TEST_CASE( "deleted local keybindings fall back to global bindings", "[input][keybindings]" )
{
    auto restore_keybindings = user_keybindings_file_guard();

    write_user_keybindings( R"({
  "type": "keybinding",
  "id": "RIGHT",
  "category": "NPC_TRADE",
  "bindings": []
})" );
    inp_mngr.init();

    auto keys = npc_trade_right_keys();
    CHECK( keys.empty() );

    write_user_keybindings( R"({
  "type": "keybinding",
  "id": "RIGHT",
  "category": "NPC_TRADE",
  "is_deleted": true,
  "bindings": []
})" );
    inp_mngr.init();

    keys = npc_trade_right_keys();
    CHECK( contains_key( keys, 'l' ) );
    CHECK( contains_key( keys, '6' ) );

    inp_mngr.save();
    CHECK( read_user_keybindings().find( "\"is_deleted\": true" ) != std::string::npos );
}
