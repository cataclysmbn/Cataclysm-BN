#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "proc_builder.h"
#include "recipe.h"

namespace cata
{
struct lua_state;
}

class item;
class JsonIn;
class JsonOut;

template<typename T>
class detached_ptr;

namespace proc
{

struct compact_part {
    std::string role;
    itype_id id = itype_id::NULL_ID();
    int n = 1;
    float hp = 1.0f;
    int dmg = 0;
    int chg = 0;
    std::vector<material_id> mat;
    std::string proc;

    auto operator==( const compact_part & ) const -> bool = default;
};

struct payload {
    schema_id id = schema_id::NULL_ID();
    hist mode = hist::none;
    std::string fp;
    fast_blob blob;
    std::vector<compact_part> parts;

    auto operator==( const payload & ) const -> bool = default;
};

struct lua_opts {
    cata::lua_state *state = nullptr;
};

struct make_opts {
    hist mode = hist::none;
    const recipe *rec = nullptr;
    std::vector<const item *> used;
    cata::lua_state *state = nullptr;
};

auto to_json( JsonOut &jsout, const compact_part &part ) -> void;
auto from_json( JsonIn &jsin, compact_part &part ) -> void;
auto to_json( JsonOut &jsout, const payload &data ) -> void;
auto from_json( JsonIn &jsin, payload &data ) -> void;
auto read_payload( const item &it ) -> std::optional<payload>;
auto write_payload( item &it, const payload &data ) -> void;
auto clear_payload( item &it ) -> void;
auto set_payload_from_json( item &it, const std::string &json ) -> void;
auto payload_json( const payload &data ) -> std::string;
auto restore_parts( const payload &data ) -> std::vector<detached_ptr<item>>;
auto make_compact_parts( const std::vector<part_fact> &facts,
                         const schema &sch ) -> std::vector<compact_part>;
auto apply_on_damage( item &it, int qty ) -> void;
auto run_full( const schema &sch, const std::vector<part_fact> &facts,
const fast_blob &blob, const lua_opts &opts = {} ) -> full_blob;
auto make_item( const schema &sch, const std::vector<part_fact> &facts,
                const make_opts &opts ) -> detached_ptr<item>;
auto blob_kcal( const item &it ) -> std::optional<int>;
auto blob_vitamins( const item &it ) -> std::optional<std::map<vitamin_id, int>>;
auto blob_mass( const item &it ) -> std::optional<int>;
auto blob_volume( const item &it ) -> std::optional<int>;
auto component_hash( const item &it ) -> std::optional<std::uint64_t>;

} // namespace proc
