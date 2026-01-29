#include "data_reader.h"

#include "debug.h"
#include "init.h"
#include "json.h"
#include "string_formatter.h"

std::string data_source_location::to_string() const
{
    if( path ) {
        if( is_lua ) {
            return string_format( "%s:%d", *path, line );
        } else {
            return string_format( "%s (offset %d)", *path, offset );
        }
    }
    return is_lua ? "<lua>" : "<unknown>";
}

void throw_error_at_data_loc( const data_source_location &loc, const std::string &message )
{
    if( loc.is_lua ) {
        // For Lua sources, throw with file:line info
        throw JsonError( string_format( "%s: %s", loc.to_string(), message ) );
    } else {
        // For JSON sources, delegate to existing JSON error handling
        json_source_location json_loc;
        json_loc.path = loc.path;
        json_loc.offset = loc.offset;
        throw_error_at_json_loc( json_loc, message );
    }
}

void show_warning_at_data_loc( const data_source_location &loc, const std::string &message )
{
    if( loc.is_lua ) {
        // For Lua sources, show warning with file:line info
        debugmsg( "%s: %s", loc.to_string(), message );
    } else {
        // For JSON sources, delegate to existing JSON warning handling
        json_source_location json_loc;
        json_loc.path = loc.path;
        json_loc.offset = loc.offset;
        show_warning_at_json_loc( json_loc, message );
    }
}
