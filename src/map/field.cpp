#include "field.h"

#include "calendar.h"
#include "int_id.h"

#include <algorithm>
#include <utility>

auto field_entry::move_cost() const -> int { return type.obj().get_move_cost(intensity - 1); }

auto field_entry::extra_radiation_min() const -> int {
    return type.obj().get_extra_radiation_min(intensity - 1);
}

auto field_entry::extra_radiation_max() const -> int {
    return type.obj().get_extra_radiation_max(intensity - 1);
}

auto field_entry::radiation_hurt_damage_min() const -> int {
    return type.obj().get_radiation_hurt_damage_min(intensity - 1);
}

auto field_entry::radiation_hurt_damage_max() const -> int {
    return type.obj().get_radiation_hurt_damage_max(intensity - 1);
}

auto field_entry::radiation_hurt_message() const -> std::string {
    return type.obj().get_radiation_hurt_message(intensity - 1);
}

auto field_entry::intensity_upgrade_chance() const -> int {
    return type.obj().get_intensity_upgrade_chance(intensity - 1);
}

auto field_entry::intensity_upgrade_duration() const -> time_duration {
    return type.obj().get_intensity_upgrade_duration(intensity - 1);
}

auto field_entry::monster_spawn_chance() const -> int {
    return type.obj().get_monster_spawn_chance(intensity - 1);
}

auto field_entry::monster_spawn_count() const -> int {
    return type.obj().get_monster_spawn_count(intensity - 1);
}

auto field_entry::monster_spawn_radius() const -> int {
    return type.obj().get_monster_spawn_radius(intensity - 1);
}

auto field_entry::monster_spawn_group() const -> mongroup_id {
    return type.obj().get_monster_spawn_group(intensity - 1);
}

auto field_entry::light_emitted() const -> float {
    return type.obj().get_light_emitted(intensity - 1);
}

auto field_entry::local_light_override() const -> float {
    return type.obj().get_local_light_override(intensity - 1);
}

auto field_entry::translucency() const -> float {
    return type.obj().get_translucency(intensity - 1);
}

auto field_entry::is_transparent() const -> bool {
    return type.obj().get_transparent(intensity - 1);
}

auto field_entry::convection_temperature_mod() const -> int {
    return type.obj().get_convection_temperature_mod(intensity - 1);
}

auto field_entry::color() const -> nc_color { return type.obj().get_color(intensity - 1); }

auto field_entry::symbol() const -> std::string { return type.obj().get_symbol(intensity - 1); }

auto field_entry::get_field_type() const -> field_type_id { return type; }

auto field_entry::set_field_type(const field_type_id& new_type) -> field_type_id {
    type = new_type;
    return type;
}

auto field_entry::get_max_field_intensity() const -> int { return type.obj().get_max_intensity(); }

auto field_entry::get_field_intensity() const -> int { return intensity; }

auto field_entry::set_field_intensity(int new_intensity) -> int {
    is_alive = new_intensity > 0;
    return intensity = std::max(std::min(new_intensity, get_max_field_intensity()), 1);
}

auto field_entry::get_field_age() const -> time_duration { return age; }

auto field_entry::set_field_age(const time_duration& new_age) -> time_duration {
    return age = new_age;
}

field::field(): _displayed_field_type(fd_null) {}

/*
Function: find_field
Returns a field entry corresponding to the field_type_id parameter passed in. If no fields are found
then returns NULL. Good for checking for existence of a field: if(myfield.find_field(fd_fire)) would
tell you if the field is on fire.
*/
auto field::find_field(const field_type_id& field_type_to_find) -> field_entry* {
    if (!_displayed_field_type) { return nullptr; }
    const auto it = _field_type_list.find(field_type_to_find);
    if (it != _field_type_list.end()) { return &it->second; }
    return nullptr;
}

auto field::find_field_c(const field_type_id& field_type_to_find) const -> const field_entry* {
    if (!_displayed_field_type) { return nullptr; }
    const auto it = _field_type_list.find(field_type_to_find);
    if (it != _field_type_list.end()) { return &it->second; }
    return nullptr;
}

auto field::find_field(const field_type_id& field_type_to_find) const -> const field_entry* {
    return find_field_c(field_type_to_find);
}

/*
Function: add_field
Inserts the given field_type_id into the field list for a given tile if it does not already exist.
Returns false if the field_type_id already exists, true otherwise.
If the field already exists, it will return false BUT it will add the intensity/age to the current
values for upkeep. If you wish to modify an already existing field use find_field and modify the
result. Intensity defaults to 1, and age to 0 (permanent) if not specified.
*/
auto field::add_field(
    const field_type_id& field_type_to_add, const int new_intensity, const time_duration& new_age)
    -> bool {
    // sanity check, we don't want to store fd_null
    if (!field_type_to_add) {
        debugmsg("Tried to add null field");
        return false;
    }
    auto it = _field_type_list.find(field_type_to_add);
    if (it != _field_type_list.end()) {
        // Most fields stack intensities, but some add duration instead
        if (it->first->stacking_type == fields::stacking_type::intensity) {
            it->second.set_field_intensity(it->second.get_field_intensity() + new_intensity);
        } else {
            time_duration half_life = field_type_to_add->half_life;
            if (new_age < half_life) { it->second.mod_field_age(new_age - half_life); }
        }
        return false;
    }
    if (!_displayed_field_type
        || field_type_to_add.obj().priority >= _displayed_field_type.obj().priority) {
        _displayed_field_type = field_type_to_add;
    }
    _field_type_list[field_type_to_add] = field_entry(field_type_to_add, new_intensity, new_age);
    return true;
}

auto field::remove_field(const field_type_id& field_to_remove) -> bool {
    const auto it = _field_type_list.find(field_to_remove);
    if (it == _field_type_list.end()) { return false; }
    remove_field(it);
    return true;
}

void field::remove_field(std::map<field_type_id, field_entry>::iterator const it) {
    _field_type_list.erase(it);
    _displayed_field_type = fd_null;
    if (!_field_type_list.empty()) {
        for (auto& fld : _field_type_list) {
            if (!_displayed_field_type
                || fld.first.obj().priority >= _displayed_field_type.obj().priority) {
                _displayed_field_type = fld.first;
            }
        }
    }
}

/*
Function: field_count
Returns the number of fields existing on the current tile.
*/
auto field::field_count() const -> unsigned int { return _field_type_list.size(); }

auto field::begin() -> std::map<field_type_id, field_entry>::iterator {
    return _field_type_list.begin();
}

auto field::begin() const -> std::map<field_type_id, field_entry>::const_iterator {
    return _field_type_list.begin();
}

auto field::end() -> std::map<field_type_id, field_entry>::iterator {
    return _field_type_list.end();
}

auto field::end() const -> std::map<field_type_id, field_entry>::const_iterator {
    return _field_type_list.end();
}

/*
Function: displayed_field_type
Returns the last added field type from the tile for drawing purposes.
*/
auto field::displayed_field_type() const -> field_type_id { return _displayed_field_type; }

auto field::displayed_description_affix() const -> description_affix {
    return _displayed_field_type.obj().desc_affix;
}

auto field::total_move_cost() const -> int {
    int current_cost = 0;
    for (auto& fld : _field_type_list) { current_cost += fld.second.move_cost(); }
    return current_cost;
}

auto field_entry::field_effects() const -> std::vector<field_effect> {
    return type->get_intensity_level(intensity - 1).field_effects;
}
