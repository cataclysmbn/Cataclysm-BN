#include "enchantment_condition.h"

#include "assign.h"
#include "catalua_impl.h"
#include "character.h"
#include "debug.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "item.h"
#include "type_id_implement.h"

#include <optional>
#include <vector>

namespace {
generic_factory<enchantment_condition> all_enchantment_conditions("Enchantment flags");
void reset_condition_functions();
} // namespace

IMPLEMENT_STRING_AND_INT_IDS(enchantment_condition, all_enchantment_conditions);

void enchantment_condition::load_enchantment_conditions(
    const JsonObject& jo, const std::string& src) {
    all_enchantment_conditions.load(jo, src);
}

void enchantment_condition::load(const JsonObject& jo, const std::string& src) {
    mandatory(jo, was_loaded, "condition_type", cond_type,
              enum_flags_reader<enchantment_condition_type>("enchantment_condition_type"));
    mandatory(jo, was_loaded, "condition_function", condition_function);
    mandatory(jo, was_loaded, "condition_info", condition_info);
}

void enchantment_condition::check() const {}

void enchantment_condition::check_consistency() { all_enchantment_conditions.check(); }


std::vector<enchantment_condition> enchantment_condition::get_all() {
    return all_enchantment_conditions.get_all();
}

void enchantment_condition::reset() {
    all_enchantment_conditions.reset();
    reset_condition_functions();
}


bool enchantment_condition::item_condition(const item& it) const {
    return condition_functions[condition_function]->check_item_condition(it);
}

bool enchantment_condition::item_character_condition(const Character& guy, const item& it) const {
    return condition_functions[condition_function]->check_item_character_condition(guy, it);
}

bool enchantment_condition::character_condition(const Character& guy, const bool active) const {
    return condition_functions[condition_function]->check_character_condition(guy, active);
}

bool enchantment_condition::generic_condition(const bool active) const {
    return condition_functions[condition_function]->check_generic_condition(active);
}

// condition_type enum stuff

namespace io {
template <>
std::string enum_to_string<enchantment_condition_type>(enchantment_condition_type data) {
    switch (data) {
        case enchantment_condition_type::CHARACTER:
            return "character";
        case enchantment_condition_type::ITEM:
            return "item";
        case enchantment_condition_type::ITEM_CHARACTER:
            return "item_and_character";
        case enchantment_condition_type::GLOBAL:
            return "global";
    }
    debugmsg("Invalid enchantment_condition_type; Defaulting to `global`");
    return "global";
}
} // namespace io

// Condition classes + Hardcode Condition Map
class enchantment_condition_always: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool /*active*/) const override { return true; }
};

class enchantment_condition_dawn: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool /*active*/) const override {
        return is_dawn(calendar::turn);
    }
};

class enchantment_condition_day: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool /*active*/) const override {
        return is_day(calendar::turn);
    }
};

class enchantment_condition_dusk: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool /*active*/) const override {
        return is_dusk(calendar::turn);
    }
};

class enchantment_condition_night: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool /*active*/) const override {
        return is_night(calendar::turn);
    }
};

class enchantment_condition_active: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool active) const override { return active; }
};

class enchantment_condition_inactive: public virtual enchantment_condition_function {
public:
    bool check_generic_condition(const bool active) const override { return !active; }
};

class enchantment_condition_inside: public virtual enchantment_condition_function {
public:
    bool check_character_condition(const Character& guy, const bool /*active*/) const override {
        return !get_map().is_outside(guy.bub_pos());
    }
};

class enchantment_condition_outside: public virtual enchantment_condition_function {
public:
    bool check_character_condition(const Character& guy, const bool /*active*/) const override {
        return get_map().is_outside(guy.bub_pos());
    }
};

class enchantment_condition_underground: public virtual enchantment_condition_function {
public:
    bool check_character_condition(const Character& guy, const bool /*active*/) const override {
        return guy.bub_pos().z() < 0;
    }
};

class enchantment_condition_aboveground: public virtual enchantment_condition_function {
public:
    bool check_character_condition(const Character& guy, const bool /*active*/) const override {
        return guy.bub_pos().z() >= 0;
    }
};

class enchantment_condition_underwater: public virtual enchantment_condition_function {
public:
    bool check_character_condition(const Character& guy, const bool /*active*/) const override {
        return get_map().is_divable(guy.bub_pos());
    }
};

class enchantment_condition_has: public virtual enchantment_condition_function {
public:
    bool check_item_character_condition(const Character& guy, const item& it) const override {
        return guy.has_item(it);
    }
};

class enchantment_condition_wield: public virtual enchantment_condition_function {
public:
    bool check_item_character_condition(const Character& guy, const item& it) const override {
        return guy.is_wielding(it);
    }
};

class enchantment_condition_worn: public virtual enchantment_condition_function {
public:
    bool check_item_character_condition(const Character& guy, const item& it) const override {
        return guy.is_worn(it);
    }
};

bool enchantment_condition_lua::check_item_character_condition(
    const Character& guy, const item& it) const {
    if (item_character_func == sol::lua_nil) {
        debugmsg(
            "Enchantment condition %s was called for `item_and_character` function. But it has none, it will never trigger.",
            name);
        return false;
    }
    try {
        sol::state_view lua(item_character_func.lua_state());
        auto params = lua.create_table();
        params["guy"] = &guy;
        params["item"] = &it;
        sol::protected_function_result res = item_character_func(params);
        check_func_result(res);
        bool ret = res;
        return ret;
    } catch (std::runtime_error& e) {
        debugmsg("Failed to run enchantment condition %s for `item_and_character`", name);
        return false;
    }
}
bool enchantment_condition_lua::check_item_condition(const item& it) const {
    if (item_func == sol::lua_nil) {
        debugmsg(
            "Enchantment condition %s was called for `item` function. But it has none, it will never trigger.",
            name);
        return false;
    }
    try {
        sol::state_view lua(item_func.lua_state());
        auto params = lua.create_table();
        params["item"] = &it;
        sol::protected_function_result res = item_func(params);
        check_func_result(res);
        bool ret = res;
        return ret;
    } catch (std::runtime_error& e) {
        debugmsg("Failed to run enchantment condition %s for `item`", name);
        return false;
    }
}
bool enchantment_condition_lua::check_character_condition(
    const Character& guy, const bool active) const {
    if (character_func == sol::lua_nil) {
        debugmsg(
            "Enchantment condition %s was called for `character` function. But it has none, it will never trigger.",
            name);
        return false;
    }
    try {
        sol::state_view lua(character_func.lua_state());
        auto params = lua.create_table();
        params["guy"] = &guy;
        params["active"] = active;
        sol::protected_function_result res = character_func(params);
        check_func_result(res);
        bool ret = res;
        return ret;
    } catch (std::runtime_error& e) {
        debugmsg("Failed to run enchantment condition %s for `character`", name);
        return false;
    }
}
bool enchantment_condition_lua::check_generic_condition(const bool active) const {
    if (generic_func == sol::lua_nil) {
        debugmsg(
            "Enchantment condition %s was called for `global` function. But it has none, it will never trigger.",
            name);
        return false;
    }
    try {
        sol::state_view lua(generic_func.lua_state());
        auto params = lua.create_table();
        params["active"] = active;
        sol::protected_function_result res = generic_func(params);
        check_func_result(res);
        bool ret = res;
        return ret;
    } catch (std::runtime_error& e) {
        debugmsg("Failed to run enchantment condition %s for `global`", name);
        return false;
    }
}

// Both this and the function below have to match
auto enchantment_condition::condition_functions = std::map<
    std::string, std::shared_ptr<enchantment_condition_function>>{
    {"always", std::make_shared<enchantment_condition_always>()},
    {"dawn", std::make_shared<enchantment_condition_dawn>()},
    {"day", std::make_shared<enchantment_condition_day>()},
    {"dusk", std::make_shared<enchantment_condition_dusk>()},
    {"night", std::make_shared<enchantment_condition_night>()},
    {"active", std::make_shared<enchantment_condition_active>()},
    {"inactive", std::make_shared<enchantment_condition_inactive>()},
    {"inside", std::make_shared<enchantment_condition_inside>()},
    {"outside", std::make_shared<enchantment_condition_outside>()},
    {"underground", std::make_shared<enchantment_condition_underground>()},
    {"aboveground", std::make_shared<enchantment_condition_aboveground>()},
    {"underwater", std::make_shared<enchantment_condition_underwater>()},
    {"has", std::make_shared<enchantment_condition_has>()},
    {"wield", std::make_shared<enchantment_condition_wield>()},
    {"worn", std::make_shared<enchantment_condition_worn>()}};

namespace {

void reset_condition_functions() {
    // Reset to original state as it is populated during loading
    enchantment_condition::condition_functions = std::map<
        std::string, std::shared_ptr<enchantment_condition_function>>{
        {"always", std::make_shared<enchantment_condition_always>()},
        {"dawn", std::make_shared<enchantment_condition_dawn>()},
        {"day", std::make_shared<enchantment_condition_day>()},
        {"dusk", std::make_shared<enchantment_condition_dusk>()},
        {"night", std::make_shared<enchantment_condition_night>()},
        {"active", std::make_shared<enchantment_condition_active>()},
        {"inactive", std::make_shared<enchantment_condition_inactive>()},
        {"inside", std::make_shared<enchantment_condition_inside>()},
        {"outside", std::make_shared<enchantment_condition_outside>()},
        {"underground", std::make_shared<enchantment_condition_underground>()},
        {"aboveground", std::make_shared<enchantment_condition_aboveground>()},
        {"underwater", std::make_shared<enchantment_condition_underwater>()},
        {"has", std::make_shared<enchantment_condition_has>()},
        {"wield", std::make_shared<enchantment_condition_wield>()},
        {"worn", std::make_shared<enchantment_condition_worn>()}};
}

} // namespace
