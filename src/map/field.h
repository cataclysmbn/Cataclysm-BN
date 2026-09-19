#pragma once

#include "calendar.h"
#include "color.h"
#include "enums.h"
#include "field_type.h"
#include "type_id.h"

#include <map>
#include <string>
#include <vector>

/**
 * An active or passive effect existing on a tile.
 * Each effect can vary in intensity and age (usually used as a time to live).
 */
class field_entry {
public:
    field_entry(): type(fd_null), intensity(1), age(0_turns), is_alive(false) {}
    field_entry(const field_type_id& t, const int i, const time_duration& a)
        : type(t),
          intensity(i),
          age(a),
          is_alive(true) {}

    auto color() const -> nc_color;

    auto symbol() const -> std::string;

    // returns the move cost of this field
    auto move_cost() const -> int;

    auto extra_radiation_min() const -> int;
    auto extra_radiation_max() const -> int;
    auto radiation_hurt_damage_min() const -> int;
    auto radiation_hurt_damage_max() const -> int;
    auto radiation_hurt_message() const -> std::string;
    auto intensity_upgrade_chance() const -> int;
    auto intensity_upgrade_duration() const -> time_duration;
    auto monster_spawn_chance() const -> int;
    auto monster_spawn_count() const -> int;
    auto monster_spawn_radius() const -> int;
    auto monster_spawn_group() const -> mongroup_id;

    auto light_emitted() const -> float;
    auto local_light_override() const -> float;
    auto translucency() const -> float;
    auto is_transparent() const -> bool;
    auto convection_temperature_mod() const -> int;

    // Returns the field_type_id of the current field entry.
    auto get_field_type() const -> field_type_id;

    // Allows you to modify the field_type_id of the current field entry.
    // This probably shouldn't be called outside of field::replace_field, as it
    // breaks the field drawing code and field lookup
    auto set_field_type(const field_type_id& new_type) -> field_type_id;

    // Returns the maximum intensity of the current field entry.
    auto get_max_field_intensity() const -> int;
    // Returns the current intensity of the current field entry.
    auto get_field_intensity() const -> int;
    // Allows you to modify the intensity of the current field entry.
    auto set_field_intensity(int new_intensity) -> int;

    /// @returns @ref age.
    auto get_field_age() const -> time_duration;
    /// Sets @ref age to the given value.
    /// @returns New value of @ref age.
    auto set_field_age(const time_duration& new_age) -> time_duration;
    /// Adds given value to @ref age.
    /// @returns New value of @ref age.
    auto mod_field_age(const time_duration& mod_age) -> time_duration {
        return set_field_age(get_field_age() + mod_age);
    }

    // Returns if the current field is dangerous or not.
    auto is_dangerous() const -> bool { return type.obj().is_dangerous(); }

    // Returns the display name of the current field given its current intensity.
    // IE: light smoke, smoke, heavy smoke
    auto name() const -> std::string { return type.obj().get_name(intensity - 1); }

    // Returns true if this is an active field, false if it should be removed.
    auto is_field_alive() const -> bool { return is_alive; }

    auto gas_can_spread() const -> bool {
        return is_field_alive() && type.obj().phase == GAS && type.obj().percent_spread > 0;
    }

    auto get_underwater_age_speedup() const -> time_duration {
        return type.obj().underwater_age_speedup;
    }

    auto get_gas_absorption_factor() const -> int { return type.obj().gas_absorption_factor; }

    auto decays_on_actualize() const -> bool { return type.obj().accelerated_decay; }

    auto field_effects() const -> std::vector<field_effect>;

private:
    // The field identifier.
    field_type_id type;
    // The intensity (higher is stronger), of the field entry.
    int intensity;
    // The age, of the field effect. 0 is permanent.
    time_duration age;
    // True if this is an active field, false if it should be destroyed next check.
    bool is_alive;
};

/**
 * A variable sized collection of field entries on a given map square.
 * It contains one (at most) entry of each field type (e. g. one smoke entry and one
 * fire entry, but not two fire entries).
 * Use @ref find_field to get the field entry of a specific type, or iterate over
 * all entries via @ref begin and @ref end (allows range based iteration).
 * There is @ref displayed_field_type to specific which field should be drawn on the map.
 */
class field {
public:
    field();

    /**
     * Returns a field entry corresponding to the field_type_id parameter passed in.
     * If no fields are found then nullptr is returned.
     */
    auto find_field(const field_type_id& field_type_to_find) -> field_entry*;
    /**
     * Returns a field entry corresponding to the field_type_id parameter passed in.
     * If no fields are found then nullptr is returned.
     */
    auto find_field_c(const field_type_id& field_type_to_find) const -> const field_entry*;
    /**
     * Returns a field entry corresponding to the field_type_id parameter passed in.
     * If no fields are found then nullptr is returned.
     */
    auto find_field(const field_type_id& field_type_to_find) const -> const field_entry*;

    /**
     * Inserts the given field_type_id into the field list for a given tile if it does not already
     * exist. If you wish to modify an already existing field use find_field and modify the result.
     * Intensity defaults to 1, and age to 0 (permanent) if not specified.
     * The intensity is added to an existing field entry, but the age is only used for newly added
     * entries.
     * @return false if the field_type_id already exists, true otherwise.
     */
    auto add_field(
        const field_type_id& field_type_to_add, int new_intensity = 1,
        const time_duration& new_age = 0_turns) -> bool;

    /**
     * Removes the field entry with a type equal to the field_type_id parameter.
     * Make sure to decrement the field counter in the submap if (and only if) the
     * function returns true.
     * @return True if the field was removed, false if it did not exist in the first place.
     */
    auto remove_field(const field_type_id& field_to_remove) -> bool;
    /**
     * Make sure to decrement the field counter in the submap.
     * Removes the field entry, the iterator must point into @ref _field_type_list and must be
     * valid.
     */
    void remove_field(std::map<field_type_id, field_entry>::iterator);

    // Returns the number of fields existing on the current tile.
    auto field_count() const -> unsigned int;

    /**
     * Returns field type that should be drawn.
     */
    auto displayed_field_type() const -> field_type_id;

    auto displayed_description_affix() const -> description_affix;

    // Returns the vector iterator to begin searching through the list.
    auto begin() -> std::map<field_type_id, field_entry>::iterator;
    auto begin() const -> std::map<field_type_id, field_entry>::const_iterator;

    // Returns the vector iterator to end searching through the list.
    auto end() -> std::map<field_type_id, field_entry>::iterator;
    auto end() const -> std::map<field_type_id, field_entry>::const_iterator;

    /**
     * Returns the total move cost from all fields.
     */
    auto total_move_cost() const -> int;

private:
    // A pointer lookup table of all field effects on the current tile.
    std::map<field_type_id, field_entry> _field_type_list;
    //_displayed_field_type currently is equal to the last field added to the square. You can modify
    // this behavior in the class functions if you wish.
    field_type_id _displayed_field_type;
};
