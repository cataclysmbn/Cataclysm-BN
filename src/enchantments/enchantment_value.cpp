#include "enchantment_value.h"

#include "assign.h"
#include "debug.h"
#include "enchantment_condition.h"
#include "generic_factory.h"
#include "json.h"
#include "type_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <vector>

namespace {
generic_factory<enchantment_value> all_enchantment_values("Enchantment Values");
}

IMPLEMENT_STRING_AND_INT_IDS(enchantment_value, all_enchantment_values);

void enchantment_value::load_enchantment_values(const JsonObject& jo, const std::string& src) {
    all_enchantment_values.load(jo, src);
}

// The result is the lowest nodes of the enchantment child tree
std::vector<enchantment_value_id> enchantment_value::define_child_enchantments(
    const enchantment_value& main, const std::vector<enchantment_value_id>& parents,
    const JsonObject& obj, const bool first) const {
    std::vector<enchantment_value_id> res;
    for (enchantment_value_id to_suffix_id : parents) {
        enchantment_value suffixed = enchantment_value(main);
        suffixed.id = enchantment_value_id(to_suffix_id.str() + "_" + obj.get_string("suffix"));
        suffixed.parent_ids = {to_suffix_id};
        if (!first && to_suffix_id == main.id) { suffixed.parent_ids = {}; }
        optional(obj, true, "desc", suffixed.desc);
        optional(obj, true, "desc_insert", suffixed.desc_insert, translation_reader());
        all_enchantment_values.insert(suffixed);
        if (obj.has_array("suffixes")) {
            for (JsonObject jo : obj.get_array("suffixes")) {
                auto temp = std::vector<enchantment_value_id>{suffixed.id};
                auto children = define_child_enchantments(main, temp, jo, false);
                res.insert(res.end(), children.begin(), children.end());
            }
        } else {
            // End of the chain, this is the lowest node
            res.push_back(suffixed.id);
        }
    }
    return res;
}
void enchantment_value::load(const JsonObject& jo, const std::string& src) {
    optional(jo, was_loaded, "can_add", can_add, true);
    optional(jo, was_loaded, "can_mult", can_mult, true);
    optional(jo, was_loaded, "can_max", can_max, false);
    optional(jo, was_loaded, "increase_good", increase_good, true);
    optional(jo, was_loaded, "unsupported_conditions", unsupported_conditions,
             enum_flags_reader<enchantment_condition_type>("enchantment_condition_type"));

    mandatory(jo, was_loaded, "desc", desc);
    optional(jo, false, "desc_insert", desc_insert);
    if (jo.has_array("suffixes")) {
        bool first = true;
        std::vector<enchantment_value_id> last_enchantment_id_set;
        for (JsonArray suffix_set : jo.get_array("suffixes")) {
            last_enchantment_id_set.push_back(id);
            std::vector<enchantment_value_id> new_defined_id_set;
            for (const JsonObject& suffix : suffix_set) {
                auto children =
                    define_child_enchantments(*this, last_enchantment_id_set, suffix, first);
                new_defined_id_set
                    .insert(new_defined_id_set.end(), children.begin(), children.end());
            }
            last_enchantment_id_set = new_defined_id_set;
            first = false;
        }
        // Okay this is the logic
        // CLIMATE_CONTROL
        //   - CLIMATE_CONTROL_COOLING
        //     - CLIMATE_CONTROL_COOLING_LEG
        //       - CLIMATE_CONTROL_COOLING_LEG_L -> Must reference CLIMATE_CONTROL_LEG
        //   - CLIMATE_CONTROL_LEG -> CANNOT reference CLIMATE_CONTROL
        //     - CLIMATE_CONTROL_LEG_L
        //   - CLIMATE_CONTROL_L -> CANNOT reference CLIMATE_CONTROL
        // So to get from CLIMATE_CONTROL_COOLING_LEG_L to CLIMAT_CONTROL_LEG_L
        // We must remove `COOLING`, so remove `CLIMATE_CONTROL` prefix
        // Then remove `COOLING` then add back `CLIMATE_CONTROL` prefix
        // As certain ones are dead ends, must add `is_valid` check too
        for (enchantment_value_id ench_id : last_enchantment_id_set) {
            enchantment_value* ench = const_cast<enchantment_value*>(&*ench_id);
            std::string id_str = ench_id.str();
            // Remove the Base Parent Id + "_"
            id_str.erase(0, id.str().length() + 1);
            // Remove the first suffix
            id_str.erase(0, id_str.find('_'));
            // Readd the prefix
            id_str = id.str() + id_str;
            // If valid add as a parent
            auto parent_id = enchantment_value_id(id_str);
            if (parent_id.is_valid() && !std::ranges::contains(ench->parent_ids, id)) {
                ench->parent_ids.emplace_back(parent_id);
            }
        }
    }
}

void enchantment_value::check() const {
    // Currently a placeholder, needed for generic factories
    // This will be important if anything more complex then two bools and non-c++ definitions
    // Gets added to enchantment values
    return;
}

void enchantment_value::check_consistency() { all_enchantment_values.check(); }

bool enchantment_value::has_parent() const { return !parent_ids.empty(); }

std::vector<enchantment_value_id> enchantment_value::get_parents() const { return parent_ids; }

std::string enchantment_value::get_desc() const {
    return string_format(desc.translated(), desc_insert);
}
std::vector<enchantment_value> enchantment_value::get_all() {
    return all_enchantment_values.get_all();
}

void enchantment_value::reset() { all_enchantment_values.reset(); }
