#pragma once

#include <map>
#include <string>

class JsonObject;

extern std::map<std::string, int> base_mutation_overlay_ordering;
extern std::map<std::string, int> tileset_mutation_overlay_ordering;

auto load_overlay_ordering( const JsonObject &jsobj ) -> void;
auto load_overlay_ordering_into_array( const JsonObject &jsobj,
                                       std::map<std::string, int> &orderarray ) -> void;
auto get_overlay_order( const std::string &overlay_id_string ) -> int;
auto reset_overlay_ordering() -> void;

