#pragma once

#include "hsv_color.h"
#include "json.h"
#include "mapgen.h"
#include "string_id.h"
#include "type_id.h"
#include "units_angle.h"
#include "vehicle_group.h"
#include "weighted_list.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 *  This class is used for random vehicle color choices
 */
class enchantment_value {
public:
    enchantment_value() = default;

    static void load_enchantment_values(const JsonObject& jo, const std::string& src);

    void load(const JsonObject& jo, const std::string& src);

    static void check_consistency();

    void check() const;

    static std::vector<enchantment_value> get_all();

    static void reset();

    enchantment_value_id id;

    bool was_loaded;
    bool can_add;
    bool can_mult;

    bool has_parent() const;
    enchantment_value_id get_parent() const;

private:
    enchantment_value_id parent_id = enchantment_value_id::NULL_ID();
};
