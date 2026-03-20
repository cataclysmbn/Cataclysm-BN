#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "enum_traits.h"
#include "string_id.h"
#include "type_id.h"

namespace proc
{

struct schema;
using schema_id = string_id<schema>;

struct slot;
using slot_id = string_id<slot>;

using part_ix = int;
inline constexpr part_ix invalid_part_ix = -1;

enum class hist : std::uint8_t {
    none = 0,
    compact,
    full,
    num_hist
};

struct part_fact {
    part_ix ix = invalid_part_ix;
    itype_id id = itype_id::NULL_ID();
    std::vector<std::string> tag;
    std::vector<flag_id> flag;
    std::map<quality_id, int> qual;
    std::vector<material_id> mat;
    std::map<vitamin_id, int> vit;
    int mass_g = 0;
    int volume_ml = 0;
    int kcal = 0;
    float hp = 1.0f;
    int chg = 0;
    std::string proc;

    auto valid() const -> bool {
        return ix >= 0 && !id.is_null();
    }

    auto operator==( const part_fact & ) const -> bool = default;
};

struct pick {
    slot_id slot;
    std::vector<part_ix> parts;

    auto empty() const -> bool {
        return parts.empty();
    }

    auto operator==( const pick & ) const -> bool = default;
};

struct fast_blob {
    int mass_g = 0;
    int volume_ml = 0;
    int kcal = 0;
    std::map<vitamin_id, int> vit;
    std::string name;

    auto empty() const -> bool {
        return mass_g == 0 && volume_ml == 0 && kcal == 0 && vit.empty() && name.empty();
    }

    auto operator==( const fast_blob & ) const -> bool = default;
};

struct full_blob {
    fast_blob data;

    auto operator==( const full_blob & ) const -> bool = default;
};

} // namespace proc

template<>
struct enum_traits<proc::hist> {
    static constexpr proc::hist last = proc::hist::num_hist;
};
