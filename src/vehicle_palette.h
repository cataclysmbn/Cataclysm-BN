#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "hsv_color.h"
#include "json.h"
#include "mapgen.h"
#include "string_id.h"
#include "type_id.h"
#include "vehicle_group.h"
#include "weighted_list.h"
#include "units_angle.h"

static std::unordered_map<vpalette_id, VehiclePalette> vehicle_color_palettes;

/**
 *  This class is used for random vehicle color choices
 */
class VehiclePalette
{
    public:
        VehiclePalette() = default;

        static void load( const JsonObject &jo );

        static void check();

        static void reset();

        int fuzzy_to_index( const vpart_id &id ) const;

        std::vector<RGBColor> pick_colors() const;

    private:
        vpalette_id id;
        std::vector<weighted_int_list<std::string>> colors;
        std::map<std::string, int> fuzzy_color_match;
};
