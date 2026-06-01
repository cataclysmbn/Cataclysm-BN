#include "lua_mod_options.h"

#include "debug.h"
#include "options.h"
#include "string_formatter.h"

namespace cata
{

/// Page ID for Lua mod settings.
static constexpr auto mod_settings_page = "mod_settings";

auto get_lua_option_id( const std::string &mod_ident, const std::string &option_id ) -> std::string
{
    return string_format( "%s.%s", mod_ident, option_id );
}

auto register_lua_option( const std::string &mod_ident, const lua_option_spec &spec ) -> bool
{
    if( mod_ident.empty() ) {
        debugmsg( "Cannot register Lua option with empty mod ident" );
        return false;
    }
    if( spec.id.empty() ) {
        debugmsg( "Cannot register Lua option with empty id" );
        return false;
    }

    auto &opts = get_options();
    const auto full_id = get_lua_option_id( mod_ident, spec.id );

    if( opts.has_option( full_id ) ) {
        debugmsg( "Lua option '%s' already registered", full_id );
        return false;
    }

    const auto name = spec.name.translated();
    const auto tooltip = spec.tooltip.translated();

    switch( spec.type ) {
        case lua_option_type::boolean:
            opts.add( full_id, mod_settings_page, name, tooltip, spec.default_bool );
            break;

        case lua_option_type::integer:
            opts.add( full_id, mod_settings_page, name, tooltip,
                      spec.min_int, spec.max_int, spec.default_int );
            break;

        case lua_option_type::floating:
            opts.add( full_id, mod_settings_page, name, tooltip,
                      spec.min_float, spec.max_float, spec.default_float, spec.step_float );
            break;

        case lua_option_type::string_select: {
            std::vector<options_manager::id_and_option> items;
            items.reserve( spec.choices.size() );
            for( const auto &choice : spec.choices ) {
                items.emplace_back( choice.id, choice.name );
            }
            opts.add( full_id, mod_settings_page, name, tooltip, items, spec.default_string );
            break;
        }

        case lua_option_type::string_input:
            opts.add( full_id, mod_settings_page, name, tooltip, spec.default_string, spec.max_length );
            break;
    }

    return true;
}

auto clear_lua_options() -> void
{
    get_options().clear_lua_options();
}

} // namespace cata
