#pragma once

#include "reload.h"

#include <vector>

class item;
class player;

namespace reload_selection {
struct selection_options {
    bool prompt = false;
    reload::discovery_options discovery;
};

enum class selection_outcome {
    automatic,
    interaction_required,
    empty_supplied,
    missing_magazine,
    nothing_to_reload,
    missing_ammunition
};

struct selection_result {
    selection_outcome outcome;
    item_reload_option selected{};
    std::vector<item_reload_option> options;
};

auto order_ammo(std::vector<item_reload_option>& options) -> void;
auto prepare(const player& who, item& base, selection_options options = {}) -> selection_result;
auto prepare(const player& who, std::vector<item_reload_option> options) -> selection_result;
} // namespace reload_selection
