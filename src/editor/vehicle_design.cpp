#include "editor/vehicle_design.h"

#include "debug_log_capture.h"
#include "json.h"
#include "map/map.h"
#include "mod_manager.h"
#include "output.h"
#include "string_formatter.h"
#include "translations.h"
#include "units_utility.h"
#include "vehicle/veh_prototype_build.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <sstream>

namespace editor {
namespace {

constexpr auto design_source = "editor";

auto int_of(const nlohmann::ordered_json& value, const char* key) -> int {
    return value.contains(key) && value[key].is_number_integer() ? value[key].get<int>() : 0;
}

/// The part a `parts` value names: a part id, or an object with a `part` member.
auto part_named(const nlohmann::ordered_json& value) -> std::string {
    if (value.is_string()) { return value.get<std::string>(); }
    return value.is_object() && value.contains("part") && value["part"].is_string()
             ? value["part"].get<std::string>()
             : std::string{};
}

/// The prototype with its display `blueprint` drawn again from what it builds, unless a palette
/// reads it, so that it shows the parts it has.
auto redrawn(nlohmann::ordered_json prototype) -> nlohmann::ordered_json {
    if (!prototype.contains("blueprint") || prototype.contains("palette")) { return prototype; }
    const auto built = build_design(prototype.dump());
    if (built.blueprint) { prototype["blueprint"] = blueprint_rows(*built.blueprint); }
    return prototype;
}

auto read_text(const std::filesystem::path& path) -> std::string {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

/// Whether the file's values hold a vehicle prototype `id`.
auto defines(const std::filesystem::path& path, const std::string& id) -> bool {
    const auto text = read_text(path);
    // Most files name neither, and parsing them all would take long.
    if (text.find("\"vehicle\"") == std::string::npos
        || text.find('"' + id + '"') == std::string::npos) {
        return false;
    }
    const auto json = nlohmann::ordered_json::parse(text, nullptr, false);
    const auto names = [&](const nlohmann::ordered_json& value) {
        return value.is_object() && value.value("type", "") == "vehicle"
            && value.value("id", "") == id;
    };
    return json.is_array() ? std::ranges::any_of(json, names) : names(json);
}

} // namespace

auto design_parts(const nlohmann::ordered_json& prototype) -> std::vector<design_part> {
    auto result = std::vector<design_part>{};
    if (prototype.contains("blueprint") && prototype.contains("palette")
        && prototype["blueprint"].is_array() && prototype["palette"].is_object()) {
        const auto& origin = prototype.value("blueprint_origin", nlohmann::ordered_json::object());
        auto y = -int_of(origin, "y");
        for (const auto& row : prototype["blueprint"]) {
            auto x = -int_of(origin, "x");
            for (const auto symbol : row.get<std::string>()) {
                const auto key = std::string(1, symbol);
                if (prototype["palette"].contains(key)) {
                    for (const auto& [index, part] :
                         prototype["palette"][key].items() | std::views::enumerate) {
                        result.push_back(
                            {.mount = point(x, y),
                             .part = part_named(part.value()),
                             .pointer = string_format("/palette/%s/%d", key, index),
                             .from_palette = true});
                    }
                }
                ++x;
            }
            ++y;
        }
    }
    if (!prototype.contains("parts") || !prototype["parts"].is_array()) { return result; }
    for (const auto& [index, entry] : prototype["parts"] | std::views::enumerate) {
        const auto mount = point(int_of(entry, "x"), int_of(entry, "y"));
        if (entry.contains("part")) {
            result.push_back(
                {.mount = mount,
                 .part = part_named(entry),
                 .pointer = string_format("/parts/%d", index)});
        } else if (entry.contains("parts") && entry["parts"].is_array()) {
            for (const auto& [member, part] : entry["parts"] | std::views::enumerate) {
                result.push_back(
                    {.mount = mount,
                     .part = part_named(part),
                     .pointer = string_format("/parts/%d/parts/%d", index, member)});
            }
        }
    }
    return result;
}

namespace {

/// A prototype read and finalized from JSON as the loader does, with what it said.
struct read_design {
    vproto_id id;
    /// None when the JSON cannot be read as a prototype.
    std::optional<vehicle_prototype> prototype;
    std::vector<loader_message> messages;
};

auto read(const std::string_view text) -> read_design {
    auto result = read_design{};
    auto thrown = std::optional<std::string>{};
    const auto said = capture_each_debugmsg_during([&] {
        try {
            auto in = std::istringstream(std::string(text));
            auto jsin = JsonIn(in, design_source);
            const auto entry = jsin.get_object();
            entry.get_string("type");
            result.id = vproto_id(entry.get_string("id"));
            auto& proto = result.prototype.emplace();
            proto.name = entry.get_string("name", "");
            read_vehicle_prototype(entry, proto);
            finalize_vehicle_prototype(result.id, proto);
        } catch (const JsonError& e) { thrown = e.what(); }
    });
    result.messages =
        said | std::views::transform([](const std::string& message) {
            return loader_message{.text = message};
        })
        | std::ranges::to<std::vector>();
    if (thrown) {
        result.messages.push_back({.text = *thrown});
        result.prototype.reset();
    }
    return result;
}

} // namespace

auto build_design(const std::string_view text) -> built_design {
    auto design = read(text);
    return {.blueprint = design.prototype ? std::move(design.prototype->blueprint) : nullptr,
            .messages = std::move(design.messages)};
}

auto apply_design(const std::string_view text) -> std::expected<vproto_id, std::string> {
    auto design = read(text);
    if (!design.prototype || !design.messages.empty()) {
        return std::unexpected(
            _("The game cannot build this prototype; fix what it reports first."));
    }
    register_vehicle_prototype(design.id, std::move(*design.prototype));
    return design.id;
}

auto spawn_design(const vproto_id& id, const tripoint_bub_ms& at, const spawn_options& options)
    -> vehicle* {
    return get_map().add_vehicle(
        id, at, units::from_degrees(options.facing), options.fuel, options.condition, false);
}

auto install_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, const part_install& install)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto id = vpart_id(install.part);
    if (!id.is_valid()) {
        return std::unexpected(string_format(_("%s is no vehicle part."), install.part));
    }
    if (!blueprint.can_mount(tripoint_mnt_veh(install.mount.x, install.mount.y, 0), id)) {
        return std::unexpected(string_format(
            _("%s cannot be mounted at %d,%d."), id->name(), install.mount.x, install.mount.y));
    }
    auto next = prototype;
    auto& parts = next["parts"];
    if (!parts.is_array()) { parts = nlohmann::ordered_json::array(); }
    // The parts go after those already at the mount, in the entry that lists them.
    const auto found = std::ranges::find_if(parts | std::views::reverse, [&](const auto& entry) {
        return int_of(entry, "x") == install.mount.x && int_of(entry, "y") == install.mount.y
            && int_of(entry, "z") == 0 && entry.contains("parts") && entry["parts"].is_array();
    });
    if (found != (parts | std::views::reverse).end()) {
        (*found)["parts"].push_back(install.part);
    } else {
        parts.push_back(
            {{"x", install.mount.x},
             {"y", install.mount.y},
             {"parts", nlohmann::ordered_json::array({install.part})}});
    }
    return redrawn(std::move(next));
}

auto remove_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, const std::size_t index)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto parts = design_parts(prototype);
    if (index >= parts.size()) { return std::unexpected(_("There is no such part.")); }
    const auto& removed = parts[index];
    if (removed.from_palette) {
        return std::unexpected(_("The blueprint's palette places this part; edit the palette."));
    }
    // The blueprint holds the parts in the order they are listed when each of them was mounted.
    auto reason = std::string{};
    if (blueprint.part_count() == static_cast<int>(parts.size())
        && !blueprint.can_unmount(static_cast<int>(index), reason)) {
        return std::unexpected(remove_color_tags(reason));
    }
    auto next = prototype;
    auto entry = 0;
    auto member = 0;
    if (std::sscanf(removed.pointer.c_str(), "/parts/%d/parts/%d", &entry, &member) == 2) {
        auto& listed = next["parts"][entry]["parts"];
        listed.erase(static_cast<std::size_t>(member));
        if (listed.empty()) { next["parts"].erase(static_cast<std::size_t>(entry)); }
    } else if (std::sscanf(removed.pointer.c_str(), "/parts/%d", &entry) == 1) {
        next["parts"].erase(static_cast<std::size_t>(entry));
    }
    return redrawn(std::move(next));
}

auto blueprint_rows(const vehicle& veh) -> std::vector<std::string> {
    auto mounts = std::vector<point>{};
    for (const auto& vp : veh.get_all_parts()) {
        if (!vp.part().removed) { mounts.push_back(vp.mount().xy().raw()); }
    }
    if (mounts.empty()) { return {}; }
    const auto [min_x, max_x] = std::ranges::minmax(mounts | std::views::transform(&point::x));
    const auto [min_y, max_y] = std::ranges::minmax(mounts | std::views::transform(&point::y));
    auto rows = std::vector<std::string>{};
    for (auto y = min_y; y <= max_y; ++y) {
        auto row = std::string{};
        for (auto x = min_x; x <= max_x; ++x) {
            const auto shown = veh.part_displayed_at(tripoint_mnt_veh(x, y, 0));
            row += shown < 0 ? ' ' : veh.part_sym(shown);
        }
        row.erase(row.find_last_not_of(' ') + 1);
        rows.push_back(std::move(row));
    }
    return rows;
}

auto design_stats(const vehicle& veh) -> std::vector<engine_client::panel_stat> {
    const auto speed = [](const int value) {
        return std::to_string(static_cast<int>(convert_velocity(value, VU_VEHICLE)));
    };
    auto cargo = units::volume{};
    for (const auto& vp : veh.get_any_parts("CARGO")) { cargo += veh.max_volume(vp.part_index()); }
    return {
        {.key = "speed",
         .label = _("Safe/Top speed"),
         .value = speed(veh.safe_ground_velocity(false, true)),
         .max = speed(veh.max_ground_velocity(false, true)),
         .unit = velocity_units(VU_VEHICLE)},
        {.key = "power",
         .label = _("Engine power"),
         .value = std::to_string(veh.ideal_engine_power() / 1000),
         .unit = "kW"},
        {.key = "mass",
         .label = _("Mass"),
         .value = string_format("%.0f", convert_weight(veh.total_mass())),
         .unit = weight_units()},
        {.key = "cargo_volume",
         .label = _("Cargo volume"),
         .value = format_volume(cargo),
         .unit = volume_units_abbr()},
    };
}

auto prototype_file(const std::string& id) -> std::optional<std::filesystem::path> {
    const auto mods =
        world_generator->active_world
            ? world_generator->active_world->info->active_mod_order
            : std::vector<mod_id>{mod_management::get_default_core_content_pack()};
    auto found = std::optional<std::filesystem::path>{};
    for (const auto& mod : mods) {
        if (!mod.is_valid()) { continue; }
        auto error = std::error_code{};
        for (const auto& file : std::filesystem::recursive_directory_iterator(mod->path, error)) {
            if (file.is_regular_file() && file.path().extension() == ".json"
                && defines(file.path(), id)) {
                found = file.path();
            }
        }
    }
    return found;
}

} // namespace editor
