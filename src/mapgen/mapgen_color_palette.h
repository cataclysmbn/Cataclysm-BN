#pragma once

#include "hsv_color.h"
#include "json.h"
#include "mapgen.h"
#include "string_id.h"
#include "type_id.h"
#include "units_angle.h"
#include "vehicle/vehicle_group.h"
#include "weighted_list.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 *  This class is used for random vehicle color choices
 */
class MapgenColorPalette {
public:
    MapgenColorPalette() = default;

    static void load_palette(const JsonObject& jo, const std::string& src);

    void load(const JsonObject& jo, const std::string& src);

    void check() const;

    static void check_definitions();

    static void reset();

    auto pick_color(unsigned int seed) const -> std::optional<RGBColor>;

    mpalette_id id;

    bool was_loaded;

    static auto define_new_palette(const JsonObject& obj) -> mpalette_id;

private:
    weighted_int_list<std::string> colors;

    static auto get_unique_id() -> mpalette_id;

    static int next_id;
};
