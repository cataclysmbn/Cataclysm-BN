#pragma once

#include <optional>
#include <vector>

#include "proc_builder.h"

class Character;
class recipe;
class item;

namespace proc
{

struct ui_pick {
    slot_id slot = slot_id::NULL_ID();
    item *src = nullptr;
    part_fact fact;

    auto valid() const -> bool {
        return !slot.is_null() && src != nullptr && fact.valid();
    }
};

struct ui_result {
    std::vector<ui_pick> picks;
    fast_blob preview;

    auto empty() const -> bool {
        return picks.empty();
    }
};

auto open_builder( Character &who, const recipe &rec ) -> std::optional<ui_result>;

} // namespace proc
