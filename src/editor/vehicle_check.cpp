#include "editor/vehicle_check.h"

#include "flag.h"
#include "item.h"
#include "itype.h"
#include "string_formatter.h"
#include "translations.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <set>

namespace editor {
namespace {

const auto fault_starter = fault_id("fault_engine_starter");

/// The fuel `engine` burns when it is a liquid a tank holds; none for another engine.
auto liquid_fuel(const vpart_info& engine) -> std::optional<itype_id> {
    const auto& fuel = engine.fuel_type;
    return fuel.is_valid() && !fuel.is_null() && fuel->phase == LIQUID
             ? std::optional(fuel)
             : std::nullopt;
}

/// The indexes of the vehicle's parts that `wanted` takes.
auto parts_where(const vehicle& veh, const std::function<auto(const vehicle_part&)->bool>& wanted)
    -> std::vector<int> {
    return std::views::iota(0, veh.part_count()) | std::views::filter([&](const int index) {
               return !veh.cpart(index).removed && wanted(veh.cpart(index));
           })
         | std::ranges::to<std::vector>();
}

auto is_wheel(const vehicle_part& part) -> bool { return part.info().has_flag(VPFLAG_WHEEL); }

/// Whether the part goes on a frame by itself, needing no other part on its mount first.
auto self_standing(const vpart_info& part) -> bool {
    return std::ranges::none_of(part.get_flags(), [](const std::string& flag) {
        return !json_flag::get(flag).requires_flag().empty();
    });
}

/// The mounts of the vehicle that hold a frame, nearest its middle first.
auto frame_mounts(const vehicle& veh) -> std::vector<point> {
    auto mounts =
        mount_stacks(veh) | std::views::filter([&](const auto& entry) {
            return std::ranges::any_of(entry.second, [&](const int index) {
                return veh.part_info(index).location == "structure";
            });
        })
        | std::views::keys | std::ranges::to<std::vector>();
    if (mounts.empty()) { return mounts; }
    const auto [low_x, high_x] = std::ranges::minmax(mounts | std::views::transform(&point::x));
    const auto [low_y, high_y] = std::ranges::minmax(mounts | std::views::transform(&point::y));
    const auto twice = point(low_x + high_x, low_y + high_y);
    std::ranges::stable_sort(mounts, {}, [&](const point& at) {
        return std::abs(at.x * 2 - twice.x) + std::abs(at.y * 2 - twice.y);
    });
    return mounts;
}

/// The first of `mounts` that takes the part `id` as the game rules; none when none does.
auto taking(const vehicle& veh, const std::vector<point>& mounts, const vpart_id& id)
    -> std::optional<point> {
    const auto refusal = mount_refusals(veh, id);
    const auto found = std::ranges::find_if(mounts, [&](const point& at) { return !refusal(at); });
    return found == mounts.end() ? std::nullopt : std::optional(*found);
}

/// The corners of the vehicle's frame: the northmost and southmost frame of its front and back
/// columns.
auto corners(const vehicle& veh) -> std::vector<point> {
    const auto frames = frame_mounts(veh);
    if (frames.empty()) { return {}; }
    const auto [low_x, high_x] = std::ranges::minmax(frames | std::views::transform(&point::x));
    auto result = std::vector<point>{};
    for (const auto x : {high_x, low_x}) {
        const auto column =
            frames | std::views::filter([&](const point& at) { return at.x == x; })
            | std::views::transform(&point::y) | std::ranges::to<std::vector>();
        const auto [low_y, high_y] = std::ranges::minmax(column);
        for (const auto y : {low_y, high_y}) {
            if (!std::ranges::contains(result, point(x, y))) { result.emplace_back(x, y); }
        }
    }
    return result;
}

/// The paste that installs the part `id` on `mounts`, each after the part it needs there first.
auto with_prerequisites(
    const part_catalog& catalog, const vehicle& veh, const std::vector<point>& mounts,
    const vpart_id& id) -> paste_install {
    auto paste = paste_install{.layers = {{}, {}}};
    for (const auto& at : mounts) {
        if (const auto first = prerequisite_of(catalog, veh, {.mount = at, .part = id.str()})) {
            paste.layers[0].push_back({.mount = at, .part = first->str()});
        }
        paste.layers[1].push_back({.mount = at, .part = id.str()});
    }
    return paste;
}

/// The front wheels' hubs that have a steerable kind, as indexes of the vehicle's parts.
auto front_hubs(const part_catalog& catalog, const vehicle& veh) -> std::vector<int> {
    const auto wheels = parts_where(veh, is_wheel);
    if (wheels.empty()) { return {}; }
    const auto front = std::ranges::max(
        wheels
        | std::views::transform([&](const int index) { return veh.cpart(index).mount.x(); }));
    return parts_where(veh, [&](const vehicle_part& part) {
        return part.mount.x() == front && !part.info().has_flag("STEERABLE") && !is_wheel(part)
            && std::ranges::any_of(veh.parts_at_relative(part.mount, false),
                                   [&](const int index) { return is_wheel(veh.cpart(index)); })
            && steerable_of(catalog, part.info()) != nullptr;
    });
}

auto check(
    const char* key, const bool ok, std::string text, const drive_fix fix, std::string action)
    -> drive_check {
    return {
        .key = key,
        .ok = ok,
        .text = std::move(text),
        .fix = ok ? drive_fix::none : fix,
        .action = ok ? std::string{} : std::move(action)};
}

} // namespace

auto drive_checks(const vehicle& veh, const std::function<auto(int)->itype_id>& held)
    -> std::vector<drive_check> {
    auto checks = std::vector<drive_check>{};
    const auto wheels = !veh.wheelcache.empty();
    checks.push_back(check(
        "wheels", veh.valid_wheel_config(),
        !wheels                          ? _("It has no wheels.")
        : !veh.sufficient_wheel_config() ? _("One wheel cannot hold it up.")
        : !veh.balanced_wheel_config()
            ? _("Its wheels do not hold up its middle: it needs wheels at both ends.")
            : _("It stands on its wheels."),
        drive_fix::add_wheels, _("Add wheels at the corners")));
    if (wheels) {
        const auto steers = veh.steering_effectiveness() > 0;
        checks.push_back(
            check("steering", steers, steers ? _("Its wheels steer.") : _("No wheel can steer."),
                  drive_fix::steer_front, _("Make the front wheels steerable")));
    }
    const auto engines = parts_where(veh, [](const vehicle_part& part) {
        return part.is_engine();
    });
    checks.push_back(check(
        "engine", !engines.empty(),
        engines.empty() ? _("It has no engine.") : _("It has an engine."), drive_fix::show_engines,
        _("Choose an engine")));
    // The liquids the engines burn, each once, and the tanks that hold one of them.
    auto fuels = std::vector<itype_id>{};
    for (const auto index : engines) {
        const auto fuel = liquid_fuel(veh.part_info(index));
        if (fuel && !std::ranges::contains(fuels, *fuel)) { fuels.push_back(*fuel); }
    }
    if (!fuels.empty()) {
        const auto fueled = parts_where(veh, [&](const vehicle_part& part) {
            return part.is_tank() && std::ranges::contains(fuels, held(veh.index_of_part(&part)));
        });
        const auto name = item::nname(fuels.front());
        checks.push_back(check(
            "fuel", !fueled.empty(),
            fueled.empty()
                //~ %s is the fuel a vehicle's engine burns, such as diesel.
                ? string_format(_("No tank holds %s for the engine."), name)
                //~ %s is the fuel a vehicle's engine burns, such as diesel.
                : string_format(_("A tank holds %s for the engine."), name),
            drive_fix::fill_tanks,
            //~ %s is the fuel a vehicle's engine burns, such as diesel.
            string_format(_("Fill the tanks with %s"), name)));
    }
    const auto starts = std::ranges::any_of(engines, [&](const int index) {
        const auto& part = veh.cpart(index);
        return part.faults_potential().contains(fault_starter)
            || part.info().fuel_type == itype_id("battery");
    });
    if (starts) {
        const auto battery =
            !parts_where(veh, [](const vehicle_part& part) { return part.is_battery(); }).empty();
        checks.push_back(check(
            "battery", battery,
            battery ? _("It has a battery for the engine.")
                    : _("The engine needs a battery to start."),
            drive_fix::add_battery, _("Add a battery")));
    }
    const auto controls = parts_where(veh, [&](const vehicle_part& part) {
        return part.info().has_flag("CONTROLS")
            && std::ranges::any_of(veh.parts_at_relative(part.mount, false), [&](const int index) {
                   return veh.cpart(index).is_seat();
               });
    });
    checks.push_back(check(
        "controls", !controls.empty(),
        controls.empty()
            ? _("No seat has controls to drive it from.")
            : _("A seat has controls to drive it from."),
        drive_fix::add_controls, _("Add a seat with controls")));
    if (!engines.empty() && veh.valid_wheel_config()) {
        const auto moves = veh.safe_ground_velocity(false, true) > 0;
        checks.push_back(check(
            "power", moves,
            moves ? _("The engine can move it.") : _("The engine is too weak to move it."),
            drive_fix::show_engines, _("Choose a stronger engine")));
    }
    return checks;
}

auto ready_to_drive(const std::vector<drive_check>& checks) -> bool {
    return std::ranges::all_of(checks, &drive_check::ok);
}

auto fixed_design(
    const nlohmann::ordered_json& prototype, const part_catalog& catalog, const drive_fix fix)
    -> std::expected<nlohmann::ordered_json, std::string> {
    auto built = build_design(prototype.dump());
    if (!built.blueprint) {
        return std::unexpected(
            _("The game cannot build this prototype; fix what it reports first."));
    }
    auto& veh = *built.blueprint;
    const auto pasted =
        [&](const paste_install& paste) -> std::expected<nlohmann::ordered_json, std::string> {
        return paste_parts(prototype, &veh, paste)
            .and_then([](const stroke_design& done)
                          -> std::expected<nlohmann::ordered_json, std::string> {
                if (done.tally.done == 0) { return std::unexpected(done.tally.reason); }
                return done.prototype;
            });
    };
    const auto nowhere = [](const vpart_info* part) {
        return std::unexpected(
            part ? string_format(_("No square of the frame takes %s."), part->name())
                 : std::string(_("The game has no such part.")));
    };
    switch (fix) {
        case drive_fix::none:
        case drive_fix::show_engines:
            return prototype;
        case drive_fix::add_wheels: {
            const auto* const wheel = most_installed(catalog, [](const vpart_info& part) {
                return part.has_flag(VPFLAG_WHEEL) && !part.has_flag("RAIL");
            });
            if (!wheel) { return nowhere(wheel); }
            return pasted(with_prerequisites(catalog, veh, corners(veh), wheel->get_id()));
        }
        case drive_fix::steer_front: {
            auto next = prototype;
            const auto hubs = front_hubs(catalog, veh);
            if (hubs.empty()) { return std::unexpected(_("No front wheel has a hub to steer.")); }
            for (const auto index : hubs) {
                const auto* const steerable = steerable_of(catalog, veh.part_info(index));
                auto swapped = set_part_member(
                    next,
                    {.index = static_cast<std::size_t>(index),
                     .key = "part",
                     .value = steerable->get_id().str()});
                if (!swapped) { return swapped; }
                next = std::move(*swapped);
            }
            return next;
        }
        case drive_fix::fill_tanks: {
            const auto engine = std::ranges::find_if(veh.engines, [&](const int index) {
                return liquid_fuel(veh.part_info(index)).has_value();
            });
            if (engine == veh.engines.end()) {
                return std::unexpected(_("No engine burns a liquid fuel."));
            }
            const auto fuel = *liquid_fuel(veh.part_info(*engine));
            const auto tanks = parts_where(veh, [](const vehicle_part& part) {
                return part.is_tank();
            });
            if (tanks.empty()) {
                const auto* const tank = most_installed(catalog, [](const vpart_info& part) {
                    return part.has_flag(VPFLAG_FLUIDTANK) && part.location == "fuel_source"
                        && self_standing(part);
                });
                const auto at =
                    tank ? taking(veh, frame_mounts(veh), tank->get_id()) : std::nullopt;
                if (!at) { return nowhere(tank); }
                auto next = pasted({.layers = {{{.mount = *at, .part = tank->get_id().str()}}}});
                if (!next) { return next; }
                const auto parts = design_parts(*next);
                const auto added = std::ranges::find_if(parts, [&](const design_part& entry) {
                    return entry.mount == *at && entry.part == tank->get_id().str();
                });
                return set_part_member(
                    *next,
                    {.index = static_cast<std::size_t>(added - parts.begin()),
                     .key = "fuel",
                     .value = fuel.str()});
            }
            auto next = prototype;
            for (const auto index : tanks) {
                auto filled = set_part_member(
                    next,
                    {.index = static_cast<std::size_t>(index), .key = "fuel", .value = fuel.str()});
                if (!filled) { return filled; }
                next = std::move(*filled);
            }
            return next;
        }
        case drive_fix::add_battery: {
            const auto* const battery = most_installed(catalog, [](const vpart_info& part) {
                return part.item.is_valid() && part.item->magazine
                    && part.item->magazine->type.contains(ammotype("battery"))
                    && !part.has_flag(VPFLAG_ENGINE) && self_standing(part);
            });
            const auto at =
                battery ? taking(veh, frame_mounts(veh), battery->get_id()) : std::nullopt;
            if (!at) { return nowhere(battery); }
            return pasted({.layers = {{{.mount = *at, .part = battery->get_id().str()}}}});
        }
        case drive_fix::add_controls: {
            const auto* const controls = most_installed(catalog, [](const vpart_info& part) {
                return part.has_flag("CONTROLS");
            });
            const auto* const seat = most_installed(catalog, [](const vpart_info& part) {
                return part.has_flag("SEAT") && part.has_flag(VPFLAG_BOARDABLE)
                    && self_standing(part);
            });
            if (!controls || !seat) { return nowhere(controls); }
            // A seat that has no controls takes them; else a frame takes a seat with them.
            const auto seats = parts_where(veh, [](const vehicle_part& part) {
                return part.is_seat();
            });
            const auto seated =
                seats | std::views::transform([&](const int index) {
                    return veh.cpart(index).mount.xy().raw();
                })
                | std::ranges::to<std::vector>();
            if (const auto at = taking(veh, seated, controls->get_id())) {
                return pasted({.layers = {{{.mount = *at, .part = controls->get_id().str()}}}});
            }
            const auto at = taking(veh, frame_mounts(veh), seat->get_id());
            if (!at) { return nowhere(seat); }
            return pasted(
                {.layers = {{{.mount = *at, .part = seat->get_id().str()}},
                            {{.mount = *at, .part = controls->get_id().str()}}}});
        }
    }
    return prototype;
}

} // namespace editor
