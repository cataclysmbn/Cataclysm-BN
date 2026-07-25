#pragma once

#include "calendar.h"
#include "character.h"
#include "item.h"
#include "json.h"
#include "string_id.h"
#include "type_id.h"


class enchantment_condition_function {
public:
    enchantment_condition_function() = default;
    virtual ~enchantment_condition_function() = default;
    virtual bool check_item_condition(const item& /*it*/) const { return false; }
    virtual bool check_item_character_condition(
        const Character& /*guy*/, const item& /*it*/) const {
        return false;
    }
    virtual bool check_character_condition(const Character& /*guy*/, const bool /*active*/) const {
        return false;
    }
    virtual bool check_generic_condition(const bool /*active*/) const { return false; }
};

class enchantment_condition_lua: public virtual enchantment_condition_function {
private:
    std::string name;
    sol::protected_function generic_func;
    sol::protected_function character_func;
    sol::protected_function item_func;
    sol::protected_function item_character_func;

public:
    enchantment_condition_lua(
        const std::string name, sol::protected_function&& generic,
        sol::protected_function&& character, sol::protected_function&& item,
        sol::protected_function&& item_character)
        : name(name),
          generic_func(std::move(generic)),
          character_func(std::move(character)),
          item_func(std::move(item)),
          item_character_func(std::move(item_character)) {}

    virtual bool check_item_condition(const item& it) const override;
    virtual bool check_item_character_condition(
        const Character& guy, const item& it) const override;
    virtual bool check_character_condition(const Character& guy, const bool active) const override;
    virtual bool check_generic_condition(const bool active) const override;
};

enum enchantment_condition_type : unsigned int {
    GLOBAL,
    ITEM,
    ITEM_CHARACTER,
    CHARACTER,
    NUM_CONDITIONS
};

class enchantment_condition {
public:
    enchantment_condition() = default;

    static void load_enchantment_conditions(const JsonObject& jo, const std::string& src);

    void load(const JsonObject& jo, const std::string& src);

    static void check_consistency();

    void check() const;

    static std::vector<enchantment_condition> get_all();

    static void reset();

    bool item_condition(const item& it) const;
    bool item_character_condition(const Character& guy, const item& it) const;
    bool character_condition(const Character& guy, const bool active) const;
    bool generic_condition(const bool active) const;

    // Generic Factory stuff
    enchantment_condition_id id;
    bool was_loaded = false;

    // Should check_item_condition or check_generic_condition be called?
    enchantment_condition_type cond_type;

    // String used as key for condition_function map
    std::string condition_function;

    // String used for display of enchantment conditions in item info
    translation condition_info;
    // First: String id of the condition function
    // Second: The condition, called via check_item_condition for item conditions
    // Or check_generic_condition for everything else
    static std::map<std::string, std::shared_ptr<enchantment_condition_function>>
        condition_functions;
};

template <> struct enum_traits<enchantment_condition_type> {
    static constexpr enchantment_condition_type last = enchantment_condition_type::NUM_CONDITIONS;
};
