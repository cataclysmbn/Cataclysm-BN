#pragma once

#include <string>

#include "memory_fast.h"

struct json_source_location {
    shared_ptr_fast<std::string> path;
    int offset = 0;
};

/** Throw error at given JSON location. */
[[noreturn]]
void throw_error_at_json_loc( const json_source_location &loc, const std::string &message );

/** Show warning at given JSON location. */
void show_warning_at_json_loc( const json_source_location &loc, const std::string &message );


