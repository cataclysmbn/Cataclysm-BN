#pragma once

#include <string>
#include <vector>

#include "procgen/proc_types.h"
#include "type_id.h"

class JsonObject;

namespace proc
{

struct hist_data {
    hist def = hist::none;
    std::vector<hist> ok;

    auto load( const JsonObject &jo ) -> void;
    auto allows( hist value ) const -> bool;
};

struct lua_data {
    std::string full;
    std::string name;
    std::string make;
    std::string validate;

    auto load( const JsonObject &jo ) -> void;
    auto empty() const -> bool {
        return full.empty() && name.empty() && make.empty() && validate.empty();
    }
};

struct slot_data {
    slot_id id = slot_id::NULL_ID();
    std::string role;
    int min = 0;
    int max = 0;
    bool rep = false;
    std::vector<std::string> ok;
    std::vector<std::string> no;

    auto load( const JsonObject &jo ) -> void;
};

struct schema {
    schema_id id = schema_id::NULL_ID();
    std::string cat;
    itype_id res = itype_id::NULL_ID();
    hist_data hist;
    std::vector<slot_data> slots;
    lua_data lua;
    bool was_loaded = false;

    auto load( const JsonObject &jo, const std::string &src ) -> void;
    auto check() const -> void;
};

auto load( const JsonObject &jo, const std::string &src ) -> void;
auto check() -> void;
auto reset() -> void;
auto all() -> const std::vector<schema>&; // *NOPAD*
auto get( const schema_id &id ) -> const schema&; // *NOPAD*
auto has( const schema_id &id ) -> bool;

} // namespace proc
