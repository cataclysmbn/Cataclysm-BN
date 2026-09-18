#pragma once

#include "cata_variant.h"
#include "memory_fast.h"

struct mapgen_arguments;
struct mapgen_parameters;
template <typename Id> class mapgen_value;
class mapgendata;

enum class mapgen_parameter_scope {
    // Should be ordered from most general to most specific
    overmap_special,
    omt,
    nest,
    last
};

template <> struct enum_traits<mapgen_parameter_scope> {
    static constexpr mapgen_parameter_scope last = mapgen_parameter_scope::last;
};

inline auto operator<(mapgen_parameter_scope l, mapgen_parameter_scope r) -> bool {
    return static_cast<int>(l) < static_cast<int>(r);
}

class mapgen_parameter {
public:
    mapgen_parameter();
    mapgen_parameter(
        const mapgen_value<std::string>& def, cata_variant_type, mapgen_parameter_scope);

    void deserialize(JsonIn&);

    auto scope() const -> mapgen_parameter_scope { return scope_; }
    auto type() const -> cata_variant_type;
    auto get(const mapgendata&) const -> cata_variant;
    auto all_possible_values(const mapgen_parameters&) const -> std::vector<std::string>;

    void check(const mapgen_parameters&, const std::string& context) const;
    void check_consistent_with(const mapgen_parameter&, const std::string& context) const;

private:
    mapgen_parameter_scope scope_;
    cata_variant_type type_;
    // Using a pointer here mostly to move the definition of mapgen_value to the
    // cpp file
    shared_ptr_fast<const mapgen_value<std::string>> default_;
};

struct mapgen_parameters {
    std::unordered_map<std::string, mapgen_parameter> map;
    using iterator = std::unordered_map<std::string, mapgen_parameter>::const_iterator;

    auto add_unique_parameter(
        const std::string& prefix, const mapgen_value<std::string>& def, cata_variant_type,
        mapgen_parameter_scope) -> iterator;

    auto params_for_scope(mapgen_parameter_scope scope) const -> mapgen_parameters;
    auto get_args(const mapgendata&, mapgen_parameter_scope scope) const -> mapgen_arguments;
    void check_and_merge(
        const mapgen_parameters&, const std::string& context,
        mapgen_parameter_scope up_to_scope = mapgen_parameter_scope::last);
};
