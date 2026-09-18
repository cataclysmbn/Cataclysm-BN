#pragma once

#include "catacharset.h"
#include "color.h"
#include "coordinates.h"
#include "string_id.h"
#include "translations.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

class JsonObject;
class mapgen_constructor;
template <typename T> struct enum_traits;
template <typename T> class generic_factory;

enum class map_extra_method : int {
    null = 0,
    map_extra_function,
    mapgen,
    update_mapgen,
    num_map_extra_methods
};

template <> struct enum_traits<map_extra_method> {
    static constexpr map_extra_method last = map_extra_method::num_map_extra_methods;
};

using map_extra_pointer = bool (*)(mapgen_constructor&, const tripoint_abs_omt&);

class map_extra {
public:
    string_id<map_extra> id = string_id<map_extra>::NULL_ID();
    std::string generator_id;
    map_extra_method generator_method = map_extra_method::null;
    bool autonote = false;
    uint32_t symbol = UTF8_getch("X");
    nc_color color = c_red;
    std::optional<std::string> looks_like;

    auto get_symbol() const -> std::string { return utf32_to_utf8(symbol); }
    auto name() const -> std::string { return _name.translated(); }
    auto description() const -> std::string { return _description.translated(); }

    // Used by generic_factory
    bool was_loaded = false;
    void load(const JsonObject& jo, const std::string& src);
    void check() const;

private:
    translation _name;
    translation _description;
};

namespace MapExtras {
using FunctionMap = std::unordered_map<std::string, map_extra_pointer>;

map_extra_pointer get_function(const std::string& name);
auto all_functions() -> FunctionMap;
auto get_all_function_names() -> std::vector<std::string>;

void apply_function(
    const string_id<map_extra>& id, mapgen_constructor& m, const tripoint_abs_omt& abs_sub);
void apply_function(
    const std::string& id, mapgen_constructor& m, const tripoint_abs_omt& abs_offset);

void load(const JsonObject& jo, const std::string& src);
void check_consistency();
void reset();

void debug_spawn_test();

/// This function provides access to all loaded map extras.
auto mapExtraFactory() -> const generic_factory<map_extra>&;

} // namespace MapExtras
