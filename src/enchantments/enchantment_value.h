#pragma once

#include "enchantment_condition.h"
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

    std::set<enchantment_condition_type> unsupported_conditions;

    bool was_loaded = false;
    bool can_add = true;
    bool can_mult = true;
    bool can_max = false;

    bool increase_good = true;

    std::string get_desc() const;
    bool has_parent() const;
    std::vector<enchantment_value_id> get_parents() const;

    // Needed for bindings
    bool operator==(const enchantment_value& rhs) const { return id == rhs.id; }
    bool operator<(const enchantment_value& rhs) const { return id < rhs.id; }

private:
    std::vector<enchantment_value_id> define_child_enchantments(
        const enchantment_value& main, const std::vector<enchantment_value_id>& parents,
        const JsonObject& obj, const bool first) const;

    std::vector<enchantment_value_id> parent_ids;
    translation desc;
    std::vector<translation> desc_insert;
};
