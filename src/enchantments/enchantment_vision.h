#pragma once

#include "enums.h"
#include "json.h"
#include "monster.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 *  This class is used for random vehicle color choices
 */
class enchantment_vision {
public:
    enchantment_vision() = default;

    static void load_enchantment_vision(const JsonObject& jo, const std::string& src);

    void load(const JsonObject& jo, const std::string& src);

    static void check_consistency();

    void check() const;

    static auto get_all() -> std::vector<enchantment_vision>;

    static void reset();

    enchantment_vision_id id;
    bool was_loaded = false;

    auto mon_passes(
        const Creature& mon, const int dist, const bool on_same_zlevel, const bool has_los) const
        -> bool;
    auto get_mon_tile(const Creature& mon) const -> std::string;
    auto get_mon_desc(const Creature& mon) const -> std::string;
    auto use_normal_mon_tile() const -> bool;
    auto get_desc() const -> std::string;

    // Needed for bindings
    auto operator==(const enchantment_vision& rhs) const -> bool { return id == rhs.id; }
    auto operator<(const enchantment_vision& rhs) const -> bool { return id < rhs.id; }

private:
    // Description shown on items
    translation desc;

    // Conditions on the view
    bool use_distance;
    bool same_zlev;
    bool require_los;
    int max_distance;
    bool detect_heat;
    std::vector<species_id> show_with_species;
    std::vector<m_flag> show_with_flags;
    std::vector<m_flag> show_without_any_flags;
    std::vector<efftype_id> show_with_effect;
    std::vector<efftype_id> show_without_any_effect;

    // What sprite and description to show
    bool show_normal;
    struct enchantment_vision_description {
        std::string tile_id;
        translation description;
    };
    std::map<creature_size, enchantment_vision_description> look_descriptions;
};
