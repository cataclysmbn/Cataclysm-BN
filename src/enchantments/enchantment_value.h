#pragma once

#include "json.h"
#include "string_id.h"
#include "type_id.h"

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

    bool was_loaded = false;
    bool can_add = true;
    bool can_mult = true;
    bool can_max = false;

    bool has_parent() const;
    enchantment_value_id get_parent() const;

private:
    enchantment_value_id parent_id = enchantment_value_id::NULL_ID();
};
