#include "calendar.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "locations.h"
#include "map/map.h"
#include "material.h"
#include "rng.h"
#include "rot.h"
#include "rot/rot_calculation.h"
#include "units_temperature.h"
#include "weather/weather.h"
#include "weather/weather_gen.h"

#include <algorithm>
#include <limits>
#include <optional>

namespace {

const auto itemcat_drugs = item_category_id("drugs");
const auto itemcat_food = item_category_id("food");

/// Perishability is intrinsic: neither processing nor ingredient ownership changes it.
auto shelf_life_of(const item& value) -> std::optional<rot::shelf_life> {
    if (value.is_corpse() ? !value.made_of_any(materials::get_rotting()) : !value.is_food()) {
        return std::nullopt;
    }
    return rot::shelf_life::from_duration(
        value.is_food() ? value.get_comestible()->spoils : 24_hours);
}

} // namespace

auto item::goes_bad() const -> bool {
    // Preserve this legacy processing predicate; shelf_life_of describes capability.
    return !has_flag(flag_PROCESSING) && shelf_life_of(*this).has_value();
}

auto item::goes_bad_after_opening(bool strict) const -> bool {
    // check if this item is explicitly a canning-type item: eg, it preserves contents
    if (strict) {
        if (type->container && type->container->preserves && !contents.empty()
            && contents.front().goes_bad()) {
            return true;
        } else {
            return false;
        }
    }

    return goes_bad()
        || (type->container && type->container->preserves && !contents.empty()
            && contents.front().goes_bad());
}

auto item::is_in_preserving_container() const -> bool {
    for (const auto* parent = parent_item(); parent != nullptr; parent = parent->parent_item()) {
        if (parent->type && parent->type->container && parent->type->container->preserves) {
            return true;
        }
    }
    return false;
}

auto item::is_in_sealing_container() const -> bool {
    for (const auto* parent = parent_item(); parent != nullptr; parent = parent->parent_item()) {
        if (parent->type && parent->type->container && parent->type->container->seals) {
            return true;
        }
    }
    return false;
}

auto item::mark_rot_checked_now() -> void { last_rot_check = calendar::turn; }

item::scoped_component_rot::scoped_component_rot(item* value, component_rot_state state)
    : target(state == component_rot_state::none ? nullptr : value) {
    if (target) { target->borrowed_rot_state = state; }
}

item::scoped_component_rot::~scoped_component_rot() {
    if (target) { target->borrowed_rot_state = component_rot_state::none; }
}

auto item::component_rot_status() const -> component_rot_state {
    auto state = component_rot_state::none;
    for (const auto* value = this; value != nullptr; value = value->parent_item()) {
        if (value->borrowed_rot_state != component_rot_state::none && !value->has_position()) {
            if (value->borrowed_rot_state == component_rot_state::snapshot) {
                return component_rot_state::snapshot;
            }
            state = component_rot_state::live;
        }
        const auto* parent = value->parent_item();
        const auto* location = value->loc ? value->loc : value->saved_loc;
        if (parent && dynamic_cast<const component_item_location*>(location)) {
            if (!parent->is_craft()) { return component_rot_state::snapshot; }
            state = component_rot_state::live;
        }
    }
    return state;
}

auto item::rot_is_suspended() const -> bool {
    return has_flag(flag_PROCESSING) || component_rot_status() == component_rot_state::snapshot;
}

auto item::restart_rot_after_snapshot() -> void {
    mark_rot_checked_now();
    // Processing flags in a completed record describe history, not a running machine.
    item_tags.erase(flag_PROCESSING);
    // Contents become live together.  Separate component records remain snapshots.
    for (auto* content : contents.all_items_ptr()) {
        content->mark_rot_checked_now();
        content->item_tags.erase(flag_PROCESSING);
    }
}

auto item::copy_rot_from(const item& source) -> void {
    rot = source.rot;
    last_rot_check = source.last_rot_check;
    if (source.component_rot_status() == component_rot_state::snapshot) {
        mark_rot_checked_now();
        // Copy construction is not complete yet, so do not run flag-change callbacks.
        item_tags.erase(flag_PROCESSING);
    }
}

auto item::get_shelf_life() const -> time_duration {
    const auto lifetime = shelf_life_of(*this);
    return lifetime ? lifetime->duration() : 0_turns;
}

auto item::get_relative_rot() const -> double {
    const auto lifetime = shelf_life_of(*this);
    if (!lifetime) { return 0.0; }
    const auto state = component_rot_status();
    // Retain the old getter-only behavior for loose legacy COMPONENT flags, but
    // in-progress craft materials are live even if add_component applied that flag.
    const auto legacy_record = state == component_rot_state::none && has_flag(flag_id("COMPONENT"));
    if (state != component_rot_state::snapshot && !legacy_record) {
        const_cast<item*>(this)->update_rot_from_location(temperature_flag::TEMP_NORMAL);
    }
    return lifetime->relative_rot(rot);
}

auto item::is_fresh() const -> bool { return goes_bad() && get_relative_rot() < 0.1; }

auto item::is_going_bad() const -> bool { return get_relative_rot() > 0.9; }

auto item::rotten() const -> bool { return get_relative_rot() > 1.0; }

auto item::get_rot() const -> time_duration {
    const_cast<item*>(this)->update_rot_from_location(temperature_flag::TEMP_NORMAL);
    return rot;
}

auto item::mod_rot(const time_duration& val) -> void { rot = rot::accumulate(rot, val); }

auto item::set_relative_rot(double val) -> void {
    const auto lifetime = shelf_life_of(*this);
    if (!lifetime || has_flag(flag_PROCESSING)) { return; }
    const auto value = lifetime->from_relative(val);
    if (!value) {
        debugmsg("Invalid relative rot: %g", val);
        return;
    }
    rot = *value;
    // Processing results start at their production time, not at inspection time.
    if (!has_flag(flag_PROCESSING_RESULT)) { mark_rot_checked_now(); }
}

auto item::set_rot(time_duration val) -> void { rot = val; }

auto item::spoilage_sort_order() const -> int {
    const auto* subject = this;
    constexpr auto bottom = std::numeric_limits<int>::max();

    if (type->container && !contents.empty()) {
        if (type->container->preserves) { return bottom - 3; }
        subject = &contents.front();
    }

    if (subject->goes_bad()) { return to_turns<int>(subject->get_shelf_life() - subject->rot); }

    if (subject->get_comestible()) {
        if (subject->get_category().get_id() == itemcat_food) {
            return bottom - 3;
        } else if (subject->get_category().get_id() == itemcat_drugs) {
            return bottom - 2;
        } else {
            return bottom - 1;
        }
    }
    return bottom;
}

auto item::calc_rot(time_point time, const units::temperature temp) const -> time_duration {
    const auto lifetime = shelf_life_of(*this);
    if (!lifetime || rot_is_suspended() || is_in_preserving_container()) { return 0_turns; }
    return calc_rot_interval(time, temp, *lifetime);
}

auto item::calc_rot_interval(
    time_point time, units::temperature temp, const rot::shelf_life& lifetime) const
    -> time_duration {
    // Sealed food past twice its shelf life needs no further aging; corpses do.
    if (!is_corpse() && lifetime.relative_rot(rot) > 2.0) { return 0_turns; }
    auto variation = 0_turns;
    // Preserve the first-update RNG draw and its +/-20% starting freshness range.
    if (last_rot_check <= calendar::start_of_cataclysm) {
        const auto bound = lifetime.duration() * 0.2f;
        variation = rng(-bound, bound);
    }
    return rot::increment({
        .elapsed = time - last_rot_check,
        .temperature = temp,
        .field_dressed_corpse = is_corpse() && has_flag(flag_FIELD_DRESS),
        .initial_variation = variation,
    });
}

namespace {

auto temperature_flag_to_highest_temperature(temperature_flag temperature) -> units::temperature {
    switch (temperature) {
        case temperature_flag::TEMP_NORMAL:
        case temperature_flag::TEMP_HEATER:
            return units::temperature_max;
        case temperature_flag::TEMP_FRIDGE:
            return temperatures::fridge;
        case temperature_flag::TEMP_FREEZER:
            return temperatures::freezer;
        case temperature_flag::TEMP_ROOT_CELLAR:
            return temperatures::root_cellar;
    }

    return units::temperature_max;
}

} // namespace


auto item::minimum_freshness_duration(temperature_flag temperature) const -> time_duration {
    if (is_in_preserving_container()) { return calendar::INDEFINITELY_LONG_DURATION; }
    const auto lifetime = shelf_life_of(*this);
    if (!lifetime || !type->comestible) { return calendar::INDEFINITELY_LONG_DURATION; }
    return lifetime->minimum_freshness(rot, temperature_flag_to_highest_temperature(temperature));
}

auto item::mod_last_rot_check(time_duration processing_duration) -> void {
    if (!has_own_flag(flag_PROCESSING)) {
        debugmsg("mod_last_rot_check called on non smoking item: %s", tname());
        return;
    }

    // Apply no rot while smoking
    last_rot_check += processing_duration;
}

auto item::has_rotten_away() const -> bool {
    if (!shelf_life_of(*this)) { return false; }
    if (is_corpse() && !can_revive()) {
        return get_rot() > 10_days;
    } else {
        return is_food() && get_relative_rot() > 2.0;
    }
}

auto item::process_rot(detached_ptr<item>&& self, const absolute_rot_process_options& options)
    -> detached_ptr<item> {
    if (!self) { return std::move(self); }
    const auto lifetime = shelf_life_of(*self);
    if (!lifetime || self->rot_is_suspended()) { return std::move(self); }
    if (self->is_in_preserving_container()) {
        self->mark_rot_checked_now();
        return std::move(self);
    }

    self->update_rot(options.context);

    auto rotted_away = false;
    if (self->is_corpse() && !self->can_revive()) {
        rotted_away = self->rot > 10_days;
    } else {
        rotted_away = self->is_food() && lifetime->relative_rot(self->rot) > 2.0;
    }
    if (rotted_away && options.carrier == nullptr && !options.seals) {
        return detached_ptr<item>();
    }
    return std::move(self);
}

auto item::process_rot(detached_ptr<item>&& self, const tripoint_bub_ms& pos)
    -> detached_ptr<item> {
    return process_rot(
        std::move(self), false, pos, nullptr, temperature_flag::TEMP_NORMAL, get_weather());
}

namespace {

auto clip_by_temperature_flag(units::temperature temperature, temperature_flag flag)
    -> units::temperature {
    switch (flag) {
        case temperature_flag::TEMP_NORMAL:
            // Just use the temperature normally
            return temperature;
        case temperature_flag::TEMP_FRIDGE:
            return std::min(temperature, temperatures::fridge);
        case temperature_flag::TEMP_FREEZER:
            return std::min(temperature, temperatures::freezer);
        case temperature_flag::TEMP_HEATER:
            return std::max(temperature, temperatures::normal);
        case temperature_flag::TEMP_ROOT_CELLAR:
            return temperatures::root_cellar;
        default:
            debugmsg("Temperature flag enum not valid: %d.  Using current temperature.",
                     static_cast<int>(flag));
            break;
    }
    return temperature;
}

} // namespace

auto item::update_rot_from_location(const temperature_flag temperature) -> void {
    if (!goes_bad() || last_rot_check == calendar::turn
        || component_rot_status() == component_rot_state::snapshot) {
        return;
    }
    if (is_in_preserving_container()) {
        mark_rot_checked_now();
        return;
    }

    auto pos = tripoint_bub_ms::zero();
    auto flag = temperature;
    if (is_loaded()) {
        pos = bub_pos();
        flag = rot::temp::for_location(get_map(), *this);
    }
    update_rot(pos, flag, get_weather());
}

auto item::update_rot(
    const tripoint_bub_ms& pos, const temperature_flag flag, const weather_manager& weather)
    -> void {
    update_rot({
        .position = bub_to_abs(pos),
        .temperature = flag,
        .weather = &weather,
        // bub_to_abs above already requires a live game/avatar.
        .local_temperature = !g->new_game ? get_map().get_temperature(pos) : 0,
    });
}

auto item::update_rot(const rot_context& context) -> void {
    const auto lifetime = shelf_life_of(*this);
    if (!lifetime || rot_is_suspended()) { return; }
    if (is_in_preserving_container()) {
        mark_rot_checked_now();
        return;
    }
    const auto now = calendar::turn;

    // if player debug menu'd the time backward it breaks stuff, just reset the
    // last_temp_check and last_rot_check in this case
    if (now - last_rot_check < 0_turns) {
        last_rot_check = now;
        return;
    }

    // process rot at most once every 100_turns (10 min)
    // note we're also gated by item::processing_speed
    static constexpr auto smallest_interval = 10_minutes;

    const auto& weather = context.weather == nullptr ? get_weather() : *context.weather;
    auto temp = weather.get_temperature(context.position);
    temp = clip_by_temperature_flag(temp, context.temperature);

    auto time = last_rot_check;

    if (now - time > 1_hours) {
        // This code is for items that were left out of reality bubble for long time

        const auto& wgen = weather.get_cur_weather_gen();
        // get_cur_weather_gen() above requires a live game.
        const auto seed = g->get_seed();
        // It's a modifier, so we need to subtract 0_f
        const auto local_mod =
            units::from_fahrenheit(g->new_game ? 0 : context.local_temperature) - 0_f;

        // Process the past of this item since the last time it was processed
        while (now - time > 1_hours) {
            // Get the environment temperature
            const auto time_delta = std::min(1_hours, now - 1_hours - time);
            time += time_delta;

            const auto env_temperature_raw = [&]() {
                if (context.position.z() >= 0) {
                    const auto weather_temperature = wgen.get_weather_temperature(
                        context.position, time, calendar::config, seed);
                    return weather_temperature + local_mod;
                }
                return temperatures::annual_average + local_mod;
            }();

            auto env_temperature_clipped =
                clip_by_temperature_flag(env_temperature_raw, context.temperature);

            // Calculate item rot
            rot = rot::accumulate(rot, calc_rot_interval(time, env_temperature_clipped, *lifetime));
            last_rot_check = time;
        }
    }

    // Remaining <1 h from above
    // and items that are held near the player
    if (now - time > smallest_interval) {
        rot = rot::accumulate(rot, calc_rot_interval(now, temp, *lifetime));
        last_rot_check = now;
    }
}
