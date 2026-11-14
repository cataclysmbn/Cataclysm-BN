#include "string_id.h"

#include <unordered_map>
#include <vector>

namespace {
using InternMapType = std::unordered_map<std::string, int>;
using ReverseLookupVecType = std::vector<const std::string*>;
} // namespace

inline static auto get_intern_map() -> InternMapType& {
    static InternMapType map{};
    return map;
}

inline static auto get_reverse_lookup_vec() -> ReverseLookupVecType& {
    static ReverseLookupVecType vec{};
    return vec;
}

template <typename S> inline static auto universal_string_id_intern(S&& s) -> int {
    int next_id = get_reverse_lookup_vec().size();
    const auto& pair = get_intern_map().emplace(std::forward<S>(s), next_id);
    if (pair.second) { // inserted
        get_reverse_lookup_vec().push_back(&pair.first->first);
    }
    return pair.first->second;
}

auto string_identity_static::string_id_intern(const std::string& s) -> int {
    return universal_string_id_intern(s);
}

auto string_identity_static::string_id_intern(std::string& s) -> int {
    return universal_string_id_intern(s);
}

auto string_identity_static::string_id_intern(std::string&& s) -> int {
    return universal_string_id_intern(std::move(s));
}

auto string_identity_static::get_interned_string(int id) -> const std::string& {
    return *get_reverse_lookup_vec()[id];
}

auto string_identity_static::empty_interned_string() -> int {
    static int empty_string_id = string_id_intern("");
    return empty_string_id;
}
