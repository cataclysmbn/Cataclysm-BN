#include "bonuses.h"

#include "character.h"
#include "damage.h"
#include "json.h"
#include "string_formatter.h"
#include "translations.h"

#include <algorithm>
#include <string>
#include <utility>

static auto needs_damage_type(affected_stat as) -> bool {
    return as == affected_stat::DAMAGE || as == affected_stat::ARMOR
        || as == affected_stat::ARMOR_PENETRATION || as == affected_stat::TARGET_ARMOR_MULTIPLIER;
}

static const std::map<std::string, affected_stat> affected_stat_map = {
    {std::make_pair("hit", affected_stat::HIT), std::make_pair("dodge", affected_stat::DODGE),
     std::make_pair("block", affected_stat::BLOCK), std::make_pair("speed", affected_stat::SPEED),
     std::make_pair("movecost", affected_stat::MOVE_COST),
     std::make_pair("damage", affected_stat::DAMAGE), std::make_pair("armor", affected_stat::ARMOR),
     std::make_pair("arpen", affected_stat::ARMOR_PENETRATION),
     std::make_pair("target_armor_multiplier", affected_stat::TARGET_ARMOR_MULTIPLIER)}};

static const std::map<std::string, scaling_stat> scaling_stat_map = {{
    std::make_pair("str", STAT_STR),
    std::make_pair("dex", STAT_DEX),
    std::make_pair("int", STAT_INT),
    std::make_pair("per", STAT_PER),
}};

static auto scaling_stat_from_string(const std::string& s) -> scaling_stat {
    const auto& iter = scaling_stat_map.find(s);
    if (iter == scaling_stat_map.end()) { return STAT_NULL; }

    return iter->second;
}

static auto affected_stat_from_string(const std::string& s) -> affected_stat {
    const auto& iter = affected_stat_map.find(s);
    if (iter != affected_stat_map.end()) { return iter->second; }

    return affected_stat::NONE;
}

static const std::map<affected_stat, std::string> affected_stat_map_translation = {{
    std::make_pair(affected_stat::HIT, translate_marker("Accuracy")),
    std::make_pair(affected_stat::DODGE, translate_marker("Dodge")),
    std::make_pair(affected_stat::BLOCK, translate_marker("Block")),
    std::make_pair(affected_stat::SPEED, translate_marker("Speed")),
    std::make_pair(affected_stat::MOVE_COST, translate_marker("Move cost")),
    std::make_pair(affected_stat::DAMAGE, translate_marker("damage")),
    std::make_pair(affected_stat::ARMOR, translate_marker("Armor")),
    std::make_pair(affected_stat::ARMOR_PENETRATION, translate_marker("Armor penetration")),
    std::make_pair(
        affected_stat::TARGET_ARMOR_MULTIPLIER, translate_marker("Target armor multiplier")),
}};

static auto string_from_affected_stat(const affected_stat& s) -> std::string {
    const auto& iter = affected_stat_map_translation.find(s);
    return iter != affected_stat_map_translation.end() ? _(iter->second) : "";
}

static const std::map<scaling_stat, std::string> scaling_stat_map_translation = {{
    std::make_pair(STAT_STR, translate_marker("strength")),
    std::make_pair(STAT_DEX, translate_marker("dexterity")),
    std::make_pair(STAT_INT, translate_marker("intelligence")),
    std::make_pair(STAT_PER, translate_marker("perception")),
}};

static auto string_from_scaling_stat(const scaling_stat& s) -> std::string {
    const auto& iter = scaling_stat_map_translation.find(s);
    return iter != scaling_stat_map_translation.end() ? _(iter->second) : "";
}

bonus_container::bonus_container() = default;

effect_scaling::effect_scaling(const JsonObject& obj) {
    if (obj.has_string("scaling-stat")) {
        stat = scaling_stat_from_string(obj.get_string("scaling-stat"));
    } else {
        stat = STAT_NULL;
    }

    scale = obj.get_float("scale");
}

void bonus_container::load(const JsonObject& jo) {
    load(jo.get_array("flat_bonuses"), false);
    load(jo.get_array("mult_bonuses"), true);
}

void bonus_container::load(const JsonArray& jarr, const bool mult) {
    for (const JsonObject& qualifiers : jarr) {
        const affected_stat as = affected_stat_from_string(qualifiers.get_string("stat"));
        if (as == affected_stat::NONE) { qualifiers.throw_error("Invalid affected stat", "stat"); }

        damage_type dt = DT_NULL;
        if (needs_damage_type(as)) {
            dt = dt_by_name(qualifiers.get_string("type"));
            if (dt == DT_NULL) { qualifiers.throw_error("Invalid damage type", "type"); }
        }

        const affected_type at(as, dt);

        auto& selected = mult ? bonuses_mult : bonuses_flat;
        selected[at].emplace_back(qualifiers);
    }
}

affected_type::affected_type(affected_stat s) { stat = s; }

affected_type::affected_type(affected_stat s, damage_type t) {
    stat = s;
    if (needs_damage_type(s)) {
        type = t;
    } else {
        type = DT_NULL;
    }
}

auto affected_type::operator<(const affected_type& other) const -> bool {
    return stat < other.stat || (stat == other.stat && type < other.type);
}
auto affected_type::operator==(const affected_type& other) const -> bool {
    // NOTE: Constructor has to ensure type is set to NULL for some stats
    return stat == other.stat && type == other.type;
}

auto bonus_container::get_flat(const Character& u, affected_stat stat, damage_type dt) const
    -> float {
    const affected_type type(stat, dt);
    const auto& iter = bonuses_flat.find(type);
    if (iter == bonuses_flat.end()) { return 0.0f; }

    float ret = 0.0f;
    for (const auto& es : iter->second) { ret += es.get(u); }

    return ret;
}
auto bonus_container::get_flat(const Character& u, affected_stat stat) const -> float {
    return get_flat(u, stat, DT_NULL);
}

auto bonus_container::get_mult(const Character& u, affected_stat stat, damage_type dt) const
    -> float {
    const affected_type type(stat, dt);
    const auto& iter = bonuses_mult.find(type);
    if (iter == bonuses_mult.end()) { return 1.0f; }

    float ret = 1.0f;
    for (const auto& es : iter->second) { ret *= es.get(u); }

    // Currently all relevant effects require non-negative values
    return std::max(0.0f, ret);
}
auto bonus_container::get_mult(const Character& u, affected_stat stat) const -> float {
    return get_mult(u, stat, DT_NULL);
}

auto bonus_container::get_description() const -> std::string {
    std::string dump;
    for (const auto& boni : bonuses_mult) {
        std::string type = string_from_affected_stat(boni.first.get_stat());

        if (needs_damage_type(boni.first.get_stat())) {
            //~ %1$s: damage type, %2$s: damage-related bonus name
            type = string_format(
                pgettext("type of damage", "%1$s %2$s"), name_by_dt(boni.first.get_damage_type()),
                type);
        }

        for (const auto& sf : boni.second) {
            if (sf.stat) {
                //~ %1$s: bonus name, %2$d: bonus percentage, %3$s: stat name
                dump += string_format(
                    pgettext("martial art bonus", "* %1$s: <stat>%2$d%%</stat> of %3$s"), type,
                    static_cast<int>(sf.scale * 100), string_from_scaling_stat(sf.stat));
            } else {
                //~ %1$s: bonus name, %2$d: bonus percentage
                dump += string_format(
                    pgettext("martial art bonus", "* %1$s: <stat>%2$d%%</stat>"), type,
                    static_cast<int>(sf.scale * 100));
            }
            dump += "\n";
        }
    }

    for (const auto& boni : bonuses_flat) {
        std::string type = string_from_affected_stat(boni.first.get_stat());

        if (needs_damage_type(boni.first.get_stat())) {
            //~ %1$s: damage type, %2$s: damage-related bonus name
            type = string_format(
                pgettext("type of damage", "%1$s %2$s"), name_by_dt(boni.first.get_damage_type()),
                type);
        }

        for (const auto& sf : boni.second) {
            if (sf.stat) {
                //~ %1$s: bonus name, %2$+d: bonus percentage, %3$s: stat name
                dump += string_format(
                    pgettext("martial art bonus", "* %1$s: <stat>%2$+d%%</stat> of %3$s"), type,
                    static_cast<int>(sf.scale * 100), string_from_scaling_stat(sf.stat));
            } else {
                //~ %1$s: bonus name, %2$+d: bonus value
                dump += string_format(
                    pgettext("martial art bonus", "* %1$s: <stat>%2$+d</stat>"), type,
                    static_cast<int>(sf.scale));
            }
            dump += "\n";
        }
    }

    return dump;
}

auto effect_scaling::get(const Character& u) const -> float {
    float bonus = 0.0f;
    switch (stat) {
        case STAT_STR:
            bonus = scale * u.get_str();
            break;
        case STAT_DEX:
            bonus = scale * u.get_dex();
            break;
        case STAT_INT:
            bonus = scale * u.get_int();
            break;
        case STAT_PER:
            bonus = scale * u.get_per();
            break;
        case STAT_NULL:
            bonus = scale;
        case NUM_STATS:
            break;
    }

    return bonus;
}
