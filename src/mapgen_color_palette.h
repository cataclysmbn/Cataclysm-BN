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
class MapgenColorPalette
{
    public:
        MapgenColorPalette() = default;

        static void load_palette( const JsonObject &jo, const std::string &src );

        void load( const JsonObject &jo, const std::string &src );

        void check() const;

        static void check_definitions();

        static void reset();

        std::optional<RGBColor> pick_color( unsigned int seed ) const;

        mpalette_id id;

        bool was_loaded;

        static mpalette_id define_new_palette( const JsonObject &obj );

    private:
        weighted_int_list<std::string> colors;

        static mpalette_id get_unique_id();

        static int next_id;

};
