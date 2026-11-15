#include "calendar.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "map/map.h"
#include "material.h"
#include "options.h"
#include "rng.h"
#include "rot.h"
#include "units_temperature.h"
#include "weather/weather.h"
#include "weather/weather_gen.h"

#include <algorithm>

namespace {

const auto itemcat_drugs = item_category_id("drugs");
const auto itemcat_food = item_category_id("food");

} // namespace

namespace item_internal {
bool goes_bad_temp_cache = false;
const item* goes_bad_temp_cache_for = nullptr;
inline bool goes_bad_cache_fetch() { return goes_bad_temp_cache; }
inline void goes_bad_cache_set(const item* i) {
    goes_bad_temp_cache = i->goes_bad();
    goes_bad_temp_cache_for = i;
}
inline void goes_bad_cache_unset() {
    goes_bad_temp_cache = false;
    goes_bad_temp_cache_for = nullptr;
}
inline bool goes_bad_cache_is_for(const item* i) { return goes_bad_temp_cache_for == i; }

struct scoped_goes_bad_cache {
    scoped_goes_bad_cache(item* i) { goes_bad_cache_set(i); }
    ~scoped_goes_bad_cache() { goes_bad_cache_unset(); }
};
} // namespace item_internal

bool item::goes_bad() const {
    if (item_internal::goes_bad_cache_is_for(this)) {
        return item_internal::goes_bad_cache_fetch();
    }
    if (has_flag(flag_PROCESSING)) { return false; }
    if (is_corpse()) {
        // Corpses rot only if they are made of rotting materials
        return made_of_any(materials::get_rotting());
    }
    return is_food() && get_comestible()->spoils != 0_turns;
}

bool item::goes_bad_after_opening(bool strict) const {
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
    for (const item* parent = parent_item(); parent != nullptr; parent = parent->parent_item()) {
        if (parent->type && parent->type->container && parent->type->container->preserves) {
            return true;
        }
    }
    return false;
}

auto item::is_in_sealing_container() const -> bool {
    for (const item* parent = parent_item(); parent != nullptr; parent = parent->parent_item()) {
        if (parent->type && parent->type->container && parent->type->container->seals) {
            return true;
        }
    }
    return false;
}

auto item::mark_rot_checked_now() -> void { last_rot_check = calendar::turn; }

time_duration item::get_shelf_life() const {
    if (goes_bad()) {
        if (is_food()) {
            return get_comestible()->spoils;
        } else if (is_corpse()) {
            return 24_hours;
        }
    }
    return 0_turns;
}

double item::get_relative_rot() const {
    // Components are frozen in time, when they are returned they are new items
    if (has_flag(flag_id("COMPONENT"))) { return rot / get_shelf_life(); }
    if (goes_bad()) {
        const_cast<item*>(this)->update_rot_from_location(temperature_flag::TEMP_NORMAL);
        return rot / get_shelf_life();
    }
    return 0;
}

void item::set_relative_rot(double val) {
    if (goes_bad()) {
        rot = get_shelf_life() * val;
        // calc_rot uses last_rot_check (when it's not turn_zero) instead of bday.
        // this makes sure the rotting starts from now, not from bday.
        // if this item is the result of smoking or milling don't do this, we want to start from
        // bday.
        if (!has_flag(flag_PROCESSING_RESULT)) { last_rot_check = calendar::turn; }
    }
}

void item::set_rot(time_duration val) { rot = val; }

int item::spoilage_sort_order() const {
    const item* subject;
    constexpr int bottom = std::numeric_limits<int>::max();

    if (type->container && !contents.empty()) {
        if (type->container->preserves) { return bottom - 3; }
        subject = &contents.front();
    } else {
        subject = this;
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
    // Avoid needlessly calculating already rotten things.  Corpses should
    // always rot away and food rots away at twice the shelf life.  If the food
    // is in a sealed container they won't rot away, this avoids needlessly
    // calculating their rot in that case.
    if (!is_corpse() && get_shelf_life() != 0_turns && rot / get_shelf_life() > 2.0) {
        return 0_seconds;
    }

    // rot modifier
    auto factor = get_option<float>("ROT_RATE");
    if (is_corpse() && has_flag(flag_FIELD_DRESS)) { factor *= 0.75f; }

    time_duration added_rot = 0_seconds;
    // simulation of different age of food at the start of the game and good/bad storage
    // conditions by applying starting variation bonus/penalty of +/- 20% of base shelf-life
    // positive = food was produced some time before calendar::start and/or bad storage
    // negative = food was stored in good conditions before calendar::start
    if (last_rot_check <= calendar::start_of_cataclysm) {
        time_duration spoil_variation = get_shelf_life() * 0.2f;
        added_rot += rng(-spoil_variation, spoil_variation);
    }
    time_duration time_delta = time - last_rot_check;
    added_rot += factor * time_delta / 1_hours * get_hourly_rotpoints_at_temp(temp) * 1_turns;
    return added_rot;
}

void item::mod_last_rot_check(time_duration processing_duration) {
    if (!has_own_flag(flag_PROCESSING)) {
        debugmsg("mod_last_rot_check called on non smoking item: %s", tname());
        return;
    }

    // Apply no rot while smoking
    last_rot_check += processing_duration;
}

bool item::has_rotten_away() const {
    if (is_corpse() && !can_revive()) {
        return get_rot() > 10_days;
    } else {
        return is_food() && get_relative_rot() > 2.0;
    }
}

auto item::process_rot(detached_ptr<item>&& self, const absolute_rot_process_options& options)
    -> detached_ptr<item> {
    if (!self) { return std::move(self); }
    if (self->is_in_preserving_container()) {
        self->mark_rot_checked_now();
        return std::move(self);
    }

    self->update_rot(options.context);

    auto rotted_away = false;
    if (self->is_corpse() && !self->can_revive()) {
        rotted_away = self->rot > 10_days;
    } else {
        const auto shelf_life = self->get_shelf_life();
        rotted_away = self->is_food() && shelf_life != 0_turns && self->rot / shelf_life > 2.0;
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

static units::temperature clip_by_temperature_flag(
    units::temperature temperature, temperature_flag flag) {
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

namespace {

struct rot_location {
    tripoint_bub_ms pos;
    temperature_flag flag;
};

/// Where @p it rots and the storage flag there; unplaced items use @p fallback at the origin.
auto rot_location_of(const item& it, const temperature_flag fallback) -> rot_location {
    if (it.is_loaded() && it.has_position()) {
        return {.pos = it.bub_pos(), .flag = rot::temp::for_location(get_map(), it)};
    }
    return {.pos = tripoint_bub_ms::zero(), .flag = fallback};
}

/// Temperature that update_rot applies right now to an item rotting in @p context.
auto current_rot_temperature(const item::rot_context& context) -> units::temperature {
    const auto& weather = context.weather == nullptr ? get_weather() : *context.weather;
    return clip_by_temperature_flag(weather.get_temperature(context.position), context.temperature);
}

} // namespace

auto item::rot_temperature(const temperature_flag fallback) const -> units::temperature {
    const auto loc = rot_location_of(*this, fallback);
    return current_rot_temperature({.position = bub_to_abs(loc.pos), .temperature = loc.flag});
}

void item::update_rot_from_location(const temperature_flag temperature) {
    if (!goes_bad() || last_rot_check == calendar::turn) { return; }
    if (is_in_preserving_container()) {
        mark_rot_checked_now();
        return;
    }

    const auto loc = rot_location_of(*this, temperature);
    update_rot(loc.pos, loc.flag, get_weather());
}

auto item::update_rot(
    const tripoint_bub_ms& pos, const temperature_flag flag, const weather_manager& weather)
    -> void {
    update_rot({
        .position = bub_to_abs(pos),
        .temperature = flag,
        .weather = &weather,
        .local_temperature = g != nullptr && !g->new_game ? get_map().get_temperature(pos) : 0,
    });
}

auto item::update_rot(const rot_context& context) -> void {
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
    const auto temp = current_rot_temperature(context);

    auto time = last_rot_check;
    item_internal::scoped_goes_bad_cache _cache(this);

    if (now - time > 1_hours) {
        // This code is for items that were left out of reality bubble for long time

        const auto& wgen = weather.get_cur_weather_gen();
        const auto seed = g != nullptr ? g->get_seed() : 0;
        // It's a modifier, so we need to subtract 0_f
        const auto local_mod =
            units::from_fahrenheit(g != nullptr && g->new_game ? 0 : context.local_temperature)
            - 0_f;

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
            rot += calc_rot(time, env_temperature_clipped);
            last_rot_check = time;
        }
    }

    // Remaining <1 h from above
    // and items that are held near the player
    if (now - time > smallest_interval) {
        rot += calc_rot(now, temp);
        last_rot_check = now;
    }
}
