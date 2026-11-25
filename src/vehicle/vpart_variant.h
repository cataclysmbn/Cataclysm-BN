#pragma once

#include "units.h"

#include <array>
#include <string>

class JsonObject;

/// CDDA part variants: physical part identity remains independent of appearance.
struct vpart_variant {
    std::string id;
    std::string label;
    std::string base_label;
    std::array<char32_t, 8> symbols{};
    std::array<char32_t, 8> symbols_broken{};

    auto load(const JsonObject& data) -> void;
    auto get_label() const -> std::string;
    auto get_symbol(units::angle direction, bool broken) const -> char32_t;
};
