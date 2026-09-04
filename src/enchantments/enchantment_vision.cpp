#include "enchantment_vision.h"

#include "assign.h"
#include "debug.h"
#include "enchantment_condition.h"
#include "enums.h"
#include "generic_factory.h"
#include "json.h"
#include "monster.h"
#include "mtype.h"
#include "type_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <vector>

namespace {
generic_factory<enchantment_vision> all_enchantment_vision("Enchantment visions");
}

IMPLEMENT_STRING_AND_INT_IDS(enchantment_vision, all_enchantment_vision);

void enchantment_vision::load_enchantment_vision(const JsonObject& jo, const std::string& src) {
    all_enchantment_vision.load(jo, src);
}

void enchantment_vision::load(const JsonObject& jo, const std::string& src) {
    mandatory(jo, was_loaded, "desc", desc);
    optional(jo, was_loaded, "distance", max_distance, -1);
    optional(jo, was_loaded, "same_z_level", same_zlev, false);
    optional(jo, was_loaded, "require_los", require_los, false);
    optional(jo, was_loaded, "detect_heat", detect_heat, false);
    optional(jo, was_loaded, "show_with_species", show_with_species,
             auto_flags_reader<species_id>{});
    optional(jo, was_loaded, "show_with_flag", show_with_flags,
             enum_flags_reader<m_flag>("m_flag"));
    optional(jo, was_loaded, "show_without_any_flag", show_without_any_flags,
             enum_flags_reader<m_flag>("m_flag"));
    optional(jo, was_loaded, "show_with_effect", show_with_effect, auto_flags_reader<efftype_id>{});
    optional(jo, was_loaded, "show_without_any_effect", show_without_any_effect,
             auto_flags_reader<efftype_id>{});

    use_distance = max_distance != -1;

    optional(jo, was_loaded, "show_normal", show_normal, false);
    if (!show_normal) {
        if (jo.has_member("vision_desc")) {
            if (jo.has_object("vision_desc")) {
                JsonObject obj = jo.get_object("vision_desc");
                enchantment_vision_description desc;
                mandatory(obj, false, "tile_id", desc.tile_id);
                mandatory(obj, false, "description", desc.description);
                const auto creature_sizes =
                    {creature_size::tiny, creature_size::small, creature_size::medium,
                     creature_size::large, creature_size::huge};
                for (auto size : creature_sizes) { look_descriptions[size] = desc; }
            } else if (jo.has_array("vision_desc")) {
                for (JsonObject obj : jo.get_array("vision_desc")) {
                    enchantment_vision_description desc;
                    creature_size size;
                    mandatory(obj, false, "tile_id", desc.tile_id);
                    mandatory(obj, false, "description", desc.description);
                    mandatory(obj, false, "size", size,
                              enum_flags_reader<creature_size>("creature_size"));
                    look_descriptions[size] = desc;
                }
            } else {
                throw JsonError(string_format(
                    "%s has `vision_desc` that is neither an array or object", id.str()));
            }
        } else {
            throw JsonError(string_format(
                "Vision enchantment %s requires either `show_normal` or `vision_desc`", id.str()));
        }
    }
}

void enchantment_vision::check() const {
    if (!show_normal) {
        const auto creature_sizes =
            {creature_size::tiny, creature_size::small, creature_size::medium, creature_size::large,
             creature_size::huge};
        for (auto size : creature_sizes) {
            if (!look_descriptions.contains(size)) {
                throw JsonError(string_format(
                    "`vision_desc` arrays must have `TINY`, `SMALL` `MEDIUM` `LARGE` and `HUGE`, %s lacks some",
                    id.str()));
            }
        }
    }
    for (species_id species : show_with_species) {
        if (!species.is_valid()) {
            debugmsg("%s has invalid show_with_species %s", id.str(), species.str());
        }
    }
    for (efftype_id effect : show_with_effect) {
        if (!effect.is_valid()) {
            debugmsg("%s has invalid show_with_effect %s", id.str(), effect.str());
        }
    }
    for (efftype_id effect : show_without_any_effect) {
        if (!effect.is_valid()) {
            debugmsg("%s has invalid show_without_any_effect %s", id.str(), effect.str());
        }
    }
    return;
}

void enchantment_vision::check_consistency() { all_enchantment_vision.check(); }

void enchantment_vision::reset() { all_enchantment_vision.reset(); }

bool enchantment_vision::mon_passes(
    const Creature& mon, const int dist, const bool on_same_zlevel, const bool has_los) const {

    if (use_distance && dist > max_distance) { return false; }
    if (same_zlev && !on_same_zlevel) { return false; }
    if (require_los && !has_los) { return false; }
    if (detect_heat && !mon.is_warm()) { return false; }

    if (show_with_species.size() > 0) {
        bool species_passes = false;
        for (species_id test_id : show_with_species) {
            if (mon.in_species(test_id)) {
                species_passes = true;
                break;
            }
        }
        if (!species_passes) { return false; }
    }
    if (show_with_flags.size() > 0) {
        bool flags_passes = false;
        for (m_flag test_id : show_with_flags) {
            if (mon.has_flag(test_id)) {
                flags_passes = true;
                break;
            }
        }
        if (!flags_passes) { return false; }
    }
    if (show_without_any_flags.size() > 0) {
        bool flags_passes = false;
        for (m_flag test_id : show_without_any_flags) {
            if (mon.has_flag(test_id)) {
                flags_passes = true;
                break;
            }
        }
        if (flags_passes) { return false; }
    }
    if (show_with_effect.size() > 0) {
        bool effect_passes = false;
        for (efftype_id test_id : show_with_effect) {
            if (mon.has_effect(test_id)) {
                effect_passes = true;
                break;
            }
        }
        if (!effect_passes) { return false; }
    }
    if (show_without_any_effect.size() > 0) {
        bool effect_passes = false;
        for (efftype_id test_id : show_without_any_effect) {
            if (mon.has_effect(test_id)) {
                effect_passes = true;
                break;
            }
        }
        if (effect_passes) { return false; }
    }
    return true;
}

std::string enchantment_vision::get_mon_desc(const Creature& mon) const {
    if (look_descriptions.contains(mon.get_size())) {
        return look_descriptions.at(mon.get_size()).description.translated();
    } else {
        debugmsg("Invalid monster description for size %s, normal: %s",
                 io::enum_to_string(mon.get_size()), std::to_string(show_normal));
        return "infrared_creature";
    }
}

std::string enchantment_vision::get_mon_tile(const Creature& mon) const {
    if (look_descriptions.contains(mon.get_size())) {
        return look_descriptions.at(mon.get_size()).tile_id;
    } else {
        debugmsg("Invalid monster tile for size %s, normal %s", io::enum_to_string(mon.get_size()),
                 std::to_string(show_normal));
        return "infrared_creature";
    }
}

bool enchantment_vision::use_normal_mon_tile() const { return show_normal; }

std::string enchantment_vision::get_desc() const { return desc.translated(); }

std::vector<enchantment_vision> enchantment_vision::get_all() {
    return all_enchantment_vision.get_all();
}
