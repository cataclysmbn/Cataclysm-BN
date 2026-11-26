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

    static auto get_all() -> std::vector<enchantment_flag>;

    static void reset();

    auto get_parents() const -> std::set<enchantment_flag_id>;

    enchantment_flag_id id;

    translation info;

    bool was_loaded = false;

    std::set<enchantment_flag_id> conflicts = std::set<enchantment_flag_id>();

    std::set<enchantment_flag_id> parents = std::set<enchantment_flag_id>();

    // Needed for bindings
    auto operator==(const enchantment_flag& rhs) const -> bool { return id == rhs.id; }
    auto operator<(const enchantment_flag& rhs) const -> bool { return id < rhs.id; }
};
