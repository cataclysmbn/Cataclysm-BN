#pragma once

#include "json.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

/**
 *  Flags used specifically for enchantments
 */
class enchantment_flag {
public:
    enchantment_flag() = default;

    static void load_enchantment_flags(const JsonObject& jo, const std::string& src);

    void load(const JsonObject& jo, const std::string& src);

    static void check_consistency();

    void check() const;

    static std::vector<enchantment_flag> get_all();

    static void reset();

    std::set<enchantment_flag_id> get_parents() const;

    enchantment_flag_id id;

    translation info;

    bool was_loaded = false;

    std::set<enchantment_flag_id> conflicts = std::set<enchantment_flag_id>();

    std::set<enchantment_flag_id> parents = std::set<enchantment_flag_id>();
};
