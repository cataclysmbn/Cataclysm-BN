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

    static std::vector<enchantment_vision> get_all();

    static void reset();

    enchantment_vision_id id;
    bool was_loaded = false;

    bool mon_passes(
        const Creature& mon, const int dist, const bool on_same_zlevel, const bool has_los) const;
    std::string get_mon_tile(const Creature& mon) const;
    std::string get_mon_desc(const Creature& mon) const;
    bool use_normal_mon_tile() const;
    std::string get_desc() const;

    // Needed for bindings
    bool operator==(const enchantment_vision& rhs) const { return id == rhs.id; }
    bool operator<(const enchantment_vision& rhs) const { return id < rhs.id; }

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
