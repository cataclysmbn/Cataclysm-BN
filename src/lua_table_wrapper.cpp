#include "catalua_sol.h"

#include "lua_table_wrapper.h"

#include <sstream>

#include "calendar.h"
#include "catalua_impl.h"
#include "debug.h"
#include "json.h"
#include "string_formatter.h"
#include "string_id.h"
#include "units.h"
#include "units_serde.h"

// Static assert to verify LuaTableWrapper satisfies DataReader concept
static_assert( DataReader<LuaTableWrapper>, "LuaTableWrapper must satisfy DataReader concept" );

// ============================================================================
// LuaTableWrapper implementation
// ============================================================================

LuaTableWrapper::LuaTableWrapper() = default;

LuaTableWrapper::LuaTableWrapper( const sol::table &tbl )
    : table_( tbl )
{
    capture_source_location();
}

LuaTableWrapper::LuaTableWrapper( const sol::table &tbl, const data_source_location &loc )
    : table_( tbl )
    , source_loc_( loc )
{
}

LuaTableWrapper::~LuaTableWrapper()
{
    // Report unvisited members if enabled (for debugging)
    if( report_unvisited_ && is_valid() ) {
        for( auto &pair : table_ ) {
            if( pair.first.is<std::string>() ) {
                std::string key = pair.first.as<std::string>();
                if( visited_members_.find( key ) == visited_members_.end() ) {
                    // Don't warn for common metadata keys
                    if( key != "copy_from" && key != "abstract" && key != "type" ) {
                        DebugLog( DL::Warn, DC::Lua ) << "Lua table has unvisited member: " << key
                                                      << " at " << line_number();
                    }
                }
            }
        }
    }
}

void LuaTableWrapper::mark_visited( const std::string &name ) const
{
    visited_members_.insert( name );
}

void LuaTableWrapper::capture_source_location()
{
    if( !table_.valid() ) {
        return;
    }

    sol::state_view lua = table_.lua_state();
    lua_State *L = lua.lua_state();

    lua_Debug ar;
    // Walk up the stack to find Lua code (not C++ bindings)
    for( int level = 1; level < 20; ++level ) {
        if( lua_getstack( L, level, &ar ) ) {
            lua_getinfo( L, "Sln", &ar );
            if( ar.source && ar.source[0] == '@' ) {
                // File source (starts with @)
                source_loc_.path = make_shared_fast<std::string>( ar.source + 1 ); // Skip the @
                source_loc_.line = ar.currentline;
                source_loc_.is_lua = true;
                return;
            }
        } else {
            break;
        }
    }

    // Fallback: no source location available
    source_loc_.path = make_shared_fast<std::string>( "<lua>" );
    source_loc_.line = 0;
    source_loc_.is_lua = true;
}

std::string LuaTableWrapper::line_number() const
{
    return source_loc_.to_string();
}

bool LuaTableWrapper::is_valid() const
{
    return table_.valid();
}

const sol::table &LuaTableWrapper::raw_table() const
{
    return table_;
}

// ============================================================================
// Member existence checking
// ============================================================================

bool LuaTableWrapper::has_member( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    return obj.valid() && !obj.is<sol::lua_nil_t>();
}

bool LuaTableWrapper::has_null( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    return obj.is<sol::lua_nil_t>();
}

bool LuaTableWrapper::has_bool( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    return obj.valid() && obj.is<bool>();
}

bool LuaTableWrapper::has_number( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    return obj.valid() && ( obj.is<int>() || obj.is<double>() );
}

bool LuaTableWrapper::has_string( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    return obj.valid() && obj.is<std::string>();
}

bool LuaTableWrapper::has_array( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    if( !obj.valid() || !obj.is<sol::table>() ) {
        return false;
    }
    // Check if it's array-like (sequential integer keys starting at 1)
    sol::table tbl = obj.as<sol::table>();
    size_t expected = 1;
    for( auto &pair : tbl ) {
        if( !pair.first.is<int>() || pair.first.as<int>() != static_cast<int>( expected ) ) {
            return false;
        }
        expected++;
    }
    return expected > 1; // At least one element
}

bool LuaTableWrapper::has_object( const std::string &name ) const
{
    if( !is_valid() ) {
        return false;
    }
    sol::object obj = table_[name];
    if( !obj.valid() || !obj.is<sol::table>() ) {
        return false;
    }
    // An object has non-sequential or string keys
    sol::table tbl = obj.as<sol::table>();
    for( auto &pair : tbl ) {
        if( pair.first.is<std::string>() ) {
            return true;
        }
    }
    // Could also be an empty table or array - treat as object if not array-like
    return !has_array( name );
}

// ============================================================================
// Value reading (throwing versions)
// ============================================================================

bool LuaTableWrapper::get_bool( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() || !obj.is<bool>() ) {
        throw_error( "expected boolean", name );
    }
    return obj.as<bool>();
}

int LuaTableWrapper::get_int( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() ) {
        throw_error( "expected integer", name );
    }
    if( obj.is<int>() ) {
        return obj.as<int>();
    }
    if( obj.is<double>() ) {
        return static_cast<int>( obj.as<double>() );
    }
    throw_error( "expected integer", name );
}

double LuaTableWrapper::get_float( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() ) {
        throw_error( "expected number", name );
    }
    if( obj.is<double>() ) {
        return obj.as<double>();
    }
    if( obj.is<int>() ) {
        return static_cast<double>( obj.as<int>() );
    }
    throw_error( "expected number", name );
}

std::string LuaTableWrapper::get_string( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() || !obj.is<std::string>() ) {
        throw_error( "expected string", name );
    }
    return obj.as<std::string>();
}

LuaArrayWrapper LuaTableWrapper::get_array( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() ) {
        // Return empty array if not present (matches JsonObject behavior)
        return LuaArrayWrapper();
    }
    if( !obj.is<sol::table>() ) {
        throw_error( "expected array", name );
    }
    return LuaArrayWrapper( obj.as<sol::table>(), source_loc_ );
}

LuaTableWrapper LuaTableWrapper::get_object( const std::string &name ) const
{
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.valid() ) {
        // Return empty wrapper if not present (matches JsonObject behavior)
        return LuaTableWrapper();
    }
    if( !obj.is<sol::table>() ) {
        throw_error( "expected object", name );
    }
    return LuaTableWrapper( obj.as<sol::table>(), source_loc_ );
}

// ============================================================================
// Value reading (with defaults)
// ============================================================================

bool LuaTableWrapper::get_bool( const std::string &name, bool fallback ) const
{
    if( !has_member( name ) ) {
        return fallback;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<bool>() ) {
        return obj.as<bool>();
    }
    return fallback;
}

int LuaTableWrapper::get_int( const std::string &name, int fallback ) const
{
    if( !has_member( name ) ) {
        return fallback;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<int>() ) {
        return obj.as<int>();
    }
    if( obj.is<double>() ) {
        return static_cast<int>( obj.as<double>() );
    }
    return fallback;
}

double LuaTableWrapper::get_float( const std::string &name, double fallback ) const
{
    if( !has_member( name ) ) {
        return fallback;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<double>() ) {
        return obj.as<double>();
    }
    if( obj.is<int>() ) {
        return static_cast<double>( obj.as<int>() );
    }
    return fallback;
}

std::string LuaTableWrapper::get_string( const std::string &name,
        const std::string &fallback ) const
{
    if( !has_member( name ) ) {
        return fallback;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<std::string>() ) {
        return obj.as<std::string>();
    }
    return fallback;
}

// ============================================================================
// Array helpers
// ============================================================================

std::vector<int> LuaTableWrapper::get_int_array( const std::string &name ) const
{
    std::vector<int> result;
    if( !has_member( name ) ) {
        return result;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.is<sol::table>() ) {
        return result;
    }
    sol::table tbl = obj.as<sol::table>();
    for( auto &pair : tbl ) {
        if( pair.second.is<int>() ) {
            result.push_back( pair.second.as<int>() );
        } else if( pair.second.is<double>() ) {
            result.push_back( static_cast<int>( pair.second.as<double>() ) );
        }
    }
    return result;
}

std::vector<std::string> LuaTableWrapper::get_string_array( const std::string &name ) const
{
    std::vector<std::string> result;
    if( !has_member( name ) ) {
        return result;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.is<sol::table>() ) {
        return result;
    }
    sol::table tbl = obj.as<sol::table>();
    for( auto &pair : tbl ) {
        if( pair.second.is<std::string>() ) {
            result.push_back( pair.second.as<std::string>() );
        }
    }
    return result;
}

// ============================================================================
// Tags helper (template implementation in header or explicit instantiations)
// ============================================================================

template<>
std::set<std::string> LuaTableWrapper::get_tags<std::string, std::set<std::string>>(
    const std::string &name ) const
{
    std::set<std::string> result;
    if( !has_member( name ) ) {
        return result;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( !obj.is<sol::table>() ) {
        return result;
    }
    sol::table tbl = obj.as<sol::table>();
    for( auto &pair : tbl ) {
        if( pair.second.is<std::string>() ) {
            result.insert( pair.second.as<std::string>() );
        }
    }
    return result;
}

// ============================================================================
// Templated read() implementation
// ============================================================================

template<>
bool LuaTableWrapper::read( const std::string &name, bool &value, bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<bool>() ) {
        value = obj.as<bool>();
        return true;
    }
    if( throw_on_error ) {
        throw_error( "expected boolean", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, int &value, bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<int>() ) {
        value = obj.as<int>();
        return true;
    }
    if( obj.is<double>() ) {
        value = static_cast<int>( obj.as<double>() );
        return true;
    }
    if( throw_on_error ) {
        throw_error( "expected integer", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, double &value, bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<double>() ) {
        value = obj.as<double>();
        return true;
    }
    if( obj.is<int>() ) {
        value = static_cast<double>( obj.as<int>() );
        return true;
    }
    if( throw_on_error ) {
        throw_error( "expected number", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, float &value, bool throw_on_error ) const
{
    double d;
    if( read( name, d, throw_on_error ) ) {
        value = static_cast<float>( d );
        return true;
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, std::string &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];
    if( obj.is<std::string>() ) {
        value = obj.as<std::string>();
        return true;
    }
    if( throw_on_error ) {
        throw_error( "expected string", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, time_duration &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    // Accept native TimeDuration userdata
    if( obj.is<time_duration>() ) {
        value = obj.as<time_duration>();
        return true;
    }

    // Accept JSON-style string ("1 hour", "30 minutes")
    if( obj.is<std::string>() ) {
        std::istringstream iss( obj.as<std::string>() );
        JsonIn jsin( iss );
        try {
            value = read_from_json_string<time_duration>( jsin, time_duration::units );
            return true;
        } catch( const JsonError & ) {
            if( throw_on_error ) {
                throw_error( "invalid duration string", name );
            }
            return false;
        }
    }

    // Accept integer as turns (backward compatibility)
    if( obj.is<int>() ) {
        value = time_duration::from_turns( obj.as<int>() );
        return true;
    }

    if( throw_on_error ) {
        throw_error( "expected duration (TimeDuration, string, or integer)", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, units::energy &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    // Accept native Energy userdata
    if( obj.is<units::energy>() ) {
        value = obj.as<units::energy>();
        return true;
    }

    // Accept JSON-style string ("1 kJ", "500 J")
    if( obj.is<std::string>() ) {
        std::istringstream iss( obj.as<std::string>() );
        JsonIn jsin( iss );
        try {
            value = read_from_json_string<units::energy>( jsin, units::energy_units );
            return true;
        } catch( const JsonError & ) {
            if( throw_on_error ) {
                throw_error( "invalid energy string", name );
            }
            return false;
        }
    }

    if( throw_on_error ) {
        throw_error( "expected energy (Energy userdata or string)", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, units::mass &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    // Accept native Mass userdata
    if( obj.is<units::mass>() ) {
        value = obj.as<units::mass>();
        return true;
    }

    // Accept JSON-style string ("1 kg", "500 g")
    if( obj.is<std::string>() ) {
        std::istringstream iss( obj.as<std::string>() );
        JsonIn jsin( iss );
        try {
            value = read_from_json_string<units::mass>( jsin, units::mass_units );
            return true;
        } catch( const JsonError & ) {
            if( throw_on_error ) {
                throw_error( "invalid mass string", name );
            }
            return false;
        }
    }

    if( throw_on_error ) {
        throw_error( "expected mass (Mass userdata or string)", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, translation &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    // Simple string -> translatable string
    if( obj.is<std::string>() ) {
        value = translation::to_translation( obj.as<std::string>() );
        return true;
    }

    // Table format for translation (matches JSON format):
    // { str = "text" } or { str = "text", ctxt = "context" }
    // or { str_sp = "same for singular/plural" }
    // or { str = "singular", str_pl = "plural" }
    if( obj.is<sol::table>() ) {
        sol::table tbl = obj.as<sol::table>();

        sol::optional<std::string> str = tbl["str"];
        sol::optional<std::string> str_sp = tbl["str_sp"];
        sol::optional<std::string> str_pl = tbl["str_pl"];
        sol::optional<std::string> ctxt = tbl["ctxt"];

        if( str_sp ) {
            value = translation::to_translation( *str_sp );
        } else if( str && str_pl ) {
            value = translation::pl_translation( *str, *str_pl );
        } else if( str ) {
            if( ctxt ) {
                value = translation::to_translation( *ctxt, *str );
            } else {
                value = translation::to_translation( *str );
            }
        } else {
            if( throw_on_error ) {
                throw_error( "translation table must have 'str' or 'str_sp'", name );
            }
            return false;
        }
        return true;
    }

    if( throw_on_error ) {
        throw_error( "expected string or translation table", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, std::vector<std::string> &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    if( obj.is<sol::table>() ) {
        sol::table tbl = obj.as<sol::table>();
        value.clear();
        for( auto &pair : tbl ) {
            if( pair.second.is<std::string>() ) {
                value.push_back( pair.second.as<std::string>() );
            }
        }
        return true;
    }

    // Single string -> single-element vector
    if( obj.is<std::string>() ) {
        value.clear();
        value.push_back( obj.as<std::string>() );
        return true;
    }

    if( throw_on_error ) {
        throw_error( "expected string array", name );
    }
    return false;
}

template<>
bool LuaTableWrapper::read( const std::string &name, std::vector<translation> &value,
                            bool throw_on_error ) const
{
    if( !has_member( name ) ) {
        return false;
    }
    mark_visited( name );
    sol::object obj = table_[name];

    if( obj.is<sol::table>() ) {
        sol::table tbl = obj.as<sol::table>();
        value.clear();
        for( auto &pair : tbl ) {
            translation t;
            if( pair.second.is<std::string>() ) {
                t = translation::to_translation( pair.second.as<std::string>() );
                value.push_back( t );
            } else if( pair.second.is<sol::table>() ) {
                // Create a temporary wrapper to use the translation read logic
                sol::table entry = pair.second.as<sol::table>();
                sol::optional<std::string> str = entry["str"];
                sol::optional<std::string> str_sp = entry["str_sp"];
                sol::optional<std::string> str_pl = entry["str_pl"];
                sol::optional<std::string> ctxt = entry["ctxt"];

                if( str_sp ) {
                    t = translation::to_translation( *str_sp );
                } else if( str && str_pl ) {
                    t = translation::pl_translation( *str, *str_pl );
                } else if( str ) {
                    if( ctxt ) {
                        t = translation::to_translation( *ctxt, *str );
                    } else {
                        t = translation::to_translation( *str );
                    }
                }
                value.push_back( t );
            }
        }
        return true;
    }

    if( throw_on_error ) {
        throw_error( "expected translation array", name );
    }
    return false;
}

// ============================================================================
// Error handling
// ============================================================================

void LuaTableWrapper::throw_error( const std::string &err ) const
{
    throw_error_at_data_loc( source_loc_, err );
}

void LuaTableWrapper::throw_error( const std::string &err, const std::string &name ) const
{
    throw_error_at_data_loc( source_loc_, string_format( "%s (member '%s')", err, name ) );
}

void LuaTableWrapper::show_warning( const std::string &err ) const
{
    show_warning_at_data_loc( source_loc_, err );
}

void LuaTableWrapper::show_warning( const std::string &err, const std::string &name ) const
{
    show_warning_at_data_loc( source_loc_, string_format( "%s (member '%s')", err, name ) );
}

// ============================================================================
// JSON serialization for data dumper
// ============================================================================

// Declaration is in lua_table_wrapper.h

void LuaTableWrapper::write_json( JsonOut &jsout ) const
{
    if( !is_valid() ) {
        jsout.write_null();
        return;
    }

    jsout.start_object();

    for( auto &pair : table_ ) {
        std::string key;
        if( pair.first.is<std::string>() ) {
            key = pair.first.as<std::string>();
        } else if( pair.first.is<int>() ) {
            key = std::to_string( pair.first.as<int>() );
        } else {
            continue; // Skip non-string/int keys
        }

        jsout.member( key );
        write_lua_value_as_json( jsout, pair.second );
    }

    jsout.end_object();
}

std::string LuaTableWrapper::to_json_string() const
{
    std::ostringstream ss;
    JsonOut jsout( ss, false ); // Not pretty-printed
    write_json( jsout );
    return ss.str();
}

void write_lua_value_as_json( JsonOut &jsout, const sol::object &val )
{
    if( val.is<sol::lua_nil_t>() || !val.valid() ) {
        jsout.write_null();
    } else if( val.is<bool>() ) {
        jsout.write( val.as<bool>() );
    } else if( val.is<int>() ) {
        jsout.write( val.as<int>() );
    } else if( val.is<double>() ) {
        jsout.write( val.as<double>() );
    } else if( val.is<std::string>() ) {
        jsout.write( val.as<std::string>() );
    } else if( val.is<sol::table>() ) {
        sol::table tbl = val.as<sol::table>();

        // Detect if it's an array (sequential integer keys starting at 1)
        bool is_array = true;
        size_t expected = 1;
        for( auto &p : tbl ) {
            if( !p.first.is<int>() || p.first.as<int>() != static_cast<int>( expected ) ) {
                is_array = false;
                break;
            }
            expected++;
        }

        if( is_array && expected > 1 ) {
            jsout.start_array();
            for( auto &p : tbl ) {
                write_lua_value_as_json( jsout, p.second );
            }
            jsout.end_array();
        } else {
            LuaTableWrapper( tbl ).write_json( jsout );
        }
    } else {
        // Handle userdata types
        std::optional<std::string> luna_type = get_luna_type( val );
        if( luna_type ) {
            if( *luna_type == "time_duration" ) {
                jsout.write( to_string( val.as<time_duration>() ) );
            } else if( *luna_type == "units::energy" || *luna_type == "Energy" ) {
                auto e_val = val.as<units::energy>();
                auto i_val = units::to_kilojoule(e_val);
                if ( ( i_val * 10 ) % 1 == 0 ) {
                    jsout.write( _( "%s kJ", i_val ) );
                }
                else {
                    jsout.write( _( "%s J", units::to_joule( e_val ) ) );
                }
            } else if( *luna_type == "units::mass" || *luna_type == "Mass" ) {
                units::mass m_val = val.as<units::mass>();
                auto i_val = units::to_kilogram(m_val);
                if ( ( i_val * 10 ) % 1 == 0 ) {
                    jsout.write( _( "%s kg", i_val ) );
                }
                else {
                    i_val = units::to_gram(m_val);
                    if ( ( i_val * 10 ) % 1 == 0 ) {
                        jsout.write( _( "%s g", i_val ) );
                    }
                    else {
                        jsout.write( _( "%s mg", units::to_milligram( m_val ) ) );
                    }
                }
            } else {
                // For unknown userdata, output type info
                jsout.write( "<" + *luna_type + ">" );
            }
        } else {
            jsout.write_null();
        }
    }
}

// ============================================================================
// LuaArrayWrapper implementation
// ============================================================================

LuaArrayWrapper::LuaArrayWrapper() = default;

LuaArrayWrapper::LuaArrayWrapper( const sol::table &tbl )
    : table_( tbl )
{
    // Count elements (Lua arrays are 1-indexed)
    size_ = 0;
    for( auto &pair : tbl ) {
        if( pair.first.is<int>() && pair.first.as<int>() > 0 ) {
            size_ = std::max( size_, static_cast<size_t>( pair.first.as<int>() ) );
        }
    }
}

LuaArrayWrapper::LuaArrayWrapper( const sol::table &tbl, const data_source_location &loc )
    : LuaArrayWrapper( tbl )
{
    source_loc_ = loc;
}

// ============================================================================
// Iterative access
// ============================================================================

bool LuaArrayWrapper::next_bool()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )]; // Lua is 1-indexed
    index_++;
    if( !obj.is<bool>() ) {
        throw_error( "expected boolean", index_ - 1 );
    }
    return obj.as<bool>();
}

int LuaArrayWrapper::next_int()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    index_++;
    if( obj.is<int>() ) {
        return obj.as<int>();
    }
    if( obj.is<double>() ) {
        return static_cast<int>( obj.as<double>() );
    }
    throw_error( "expected integer", index_ - 1 );
}

double LuaArrayWrapper::next_float()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    index_++;
    if( obj.is<double>() ) {
        return obj.as<double>();
    }
    if( obj.is<int>() ) {
        return static_cast<double>( obj.as<int>() );
    }
    throw_error( "expected number", index_ - 1 );
}

std::string LuaArrayWrapper::next_string()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    index_++;
    if( !obj.is<std::string>() ) {
        throw_error( "expected string", index_ - 1 );
    }
    return obj.as<std::string>();
}

LuaArrayWrapper LuaArrayWrapper::next_array()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    index_++;
    if( !obj.is<sol::table>() ) {
        throw_error( "expected array", index_ - 1 );
    }
    return LuaArrayWrapper( obj.as<sol::table>(), source_loc_ );
}

LuaTableWrapper LuaArrayWrapper::next_object()
{
    if( index_ >= size_ ) {
        throw_error( "array index out of bounds" );
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    index_++;
    if( !obj.is<sol::table>() ) {
        throw_error( "expected object", index_ - 1 );
    }
    return LuaTableWrapper( obj.as<sol::table>(), source_loc_ );
}

void LuaArrayWrapper::skip_value()
{
    if( index_ < size_ ) {
        index_++;
    }
}

// ============================================================================
// Random access
// ============================================================================

bool LuaArrayWrapper::get_bool( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( !obj.is<bool>() ) {
        throw_error( "expected boolean", index );
    }
    return obj.as<bool>();
}

int LuaArrayWrapper::get_int( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( obj.is<int>() ) {
        return obj.as<int>();
    }
    if( obj.is<double>() ) {
        return static_cast<int>( obj.as<double>() );
    }
    throw_error( "expected integer", index );
}

double LuaArrayWrapper::get_float( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( obj.is<double>() ) {
        return obj.as<double>();
    }
    if( obj.is<int>() ) {
        return static_cast<double>( obj.as<int>() );
    }
    throw_error( "expected number", index );
}

std::string LuaArrayWrapper::get_string( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( !obj.is<std::string>() ) {
        throw_error( "expected string", index );
    }
    return obj.as<std::string>();
}

LuaArrayWrapper LuaArrayWrapper::get_array( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( !obj.is<sol::table>() ) {
        throw_error( "expected array", index );
    }
    return LuaArrayWrapper( obj.as<sol::table>(), source_loc_ );
}

LuaTableWrapper LuaArrayWrapper::get_object( size_t index ) const
{
    sol::object obj = table_[static_cast<int>( index + 1 )];
    if( !obj.is<sol::table>() ) {
        throw_error( "expected object", index );
    }
    return LuaTableWrapper( obj.as<sol::table>(), source_loc_ );
}

// ============================================================================
// Type checking
// ============================================================================

bool LuaArrayWrapper::test_bool() const
{
    if( index_ >= size_ ) {
        return false;
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    return obj.is<bool>();
}

bool LuaArrayWrapper::test_int() const
{
    if( index_ >= size_ ) {
        return false;
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    return obj.is<int>() || obj.is<double>();
}

bool LuaArrayWrapper::test_float() const
{
    return test_int(); // Same check for Lua
}

bool LuaArrayWrapper::test_string() const
{
    if( index_ >= size_ ) {
        return false;
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    return obj.is<std::string>();
}

bool LuaArrayWrapper::test_array() const
{
    if( index_ >= size_ ) {
        return false;
    }
    sol::object obj = table_[static_cast<int>( index_ + 1 )];
    return obj.is<sol::table>();
}

bool LuaArrayWrapper::test_object() const
{
    return test_array(); // Both are tables in Lua
}

// ============================================================================
// Error handling
// ============================================================================

void LuaArrayWrapper::throw_error( const std::string &err ) const
{
    throw_error_at_data_loc( source_loc_, err );
}

void LuaArrayWrapper::throw_error( const std::string &err, size_t index ) const
{
    throw_error_at_data_loc( source_loc_, string_format( "%s (at index %zu)", err, index ) );
}

// ============================================================================
// Iterator implementation
// ============================================================================

LuaArrayWrapper::const_iterator::const_iterator( const LuaArrayWrapper *parent, size_t index )
    : parent_( parent )
    , index_( index )
{
}

sol::object LuaArrayWrapper::const_iterator::operator*() const
{
    return parent_->table_[static_cast<int>( index_ + 1 )];
}

LuaArrayWrapper::const_iterator &LuaArrayWrapper::const_iterator::operator++()
{
    ++index_;
    return *this;
}

LuaArrayWrapper::const_iterator LuaArrayWrapper::const_iterator::operator++( int )
{
    const_iterator tmp = *this;
    ++index_;
    return tmp;
}

bool LuaArrayWrapper::const_iterator::operator==( const const_iterator &other ) const
{
    return parent_ == other.parent_ && index_ == other.index_;
}

bool LuaArrayWrapper::const_iterator::operator!=( const const_iterator &other ) const
{
    return !( *this == other );
}

LuaArrayWrapper::const_iterator LuaArrayWrapper::begin() const
{
    return const_iterator( this, 0 );
}

LuaArrayWrapper::const_iterator LuaArrayWrapper::end() const
{
    return const_iterator( this, size_ );
}
