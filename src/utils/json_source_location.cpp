#include "json_source_location.h"

#include "debug.h"
#include "init.h"

void throw_error_at_json_loc(const json_source_location& loc, const std::string& message) {
    if (!loc.path) { throw JsonError(string_format("Json error: (unknown pos): %s", message)); }
    shared_ptr_fast<std::istream> stream = DynamicDataLoader::get_instance().get_cached_stream(
        *loc.path);
    if (!stream) {
        throw JsonError(string_format("Json error: (%s:%d): %s", *loc.path, loc.offset, message));
    }
    // Pass a new location with offset 0 so JsonIn stream would begin from the beginning of the file
    // and not from some old cached position.
    JsonIn jsin(*stream, json_source_location{loc.path, 0});
    jsin.error(message, loc.offset);
}

void show_warning_at_json_loc(const json_source_location& loc, const std::string& message) {
    try {
        throw_error_at_json_loc(loc, message);
    } catch (const JsonError& err) { debugmsg("%s", err.what()); }
}
