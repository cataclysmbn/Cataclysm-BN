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

extern std::unordered_map<string_id<VehiclePalette>, VehiclePalette> vehicle_color_palettes;

/**
 *  This class is used for random vehicle color choices
 */
class VehiclePalette
{
    public:
        VehiclePalette() = default;

        void load( const JsonObject &jo );

        int fuzzy_to_index( const vpart_id &id ) const;

        std::vector<RGBColor> pick_colors( const int index ) const;

    private:
        std::vector<weighted_int_list<RGBColor>> colors;
        std::map<std::string, int> fuzzy_color_match;
};
