#include "editor/mapgen_legend_text.h"

#include "item.h"
#include "map/field_type.h"
#include "map/mapdata.h"
#include "mtype.h"
#include "regional_settings.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "vehicle/veh_type.h"

#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#include <ranges>
#include <vector>

namespace editor {
namespace {

using json = nlohmann::ordered_json;

struct layer_text {
    std::string_view layer;
    const char* label;
    /// The member of an object mapping that names what it places; none: the object reads as its
    /// members.
    const char* key = nullptr;
};

constexpr auto layer_texts = std::array{
    layer_text{"terrain", translate_marker("Terrain")},
    layer_text{"furniture", translate_marker("Furniture")},
    layer_text{"fields", translate_marker("Fields"), "field"},
    layer_text{"npcs", translate_marker("NPCs"), "class"},
    layer_text{"signs", translate_marker("Signs"), "signage"},
    layer_text{"vendingmachines", translate_marker("Vending machines"), "item_group"},
    layer_text{"toilets", translate_marker("Toilets")},
    layer_text{"gaspumps", translate_marker("Gas pumps"), "fuel"},
    layer_text{"items", translate_marker("Item groups"), "item"},
    layer_text{"monsters", translate_marker("Monster groups"), "monster"},
    layer_text{"vehicles", translate_marker("Vehicles"), "vehicle"},
    layer_text{"item", translate_marker("Items"), "item"},
    layer_text{"artifact", translate_marker("Artifacts")},
    layer_text{"artifacts", translate_marker("Artifacts")},
    layer_text{"traps", translate_marker("Traps")},
    layer_text{"monster", translate_marker("Monsters"), "monster"},
    layer_text{"rubble", translate_marker("Rubble")},
    layer_text{"computers", translate_marker("Computers"), "name"},
    layer_text{"sealed_item", translate_marker("Sealed items"), "item"},
    layer_text{"nested", translate_marker("Nested chunks"), "chunks"},
    layer_text{"liquids", translate_marker("Liquids"), "liquid"},
    layer_text{"graffiti", translate_marker("Graffiti"), "text"},
    layer_text{"translate", translate_marker("Translate")},
    layer_text{"zones", translate_marker("Zones"), "type"},
    layer_text{"ter_furn_transforms", translate_marker("Transforms"), "transform"},
    layer_text{"faction_owner_character", translate_marker("Owner"), "id"},
    layer_text{"remove_all", translate_marker("Remove all")}};

auto text_of(const std::string_view layer) -> const layer_text* {
    const auto found = std::ranges::find(layer_texts, layer, &layer_text::layer);
    return found == layer_texts.end() ? nullptr : &*found;
}

/// What the default region puts most where `id` stands in for a choice of the region's.
template <typename Id>
auto likeliest(const Id& id, const std::map<Id, weighted_int_list<Id>>& choices)
    -> std::optional<Id> {
    const auto found = choices.find(id);
    if (found == choices.end() || found->second.empty()) { return std::nullopt; }
    return std::ranges::max(found->second, {}, &weighted_object<int, Id>::weight).obj;
}

auto region_choices() -> const region_terrain_and_furniture_settings* {
    const auto found = region_settings_map.find("default");
    return found == region_settings_map.end()
             ? nullptr
             : &found->second.region_terrain_and_furniture;
}

auto terrain_name(const std::string& id) -> std::optional<std::string> {
    const auto ter = ter_str_id(id);
    if (!ter.is_valid()) { return std::nullopt; }
    const auto* const region = region_choices();
    const auto chosen = region ? likeliest(ter.id(), region->terrain) : std::nullopt;
    return chosen ? string_format(_("%s (by region)"), chosen->obj().name()) : ter.obj().name();
}

auto furniture_name(const std::string& id) -> std::optional<std::string> {
    const auto furn = furn_str_id(id);
    if (!furn.is_valid()) { return std::nullopt; }
    const auto* const region = region_choices();
    const auto chosen = region ? likeliest(furn.id(), region->furniture) : std::nullopt;
    return chosen ? string_format(_("%s (by region)"), chosen->obj().name()) : furn.obj().name();
}

/// The game's name of `id` on `layer`; the id itself for what has none.
auto name_of(const std::string_view layer, const std::string& id) -> std::string {
    const auto named = [&]() -> std::optional<std::string> {
        if (layer == "terrain") { return terrain_name(id); }
        if (layer == "furniture") { return furniture_name(id); }
        if (layer == "traps" && trap_str_id(id).is_valid()) { return trap_str_id(id)->name(); }
        if (layer == "fields" && field_type_str_id(id).is_valid()) {
            return field_type_str_id(id)->get_name();
        }
        if ((layer == "item" || layer == "sealed_item") && itype_id(id).is_valid()) {
            return item::nname(itype_id(id));
        }
        if (layer == "monster" && mtype_id(id).is_valid()) { return mtype_id(id)->nname(); }
        if (layer == "vehicles" && vproto_id(id).is_valid()) { return _(vproto_id(id)->name); }
        return std::nullopt;
    }();
    return named.value_or(id);
}

struct choice {
    std::string name;
    int weight = 1;
};

struct gathered {
    std::vector<choice> choices;
    std::vector<std::string> ids;
};

auto add(gathered& into, const std::string& name, const int weight) -> void {
    const auto found = std::ranges::find(into.choices, name, &choice::name);
    if (found != into.choices.end()) {
        found->weight += weight;
    } else {
        into.choices.push_back({.name = name, .weight = weight});
    }
}

auto add_id(gathered& into, const std::string& id) -> void {
    if (!std::ranges::contains(into.ids, id)) { into.ids.push_back(id); }
}

/// An object's members as `key: value`, a string as it reads.
auto members_text(const json& value) -> std::string {
    return value.items() | std::views::transform([](const auto& member) {
               const auto& inner = member.value();
               return member.key() + ": "
                    + (inner.is_string() ? inner.template get<std::string>() : inner.dump());
           })
         | std::views::join_with(std::string{", "}) | std::ranges::to<std::string>();
}

auto gather(const std::string_view layer, const json& value, const int weight, gathered& into)
    -> void {
    if (value.is_string()) {
        const auto id = value.get<std::string>();
        add(into, name_of(layer, id), weight);
        add_id(into, id);
        return;
    }
    if (value.is_array()) {
        for (const auto& entry : value) {
            const auto weighted =
                entry.is_array() && entry.size() == 2 && entry[1].is_number_integer();
            gather(layer, weighted ? entry[0] : entry,
                   weight * (weighted ? entry[1].get<int>() : 1), into);
        }
        return;
    }
    if (!value.is_object()) {
        add(into, value.dump(), weight);
        return;
    }
    if (value.contains("param") && value["param"].is_string()) {
        add(into, string_format(_("parameter %s"), value["param"].get<std::string>()), weight);
        if (value.contains("fallback") && value["fallback"].is_string()) {
            add_id(into, value["fallback"].get<std::string>());
        }
        return;
    }
    if (value.contains("switch") && value["switch"].is_object() && value["switch"].contains("param")
        && value["switch"]["param"].is_string()) {
        add(into, string_format(_("by parameter %s"), value["switch"]["param"].get<std::string>()),
            weight);
        return;
    }
    const auto* const text = text_of(layer);
    if (text && text->key && value.contains(text->key)) {
        gather(layer, value[text->key], weight, into);
        return;
    }
    add(into, value.empty() ? std::string(_("yes")) : members_text(value), weight);
}

} // namespace

auto legend_layer_label(const std::string_view layer) -> std::string {
    const auto* const text = text_of(layer);
    return text ? std::string(_(text->label)) : std::string(layer);
}

auto legend_value(const std::string_view layer, const std::string_view value) -> legend_value_text {
    const auto parsed = json::parse(value, nullptr, false);
    if (parsed.is_discarded()) { return {.names = std::string(value), .ids = {}}; }
    auto into = gathered{};
    gather(layer, parsed, 1, into);
    // One choice is certain, whatever its weight.
    const auto weighed = into.choices.size() > 1;
    return {
        .names = into.choices | std::views::transform([&](const choice& entry) {
                     return weighed && entry.weight > 1
                              ? string_format("%s x%d", entry.name, entry.weight)
                              : entry.name;
                 })
               | std::views::join_with(std::string{", "}) | std::ranges::to<std::string>(),
        .ids =
            into.ids | std::views::join_with(std::string{", "}) | std::ranges::to<std::string>()};
}

auto shown_symbol(const std::string& symbol) -> std::string {
    return symbol == " " ? std::string("␣") : symbol;
}

} // namespace editor
