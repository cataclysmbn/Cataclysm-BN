#include "enchantment_value.h"

#include "assign.h"
#include "debug.h"
#include "enchantment_condition.h"
#include "generic_factory.h"
#include "type_id_implement.h"

#include <optional>

namespace {
generic_factory<enchantment_value> all_enchantment_values("Enchantment Values");
}

IMPLEMENT_STRING_AND_INT_IDS(enchantment_value, all_enchantment_values);

void enchantment_value::load_enchantment_values(const JsonObject& jo, const std::string& src) {
    all_enchantment_values.load(jo, src);
}

void enchantment_value::load(const JsonObject& jo, const std::string& src) {
    optional(jo, was_loaded, "can_add", can_add, true);
    optional(jo, was_loaded, "can_mult", can_mult, true);
    optional(jo, was_loaded, "can_max", can_max, false);
    optional(jo, was_loaded, "increase_good", increase_good, true);
    optional(jo, was_loaded, "unsupported_conditions", unsupported_conditions,
             enum_flags_reader<enchantment_condition_type>("enchantment_condition_type"));

    mandatory(jo, was_loaded, "desc", desc);
    if (jo.has_array("suffixes")) {
        for (JsonValue val : jo.get_array("suffixes")) {
            JsonArray suffix = val.get_array();
            enchantment_value suffixed = enchantment_value(*this);
            suffixed.id = enchantment_value_id(suffixed.id.str() + "_" + suffix.next_string());
            suffixed.parent_id = id;
            suffixed.desc = suffix.next_string();
            all_enchantment_values.insert(suffixed);
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

bool enchantment_value::has_parent() const { return parent_id != enchantment_value_id::NULL_ID(); }

enchantment_value_id enchantment_value::get_parent() const { return parent_id; }

std::vector<enchantment_value> enchantment_value::get_all() {
    return all_enchantment_values.get_all();
}

void enchantment_value::reset() { all_enchantment_values.reset(); }
