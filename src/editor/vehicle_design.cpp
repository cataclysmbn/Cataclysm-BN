#include "editor/vehicle_design.h"

#include "calendar.h"
#include "debug_log_capture.h"
#include "editor/history_label.h"
#include "editor/mapgen_check.h"
#include "editor/recent_documents.h"
#include "engine_client_vehicle_look.h"
#include "flag.h"
#include "fstream_utils.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "mod_manager.h"
#include "output.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "units_utility.h"
#include "vehicle/veh_prototype_build.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <tuple>

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

/// The prototype with its display `blueprint` drawn again from `veh`, the vehicle it builds,
/// unless a palette reads it, so that it shows the parts it has.
auto drawn(nlohmann::ordered_json prototype, const vehicle& veh) -> nlohmann::ordered_json {
    if (prototype.contains("blueprint") && !prototype.contains("palette")) {
        prototype["blueprint"] = blueprint_rows(veh);
    }
    return prototype;
}

/// The prototype `drawn` from the vehicle it builds; as it is when the game cannot build it.
auto redrawn(nlohmann::ordered_json prototype) -> nlohmann::ordered_json {
    if (!prototype.contains("blueprint") || prototype.contains("palette")) { return prototype; }
    const auto built = build_design(prototype.dump());
    return built.blueprint ? drawn(std::move(prototype), *built.blueprint) : prototype;
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

/// The JSON pointer of the innermost member of `value` whose text in `value.dump()` holds the byte
/// `offset` of it, or the byte right after it.
auto pointer_at(const nlohmann::ordered_json& value, const std::size_t offset) -> std::string {
    if (!value.is_structured()) { return {}; }
    // After the opening bracket; each member after the comma of the one before.
    auto at = std::size_t{1};
    for (const auto& entry : value.items()) {
        const auto start =
            at + (value.is_object() ? nlohmann::ordered_json(entry.key()).dump().size() + 1 : 0);
        const auto end = start + entry.value().dump().size();
        if (offset <= end) {
            auto key = std::string{};
            for (const auto ch : entry.key()) {
                key += ch == '~' ? std::string("~0")
                     : ch == '/' ? std::string("~1")
                                 : std::string(1, ch);
            }
            return "/" + key + (offset >= start ? pointer_at(entry.value(), offset - start) : "");
        }
        at = end + 1;
    }
    return {};
}

/// What the loader threw reading `text`, a prototype's JSON as `dump()` writes it, on one line:
/// the pointer of the member it stopped at and what it said there.
auto thrown_text(const std::string_view thrown, const std::string_view text) -> std::string {
    constexpr auto header = std::string_view("Json error: ");
    const auto at = thrown.find(header);
    // The place leads as "<source>:<line>:<column>: ".
    const auto colon = at == std::string_view::npos ? at : thrown.find(": ", at + header.size());
    if (colon == std::string_view::npos) { return plain_loader_text(thrown); }
    const auto place = thrown.substr(at + header.size(), colon - at - header.size());
    auto said = std::string(thrown.substr(colon + 2));
    // The text around the place is quoted after a blank line.
    said = said.substr(0, said.find("\n\n"));
    std::ranges::replace(said, '\n', ' ');
    auto column = 0;
    const auto mark = place.rfind(':');
    const auto read =
        mark != std::string_view::npos
        && std::from_chars(place.data() + mark + 1, place.data() + place.size(), column).ec
               == std::errc{};
    const auto parsed = nlohmann::ordered_json::parse(text, nullptr, false);
    const auto pointer =
        read && column > 0 && !parsed.is_discarded()
            ? pointer_at(parsed, static_cast<std::size_t>(column - 1))
            : std::string{};
    //~ %1$s is the JSON pointer of the member the loader stopped at, %2$s what it said.
    return pointer.empty() ? said : string_format(_("%1$s: %2$s"), pointer, said);
}

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
        result.messages.push_back({.text = thrown_text(*thrown, text)});
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

auto vehicle_state(const vehicle& veh) -> std::string { return serialize(veh); }

auto place_vehicle_state(const std::string& state, const tripoint_bub_ms& at)
    -> std::expected<vehicle*, std::string> {
    auto saved = nlohmann::ordered_json::parse(state, nullptr, false);
    if (!saved.is_object() || !saved.contains("parts") || !saved["parts"].is_array()) {
        return std::unexpected(_("That is no saved vehicle."));
    }
    const auto in_the_way = std::unexpected(_("Something is in the way of the vehicle there."));
    auto& here = get_map();
    auto* const copy = here.add_vehicle(
        vproto_id("none"), at, units::from_degrees(int_of(saved, "faceDir")), 0, 0, false);
    if (copy == nullptr) { return in_the_way; }
    // The copy keeps the place, world and clock it was placed in.
    const auto own = nlohmann::ordered_json::parse(serialize(*copy));
    for (const auto* const key : {"posx", "posy", "om_id", "dimension_id", "last_update_turn"}) {
        if (own.contains(key)) {
            saved[key] = own[key];
        } else {
            saved.erase(key);
        }
    }
    try {
        deserialize(*copy, saved.dump());
    } catch (const JsonError& error) {
        here.destroy_vehicle(copy);
        return std::unexpected(plain_loader_text(error.what()));
    }
    const auto blocked = std::ranges::any_of(copy->get_all_parts(), [&](const auto& part) {
        return !here.inbounds(part.pos()) || here.impassable(part.pos())
            || here.veh_at(part.pos()).has_value();
    });
    if (blocked) {
        here.destroy_vehicle(copy);
        return in_the_way;
    }
    here.add_vehicle_to_cache(copy);
    return copy;
}

auto restore_vehicle_state(vehicle& veh, const std::string& state)
    -> std::expected<vehicle*, std::string> {
    const auto at = veh.bub_ms_location();
    get_map().destroy_vehicle(&veh);
    return place_vehicle_state(state, at);
}

namespace {
/// Whether a frame of the vehicle is being taken off, as vehicle::is_structural_part_removed,
/// which lets the rest stay put.
auto frame_removed(const vehicle& veh) -> bool {
    return std::ranges::any_of(std::views::iota(0, veh.part_count()), [&](const int index) {
        return veh.cpart(index).removed && veh.part_info(index).location == "structure";
    });
}

/// The part a designer would mount on the vehicle, and what the vehicle's mounts share.
struct mount_check {
    const vehicle& veh;
    vpart_id id;
    /// Whether a frame of it is being taken off.
    bool removing = false;
};

/// Why the game would not mount the part at `at`, as the first rule `vehicle::can_mount` checks
/// that it breaks reads; none when it would mount it.
auto refused_mount(const mount_check& check, const point& at) -> std::optional<std::string> {
    const auto& [veh, id, removing] = check;
    const auto mount = tripoint_mnt_veh(at.x, at.y, 0);
    if (veh.can_mount(mount, id)) { return std::nullopt; }
    if (!id.is_valid()) { return string_format(_("%s is no vehicle part."), id.str()); }
    const auto& part = *id;
    const auto name = part.name();
    const auto here = veh.parts_at_relative(mount, false);
    const auto info = [&](const int index) -> const vpart_info& { return veh.part_info(index); };
    const auto square = [&](const point& step) {
        return tripoint_mnt_veh(at.x + step.x, at.y + step.y, 0);
    };
    constexpr auto around =
        std::array{point_zero, point_east, point_south, point_west, point_north};
    const auto touch_refusal = string_format(_("%s must touch the vehicle's frame."), name);
    if (part.has_flag("NOINSTALL")) { return string_format(_("%s cannot be installed."), name); }
    // The game's own order of its rules, so that the reason given is the one that refuses it.
    if (part.has_flag("EXTENDABLE")) {
        if (!here.empty()) { return string_format(_("%s needs an empty mount of its own."), name); }
        if (veh.part_count() > 0) {
            // It touches a frame or another extendable part, else the game would mount it.
            return touch_refusal;
        }
    }
    // As vehicle::has_structural_part: a frame, not a protrusion.
    const auto structural = [&](const point& step) {
        return std::ranges::any_of(veh.parts_at_relative(square(step), false), [&](const int index) {
            return info(index).location == "structure" && !info(index).has_flag("PROTRUSION");
        });
    };
    const auto touches =
        veh.part_count() == 0 || removing || std::ranges::any_of(around, structural);
    if (here.empty() && part.location != "structure") {
        // A frame here would be refused too, for not touching the vehicle's.
        return touches
                 ? string_format(_("%s goes on a frame: install a frame here first."), name)
                 : string_format(
                       _("%s goes on a frame, and a frame here would not touch the vehicle's "
                         "frame."),
                       name);
    }
    if (!here.empty()) {
        const auto& base = info(here.front());
        if (std::ranges::any_of(std::array{"ANIMAL_CTRL", "PROTRUSION", "NOCOLLIDE", "EXTENDABLE"},
                                [&](const char* const flag) { return base.has_flag(flag); })) {
            return string_format(_("%s fills this mount: nothing else goes on it."), base.name());
        }
    }
    for (const auto index : here) {
        const auto& other = info(index);
        if (other.get_id() == id) { return string_format(_("This mount already has %s."), name); }
        if (!part.location.empty() && part.location == other.location) {
            return string_format(
                _("%1$s goes where %2$s already is on this mount."), name, other.name());
        }
        if (part.has_flag("CARGO") && other.has_flag("CARGO")) {
            return string_format(_("This mount already holds cargo in %s."), other.name());
        }
    }
    if (!touches) { return touch_refusal; }
    if (auto conflict = std::string{}; veh.has_engine_conflict(&part, conflict)) {
        return string_format(
            _("%1$s cannot go with the vehicle's other %2$s engine."), name, conflict);
    }
    for (const auto& flag : part.get_flags()) {
        const auto needed = json_flag::get(flag).requires_flag();
        if (needed.empty() || std::ranges::any_of(here, [&](const int index) {
                return info(index).has_flag(needed);
            })) {
            continue;
        }
        // A part that has the flag, by name, for a designer who does not know the flag.
        const auto& all = vpart_info::get_all();
        const auto example = std::ranges::find_if(all, [&](const vpart_info& candidate) {
            return candidate.has_flag(needed) && !candidate.has_flag("NOINSTALL");
        });
        return example == all.end()
                 ? string_format(
                       _("%1$s needs a part with %2$s on this mount: install one here first."),
                       name, needed)
                 : string_format(
                       _("%1$s needs a part such as %2$s on this mount: install one here first."),
                       name, example->name());
    }
    // The first part on the mount that `has` takes, by name.
    const auto with_flag = [&](const auto& has) -> std::optional<std::string> {
        const auto found = std::ranges::find_if(here, [&](const int index) {
            return has(info(index));
        });
        return found == here.end() ? std::nullopt : std::optional(info(*found).name());
    };
    const auto mirror = [](const vpart_info& entry) {
        return entry.has_flag("VISION") && !entry.has_flag("CAMERA");
    };
    if (mirror(part)) {
        if (const auto blocking = with_flag([](const vpart_info& entry) {
                return entry.has_flag("OPAQUE");
            })) {
            return string_format(_("%1$s needs a clear view, which %2$s blocks."), name, *blocking);
        }
    }
    if (part.has_flag("OPAQUE")) {
        if (const auto seeing = with_flag(mirror)) {
            return string_format(_("%1$s would block the view of %2$s."), name, *seeing);
        }
    }
    if (part.has_flag("TURRET_MOUNT")) {
        if (const auto mount = with_flag([](const vpart_info& entry) {
                return entry.has_flag("TURRET_MOUNT");
            })) {
            return string_format(_("This mount already has a turret mount, %s."), *mount);
        }
    }
    if (part.has_flag("TURRET_CONTROLS") || part.has_flag("LASER_DESIGNATOR")) {
        if (const auto manual = with_flag([](const vpart_info& entry) {
                return entry.has_flag("MANUAL");
            })) {
            return string_format(_("%1$s cannot work %2$s, which is fired by hand."), name, *manual);
        }
    }
    if (here.empty()) { return string_format(_("%s cannot go on this mount."), name); }
    const auto names =
        here | std::views::transform([&](const int index) { return info(index).name(); })
        | std::ranges::to<std::vector>();
    return string_format(_("%1$s cannot share this mount with %2$s."), name, join(names, ", "));
}
} // namespace

auto mount_refusals(const vehicle& veh, const vpart_id& id)
    -> std::function<auto(const point&)->std::optional<std::string>> {
    // The game's rules read only the parts on the ground-level mount and on the four beside it,
    // besides what the whole vehicle shares, so that mounts whose five stacks are alike share
    // their answer.
    auto stacks = std::map<point, std::string>{};
    for (const auto& vp : veh.get_all_parts()) {
        if (!vp.part().removed && vp.mount().z() == 0) {
            stacks[vp.mount().xy().raw()] += vp.info().get_id().str() + ",";
        }
    }
    return [check = mount_check{.veh = veh, .id = id, .removing = frame_removed(veh)},
            stacks = std::move(stacks),
            known = std::map<std::string, std::optional<std::string>>{}](const point& at) mutable {
        auto key = std::string{};
        for (const auto& step :
             std::array{point_zero, point_east, point_south, point_west, point_north}) {
            const auto found = stacks.find(at + step);
            key += (found == stacks.end() ? std::string{} : found->second) + ";";
        }
        const auto [entry, added] = known.try_emplace(std::move(key));
        if (added) {
            entry->second = refused_mount(check, at).transform([](std::string text) {
                return capitalize_letter(text);
            });
        }
        return entry->second;
    };
}

auto mount_refusal(const vehicle& veh, const point& at, const vpart_id& id)
    -> std::optional<std::string> {
    return mount_refusals(veh, id)(at);
}

auto install_live_part(vehicle& veh, const part_install& install)
    -> std::expected<int, std::string> {
    const auto id = vpart_id(install.part);
    if (!id.is_valid()) {
        return std::unexpected(string_format(_("%s is no vehicle part."), install.part));
    }
    const auto mount = tripoint_mnt_veh(install.mount.x, install.mount.y, 0);
    if (auto refused = mount_refusal(veh, install.mount, id)) {
        return std::unexpected(std::move(*refused));
    }
    const auto index = veh.install_part(mount, id);
    if (index < 0) {
        return std::unexpected(string_format(
            _("%s cannot be mounted at %d,%d."), id->name(), install.mount.x, install.mount.y));
    }
    get_map().add_vehicle_to_cache(&veh);
    return index;
}

auto remove_live_part(vehicle& veh, const int index) -> std::expected<void, std::string> {
    if (index < 0 || index >= veh.part_count()) { return std::unexpected(_("No such part.")); }
    if (veh.part_count() < 2) { return std::unexpected(_("A vehicle keeps its last part.")); }
    auto reason = std::string{};
    if (!veh.can_unmount(index, reason)) { return std::unexpected(reason); }
    const auto mount = veh.part(index).mount;
    const auto at = veh.bub_part_location(index);
    veh.remove_part(index);
    veh.part_removal_cleanup();
    if (veh.parts_at_relative(mount, true).empty()) {
        get_map().clear_vehicle_point_from_cache(&veh, at);
    }
    return {};
}

auto set_part_health(vehicle& veh, const int index, const int percent)
    -> std::expected<void, std::string> {
    if (index < 0 || index >= veh.part_count()) { return std::unexpected(_("No such part.")); }
    if (percent < 0 || percent > 100) { return std::unexpected(_("That value is out of range.")); }
    auto& part = veh.part(index);
    veh.set_hp(part, part.info().durability * percent / 100);
    return {};
}

namespace {

/// The game's item types that `holds`, by name.
auto items_by_name(const std::function<auto(const itype&)->bool>& holds) -> std::vector<itype_id> {
    struct named {
        std::string name;
        itype_id id;
    };
    auto found =
        item_controller->all() | std::views::filter([&](const itype* type) { return holds(*type); })
        | std::views::transform([](const itype* type) {
              return named{.name = item::nname(type->get_id()), .id = type->get_id()};
          })
        | std::ranges::to<std::vector>();
    std::ranges::sort(found, {}, [](const named& entry) { return std::tie(entry.name, entry.id); });
    return found | std::views::transform(&named::id) | std::ranges::to<std::vector>();
}

/// The rounds of the ammunition types `types`, by name.
auto rounds_of(const std::set<ammotype>& types) -> std::vector<itype_id> {
    return items_by_name([&](const itype& type) {
        return type.ammo && types.contains(type.ammo->type);
    });
}

} // namespace

auto tank_fuels() -> std::vector<itype_id> {
    return items_by_name([](const itype& type) { return type.phase >= LIQUID; });
}

auto turret_rounds(const vpart_id& id) -> std::vector<itype_id> {
    if (!id.is_valid() || !id->has_flag("TURRET") || !id->item.is_valid() || !id->item->gun) {
        return {};
    }
    return rounds_of(id->item->gun->ammo);
}

auto part_fuels(const vehicle& veh, const int index) -> std::vector<itype_id> {
    if (index < 0 || index >= veh.part_count()) { return {}; }
    const auto& part = veh.cpart(index);
    if (!part.is_fuel_store(false) && !part.is_turret()) { return {}; }
    return part.is_tank() ? tank_fuels() : rounds_of(part.get_base().ammo_types());
}

/// The item variable of a part's base naming what the part held before it was emptied.
constexpr auto emptied_of = "editor_emptied_of";

auto set_part_fuel(vehicle& veh, const int index, const part_fuel& fuel)
    -> std::expected<void, std::string> {
    if (index < 0 || index >= veh.part_count()) { return std::unexpected(_("No such part.")); }
    auto& part = veh.part(index);
    if (!part.is_fuel_store(false) && !part.is_turret()) {
        return std::unexpected(_("This part holds no fuel."));
    }
    if (fuel.amount < 0) { return std::unexpected(_("That value is out of range.")); }
    // What it holds now outranks whatever it held before it was last emptied.
    const auto holding = part.ammo_remaining() > 0;
    if (holding) { part.get_base().erase_var(emptied_of); }
    if (fuel.amount == 0) {
        // What it held is what a later fill that names nothing fills it with.
        if (holding) { part.get_base().set_var(emptied_of, part.ammo_current().str()); }
        // A turret's rounds are its gun's, which the part does not empty.
        if (part.is_turret()) {
            part.get_base().ammo_unset();
        } else {
            part.ammo_unset();
        }
        return {};
    }
    // A part that holds nothing is filled with what it held last, else what it takes by default.
    const auto chosen = [](const itype_id& id) { return !id.is_empty() && !id.is_null(); };
    const auto named = itype_id(fuel.fuel);
    const auto emptied = itype_id(part.get_base().get_var(emptied_of, ""));
    const auto type =
        chosen(named)     ? named
        : holding         ? part.ammo_current()
        : chosen(emptied) ? emptied
        : part.is_tank()  ? part.info().fuel_type
        : chosen(part.info().default_ammo)
            ? part.info().default_ammo
            : part.get_base().ammo_default();
    if (!chosen(type)) {
        return std::unexpected(
            part.is_turret() ? _("Choose the Ammunition first.") : _("Choose the Fuel type first."));
    }
    // A tank holds a liquid, a battery, magazine or turret the ammunition its item takes.
    const auto holds =
        type.is_valid()
        && (part.is_tank() ? type->phase >= LIQUID
                           : type->ammo && part.get_base().ammo_types().contains(type->ammo->type));
    if (!holds || part.ammo_set(type, fuel.amount) < 0) {
        return std::unexpected(string_format(_("This part cannot hold %s."), type.str()));
    }
    part.get_base().erase_var(emptied_of);
    return {};
}

auto add_part_cargo(vehicle& veh, const int index, const std::string& item_id)
    -> std::expected<void, std::string> {
    if (index < 0 || index >= veh.part_count()) { return std::unexpected(_("No such part.")); }
    if (!veh.part_flag(index, VPFLAG_CARGO)) {
        return std::unexpected(_("This part holds no cargo."));
    }
    const auto type = itype_id(item_id);
    if (!type.is_valid()) { return std::unexpected(string_format(_("%s is no item."), item_id)); }
    const auto before = veh.get_items(index).size();
    veh.add_item(index, item::spawn(type, calendar::turn));
    if (veh.get_items(index).size() == before) {
        return std::unexpected(_("It does not fit there."));
    }
    return {};
}

namespace {

/// Lists each part after the parts already at its mount, in the last entry that lists them.
auto append_parts(nlohmann::ordered_json& prototype, const std::vector<part_install>& installs)
    -> void {
    auto& parts = prototype["parts"];
    if (!parts.is_array()) { parts = nlohmann::ordered_json::array(); }
    auto listed = std::map<point, std::size_t>{};
    for (auto index = std::size_t{0}; index < parts.size(); ++index) {
        const auto& entry = parts[index];
        if (int_of(entry, "z") == 0 && entry.contains("parts") && entry["parts"].is_array()) {
            listed[point(int_of(entry, "x"), int_of(entry, "y"))] = index;
        }
    }
    for (const auto& install : installs) {
        const auto [found, added] = listed.try_emplace(install.mount, parts.size());
        if (added) {
            parts.push_back(
                {{"x", install.mount.x},
                 {"y", install.mount.y},
                 {"parts", nlohmann::ordered_json::array()}});
        }
        parts[found->second]["parts"].push_back(install.part);
    }
}

/// Installs each part on its mount of `veh` that takes it, passing over them while one more
/// does; a mount that already has the part keeps it as it is, and `placed` hears of each part
/// that went on.
auto spread(
    vehicle& veh, const std::vector<part_install>& wanted,
    const std::function<void(const part_install&)>& placed) -> stroke_tally {
    const auto mount_of = [](const point& at) { return tripoint_mnt_veh(at.x, at.y, 0); };
    auto pending =
        wanted | std::views::filter([&](const part_install& install) {
            return std::ranges::
                none_of(veh.parts_at_relative(mount_of(install.mount), true), [&](const int index) {
                    return veh.part_info(index).get_id().str() == install.part;
                });
        })
        | std::ranges::to<std::vector>();
    auto tally = stroke_tally{};
    // The game refreshes the whole vehicle on each part it installs; a stroke refreshes it once,
    // and after each engine, whose exclusions `can_mount` reads from the refreshed vehicle.
    veh.suspend_refresh();
    for (auto spreading = true; spreading;) {
        spreading = false;
        auto left = std::vector<part_install>{};
        for (const auto& install : pending) {
            const auto id = vpart_id(install.part);
            const auto at = mount_of(install.mount);
            if (veh.install_part(at, id) >= 0) {
                if (id->has_flag(VPFLAG_ENGINE)) {
                    veh.enable_refresh();
                    veh.suspend_refresh();
                }
                placed(install);
                ++tally.done;
                spreading = true;
            } else {
                left.push_back(install);
            }
        }
        pending = std::move(left);
    }
    veh.enable_refresh();
    tally.refused = static_cast<int>(pending.size());
    if (!pending.empty()) {
        const auto& [at, part] = pending.front();
        const auto id = vpart_id(part);
        tally.reason =
            mount_refusal(veh, at, id)
                .value_or(
                    string_format(_("%s cannot be mounted at %d,%d."), id->name(), at.x, at.y));
    }
    return tally;
}

/// `spread` of each layer of the paste on `veh` in turn, their tallies added up.
auto spread_layers(
    vehicle& veh, const paste_install& paste,
    const std::function<void(const part_install&)>& placed)
    -> std::expected<stroke_tally, std::string> {
    for (const auto& install : paste.layers | std::views::join) {
        if (!vpart_id(install.part).is_valid()) {
            return std::unexpected(string_format(_("%s is no vehicle part."), install.part));
        }
    }
    auto total = stroke_tally{};
    for (const auto& layer : paste.layers) {
        const auto tally = spread(veh, layer, placed);
        total.done += tally.done;
        total.refused += tally.refused;
        if (total.reason.empty()) { total.reason = tally.reason; }
    }
    return total;
}

/// The stroke as a paste of one layer.
auto one_layer(const stroke_install& install) -> paste_install {
    return {
        .layers = {
            install.mounts | std::views::transform([&](const point& at) {
                return part_install{.mount = at, .part = install.part};
            })
            | std::ranges::to<std::vector>()}};
}

/// Takes the parts at the JSON pointers of `design_parts` out of the prototype, and the entries
/// they leave empty.
auto erase_parts(nlohmann::ordered_json& prototype, const std::vector<std::string>& pointers)
    -> void {
    struct listed_at {
        int entry = 0;
        int member = -1;
        auto operator<=>(const listed_at&) const = default; // *NOPAD*
    };
    auto places =
        pointers | std::views::transform([](const std::string& pointer) {
            auto at = listed_at{};
            std::sscanf(pointer.c_str(), "/parts/%d/parts/%d", &at.entry, &at.member);
            return at;
        })
        | std::ranges::to<std::vector>();
    // Last first, so that each place still names its part.
    std::ranges::sort(places, std::greater{});
    auto& parts = prototype["parts"];
    for (const auto& [entry, member] : places) {
        if (member < 0) {
            parts.erase(static_cast<std::size_t>(entry));
            continue;
        }
        auto& listed = parts[entry]["parts"];
        listed.erase(static_cast<std::size_t>(member));
        if (listed.empty()) { parts.erase(static_cast<std::size_t>(entry)); }
    }
}

/// Whether taking the mount `at` out of `mounts` leaves the mounts beside it in more than one
/// piece, joined side to side.
auto splits(const std::set<point>& mounts, const point& at) -> bool {
    auto beside =
        four_adjacent_offsets | std::views::transform([&](const point& step) { return at + step; })
        | std::views::filter([&](const point& next) { return mounts.contains(next); })
        | std::ranges::to<std::set>();
    if (beside.size() < 2) { return false; }
    // Spreads from one of them nearest first, so that a way around `at` close by ends it soon.
    auto reached = std::vector<point>{*beside.begin()};
    auto seen = std::set<point>{at, *beside.begin()};
    beside.erase(beside.begin());
    for (auto next = std::size_t{0}; next < reached.size() && !beside.empty(); ++next) {
        const auto from = reached[next];
        for (const auto& step : four_adjacent_offsets) {
            if (mounts.contains(from + step) && seen.insert(from + step).second) {
                beside.erase(from + step);
                reached.push_back(from + step);
            }
        }
    }
    return !beside.empty();
}

} // namespace

auto install_part(
    const nlohmann::ordered_json& prototype, const vehicle& blueprint, const part_install& install)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto id = vpart_id(install.part);
    if (!id.is_valid()) {
        return std::unexpected(string_format(_("%s is no vehicle part."), install.part));
    }
    if (auto refused = mount_refusal(blueprint, install.mount, id)) {
        return std::unexpected(std::move(*refused));
    }
    auto next = prototype;
    append_parts(next, {install});
    return redrawn(std::move(next));
}

auto install_parts(
    const nlohmann::ordered_json& prototype, vehicle* const blueprint,
    const stroke_install& install) -> std::expected<stroke_design, std::string> {
    return paste_parts(prototype, blueprint, one_layer(install));
}

auto paste_parts(
    const nlohmann::ordered_json& prototype, vehicle* const blueprint, const paste_install& paste)
    -> std::expected<stroke_design, std::string> {
    if (!blueprint) {
        return std::unexpected(
            _("The game cannot build this prototype; fix what it reports first."));
    }
    auto placed = std::vector<part_install>{};
    return spread_layers(
               *blueprint, paste, [&](const part_install& install) { placed.push_back(install); })
        .transform([&](const stroke_tally& tally) {
            auto next = prototype;
            append_parts(next, placed);
            return stroke_design{.prototype = redrawn(std::move(next)), .tally = tally};
        });
}

auto remove_parts(const nlohmann::ordered_json& prototype, const std::vector<point>& mounts)
    -> std::expected<stroke_design, std::string> {
    auto next = prototype;
    auto tally = stroke_tally{};
    for (;;) {
        const auto built = build_design(next.dump());
        const auto parts = design_parts(next);
        // The blueprint holds the parts in the order they are listed when each was mounted.
        if (!built.blueprint || built.blueprint->part_count() != static_cast<int>(parts.size())) {
            return std::unexpected(
                _("The game cannot build this prototype; fix what it reports first."));
        }
        const auto& veh = *built.blueprint;
        // The indexes of the parts on each mount, in the order the game installs them.
        auto stacks = std::map<point, std::vector<std::size_t>>{};
        for (const auto& [index, part] : parts | std::views::enumerate) {
            stacks[part.mount].push_back(static_cast<std::size_t>(index));
        }
        auto occupied = stacks | std::views::keys | std::ranges::to<std::set>();
        auto removed = std::vector<std::string>{};
        auto taken_from = std::vector<point>{};
        auto left = 0;
        auto reason = std::string{};
        for (const auto& at : mounts) {
            const auto found = stacks.find(at);
            if (found == stacks.end()) { continue; }
            const auto& here = found->second;
            const auto top = here.back();
            auto why = std::string{};
            if (parts[top].from_palette) {
                why = _("The blueprint's palette places this part; edit the palette.");
            } else if (parts.size() - removed.size() < 2) {
                why = _("A vehicle keeps its last part.");
            } else if (!veh.can_unmount(static_cast<int>(top), why)) {
                if (why.empty()) { why = _("The game keeps this part."); }
            } else if (here.size() == 1) {
                // Parts going together may split what each alone would not.
                if (splits(occupied, at)) {
                    why = _("Removing this part would split the vehicle.");
                } else {
                    occupied.erase(at);
                }
            }
            if (why.empty()) {
                removed.push_back(parts[top].pointer);
                taken_from.push_back(at);
            } else {
                left += static_cast<int>(here.size());
                if (reason.empty()) { reason = remove_color_tags(why); }
            }
        }
        if (removed.empty()) {
            tally.refused = left;
            tally.reason = reason;
            // The pass built what is left.
            return stroke_design{.prototype = drawn(std::move(next), veh), .tally = tally};
        }
        tally.done += static_cast<int>(removed.size());
        std::ranges::copy(taken_from, std::back_inserter(tally.removed));
        erase_parts(next, removed);
    }
}

auto install_live_parts(vehicle& veh, const stroke_install& install)
    -> std::expected<stroke_tally, std::string> {
    return paste_live_parts(veh, one_layer(install));
}

auto paste_live_parts(vehicle& veh, const paste_install& paste)
    -> std::expected<stroke_tally, std::string> {
    const auto tally = spread_layers(veh, paste, [](const part_install& /*install*/) {});
    if (tally && tally->done > 0) {
        get_map().add_vehicle_to_cache(&veh);
        // The parts went on while the vehicle did not refresh, so the map buffer indexed them
        // before their places were known.
        get_map().get_mapbuffer().refresh_vehicle_footprint(&veh);
    }
    return tally;
}

auto remove_live_parts(vehicle& veh, const std::vector<point>& removed) -> stroke_tally {
    auto mounts = removed;
    auto tally = stroke_tally{};
    for (auto removing = true; removing;) {
        removing = false;
        tally.refused = 0;
        tally.reason.clear();
        for (const auto& [at, given] : std::views::zip(mounts, removed)) {
            const auto here = veh.parts_at_relative(tripoint_mnt_veh(at.x, at.y, 0), false);
            if (here.empty()) { continue; }
            // Taking the part at 0,0 away moves every mount, which the anchor follows.
            const auto anchor = veh.pivot_anchor[0];
            const auto gone = remove_live_part(veh, here.back());
            if (!gone) {
                tally.refused += static_cast<int>(here.size());
                if (tally.reason.empty()) { tally.reason = remove_color_tags(gone.error()); }
                continue;
            }
            ++tally.done;
            tally.removed.push_back(given);
            removing = true;
            const auto moved = (anchor - veh.pivot_anchor[0]).raw().xy();
            for (auto& mount : mounts) { mount -= moved; }
        }
    }
    return tally;
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
    erase_parts(next, {removed.pointer});
    return redrawn(std::move(next));
}

auto set_part_member(const nlohmann::ordered_json& prototype, const part_member& change)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto parts = design_parts(prototype);
    if (change.index >= parts.size()) { return std::unexpected(_("There is no such part.")); }
    auto next = prototype;
    auto& entry = next[nlohmann::ordered_json::json_pointer(parts[change.index].pointer)];
    if (entry.is_string()) { entry = nlohmann::ordered_json{{"part", entry}}; }
    if (change.value) {
        entry[change.key] = *change.value;
    } else {
        entry.erase(change.key);
    }
    if (entry.size() == 1 && entry.contains("part")) {
        entry = nlohmann::ordered_json(entry["part"]);
    }
    const auto before = build_design(prototype.dump()).messages.size();
    if (const auto built = build_design(next.dump()); built.messages.size() > before) {
        return std::unexpected(built.messages.back().text);
    }
    return next;
}

auto move_part(const nlohmann::ordered_json& prototype, const std::size_t index, const int step)
    -> std::expected<nlohmann::ordered_json, std::string> {
    const auto parts = design_parts(prototype);
    if (index >= parts.size()) { return std::unexpected(_("There is no such part.")); }
    const auto& moved = parts[index];
    if (moved.from_palette) {
        return std::unexpected(_("The blueprint's palette places this part; edit the palette."));
    }
    const auto same_mount = [&](const std::size_t other) {
        return parts[other].mount == moved.mount;
    };
    const auto first_on_mount = [&](auto&& candidates) -> std::optional<std::size_t> {
        const auto found = std::ranges::find_if(candidates, same_mount);
        return found == std::ranges::end(candidates) ? std::nullopt : std::optional(*found);
    };
    const auto found =
        step < 0 ? first_on_mount(std::views::iota(std::size_t{0}, index) | std::views::reverse)
                 : first_on_mount(std::views::iota(index + 1, parts.size()));
    if (!found) { return std::unexpected(_("The part is already at that end of its mount.")); }
    const auto other = *found;
    if (parts[other].from_palette) {
        return std::unexpected(_("The blueprint's palette places this part; edit the palette."));
    }
    const auto listed = [](const design_part& part) {
        auto entry = 0;
        auto member = 0;
        return std::sscanf(part.pointer.c_str(), "/parts/%d/parts/%d", &entry, &member) == 2;
    };
    if (!listed(moved) || !listed(parts[other])) {
        return std::unexpected(_("Only parts listed in a mount's parts array move."));
    }
    auto next = prototype;
    const auto at = nlohmann::ordered_json::json_pointer(moved.pointer);
    const auto to = nlohmann::ordered_json::json_pointer(parts[other].pointer);
    std::swap(next[at], next[to]);
    const auto before = build_design(prototype.dump()).messages.size();
    if (const auto built = build_design(next.dump()); built.messages.size() > before) {
        return std::unexpected(built.messages.back().text);
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
            // What the part is drawn as when whole and closed, since that is the design.
            const auto* const part = shown < 0 ? nullptr : &veh.cpart(shown);
            row += !part ? ' '
                 : part->proxy_sym != '\0'
                     ? part->proxy_sym
                     : static_cast<char>(part->info().sym);
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

auto export_design(const vehicle& veh, const std::string& id) -> nlohmann::ordered_json {
    auto mounts = std::vector<point>{};
    auto stacks = std::vector<nlohmann::ordered_json>{};
    for (const auto& vp : veh.get_all_parts()) {
        const auto& part = vp.part();
        if (part.removed) { continue; }
        const auto& part_id =
            part.proxy_part_id.is_null() ? part.info().get_id() : part.proxy_part_id;
        const auto fuel = part.is_tank() ? part.ammo_current() : itype_id::NULL_ID();
        const auto named =
            fuel.is_null()
                ? nlohmann::ordered_json(part_id.str())
                : nlohmann::ordered_json{{"part", part_id.str()}, {"fuel", fuel.str()}};
        const auto mount = vp.mount().xy().raw();
        const auto found = std::ranges::find(mounts, mount);
        if (found == mounts.end()) {
            mounts.push_back(mount);
            stacks.push_back(nlohmann::ordered_json::array({named}));
        } else {
            stacks[found - mounts.begin()].push_back(named);
        }
    }
    auto parts = nlohmann::ordered_json::array();
    for (const auto& [mount, stack] : std::views::zip(mounts, stacks)) {
        parts.push_back({{"x", mount.x}, {"y", mount.y}, {"parts", stack}});
    }
    return {
        {"id", id},
        {"type", "vehicle"},
        {"name", veh.name},
        {"blueprint", blueprint_rows(veh)},
        {"parts", std::move(parts)}};
}

auto prototype_file(const std::string& id) -> std::optional<std::filesystem::path> {
    const auto mods = loaded_mods();
    auto found = std::optional<std::filesystem::path>{};
    for (const auto& mod : mods) {
        if (!mod.is_valid()) { continue; }
        auto error = std::error_code{};
        for (const auto& file : std::filesystem::recursive_directory_iterator(mod->path, error)) {
            if (file.is_regular_file() && file.path().extension() == ".json"
                && defines(file.path(), id)) {
                found = file.path().lexically_normal();
            }
        }
    }
    return found;
}

auto mount_stacks(const vehicle& veh) -> std::map<point, std::vector<int>> {
    auto stacks = std::map<point, std::vector<int>>{};
    for (const auto& vp : veh.get_all_parts()) {
        if (!vp.part().removed && vp.mount().z() == 0) {
            stacks[vp.mount().xy().raw()].push_back(static_cast<int>(vp.part_index()));
        }
    }
    return stacks;
}

auto mount_looks(const vehicle& veh, std::vector<int> indexes, const int facing)
    -> std::vector<engine_client::look> {
    const auto shown = indexes.empty() ? -1 : veh.part_displayed_at(veh.cpart(indexes[0]).mount);
    if (const auto top = std::ranges::find(indexes, shown); top != indexes.end()) {
        std::ranges::rotate(top, std::next(top), indexes.end());
    }
    return indexes | std::views::transform([&](const int index) {
               const auto turned = to_degrees(normalize(veh.cpart(index).direction)) + facing;
               return engine_client::world::vehicle_part_own_appearance(
                   veh, index, static_cast<int>(std::round(turned)) % 360);
           })
         | std::ranges::to<std::vector>();
}

namespace {

/// The name of the part `id`, or the id when the game has no such part.
auto part_name(const std::string& id) -> std::string {
    const auto part = vpart_id(id);
    return part.is_valid() ? part->name() : id;
}

/// The entries of `after` that `before` lacks, counting repeats, in `after`'s order.
template <typename T>
auto added(const std::vector<T>& before, const std::vector<T>& after) -> std::vector<T> {
    auto left = std::map<T, int>{};
    for (const auto& entry : before) { ++left[entry]; }
    auto result = std::vector<T>{};
    for (const auto& entry : after) {
        if (const auto found = left.find(entry); found != left.end() && found->second > 0) {
            --found->second;
        } else {
            result.push_back(entry);
        }
    }
    return result;
}

struct mounted_part {
    point mount;
    std::string part;
    auto operator==(const mounted_part&) const -> bool = default;                // *NOPAD*
    auto operator<=>(const mounted_part&) const -> std::weak_ordering = default; // *NOPAD*
};

auto mounted(const nlohmann::ordered_json& prototype) -> std::vector<mounted_part> {
    return design_parts(prototype) | std::views::transform([](const design_part& entry) {
               return mounted_part{.mount = entry.mount, .part = entry.part};
           })
         | std::ranges::to<std::vector>();
}

auto at_mount(const char* const format, const std::string& name, const point& at) -> std::string {
    return string_format(format, name, at.x, at.y);
}

/// The names of the parts, each once, in the order of the alphabet.
auto part_names(const std::vector<mounted_part>& parts) -> std::vector<std::string> {
    auto names =
        parts
        | std::views::transform([](const mounted_part& entry) { return part_name(entry.part); })
        | std::ranges::to<std::vector>();
    std::ranges::sort(names);
    names.erase(std::ranges::unique(names).begin(), names.end());
    return names;
}

} // namespace

auto item_spawn_name(const nlohmann::ordered_json& entry) -> std::string {
    auto names = std::vector<std::string>{};
    const auto add = [&](const nlohmann::ordered_json& value, const bool group) {
        const auto one = [&](const nlohmann::ordered_json& id) {
            if (!id.is_string()) { return id.dump(); }
            const auto type = itype_id(id.get<std::string>());
            return !group && type.is_valid() ? item::nname(type) : id.get<std::string>();
        };
        if (value.is_array()) {
            std::ranges::copy(value | std::views::transform(one), std::back_inserter(names));
        } else {
            names.push_back(one(value));
        }
    };
    if (entry.contains("items")) { add(entry["items"], false); }
    if (entry.contains("item_groups")) { add(entry["item_groups"], true); }
    return join(names, ", ");
}

auto named_together(const std::vector<std::string>& names) -> std::string {
    if (names.size() < 2) { return names.empty() ? std::string{} : names.front(); }
    const auto head = join(std::vector(names.begin(), names.end() - 1), ", ");
    //~ The last two of a list of names, such as "wheel hub assembly and wheel".
    return string_format(_("%1$s and %2$s"), head, names.back());
}

auto design_step_label(const document_edit& edit) -> std::string {
    if (!edit.before.is_object() || !edit.after.is_object()) { return step_label(edit); }
    const auto before = mounted(edit.before);
    const auto after = mounted(edit.after);
    const auto installed = added(before, after);
    const auto removed = added(after, before);
    if (installed.size() == 1 && removed.empty()) {
        return at_mount(_("Install %s at %d,%d"), part_name(installed[0].part), installed[0].mount);
    }
    if (removed.size() == 1 && installed.empty()) {
        return at_mount(_("Remove %s at %d,%d"), part_name(removed[0].part), removed[0].mount);
    }
    if (!installed.empty() && removed.empty()) {
        // The same parts on each mount, in the order they went on, as a wheel and its hub.
        auto stacks = std::map<point, std::vector<std::string>>{};
        for (const auto& entry : installed) {
            stacks[entry.mount].push_back(part_name(entry.part));
        }
        const auto& first = stacks.begin()->second;
        if (first.size() > 1
            && std::ranges::all_of(stacks | std::views::values, [&](const auto& names) {
                   return names == first;
               })) {
            return stacks.size() == 1
                     ? at_mount(_("Install %s at %d,%d"), named_together(first),
                                stacks.begin()->first)
                     : string_format(_("Install %1$s ×%2$d"), named_together(first), stacks.size());
        }
        const auto names = part_names(installed);
        if (names.size() == 1) {
            //~ A step of the vehicle editor that installed one part on many mounts: its name and
            //~ how many.
            return string_format(_("Install %1$s ×%2$d"), names[0], installed.size());
        }
        return string_format(
            vgettext("Install %d part", "Install %d parts", installed.size()), installed.size());
    }
    if (!removed.empty() && installed.empty()) {
        const auto names = part_names(removed);
        if (names.size() == 1) {
            //~ A step of the vehicle editor that removed one part from many mounts: its name and
            //~ how many.
            return string_format(_("Remove %1$s ×%2$d"), names[0], removed.size());
        }
        //~ A step of the vehicle editor that removed many parts: how many, and their names.
        return string_format(
            vgettext("Remove %1$d part (%2$s)", "Remove %1$d parts (%2$s)", removed.size()),
            removed.size(), join(names, ", "));
    }
    // The same parts on the same mounts, in another order.
    if (before != after && installed.empty() && removed.empty()) {
        const auto moved = std::ranges::mismatch(before, after).in1;
        return string_format(_("Reorder parts at %d,%d"), moved->mount.x, moved->mount.y);
    }
    // A part's own members, such as a tank's fuel.
    const auto parts = design_parts(edit.after);
    const auto changed = std::ranges::find_if(parts, [&](const design_part& entry) {
        const auto at = nlohmann::ordered_json::json_pointer(entry.pointer);
        return !edit.before.contains(at) || edit.before[at] != edit.after[at];
    });
    if (before == after && changed != parts.end()) {
        return at_mount(_("Set %s at %d,%d"), part_name(changed->part), changed->mount);
    }
    const auto spawns = [](const nlohmann::ordered_json& prototype) {
        const auto items = prototype.value("items", nlohmann::ordered_json::array());
        return items.is_array()
                 ? items | std::ranges::to<std::vector>()
                 : std::vector<nlohmann::ordered_json>{};
    };
    const auto spawned = added(spawns(edit.before), spawns(edit.after));
    const auto taken = added(spawns(edit.after), spawns(edit.before));
    const auto spawn_at = [](const nlohmann::ordered_json& entry) {
        return point(entry.value("x", 0), entry.value("y", 0));
    };
    if (spawned.size() == 1 && taken.empty()) {
        return at_mount(_("Add %s at %d,%d"), item_spawn_name(spawned[0]), spawn_at(spawned[0]));
    }
    if (taken.size() == 1 && spawned.empty()) {
        return at_mount(_("Remove %s at %d,%d"), item_spawn_name(taken[0]), spawn_at(taken[0]));
    }
    if (spawned.size() == 1 && taken.size() == 1) {
        return at_mount(
            _("Set chance of %s at %d,%d"), item_spawn_name(spawned[0]), spawn_at(spawned[0]));
    }
    return step_label(edit);
}

namespace {

/// What tells the shapes of one part apart from other parts, as the game's install menu groups
/// them: their name and item.
auto shape_key(const vpart_info& part) -> std::string {
    return part.name() + '\n' + part.item.str();
}

/// `shapes` with the one the game's prototypes install most first, the game's order kept among
/// equals.
auto most_used_first(std::vector<const vpart_info*> shapes, const std::map<vpart_id, int>& uses)
    -> std::vector<const vpart_info*> {
    std::ranges::stable_sort(shapes, std::greater{}, [&](const vpart_info* part) {
        const auto found = uses.find(part->get_id());
        return found == uses.end() ? 0 : found->second;
    });
    return shapes;
}

} // namespace

auto category_of(const vpart_info& part) -> part_category {
    const auto any = [&](const std::initializer_list<const char*> flags) {
        return std::ranges::any_of(flags, [&](const char* const flag) {
            return part.has_flag(flag);
        });
    };
    // A frame holds the rest up, as vehicle::has_structural_part takes it, and does nothing else.
    if (part.location == "structure"
        && !any({"PROTRUSION", "EXTENDABLE", "ENGINE", "ANIMAL_CTRL", "PROPELLER"})) {
        return part_category::frames;
    }
    if (part.location == "axle" || any({"WHEEL"})) { return part_category::wheels; }
    if (any({"TURRET", "TURRET_MOUNT", "TURRET_CONTROLS", "LASER_DESIGNATOR"})) {
        return part_category::weapons;
    }
    if (any({"ENGINE", "ALTERNATOR", "PROPELLER", "ANIMAL_CTRL"})) {
        return part_category::engines;
    }
    // Tools that work the ground or lift and appliances, however much they hold.
    if (any({"PLANTER", "PLOW", "REAPER", "ROCKWHEEL", "ROADHEAD", "CRANE", "EXTRA_DRAG", "FRIDGE",
             "FREEZER", "CRAFTER", "WORKBENCH", "AUTODOC", "AUTOCLAVE", "FAUCET", "SCOOP",
             "CAPTURE_MONSTER_VEH"})) {
        return part_category::other;
    }
    if (part.location == "fuel_source" || part.location == "on_battery_mount"
        || any({"FLUIDTANK", "BATTERY_MOUNT"}) || part.fuel_type == itype_id("battery")) {
        return part_category::fuel;
    }
    if (any({"SEAT", "SEATBELT", "BED", "CONTROLS", "CTRL_ELECTRONIC", "REMOTE_CONTROLS"})) {
        return part_category::seats;
    }
    if (any({"OPENABLE", "WINDOW", "WINDSHIELD", "CURTAIN"})) { return part_category::doors; }
    if (any({"LIGHT", "CONE_LIGHT", "WIDE_CONE_LIGHT", "CIRCLE_LIGHT", "DOME_LIGHT", "AISLE_LIGHT",
             "ATOMIC_LIGHT"})) {
        return part_category::lights;
    }
    if (any({"CARGO"})) { return part_category::storage; }
    return part_category::other;
}

auto category_name(const part_category category) -> std::string {
    switch (category) {
        case part_category::frames:
            return _("Frames");
        case part_category::wheels:
            return _("Wheels & hubs");
        case part_category::engines:
            return _("Engines & motors");
        case part_category::fuel:
            return _("Fuel & batteries");
        case part_category::seats:
            return _("Seats & controls");
        case part_category::doors:
            return _("Doors & windows");
        case part_category::lights:
            return _("Lights");
        case part_category::storage:
            return _("Storage");
        case part_category::weapons:
            return _("Weapons");
        case part_category::other:
            break;
    }
    return _("Other");
}

auto read_part_catalog() -> part_catalog {
    auto uses = std::map<vpart_id, int>{};
    auto vehicles = std::map<vpart_id, int>{};
    auto filled = std::map<itype_id, int>{};
    for (const auto& id : vehicle_prototype::get_all()) {
        auto installed = std::set<vpart_id>{};
        for (const auto& part : id->parts) {
            ++uses[part.part];
            installed.insert(part.part);
            if (!part.fuel.is_null()) { ++filled[part.fuel]; }
        }
        for (const auto& part : installed) { ++vehicles[part]; }
    }
    auto kinds = std::vector<part_kind>{};
    auto found = std::map<std::string, std::size_t>{};
    for (const auto& part : vpart_info::get_all()) {
        const auto [at, added] = found.try_emplace(shape_key(part), kinds.size());
        if (added) { kinds.emplace_back(); }
        kinds[at->second].shapes.push_back(&part);
    }
    for (auto& kind : kinds) {
        kind.shapes = most_used_first(std::move(kind.shapes), uses);
        kind.category = category_of(*kind.shapes.front());
    }
    std::ranges::stable_sort(kinds, {}, [](const part_kind& kind) {
        return std::tuple(kind.category, kind.shapes.front()->name());
    });
    auto fuels = filled | std::views::keys | std::ranges::to<std::vector>();
    std::ranges::stable_sort(fuels, std::greater{}, [&](const itype_id& id) { return filled[id]; });
    const auto frames =
        uses | std::views::filter([](const auto& entry) {
            return entry.first.is_valid() && entry.first->location == "structure"
                && !entry.first->has_flag("PROTRUSION");
        })
        | std::ranges::to<std::vector>();
    return {
        .kinds = std::move(kinds),
        .tank_fuels = std::move(fuels),
        .frame =
            frames.empty()
                ? std::nullopt
                : std::optional(
                      std::ranges::
                          max_element(frames, {}, [](const auto& entry) { return entry.second; })
                              ->first),
        .uses = std::move(uses),
        .vehicles = std::move(vehicles)};
}

auto most_installed(
    const part_catalog& catalog, const std::function<auto(const vpart_info&)->bool>& wanted)
    -> const vpart_info* {
    const auto candidates =
        vpart_info::get_all() | std::views::filter([&](const vpart_info& part) {
            return !part.has_flag("NOINSTALL") && wanted(part);
        })
        | std::views::transform([](const vpart_info& part) { return &part; })
        | std::ranges::to<std::vector>();
    if (candidates.empty()) { return nullptr; }
    const auto uses = [&](const vpart_info* part) {
        const auto found = catalog.uses.find(part->get_id());
        return found == catalog.uses.end() ? 0 : found->second;
    };
    // The first of the most installed, in the game's order.
    return *std::ranges::max_element(candidates, [&](const vpart_info* a, const vpart_info* b) {
        return uses(a) < uses(b);
    });
}

auto in_front_half(const vehicle& veh, const point& at) -> bool {
    const auto stacks = mount_stacks(veh);
    if (stacks.empty()) { return false; }
    const auto [low, high] = std::ranges::minmax(
        stacks | std::views::keys | std::views::transform(&point::x));
    return at.x * 2 > low + high;
}

auto prerequisite_of(const part_catalog& catalog, const vehicle& veh, const part_install& install)
    -> std::optional<vpart_id> {
    const auto id = vpart_id(install.part);
    if (!id.is_valid()) { return std::nullopt; }
    const auto here =
        veh.parts_at_relative(tripoint_mnt_veh(install.mount.x, install.mount.y, 0), false);
    const auto missing =
        id->get_flags() | std::views::transform([](const std::string& flag) {
            return json_flag::get(flag).requires_flag();
        })
        | std::views::filter([&](const std::string& needed) {
              return !needed.empty() && std::ranges::none_of(here, [&](const int index) {
                  return veh.part_info(index).has_flag(needed);
              });
          })
        | std::ranges::to<std::vector>();
    if (missing.empty()) { return std::nullopt; }
    const auto& needed = missing.front();
    const auto front = in_front_half(veh, install.mount);
    const auto refusal = [&](const vpart_info& part) {
        return mount_refusal(veh, install.mount, part.get_id());
    };
    const auto fits = [&](const vpart_info& part) {
        return part.has_flag(needed) && !refusal(part);
    };
    const auto* chosen = most_installed(catalog, [&](const vpart_info& part) {
        return part.has_flag("STEERABLE") == front && fits(part);
    });
    if (!chosen) { chosen = most_installed(catalog, fits); }
    return chosen ? std::optional(chosen->get_id()) : std::nullopt;
}

auto search_rank(const std::string& name, const std::string& id, const std::string& filter)
    -> std::optional<int> {
    const auto wanted = to_lower_case(trim(filter));
    const auto named = to_lower_case(name);
    if (named == wanted) { return 0; }
    auto found = named.find(wanted);
    if (found == std::string::npos) {
        return lcmatch(id, wanted) ? std::optional(3) : std::nullopt;
    }
    // A name that starts with it or has a word that does, as "vehicle tank" for "tank".
    for (; found != std::string::npos; found = named.find(wanted, found + 1)) {
        if (found == 0 || !std::isalnum(static_cast<unsigned char>(named[found - 1]))) { return 1; }
    }
    return 2;
}

auto part_catalog::shapes_of(const vpart_id& id) const -> std::vector<const vpart_info*> {
    const auto found = std::ranges::find_if(kinds, [&](const part_kind& kind) {
        return std::ranges::contains(kind.shapes, id, &vpart_info::get_id);
    });
    return found == kinds.end() ? std::vector<const vpart_info*>{} : found->shapes;
}

auto part_labels(const std::vector<const vpart_info*>& parts) -> std::vector<std::string> {
    auto named = std::map<std::string, std::vector<std::string>>{};
    for (const auto* const part : parts) { named[part->name()].push_back(part->get_id().str()); }
    /// What tells the id of `part` from the others of its name: the rest of it after what their
    /// ids share up to its last `_`, less the whole words its name says already when `terse`.
    const auto apart = [&](const vpart_info& part, const bool terse) {
        const auto& twins = named[part.name()];
        auto shared = std::string_view(twins.front());
        for (const auto& other : twins) {
            shared = shared.substr(
                0, static_cast<std::size_t>(
                       std::ranges::mismatch(shared, other).in1 - shared.begin()));
        }
        shared = shared.substr(0, shared.rfind('_') + 1);
        const auto rest = part.get_id().str().substr(shared.size());
        if (!terse) { return rest; }
        auto spaced = to_lower_case(part.name());
        std::ranges::replace_if(
            spaced, [](const unsigned char letter) { return !std::isalnum(letter); }, ' ');
        const auto said = string_split(spaced, ' ');
        return join(
            rest | std::views::split('_') | std::views::transform([](const auto& word) {
                return std::string(std::string_view(word));
            }) | std::views::filter([&](const std::string& word) {
                return !std::ranges::contains(said, word);
            }) | std::ranges::to<std::vector>(),
            "_");
    };
    const auto label = [](const vpart_info& part, const std::string& rest) {
        //~ A vehicle part's name, and what tells it from another part of that name, such
        //~ as "frame (horizontal)".
        return rest.empty() ? part.name() : string_format(_("%1$s (%2$s)"), part.name(), rest);
    };
    const auto terse =
        parts | std::views::transform([&](const vpart_info* part) {
            return named[part->name()].size() < 2 ? part->name() : label(*part, apart(*part, true));
        })
        | std::ranges::to<std::vector>();
    auto counts = std::map<std::string, int>{};
    for (const auto& text : terse) { ++counts[text]; }
    // Terse unless that makes two of one name read the same.
    return std::views::zip(parts, terse) | std::views::transform([&](const auto& entry) {
               const auto& [part, text] = entry;
               return counts[text] > 1 ? label(*part, apart(*part, false)) : text;
           })
         | std::ranges::to<std::vector>();
}

} // namespace editor
