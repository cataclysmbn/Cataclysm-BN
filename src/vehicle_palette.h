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

/**
 *  This class is used for random vehicle color choices
 */
class VehiclePalette
{
    public:
        VehiclePalette() = default;

        static void load_palette( const JsonObject &jo, const std::string &src );

        void load( const JsonObject &jo, const std::string &src );

        void check() const;

        static void check_definitions();

        static void reset();

        int fuzzy_to_index( const vpart_id &id ) const;

        std::vector<RGBColor> pick_colors() const;

        vpalette_id id;

        bool was_loaded;

    private:
        std::vector<weighted_int_list<std::string>> colors;
        std::map<std::string, int> fuzzy_color_match;
};
